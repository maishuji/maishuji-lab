# Heightmapped terrain

advanced_examples/04-heightmapped-terrain.cpp turns a compact heightmap into
an indexed 3D terrain mesh and tiles a dedicated rock texture across it. It is
the next step after the textured cube: the cube demonstrates one textured
object, while this lesson demonstrates how a map becomes many connected
triangles.

## Map generation

The source heightmap is a 9x9 array of elevation samples. Each sample becomes
one TexturedMeshVertex, so the map contains:

- 81 vertices;
- 8x8 cells;
- 128 indexed triangles;
- 384 indices.

The example converts each byte into a world-space Y coordinate with a visible
height scale. It also gives each vertex a small height-based color tint so the
relief remains readable without adding a lighting system. The generated mesh
uses the existing TexturedMesh API; there is no scene graph, tilemap runtime,
streaming system, or heap allocation.

The UV coordinates span 0 to 4 across the map in both directions. The 256x256
rock-and-dirt texture therefore repeats four times across the terrain instead
of stretching once over the whole map. The texture is opaque and the terrain
is submitted through the opaque PVR list, with the dark-blue background
submitted before it.

The camera sweeps around the map for 240 frames. It then uses a fixed capture
camera for 900 frames so the emulator checker can inspect a stable view without
racing the animation.

## Asset and cost

advanced_examples/assets/terrain-rock.png is the source bitmap generated for
this lesson. The pinned KOS pvrtex utility converts it to the square-twiddled
ARGB4444 terrain-rock.pvr, and terrain_asset.hpp embeds that PVRT payload so
the CDI has no runtime filesystem dependency.

The embedded PVRT fixture is 131,088 bytes: a 16-byte header followed by a
256x256 ARGB4444 payload. The uploaded texture consumes 131,072 bytes of texel
storage. Each terrain frame submits 128 polygon headers and 384 textured
vertices. Since the current indexed submission path keeps the cost visible, it
reprojects referenced vertices per triangle and does not maintain a transformed
vertex cache.

## Build and validate

Build the CDI with the pinned KOS container, then run the host-side Flycast
capture check:

~~~sh
make dreamcast-heightmapped-terrain-cdi
make flycast-heightmapped-terrain
~~~

The validator waits for the exact guest marker
maishuji: heightmapped terrain passed (256x256 ARGB4444; 81 vertices; 128 triangles).
It requires three consecutive stable captures and checks:

- a varying textured terrain region;
- both far and near terrain strips;
- variation at the terrain center;
- the dark-blue sky/background outside the map.

On 2026-09-30, the pinned KOS image
maishuji/dc-kos-image@sha256:21832edbd57c4eb91b316c61b61008a64344703f476197887601aea5422b9f3f
produced a passing Debug CDI and Flycast capture. The observed terrain crop
mean was 0.230 0.173 0.120, with variation 0.092 0.060 0.033; all three
sky samples were 0.031 0.063 0.141. This validates the fixed fixture in the
tested emulator setup, not every possible heightmap, camera, texture, or
physical Dreamcast configuration.
