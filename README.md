# maishuji-lab

A small C++20 learning framework over KallistiOS for exploring Sega Dreamcast hardware. The project aims to make KOS easier to use while keeping PVR lists, textures, memory, submission, and synchronization visible.

## Set up the development environment

Open this repository in VS Code and choose **Dev Containers: Reopen in Container**. The development container and CI use the same image digest and KOS toolchain. See [`docs/toolchain.md`](docs/toolchain.md) for the pinned versions and setup checks.

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

To send the ELF to a Dreamcast running `dcload-ip` over a Broadband Adapter, provide the console's address:

```sh
make run-dc DC_IP=192.168.0.84
```

CI builds Debug and Release ELFs in the pinned container and publishes each as a build artifact. Cross-compilation confirms the toolchain and linker setup; boot and rendering still need an emulator or Dreamcast hardware.
