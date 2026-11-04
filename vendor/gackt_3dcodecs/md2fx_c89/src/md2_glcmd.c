#include "md2/md2_error.h"
#include "md2/md2_fixed.h"
#include "md2/md2_glcmd.h"

void md2_glcmd_iter_init(md2_glcmd_iter_t *it, const md2_model_t *model)
{
    if (it != 0)
    {
        it->model = model;
        it->pos = 0;
        it->remaining = 0;
        it->primitive = 0;
    }
}

int md2_glcmd_iter_next(md2_glcmd_iter_t *it, md2_glcmd_cmd_t *out_cmd)
{
    md2_i32 count;

    if (it == 0 || out_cmd == 0 || it->model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (it->remaining != 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }
    if (it->pos >= it->model->num_glcmds)
    {
        return 0;
    }

    count = it->model->glcmds[it->pos++];
    if (count == 0)
    {
        return 0;
    }

    if (count > 0)
    {
        it->primitive = MD2_GLCMD_STRIP;
        it->remaining = count;
    }
    else
    {
        it->primitive = MD2_GLCMD_FAN;
        it->remaining = -count;
    }

    if (it->remaining < 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    if (it->pos + it->remaining * 3 > it->model->num_glcmds)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    out_cmd->primitive = it->primitive;
    out_cmd->vertex_count = it->remaining;
    return 1;
}

int md2_glcmd_iter_next_vertex(md2_glcmd_iter_t *it, md2_glcmd_vertex_t *out_vtx)
{
    md2_u32 bits;
    int rc;

    if (it == 0 || out_vtx == 0 || it->model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (it->remaining <= 0)
    {
        return 0;
    }
    if (it->pos + 3 > it->model->num_glcmds)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    bits = (md2_u32)it->model->glcmds[it->pos++];
    rc = md2_fx_from_f32_bits(bits, &out_vtx->s);
    if (rc != MD2_OK) return rc;

    bits = (md2_u32)it->model->glcmds[it->pos++];
    rc = md2_fx_from_f32_bits(bits, &out_vtx->t);
    if (rc != MD2_OK) return rc;

    out_vtx->vertex_index = it->model->glcmds[it->pos++];
    --it->remaining;

    return 1;
}

void md2_glcmd_builder_init(md2_glcmd_builder_t *builder, md2_model_t *model)
{
    if (builder != 0)
    {
        builder->model = model;
        builder->pos = 0;
        builder->remaining = 0;
        builder->open_primitive = 0;
    }
    if (model != 0)
    {
        model->num_glcmds = 0;
    }
}

int md2_glcmd_builder_begin(md2_glcmd_builder_t *builder,
                            md2_i32 primitive,
                            md2_i32 vertex_count)
{
    md2_i32 signed_count;

    if (builder == 0 || builder->model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (builder->remaining != 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }
    if (primitive != MD2_GLCMD_STRIP && primitive != MD2_GLCMD_FAN)
    {
        return MD2_ERR_BAD_GLCMD;
    }
    if (vertex_count <= 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }
    if (builder->pos + 1 + vertex_count * 3 + 1 > MD2_MAX_GLCMDS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    signed_count = (primitive == MD2_GLCMD_STRIP) ? vertex_count : -vertex_count;
    builder->model->glcmds[builder->pos++] = signed_count;
    builder->remaining = vertex_count;
    builder->open_primitive = primitive;
    builder->model->num_glcmds = builder->pos;
    return MD2_OK;
}

int md2_glcmd_builder_vertex(md2_glcmd_builder_t *builder,
                             md2_fx32_t s,
                             md2_fx32_t t,
                             md2_i32 vertex_index)
{
    md2_u32 s_bits;
    md2_u32 t_bits;
    int rc;

    if (builder == 0 || builder->model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (builder->remaining <= 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }

    rc = md2_fx_to_f32_bits(s, &s_bits);
    if (rc != MD2_OK) return rc;
    rc = md2_fx_to_f32_bits(t, &t_bits);
    if (rc != MD2_OK) return rc;

    builder->model->glcmds[builder->pos++] = (md2_i32)s_bits;
    builder->model->glcmds[builder->pos++] = (md2_i32)t_bits;
    builder->model->glcmds[builder->pos++] = vertex_index;
    --builder->remaining;
    builder->model->num_glcmds = builder->pos;
    return MD2_OK;
}

int md2_glcmd_builder_end(md2_glcmd_builder_t *builder)
{
    if (builder == 0 || builder->model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (builder->remaining != 0)
    {
        return MD2_ERR_BAD_GLCMD;
    }
    if (builder->pos >= MD2_MAX_GLCMDS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    builder->model->glcmds[builder->pos++] = 0;
    builder->model->num_glcmds = builder->pos;
    builder->open_primitive = 0;
    return MD2_OK;
}
