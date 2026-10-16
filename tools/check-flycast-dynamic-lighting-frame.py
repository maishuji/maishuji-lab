
"""Check advanced11's fixed opposite-light comparison, not its animation.

Usage: python3 tools/check-flycast-dynamic-lighting-frame.py capture.png

ImageMagick 6/7 reads real captures; check_frame accepts packed RGB8 directly.
Capture dimensions follow the existing harness: >=320x240, aspect 1.30..1.36.
All regions/counts use 640x480 coordinates. Initial thresholds are UNCALIBRATED:
synthetic tests exercise the checker, not Flycast or real Dreamcast rendering.

The independent pinhole projection assumes camera (0,6,10)->(0,1,0), vertical
FOV .95, aspect 4/3, yaw .3 and translations +/-3. Car hulls come from authored
OBJ positions, not from the capture or the runtime's rendering implementation.
Their convex silhouettes conservatively exclude body pixels from shadow counts
(with a 2px guard). No shadow rasterizer or per-pixel depth reference is needed.
Shadow search windows are x=70..310 / 330..590, y=225..350; each centroid must
lie >=20px outward from the projected wheel-contact center. Each search needs
>=24 shadow pixels outside BOTH cars. Markers are sought within 16px of their
projected centers (~356,80 and ~275,98), not next to their owning car.
"""

from __future__ import annotations

import argparse
import math
import shutil
import subprocess
import sys
from collections.abc import Callable
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
from typing import NamedTuple

WIDTH, HEIGHT = 640, 480
ASSET = Path(__file__).resolve().parents[1] / "advanced_examples" / "assets" / "car_2cv.obj"
GROUND = (229, 217, 190)
SHADOW = (66, 79, 99)
GOLD = ((255, 236, 161), (231, 165, 64))
MIN_BLUE, MIN_SHADOW, MIN_MARKER, MIN_FLOOR = 150, 24, 12, 2000
MIN_DISPLACEMENT = 20
RGB = tuple[int, int, int]
Point = tuple[float, float]
Region = tuple[int, int, int, int]  # left, top, right, bottom; exclusive maxima


@dataclass(frozen=True)
class Sample:
    count: int
    x: float | None
    y: float | None

    def describe(self) -> str:
        center = "none" if self.x is None else f"({self.x:.1f},{self.y:.1f})"
        return f"count={self.count}, centroid={center}"


class CheckResult(NamedTuple):
    failures: tuple[str, ...]
    measurements: tuple[str, ...]

    @property
    def passed(self) -> bool:
        return not self.failures


def validate_dimensions(width: int, height: int) -> None:
    """Use the same minimum dimensions/aspect tolerance as existing checkers."""
    if width < 320 or height < 240 or not 1.30 <= width / height <= 1.36:
        raise ValueError(f"unexpected capture dimensions: {width}x{height}; "
                         "expected a 4:3 game window of at least 320x240")


def project(point: tuple[float, float, float], translation: float) -> Point:
    """Project a model-local point into the fixed capture camera."""
    x, y, z = point
    c, s = math.cos(.3), math.sin(.3)
    x, z = c * x + s * z + translation, -s * x + c * z
    # Camera forward=(0,-1,-2)/sqrt(5); right=+X; up=(0,2,-1)/sqrt(5).
    depth = (-(y - 6) - 2 * (z - 10)) / math.sqrt(5)
    up = (2 * (y - 6) - (z - 10)) / math.sqrt(5)
    focal = HEIGHT / (2 * math.tan(.95 / 2))
    return WIDTH / 2 + focal * x / depth, HEIGHT / 2 - focal * up / depth


def turn(a: Point, b: Point, p: Point) -> float:
    return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])


def convex_hull(points: list[Point]) -> tuple[Point, ...]:
    points = sorted(set(points))
    halves = []
    for ordered in (points, list(reversed(points))):
        half: list[Point] = []
        for p in ordered:
            while len(half) >= 2 and turn(half[-2], half[-1], p) <= 0:
                half.pop()
            half.append(p)
        halves.append(half[:-1])
    return tuple(halves[0] + halves[1])


