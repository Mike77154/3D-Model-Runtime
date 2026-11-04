# hl2modelib 1.0.16 — Source SDK sequence/gesture audit

## Scope and reference

Local reference: user-provided `source-sdk-2013-master.zip`, `hl2sdk-sdk2013.zip`, `bone_setup.cpp` and `studio.h` (`Source SDK 2013`). **Valve executable and its original GPU renderer were not run**. This is a source-code-guided reconstruction with independently rendered source-model assets.

| Source code function/concept | Prior 1.0.15 | v1.0.16 |
|---|---|---|
| `mstudioseqdesc_t::anim(x,y)` | One blend cell by index | Actual grid cells selected by normalized (Q16.16) pose parameters |
| `CalcAnimation`: `cycle * (numframes-1)` | Whole frame only | Fractional cycle, adjacent frames, local transforms mixed |
| `CalcPoseSingle` | No sequence blend evaluator | 1D/2D blend grid, bilinear row interpolation (NOT Valve's optional 3-way triangular blend) |
| `BlendBones` | Whole poses only | Local rotation-matrix interpolation with orthonormalization; **NOT exact quaternion slerp** |
| `AddLocalLayers` / additive gesture | No dedicated API | Explicit caller-weighted delta layer; it **does not inspect or apply Source layer rules automatically** |
| `Studio_LocalPoseParameter` | No normalized controls | Caller inputs normalized grid coordinate; cyclic poses, `posekey`, pose-loop mapping and virtualmodel remap not emulated |
| Root motion | Separate output available | Remains a separate entity-motion channel, intentionally not baked into skinned vertices |
| IK and local hierarchy | Partial SDK-inspired IK | Unchanged; exact Valve ground IK/local hierarchy pending |
| ANI external sections | Supported in 1.0.15 | Preserved, accepted in the new sequence sampler |

## New vendor

`vendors/source/animator/hl2m_sequence_eval89.c` compiles as its own translation unit.

- `hl2m_master_sequence89(mdl_bytes,mdl_size,ani_bytes,ani_size,&mdl,seq,cycle,axisX,axisY,&pose,ids)`
- `hl2m_master_gesture89(&base,&delta,per_bone_weights,strength,&pose)`
- `hl2m_sequence_sample_cycle89(...)` samples any standalone animation at a fractional cycle
- `hl2m_sequence_blend_local89(...)` interpolates compatible local transforms

`cycle`, `axisX`, `axisY` and `strength` are Q16.16 values (65536 = 1). The axes are **normalized cell coordinates**, *not* Source pose-parameter units. Negative/over-one cycle values wrap only for `STUDIO_LOOPING`; other sequences clamp. For blending, zero motion in some cells and exact Source three-way transitions need further work. The additive API requires caller-supplied delta pose and weights; it should not be applied to a pose just because its name contains "gesture".

## Real corpus tests

Version 1.0.16 built with `-std=c89 -O2 -Wall -Wextra -Werror -pedantic` and all existing tests.

| Asset | Sequence | Test |
|---|---|---|
| T-103 (57 bones) | `S_Walk`, grid 9x1 | Selected native animation cells, 4 snapshots, textured OBJ; no missing bones |
| T-103 | `S_Rage` | Four sequence-cycle samples, textured reconstruction |
| T-103 | `S_Melee_Gesture_RH` | Four samples; fractional sample at 1/4 cycle is between native frames 11 and 12 |
| Ustanak (63 bones) | `alert` (sectioned) | Four textured poses, no missing bones |
| Hunter beta (79 bones) | `att1` | Source-style cycle probe passes 3 selected samples |
| Lisa Trevor (28 bones) | `idle` | Cycle probe passes 3 selected samples |

For T-103 `S_Melee_Gesture_RH`, fraction 1/4 corresponds to frame 11.25, and compared with the original integer frame 11, 22,671 out of 22,671 drawing vertices have displacement > 0.001 Source units; average displacement ~0.1482, maximum ~1.184. This demonstrates fractional skinning, **not** that it matches Valve's interpolation exactly.

## Limitations that require a deeper Source comparison

1. Valve uses quaternion slerp (and `QuaternionAlign`) for `BlendBones` and some bone-specific blend methods. Current orthogonalized matrix interpolation is a rigid, portable approximation and may differ for large rotations.
2. Real `CalcPoseSingle` supports three-way triangular blending and `PoseIsAllZeros`, sequence blend weights, `posekeys`, pose-parameter remapping, virtual models, and fade timing. Not reproduced in full.
3. Layer ordering, bone-weight animation layers, loop events, realtime/cyclepose and IK rules are not automatically assembled by v1.0.16.
4. Anomalous Licker `Tongue` and Hunter ragdoll channels remain as in v1.0.15; optional guard is still separate. No silent cleanup of author assets.
5. Four screenshots per motion are **samples**, not original-frequency frame-accurate footage; no Source executable was launched.

## Reproduce

```
make CFLAGS="-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude" check
make CFLAGS="-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude" hl2m_sequence_probe116 hl2m_sequence_obj116
./hl2m_sequence_probe116 <model.mdl> -1
./hl2m_sequence_probe116 <model.mdl> <sequence_index>
./hl2m_sequence_obj116 <model.mdl> <model.vvd> <model.dx90.vtx> <seq> <cycle_Q16> <out.obj> [axisX_Q16] [axisY_Q16]
```

No bundled commercial game assets are shipped in the FULL SOURCE archive. Preview archive holds user-supplied data processed into sample OBJ/MTL/textures. Provenance belongs to the supplied corpus.
