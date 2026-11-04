# FragFormats C89 R16 — GameStudio MDL7 matrix audit + smooth normals

Incremental **source-only** update over R15; retains GameStudio MDL3/4/5/7, original ARG parser, DirectX `.x`, DBO experiment, XMDL/UGH probes, tests and documentation.

## What's new

- `fx_div_q16` signed fixed-point division using only bounded unsigned-long operations. Designed for 32-bit MSVC/GCC-compatible C89 (no 64-bit integer arithmetic).
- `fx_inverse_affine` inverse for nonsingular **affine** 4x4 Q16.16 matrices (including nonuniform scale, skew/shear and reflections). Rejects projective, near-singular and numerically unstable cases. Uses cofactor/adjugate and two-sided product validation. The previously existing `fx_inverse_rigid` remains available for rigid-only use.
- MDL7 `--skin-invbind` now uses the independently tested general affine inversion for its **translation-only bind matrices**. This is still an experimental hypothesis: no native animated MDL7 reference has been verified yet.
- `--smooth-normals` for MDL7 creates a unit normal for each vertex from the adjacent post-deformation triangles, with corresponding OBJ `vn`/face indices. Retains earlier `--face-normals` (one face normal per triangle); the flags are mutually exclusive. No original model normals are claimed.
- Focused Ghidra cross-reference and instruction traces linking the MDL7 function `0x10010890` directly to determinant/inverse/multiply helpers, including calls at `0x10011ECE`, `0x10011EDD`, `0x10011F20`, `0x10011F56`, `0x10012070`, `0x1001207F`.

## Commands

```sh
make CC=gcc
sh tests/run_tests_r16.sh
sh tests/run_tests_r14.sh
make test
./fraginspect model.mdl smooth.obj rig.tsv --frame=1 --skin-invbind --smooth-normals
./fraginspect model.mdl flat.obj rig.tsv --frame=1 --skin-invbind --face-normals
```

`--skin-raw`, `--skin-pivot`, `--skin-hierarchy`, `--skin-invbind` are **diagnostic** skinning modes, not an independently established native GameStudio animation implementation. `--blend-rot` uses quaternion NLERP of rigid matrices, not arbitrary affine blending. `--smooth-normals` uses topological vertex adjacency; duplicated seam vertices are not welded.

## Quality, limits and safety

- Core runtime is C89, no malloc/calloc/realloc/free/heap, no float/double/long long, no third-party runtime libraries.
- Static upper bounds in the readers remain in place; bad indices and unsupported layouts are rejected.
- Unit tests include affine inversion with shear, scaling, negative determinant, round-trip, near-singular rejection, and per-vertex normals on an animated **synthetic** MDL7.
- The GameStudio SDK original and third-party assets are *not* included in this source package.
- MDL2 decoding is still probe-only. UGH and Blade3D XMDL are still incomplete. Complex MDL7 deformer variants and authenticated animated skeletons need more corpus validation.

See `research/R16_GHIDRA_FINDINGS.md` and separate Ghidra analysis ZIP for exact machine-code evidence and scripts.

## R16 verification record

- `make test` (R4 through R16, plus color-pack test) completed successfully with GCC, **exit code 0**.
- R14, R15, and R16 targeted tests also passed with Clang; both compilers used strict C89 flags.
- ZIP extracted in a fresh directory, compiled with GCC and passed `tests/run_tests_r16.sh`.
- Original fragMOTION ARG samples (`anchor`, `locked`, `jarjar`, `snowman`) reprocessed with R16: geometry and animations reported by the unchanged ARG parser. These third-party files are not bundled.
- Real animated MDL7 reference corpus still absent; all new skinning/normal tests use synthetic MDL7 fixtures.
