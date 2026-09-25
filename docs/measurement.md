# Raw and wrapped submission costs

This note records the first structural comparison between the raw KOS PVR
reference and the maishuji submission path. It uses the pinned target build
and source inspection; it does not claim SH-4 cycle timings or real-hardware
frame-rate results.

## Scope and reproduction

The comparison uses the pinned maishuji/dc-kos-image digest
sha256:21832edbd57c4eb91b316c61b61008a64344703f476197887601aea5422b9f3f
(KallistiOS 2.2.2, GCC 15.2.1, -m4-single-only). From the repository root:

~~~sh
make host-test
make host-run
docker run --rm --user 1000:1000 \
  -v "$PWD:/workspace" -w /workspace \
  maishuji/dc-kos-image@sha256:21832edbd57c4eb91b316c61b61008a64344703f476197887601aea5422b9f3f \
  make dreamcast-textured-cdi DC_BUILD_TYPE=Release
docker run --rm --user 1000:1000 \
  -v "$PWD:/workspace" -w /workspace \
  maishuji/dc-kos-image@sha256:21832edbd57c4eb91b316c61b61008a64344703f476197887601aea5422b9f3f \
  ./tools/with-kos.sh sh-elf-size \
  build-dreamcast/maishuji-pvr-smoke.elf \
  build-dreamcast/maishuji-colored-primitives.elf \
  build-dreamcast/maishuji-textured-quad.elf
make flycast-textured-quad FLYCAST_STABLE_SAMPLES=3
~~~

The Docker commands keep the build and KOS environment identical to CI. The
Flycast command runs on the host after the CDI has been created; its pixel
checker is a rendering gate, not a performance benchmark.

## Packet and lifetime comparison

Source inspection of examples/pvr_smoke.cpp and src/kos/pvr_backend.cpp gives
this per-frame shape:

| Path | Polygon header packets | Vertex packets | pvr_prim() packet calls per submitted primitive |
| --- | ---: | ---: | ---: |
| Raw reference triangle | 1 | 3 | 4 |
| Wrapped colored triangle | 1 | 3 | 4 |
| Wrapped colored quad | 1 | 4 | 5 |
| Wrapped textured quad | 1 | 4 | 5 |

The wrapper validates state, converts public vertices to aligned KOS packet
storage, compiles the corresponding polygon context, and emits the same packet
shape for equivalent geometry. The current Dreamcast backend does not allocate
from the heap for each submission and does not use virtual dispatch in the
rendering path. Texture allocation and upload happen outside list submission;
Texture::release() waits for the render boundary before freeing PVR memory.

The raw reference and wrapped examples both render 600 frames. The textured
example additionally repeats eight allocation/upload/draw/release lifetimes,
with 75 frames per lifetime, so VRAM reuse is exercised rather than inferred
from one allocation.

## Pinned Release ELF sizes

The following result was produced by sh-elf-size after the pinned Release
build:

| ELF | text | data | bss | decimal total |
| --- | ---: | ---: | ---: | ---: |
| maishuji-pvr-smoke.elf | 304276 | 13420 | 40900 | 358596 |
| maishuji-colored-primitives.elf | 307920 | 13412 | 40340 | 361672 |
| maishuji-textured-quad.elf | 309244 | 13412 | 40340 | 362996 |

These totals include the KOS runtime and example code, so they are useful for
regression tracking but do not isolate the cost of one wrapper function. The
textured example is 4,400 bytes larger than the raw reference in this build;
that difference includes the texture implementation, example, and linked
code paths.

## Limits of this evidence

This is a structural packet-count and binary-size comparison. It does not
measure CPU submission time, cache effects, synchronization time, frame rate,
VRAM high-water marks, or DMA behavior. Flycast v2.7 confirms the textured
output and repeated target workload reaches the expected rendered frame, but
emulator timing is not evidence of SH-4 performance. Hardware measurements
remain open until the examples run on a Dreamcast with a known loader and
video setup.

For the lifecycle mapping and wait boundaries, see
[docs/kos-backend.md](kos-backend.md) and [docs/lifecycle.md](lifecycle.md).
For primitive packet details and texture ownership, see
[docs/primitives.md](primitives.md) and [docs/textures.md](textures.md). The
Flycast serial and runtime limitations are recorded in
[docs/flycast-serial-logs.md](flycast-serial-logs.md).
