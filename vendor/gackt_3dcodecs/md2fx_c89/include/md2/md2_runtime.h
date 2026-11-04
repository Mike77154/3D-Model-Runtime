#ifndef MD2_RUNTIME_H
#define MD2_RUNTIME_H

#include "md2_model.h"

#ifndef MD2_RUNTIME_MAX_VERTICES
#define MD2_RUNTIME_MAX_VERTICES (MD2_MAX_TRIANGLES * 3)
#endif

#ifndef MD2_RUNTIME_MAX_INDICES
#define MD2_RUNTIME_MAX_INDICES (MD2_MAX_TRIANGLES * 3)
#endif

/* Source kind for the expanded runtime mesh. */
#define MD2_RUNTIME_SOURCE_TRIANGLES     1
#define MD2_RUNTIME_SOURCE_GLCMDS        2
#define MD2_RUNTIME_SOURCE_GLCMDS_STREAM 3

#define MD2_RUNTIME_TEXCOORD_INDEX_NONE ((md2_u16)0xFFFFU)

typedef struct md2_runtime_source_vertex_s
{
    md2_u16 index_xyz;
    md2_u16 index_st;
    md2_u8 source_kind;
    md2_u8 reserved0;
    md2_fx32_t u;
    md2_fx32_t v;
} md2_runtime_source_vertex_t;

typedef struct md2_runtime_mesh_s
{
    md2_i32 source_kind;
    md2_i32 num_vertices;
    md2_i32 num_indices;
    md2_runtime_source_vertex_t vertices[MD2_RUNTIME_MAX_VERTICES];
    md2_u16 indices[MD2_RUNTIME_MAX_INDICES];
} md2_runtime_mesh_t;

typedef struct md2_runtime_vertex_s
{
    md2_fx32_t x;
    md2_fx32_t y;
    md2_fx32_t z;
    md2_fx32_t u;
    md2_fx32_t v;
    md2_u8 lightnormalindex;
    md2_u8 reserved0;
    md2_u8 reserved1;
    md2_u8 reserved2;
} md2_runtime_vertex_t;

void md2_runtime_mesh_init(md2_runtime_mesh_t *mesh);

int md2_runtime_build_triangle_mesh(const md2_model_t *model,
                                    md2_runtime_mesh_t *out_mesh);

int md2_runtime_build_glcmd_mesh(const md2_model_t *model,
                                 md2_runtime_mesh_t *out_mesh);

int md2_runtime_build_glcmd_stream_mesh(const md2_model_t *model,
                                        md2_runtime_mesh_t *out_mesh);

int md2_runtime_pose_frame(const md2_model_t *model,
                           const md2_runtime_mesh_t *mesh,
                           md2_i32 frame_index,
                           md2_runtime_vertex_t *out_vertices);

int md2_runtime_pose_lerp(const md2_model_t *model,
                          const md2_runtime_mesh_t *mesh,
                          md2_i32 frame_a_index,
                          md2_i32 frame_b_index,
                          md2_fx32_t t,
                          md2_runtime_vertex_t *out_vertices);

int md2_runtime_pose_lerp_q2(const md2_model_t *model,
                             const md2_runtime_mesh_t *mesh,
                             md2_i32 old_frame_index,
                             md2_i32 current_frame_index,
                             md2_fx32_t backlerp,
                             const md2_fx_vec3_t *old_origin_minus_origin_model,
                             md2_runtime_vertex_t *out_vertices);

int md2_runtime_pose_lerp_q2_ex(const md2_model_t *model,
                                const md2_runtime_mesh_t *mesh,
                                md2_i32 old_frame_index,
                                md2_i32 current_frame_index,
                                md2_fx32_t backlerp,
                                const md2_fx_vec3_t *old_origin_minus_origin_model,
                                const md2_fx_vec3_t *normal_table,
                                md2_i32 normal_table_count,
                                md2_fx32_t shell_scale,
                                md2_runtime_vertex_t *out_vertices);

#endif /* MD2_RUNTIME_H */
