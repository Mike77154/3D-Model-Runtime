#ifndef IQM_MATH_H
#define IQM_MATH_H

#include "iqm_loader.h"

/*
   Fixed-point skeletal helpers for IQM.
   - 16.16 fixed point by default.
   - No heap allocations.
   - No C float/double declarations.
   - Matrices are row-major 3x4 affine transforms:
       x' = m0*x + m1*y + m2*z + m3
       y' = m4*x + m5*y + m6*z + m7
       z' = m8*x + m9*y + m10*z + m11
*/

typedef struct iqm_transform_s
{
    iqm_fixed translate[3];
    iqm_fixed rotate[4];
    iqm_fixed scale[3];
} iqm_transform;

typedef struct iqm_mat3x4_s
{
    iqm_fixed m[12];
} iqm_mat3x4;

#ifdef __cplusplus
extern "C" {
#endif

iqm_fixed iqm_fixed_abs(iqm_fixed v);
iqm_fixed iqm_fixed_div(iqm_fixed a, iqm_fixed b);
iqm_fixed iqm_fixed_sqrt(iqm_fixed v);
iqm_fixed iqm_fixed_lerp(iqm_fixed a, iqm_fixed b, iqm_fixed alpha);

void iqm_transform_identity(iqm_transform *out_trs);
void iqm_quat_identity(iqm_fixed q[4]);
void iqm_quat_normalize(iqm_fixed q[4]);
void iqm_quat_nlerp(iqm_fixed out_q[4], const iqm_fixed a[4], const iqm_fixed b[4], iqm_fixed alpha);

int iqm_get_base_joint_transform(const iqm_model *model, iqm_u32 joint_index, iqm_transform *out_trs);
int iqm_get_frame_pose_transform(const iqm_model *model, iqm_u32 frame_index, iqm_u32 pose_index, iqm_transform *out_trs);
int iqm_get_frame_pose_transform_lerp(const iqm_model *model, iqm_u32 frame_a, iqm_u32 frame_b, iqm_fixed alpha, iqm_u32 pose_index, iqm_transform *out_trs);

void iqm_mat3x4_identity(iqm_mat3x4 *out_m);
void iqm_mat3x4_from_transform(iqm_mat3x4 *out_m, const iqm_transform *trs);
void iqm_mat3x4_mul(iqm_mat3x4 *out_m, const iqm_mat3x4 *parent, const iqm_mat3x4 *local);
void iqm_mat3x4_transform_point(const iqm_mat3x4 *m, const iqm_fixed in_v[3], iqm_fixed out_v[3]);
void iqm_mat3x4_transform_vector(const iqm_mat3x4 *m, const iqm_fixed in_v[3], iqm_fixed out_v[3]);

int iqm_build_base_matrices(const iqm_model *model, iqm_mat3x4 *out_mats, iqm_u32 max_mats);
int iqm_build_frame_matrices(const iqm_model *model, iqm_u32 frame_index, iqm_mat3x4 *out_mats, iqm_u32 max_mats);
int iqm_build_frame_matrices_lerp(const iqm_model *model, iqm_u32 frame_a, iqm_u32 frame_b, iqm_fixed alpha, iqm_mat3x4 *out_mats, iqm_u32 max_mats);

int iqm_skin_vertex_position(const iqm_model *model, iqm_u32 vertex_index, const iqm_mat3x4 *joint_mats, iqm_u32 num_joint_mats, iqm_fixed out_pos[3]);
int iqm_skin_vertex_normal(const iqm_model *model, iqm_u32 vertex_index, const iqm_mat3x4 *joint_mats, iqm_u32 num_joint_mats, iqm_fixed out_normal[3]);

#ifdef __cplusplus
}
#endif

#endif
