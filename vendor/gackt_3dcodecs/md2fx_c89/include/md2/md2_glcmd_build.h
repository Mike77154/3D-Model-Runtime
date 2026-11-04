#ifndef MD2_GLCMD_BUILD_H
#define MD2_GLCMD_BUILD_H

#include "md2_glcmd.h"

/*
 * Quake II style GL-command generation workspace.
 *
 * The original tool code used process-global scratch arrays while it greedily
 * searched triangle fans and strips.  md2fx keeps that static-storage shape but
 * makes the storage explicit: put one of these in static storage,
 * caller-owned memory, or an outer arena and pass it to the builder.
 */

#define MD2_GLCMD_BUILD_MODE_QUAKE2_ORDER 0
#define MD2_GLCMD_BUILD_MODE_GLOBAL_BEST  1
#define MD2_GLCMD_BUILD_MODE_AUTO         2

typedef struct md2_glcmd_build_arena_s
{
    md2_i32 used[MD2_MAX_TRIANGLES];

    md2_u16 cur_xyz[MD2_MAX_TRIANGLES + 2];
    md2_u16 cur_st[MD2_MAX_TRIANGLES + 2];
    md2_i32 cur_tris[MD2_MAX_TRIANGLES];

    md2_u16 best_xyz[MD2_MAX_TRIANGLES + 2];
    md2_u16 best_st[MD2_MAX_TRIANGLES + 2];
    md2_i32 best_tris[MD2_MAX_TRIANGLES];

    md2_i32 saved_glcmds[MD2_MAX_GLCMDS];

    md2_i32 cur_len;
    md2_i32 best_len;
    md2_i32 best_type;
    md2_i32 best_start_tri;
    md2_i32 best_start_v;
    md2_i32 saved_num_glcmds;
} md2_glcmd_build_arena_t;

typedef struct md2_glcmd_build_stats_s
{
    md2_i32 build_mode;
    md2_i32 num_primitives;
    md2_i32 num_strips;
    md2_i32 num_fans;
    md2_i32 num_single_triangle_primitives;
    md2_i32 num_gl_vertices;
    md2_i32 num_glcmds;
    md2_i32 emitted_triangle_count;
    md2_i32 max_primitive_vertices;
    md2_i32 max_primitive_triangles;
    md2_i32 chosen_start_tri_count;
} md2_glcmd_build_stats_t;

void md2_glcmd_build_arena_init(md2_glcmd_build_arena_t *arena);
void md2_glcmd_build_stats_init(md2_glcmd_build_stats_t *stats);

int md2_glcmd_build_from_triangles_ex(md2_model_t *model,
                                      md2_glcmd_build_arena_t *arena,
                                      md2_i32 build_mode,
                                      md2_glcmd_build_stats_t *out_stats);

int md2_glcmd_build_from_triangles(md2_model_t *model,
                                   md2_glcmd_build_arena_t *arena,
                                   md2_glcmd_build_stats_t *out_stats);

#endif /* MD2_GLCMD_BUILD_H */
