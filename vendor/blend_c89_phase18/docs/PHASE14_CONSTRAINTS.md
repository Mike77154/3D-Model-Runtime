# Phase 14 — Restricted pose constraints and per-rig evaluation

## Source analysis

Studied user-provided Blender `blender-2.79a.tar.gz` and `blender-4.5.14.tar.xz`, specifically their `source/blender/makesdna/DNA_constraint_types.h` and adjacent object/armature DNA definitions. Types retained in the disk schema: `bConstraint`, `bLocateLikeConstraint`, `bRotateLikeConstraint`, `bTransLikeConstraint`. The numeric constraint type identifiers are 9, 8, and 23. Disabled mask bits are `CONSTRAINT_DISABLE (1<<2)` and `CONSTRAINT_OFF (1<<9)`; space code 2 is pose space.

Source Blender files are GPL and **not included** in this source package. Our module is an independently written reader of the documented binary structures.

## Execution order

1. `--play-rigs` identifies Mesh/Armature object relationships through serialized pointers, not names.
2. `bc_play_rest_select()` collects the armature's own rest bones and parent order.
3. `bc_play_constraints_setup()` finds the armature Object's pose channels and validates all active constraint records using SDNA offsets, target object pointers and subtarget bone names. Rejects unknown or unsupported active constraints, invalid SDNA pointers and cycles (bounded traversal).
4. `bc_play_pose()` evaluates the basis transforms for a bone, then applies its approved copy operation in armature pose space against a previously evaluated target. Finally, deformation matrices are recomputed from the **constrained pose** (not from the unconstrained pose).
5. The pair-specific skinning logic transforms mesh-local coordinates with the corrected armature/mesh spaces and exports OBJ.

This evaluation is **narrower than Blender's**. It intentionally rejects cases such as target bones that evaluate later in the tree, partial influences, ambiguous mix semantics, per-axis copies, scaling/shear on Copy Rotation, or different coordinate spaces.

### Constraint behavior

| Type | Accepted | Result | Reject |
| --- | --- | --- | --- |
| Copy Location / 9 | `flag == 7`, pose→pose, influence 1 | replace pose-space translation 3-vector | inverted axes, offset, missing target |
| Copy Rotation / 8 | `flag == 7`, `mix_mode == 0`, auto Euler mode and orthonormal matrices | replace pose-space 3x3 rotation | scale/shear, partial axes, non-default mix |
| Copy Transforms / 23 | `flag == 0`, `mix_mode == 0` | replace entire pose matrix | other mix methods, shear options |

All transformations are performed in Q16.16, C89, row-major matrices, column-vector convention. No copying beyond one previously evaluated target or more than one effective active constraint per bone. A disabled constraint does not consume that allowance.

### Tests executed

- `tests/test_phase14_constraints.py`: **40 synthetic scenarios** spanning 32/64 bit pointers, little/big endian; Copy Location, Copy Rotation, Copy Transforms, disabled constraints, unsupported type, invalid target, self/forward target, partial influence, non-default mix and rejection of global legacy playback.
- Full regression suite passes for Phase 04–13 including FCurves, Euler, NLA, multirig, object/armature spaces, compression and UV.
- Seven **authentic static** Blender `.blend` files from the user-provided source archives still export their geometry/UV as before.

**Not verified:** comparison of a Blender-authored rig with Copy constraints against actual evaluated Blender pose matrices. The test binary assets generated here are synthetic. No complete Blender constraints, IK solver, NLA mixer or complete 1.x–5.x decoder is claimed.

### Unsupported behavior that remains important

- No IK (CCD, FABRIK, analytic two-bone solver), Spline IK, Stretch To, Damped Track, Follow Path.
- No inverse/negative constraint influence, animated constraints, target head-tail offsets or custom spaces.
- No general Copy Rotation decomposition from matrices with scale/shear.
- Constraints affecting *Object* transforms are rejected by the existing scene-link auditor.
- Precompiled compression libraries are Linux x86-64, exempt from the project's C89/no-heap guarantee; Win32 compressed decoding remains unverified.
- `--play-rigs` still requires simple supported Armature modifiers and a compatible Action or simple NLA strip.
