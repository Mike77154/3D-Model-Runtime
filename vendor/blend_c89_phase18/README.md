# BLEND C89 CODEC — PHASE 18 (Multi-link IK)

**New:** a standalone, preview-quality FABRIK-style 3D Q16.16 IK solver for **3–8 bones**, with fixed root, explicit rigid rest matrices, cone-cosine joint limits and exported 3×4 deformation palettes. New command `--ik-chain`, interoperable with `--skin-tsv`.

**Tests executed:** 37 positive and 10 negative goal cases; 80 random oblique rest frames; 160 mutated-input crash smoke tests; actual chained skinning; Clang UBSan and source-only C89 build; prior animation regression suites and seven genuine Blender geometry exports.

**Scope warning:** This is a **standalone solver**; it does **not** yet evaluate >2-link Blender IK constraints automatically through `--play-rigs`. Blender-specific pole angles, per-axis limits, Tree-IK, IK stretch and native solver parity remain unsupported. Vendored binary zlib/Zstandard libraries are not wholly C89/heap-free.

```sh
make clean all
./blendc89 --ik-chain examples/phase18_chain_3link.tsv examples/phase18_palette.tsv examples/phase18_joints.tsv
./blendc89 --skin-tsv examples/phase18_palette.tsv examples/phase18_vertices.tsv examples/phase18_skinned.tsv
python3 tests/test_phase18_chain.py
python3 tests/test_phase18_oblique.py
python3 tests/test_phase18_skinning.py
```

Full scope, grammar, validation and limitations: [`docs/PHASE18_MULTILINK_FABRIK.md`](docs/PHASE18_MULTILINK_FABRIK.md).

---

# BLEND C89 CODEC — PHASE 17 (IK rest-axis + roll orientation)

**New in Phase 17:** removes the strict world +Y rest-axis assumption for a *bounded* serialized two-bone IK constraint. Reads `Bone.arm_head`, `arm_tail` and `arm_mat` dynamically from SDNA, checks rigid rest frames, preserves original local bone roll under minimal-swing IK, and exports the deformed mesh via the existing `--play-rigs` command.

- **Validated:** 48 synthetic multi-rig rest orientations (including 36 oblique frames), 12 malformed-rest rejections, direct C89 roll probe, source-only and sanitizer builds, prior regressions, 7 genuine Blender-source geometry exports.
- **Not validated:** actual Blender-authored IK animation playback/parity. Still restricted to 2-link, static-target, simple IK; not full Blender IK/constraints/NLA.
- **Strict rules:** original C code uses C89, signed Q16.16 and fixed storage; bundled Gzip/Zstandard static Linux libraries do **not** satisfy the whole-program heap-free/C89 promise.
- Details: [`docs/PHASE17_IK_REST_ORIENTATION.md`](docs/PHASE17_IK_REST_ORIENTATION.md).

```sh
make clean all
python3 tests/test_phase17_rest_orientation.py
./blendc89 --play-rigs compatible.blend 5 frame
```

---

# BLEND C89 Codec — Phase 16: serialized IK2 playback (bounded)

SDNA-based `.blend` reader and Q16.16 skeletal preview. This release **extends Phase 13**, preserving its multirig separation and earlier geometry, UV, weight, Euler, NLA timing, and compression support.


## Phase 16 — Integrating serialized IK into `--play-rigs`

The Phase 15 fixed-point analytical two-bone IK kernel now participates directly in the multirig OBJ playback path for a **strictly limited subset** of Blender's stored IK constraints: unanimated static target/pole Objects, canonical two-bone +Y rest skeleton, root Action location and supported pose-space settings. Target world coordinates are converted to the Armature Object's local space; each rig retains separate matrices and vertex groups. Unsupported rigs fail closed.

Quick test:

```sh
make clean all
python3 tests/test_phase16_ik_playback.py
./blendc89 --play-rigs examples/phase16_two_rigs_ik_SYNTHETIC.blend 5 examples/phase16_test
```

