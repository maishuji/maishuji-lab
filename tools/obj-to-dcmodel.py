#!/usr/bin/env python3
"""Convert a deliberately small Wavefront OBJ subset to an embedded DCM1 blob.

The target loader is intentionally not an OBJ or glTF parser.  This tool is the
offline conversion step: it triangulates faces, duplicates vertices at UV
seams, quantizes positions to signed Q8.8, and emits a C++ byte-array header.
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


HEADER_SIZE = 24
VERTEX_SIZE = 16
POSITION_SCALE = 256.0
MAX_UINT16 = 0xFFFF


@dataclass(frozen=True)
class Vertex:
    x: int
    y: int
    z: int
    u: int
    v: int
    argb: int = 0xFFFFFFFF


def parse_index(value: str, count: int, label: str) -> int:
    try:
        index = int(value)
    except ValueError as error:
        raise ValueError(f"invalid {label} index {value!r}") from error
    if index == 0:
        raise ValueError(f"OBJ {label} index zero is not supported")
    resolved = index if index > 0 else count + index + 1
    if not 1 <= resolved <= count:
        raise ValueError(f"OBJ {label} index {index} is outside 1..{count}")
    return resolved - 1


def quantize_position(value: str, axis: str) -> int:
    try:
        quantized = round(float(value) * POSITION_SCALE)
    except ValueError as error:
        raise ValueError(f"invalid {axis} position {value!r}") from error
    if not -32768 <= quantized <= 32767:
        raise ValueError(
            f"{axis} position {value!r} does not fit signed Q8.8"
        )
    return quantized


def quantize_uv(value: str, axis: str) -> int:
    try:
        coordinate = float(value)
    except ValueError as error:
        raise ValueError(f"invalid {axis} UV coordinate {value!r}") from error
    if not 0.0 <= coordinate <= 1.0:
        raise ValueError(
            f"{axis} UV coordinate {coordinate} is outside the supported 0..1 range"
        )
    return max(0, min(MAX_UINT16, round(coordinate * MAX_UINT16)))


def parse_obj(path: Path) -> tuple[list[Vertex], list[int]]:
    positions: list[tuple[str, str, str]] = []
    texcoords: list[tuple[str, str]] = []
    vertices: list[Vertex] = []
    indices: list[int] = []
    vertex_lookup: dict[tuple[int, int], int] = {}

    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        fields = raw_line.split("#", 1)[0].split()
        if not fields:
            continue
        record = fields[0]
        try:
            if record == "v":
                if len(fields) != 4:
                    raise ValueError("a position must have exactly three coordinates")
                positions.append((fields[1], fields[2], fields[3]))
            elif record == "vt":
                if len(fields) not in (2, 3):
                    raise ValueError("a texture coordinate must have U and optional V")
                texcoords.append((fields[1], fields[2] if len(fields) == 3 else "0"))
            elif record == "f":
                if len(fields) < 4:
                    raise ValueError("a face must contain at least three corners")
                corners: list[int] = []
                for token in fields[1:]:
                    pieces = token.split("/")
                    if len(pieces) < 2 or not pieces[1]:
                        raise ValueError(
                            "faces must use v/vt or v/vt/vn corners"
                        )
                    position_index = parse_index(
                        pieces[0], len(positions), "position"
                    )
                    texcoord_index = parse_index(
                        pieces[1], len(texcoords), "texture-coordinate"
                    )
                    key = (position_index, texcoord_index)
                    if key not in vertex_lookup:
                        px, py, pz = positions[position_index]
                        tu, tv = texcoords[texcoord_index]
                        vertex_lookup[key] = len(vertices)
                        vertices.append(
                            Vertex(
                                quantize_position(px, "x"),
                                quantize_position(py, "y"),
                                quantize_position(pz, "z"),
                                quantize_uv(tu, "u"),
                                quantize_uv(str(1.0 - float(tv)), "v"),
                            )
                        )
                    corners.append(vertex_lookup[key])
                for corner in range(1, len(corners) - 1):
                    indices.extend((corners[0], corners[corner], corners[corner + 1]))
            elif record in {"o", "g", "s", "usemtl", "mtllib"}:
                continue
            else:
                raise ValueError(f"unsupported OBJ record {record!r}")
        except ValueError as error:
            raise ValueError(f"{path}:{line_number}: {error}") from error

    if not vertices:
        raise ValueError(f"{path}: no textured vertices were found")
    if not indices:
        raise ValueError(f"{path}: no faces were found")
    if len(vertices) > MAX_UINT16 or len(indices) > MAX_UINT16:
        raise ValueError("DCM1 stores vertex and index counts as uint16")
    if max(indices) >= len(vertices):
        raise ValueError("internal error: generated index is out of range")
    return vertices, indices


def encode(vertices: list[Vertex], indices: list[int]) -> bytes:
    vertex_offset = HEADER_SIZE
    index_offset = vertex_offset + len(vertices) * VERTEX_SIZE
    file_size = index_offset + len(indices) * 2
    header = struct.pack(
        "<4sHHHHIII",
        b"DCM1",
        1,
        1,
        len(vertices),
        len(indices),
        vertex_offset,
        index_offset,
        file_size,
    )
    payload = bytearray(header)
    for vertex in vertices:
        payload.extend(
            struct.pack(
                "<hhhHHIH",
                vertex.x,
                vertex.y,
                vertex.z,
                vertex.u,
                vertex.v,
                vertex.argb,
                0,
            )
        )
    payload.extend(struct.pack(f"<{len(indices)}H", *indices))
    assert len(payload) == file_size
    return bytes(payload)


def format_header(data: bytes, namespace: str, symbol: str) -> str:
    namespace_parts = namespace.split("::")
    opening = "\n".join(f"namespace {part} {{" for part in namespace_parts)
    closing = "\n".join(f"}} // namespace {part}" for part in reversed(namespace_parts))
    lines = [
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstdint>",
        "",
        opening,
        "",
        f"inline constexpr std::array<std::uint8_t, {len(data)}> {symbol}{{",
    ]
    for offset in range(0, len(data), 16):
        chunk = data[offset : offset + 16]
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    lines.extend(["};", "", closing, ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="textured OBJ source")
    parser.add_argument("output", type=Path, help="generated C++ header")
    parser.add_argument("--namespace", default="maishuji::model_asset")
    parser.add_argument("--symbol", default="bytes")
    args = parser.parse_args()

    try:
        vertices, indices = parse_obj(args.input)
        data = encode(vertices, indices)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            format_header(data, args.namespace, args.symbol), encoding="utf-8"
        )
    except (OSError, ValueError, struct.error) as error:
        print(f"obj-to-dcmodel: {error}", file=sys.stderr)
        return 1

    print(
        f"{args.input}: {len(vertices)} vertices, {len(indices) // 3} triangles, "
        f"{len(data)} bytes -> {args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
