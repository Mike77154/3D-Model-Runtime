#include "md2/md2_anim.h"
#include "md2/md2_error.h"
#include "md2/md2_gpu.h"

static void md2_gpu_write_u16_le(md2_u8 *dst, md2_u16 value)
{
    dst[0] = (md2_u8)(value & 0xFFU);
    dst[1] = (md2_u8)((value >> 8) & 0xFFU);
}

static int md2_gpu_stream_ok(const md2_gpu_stream_t *stream, md2_i32 min_stride)
{
    if (stream == 0)
    {
        return 0;
    }
    if (stream->data == 0)
    {
        return 0;
    }
    if (stream->stride < min_stride)
    {
        return 0;
    }
    return 1;
}

static int md2_gpu_write_vertex_aos16(const md2_runtime_vertex_t *v,
                                      const md2_gpu_quant_desc_t *desc,
                                      md2_u8 *dst)
{
    md2_u16 px;
    md2_u16 py;
    md2_u16 pz;
    md2_u16 pu;
    md2_u16 pv;

    if (v == 0 || desc == 0 || dst == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    px = md2_gpu_pack_pos16(v->x, desc->translate.x, desc->scale.x);
    py = md2_gpu_pack_pos16(v->y, desc->translate.y, desc->scale.y);
    pz = md2_gpu_pack_pos16(v->z, desc->translate.z, desc->scale.z);
    pu = md2_gpu_pack_unorm16(v->u);
    pv = md2_gpu_pack_unorm16(v->v);

    md2_gpu_write_u16_le(dst + 0, px);
    md2_gpu_write_u16_le(dst + 2, py);
    md2_gpu_write_u16_le(dst + 4, pz);
    md2_gpu_write_u16_le(dst + 6, pu);
    md2_gpu_write_u16_le(dst + 8, pv);
    dst[10] = v->lightnormalindex;
    dst[11] = 0;

    return MD2_OK;
}

static int md2_gpu_write_vertex_soa16(const md2_runtime_vertex_t *v,
                                      const md2_gpu_quant_desc_t *desc,
                                      const md2_gpu_soa16_streams_t *streams,
                                      md2_i32 index)
{
    md2_u16 px;
    md2_u16 py;
    md2_u16 pz;
    md2_u16 pu;
    md2_u16 pv;
    md2_u8 *dst;

    if (v == 0 || desc == 0 || streams == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    px = md2_gpu_pack_pos16(v->x, desc->translate.x, desc->scale.x);
    py = md2_gpu_pack_pos16(v->y, desc->translate.y, desc->scale.y);
    pz = md2_gpu_pack_pos16(v->z, desc->translate.z, desc->scale.z);
    pu = md2_gpu_pack_unorm16(v->u);
    pv = md2_gpu_pack_unorm16(v->v);

    dst = streams->x.data + index * streams->x.stride;
    md2_gpu_write_u16_le(dst, px);
    dst = streams->y.data + index * streams->y.stride;
    md2_gpu_write_u16_le(dst, py);
    dst = streams->z.data + index * streams->z.stride;
    md2_gpu_write_u16_le(dst, pz);
    dst = streams->u.data + index * streams->u.stride;
    md2_gpu_write_u16_le(dst, pu);
    dst = streams->v.data + index * streams->v.stride;
    md2_gpu_write_u16_le(dst, pv);
    streams->normal_index.data[index * streams->normal_index.stride] = v->lightnormalindex;

    return MD2_OK;
}

void md2_gpu_bounds_init(md2_gpu_bounds_t *bounds)
{
    if (bounds != 0)
    {
        bounds->min.x = 0;
        bounds->min.y = 0;
        bounds->min.z = 0;
        bounds->max.x = 0;
        bounds->max.y = 0;
        bounds->max.z = 0;
    }
}

int md2_gpu_compute_pose_bounds(const md2_runtime_vertex_t *vertices,
                                md2_i32 num_vertices,
                                md2_gpu_bounds_t *out_bounds)
{
    md2_i32 i;

    if (vertices == 0 || out_bounds == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (num_vertices <= 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    out_bounds->min.x = out_bounds->max.x = vertices[0].x;
    out_bounds->min.y = out_bounds->max.y = vertices[0].y;
    out_bounds->min.z = out_bounds->max.z = vertices[0].z;

    for (i = 1; i < num_vertices; ++i)
    {
        if (vertices[i].x < out_bounds->min.x) out_bounds->min.x = vertices[i].x;
        if (vertices[i].y < out_bounds->min.y) out_bounds->min.y = vertices[i].y;
        if (vertices[i].z < out_bounds->min.z) out_bounds->min.z = vertices[i].z;
        if (vertices[i].x > out_bounds->max.x) out_bounds->max.x = vertices[i].x;
        if (vertices[i].y > out_bounds->max.y) out_bounds->max.y = vertices[i].y;
        if (vertices[i].z > out_bounds->max.z) out_bounds->max.z = vertices[i].z;
    }

    return MD2_OK;
}

int md2_gpu_compute_runtime_bounds(const md2_model_t *model,
                                   const md2_runtime_mesh_t *mesh,
                                   md2_gpu_bounds_t *out_bounds)
{
    md2_i32 frame_index;
    md2_i32 vertex_index;
    md2_fx_vec3_t pos;
    int rc;
    int first;

    if (model == 0 || mesh == 0 || out_bounds == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (mesh->num_vertices <= 0 || model->num_frames <= 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    first = 1;
    for (frame_index = 0; frame_index < model->num_frames; ++frame_index)
    {
        for (vertex_index = 0; vertex_index < mesh->num_vertices; ++vertex_index)
        {
            rc = md2_anim_sample_frame_vertex(&model->frames[frame_index],
                                              model->num_xyz,
                                              (md2_i32)mesh->vertices[vertex_index].index_xyz,
                                              &pos,
                                              0);
            if (rc != MD2_OK)
            {
                return rc;
            }

            if (first)
            {
                out_bounds->min.x = out_bounds->max.x = pos.x;
                out_bounds->min.y = out_bounds->max.y = pos.y;
                out_bounds->min.z = out_bounds->max.z = pos.z;
                first = 0;
            }
            else
            {
                if (pos.x < out_bounds->min.x) out_bounds->min.x = pos.x;
                if (pos.y < out_bounds->min.y) out_bounds->min.y = pos.y;
                if (pos.z < out_bounds->min.z) out_bounds->min.z = pos.z;
                if (pos.x > out_bounds->max.x) out_bounds->max.x = pos.x;
                if (pos.y > out_bounds->max.y) out_bounds->max.y = pos.y;
                if (pos.z > out_bounds->max.z) out_bounds->max.z = pos.z;
            }
        }
    }

    if (first)
    {
        return MD2_ERR_BAD_ARG;
    }

    return MD2_OK;
}

int md2_gpu_make_quant_desc(const md2_gpu_bounds_t *bounds,
                            md2_gpu_quant_desc_t *out_desc)
{
    md2_fx32_t range;

    if (bounds == 0 || out_desc == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    out_desc->translate = bounds->min;

    range = (md2_fx32_t)(bounds->max.x - bounds->min.x);
    out_desc->scale.x = (range > 0) ? md2_fx_ceil_div_i32(range, 65535) : 1;

    range = (md2_fx32_t)(bounds->max.y - bounds->min.y);
    out_desc->scale.y = (range > 0) ? md2_fx_ceil_div_i32(range, 65535) : 1;

    range = (md2_fx32_t)(bounds->max.z - bounds->min.z);
    out_desc->scale.z = (range > 0) ? md2_fx_ceil_div_i32(range, 65535) : 1;

    if (out_desc->scale.x <= 0) out_desc->scale.x = 1;
    if (out_desc->scale.y <= 0) out_desc->scale.y = 1;
    if (out_desc->scale.z <= 0) out_desc->scale.z = 1;

    return MD2_OK;
}

md2_u16 md2_gpu_pack_unorm16(md2_fx32_t value)
{
    md2_u32 clamped;
    md2_u32 product;
    md2_u32 rounded;

    clamped = (md2_u32)md2_fx_clamp_unit(value);
    product = clamped * 65535U;
    rounded = product + 0x8000U;
    return (md2_u16)(rounded >> 16);
}

md2_fx32_t md2_gpu_unpack_unorm16(md2_u16 value)
{
    md2_u32 numer;
    md2_u32 rounded;

    numer = ((md2_u32)value << 16);
    rounded = numer + 32767U;
    return (md2_fx32_t)(rounded / 65535U);
}

md2_u16 md2_gpu_pack_pos16(md2_fx32_t value,
                           md2_fx32_t translate,
                           md2_fx32_t scale)
{
    md2_fx32_t rel;
    md2_i32 q;

    if (scale <= 0)
    {
        return 0;
    }

    rel = (md2_fx32_t)(value - translate);
    if (rel <= 0)
    {
        return 0;
    }

    q = (md2_i32)((rel + (scale >> 1)) / scale);

    if (q < 0)
    {
        q = 0;
    }
    if (q > 65535)
    {
        q = 65535;
    }

    return (md2_u16)q;
}

md2_fx32_t md2_gpu_unpack_pos16(md2_u16 value,
                                md2_fx32_t translate,
                                md2_fx32_t scale)
{
    return (md2_fx32_t)(translate + (md2_i32)value * scale);
}

int md2_gpu_export_aos16(const md2_runtime_vertex_t *vertices,
                         md2_i32 num_vertices,
                         const md2_gpu_quant_desc_t *desc,
                         md2_u8 *out_bytes,
                         md2_i32 out_stride)
{
    md2_i32 i;
    int rc;

    if (vertices == 0 || desc == 0 || out_bytes == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (num_vertices < 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (out_stride < MD2_GPU_AOS16_STRIDE)
    {
        return MD2_ERR_BAD_STRIDE;
    }

    for (i = 0; i < num_vertices; ++i)
    {
        rc = md2_gpu_write_vertex_aos16(&vertices[i],
                                        desc,
                                        out_bytes + i * out_stride);
        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    return MD2_OK;
}

int md2_gpu_export_soa16(const md2_runtime_vertex_t *vertices,
                         md2_i32 num_vertices,
                         const md2_gpu_quant_desc_t *desc,
                         const md2_gpu_soa16_streams_t *out_streams)
{
    md2_i32 i;
    int rc;

    if (vertices == 0 || desc == 0 || out_streams == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (num_vertices < 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (!md2_gpu_stream_ok(&out_streams->x, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->y, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->z, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->u, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->v, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->normal_index, MD2_GPU_SOA16_U8_STRIDE))
    {
        return MD2_ERR_BAD_STRIDE;
    }

    for (i = 0; i < num_vertices; ++i)
    {
        rc = md2_gpu_write_vertex_soa16(&vertices[i], desc, out_streams, i);
        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    return MD2_OK;
}

int md2_gpu_pose_frame_aos16(const md2_model_t *model,
                             const md2_runtime_mesh_t *mesh,
                             md2_i32 frame_index,
                             const md2_gpu_quant_desc_t *desc,
                             md2_u8 *out_bytes,
                             md2_i32 out_stride)
{
    md2_i32 i;
    md2_runtime_vertex_t v;
    md2_fx_vec3_t pos;
    md2_u8 normal_index;
    int rc;

    if (model == 0 || mesh == 0 || desc == 0 || out_bytes == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (frame_index < 0 || frame_index >= model->num_frames)
    {
        return MD2_ERR_BAD_INDEX;
    }
    if (out_stride < MD2_GPU_AOS16_STRIDE)
    {
        return MD2_ERR_BAD_STRIDE;
    }

    for (i = 0; i < mesh->num_vertices; ++i)
    {
        rc = md2_anim_sample_frame_vertex(&model->frames[frame_index],
                                          model->num_xyz,
                                          (md2_i32)mesh->vertices[i].index_xyz,
                                          &pos,
                                          &normal_index);
        if (rc != MD2_OK)
        {
            return rc;
        }

        v.x = pos.x;
        v.y = pos.y;
        v.z = pos.z;
        v.u = mesh->vertices[i].u;
        v.v = mesh->vertices[i].v;
        v.lightnormalindex = normal_index;
        v.reserved0 = 0;
        v.reserved1 = 0;
        v.reserved2 = 0;

        rc = md2_gpu_write_vertex_aos16(&v, desc, out_bytes + i * out_stride);
        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    return MD2_OK;
}

int md2_gpu_pose_lerp_aos16(const md2_model_t *model,
                            const md2_runtime_mesh_t *mesh,
                            md2_i32 frame_a_index,
                            md2_i32 frame_b_index,
                            md2_fx32_t t,
                            const md2_gpu_quant_desc_t *desc,
                            md2_u8 *out_bytes,
                            md2_i32 out_stride)
{
    md2_i32 i;
    md2_runtime_vertex_t v;
    md2_fx_vec3_t pos;
    md2_u8 normal_index;
    int rc;

    if (model == 0 || mesh == 0 || desc == 0 || out_bytes == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (frame_a_index < 0 || frame_a_index >= model->num_frames ||
        frame_b_index < 0 || frame_b_index >= model->num_frames)
    {
        return MD2_ERR_BAD_INDEX;
    }
    if (out_stride < MD2_GPU_AOS16_STRIDE)
    {
        return MD2_ERR_BAD_STRIDE;
    }

    for (i = 0; i < mesh->num_vertices; ++i)
    {
        rc = md2_anim_lerp_vertex(&model->frames[frame_a_index],
                                  &model->frames[frame_b_index],
                                  model->num_xyz,
                                  (md2_i32)mesh->vertices[i].index_xyz,
                                  t,
                                  &pos,
                                  &normal_index);
        if (rc != MD2_OK)
        {
            return rc;
        }

        v.x = pos.x;
        v.y = pos.y;
        v.z = pos.z;
        v.u = mesh->vertices[i].u;
        v.v = mesh->vertices[i].v;
        v.lightnormalindex = normal_index;
        v.reserved0 = 0;
        v.reserved1 = 0;
        v.reserved2 = 0;

        rc = md2_gpu_write_vertex_aos16(&v, desc, out_bytes + i * out_stride);
        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    return MD2_OK;
}

int md2_gpu_pose_frame_soa16(const md2_model_t *model,
                             const md2_runtime_mesh_t *mesh,
                             md2_i32 frame_index,
                             const md2_gpu_quant_desc_t *desc,
                             const md2_gpu_soa16_streams_t *out_streams)
{
    md2_i32 i;
    md2_runtime_vertex_t v;
    md2_fx_vec3_t pos;
    md2_u8 normal_index;
    int rc;

    if (model == 0 || mesh == 0 || desc == 0 || out_streams == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (frame_index < 0 || frame_index >= model->num_frames)
    {
        return MD2_ERR_BAD_INDEX;
    }
    if (!md2_gpu_stream_ok(&out_streams->x, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->y, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->z, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->u, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->v, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->normal_index, MD2_GPU_SOA16_U8_STRIDE))
    {
        return MD2_ERR_BAD_STRIDE;
    }

    for (i = 0; i < mesh->num_vertices; ++i)
    {
        rc = md2_anim_sample_frame_vertex(&model->frames[frame_index],
                                          model->num_xyz,
                                          (md2_i32)mesh->vertices[i].index_xyz,
                                          &pos,
                                          &normal_index);
        if (rc != MD2_OK)
        {
            return rc;
        }

        v.x = pos.x;
        v.y = pos.y;
        v.z = pos.z;
        v.u = mesh->vertices[i].u;
        v.v = mesh->vertices[i].v;
        v.lightnormalindex = normal_index;
        v.reserved0 = 0;
        v.reserved1 = 0;
        v.reserved2 = 0;

        rc = md2_gpu_write_vertex_soa16(&v, desc, out_streams, i);
        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    return MD2_OK;
}

int md2_gpu_pose_lerp_soa16(const md2_model_t *model,
                            const md2_runtime_mesh_t *mesh,
                            md2_i32 frame_a_index,
                            md2_i32 frame_b_index,
                            md2_fx32_t t,
                            const md2_gpu_quant_desc_t *desc,
                            const md2_gpu_soa16_streams_t *out_streams)
{
    md2_i32 i;
    md2_runtime_vertex_t v;
    md2_fx_vec3_t pos;
    md2_u8 normal_index;
    int rc;

    if (model == 0 || mesh == 0 || desc == 0 || out_streams == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (frame_a_index < 0 || frame_a_index >= model->num_frames ||
        frame_b_index < 0 || frame_b_index >= model->num_frames)
    {
        return MD2_ERR_BAD_INDEX;
    }
    if (!md2_gpu_stream_ok(&out_streams->x, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->y, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->z, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->u, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->v, MD2_GPU_SOA16_U16_STRIDE) ||
        !md2_gpu_stream_ok(&out_streams->normal_index, MD2_GPU_SOA16_U8_STRIDE))
    {
        return MD2_ERR_BAD_STRIDE;
    }

    for (i = 0; i < mesh->num_vertices; ++i)
    {
        rc = md2_anim_lerp_vertex(&model->frames[frame_a_index],
                                  &model->frames[frame_b_index],
                                  model->num_xyz,
                                  (md2_i32)mesh->vertices[i].index_xyz,
                                  t,
                                  &pos,
                                  &normal_index);
        if (rc != MD2_OK)
        {
            return rc;
        }

        v.x = pos.x;
        v.y = pos.y;
        v.z = pos.z;
        v.u = mesh->vertices[i].u;
        v.v = mesh->vertices[i].v;
        v.lightnormalindex = normal_index;
        v.reserved0 = 0;
        v.reserved1 = 0;
        v.reserved2 = 0;

        rc = md2_gpu_write_vertex_soa16(&v, desc, out_streams, i);
        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    return MD2_OK;
}
