# Phase 18 — independent multi-link 3D IK + cone limits (C89/Q16.16)

## Implementation scope

Added `include/bc_ik_chain.h` and `src/bc_ik_chain.c`. This is a **new standalone inverse-kinematics solver** for serial chains of **3 through 8 links**, retaining Phase 17's two-link analytic Blender playback unchanged. **It is not yet connected to `--play-rigs` for serialized Blender IK chains longer than two bones.** This release does not claim compatibility with Blender's native IK solver.

`--ik-chain` reads a fixed-record, text Q16.16 goal describing an explicit rigid rest frame per bone, joint coordinates, target and optional cosine-angle constraints; writes (a) armature-space deform matrices `pose * inverse(rest)` in the same 3x4 TSV format consumed by the existing `--skin-tsv`, and (b) solved joint positions. No heap allocation occurs in the project C code. All intermediate numeric calculations use **signed 32-bit integers** with checked Q16.16 operations; the implementation does not use `float`, `double`, `long long`, or recursion.

## Solver behavior

- Fixed root, preserved bone lengths; forward/backward FABRIK-style positional relaxation. Maximum 96 iterations.
- Rejects unreachable targets (never silently stretches), nonconvergent goals, invalid or nonrigid rest frames, zero/short segments, invalid cone limits and degenerate rotations.
- Each joint's optional `cosine_limit` is a minimum dot product of consecutive segment directions; a conservative cone clamp restricts excessive bending. `-1` disables the joint restriction, `0` allows turns up to approximately 90°, `0.7071` allows up to approximately 45°.
- Local roll is retained by shortest-arc swing of the explicit rest matrix onto each solved segment. Ambiguous near-antiparallel 180° rotations are rejected, not guessed.
- Supported target coordinates and rest joints: each component in [-32,32] model units; total chain length ≤32 units, single link length ≤16 units. Scene/model scale can be normalized by callers.
- The accepted Q16.16 residual is at most 512 raw units (~0.0078125) **as computed by truncated fixed-point dot products**. The source-level integration tests allow a 0.015-unit geometric endpoint discrepancy to account for truncation. This is a preview-quality solver rather than numerical equality with Blender.

## Fixed input grammar

```text
chain N                         # N in [3,8]
target X Y Z                    # decimal with ≤4 fractional digits
iterations N                    # N in [1,96]
node 0 X Y Z
...
node N X Y Z
limit 1 MIN_COSINE             # each joint 1..N-1
...
limit N-1 MIN_COSINE
matrix 0 M00 M01 M02 M03 M10 M11 M12 M13 M20 M21 M22 M23
...
matrix N-1 ...
```

The rows must appear in this exact order; `#` at the beginning of a line indicates a comment. `matrix` values are the original **rigid** armature-space rest transforms, row-major 3×4, whose translations match `node i` and whose local +Y directions match the vector from `node i` to `node i+1`. No default matrix or inferred bone roll is used. Rotation and matrix values use 4-decimal numbers to match `bc_parse_q16`.

## Commands

```sh
make clean all
./blendc89 --ik-chain examples/phase18_chain_3link.tsv examples/phase18_palette.tsv examples/phase18_joints.tsv
./blendc89 --skin-tsv examples/phase18_palette.tsv examples/phase18_vertices.tsv examples/phase18_skinned.tsv
# Source-only C89 path; no zlib/zstd linkage, uncompressed .blend only
make source-only
./blendc89_uncompressed --ik-chain examples/phase18_chain_3link.tsv examples/phase18_pal2.tsv examples/phase18_j2.tsv
```

Windows batch file `build_win32_mingw32.bat` includes the new source, but **Win32 builds were not executed in this environment**.

## Executed validation (Phase 18)

- GCC strict C89 (`-std=c89 -pedantic -Wall -Wextra -Werror`) complete build: passed.
- C89 source-only uncompressed build: passed.
- Clang strict C89 with `-fsanitize=undefined -fno-sanitize-recover=all`: passed new solver, orientation, skinning and mutation suites; instrumented C only, prebuilt vendor libs are not instrumented.
- 37 successful generated goals (3/4/5/6/8 links, cardinal rest frames, including two active cone-limit examples); 10 deliberately invalid scenarios rejected.
- 80 additional seeded random 3D rigid rest frames and targets solved within preview tolerance.
- 160 binary text-input mutations completed without process-signal failures (crash smoke-test, **not** a proof of general safety).
- Explicit end-to-end output: `--ik-chain` palette fed to existing `--skin-tsv` with bone-weighted vertices; output matches solved joints within 0.003 units in the canonical fixture.
- Previous Phase 09, 12, 13, 14, 15, 16, 17 regression suites passed in this run. See `PHASE18_TEST_RUN.log` and `PHASE18_SANITIZER.log`.
- Seven **genuine source-archive `.blend` assets** retain geometry export counts; this does not establish animation-solver parity with Blender.

## Important limitations

- Not connected to automatic `--play-rigs` when a Blender file contains IK chains of 3+ bones. Those cases remain unsupported in native `.blend` playback.
- The cone clamp is **not** equivalent to Blender's independent per-axis IK locks, stiffness, min/max rotation limits or its IK pole-angle semantics.
- No moving Blender targets, Blender Tree-IK, multi-effector IK, twist constraints, axis IK limits, stretch, B-Bones, constraints stacking, complete NLA blending, or compatibility claim for authentic Blender-authored complex rigs.
- 180° swing ambiguity and mathematically unreachable constrained configurations fail closed.
- Precompiled Linux x86-64 zlib/Zstandard archives remain vendored third-party libraries and are **not** heap-free C89 source. Standard IO functions may internally allocate. The original project code uses no explicit dynamic allocation.
- The Blender source distributions supplied by the user were consulted for binary structures; **none of their GPL implementation source is copied** into this standalone solver. Bundled third-party license notices are preserved.

## Future Phase 19 integration path

Extract ordered chains from `bPoseChannel.parent`, `Bone.arm_mat`, `Bone.arm_head/tail`; evaluate target object transforms in the correct coordinate space; map Blender per-axis IK flags to supported behaviors or reject, rather than treating cone limits as equivalent; then pass the per-rig chain result into the existing multi-rig skinning palette. Verify using authentic Blender-animated, exported pose baselines before stating native playback parity.
