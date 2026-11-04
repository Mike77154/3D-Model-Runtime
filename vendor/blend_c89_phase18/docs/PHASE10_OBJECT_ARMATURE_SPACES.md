# PHASE 10 — Object links, Armature modifier, and mesh/armature spaces

## Scope and historical observations

Investigated the user's Blender source archives (2.79a, 3.6.3 and 4.5.14):
- `Object.data` references the datablock (`Mesh`, `bArmature`, etc.), not a vertex array.
- `Object.parent` and `Object.modifiers` are serialized pointers/list bases.
- `ArmatureModifierData.object` is the armature **Object**, not `bArmature` data.
- `ArmatureModifierData.deformflag` selects vertex groups, envelopes, quaternion/dual-quaternion preservation, etc.
- Saved SDNA for some files uses `Object.size` for scale rather than source-level `Object.scale`.

No Blender GPL source was copied into new `.c` files. Original implementation relies on SDNA introspection at runtime.

## New files and API

- `src/bc_object_links.c`: `bc_export_object_links()`, `bc_object_playback_spaces()`
- `src/bc_rig_q16.c`: `bc_rig_inverse_affine()` (adjugate/determinant, checked Q16.16)
- `tests/test_phase10_objects.py`: synthetic modifier link, 32/64-bit and both endiannesses, failure paths; genuine startup-file Object list
- `tests/test_phase10_affine.py`: numerical inverse and roundtrip cases
- `tests/affine_probe.c`: C89 integer-only inverse test harness

CLI:

```sh
./blendc89 --objects scene.blend objects.tsv
./blendc89 --play-obj rigged.blend 5 frame0005.obj
./blendc89 --play-sequence rigged.blend 1 24 clip
```

The `--objects` output columns are:

`id, name, data_code, parent_id, armature_id, modifiers, armature_modifiers, other_modifiers, unresolved, local_trs_valid, 16 local_matrix_row_major_Q16 fields`

`local_matrix` is **raw local LRS only**. It does not incorporate animation, drivers, parenting, constraints, or the evaluated dependency graph. If unavailable it outputs `NA` values, never invented transforms.

## Coordinate correction

For one eligible Armature modifier with unparented mesh/rig objects:

- M = mesh object's local matrix
- A = armature object's local matrix
- D = bone's pose times inverse rest matrix in armature-local space
- C = inverse(A) * M, mapping mesh-local positions into armature-local positions
- D_mesh = inverse(C) * D * C
- v_out = weighted sum of D_mesh * v_in

The code evaluates the equivalent `inverse(M)*A*D*inverse(A)*M` via checked Q16.16 operations. Noninvertible matrices and arithmetic overflow are rejected.

**Binding guard:** for scenes containing serialized Object structures, `--play-obj` requires exactly one mesh datablock and one armature datablock connected through a single unambiguous Armature modifier; it also rejects additional modifiers on the mesh, unsupported modifier deformation flags, modifier vertex-group masks, parents, and constraints on the mesh or armature Object. This deliberately rules out meshes that need Blender's evaluated dependency graph rather than silently applying the wrong bone transforms. Legacy synthetic files without Object structures retain the previous basic playback route.

Only `--play-obj` / `--play-sequence` receive the object-space correction. `--play-blend` accepts explicitly armature-local vertex influences and is unchanged.

## Tests executed (2026-11-02)

- `make clean all`, using `cc -std=c89 -pedantic -Wall -Wextra -Werror -O2`
- Phase 10 synthetic mesh-to-rig link, cyclic modifier, invalid target, 32/64-bit pointer and little/big-endian samples: PASS
- 131 affine inverse/multiply roundtrips: PASS, max element deviation from identity `0.000229` (in Q16 decimal)
- Three genuine startup `.blend` file `--objects` reads: PASS, for archived 2.79a/3.6.3/4.5.14 sources (headers may have different Blender version)
- Seven earlier genuine OBJ extractions: PASS (geometric file reading only)
- Phase 08 and 09 synthetic playback and Euler regressions: PASS
- Earlier synthetic metadata, SDNA, mesh, IPO, UV, armature and FCurve tests: PASS

**Not yet validated:** an actual exported *animated* human/creature `.blend` model with its Blender-computed reference poses. Synthetic playback does not prove equivalence to Blender's renderer or dependency graph.

## Remaining substantial work

- Real animated rig corpus; compare frames and actual model animation with Blender.
- Multiple meshes, shared meshes, multiple armatures, per-object action/slot selection.
- Pose evaluation equivalence with constraints, IK, NLA, parenting, drivers, modifiers, B-Bones, dual-quaternion skinning and shape keys.
- Complete Gzip/Zstd source-only C89 Win32 vendors (Linux x86-64 currently uses static third-party libs whose implementation is not strict C89/no-heap).
- Legacy Blender 2.01/2.02 readfile and later-version genuine animation corpus.

This is an experimental, bounded reader/player and not a complete Blender replacement.
