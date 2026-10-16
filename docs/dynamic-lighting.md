# Advanced example 11: dynamic lighting and a 2CV ground shadow

Example [11-dynamic-lighting.cpp](../advanced_examples/11-dynamic-lighting.cpp)
shows an original low-poly 2CV-inspired car under an orbiting point light. The
car stays still: its facet colors change and its hard shadow travels around
it on a flat, warm-colored ground. A gold octahedron marks the light; a thin
ground ring shows the orbit's horizontal radius.

**Status:** implementation and assets are present, but generation, compilation,
host tests, and Flycast execution have **not been run** in this session because
command execution became unavailable. The frame-check thresholds are initial,
not calibrated against an actual capture. No physical Dreamcast validation or
frame-rate claim is made.

## Build and use

Host prerequisites are CMake, a C++20 compiler, Make, and Python **3.10 or newer**
(standard library only). Dreamcast builds use the existing
[pinned KOS environment](toolchain.md). CMake invokes the host Python interpreter
even when cross-compiling; Python is not part of the Dreamcast executable.

From the repository root:

```sh
make host-test-dynamic-lighting
make in-container TARGET=dreamcast-dynamic-lighting-cdi
```

The interactive outputs are
`build-dreamcast/maishuji-dynamic-lighting.elf` and
`build-dreamcast/maishuji-dynamic-lighting.cdi`. Open the CDI in Flycast, or use
the existing hardware loader with an actual console address:

```sh
make run-dc DC_ELF=build-dreamcast/maishuji-dynamic-lighting.elf DC_IP=YOUR_DREAMCAST_IP
```

The animation runs without a controller. With a Maple controller:

| Button | Action |
| --- | --- |
| A | Pause/resume the light |
| B | Reverse its direction |
| X | Toggle the ground shadow |
| Start | Exit and release resources |

Animation advances one revolution per 720 successfully submitted frames: 12
seconds **if** the program sustains 60 Hz, 14.4 seconds at 50 Hz. This is an
explicit frame-based teaching animation, not a measured or time-based speed.
The ambient contribution keeps the unlit side readable; headlights and windows
are opaque colored geometry, not additional lights or transparent glass.

## Design and implementation plan

The source is split by responsibility rather than by rendering abstraction:

```text
Authored OBJ + MTL
        |
        v
Host Python conversion (existing Q8.8 position / UNORM16 UV rules)
        |
        v
Build-generated mesh + atlas headers
        |
        v
Startup: expand triangle corners; cache each face normal and centroid
        |
        v
Frame: choose light position
        |
        +--> Lambert intensity --> update car vertex colors
        |
        +--> Project light rays --> update opaque ground-shadow vertices
        |
        v
Opaque list: ground, orbit guide, shadow, car, light marker
        |
        v
Punch-through list: preuploaded title and control labels
```

| Logical step | Files / responsibility | Verification gate |
| --- | --- | --- |
| Author the car | OBJ, MTL, atlas; named parts, grounded wheels, mirrored surfaces | Python geometry, UV, quantization, symmetry, and determinism tests |
| Compute lighting and projection | Example-local [dynamic_lighting.hpp](../advanced_examples/dynamic_lighting.hpp) | Host normal, light, ray, ground-contact, bounds, and full-orbit checks |
| Submit the lesson | Example 11, CMake/Make targets, static texture staging | Debug/Release target builds; resource/status review |
| Check the image | Fixed opposite-light capture target and pixel checker | Three stable Flycast captures plus guest readiness marker |
| Confirm hardware behavior | Same ELF on a physical Dreamcast | Visual depth/bias checks and measured timing, still open |

These steps are implemented in source; the execution gates remain open. No new
public renderer API, normal-bearing model format, scene graph, shadow-map
renderer, or PVR modifier-volume backend was introduced.

## Assets and regeneration

- [car_2cv.obj](../advanced_examples/assets/car_2cv.obj): editable triangle mesh,
  with **+Y up**, **+Z front**, and tires touching `Y=0`.
