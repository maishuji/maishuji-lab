# Mipmap texture

`advanced_examples/06-mipmap-texture.cpp` demonstrates the PVR mipmap path with
one 256x256 checker texture. The scene is deliberately a side-by-side test:
the left strip uses bilinear sampling from the base level, while the right
strip enables bilinear mipmap sampling. Both strips use the same geometry,
texture coordinates, camera, and texture allocation.

## What changes

A mipmap is a reduced-resolution copy of a texture. The PVR can select a level
whose texel density better matches the projected polygon, reducing distant
aliasing at the cost of extra texture memory.

The public API keeps this choice explicit:

- `Texture::allocate()` and `Texture::upload()` remain the simple base-level,
  row-major path used by the earlier lessons.
- `Texture::allocate_mipmapped()` allocates the packed PVR chain and records its
  level count and byte size.
- `Texture::upload_mip_chain()` uploads the complete byte-addressed chain.
- `TextureSampling{TextureFilter::Bilinear, true}` selects bilinear filtering
  with mipmap use for a submission.

The old `RenderList::submit()` overloads still select nearest filtering without
mipmaps, so existing examples keep their original behavior.

## Asset boundary and memory cost

The checker source is converted with the pinned KOS `pvrtex` utility:

~~~sh
/opt/toolchains/dc/kos/utils/pvrtex/pvrtex \
  -i advanced_examples/assets/mipmap-checker.png \
  -o advanced_examples/assets/mipmap-checker.pvr \
  -f argb4444 \
  -m
~~~

The PVRT header identifies texture type 2 (mipmap) and ARGB4444 format. KOS
expects uncompressed mipmap storage as a packed chain ordered from the 1x1
level up to the base level, with six leading padding bytes. `pvrtex` omits four
of those leading bytes from the file payload, so the example restores them in
an aligned upload buffer before calling `upload_mip_chain()`. This is a
format-specific loader boundary, not a general PVR file reader.

For a 256x256 ARGB4444 texture, the nine levels use:

~~~text
256, 128, 64, 32, 16, 8, 4, 2, 1 pixels per side
~~~

The base level uses 131,072 bytes. The complete KOS upload uses 174,768 bytes,
including six leading padding bytes, so mipmaps add 43,696 bytes (33.3%) to
this texture allocation. The extra levels are not free on Dreamcast VRAM.

## Build and validate

Build the CDI with the pinned KOS container, then run the host-side Flycast
check:

~~~sh
make dreamcast-mipmap-texture-cdi
make flycast-mipmap-texture
~~~

The validator requires three consecutive stable captures and the exact guest
marker:

~~~text
maishuji: mipmap texture passed (9 levels; 174768 bytes; bilinear filtering)
~~~

It checks that the base-level and mipmapped strips are both visible and
varying, that their measured image statistics differ, and that the dark-blue
background remains outside the runway. The comparison is an emulator rendering
check; it does not establish behavior on physical Dreamcast hardware.

On 2026-09-30, the pinned KOS image
`maishuji/dc-kos-image@sha256:21832edbd57c4eb91b316c61b61008a64344703f476197887601aea5422b9f3f`
produced a passing Debug CDI and Flycast run. The checker measured the base
strip as `0.194 0.194 0.197` with variation `0.034 0.031 0.018`, and the
mipmapped strip as `0.222 0.223 0.226` with variation `0.064 0.062 0.056`.
The three background samples were `0.031 0.047 0.110`. These measurements are
fixture-specific observations, not universal thresholds for every camera or
texture.
