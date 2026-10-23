# Fixed-step timing: example 15

[Lesson index](examples.md) · [next-lesson plan](next-lessons.md)

`examples/15-fixed-step.cpp` separates elapsed wall-clock time from fixed
simulation updates and rendering. The live executable reads KOS uptime with
`timer_us_gettime64()`. The separately built scripted executable feeds a named,
deterministic sequence solely so the display and catch-up behavior can be
captured without pretending to measure emulator or Dreamcast cadence.

The lesson-local [`Accumulator`](../examples/fixed_step.hpp) uses integer
microseconds. One simulation step is 16,667 microseconds: an explicit rounded
approximation of 60 Hz. Fractional time remains in the accumulator so short
render intervals eventually produce an update. A render may run at most four
updates. Additional whole steps are discarded and counted, preventing a long
pause from creating an unbounded catch-up spiral.

The first timestamp establishes a baseline. Equal timestamps add no time. A
backward timestamp rebases the clock and clears its partial remainder rather
than allowing unsigned subtraction to create a huge elapsed interval. These
policies are application choices demonstrated by this lesson, not scheduler or
PVR guarantees.

## Build and validate

```sh
make host-test-fixed-step
make in-container TARGET=dreamcast-fixed-step-scripted-cdi DC_BUILD_TYPE=Release
make flycast-fixed-step-scripted
```

The host test covers partial and multiple steps, repeated and backward
timestamps, integer remainder accumulation, bounded catch-up, and dropped-step
counters. The cross-build checks compatibility with the pinned KOS toolchain.
The Flycast target checks the supplied scripted visual fixture and its exact
guest marker; it does **not** prove 60 Hz hardware cadence or measure timing.

For the live KOS clock build:

```sh
make in-container TARGET=dreamcast-fixed-step-cdi DC_BUILD_TYPE=Release
```

The yellow marker advances from elapsed time, while the green marker advances
only in fixed increments. The elapsed marker caps a single visual movement at
100 ms to keep the comparison readable after a pause; that display cap is
separate from the accumulator's four-update catch-up policy. Live performance
claims require a named emulator or real-console measurement and are outside
this foundational example.
