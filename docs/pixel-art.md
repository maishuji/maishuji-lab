# Pixel-art coordinates

[maishuji/pixel.hpp](../include/maishuji/pixel.hpp) provides the portable
coordinate rules used by the pixel-art work. It does not submit geometry,
configure a PVR list, or change texture sampling; it keeps those rendering
costs visible at the existing RenderList boundary.

## Logical grid

PixelGrid models a 320x240 logical canvas displayed at 640x480. One logical
pixel becomes a 2x2 block of output pixels:

~~~cpp
#include <maishuji/pixel.hpp>

const maishuji::PixelGrid grid;
const maishuji::PixelPoint output = grid.to_output({10.25f, 20.75f});
// output is {20.0f, 42.0f};
~~~

Coordinates are rounded to the nearest logical pixel before the scale is
applied. Half-pixel values round away from zero. The helper does not clamp to
the logical viewport, so callers can intentionally place geometry off-screen
and leave clipping to the rendering path.

This is a coordinate utility, not a complete sprite system. The texture API
still owns the texture allocation and upload, while TexturedQuad still owns
the submitted UVs and vertex positions. A later sprite helper can build on
these rules without hiding the texture lifetime or PVR submission boundary.

## Subpixel comparison

For pixel-art sprites, call to_output after updating the sprite in logical
coordinates. A position such as (10.25, 20.75) remains visually stable until
rounding crosses the next logical pixel. Effects or 3D elements that need
subpixel movement can continue to submit their full-resolution coordinates
directly through the existing primitive API.

The helper is covered by host tests. Host tests verify the portable rounding
and mapping contract; they do not verify PVR rasterization or a real
Dreamcast's display timing.
