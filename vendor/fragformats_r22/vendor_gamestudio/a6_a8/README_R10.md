# R10: raw matrix skinning preview

C89 `mdl7_rig.c` now stores a bounded selected-frame matrix palette and
uses its previously parsed deformer weights for optional raw weighted skinning.

Command `--skin-raw` is a deliberately conservative diagnostic; the numeric
result is well-defined for row-vector affine inputs, but GameStudio native
bind-pose and multiplication conventions are **not** yet established.

Limits: 4,096 bones with animation preview, 131,072 vertices per group,
255 influences per vertex; weight totals must be 1.0 +/- 3 Q16.16 units
for each weighted vertex. Other import paths retain their R9 behavior.
