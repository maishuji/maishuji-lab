# VMU LCD testability: research gate

[Next-lesson plan](next-lessons.md) · [existing input boundary](input.md)

**Status: no LCD example implemented or LCD visual test run.** This assessment
uses the installed KOS image pinned by [`tools/toolchain.lock`](../tools/toolchain.lock):
KOS 2.3.0, commit `63702a8`, image digest `sha256:f89d754629d003f842f8e3a37e5082669542ef965123380a5541157138064f3c`.
The available emulator is Flycast Flatpak v2.7 (`flatpak info org.flycast.Flycast`,
observed 2026-10-17). Its VMU LCD view/capture path has **not** been confirmed.

## What the pinned API proves

KOS [vmu.h](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/include/dc/maple/vmu.h)
specifies a 48×32 one-bit bitmap (192 bytes). `vmu_draw_lcd()` returns
`MAPLE_EOK`, `MAPLE_EAGAIN` or `MAPLE_EINVALID`. The pinned
[driver](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/hardware/maple/vmu.c)
checks for a VMU, locks its Maple frame, copies 192 bytes into the frame and
queues a `MAPLE_COMMAND_BWRITE`. Therefore `MAPLE_EOK` means **queued**, not
that an LCD update was acknowledged or displayed. `vmu_draw_lcd_rotated()`
changes the orientation and requires four-byte-aligned input. Reacquire device
references between updates and do not hold a borrowed Maple pointer across a
detach. These are source-backed observations, not live-device evidence.

## The test gap

The existing [`test-flycast-render.sh`](../tools/test-flycast-render.sh) discovers
one titled **game** window and captures that window via ImageMagick. A passing
640×480 game frame says nothing about the VMU's 48×32 LCD. Likewise, a Flycast
startup log listing `A1: Visual Memory` does not prove a bitmap appeared. The
specific blocker is an independent, observable LCD output channel in the
installed emulator or access to a physical VMU, not a missing C API.

| Gate | What it proves | Does not prove |
| --- | --- | --- |
| Host-generated 192-byte bitmap + golden preview | Pixel packing, bounds and orientation policy | Maple transfer, visible LCD |
| Pinned KOS Debug/Release cross-build | API/link compatibility | Runtime detection or display |
| Guest log for detected LCD function and `MAPLE_EOK` | Device capability and request queued on that run | Completion or visible pixels |
| Isolated Flycast VMU LCD-window capture, if available | Displayed pixels in that emulator version | Physical VMU output |
| Photo/visual check on a named real VMU | Visible physical output on that setup | All VMU models or hotplug behavior |

## Investigation to unblock a test

1. With Flycast v2.7, determine whether the VMU LCD is exposed as an independent
   window, overlay or accessible debug output. If it is, extend a **separate**
   bounded, instance-scoped capture harness; do not repurpose the game-frame
   screenshot. Use isolated temporary Flycast configuration/data directories so
   testing never reuses the owner's VMU save files.
2. Prepare two distinguishable 48×32 fixtures (e.g. an asymmetric arrow and its
   inverse), assert the exact 192-byte buffers with host tests, and show a
   host preview to a human. Never infer bit orientation solely from a mirror-
   symmetric icon.
3. On KOS, detect an LCD-capable device in an explicitly chosen physical slot;
   send paced changes, treat `MAPLE_EAGAIN` as busy rather than success, and log
   failures. Do not attempt raw VMU filesystem writes, reformatting or capacity
   changes. The [KOS header](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/include/dc/maple/vmu.h)
   warns that incorrect direct block writes can corrupt the filesystem.
4. Record the emulator version, selected subslot, exact capture/reproduction
   steps and actual observed pixels. If no independent LCD capture is possible,
   leave visual acceptance pending until a real VMU test is available; do not
   promote a guest marker to a visual pass.

This is a test-oracle limitation, **not** evidence that VMU LCD support is
broken. No Flycast VMU visual experiment or real-console run was performed in
this investigation.
