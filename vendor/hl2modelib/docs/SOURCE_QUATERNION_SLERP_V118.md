# Source 2013 QuaternionSlerp / BlendBones audit — v1.0.18

**Reference in the bundled Source SDK:** `src/public/bone_setup.cpp`:
`CalcPoseSingle`, `CalcAnimation`, `BlendBones` (uses QuaternionSlerp for
non-delta rotations). The source code is a reference, not vendored verbatim.

## Change in pipeline

Previous:
`decompressed MDL -> 3x3 affine matrices -> matrix lerp -> Gram-Schmidt -> world bones`

New:
`decompressed MDL -> local matrix -> quaternion -> shortest-arc SLERP -> local matrix -> world bones`

`hl2m_quaternion_blend_matrix89` takes rigid local bone matrices, applies
shortest-arc quaternion interpolation, then interpolates translation linearly.
Uses only signed 32-bit Q16.16 arithmetic and 16 iterations of integer CORDIC.

`hl2m_sequence_blend_local89`, including 1D/2D group blending, and
`hl2m_sequence_add_delta89` call this module. Existing master entry points
remain available. Animation sampling at fractional cycle follows 1.0.16.

## Regression tests

- 180-degree half-turn must be a proper 90-degree orientation.
- +170 to -170 degree crossing must pass near 180 degrees (short path).
- weight 0/1 preserve original matrices exactly.
- rigid axes length and translation blending are checked.
- additive gesture half-strength rotation checked.
- 1,000 random 3D orientations independently compared with SciPy SLERP;
  average Frobenius error 0.000111681, maximum 0.000265301.
- Hunter beta Source 49 sequence `att1`: 25%, 50%, 75% cycle exported with
  213,429 drawable vertices, 71,143 triangles and no missing bones for each.
- Hunter `walk_all` (5-cell blend), Nemesis `ft2_walk`, Licker `att2`:
  three fractional time samples returned without error and passed the
  current bone-axis plausibility check.
- `make check` including legacy tests passes with C89 -Werror -pedantic.

## Known gaps / what NOT to infer

- This approximates float Source QuaternionSlerp using Q16.16; it is not bitwise Valve parity.
- Existing bone pose reconstruction, native RLE, section/ANI, and asset anomalies
  remain unchanged. Some Licker tongue translations and additive sequences
  still need asset/semantic investigation.
- This release does not simulate movement roots, IK rules, full layer scheduling,
  or model-specific source engine poseparameter remapping.
- Source engine itself was not executed, so no Source-vs-codec pixel comparison.
- The Hunter OBJ meshes are derived from assets supplied by the user and are
  shipped only in a separate results ZIP. No game models are bundled with the source.

## Reproduce

`make CFLAGS="-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude" check`

`make check-quaternion118`

`make hl2m_sequence_probe116 hl2m_sequence_obj116`

`./hl2m_sequence_probe116 hunterb.mdl 1`

`./hl2m_sequence_obj116 hunterb.mdl hunterb.vvd hunterb.dx90.vtx 1 32768 mid.obj`
