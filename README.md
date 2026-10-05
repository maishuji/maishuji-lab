# maishuji-lab

A small C++20 learning framework over KallistiOS for exploring Sega Dreamcast hardware. The project aims to make KOS easier to use while keeping PVR lists, textures, memory, submission, and synchronization visible.

## Set up the development environment

Open this repository in VS Code and choose **Dev Containers: Reopen in Container**. The development container and CI use the same image digest and KOS toolchain. See [`docs/toolchain.md`](docs/toolchain.md) for the pinned versions and setup checks, and [`docs/kos-backend.md`](docs/kos-backend.md) for the raw KOS/PVR boundary and lifetime rules.
For a visual overview, see [the PVR architecture diagrams](docs/architecture-diagrams.md).

Verify the environment in the integrated terminal:

```sh
make check-toolchain
```

## Build

Build and run the small C++20 host compiler check:

```sh
make host-run
make host-test
```

Dreamcast cross-build and packaging targets use the pinned KOS container. From the host, dispatch them without opening a separate container terminal:

```sh
make in-container TARGET=dreamcast-model-loader-cdi
make flycast-model-loader
```

You can forward Make variables to the container, for example `make in-container TARGET=dreamcast-build DC_BUILD_TYPE=Debug`. When already inside the Dev Container, the dispatcher runs the target directly. Flycast and native host targets remain local.

See [docs/lifecycle.md](docs/lifecycle.md) for the lifecycle mapping and ownership rules, [docs/primitives.md](docs/primitives.md) for colored geometry, [docs/textures.md](docs/textures.md) for texture memory and uploads, [docs/pixel-art.md](docs/pixel-art.md) for logical pixel coordinates, [docs/spatial-math.md](docs/spatial-math.md) for transforms and cameras, [docs/meshes.md](docs/meshes.md) for indexed projection, [docs/textured-3d.md](docs/textured-3d.md) for the textured 3D lesson, [docs/heightmapped-terrain.md](docs/heightmapped-terrain.md) for the heightmapped terrain lesson, [docs/terrain-walk.md](docs/terrain-walk.md) for controller movement and height sampling, [docs/mipmap-texture.md](docs/mipmap-texture.md) for mipmap allocation and filtering, [docs/basic-3d.md](docs/basic-3d.md) for the 3D example, [docs/fog.md](docs/fog.md) for camera-space linear fog, [docs/text.md](docs/text.md) for BIOS-font text as a PVR texture, [docs/particles.md](docs/particles.md) for repeated textured submissions and packet budgets, [docs/budgets.md](docs/budgets.md) for the mixed-list packet budget lesson, [docs/advanced-text.md](docs/advanced-text.md) for the multilingual atlas example, and [docs/measurement.md](docs/measurement.md) for the raw-versus-wrapper cost comparison.
See [docs/multi-texture.md](docs/multi-texture.md) for the two-texture submission lesson and [docs/tile-workload.md](docs/tile-workload.md) for the PVR tile-coverage comparison.
See [docs/lighting.md](docs/lighting.md) for the offset-color lighting lesson and its KOS polygon-header mapping.
See [docs/frustum-culling.md](docs/frustum-culling.md) for the advanced object-level frustum-culling example, fixed camera diagram, and CPU/PVR cost boundary.

For PNG-to-PVR conversion and the generated asset workflow, see [docs/pvr-assets.md](docs/pvr-assets.md).

Build the PVR smoke example for Dreamcast:

```sh
make dreamcast-build DC_BUILD_TYPE=Debug
make dreamcast-build DC_BUILD_TYPE=Release
```

