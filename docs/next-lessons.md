# Next lessons: fixed-step time, AICA sound, VMU LCD

This is a staged plan, not a claim that all three lessons are implemented.
Follow the [lesson index](examples.md) for existing examples. The repository
currently has an unrelated local `.gitignore` edit: preserve it. Keep the new
lessons independent of the existing PVR/input implementation and avoid broad
framework or toolchain changes while the working tree is in flux.

## 1. Fixed-step time (`examples/15-fixed-step.cpp`) — implemented

**Goal:** distinguish *elapsed wall-clock time*, *simulation updates*, and
*rendering*. One independent capability belongs in `examples/`; a later
advanced lesson can combine it with sound/input. Read uptime through the
pinned KOS [timer API](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/include/kos/timer.h):
`timer_us_gettime64()` returns microseconds since KOS startup. The existing
[camera example](../advanced_examples/12-input-driven-camera.cpp) caps a
variable elapsed step; it does **not** demonstrate a fixed-step accumulator.

1. Add a small portable, lesson-local accumulator. Pass it integer elapsed
   microseconds; simulate at approximately 60 Hz, with a documented integer
   rounding policy, bounded catch-up and an explicit count of dropped backlog.
   Handle first tick, identical timestamps, backward clock readings, and large
   pauses without unsigned underflow or an unbounded catch-up spiral.
2. Add host tests for zero/partial/multiple steps, the catch-up cap, stalls,
   backward timestamps, repeated runs, and accumulated rounding. Keep the
   update callback independent of KOS/PVR so the same sequence is reproducible.
3. Make a small deterministic visual (e.g. two markers showing variable vs
   fixed-step motion and counters for updates, render frames and discarded
   backlog). The KOS clock is the live source; a separately labeled scripted
   clock sequence is the rendering fixture, never silently substituted.
4. Wire host tests, Debug/Release pinned KOS builds, a CDI, a bounded scripted
   Flycast capture and documentation. Demonstrate that a passing capture proves
   a *fixture*, not exact timing or hardware frame cadence. Measure live timing
   separately before making performance claims.

**Result:** the portable accumulator, host tests, live and explicitly scripted
Dreamcast targets, CDI packaging, visual fixture checker and [lesson guide](fixed-step.md)
are implemented. The lesson does not add a scheduler, global engine clock,
threads, or sound synchronization. Its scripted capture demonstrates supplied
inputs; it does not measure emulator or hardware cadence.

## 2. AICA sound (`examples/16-aica-sound.cpp`) — implemented, audio gate open

The pinned KOS `63702a8` [sound effect manager](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/include/dc/sound/sfxmgr.h)
provides `snd_sfx_load_raw_buf`, `snd_sfx_play`, `snd_sfx_unload` and an
invalid-handle constant. It accepts a short 16-bit mono PCM sound; raw data
must be a multiple of 32 bytes per channel and at most 65534 samples. The
[sound lifecycle API](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/include/dc/sound/sound.h)
warns that `snd_init()` can replace the AICA program and that shutting down
sound is normally unnecessary. Therefore keep this first sound lesson an
independent executable, not a shared framework audio owner or a change to the
PVR backend. Generate a brief original PCM tone in fixed storage, check
initialization/load/play results and release its SFX handle. An optional A
button edge can replay the tone through the existing input API; absence of a
controller must not prevent an initial demonstration.

**Checks:** pure host waveform tests, pinned KOS Debug/Release compile/link,
and a bounded guest marker for resource/error paths. A log marker or a visual
Flycast frame does **not** prove that sound was audible: verify audibility by
listening on a named emulator/audio configuration or record a controlled
loopback capture if available. Never claim physical AICA output without an
actual console/audio test. Keep the implementation separate from Step 1 and
preserve unrelated changes. The [sound lesson](aica-sound.md) now has host
waveform tests, pinned Debug/Release cross-builds and a packaged Release CDI;
audible output and panning remain unverified.

## 3. VMU LCD (`examples/17-vmu-lcd.cpp`) — research gate before code

KOS `63702a8` [vmu.h](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/include/dc/maple/vmu.h)
accepts a 48×32, one-bit bitmap via `vmu_draw_lcd`; the rotated variant
requires four-byte alignment. Its [driver](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/hardware/maple/vmu.c)
copies 192 bytes into a Maple frame and queues the request. `MAPLE_EOK` means
that the frame was queued, **not** that a user saw the image. `MAPLE_EAGAIN`
means the device frame was busy, and `MAPLE_EINVALID` indicates an unsupported
device. Do not write VMU filesystem blocks or toggle capacity as part of an LCD
lesson.

Before implementation, establish a trustworthy test oracle:

1. Determine whether the installed Flycast v2.7 Flatpak can expose a VMU LCD
   view, and whether the existing window-capture runner can capture it. The
   main PVR screenshot is *not* a VMU-screen capture. This is **not yet
   verified**; do not guess from a Flycast startup log listing a VMU.
2. If a VMU view cannot be captured, offer two distinct gates: host bitmap
   tests (192-byte layout, bit order and a golden preview), plus a pinned KOS
   boot that checks detection and queued-call results. Label visual output
   **unverified** until a manual physical VMU check or a reproducible emulator
   LCD-window capture is obtained.
3. For the actual lesson, select a specific physical controller subslot,
   verify LCD capability at every use, never keep a borrowed Maple pointer
   across detach, pace updates and handle `MAPLE_EAGAIN` without spinning or
   blocking an IRQ. Test missing, unsupported and busy states. Do not add a
   save-game lesson or perform destructive VMU writes.

See the [VMU LCD testability assessment](vmu-lcd-testability.md) for the
versioned evidence and test matrix.

**Blocker:** verifying *visible LCD pixels*, not the existence of a KOS call.
No VMU LCD executable should claim to be validated until a second display
oracle has been identified and exercised. A KOS build proves API/link
compatibility only; host bitmap tests prove pixels sent, not pixels shown.
