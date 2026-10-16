"""Host asset checks: python3 -B -m unittest discover -s tests -p test_2cv_assets.py -v.

The checked-in OBJ/MTL are source inputs. Only source files and the existing
atlas header are required: all DCM1/PPM/triangle-preview outputs are generated
in temporary directories. No runtime, emulator, image library, or KOS required.
"""

from __future__ import annotations

import importlib.util
import re
import struct
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "advanced_examples" / "assets"


def load_python(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    previous = sys.dont_write_bytecode
    try:
        sys.dont_write_bytecode = True
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = previous
    return module


GENERATOR = load_python("test_car_2cv_generator", ROOT / "tools" / "generate-2cv-model.py")
CONVERTER = GENERATOR.load_converter()


class Car2cvAssetsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = GENERATOR.read_source(ASSETS / "car_2cv.obj")
        cls.palette = GENERATOR.read_palette(cls.source.material_path)
        cls.vertices, cls.indices = CONVERTER.parse_obj(ASSETS / "car_2cv.obj")

    def points(self, face):
        return [self.source.positions[p] for p, _ in face.corners]

    def assert_triangle_reflection(self, source):
        """Compare per-material triangle multisets, ignoring winding (checked separately).

        The only fallback is a central coplanar patch with a symmetric boundary:
        e.g. a full-width rectangular underside cannot have reflection-invariant
        triangles without adding center vertices. Nonplanar/off-center facets get
        no exemption. Existing centerline cabin/bonnet crown fans match exactly.
        """
        def key(points):
            return tuple(sorted(points))

        def reflected(points):
            return [(-x, y, z) for x, y, z in points]

        triangles, mirror = Counter(), Counter()
        planar, membership = defaultdict(list), {}
        central_features = {"lower_body", "arched_cabin", "curved_bonnet",
                            "front_bumper", "rear_bumper", "grille", "grille_surround", "grille_slats"}
        for face in source.faces:
            points = [source.positions[p] for p, _ in face.corners]
            triangle = (face.material, key(points))
            triangles[triangle] += 1
            mirror[(face.material, key(reflected(points)))] += 1
            n = GENERATOR.unit(GENERATOR.normal(points))
            if face.feature not in central_features or abs(n[0]) > 1e-10:
                continue
            if next(v for v in n if abs(v) > 1e-10) < 0:
                n = tuple(-v for v in n)
            plane = tuple(round(v, 8) for v in (*n, GENERATOR.dot(n, points[0])))
            group = (face.feature, face.material, plane)
            planar[group].append(points)
            membership[triangle] = group

        checked = set()
        for triangle in triangles - mirror:
            self.assertIn(triangle, membership,
                          f"triangle has no reflected counterpart: {triangle}")
            group = membership[triangle]
            if group in checked:
                continue
            checked.add(group)
            edges = Counter()
            for a, b, c in planar[group]:
                edges.update((key((a, b)), key((b, c)), key((c, a))))
            self.assertTrue(all(count in (1, 2) for count in edges.values()), group)
            boundary = Counter({edge: count for edge, count in edges.items() if count == 1})
            self.assertTrue(boundary, group)
            xs = [p[0] for edge in boundary for p in edge]
            self.assertTrue(min(xs) < 0 < max(xs), "fallback must span the centerline")
            self.assertEqual(boundary, Counter({key(reflected(edge)): count
                                               for edge, count in boundary.items()}), group)

    def test_per_material_triangle_geometry_is_reflection_symmetric(self):
        self.assert_triangle_reflection(self.source)

    def test_triangle_reflection_rejects_original_nonplanar_diagonal_bug(self):
        # First left rear shoulder quad: undo only its diagonal, not its vertices.
        faces = list(self.source.faces)
        first, second = faces[8:10]
        self.assertEqual(first.feature, "lower_body")
        a, b, d = first.corners
        b2, c, d2 = second.corners
        self.assertEqual((b2, d2), (b, d))
        points = [self.source.positions[p] for p, _ in (a, b, c, d)]
        self.assertGreater(abs(GENERATOR.dot(GENERATOR.normal(points[:3]),
                                             GENERATOR.sub(points[3], points[0]))), 1e-8)
        faces[8] = first._replace(corners=(a, b, c))
        faces[9] = second._replace(corners=(a, c, d))
        with self.assertRaises(AssertionError):
            self.assert_triangle_reflection(self.source._replace(faces=faces))

    def test_deterministic_derivation_preserves_authored_inputs(self):
        with tempfile.TemporaryDirectory(prefix="car-2cv-assets-") as tmp:
            scratch = Path(tmp)
            # Scratch copies also verify that same-directory generation leaves sources alone.
            before = {n: (ASSETS / n).read_bytes() for n in ("car_2cv.obj", "car_2cv.mtl")}
            for name, data in before.items():
                (scratch / name).write_bytes(data)
            first, second = scratch, scratch / "second"
            counts = GENERATOR.generate(first, scratch / "car_2cv.obj")
            self.assertEqual(counts, GENERATOR.generate(second, scratch / "car_2cv.obj"))
            self.assertEqual(counts[:2], (len(self.vertices), len(self.source.faces)))
            for name in GENERATOR.OUTPUTS:
                self.assertEqual((first / name).read_bytes(), (second / name).read_bytes(), name)
            for name, data in before.items():
                self.assertEqual((scratch / name).read_bytes(), data)
                self.assertEqual((ASSETS / name).read_bytes(), data)
            self.assertEqual(sorted(p.name for p in second.iterdir()), sorted(GENERATOR.OUTPUTS))
            self.assertEqual((first / "car_2cv_texture.hpp").read_bytes(),
                             (ASSETS / "car_2cv_texture.hpp").read_bytes())
            encoded = CONVERTER.encode(self.vertices, self.indices)
            header = (first / "car_2cv_model_asset.hpp").read_text(encoding="utf-8")
            self.assertEqual(bytes(int(h, 16) for h in re.findall(r"0x([0-9a-f]{2})", header)), encoded)
            self.assertIn("namespace car_2cv_model_asset", header)
            self.assertEqual((first / "car_2cv_mesh.hpp").read_text(encoding="utf-8"),
                             GENERATOR.make_mesh_header(self.vertices, self.indices))
            ppm = (first / "car_2cv_texture.ppm").read_text(encoding="utf-8")
            tokens = " ".join(line.split("#", 1)[0] for line in ppm.splitlines()).split()
            self.assertEqual(tokens[:4], ["P3", "64", "32", "255"])
            expected = [v for c in GENERATOR.atlas_pixels(self.palette) for v in GENERATOR.rgb(c)]
            self.assertEqual([int(v) for v in tokens[4:]], expected)

    def test_triangle_counts_indices_and_dcm1_layout(self):
        self.assertTrue(250 <= len(self.source.faces) <= 500)
        self.assertEqual(len(self.indices), len(self.source.faces) * 3)
        self.assertLessEqual(len(self.vertices), 65535)
        self.assertLessEqual(len(self.indices), 65535)
        self.assertEqual(set(self.indices), set(range(len(self.vertices))))
        for face in self.source.faces:
            self.assertEqual(len(face.corners), 3)
            self.assertIn(face.material, dict(self.palette))
            for p, uv in face.corners:
                self.assertTrue(0 <= p < len(self.source.positions))
                self.assertTrue(0 <= uv < len(self.source.uvs))
        data = CONVERTER.encode(self.vertices, self.indices)
        fields = struct.unpack_from("<4sHHHHIII", data)
        self.assertEqual(fields[:5], (b"DCM1", 1, 1, len(self.vertices), len(self.indices)))
        self.assertEqual(fields[5:], (24, 24 + 16 * len(self.vertices), len(data)))
        self.assertTrue(all(v.argb == 0xFFFFFFFF for v in self.vertices), "no baked vertex lighting")

    def test_mesh_header_matches_converter_quantization_and_indices(self):
        header = GENERATOR.make_mesh_header(self.vertices, self.indices)
        for include in ('<array>', '<cstdint>', '"maishuji/mesh.hpp"'):
            self.assertIn(f"#include {include}", header)
        self.assertIn("namespace maishuji::car_2cv_mesh", header)
        self.assertIn(f"std::array<maishuji::TexturedMeshVertex, {len(self.vertices)}> vertices", header)
        self.assertIn(f"std::array<std::uint16_t, {len(self.indices)}> indices", header)
        rows = re.findall(
            r"^    maishuji::TexturedMeshVertex\{\{([^}]+)\}, ([^,]+), ([^,]+), "
            r"\{255, 255, 255, 255\}, \{0, 0, 0, 0\}\},$", header, re.MULTILINE)
        self.assertEqual(len(rows), len(self.vertices))
        for row, vertex in zip(rows, self.vertices):
            tokens = row[0].split(", ") + list(row[1:])
            expected = (vertex.x / 256, vertex.y / 256, vertex.z / 256,
                        vertex.u / 65535, vertex.v / 65535)
            for token, value in zip(tokens, expected):
                self.assertTrue(token.endswith("f"), token)
                self.assertEqual(struct.pack("<f", float(token[:-1])), struct.pack("<f", value))
        block = re.search(r"indices\{\n(.*?)\n\};", header, re.DOTALL)
        self.assertIsNotNone(block)
        if block is not None:
            self.assertEqual([int(i) for i in re.findall(r"\d+", block[1])], self.indices)

    def test_nondegeneracy_and_quantization_preserve_geometric_normals(self):
        GENERATOR.validate(self.source, self.palette)
        for i, face in enumerate(self.source.faces):
            with self.subTest(triangle=i, feature=face.feature):
                n = GENERATOR.normal(self.points(face))
                self.assertGreater(GENERATOR.dot(n, n), 1e-14)
                vertices = [self.vertices[j] for j in self.indices[i * 3:i * 3 + 3]]
                qn = GENERATOR.normal([(v.x, v.y, v.z) for v in vertices])
                self.assertGreater(GENERATOR.dot(qn, qn), 0)
                self.assertGreater(GENERATOR.dot(n, qn), 0)

    def test_closed_components_are_manifold_and_outward(self):
        groups = defaultdict(list)
        for face in self.source.faces:
            groups[face.feature].append(face)
        closed = {"lower_body", "arched_cabin", "curved_bonnet", "front_bumper", "rear_bumper"}
        closed.update(name for name in groups if name.endswith(("_wheel", "_headlight")))
        for name in sorted(closed):
            with self.subTest(component=name):
                directed, volume6 = Counter(), 0.0
                for face in groups[name]:
                    a, b, c = self.points(face)
                    volume6 += GENERATOR.dot(a, GENERATOR.cross(b, c))
                    a, b, c = [p for p, _ in face.corners]
                    directed.update(((a, b), (b, c), (c, a)))
                for (a, b), count in directed.items():
                    self.assertEqual(count, 1, "nonmanifold edge")
                    self.assertEqual(directed[(b, a)], 1, "inconsistent adjacent winding")
                self.assertGreater(volume6, 0, "inside-out component")

    def test_open_patches_face_outward(self):
        for face in self.source.faces:
            name, expected = face.feature, None
            if name.endswith(("_door_window", "_hubcap")):
                expected = (-1, 0, 0) if name.startswith("left_") else (1, 0, 0)
            elif name.endswith("_wing") or name == "canvas_roof":
                expected = (0, 1, 0)
            elif name in {"grille", "grille_surround", "grille_slats"}:
                expected = (0, 0, 1)
            elif name.endswith("_taillight"):
                expected = (0, 0, -1)
            elif name in {"windscreen", "rear_window"}:
                expected = (0, 1, 1 if name == "windscreen" else -1)
            if expected is not None:
                self.assertGreater(GENERATOR.dot(GENERATOR.normal(self.points(face)), expected), 0, name)

    def test_named_features_symmetry_dimensions_and_ground_contact(self):
        groups, materials = defaultdict(set), defaultdict(set)
        for face in self.source.faces:
            groups[face.feature].update(self.points(face))
            materials[face.material].update(self.points(face))
        required = {"lower_body", "arched_cabin", "curved_bonnet", "canvas_roof", "windscreen",
                    "rear_window", "grille", "grille_slats", "front_bumper", "rear_bumper"}
        for side in ("left", "right"):
            required.update(f"{side}_{end}_{part}" for end in ("front", "rear")
                            for part in ("wheel", "hubcap", "wing", "door_window"))
            required.update((f"{side}_headlight", f"{side}_taillight"))
        self.assertTrue(required.issubset(groups), required - groups.keys())
        for points in materials.values():
            self.assertEqual(points, {(-x, y, z) for x, y, z in points})
        positions = self.source.positions
        dimensions = [max(p[a] for p in positions) - min(p[a] for p in positions) for a in range(3)]
        for actual, expected in zip(dimensions, (1.54, 1.612, 3.80)):
            self.assertAlmostEqual(actual, expected, delta=.01)
        self.assertEqual(min(p[1] for p in positions), 0)
        for side in ("left", "right"):
            for end in ("front", "rear"):
                points = groups[f"{side}_{end}_wheel"]
                self.assertEqual(min(p[1] for p in points), 0)
                self.assertEqual(min(round(p[1] * 256) for p in points), 0)
                self.assertAlmostEqual(max(p[0] for p in points) - min(p[0] for p in points), .13)
                self.assertEqual(len({p[1:] for p in points}), 13, "12 tire samples plus cap center")
        self.assertGreater(min(p[2] for p in groups["grille"]), 0, "+Z is front")

    def test_uvs_atlas_source_colors_and_checked_in_header_agree(self):
        atlas = GENERATOR.atlas_pixels(self.palette)
        self.assertEqual(len(atlas), 64 * 32)
        self.assertTrue(all(c >> 12 == 15 for c in atlas))
        self.assertEqual(len(set(atlas)), 8)
        self.assertEqual(GENERATOR.make_texture(self.palette),
                         (ASSETS / "car_2cv_texture.hpp").read_text(encoding="utf-8"))
        for i, face in enumerate(self.source.faces):
            expected = dict(self.palette)[face.material]
            for corner, (_, uv) in enumerate(face.corners):
                u, v = self.source.uvs[uv]
                vertex = self.vertices[self.indices[i * 3 + corner]]
                self.assertEqual(vertex.u, round(u * 65535))
                self.assertEqual(vertex.v, round((1 - v) * 65535))
                x, y = int(vertex.u / 65535 * 64), int(vertex.v / 65535 * 32)
                for dx, dy in ((0, 0), (-3, 0), (3, 0), (0, -3), (0, 3)):
                    self.assertEqual(atlas[(y + dy) * 64 + x + dx], expected)

    def test_derived_preview_matches_visible_source_triangles(self):
        svg = GENERATOR.make_preview(self.source, self.palette)
        root = ET.fromstring(svg)
        ns = {"s": "http://www.w3.org/2000/svg"}
        polygons = root.findall(".//s:g[@id='actual-model-triangles']/s:polygon", ns)
        view = GENERATOR.unit((5, 3, 6))
        visible = sum(GENERATOR.dot(GENERATOR.normal(self.points(f)), view) > 0
                      for f in self.source.faces)
        self.assertEqual(len(polygons), visible)
        self.assertGreater(visible, 100)
        for polygon in polygons:
            points = polygon.attrib["points"].split()
            self.assertEqual(len(points), 3)
            for p in points:
                x, y = map(float, p.split(","))
                self.assertTrue(35 < x < 965 and 150 < y < 550, (x, y))
        self.assertIn("Not a Dreamcast render", svg)
        design = (ASSETS / "car_2cv_preview.svg").read_text(encoding="utf-8")
        ET.fromstring(design)
        self.assertTrue("DESIGN ILLUSTRATION" in design or "HOST TRIANGLE PREVIEW" in design)
        self.assertNotIn("<image", design, "no downloaded raster assets")


if __name__ == "__main__":
    unittest.main()
