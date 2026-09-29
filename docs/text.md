# BIOS-font text through the PVR

`examples/08-pvr-text.cpp` turns two lines of the Dreamcast BIOS bitmap font
into the same ARGB4444 texture format used by the textured-quad lesson. It
then submits that texture through the punch-through list, so zero-alpha texels
leave the dark background untouched.

## The path

The example calls KallistiOS `bfont_draw_str_ex()` with `bpp = 16` and a
zero-filled `256x64` `std::array`:

~~~cpp
bfont_set_encoding(BFONT_CODE_ISO8859_1);
bfont_draw_str_ex(pixels.data(), 256, 0xffff, 0x0000, 16, false,
                  "PVR TEXT\nBIOS FONT");
~~~

KOS writes the 16-bit foreground value directly into the destination buffer.
That makes `0xffff` opaque white in this project’s ARGB4444 contract, while the
zero-filled background is transparent. The buffer is uploaded once through
`Texture::allocate()` and `Texture::upload()`, then sampled by one
`TexturedQuad`. The destination quad is `512x128`, so each BIOS-font pixel is
shown at 2x scale with nearest filtering.

The lesson deliberately uses the BIOS font rather than adding a general font
asset pipeline. It demonstrates the boundary clearly:

| Step | Owner or operation | Visible cost |
| --- | --- | --- |
| Rasterization | KOS BIOS font writes into CPU RAM | CPU work for the requested string before the frame loop |
| Allocation | `Texture::allocate(pvr, 256, 64)` | 32 KiB of PVR texture memory |
| Upload | `Texture::upload()` | One CPU-to-PVR texture transfer |
| Submission | Punch-through textured list | One polygon header and four textured vertices per frame |
| Release | `Texture::release()` | Waits for the PVR before freeing texture memory |

This is bitmap text, not a retained text renderer. The string uses the
ISO-8859-1 BIOS-font mode in the example; Japanese encodings and full-width
characters have different glyph widths and need their own texture sizing
rules. The example also rebuilds no glyph cache and does not claim a general
layout, wrapping, kerning, or UTF-8 API.

## Checks

Build the text ELF and CDI with the pinned toolchain, then run the host-side
Flycast gate:

~~~sh
make dreamcast-text-cdi
make flycast-text
~~~

The frame checker expects both bright text lines inside the scaled quad and a
dark sample above and below it. The required guest marker is
`maishuji: PVR text passed`. Flycast validation is emulator evidence only; a
real Dreamcast check remains separate.
