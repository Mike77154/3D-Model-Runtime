#ifndef MD2_HEURISTIC_H
#define MD2_HEURISTIC_H

#include "md2_glcmd_build.h"
#include "md2_runtime.h"

/*
 * Quake-II-family heuristic helpers for deciding whether the MD2 should be
 * consumed from triangles[] or from the strip/fan glcmd stream.
 *
 * All scratch memory is caller-owned.  No heap allocation is performed.
 */

#define MD2_Q2_MAX_LBM_HEIGHT 480

#define MD2_HEURISTIC_FLAG_BASE_VALID                0x00000001U
#define MD2_HEURISTIC_FLAG_Q2_STRICT_VALID           0x00000002U
#define MD2_HEURISTIC_FLAG_HAS_UV_SEAMS              0x00000004U
#define MD2_HEURISTIC_FLAG_HAS_DEGENERATES           0x00000008U
#define MD2_HEURISTIC_FLAG_GLCMDS_VALID              0x00000010U
#define MD2_HEURISTIC_FLAG_GLCMDS_TRI_MATCH          0x00000020U
#define MD2_HEURISTIC_FLAG_PREFERS_GLCMDS            0x00000040U
#define MD2_HEURISTIC_FLAG_PREFERS_TRIANGLES         0x00000080U
#define MD2_HEURISTIC_FLAG_HAS_UNUSED_XYZ            0x00000100U
#define MD2_HEURISTIC_FLAG_HAS_UNUSED_ST             0x00000200U
#define MD2_HEURISTIC_FLAG_HAS_BOUNDARY_EDGES        0x00000400U
#define MD2_HEURISTIC_FLAG_HAS_NONMANIFOLD_EDGES     0x00000800U
#define MD2_HEURISTIC_FLAG_GLCMDS_TOPOLOGY_MATCH     0x00001000U
#define MD2_HEURISTIC_FLAG_GLCMDS_UNORDERED_MATCH    0x00002000U
#define MD2_HEURISTIC_FLAG_GLCMDS_HAS_DEGENERATES    0x00004000U
#define MD2_HEURISTIC_FLAG_GLCMDS_HAS_UNRESOLVED_ST  0x00008000U
#define MD2_HEURISTIC_FLAG_REBUILT_GLCMDS            0x00010000U
#define MD2_HEURISTIC_FLAG_BUILD_ROUNDTRIP_OK        0x00020000U
#define MD2_HEURISTIC_FLAG_GLCMDS_LOSSLESS           0x00040000U
#define MD2_HEURISTIC_FLAG_GLCMDS_EXACT_MATCH        0x00080000U
#define MD2_HEURISTIC_FLAG_GLCMDS_MISSING_TRIS       0x00100000U
#define MD2_HEURISTIC_FLAG_GLCMDS_EXCESS_TRIS        0x00200000U

#define MD2_HEURISTIC_REASON_NONE                    0
#define MD2_HEURISTIC_REASON_GLCMDS_LOSSLESS         1
#define MD2_HEURISTIC_REASON_GLCMDS_INVALID          2
#define MD2_HEURISTIC_REASON_GLCMDS_TOPOLOGY_MISMATCH 3
#define MD2_HEURISTIC_REASON_GLCMDS_UNRESOLVED_ST    4
#define MD2_HEURISTIC_REASON_GLCMDS_DEGENERATE       5
#define MD2_HEURISTIC_REASON_GLCMDS_COUNT_MISMATCH   6

#define MD2_HEURISTIC_SOURCE_NONE          0
#define MD2_HEURISTIC_SOURCE_TRIANGLES     MD2_RUNTIME_SOURCE_TRIANGLES
#define MD2_HEURISTIC_SOURCE_GLCMDS        MD2_RUNTIME_SOURCE_GLCMDS
#define MD2_HEURISTIC_SOURCE_GLCMDS_STREAM MD2_RUNTIME_SOURCE_GLCMDS_STREAM

#define MD2_HEURISTIC_ST_NONE ((md2_u16)0xFFFFU)

