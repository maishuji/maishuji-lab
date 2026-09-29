# Particle batches through one PVR list

`examples/09-particle-batch.cpp` uses the existing textured-quad API to submit
24 small moving particles in one punch-through render-list scope. It is a
teaching batch: the CPU owns the particle positions and loops over them, while
the PVR still receives one polygon header and four textured vertices for each
quad.

## The path

The example creates one 16x16 ARGB4444 texture containing a white circular
mask. Each particle supplies a colored `TexturedQuad` with the same normalized
UVs and a deterministic position derived from its index and frame number. The
transparent texels are discarded by the punch-through list, leaving only the
circular particles visible.

The repeated submission shape is intentionally explicit:

~~~cpp
for(const Particle &particle : particles) {
    const maishuji::TexturedQuad quad =
        make_particle_quad(particle, frame_index);
    status = list.submit(texture, quad);
}
~~~

No scene graph, particle allocator, controller input, DMA, or special particle
primitive is introduced. The point is to see the boundary between a CPU-side
simulation and repeated PVR packet submission.

## Structural budget

For 24 textured quads in one list, the current backend emits the following
structural packet shape per frame:

| Resource | Count | Boundary |
| --- | ---: | --- |
| Textured polygon headers | 24 | One compiled PVR header per `submit()` |
| Textured vertex packets | 96 | Four vertices per quad |
| `pvr_prim()` calls | 120 | Five calls per textured quad, excluding list/scene control |
| Texture allocations | 1 | 16x16x2 = 512 bytes in PVR texture memory |
| Per-frame particle heap allocations | 0 | The example keeps particle data static and builds one quad on the stack at a time |

These are source-derived packet counts, not measured timing or VRAM
high-water marks. A real benchmark would need a known Dreamcast, cache state,
video mode, vertex-buffer configuration, synchronization policy, and a timing
method. The Flycast gate checks rendering only.

## Checks

Build and package with the pinned toolchain, then run the capability-specific
Flycast check:

~~~sh
make dreamcast-particles-cdi
make flycast-particles
~~~

After the demo frames, the example logs its completion marker and holds the
same rendered batch for a bounded capture window. That hold is an emulator-test
lifetime aid, not part of the particle simulation.

The checker samples the red, green, and blue particle columns plus dark regions
outside the batch. The required guest marker is
`maishuji: particle batch passed`. Emulator validation does not replace real
Dreamcast hardware validation.
