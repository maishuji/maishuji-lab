# Lesson index

`examples/` teaches one capability at a time. `advanced_examples/` combines
capabilities or investigates a larger algorithm or hardware cost. Numbers are
stable identifiers, not a requirement to complete every earlier lesson.

## PVR foundations

Start with [lifecycle](lifecycle.md), [primitives](primitives.md),
[textures](textures.md), and [text](text.md). Continue with [pixel coordinates](pixel-art.md),
[spatial math](spatial-math.md), [meshes](meshes.md), [fog](fog.md),
[offset colors](lighting.md), [particles](particles.md), and [packet budgets](budgets.md).
The raw reference remains [pvr_smoke.cpp](../examples/pvr_smoke.cpp).

## Input learning path

The input track is being implemented in the order below. Planned filenames
are identifiers, not available build targets until their lesson is added.

| Lesson | Level | Teaches | Prerequisite | Status |
| --- | --- | --- | --- | --- |
| 11 controller state | Foundation | Owned raw samples and a controller dashboard | Lifecycle, primitives, text | Planned |
| 12 button transitions | Foundation | Held, pressed, released, connection history | Controller state | Planned |
| 13 analog input | Foundation | Endpoint normalization and radial dead zones | Controller state | Planned |
| 14 controller ports | Foundation | Physical A-D slots and availability | Controller state | Planned |
| 12 input-driven camera (advanced) | Advanced | Input, elapsed time, and camera movement | Analog input, spatial math | Planned |

Read the [input contract](input.md) before implementing or interpreting input
behavior. The existing [terrain walk](terrain-walk.md) and
[frustum-culling example](frustum-culling.md) currently demonstrate small amounts
of direct KOS input; they are not substitutes for the foundation lessons.

## Advanced rendering

Explore [textured meshes](textured-3d.md), [terrain](heightmapped-terrain.md),
[terrain walking](terrain-walk.md), [mipmaps](mipmap-texture.md),
[model loading](model-loader.md), [multiple textures](multi-texture.md),
[tile workload](tile-workload.md), [frustum culling](frustum-culling.md),
[dynamic lighting](dynamic-lighting.md), and [multilingual text](advanced-text.md).

## Evidence levels

Host tests verify portable logic; cross-builds verify API/link compatibility;
scripted captures verify rendering of supplied fixtures. None alone proves
physical controller input. Live emulator mapping and real-console tests are
separate evidence. Autonomous graphics lessons remain autonomous unless their
teaching purpose explicitly includes input.
