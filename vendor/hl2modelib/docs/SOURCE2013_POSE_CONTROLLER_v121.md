# hl2modelib v1.0.21 — Source 2013 normalized pose controllers

## Comparison with Valve Source 2013

Local SDK reference: `src/public/bone_setup.cpp`, functions
`Studio_LocalPoseParameter`, `Studio_SetPoseParameter`,
`CBoneSetup::AddSequenceLayers`, and `src/public/studio.h`
`mstudioposeparamdesc_t` / `studiohdr_t`.

- `CBoneSetup` takes **global normalized** pose controls (0 to 1), indexed by
  pose parameter ID. Source resolves per-sequence `seqdesc.paramindex[]`.
- Source converts the normalized control using the descriptor's `start/end`
  before interpolating the sequence's local `paramstart/paramend` or `poseKey`.
- In an autolayer, the child sequence can reference **different parameter
  indices**; copying the parent's two physical controls is incorrect.
- Pose parameters may loop (for example an angular parameter with loop=360).
  The encode/wrap operation follows the centered SDK normalization scheme.

## New API and vendor

- `hl2m_poseparams_read121`: validated MDL-local descriptor parser.
- `hl2m_pose_control_from_physical121`: physical to normalized (incl. loop).
- `hl2m_pose_physical_from_control121`: normalized to physical.
- `hl2m_pose_sequence_axes121`: per-sequence axis selector.
- `hl2m_master_sequence_controls121`: opt-in normalized global inputs and
  independent controller lookup for each recursive child sequence.

All code uses strict C89 / signed 32-bit Q16.16, no heap allocation, no
floating point and no 64-bit arithmetic. The shader, geometry and other
vendors remain unchanged. Invalid controller count and malformed MDL
pose parameter offsets return errors before changing the output pose.

## Reproducible checks

The `test_poseparams121` suite validates the real binary descriptor layout,
0.75 = +45 deg when the range is -90..+90, normalization round-trips,
a 360-degree wrapped +200 deg = -160 deg, different parameter IDs between
parent and child, corrupted descriptor tables and controller bounds.

The `hl2m_pose_controller_probe121` binary compares all local matrices
against the older physical-value sequence API, then samples two different
controller values to see whether the model actually changes pose.

Test corpus: `RE3RNemesis.zip`, `RE3RhunterB.zip`, `3FormsBirkinRE2R.zip`.

| Model | Sequence | 0.75 control physical | Matrix differences vs physical API | Bones changing from 0.25 to 0.75 |
|---|---|---:|---:|---:|
| Nemesis | `aim_pitch` | +45 deg | 0 | 3 |
| Nemesis | `aim_yaw` | +45 deg | 0 | 2 |
| Hunter beta | `walk_all` | +90 deg | 0 | 32 |
| Birkin G1 | `fast_walk` (autolayer) | no declared controllers | 0 | 0 |

**Limits:** These tests establish real MDL controller parsing and per-axis
motion selection, not runtime equivalence to a running Source 2013 renderer.
Virtual model remaps, `STUDIO_AL_LOCAL`, world-space layers, IK and complete
pose continuity remain unverified and unsupported in this new API.
