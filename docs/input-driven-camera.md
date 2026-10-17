# Input-driven camera: advanced example 12

[Lesson index](examples.md) · [Controller state](controller-state.md) · [Analog input](analog-input.md)

`advanced_examples/12-input-driven-camera.cpp` combines copied physical-port-A
input, normalized stick motion, bounded elapsed time, and the existing 3D mesh
camera. The tiled floor and four colored posts provide stationary references
for movement; a text overlay displays the camera position, heading, and input
availability. It uses fixed vertex/index arrays, not a scene graph.

The application mapping in `camera_controls.hpp` is explicit: D-pad up/down
moves forward/back and left/right strafes; primary stick Y moves forward/back
(negative is forward), and X turns right/left. The stick passes through a
radial dead zone; combined D-pad/stick movement is capped at unit magnitude. Start exits
the live lesson. No controller means no motion rather than an implicit fixture.
The live update reads KOS uptime in milliseconds and caps a single update at
50 ms so a slow frame does not cause a large movement jump. There is no
promise of new Maple packets for each read.

```sh
make host-test-input
make in-container TARGET=dreamcast-input-camera-cdi
```

A **separate** scripted CDI labels itself `SCRIPTED / NOT LIVE`. It moves
forward, turns, checks its final pose and logs
`maishuji: input camera scripted passed`; it holds the scene long enough for a
bounded Flycast capture. The frame checker checks the label and floor, not real
controller input or the path through time:

```sh
make in-container TARGET=dreamcast-input-camera-scripted-cdi
make flycast-input-camera-scripted
```

On 2026-10-17, the pinned KOS `63702a8` Debug and Release builds linked;
the scripted Release CDI produced stable Flycast captures and the guest marker.
The visible post and tiled floor confirm a rendered viewpoint, not a verified
interactive trajectory. Host tests verify bounded timing, movement direction,
and neutral invalid input. Live mapping, physical direction, cadence and
controller loss on a Dreamcast remain manual gates; see the
[input contract](input.md).
