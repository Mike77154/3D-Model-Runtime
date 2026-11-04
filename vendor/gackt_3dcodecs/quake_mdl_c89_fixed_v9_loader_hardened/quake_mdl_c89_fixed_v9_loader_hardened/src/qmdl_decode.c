
#include "qmdl_internal.h"


static int qmdl_u32_has_sign_bit(qmdl_u32 value)
{
    return (value & 0x80000000u) != 0u;
}

static int qmdl_header_shape_is_quake_parseable(const qmdl_header *header)
{
    if (header == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (qmdl_u32_has_sign_bit(header->numskins) ||
        qmdl_u32_has_sign_bit(header->skinwidth) ||
        qmdl_u32_has_sign_bit(header->skinheight) ||
        qmdl_u32_has_sign_bit(header->numverts) ||
        qmdl_u32_has_sign_bit(header->numtris) ||
        qmdl_u32_has_sign_bit(header->numframes))
    {
        return QMDL_ERR_FORMAT;
    }
    if (header->numskins == 0u ||
        header->skinwidth == 0u ||
        header->skinheight == 0u ||
        header->numverts == 0u ||
        header->numtris == 0u ||
        header->numframes == 0u)
    {
        return QMDL_ERR_FORMAT;
    }
    return QMDL_OK;
}

static int qmdl_read_trivertx(qmdl_cursor *cur, qmdl_trivertx *out_value)
{
    int rc;
    qmdl_u8 tmp;

    if (cur == 0 || out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_read_u8(cur, &tmp);
    if (rc != QMDL_OK) return rc;
    out_value->v[0] = tmp;

    rc = qmdl_read_u8(cur, &tmp);
    if (rc != QMDL_OK) return rc;
    out_value->v[1] = tmp;

    rc = qmdl_read_u8(cur, &tmp);
    if (rc != QMDL_OK) return rc;
    out_value->v[2] = tmp;

    rc = qmdl_read_u8(cur, &tmp);
    if (rc != QMDL_OK) return rc;
    out_value->lightnormalindex = tmp;

    return QMDL_OK;
}

static int qmdl_read_simple_subframe(qmdl_cursor *cur,
                                     qmdl_u32 numverts,
                                     qmdl_subframe *subframe,
                                     qmdl_trivertx *packed_verts,
                                     qmdl_u32 first_vert)
{
    qmdl_u32 i;
    int rc;

    rc = qmdl_read_trivertx(cur, &subframe->bboxmin);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_read_trivertx(cur, &subframe->bboxmax);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_read_bytes(cur, subframe->name, 16u);
    if (rc != QMDL_OK) return rc;

    subframe->first_vert = first_vert;

    for (i = 0; i < numverts; ++i)
    {
        rc = qmdl_read_trivertx(cur, &packed_verts[first_vert + i]);
        if (rc != QMDL_OK) return rc;
    }

    return QMDL_OK;
}

static int qmdl_parse_header(qmdl_cursor *cur, qmdl_header *header)
{
    int rc;

    if (cur == 0 || header == 0)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_read_u32_le(cur, &header->ident);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_u32_le(cur, &header->version);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_read_fx16_f32_le(cur, &header->scale[0]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_fx16_f32_le(cur, &header->scale[1]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_fx16_f32_le(cur, &header->scale[2]);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_read_fx16_f32_le(cur, &header->origin[0]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_fx16_f32_le(cur, &header->origin[1]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_fx16_f32_le(cur, &header->origin[2]);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_read_fx16_f32_le(cur, &header->bounding_radius);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_read_fx16_f32_le(cur, &header->eye_position[0]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_fx16_f32_le(cur, &header->eye_position[1]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_fx16_f32_le(cur, &header->eye_position[2]);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_read_u32_le(cur, &header->numskins);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_u32_le(cur, &header->skinwidth);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_u32_le(cur, &header->skinheight);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_u32_le(cur, &header->numverts);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_u32_le(cur, &header->numtris);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_u32_le(cur, &header->numframes);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_u32_le(cur, &header->synctype);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_u32_le(cur, &header->flags);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_fx16_f32_le(cur, &header->size);
    if (rc != QMDL_OK) return rc;

    if (header->ident != QMDL_IDENT || header->version != QMDL_VERSION)
    {
        return QMDL_ERR_FORMAT;
    }

    return qmdl_header_shape_is_quake_parseable(header);
}

static int qmdl_measure_after_header(qmdl_cursor *cur,
                                     const qmdl_header *header,
                                     qmdl_requirements *requirements)
{
    qmdl_u32 i;
    qmdl_u32 j;
    qmdl_u32 skin_pixels_per_image;
    qmdl_u32 group_count;
    qmdl_u32 group_pixel_count;
    qmdl_u32 type;
    qmdl_u8 tmp_bytes[16];
    qmdl_trivertx tmp_vert;
    size_t per_image_size;
    size_t block_size;
    int rc;

    requirements->skins = header->numskins;
    requirements->stverts = header->numverts;
    requirements->triangles = header->numtris;
    requirements->frames = header->numframes;
    requirements->encoded_bytes = 84u;

    rc = qmdl_u32_mul(header->skinwidth, header->skinheight, &skin_pixels_per_image);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    rc = qmdl_size_mul((size_t)header->skinwidth, (size_t)header->skinheight, &per_image_size);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    for (i = 0; i < header->numskins; ++i)
    {
        rc = qmdl_read_u32_le(cur, &type);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_size_add(&requirements->encoded_bytes, 4u);
        if (rc != QMDL_OK) return rc;

        if (type == QMDL_SKIN_SINGLE)
        {
            if ((size_t)(cur->end - cur->ptr) < per_image_size)
            {
                return QMDL_ERR_SIZE;
            }
            cur->ptr += per_image_size;
            rc = qmdl_u32_add(&requirements->skin_pixels, skin_pixels_per_image);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add(&requirements->encoded_bytes, per_image_size);
            if (rc != QMDL_OK) return rc;
        }
        else
        {
            rc = qmdl_read_u32_le(cur, &group_count);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add(&requirements->encoded_bytes, 4u);
            if (rc != QMDL_OK) return rc;
            if (qmdl_u32_has_sign_bit(group_count) || group_count == 0u)
            {
                return QMDL_ERR_FORMAT;
            }

            rc = qmdl_u32_add(&requirements->skin_intervals, group_count);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add_mul(&requirements->encoded_bytes, (size_t)group_count, 4u);
            if (rc != QMDL_OK) return rc;

            rc = qmdl_size_mul((size_t)group_count, 4u, &block_size);
            if (rc != QMDL_OK) return rc;
            if ((size_t)(cur->end - cur->ptr) < block_size)
            {
                return QMDL_ERR_SIZE;
            }
            cur->ptr += block_size;

            rc = qmdl_u32_mul(group_count, skin_pixels_per_image, &group_pixel_count);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_u32_add(&requirements->skin_pixels, group_pixel_count);
            if (rc != QMDL_OK) return rc;

            rc = qmdl_size_add_mul(&requirements->encoded_bytes,
                                   (size_t)group_count,
                                   per_image_size);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_mul((size_t)group_count, per_image_size, &block_size);
            if (rc != QMDL_OK) return rc;
            if ((size_t)(cur->end - cur->ptr) < block_size)
            {
                return QMDL_ERR_SIZE;
            }
            cur->ptr += block_size;
        }
    }

    rc = qmdl_size_add_mul(&requirements->encoded_bytes, (size_t)header->numverts, 12u);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_size_mul((size_t)header->numverts, 12u, &block_size);
    if (rc != QMDL_OK) return rc;
    if ((size_t)(cur->end - cur->ptr) < block_size)
    {
        return QMDL_ERR_SIZE;
    }
    cur->ptr += block_size;

    rc = qmdl_size_add_mul(&requirements->encoded_bytes, (size_t)header->numtris, 16u);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_size_mul((size_t)header->numtris, 16u, &block_size);
    if (rc != QMDL_OK) return rc;
    if ((size_t)(cur->end - cur->ptr) < block_size)
    {
        return QMDL_ERR_SIZE;
    }
    cur->ptr += block_size;

    for (i = 0; i < header->numframes; ++i)
    {
        rc = qmdl_read_u32_le(cur, &type);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_size_add(&requirements->encoded_bytes, 4u);
        if (rc != QMDL_OK) return rc;

        if (type == QMDL_FRAME_SINGLE)
        {
            rc = qmdl_read_trivertx(cur, &tmp_vert);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_read_trivertx(cur, &tmp_vert);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_read_bytes(cur, tmp_bytes, 16u);
            if (rc != QMDL_OK) return rc;

            rc = qmdl_size_add(&requirements->encoded_bytes, 24u);
            if (rc != QMDL_OK) return rc;

            rc = qmdl_u32_add(&requirements->subframes, 1u);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_u32_add(&requirements->packed_verts, header->numverts);
            if (rc != QMDL_OK) return rc;

            rc = qmdl_size_add_mul(&requirements->encoded_bytes, (size_t)header->numverts, 4u);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_mul((size_t)header->numverts, 4u, &block_size);
            if (rc != QMDL_OK) return rc;
            if ((size_t)(cur->end - cur->ptr) < block_size)
            {
                return QMDL_ERR_SIZE;
            }
            cur->ptr += block_size;
        }
        else
        {
            rc = qmdl_read_u32_le(cur, &group_count);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_read_trivertx(cur, &tmp_vert);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_read_trivertx(cur, &tmp_vert);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add(&requirements->encoded_bytes, 12u);
            if (rc != QMDL_OK) return rc;

            if (qmdl_u32_has_sign_bit(group_count) || group_count == 0u)
            {
                return QMDL_ERR_FORMAT;
            }

            rc = qmdl_u32_add(&requirements->frame_intervals, group_count);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_u32_add(&requirements->subframes, group_count);
            if (rc != QMDL_OK) return rc;

            rc = qmdl_u32_mul(group_count, header->numverts, &j);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_u32_add(&requirements->packed_verts, j);
            if (rc != QMDL_OK) return rc;

            rc = qmdl_size_add_mul(&requirements->encoded_bytes, (size_t)group_count, 4u);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_mul((size_t)group_count, 4u, &block_size);
            if (rc != QMDL_OK) return rc;
            if ((size_t)(cur->end - cur->ptr) < block_size)
            {
                return QMDL_ERR_SIZE;
            }
            cur->ptr += block_size;

            for (j = 0; j < group_count; ++j)
            {
                rc = qmdl_read_trivertx(cur, &tmp_vert);
                if (rc != QMDL_OK) return rc;
                rc = qmdl_read_trivertx(cur, &tmp_vert);
                if (rc != QMDL_OK) return rc;
                rc = qmdl_read_bytes(cur, tmp_bytes, 16u);
                if (rc != QMDL_OK) return rc;
                rc = qmdl_size_add(&requirements->encoded_bytes, 24u);
                if (rc != QMDL_OK) return rc;

                rc = qmdl_size_add_mul(&requirements->encoded_bytes, (size_t)header->numverts, 4u);
                if (rc != QMDL_OK) return rc;
                rc = qmdl_size_mul((size_t)header->numverts, 4u, &block_size);
                if (rc != QMDL_OK) return rc;
                if ((size_t)(cur->end - cur->ptr) < block_size)
                {
                    return QMDL_ERR_SIZE;
                }
                cur->ptr += block_size;
            }
        }
    }

    return QMDL_OK;
}

int qmdl_measure(const void *data,
                 size_t data_size,
                 qmdl_requirements *requirements)
{
    qmdl_cursor cur;
    qmdl_header header;
    int rc;

    if (data == 0 || requirements == 0)
    {
        return QMDL_ERR_ARGS;
    }

    qmdl_requirements_clear(requirements);

    cur.ptr = (const qmdl_u8 *)data;
    cur.end = cur.ptr + data_size;

    rc = qmdl_parse_header(&cur, &header);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    rc = qmdl_measure_after_header(&cur, &header, requirements);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    return QMDL_OK;
}

static int qmdl_check_storage(const qmdl_storage *storage,
                              const qmdl_requirements *requirements)
{
    if (storage == 0 || requirements == 0)
    {
        return QMDL_ERR_ARGS;
    }

    if (requirements->skins > storage->skins_cap) return QMDL_ERR_CAPACITY;
    if (requirements->skin_intervals > storage->skin_intervals_cap) return QMDL_ERR_CAPACITY;
    if (requirements->skin_pixels > storage->skin_pixels_cap) return QMDL_ERR_CAPACITY;
    if (requirements->stverts > storage->stverts_cap) return QMDL_ERR_CAPACITY;
    if (requirements->triangles > storage->triangles_cap) return QMDL_ERR_CAPACITY;
    if (requirements->frames > storage->frames_cap) return QMDL_ERR_CAPACITY;
    if (requirements->frame_intervals > storage->frame_intervals_cap) return QMDL_ERR_CAPACITY;
    if (requirements->subframes > storage->subframes_cap) return QMDL_ERR_CAPACITY;
    if (requirements->packed_verts > storage->packed_verts_cap) return QMDL_ERR_CAPACITY;

    if (requirements->skins != 0u && storage->skins == 0) return QMDL_ERR_CAPACITY;
    if (requirements->skin_intervals != 0u && storage->skin_intervals == 0) return QMDL_ERR_CAPACITY;
    if (requirements->skin_pixels != 0u && storage->skin_pixels == 0) return QMDL_ERR_CAPACITY;
    if (requirements->stverts != 0u && storage->stverts == 0) return QMDL_ERR_CAPACITY;
    if (requirements->triangles != 0u && storage->triangles == 0) return QMDL_ERR_CAPACITY;
    if (requirements->frames != 0u && storage->frames == 0) return QMDL_ERR_CAPACITY;
    if (requirements->frame_intervals != 0u && storage->frame_intervals == 0) return QMDL_ERR_CAPACITY;
    if (requirements->subframes != 0u && storage->subframes == 0) return QMDL_ERR_CAPACITY;
    if (requirements->packed_verts != 0u && storage->packed_verts == 0) return QMDL_ERR_CAPACITY;

    return QMDL_OK;
}

int qmdl_decode_ex(const void *data,
                   size_t data_size,
                   const qmdl_storage *storage,
                   qmdl_model *model,
                   qmdl_requirements *used,
                   qmdl_u32 decode_flags)
{
    qmdl_requirements req;
    qmdl_cursor cur;
    qmdl_u32 skin_pixels_per_image;
    qmdl_u32 i;
    qmdl_u32 j;
    qmdl_u32 type;
    qmdl_u32 group_count;
    qmdl_u32 skin_index;
    qmdl_u32 skin_interval_index;
    qmdl_u32 skin_pixel_index;
    qmdl_u32 frame_index;
    qmdl_u32 frame_interval_index;
    qmdl_u32 subframe_index;
    qmdl_u32 packed_vert_index;
    int rc;

    if (data == 0 || storage == 0 || model == 0)
    {
        return QMDL_ERR_ARGS;
    }

    qmdl_requirements_clear(&req);
    rc = qmdl_measure(data, data_size, &req);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    rc = qmdl_check_storage(storage, &req);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    qmdl_model_clear(model);
    model->skins = storage->skins;
    model->skin_intervals = storage->skin_intervals;
    model->skin_pixels = storage->skin_pixels;
    model->stverts = storage->stverts;
    model->triangles = storage->triangles;
    model->frames = storage->frames;
    model->frame_intervals = storage->frame_intervals;
    model->subframes = storage->subframes;
    model->packed_verts = storage->packed_verts;

    model->num_skin_intervals = req.skin_intervals;
    model->num_skin_pixels = req.skin_pixels;
    model->num_frame_intervals = req.frame_intervals;
    model->num_subframes = req.subframes;
    model->num_packed_verts = req.packed_verts;

    cur.ptr = (const qmdl_u8 *)data;
    cur.end = cur.ptr + data_size;

    rc = qmdl_parse_header(&cur, &model->header);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    rc = qmdl_u32_mul(model->header.skinwidth, model->header.skinheight, &skin_pixels_per_image);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    skin_index = 0u;
    skin_interval_index = 0u;
    skin_pixel_index = 0u;

    for (i = 0; i < model->header.numskins; ++i)
    {
        rc = qmdl_read_u32_le(&cur, &type);
        if (rc != QMDL_OK) return rc;

        model->skins[skin_index].type = (type == QMDL_SKIN_SINGLE) ?
                                        QMDL_SKIN_SINGLE :
                                        QMDL_SKIN_GROUP;
        model->skins[skin_index].first_interval = skin_interval_index;
        model->skins[skin_index].first_pixel = skin_pixel_index;

        if (type == QMDL_SKIN_SINGLE)
        {
            model->skins[skin_index].num_images = 1u;
            rc = qmdl_read_bytes(&cur,
                                 model->skin_pixels + skin_pixel_index,
                                 (size_t)skin_pixels_per_image);
            if (rc != QMDL_OK) return rc;
            skin_pixel_index += skin_pixels_per_image;
        }
        else
        {
            rc = qmdl_read_u32_le(&cur, &group_count);
            if (rc != QMDL_OK) return rc;
            if (qmdl_u32_has_sign_bit(group_count) || group_count == 0u) return QMDL_ERR_FORMAT;

            model->skins[skin_index].num_images = group_count;

            for (j = 0; j < group_count; ++j)
            {
                rc = qmdl_read_fx16_f32_le(&cur, &model->skin_intervals[skin_interval_index + j]);
                if (rc != QMDL_OK) return rc;
            }
            for (j = 0; j < group_count; ++j)
            {
                rc = qmdl_read_bytes(&cur,
                                     model->skin_pixels + skin_pixel_index,
                                     (size_t)skin_pixels_per_image);
                if (rc != QMDL_OK) return rc;
                skin_pixel_index += skin_pixels_per_image;
            }
            skin_interval_index += group_count;
        }

        skin_index++;
    }

    for (i = 0; i < model->header.numverts; ++i)
    {
        rc = qmdl_read_i32_le(&cur, &model->stverts[i].onseam);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_read_i32_le(&cur, &model->stverts[i].s);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_read_i32_le(&cur, &model->stverts[i].t);
        if (rc != QMDL_OK) return rc;
    }

    for (i = 0; i < model->header.numtris; ++i)
    {
        rc = qmdl_read_i32_le(&cur, &model->triangles[i].facesfront);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_read_i32_le(&cur, &model->triangles[i].vertindex[0]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_read_i32_le(&cur, &model->triangles[i].vertindex[1]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_read_i32_le(&cur, &model->triangles[i].vertindex[2]);
        if (rc != QMDL_OK) return rc;

        if ((decode_flags & QMDL_DECODE_STRICT_TRIANGLES) != 0u)
        {
            for (j = 0; j < 3u; ++j)
            {
                if (model->triangles[i].vertindex[j] < 0 ||
                    (qmdl_u32)model->triangles[i].vertindex[j] >= model->header.numverts)
                {
                    return QMDL_ERR_FORMAT;
                }
            }
        }
    }

    frame_index = 0u;
    frame_interval_index = 0u;
    subframe_index = 0u;
    packed_vert_index = 0u;

    for (i = 0; i < model->header.numframes; ++i)
    {
        rc = qmdl_read_u32_le(&cur, &type);
        if (rc != QMDL_OK) return rc;

        model->frames[frame_index].type = (type == QMDL_FRAME_SINGLE) ?
                                          QMDL_FRAME_SINGLE :
                                          QMDL_FRAME_GROUP;
        model->frames[frame_index].first_interval = frame_interval_index;
        model->frames[frame_index].first_subframe = subframe_index;

        if (type == QMDL_FRAME_SINGLE)
        {
            model->frames[frame_index].num_subframes = 1u;
            rc = qmdl_read_simple_subframe(&cur,
                                           model->header.numverts,
                                           &model->subframes[subframe_index],
                                           model->packed_verts,
                                           packed_vert_index);
            if (rc != QMDL_OK) return rc;

            model->frames[frame_index].bboxmin = model->subframes[subframe_index].bboxmin;
            model->frames[frame_index].bboxmax = model->subframes[subframe_index].bboxmax;

            subframe_index += 1u;
            packed_vert_index += model->header.numverts;
        }
        else
        {
            rc = qmdl_read_u32_le(&cur, &group_count);
            if (rc != QMDL_OK) return rc;
            if (qmdl_u32_has_sign_bit(group_count) || group_count == 0u) return QMDL_ERR_FORMAT;

            model->frames[frame_index].num_subframes = group_count;

            rc = qmdl_read_trivertx(&cur, &model->frames[frame_index].bboxmin);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_read_trivertx(&cur, &model->frames[frame_index].bboxmax);
            if (rc != QMDL_OK) return rc;

            for (j = 0; j < group_count; ++j)
            {
                rc = qmdl_read_fx16_f32_le(&cur, &model->frame_intervals[frame_interval_index + j]);
                if (rc != QMDL_OK) return rc;
            }

            for (j = 0; j < group_count; ++j)
            {
                rc = qmdl_read_simple_subframe(&cur,
                                               model->header.numverts,
                                               &model->subframes[subframe_index + j],
                                               model->packed_verts,
                                               packed_vert_index + (j * model->header.numverts));
                if (rc != QMDL_OK) return rc;
            }

            frame_interval_index += group_count;
            subframe_index += group_count;
            packed_vert_index += group_count * model->header.numverts;
        }

        frame_index++;
    }

    if (used != 0)
    {
        *used = req;
    }

    return QMDL_OK;
}

int qmdl_decode(const void *data,
                size_t data_size,
                const qmdl_storage *storage,
                qmdl_model *model,
                qmdl_requirements *used)
{
    return qmdl_decode_ex(data,
                          data_size,
                          storage,
                          model,
                          used,
                          QMDL_DECODE_STRICT_TRIANGLES);
}
