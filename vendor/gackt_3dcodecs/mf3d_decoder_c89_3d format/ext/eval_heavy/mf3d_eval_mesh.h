#ifndef MF3D_EVAL_MESH_H
#define MF3D_EVAL_MESH_H

#include "../../src/mf3d.h"

#ifndef MF3D_EVAL_MAX_VERTICES
#define MF3D_EVAL_MAX_VERTICES 32768
#endif
#ifndef MF3D_EVAL_MAX_TRIANGLES
#define MF3D_EVAL_MAX_TRIANGLES 65536
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_eval_mesh_s {
    mf3d_vec3 vertices[MF3D_EVAL_MAX_VERTICES];
    mf3d_tri triangles[MF3D_EVAL_MAX_TRIANGLES];
    mf3d_u32 vertex_count;
    mf3d_u32 triangle_count;
} mf3d_eval_mesh;

typedef struct mf3d_eval_aabb_s {
    mf3d_vec3 minv;
    mf3d_vec3 maxv;
    int valid;
} mf3d_eval_aabb;

void mf3d_eval_mesh_init(mf3d_eval_mesh *m);
int mf3d_eval_mesh_add_vertex(mf3d_eval_mesh *m, mf3d_fix x, mf3d_fix y, mf3d_fix z, mf3d_u32 *out_index);
int mf3d_eval_mesh_add_triangle(mf3d_eval_mesh *m, mf3d_u32 a, mf3d_u32 b, mf3d_u32 c);
void mf3d_eval_aabb_init(mf3d_eval_aabb *b);
void mf3d_eval_aabb_add(mf3d_eval_aabb *b, const mf3d_vec3 *p);
void mf3d_eval_model_aabb(const mf3d_model *m, mf3d_eval_aabb *b);
mf3d_fix mf3d_eval_abs(mf3d_fix v);
mf3d_fix mf3d_eval_min(mf3d_fix a, mf3d_fix b);
mf3d_fix mf3d_eval_max(mf3d_fix a, mf3d_fix b);
mf3d_fix mf3d_eval_clamp(mf3d_fix v, mf3d_fix lo, mf3d_fix hi);
mf3d_fix mf3d_eval_mul(mf3d_fix a, mf3d_fix b);
mf3d_fix mf3d_eval_div(mf3d_fix a, mf3d_fix b);
mf3d_fix mf3d_eval_lerp(mf3d_fix a, mf3d_fix b, mf3d_fix t);
mf3d_fix mf3d_eval_sqrt(mf3d_fix v);
void mf3d_eval_vec_sub(const mf3d_vec3 *a, const mf3d_vec3 *b, mf3d_vec3 *r);
void mf3d_eval_vec_cross(const mf3d_vec3 *a, const mf3d_vec3 *b, mf3d_vec3 *r);
void mf3d_eval_vec_normalize(mf3d_vec3 *v);
void mf3d_eval_transform_point(const mf3d_fix t[12], const mf3d_vec3 *p, mf3d_vec3 *outp);
void mf3d_eval_identity(mf3d_fix t[12]);

#ifdef __cplusplus
}
#endif
#endif
