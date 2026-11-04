# hl2modelib 1.0.0 engine runtime push

This pass focuses on the runtime side of the Source-style model pipeline. Earlier versions could decode, compile, validate, sample animation, apply flex targets and perform CPU skinning. Version 1.0.0 adds a higher-level engine step plus helper APIs for the common runtime pieces that normally sit between a model codec and a renderer/collision system.

## New public helpers

```c
int hl2m_blend_poses(...);
int hl2m_apply_additive_pose(...);
int hl2m_build_attachment_poses_from_qc(...);
int hl2m_transform_hitboxes_to_world(...);
int hl2m_engine_filter_draws_for_lod(...);
int hl2m_runtime_eval_sequence(...);
```

## Runtime flow

```text
QC + reference SMD + animation SMD + VTA + engine mesh
        |
        v
frame-time pose sampling
        |
        v
IK metadata solve
        |
        v
pose matrices + inverse bind
        |
        v
flex controller / VTA application
        |
        v
CPU skinning
        |
        v
sequence event collection + LOD draw filter
```

## Attachments

`hl2m_build_attachment_poses_from_qc()` resolves QC `$attachment` records against current pose matrices. The output includes the attachment name, source bone, local position, world position and a 3x4 world matrix. This is intended for muzzle flashes, item sockets, camera tags and engine-specific markers.

## Hitboxes

`hl2m_transform_hitboxes_to_world()` transforms local hitbox AABBs through the chosen bone matrix and rebuilds a world-space AABB from the eight transformed corners. This keeps the API conservative and deterministic while still allowing gameplay code to query animated hitboxes.

## LOD draw filtering

`hl2m_engine_filter_draws_for_lod()` compacts the draw table to the requested LOD. If the source engine model only contains base draws, it falls back to LOD 0 and records a warning bit rather than failing. Vertex/index buffers remain caller-owned and fixed-capacity.

## Probe

`hl2m_sequence_runtime_probe` validates the whole runtime path in the synthetic suite:

```sh
./hl2m_sequence_runtime_probe examples/synthetic/test_parity.qc examples/synthetic/test_joints.smd examples/synthetic/test_joints_anim.smd examples/synthetic/test_joints.vta
```

The probe checks sampled bones, skinning, flex, events, LOD selection, attachments, transformed hitboxes and pose blending/additive helpers.

## Scope

This is still a clean-room, deterministic, fixed-capacity runtime bridge. It is not a byte-exact reproduction of the closed StudioMDL/runtime behavior, but it is more practical for dropping the generated Source-style asset bundle into an independent game engine.
