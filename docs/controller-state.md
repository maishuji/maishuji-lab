# Controller state: lesson 11

[Lesson index](examples.md) · [Input contract](input.md)

`examples/11-controller-state.cpp` displays one physical Maple port (A). It
samples once per frame and displays the copied cooked button bits, signed primary
stick axes, raw trigger bytes, connection status, a button diagram, and stick and
trigger plots. The interface in `include/maishuji/input.hpp` owns its state: no
KOS pointer escapes the adapter. PVR is only the display for this input lesson.

## Build and inspect

From the host, use the pinned KOS container to package the **live** lesson:

```sh
make host-test-input
make in-container TARGET=dreamcast-controller-state-cdi
```

Open `build-dreamcast/maishuji-controller-state.cdi` in Flycast or on a configured
console. With a controller in port A, press buttons, move the primary stick,
and squeeze the triggers; press Start to exit. Without a controller the display
shows `DISCONNECTED` and neutral values, **not a simulated controller**. A
non-controller device is `UNSUPPORTED`; if KOS exposes a controller but no status
pointer, the display says `UNAVAILABLE`. The status is a copied *cached* KOS
value, not proof of a fresh packet. Capability bits mask unsupported controls.

For repeatable display checks use a **different executable**:

```sh
make in-container TARGET=dreamcast-controller-state-scripted-cdi
make flycast-controller-state-scripted
```

It visibly says `SCRIPTED / NOT LIVE`, shows a disconnected state for the first
60 frames, an unavailable interval at frames 120–149, then a fixed A/stick/trigger
fixture. Its runtime check logs `maishuji: controller state scripted passed` once
at frame 180; it stops after 3600 frames to leave time for window capture. The screenshot checker probes selected
pixels in the fixed fixture. A successful synthetic unit test alone does **not**
verify rendering; the automated Flycast check requires a visible, unobscured window. The Flycast runner's runtime marker check
requires working guest serial-log capture to be conclusive.

## Evidence and limits

`make host-test-input` verifies portable mapping and isolated host fixtures; it
cannot exercise Maple. Debug/Release KOS cross-builds verify API and linker
compatibility, not real input. Emulator/controller mapping, disconnect latency,
physical axis convention and replacement behavior still need an interactive
Flycast and real-console check. `maple_dev_status()` in the pinned KOS version
returns cached, borrowed mutable status; the adapter copies it during a short
saved-IRQ-mask section. Do not store a Maple device pointer across frames or
interpret successive reads as new packets. See [input contract](input.md) for
source-backed boundary details and deferred scope.
