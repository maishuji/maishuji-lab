# Multi-texture rendering

The multi-texture lesson renders one satellite-like assembly from two
independently allocated 16x16 ARGB4444 textures through one opaque PVR list.
The metal body uses one texture and both solar-panel wings use another. The
example therefore demonstrates material regions on one object instead of just
placing unrelated colored panels next to each other.

Build and validate it with:

~~~sh
make dreamcast-multi-texture-cdi
make flycast-multi-texture
~~~

The body uses a deterministic metal-and-bolt pattern and the wings use a
deterministic blue solar-cell grid. The runtime submits one colored background
quad, two solar-wing quads, and one body quad. This is four polygon submissions
and two texture allocations; each textured region carries its own polygon
header and four vertices. The example deliberately keeps all regions in the
same opaque list, where changing texture state is explicit at each submission
boundary.

The satellite is held at a fixed pose. This lesson intentionally has no
per-frame motion, so a changing capture indicates a texture, depth, or emulator
problem rather than animation.

The textures are generated in the example and uploaded as CPU-side ARGB4444
arrays. Each 16x16 texture consumes 512 bytes of texel storage, excluding KOS
allocator and PVR bookkeeping. The checker verifies the blue solar wings, the
contrasting metal body, and the dark background in a stable Flycast capture and
requires the guest completion marker.

This lesson demonstrates multiple texture ownership and submission selection;
it does not measure texture-switch timing, VRAM high-water, SH-4 performance,
or physical Dreamcast throughput. Flycast is the runtime validation target in
the current environment.
