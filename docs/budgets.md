# PVR list and vertex-buffer budgets

The packet-budget lesson makes a small mixed workload countable. It submits one
triangle and one quad through each of the opaque, punch-through, and translucent
lists. The punch-through list also shows the texture-backed path without hiding
that it is still one header plus four vertex packets.

Run it with:

~~~sh
make dreamcast-budget-cdi
make flycast-budget
~~~

## Validation evidence

On 2026-09-29, the pinned KOS 2.2.2 / GCC 15.2.1 container built and
packaged both Debug and Release forms. Flycast Flatpak 2.7 then passed
`make flycast-budget` with the required guest marker and a stable rendered
frame:

~~~text
PASS: Flycast rendered opaque, punch-through, and translucent budget panels.
Stable frames: 7
Required runtime marker: passed
~~~

The expected result is that all six submissions remain visible in their three
list panels and the guest marker reports the source-derived budget. This is
confidence in the target build and emulator rendering path only; it does not
verify a real Dreamcast, SH-4 timing, or vertex-buffer high-water usage.

## Structural budget

The current backend emits one 32-byte `pvr_poly_hdr_t` packet followed by one
32-byte `pvr_vertex_t` packet for every submitted vertex. The lesson therefore
has this source-derived shape per frame:

| Submission | Count | Header packets | Vertex packets | `pvr_prim()` calls |
| --- | ---: | ---: | ---: | ---: |
| Colored triangles | 3 | 3 | 9 | 12 |
| Colored quads | 2 | 2 | 8 | 10 |
| Textured quad | 1 | 1 | 4 | 5 |
| **Total** | **6** | **6** | **21** | **27** |

The packet estimate is `(6 + 21) * 32 = 864` bytes before list and scene
control. That is a submission-shape estimate, not a measurement of the PVR
vertex buffer's actual high-water mark: KOS may reserve or align additional
space, and the renderer's buffering and synchronization behavior are not
visible from this source count.

The example uses `static_assert` expressions for the arithmetic and prints the
same budget in its guest completion marker. If a primitive is added or removed,
the compile-time count and the documented table should change together.

## What the lesson teaches

- A triangle costs one header plus three vertex packets.
- A quad is a triangle strip and costs one header plus four vertex packets.
- Texturing changes the compiled polygon state, not the four-vertex submission
  shape.
- Separate list scopes make list selection explicit; they do not create a
  generic batching or index-buffer abstraction.
- The current wrapper submits packets directly and performs no per-submission
  heap allocation.

This lesson does not benchmark SH-4 cycles, frame rate, VRAM fragmentation,
vertex-buffer high-water, cache behavior, DMA, or real Dreamcast throughput.
Flycast verifies the visible mixed-list workload only; hardware validation
remains a separate boundary.
