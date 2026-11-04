# Phase 06 — Q16.16 rigid bind matrices and linear blend skinning

## Summary

This is a **tested integer-only skeletal math kernel**, integrated into the Phase05 `.blend` reader with a raw `Bone.arm_mat` exporter and two **offline** TSV commands. Do not describe this as a Blender-compatible, fully evaluated character animation renderer.

New C sources:

- `include/bc_rig_q16.h` — standalone API for the game-engine integration.
- `src/bc_rig_q16.c` — checked `Q16.16` add, multiply and divide, 4x4 matrices, TRS (unit `w,x,y,z` quaternion), rigid inverse, `pose * inverse(rest)`, already-topologically-ordered parent transforms and linear blend skinning.
- `src/bc_rest_matrices.c` — extract **serialized** `Bone.arm_mat` via per-file SDNA. Transposes Blender's column-major physical matrix order to row-major TSV for our math API.
- `src/bc_skin_tsv.c` — bounded streaming vertex skinning and deformation-palette composer.

## CLI

```
blendc89 --rest-matrices character.blend rest.tsv
blendc89 --palette-tsv rest.tsv evaluated_pose.tsv deform_palette.tsv
blendc89 --skin-tsv deform_palette.tsv vertices.tsv skinned.tsv
```

All earlier Phase 01–05 commands are retained. `--rest-matrices` requires valid `bArmature`, `Bone.arm_mat` and linked lists; rejects missing geometry. It reads raw **rest** matrices, not the animated pose. The `--palette-tsv` command accepts the exported `--rest-matrices` 20-column format for **one** armature (`armature=0`), or a 13-column matrix TSV, and a 13-column pose TSV. The pose TSV must be in **already-evaluated armature space**, which this phase does **not** generate from animation curves.

13-column matrix TSV (one row per bone, row-major 3x4 with implicit final row `[0,0,0,1]`):

```
bone_id m00 m01 m02 m03 m10 m11 m12 m13 m20 m21 m22 m23
0       1   0   0   2   0   1   0   0   0   0   1   0
```

The `--rest-matrices` variant writes `armature_id`, `bone_index`, `parent_index`, `bone_name`, and **16** row-major matrix scalars. The exported rest file alone must **not** be passed to `--skin-tsv` as a deformation palette.

Vertex TSV:

```
vertex_index x y z n bone_index_0 weight_0 [bone_index_1 weight_1 ...]
0 1 0 0 2 0 0.2500 1 0.7500
```

- Each coordinate and weight is a decimal with up to four digits after the point and is decoded to Q16.16. Bone ids are nonnegative integers; up to 8192 bones; at most **8 influences per vertex**.
- The input palette contains **deformation matrices** `P * inverse(R)`. It can be produced via `--palette-tsv` or supplied by an existing runtime. A pose may have scale, but the rest inverse currently accepts orthonormal rigid transforms only (no rest shear or scale).
- Weights are normalized per vertex, and zero total weight leaves the source position unchanged. This is ordinary **linear blend skinning**, not Blender dual-quaternion `Preserve Volume` mode.
- The generic parent-world helper requires an already ordered parent list. Blender's bone transform inheritance is more complex; its armature flags/IK/constraints are **not** modeled by that helper.
- The exporter **does not yet map `MDeformWeight.def_nr` and named vertex groups to `Bone` ids automatically**. Callers must construct the vertex TSV with the correct IDs; the old `--weights` output retains its group names to facilitate that mapping.

## Testing and limits

- Linux `cc -std=c89 -pedantic -Wall -Wextra -Werror -O2` build successful. `build_win32_mingw32.bat` updated, **not cross-compiled here**.
- 60,000 integer arithmetic comparisons against exact Python reference for signed multiplication and division. Q16.16 math returns errors on overflow and never silently wraps its arithmetic.
- C89 rig harness and strict UBSan harness succeeded.
- New synthetic fixtures: 32-bit and 64-bit serialized pointer variants, `--rest-matrices` transpose, pose/rest composition, weighted skinning, bad indices and unsupported rest scale.
- 120 malformed TSV mutations did not cause signal termination. Prior Phase01–05 regression tests also pass.
- **No genuine Blender-generated `.blend` corpus was executed for this phase**, and therefore no real Blender version has a demonstrated full end-to-end animation import.
- No NLA, drivers, IK, constraints, B-Bones, vertex-group automatic mapping, quaternion skinning, envelope weights, non-rigid inverse, Gzip/Zstandard or >2GiB files.
- No explicit allocation (`malloc`/`calloc`/`realloc`/`free`) or C `float`, `double`, `long long` types in project C files. The host standard I/O library **may allocate memory internally**, so this is not a process-wide zero-heap guarantee.

## Examples

```
make
./blendc89 --palette-tsv examples/rest.tsv examples/pose.tsv examples/deform_palette.tsv
./blendc89 --skin-tsv examples/deform_palette.tsv examples/vertices.tsv examples/skinned.tsv
```

The included examples are artificial skeletal data and should not be mistaken for real Blender sample exports.

## References (algorithm guidance; not copied code)

- Blender armature modifier/vertex group binding: https://docs.blender.org/manual/en/latest/modeling/modifiers/deform/armature.html
- Blender bone matrix notes: https://github.com/blender/blender/blob/main/source/blender/makesdna/DNA_armature_types.h
- Blender pose/rest deformation example: https://github.com/blender/blender/blob/main/doc/python_api/examples/bpy.types.PoseBone.bbone_segment_matrix.0.py
