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

    def test_rejects_wrong_dimensions(self):
        self.assertTrue(checker.check_pixels(b""))


if __name__ == "__main__":
    unittest.main()
