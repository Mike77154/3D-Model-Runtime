#include "usd3d_skinning_runtime.h"

#include <string.h>

void usd3d_skinning_result_init(usd3d_skinning_result *r)
{
    if (r == 0) return;
    memset(r, 0, sizeof(*r));
}

int usd3d_skinning_runtime_normalize_weights(usd3d_fixed *weights, unsigned long count, usd3d_fixed *out_sum)
{
    unsigned long i;
    usd3d_fixed sum;
    if (weights == 0) return USD3D_ERR_BAD_ARG;
    sum = 0L;
    for (i = 0UL; i < count; ++i) sum += weights[i];
    if (out_sum != 0) *out_sum = sum;
    if (sum == 0L) return USD3D_ERR_PARSE;
    if (sum == USD3D_FIXED_ONE) return USD3D_OK;
    for (i = 0UL; i < count; ++i) weights[i] = usd3d_fixed_div(weights[i], sum);
    return USD3D_OK;
}

int usd3d_skinning_transform_direction(const usd3d_vec3 *dir, const usd3d_fixed *m16, usd3d_vec3 *out_dir)
{
    if (dir == 0 || m16 == 0 || out_dir == 0) return USD3D_ERR_BAD_ARG;
    out_dir->x = usd3d_fixed_mul(dir->x, m16[0]) + usd3d_fixed_mul(dir->y, m16[4]) + usd3d_fixed_mul(dir->z, m16[8]);
    out_dir->y = usd3d_fixed_mul(dir->x, m16[1]) + usd3d_fixed_mul(dir->y, m16[5]) + usd3d_fixed_mul(dir->z, m16[9]);
    out_dir->z = usd3d_fixed_mul(dir->x, m16[2]) + usd3d_fixed_mul(dir->y, m16[6]) + usd3d_fixed_mul(dir->z, m16[10]);
    return USD3D_OK;
}

static void sr_add_weighted_vec(usd3d_vec3 *acc, const usd3d_vec3 *v, usd3d_fixed w)
{
    acc->x += usd3d_fixed_mul(v->x, w);
    acc->y += usd3d_fixed_mul(v->y, w);
    acc->z += usd3d_fixed_mul(v->z, w);
}

int usd3d_skinning_vertex_runtime(const usd3d_skinning_vertex_io *in_vertex,
                                  const usd3d_fixed *skin_matrices,
                                  unsigned long joint_count,
                                  const usd3d_fixed *geom_bind_xform,
                                  const usd3d_vec3 *blend_offsets,
                                  const usd3d_fixed *blend_weights,
                                  unsigned long blend_count,
                                  usd3d_skinning_result *out_result)
{
    usd3d_vec3 bind_point;
    usd3d_vec3 bind_normal;
    usd3d_vec3 bind_tangent;
    usd3d_vec3 tmp;
    usd3d_fixed weights[USD3D_SKINNING_RUNTIME_MAX_INFLUENCES];
    unsigned long i;
    unsigned long count;
    unsigned long j;
    int r;
    if (in_vertex == 0 || skin_matrices == 0 || out_result == 0) return USD3D_ERR_BAD_ARG;
    usd3d_skinning_result_init(out_result);
    bind_point = in_vertex->point;
    bind_normal = in_vertex->normal;
    bind_tangent = in_vertex->tangent;
    if (geom_bind_xform != 0) {
        usd3d_skinning_transform_point(geom_bind_xform, &bind_point, &tmp);
        bind_point = tmp;
        usd3d_skinning_transform_direction(&bind_normal, geom_bind_xform, &tmp);
        bind_normal = tmp;
        usd3d_skinning_transform_direction(&bind_tangent, geom_bind_xform, &tmp);
        bind_tangent = tmp;
    }
    if (blend_offsets != 0 && blend_weights != 0) {
        for (i = 0UL; i < blend_count; ++i) usd3d_skinning_apply_blendshape(&bind_point, &blend_offsets[i], blend_weights[i]);
    }
    count = in_vertex->influence_count;
    if (count > (unsigned long)USD3D_SKINNING_RUNTIME_MAX_INFLUENCES) {
        count = (unsigned long)USD3D_SKINNING_RUNTIME_MAX_INFLUENCES;
        out_result->clipped_influences = 1;
    }
    for (i = 0UL; i < count; ++i) weights[i] = in_vertex->joint_weights[i];
    r = usd3d_skinning_runtime_normalize_weights(weights, count, &out_result->weight_sum);
    if (r == USD3D_OK) out_result->normalized = 1;
    if (count == 0UL) return USD3D_ERR_PARSE;
    for (i = 0UL; i < count; ++i) {
        j = in_vertex->joint_indices[i];
        if (j >= joint_count) continue;
        usd3d_skinning_transform_point(&skin_matrices[j * 16UL], &bind_point, &tmp);
        sr_add_weighted_vec(&out_result->point, &tmp, weights[i]);
        usd3d_skinning_transform_direction(&bind_normal, &skin_matrices[j * 16UL], &tmp);
        sr_add_weighted_vec(&out_result->normal, &tmp, weights[i]);
        usd3d_skinning_transform_direction(&bind_tangent, &skin_matrices[j * 16UL], &tmp);
        sr_add_weighted_vec(&out_result->tangent, &tmp, weights[i]);
    }
    return USD3D_OK;
}

int usd3d_skinning_run_mesh(const usd3d_skinning_mesh_job *job)
{
    unsigned long i;
    if (job == 0 || job->vertices == 0 || job->skin_matrices == 0 || job->out_vertices == 0) return USD3D_ERR_BAD_ARG;
    if (job->vertex_count > job->out_capacity) return USD3D_ERR_LIMIT;
    for (i = 0UL; i < job->vertex_count; ++i) {
        if (usd3d_skinning_vertex_runtime(&job->vertices[i], job->skin_matrices, job->joint_count, job->geom_bind_xform, job->blend_offsets, job->blend_weights, job->blend_count, &job->out_vertices[i]) != USD3D_OK) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}
