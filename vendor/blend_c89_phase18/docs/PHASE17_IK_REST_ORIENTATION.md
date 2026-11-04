# Phase 17 — Rest-axis and roll-aware analytic IK2 in strict C89

**Scope:** extend Phase 16's bounded serialized `bKinematicConstraint` playback from canonical +Y rest frames to arbitrary, rigid Blender bone rest directions and rest roll. **This is not Blender IK solver parity.**

## Consulted user-supplied sources

- `blender-2.79a/source/blender/makesdna/DNA_armature_types.h`: `Bone.arm_head`, `Bone.arm_tail`, `Bone.arm_mat`, `arm_roll`.
- `blender-4.5.14/source/blender/makesdna/DNA_armature_types.h`: corresponding rest-space fields and their comments.
- No code was copied from Blender. The phase contains an independent C89 mathematical implementation.

## Changed source modules

- `include/bc_ik2.h`: `bc_ik2_solve_rest()` and `bc_ik2_segment_length()` APIs.
- `src/bc_ik2.c`: checked Q16.16 segment lengths, rest-frame validation, shortest-arc swing quaternion, deterministic antiparallel fallback, and bone pose matrices that preserve the original roll.
- `src/bc_playback_tsv.c`: `Bone.arm_head`/`arm_tail` ingestion, IK rest-frame storage per bone, rest-aware solve in `--play-rigs`, tolerant checked comparisons for Q16.16 parent/child matrix round-off.
- `tests/test_phase13_multirig.py`: optional noncanonical synthetic Blender DNA rigs (keeps original fixtures unchanged by default).
- `tests/test_phase17_rest_orientation.py`: full serialized multirig playback with cardinal, rolled and oblique rest frames; deliberately invalid rest frames.
- `tests/test_phase17_rest_roll.c`: direct strict-C89 kernel test proving that pose local axes retain rest roll while ordinary non-antiparallel swing deformation remains independent of that local roll.

## Algorithm

For each IK bone, read serialized armature-space rest head/tail and rest matrix. Check that:

1. Rest transform is rigid and invertible (orthonormal 3x3, no scale or shear).
2. Translation agrees with the rest head within a small fixed-point tolerance.
3. `rest_matrix * local(+Y, bone_length)` agrees with the actual head-to-tail vector.
4. Child head agrees with parent tail.
5. Bone lengths match the original two-link analytic IK goal.

Solve the two-link joint positions with the unchanged analytic solver. For each bone, compute the shortest-arc quaternion mapping its original head-to-tail direction into its solved direction, convert to a Q16.16 rotation matrix, and apply it to the complete original rest basis. The posed bone origin is the IK root or elbow. This preserves the bone's original local roll instead of rebuilding a global-up look-at frame. The deformation matrix remains `pose * inverse(rest)` in armature space, subsequently transformed to mesh space by Phase 10.

**180-degree ambiguity:** for rigorously antiparallel segments there is no unique shortest rotation axis. The implementation chooses the bone's rest-local X axis. Nearly-antiparallel but numerically ambiguous cases are rejected. This is deterministic but **not necessarily Blender-equivalent** in twist/pole behavior.

## Commands

```sh
make clean all
python3 tests/test_phase17_rest_orientation.py
cc -std=c89 -pedantic -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -Iinclude \
   src/bc_ik2.c src/bc_rig_q16.c tests/test_phase17_rest_roll.c \
   -Wl,--gc-sections -o /tmp/test_ik17_rest
/tmp/test_ik17_rest
./blendc89 --play-rigs path/to/compatible_scene.blend 5 frame
```

No new CLI is necessary: `--play-rigs` now automatically uses rest-aware IK if a compatible IK constraint is present. `--ik2-palette` remains backward-compatible with its canonical standalone Phase 15 goal format.

## Observed validation results

- 48 positive serialized `.blend` synthetic playback scenarios: 12 cardinal/rolled (3 variants × 4 pointer/byte-order layouts) plus 36 oblique 3D rest rotations with varied roll. Every test also checked another independent rig in the scene.
- 12 negative synthetic rest input cases: mismatch between rest matrix and bone endpoints, disconnected bone chain, and scaled rest matrix, each in four layouts. Rejected with no partial OBJ output.
- Direct C89 roll orientation kernel probe passes.
- Source-only (uncompressed) strict C89 build and 48+12 tests pass.
- GCC and Clang strict C89 builds pass, as does the new suite under Clang UBSan with no detected undefined behavior in those runs.
- Phase 09, Phase 12, Phase 13, Phase 14, Phase 15 and Phase 16 regression suites pass.
- Seven authentic `.blend` files extracted from historic Blender **source distributions** still export their expected OBJ vertex/face/UV counts. These files do **not** validate authentic IK animation playback.

Full actual execution transcript: `docs/PHASE17_TEST_RUN.log`.

## Explicitly unsupported

- More than 2 IK links, arbitrary pole-angle semantics, NLA mixing or complex IK flags, moving/parented IK targets, IK stretch, IK limits, constraints stacked with IK, drivers, B-Bones, iterated Blender solver parity, automatic armature dependencies.
- Rest frames containing scale or shear; zero-length/disconnected bones; numerically ambiguous orientation cases.
- Genuine Blender-authoring comparison of IK poses, especially roll behavior and longitudinal twist.
- Third-party Gzip/Zstandard binaries remain bundled **Linux x86-64** static archives, **not** portable heap-free C89 source. A source-only C89 build is available for uncompressed `.blend`; no Win32 cross compilation was performed here.

## Licensing

Blender DNA code was consulted as a behavioral reference, not copied. The existing project LICENSE applies to original implementation, while bundled third-party compression archives retain their separate licensing notices. Do not treat their presence as proof of the `no heap` condition.
