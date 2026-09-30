# Terrain walk

advanced_examples/05-terrain-walk.cpp builds on the heightmapped terrain
lesson by putting a player position on the map. The D-pad or analog stick
moves the player, the heightmap supplies the player's Y coordinate, and the
camera follows the player across the terrain.

## Movement and collision

The example reuses the 9x9 heightmap, rock texture, and indexed mesh from
advanced_examples/04-heightmapped-terrain.cpp. `sample_height` converts the
player's world-space X/Z position into clamped heightmap coordinates and
bilinearly interpolates the four surrounding samples. `clamp_player` keeps the
player inside the map margin and applies that sampled height, so the marker
does not fall below or leave the generated surface.

The controller is read through KOS's Maple controller API. D-pad input gives
digital movement and the analog stick contributes continuous movement after a
small dead zone. A scripted walk is used while no input is active, which keeps
the example and its emulator capture deterministic; the first real movement
input switches to live controller movement. The magenta triangle marks the
player position and makes the camera-follow behavior visible.

## Stable emulator validation

The program runs a 240-frame scripted walk, emits the guest completion marker,
and then holds a fixed player pose for 900 frames. The host-side checker waits
for three consecutive captures and checks the tiled terrain, far and near
terrain variation, blue background, and magenta player marker. This separates
interactive input from the deterministic frame used for automated validation.

The source-derived geometry remains 81 vertices, 128 indexed triangles, and
384 indices. Each frame also submits one three-vertex player marker through
the opaque list. The example does not add a collision system, scene graph,
physics integration, or streamed map data.

## Build and validate

Build the CDI with the pinned KOS container, then run the host-side Flycast
capture check:

~~~sh
make dreamcast-terrain-walk-cdi
make flycast-terrain-walk
~~~

The validator requires the exact guest marker
`maishuji: terrain walk passed (controller movement; height sampling; 81 vertices; 128 triangles)`.
On 2026-09-30, the pinned KOS image
`maishuji/dc-kos-image@sha256:21832edbd57c4eb91b316c61b61008a64344703f476197887601aea5422b9f3f`
produced a passing Debug CDI and Flycast capture with the terrain and marker
assertions enabled. This validates the fixed fixture in the tested emulator
setup, not physical Dreamcast input, every controller model, or hardware
timing.
