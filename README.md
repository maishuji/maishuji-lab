# maishuji-lab

A small C++20 learning framework over KallistiOS for exploring Sega Dreamcast hardware. The project aims to make KOS easier to use while keeping PVR lists, textures, memory, submission, and synchronization visible.

## Set up the development environment

Open this repository in VS Code and choose **Dev Containers: Reopen in Container**. The development container and CI use the same image digest and KOS toolchain. See [`docs/toolchain.md`](docs/toolchain.md) for the pinned versions and setup checks, and [`docs/kos-backend.md`](docs/kos-backend.md) for the raw KOS/PVR boundary and lifetime rules.

Verify the environment in the integrated terminal:

```sh
make check-toolchain
```

## Build

Build and run the small C++20 host compiler check:

```sh
make host-run
```

Build the PVR smoke example for Dreamcast:

```sh
make dreamcast-build DC_BUILD_TYPE=Debug
make dreamcast-build DC_BUILD_TYPE=Release
```

The output is `build-dreamcast/maishuji-pvr-smoke.elf`. The example initializes video and PVR, then displays a colored triangle. It verifies the C++20 compiler, KOS headers and libraries, CMake cross-compilation, and the SH-4 linker setup.

To send the ELF to a Dreamcast running `dcload-ip` over a Broadband Adapter, replace the placeholder with the console's address:

```sh
make run-dc DC_IP=YOUR_DREAMCAST_IP
```

To create an optional self-booting CDI, use the pinned container for packaging. If Flycast is installed as a Flatpak, launch it with read-only access to the build output:

```sh
make dreamcast-cdi
flatpak run --filesystem="$PWD/build-dreamcast:ro" org.flycast.Flycast \
  "$PWD/build-dreamcast/maishuji-pvr-smoke.cdi"
```

To check the rendered triangle automatically, first create the CDI in the pinned development container, then run the Flycast pixel test from the Linux host. It requires the Flycast Flatpak, an X11/XWayland display, `xdotool`, `xwininfo` (usually provided by `x11-utils`), and ImageMagick 6 or 7. Close any existing Flycast instance before the test. The test waits up to 30 seconds for three consecutive passing captures by default, checking averaged color regions inside and outside the triangle. The raw smoke gates rendering on its target runtime probes before submitting the triangle. It writes a screenshot beside the CDI, leaving the passing frame or last render-timeout frame for inspection. Set `FLYCAST_STABLE_SAMPLES` to adjust the required number of consecutive captures; with a serial/dcload console, set `FLYCAST_REQUIRE_RUNTIME_MARKERS=1` to require the KOS log markers too.

```sh
make dreamcast-cdi
make flycast-smoke
```

CI runs the host C++20 smoke check, builds Debug and Release Dreamcast ELFs in the pinned container, and publishes each target ELF as a build artifact. Cross-compilation confirms the toolchain and linker setup; runtime results are recorded separately from build results.
