# PVR tile workload

The tile-workload lesson makes the Dreamcast rendering split visible. It puts
two fixed six-quad workloads beside each other:

- the left panel uses small quads with gaps, so neighboring polygons touch few
  common screen tiles;
- the right panel uses six nested quads, so the same tiles receive several
  polygon references and several layers of potentially overlapping work.

Both panels submit the same number of workload polygons through one opaque list.
The difference is screen-space coverage, not a hidden change in the CPU loop.

## Build and validate

Run the target build in the pinned KOS container, then validate the CDI in
Flycast:

```sh
make dreamcast-pvr-tile-workload-cdi
make flycast-pvr-tile-workload
```

The Flycast checker requires a stable 4:3 capture, a blue background, a green
low-overlap panel, and two visibly different red layers from the nested panel.
The guest also emits a completion marker after 180 warm-up frames and holds
the fixed scene for the capture window.

## What SH-4 and PowerVR2 each do

The SH-4 runs the application loop. It computes the quad coordinates, writes
the KOS polygon packets, and submits them to the PowerVR2 tile accelerator.
The SH-4 is not rasterizing every covered pixel in this example.

The PowerVR2's Tile Accelerator (TA) receives those packets and builds tile
lists. The renderer then processes screen tiles and uses the per-tile polygon
references to decide which opaque polygons cover each tile. This is why a
polygon's screen-space footprint matters even when the application submits the
same number of polygons.

The friendly panel keeps its six quads separated. The nested panel causes the
same 32x32 tile regions to be referenced by multiple polygons, increasing
overlap and potential overdraw. The lesson is intentionally about the
submission shape that the PVR sees; it does not claim that every overlapping
scene is automatically slow or that a single quad is always preferable.

## Source-derived estimate

The example estimates coverage on a 640x480 screen divided into 32x32 tiles.
For each workload quad it counts the conservative axis-aligned tile bounds,
then reports:

- `tile_references`: the sum of touched tiles across the six quads;
- `max_tile_layers`: the largest number of workload quads touching one tile;
- `polygon_count`: the six quads in that side's workload.

These are teaching estimates, not a readback of an internal PVR counter. The
conservative bounds can count a tile touched by a quad's bounding box even if
the exact polygon does not cover every pixel in that tile. The three background
and panel backdrop quads are excluded from the reported comparison, although
they are submitted so the capture remains easy to inspect.

This lesson does not measure SH-4 time, TA time, OPB high-water, DMA, cache
behavior, or real Dreamcast throughput. Flycast is the current runtime
validation target; physical-console validation remains a later follow-up.
