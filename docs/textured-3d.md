# Textured 3D cube

`advanced_examples/03-textured-3d.cpp` combines the PVR asset lesson with the
indexed 3D mesh path. It loads the generated `pvr_asset.hpp` controller image,
decodes its square-twiddled ARGB4444 payload once, uploads one 256x256 texture,
and maps that texture onto a rotating cube.

## What the example teaches

The cube uses 24 vertices rather than eight shared vertices so each face can
have its own UV orientation. Its 36 indices describe 12 triangles. The public
`TexturedMeshVertex` and `TexturedMesh` views add UV coordinates to the existing
span-backed indexed mesh boundary; `RenderList::submit` applies the same
model-view-projection and homogeneous clipping path as the colored mesh, then
submits textured triangle packets through the active PVR list.

The frame has two explicit list scopes:

1. An opaque dark-blue background quad.
2. A punch-through textured cube, using the texture alpha so transparent
   controller pixels reveal the background.

The default primitive configuration keeps depth comparison as `Greater`, depth
writes enabled, and culling disabled. The camera and CPU transform remain
visible in the source so the example shows where the Dreamcast-side work is
performed.

## Cost and lifetime

The embedded PVRT fixture is 131,088 bytes: a 16-byte header followed by a
256x256 ARGB4444 payload. The uploaded texture therefore consumes 131,072
bytes of texel storage. The texture is allocated once, uploaded once, reused
for every frame, and released before PVR shutdown.

Each cube frame submits 12 polygon headers and 36 textured vertices. The
indexed API intentionally projects each triangle reference again; it does not
add a transformed-vertex cache, scene graph, or material system. The 180-frame
animation and held capture pose make the rendering behavior observable without
making frame timing part of the lesson.

## Build and validate

Build the CDI with the pinned KOS container, then run the host-side Flycast
capture check:

~~~sh
make dreamcast-textured-3d-cdi
make flycast-textured-3d
~~~

The validator waits for the exact guest marker
`maishuji: textured 3D passed (PVRT ARGB4444; rotating cube; 12 triangles)`,
captures the fixed 640x480 game window, and requires three consecutive stable
frames. Its ImageMagick checks cover a visible, varying cube, variation at the
cube center, and the dark-blue background at all four outside samples. The
capture hold uses a fixed pose so the pixel assertions do not race the
animation.

The check is evidence that this fixture rendered correctly in the tested
Flycast setup. It does not replace validation on a physical Dreamcast, and it
does not prove every camera, winding, texture format, or mesh configuration.
