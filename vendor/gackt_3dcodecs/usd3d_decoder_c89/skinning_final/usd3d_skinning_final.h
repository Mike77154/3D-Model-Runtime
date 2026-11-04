#ifndef USD3D_SKINNING_FINAL_H
#define USD3D_SKINNING_FINAL_H

#include "usd3d.h"
#include "usd3d_skinning_cpu.h"

#ifndef USD3D_SKINNING_FINAL_MAX_JOINTS
#define USD3D_SKINNING_FINAL_MAX_JOINTS 256
#endif

typedef struct usd3d_skinning_pose_s {
    usd3d_fixed joint_local[USD3D_SKINNING_FINAL_MAX_JOINTS * 16UL];
    usd3d_fixed joint_world[USD3D_SKINNING_FINAL_MAX_JOINTS * 16UL];
    usd3d_fixed inverse_bind[USD3D_SKINNING_FINAL_MAX_JOINTS * 16UL];
    long parent_index[USD3D_SKINNING_FINAL_MAX_JOINTS];
    unsigned long joint_count;
} usd3d_skinning_pose;

void usd3d_skinning_pose_init(usd3d_skinning_pose *pose);
void usd3d_skinning_matrix_identity(usd3d_fixed *m16);
int usd3d_skinning_matrix_mul(const usd3d_fixed *a16, const usd3d_fixed *b16, usd3d_fixed *out16);
int usd3d_skinning_pose_compute_world(usd3d_skinning_pose *pose);
int usd3d_skinning_pose_compute_skin_matrices(const usd3d_skinning_pose *pose, usd3d_fixed *out_skin_matrices, unsigned long out_joint_count);
int usd3d_skinning_apply_blendshape(usd3d_vec3 *point, const usd3d_vec3 *offset, usd3d_fixed weight);
int usd3d_skinning_vertex_final(const usd3d_vec3 *bind_point,
                                const usd3d_vec3 *blend_offsets,
                                const usd3d_fixed *blend_weights,
                                unsigned long blend_count,
                                const usd3d_u32 *joint_indices,
                                const usd3d_fixed *joint_weights,
                                unsigned long influence_count,
                                const usd3d_fixed *skin_matrices,
                                unsigned long joint_count,
                                usd3d_vec3 *out_point);

#endif
