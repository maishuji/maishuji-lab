# Basic 3D example

examples/05-basic-3d.cpp demonstrates the smallest 3D path built on the
existing PVR lifecycle and colored primitive APIs. It creates one indexed
colored cube, applies a model transform, projects each indexed triangle
through a camera, and submits the resulting screen-space triangles.

## Flow

1. MeshVertex stores a world-space position and a vertex color.
2. Transform produces the model matrix for the current frame.
3. Camera produces view and perspective matrices.
4. RenderList::submit(Mesh, ...) performs model-view-projection and viewport
   mapping on the CPU.
5. Each projected triangle enters the existing colored KOS packet path.
6. The example repeats the same mesh for 180 frames, then shuts down cleanly.

The example keeps culling disabled so visibility is determined by the existing
depth path while the triangle winding remains easy to inspect. The viewport is
the fixed 640x480 baseline and the cube stays around the camera target, so the
frame gate can sample a central region without depending on one exact rotation.

## Reproduce

~~~sh
make dreamcast-basic-3d-cdi
make flycast-basic-3d
~~~

The Flycast checker expects a colorful central mesh and a dark surrounding
background. Its exact runtime marker is an additional completion gate, while
the frame checker verifies that projected geometry reached the rendered output.

## Costs made explicit

The first mesh path recomputes projection for every referenced index and
submits every triangle immediately. It uses no temporary transformed-vertex
cache, no heap allocation, and no scene graph. This makes the CPU transform
work and one-header-plus-three-vertices-per-triangle submission cost visible.
Later batching work can measure or change those costs deliberately without
changing the basic ownership boundary.
