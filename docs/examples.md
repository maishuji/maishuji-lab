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

Read these input lessons in order. Each foundation example teaches one input
concept; the advanced camera lesson combines them with 3D rendering.

| Lesson | Level | Teaches | Prerequisite | Status |
| --- | --- | --- | --- | --- |
| [11 controller state](controller-state.md) | Foundation | Owned raw samples and a controller dashboard | Lifecycle, primitives, text | Implemented |
| [12 button transitions](controller-transitions.md) | Foundation | Held, pressed, released, connection history | Controller state | Implemented |
| [13 analog input](analog-input.md) | Foundation | Endpoint normalization and radial dead zones | Controller state | Implemented |
| [14 controller ports](controller-ports.md) | Foundation | Physical A-D slots and availability | Controller state | Implemented |
| [12 input-driven camera (advanced)](input-driven-camera.md) | Advanced | Input, elapsed time, and camera movement | Analog input, spatial math | Implemented |

Read the [input contract](input.md) before implementing or interpreting input
behavior. The existing [terrain walk](terrain-walk.md) and
[frustum-culling example](frustum-culling.md) now reuse the copied-state input
helpers for movement and toggle behavior.

## Timing and peripheral lessons

[Fixed-step timing](fixed-step.md) is implemented as the `examples/15`
foundation, with separate live-clock and scripted-fixture builds. [AICA sound](aica-sound.md) is implemented
as `examples/16-aica-sound.cpp` with host waveform tests and a pinned KOS
cross-build; audible playback still needs a listening test. The
[VMU LCD testability assessment](vmu-lcd-testability.md) identifies the missing
LCD-specific visual oracle before an `examples/17` lesson can claim validation.

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