In the generated fixture, the IK end vertex reaches `(1, -2, 0)` at frame 5; the independent second rig reaches `(5, 0, 0)`. See `docs/PHASE16_SERIALIZED_IK2_PLAYBACK.md` for exact accepted conditions, reject cases and limitations. Native Blender-authored IK rig playback **has not been validated**.

---

## Phase 14 historical section (retained)

The new `src/bc_constraints.c` module decodes serialized `bPoseChannel.constraints` lists through their SDNA metadata and evaluates a deliberately **restricted** same-armature subset during `--play-rigs`:

- Copy Location (constraint type 9), all X/Y/Z axes, replace only;
- Copy Rotation (type 8), all axes, replace only, rigid/orthonormal 3x3 matrices;
- Copy Transforms (type 23), replace only, whole pose-space matrix.

Restrictions: pose space for both owner and target, full influence, headtail zero, target sub-bone of the **same armature Object**, target bone already evaluated, at most one active copy constraint per owner. Disabled or zero-influence constraints are skipped. Active IK/other constraint types, partial influence, copy offsets, mix modes, different target spaces/objects, forward references and cyclic graphs are rejected; the renderer does NOT silently ignore them. The older global `--play-obj` / `--play-blend` paths refuse active constraints rather than evaluate them against a possibly wrong object.

This is *not* a full Blender dependency-graph evaluation, nor a full IK implementation. Blender-authored character comparisons and correct handling of all constraint stacking, local/world spaces, parenting, drivers, B-Bones and NLA mixing are still open.

## Commands

```sh
make clean all
./blendc89 --rig-pairs scene.blend pairs.tsv
./blendc89 --play-rigs scene.blend 5 output_prefix
python3 tests/test_phase14_constraints.py
python3 tests/test_phase13_multirig.py
python3 tests/test_phase07_real_corpus.py /mnt/data/_blend_real_corpus
```

`--play-rigs` writes `output_prefix_mesh0000.obj`, etc. See `docs/PHASE14_CONSTRAINTS.md` for precise semantics, implementation and limits.

## Project restrictions

All code authored by this project is strict C89 (`-std=c89 -pedantic -Wall -Wextra -Werror`); no explicit malloc/calloc/realloc/free, floating-point types or `long long`. Source-archive Blender code was consulted for its on-disk structures but **not copied**. The included **precompiled third-party** Linux x86-64 static zlib/zstd libraries do not comply with the same guarantee; standard C file IO may allocate internally. Win32 uncompressed build instructions are provided but **cross-compilation was not tested here**. Native Blender 1.x–5.x animated rig compatibility remains unverified.

---

## Phase 15 — Analytic two-bone IK (bounded) and Blender IK audit

New files: `src/bc_ik2.c`, `include/bc_ik2.h`, `src/bc_ik_audit.c`.

An independently implemented two-segment IK solver works in 3D Q16.16 with a user-specified pole and produces two 3x4 deformation matrices for the existing `--skin-tsv` vertex kernel. This tool uses a **canonical straight +Y rest configuration** and cannot automatically reproduce arbitrary Blender bone orientation, constraints or IK controllers. `--ik-audit` scans stored Blender IK constraints but **does not evaluate them**. Blender IK in `--play-rigs` remains intentionally unsupported, rather than silently skipped.

```sh
make clean all
./blendc89 --ik2-palette examples/phase15_ik2_goal.tsv examples/phase15_palette.tsv examples/phase15_joints.tsv
./blendc89 --skin-tsv examples/phase15_palette.tsv examples/phase15_ik2_vertices.tsv examples/phase15_skinned.tsv
./blendc89 --ik-audit some_scene.blend ik_audit.tsv
python3 tests/test_phase15_ik2.py
python3 tests/test_phase15_ik_audit.py
```

Refer to `docs/PHASE15_IK2_ANALYTIC_AND_SDNA_AUDIT.md` for detailed contracts, real validation results and limitations. Phase 01–14 code is preserved.
