# PSKX shape keys / morph targets in fixed point

This module adds heapless PSKX morph support on top of the normal PSK point table.

## Parsed chunks

| Chunk | Role |
|---|---|
| `MRPHINFO` | Morph target metadata: fixed 64-byte name + delta count. |
| `MRPHDATA` | Morph deltas: position delta + affected point index. |

The implementation also accepts `MORPHINF*` and `MORPHDAT*` prefixes for exporter variants.

## Storage model

```text
psk3d_model
  morph_targets[]  -> name, vertex_count, data_offset
  morph_deltas[]   -> point_index, position_delta Q16.16
```

No heap is used. Tune capacities before including the header:

```c
#define PSK3D_MAX_MORPH_TARGETS 256u
#define PSK3D_MAX_MORPH_DELTAS  131072u
#define PSK3D_MAX_MORPH_LAYERS  32u
#include "pskpsa3d.h"
```

## Weight-array path

```c
psk3d_morph_weights weights;
psk3d_skinned_points morphed;

psk3d_clear_morph_weights(&weights);
psk3d_set_morph_weight(&weights, &model, smile_index, PSK3D_Q16_HALF);
psk3d_apply_shape_keys_points(&model, &weights, &morphed);
```

## Stack path

The stack path is useful for temporary expressions, layered gameplay damage morphs, or blend modules that do not want a full target-sized weight array.

```c
psk3d_morph_stack stack;
psk3d_skinned_points morphed;

psk3d_clear_morph_stack(&stack);
psk3d_morph_stack_push(&stack, blink_index, PSK3D_Q16_ONE);
psk3d_morph_stack_push(&stack, damage_index, PSK3D_Q16_HALF);
psk3d_apply_shape_key_stack_points(&model, &stack, &morphed);
```

## Morph + skinning path

Morphs are applied to bind/reference points first, then LBS uses the cache and current global pose.

```c
psk3d_build_skin_cache(&model, &cache);
psk3d_apply_shape_keys_points(&model, &weights, &morphed);
psk3d_skin_points_lbs_cached_from_points(&model, &cache, &global_pose, &morphed, &out);
```

Convenience path:

```c
psk3d_skin_points_lbs_cached_morphed(&model, &cache, &global_pose, &weights, &out);
```

## Notes

- Shape deltas are Q16.16 and may use negative weights or weights above 1.0 for authored overshoot.
- The code validates target ranges and point indices.
- Normal morph deltas are not implemented yet; normals can still be skinned from `VTXNORMS` after the point pass.
