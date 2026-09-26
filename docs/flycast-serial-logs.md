# Dreamcast serial logs with Flycast

This guide records the serial/logging findings from the raw PVR work so
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

- `config:Debug.SerialConsoleEnabled=yes`: request that the Dreamcast serial
  console
  be dumped to Flycast's stdout.
- `config:Debug.SerialPTY=yes`: request a PTY-backed serial console. The option is
  present, but this setup did not expose a stable PTY path that can be copied
  into a template, so this guide does not assume one.

The command-line form uses a colon between the section and key. Because these
options are stored in Flycast's `[config]` section, the complete command-line
form is `-config "config:Debug.SerialConsoleEnabled=yes"`. Flycast's source
defines the corresponding options in
[`core/cfg/option.cpp`](https://github.com/flyinghead/flycast/blob/master/core/cfg/option.cpp).

The first command tested in this project used `Debug:SerialConsoleEnabled=true`,
which targeted the wrong section and did not enable the feature. With the
corrected `config:Debug.SerialConsoleEnabled=yes` form, Flycast v2.7 emitted
the guest KOS startup console into the captured output. This means the option
does work for the intended serial-console path; it is not merely a label for
Flycast's own emulator log.

## Basic Flycast command

Build the self-booting image in the pinned development container, then launch
it from the host. Put Flycast options before the CDI path:

```sh
make dreamcast-cdi

flatpak run \
  --filesystem="$PWD/build-dreamcast:ro" \
  org.flycast.Flycast \
  -config "config:Debug.SerialConsoleEnabled=yes" \
  -config "window:title=MAISHUJI_PVR_SMOKE" \
  "$PWD/build-dreamcast/maishuji-pvr-smoke.cdi"
```

To capture Flycast's stdout/stderr for inspection:

```sh
flatpak run \
  --filesystem="$PWD/build-dreamcast:ro" \
  org.flycast.Flycast \
  -config "config:Debug.SerialConsoleEnabled=yes" \
  "$PWD/build-dreamcast/maishuji-pvr-smoke.cdi" 2>&1 | tee flycast.log
```

The resulting log can contain Flycast/REIOS output and may contain guest
serial output if the guest's KOS debug device is actually connected to the
emulated serial path. Do not use the presence of `REIOS: Booting up` as proof
that `dbglog()` is working.

## Flycast behavior and known caveats

Flycast's own README points users to its
[configuration wiki](https://github.com/TheArcadeStriker/flycast-wiki/wiki/Configuration-files-and-command-line-parameters)
for these debug-only settings. The wiki describes
`Debug.SerialConsoleEnabled` as a serial-cable option and `Debug.SerialPTY` as
a Linux/Unix debugging option; they are not normal graphics or game settings.

The serial option is transient when passed with `-config`, so it does not need
to modify `emu.cfg`. If editing `emu.cfg` instead, do it while Flycast is
closed: Flycast rewrites the file on exit, which can silently undo a manual
change. The setting belongs in the `[config]` section:

```ini
[config]
Debug.SerialConsoleEnabled = yes
```

There are two observable output paths during a KOS boot:

- early startup text may be sent to the framebuffer console, so it is visible
  in the Flycast window rather than in the serial log;
- later guest serial output is forwarded by Flycast when the serial console is
  enabled.

Guest output is also buffered. For a marker that must survive a crash or an
immediate emulator stop, flush after writing it:

```cpp
dbglog(DBG_NOTICE, "mygame: reached renderer init\n");
dbgio_flush();
```

The KOS `dbglog()` macro ultimately writes through `printf()` to the active
`dbgio` device, so this is still subject to the selected KOS backend. Flycast
can only forward bytes that the guest actually sends to its emulated serial
device.

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

`FLYCAST_REQUIRE_RUNTIME_MARKERS=1` only changes the pass/fail gate. The current
script now enables Flycast's serial-console forwarding in addition to its
isolated log capture and window title. This does not guarantee guest output:
the selected KOS `dbgio` route must still deliver bytes to the emulated serial
path.

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

The same launcher accepts FLYCAST_FRAME_CHECKER so examples can use a
capability-specific pixel checker. The textured example has its own CDI target
and checker:

~~~sh
make dreamcast-textured-cdi
make flycast-textured-quad
~~~

That check samples the centers of the three 176x176 quads and the surrounding
background after normalizing the capture to 640x480. It proves that the
textured example is visibly rendering in Flycast, but it does not establish
Dreamcast hardware timing, exact alpha/blend equations, or long-run VRAM
accounting.

The pixel-sprite example has a matching capability-specific check:

~~~sh
make dreamcast-pixel-sprites-cdi
make flycast-pixel-sprites
~~~

It samples broad regions around the moving sprites after normalizing the
capture to 640x480. The left region must be red-dominant, the right region must
be green/blue-dominant, and the surrounding samples must remain dark. The
example renders a finite 96-frame sequence and exits, so this target requires
one passing capture; a three-sample run can race the emulator closing its game
window.

## Recorded textured runtime check

On 2026-09-25, the pinned Release textured CDI passed the host render gate with
Flycast v2.7:

~~~sh
make flycast-textured-quad FLYCAST_STABLE_SAMPLES=3
~~~

The gate required three consecutive captures and reported:

~~~text
PASS: Flycast rendered opaque, punch-through, and translucent textured quads.
  opaque center:      0.237 0.237 0.474
  punch center:       0.237 0.237 0.474
  translucent center: 0.237 0.237 0.474
  above quads:        0.020 0.020 0.000
  left of quads:      0.020 0.020 0.000
  right of quads:     0.020 0.020 0.000
  below quads:        0.020 0.020 0.000
Stable frames: 3
Runtime probes: render-gated
~~~

The guest log also reported maishuji: textured quad passed (8 texture cycles).
This confirms the repeated allocation/upload/draw/release workload reached
normal completion while the captured frame showed all three list variants.
The test is render-gated rather than marker-gated, so the serial-forwarding
caveats above still apply. A preceding attempt failed in the host X11 window
capture step; the clean rerun passed without changing the CDI.

## Recorded pixel-sprite runtime check

On 2026-09-26, the pinned Debug pixel-sprite CDI passed the host render gate
with Flycast v2.7:

~~~sh
make flycast-pixel-sprites
~~~

The target reported:

~~~text
PASS: Flycast rendered both atlas sprite regions.
  left sprite region:  0.181 0.055 0.080
  right sprite region: 0.055 0.181 0.165
  above sprites:       0.020 0.020 0.000
  below sprites:       0.020 0.020 0.000
Stable frames: 1
Runtime probes: render-gated
~~~

The guest log also reported `maishuji: pixel sprites passed (96 frames;
snapped versus subpixel)` and exited with return code 0. This confirms that
Flycast booted the CDI, completed the finite frame sequence, and displayed the
two atlas regions with the expected color roles. It does not establish
Dreamcast hardware timing, exact texture sampling, or physical-console output.

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
