"""Check advanced11's animated single-car lighting frame.

Usage: python3 tools/check-flycast-dynamic-lighting-frame.py capture.png

ImageMagick 6/7 reads real captures; check_frame accepts packed RGB8 directly.
Capture dimensions follow the existing harness: >=320x240, aspect 1.30..1.36.
All regions/counts use 640x480 coordinates. Thresholds are initial/uncalibrated:
synthetic tests exercise the checker, not Flycast or real Dreamcast rendering.

The independent projection mirrors the runtime's normal camera:
camera (5,5,8) -> (0,1.4,0), vertical FOV .95, aspect 4/3. The car is static;
the light and marker orbit, so marker/shadow checks use broad regions and only
require a shadow centroid displaced from the projected wheel-contact center.
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

MIN_BLUE, MIN_SHADOW, MIN_MARKER = 150, 24, 8
MIN_DISPLACEMENT = 12.0
MIN_FLOOR = 2000

RGB = tuple[int, int, int]
Point = tuple[float, float]
Vec3 = tuple[float, float, float]
Region = tuple[int, int, int, int]  # left, top, right, bottom; exclusive maxima

CAMERA_POSITION: Vec3 = (5.0, 5.0, 8.0)
CAMERA_TARGET: Vec3 = (0.0, 1.4, 0.0)
CAMERA_UP: Vec3 = (0.0, 1.0, 0.0)
CAMERA_FOV = 0.95

BLUE_REGION = (170, 180, 470, 360)
MARKER_REGION = (120, 20, 520, 220)
SHADOW_REGION = (80, 220, 560, 390)
LEFT_FLOOR_REGION = (40, 350, 300, 425)
RIGHT_FLOOR_REGION = (340, 350, 600, 425)


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
    if width < 320 or height < 240 or not 1.30 <= width / height <= 1.36:
        raise ValueError(f"unexpected capture dimensions: {width}x{height}; "
                         "expected a 4:3 game window of at least 320x240")


def sub(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def dot(a: Vec3, b: Vec3) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: Vec3, b: Vec3) -> Vec3:
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def normalize(v: Vec3) -> Vec3:
    length_sq = dot(v, v)
    if not math.isfinite(length_sq) or length_sq <= 1.0e-12:
        raise ValueError("invalid camera basis")
    inv = 1.0 / math.sqrt(length_sq)
    return (v[0] * inv, v[1] * inv, v[2] * inv)


@lru_cache(maxsize=1)
def camera_basis() -> tuple[Vec3, Vec3, Vec3]:
    forward = normalize(sub(CAMERA_TARGET, CAMERA_POSITION))
    right = normalize(cross(forward, CAMERA_UP))
    up = cross(right, forward)
    return right, up, forward


def project(point: Vec3) -> Point:
    right, up, forward = camera_basis()
    view = sub(point, CAMERA_POSITION)
    depth = dot(view, forward)
    if not math.isfinite(depth) or depth <= 1.0e-5:
        raise ValueError("projected point is behind the camera")
    focal = HEIGHT / (2 * math.tan(CAMERA_FOV / 2))
    x = WIDTH / 2 + focal * dot(view, right) / depth
    y = HEIGHT / 2 - focal * dot(view, up) / depth
    return x, y


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
    return all(turn(a, b, (x, y)) >= -2 * math.hypot(b[0] - a[0], b[1] - a[1])
               for a, b in zip(hull, hull[1:] + hull[:1]))


@lru_cache(maxsize=1)
def car_hull() -> tuple[Point, ...]:
    positions = []
    for line in ASSET.read_text(encoding="utf-8").splitlines():
        fields = line.split("#", 1)[0].split()
        if fields and fields[0] == "v":
            positions.append(tuple(map(float, fields[1:])))
    if not positions or any(len(p) != 3 for p in positions):
        raise ValueError(f"expected XYZ positions in {ASSET}")
    hull = convex_hull([project(position) for position in positions])
    if len(hull) < 3:
        raise ValueError("car projection has no conservative silhouette")
    return hull


def near(color: RGB, target: RGB, tolerance: int) -> bool:
    return all(abs(a - b) <= tolerance for a, b in zip(color, target))


def is_blue(color: RGB) -> bool:
    r, g, b = color
    return b >= 45 and g >= 22 and b >= 1.28 * g and b >= 2.7 * r


def is_shadow(color: RGB) -> bool:
    r, g, b = color
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
            if predicate(color) and location(x + 0.5, y + 0.5):
                count += 1
                sx += x
                sy += y
    return Sample(count, sx / count if count else None, sy / count if count else None)


def check_frame(width: int, height: int, pixels: bytes) -> CheckResult:
    validate_dimensions(width, height)
    if len(pixels) != width * height * 3:
        raise ValueError(f"expected {width * height * 3} packed RGB8 bytes, got {len(pixels)}")
    hull = car_hull()
    failures, measurements = [], []

    blue = measure(width, height, pixels, BLUE_REGION, is_blue,
                   lambda x, y: inside_car(x, y, hull))
    shadow = measure(width, height, pixels, SHADOW_REGION, is_shadow,
                     lambda x, y: not inside_car(x, y, hull))
    marker = measure(width, height, pixels, MARKER_REGION,
                     lambda rgb: any(near(rgb, gold, 18) for gold in GOLD))
    floor_left = measure(width, height, pixels, LEFT_FLOOR_REGION, lambda rgb: near(rgb, GROUND, 16))
    floor_right = measure(width, height, pixels, RIGHT_FLOOR_REGION, lambda rgb: near(rgb, GROUND, 16))

    contact_x, contact_y = project((0.0, 0.0, 0.0))
    displacement = None
    if shadow.x is not None and shadow.y is not None:
        displacement = math.hypot(shadow.x - contact_x, shadow.y - contact_y)

    measurements.append(f"car blue: {blue.describe()} in {BLUE_REGION}")
    measurements.append(f"exterior shadow: {shadow.describe()} in {SHADOW_REGION}")
    measurements.append(f"light marker: {marker.describe()} in {MARKER_REGION}")
    measurements.append(f"shadow displacement from contact ({contact_x:.1f},{contact_y:.1f}): "
                        f"{'none' if displacement is None else f'{displacement:.1f}px'}")
    measurements.append(f"left warm floor: {floor_left.describe()} in {LEFT_FLOOR_REGION}")
    measurements.append(f"right warm floor: {floor_right.describe()} in {RIGHT_FLOOR_REGION}")

    if blue.count < MIN_BLUE:
        failures.append(f"car: expected >= {MIN_BLUE} blue car pixels in {BLUE_REGION}")
    if shadow.count < MIN_SHADOW:
        failures.append(
            f"shadow: expected >= {MIN_SHADOW} shadow pixels outside projected car silhouette in {SHADOW_REGION}"
        )
    if displacement is None or displacement < MIN_DISPLACEMENT:
        failures.append(f"shadow: centroid must be >= {MIN_DISPLACEMENT:.0f}px from projected wheel-contact center")
    if marker.count < MIN_MARKER:
        failures.append(f"marker: expected >= {MIN_MARKER} gold light-marker pixels in {MARKER_REGION}")
    if floor_left.count < MIN_FLOOR:
        failures.append(f"floor-left: expected >= {MIN_FLOOR} warm-floor pixels in {LEFT_FLOOR_REGION}")
    if floor_right.count < MIN_FLOOR:
        failures.append(f"floor-right: expected >= {MIN_FLOOR} warm-floor pixels in {RIGHT_FLOOR_REGION}")
    return CheckResult(tuple(failures), tuple(measurements))


def imagemagick_commands() -> tuple[list[str], list[str]]:
    if shutil.which("magick"):
        return ["magick", "identify"], ["magick"]
    if shutil.which("identify") and shutil.which("convert"):
        return ["identify"], ["convert"]
    raise RuntimeError("ImageMagick 6 or 7 is required (convert/identify or magick)")


def read_capture(path: Path) -> bytes:
    identify, convert = imagemagick_commands()
    image = str(path.resolve()) + "[0]"
    dimensions = subprocess.run(
        identify + ["-format", "%w %h", image],
        check=True,
        capture_output=True,
        timeout=30,
    ).stdout.split()
    if len(dimensions) != 2:
        raise ValueError("could not read one capture's dimensions")
    validate_dimensions(int(dimensions[0]), int(dimensions[1]))
    result = subprocess.run(
        convert + [image, "-resize", "640x480!", "-alpha", "off",
                   "-colorspace", "sRGB", "-depth", "8", "RGB:-"],
        check=True,
        capture_output=True,
        timeout=30,
    )
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
    print("Thresholds: initial/uncalibrated; static visibility check only, not animation proof.")
    for measurement in result.measurements:
        print("  " + measurement)
    for failure in result.failures:
        print("FAIL: " + failure)
    if result.passed:
        print("PASS: static check shows one blue car, warm floor, one light marker and an exterior shadow.")
    return 0 if result.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
