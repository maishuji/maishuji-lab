# Dreamcast serial logs with Flycast

This guide records the serial/logging findings from the Phase 1 PVR work so
they can be reused in another Dreamcast template project. It covers two
different channels that are easy to confuse:

1. Flycast's own emulator log, which includes messages such as
   `REIOS: Booting up`.
2. The Dreamcast program's KOS `dbgio` output, which is where `printf()` and
   `dbglog()` write.

Seeing Flycast boot messages proves that the emulator started the CDI. It does
not prove that the guest program's KOS log output is connected to the same
terminal.

## What was verified

The installed Flycast Flatpak is v2.7. Its debug configuration includes:

- `Debug:SerialConsoleEnabled=true`: request that the Dreamcast serial console
  be dumped to Flycast's stdout.
- `Debug:SerialPTY=true`: request a PTY-backed serial console. The option is
  present, but this setup did not expose a stable PTY path that can be copied
  into a template, so this guide does not assume one.

The command-line form uses a colon between the section and key:
`-config "Debug:SerialConsoleEnabled=true"`. Flycast's source defines the
corresponding options in [`core/cfg/option.cpp`](https://github.com/flyinghead/flycast/blob/master/core/cfg/option.cpp).

In this project, enabling `Debug:SerialConsoleEnabled=true` did not make the
raw KOS `dbglog()` markers appear in Flycast stdout. The marker strings were
still useful when a real serial/dcload console was connected, but they were not
a reliable Flycast-only signal in the tested Flatpak environment. Keep this
distinction in future templates instead of treating Flycast's emulator log as
the guest serial log.

## Basic Flycast command

Build the self-booting image in the pinned development container, then launch
it from the host. Put Flycast options before the CDI path:

```sh
make dreamcast-cdi

flatpak run \
  --filesystem="$PWD/build-dreamcast:ro" \
  org.flycast.Flycast \
  -config "Debug:SerialConsoleEnabled=true" \
  -config "window:title=MAISHUJI_PVR_SMOKE" \
  "$PWD/build-dreamcast/maishuji-pvr-smoke.cdi"
```

To capture Flycast's stdout/stderr for inspection:

```sh
flatpak run \
  --filesystem="$PWD/build-dreamcast:ro" \
  org.flycast.Flycast \
  -config "Debug:SerialConsoleEnabled=true" \
  "$PWD/build-dreamcast/maishuji-pvr-smoke.cdi" 2>&1 | tee flycast.log
```

The resulting log can contain Flycast/REIOS output and may contain guest
serial output if the guest's KOS debug device is actually connected to the
emulated serial path. Do not use the presence of `REIOS: Booting up` as proof
that `dbglog()` is working.

## KOS side: what `dbglog()` uses

KOS diagnostics are routed through the current `dbgio` device. A minimal log
call is:

```cpp
#include <kos.h>

void log_startup() {
    dbglog_set_level(DBG_INFO);
    dbglog(DBG_NOTICE, "app: booted\n");
}
```

Useful levels include `DBG_ERROR`, `DBG_WARNING`, `DBG_NOTICE`, and
`DBG_INFO`. Keep a short, unique prefix on every runtime marker so a harness
can search for it without depending on unrelated KOS or emulator text.

If a template needs an explicit output device, select it deliberately and test
the choice on the target transport:

```cpp
// On-screen diagnostic output; it can interfere with rendered frames.
dbgio_dev_select("fb");

// Physical Dreamcast serial output; do not force this when no serial path is
// present because writes can time out and KOS may disable the device.
dbgio_dev_select("scif");
```

For dcload-ip development, prefer the dcload-provided console path when it is
available instead of forcing the physical SCIF device. The exact active
device depends on how dcload and the startup configuration are installed, so
verify it with the actual Dreamcast/dcload setup used by the template.

## This repository's runtime markers

The raw PVR smoke emits these markers:

```text
maishuji-lab: runtime probes passed
maishuji-lab: normal shutdown passed
maishuji-lab: global destructor passed
```

They cover different points in the target lifetime:

- runtime probes completed before triangle submission;
- the explicit PVR idle boundary and shutdown path completed;
- a global RAII object was destroyed during normal process exit.

The Flycast test script captures Flycast output in a temporary log and can
optionally require all three markers:

```sh
make dreamcast-cdi
FLYCAST_REQUIRE_RUNTIME_MARKERS=1 make flycast-smoke
```

`FLYCAST_REQUIRE_RUNTIME_MARKERS=1` only changes the pass/fail gate; it does
not itself enable Flycast's serial-console option. The current script launches
Flycast with its isolated log capture and window title, so a template that
needs guest serial output should add
`-config "Debug:SerialConsoleEnabled=true"` to its launcher and verify that
the selected KOS `dbgio` route is visible there.

Use that mode only when the selected serial/dcload route is known to deliver
KOS output into the captured Flycast log. In the tested Flatpak setup, the
default is intentionally render-gated:

```sh
make dreamcast-cdi
make flycast-smoke
```

The implementation is in
[`tools/test-flycast-render.sh`](../tools/test-flycast-render.sh). It waits
for stable rendered frames by default; marker checking is an additional gate,
not a replacement for the frame check.

## Real Dreamcast with dcload-ip

For a physical console, the normal development path is:

```sh
make run-dc DC_IP=YOUR_DREAMCAST_IP
```

Use the dcload console or the serial terminal attached to the configured
debugging path to read the KOS output. If no application markers appear:

1. Confirm that the new ELF actually booted rather than an older image.
2. Confirm the selected `dbgio` device and its initialization order.
3. Check that the log level allows the selected `dbglog()` level.
4. Add one early marker immediately after startup and one marker before the
   rendering loop.
5. Only then enable strict marker gating in an automated test.

Avoid routing normal diagnostics to the framebuffer in a rendered-frame test:
the text can change the pixels being checked. Likewise, do not force SCIF in a
template that is normally used through dcload-ip unless the serial transport
is part of the documented hardware setup.

## Recommendations for a reusable template

For a future Dreamcast template, keep logging transport and test policy
separate:

- provide a small `log_init()` function that selects the intended KOS debug
  device for the target transport;
- use stable, namespaced marker strings for lifecycle milestones;
- make Flycast serial-console enabling an explicit launcher option;
- preserve a render-only test mode for emulators where guest serial output is
  not surfaced;
- enable strict marker gating only for a known-good serial/dcload harness;
- retain the Flycast log file on failure instead of deleting it before the
  diagnostic output has been inspected.

This keeps missing serial output from being mistaken for a PVR rendering or
shutdown failure, while still allowing the same markers to become a strong
runtime assertion on real hardware.
