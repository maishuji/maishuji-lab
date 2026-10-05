# Planned advanced example: view-frustum culling

Implemented in `advanced_examples/10-frustum-culling.cpp`; the fixed-scene
walkthrough and validation record are in [frustum-culling.md](frustum-culling.md).
Physical Dreamcast validation and measured CPU timing remain open.

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
The existing clipper already drops fully outside triangles, so the fixed lesson
should show skipped CPU mesh work without promising fewer PVR packets.

Make the lesson self-explanatory on screen and in its source: label each object
or region with its visibility case, show the camera/frustum boundary and object
bounds in a simple diagram or overlay, and display submitted versus skipped
object and triangle counts for both modes. Start the example in a fixed pose
that demonstrates all three cases without controller input. Document the exact
camera, bounds, and expected decisions in an example README, then explain one
frame from bound test through mesh submission to PVR packets. A reader should
be able to predict what changes when culling is toggled before running it.

## Completion criteria

- Define a conservative object-space sphere or box, transform it correctly for
  the example's scale and rotation, and test it against the camera's six planes.
  Boundary contact counts as potentially visible. Handle invalid cameras and
  non-finite bounds without silently dropping visible geometry.
- Compare an unculled submission mode with CPU frustum culling for the same
  fixed scene. Report per-frame object decisions, triangles submitted to the
  existing mesh path, and source-derived polygon-header/vertex packet counts
  where determinable. Explain that outside objects may have produced zero PVR
  packets even in the unculled mode. These are structural counts, not elapsed
  time or hardware-counter measurements.
- Provide visible labels and a concise on-screen legend for the fixed cases;
  give exact coordinates and expected outcomes in the lesson documentation, including
  why the plane-intersecting object remains submitted.
- Cover inside, outside, plane intersection, camera movement, and transformed
  bounds in host tests. Build Debug and Release ELFs with the pinned toolchain;
  package a CDI and check visible output plus a runtime marker in Flycast.
  Record physical Dreamcast validation separately when available.

The current source baseline is the indexed-mesh behavior documented in
[meshes.md](meshes.md), inspected on 2026-10-05. This is a design proposal;
the example, counts, target build, emulator result, and hardware behavior have
not yet been verified. No performance gain is claimed in advance.
