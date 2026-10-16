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
CAR_BOXES = ((187, 225, 217, 272), (427, 225, 457, 272))
SHADOW_BOXES = ((115, 285, 145, 301), (520, 245, 540, 261))
MARKER_BOXES = ((351, 75, 361, 85), (270, 93, 280, 103))


def paint(pixels, box, color):
    left, top, right, bottom = box
    row = bytes(color) * (right - left)
    for y in range(top, bottom):
        start = (y * WIDTH + left) * 3
        pixels[start:start + len(row)] = row


def composition(*, cars=(True, True), shadows=(True, True), markers=(True, True),
                floor=True, shadow_boxes=SHADOW_BOXES, shadow_color=SHADOW):
    pixels = bytearray(bytes(SKY) * WIDTH * HEIGHT)
    if floor:
        paint(pixels, (0, 165, WIDTH, 440), FLOOR)
    for enabled, box in zip(shadows, shadow_boxes):
        if enabled:
            paint(pixels, box, shadow_color)
    for enabled, box in zip(cars, CAR_BOXES):
        if enabled:
            paint(pixels, box, BLUE)
    for enabled, box in zip(markers, MARKER_BOXES):
        if enabled:
            paint(pixels, box, GOLD)
    return pixels


def check(pixels):
    return CHECKER.check_frame(WIDTH, HEIGHT, pixels)


