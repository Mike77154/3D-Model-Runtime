# Fixed-point quaternion SLERP

This version adds SLERP as an optional, separate interpolation path.

## Why keep both?

- `psk3d_quat_nlerp()` is compact and fast. It is good for dense animation tracks and retro targets.
- `psk3d_quat_slerp()` keeps constant angular velocity along the quaternion arc. It is better for visible long rotations and slower key spacing.

The public sequence calls mirror that split:

```c
psk3d_sample_sequence_linear(&animset, seq, frame, t_q16, &pose);      /* NLERP */
psk3d_sample_sequence_slerp(&animset, seq, frame, t_q16, &pose);       /* SLERP */
psk3d_sample_sequence_linear_ex(&animset, seq, frame, t_q16,
                                PSK3D_ROT_SLERP, &pose);
```

## Integer-only design

The implementation does not call libm and does not declare real-valued language types. The path is:

```text
dot = normalize(a) dot normalize(b)
if dot < 0: flip b for shortest path
theta = acos(dot)
out = (sin((1-t)*theta) / sin(theta)) * a
    + (sin(t*theta)     / sin(theta)) * b
normalize(out)
```

`sin()` is approximated with a Q16.16 lookup table over `[0, pi/2]`, mirrored for `[0, pi]`.
`acos()` is found through a fixed-point binary search over the monotonic cosine curve on `[0, pi]`.

## Fallbacks

The SLERP function falls back to NLERP when the two rotations are almost identical or when the sine denominator becomes too small. This avoids unstable division near zero while preserving the API result.
