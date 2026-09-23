# Phase 2 PVR lifecycle

Phase 2 introduces the first maishuji library boundary around the raw KOS
PVR sequence. It covers initialization and safe frame/list lifetimes; it does
not submit vertices or textures yet.
For a quick visual overview, see [the PVR architecture diagrams](architecture-diagrams.md).

## Public types

The public API is include/maishuji/pvr.hpp:

- maishuji::Pvr owns one initialized video/PVR context. It is non-copyable and
  non-movable, and the caller must explicitly call shutdown().
- maishuji::Frame represents one scene submission interval. It is non-copyable
  and non-movable.
- maishuji::RenderList represents one active PVR polygon list within a frame.
  It is non-copyable and non-movable.
- maishuji::List keeps opaque, punch-through, and translucent lists as
  project-level concepts. The KOS constants stay in the backend.
- maishuji::Status reports fallible setup, acquisition, submission, and cleanup
  operations without requiring exceptions or C++23 std::expected.

The API stores no persistent primitive queue and performs no per-frame heap
allocation.

## KOS mapping

| maishuji operation | KOS operation | Boundary and cost |
| --- | --- | --- |
| Pvr::initialize() | vid_set_enabled(0), vid_set_mode(), pvr_init() | Selects the fixed 640x480 RGB565 baseline and allocates KOS-managed bins and vertex-buffer memory from the PVR memory budget. |
| Pvr::begin_frame() | pvr_wait_ready(), pvr_scene_begin() | Waits for the next submission boundary before opening one scene. |
| Frame::begin_list() | pvr_list_begin() | Opens one configured polygon list; a disabled list or overlapping list is rejected before the KOS call. |
| RenderList::finish() | pvr_list_finish() | Closes the active list. The list cannot be reopened in the same scene. |
| Frame::finish() | pvr_scene_finish() | Closes scene submission but does not prove that the GPU has finished reading resources. |
| Pvr::shutdown() | pvr_wait_render_done(), pvr_shutdown(), vid_set_enabled(0) | Performs the explicit idle boundary before releasing PVR ownership. This wait is intentionally visible in the API. |

The KOS backend uses public <kos.h> and <dc/pvr.h> declarations only. It
keeps the raw baseline's three-list configuration, direct non-DMA submission
policy, 512 KiB vertex buffer, disabled FSAA, enabled translucent autosort,
OPB overflow count of 3, and vertex-buffer double buffering.

## Lifetime rules

A Pvr object must outlive its Frame and RenderList objects and must be shut
down after all active scopes have finished. Pvr::shutdown() returns
FrameActive instead of guessing how to close an open scene.

Frame::finish() returns RenderListActive when a list remains open. A
RenderList must finish before its frame. Both scope types close an active KOS
scope from their non-throwing destructors as a best-effort safety net; the
destructors never call pvr_wait_render_done(). Call finish() explicitly when
the status must be observed. completion_status() retains the status reported
by explicit or destructor cleanup.

Failed acquisition never activates a guard:

- a failed ready wait or scene begin leaves Frame::active() false;
- a failed list begin leaves RenderList::active() false;
- a failed list or scene finish closes the corresponding C++ state so later
  cleanup cannot issue an unmatched KOS finish call.

Debug builds assert if an initialized Pvr is destroyed without explicit
shutdown. This makes PVR termination ownership visible rather than hiding a
GPU wait in a destructor.

## Backend isolation and tests

src/pvr.cpp contains the portable state machine. It calls a fixed internal PVR
lifecycle adapter, not a generic renderer interface or virtual dispatch.
Dreamcast builds link src/kos/pvr_backend.cpp; host builds link the recording
adapter in src/host/recording_backend.cpp.

The host recording adapter tests:

- initialization and shutdown ownership;
- one active frame and one active list at a time;
- disabled-list and invalid-configuration rejection;
- failed ready, scene, list, render-wait, and initialization operations;
- destructor cleanup and retained completion status.

Run the host checks with:

~~~sh
make host-run
make host-test
~~~

The pinned target build compiles both the raw reference and
maishuji-hello-pvr, which opens and finishes an opaque list for 600 frames.
That proves target compilation and ABI compatibility; it is not emulator or
real-hardware runtime evidence. Primitive submission remains in the raw
reference until Phase 3.