class DynamicLightingCompositionTests(unittest.TestCase):
    def assert_failure(self, result, fragment):
        self.assertFalse(result.passed)
        self.assertIn(fragment, "\n".join(result.failures))

    def test_valid_synthetic_static_composition(self):
        result = check(composition())
        self.assertTrue(result.passed, result.failures)
        measurements = "\n".join(result.measurements)
        self.assertIn("centroid=", measurements)
        self.assertIn("outward displacement=", measurements)
        self.assertIn("left warm floor", measurements)
        self.assertIn("right warm floor", measurements)

    def test_both_shadows_required_not_just_models(self):
        result = check(composition(shadows=(False, False)))
        for name in ("left", "right"):
            self.assert_failure(result, f"{name}: expected >= 24 shadow pixels")
        self.assertIn("exterior shadow count=0, centroid=none", "\n".join(result.measurements))

    def test_each_missing_shadow_rejected_independently(self):
        for missing, name in enumerate(("left", "right")):
            with self.subTest(missing=name):
                flags = tuple(i != missing for i in range(2))
                self.assert_failure(check(composition(shadows=flags)),
                                    f"{name}: expected >= 24 shadow pixels")

    def test_two_shadows_cast_to_same_side_rejected(self):
        for boxes, failed_side in (
            ((SHADOW_BOXES[0], (355, 285, 375, 301)), "right"),
            (((280, 245, 300, 261), SHADOW_BOXES[1]), "left"),
        ):
            with self.subTest(side=failed_side):
                result = check(composition(shadow_boxes=boxes))
                self.assert_failure(result, f"{failed_side}: shadow centroid must be >=20px")
                self.assertNotIn("expected >= 24 shadow pixels", "\n".join(result.failures))

    def test_shadows_directly_below_cars_do_not_prove_opposite_lights(self):
        boxes = ((190, 330, 210, 342), (430, 330, 450, 342))
        result = check(composition(shadow_boxes=boxes))
        for name in ("left", "right"):
            self.assert_failure(result, f"{name}: shadow centroid must be >=20px")
        self.assertNotIn("expected >= 24 shadow pixels", "\n".join(result.failures))

    def test_body_colored_patches_are_not_exterior_shadows(self):
        pixels = composition(shadows=(False, False))
        for box in ((195, 245, 205, 255), (435, 245, 445, 255)):
            paint(pixels, box, SHADOW)
        result = check(pixels)
        for name in ("left", "right"):
            self.assert_failure(result, f"{name}: expected >= 24 shadow pixels")

    def test_wrong_shadow_colors_rejected(self):
        # In particular the dark blue sky must not count as a floor shadow.
        for color in (SKY, (34, 34, 34), (60, 90, 98), BLUE):
            with self.subTest(color=color):
                result = check(composition(shadow_color=color))
                self.assert_failure(result, "left: expected >= 24 shadow pixels")
                self.assert_failure(result, "right: expected >= 24 shadow pixels")

    def test_scalar_lit_body_palette_cannot_impersonate_shadow(self):
        palette = (BLUE, (255, 238, 204), (136, 204, 221), (34, 34, 34),
                   (187, 204, 221), (221, 204, 153), (255, 255, 255), (238, 68, 51))
        for base in palette:
            for step in range(71):
                shade = .30 + step / 100
                color = tuple(round(channel * shade) for channel in base)
                with self.subTest(base=base, shade=shade):
                    self.assertFalse(CHECKER.is_shadow(color))

    def test_tiny_shadow_specks_rejected(self):
        pixels = composition(shadows=(False, False))
        paint(pixels, (120, 290, 123, 293), SHADOW)
        paint(pixels, (525, 250, 528, 253), SHADOW)
        result = check(pixels)
        self.assert_failure(result, "left: expected >= 24 shadow pixels")
        self.assert_failure(result, "right: expected >= 24 shadow pixels")

    def test_each_car_required(self):
        for missing, name in enumerate(("left", "right")):
            with self.subTest(missing=name):
                flags = tuple(i != missing for i in range(2))
                self.assert_failure(check(composition(cars=flags)),
                                    f"{name}: expected >= 150 blue car pixels")

    def test_each_light_marker_required(self):
        for missing, name in enumerate(("left", "right")):
            with self.subTest(missing=name):
                flags = tuple(i != missing for i in range(2))
                self.assert_failure(check(composition(markers=flags)),
                                    f"{name}: expected >= 12 gold light-marker pixels")

    def test_floor_required_in_both_halves(self):
        result = check(composition(floor=False))
        for name in ("left", "right"):
            self.assert_failure(result, f"{name}: expected >= 2000 warm-floor pixels")
        pixels = composition()
        paint(pixels, (340, 350, 600, 425), SKY)
        self.assert_failure(check(pixels), "right: expected >= 2000 warm-floor pixels")

    def test_ambient_blue_and_small_color_errors_accepted(self):
        pixels = composition(shadow_color=(70, 82, 102))
        for box in CAR_BOXES:
            paint(pixels, box, (15, 41, 61))
        result = check(pixels)
        self.assertTrue(result.passed, result.failures)

    def test_lower_gold_marker_faces_also_count(self):
        pixels = composition()
        for box in MARKER_BOXES:
            paint(pixels, box, (231, 165, 64))
        result = check(pixels)
        self.assertTrue(result.passed, result.failures)

    def test_320_by_240_packed_rgb_supported(self):
        full = composition()
        small = b"".join(full[(y * WIDTH + x) * 3:(y * WIDTH + x) * 3 + 3]
                         for y in range(0, HEIGHT, 2) for x in range(0, WIDTH, 2))
        result = CHECKER.check_frame(320, 240, small)
        self.assertTrue(result.passed, result.failures)

    def test_rejects_wrong_dimensions_and_truncated_rgb(self):
        for width, height in ((0, 0), (319, 240), (640, 239), (640, 360), (480, 640)):
            with self.subTest(size=(width, height)):
                with self.assertRaisesRegex(ValueError, "4:3 game window"):
                    CHECKER.check_frame(width, height, b"")
        with self.assertRaisesRegex(ValueError, "packed RGB8 bytes"):
            check(b"\x00" * 100)

    def test_projection_landmarks_independent_of_synthetic_boxes(self):
        for offset, expected_x in ((-3, 200), (3, 440)):
            x, y = CHECKER.project((0, 0, 0), offset)
            self.assertAlmostEqual(x, expected_x, delta=2)
            self.assertAlmostEqual(y, 276, delta=2)
        for point, offset, expected in (((4, 4.5, 0), -3, (356, 80)),
                                        ((-4, 4.5, 0), 3, (275, 98))):
            x, y = CHECKER.project(point, offset)
            self.assertAlmostEqual(x, expected[0], delta=2)
            self.assertAlmostEqual(y, expected[1], delta=2)

    def test_convex_masks_cover_authored_projected_vertices(self):
        positions = [tuple(map(float, line.split()[1:]))
                     for line in CHECKER.ASSET.read_text(encoding="utf-8").splitlines()
                     if line.startswith("v ")]
        for offset, hull in zip((-3, 3), CHECKER.car_hulls()):
            for position in positions:
                self.assertTrue(CHECKER.inside_car(*CHECKER.project(position, offset), hull))