- [car_2cv.mtl](../advanced_examples/assets/car_2cv.mtl): eight opaque diffuse
  colors. Material order defines the atlas slots; there is no baked lighting.
- [car_2cv_texture.hpp](../advanced_examples/assets/car_2cv_texture.hpp): checked-in
  reference for the 64x32 linear ARGB4444 swatch atlas.
- [car_2cv_preview.svg](../advanced_examples/assets/car_2cv_preview.svg): an
  explicitly labeled **design illustration**, not a projection of the OBJ and
  not a Dreamcast screenshot.
- [generate-2cv-model.py](../tools/generate-2cv-model.py): validates geometry and
  derives mesh, atlas, DCM1 embedding, PPM atlas, and triangle-preview outputs.

The authored source has a tall curved cabin, a canvas-colored roof, separate
wings, four narrow 12-sided wheels, round lamps, glazing patches, grille, and
cream bumpers. It is an original stylized study, not a dimensionally exact
vehicle reconstruction. No downloaded geometry, textures, or brand logos are
included; the repository [MIT license](../LICENSE) applies to these files.

CMake automatically derives assets into `<build-directory>/generated/` and
rebuilds them when OBJ, MTL, or conversion code changes. Preview them separately
without rewriting source assets:

```sh
make car-assets
```

This writes to `build-host/generated/`, including `car_2cv_preview.svg`, an
actual source-triangle projection with host-only flat shading and painter
ordering. That preview is also **not** a Dreamcast render. To select another
output directory explicitly:

```sh
python3 -B tools/generate-2cv-model.py --output-dir build-host/generated
```

`-B` prevents Python bytecode cache files. The tool never overwrites the OBJ
or MTL. Its default output directory is the source asset directory, so pass
`--output-dir` as above to preserve the authored design illustration.

The generated mesh uses the existing [OBJ converter](../tools/obj-to-dcmodel.py)
quantization rules, but exposes constexpr indexed vertices directly. Example 11
needs the geometry for CPU lighting, so it does **not** duplicate the earlier
lesson's DCM1 decoder. The generated DCM1 header remains available for format
experiments but is not included in this executable. The runtime uses the
**build-generated atlas**, not the checked-in reference header, so MTL edits
propagate to the rendering asset.

## Lighting math

At startup, each triangle caches:

```text
normal   = normalize(cross(b - a, c - a))
centroid = (a + b + c) / 3
```

Each frame uses `light = (4*cos(angle), 4.5, 4*sin(angle))` and:

```text
to_light  = normalize(light - centroid)
intensity = 0.30 + 0.70 * max(dot(normal, to_light), 0)
```

All three triangle corners get the same intensity, which the textured PVR path
multiplies with the unlit atlas color. Source indices are expanded into distinct
corners once so adjacent facets can have different colors even when their
source positions are shared. The original colors are retained separately;
shading never accumulates from an already darkened previous frame.

This is CPU-computed flat Lambert shading, without attenuation, gamma
correction, specular highlights, or self-shadowing. The PVR receives ordinary
projected textured vertices; its offset-color mode is not needed. See the
separate [offset-color lesson](lighting.md) and [mesh submission boundary](meshes.md).

## Ground shadow and depth policy

For a source point `p` and light `L`, intersect the ray beyond the point with
the receiver `Y=0`:

```text
amount = p.y / (L.y - p.y)
shadow = p + (p - L) * amount
shadow.y = 0
```

The helper rejects non-finite data, below-ground source points, and a light at
or below the source point. The fixed orbit stays above the entire car. Tire
contact points have `amount=0`, so their shadow positions do not drift.

Rendering lifts this geometric projection to `Y=0.003` to separate it from the
ground. All projected triangles use the **same opaque dark color**, no culling,
`DepthCompare::GreaterOrEqual`, and enabled depth writes. Overlapping triangles
therefore replace the same color rather than repeatedly blending and making
dark seams. The car retains ordinary depth testing, hiding shadow geometry
behind its visible surfaces. The small receiver bias needs visual verification
on Flycast and hardware; it is not a guarantee for arbitrary camera scales.

