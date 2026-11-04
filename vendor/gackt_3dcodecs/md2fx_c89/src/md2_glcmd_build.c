#include <string.h>

#include "md2/md2_error.h"
#include "md2/md2_fixed.h"
#include "md2/md2_glcmd_build.h"
#include "md2/md2_validate.h"

#define MD2_GLCMD_BUILD_FAN_TYPE   0
#define MD2_GLCMD_BUILD_STRIP_TYPE 1
#define MD2_GLCMD_BUILD_TEMP_USED  2
#define MD2_GLCMD_BUILD_USED       1

void md2_glcmd_build_arena_init(md2_glcmd_build_arena_t *arena)
{
    if (arena != 0)
    {
        memset(arena, 0, sizeof(*arena));
        arena->best_start_tri = -1;
        arena->best_start_v = -1;
    }
}

void md2_glcmd_build_stats_init(md2_glcmd_build_stats_t *stats)
{
    if (stats != 0)
    {
        memset(stats, 0, sizeof(*stats));
        stats->build_mode = MD2_GLCMD_BUILD_MODE_QUAKE2_ORDER;
    }
}

static void md2_glcmd_build_reset_work(md2_glcmd_build_arena_t *arena)
{
    if (arena != 0)
    {
        memset(arena->used, 0, sizeof(arena->used));
        arena->cur_len = 0;
        arena->best_len = 0;
        arena->best_type = MD2_GLCMD_BUILD_FAN_TYPE;
        arena->best_start_tri = -1;
        arena->best_start_v = -1;
    }
}

static void md2_glcmd_build_clear_temp(md2_glcmd_build_arena_t *arena)
{
    md2_i32 i;
    md2_i32 tri_index;

    for (i = 0; i < arena->cur_len; ++i)
    {
        tri_index = arena->cur_tris[i];
        if (tri_index >= 0 && tri_index < MD2_MAX_TRIANGLES)
        {
            if (arena->used[tri_index] == MD2_GLCMD_BUILD_TEMP_USED)
            {
                arena->used[tri_index] = 0;
            }
        }
    }
}

static int md2_glcmd_build_same_edge(const md2_triangle_t *tri,
                                     md2_i32 k,
                                     md2_u16 a_xyz,
                                     md2_u16 a_st,
                                     md2_u16 b_xyz,
                                     md2_u16 b_st)
{
    if (tri->index_xyz[k] != a_xyz)
    {
        return 0;
    }
    if (tri->index_st[k] != a_st)
    {
        return 0;
    }
    if (tri->index_xyz[(k + 1) % 3] != b_xyz)
    {
        return 0;
    }
    if (tri->index_st[(k + 1) % 3] != b_st)
    {
        return 0;
    }
    return 1;
}

