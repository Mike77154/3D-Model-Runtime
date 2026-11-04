# Phase 03 — MDL7 animation (experimental rigid skinning)

## New modules

| Module | Responsibility |
|---|---|
| `src/mdl7/gs3d_mdl7_keys.c` | Decode on-disk MDL7 frame/bone key records |
| `src/anim/gs3d_math.c` | Saturating Q16.16 affine mat4 multiply/point transforms, ISO C89 |
| `src/anim/gs3d_pose_keys.c` | Load keys from first group and default missing bones to identity |
| `src/anim/gs3d_pose_hierarchy.c` | Rest and world matrices, skin matrices per parent relationship |
| `src/anim/gs3d_pose_skin.c` | Public build/apply pose and transform point API |
| `src/anim/gs3d_clip.c` | Find exactly named frames without separate clip allocations |

## Steps

- `gs3d_open(...)` imports a model into fixed global buffers.
- `gs3d_build_pose(group, frame)` loads matrices from MDL7 group 0, assembles joint hierarchy, creates `gs3d_skin_matrix[bone]` in Q16.16.
- `gs3d_apply_pose(group, frame)` ALSO restores group rest vertices and frame position overrides, then applies rigid skinning using each vertex's bone index; safe to call repeatedly.
- `gs3d_transform_point(bone, &rest_point, &posed_point)` transforms one point after a successful build.
- `gs3d_find_frame(group, "walk")` returns a frame index or -1.
- `gs3d_export input.mdl output_prefix [frame_number]` optionally exports a posed OBJ + MTL/TGA.

## Algebra and assumptions

Disk matrix is read as sixteen binary32 elements in column-major order; all components are represented as signed Q16.16 in memory, no float/double operations. For affine frames, last row is normalized. Joint local rest offset is the bone xyz, relative to parent; for animation key K, `world = parentWorld * Translate(localRest) * K`; `skin = world * Translate(-absoluteRest)`. This is a **documented provisional convention** that passes synthetic bind/pivot tests, **not verified for every commercial MDL7**. Some MDL7 implementations may use different matrix conventions. Use real files to confirm before relying on full game animation fidelity.

Frame-vertex record `vertindex` indexes the vertex to overwrite; it does **not** replace that vertex's skin bone attachment. This bug was corrected in phase 03.

## Deliberate boundaries

- Supports **rigid one-bone-per-vertex**, not a weighted blend. If a group declares a deformer or max weights, the reader returns `GS3D_ERR_UNSUPPORTED` rather than guessing at layout.
- Does not interpolate rotations between frames, interpret animation timebases, or compute skinned normals.
- Frame names are exact matching; no inferred animations from prefixes.
- No C++ or dynamic allocation; all matrices held in static bounded buffers.
- Global state is not thread-safe and source bytes/callbacks must remain valid until `gs3d_close`.
- Intermediate Q16 operations saturate at 32-bit range, including coordinate overflow.

## Tests

Run `./BUILD_LINUX_TEST.sh`. `tests/test_pose.c` checks Q16 saturation, matrix composition, one-bone translation, two-bone pivot rotation, independent groups, frame vertex override attachment, idempotent frame pose application, exact name lookup and unsupported weighted group rejection. `tests/test_robustness.c` truncates and mutates synthetically generated records, exercising poses where feasible. No audited fuzzing guarantee.

## Sources

- https://github.com/assimp/assimp/blob/master/code/AssetLib/MDL/MDLFileData.h
- https://github.com/assimp/assimp/blob/master/code/AssetLib/MDL/MDLLoader.cpp
- https://github.com/assimp/assimp/issues/6633
- https://github.com/assimp/assimp/issues/6632
- https://packages.debian.org/forky/all/assimp-testmodels/filelist

The software source is an independent original C89 implementation; third-party code and model samples are NOT bundled.
