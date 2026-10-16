"""Synthetic checker tests, NOT Flycast/runtime or real-hardware validation.

python3 -B -m unittest discover -s tests -p test_dynamic_lighting_frame.py -v

Packed RGB fixtures deliberately approximate composition with rectangles. They
exercise negative decisions without ImageMagick, a capture, KOS, or an emulator.
ImageMagick adapter tests mock subprocesses; no external commands are executed.
"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import subprocess
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "check-flycast-dynamic-lighting-frame.py"
spec = importlib.util.spec_from_file_location("test_dynamic_lighting_checker", SCRIPT)
if spec is None or spec.loader is None:
    raise RuntimeError(f"cannot import {SCRIPT}")
CHECKER = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = CHECKER
previous = sys.dont_write_bytecode
try:
    sys.dont_write_bytecode = True
    spec.loader.exec_module(CHECKER)
finally:
    sys.dont_write_bytecode = previous

WIDTH, HEIGHT = 640, 480
SKY = (27, 40, 59)
FLOOR = (229, 217, 190)
BLUE = (51, 136, 204)
SHADOW = (66, 79, 99)
GOLD = (255, 236, 161)

CAR_BOX = (280, 230, 360, 300)
MARKER_BOX = (300, 70, 320, 90)
SHADOW_BOX = (410, 285, 440, 305)


def paint(pixels: bytearray, box: tuple[int, int, int, int], color: tuple[int, int, int]) -> None:
    left, top, right, bottom = box
    row = bytes(color) * (right - left)
    for y in range(top, bottom):
        start = (y * WIDTH + left) * 3
        pixels[start:start + len(row)] = row


def composition(*, car: bool = True, shadow: bool = True, marker: bool = True,
                floor: bool = True,
                shadow_box: tuple[int, int, int, int] = SHADOW_BOX,
                shadow_color: tuple[int, int, int] = SHADOW) -> bytearray:
    pixels = bytearray(bytes(SKY) * WIDTH * HEIGHT)
    if floor:
        paint(pixels, (0, 165, WIDTH, 440), FLOOR)
    if shadow:
        paint(pixels, shadow_box, shadow_color)
    if car:
        paint(pixels, CAR_BOX, BLUE)
    if marker:
        paint(pixels, MARKER_BOX, GOLD)
    return pixels


def check(pixels: bytes | bytearray):
    return CHECKER.check_frame(WIDTH, HEIGHT, bytes(pixels))


class DynamicLightingCompositionTests(unittest.TestCase):
    def assert_failure(self, result, fragment):
        self.assertFalse(result.passed)
        self.assertIn(fragment, "\n".join(result.failures))

    def test_valid_synthetic_composition(self):
        result = check(composition())
        self.assertTrue(result.passed, result.failures)
        measurements = "\n".join(result.measurements)
        self.assertIn("centroid=", measurements)
        self.assertIn("shadow displacement", measurements)
        self.assertIn("left warm floor", measurements)
        self.assertIn("right warm floor", measurements)

    def test_shadow_required(self):
        result = check(composition(shadow=False))
        self.assert_failure(result, "shadow: expected >=")
        self.assertIn("exterior shadow: count=0, centroid=none", "\n".join(result.measurements))

    def test_shadow_inside_car_does_not_count(self):
        inside_car_box = (300, 250, 330, 280)
        result = check(composition(shadow_box=inside_car_box))
        self.assert_failure(result, "shadow: expected >=")

    def test_wrong_shadow_colors_rejected(self):
        for color in (SKY, (34, 34, 34), (60, 90, 98), BLUE):
            with self.subTest(color=color):
                result = check(composition(shadow_color=color))
                self.assert_failure(result, "shadow: expected >=")

    def test_tiny_shadow_specks_rejected(self):
        pixels = composition(shadow=False)
        paint(pixels, (412, 286, 416, 290), SHADOW)
        result = check(pixels)
        self.assert_failure(result, "shadow: expected >=")

    def test_shadow_displacement_required(self):
        near_contact = (315, 280, 335, 295)
        result = check(composition(shadow_box=near_contact))
        self.assert_failure(result, "shadow: centroid must be >=")

    def test_car_required(self):
        result = check(composition(car=False))
        self.assert_failure(result, "car: expected >=")

    def test_marker_required(self):
        result = check(composition(marker=False))
        self.assert_failure(result, "marker: expected >=")

    def test_floor_required_in_both_halves(self):
        result = check(composition(floor=False))
        self.assert_failure(result, "floor-left: expected >=")
        self.assert_failure(result, "floor-right: expected >=")
        pixels = composition()
        paint(pixels, (340, 350, 600, 425), SKY)
        self.assert_failure(check(pixels), "floor-right: expected >=")

    def test_ambient_blue_and_small_color_errors_accepted(self):
        pixels = composition(shadow_color=(70, 82, 102))
        paint(pixels, CAR_BOX, (15, 41, 61))
        result = check(pixels)
        self.assertTrue(result.passed, result.failures)

    def test_lower_gold_marker_faces_also_count(self):
        pixels = composition()
        paint(pixels, MARKER_BOX, (231, 165, 64))
        result = check(pixels)
        self.assertTrue(result.passed, result.failures)

    def test_320_by_240_packed_rgb_supported(self):
        full = composition()
        small = b"".join(
            full[(y * WIDTH + x) * 3:(y * WIDTH + x) * 3 + 3]
            for y in range(0, HEIGHT, 2)
            for x in range(0, WIDTH, 2)
        )
        result = CHECKER.check_frame(320, 240, small)
        self.assertTrue(result.passed, result.failures)

    def test_rejects_wrong_dimensions_and_truncated_rgb(self):
        for width, height in ((0, 0), (319, 240), (640, 239), (640, 360), (480, 640)):
            with self.subTest(size=(width, height)):
                with self.assertRaisesRegex(ValueError, "4:3 game window"):
                    CHECKER.check_frame(width, height, b"")
        with self.assertRaisesRegex(ValueError, "packed RGB8 bytes"):
            check(b"\x00" * 100)

    def test_projection_landmark_independent_of_synthetic_boxes(self):
        x, y = CHECKER.project((0, 0, 0))
        self.assertAlmostEqual(x, 320, delta=8)
        self.assertAlmostEqual(y, 298, delta=8)

    def test_convex_mask_covers_authored_projected_vertices(self):
        positions = [
            tuple(map(float, line.split()[1:]))
            for line in CHECKER.ASSET.read_text(encoding="utf-8").splitlines()
            if line.startswith("v ")
        ]
        hull = CHECKER.car_hull()
        for position in positions:
            self.assertTrue(CHECKER.inside_car(*CHECKER.project(position), hull))


class ImageMagickAdapterTests(unittest.TestCase):
    def test_imagemagick_7_and_6_selection(self):
        with patch.object(CHECKER.shutil, "which", return_value="/usr/bin/magick"):
            self.assertEqual(CHECKER.imagemagick_commands(), (["magick", "identify"], ["magick"]))
        with patch.object(
            CHECKER.shutil,
            "which",
            side_effect=lambda name: None if name == "magick" else f"/usr/bin/{name}",
        ):
            self.assertEqual(CHECKER.imagemagick_commands(), (["identify"], ["convert"]))
        with patch.object(CHECKER.shutil, "which", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "ImageMagick"):
                CHECKER.imagemagick_commands()

    def test_dimensions_checked_before_rgb_conversion(self):
        response = subprocess.CompletedProcess([], 0, stdout=b"640 360", stderr=b"")
        with (
            patch.object(CHECKER, "imagemagick_commands", return_value=(["identify"], ["convert"])),
            patch.object(CHECKER.subprocess, "run", return_value=response) as run,
        ):
            with self.assertRaisesRegex(ValueError, "4:3"):
                CHECKER.read_capture(Path("capture.png"))
        self.assertEqual(run.call_count, 1)

    def test_read_requests_bounded_rgb8_resize_without_shell(self):
        raw = bytes(composition())
        responses = [
            subprocess.CompletedProcess([], 0, stdout=b"1280 960", stderr=b""),
            subprocess.CompletedProcess([], 0, stdout=raw, stderr=b""),
        ]
        with (
            patch.object(CHECKER, "imagemagick_commands", return_value=(["identify"], ["convert"])),
            patch.object(CHECKER.subprocess, "run", side_effect=responses) as run,
        ):
            self.assertEqual(CHECKER.read_capture(Path("a capture.png")), raw)
        command = run.call_args_list[1].args[0]
        self.assertEqual(
            command[2:],
            ["-resize", "640x480!", "-alpha", "off", "-colorspace", "sRGB", "-depth", "8", "RGB:-"],
        )
        self.assertTrue(command[1].endswith("a capture.png[0]"))
        for call in run.call_args_list:
            self.assertTrue(call.kwargs["check"])
            self.assertEqual(call.kwargs["timeout"], 30)
            self.assertNotIn("shell", call.kwargs)

    def test_truncated_conversion_rejected(self):
        responses = [
            subprocess.CompletedProcess([], 0, stdout=b"640 480", stderr=b""),
            subprocess.CompletedProcess([], 0, stdout=b"short", stderr=b""),
        ]
        with (
            patch.object(CHECKER, "imagemagick_commands", return_value=(["identify"], ["convert"])),
            patch.object(CHECKER.subprocess, "run", side_effect=responses),
        ):
            with self.assertRaisesRegex(ValueError, "expected 640x480 RGB8"):
                CHECKER.read_capture(Path("capture.png"))

    def test_cli_failure_reports_measurements_and_nonzero_exit(self):
        output = io.StringIO()
        with (
            patch.object(CHECKER.Path, "is_file", return_value=True),
            patch.object(CHECKER, "read_capture", return_value=bytes(composition(shadow=False))),
            contextlib.redirect_stdout(output),
        ):
            self.assertEqual(CHECKER.main(["capture.png"]), 1)
        self.assertIn("FAIL: shadow:", output.getvalue())
        self.assertIn("count=0, centroid=none", output.getvalue())
        self.assertIn("uncalibrated", output.getvalue())
        self.assertNotIn("PASS:", output.getvalue())

    def test_cli_success_does_not_claim_animation_validation(self):
        output = io.StringIO()
        with (
            patch.object(CHECKER.Path, "is_file", return_value=True),
            patch.object(CHECKER, "read_capture", return_value=bytes(composition())),
            contextlib.redirect_stdout(output),
        ):
            self.assertEqual(CHECKER.main(["capture.png"]), 0)
        self.assertIn("PASS: static check", output.getvalue())
        self.assertIn("not animation proof", output.getvalue())

    def test_cli_conversion_error_is_actionable(self):
        error = subprocess.CalledProcessError(1, ["convert"], stderr=b"invalid image header")
        output = io.StringIO()
        with (
            patch.object(CHECKER.Path, "is_file", return_value=True),
            patch.object(CHECKER, "read_capture", side_effect=error),
            contextlib.redirect_stderr(output),
        ):
            self.assertEqual(CHECKER.main(["capture.png"]), 1)
        self.assertIn("invalid image header", output.getvalue())

    def test_cli_missing_capture_and_dependency_are_setup_errors(self):
        with (
            patch.object(CHECKER.Path, "is_file", return_value=False),
            contextlib.redirect_stderr(io.StringIO()),
        ):
            self.assertEqual(CHECKER.main(["missing.png"]), 2)
        with (
            patch.object(CHECKER.Path, "is_file", return_value=True),
            patch.object(CHECKER, "read_capture", side_effect=RuntimeError("ImageMagick required")),
            contextlib.redirect_stderr(io.StringIO()),
        ):
            self.assertEqual(CHECKER.main(["capture.png"]), 2)


if __name__ == "__main__":
    unittest.main()
