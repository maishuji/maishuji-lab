# Colored primitives

The colored primitive API submits a triangle or quadrilateral to the active
PVR polygon list while keeping the packet and list costs visible.

## Public API

~~~cpp
maishuji::Triangle triangle{
    {320.0f, 88.0f, 1.0f, {255, 64, 64, 255}},
    {88.0f, 392.0f, 1.0f, {64, 255, 64, 255}},
    {552.0f, 392.0f, 1.0f, {64, 128, 255, 255}},
};

maishuji::Pvr pvr;
pvr.initialize();

maishuji::Frame frame;
maishuji::RenderList list;

pvr.begin_frame(frame);
frame.begin_list(list, maishuji::List::Opaque);
list.submit(triangle);
list.finish();
frame.finish();
~~~

Color stores red, green, blue, and alpha bytes and packs them into the ARGB
value expected by a generic KOS vertex. Vertex stores screen position, positive
depth, and one color. Triangle submits three vertices. Quad submits four
vertices as one triangle strip.

The submission methods do not allocate a primitive queue. They compile one
polygon header and submit that header plus the vertices directly to the
currently open KOS list.

## Coordinate and winding rules

The default video mode is 640x480 RGB565. The origin is the upper-left of the
screen, so x increases to the right and y increases downward. The z value is
passed to the PVR unchanged; keep it positive and use smaller values for
geometry intended to be in front when using the default depth comparison.

A quad is ordered as top-left, bottom-left, top-right, bottom-right. This is a
triangle strip: the first three vertices form one triangle and the fourth
extends the strip into the second triangle. The order is part of the API
contract, not a hidden index-buffer operation.

Culling defaults to Culling::None, which is useful for the 2D teaching examples
and avoids making visibility depend on winding. To use hardware culling, pass
PrimitiveConfiguration{Culling::Clockwise} or
PrimitiveConfiguration{Culling::CounterClockwise} and keep the vertex order
consistent with the selected mode.

## KOS mapping and cost

For each submission, the backend:

1. Compiles a public pvr_poly_hdr_t from pvr_poly_cxt_col() for the active
   list.
2. Converts public vertices explicitly into aligned pvr_vertex_t storage.
3. Calls pvr_prim() once for the header and once per vertex.
4. Marks only the final vertex with PVR_CMD_VERTEX_EOL.

A triangle therefore submits one 32-byte header and three 32-byte vertices. A
quad submits one header and four vertices. The direct path performs no
per-frame heap allocation, but it still consumes CPU time for header
compilation and packet submission. pvr_scene_finish() closes submission; the
later render wait remains the resource-idle boundary.


For texture-backed geometry, see [textures.md](textures.md). It keeps texture
VRAM ownership, ARGB4444 upload data, sampling, and release synchronization
separate from the colored primitive packet path.
