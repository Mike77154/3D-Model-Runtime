# hl2modelib v1.0.19 — Source 2013 sequence pose parameters, per-bone weights and additive layers

## Goal

Follow the *behaviour* (not copy the implementation) of Source SDK 2013 `src/public/bone_setup.cpp` / `src/public/studio.h`, particularly `Studio_LocalPoseParameter`, `CalcAnimation`, `SlerpBones`, `AddSequenceLayers` and `mstudioseqdesc_t`. ANSI C89, Q16.16 only. The vendored Source SDK itself is **not** redistributed in this ZIP.

## Discovered bug — the real cause

The animdesc `STUDIO_DELTA` flag makes **unanimated** bones neutral: position (0,0,0), rotation (0,0,0,1). In v1.0.18, the native sampler's Euler-pose branch treated DELTA bones correctly, **but** `a89_matrices_internal()` initialized its independent quaternion fallback array with `bone.quat` on *every* bone, restoring non-neutral rest rotations during matrix conversion. This was not obvious with absolute sequences; it catastrophically distorted automatic additive layers.

Fix: initialize fallback quaternions to identity for DELTA animdesc, and restore per-bone bind rotation/position only for *animated channels lacking* `STUDIO_ANIM_DELTA`, mirroring the SDK's `CalcBoneQuaternion` / `CalcBonePosition`. Unanimated channels remain neutral.

## Added vendored modules and APIs

- `vendors/source/animator/hl2m_sequence_layers89.c`: bounds-checked parsing of Studio seqdesc metadata (`numautolayers`, `autolayerindex`, `weightlistindex`, `posekeyindex`), bone-weight masks, pose-parameter mapping, cycle envelope with cubic spline/XFADE/NOBLEND, delta/absolute masks and POST order.
- `hl2m_master_sequence_metadata89`: inspect a sequence's metadata and decoded 32-bit fixed masks.
- `hl2m_master_sequence_pose89`: accepts **physical sequence-local** pose parameter settings (Source units, Q16.16), converts to grid coordinates using paramstart/end or posekeys; it differs from legacy `hl2m_master_sequence89` which continues to accept normalized (0..65536) values.
- `hl2m_master_layer_mask89`: explicit per-bone weighted rotation blend and relative delta overlay. For DELTA the incoming matrices **must be identity-relative**, not bind-space.
- `hl2m_master_sequence_layered89`: opt-in evaluation of a sequence with **one level** of its direct cycle-driven, non-local autolayers. Reports declared/applied/delta/skipped/unsupported. Fails rather than silently applying `STUDIO_AL_POSE`, `STUDIO_AL_LOCAL`, `STUDIO_WORLD` or nested delta autolayers incorrectly.

The old public APIs are unchanged; master ABI is incremented to `0x0119`. Engines compiling against old struct declarations should rebuild.

### Minimal usage

```c
hl2m_pose_matrices pose;
hl2m_sequence_layers_report89 layer_report;
int r = hl2m_master_sequence_layered89(mdl_bytes, mdl_len,
    ani_bytes, ani_len, &mdl, sequence, 32768, 0, 0,
    &pose, &layer_report);
if (r == HL2M_OK) { /* pose.local/world contain composed transforms */ }
```

`ani_bytes` may be `NULL` and `ani_len` zero when a model needs no external ANI.

## Real corpus tests (user-supplied; assets not included in source ZIP)

- `3FormsBirkinRE2R.zip`, `g1.mdl`, sequence `fast_walk` index 11: it declares automatic sequence 20, `add_eyeball` (STUDIO_DELTA + STUDIO_POST), authored in four RLE channels (bones 47,49,51,153). Initially the prototype's fallback-quaternion bug incorrectly changed 80 bones and rotated the entire model. After the fix, **4/156 bones** have significant local matrix changes, and the global bounding box is unchanged. No missing skinning bones, 157171 triangles.
- `fast_idle`, `fast_flinch_head`, `fast_reaction`, `fast_walk`, `fast_run`, `att11`, `att12`, `att13`: all eight G1 sequences apply the same automatic layer with four significant local bone changes.
- `RE3RhunterB.zip`, `hunterb.mdl` reference sequence: no autolayers; layered and base pose identical, 79 bones.
- `RE3RNemesis.zip`, `nemesis_ft2_v2.mdl`, `dodge_l`: no autolayers; layered and base pose identical, 124 bones.
- G1 exported pose at cycle 0.5: 471513 drawing vertices / 157171 triangles, no missing bones both before and after. Compared actual OBJ vertices: 21307 vertices moved more than 0.01 Source units (maximum 5.602 units); the affected region is local to the 4-bone layer.

These are **local runtime and external diagnostic rasterizer tests**, not direct frame-by-frame comparison with the Valve executable.

## Regression tests

`tests/test_delta_identity119.c`: DELTA animation with no channels and non-neutral bind quaternions must output identity local rotations and zero positions on all bones.

`tests/test_sequence_layers119.c`: validates per-bone weight mask, PRE/POST delta application, cycle envelope, pose parameter remapping and pose keys, unsupported pose-driven envelope.

Run:

```sh
make CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude' check
```

## Still not full Source parity

- Virtual model relative-sequence remapping, pose-driven autolayers, recursive layers, STUDIO_WORLD, Source's optional triangular 3-way blending, IK locks and automatic motion transition state are not reproduced.
- The new layer blender uses quaternion matrix reconstruction with 32-bit fixed approximations; precision differs from Source floating point.
- Envelopes with pose-driven settings deliberately return `HL2M_ERR_UNSUPPORTED` unless the caller resolves the required pose value.
- The API's physical pose controls are sequence-local, not the complete shared/global `CStudioHdr` pose parameter system.
- No new claim of full animation/shader correctness for the Licker, Nemesis, Hunter or Birkin.