def inside_car(x: float, y: float, hull: tuple[Point, ...]) -> bool:
    # Cross products are scaled by edge length so the guard is in screen pixels.
    return all(turn(a, b, (x, y)) >= -2 * math.hypot(b[0] - a[0], b[1] - a[1])
               for a, b in zip(hull, hull[1:] + hull[:1]))


@lru_cache(maxsize=1)
def car_hulls() -> tuple[tuple[Point, ...], ...]:
    positions = []
    for line in ASSET.read_text(encoding="utf-8").splitlines():
        fields = line.split("#", 1)[0].split()
        if fields and fields[0] == "v":
            positions.append(tuple(map(float, fields[1:])))
    if not positions or any(len(p) != 3 for p in positions):
        raise ValueError(f"expected XYZ positions in {ASSET}")
    hulls = tuple(convex_hull([project(p, offset) for p in positions])
                  for offset in (-3, 3))
    if any(len(hull) < 3 for hull in hulls):
        raise ValueError("car projection has no conservative silhouette")
    return hulls


def near(color: RGB, target: RGB, tolerance: int) -> bool:
    return all(abs(a - b) <= tolerance for a, b in zip(color, target))


def is_blue(color: RGB) -> bool:
    r, g, b = color
    # Includes ambient-only paint (15,41,61), excludes sky, shadow, glass/metal.
    return b >= 45 and g >= 22 and b >= 1.28 * g and b >= 2.7 * r


def is_shadow(color: RGB) -> bool:
    r, g, b = color
    # Channel gaps reject scalar-lit blue/glass/metal and gray rubber surfaces.
    return near(color, SHADOW, 10) and b - g >= 12 and g - r >= 5


