# Controller input

This document defines the input track's contract. The [lesson index](examples.md)
distinguishes planned lessons from implemented ones. Input sampling is independent
of PVR; rendering only visualizes owned sample values.

## Verified KOS boundary

Source inspection used the pinned KOS 2.3.0 image from
[toolchain.lock](../tools/toolchain.lock), digest
`sha256:f89d754629d003f842f8e3a37e5082669542ef965123380a5541157138064f3c`.
Relevant upstream source locations at the locked commit are:

- [controller.h](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/include/dc/maple/controller.h)
- [controller.c](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/hardware/maple/controller.c)
- [maple_enum.c](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/hardware/maple/maple_enum.c)
- [maple_driver.c](https://github.com/KallistiOS/KallistiOS/blob/63702a858c17c915b564b378407b1576c78668ec/kernel/arch/dreamcast/hardware/maple/maple_driver.c)

These findings come from the installed source in the pinned image, not a hardware
run. Its `cont_state_t` has pressed-high button bits, primary and secondary axes
in `[-128,127]`, and triggers in `[0,255]`. The driver has already inverted the
wire button bits and subtracted 128 from stick bytes. Do not do either again.
The existence of a field does not imply that a particular controller supports it;
capability bits and button-state bits have different layouts.

`maple_enum_dev(port,0)` selects top-level physical A-D ports for indices 0-3.
It does not bounds-check: validate the port before calling. In contrast,
`maple_enum_type(0,MAPLE_FUNC_CONTROLLER)` selects the first matching device,
not necessarily port A. Do not silently change player assignment when another
controller disappears.

### Cached state, lifetime, and synchronization

The pinned implementation of `maple_dev_status()` returns a borrowed mutable
pointer, or null for an invalid/unbound device. Contrary to the public header's
waiting description, this implementation neither waits nor checks freshness.
The driver initially zero-fills status, overwrites it from Maple interrupt
callbacks, and frees it on detach. An all-zero first sample is possible before
the first successful reply; failed input replies can leave stale values.

The adapter must copy status and capability data while lookup and dereferences
are protected. The intended strategy is a short interrupt-disabled section using
public `irq_disable()` / `irq_restore()` from `<kos/irq.h>`, with explicit compiler
memory barriers because the pinned SH-4 wrappers omit a memory clobber. Never
block, allocate, render, log, yield, or retain a borrowed pointer in that section.
Restore the saved interrupt state, not unconditional enable. Process copied
values after restoration. Sampling must not overlap Maple teardown.

This is a coherent copy of cached state, not a new bus transaction, a timestamped
hardware event, or a same-packet capability/status pair. Detection can lag unplugging.
No claim is made that every physical replacement is observable between samples.

## Portable API policy

The library now separates a copied controller snapshot and KOS adapter from
application input mapping. Button transitions and analog processing are
implemented; physical-port dashboard is planned next.
It uses fixed storage, no PVR ownership, and no invented bus initialization API.

- The implemented snapshot API distinguishes disconnected, unsupported, unavailable, valid, and invalid port.
- Invalid/unavailable snapshots have neutral values; applications must avoid using stale prior samples.
- The inspector samples once per frame, then reuses the value.
- `ButtonTracker` seeds history on first valid input without synthesizing presses.
- A lost/unavailable sample resets history; it does not synthesize button releases.
- Recovery seeds history again. Connection changes and button edges are distinct.
- `normalized_stick` divides negative stick endpoints by 128 and positive endpoints by 127;
  triggers divide by 255. Raw values remain visible in the introductory lesson.
- Radial dead-zone processing is explicit, with no automatic smoothing.
- Keep numeric stick Y sign; inversion for a camera or screen is application policy.
- No callback ownership, device registry, action-binding engine, or generic keyboard/
  mouse abstraction is required for these lessons.

## Validation plan

Each implementation step must add and execute focused host tests and build with
the locked KOS toolchain. Tests cover edges, first sample, held buttons, simultaneous
buttons, loss/recovery, endpoints, dead-zone boundaries, diagonals, and port isolation.
A separately labeled scripted mode may exercise the display and deterministic
checks. Never replace an absent live controller with hidden scripted input.

Interactive lessons stay open for inspection; bounded scripted tests are separate.
A screenshot or synthetic sample test is not evidence for physical Maple input.
Verify live Flycast mappings and real-console reconnect behavior separately.
Hardware latency, physical axis direction, freshness, and hotplug timing remain
unverified until explicitly measured on a named configuration.
