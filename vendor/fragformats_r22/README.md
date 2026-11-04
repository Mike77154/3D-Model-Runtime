# FragFormats R21 — SDK/Ghidra UGH structural investigation

See `README_R21.md` for the updated UGH findings, the candidate SDK header/object-record probe, resynchronizing compare tool, and limitations. All legacy vendors remain present.

## R18 focus: ARG animations, UGH and Blade3D (2026-11-02)

NEW: `--arg-clip=N --arg-frame=N` on `.arg` with an OBJ and TSV output samples original Euler rotation, translation and scale curves per bone to Q16.16. Outputs `POSE_CURVE_Q16` and `POSE_BONE_Q16`, preserving raw keyframes. Missing keys use identity defaults. Clip IDs, frame bounds and all vertex bone IDs are verified. Works on Jar Jar/Snowman original ARG samples.

```sh
make CC=gcc
./fraginspect snowman.arg snowman_base.obj snowman_pose.tsv --arg-clip=1 --arg-frame=8
sh tests/run_tests_r18.sh
```

**Crucial:** sampled pose curves are not yet applied to OBJ geometry. SDK documentation doesn't settle Euler axis units/order. The OBJ remains base geometry. No complete UGH or Blade3D XMDL decoder is claimed. See `research/R18_ARG_UGH_XMDL_FINDINGS.md` for new Ghidra investigations, including UGH's 64 KiB stream copy (NOT an on-disk record size).

# FragFormats C89 R18 — ARG / UGH / Blade3D investigation

R17 switches development focus from GameStudio to fragMOTION ARG/UGH and Blade3D XMDL. This is a **source-only** bundle; all previous R16 vendors are preserved.

## New verified functionality

- **ARG**: OBJ/MTL output with face material assignments, per-corner normals/UV preserved, v1.2 per-corner color and UV group trace, external color texture path lookup, clip events, frames, FPS/loop bits and original animation key traces.
- **ARG rig**: four fixed-point Q16.16 weights per vertex including residual and non-finite/invalid weight rejection; bone matrices and clip keys remain available in raw IEEE bits in TSV. **Full animated skinning is not yet reproduced.**
- **UGH**: preserve probe-only scope; a 16-byte `ugh0` header recognized and other words displayed as opaque data. Ghidra traced the exporter but **not a verified full parser**.
- **Blade3D XMDL**: Ghidra confirmed `xmdl` and `0100` checks in the original importer; XMDL remains probe-only without authentic samples. XIR internal test geometry is not native Blade3D.

### Commands

```sh
make CC=gcc
sh tests/run_tests_r17.sh
make test
# ARG model, static OBJ, detailed rig/clip trace, material library
./fraginspect model.arg model.obj model.tsv model.mtl
# ARG legacy call still supported
./fraginspect model.arg model.obj model.tsv
# UGH and Blade3D are **probes**, not full conversion
./fraginspect unknown.ugh
./fraginspect unknown.xmdl
```

Place `model.obj`, `model.mtl` and the **existing external texture image** in the same directory (or update the MTL texture path as needed). Missing texture files are not generated. OBJ does not contain bones or clips; their original records are in TSV.

**Format convention:** R17 uses `map_Kd` only for texture layers flagged `layerColor = 0` in the original SDK. Other layer types (detail, alpha, bump, environment, light) are reported as numeric `LAYER` records in TSV rather than incorrectly mapped to a diffuse texture.

**Ghidra evidence:** see `research/R17_ARG_UGH_XMDL_GHIDRA.md`, plus separate Ghidra artifact for script/output.

---

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

## R22 major breakthrough: UGH native importer and linked-object layout

See `README_R22.md` for the confirmed import address (`0x00469FA1`), two-stage wrapper and new independent `ughnativeprobe` and `ughtree` C89 utilities. All UGH structural outputs are still research data until verified against a genuinely saved UGH file.
