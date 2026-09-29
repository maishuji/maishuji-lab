# PVR offset-color lighting

This example teaches the PVR's offset-color path. It compares ordinary
Gouraud vertex colors with a second quad that adds a different offset color
at each vertex.

## Public API

~~~cpp
maishuji::Vertex vertex{
    350.0f, 140.0f, 1.0f,
    {32, 32, 48, 255},
    {128, 64, 0, 255},
};

maishuji::PrimitiveConfiguration lighting{};
lighting.enable_offset_color = true;
list.submit(quad, lighting);
~~~

The base color remains the ordinary vertex color. The offset color is an
additive per-vertex color. The PVR interpolates the submitted vertex data
across the polygon; the result is useful for simple highlights, glow-like
accents, and other fixed-function effects.

This is deliberately not a general lighting model: there are no normals,
light positions, attenuation values, materials, or scene objects. Those
would hide the PVR boundary and belong to a later, separately justified
layer.

## KOS mapping

The backend maps the API to the public KOS polygon path:

1. enable_offset_color sets pvr_poly_cxt_t::gen.specular.
2. Vertex::offset_color is packed into pvr_vertex_t::oargb.
3. The compiled polygon header enables the PVR offset-color calculation.

KOS documents this header mode as specular lighting and describes oargb as
the vertex offset color. The offset color's alpha channel is ignored by the
PVR calculation. See the [KOS polygon header reference](https://kos-docs.dreamcast.wiki/pvr__header_8h_source.html).

The same offset-color data is carried through the mesh clipping and projection
path, and textured vertices have the field available for a future textured
lighting example.

## Cost and validation

Offset-color lighting does not add a packet. A triangle still submits one
32-byte polygon header and three 32-byte generic vertices; a quad submits one
header and four vertices. The wrapper adds one public color field per vertex
and one header-state choice, but it performs no per-frame heap allocation.

Host tests verify that the enable flag and per-vertex data reach the recording
backend. The pinned target build verifies the KOS field names and packet types.
Neither check claims real-console lighting output; emulator or hardware runtime
validation remains a separate step.

Build the example with:

~~~sh
make dreamcast-build DC_BUILD_TYPE=Debug
make dreamcast-lighting-cdi
~~~
