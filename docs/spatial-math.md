# Spatial math

The public spatial-math header provides the portable 3D calculations used
before geometry reaches the existing PVR primitive boundary. The header
contains no KOS types and performs no allocation.

## Coordinate conventions

The helpers use column vectors and row-major Mat4 storage. Translation is in
the last column. Transform composes scale, XYZ Euler rotation, and translation
in that order.

Camera uses a right-handed view: the camera looks from position toward target,
and visible points have positive clip w after projection. projection_matrix()
produces the usual perspective clip range. ProjectedPoint::normalized_device
is normalized device coordinates (NDC), where x and y are normally in [-1, 1]
for visible geometry and z carries the depth relation.

The math layer does not clip triangles or map NDC to pixels. Those policies
belong at the mesh submission boundary, where the viewport and PVR depth
convention are explicit.

## Example

~~~cpp
#include <maishuji/math.hpp>

const maishuji::Camera camera{
    {0.0f, 1.0f, 3.0f},
    {0.0f, 0.0f, 0.0f},
};
const maishuji::Transform model{
    {0.0f, 0.0f, 0.0f},
    {},
    {1.0f, 1.0f, 1.0f},
};
const maishuji::Mat4 model_view_projection =
    maishuji::view_projection_matrix(camera) *
    maishuji::transform_matrix(model);
const maishuji::ProjectedPoint point =
    maishuji::project_point(model_view_projection, {0.0f, 0.0f, 0.0f});
~~~

The host tests cover constexpr vector/matrix operations, transform order,
camera view placement, and perspective projection. They verify portable
arithmetic only; they do not claim a PVR rasterization result.
