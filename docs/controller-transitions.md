# Button transitions: lesson 12

[Lesson index](examples.md) · [Input contract](input.md) · [Controller state](controller-state.md)

`examples/12-controller-transitions.cpp` separates a button's **held** state
from its one-update **pressed** and **released** edges. Pressing A or B increments
its counter only once, even if held for several frames. The controller diagram
still shows held buttons; the text area shows edges and cumulative press counts.
Start exits the live lesson.

```sh
make host-test-input
make in-container TARGET=dreamcast-controller-transitions-cdi
```

The live CDI reads physical port A. Without a controller, it displays
`DISCONNECTED` and no edges; it never substitutes a test controller. A first
valid sample seeds the `ButtonTracker` history, so a button already held at
connection does **not** emit a synthetic press. Disconnect, unsupported, and
unavailable samples clear history without emitting releases; recovery seeds it
again. Edges require consecutive valid samples. A controller exchanged between
polls without a detectable invalid sample cannot be identified by this API.
Switching the tracker's physical port resets history as well.

For a deterministic non-live demonstration, package the separate CDI:

```sh
make in-container TARGET=dreamcast-controller-transitions-scripted-cdi
make flycast-controller-transitions-scripted
```

It displays `SCRIPTED / NOT LIVE`, steps through disconnected, valid A held,
A released/pressed, unavailable, B held/released/pressed, and checks that A and
B were each counted exactly once before logging
`maishuji: controller transitions scripted passed`. The fixture stays visible
for up to 3600 frames. The Flycast check probes the scripted B-held diagram and guest completion
marker, not actual button edges or input. Host tests exercise the edge policy;
cross-builds check KOS compatibility. Neither demonstrates physical hotplug timing or new Maple
packet arrival. See [the input contract](input.md) for the cached-status limit.