The raw reference output is build-dreamcast/maishuji-pvr-smoke.elf. The lifecycle example is build-dreamcast/maishuji-hello-pvr.elf; the colored primitive example is build-dreamcast/maishuji-colored-primitives.elf; the textured example is build-dreamcast/maishuji-textured-quad.elf; the pixel-sprite example is build-dreamcast/maishuji-pixel-sprites.elf; the basic-3D example is build-dreamcast/maishuji-basic-3d.elf; the fogged
3D example is build-dreamcast/maishuji-fogged-3d.elf; the text example is build-dreamcast/maishuji-pvr-text.elf; the particle example is build-dreamcast/maishuji-particle-batch.elf; the packet-budget example is build-dreamcast/maishuji-pvr-budget.elf; the advanced multilingual text example is build-dreamcast/maishuji-advanced-multilingual-text.elf; the PVR asset example is build-dreamcast/maishuji-pvr-asset.elf; the textured 3D example is build-dreamcast/maishuji-textured-3d.elf; the heightmapped terrain example is build-dreamcast/maishuji-heightmapped-terrain.elf; the terrain walk example is build-dreamcast/maishuji-terrain-walk.elf. The mipmap texture example is build-dreamcast/maishuji-mipmap-texture.elf. The raw example initializes video and PVR, then displays a colored triangle. The pixel-sprite example compares logical-grid snapping with direct subpixel output coordinates using a deterministic two-cell texture atlas. The basic-3D example projects a colored indexed cube through a camera and model transform.
The PVR lighting example compares ordinary Gouraud vertex colors with per-vertex additive offset colors. The PVR text example rasterizes the BIOS font into an ARGB4444 texture and submits it through the punch-through list. The particle example submits 24 moving textured quads through one punch-through list and documents the structural packet count. The packet-budget example submits a fixed mix through all three lists and reports its structural header, vertex, primitive-call, and packet-byte budget. The advanced multilingual text example displays Japanese, Traditional Chinese, and English through a small open-source glyph atlas.
The PVR asset example parses a generated square-twiddled PVRT file,
detwiddles its ARGB4444 payload, uploads it through the texture API, and draws the transparent controller over a colored background. The textured 3D example reuses that asset on a rotating indexed cube through the textured mesh API; see [docs/textured-3d.md](docs/textured-3d.md). The terrain walk example reuses the heightmapped rock surface, samples its elevation under a controller-driven player, and follows that player with a camera; see [docs/terrain-walk.md](docs/terrain-walk.md). The mipmap texture example compares base-level and mipmapped bilinear sampling on the same perspective runway; see [docs/mipmap-texture.md](docs/mipmap-texture.md).

To send the ELF to a Dreamcast running `dcload-ip` over a Broadband Adapter, replace the placeholder with the console's address:

```sh
make run-dc DC_IP=YOUR_DREAMCAST_IP
```

For the Flycast serial-console and KOS `dbgio` findings, including the
limitations of guest log capture in the tested Flatpak setup, see
[`docs/flycast-serial-logs.md`](docs/flycast-serial-logs.md).

To create the default PVR smoke self-booting CDI, use the pinned container for packaging. If Flycast is installed as a Flatpak, launch it with read-only access to the build output:

```sh
make dreamcast-smoke-cdi
flatpak run --filesystem="$PWD/build-dreamcast:ro" org.flycast.Flycast \
  "$PWD/build-dreamcast/maishuji-pvr-smoke.cdi"
```

To check the rendered triangle automatically, first create the CDI in the pinned development container, then run the Flycast pixel test from the Linux host. It requires the Flycast Flatpak, an X11/XWayland display, `xdotool`, `xwininfo` (usually provided by `x11-utils`), and ImageMagick 6 or 7. Close any existing Flycast instance before the test. The test waits up to 30 seconds for three consecutive passing captures by default, checking averaged color regions inside and outside the triangle. The raw smoke gates rendering on its target runtime probes before submitting the triangle. It writes a screenshot beside the CDI, leaving the passing frame or last render-timeout frame for inspection. The launcher enables Flycast serial-console forwarding; set `FLYCAST_STABLE_SAMPLES` to adjust the required number of consecutive captures, and set `FLYCAST_REQUIRE_RUNTIME_MARKERS=1` to require the KOS log markers too.

```sh
make dreamcast-smoke-cdi
make flycast-smoke
```

For the lifecycle example, package the CDI and run its dark-frame check
with an exact guest completion marker:

~~~sh
make dreamcast-lifecycle-cdi
make flycast-lifecycle
~~~

For the textured example, package the CDI in the pinned container and run
the capability-specific Flycast check on the host:

~~~sh
make dreamcast-textured-cdi
make flycast-textured-quad
~~~

For the pixel-sprite example, package the CDI and run the capability-specific
Flycast check on the host:

~~~sh
make dreamcast-pixel-sprites-cdi
make flycast-pixel-sprites
~~~

For the basic-3D example, package the CDI and run the projected-mesh
Flycast check on the host:

~~~sh
make dreamcast-basic-3d-cdi
make flycast-basic-3d
make dreamcast-fogged-3d-cdi
make flycast-fogged-3d
~~~
For the PVR lighting example, package the CDI in the pinned container and
run its capability-specific Flycast check:

~~~sh
make dreamcast-lighting-cdi
make flycast-lighting
~~~

For the PVR text example, package the CDI in the pinned container and run its
capability-specific Flycast check:

~~~sh
make dreamcast-text-cdi
make flycast-text
~~~

