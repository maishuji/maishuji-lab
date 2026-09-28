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

Culling remains a PrimitiveConfiguration choice passed to the same KOS polygon
context. The mesh API therefore adds CPU transform work without hiding list
selection, packet layout, or synchronization behind a generic renderer.
