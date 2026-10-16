
"""Derive Dreamcast-ready assets from the AUTHORED car_2cv.obj and car_2cv.mtl.

python3 -B tools/generate-2cv-model.py --output-dir /tmp/car-2cv-derived

OBJ/MTL are editable source inputs and are NEVER written by this tool. Outputs:
car_2cv_model_asset.hpp, car_2cv_mesh.hpp, car_2cv_texture.hpp,
car_2cv_texture.ppm, and an accurate
car_2cv_preview.svg triangle projection. The checked-in SVG starts as a labeled
design illustration; running with the default output directory replaces that
illustration with the derived host preview. Example 11 consumes the generated
mesh and atlas headers; CMake writes them into the build tree.
Uses only the standard library and the existing, unchanged OBJ/DCM1 converter.
"""

from __future__ import annotations

import argparse
import importlib.util
import math
import sys
from collections.abc import Sequence
from pathlib import Path
from typing import NamedTuple

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "advanced_examples" / "assets" / "car_2cv.obj"
WIDTH, HEIGHT, TILE = 64, 32, 16
OUTPUTS = ("car_2cv_model_asset.hpp", "car_2cv_mesh.hpp", "car_2cv_texture.hpp",
           "car_2cv_texture.ppm", "car_2cv_preview.svg")
Point = tuple[float, float, float]


class Face(NamedTuple):
    feature: str
    material: str
    corners: tuple[tuple[int, int], ...]


class Source(NamedTuple):
    positions: list[Point]
    uvs: list[tuple[float, float]]
    faces: list[Face]
    material_path: Path


def load_converter():
    path = ROOT / "tools" / "obj-to-dcmodel.py"
    spec = importlib.util.spec_from_file_location("car_2cv_converter", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module  # Required by the converter's dataclass.
    previous = sys.dont_write_bytecode
    try:
        sys.dont_write_bytecode = True
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = previous
    return module


def read_source(path: Path) -> Source:
    """Retain names/materials; resolve negative indices just like the converter."""
    converter = load_converter()
    positions, uvs, faces = [], [], []
    feature, material, library = "", "", None
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        fields = line.split("#", 1)[0].split()
        if not fields:
            continue
        record, *values = fields
        if record == "v":
            x, y, z = map(float, values)
            positions.append((x, y, z))
        elif record == "vt":
            u, v = map(float, values)
            uvs.append((u, v))
        elif record == "o":
            feature = values[0]
        elif record == "usemtl":
            material = values[0]
        elif record == "mtllib":
            if library is not None or len(values) != 1:
                raise ValueError("expected one MTL source library")
            library = path.parent / values[0]
        elif record == "f":
            if len(values) != 3 or not feature or not material:
                raise ValueError(f"{path}:{number}: expected a named, materialed triangle")
            corners = []
            for token in values:
                p, uv = token.split("/")
                corners.append((converter.parse_index(p, len(positions), "position"),
                                converter.parse_index(uv, len(uvs), "UV")))
            faces.append(Face(feature, material, tuple(corners)))
        elif record != "s":
            raise ValueError(f"{path}:{number}: unsupported source record {record}")
    if not faces or library is None:
        raise ValueError("source needs triangles and an MTL library")
    return Source(positions, uvs, faces, library)


def read_palette(path: Path) -> list[tuple[str, int]]:
    """MTL declaration order defines eight atlas slots; Kd is quantized to RGB444."""
    names, diffuse, opacity = [], {}, {}
    name = ""
    for line in path.read_text(encoding="utf-8").splitlines():
        fields = line.split("#", 1)[0].split()
        if not fields:
            continue
        if fields[0] == "newmtl":
            name = fields[1]
            if name in names:
                raise ValueError(f"duplicate material {name}")
            names.append(name)
        elif fields[0] == "Kd":
            values = tuple(float(v) for v in fields[1:])
            if len(values) != 3 or not all(0 <= v <= 1 for v in values):
                raise ValueError(f"invalid diffuse color for {name}")
            r, g, b = (round(v * 15) for v in values)
            diffuse[name] = 0xF000 | (r << 8) | (g << 4) | b
        elif fields[0] == "d":
            opacity[name] = float(fields[1])
    if len(names) != 8 or any(n not in diffuse or opacity.get(n) != 1 for n in names):
        raise ValueError("expected exactly eight diffuse, explicitly opaque MTL materials")
    return [(n, diffuse[n]) for n in names]


def sub(a: Point, b: Point) -> Point:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a: Point, b: Point) -> Point:
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def dot(a: Point, b: Point) -> float:
    return sum(x * y for x, y in zip(a, b))


