# Textures

The texture API keeps CPU source data, PVR video memory, and the synchronization
boundary explicit. The current path supports one small, uncompressed format:
16-bit ARGB4444 pixels stored as a non-twiddled texture and sampled with nearest
filtering.

## Public API

~~~cpp
#include <array>
#include <span>

std::array<std::uint16_t, 32 * 32> pixels{};
maishuji::Texture texture;

maishuji::Status status = texture.allocate(pvr, 32, 32);
if(maishuji::succeeded(status))
    status = texture.upload(std::span<const std::uint16_t>{pixels});

maishuji::TexturedQuad quad{
    {80.0f, 100.0f, 1.0f, 0.0f, 0.0f},
    {80.0f, 220.0f, 1.0f, 0.0f, 1.0f},
    {200.0f, 100.0f, 1.0f, 1.0f, 0.0f},
    {200.0f, 220.0f, 1.0f, 1.0f, 1.0f},
};

list.submit(texture, quad);
~~~

Texture is move-only. Allocation and upload are separate so the caller can see
which operation consumes PVR VRAM and which operation copies CPU data into it.
The source span is not retained after upload returns.

Supported dimensions are powers of two from 4 through 1024 for both width and
height. The upload span must contain exactly width * height 16-bit pixels.
The resulting byte count is width * height * 2; these dimensions make the
count a multiple of the 32-byte boundary required by KOS pvr_txr_load().

## Pixel and layout contract

Each std::uint16_t uses ARGB4444 packing:

~~~text
bits 15..12: alpha
bits 11..8 : red
bits  7..4 : green
bits  3..0 : blue
~~~

For example, an opaque white pixel is 0xffff, while a half-alpha red pixel is
0x8f00. Pixels are supplied in ordinary row-major order because the backend
selects PVR_TXRFMT_NONTWIDDLED. The KOS backend combines that flag with
PVR_TXRFMT_ARGB4444 and uses PVR_FILTER_NEAREST when it compiles the polygon
header.

The API deliberately does not accept mipmaps, VQ data, paletted data, stride
textures, or a caller-selected filter yet. Those formats need their own size,
packing, and lifetime contracts.

## Ownership and synchronization

allocate() requires an initialized Pvr and obtains a block from KOS
pvr_mem_malloc(). The returned handle is private to the texture. upload() copies
the supplied pixels into that PVR allocation through pvr_txr_load().

submit() requires an active RenderList belonging to the same Pvr that owns the
texture. A texture must remain allocated until the submitted rendering has
finished using it. release() therefore waits for the PVR render boundary before
calling KOS pvr_mem_free(). That wait is an intentional synchronization cost;
release during an active frame returns FrameActive, and a failed render wait
leaves the allocation owned by the texture so the caller can retry.

Release textures before shutting down their Pvr. Destruction of an allocated
Texture is a programmer error detected by the debug assertion; explicit
release keeps the lifetime and wait visible.

Move construction and move assignment transfer the PVR handle without copying
texture data. The moved-from object is empty and can be destroyed normally.

## List variants and example

examples/03-textured-quad.cpp uploads one alpha-bearing checker texture,
submits three quads in one frame—opaque, punch-through, and translucent—and
then repeats that workload for eight texture lifetimes. Each lifetime renders
75 frames, waits at Texture::release(), and allocates the next texture only
after the previous PVR handle has been freed. The total remains 600 frames,
but the example now exercises repeated VRAM reuse instead of only one
allocation. The texture alpha values make the list differences visible
without adding a material system. The example builds each 176x176 output quad
through Sprite: its 88x88 logical size is scaled by PixelGrid, while the
texture remains an independently allocated resource. SpriteRegion can derive
the normalized UVs for a sub-rectangle from texture.width() and
texture.height() without changing that ownership model.

The KOS mapping is intentionally direct:

| maishuji operation | KOS operation | visible cost |
| --- | --- | --- |
| Texture::allocate() | pvr_mem_malloc(width * height * 2) | reserves PVR VRAM |
| Texture::upload() | pvr_txr_load() | copies CPU pixels over the TA/PVR path |
| textured submission | pvr_poly_cxt_txr() plus one header and four vertex packets | compiles state and consumes list/vertex-buffer space |
| Texture::release() | pvr_wait_render_done() then pvr_mem_free() | blocks until the GPU is idle before freeing VRAM |

The host recording backend tests these ownership, validation, move, failure, and
wait rules without claiming to emulate PVR rasterization. Target Debug and
Release builds are the cross-compilation check, and the dedicated Flycast gate
checks the rendered textured output. The repeated lifetime workload is not a
hardware performance measurement; real Dreamcast validation remains separate.
