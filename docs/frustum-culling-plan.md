# Planned advanced example: view-frustum culling

The current indexed-mesh path transforms and clips each submitted triangle
against six homogeneous clip planes; see [meshes.md](meshes.md). An advanced
example can teach a different decision: reject a whole object whose
conservative bound is fully outside the camera's view frustum *before* calling
`RenderList::submit(mesh, camera, transform, viewport)`. This fits the existing
camera and mesh APIs without a scene graph or a new rendering backend.

Use a small fixed scene with several copies of an existing mesh and a camera
pose that leaves some copies visible, some wholly outside, and one crossing a
frustum plane. A visible toggle or deterministic camera motion should expose
which objects are submitted. Keep the bound and plane test on the CPU, with no
heap allocation. Passing a bound must still use the existing per-triangle
clipping path, because a conservative object test cannot clip a partly visible
triangle. PVR polygon culling is a separate winding-state decision.

## Completion criteria

- Define a conservative object-space sphere or box, transform it correctly for
  the example's scale and rotation, and test it against the camera's six planes.
  Boundary contact counts as potentially visible. Handle invalid cameras and
  non-finite bounds without silently dropping visible geometry.
- Compare an unculled submission mode with CPU frustum culling for the same
  fixed scene. Report per-frame object decisions, triangles submitted to the
  existing mesh path, and source-derived polygon-header/vertex packet counts.
  State that these are structural counts, not elapsed-time or hardware-counter
  measurements.
- Cover inside, outside, plane intersection, camera movement, and transformed
  bounds in host tests. Build Debug and Release ELFs with the pinned toolchain;
  package a CDI and check visible output plus a runtime marker in Flycast.
  Record physical Dreamcast validation separately when available.

The current source baseline is the indexed-mesh behavior documented in
[meshes.md](meshes.md), inspected on 2026-10-05. This is a design proposal;
the example, counts, target build, emulator result, and hardware behavior have
not yet been verified. No performance gain is claimed in advance.
