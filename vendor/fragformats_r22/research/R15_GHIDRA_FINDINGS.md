# R15 — targeted Ghidra 12.0.4 investigation of fragMOTION GameStudio matrix helpers

**Original input:** FileTypeMDL_GS.dll from supplied fragMOTION 1.3.2 archive; prior imported Ghidra project `GS_R12` reused with `-noanalysis`.

The new script `InspectInverseR15.java` ran on **the actual imported plugin**. The report and 692-line instruction/decompilation trace are in the separate `FragFormats_R15_Ghidra_Analysis.zip`.

| Address | Instructions | Decompiler output | Evidence |
|---|---:|---:|---|
| 0x1001AC70 | 56 | 1,224 characters | Matrix product: repeatedly computes a 4-term sum and copies sixteen floats into destination. |
| 0x1001ADA0 | 121 | 2,810 characters | Computes a large alternating product sum of four-by-four components: determinant algebra. |
| 0x1001AEF0 | 412 | 5,642 characters | Receives a determinant-like scalar; when nonzero, obtains reciprocal, computes cofactors, writes sixteen components in place: inverse matrix helper. |

These are **helpers in fragMOTION**, and this by itself does not establish that the MDL7 importer uses this inversion function to calculate an inverse bind matrix. Caller-side tracking and animation-bearing original models are still required for native correspondence.

## C89 R15 changes

- Reusable `fx_inverse_rigid` implementation handles orthonormal (det=+1) row-vector affine transforms with a transposed rotation and inverse translation, avoiding FP and 64-bit types. It **rejects** nonrigid matrices. This is **not** a general 4x4 inversion equivalent to the original FPU helper.
- `--skin-invbind`: separate diagnostic pipeline using `inverse(Translate(absolute_bind_bone_position)) * posed_world_matrix` for MDL7. Same assumed local-space animation convention as `--skin-hierarchy`, but explicit inverse-bind evaluation. Original compatibility NOT proven.
- Numeric tests cover point roundtrip, matrix product identity, reflection/shear/scale/perspective rejection, parity against hierarchical skinning, interpolation and normals.

## Limits

No original GameStudio animated skeleton is included or validated. The recovered matrix inverse helper uses floating-point internal operations; the R15 C89 math deliberately supports only rigid matrices and is not a literal port. All R15 skeleton tests are synthetic. The underlying MDL3/4/5/7/ARG/DirectX/DBO status is unchanged.
