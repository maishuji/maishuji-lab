# Raw KOS/PVR boundary

This document records the Phase 1 baseline before introducing maishuji-lab
rendering abstractions. The target source of truth is the pinned KallistiOS
environment in [`tools/toolchain.lock`](../tools/toolchain.lock): KOS 2.2.2,
source snapshot `08FEB26`, commit
`0aa363a145ead0c6549e77bc7f468dfd8e10134f`.

The workstation KOS installation is not evidence for this baseline. Target
builds must run in the pinned development container or CI image. The current
raw reference is [`examples/pvr_smoke.cpp`](../examples/pvr_smoke.cpp).

## Boundary decision

The project uses public KOS headers and calls only:

| Concern | Public KOS boundary | Phase 1 rule |
| --- | --- | --- |
| Video mode | `vid_set_enabled()`, `vid_set_mode()` | Set a 2D mode before PVR initialization. |
| PVR setup | `pvr_init()` with `pvr_init_params_t` | Configure enabled lists, vertex buffer, and DMA policy explicitly. |
| Frame pacing | `pvr_wait_ready()` | Wait before beginning each scene. |
| Scene lifetime | `pvr_scene_begin()`, `pvr_scene_finish()` | One scene is open at a time. |
| List lifetime | `pvr_list_begin()`, `pvr_list_finish()` | Submit each list as one contiguous group; do not reopen a finished list in the same scene. |
| Primitive submission | `pvr_prim()` | Submit only aligned, 32-byte-multiple packets to the currently open list. |
| Polygon setup | `pvr_poly_cxt_*()`, `pvr_poly_compile()` | Compile a public polygon header before submission. |
| Shutdown | `pvr_wait_render_done()`, `pvr_shutdown()` | Wait before releasing resources or leaving PVR ownership. |

Private KOS headers, internal PVR structs, direct register access, and
hard-coded Tile Accelerator addresses are outside this boundary.

## Build-system decision

The project keeps CMake as its primary build because its native KOS toolchain
file reproduces the target compiler, linker, C++20 mode, disabled exceptions
and RTTI, and no-LTO link policy used by the project. The tracked
[`tools/kos-makefile-smoke.mk`](../tools/kos-makefile-smoke.mk) builds the same
raw source with KOS's supplied Makefile rules and `kos-c++` wrapper. It uses
the same C++20, no-exceptions, no-RTTI, and no-LTO policy; CMake may add
Debug-only frame-pointer instrumentation. The Makefile path exists as a
compatibility check, not as a second project build graph; the two paths must
continue to produce independently buildable raw reference ELFs.

## Effective target policy

The pinned CMake builds and native KOS Makefile smoke agree on the policy that
matters to the library:

- C++20 without compiler extensions;
- exceptions and RTTI disabled for target code;
- no LTO at link time;
- KOS's `kos-c++` compiler/linker wrappers and startup objects;
- `-m4-single-only` reapplied by `tools/with-kos.sh` after KOS environment
  setup, for both compile and link flags.

The pinned Debug CMake command additionally uses `-DFRAME_POINTERS` and
`-fno-omit-frame-pointer`; Release uses optimization and `-DNDEBUG`. Those are
build-mode instrumentation choices, not API requirements. Host C++20 checks
remain native and never link KOS libraries.

## Initialization baseline

The raw smoke configures the following `pvr_init_params_t` values:

| Field | Value | Reason |
| --- | --- | --- |
| Opaque polygon bin | `PVR_BINSIZE_16` | Keep the opaque list enabled for the reference triangle. |
| Opaque modifier bin | `PVR_BINSIZE_0` | Not used by the reference. |
| Translucent polygon bin | `PVR_BINSIZE_16` | Keep the second polygon list available for the Phase 1 boundary. |
| Translucent modifier bin | `PVR_BINSIZE_0` | Not used by the reference. |
| Punch-through bin | `PVR_BINSIZE_16` | Keep all three polygon list types enabled explicitly. |
| Vertex buffer | `512 * 1024` bytes | Match the pinned KOS default while keeping the budget visible. |
| Vertex DMA | `0` | Use the direct `pvr_prim()` path for the first baseline. |
| FSAA | `0` | Avoid changing the reference image through horizontal scaling. |
| Translucent autosort | enabled | Preserve KOS's default translucent ordering policy. |
| OPB overflow | `3` | Match the pinned KOS default. |
| Vertex-buffer double buffering | enabled | Preserve KOS's default frame-buffering behavior. |

