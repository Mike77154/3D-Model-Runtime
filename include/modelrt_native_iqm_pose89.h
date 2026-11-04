#ifndef MODELRT_NATIVE_IQM_POSE89_H
#define MODELRT_NATIVE_IQM_POSE89_H
#include "model_runtime89.h"
/* Local IQM transform and computed global world transform for a sampled frame.
 * Quaternion XYZW and scale are preserved in original IQM axis convention.
 * This optional format-specific API does not change modelrt_bone_view ABI. */
typedef struct modelrt_iqm_pose89_s {
    modelrt_u32 joint_index;
    modelrt_i32 parent;
    modelrt_q16 translation[3];
    modelrt_q16 rotation_xyzw[4];
    modelrt_q16 scale[3];
    modelrt_q16 world_matrix[12];
} modelrt_iqm_pose89;
int modelrt_native_iqm_pose89(modelrt_handle *handle, int animation_index,
                               modelrt_u32 relative_frame, modelrt_u32 joint,
                               modelrt_iqm_pose89 *out);
/* Q16.16 seconds, quaternion shortest-path normalized interpolation. */
int modelrt_native_iqm_pose_time89(modelrt_handle *handle, int animation_index,
                                   modelrt_q16 seconds, modelrt_u32 joint,
                                   modelrt_iqm_pose89 *out);
#endif
