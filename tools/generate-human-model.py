#!/usr/bin/env python3
"""Generate the readable low-poly human OBJ and its tiny ARGB4444 atlas."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1] / "advanced_examples" / "assets"
BOXES = [
    ("torso", (-0.36, 0.20, -0.17), (0.36, 1.08, 0.17), 0),
    ("hips", (-0.32, -0.12, -0.16), (0.32, 0.22, 0.16), 1),
    ("neck", (-0.11, 1.07, -0.10), (0.11, 1.22, 0.10), 2),
    ("head", (-0.25, 1.20, -0.22), (0.25, 1.70, 0.20), 2),
    ("hair", (-0.26, 1.58, -0.23), (0.26, 1.75, 0.21), 3),
    ("left_upper_arm", (-0.58, 0.48, -0.14), (-0.35, 1.06, 0.14), 0),
    ("left_forearm", (-0.64, 0.00, -0.13), (-0.44, 0.50, 0.13), 2),
    ("right_upper_arm", (0.35, 0.50, -0.14), (0.66, 1.06, 0.14), 0),
    ("right_forearm", (0.55, 0.06, -0.13), (0.76, 0.52, 0.13), 2),
    ("left_thigh", (-0.29, -0.77, -0.14), (-0.02, -0.10, 0.14), 1),
    ("right_thigh", (0.02, -0.75, -0.14), (0.29, -0.10, 0.14), 1),
    ("left_shin", (-0.28, -1.35, -0.13), (-0.04, -0.75, 0.13), 1),
    ("right_shin", (0.04, -1.35, -0.13), (0.28, -0.73, 0.13), 1),
    ("left_boot", (-0.31, -1.48, -0.29), (-0.02, -1.31, 0.14), 3),
    ("right_boot", (0.02, -1.48, -0.29), (0.31, -1.31, 0.14), 3),
    ("left_eye", (-0.14, 1.43, -0.235), (-0.07, 1.49, -0.215), 3),
    ("right_eye", (0.07, 1.43, -0.235), (0.14, 1.49, -0.215), 3),
]
FACES = ((0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1),
         (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0))
COLORS = (0xF39B, 0xF347, 0xFDAB, 0xF223)


def main() -> None:
    lines = ["# Static, deliberately faceted human for the DCM1 loader lesson."]
    vertex = uv = 1
    for name, low, high, material in BOXES:
        x0, y0, z0 = low
        x1, y1, z1 = high
        corners = ((x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
                   (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1))
        lines.append(f"o {name}")
        lines.extend(f"v {x:.3f} {y:.3f} {z:.3f}" for x, y, z in corners)
        tile_x = material % 2
        tile_y = material // 2
        # Keep all samples well inside their atlas tile with a half texel inset.
        u0, u1 = tile_x * .5 + .04, (tile_x + 1) * .5 - .04
        v0, v1 = tile_y * .5 + .04, (tile_y + 1) * .5 - .04
        for face in FACES:
            lines.extend((f"vt {u0:.3f} {v0:.3f}", f"vt {u1:.3f} {v0:.3f}",
                          f"vt {u1:.3f} {v1:.3f}", f"vt {u0:.3f} {v1:.3f}"))
            a, b, c, d = face
            lines.append(f"f {vertex+a}/{uv} {vertex+b}/{uv+1} {vertex+c}/{uv+2} {vertex+d}/{uv+3}")
            uv += 4
        vertex += 8
    (ROOT / "human.obj").write_text("\n".join(lines) + "\n")

    header = ["#pragma once", "#include <array>", "#include <cstdint>",
              "#include <cstddef>", "namespace maishuji::human_model_texture {",
              "inline constexpr std::uint16_t width = 64;",
              "inline constexpr std::uint16_t height = 64;",
              "constexpr std::array<std::uint16_t, width * height> make_pixels() noexcept {",
              "    std::array<std::uint16_t, width * height> result{};",
              "    constexpr std::uint16_t colors[] = {" + ", ".join(f"0x{c:04x}" for c in COLORS) + "};",
              "    for(std::size_t y = 0; y < height; ++y)",
              "        for(std::size_t x = 0; x < width; ++x)",
              "            result[y * width + x] = colors[(y / 32) * 2 + x / 32];",
              "    return result;", "}",
              "inline constexpr auto pixels = make_pixels();",
              "} // namespace maishuji::human_model_texture"]
    (ROOT / "human_model_texture.hpp").write_text("\n".join(header) + "\n")


if __name__ == "__main__":
    main()
