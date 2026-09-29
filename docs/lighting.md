# PVR offset-color lighting

This example teaches the PVR's offset-color path. It compares an ordinary
textured quad with a second quad that adds a different offset color at each
vertex. Both use the same small opaque texture, so the visible difference comes
from the PVR vertex colors rather than from different image assets.

## Public API

```cpp
maishuji::TexturedVertex vertex{
    350.0f, 140.0f, 1.0f,
    0.0f, 0.0f,
    {32, 32, 48, 255},
    {128, 64, 0, 255},
};

maishuji::PrimitiveConfiguration lighting{};
lighting.enable_offset_color = true;
list.submit(texture, quad, lighting);
```

The base color remains the ordinary vertex color. The offset color is an
additive per-vertex color. The PVR interpolates the submitted vertex data
across the polygon; the result is useful for simple highlights, glow-like
accents, and other fixed-function effects.

The runtime lesson uses an 8x8 opaque ARGB4444 texture because 8 pixels is the
smallest texture dimension supported by the PVR texture-size fields. On the
pinned KOS/Flycast path, the untextured packed-color packet does not preserve
this offset word, while the generic textured vertex packet does. The base quad
also enables the offset-color header with zero offsets, making the comparison
use the same packet form on both sides.

This is deliberately not a general lighting model: there are no normals,
light positions, attenuation values, materials, or scene objects. Those would
hide the PVR boundary and belong to a later, separately justified layer.

## KOS mapping

The backend maps the API to the public KOS polygon path:

1. `enable_offset_color` sets `pvr_poly_cxt_t::gen.specular`.
2. `TexturedVertex::offset_color` is packed into `pvr_vertex_t::oargb`.
3. The compiled polygon header enables the PVR offset-color calculation.

KOS documents this header mode as specular lighting and describes `oargb` as
the vertex offset color. The offset color's alpha channel is ignored by the
PVR calculation. See the [KOS polygon header reference](https://kos-docs.dreamcast.wiki/pvr__header_8h_source.html).

The same offset-color data is carried through the mesh clipping and projection
path, and textured vertices expose it directly for the packet form used here.

## Cost and validation

Offset-color lighting does not add a packet. A textured quad submits one
32-byte polygon header and four 32-byte generic vertices. The lesson's 8x8
ARGB4444 texture costs 128 bytes of PVR texture memory. The wrapper adds one
public color field per vertex and one header-state choice, but it performs no
per-frame heap allocation.

Host tests verify that the enable flag and per-vertex data reach the recording
backend. The pinned target build verifies the KOS field names and packet types.
The Flycast runtime gate verifies the visible base-vs-offset result separately;
it does not claim real-console lighting output.

The runtime validation was performed on 2026-09-29 with the locked KOS 2.2.2
container and Flycast Flatpak 2.7:

```sh
make dreamcast-lighting-cdi
make flycast-lighting
```

The run emitted `maishuji: PVR offset-color lighting passed`, and the frame
checker accepted the 4:3 capture after verifying the base quad and all four
colored offset regions. Real Dreamcast hardware remains unverified here.