The bin sizes and vertex buffer consume PVR texture memory. A future library
configuration must therefore expose these costs instead of silently choosing a
large global allocation.

## Submission and packet constraints

The pinned public `dc/pvr.h` contract requires data passed to `pvr_prim()` to
be 32-byte aligned and its byte count to be divisible by 32. The generic
`pvr_vertex_t` and `pvr_poly_hdr_t` layouts are each 32 bytes and 32-byte
aligned on the target. The smoke keeps target assertions for those properties;
host layout is not evidence for SH-4 layout.

The reference submits one compiled polygon header followed by three generic
vertices. The final vertex carries `PVR_CMD_VERTEX_EOL`, which terminates the
triangle strip. The list selected by the polygon header must match the list
opened with `pvr_list_begin()`.

## Frame state machine

The non-DMA reference follows this sequence for every frame:

```text
ready
  -> pvr_wait_ready()
collecting scene
  -> pvr_scene_begin()
collecting list
  -> pvr_list_begin(list)
submitting list data
  -> pvr_prim(header/vertices)
list closed
  -> pvr_list_finish()
scene closed
  -> pvr_scene_finish()
ready for the next frame
```

`pvr_scene_finish()` hands the collected scene to the PVR; it does not prove
that rendering has completed. `pvr_wait_ready()` provides the next-frame
submission boundary, while `pvr_wait_render_done()` is the explicit boundary
for resources that the GPU may still be reading, such as textures.

The non-DMA rules are deliberately strict: all primitives for one list are
submitted together, and a list cannot be reopened after `pvr_list_finish()` in
the same scene. The eventual `Frame` and `RenderList` types must represent
these transitions rather than implying that arbitrary nested scopes can reopen
hardware lists.

## Resource lifetime rules

The raw boundary has four distinct lifetime intervals:

1. The selected video mode must exist before `pvr_init()` and remain active
   while the PVR is initialized.
2. PVR-owned bins and vertex buffers begin at `pvr_init()` and end at
   `pvr_shutdown()`. They consume texture memory and are not application-owned
   allocations.
3. Direct-submission packet storage must be aligned and readable for the
   duration of each `pvr_prim()` call. The first baseline submits immediately;
   it does not expose a persistent packet queue to the application.
4. Any resource referenced by submitted rendering, especially texture-memory
   allocations, remains alive until `pvr_wait_render_done()` establishes that
   the previous scene has stopped using it. `pvr_scene_finish()` only closes
   submission for the scene and is not a safe free/overwrite boundary.

This gives the first library design a deliberately narrow contract: a frame
owns its open scene, a render list borrows that frame, and resource destruction
must happen after an explicit idle boundary. A constructor or destructor must
not imply a hidden wait or silently reopen a KOS list.

## Initial backend/API constraints

The Phase 2 API can now be kept small and testable:

- a single active PVR context is supported initially;
- initialization is an explicit fallible operation that returns a status/result;
- `List::Opaque`, `List::PunchThrough`, and `List::Translucent` remain project
  concepts mapped to KOS constants only inside the backend;
- `Frame` and `RenderList` are non-copyable and initially non-movable;
- a `RenderList` can exist only while its owning `Frame` is collecting a scene;
- failed acquisition creates no guard that later calls a KOS finish function;
- ordinary destructors are non-throwing and do not perform hidden waits;
- Debug checks reject nested frames, overlapping lists, closed-list reopening,
  disabled-list use, and submission outside an open list;
- the public layer does not expose `pvr_*` types, private KOS headers, or a
  generic renderer interface.

