# Dreamcast toolchain

The supported initial build environment is the Linux/amd64 Dreamcast image pinned by digest in [`tools/toolchain.lock`](../tools/toolchain.lock). The same digest is used by the VS Code development container and CI. Do not combine KOS headers, static libraries, or SH-4 flags from different toolchain installations.

The image is based on the tag `maishuji/dc-kos-image:15.2.1-dev-08feb26-gdb-kp08feb26`, whose inspected manifest digest is `sha256:21832edbd57c4eb91b316c61b61008a64344703f476197887601aea5422b9f3f`. Its verified configuration is:

| Component | Pinned value |
| --- | --- |
| Platform | `linux/amd64` |
| KallistiOS | `2.2.2`, source snapshot `08FEB26`, commit `0aa363a145ead0c6549e77bc7f468dfd8e10134f` |
| SH compiler | GCC `15.2.1` (20260214 build) |
| Binutils | `2.45.1` |
| Newlib | `4.6.0` |
| SH-4 sub-architecture | `pristine` |
| SH-4 floating-point ABI | `-m4-single-only` |
| CMake | `3.31.4` |
| GNU Make | `4.4.1` |
| Extra tools | `mkdcdisc` and `dc-tool-ip` |

The exact lock source is `tools/toolchain.lock`. It records the KOS snapshot tag and the corresponding full Git commit. The image omits Git metadata, so `tools/check-toolchain.sh` validates the KOS version, headers, library, compiler, linker, Newlib, and ABI flags from inside it; the immutable image digest is pinned independently in both the devcontainer and CI. `tools/with-kos.sh` clears inherited KOS settings and sources the container's `/opt/toolchains/dc/kos/environ.sh` once in the same shell process that runs the requested command. It takes an optional `KOS_ENV` override for diagnostics, but any override must still pass the exact-version checks.

## First use

Open this checkout in VS Code and run **Dev Containers: Reopen in Container**. The container runs the same version check after creation. From a terminal, run:

```sh
./tools/with-kos.sh ./tools/check-toolchain.sh
```

The check validates KOS's version, the compiler, ABI, linker, Newlib, CMake, and Make against the lock file. It reports whether the optional packaging and hardware upload tools are available.

Docker users can explicitly fetch the locked environment with:

```sh
docker pull maishuji/dc-kos-image@sha256:21832edbd57c4eb91b316c61b61008a64344703f476197887601aea5422b9f3f
```

Dreamcast builds and CI use this container. A host's preinstalled KOS is not an interchangeable substitute: for example, the observed `/opt/toolchains/dc/kos` on the workstation reports KOS `2.3.0`, GCC `15.2.0`, and `-m4-single`. Host C++ builds use the native compiler and never link target KOS libraries.

The installed template originally used different image tags for development and CI. This project pins the same manifest for both. The newer `16.2.0-06sep26-kp18jul26` image is available on the workstation, but its GCC 16.2.0 toolchain falls back to the `-m4-single-only` ABI. Keep the established environment fixed until a compiler and ABI upgrade has passed the same checks and a target smoke run.

GCC `15.2.1` in the image does not support `-m4-single`; the image's KOS libraries use `-m4-single-only`. KOS's environment script probes the former, emits a warning, and correctly falls back to the latter. The wrapper disables core dumps while loading KOS, and the lock check verifies that effective compile and link flags use the pinned ABI. Keep target builds inside this image because the workstation's separate KOS installation uses a different ABI.

The project minimum is CMake `3.13`, matching the minimum required by KOS's supplied CMake toolchain. The pinned image provides CMake `3.31.4`. Dreamcast builds use KOS's `kallistios.toolchain.cmake` file and Unix Makefiles; host builds use a separate build directory and native compiler.