The example rasterizes two lines with KOS's BIOS font into a 256x64 ARGB4444
texture, scales that texture 2x in the PVR, and uses the punch-through list so
transparent texels do not cover the background. Emulator validation still does
not replace real Dreamcast hardware validation.

The example uses the smallest valid 8x8 ARGB4444 texture so its textured
vertices carry KOS offset colors. The checker verifies that the base quad is
visible, the lit quad is brighter, and its four corners retain the expected
red, green, blue, and purple offset roles. It also requires the exact guest
completion marker. Emulator validation still does not replace real Dreamcast
hardware validation.

For the particle-batch example, package the CDI in the pinned container and run
its capability-specific Flycast check:

~~~sh
make dreamcast-particles-cdi
make flycast-particles
~~~

The example uses one 16x16 circular texture and submits 24 independently
positioned quads per frame. This is a teaching batch in one render-list scope,
not a new hardware particle primitive or a performance benchmark.

For the packet-budget example, package the CDI and run its capability-specific Flycast check:

~~~sh
make dreamcast-budget-cdi
make flycast-budget
~~~

The example uses six submissions across opaque, punch-through, and translucent lists. Its 6 headers, 21 vertices, and 27 primitive calls are source-derived structural estimates; they do not measure timing, VRAM high-water, DMA, or real Dreamcast throughput.

For the advanced multilingual text example:

~~~sh
make dreamcast-advanced-text-cdi
make flycast-advanced-text
~~~

It uses an open-source Noto-derived ARGB4444 atlas and a deliberately narrow UTF-8 decoder; see [docs/advanced-text.md](docs/advanced-text.md) for the asset notice and limitations.

For the PVR asset example:

~~~sh
make dreamcast-pvr-asset-cdi
make flycast-pvr-asset
~~~

For the textured 3D example, package the CDI in the pinned container and run
the fixed-pose rotating-cube check:

~~~sh
make dreamcast-textured-3d-cdi
make flycast-textured-3d
~~~

For the heightmapped terrain example, package the CDI in the pinned container
and run the stable terrain capture check:

~~~sh
make dreamcast-heightmapped-terrain-cdi
make flycast-heightmapped-terrain
~~~

For the terrain walk example, package the CDI in the pinned container and run
the deterministic controller-and-height-sampling capture check:

~~~sh
make dreamcast-terrain-walk-cdi
make flycast-terrain-walk
~~~

For the mipmap texture example, package the CDI in the pinned container and
run the stable side-by-side sampling check:

~~~sh
make dreamcast-mipmap-texture-cdi
make flycast-mipmap-texture
~~~

The external conversion command and generated-file workflow are documented in [docs/pvr-assets.md](docs/pvr-assets.md). Emulator validation still does not replace real Dreamcast hardware validation.

CI runs the host C++20 smoke check, builds Debug and Release Dreamcast ELFs in the pinned container, and publishes each ELF with its linker map, size summary, toolchain lock, and build manifest. Cross-compilation confirms the toolchain and linker setup; runtime results are recorded separately from build results.

## Advanced model loader

The model-loader lesson converts a readable Wavefront OBJ satellite into a
compact DCM1 asset, validates it on the Dreamcast, expands it into the existing
textured indexed-mesh API, and renders it with a generated 32x32 ARGB4444
texture. It deliberately keeps scene graphs, materials, animation, and texture
conversion out of the target loader; the study and format limits are recorded
in [docs/model-loader.md](docs/model-loader.md).

Regenerate the embedded model header after changing the source OBJ:

~~~sh
python3 tools/obj-to-dcmodel.py \
  advanced_examples/assets/satellite.obj \
  advanced_examples/assets/model_asset.hpp
~~~

Then package and check the lesson with:

~~~sh
make in-container TARGET=dreamcast-model-loader-cdi
make flycast-model-loader
~~~

For the multi-texture lesson, package the CDI in the pinned container and run
the dedicated Flycast checker:

~~~sh
make dreamcast-multi-texture-cdi
make flycast-multi-texture
~~~

For the PVR tile-workload comparison, package the CDI and run the stable
side-by-side coverage check:

~~~sh
make dreamcast-pvr-tile-workload-cdi
make flycast-pvr-tile-workload
~~~

The tile-workload example compares six small, mostly non-overlapping quads
with six nested quads. It estimates 32x32 tile references and maximum layer
depth from the submitted screen-space bounds; these are explanatory counts,
not hardware timing or PVR counter measurements. See
[docs/tile-workload.md](docs/tile-workload.md).
