# hl2modelib v1.0.6 - native T-103 contact / 2-bone IK / kinematic physics

This implementation was compared against the **user-provided** Source SDK 2013
`src/public/bone_setup.cpp` and `src/public/bone_setup.h`.
Reference functions inspected: `Studio_SolveIK`, `Studio_AlignIKMatrix`,
`CIKContext::BuildBoneChain`, `CIKContext::SolveLock` and sequence IK context.
We do NOT copy or embed Valve's C++ code; the implementation is separately
written in ANSI C89 with only fixed-point 16.16, caller-owned state, no heap.

## What changed

- `vendors/source/ik/hl2m_ik_2bone89.c`: **two-segment IK** preserving
  bone segment lengths and preferred (current-pose) knee plane. Reach is
  clamped; thigh, shin and their descendants rotate with the joints. End-effector
  orientation is restored, then the toe descendant chain is recomposed.
  New hip, knee, ankle local matrices are rebuilt against their parents.
  Old root-only IK helper remains available.
- `vendors/source/physics/hl2m_character89.c`: caller-owned kinematic
  root-motion translation with ground penetration guard and foot-contact
  capture/release (hysteresis based on height and per-frame movement).
  This is NOT VPhysics; it is sufficient for a basic walking ground plane.
- `examples/hl2m_export_animated_obj_ik106.c`: `--ik-grounded` samples the
  earlier frames of a native walking clip, extracts the separate entity motion,
  records foot anchors in **world** space, solves IK in **model** space, and
  then performs CPU skinning. It uses named T-103 phase 1 legs and numbered
  phase 2/3 legs. The floor is **estimated** as minimum foot height across the
  clip; for real games the engine must supply floor probes/collision casts.
- `tests/test_sdk_ik106.c`: synthetic checks for foot movement, retained toe
  hierarchy, zero-weight identity, persistent contact target and grounded
  character motion.
- `tests/test_t103_ik_real106.c`: optional validation of both T-103 leg
  chains on all frames of native `Walk_F` in all 3 phases.

## Public master API

```c
hl2m_master_ik_two_bone89(&pose, &chain, &corrected, &report);
hl2m_master_foot_lock89(&foot_state, foot_world, floor_z,
                       3*HL2M_FIXED_ONE, 4*HL2M_FIXED_ONE,
                       &target_world, &planted);
hl2m_master_character_move89(&actor_body, world_motion_delta,
                             floor_z, step_height);
```

You must map each chain's hip/knee/ankle IDs from `mdl.bones[]`; do **not**
assume other Source-model rigs use T-103's bone names.

## Correct engine ordering

1. Decode current MDL/VVD/VTX + sampled sequence/animation.
2. Obtain the separate **entity** displacement from `hl2m_master_motion89`.
3. Query ground at each foot with the engine's own collision/PHY scene.
4. Update foot-contact anchors in world space using entity movement.
5. Convert contact targets back into the pose's model space.
6. Run **two-bone IK** for a planted leg; otherwise keep native animation.
7. Skin with `hl2m_master_skin89`; apply entity transform ONCE in presenter.
8. Blend contact weights on stance transition in engine to prevent pops.

## Limits and experimental results

Source IK evaluates authored IK rules, sequence-local blend weights, foot
orientations, knee direction hints, world locks and multiple animation layers.
This clean-room solver **does not yet parse Source IK rules from native MDL
sequences** nor perform a continuous time-domain blend. Its real-model test
uses a deliberately small target of (+1 X,+1 Z) and verifies solver stability;
that is NOT a certification of visual accuracy against Valve's original engine.

C89 approximations use 16-bit fractional transformations and bounded integer
products. Valid T-103 limb lengths fit well; out-of-range models return an
error rather than silently allocating or requiring floating point.

Preview `--ik-grounded` is diagnostic, not physical simulation: there is no
SMD motion extraction rewrite, no ragdoll, no VPhysics convex rigid bodies,
no contact constraints from original PHY, no surface friction solver, no
inter-frame pose interpolation or native sequence blend. Source 2013 rig
pipeline is the reference architecture, not code/behavioral parity.

**Corpus regression:** Phase1: 57 frames x 2 legs; Phase2: 130 x2;
Phase3: 255 x2; all valid in both bone chains for the small offset target.
This validates geometry and native pose input; do not interpret as restored
animation playback fidelity.

## Reproduce

```sh
make clean
make CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
make check CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
./test_t103_ik_real106 /path/to/tyrantphase1.mdl
./test_t103_ik_real106 /path/to/tyrantphase2.mdl
./test_t103_ik_real106 /path/to/tyrantphase3.mdl
./hl2m_export_animated_obj_ik106 model.mdl model.vvd model.dx90.vtx 1 28 output.obj --ik-grounded
```
