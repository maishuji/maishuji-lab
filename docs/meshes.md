# Indexed mesh submission

The mesh boundary converts a small indexed triangle mesh into the existing
colored PVR primitive path. It deliberately does not add a persistent scene
graph, material system, or vertex queue.

~~~cpp
#include <array>

#include <maishuji/mesh.hpp>
#include <maishuji/pvr.hpp>

const std::array<maishuji::MeshVertex, 3> vertices{
    maishuji::MeshVertex{{-0.5f, -0.5f, 0.0f}, {255, 64, 64, 255}},
    maishuji::MeshVertex{{0.5f, -0.5f, 0.0f}, {64, 255, 64, 255}},
    maishuji::MeshVertex{{0.0f, 0.5f, 0.0f}, {64, 128, 255, 255}},
};
const std::array<std::uint16_t, 3> indices{0, 1, 2};
const maishuji::Mesh mesh{vertices, indices};

maishuji::Camera camera{};
maishuji::Transform transform{};
maishuji::Viewport viewport{640.0f, 480.0f};
list.submit(mesh, camera, transform, viewport);
~~~

The mesh owns no storage: its spans must remain valid for the duration of the
submission call. Every three indices form one triangle. An empty mesh,
non-triangular index count, out-of-range index, or invalid viewport returns
MeshInvalidData.

Each vertex is transformed by model, view, and perspective matrices.
Camera validity is checked before any submission. The resulting triangle is
then clipped against all six homogeneous clip planes. A fully outside or
behind-camera triangle is discarded; a partially visible triangle is
triangulated after clipping. The normalized device result maps to the existing
PVR screen convention: x increases right, y increases down, and depth maps
from NDC [-1, 1] to the positive [0, 1] range used by the current examples.
The mapping is reversed for PVR depth ordering: nearer geometry receives the
larger submitted value, farther geometry the smaller value, matching the
default DepthCompare::Greater policy.

## Recorded 3D depth-ordering finding

On 2026-09-28, review of the fogged cube found that the first projection path
mapped nearer geometry to smaller submitted z values while the explicit default
policy used DepthCompare::Greater. That combination could let the far cube face
win the depth test. The existing color/background checker did not detect this
because it only established that a cube-shaped image was visible.

The correction keeps the pinned KOS-compatible Greater comparison and reverses
the mesh viewport depth mapping:

- the near plane maps to the larger submitted value;
- the far plane maps to the smaller submitted value.

The host lifecycle test now submits overlapping near and far triangles and
asserts that the near triangle receives the larger PVR depth. The fogged
example also uses a static capture pose with a blue near face and red far face;
its Flycast checker samples the center and requires blue to dominate, covering
the visible occlusion case.

Reproduce the corrected checks with:

~~~sh
make host-test
make dreamcast-fogged-3d-cdi
make flycast-fogged-3d
~~~

Observed corrected Flycast result:

~~~text
PASS: Flycast rendered the fogged 3D mesh with near-face depth ordering.
  mesh center: 0.081 0.154 0.485
  above mesh:  0.020 0.020 0.000
  left mesh:   0.020 0.020 0.000
  right mesh:  0.020 0.020 0.000
  below mesh:  0.020 0.020 0.000
Stable frames: 2
Runtime probes: render-gated
Required runtime marker: passed
~~~

The emulator check validates this fixed fixture and the runtime marker. It does
not prove every possible winding, camera, or mesh ordering case; arbitrary
scenes still need appropriate culling and depth policy choices.

## Cost and boundary

The first implementation projects every referenced vertex for every triangle,
so shared indexed vertices are intentionally recomputed. This keeps the
per-triangle CPU work visible and avoids a temporary transformed-vertex cache
or heap allocation. Each projected triangle then uses the same aligned KOS
packet path as RenderList::submit(Triangle): one polygon header and three
vertex packets.

A failed backend submission can occur after earlier triangles in the same
mesh have already been submitted. Camera and index validation happen before
triangle submission, but the backend is still allowed to fail part-way through
a clipped mesh. Callers that need all-or-nothing batching should validate their
mesh and use a separate recording or command-building layer; this API does not
pretend to provide transactional submission.

## KOS mapping

Mesh projection is a CPU-side calculation. The KOS backend receives the same
screen-space pvr_vertex_t packets used by the colored Triangle API; it does not
receive a maishuji Mesh or a matrix object.

For each indexed triangle, the path:

1. Applies model, view, and perspective matrices on the SH-4-side submission
   code.
2. Performs perspective division and maps NDC to the 640x480 viewport.
3. Packs the resulting position and color into the existing PVR vertex packet.
4. Compiles the active list's polygon header and submits one header plus three
   vertices through pvr_prim().

Culling and depth policy remain PrimitiveConfiguration choices passed to the
same KOS polygon context. The default is no culling, DepthCompare::Greater,
and depth writes enabled; this matches the pinned KOS context while making the
choice visible in the public API. Use a different policy when the mesh's
winding or layering requires it. The mesh API therefore adds CPU transform work without
hiding list selection, packet layout, or synchronization behind a generic
renderer. Optional camera-space linear fog is documented in
[fog.md](fog.md); it blends mesh vertex colors before this same clipping and
submission path.