static int md2_glcmd_build_begin_candidate(const md2_model_t *model,
                                           md2_glcmd_build_arena_t *arena,
                                           md2_i32 start_tri,
                                           md2_i32 start_v)
{
    const md2_triangle_t *tri;

    if (start_tri < 0 || start_tri >= model->num_tris || start_v < 0 || start_v >= 3)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (arena->used[start_tri] != 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    tri = &model->triangles[start_tri];
    arena->cur_xyz[0] = tri->index_xyz[start_v % 3];
    arena->cur_xyz[1] = tri->index_xyz[(start_v + 1) % 3];
    arena->cur_xyz[2] = tri->index_xyz[(start_v + 2) % 3];
    arena->cur_st[0] = tri->index_st[start_v % 3];
    arena->cur_st[1] = tri->index_st[(start_v + 1) % 3];
    arena->cur_st[2] = tri->index_st[(start_v + 2) % 3];
    arena->cur_tris[0] = start_tri;
    arena->cur_len = 1;
    arena->used[start_tri] = MD2_GLCMD_BUILD_TEMP_USED;

    return MD2_OK;
}

static md2_i32 md2_glcmd_build_fan_len(const md2_model_t *model,
                                       md2_glcmd_build_arena_t *arena,
                                       md2_i32 start_tri,
                                       md2_i32 start_v)
{
    const md2_triangle_t *last;
    const md2_triangle_t *check;
    md2_u16 m1;
    md2_u16 m2;
    md2_u16 st1;
    md2_u16 st2;
    md2_i32 j;
    md2_i32 k;
    int rc;

    rc = md2_glcmd_build_begin_candidate(model, arena, start_tri, start_v);
    if (rc != MD2_OK)
    {
        return 0;
    }

    last = &model->triangles[start_tri];
    m1 = last->index_xyz[(start_v + 0) % 3];
    st1 = last->index_st[(start_v + 0) % 3];
    m2 = last->index_xyz[(start_v + 2) % 3];
    st2 = last->index_st[(start_v + 2) % 3];

next_tri:
    for (j = start_tri + 1; j < model->num_tris; ++j)
    {
        check = &model->triangles[j];
        for (k = 0; k < 3; ++k)
        {
            if (!md2_glcmd_build_same_edge(check, k, m1, st1, m2, st2))
            {
                continue;
            }

            if (arena->used[j] != 0)
            {
                goto done;
            }

            if (arena->cur_len >= MD2_MAX_TRIANGLES)
            {
                goto done;
            }

            m2 = check->index_xyz[(k + 2) % 3];
            st2 = check->index_st[(k + 2) % 3];

            arena->cur_xyz[arena->cur_len + 2] = m2;
            arena->cur_st[arena->cur_len + 2] = st2;
            arena->cur_tris[arena->cur_len] = j;
            ++arena->cur_len;
            arena->used[j] = MD2_GLCMD_BUILD_TEMP_USED;
            goto next_tri;
        }
    }

done:
    md2_glcmd_build_clear_temp(arena);
    return arena->cur_len;
}

static md2_i32 md2_glcmd_build_strip_len(const md2_model_t *model,
                                         md2_glcmd_build_arena_t *arena,
                                         md2_i32 start_tri,
                                         md2_i32 start_v)
{
    const md2_triangle_t *last;
    const md2_triangle_t *check;
    md2_u16 m1;
    md2_u16 m2;
    md2_u16 st1;
    md2_u16 st2;
    md2_i32 j;
    md2_i32 k;
    int rc;

    rc = md2_glcmd_build_begin_candidate(model, arena, start_tri, start_v);
    if (rc != MD2_OK)
    {
        return 0;
    }

    last = &model->triangles[start_tri];
    m1 = last->index_xyz[(start_v + 2) % 3];
    st1 = last->index_st[(start_v + 2) % 3];
    m2 = last->index_xyz[(start_v + 1) % 3];
    st2 = last->index_st[(start_v + 1) % 3];

next_tri:
    for (j = start_tri + 1; j < model->num_tris; ++j)
    {
        check = &model->triangles[j];
        for (k = 0; k < 3; ++k)
        {
            if (!md2_glcmd_build_same_edge(check, k, m1, st1, m2, st2))
            {
                continue;
            }

            if (arena->used[j] != 0)
            {
                goto done;
            }

            if (arena->cur_len >= MD2_MAX_TRIANGLES)
            {
                goto done;
            }

            if ((arena->cur_len & 1) != 0)
            {
                m2 = check->index_xyz[(k + 2) % 3];
                st2 = check->index_st[(k + 2) % 3];
            }
            else
            {
                m1 = check->index_xyz[(k + 2) % 3];
                st1 = check->index_st[(k + 2) % 3];
            }

            arena->cur_xyz[arena->cur_len + 2] = check->index_xyz[(k + 2) % 3];
            arena->cur_st[arena->cur_len + 2] = check->index_st[(k + 2) % 3];
            arena->cur_tris[arena->cur_len] = j;
            ++arena->cur_len;
            arena->used[j] = MD2_GLCMD_BUILD_TEMP_USED;
            goto next_tri;
        }
    }

done:
    md2_glcmd_build_clear_temp(arena);
    return arena->cur_len;
}

static int md2_glcmd_build_emit_vertex(md2_model_t *model,
                                       md2_glcmd_builder_t *builder,
                                       md2_u16 index_xyz,
                                       md2_u16 index_st)
{
    md2_fx32_t s;
    md2_fx32_t t;
    int rc;

    if ((md2_i32)index_xyz >= model->num_xyz || (md2_i32)index_st >= model->num_st)
    {
        return MD2_ERR_BAD_INDEX;
    }

    rc = md2_fx_texel_center_div_i32(model->texcoords[index_st].s,
                                     model->skinwidth,
                                     &s);
    if (rc != MD2_OK)
    {
        return rc;
    }

    rc = md2_fx_texel_center_div_i32(model->texcoords[index_st].t,
                                     model->skinheight,
                                     &t);
    if (rc != MD2_OK)
    {
        return rc;
    }

    return md2_glcmd_builder_vertex(builder, s, t, (md2_i32)index_xyz);
}

static void md2_glcmd_build_copy_best(md2_glcmd_build_arena_t *arena,
                                      md2_i32 type,
                                      md2_i32 len,
                                      md2_i32 start_tri,
                                      md2_i32 start_v)
{
    md2_i32 i;

    arena->best_type = type;
    arena->best_len = len;
    arena->best_start_tri = start_tri;
    arena->best_start_v = start_v;

    for (i = 0; i < len + 2; ++i)
    {
        arena->best_xyz[i] = arena->cur_xyz[i];
        arena->best_st[i] = arena->cur_st[i];
    }

    for (i = 0; i < len; ++i)
    {
        arena->best_tris[i] = arena->cur_tris[i];
    }
}

static int md2_glcmd_build_candidate_better(const md2_glcmd_build_arena_t *arena,
                                            md2_i32 type,
                                            md2_i32 len,
                                            md2_i32 start_tri,
                                            md2_i32 start_v,
                                            md2_i32 build_mode)
{
    if (len > arena->best_len)
    {
        return 1;
    }
    if (len < arena->best_len)
    {
        return 0;
    }
    if (build_mode == MD2_GLCMD_BUILD_MODE_QUAKE2_ORDER)
    {
        return 0;
    }

    if (type == MD2_GLCMD_BUILD_STRIP_TYPE &&
        arena->best_type != MD2_GLCMD_BUILD_STRIP_TYPE)
    {
        return 1;
    }
    if (type != arena->best_type)
    {
        return 0;
    }
    if (arena->best_start_tri < 0 || start_tri < arena->best_start_tri)
    {
        return 1;
    }
    if (start_tri == arena->best_start_tri && start_v < arena->best_start_v)
    {
        return 1;
    }

    return 0;
}

static void md2_glcmd_build_consider_candidate(const md2_model_t *model,
                                               md2_glcmd_build_arena_t *arena,
                                               md2_i32 tri_index,
                                               md2_i32 start_v,
                                               md2_i32 type,
                                               md2_i32 build_mode)
{
    md2_i32 len;

    if (type == MD2_GLCMD_BUILD_STRIP_TYPE)
    {
        len = md2_glcmd_build_strip_len(model, arena, tri_index, start_v);
    }
    else
    {
        len = md2_glcmd_build_fan_len(model, arena, tri_index, start_v);
    }

    if (md2_glcmd_build_candidate_better(arena,
                                         type,
                                         len,
                                         tri_index,
                                         start_v,
                                         build_mode))
    {
        md2_glcmd_build_copy_best(arena, type, len, tri_index, start_v);
    }
}

static int md2_glcmd_build_select_q2_order(const md2_model_t *model,
                                           md2_glcmd_build_arena_t *arena,
                                           md2_i32 tri_index)
{
    md2_i32 start_v;
    md2_i32 type;

    arena->best_len = 0;
    arena->best_type = MD2_GLCMD_BUILD_FAN_TYPE;
    arena->best_start_tri = tri_index;
    arena->best_start_v = 0;

    for (type = 0; type < 2; ++type)
    {
        for (start_v = 0; start_v < 3; ++start_v)
        {
            md2_glcmd_build_consider_candidate(model,
                                               arena,
                                               tri_index,
                                               start_v,
                                               type,
                                               MD2_GLCMD_BUILD_MODE_QUAKE2_ORDER);
        }
    }

    return (arena->best_len > 0) ? MD2_OK : MD2_ERR_BAD_GLCMD;
}

static int md2_glcmd_build_select_global_best(const md2_model_t *model,
                                              md2_glcmd_build_arena_t *arena)
{
    md2_i32 tri_index;
    md2_i32 start_v;
    md2_i32 type;

    arena->best_len = 0;
    arena->best_type = MD2_GLCMD_BUILD_FAN_TYPE;
    arena->best_start_tri = -1;
    arena->best_start_v = -1;

    for (tri_index = 0; tri_index < model->num_tris; ++tri_index)
    {
        if (arena->used[tri_index] != 0)
        {
            continue;
        }
        for (type = 0; type < 2; ++type)
        {
            for (start_v = 0; start_v < 3; ++start_v)
            {
                md2_glcmd_build_consider_candidate(model,
                                                   arena,
                                                   tri_index,
                                                   start_v,
                                                   type,
                                                   MD2_GLCMD_BUILD_MODE_GLOBAL_BEST);
            }
        }
    }

    return (arena->best_len > 0) ? MD2_OK : MD2_ERR_BAD_GLCMD;
}

static void md2_glcmd_build_stats_add_primitive(md2_glcmd_build_stats_t *stats,
                                                md2_i32 primitive,
                                                md2_i32 tri_count,
                                                md2_i32 vertex_count)
{
    stats->num_primitives = (md2_i32)(stats->num_primitives + 1);
    stats->num_gl_vertices = (md2_i32)(stats->num_gl_vertices + vertex_count);
    stats->emitted_triangle_count = (md2_i32)(stats->emitted_triangle_count + tri_count);
    stats->chosen_start_tri_count = (md2_i32)(stats->chosen_start_tri_count + 1);

    if (tri_count == 1)
    {
        stats->num_single_triangle_primitives =
            (md2_i32)(stats->num_single_triangle_primitives + 1);
    }
    if (vertex_count > stats->max_primitive_vertices)
    {
        stats->max_primitive_vertices = vertex_count;
    }
    if (tri_count > stats->max_primitive_triangles)
    {
        stats->max_primitive_triangles = tri_count;
    }

    if (primitive == MD2_GLCMD_STRIP)
    {
        stats->num_strips = (md2_i32)(stats->num_strips + 1);
    }
    else
    {
        stats->num_fans = (md2_i32)(stats->num_fans + 1);
    }
}

static int md2_glcmd_build_emit_best(md2_model_t *model,
                                     md2_glcmd_build_arena_t *arena,
                                     md2_glcmd_builder_t *builder,
                                     md2_glcmd_build_stats_t *stats)
{
    md2_i32 i;
    md2_i32 vertex_count;
    md2_i32 primitive;
    int rc;

    if (arena->best_len <= 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    for (i = 0; i < arena->best_len; ++i)
    {
        if (arena->best_tris[i] < 0 || arena->best_tris[i] >= model->num_tris)
        {
            return MD2_ERR_BAD_INDEX;
        }
        arena->used[arena->best_tris[i]] = MD2_GLCMD_BUILD_USED;
    }

    vertex_count = arena->best_len + 2;
    primitive = (arena->best_type == MD2_GLCMD_BUILD_STRIP_TYPE) ?
                MD2_GLCMD_STRIP : MD2_GLCMD_FAN;

    rc = md2_glcmd_builder_begin(builder, primitive, vertex_count);
    if (rc != MD2_OK)
    {
        return rc;
    }

    for (i = 0; i < vertex_count; ++i)
    {
        rc = md2_glcmd_build_emit_vertex(model,
                                         builder,
                                         arena->best_xyz[i],
                                         arena->best_st[i]);
        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    md2_glcmd_build_stats_add_primitive(stats,
                                        primitive,
                                        arena->best_len,
                                        vertex_count);
    return MD2_OK;
}

static int md2_glcmd_build_check_input(const md2_model_t *model)
{
    int rc;

    if (model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (model->num_xyz <= 0 || model->num_st <= 0 || model->num_tris <= 0)
    {
        return MD2_ERR_BAD_LAYOUT;
    }
    if (model->num_xyz > MD2_MAX_VERTS ||
        model->num_st > MD2_MAX_TEXCOORDS ||
        model->num_tris > MD2_MAX_TRIANGLES)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    rc = md2_validate_indices(model);
    if (rc != MD2_OK)
    {
        return rc;
    }
    if (model->skinwidth <= 0 || model->skinheight <= 0)
    {
        return MD2_ERR_BAD_LAYOUT;
    }

    return MD2_OK;
}

static int md2_glcmd_build_from_triangles_impl(md2_model_t *model,
                                               md2_glcmd_build_arena_t *arena,
                                               md2_i32 build_mode,
                                               md2_glcmd_build_stats_t *out_stats)
{
    md2_glcmd_builder_t builder;
    md2_glcmd_build_stats_t stats;
    md2_i32 tri_index;
    md2_i32 remaining;
    int rc;

    rc = md2_glcmd_build_check_input(model);
    if (rc != MD2_OK)
    {
        return rc;
    }

    md2_glcmd_build_reset_work(arena);
    md2_glcmd_build_stats_init(&stats);
    stats.build_mode = build_mode;
    md2_glcmd_builder_init(&builder, model);

    remaining = model->num_tris;

    if (build_mode == MD2_GLCMD_BUILD_MODE_GLOBAL_BEST)
    {
        while (remaining > 0)
        {
            rc = md2_glcmd_build_select_global_best(model, arena);
            if (rc != MD2_OK)
            {
                return rc;
            }

            rc = md2_glcmd_build_emit_best(model, arena, &builder, &stats);
            if (rc != MD2_OK)
            {
                return rc;
            }
            remaining = (md2_i32)(remaining - arena->best_len);
        }
    }
    else
    {
        for (tri_index = 0; tri_index < model->num_tris; ++tri_index)
        {
            if (arena->used[tri_index] != 0)
            {
                continue;
            }

            rc = md2_glcmd_build_select_q2_order(model, arena, tri_index);
            if (rc != MD2_OK)
            {
                return rc;
            }

            rc = md2_glcmd_build_emit_best(model, arena, &builder, &stats);
            if (rc != MD2_OK)
            {
                return rc;
            }
            remaining = (md2_i32)(remaining - arena->best_len);
        }
    }

    if (remaining != 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    rc = md2_glcmd_builder_end(&builder);
    if (rc != MD2_OK)
    {
        return rc;
    }

    stats.num_glcmds = model->num_glcmds;
    if (out_stats != 0)
    {
        *out_stats = stats;
    }

    return md2_validate_glcmds(model);
}

static void md2_glcmd_build_save_current(const md2_model_t *model,
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

static void md2_glcmd_build_restore_saved(md2_model_t *model,
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

static int md2_glcmd_build_stats_better(const md2_glcmd_build_stats_t *a,
                                        const md2_glcmd_build_stats_t *b)
{
    if (a->num_glcmds < b->num_glcmds) return 1;
    if (a->num_glcmds > b->num_glcmds) return 0;
    if (a->num_primitives < b->num_primitives) return 1;
    if (a->num_primitives > b->num_primitives) return 0;
    if (a->num_gl_vertices < b->num_gl_vertices) return 1;
    if (a->num_gl_vertices > b->num_gl_vertices) return 0;
    if (a->num_strips > b->num_strips) return 1;
    return 0;
}

int md2_glcmd_build_from_triangles_ex(md2_model_t *model,
                                      md2_glcmd_build_arena_t *arena,
                                      md2_i32 build_mode,
                                      md2_glcmd_build_stats_t *out_stats)
{
    md2_glcmd_build_stats_t q2_stats;
    md2_glcmd_build_stats_t global_stats;
    int q2_rc;
    int global_rc;

    if (model == 0 || arena == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    if (build_mode == MD2_GLCMD_BUILD_MODE_QUAKE2_ORDER ||
        build_mode == MD2_GLCMD_BUILD_MODE_GLOBAL_BEST)
    {
        return md2_glcmd_build_from_triangles_impl(model,
                                                   arena,
                                                   build_mode,
                                                   out_stats);
    }

    if (build_mode != MD2_GLCMD_BUILD_MODE_AUTO)
    {
        return MD2_ERR_BAD_ARG;
    }

    q2_rc = md2_glcmd_build_from_triangles_impl(model,
                                                arena,
                                                MD2_GLCMD_BUILD_MODE_QUAKE2_ORDER,
                                                &q2_stats);
    if (q2_rc == MD2_OK)
    {
        md2_glcmd_build_save_current(model, arena);
    }

    global_rc = md2_glcmd_build_from_triangles_impl(model,
                                                    arena,
                                                    MD2_GLCMD_BUILD_MODE_GLOBAL_BEST,
                                                    &global_stats);
    if (global_rc == MD2_OK)
    {
        if (q2_rc == MD2_OK && !md2_glcmd_build_stats_better(&global_stats, &q2_stats))
        {
            md2_glcmd_build_restore_saved(model, arena);
            if (out_stats != 0)
            {
                *out_stats = q2_stats;
            }
        }
        else if (out_stats != 0)
        {
            *out_stats = global_stats;
        }
        return MD2_OK;
    }

    if (q2_rc == MD2_OK)
    {
        md2_glcmd_build_restore_saved(model, arena);
        if (out_stats != 0)
        {
            *out_stats = q2_stats;
        }
        return MD2_OK;
    }

    return global_rc;
}

int md2_glcmd_build_from_triangles(md2_model_t *model,
                                   md2_glcmd_build_arena_t *arena,
                                   md2_glcmd_build_stats_t *out_stats)
{
    return md2_glcmd_build_from_triangles_ex(model,
                                             arena,
                                             MD2_GLCMD_BUILD_MODE_QUAKE2_ORDER,
                                             out_stats);
}
