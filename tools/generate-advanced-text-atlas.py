#!/usr/bin/env python3
"""Generate the small ARGB4444 atlas used by the advanced text lesson."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import shutil
import subprocess
import sys


ATLAS_WIDTH = 256
ATLAS_HEIGHT = 128
CELL_SIZE = 32
COLUMNS = ATLAS_WIDTH // CELL_SIZE
POINT_SIZE = 26


@dataclass(frozen=True)
class GlyphSpec:
    character: str
    font: str
    advance: int


# The atlas contains only the glyphs used by the three fixed lesson strings.
GLYPHS = (
    GlyphSpec("ケ", "Noto-Sans-CJK-JP", 32),
    GlyphSpec("ー", "Noto-Sans-CJK-JP", 32),
    GlyphSpec("キ", "Noto-Sans-CJK-JP", 32),
    GlyphSpec("は", "Noto-Sans-CJK-JP", 32),
    GlyphSpec("嘘", "Noto-Sans-CJK-JP", 32),
    GlyphSpec("だ", "Noto-Sans-CJK-JP", 32),
    GlyphSpec("。", "Noto-Sans-CJK-JP", 32),
    GlyphSpec("蛋", "Noto-Sans-CJK-TC", 32),
    GlyphSpec("糕", "Noto-Sans-CJK-TC", 32),
    GlyphSpec("是", "Noto-Sans-CJK-TC", 32),
    GlyphSpec("個", "Noto-Sans-CJK-TC", 32),
    GlyphSpec("謊", "Noto-Sans-CJK-TC", 32),
    GlyphSpec("言", "Noto-Sans-CJK-TC", 32),
    GlyphSpec("T", "Noto-Sans", 12),
    GlyphSpec("h", "Noto-Sans", 12),
    GlyphSpec("e", "Noto-Sans", 12),
    GlyphSpec("c", "Noto-Sans", 12),
    GlyphSpec("a", "Noto-Sans", 12),
    GlyphSpec("k", "Noto-Sans", 12),
    GlyphSpec("i", "Noto-Sans", 12),
    GlyphSpec("s", "Noto-Sans", 12),
    GlyphSpec("l", "Noto-Sans", 12),
    GlyphSpec(".", "Noto-Sans", 12),
)


def render_glyph(convert: str, glyph: GlyphSpec) -> bytes:
    command = [
        convert,
        "-background",
        "none",
        "-fill",
        "white",
        "-font",
        glyph.font,
        "-pointsize",
        str(POINT_SIZE),
        "-gravity",
        "center",
        f"label:{glyph.character}",
        "-extent",
        f"{CELL_SIZE}x{CELL_SIZE}",
        "-depth",
        "8",
        "rgba:-",
    ]
    result = subprocess.run(command, check=False, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    if result.returncode != 0:
        error = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"could not render {glyph.character!r}: {error}")

    expected_bytes = CELL_SIZE * CELL_SIZE * 4
    if len(result.stdout) != expected_bytes:
        raise RuntimeError(
            f"unexpected pixel size for {glyph.character!r}: "
            f"{len(result.stdout)} != {expected_bytes}"
        )
    return result.stdout


def to_argb4444(rgba: bytes) -> list[int]:
    pixels: list[int] = []
    for index in range(0, len(rgba), 4):
        alpha = (rgba[index + 3] * 15 + 127) // 255
        pixels.append((alpha << 12) | 0x0FFF)
    return pixels


def format_values(values: list[int], per_line: int = 12) -> str:
    lines = []
    for start in range(0, len(values), per_line):
        row = values[start:start + per_line]
        lines.append("    " + ", ".join(f"0x{value:04x}" for value in row))
    return ",\n".join(lines)


def generate(output: Path) -> None:
    convert = shutil.which("convert")
    if convert is None:
        raise RuntimeError("ImageMagick 'convert' is required")
    if len(GLYPHS) > (ATLAS_WIDTH // CELL_SIZE) * (ATLAS_HEIGHT // CELL_SIZE):
        raise RuntimeError("the glyph list does not fit in the atlas")

    atlas = [0] * (ATLAS_WIDTH * ATLAS_HEIGHT)
    metadata = []
    for index, glyph in enumerate(GLYPHS):
        cell_x = (index % COLUMNS) * CELL_SIZE
        cell_y = (index // COLUMNS) * CELL_SIZE
        pixels = to_argb4444(render_glyph(convert, glyph))
        for y in range(CELL_SIZE):
            destination = (cell_y + y) * ATLAS_WIDTH + cell_x
            source = y * CELL_SIZE
            atlas[destination:destination + CELL_SIZE] = pixels[source:source + CELL_SIZE]
        metadata.append((ord(glyph.character), cell_x, cell_y, glyph.advance))

    output.parent.mkdir(parents=True, exist_ok=True)
    glyph_rows = ",\n".join(
        f"    {{{codepoint}, {cell_x}, {cell_y}, {advance}}}"
        for codepoint, cell_x, cell_y, advance in metadata
    )
    header_lines = [
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstdint>",
        "",
        "namespace maishuji::advanced_text_asset {",
        "",
        f"inline constexpr std::uint16_t atlas_width = {ATLAS_WIDTH};",
        f"inline constexpr std::uint16_t atlas_height = {ATLAS_HEIGHT};",
        f"inline constexpr std::uint16_t cell_size = {CELL_SIZE};",
        "",
        "struct Glyph {",
        "    std::uint32_t codepoint;",
        "    std::uint16_t cell_x;",
        "    std::uint16_t cell_y;",
        "    std::uint16_t advance;",
        "};",
        "",
        f"inline constexpr std::array<Glyph, {len(metadata)}> glyphs{{{{",
        glyph_rows,
        "}};",
        "",
        "// Generated from the open-source Noto CJK and Noto Sans fonts.",
        "alignas(32) inline constexpr std::array<std::uint16_t,",
        "                                      atlas_width * atlas_height>",
        "    pixels{{",
        format_values(atlas),
        "}};",
        "",
        "} // namespace maishuji::advanced_text_asset",
        "",
    ]
    output.write_text("\n".join(header_lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("advanced_examples/assets/multilingual_font.hpp"),
    )
    args = parser.parse_args()
    try:
        generate(args.output)
    except (OSError, RuntimeError) as error:
        print(f"atlas generation failed: {error}", file=sys.stderr)
        return 1
    print(f"generated {args.output} ({len(GLYPHS)} glyphs; {ATLAS_WIDTH}x{ATLAS_HEIGHT})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
