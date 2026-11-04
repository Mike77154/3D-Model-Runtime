#include "usd3d_skinning_final.h"

#include <string.h>

void usd3d_skinning_matrix_identity(usd3d_fixed *m16)
{
    unsigned int i;
    if (m16 == 0) return;
    for (i = 0U; i < 16U; ++i) m16[i] = 0L;
    m16[0] = USD3D_FIXED_ONE;
    m16[5] = USD3D_FIXED_ONE;
    m16[10] = USD3D_FIXED_ONE;
    m16[15] = USD3D_FIXED_ONE;
}

void usd3d_skinning_pose_init(usd3d_skinning_pose *pose)
{
    unsigned long i;
    if (pose == 0) return;
    memset(pose, 0, sizeof(*pose));
    for (i = 0UL; i < (unsigned long)USD3D_SKINNING_FINAL_MAX_JOINTS; ++i) {
        usd3d_skinning_matrix_identity(&pose->joint_local[i * 16UL]);
        usd3d_skinning_matrix_identity(&pose->joint_world[i * 16UL]);
        usd3d_skinning_matrix_identity(&pose->inverse_bind[i * 16UL]);
        pose->parent_index[i] = -1L;
    }
}

int usd3d_skinning_matrix_mul(const usd3d_fixed *a16, const usd3d_fixed *b16, usd3d_fixed *out16)
{
    unsigned int row;
    unsigned int col;
    unsigned int k;
    usd3d_fixed tmp[16];
    usd3d_fixed sum;
    if (a16 == 0 || b16 == 0 || out16 == 0) return USD3D_ERR_BAD_ARG;
    for (col = 0U; col < 4U; ++col) {
        for (row = 0U; row < 4U; ++row) {
            sum = 0L;
            for (k = 0U; k < 4U; ++k) {
                sum += usd3d_fixed_mul(a16[k * 4U + row], b16[col * 4U + k]);
            }
            tmp[col * 4U + row] = sum;
        }
    }
    for (k = 0U; k < 16U; ++k) out16[k] = tmp[k];
    return USD3D_OK;
}

int usd3d_skinning_pose_compute_world(usd3d_skinning_pose *pose)
{
    unsigned long i;
    long p;
    if (pose == 0) return USD3D_ERR_BAD_ARG;
    if (pose->joint_count > (unsigned long)USD3D_SKINNING_FINAL_MAX_JOINTS) return USD3D_ERR_LIMIT;
    for (i = 0UL; i < pose->joint_count; ++i) {
        p = pose->parent_index[i];
        if (p >= 0L && (unsigned long)p < pose->joint_count) {
            usd3d_skinning_matrix_mul(&pose->joint_world[((unsigned long)p) * 16UL], &pose->joint_local[i * 16UL], &pose->joint_world[i * 16UL]);
        } else {
            memcpy(&pose->joint_world[i * 16UL], &pose->joint_local[i * 16UL], sizeof(usd3d_fixed) * 16U);
        }
    }
    return USD3D_OK;
}

int usd3d_skinning_pose_compute_skin_matrices(const usd3d_skinning_pose *pose, usd3d_fixed *out_skin_matrices, unsigned long out_joint_count)
{
    unsigned long i;
    if (pose == 0 || out_skin_matrices == 0) return USD3D_ERR_BAD_ARG;
    if (out_joint_count < pose->joint_count) return USD3D_ERR_LIMIT;
    for (i = 0UL; i < pose->joint_count; ++i) {
        usd3d_skinning_matrix_mul(&pose->joint_world[i * 16UL], &pose->inverse_bind[i * 16UL], &out_skin_matrices[i * 16UL]);
    }
    return USD3D_OK;
}

int usd3d_skinning_apply_blendshape(usd3d_vec3 *point, const usd3d_vec3 *offset, usd3d_fixed weight)
{
    if (point == 0 || offset == 0) return USD3D_ERR_BAD_ARG;
    point->x += usd3d_fixed_mul(offset->x, weight);
    point->y += usd3d_fixed_mul(offset->y, weight);
    point->z += usd3d_fixed_mul(offset->z, weight);
    return USD3D_OK;
}

int usd3d_skinning_vertex_final(const usd3d_vec3 *bind_point,
                                const usd3d_vec3 *blend_offsets,
                                const usd3d_fixed *blend_weights,
                                unsigned long blend_count,
                                const usd3d_u32 *joint_indices,
                                const usd3d_fixed *joint_weights,
                                unsigned long influence_count,
                                const usd3d_fixed *skin_matrices,
                                unsigned long joint_count,
                                usd3d_vec3 *out_point)
{
    unsigned long i;
    usd3d_vec3 p;
    if (bind_point == 0 || joint_indices == 0 || joint_weights == 0 || skin_matrices == 0 || out_point == 0) return USD3D_ERR_BAD_ARG;
    p = *bind_point;
    if (blend_offsets != 0 && blend_weights != 0) {
        for (i = 0UL; i < blend_count; ++i) usd3d_skinning_apply_blendshape(&p, &blend_offsets[i], blend_weights[i]);
    }
    return usd3d_skinning_cpu_vertex(&p, joint_indices, joint_weights, influence_count, skin_matrices, joint_count, out_point);
}
