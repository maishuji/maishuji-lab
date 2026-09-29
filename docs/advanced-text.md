# Advanced multilingual text

`advanced_examples/01-multilingual-text.cpp` displays the same idea in three
scripts:

~~~text
ケーキは嘘だ。
蛋糕是個謊言。
The cake is a lie.
~~~

The first two lines cannot use the Dreamcast BIOS font from the earlier text
lesson. That API is an ISO-8859-1 bitmap-font path, so this lesson uses a small
pre-rasterized glyph atlas instead.

## Rendering path

The tracked atlas is `256x128` ARGB4444: 64 KiB of PVR texture memory. It was
generated from open-source Noto Sans CJK and Noto Sans glyphs; the attribution
and license are in [advanced_examples/assets/NOTICE.md](../advanced_examples/assets/NOTICE.md).
The original font files are not part of the Dreamcast binary.

To regenerate the header on a host with ImageMagick and the registered Noto
fonts installed:

~~~sh
python3 tools/generate-advanced-text-atlas.py
~~~

The example performs a deliberately narrow UTF-8 pass:

- It decodes the three fixed strings into Unicode code points.
- It maps those code points to atlas cells and applies simple advances.
- It emits one punch-through textured quad per non-space glyph.
- It uploads the atlas once and reuses it for every frame.

The current workload is 28 glyph quads per frame: 28 polygon headers, 112
vertices, and 140 `pvr_prim()` calls. These are structural packet counts from
the wrapper path, not SH-4 timings or a vertex-buffer high-water measurement.
The atlas is intentionally incomplete: unsupported code points fail the example
instead of silently falling back. There is no shaping, kerning, line wrapping,
font fallback, or general Unicode text API.

## Build and runtime check

~~~sh
make dreamcast-advanced-text-cdi
make flycast-advanced-text
~~~

The Flycast checker verifies three bright line regions against the uniform
background produced by the lesson, plus the exact guest marker
`maishuji: advanced multilingual text passed`. Emulator validation does not
replace real Dreamcast hardware validation.

On 2026-09-29, the pinned KOS 2.2.2 / GCC 15.2.1 container built and packaged
both Debug and Release CDIs successfully. `make host-test` passed, and
`make flycast-advanced-text` passed with eight stable frames and the required
runtime marker. The checked capture was 960x720 and measured Japanese,
Traditional Chinese, and English text regions. No real Dreamcast hardware was
available for this validation.
