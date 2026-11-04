#include "usd3d_skinning_cpu.h"

int usd3d_skinning_transform_point(const usd3d_fixed *m16, const usd3d_vec3 *in_point, usd3d_vec3 *out_point)
{
    usd3d_fixed x;
    usd3d_fixed y;
    usd3d_fixed z;
    if (m16 == 0 || in_point == 0 || out_point == 0) return USD3D_ERR_BAD_ARG;
    x = in_point->x;
    y = in_point->y;
    z = in_point->z;
    out_point->x = usd3d_fixed_mul(m16[0], x) + usd3d_fixed_mul(m16[4], y) + usd3d_fixed_mul(m16[8], z) + m16[12];
    out_point->y = usd3d_fixed_mul(m16[1], x) + usd3d_fixed_mul(m16[5], y) + usd3d_fixed_mul(m16[9], z) + m16[13];
    out_point->z = usd3d_fixed_mul(m16[2], x) + usd3d_fixed_mul(m16[6], y) + usd3d_fixed_mul(m16[10], z) + m16[14];
    return USD3D_OK;
}

int usd3d_skinning_normalize_weights(usd3d_fixed *weights, unsigned long count)
{
    unsigned long i;
    usd3d_fixed sum;
    if (weights == 0) return USD3D_ERR_BAD_ARG;
    sum = 0L;
    for (i = 0UL; i < count; ++i) sum += weights[i];
    if (sum == 0L) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < count; ++i) weights[i] = usd3d_fixed_div(weights[i], sum);
    return USD3D_OK;
}

int usd3d_skinning_cpu_vertex(const usd3d_vec3 *bind_point,
                              const usd3d_u32 *joint_indices,
                              const usd3d_fixed *joint_weights,
                              unsigned long influence_count,
                              const usd3d_fixed *joint_matrices,
                              unsigned long joint_count,
                              usd3d_vec3 *out_point)
{
    unsigned long i;
    usd3d_vec3 tp;
    usd3d_u32 ji;
    usd3d_fixed w;
    if (bind_point == 0 || joint_indices == 0 || joint_weights == 0 || joint_matrices == 0 || out_point == 0) return USD3D_ERR_BAD_ARG;
    out_point->x = 0L;
    out_point->y = 0L;
    out_point->z = 0L;
    if (influence_count > (unsigned long)USD3D_SKINNING_MAX_INFLUENCES) influence_count = (unsigned long)USD3D_SKINNING_MAX_INFLUENCES;
    for (i = 0UL; i < influence_count; ++i) {
        ji = joint_indices[i];
        if ((unsigned long)ji >= joint_count) continue;
        w = joint_weights[i];
        if (w == 0L) continue;
        if (usd3d_skinning_transform_point(&joint_matrices[((unsigned long)ji) * 16UL], bind_point, &tp) != USD3D_OK) return USD3D_ERR_PARSE;
        out_point->x += usd3d_fixed_mul(tp.x, w);
        out_point->y += usd3d_fixed_mul(tp.y, w);
        out_point->z += usd3d_fixed_mul(tp.z, w);
    }
    return USD3D_OK;
}
