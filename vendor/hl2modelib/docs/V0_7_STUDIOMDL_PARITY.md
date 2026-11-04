# hl2modelib 0.7.0 - StudioMDL parity push

This pass moves `hl2modelib` from a simple clean-room Source model bundle writer toward a stricter compile/decode loop for StudioMDL-adjacent metadata.

## New QC metadata captured

The QC decoder now keeps structured side tables for:

- `flexcontroller`
- `flexrule`
- sequence-local `event`
- `ikchain`
- sequence-local `ikrule`
- multiple `$hbox` entries

The normal command list is still preserved, but the compiler no longer has to rediscover these concepts from raw token strings.

## Multiple hitboxes

The MDL writer now emits the QC hitbox list into the generated hitbox set. The MDL decoder reads the generated hitboxes back into `hl2m_mdl.hitboxes[]`, so a compile/decode regression can verify that the metadata survived.

## Sequence events

Sequence-local QC `event` commands are emitted into the generated sequence descriptor event table. The decoder reads them into `hl2m_mdl.events[]`, including event id, cycle, options and optional name metadata.

This is meant for engine dispatch and parity testing. It does not yet reproduce every StudioMDL event edge case.

## IK chains and IK rules

The QC decoder captures `ikchain` and `ikrule` commands. The writer emits IK rule records into the generated animation descriptor region, and the decoder reads them back into `hl2m_mdl.ikrules[]`.

This gives a fixed-capacity metadata path for engines and tools. It is not a full Source IK runtime solver.

## Flex controller bridge

VTA already stores morph target deltas. The new flex controller path adds:

```c
hl2m_flex_state_from_qc(...);
hl2m_flex_state_set(...);
hl2m_engine_apply_flex_controllers(...);
```

For simple controller-to-target rules, an engine can now apply QC-side flex controller state to VTA target deltas without dynamic allocation.

## Grouped collision hulls

`hl2m_build_convex_decomp_from_smd()` now supports deterministic grouping modes:

```c
HL2M_COLLISION_AABB
HL2M_COLLISION_PER_BONE_AABB
HL2M_COLLISION_PER_MATERIAL_AABB
HL2M_COLLISION_BONE_MATERIAL_AABB
```

Each group becomes a compact hull record and can be written through `hl2m_write_phy_from_convex_decomp()`. The high-level compiler uses this path for `.phy` output.

This is intentionally conservative: it is fixed-point, deterministic, bounded and portable. It is not a complete VPhysics convex decomposition or HACD solver.

## New regression probe

`hl2m_parity_probe` compiles `QC + SMD + anim SMD + VTA`, decodes the generated bundle and fails unless it sees:

- at least two QC-driven MDL hitboxes,
- at least one sequence event,
- at least one IK rule,
- at least one flex controller,
- at least one grouped PHY solid.

Synthetic input:

```text
examples/synthetic/test_parity.qc
```

Run:

```sh
make check
```

## Scope

`hl2modelib` 0.7.0 is still not a byte-for-byte clone of StudioMDL. The goal here is clean-room, C89, fixed-capacity data paths that preserve key Source model concepts and can be used by an engine or exporter without dynamic allocation.
