"""Unit checks for the scripted dashboard's pixel predicates (not Maple input)."""
import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "controller_frame", ROOT / "tools/check-flycast-controller-state-frame.py"
)
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)


class ControllerFrameTests(unittest.TestCase):
    def setUp(self):
        self.pixels = bytearray(checker.WIDTH * checker.HEIGHT * 3)
        for x, y, color in (
            (620, 100, (12, 25, 40)),
            (382, 403, (38, 64, 82)),
            (508, 302, (101, 230, 159)),
            (473, 267, (82, 108, 124)),
            (261, 348, (255, 196, 94)),
            (553, 205, (255, 196, 94)),
            (79, 210, (82, 108, 124)),
        ):
            offset = (y * checker.WIDTH + x) * 3
            self.pixels[offset:offset + 3] = bytes(color)

    def test_expected_scripted_pixels(self):
        self.assertEqual(checker.check_pixels(self.pixels), [])

    def test_rejects_missing_held_button(self):
        offset = (302 * checker.WIDTH + 508) * 3
        self.pixels[offset:offset + 3] = bytes((82, 108, 124))
        self.assertTrue(any("A held" in failure for failure in checker.check_pixels(self.pixels)))

    def test_transition_fixture(self):
        a = (302 * checker.WIDTH + 508) * 3
        b = (267 * checker.WIDTH + 473) * 3
        trigger = (205 * checker.WIDTH + 553) * 3
        self.pixels[a:a + 3] = bytes((82, 108, 124))
        self.pixels[b:b + 3] = bytes((101, 230, 159))
        self.pixels[trigger:trigger + 3] = bytes((82, 108, 124))
        self.assertEqual(checker.check_pixels(self.pixels, "transitions"), [])
        self.assertTrue(checker.check_pixels(self.pixels, "state"))

    def test_analog_fixture(self):
        for x, y, color in (
            (270, 390, (255, 196, 94)),
            (258, 380, (101, 230, 159)),
            (79, 280, (255, 196, 94)),
        ):
            offset = (y * checker.WIDTH + x) * 3
            self.pixels[offset:offset + 3] = bytes(color)
        self.assertEqual(checker.check_pixels(self.pixels, "analog"), [])
        self.assertTrue(checker.check_pixels(self.pixels, "unknown"))

    def test_port_fixture(self):
        for x, y, color in (
            (30, 60, (101, 230, 159)),
            (30, 162, (82, 108, 124)),
            (30, 264, (255, 196, 94)),
            (30, 366, (255, 196, 94)),
            (620, 460, (12, 25, 40)),
        ):
            offset = (y * checker.WIDTH + x) * 3
            self.pixels[offset:offset + 3] = bytes(color)
        self.assertEqual(checker.check_pixels(self.pixels, "ports"), [])
        self.assertTrue(checker.check_pixels(bytes(len(self.pixels)), "ports"))

    def test_camera_requires_bright_label_and_floor(self):
        self.assertTrue(checker.check_pixels(self.pixels, "camera"))
        for y in range(15, 145, 4):
            for x in range(18, 515, 4):
                offset = (y * checker.WIDTH + x) * 3
                self.pixels[offset:offset + 3] = bytes((220, 220, 220))
        for y in range(260, 465, 4):
            for x in range(120, 520, 4):
                offset = (y * checker.WIDTH + x) * 3
                self.pixels[offset:offset + 3] = bytes((48, 92, 104))
        self.assertEqual(checker.check_pixels(self.pixels, "camera"), [])

    def test_fixed_step_fixture_requires_both_motion_markers(self):
        self.assertTrue(checker.check_pixels(self.pixels, "fixed-step"))
        for y in range(15, 145, 4):
            for x in range(18, 540, 4):
                offset = (y * checker.WIDTH + x) * 3
                self.pixels[offset:offset + 3] = bytes((220, 220, 220))
        for x, y, color in (
            (101, 241, (255, 190, 78)),
            (101, 351, (89, 224, 166)),
        ):
            for sample_y in range(y, y + 32, 4):
                for sample_x in range(x, x + 32, 4):
                    offset = (sample_y * checker.WIDTH + sample_x) * 3
                    self.pixels[offset:offset + 3] = bytes(color)
        self.assertEqual(checker.check_pixels(self.pixels, "fixed-step"), [])

    def test_rejects_wrong_dimensions(self):
        self.assertTrue(checker.check_pixels(b""))


if __name__ == "__main__":
    unittest.main()