class ImageMagickAdapterTests(unittest.TestCase):
    def test_imagemagick_7_and_6_selection(self):
        with patch.object(CHECKER.shutil, "which", return_value="/usr/bin/magick"):
            self.assertEqual(CHECKER.imagemagick_commands(), (["magick", "identify"], ["magick"]))
        with patch.object(CHECKER.shutil, "which",
                          side_effect=lambda name: None if name == "magick" else f"/usr/bin/{name}"):
            self.assertEqual(CHECKER.imagemagick_commands(), (["identify"], ["convert"]))
        with patch.object(CHECKER.shutil, "which", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "ImageMagick"):
                CHECKER.imagemagick_commands()

    def test_dimensions_checked_before_rgb_conversion(self):
        response = subprocess.CompletedProcess([], 0, stdout=b"640 360", stderr=b"")
        with patch.object(CHECKER, "imagemagick_commands", return_value=(["identify"], ["convert"])), \
                patch.object(CHECKER.subprocess, "run", return_value=response) as run:
            with self.assertRaisesRegex(ValueError, "4:3"):
                CHECKER.read_capture(Path("capture.png"))
        self.assertEqual(run.call_count, 1)

    def test_read_requests_bounded_rgb8_resize_without_shell(self):
        raw = bytes(composition())
        responses = [subprocess.CompletedProcess([], 0, stdout=b"1280 960", stderr=b""),
                     subprocess.CompletedProcess([], 0, stdout=raw, stderr=b"")]
        with patch.object(CHECKER, "imagemagick_commands", return_value=(["identify"], ["convert"])), \
                patch.object(CHECKER.subprocess, "run", side_effect=responses) as run:
            self.assertEqual(CHECKER.read_capture(Path("a capture.png")), raw)
        command = run.call_args_list[1].args[0]
        self.assertEqual(command[2:], ["-resize", "640x480!", "-alpha", "off",
                                      "-colorspace", "sRGB", "-depth", "8", "RGB:-"])
        self.assertTrue(command[1].endswith("a capture.png[0]"))
        for call in run.call_args_list:
            self.assertTrue(call.kwargs["check"])
            self.assertEqual(call.kwargs["timeout"], 30)
            self.assertNotIn("shell", call.kwargs)

    def test_truncated_conversion_rejected(self):
        responses = [subprocess.CompletedProcess([], 0, stdout=b"640 480", stderr=b""),
                     subprocess.CompletedProcess([], 0, stdout=b"short", stderr=b"")]
        with patch.object(CHECKER, "imagemagick_commands", return_value=(["identify"], ["convert"])), \
                patch.object(CHECKER.subprocess, "run", side_effect=responses):
            with self.assertRaisesRegex(ValueError, "expected 640x480 RGB8"):
                CHECKER.read_capture(Path("capture.png"))

    def test_cli_failure_reports_measurements_and_nonzero_exit(self):
        output = io.StringIO()
        with patch.object(CHECKER.Path, "is_file", return_value=True), \
                patch.object(CHECKER, "read_capture", return_value=composition(shadows=(False, False))), \
                contextlib.redirect_stdout(output):
            self.assertEqual(CHECKER.main(["capture.png"]), 1)
        self.assertIn("FAIL: left:", output.getvalue())
        self.assertIn("count=0, centroid=none", output.getvalue())
        self.assertIn("uncalibrated", output.getvalue())
        self.assertNotIn("PASS:", output.getvalue())

    def test_cli_success_does_not_claim_animation_validation(self):
        output = io.StringIO()
        with patch.object(CHECKER.Path, "is_file", return_value=True), \
                patch.object(CHECKER, "read_capture", return_value=composition()), \
                contextlib.redirect_stdout(output):
            self.assertEqual(CHECKER.main(["capture.png"]), 0)
        self.assertIn("PASS: static comparison", output.getvalue())
        self.assertIn("not animation proof", output.getvalue())

    def test_cli_conversion_error_is_actionable(self):
        error = subprocess.CalledProcessError(1, ["convert"], stderr=b"invalid image header")
        output = io.StringIO()
        with patch.object(CHECKER.Path, "is_file", return_value=True), \
                patch.object(CHECKER, "read_capture", side_effect=error), \
                contextlib.redirect_stderr(output):
            self.assertEqual(CHECKER.main(["capture.png"]), 1)
        self.assertIn("invalid image header", output.getvalue())

    def test_cli_missing_capture_and_dependency_are_setup_errors(self):
        with patch.object(CHECKER.Path, "is_file", return_value=False), \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(CHECKER.main(["missing.png"]), 2)
        with patch.object(CHECKER.Path, "is_file", return_value=True), \
                patch.object(CHECKER, "read_capture", side_effect=RuntimeError("ImageMagick required")), \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(CHECKER.main(["capture.png"]), 2)


if __name__ == "__main__":
    unittest.main()
