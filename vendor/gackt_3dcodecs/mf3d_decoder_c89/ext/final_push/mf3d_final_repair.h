#ifndef MF3D_FINAL_REPAIR_H
#define MF3D_FINAL_REPAIR_H

#include "../../src/mf3d.h"
#include "../eval_heavy/mf3d_eval_mesh.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_repair_stats_s {
    mf3d_u32 input_vertices;
    mf3d_u32 input_triangles;
    mf3d_u32 output_vertices;
    mf3d_u32 output_triangles;
    mf3d_u32 degenerate_triangles;
    mf3d_u32 duplicate_vertices;
    mf3d_u32 boundary_edges;
    mf3d_u32 nonmanifold_edges;
    mf3d_u32 reversed_outward_triangles;
    mf3d_eval_aabb bounds;
} mf3d_repair_stats;

void mf3d_repair_stats_init(mf3d_repair_stats *s);
int mf3d_eval_mesh_from_model(const mf3d_model *src, mf3d_eval_mesh *out_mesh);
int mf3d_eval_mesh_weld_clean(const mf3d_eval_mesh *src, mf3d_eval_mesh *out_mesh, mf3d_fix tolerance, mf3d_repair_stats *stats);
int mf3d_eval_mesh_orient_outward_aabb(mf3d_eval_mesh *mesh, mf3d_repair_stats *stats);
void mf3d_eval_mesh_edge_report(const mf3d_eval_mesh *mesh, mf3d_repair_stats *stats);
int mf3d_eval_mesh_vertex_normals(const mf3d_eval_mesh *mesh, mf3d_vec3 *normals, mf3d_u32 normal_cap);

#ifdef __cplusplus
}
#endif
#endif
