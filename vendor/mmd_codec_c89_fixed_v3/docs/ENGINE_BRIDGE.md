# Engine bridge notes

## Coordinate handoff

MMD uses its own practical conventions. This codec does not flip axes, rescale units, or reorder triangles. It preserves the source values as Q16.16 so the host engine can decide policy.

Suggested bridge stages:

1. Load PMD/PMX with `mmd_load_model()`.
2. Build your engine mesh from `vertices[]` and `indices[]`.
3. Convert Q16.16 to your renderer math only at the renderer boundary.
4. Build skeleton nodes from `bones[]`.
5. Bind IK metadata from `ik_links[]`.
6. Bind facial sliders from `morphs[]` + `morph_offsets[]`.
7. Bind PMX physics from `rigid_bodies[]` and `joints[]` if your runtime has physics.
8. Load VMD/VPD separately and resolve key names against model bone/morph names.

## No-heap pattern

Allocate one global or arena-owned `MMD_Model`, `MMD_Motion`, and `MMD_Pose` at engine level. The codec itself never owns memory and never calls heap APIs.

## Name matching

PMD/VMD/VPD names are byte-preserved. PMX UTF-8 text is copied as-is; PMX UTF-16LE text is reduced to ASCII when possible and `?` otherwise. For full Japanese name matching, add a transcoder outside this no-heap codec or compare original encoded bytes in a higher-level asset pipeline.
