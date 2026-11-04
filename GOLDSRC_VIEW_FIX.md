# GoldSrc / Half-Life viewer fix

This revision fixes two independent problems observed with real GoldSrc MDL assets.

## Animation pose path

`provider_hl1_89.c` now uses the quaternion sequence decoder from `hl1modelib`:

- `hlfp_mdl_decode_sequence_time_quat_ctrl()`
- `hlfp_mdl_quat_pose_to_world_matrices()`

This follows the GoldSrc Studio convention more closely than the old generic Euler path:
animated rotations are sampled as quaternion pairs and slerped, and `STUDIO_X/Y/Z`
motion-bone channels are removed from the skeleton pose before hierarchy assembly.

The reported clip end time is now `(numframes - 1) / fps`, matching the valid frame range.

## Regression fixture

`tests/goldsrc_motion_regression.mdl` contains a sequence whose root translates +100 on X
while declaring `STUDIO_X`. `tests/goldsrc_regression.c` verifies that the runtime exposes
root X = 0 at the animated frame, proving that motion extraction does not leak into the
rendered skeleton pose.

Run:

    make goldsrc-regression


## 2026-08-16: GoldSrc Z-up -> runtime Y-up basis fix

GoldSrc Studio/MDL/SMD data is evaluated in its native coordinate basis.  The
runtime viewer convention is Y-up, so the GoldSrc provider now converts only
its emitted views with the rigid basis change:

    runtime.x = goldsrc.x
    runtime.y = goldsrc.z
    runtime.z = -goldsrc.y

The same basis change is applied to final skinned vertex positions, normals,
and emitted bone positions.  Native bind/animation matrices remain untouched;
there is no format conversion and no re-authoring of clips.

Regression: `make goldsrc-axis-regression` opens a known SMD point (1,2,3) and
requires runtime (1,3,-2), while +Z native normal must become +Y runtime.