def normal(points: Sequence[Point]) -> Point:
    return cross(sub(points[1], points[0]), sub(points[2], points[0]))


def unit(a: Point) -> Point:
    length = math.sqrt(dot(a, a))
    return (a[0] / length, a[1] / length, a[2] / length)


def rgb(color: int) -> tuple[int, int, int]:
    return (((color >> 8) & 15) * 17, ((color >> 4) & 15) * 17, (color & 15) * 17)


def atlas_pixels(palette: list[tuple[str, int]]) -> list[int]:
    return [palette[(y // TILE) * 4 + x // TILE][1]
            for y in range(HEIGHT) for x in range(WIDTH)]


def validate(source: Source, palette: list[tuple[str, int]]) -> None:
    slots = {name: i for i, (name, _) in enumerate(palette)}
    for face in source.faces:
        points = [source.positions[p] for p, _ in face.corners]
        n = normal(points)
        quantized = [(round(x * 256), round(y * 256), round(z * 256)) for x, y, z in points]
        if dot(n, n) < 1e-14 or dot(n, normal(quantized)) <= 0:
            raise ValueError(f"degenerate or Q8.8-inverted triangle in {face.feature}")
        slot = slots[face.material]
        expected = ((slot % 4 + .5) / 4, 1 - (slot // 4 + .5) / 2)
        for _, uv in face.corners:
            if source.uvs[uv] != expected:
                raise ValueError(f"{face.feature}: UV is not centered in {face.material} swatch")


def make_texture(palette: list[tuple[str, int]]) -> str:
    colors = ", ".join(f"0x{color:04x}" for _, color in palette)
    names = ", ".join(n for n, _ in palette[:4]) + " / " + ", ".join(n for n, _ in palette[4:])
    return f"""#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Derived from the authored car_2cv.mtl palette. Opaque, unlit base colors.
// Top-down linear ARGB4444; {names}.
namespace maishuji::car_2cv_texture {{
inline constexpr std::uint16_t width = {WIDTH};
inline constexpr std::uint16_t height = {HEIGHT};
constexpr std::array<std::uint16_t, width * height> make_pixels() noexcept {{
    std::array<std::uint16_t, width * height> result{{}};
    constexpr std::uint16_t colors[] = {{{colors}}};
    for(std::size_t y = 0; y < height; ++y) {{
        for(std::size_t x = 0; x < width; ++x) {{
            result[y * width + x] = colors[(y / {TILE}) * 4 + x / {TILE}];
        }}
    }}
    return result;
}}
inline constexpr auto pixels = make_pixels();
}} // namespace maishuji::car_2cv_texture
"""


def make_ppm(palette: list[tuple[str, int]]) -> str:
    lines = ["P3", "# Unlit car_2cv.mtl swatch atlas", f"{WIDTH} {HEIGHT}", "255"]
    pixels = atlas_pixels(palette)
    for y in range(HEIGHT):
        lines.append(" ".join(" ".join(str(v) for v in rgb(c))
                              for c in pixels[y * WIDTH:(y + 1) * WIDTH]))
    return "\n".join(lines) + "\n"


def make_preview(source: Source, palette: list[tuple[str, int]]) -> str:
    """Actual source triangles, backface culled, sorted far-to-near, flat lit."""
    view, light = unit((5, 3, 6)), unit((-.4, .9, .7))
    right = unit((view[2], 0, -view[0]))
    up, colors, polygons = cross(view, right), dict(palette), []
    for face in source.faces:
        points = [source.positions[p] for p, _ in face.corners]
        n = unit(normal(points))
        if dot(n, view) <= 0:
            continue
        shade = .48 + .52 * max(0, dot(n, light))
        color = "#" + "".join(f"{round(v * shade):02x}" for v in rgb(colors[face.material]))
        xy = " ".join(f"{500 + 175 * dot(p, right):.2f},{465 - 175 * dot(p, up):.2f}" for p in points)
        depth = sum(dot(p, view) for p in points) / 3
        polygons.append((depth, f'<polygon points="{xy}" fill="{color}" stroke="{color}" stroke-width="0.4"/>'))
    mesh = "\n".join(p for _, p in sorted(polygons, key=lambda item: item[0]))
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="650" viewBox="0 0 1000 650" role="img" aria-labelledby="title desc">
<title id="title">2CV-inspired source mesh: host triangle preview</title>
<desc id="desc">Actual OBJ triangle projection with preview-only flat lighting and painter ordering. Not a Dreamcast render. Overlapping parts may have painter-order artifacts.</desc>
<rect width="1000" height="650" fill="#f4f1e9"/>
<rect x="20" y="20" width="960" height="610" rx="20" fill="none" stroke="#d5d9d4"/>
<g font-family="sans-serif" fill="#263947">
<text x="50" y="65" font-size="12" letter-spacing="2">MAISHUJI LAB / ORIGINAL AUTHORED ASSET</text>
<text x="50" y="112" font-size="40" font-weight="700">Blue, cream, canvas.</text>
<text x="52" y="145" font-size="14">{len(source.faces)} source triangles / +Y up / +Z front / opaque unlit atlas</text>
<ellipse cx="500" cy="480" rx="330" ry="52" fill="#263947" opacity=".06"/>
<g id="actual-model-triangles">
{mesh}
</g>
<path d="M52 560H948" stroke="#cfd5d0"/>
<text x="52" y="592" font-size="13">HOST TRIANGLE PREVIEW — not a target render; lighting is not baked into the atlas.</text>
</g>
</svg>
"""


def make_mesh_header(vertices, indices: Sequence[int]) -> str:
    """Emit converter vertices directly: Q8.8 / 256, UNORM16 UV / 65535, white.

    Nine significant decimal digits round-trip every IEEE-754 binary32 value.
    Explicit floating suffixes avoid narrowing double literals in C++ aggregates.
    Normals remain geometric triangle cross-products, not an extra asset field.
    """
    def literal(value: float) -> str:
        text = format(value, ".9g")
        if "." not in text and "e" not in text:
            text += ".0"
        return text + "f"

    lines = ["#pragma once", "", "#include <array>", "#include <cstdint>",
             '#include "maishuji/mesh.hpp"', "",
             "// Build-generated from car_2cv.obj using the existing converter quantization.",
             "// White base vertex colors; zero offset colors; no DCM1 decode required.",
             "namespace maishuji::car_2cv_mesh {",
             f"inline constexpr std::array<maishuji::TexturedMeshVertex, {len(vertices)}> vertices{{{{"]
    for vertex in vertices:
        position = ", ".join(literal(v / 256) for v in (vertex.x, vertex.y, vertex.z))
        u, v = literal(vertex.u / 65535), literal(vertex.v / 65535)
        lines.append("    maishuji::TexturedMeshVertex{{" + position + "}, " + u + ", " + v +
                     ", {255, 255, 255, 255}, {0, 0, 0, 0}},")
    lines += ["}};", "", f"inline constexpr std::array<std::uint16_t, {len(indices)}> indices{{"]
    for offset in range(0, len(indices), 16):
        lines.append("    " + ", ".join(str(i) for i in indices[offset:offset + 16]) + ",")
    lines += ["};", "} // namespace maishuji::car_2cv_mesh", ""]
    return "\n".join(lines)


def generate(output: Path, source_path: Path = SOURCE) -> tuple[int, int, int]:
    source = read_source(source_path)
    palette = read_palette(source.material_path)
    validate(source, palette)
    converter = load_converter()
    vertices, indices = converter.parse_obj(source_path)
    data = converter.encode(vertices, indices)
    derived = {"car_2cv_model_asset.hpp": converter.format_header(
                   data, "maishuji::car_2cv_model_asset", "bytes"),
               "car_2cv_mesh.hpp": make_mesh_header(vertices, indices),
               "car_2cv_texture.hpp": make_texture(palette),
               "car_2cv_texture.ppm": make_ppm(palette),
               "car_2cv_preview.svg": make_preview(source, palette)}
    inputs = {source_path.resolve(), source.material_path.resolve()}
    if any((output / name).resolve() in inputs for name in derived):
        raise ValueError("output would overwrite a source input")
    output.mkdir(parents=True, exist_ok=True)
    for name, text in derived.items():
        (output / name).write_text(text, encoding="utf-8")
    return len(vertices), len(indices) // 3, len(data)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SOURCE, help="authored OBJ input (never written)")
    parser.add_argument("--output-dir", type=Path, default=SOURCE.parent,
                        help="derived output directory; SVG illustration will be replaced")
    args = parser.parse_args()
    vertices, triangles, size = generate(args.output_dir, args.source)
    print(f"2CV: {vertices} DCM1 vertices, {triangles} triangles, {size} bytes; "
          f"{WIDTH}x{HEIGHT} opaque ARGB4444 atlas -> {args.output_dir}")


if __name__ == "__main__":
    main()
