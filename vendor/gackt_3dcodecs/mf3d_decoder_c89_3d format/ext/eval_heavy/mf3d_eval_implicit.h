#ifndef MF3D_EVAL_IMPLICIT_H
#define MF3D_EVAL_IMPLICIT_H

#include "mf3d_eval_mesh.h"

#ifndef MF3D_IMPL_MAX_NODES
#define MF3D_IMPL_MAX_NODES 128
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MF3D_IMPL_SPHERE 1U
#define MF3D_IMPL_BOX 2U
#define MF3D_IMPL_PLANE 3U
#define MF3D_IMPL_UNION 4U
#define MF3D_IMPL_INTERSECTION 5U
#define MF3D_IMPL_DIFFERENCE 6U
#define MF3D_IMPL_NEGATE 7U

typedef struct mf3d_impl_node_s {
    mf3d_u32 kind;
    mf3d_i32 a;
    mf3d_i32 b;
    mf3d_vec3 p;
    mf3d_vec3 q;
    mf3d_fix r;
} mf3d_impl_node;

typedef struct mf3d_impl_program_s {
    mf3d_impl_node nodes[MF3D_IMPL_MAX_NODES];
    mf3d_u32 node_count;
    mf3d_i32 root;
} mf3d_impl_program;

typedef struct mf3d_impl_polygonize_options_s {
    mf3d_eval_aabb bounds;
    mf3d_u32 cells_x;
    mf3d_u32 cells_y;
    mf3d_u32 cells_z;
} mf3d_impl_polygonize_options;

void mf3d_impl_init(mf3d_impl_program *p);
int mf3d_impl_add_sphere(mf3d_impl_program *p, mf3d_fix cx, mf3d_fix cy, mf3d_fix cz, mf3d_fix radius, mf3d_i32 *idx);
int mf3d_impl_add_box(mf3d_impl_program *p, mf3d_fix cx, mf3d_fix cy, mf3d_fix cz, mf3d_fix sx, mf3d_fix sy, mf3d_fix sz, mf3d_i32 *idx);
int mf3d_impl_add_plane(mf3d_impl_program *p, mf3d_fix nx, mf3d_fix ny, mf3d_fix nz, mf3d_fix d, mf3d_i32 *idx);
int mf3d_impl_add_op(mf3d_impl_program *p, mf3d_u32 kind, mf3d_i32 a, mf3d_i32 b, mf3d_i32 *idx);
mf3d_fix mf3d_impl_eval(const mf3d_impl_program *p, mf3d_i32 node, const mf3d_vec3 *pt);
int mf3d_impl_polygonize_tets(const mf3d_impl_program *p, mf3d_i32 root, const mf3d_impl_polygonize_options *opt, mf3d_eval_mesh *out_mesh);

#ifdef __cplusplus
}
#endif
#endif
