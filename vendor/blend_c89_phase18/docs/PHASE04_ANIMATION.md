# Phase 04 — Animation architecture and integration notes

## Independent vendors

```text
BLEND file
   |
   +-- scan BHead / SDNA ----------------------------- binary / DNA / refs
   |
   +-- Mesh / UV / weights / bones ------------------- previous modules
   |
   +-- bAction [AC] --+-- legacy curves -> FCurve ----- bc_anim_legacy.c
   |                  |
   |                  +-- Layer/Strip/Channelbag ------ bc_anim_layered.c
   |                                   |
   |                      shared FCurve keys --------- bc_anim_fcurve.c
   |
   +-- Ipo [IP] -> IpoCurve -> BezTriple / BPoint ---- bc_anim_ipo.c
   |
   +-- Object [OB] -> bPose -> bPoseChannel --------- bc_pose.c
```

## Data exchange contract

1. `--bones` emits hierarchy and reference/rest positions, and its indices are local to each armature.
2. `--weights` emits per-vertex group influence, whose names may correspond to armature bones **only after object modifier / parenting semantics are resolved**.
3. `--keys` extracts time/value curves, handles, component and a string RNA path; these are not already sampled matrices.
4. `--pose` extracts saved local channel LRS values (object-space relationships and parent composition are *not* evaluated).
5. `--ipo` extracts historical numeric channel values. Mapping to contemporary property names requires vintage `blocktype` and `adrcode` tables.

## What a full evaluator still needs

- FCurve evaluation: constant, linear, Bezier via deterministic integer Q16 curve solver, plus all easing variations.
- Action slot assignment: traverse `AnimData.action` and `AnimData.slot_handle` for a given ID; do not merely assume any slot is bound to any object.
- Object-to-Armature deforming modifiers and correct vertex-group/bone linking.
- Bone rest and pose matrices; normalization/clamping of quaternions in an appropriate wider fixed-point intermediate.
- Layer/strip blending and timing; NLA strips; constraints, parenting, modifiers/driver behavior.
- Actual Blender corpora across 1.x, 2.4x, 2.79, 2.93, 3.6, 4.4, 4.5 and 5.x. Only claim each vendor after passing positive and negative real-file tests.

## Formats by origin

- Historic IPO uses `IpoCurve` numeric `blocktype`/`adrcode`, and stores its `ipo` interpolation as an independent value. `BPoint` and `BezTriple` can store samples.
- Legacy Animato uses `bAction.curves` containing linked `FCurve`; each `FCurve` has `bezt` or `fpt`, a `totvert`, `array_index`, and `rna_path` (or modern `rna_path_ptr`).
- Layered/slotted Action, introduced around Blender 4.4, introduces arrays of `ActionLayer`, `ActionStrip`, `ActionStripKeyframeData`, `ActionChannelbag`, and slot handles. A zero-layer Action may still hold legacy curves.

## Testing strategy

- 11 synthetic modern/legacy/pose cases, 4 historical IPO cases.
- 120 deterministically mutated binary fixtures for `--keys`, `--ipo`, `--pose`; no process signal termination. This is a finite smoke check, not exhaustive fuzz testing or a security audit.
- Previous Phase 01–03 regression suite remains in the repository unchanged.

## Important restrictions

The project C source never manipulates IEEE values as host floating-point. The Q16 export is lossy and should not replace lossless original bit storage in a later runtime. File I/O uses `FILE*` for portability, with no *explicit* heap allocations; the standard library's internal allocation policy is outside this source-level guarantee.
