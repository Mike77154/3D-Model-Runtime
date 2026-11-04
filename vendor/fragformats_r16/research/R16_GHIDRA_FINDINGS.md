# R16 — Ghidra MDL7 matrix references (observed)

Analyzed file: original fragMOTION `FileTypeMDL_GS.dll` (SHA-256 `449b57d63da7bd7f5ce8acf20116aea07cb8744b242ddf20b668b51d770ac057`).

Ghidra version: **12.0.4**, existing `GS_R12` project, read-only, no automatic reanalysis. Scripts and their executed output in `FragFormats_R16_Ghidra_Analysis.zip`.

## Cross-references, not guessed function labels

- `0x1001AC70` — 4x4 matrix multiply, directly called by MDL7 import routine `0x10010890` at `0x10011F20`, `0x10011F56`, `0x10013995` (3 calls).
- `0x1001ADA0` — determinant helper; calls at `0x10011ECE`, `0x10012070` (2 calls).
- `0x1001AEF0` — general matrix inverse helper; calls at `0x10011EDD`, `0x1001207F` (2 calls).
- `0x1001AD60` — 4x4 identity; 9 direct callsites in MDL7 importer.
- `0x1001D9E0` — x87 float-to-integer conversion; 16 direct callsites in the import routine.

## Why the first inverse call is interesting

The `0x10011EAA..0x10011F56` region calculates an offset from a low-byte table index shifted left by six (64 bytes), copies the selected 64-byte matrix via `0x1001AC50`, calls the determinant helper at `0x10011ECE`, calls the inverse helper at `0x10011EDD`, and subsequently calls the multiply helper at `0x10011F20` and `0x10011F56`.

This verifies an **inverse-and-compose path** inside the MDL7 import function. It does *not* establish whether that matrix is a bind-pose inverse, a model transform, a skin/UV transform or a temporary. The exact provenance and resulting vertex transformation have not yet been reconstructed.

## Runtime implementation distinct from Ghidra proof

`fx_inverse_affine` in the independent vendor supports nonsingular Q16.16 affine matrices with scale or shear; returns failure for ill-conditioned/inexact inverses through two-sided round-trip verification. It is **not a 4x4 projective inverse** and does not reproduce all of fragMOTION's FPU results. The MDL7 bind diagnostic still treats file bone positions as absolute translations. The new `--smooth-normals` mode averages **generated flat face normals** over each shared vertex after deforming positions; it does not restore stored GameStudio vertex normals or smooth across duplicated seams.

## Open issues

1. Identify matrix objects passed to inverse calls at `0x10011EDD` and `0x1001207F`.
2. Validate skeleton pose and interpolation against an authentic animated MDL7 with weights.
3. Distinguish all A6/A7/A8 MDL7 revision offsets, material formats and stored normal conventions.
4. Recover MDL2 of A3 and finalize XMDL/UGH independent readers.
