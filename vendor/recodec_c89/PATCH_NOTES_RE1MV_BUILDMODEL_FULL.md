# Patch notes: RE1MV Build_Model full descriptor pass

- Replaced the generic 15-limb parent fallback for animated RE1 EMD preview with
  a RE1MV-style matrix-stack interpreter.
- Added `EMD_Type` detection from the EMR part list using the RE1MV `Check_Id()`
  XOR method.
- Added static operation tables translated from RE1MV `Build_Model()` for all
  known types present in the source.
- Fixed root motion handling: object 0 now uses the animated root translation,
  while non-root objects use `Obj_Pos`/bind offsets from `EMR+8`.
- The selected EMR bank now drives both bind offsets and `EMD_Type` detection:
  bank A uses `EMR_A`, bank B uses `EMR_B`.
- Updated viewer log to print `emd_type` and whether the matrix-stack
  Build_Model path was used.

Verification artifacts were produced with `EM1111(5).EMD`:

```text
Bank A clip 0 frame 10: emd_type=0x3C0006 build_model=1
Bank B clip 3 frame 10: emd_type=0x410000 build_model=1
GRC validate: status=ok, invalid=0, degenerate=0
```

Honest caveat: this is a source-compatible diagnostic renderer/export staging
pass. The whole recodec repository still has legacy heap/float APIs; only the
new RE1 Build_Model descriptor path avoids adding more of those violations.

## v11 coordinate/animation table correction

- Fixed RE1 TMD vertex load to mirror RE1MV: `Calculate_Pos()` scaling/sign and display axis order `(Z,Y,X)`.
- Added `tools/emd_anim_table.c`, a fixed static-buffer EDD/EMR clip/pose table inspector with no heap and no floating point.
- Added EM1111 validation output and a Bank B clip 03 frame strip under `docs/`.

## v12 diagnostic renderer cleanup

- Flipped only final BMP presentation Y so frame strips are not vertically mirrored.
- Matched RE1MV angle conversion denominator to `0x0FFF`.
- Removed remaining compiler warnings from `tools/emd_textured_viewer.c` in this pass.
- Added `docs/EM1111_ANIM_CLIP_ATLAS_FIXED.png`, rendering start/mid/end for every EDD-separated clip.
