# Phase 11 — Additive animation + synchronized locomotion cycles

## Source base

This is an incremental modification of the original Phase 10 ZIP, not a rewrite.
It retains the previous parsers, registry, animation core, vendors, tests, and
four existing manifest profiles.

## New modules (13 C files)

- `src/additive/gs3d_additive_math.c`: fixed-point local TRS reference deltas.
- `src/additive/gs3d_additive_validate.c`: skeletal clip/mask compatibility.
- `src/additive/gs3d_additive_skeletal.c`: bone-local additive sample + one skinning pass.
- `src/additive/gs3d_additive_legacy.c`: per-vertex morph additive for MDL3/4/5.
- `src/additive/gs3d_additive_apply.c`: common evaluation interface.
- `src/sync/gs3d_sync_phase.c`: master loop phase (Q16.16).
- `src/sync/gs3d_sync_sample.c`: follower sampling at normalized phase.
- `src/vendors/gs3d_vendor_additive_router.c`: generation dispatch.
- `src/vendors/a4/gs3d_a4_additive.c` through `a8/gs3d_a8_additive.c`:
  independent vendor entry points (shared binary and math core).

The original TRS parser in `src/anim/gs3d_trs_fixed.c` exposes four
internal helpers so there is no duplicate quaternion decoder.

## Math

Local translation: `baseT + strength * (addT - refT)`.
Local orientation: `baseQ * nlerp(identity, inverse(refQ)*addQ, strength)`.
Local scale: `baseS * [1 + strength * (addS/refS - 1)]`.
For each bone, `strength = global_strength * mask[bone]`.
The final hierarchy is computed once, followed by one skinning pass.
The rotation method is NLERP rather than exact SLERP; error is bounded by
approximation and quantization but has not been certified for all assets.
Affine shear, reflection, and degenerate scale are deliberately unsupported.

For MDL3/4/5, no bone masks are assumed; the operation is the per-vertex
`base + strength * (add - reference)` on positions. The base normal index is
retained; normal blending for morphs is **not implemented**.

## Loop synchronization

`gs3d_sync_phase` maps a loop clip to normalized Q16 phase.
`gs3d_sync_sample` maps that phase onto another loop clip of potentially
another frame count or speed, optionally with a fixed phase offset.
No audio, foot-plant detection, contact markers, authored sync markers,
root-motion unwrapping, or automatic gait matching is implied.
Both clips MUST be explicit LOOP tracks; CLAMP/PINGPONG rejected.

## Build / tests

```
sh scripts/check_phase11.sh
./gs3d_additive tests/fixtures/mdl7_skeletal_sequence.mdl pose 7 0 0 1 0 32768 1
./gs3d_additive tests/fixtures/mdl3_morph_fraction.mdl legacy_pose 4 0 0 1 0 32768 0
```

- GCC and Clang C89 strict builds pass for Phase 11.
- AddressSanitizer/UBSan passed with the Phase 11 skeletal and mutated corpus tests.
  Retest after every local modification.
- 500 mutated MDL7 inputs and 9,000 loop phase/timing cases passed.
- Legacy-only and MDL7-only build variants passed.
- Prior regression tests 06, 07, 08, 09, and 10 passed individually
  after Phase 11 integration, compiled at `-O0`.
- No native MinGW32 compiler available; Win32 executables NOT certified.
- Real MDL7 model with weighted bones and additive clips NOT available in this
  corpus. Skeletal tests remain synthetic. Parser security coverage is partial.

## Honest remaining limitations

- Q16.16 precision, saturation for extreme coordinates/weights.
- Single-instance globals, no thread safety or concurrent models.
- Additive assumes the SAME bind skeleton, no retargeting.
- Additive reference is supplied explicitly; no automatic selection of idle.
- MDL7 deformer binary layout and world-matrix conventions need real-game QA.
- Multi-group evaluation does not guarantee atomic rollback on mid-read error.
- Source-only; does not modify the separate Model Runtime or game engine.

## Technical sources

- https://github.com/stride3d/stride-docs/blob/master/en/manual/animation/additive-animation.md
- https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Advanced_Topics/GLTF_Animation.html
- https://docs.godotengine.org/en/latest/tutorials/animation/animation_tree.html
- https://github.com/assimp/assimp/blob/master/code/AssetLib/MDL/MDLFileData.h
