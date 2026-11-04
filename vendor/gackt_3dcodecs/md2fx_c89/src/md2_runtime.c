#include <string.h>

#include "md2/md2_anim.h"
#include "md2/md2_error.h"
#include "md2/md2_fixed.h"
#include "md2/md2_glcmd.h"
#include "md2/md2_runtime.h"
#include "md2/md2_validate.h"

static md2_fx32_t md2_runtime_from_i32(md2_i32 v)
{
    return (md2_fx32_t)(v * (md2_i32)MD2_FX_ONE);
}

static int md2_runtime_find_triangle_vertex(const md2_runtime_mesh_t *mesh,
                                            md2_u16 index_xyz,
                                            md2_u16 index_st)
{
    md2_i32 i;

    for (i = 0; i < mesh->num_vertices; ++i)
    {
        if (mesh->vertices[i].source_kind == (md2_u8)MD2_RUNTIME_SOURCE_TRIANGLES &&
            mesh->vertices[i].index_xyz == index_xyz &&
            mesh->vertices[i].index_st == index_st)
        {
            return (int)i;
        }
    }

    return -1;
}

static int md2_runtime_find_glcmd_vertex(const md2_runtime_mesh_t *mesh,
                                         md2_u16 index_xyz,
                                         md2_fx32_t u,
                                         md2_fx32_t v)
{
    md2_i32 i;

    for (i = 0; i < mesh->num_vertices; ++i)
    {
        if (mesh->vertices[i].source_kind == (md2_u8)MD2_RUNTIME_SOURCE_GLCMDS &&
            mesh->vertices[i].index_xyz == index_xyz &&
            mesh->vertices[i].u == u &&
            mesh->vertices[i].v == v)
        {
            return (int)i;
        }
    }

    return -1;
}

static int md2_runtime_append_triangle_vertex(const md2_model_t *model,
                                              md2_runtime_mesh_t *mesh,
                                              md2_u16 index_xyz,
                                              md2_u16 index_st,
                                              md2_u16 *out_index)
{
    md2_i32 found;
    md2_i32 pos;
    const md2_texcoord_t *st;
    md2_fx32_t u;
    md2_fx32_t v;

    found = (md2_i32)md2_runtime_find_triangle_vertex(mesh, index_xyz, index_st);
    if (found >= 0)
    {
        *out_index = (md2_u16)found;
        return MD2_OK;
    }

    if (mesh->num_vertices >= MD2_RUNTIME_MAX_VERTICES)
    {
        return MD2_ERR_OVER_CAPACITY;
    }
    if (model->skinwidth <= 0 || model->skinheight <= 0)
    {
        return MD2_ERR_BAD_LAYOUT;
    }

    st = &model->texcoords[index_st];
    u = md2_fx_div_i32(md2_runtime_from_i32((md2_i32)st->s), model->skinwidth);
    v = md2_fx_div_i32(md2_runtime_from_i32((md2_i32)st->t), model->skinheight);

    pos = mesh->num_vertices;
    mesh->vertices[pos].index_xyz = index_xyz;
    mesh->vertices[pos].index_st = index_st;
    mesh->vertices[pos].source_kind = (md2_u8)MD2_RUNTIME_SOURCE_TRIANGLES;
    mesh->vertices[pos].reserved0 = 0;
    mesh->vertices[pos].u = u;
    mesh->vertices[pos].v = v;
    mesh->num_vertices = pos + 1;

    *out_index = (md2_u16)pos;
    return MD2_OK;
}

static int md2_runtime_append_glcmd_vertex(md2_runtime_mesh_t *mesh,
                                           md2_u16 index_xyz,
                                           md2_fx32_t u,
                                           md2_fx32_t v,
                                           md2_u16 *out_index)
{
    md2_i32 found;
    md2_i32 pos;

    found = (md2_i32)md2_runtime_find_glcmd_vertex(mesh, index_xyz, u, v);
    if (found >= 0)
    {
        *out_index = (md2_u16)found;
        return MD2_OK;
    }

    if (mesh->num_vertices >= MD2_RUNTIME_MAX_VERTICES)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    pos = mesh->num_vertices;
    mesh->vertices[pos].index_xyz = index_xyz;
    mesh->vertices[pos].index_st = MD2_RUNTIME_TEXCOORD_INDEX_NONE;
    mesh->vertices[pos].source_kind = (md2_u8)MD2_RUNTIME_SOURCE_GLCMDS;
    mesh->vertices[pos].reserved0 = 0;
    mesh->vertices[pos].u = u;
    mesh->vertices[pos].v = v;
    mesh->num_vertices = pos + 1;

    *out_index = (md2_u16)pos;
    return MD2_OK;
}

