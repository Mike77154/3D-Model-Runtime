# hl2modelib 0.8.0 runtime parity push

This version continues the clean-room Source model pipeline with a stronger runtime-facing layer.

## Added API areas

- Flex controller evaluation:
  - `hl2m_flex_state_clamp()`
  - `hl2m_flex_eval_weights()`
  - `hl2m_engine_apply_flex_weights()`

- Events:
  - `hl2m_collect_qc_sequence_events()`

- IK:
  - `hl2m_ik_pose_from_smd_frame()`
  - `hl2m_solve_ik_rules()`

- Hitboxes:
  - `hl2m_build_hitboxes_from_qc_smd()`

- Collision cooking inputs:
  - `hl2m_convex_decomp_split_long_axes()`

- Parity summary:
  - `hl2m_build_studiomdl_parity_report()`

## Design notes

The goal is not to pretend byte-perfect StudioMDL equivalence. The goal is to make each StudioMDL-like feature survive as data through the clean-room pipeline and become visible to a game engine:

```text
QC/SMD/VTA source intent
    -> structured IR
    -> MDL/VVD/VTX/PHY/ANI writers
    -> decoders
    -> engine/runtime helpers
```

All additions keep the existing project constraints: C89-oriented, no dynamic allocation, no `float`/`double`, fixed-capacity structs, and 16.16 fixed-point math.

## Current limits

- Flex rules are evaluated with a compact deterministic stack evaluator, not Valve's full expression compiler.
- IK solving is a conservative fixed-point runtime bridge for authored QC metadata, not the exact Source animation runtime.
- Convex splitting is deterministic long-axis subdivision of hull boxes, not HACD/VPhysics/IVP byte-identical cooking.
- The parity report is a feature survival report, not a certification of byte-perfect StudioMDL output.
