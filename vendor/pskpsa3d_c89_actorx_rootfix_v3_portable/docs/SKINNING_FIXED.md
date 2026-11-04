# Fixed-point animation and skinning notes

This version adds the missing runtime stage after decoding PSK/PSA:

1. Read ActorX local bone transforms from PSK `REFSKELT` and PSA `ANIMKEYS`.
2. Sample animation keys using either nearest-frame or fixed-point linear interpolation.
3. Interpolate translations with Q16.16 lerp.
4. Interpolate quaternions with shortest-path NLERP for speed, or SLERP for constant angular velocity.
5. Convert local bone transforms to global transforms through the parent tree.
6. Skin every PSK point with Linear Blend Skinning:

```text
skinned_point = sum(weight_i * (current_global_bone_i * inverse(bind_global_bone_i) * bind_point))
```

Weights are read from PSK `RAWWEIGHTS`. If a point's weights do not sum to `1.0` exactly, the runtime normalizes the point's contributing weights on the fly. Points with no positive influence are copied unchanged.

## NLERP vs SLERP

`psk3d_sample_sequence_linear()` remains the fast path and uses shortest-path NLERP. `psk3d_sample_sequence_slerp()` is the quality path and uses a fixed-point SLERP implementation with a sine lookup table and an integer acos search. Both normalize their output and both flip the second quaternion when the dot product is negative, so they use the short rotation path.

## Public calls

```c
psk3d_build_skin_cache(&model, &skin_cache);
psk3d_sample_sequence_linear(&animset, seq, frame, t_q16, &local_pose);
psk3d_sample_sequence_slerp(&animset, seq, frame, t_q16, &local_pose);
psk3d_globalize_pose(model.bones, model.bone_count, &local_pose, &global_pose);
psk3d_skin_points_lbs_cached(&model, &skin_cache, &global_pose, &skinned_points);
```

## Fixed math added

```c
psk3d_q16_mul();
psk3d_q16_div();
psk3d_q16_sqrt();
psk3d_q16_lerp();
psk3d_quat_normalize();
psk3d_quat_mul();
psk3d_quat_nlerp();
psk3d_quat_slerp();
psk3d_quat_rotate_vec3();
```

All functions are integer-only. No `float`, `double`, `malloc`, `free`, or `realloc` are used by the core.

## Scope

This is CPU-side mesh deformation outputting skinned point positions. It deliberately does not own renderer buffers, GPU buffers, or platform I/O. Wedges/faces/materials remain in the parsed PSK model, so a renderer can consume `model.faces`, resolve wedges to point indices, and pull positions from `psk3d_skinned_points.points[point_index]`.


## Cached skinning path

The first LBS version is deliberately simple and scans all influences for every point. The cached path builds a point-to-influence linked list once:

```c
psk3d_build_skin_cache(&model, &skin_cache);
```

Then each animation frame can do:

```c
psk3d_globalize_pose(model.bones, model.bone_count, &local_pose, &global_pose);
psk3d_skin_points_lbs_cached(&model, &skin_cache, &global_pose, &skinned_points);
```

This avoids the big `for each point -> scan all RAWWEIGHTS` cost and turns the hot skinning stage into a pass over each point's own positive influences.