/* Explicit scratch arena used by topology checks and command decoding. */
typedef struct md2_heuristic_arena_s
{
    md2_i32 exact_matched[MD2_MAX_TRIANGLES];
    md2_i32 unordered_matched[MD2_MAX_TRIANGLES];
    md2_u16 cmd_xyz[MD2_MAX_TRIANGLES + 2];
    md2_u16 cmd_st[MD2_MAX_TRIANGLES + 2];
} md2_heuristic_arena_t;

typedef struct md2_heuristic_report_s
{
    md2_u32 flags;
    md2_i32 chosen_source;
    int base_validation_rc;
    int q2_validation_rc;
    int glcmd_validation_rc;

    md2_i32 triangle_degenerate_xyz_count;
    md2_i32 triangle_degenerate_st_count;
    md2_i32 triangle_unused_xyz_count;
    md2_i32 triangle_unused_st_count;
    md2_i32 uv_seam_xyz_count;
    md2_i32 edge_boundary_count;
    md2_i32 edge_nonmanifold_count;

    md2_i32 glcmd_strip_count;
    md2_i32 glcmd_fan_count;
    md2_i32 glcmd_vertex_count;
    md2_i32 glcmd_triangle_count;
    md2_i32 glcmd_degenerate_count;
    md2_i32 glcmd_unresolved_st_count;
    md2_i32 glcmd_topology_match_count;
    md2_i32 glcmd_topology_unordered_match_count;
    md2_i32 glcmd_topology_miss_count;
    md2_fx32_t glcmd_triangle_coverage_q16;
    md2_fx32_t glcmd_exact_coverage_q16;
    md2_fx32_t glcmd_vertex_per_tri_q16;
    md2_i32 glcmd_quality_score;
    md2_i32 decision_reason;

    md2_i32 rebuilt_build_mode;
    md2_i32 rebuilt_num_primitives;
    md2_i32 rebuilt_num_strips;
    md2_i32 rebuilt_num_fans;
    md2_i32 rebuilt_num_single_triangle_primitives;
    md2_i32 rebuilt_num_gl_vertices;
    md2_i32 rebuilt_num_glcmds;
    md2_i32 rebuilt_emitted_triangle_count;
    md2_i32 rebuilt_max_primitive_vertices;
    md2_i32 rebuilt_max_primitive_triangles;
    md2_fx32_t rebuilt_vertex_per_tri_q16;
    md2_i32 rebuilt_quality_score;
} md2_heuristic_report_t;

void md2_heuristic_arena_init(md2_heuristic_arena_t *arena);
void md2_heuristic_report_init(md2_heuristic_report_t *report);

int md2_validate_model_quake2(const md2_model_t *model);

int md2_heuristic_analyze_model(const md2_model_t *model,
                                md2_heuristic_arena_t *arena,
                                md2_heuristic_report_t *out_report);

int md2_heuristic_choose_runtime_source(const md2_model_t *model,
                                        md2_heuristic_report_t *inout_report);

int md2_heuristic_glcmds_are_lossless(const md2_heuristic_report_t *report);

int md2_heuristic_count_edges(const md2_model_t *model,
                              md2_i32 *out_boundary_edges,
                              md2_i32 *out_nonmanifold_edges);

int md2_heuristic_build_best_mesh(const md2_model_t *model,
                                  md2_heuristic_arena_t *arena,
                                  md2_runtime_mesh_t *out_mesh,
                                  md2_heuristic_report_t *out_report);

int md2_heuristic_rebuild_glcmds(md2_model_t *model,
                                 md2_glcmd_build_arena_t *build_arena,
                                 md2_heuristic_arena_t *heuristic_arena,
                                 md2_glcmd_build_stats_t *out_build_stats,
                                 md2_heuristic_report_t *out_report);

int md2_heuristic_sanitize_frame_pair(const md2_model_t *model,
                                      md2_i32 *frame_index,
                                      md2_i32 *old_frame_index);

#endif /* MD2_HEURISTIC_H */
