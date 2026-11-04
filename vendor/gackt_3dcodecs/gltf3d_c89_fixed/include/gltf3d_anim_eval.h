#ifndef GLTF3D_ANIM_EVAL_H
#define GLTF3D_ANIM_EVAL_H

#include "gltf3d.h"

#ifdef __cplusplus
extern "C" {
#endif

int gltf3d_anim_eval_channel_fixed(const gltf3d_asset *a,
                                   int animation_index,
                                   int channel_local_index,
                                   gltf3d_fx time_seconds,
                                   gltf3d_fx *out_values,
                                   int out_value_capacity,
                                   int *target_node,
                                   int *target_path);
int gltf3d_anim_apply_channel_to_node(gltf3d_asset *a,
                                      int animation_index,
                                      int channel_local_index,
                                      gltf3d_fx time_seconds);

#ifdef __cplusplus
}
#endif
#endif