def measure(width: int, height: int, pixels: bytes, region: Region,
            predicate: Callable[[RGB], bool],
            location: Callable[[float, float], bool] = lambda x, y: True) -> Sample:
    count = sx = sy = 0
    left, top, right, bottom = region
    for y in range(top, bottom):
        row = (y * height // HEIGHT) * width
        for x in range(left, right):
            offset = (row + x * width // WIDTH) * 3
            color = (pixels[offset], pixels[offset + 1], pixels[offset + 2])
            if predicate(color) and location(x + .5, y + .5):
                count += 1
                sx += x
                sy += y
    return Sample(count, sx / count if count else None, sy / count if count else None)


def check_frame(width: int, height: int, pixels: bytes) -> CheckResult:
    """Inspect RGB8 without subprocesses; non-640 captures use nearest sampling.

    The CLI instead uses ImageMagick resizing, like the existing shell checkers.
    This is a static composition heuristic, not proof of animation or lighting
    correctness. A synthetic drawing can satisfy it; it is not a reference render.
    """
    validate_dimensions(width, height)
    if len(pixels) != width * height * 3:
        raise ValueError(f"expected {width * height * 3} packed RGB8 bytes, got {len(pixels)}")
    hulls = car_hulls()
    failures, measurements = [], []

    def outside_cars(x: float, y: float) -> bool:
        return not any(inside_car(x, y, hull) for hull in hulls)

    for i, (name, offset, direction) in enumerate((("left", -3, -1), ("right", 3, 1))):
        blue_region = (120, 180, 290, 335) if i == 0 else (350, 180, 550, 335)
        blue = measure(width, height, pixels, blue_region, is_blue,
                       lambda x, y, hull=hulls[i]: inside_car(x, y, hull))
        shadow_region = (70, 225, 310, 350) if i == 0 else (330, 225, 590, 350)
        shadow = measure(width, height, pixels, shadow_region, is_shadow, outside_cars)
        contact_x, _ = project((0, 0, 0), offset)
        displacement = None if shadow.x is None else direction * (shadow.x - contact_x)
        marker_x, marker_y = project((4 if i == 0 else -4, 4.5, 0), offset)
        marker_region = (math.floor(marker_x - 16), math.floor(marker_y - 16),
                         math.ceil(marker_x + 16), math.ceil(marker_y + 16))
        marker = measure(width, height, pixels, marker_region,
                         lambda rgb: any(near(rgb, gold, 18) for gold in GOLD))
        dx_text = "none" if displacement is None else f"{displacement:.1f}px"
        measurements.append(f"{name}: blue {blue.describe()}; exterior shadow {shadow.describe()}; "
                            f"outward displacement={dx_text} (need >=20px, contact x={contact_x:.1f}); "
                            f"marker {marker.describe()} near ({marker_x:.1f},{marker_y:.1f})")
        if blue.count < MIN_BLUE:
            failures.append(f"{name}: expected >= {MIN_BLUE} blue car pixels inside its projected silhouette")
        if shadow.count < MIN_SHADOW:
            failures.append(f"{name}: expected >= {MIN_SHADOW} shadow pixels outside projected car silhouettes")
        if displacement is None or displacement < MIN_DISPLACEMENT:
            failures.append(f"{name}: shadow centroid must be >=20px "
                            f"{'left' if direction < 0 else 'right'} of wheel-contact center")
        if marker.count < MIN_MARKER:
            failures.append(f"{name}: expected >= {MIN_MARKER} gold light-marker pixels in {marker_region}")

    for name, region in (("left", (40, 350, 300, 425)), ("right", (340, 350, 600, 425))):
        floor = measure(width, height, pixels, region, lambda rgb: near(rgb, GROUND, 16))
        measurements.append(f"{name} warm floor: {floor.describe()} in {region}")
        if floor.count < MIN_FLOOR:
            failures.append(f"{name}: expected >= {MIN_FLOOR} warm-floor pixels in {region}")
    return CheckResult(tuple(failures), tuple(measurements))


def imagemagick_commands() -> tuple[list[str], list[str]]:
    if shutil.which("magick"):
        return ["magick", "identify"], ["magick"]
    if shutil.which("identify") and shutil.which("convert"):
        return ["identify"], ["convert"]
    raise RuntimeError("ImageMagick 6 or 7 is required (convert/identify or magick)")


def read_capture(path: Path) -> bytes:
    identify, convert = imagemagick_commands()
    # Absolute paths cannot become command options. Only the first frame is read.
    image = str(path.resolve()) + "[0]"
    dimensions = subprocess.run(identify + ["-format", "%w %h", image],
                                check=True, capture_output=True, timeout=30).stdout.split()
    if len(dimensions) != 2:
        raise ValueError("could not read one capture's dimensions")
    validate_dimensions(int(dimensions[0]), int(dimensions[1]))
    result = subprocess.run(convert + [image, "-resize", "640x480!", "-alpha", "off",
                                      "-colorspace", "sRGB", "-depth", "8", "RGB:-"],
                            check=True, capture_output=True, timeout=30)
    if len(result.stdout) != WIDTH * HEIGHT * 3:
        raise ValueError(f"ImageMagick returned {len(result.stdout)} bytes; expected 640x480 RGB8")
    return result.stdout


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="Flycast game-window capture")
    args = parser.parse_args(argv)
    if not args.capture.is_file():
        print(f"Image not found: {args.capture}", file=sys.stderr)
        return 2
    try:
        result = check_frame(WIDTH, HEIGHT, read_capture(args.capture))
    except RuntimeError as error:
        print(f"Dynamic-lighting frame check: {error}", file=sys.stderr)
        return 2
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        detail = str(error)
        if isinstance(error, subprocess.CalledProcessError):
            detail += ": " + (error.stderr or b"").decode("utf-8", errors="replace")[:1000]
        print(f"FAIL: dynamic-lighting capture could not be checked: {detail}", file=sys.stderr)
        return 1
    print("Thresholds: initial/uncalibrated; static visibility comparison only, not animation proof.")
    for measurement in result.measurements:
        print("  " + measurement)
    for failure in result.failures:
        print("FAIL: " + failure)
    if result.passed:
        print("PASS: static comparison shows two blue cars, warm floor, two light markers "
              "and oppositely displaced exterior shadows.")
    return 0 if result.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
