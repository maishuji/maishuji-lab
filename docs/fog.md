# Camera-space linear fog

The mesh path supports optional CPU-side linear fog through maishuji::Fog.
Fog is applied to each mesh vertex after the model and view transforms, using
positive camera-space depth (-view_position.z) before clipping and PVR
submission.

~~~cpp
const maishuji::Fog fog{
    {6, 8, 24, 255}, // fog color
    2.5f,            // no fog before this camera-space depth
    4.75f,           // full fog at and beyond this depth
    true,            // enabled
};

list.submit_fogged(mesh, camera, transform, {640.0f, 480.0f}, fog);
~~~

The blend is linear between start and end. The fog color replaces the vertex
RGB channels at full strength; alpha is preserved. Clipped vertices
interpolate the already-fogged colors, so a partially visible triangle remains
consistent with the frustum path. A disabled fog configuration returns the
original mesh colors. An enabled configuration requires finite, non-negative
start and end > start; invalid settings return Status::InvalidFog before
any triangle is submitted.

This is an intentional CPU-side teaching path. It keeps the fog range,
camera-space convention, and per-vertex color cost visible without introducing
global renderer state. It adds one model-view transform and one color blend per
source mesh vertex; it does not allocate a fog table or a second primitive
queue.

## Example and validation

examples/06-fogged-3d.cpp rotates the indexed cube from the basic 3D example
through a blue-tinted fog range. Build and run its deterministic emulator
check with:

~~~sh
make dreamcast-fogged-3d-cdi
make flycast-fogged-3d
~~~

The runtime marker reports 180 frames and the checker holds a static final
cube pose whose near face is blue and far face is red. It expects the near blue
face to remain visible at the center, so the check covers depth ordering as
well as fogged output. This validates the target build and emulator-visible
output; it does not claim timing measurements or physical console validation.