The allowed KOS-facing includes for this backend are `<kos.h>` for startup,
video, and diagnostics and the public `<dc/pvr.h>` family for initialization,
scene/list submission, packet compilation, and PVR texture-memory allocation.
The implementation may read KOS internals for explanation and compatibility
work, but it must not include `pvr_internal.h` or depend on private state.

## Error and runtime policy

Phase 1 keeps the raw KOS return values visible:

- fallible setup and submission calls return an explicit status to the caller;
- a failed setup path must not create a guard that later performs an unmatched
  finish call;
- programmer misuse is asserted in Debug-oriented library checks;
- cleanup is non-throwing and does not require exceptions or RTTI;
- the target path uses C++20 without extensions, with exceptions and RTTI
  disabled as established by the Phase 0 build;
- target rendering and host contract tests remain separate because host
  execution cannot establish PVR behavior.

The current raw example logs a failure, waits for rendering to stop, shuts down
the PVR, disables video, and exits for an unrecoverable smoke error. Its normal
path renders a finite reference run, performs the same wait/shutdown sequence,
and returns successfully. Phase 2 will turn this policy into a small library
result/status type and lifecycle checks without introducing `std::expected` or
a generic renderer.

## C++20 evidence

The host smoke compiles, links, and executes checks for the small language
subset currently needed by the project:

- concepts and `constexpr` evaluation for a numeric helper;
- `std::array` and fixed-extent `std::span` access;
- a move-only type with non-throwing move construction and assignment;
- deterministic RAII destruction of the moved-to owner.

The raw target smoke separately compiles concepts, `constexpr`, and
`std::span` together with the KOS headers and target packet assertions. The
host executable proves native execution of the portable probes; the target
ELF build proves target compile/link compatibility. Neither result claims that
all of the C++ standard library or runtime facilities are available on the
Dreamcast. Exceptions, RTTI, and hidden allocation remain outside the chosen
target policy.

The target smoke also executes a deliberately small runtime probe set before
rendering: a global constructor, a local move-only object and its destructor,
a PVR texture-memory allocation request that must fail, and the normal PVR
shutdown path. Rendering is gated on those probes passing, so the Flycast
harness's three consecutive valid captures establish that the probe path
completed before submission. Flycast's KOS `dbgio` output is not surfaced by
the installed Flatpak by default; when a serial/dcload console is available,
the harness can additionally require the runtime log markers with
`FLYCAST_REQUIRE_RUNTIME_MARKERS=1`. This does not claim that
exceptions, RTTI, or arbitrary standard-library allocation are appropriate for
the library; those remain disabled or outside the target policy.

## Evidence status

Phase 0 established the following evidence against the pinned image:

- host C++20 compile/link/execute smoke passed;
- fresh Debug and Release SH-4 ELFs built;
- target assertions for the generic PVR packet layouts passed during the
  target builds;
- the self-boot CDI opened in Flycast v2.7 and rendered the expected colored
  triangle; the automated pixel check passed;
- the prior upload attempt used an unconfirmed console address and did not
  execute, so no real-hardware result is claimed.

The Phase 1 raw baseline now adds the following evidence:

- the explicit three-list, non-DMA `pvr_init_params_t` configuration builds in
  both Debug and Release with the pinned KOS 2.2.2 image;
- both the CMake path and the minimal native KOS Makefile path build the raw
  reference ELF with the pinned compiler and linker wrappers;
- the pinned Debug CDI boots in Flycast v2.7 and passes three consecutive
  captures of the expected colored triangle;
- the raw failure path explicitly waits for rendering, shuts down the PVR,
  disables video, and returns an error status;
- the same CDI reaches rendering only after the global-constructor,
  local-move-only-cleanup, and PVR-allocation-failure probes pass; the finite
  run then exercises normal PVR shutdown and process exit;
- the host C++20 probe executes the concepts, `constexpr`, `span`, move-only,
  and RAII checks.

No real-hardware result is claimed. The remaining Phase 1 work is to turn
these observations into the small backend/API decisions, document the target
console-output limitation, and preserve the raw triangle as the comparison
reference.
