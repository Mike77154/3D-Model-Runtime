#include <string.h>

#include "md2/md2_error.h"
#include "md2/md2_fixed.h"
#include "md2/md2_glcmd.h"
#include "md2/md2_heuristic.h"
#include "md2/md2_validate.h"

#define MD2_HEURISTIC_UV_EPS ((md2_fx32_t)2)

void md2_heuristic_arena_init(md2_heuristic_arena_t *arena)
{
    if (arena != 0)
    {
        memset(arena, 0, sizeof(*arena));
    }
}

void md2_heuristic_report_init(md2_heuristic_report_t *report)
{
    if (report != 0)
    {
        memset(report, 0, sizeof(*report));
        report->chosen_source = MD2_HEURISTIC_SOURCE_NONE;
        report->base_validation_rc = MD2_OK;
        report->q2_validation_rc = MD2_OK;
        report->glcmd_validation_rc = MD2_OK;
    }
}

static int md2_heuristic_validate_core_model(const md2_model_t *model)
{
    md2_i32 i;
    int rc;

    if (model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    if (model->skinwidth <= 0 || model->skinheight <= 0)
    {
        return MD2_ERR_BAD_LAYOUT;
    }

    if (model->num_skins < 0 || model->num_skins > MD2_MAX_SKINS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }
    if (model->num_xyz <= 0 || model->num_xyz > MD2_MAX_VERTS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }
    if (model->num_st <= 0 || model->num_st > MD2_MAX_TEXCOORDS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }
    if (model->num_tris <= 0 || model->num_tris > MD2_MAX_TRIANGLES)
    {
        return MD2_ERR_OVER_CAPACITY;
    }
    if (model->num_frames <= 0 || model->num_frames > MD2_MAX_FRAMES)
    {
        return MD2_ERR_OVER_CAPACITY;
    }
    if (model->num_glcmds < 0 || model->num_glcmds > MD2_MAX_GLCMDS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    rc = md2_validate_indices(model);
    if (rc != MD2_OK)
    {
        return rc;
    }

    for (i = 0; i < model->num_frames; ++i)
    {
        if (model->frames[i].scale[0] < 0 ||
            model->frames[i].scale[1] < 0 ||
            model->frames[i].scale[2] < 0)
        {
            return MD2_ERR_BAD_LAYOUT;
        }
    }

    return MD2_OK;
}

int md2_validate_model_quake2(const md2_model_t *model)
{
    int rc;

    rc = md2_heuristic_validate_core_model(model);
    if (rc != MD2_OK)
    {
        return rc;
    }

    if (model->skinheight > MD2_Q2_MAX_LBM_HEIGHT)
    {
        return MD2_ERR_BAD_LAYOUT;
    }

    if (model->num_glcmds > 0)
    {
        rc = md2_validate_glcmds(model);
        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    return MD2_OK;
}

static int md2_tri_xyz_degenerate(const md2_triangle_t *tri)
{
    if (tri->index_xyz[0] == tri->index_xyz[1]) return 1;
    if (tri->index_xyz[0] == tri->index_xyz[2]) return 1;
    if (tri->index_xyz[1] == tri->index_xyz[2]) return 1;
    return 0;
}

static int md2_tri_st_degenerate(const md2_triangle_t *tri)
{
    if (tri->index_st[0] == tri->index_st[1]) return 1;
    if (tri->index_st[0] == tri->index_st[2]) return 1;
    if (tri->index_st[1] == tri->index_st[2]) return 1;
    return 0;
}

static int md2_model_references_xyz(const md2_model_t *model, md2_i32 xyz)
{
    md2_i32 i;
    md2_i32 j;

    for (i = 0; i < model->num_tris; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            if ((md2_i32)model->triangles[i].index_xyz[j] == xyz)
            {
                return 1;
            }
        }
    }
    return 0;
}

static int md2_model_references_st(const md2_model_t *model, md2_i32 st)
{
    md2_i32 i;
    md2_i32 j;

    for (i = 0; i < model->num_tris; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            if ((md2_i32)model->triangles[i].index_st[j] == st)
            {
                return 1;
            }
        }
    }
    return 0;
}

static int md2_model_xyz_has_uv_seam(const md2_model_t *model, md2_i32 xyz)
{
    md2_i32 i;
    md2_i32 j;
    md2_i32 first_st;

    first_st = -1;
    for (i = 0; i < model->num_tris; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            if ((md2_i32)model->triangles[i].index_xyz[j] == xyz)
            {
                if (first_st < 0)
                {
                    first_st = (md2_i32)model->triangles[i].index_st[j];
                }
                else if (first_st != (md2_i32)model->triangles[i].index_st[j])
                {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void md2_heuristic_get_edge(const md2_model_t *model,
                                   md2_i32 tri_index,
                                   md2_i32 edge_index,
                                   md2_u16 *out_a,
                                   md2_u16 *out_b)
{
    const md2_triangle_t *tri;
    md2_i32 next;

    tri = &model->triangles[tri_index];
    next = edge_index + 1;
    if (next == 3)
    {
        next = 0;
    }

    *out_a = tri->index_xyz[edge_index];
    *out_b = tri->index_xyz[next];
}

static int md2_heuristic_same_undirected_edge(md2_u16 a0,
                                              md2_u16 b0,
                                              md2_u16 a1,
                                              md2_u16 b1)
{
    if (a0 == a1 && b0 == b1) return 1;
    if (a0 == b1 && b0 == a1) return 1;
    return 0;
}

static int md2_heuristic_is_first_edge_ref(const md2_model_t *model,
                                           md2_i32 tri_index,
                                           md2_i32 edge_index,
                                           md2_u16 a,
                                           md2_u16 b)
{
    md2_i32 i;
    md2_i32 e;
    md2_u16 ea;
    md2_u16 eb;

    for (i = 0; i <= tri_index; ++i)
    {
        for (e = 0; e < 3; ++e)
        {
            if (i == tri_index && e >= edge_index)
            {
                return 1;
            }
            md2_heuristic_get_edge(model, i, e, &ea, &eb);
            if (md2_heuristic_same_undirected_edge(a, b, ea, eb))
            {
                return 0;
            }
        }
    }

    return 1;
}

static md2_i32 md2_heuristic_count_edge_refs(const md2_model_t *model,
                                             md2_u16 a,
                                             md2_u16 b)
{
    md2_i32 i;
    md2_i32 e;
    md2_i32 count;
    md2_u16 ea;
    md2_u16 eb;

    count = 0;
    for (i = 0; i < model->num_tris; ++i)
    {
        for (e = 0; e < 3; ++e)
        {
            md2_heuristic_get_edge(model, i, e, &ea, &eb);
            if (md2_heuristic_same_undirected_edge(a, b, ea, eb))
            {
                ++count;
            }
        }
    }

    return count;
}

int md2_heuristic_count_edges(const md2_model_t *model,
                              md2_i32 *out_boundary_edges,
                              md2_i32 *out_nonmanifold_edges)
{
    md2_i32 i;
    md2_i32 e;
    md2_i32 refs;
    md2_i32 boundary;
    md2_i32 nonmanifold;
    md2_u16 a;
    md2_u16 b;
    int rc;

    if (model == 0 || out_boundary_edges == 0 || out_nonmanifold_edges == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    rc = md2_validate_indices(model);
    if (rc != MD2_OK)
    {
        return rc;
    }

    boundary = 0;
    nonmanifold = 0;

    for (i = 0; i < model->num_tris; ++i)
    {
        for (e = 0; e < 3; ++e)
        {
            md2_heuristic_get_edge(model, i, e, &a, &b);
            if (a == b)
            {
                continue;
            }
            if (!md2_heuristic_is_first_edge_ref(model, i, e, a, b))
            {
                continue;
            }

            refs = md2_heuristic_count_edge_refs(model, a, b);
            if (refs == 1)
            {
                ++boundary;
            }
            else if (refs > 2)
            {
                ++nonmanifold;
            }
        }
    }

    *out_boundary_edges = boundary;
    *out_nonmanifold_edges = nonmanifold;
    return MD2_OK;
}

static int md2_heuristic_analyze_triangles(const md2_model_t *model,
                                           md2_heuristic_report_t *report)
{
    md2_i32 i;
    int rc;

    for (i = 0; i < model->num_tris; ++i)
    {
        if (md2_tri_xyz_degenerate(&model->triangles[i]))
        {
            ++report->triangle_degenerate_xyz_count;
        }
        if (md2_tri_st_degenerate(&model->triangles[i]))
        {
            ++report->triangle_degenerate_st_count;
        }
    }

    for (i = 0; i < model->num_xyz; ++i)
    {
        if (!md2_model_references_xyz(model, i))
        {
            ++report->triangle_unused_xyz_count;
        }
        if (md2_model_xyz_has_uv_seam(model, i))
        {
            ++report->uv_seam_xyz_count;
        }
    }

    for (i = 0; i < model->num_st; ++i)
    {
        if (!md2_model_references_st(model, i))
        {
            ++report->triangle_unused_st_count;
        }
    }

    rc = md2_heuristic_count_edges(model,
                                   &report->edge_boundary_count,
                                   &report->edge_nonmanifold_count);
    if (rc != MD2_OK)
    {
        return rc;
    }

    if (report->triangle_degenerate_xyz_count > 0 ||
        report->triangle_degenerate_st_count > 0)
    {
        report->flags |= MD2_HEURISTIC_FLAG_HAS_DEGENERATES;
    }
    if (report->triangle_unused_xyz_count > 0)
    {
        report->flags |= MD2_HEURISTIC_FLAG_HAS_UNUSED_XYZ;
    }
    if (report->triangle_unused_st_count > 0)
    {
        report->flags |= MD2_HEURISTIC_FLAG_HAS_UNUSED_ST;
    }
    if (report->uv_seam_xyz_count > 0)
    {
        report->flags |= MD2_HEURISTIC_FLAG_HAS_UV_SEAMS;
    }
    if (report->edge_boundary_count > 0)
    {
        report->flags |= MD2_HEURISTIC_FLAG_HAS_BOUNDARY_EDGES;
    }
    if (report->edge_nonmanifold_count > 0)
    {
        report->flags |= MD2_HEURISTIC_FLAG_HAS_NONMANIFOLD_EDGES;
    }

    return MD2_OK;
}

static int md2_heuristic_tri_degenerate_u16(md2_u16 a, md2_u16 b, md2_u16 c)
{
    if (a == b) return 1;
    if (a == c) return 1;
    if (b == c) return 1;
    return 0;
}

static int md2_heuristic_fx_near(md2_fx32_t a, md2_fx32_t b)
{
    md2_fx32_t d;

    d = (a >= b) ? (md2_fx32_t)(a - b) : (md2_fx32_t)(b - a);
    return d <= MD2_HEURISTIC_UV_EPS;
}

static md2_fx32_t md2_heuristic_ratio_q16(md2_i32 numer, md2_i32 denom)
{
    if (numer <= 0 || denom <= 0)
    {
        return 0;
    }

    return md2_fx_div_i32((md2_fx32_t)(numer * (md2_i32)MD2_FX_ONE), denom);
}

static md2_i32 md2_heuristic_abs_i32(md2_i32 value)
{
    return (value < 0) ? (md2_i32)(-value) : value;
}

static md2_i32 md2_heuristic_glcmd_quality(const md2_model_t *model,
                                           const md2_heuristic_report_t *report)
{
    md2_i32 count_delta;
    md2_i32 primitive_count;
    md2_i32 score;

    if (model == 0 || report == 0)
    {
        return 0;
    }

    count_delta = (md2_i32)(report->glcmd_triangle_count - model->num_tris);
    primitive_count = (md2_i32)(report->glcmd_strip_count + report->glcmd_fan_count);

    score = 0;
    score = (md2_i32)(score + report->glcmd_topology_unordered_match_count * 1000);
    score = (md2_i32)(score + report->glcmd_topology_match_count * 100);
    score = (md2_i32)(score - report->glcmd_topology_miss_count * 2000);
    score = (md2_i32)(score - report->glcmd_unresolved_st_count * 2000);
    score = (md2_i32)(score - report->glcmd_degenerate_count * 2000);
    score = (md2_i32)(score - md2_heuristic_abs_i32(count_delta) * 1500);
    score = (md2_i32)(score - primitive_count * 8);
    score = (md2_i32)(score - report->glcmd_vertex_count);

    return score;
}

static void md2_heuristic_finalize_glcmd_metrics(const md2_model_t *model,
                                                 md2_heuristic_report_t *report)
{
    if (model == 0 || report == 0)
    {
        return;
    }

    report->glcmd_triangle_coverage_q16 =
        md2_heuristic_ratio_q16(report->glcmd_topology_unordered_match_count,
                                model->num_tris);
    report->glcmd_exact_coverage_q16 =
        md2_heuristic_ratio_q16(report->glcmd_topology_match_count,
                                model->num_tris);
    report->glcmd_vertex_per_tri_q16 =
        md2_heuristic_ratio_q16(report->glcmd_vertex_count,
                                model->num_tris);
    report->glcmd_quality_score = md2_heuristic_glcmd_quality(model, report);

    if (report->glcmd_degenerate_count > 0)
    {
        report->flags |= MD2_HEURISTIC_FLAG_GLCMDS_HAS_DEGENERATES;
    }
    if (report->glcmd_unresolved_st_count > 0)
    {
        report->flags |= MD2_HEURISTIC_FLAG_GLCMDS_HAS_UNRESOLVED_ST;
    }

    if (report->glcmd_triangle_count == model->num_tris)
    {
        report->flags |= MD2_HEURISTIC_FLAG_GLCMDS_TRI_MATCH;
    }
    else if (report->glcmd_triangle_count < model->num_tris)
    {
        report->flags |= MD2_HEURISTIC_FLAG_GLCMDS_MISSING_TRIS;
    }
    else
    {
        report->flags |= MD2_HEURISTIC_FLAG_GLCMDS_EXCESS_TRIS;
    }

    if (report->glcmd_topology_match_count == model->num_tris &&
        report->glcmd_triangle_count == model->num_tris &&
        report->glcmd_topology_miss_count == 0)
    {
        report->flags |= MD2_HEURISTIC_FLAG_GLCMDS_EXACT_MATCH;
    }

    if (report->glcmd_topology_unordered_match_count == model->num_tris &&
        report->glcmd_triangle_count == model->num_tris &&
        report->glcmd_topology_miss_count == 0)
    {
        report->flags |= MD2_HEURISTIC_FLAG_GLCMDS_TOPOLOGY_MATCH;
        report->flags |= MD2_HEURISTIC_FLAG_GLCMDS_UNORDERED_MATCH;
    }
}

static int md2_heuristic_resolve_st(const md2_model_t *model,
                                    md2_u16 index_xyz,
                                    md2_fx32_t u,
                                    md2_fx32_t v,
                                    md2_u16 *out_st)
{
    md2_i32 i;
    md2_i32 j;
    md2_u16 st_index;
    md2_fx32_t su;
    md2_fx32_t tv;
    int rc;

    if (model == 0 || out_st == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    *out_st = MD2_HEURISTIC_ST_NONE;

    for (i = 0; i < model->num_tris; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            if (model->triangles[i].index_xyz[j] != index_xyz)
            {
                continue;
            }
            st_index = model->triangles[i].index_st[j];
            rc = md2_fx_texel_center_div_i32(model->texcoords[st_index].s,
                                           model->skinwidth,
                                           &su);
            if (rc != MD2_OK) return rc;
            rc = md2_fx_texel_center_div_i32(model->texcoords[st_index].t,
                                           model->skinheight,
                                           &tv);
            if (rc != MD2_OK) return rc;

            if (md2_heuristic_fx_near(u, su) && md2_heuristic_fx_near(v, tv))
            {
                *out_st = st_index;
                return MD2_OK;
            }
        }
    }

    return MD2_OK;
}

static int md2_heuristic_corner_equal(const md2_triangle_t *tri,
                                      md2_i32 corner,
                                      md2_u16 xyz,
                                      md2_u16 st)
{
    return tri->index_xyz[corner] == xyz && tri->index_st[corner] == st;
}

static int md2_heuristic_match_exact_tri(const md2_triangle_t *tri,
                                         md2_u16 x0,
                                         md2_u16 s0,
                                         md2_u16 x1,
                                         md2_u16 s1,
                                         md2_u16 x2,
                                         md2_u16 s2)
{
    if (md2_heuristic_corner_equal(tri, 0, x0, s0) &&
        md2_heuristic_corner_equal(tri, 1, x1, s1) &&
        md2_heuristic_corner_equal(tri, 2, x2, s2))
    {
        return 1;
    }
    if (md2_heuristic_corner_equal(tri, 1, x0, s0) &&
        md2_heuristic_corner_equal(tri, 2, x1, s1) &&
        md2_heuristic_corner_equal(tri, 0, x2, s2))
    {
        return 1;
    }
    if (md2_heuristic_corner_equal(tri, 2, x0, s0) &&
        md2_heuristic_corner_equal(tri, 0, x1, s1) &&
        md2_heuristic_corner_equal(tri, 1, x2, s2))
    {
        return 1;
    }
    return 0;
}

static int md2_heuristic_wedge_equal(const md2_triangle_t *tri,
                                     md2_i32 corner,
                                     md2_u16 xyz,
                                     md2_u16 st)
{
    return tri->index_xyz[corner] == xyz && tri->index_st[corner] == st;
}

static int md2_heuristic_match_unordered_tri(const md2_triangle_t *tri,
                                             md2_u16 x0,
                                             md2_u16 s0,
                                             md2_u16 x1,
                                             md2_u16 s1,
                                             md2_u16 x2,
                                             md2_u16 s2)
{
    int used0;
    int used1;
    int used2;

    used0 = 0;
    used1 = 0;
    used2 = 0;

    if (md2_heuristic_wedge_equal(tri, 0, x0, s0)) used0 = 1;
    else if (md2_heuristic_wedge_equal(tri, 1, x0, s0)) used1 = 1;
    else if (md2_heuristic_wedge_equal(tri, 2, x0, s0)) used2 = 1;
    else return 0;

    if (!used0 && md2_heuristic_wedge_equal(tri, 0, x1, s1)) used0 = 1;
    else if (!used1 && md2_heuristic_wedge_equal(tri, 1, x1, s1)) used1 = 1;
    else if (!used2 && md2_heuristic_wedge_equal(tri, 2, x1, s1)) used2 = 1;
    else return 0;

    if (!used0 && md2_heuristic_wedge_equal(tri, 0, x2, s2)) used0 = 1;
    else if (!used1 && md2_heuristic_wedge_equal(tri, 1, x2, s2)) used1 = 1;
    else if (!used2 && md2_heuristic_wedge_equal(tri, 2, x2, s2)) used2 = 1;
    else return 0;

    return used0 && used1 && used2;
}

static int md2_heuristic_mark_exact_match(const md2_model_t *model,
                                          md2_heuristic_arena_t *arena,
                                          md2_u16 x0,
                                          md2_u16 s0,
                                          md2_u16 x1,
                                          md2_u16 s1,
                                          md2_u16 x2,
                                          md2_u16 s2)
{
    md2_i32 i;

    for (i = 0; i < model->num_tris; ++i)
    {
        if (arena->exact_matched[i] != 0)
        {
            continue;
        }
        if (md2_heuristic_match_exact_tri(&model->triangles[i], x0, s0, x1, s1, x2, s2))
        {
            arena->exact_matched[i] = 1;
            return 1;
        }
    }

    return 0;
}

static int md2_heuristic_mark_unordered_match(const md2_model_t *model,
                                              md2_heuristic_arena_t *arena,
                                              md2_u16 x0,
                                              md2_u16 s0,
                                              md2_u16 x1,
                                              md2_u16 s1,
                                              md2_u16 x2,
                                              md2_u16 s2)
{
    md2_i32 i;

    for (i = 0; i < model->num_tris; ++i)
    {
        if (arena->unordered_matched[i] != 0)
        {
            continue;
        }
        if (md2_heuristic_match_unordered_tri(&model->triangles[i], x0, s0, x1, s1, x2, s2))
        {
            arena->unordered_matched[i] = 1;
            return 1;
        }
    }

    return 0;
}

static void md2_heuristic_score_cmd_triangle(const md2_model_t *model,
                                             md2_heuristic_arena_t *arena,
                                             md2_heuristic_report_t *report,
                                             md2_i32 ia,
                                             md2_i32 ib,
                                             md2_i32 ic)
{
    md2_u16 x0;
    md2_u16 x1;
    md2_u16 x2;
    md2_u16 s0;
    md2_u16 s1;
    md2_u16 s2;
    int exact;

    x0 = arena->cmd_xyz[ia];
    x1 = arena->cmd_xyz[ib];
    x2 = arena->cmd_xyz[ic];
    s0 = arena->cmd_st[ia];
    s1 = arena->cmd_st[ib];
    s2 = arena->cmd_st[ic];

    if (md2_heuristic_tri_degenerate_u16(x0, x1, x2))
    {
        ++report->glcmd_degenerate_count;
    }

    if (s0 == MD2_HEURISTIC_ST_NONE ||
        s1 == MD2_HEURISTIC_ST_NONE ||
        s2 == MD2_HEURISTIC_ST_NONE)
    {
        ++report->glcmd_topology_miss_count;
        return;
    }

    exact = md2_heuristic_mark_exact_match(model, arena, x0, s0, x1, s1, x2, s2);
    if (exact)
    {
        ++report->glcmd_topology_match_count;
    }

    if (md2_heuristic_mark_unordered_match(model, arena, x0, s0, x1, s1, x2, s2))
    {
        ++report->glcmd_topology_unordered_match_count;
    }
    else
    {
        ++report->glcmd_topology_miss_count;
    }
}

static int md2_heuristic_read_command_vertices(md2_glcmd_iter_t *it,
                                               const md2_model_t *model,
                                               md2_heuristic_arena_t *arena,
                                               md2_heuristic_report_t *report,
                                               md2_i32 vertex_count)
{
    md2_glcmd_vertex_t vtx;
    md2_i32 i;
    int got;
    int rc;

    if (vertex_count > MD2_MAX_TRIANGLES + 2)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    for (i = 0; i < vertex_count; ++i)
    {
        got = md2_glcmd_iter_next_vertex(it, &vtx);
        if (got <= 0)
        {
            if (got == 0)
            {
                return MD2_ERR_BAD_GLCMD;
            }
            return got;
        }

        if (vtx.vertex_index < 0 || vtx.vertex_index >= model->num_xyz)
        {
            return MD2_ERR_BAD_INDEX;
        }

        arena->cmd_xyz[i] = (md2_u16)vtx.vertex_index;
        rc = md2_heuristic_resolve_st(model,
                                      (md2_u16)vtx.vertex_index,
                                      vtx.s,
                                      vtx.t,
                                      &arena->cmd_st[i]);
        if (rc != MD2_OK)
        {
            return rc;
        }
        if (arena->cmd_st[i] == MD2_HEURISTIC_ST_NONE)
        {
            ++report->glcmd_unresolved_st_count;
        }
    }

    return MD2_OK;
}

static int md2_heuristic_score_strip(const md2_model_t *model,
                                     md2_heuristic_arena_t *arena,
                                     md2_heuristic_report_t *report,
                                     md2_i32 vertex_count)
{
    md2_i32 tri_index;
    md2_i32 ia;
    md2_i32 ib;
    md2_i32 ic;

    if (vertex_count < 3)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    for (tri_index = 0; tri_index < vertex_count - 2; ++tri_index)
    {
        if ((tri_index & 1) == 0)
        {
            ia = tri_index;
            ib = tri_index + 1;
            ic = tri_index + 2;
        }
        else
        {
            ia = tri_index + 1;
            ib = tri_index;
            ic = tri_index + 2;
        }

        md2_heuristic_score_cmd_triangle(model, arena, report, ia, ib, ic);
    }

    return MD2_OK;
}

static int md2_heuristic_score_fan(const md2_model_t *model,
                                   md2_heuristic_arena_t *arena,
                                   md2_heuristic_report_t *report,
                                   md2_i32 vertex_count)
{
    md2_i32 tri_index;

    if (vertex_count < 3)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    for (tri_index = 0; tri_index < vertex_count - 2; ++tri_index)
    {
        md2_heuristic_score_cmd_triangle(model,
                                         arena,
                                         report,
                                         0,
                                         tri_index + 1,
                                         tri_index + 2);
    }

    return MD2_OK;
}

static int md2_heuristic_analyze_glcmds(const md2_model_t *model,
                                        md2_heuristic_arena_t *arena,
                                        md2_heuristic_report_t *report)
{
    md2_glcmd_iter_t it;
    md2_glcmd_cmd_t cmd;
    int got;
    int rc;

    if (model->num_glcmds <= 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    rc = md2_validate_glcmds(model);
    if (rc != MD2_OK)
    {
        return rc;
    }

    md2_glcmd_iter_init(&it, model);

    for (;;)
    {
        got = md2_glcmd_iter_next(&it, &cmd);
        if (got <= 0)
        {
            if (got < 0)
            {
                return got;
            }
            break;
        }

        report->glcmd_vertex_count += cmd.vertex_count;
        report->glcmd_triangle_count += cmd.vertex_count - 2;

        rc = md2_heuristic_read_command_vertices(&it,
                                                 model,
                                                 arena,
                                                 report,
                                                 cmd.vertex_count);
        if (rc != MD2_OK)
        {
            return rc;
        }

        if (cmd.primitive == MD2_GLCMD_STRIP)
        {
            ++report->glcmd_strip_count;
            rc = md2_heuristic_score_strip(model, arena, report, cmd.vertex_count);
        }
        else if (cmd.primitive == MD2_GLCMD_FAN)
        {
            ++report->glcmd_fan_count;
            rc = md2_heuristic_score_fan(model, arena, report, cmd.vertex_count);
        }
        else
        {
            rc = MD2_ERR_BAD_GLCMD;
        }

        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    if (report->glcmd_triangle_count <= 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    md2_heuristic_finalize_glcmd_metrics(model, report);

    return MD2_OK;
}

int md2_heuristic_analyze_model(const md2_model_t *model,
                                md2_heuristic_arena_t *arena,
                                md2_heuristic_report_t *out_report)
{
    md2_heuristic_report_t local_report;
    md2_heuristic_report_t *report;
    int rc;

    if (model == 0 || arena == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    report = (out_report != 0) ? out_report : &local_report;
    md2_heuristic_report_init(report);
    md2_heuristic_arena_init(arena);

    rc = md2_heuristic_validate_core_model(model);
    report->base_validation_rc = rc;
    if (rc != MD2_OK)
    {
        report->chosen_source = MD2_HEURISTIC_SOURCE_NONE;
        return rc;
    }
    report->flags |= MD2_HEURISTIC_FLAG_BASE_VALID;

    report->q2_validation_rc = md2_validate_model_quake2(model);
    if (report->q2_validation_rc == MD2_OK)
    {
        report->flags |= MD2_HEURISTIC_FLAG_Q2_STRICT_VALID;
    }

    rc = md2_heuristic_analyze_triangles(model, report);
    if (rc != MD2_OK)
    {
        return rc;
    }

    report->glcmd_validation_rc = md2_heuristic_analyze_glcmds(model, arena, report);
    if (report->glcmd_validation_rc == MD2_OK)
    {
        report->flags |= MD2_HEURISTIC_FLAG_GLCMDS_VALID;
        if (md2_heuristic_glcmds_are_lossless(report))
        {
            report->flags |= MD2_HEURISTIC_FLAG_GLCMDS_LOSSLESS;
        }
    }

    return md2_heuristic_choose_runtime_source(model, report);
}

int md2_heuristic_glcmds_are_lossless(const md2_heuristic_report_t *report)
{
    if (report == 0)
    {
        return 0;
    }

    return ((report->flags & MD2_HEURISTIC_FLAG_GLCMDS_VALID) != 0U) &&
           ((report->flags & MD2_HEURISTIC_FLAG_GLCMDS_TRI_MATCH) != 0U) &&
           ((report->flags & MD2_HEURISTIC_FLAG_GLCMDS_TOPOLOGY_MATCH) != 0U) &&
           report->glcmd_degenerate_count == 0 &&
           report->glcmd_unresolved_st_count == 0 &&
           report->glcmd_topology_miss_count == 0;
}

int md2_heuristic_choose_runtime_source(const md2_model_t *model,
                                        md2_heuristic_report_t *inout_report)
{
    int gl_ok;

    if (model == 0 || inout_report == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    gl_ok = md2_heuristic_glcmds_are_lossless(inout_report);

    if (gl_ok)
    {
        inout_report->chosen_source = MD2_HEURISTIC_SOURCE_GLCMDS_STREAM;
        inout_report->decision_reason = MD2_HEURISTIC_REASON_GLCMDS_LOSSLESS;
        inout_report->flags |= MD2_HEURISTIC_FLAG_PREFERS_GLCMDS;
        inout_report->flags |= MD2_HEURISTIC_FLAG_GLCMDS_LOSSLESS;
        inout_report->flags &= (md2_u32)(~MD2_HEURISTIC_FLAG_PREFERS_TRIANGLES);
    }
    else
    {
        inout_report->chosen_source = MD2_HEURISTIC_SOURCE_TRIANGLES;
        if ((inout_report->flags & MD2_HEURISTIC_FLAG_GLCMDS_VALID) == 0U)
        {
            inout_report->decision_reason = MD2_HEURISTIC_REASON_GLCMDS_INVALID;
        }
        else if (((inout_report->flags & MD2_HEURISTIC_FLAG_GLCMDS_TRI_MATCH) == 0U) ||
                 ((inout_report->flags & MD2_HEURISTIC_FLAG_GLCMDS_MISSING_TRIS) != 0U) ||
                 ((inout_report->flags & MD2_HEURISTIC_FLAG_GLCMDS_EXCESS_TRIS) != 0U))
        {
            inout_report->decision_reason = MD2_HEURISTIC_REASON_GLCMDS_COUNT_MISMATCH;
        }
        else if (inout_report->glcmd_unresolved_st_count != 0)
        {
            inout_report->decision_reason = MD2_HEURISTIC_REASON_GLCMDS_UNRESOLVED_ST;
        }
        else if (inout_report->glcmd_degenerate_count != 0)
        {
            inout_report->decision_reason = MD2_HEURISTIC_REASON_GLCMDS_DEGENERATE;
        }
        else
        {
            inout_report->decision_reason = MD2_HEURISTIC_REASON_GLCMDS_TOPOLOGY_MISMATCH;
        }
        inout_report->flags |= MD2_HEURISTIC_FLAG_PREFERS_TRIANGLES;
        inout_report->flags &= (md2_u32)(~MD2_HEURISTIC_FLAG_PREFERS_GLCMDS);
        inout_report->flags &= (md2_u32)(~MD2_HEURISTIC_FLAG_GLCMDS_LOSSLESS);
    }

    return MD2_OK;
}

int md2_heuristic_build_best_mesh(const md2_model_t *model,
                                  md2_heuristic_arena_t *arena,
                                  md2_runtime_mesh_t *out_mesh,
                                  md2_heuristic_report_t *out_report)
{
    md2_heuristic_report_t local_report;
    md2_heuristic_report_t *report;
    int rc;

    if (model == 0 || arena == 0 || out_mesh == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    report = (out_report != 0) ? out_report : &local_report;
    rc = md2_heuristic_analyze_model(model, arena, report);
    if (rc != MD2_OK)
    {
        return rc;
    }

    if (report->chosen_source == MD2_HEURISTIC_SOURCE_GLCMDS_STREAM)
    {
        rc = md2_runtime_build_glcmd_stream_mesh(model, out_mesh);
        if (rc == MD2_OK)
        {
            return MD2_OK;
        }
        report->chosen_source = MD2_HEURISTIC_SOURCE_TRIANGLES;
        report->flags |= MD2_HEURISTIC_FLAG_PREFERS_TRIANGLES;
        report->flags &= (md2_u32)(~MD2_HEURISTIC_FLAG_PREFERS_GLCMDS);
    }

    return md2_runtime_build_triangle_mesh(model, out_mesh);
}


static void md2_heuristic_save_glcmds(const md2_model_t *model,
                                      md2_glcmd_build_arena_t *arena)
{
    md2_i32 i;
    md2_i32 count;

    count = model->num_glcmds;
    if (count < 0)
    {
        count = 0;
    }
    if (count > MD2_MAX_GLCMDS)
    {
        count = MD2_MAX_GLCMDS;
    }

    for (i = 0; i < count; ++i)
    {
        arena->saved_glcmds[i] = model->glcmds[i];
    }
    arena->saved_num_glcmds = count;
}

static void md2_heuristic_restore_glcmds(md2_model_t *model,
                                         const md2_glcmd_build_arena_t *arena)
{
    md2_i32 i;
    md2_i32 count;

    count = arena->saved_num_glcmds;
    if (count < 0)
    {
        count = 0;
    }
    if (count > MD2_MAX_GLCMDS)
    {
        count = MD2_MAX_GLCMDS;
    }

    for (i = 0; i < count; ++i)
    {
        model->glcmds[i] = arena->saved_glcmds[i];
    }
    model->num_glcmds = count;
}

static int md2_heuristic_rebuild_stats_better(const md2_glcmd_build_stats_t *candidate,
                                              const md2_heuristic_report_t *candidate_report,
                                              const md2_glcmd_build_stats_t *best,
                                              const md2_heuristic_report_t *best_report)
{
    if (candidate->num_glcmds < best->num_glcmds) return 1;
    if (candidate->num_glcmds > best->num_glcmds) return 0;
    if (candidate->num_primitives < best->num_primitives) return 1;
    if (candidate->num_primitives > best->num_primitives) return 0;
    if (candidate->num_gl_vertices < best->num_gl_vertices) return 1;
    if (candidate->num_gl_vertices > best->num_gl_vertices) return 0;
    if (candidate->num_strips > best->num_strips) return 1;
    if (candidate->num_strips < best->num_strips) return 0;
    if (candidate_report->glcmd_quality_score > best_report->glcmd_quality_score) return 1;
    return 0;
}

static void md2_heuristic_apply_rebuild_stats(const md2_model_t *model,
                                              const md2_glcmd_build_stats_t *stats,
                                              md2_heuristic_report_t *report)
{
    report->flags |= MD2_HEURISTIC_FLAG_REBUILT_GLCMDS;
    report->rebuilt_build_mode = stats->build_mode;
    report->rebuilt_num_primitives = stats->num_primitives;
    report->rebuilt_num_strips = stats->num_strips;
    report->rebuilt_num_fans = stats->num_fans;
    report->rebuilt_num_single_triangle_primitives =
        stats->num_single_triangle_primitives;
    report->rebuilt_num_gl_vertices = stats->num_gl_vertices;
    report->rebuilt_num_glcmds = stats->num_glcmds;
    report->rebuilt_emitted_triangle_count = stats->emitted_triangle_count;
    report->rebuilt_max_primitive_vertices = stats->max_primitive_vertices;
    report->rebuilt_max_primitive_triangles = stats->max_primitive_triangles;
    report->rebuilt_vertex_per_tri_q16 =
        md2_heuristic_ratio_q16(stats->num_gl_vertices, model->num_tris);
    report->rebuilt_quality_score = report->glcmd_quality_score;
}

static int md2_heuristic_try_rebuild_mode(md2_model_t *model,
                                          md2_glcmd_build_arena_t *build_arena,
                                          md2_heuristic_arena_t *heuristic_arena,
                                          md2_i32 build_mode,
                                          md2_glcmd_build_stats_t *out_stats,
                                          md2_heuristic_report_t *out_report)
{
    int rc;

    rc = md2_glcmd_build_from_triangles_ex(model,
                                           build_arena,
                                           build_mode,
                                           out_stats);
    if (rc != MD2_OK)
    {
        return rc;
    }

    rc = md2_heuristic_analyze_model(model, heuristic_arena, out_report);
    if (rc != MD2_OK)
    {
        return rc;
    }

    if (!md2_heuristic_glcmds_are_lossless(out_report))
    {
        return MD2_ERR_BAD_GLCMD;
    }

    return MD2_OK;
}

int md2_heuristic_rebuild_glcmds(md2_model_t *model,
                                 md2_glcmd_build_arena_t *build_arena,
                                 md2_heuristic_arena_t *heuristic_arena,
                                 md2_glcmd_build_stats_t *out_build_stats,
                                 md2_heuristic_report_t *out_report)
{
    md2_heuristic_report_t local_report;
    md2_heuristic_report_t q2_report;
    md2_heuristic_report_t global_report;
    md2_heuristic_report_t best_report;
    md2_heuristic_report_t *report;
    md2_glcmd_build_stats_t q2_stats;
    md2_glcmd_build_stats_t global_stats;
    md2_glcmd_build_stats_t best_stats;
    int q2_rc;
    int global_rc;
    int have_best;

    if (model == 0 || build_arena == 0 || heuristic_arena == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    report = (out_report != 0) ? out_report : &local_report;
    have_best = 0;

    md2_heuristic_save_glcmds(model, build_arena);

    q2_rc = md2_heuristic_try_rebuild_mode(model,
                                           build_arena,
                                           heuristic_arena,
                                           MD2_GLCMD_BUILD_MODE_QUAKE2_ORDER,
                                           &q2_stats,
                                           &q2_report);
    if (q2_rc == MD2_OK)
    {
        have_best = 1;
        best_stats = q2_stats;
        best_report = q2_report;
        md2_heuristic_save_glcmds(model, build_arena);
    }

    global_rc = md2_heuristic_try_rebuild_mode(model,
                                               build_arena,
                                               heuristic_arena,
                                               MD2_GLCMD_BUILD_MODE_GLOBAL_BEST,
                                               &global_stats,
                                               &global_report);
    if (global_rc == MD2_OK)
    {
        if (!have_best ||
            md2_heuristic_rebuild_stats_better(&global_stats,
                                               &global_report,
                                               &best_stats,
                                               &best_report))
        {
            have_best = 1;
            best_stats = global_stats;
            best_report = global_report;
            md2_heuristic_save_glcmds(model, build_arena);
        }
    }

    if (!have_best)
    {
        md2_heuristic_restore_glcmds(model, build_arena);
        return (global_rc != MD2_OK) ? global_rc : q2_rc;
    }

    md2_heuristic_restore_glcmds(model, build_arena);
    *report = best_report;
    md2_heuristic_apply_rebuild_stats(model, &best_stats, report);
    report->flags |= MD2_HEURISTIC_FLAG_BUILD_ROUNDTRIP_OK;

    if (out_build_stats != 0)
    {
        *out_build_stats = best_stats;
    }

    return MD2_OK;
}

int md2_heuristic_sanitize_frame_pair(const md2_model_t *model,
                                      md2_i32 *frame_index,
                                      md2_i32 *old_frame_index)
{
    if (model == 0 || frame_index == 0 || old_frame_index == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (model->num_frames <= 0)
    {
        return MD2_ERR_BAD_LAYOUT;
    }

    if (*frame_index < 0 || *frame_index >= model->num_frames ||
        *old_frame_index < 0 || *old_frame_index >= model->num_frames)
    {
        *frame_index = 0;
        *old_frame_index = 0;
        return MD2_ERR_BAD_INDEX;
    }

    return MD2_OK;
}
