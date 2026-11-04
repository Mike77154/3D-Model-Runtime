# V6: PSA blend stack + final triangle stream

This module pushes the importer closer to a runtime-ready asset path while staying C89, heapless, and Q16.16-only.

## PSA blend stack

`psk3d_anim_stack` is a static list of animation layers. Each layer samples one PSA sequence at a fixed frame + subframe and blends it into an output pose.

Supported layer modes:

- `PSK3D_BLEND_OVERRIDE`: blends current pose toward the sampled layer.
- `PSK3D_BLEND_ADDITIVE`: treats the sampled layer as a delta from a base pose and adds that delta into the current pose.

Rotation blending uses the selected quaternion path per layer:

- `PSK3D_ROT_NLERP`: cheap fixed-point path.
- `PSK3D_ROT_SLERP`: higher-quality fixed-point path using the existing sine LUT / acos search.

Example:

```c
static psk3d_anim_stack stack;
static psk3d_pose base_pose;
static psk3d_pose local_pose;

psk3d_clear_anim_stack(&stack);
psk3d_anim_stack_push(&stack, idle_seq, idle_frame, idle_t,
                      PSK3D_Q16_ONE, PSK3D_ROT_NLERP,
                      PSK3D_BLEND_OVERRIDE);
psk3d_anim_stack_push(&stack, aim_seq, aim_frame, aim_t,
                      PSK3D_Q16_HALF, PSK3D_ROT_SLERP,
                      PSK3D_BLEND_ADDITIVE);

psk3d_sample_anim_stack(&animset, &stack, &base_pose, &local_pose);
```

## Pose helpers

- `psk3d_pose_blend()` blends positions/scales linearly and rotations by NLERP or SLERP.
- `psk3d_pose_apply_additive()` adds position/scale deltas and applies weighted quaternion deltas.
- `psk3d_quat_conjugate()` is now public for callers that need their own delta rotations.

## Final triangle stream

PSK stores faces through wedge indices, and wedges point back to base points. That is faithful to ActorX, but not the most convenient shape for a renderer. V6 adds a resolved stream:

```c
static psk3d_triangle_stream stream;

psk3d_build_triangle_stream(&model, &skinned_points,
                            &skinned_normals, &stream);
```

Each `psk3d_final_vertex` contains:

- skinned position
- skinned or fallback normal
- main UV
- first extra UV set, if present
- vertex color, if present
- original point/wedge/material indices

The stream is intentionally non-indexed. That makes it simple for immediate-mode/debug renderers, retro ports, software rasterizers, or later dedup passes owned by the engine.

## Memory model

No allocation is performed. Caller owns the `psk3d_anim_stack`, `psk3d_pose`, and `psk3d_triangle_stream` storage. For large meshes, define:

```c
#define PSK3D_MAX_TRI_VERTS (MY_FACE_CAP * 3u)
#define PSK3D_MAX_ANIM_LAYERS 16u
#include "pskpsa3d.h"
```
