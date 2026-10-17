# Physical controller ports: lesson 14

[Lesson index](examples.md) · [Input contract](input.md) · [Controller state](controller-state.md)

`examples/14-controller-ports.cpp` samples fixed Maple top-level slots A–D
independently once per update. Four rows show a colored availability indicator,
copied button/axis/trigger values, and the port's letter. Green means valid,
muted means disconnected, amber means unsupported or unavailable; **the text
label distinguishes the latter two**. Press Start on port A to exit the live
lesson.

```sh
make host-test-input
make in-container TARGET=dreamcast-controller-ports-cdi
```

A device on C never becomes player A just because A is empty. The KOS adapter
validates port indices before `maple_enum_dev(port,0)`, redoes lookup on every
sample, and copies device state before releasing the protected region. It does
not keep a borrowed device pointer between updates. Unplugging may not be
observed immediately because the pinned KOS status is cached; replacement
between two polls without a visible invalid sample cannot be identified.

The **separate scripted CDI** demonstrates A valid with A held and X=64, B
disconnected, C unsupported, and D unavailable. It labels the fixtures and
logs `maishuji: controller ports scripted passed` after checking all four
states; it does not exercise real device discovery:

```sh
make in-container TARGET=dreamcast-controller-ports-scripted-cdi
make flycast-controller-ports-scripted
```

The Flycast checker probes the four scripted indicators and the guest marker,
not actual Maple discovery. Host fixtures verify slot isolation, invalid indices,
and neutralization;
cross-builds check KOS compatibility. A live emulator mapping and physical
console hotplug test remain open evidence gates.
