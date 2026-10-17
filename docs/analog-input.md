# Analog input: lesson 13

[Lesson index](examples.md) · [Input contract](input.md) · [Controller state](controller-state.md)

`examples/13-analog-input.cpp` displays cooked stick and trigger bytes, a
normalized stick vector, and a radial-dead-zone vector. On the stick plot, the
larger gold square is raw input and the smaller green square is processed input;
zero remains centered. The Y coordinate is **not inverted** by the helper:
negative remains negative. Start exits the live lesson.

```sh
make host-test-input
make in-container TARGET=dreamcast-analog-input-cdi
```

`normalized_stick` divides negative axes by 128 and positive axes by 127, so
both signed endpoints reach magnitude one. `normalized_trigger` divides a
supported trigger byte by 255. Unsupported or invalid channels are neutral.
`apply_radial_dead_zone` takes an explicit threshold in `[0,1)`; values inside
or at the boundary become zero, then the remaining radius is rescaled to
`[0,1]`. A diagonal's magnitude is clamped to one without losing its direction.
Invalid thresholds or non-finite inputs return `false` and clear the output;
there is no implicit smoothing or camera mapping.

A separately labeled scripted CDI displays the positive diagonal and trigger
fixture and logs `maishuji: analog input scripted passed` after checking the
processed vector. Package it with:

```sh
make in-container TARGET=dreamcast-analog-input-scripted-cdi
make flycast-analog-input-scripted
```

The Flycast check probes the raw and processed plot points and triggers, plus
the guest completion marker; it cannot verify physical axes. The scripted mode
is **not** live Maple input. Host tests prove arithmetic and
boundary policy, and cross-builds check KOS compatibility; physical stick
orientation, controller noise and trigger travel still need measured hardware
or interactive emulator input. See [the input contract](input.md) for the
cached-status limit.
