# GoldSrc pose/raster fix notes

## Goal

Fix the GoldSrc MDL bone matrix path so meshes are baked with the same pose convention used by the Half-Life SDK: animation deltas are added to bone `value[]` via `scale[]`, rotations are converted through AngleQuaternion-compatible quaternions, and blended frames are interpolated with QuaternionSlerp before creating bone matrices.

## Industrial constraints kept

- C89 build flags remain active: `-std=c89 -Wall -Wextra -pedantic -O2`.
- Runtime path remains fixed-point 16.16.
- No `malloc`, `realloc`, `calloc`, or `free` were introduced.
- No heap-owned state was introduced; demo and runtime buffers are caller-owned/static.
- Existing fixed-capacity model limits and scratch arenas are preserved.

## Main changes

### `hlfp.h`

Added public fixed-point quaternion helpers:

- `hlfp_goldsrc_angles_to_quat`
- `hlfp_quat_slerp`
- `hlfp_quat_to_mat3x4`
- `hlfp_goldsrc_angles_to_bone_mat3x4`

### `hlfp_math.c`

Added GoldSrc-compatible AngleQuaternion and QuaternionMatrix paths in fixed point.
`hlfp_mdl_pose_to_local_matrices` now uses `hlfp_goldsrc_angles_to_bone_mat3x4` for MDL bone poses instead of the old entity/camera-style Euler matrix builder.

### `hlfp_ctrlquat.c`

Improved fixed-frame quaternion decoding:

- animation sample pair extraction follows the `angle1`/`angle2` pattern;
- position interpolation uses fractional frame position;
- rotation interpolation uses fixed-point QuaternionSlerp;
- 2-way and 4-way sequence blends use quaternion pair slerps instead of linear Euler blending.

### `demo_render_goldsrc_pose.c`

New software-render demo for real MDL input:

```sh
./demo_render_goldsrc_pose <model.mdl> <output_prefix> [sequence] [frame]
```

It writes:

- `<output_prefix>_skeleton.tga`
- `<output_prefix>_textured.tga`

Important defaults:

- `body_value = 0`
- `skin_family = 0`
- all bodyparts selected through GoldSrc bodygroup rules
- skeleton-only pass
- textured pass
- front capture convention: screen `(Y, Z)`, depth `X`

## Validation performed

Commands run in this package:

```sh
make clean
make all
make test_output.txt
./demo_render_goldsrc_pose /mnt/data/zombie_female.mdl /mnt/data/hl1fix/zombie_goldsrc_front 0 0
```

Results/logs are in `artifacts/`.