This works because the receiver is a **uniformly colored plane**. It is not a
multiplicative shadow over arbitrary textures, slopes, or other objects. It
provides hard ground shadows only: no penumbra, terrain following, transparency,
or shadows cast onto the car itself. The 14x14 receiver conservatively contains
the rotating projection in the normal scene and both fixed capture instances.

## Validation and comparison capture

The host tests exercise the actual helper and generated asset, including a
720-frame orbit, unchanged UV/base colors, ground contact, mirrored triangle
surfaces, invalid input, rigid-transform projection equivalence, receiver
containment, and marker framing. They are also included in `make host-test`.

For a stable visual comparison:

```sh
make in-container TARGET=dreamcast-dynamic-lighting-capture-cdi
make flycast-dynamic-lighting
```

This uses `maishuji-dynamic-lighting-capture`, compiled with
`MAISHUJI_DYNAMIC_LIGHTING_CAPTURE=1`. One camera at `(0,6,10)` looks at `(0,1,0)`
with vertical FOV `0.95`. The cars are translated to `X=-3` and `X=+3`, both
with yaw `0.3` radians. Their model-local lights are `(4,4.5,0)` and
`(-4,4.5,0)`. Each instance transforms its car, shadow, and marker together, so
both use the **same public mesh path as the animation**. There is one shared
ground and no overlapping orbit rings. The fixed lights are independent
comparison fixtures, not a physical two-light solution for both cars.

After 120 successful frames and a render-completion wait, the target emits:

```text
maishuji: dynamic lighting capture ready (opposite lights; planar shadows)
```

The [frame checker](../tools/check-flycast-dynamic-lighting-frame.py) looks for
both blue cars, warm ground, gold light markers, and shadow-colored pixels
**outside both conservative car silhouettes** with oppositely displaced
centroids. Missing shadows cannot pass merely because cars are visible.
The [synthetic checker tests](../tests/test_dynamic_lighting_frame.py) cover
negative cases, but do not establish target rendering. The checker is an
initial uncalibrated visibility heuristic, not a reference renderer or proof of
animation. Manually observe a full orbit and the controls separately.

Flycast prerequisites and guest-log limitations are the same as the existing
[render checks](flycast-serial-logs.md). The harness invokes Python checkers via
`python3 -B`; they need not have an executable bit. Its existing executable
contract for shell checkers is unchanged.

### Source-derived budgets, not measurements

The authored tally is 498 triangles / 1,494 indices; tests and static assertions
lock that intended count, but conversion has not been executed here. With all
input triangles surviving clipping, one animated frame contains 1,198 input
triangles: 498 car, 498 shadow, 192 orbit guide, eight marker, and two ground.
At the current one-header/three-vertex submission shape, that is 153,344 nominal
packet bytes before overlays. The three screen quads and two text quads add
800 bytes, for **154,144 nominal bytes**.

The fixed comparison submits 2,010 input triangles and seven quads, giving
**258,400 nominal bytes**. These figures exclude clipping expansion, scene/list
control, tile references, OPB storage, and actual PVR buffer high-water use.
They are neither throughput nor frame-time measurements. The configuration
retains the project's 512 KiB vertex buffer.

The atlas costs 4,096 bytes of PVR texture storage. Four 512x32 ARGB4444 label
textures add 131,072 bytes; one shared 32,768-byte CPU staging array is reused
at startup. Expanded mesh/normal storage is fixed in static memory, not the
SH-4 stack; there is no per-frame heap allocation.

### Evidence boundary

Source review used the repository baseline `903fba7` and the toolchain lock
for KOS **2.3.0**, snapshot **06SEP26**, commit
`63702a858c17c915b564b378407b1576c78668ec`, GCC **16.2.0**.
Python generator/checker editor diagnostics passed after fixes. C++ editor
diagnostics could not resolve the project include path before configuration;
no compilation success is claimed. No asset generation, shell syntax check,
C++/Python test execution, target build, CDI packaging, Flycast run, or real
console run was possible in this session. Run the commands above before treating
this example or its visual thresholds as validated.
