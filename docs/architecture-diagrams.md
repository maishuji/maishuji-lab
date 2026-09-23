# PVR architecture diagrams

These diagrams give a quick visual orientation to the Phase 2 PVR boundary.
The first is a C4-style logical component view; the second follows one
01-hello-pvr frame from initialization through explicit shutdown. The diagrams
keep the component labels short and the sequence to six lanes so they remain
readable in narrow Markdown renderers.

## C4-style component view

This C4-style view separates the public maishuji lifecycle components from the
KOS boundary and Dreamcast hardware. The component path is intentionally linear:
short edge labels keep the preview readable, while the sequence diagram below
shows the detailed call order.

~~~mermaid
%%{init: {"theme": "base", "flowchart": {"htmlLabels": true, "nodeSpacing": 48, "rankSpacing": 68, "curve": "linear"}}}%%
flowchart LR
    app["01-hello-pvr<br/>example loop + shutdown"]

    subgraph library [maishuji static library]
        direction LR
        pvr["Pvr<br/>hardware context"]
        frame["Frame<br/>scene scope"]
        list["RenderList<br/>polygon list"]
        adapter["KOS backend<br/>lifecycle adapter"]

        pvr -->|begins| frame
        frame -->|opens| list
        list -->|closes through| adapter
    end

    subgraph platform [Dreamcast platform]
        direction LR
        kos["KallistiOS API<br/>kos.h + dc/pvr.h"]
        hardware["Dreamcast PVR<br/>PowerVR2 renderer"]

        kos -->|controls| hardware
    end

    app -->|drives| pvr
    adapter -->|calls| kos
~~~

Read the boundary from left to right:

- The example owns the application-level loop.
- Pvr owns the hardware context; Frame and RenderList borrow that live
  context through their scopes.
- The adapter is the only component that knows KOS names such as
  pvr_scene_begin() and PVR_LIST_OP_POLY.
- pvr_scene_finish() closes submission, while pvr_wait_render_done() is
  the explicit GPU-idle boundary used during shutdown.

Host lifecycle tests replace the KOS adapter with a recording adapter. That
test-only path is intentionally omitted from the runtime diagram so the
Dreamcast flow stays readable.

## Frame sequence

This sequence shows the normal path used by 01-hello-pvr. Phase 2 opens and
closes scopes; Phase 3 will add primitive submission between list begin and
list finish.

~~~mermaid
%%{init: {"theme": "base", "sequence": {"useMaxWidth": true, "wrap": true, "diagramMarginX": 28, "diagramMarginY": 18, "actorMargin": 42, "width": 170}}}%%
sequenceDiagram
    autonumber
    participant App as 01-hello-pvr
    participant Pvr as Pvr
    participant Frame as Frame
    participant List as RenderList
    participant Backend as KOS backend
    participant KOS as KOS + Dreamcast PVR

    App->>Pvr: initialize(configuration)
    Pvr->>Backend: initialize(configuration)
    Backend->>KOS: vid_set_enabled(0)
    Backend->>KOS: vid_set_mode(640x480, RGB565)
    Backend->>KOS: pvr_init(params)
    KOS-->>Backend: initialization result
    Backend-->>Pvr: Status
    Pvr-->>App: Success

    loop 600 frames in 01-hello-pvr
        App->>Pvr: begin_frame(frame)
        Pvr->>Backend: wait_ready()
        Backend->>KOS: pvr_wait_ready()
        KOS-->>Backend: ready
        Pvr->>Backend: scene_begin()
        Backend->>KOS: pvr_scene_begin()
        Pvr-->>App: Success - Frame active

        App->>Frame: begin_list(list, Opaque)
        Frame->>Backend: list_begin(Opaque)
        Backend->>KOS: pvr_list_begin(OP_POLY)
        KOS-->>Backend: list open
        Frame-->>App: Success - RenderList active

        Note over App,List: Phase 2 scope boundary. Phase 3 adds pvr_prim() here.

        App->>List: finish()
        List->>Backend: list_finish()
        Backend->>KOS: pvr_list_finish()
        KOS-->>Backend: list closed
        List-->>App: Success

        App->>Frame: finish()
        Frame->>Backend: scene_finish()
        Backend->>KOS: pvr_scene_finish()
        KOS-->>Backend: scene submitted
        Backend-->>Frame: scene submission result
        Frame-->>App: Success
    end

    App->>Pvr: shutdown()
    Pvr->>Backend: wait_render_done()
    Backend->>KOS: pvr_wait_render_done()
    KOS-->>Backend: GPU idle
    Backend-->>Pvr: render wait complete
    Pvr->>Backend: shutdown()
    Backend->>KOS: pvr_shutdown()
    Backend->>KOS: vid_set_enabled(0)
    Pvr-->>App: Success
~~~

The important timing distinction is visible in the final three steps:
pvr_scene_finish() hands the scene to the PVR, but pvr_wait_render_done() is
the operation that makes it safe to release PVR-owned resources.

## Failure and ownership rules

The normal sequence has explicit status-returning failure points:

| Operation | If it fails | C++ lifetime result |
| --- | --- | --- |
| Pvr::initialize() | Returns an initialization status | No active Pvr context is created |
| Pvr::begin_frame() | Returns a ready-wait or scene-begin status | Frame::active() remains false |
| Frame::begin_list() | Returns disabled, overlap, reopen, or backend status | RenderList::active() remains false |
| RenderList::finish() | Returns a list-finish status | The list is still closed in C++ |
| Frame::finish() | Returns RenderListActive or scene-finish status | The frame cannot be silently reopened |
| Pvr::shutdown() | Returns FrameActive, render-wait, or shutdown status | The caller retains explicit termination ownership |

For the source-level rules behind these diagrams, see
[docs/lifecycle.md](lifecycle.md) and
[docs/kos-backend.md](kos-backend.md).
