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

Each vertex is transformed by model, view, and perspective matrices. Positive
clip w is required. The normalized device result maps to the existing PVR
screen convention: x increases right, y increases down, and depth maps from
NDC [-1, 1] to the positive [0, 1] range used by the current examples.
Points outside the viewport are still submitted; the PVR remains responsible
for its normal raster clipping. Geometry behind the camera returns
MeshProjectionFailed.

## Cost and boundary

The first implementation projects every referenced vertex for every triangle,
so shared indexed vertices are intentionally recomputed. This keeps the
per-triangle CPU work visible and avoids a temporary transformed-vertex cache
or heap allocation. Each projected triangle then uses the same aligned KOS
packet path as RenderList::submit(Triangle): one polygon header and three
vertex packets.

A failed backend submission can occur after earlier triangles in the same mesh
have already been submitted. Callers that need all-or-nothing batching should
validate their mesh and use a separate recording or command-building layer;
this API does not pretend to provide transactional submission.