static int md2_runtime_append_glcmd_stream_vertex(md2_runtime_mesh_t *mesh,
                                                  md2_u16 index_xyz,
                                                  md2_fx32_t u,
                                                  md2_fx32_t v,
                                                  md2_u16 *out_index)
{
    md2_i32 pos;

    if (mesh == 0 || out_index == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (mesh->num_vertices >= MD2_RUNTIME_MAX_VERTICES)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    pos = mesh->num_vertices;
    mesh->vertices[pos].index_xyz = index_xyz;
    mesh->vertices[pos].index_st = MD2_RUNTIME_TEXCOORD_INDEX_NONE;
    mesh->vertices[pos].source_kind = (md2_u8)MD2_RUNTIME_SOURCE_GLCMDS_STREAM;
    mesh->vertices[pos].reserved0 = 0;
    mesh->vertices[pos].u = u;
    mesh->vertices[pos].v = v;
    mesh->num_vertices = pos + 1;

    *out_index = (md2_u16)pos;
    return MD2_OK;
}

static int md2_runtime_emit_triangle(md2_runtime_mesh_t *mesh,
                                     md2_u16 a,
                                     md2_u16 b,
                                     md2_u16 c)
{
    if (mesh->num_indices + 3 > MD2_RUNTIME_MAX_INDICES)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    mesh->indices[mesh->num_indices++] = a;
    mesh->indices[mesh->num_indices++] = b;
    mesh->indices[mesh->num_indices++] = c;
    return MD2_OK;
}

static int md2_runtime_read_glcmd_runtime_index(md2_glcmd_iter_t *it,
                                                md2_runtime_mesh_t *mesh,
                                                md2_i32 num_xyz,
                                                md2_u16 *out_index)
{
    md2_glcmd_vertex_t vtx;
    int rc;
    int got;

    if (it == 0 || mesh == 0 || out_index == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    got = md2_glcmd_iter_next_vertex(it, &vtx);
    if (got <= 0)
    {
        if (got == 0)
        {
            return MD2_ERR_BAD_GLCMD;
        }
        return got;
    }

    if (vtx.vertex_index < 0 || vtx.vertex_index >= num_xyz)
    {
        return MD2_ERR_BAD_INDEX;
    }

    rc = md2_runtime_append_glcmd_vertex(mesh,
                                         (md2_u16)vtx.vertex_index,
                                         vtx.s,
                                         vtx.t,
                                         out_index);
    if (rc != MD2_OK)
    {
        return rc;
    }

    return MD2_OK;
}

static int md2_runtime_emit_strip(md2_glcmd_iter_t *it,
                                  md2_runtime_mesh_t *mesh,
                                  md2_i32 num_xyz,
                                  md2_i32 vertex_count)
{
    md2_u16 a;
    md2_u16 b;
    md2_u16 c;
    md2_u16 next;
    md2_i32 tri_index;
    int rc;

    if (vertex_count < 3)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    rc = md2_runtime_read_glcmd_runtime_index(it, mesh, num_xyz, &a);
    if (rc != MD2_OK) return rc;
    rc = md2_runtime_read_glcmd_runtime_index(it, mesh, num_xyz, &b);
    if (rc != MD2_OK) return rc;
    rc = md2_runtime_read_glcmd_runtime_index(it, mesh, num_xyz, &c);
    if (rc != MD2_OK) return rc;

    rc = md2_runtime_emit_triangle(mesh, a, b, c);
    if (rc != MD2_OK) return rc;

    for (tri_index = 1; tri_index < vertex_count - 2; ++tri_index)
    {
        rc = md2_runtime_read_glcmd_runtime_index(it, mesh, num_xyz, &next);
        if (rc != MD2_OK)
        {
            return rc;
        }

        if ((tri_index & 1) != 0)
        {
            rc = md2_runtime_emit_triangle(mesh, c, b, next);
        }
        else
        {
            rc = md2_runtime_emit_triangle(mesh, b, c, next);
        }
        if (rc != MD2_OK)
        {
            return rc;
        }

        a = b;
        b = c;
        c = next;
        (void)a;
    }

    return MD2_OK;
}

static int md2_runtime_emit_fan(md2_glcmd_iter_t *it,
                                md2_runtime_mesh_t *mesh,
                                md2_i32 num_xyz,
                                md2_i32 vertex_count)
{
    md2_u16 root;
    md2_u16 prev;
    md2_u16 cur;
    md2_u16 next;
    md2_i32 tri_index;
    int rc;

    if (vertex_count < 3)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    rc = md2_runtime_read_glcmd_runtime_index(it, mesh, num_xyz, &root);
    if (rc != MD2_OK) return rc;
    rc = md2_runtime_read_glcmd_runtime_index(it, mesh, num_xyz, &prev);
    if (rc != MD2_OK) return rc;
    rc = md2_runtime_read_glcmd_runtime_index(it, mesh, num_xyz, &cur);
    if (rc != MD2_OK) return rc;

    rc = md2_runtime_emit_triangle(mesh, root, prev, cur);
    if (rc != MD2_OK) return rc;

    for (tri_index = 1; tri_index < vertex_count - 2; ++tri_index)
    {
        rc = md2_runtime_read_glcmd_runtime_index(it, mesh, num_xyz, &next);
        if (rc != MD2_OK)
        {
            return rc;
        }

        rc = md2_runtime_emit_triangle(mesh, root, cur, next);
        if (rc != MD2_OK)
        {
            return rc;
        }

        prev = cur;
        cur = next;
        (void)prev;
    }

    return MD2_OK;
}

void md2_runtime_mesh_init(md2_runtime_mesh_t *mesh)
{
    if (mesh != 0)
    {
        memset(mesh, 0, sizeof(*mesh));
    }
}

int md2_runtime_build_triangle_mesh(const md2_model_t *model,
                                    md2_runtime_mesh_t *out_mesh)
{
    md2_i32 tri_index;
    md2_i32 corner;
    md2_u16 runtime_index;
    int rc;
    const md2_triangle_t *tri;

    if (model == 0 || out_mesh == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    rc = md2_validate_indices(model);
    if (rc != MD2_OK)
    {
        return rc;
    }

    md2_runtime_mesh_init(out_mesh);
    out_mesh->source_kind = MD2_RUNTIME_SOURCE_TRIANGLES;

    if (model->num_tris > MD2_RUNTIME_MAX_INDICES / 3)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    for (tri_index = 0; tri_index < model->num_tris; ++tri_index)
    {
        tri = &model->triangles[tri_index];
        for (corner = 0; corner < 3; ++corner)
        {
            rc = md2_runtime_append_triangle_vertex(model,
                                                    out_mesh,
                                                    tri->index_xyz[corner],
                                                    tri->index_st[corner],
                                                    &runtime_index);
            if (rc != MD2_OK)
            {
                return rc;
            }

            if (out_mesh->num_indices >= MD2_RUNTIME_MAX_INDICES)
            {
                return MD2_ERR_OVER_CAPACITY;
            }

            out_mesh->indices[out_mesh->num_indices] = runtime_index;
            ++out_mesh->num_indices;
        }
    }

    return MD2_OK;
}

int md2_runtime_build_glcmd_mesh(const md2_model_t *model,
                                 md2_runtime_mesh_t *out_mesh)
{
    md2_glcmd_iter_t it;
    md2_glcmd_cmd_t cmd;
    int rc;
    int got;

    if (model == 0 || out_mesh == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    if (model->num_glcmds <= 0 || model->glcmds[model->num_glcmds - 1] != 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    md2_runtime_mesh_init(out_mesh);
    out_mesh->source_kind = MD2_RUNTIME_SOURCE_GLCMDS;

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

        if (cmd.primitive == MD2_GLCMD_STRIP)
        {
            rc = md2_runtime_emit_strip(&it, out_mesh, model->num_xyz, cmd.vertex_count);
        }
        else if (cmd.primitive == MD2_GLCMD_FAN)
        {
            rc = md2_runtime_emit_fan(&it, out_mesh, model->num_xyz, cmd.vertex_count);
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

    return MD2_OK;
}


static int md2_runtime_stream_read_append(md2_glcmd_iter_t *it,
                                          md2_runtime_mesh_t *mesh,
                                          md2_i32 num_xyz,
                                          md2_u16 *out_index)
{
    md2_glcmd_vertex_t vtx;
    int got;
    int rc;

    if (it == 0 || mesh == 0 || out_index == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    got = md2_glcmd_iter_next_vertex(it, &vtx);
    if (got <= 0)
    {
        if (got == 0)
        {
            return MD2_ERR_BAD_GLCMD;
        }
        return got;
    }

    if (vtx.vertex_index < 0 || vtx.vertex_index >= num_xyz)
    {
        return MD2_ERR_BAD_INDEX;
    }

    rc = md2_runtime_append_glcmd_stream_vertex(mesh,
                                                (md2_u16)vtx.vertex_index,
                                                vtx.s,
                                                vtx.t,
                                                out_index);
    return rc;
}

static int md2_runtime_stream_read_block(md2_glcmd_iter_t *it,
                                         md2_runtime_mesh_t *mesh,
                                         md2_i32 num_xyz,
                                         md2_i32 vertex_count,
                                         md2_u16 *out_first)
{
    md2_i32 i;
    md2_u16 index_value;
    int rc;

    if (vertex_count < 3)
    {
        return MD2_ERR_BAD_GLCMD;
    }
    if (mesh->num_vertices + vertex_count > MD2_RUNTIME_MAX_VERTICES)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    *out_first = (md2_u16)mesh->num_vertices;

    for (i = 0; i < vertex_count; ++i)
    {
        rc = md2_runtime_stream_read_append(it, mesh, num_xyz, &index_value);
        if (rc != MD2_OK)
        {
            return rc;
        }
        (void)index_value;
    }

    return MD2_OK;
}

static int md2_runtime_emit_stream_strip(md2_runtime_mesh_t *mesh,
                                         md2_u16 first,
                                         md2_i32 vertex_count)
{
    md2_i32 i;
    md2_u16 a;
    md2_u16 b;
    md2_u16 c;
    int rc;

    for (i = 2; i < vertex_count; ++i)
    {
        a = (md2_u16)(first + i - 2);
        if ((i & 1) != 0)
        {
            b = (md2_u16)(first + i);
            c = (md2_u16)(first + i - 1);
        }
        else
        {
            b = (md2_u16)(first + i - 1);
            c = (md2_u16)(first + i);
        }

        rc = md2_runtime_emit_triangle(mesh, a, b, c);
        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    return MD2_OK;
}

static int md2_runtime_emit_stream_fan(md2_runtime_mesh_t *mesh,
                                       md2_u16 first,
                                       md2_i32 vertex_count)
{
    md2_i32 i;
    int rc;

    for (i = 2; i < vertex_count; ++i)
    {
        rc = md2_runtime_emit_triangle(mesh,
                                       first,
                                       (md2_u16)(first + i - 1),
                                       (md2_u16)(first + i));
        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    return MD2_OK;
}

int md2_runtime_build_glcmd_stream_mesh(const md2_model_t *model,
                                        md2_runtime_mesh_t *out_mesh)
{
    md2_glcmd_iter_t it;
    md2_glcmd_cmd_t cmd;
    md2_u16 first;
    int rc;
    int got;

    if (model == 0 || out_mesh == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (model->num_glcmds <= 0 || model->glcmds[model->num_glcmds - 1] != 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    md2_runtime_mesh_init(out_mesh);
    out_mesh->source_kind = MD2_RUNTIME_SOURCE_GLCMDS_STREAM;

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

        rc = md2_runtime_stream_read_block(&it,
                                           out_mesh,
                                           model->num_xyz,
                                           cmd.vertex_count,
                                           &first);
        if (rc != MD2_OK)
        {
            return rc;
        }

        if (cmd.primitive == MD2_GLCMD_STRIP)
        {
            rc = md2_runtime_emit_stream_strip(out_mesh, first, cmd.vertex_count);
        }
        else if (cmd.primitive == MD2_GLCMD_FAN)
        {
            rc = md2_runtime_emit_stream_fan(out_mesh, first, cmd.vertex_count);
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

    return MD2_OK;
}

int md2_runtime_pose_frame(const md2_model_t *model,
                           const md2_runtime_mesh_t *mesh,
                           md2_i32 frame_index,
                           md2_runtime_vertex_t *out_vertices)
{
    md2_i32 i;
    md2_fx_vec3_t pos;
    md2_u8 normal_index;
    int rc;
    md2_u16 source_xyz;

    if (model == 0 || mesh == 0 || out_vertices == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (frame_index < 0 || frame_index >= model->num_frames)
    {
        return MD2_ERR_BAD_INDEX;
    }

    for (i = 0; i < mesh->num_vertices; ++i)
    {
        source_xyz = mesh->vertices[i].index_xyz;
        rc = md2_anim_sample_frame_vertex(&model->frames[frame_index],
                                          model->num_xyz,
                                          (md2_i32)source_xyz,
                                          &pos,
                                          &normal_index);
        if (rc != MD2_OK)
        {
            return rc;
        }

        out_vertices[i].x = pos.x;
        out_vertices[i].y = pos.y;
        out_vertices[i].z = pos.z;
        out_vertices[i].u = mesh->vertices[i].u;
        out_vertices[i].v = mesh->vertices[i].v;
        out_vertices[i].lightnormalindex = normal_index;
        out_vertices[i].reserved0 = 0;
        out_vertices[i].reserved1 = 0;
        out_vertices[i].reserved2 = 0;
    }

    return MD2_OK;
}

int md2_runtime_pose_lerp(const md2_model_t *model,
                          const md2_runtime_mesh_t *mesh,
                          md2_i32 frame_a_index,
                          md2_i32 frame_b_index,
                          md2_fx32_t t,
                          md2_runtime_vertex_t *out_vertices)
{
    md2_i32 i;
    md2_fx_vec3_t pos;
    md2_u8 normal_index;
    int rc;
    md2_u16 source_xyz;

    if (model == 0 || mesh == 0 || out_vertices == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (frame_a_index < 0 || frame_a_index >= model->num_frames ||
        frame_b_index < 0 || frame_b_index >= model->num_frames)
    {
        return MD2_ERR_BAD_INDEX;
    }

    for (i = 0; i < mesh->num_vertices; ++i)
    {
        source_xyz = mesh->vertices[i].index_xyz;
        rc = md2_anim_lerp_vertex(&model->frames[frame_a_index],
                                  &model->frames[frame_b_index],
                                  model->num_xyz,
                                  (md2_i32)source_xyz,
                                  t,
                                  &pos,
                                  &normal_index);
        if (rc != MD2_OK)
        {
            return rc;
        }

        out_vertices[i].x = pos.x;
        out_vertices[i].y = pos.y;
        out_vertices[i].z = pos.z;
        out_vertices[i].u = mesh->vertices[i].u;
        out_vertices[i].v = mesh->vertices[i].v;
        out_vertices[i].lightnormalindex = normal_index;
        out_vertices[i].reserved0 = 0;
        out_vertices[i].reserved1 = 0;
        out_vertices[i].reserved2 = 0;
    }

    return MD2_OK;
}


static md2_fx32_t md2_runtime_mul_signed_unit(md2_fx32_t value,
                                              md2_fx32_t signed_unit)
{
    if (signed_unit < 0)
    {
        return (md2_fx32_t)(-md2_fx_mul_unit(value, (md2_fx32_t)(-signed_unit)));
    }
    return md2_fx_mul_unit(value, signed_unit);
}

static md2_fx32_t md2_runtime_axis_q2_lerp(const md2_frame_t *old_frame,
                                           const md2_frame_t *current_frame,
                                           md2_u16 source_xyz,
                                           md2_i32 axis,
                                           md2_fx32_t move,
                                           md2_fx32_t back_scale,
                                           md2_fx32_t front_scale)
{
    md2_i32 old_q;
    md2_i32 current_q;

    old_q = (md2_i32)old_frame->verts[source_xyz].v[axis];
    current_q = (md2_i32)current_frame->verts[source_xyz].v[axis];

    return (md2_fx32_t)(move + old_q * back_scale + current_q * front_scale);
}

int md2_runtime_pose_lerp_q2(const md2_model_t *model,
                             const md2_runtime_mesh_t *mesh,
                             md2_i32 old_frame_index,
                             md2_i32 current_frame_index,
                             md2_fx32_t backlerp,
                             const md2_fx_vec3_t *old_origin_minus_origin_model,
                             md2_runtime_vertex_t *out_vertices)
{
    return md2_runtime_pose_lerp_q2_ex(model,
                                       mesh,
                                       old_frame_index,
                                       current_frame_index,
                                       backlerp,
                                       old_origin_minus_origin_model,
                                       0,
                                       0,
                                       0,
                                       out_vertices);
}

int md2_runtime_pose_lerp_q2_ex(const md2_model_t *model,
                                const md2_runtime_mesh_t *mesh,
                                md2_i32 old_frame_index,
                                md2_i32 current_frame_index,
                                md2_fx32_t backlerp,
                                const md2_fx_vec3_t *old_origin_minus_origin_model,
                                const md2_fx_vec3_t *normal_table,
                                md2_i32 normal_table_count,
                                md2_fx32_t shell_scale,
                                md2_runtime_vertex_t *out_vertices)
{
    const md2_frame_t *old_frame;
    const md2_frame_t *current_frame;
    md2_fx_vec3_t delta;
    md2_fx32_t back;
    md2_fx32_t front;
    md2_fx32_t move[3];
    md2_fx32_t back_scale[3];
    md2_fx32_t front_scale[3];
    md2_i32 i;
    md2_i32 j;
    md2_u16 source_xyz;
    md2_u8 normal_index;

    if (model == 0 || mesh == 0 || out_vertices == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (old_frame_index < 0 || old_frame_index >= model->num_frames ||
        current_frame_index < 0 || current_frame_index >= model->num_frames)
    {
        return MD2_ERR_BAD_INDEX;
    }
    if (normal_table_count < 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    old_frame = &model->frames[old_frame_index];
    current_frame = &model->frames[current_frame_index];

    if (old_origin_minus_origin_model != 0)
    {
        delta = *old_origin_minus_origin_model;
    }
    else
    {
        delta.x = 0;
        delta.y = 0;
        delta.z = 0;
    }

    back = md2_fx_clamp_unit(backlerp);
    front = (md2_fx32_t)(MD2_FX_ONE - back);

    move[0] = (md2_fx32_t)(md2_fx_mul_unit((md2_fx32_t)(old_frame->translate[0] + delta.x), back) +
                           md2_fx_mul_unit(current_frame->translate[0], front));
    move[1] = (md2_fx32_t)(md2_fx_mul_unit((md2_fx32_t)(old_frame->translate[1] + delta.y), back) +
                           md2_fx_mul_unit(current_frame->translate[1], front));
    move[2] = (md2_fx32_t)(md2_fx_mul_unit((md2_fx32_t)(old_frame->translate[2] + delta.z), back) +
                           md2_fx_mul_unit(current_frame->translate[2], front));

    for (j = 0; j < 3; ++j)
    {
        back_scale[j] = md2_fx_mul_unit(old_frame->scale[j], back);
        front_scale[j] = md2_fx_mul_unit(current_frame->scale[j], front);
    }

    for (i = 0; i < mesh->num_vertices; ++i)
    {
        source_xyz = mesh->vertices[i].index_xyz;
        if ((md2_i32)source_xyz >= model->num_xyz)
        {
            return MD2_ERR_BAD_INDEX;
        }

        normal_index = current_frame->verts[source_xyz].lightnormalindex;

        out_vertices[i].x = md2_runtime_axis_q2_lerp(old_frame,
                                                     current_frame,
                                                     source_xyz,
                                                     0,
                                                     move[0],
                                                     back_scale[0],
                                                     front_scale[0]);
        out_vertices[i].y = md2_runtime_axis_q2_lerp(old_frame,
                                                     current_frame,
                                                     source_xyz,
                                                     1,
                                                     move[1],
                                                     back_scale[1],
                                                     front_scale[1]);
        out_vertices[i].z = md2_runtime_axis_q2_lerp(old_frame,
                                                     current_frame,
                                                     source_xyz,
                                                     2,
                                                     move[2],
                                                     back_scale[2],
                                                     front_scale[2]);

        if (normal_table != 0 && shell_scale != 0)
        {
            if ((md2_i32)normal_index >= normal_table_count)
            {
                return MD2_ERR_BAD_INDEX;
            }
            out_vertices[i].x = (md2_fx32_t)(out_vertices[i].x +
                md2_runtime_mul_signed_unit(shell_scale, normal_table[normal_index].x));
            out_vertices[i].y = (md2_fx32_t)(out_vertices[i].y +
                md2_runtime_mul_signed_unit(shell_scale, normal_table[normal_index].y));
            out_vertices[i].z = (md2_fx32_t)(out_vertices[i].z +
                md2_runtime_mul_signed_unit(shell_scale, normal_table[normal_index].z));
        }

        out_vertices[i].u = mesh->vertices[i].u;
        out_vertices[i].v = mesh->vertices[i].v;
        out_vertices[i].lightnormalindex = normal_index;
        out_vertices[i].reserved0 = 0;
        out_vertices[i].reserved1 = 0;
        out_vertices[i].reserved2 = 0;
    }

    return MD2_OK;
}
