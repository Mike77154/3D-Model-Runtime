#ifndef USD3D_SKINNING_RUNTIME_H
#define USD3D_SKINNING_RUNTIME_H

#include "usd3d.h"
#include "usd3d_skinning_final.h"

#ifndef USD3D_SKINNING_RUNTIME_MAX_POINTS
#define USD3D_SKINNING_RUNTIME_MAX_POINTS 8192
#endif

#ifndef USD3D_SKINNING_RUNTIME_MAX_INFLUENCES
#define USD3D_SKINNING_RUNTIME_MAX_INFLUENCES 8
#endif

typedef struct usd3d_skinning_vertex_io_s {
    usd3d_vec3 point;
    usd3d_vec3 normal;
    usd3d_vec3 tangent;
    usd3d_u32 joint_indices[USD3D_SKINNING_RUNTIME_MAX_INFLUENCES];
    usd3d_fixed joint_weights[USD3D_SKINNING_RUNTIME_MAX_INFLUENCES];
    unsigned long influence_count;
} usd3d_skinning_vertex_io;

typedef struct usd3d_skinning_result_s {
    usd3d_vec3 point;
    usd3d_vec3 normal;
    usd3d_vec3 tangent;
    usd3d_fixed weight_sum;
    int normalized;
    int clipped_influences;
} usd3d_skinning_result;

typedef struct usd3d_skinning_mesh_job_s {
    const usd3d_skinning_vertex_io *vertices;
    unsigned long vertex_count;
    const usd3d_fixed *skin_matrices;
    unsigned long joint_count;
    const usd3d_fixed *geom_bind_xform;
    const usd3d_vec3 *blend_offsets;
    const usd3d_fixed *blend_weights;
    unsigned long blend_count;
    usd3d_skinning_result *out_vertices;
    unsigned long out_capacity;
} usd3d_skinning_mesh_job;

void usd3d_skinning_result_init(usd3d_skinning_result *r);
int usd3d_skinning_runtime_normalize_weights(usd3d_fixed *weights, unsigned long count, usd3d_fixed *out_sum);
int usd3d_skinning_transform_direction(const usd3d_vec3 *dir, const usd3d_fixed *m16, usd3d_vec3 *out_dir);
int usd3d_skinning_vertex_runtime(const usd3d_skinning_vertex_io *in_vertex,
                                  const usd3d_fixed *skin_matrices,
                                  unsigned long joint_count,
                                  const usd3d_fixed *geom_bind_xform,
                                  const usd3d_vec3 *blend_offsets,
                                  const usd3d_fixed *blend_weights,
                                  unsigned long blend_count,
                                  usd3d_skinning_result *out_result);
int usd3d_skinning_run_mesh(const usd3d_skinning_mesh_job *job);

#endif
