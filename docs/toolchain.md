# Dreamcast toolchain

The supported initial build environment is the Linux/amd64 Dreamcast image pinned by digest in [`tools/toolchain.lock`](../tools/toolchain.lock). The same digest is used by the VS Code development container and CI. Do not combine KOS headers, static libraries, or SH-4 flags from different toolchain installations.

The image is based on the tag `maishuji/dc-kos-image:16.2.0-06sep26`, whose inspected manifest digest is `sha256:f89d754629d003f842f8e3a37e5082669542ef965123380a5541157138064f3c`. Its verified configuration is:

| Component | Pinned value |
| --- | --- |
| Platform | `linux/amd64` |
| KallistiOS | `2.3.0`, source snapshot `06SEP26`, commit `63702a858c17c915b564b378407b1576c78668ec` |
| SH compiler | GCC `16.2.0` |
| Binutils | `2.47.20260726` |
| Newlib | `4.6.0` |
| SH-4 sub-architecture | `pristine` |
| SH-4 floating-point ABI | `-m4-single` |
| CMake | `3.31.4` |
| GNU Make | `4.4.1` |
| Extra tools | `mkdcdisc` and `dc-tool-ip` |

The exact lock source is `tools/toolchain.lock`. It records the KOS snapshot tag and the corresponding full Git commit. The image omits Git metadata, so `tools/check-toolchain.sh` validates the KOS version, headers, library, compiler, linker, Newlib, and ABI flags from inside it; the immutable image digest is pinned independently in both the devcontainer and CI. `tools/with-kos.sh` clears inherited KOS settings and sources the container's `/opt/toolchains/dc/kos/environ.sh` once in the same shell process that runs the requested command. KOS's script selects a floating-point ABI from its own compiler probe, so the wrapper reapplies the locked ABI to both compile and link flags afterward. It takes an optional `KOS_ENV` override for diagnostics, but any override must still pass the exact-version checks.

## First use

Open this checkout in VS Code and run **Dev Containers: Reopen in Container**. The container runs the same version check after creation. From a terminal, run:

```sh
./tools/with-kos.sh ./tools/check-toolchain.sh
```

The check validates KOS's version, the compiler, ABI, linker, Newlib, CMake, and Make against the lock file. It reports whether the optional packaging and hardware upload tools are available.

When a mismatch is detected outside a Docker or Dev Container environment, the checker prints a host-toolchain hint. Local installations remain supported when every locked component matches; the hint is diagnostic rather than a prohibition.

## Run Dreamcast Make targets from the host

Dreamcast build and packaging targets require the pinned KOS image. The generic `in-container` target dispatches a Make goal through that image when called from the host and runs the goal directly when already inside a Docker or Dev Container:

```sh
make in-container TARGET=dreamcast-build DC_BUILD_TYPE=Debug
make in-container TARGET=dreamcast-model-loader-cdi
```

CDI packaging targets pass `--allow-overwrite` to `mkdcdisc` by default because
each target owns a deterministic output path and rebuilding it should refresh
that image. Set `MKDCDISC_OPTIONS=` when an existing CDI must be preserved.

Additional Make variable assignments are forwarded to the inner Make process. The dispatcher mounts the repository at `/workspace`, uses the locked `linux/amd64` image, and maps the current user and group so generated artifacts remain writable by the host. Set `CONTAINER_RUNTIME=podman` when using a compatible Podman installation instead of Docker.

The dispatcher rejects `flycast-*` and `host-*` goals because those require the workstation display, Flatpak, or native compiler. After producing a CDI, run its Flycast target directly on the host, for example `make flycast-model-loader`.

Docker users can explicitly fetch the locked environment with:

```sh
docker pull maishuji/dc-kos-image@sha256:f89d754629d003f842f8e3a37e5082669542ef965123380a5541157138064f3c
```

Dreamcast builds and CI use this container. The workstation KOS installation is not an interchangeable substitute: it reports KOS `2.3.0`, GCC `15.2.0`, and `-m4-single`, while the pinned image uses GCC `16.2.0` and the complete locked image stack. Host C++ builds use the native compiler and never link target KOS libraries.

The installed template originally used different image tags for development and CI. This project pins the same KOS `2.3.0` manifest for both. The locked image uses GCC `16.2.0`, KOS snapshot `06SEP26`, and `-m4-single`; keep target builds inside this complete environment so the compiler, headers, libraries, and ABI remain aligned.

KOS environment scripts probe `-m4-single`, which is also the ABI locked by this image. The wrapper removes both single-precision variants before reapplying the locked `-m4-single` flag to compile and link commands. The lock check rejects `-m4-single-only` as the conflicting ABI. Keep target builds inside this image because the workstation GCC and KOS versions differ from the pinned stack.

The project minimum is CMake `3.13`, matching the minimum required by KOS's supplied CMake toolchain. The pinned image provides CMake `3.31.4`. Dreamcast builds use KOS's `kallistios.toolchain.cmake` file and Unix Makefiles; host builds use a separate build directory and native compiler.

## CI build evidence

Each pinned Debug and Release Dreamcast job publishes the four example ELFs,
their linker map files, a target-size summary, a copy of tools/toolchain.lock,
and target-build-manifest.txt. The manifest records the checked-out source
commit, requested build type, verified KOS/compiler versions, working-tree
status, ELF sizes, and the expected map files. The CI workflow intentionally
does not publish CDIs: the current images are large, and emulator output is
recorded as a separate runtime result rather than as a cross-build artifact.

To reproduce the manifest inside the pinned image after a target build:

~~~sh
./tools/with-kos.sh ./tools/write-target-build-manifest.sh \
  build-dreamcast Release
~~~

The script fails if an expected ELF or linker map is missing, so an artifact
can be inspected without assuming that a successful compiler exit produced all
evidence files.
