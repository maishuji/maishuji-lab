"""Check explicitly scripted controller dashboards, never physical input.

Usage: python3 tools/check-flycast-controller-state-frame.py CAPTURE.png
Set MAISHUJI_FRAME_SCENE=transitions or analog (default: state).
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

WIDTH, HEIGHT = 640, 480


def rgb_at(data: bytes, x: int, y: int) -> tuple[int, int, int]:
    start = (y * WIDTH + x) * 3
    return tuple(data[start:start + 3])


def check_pixels(data: bytes, scene: str = "state") -> list[str]:
    if len(data) != WIDTH * HEIGHT * 3:
        return ["expected packed 640x480 RGB pixels"]
    samples = {
        "background": (620, 100, lambda r, g, b: b > r and b < 80),
        "panel": (382, 403, lambda r, g, b: b > g > r and 30 < r < 70),
    }
    if scene == "state":
        samples.update({
            "A held": (508, 302, lambda r, g, b: g > r * 1.6 and g > b),
            "B not held": (473, 267, lambda r, g, b: 55 < r < 130 and b > r),
            "stick dot": (261, 348, lambda r, g, b: r > 180 and r > g > b),
            "right trigger": (553, 205, lambda r, g, b: r > 180 and r > g > b),
            "left trigger zero region": (79, 210, lambda r, g, b: 55 < r < 130),
        })
    elif scene == "transitions":
        samples.update({
            "B held": (473, 267, lambda r, g, b: g > r * 1.6 and g > b),
            "A not held": (508, 302, lambda r, g, b: 55 < r < 130 and b > r),
            "no trigger": (553, 205, lambda r, g, b: 55 < r < 130),
        })
    elif scene == "analog":
        samples.update({
            "raw diagonal": (270, 390, lambda r, g, b: r > 180 and r > g > b),
            "processed diagonal": (258, 380, lambda r, g, b: g > r * 1.6 and g > b),
            "right trigger": (553, 205, lambda r, g, b: r > 180 and r > g > b),
            "left trigger mid": (79, 280, lambda r, g, b: r > 180 and r > g > b),
        })
    else:
        return [f"unknown scripted scene: {scene}"]
    return [f"missing {name} at ({x},{y}): {rgb_at(data,x,y)}"
            for name, (x, y, predicate) in samples.items()
            if not predicate(*rgb_at(data, x, y))]


def main() -> int:
    if len(sys.argv) != 2 or not Path(sys.argv[1]).is_file():
        print("Usage: python3 check-flycast-controller-state-frame.py CAPTURE.png", file=sys.stderr)
        return 2
    if shutil.which("magick"):
        identify, convert = ["magick", "identify"], ["magick"]
    elif shutil.which("identify") and shutil.which("convert"):
        identify, convert = ["identify"], ["convert"]
    else:
        print("ImageMagick 6 or 7 required", file=sys.stderr)
        return 2
    image = str(Path(sys.argv[1]).resolve()) + "[0]"
    try:
        dimensions = subprocess.run(identify + ["-format", "%w %h", image],
                                    capture_output=True, check=True, timeout=15).stdout.split()
        width, height = map(int, dimensions)
        if width < 320 or height < 240 or not 1.30 < width / height < 1.36:
            raise ValueError("expected a 4:3 game capture of at least 320x240")
        data = subprocess.run(convert + [image, "-resize", "640x480!", "-alpha", "off",
                                         "-depth", "8", "RGB:-"],
                              capture_output=True, check=True, timeout=15).stdout
        scene = os.environ.get("MAISHUJI_FRAME_SCENE", "state")
        failures = check_pixels(data, scene)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"FAIL: cannot inspect controller screenshot: {error}", file=sys.stderr)
        return 1
    for failure in failures:
        print("FAIL:", failure)
    if failures:
        return 1
    print(f"PASS: scripted {scene} dashboard pixels; not real input")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
