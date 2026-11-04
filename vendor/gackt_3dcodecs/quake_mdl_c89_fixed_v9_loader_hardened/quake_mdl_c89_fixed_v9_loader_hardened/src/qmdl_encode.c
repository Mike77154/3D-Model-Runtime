
#include "qmdl_internal.h"


static int qmdl_encode_u32_has_sign_bit(qmdl_u32 value)
{
    return (value & 0x80000000u) != 0u;
}

static int qmdl_encode_header_shape_is_quake_parseable(const qmdl_header *header)
{
    if (header == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (qmdl_encode_u32_has_sign_bit(header->numskins) ||
        qmdl_encode_u32_has_sign_bit(header->skinwidth) ||
        qmdl_encode_u32_has_sign_bit(header->skinheight) ||
        qmdl_encode_u32_has_sign_bit(header->numverts) ||
        qmdl_encode_u32_has_sign_bit(header->numtris) ||
        qmdl_encode_u32_has_sign_bit(header->numframes))
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

static int qmdl_write_trivertx(qmdl_writer *wr, const qmdl_trivertx *value)
{
    int rc;

    rc = qmdl_write_u8(wr, value->v[0]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_u8(wr, value->v[1]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_u8(wr, value->v[2]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_u8(wr, value->lightnormalindex);
    if (rc != QMDL_OK) return rc;

    return QMDL_OK;
}

static int qmdl_write_simple_subframe(qmdl_writer *wr,
                                      const qmdl_subframe *subframe,
                                      const qmdl_trivertx *packed_verts,
                                      qmdl_u32 numverts)
{
    qmdl_u32 i;
    int rc;

    rc = qmdl_write_trivertx(wr, &subframe->bboxmin);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_trivertx(wr, &subframe->bboxmax);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_bytes(wr, subframe->name, 16u);
    if (rc != QMDL_OK) return rc;

    for (i = 0; i < numverts; ++i)
    {
        rc = qmdl_write_trivertx(wr, &packed_verts[subframe->first_vert + i]);
        if (rc != QMDL_OK) return rc;
    }

    return QMDL_OK;
}

int qmdl_validate_model(const qmdl_model *model,
                        qmdl_requirements *requirements)
{
    qmdl_requirements req;
    qmdl_u32 pixels_per_image;
    qmdl_u32 running_skin_intervals;
    qmdl_u32 running_skin_pixels;
    qmdl_u32 running_frame_intervals;
    qmdl_u32 running_subframes;
    qmdl_u32 running_packed_verts;
    qmdl_u32 i;
    qmdl_u32 j;
    int rc;

    if (model == 0)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_encode_header_shape_is_quake_parseable(&model->header);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    qmdl_requirements_clear(&req);
    req.skins = model->header.numskins;
    req.stverts = model->header.numverts;
    req.triangles = model->header.numtris;
    req.frames = model->header.numframes;
    req.encoded_bytes = 84u;

    if (model->header.numskins != 0u && model->skins == 0) return QMDL_ERR_ARGS;
    if (model->header.numverts != 0u && model->stverts == 0) return QMDL_ERR_ARGS;
    if (model->header.numtris != 0u && model->triangles == 0) return QMDL_ERR_ARGS;
    if (model->header.numframes != 0u && model->frames == 0) return QMDL_ERR_ARGS;
    if (model->num_subframes != 0u && model->subframes == 0) return QMDL_ERR_ARGS;
    if (model->num_packed_verts != 0u && model->packed_verts == 0) return QMDL_ERR_ARGS;
    if (model->header.synctype != QMDL_SYNC_SYNC &&
        model->header.synctype != QMDL_SYNC_RAND)
    {
        return QMDL_ERR_RANGE;
    }

    rc = qmdl_u32_mul(model->header.skinwidth, model->header.skinheight, &pixels_per_image);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    if (model->num_skin_intervals != 0u && model->skin_intervals == 0) return QMDL_ERR_ARGS;
    if (model->num_skin_pixels != 0u && model->skin_pixels == 0) return QMDL_ERR_ARGS;
    if (model->num_frame_intervals != 0u && model->frame_intervals == 0) return QMDL_ERR_ARGS;
    running_skin_intervals = 0u;
    running_skin_pixels = 0u;
    for (i = 0; i < model->header.numskins; ++i)
    {
        if (model->skins[i].type == QMDL_SKIN_SINGLE)
        {
            if (model->skins[i].num_images != 1u) return QMDL_ERR_FORMAT;
            if (model->skins[i].first_pixel != running_skin_pixels) return QMDL_ERR_FORMAT;
            rc = qmdl_u32_add(&running_skin_pixels, pixels_per_image);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add(&req.encoded_bytes, 4u);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add(&req.encoded_bytes, (size_t)pixels_per_image);
            if (rc != QMDL_OK) return rc;
        }
        else if (model->skins[i].type == QMDL_SKIN_GROUP)
        {
            if (model->skins[i].num_images == 0u) return QMDL_ERR_FORMAT;
            if (model->skins[i].first_pixel != running_skin_pixels) return QMDL_ERR_FORMAT;
            if (model->skins[i].first_interval != running_skin_intervals) return QMDL_ERR_FORMAT;

            rc = qmdl_u32_add(&running_skin_intervals, model->skins[i].num_images);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_group_times_are_strictly_increasing(
                    model->skin_intervals + model->skins[i].first_interval,
                    model->skins[i].num_images);
            if (rc != QMDL_OK)
            {
                return (rc == QMDL_ERR_FORMAT) ? QMDL_ERR_RANGE : rc;
            }
            for (j = 0; j < model->skins[i].num_images; ++j)
            {
                rc = qmdl_u32_add(&running_skin_pixels, pixels_per_image);
                if (rc != QMDL_OK) return rc;
            }

            rc = qmdl_size_add(&req.encoded_bytes, 8u);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add_mul(&req.encoded_bytes, (size_t)model->skins[i].num_images, 4u);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add_mul(&req.encoded_bytes,
                                   (size_t)model->skins[i].num_images,
                                   (size_t)pixels_per_image);
            if (rc != QMDL_OK) return rc;
        }
        else
        {
            return QMDL_ERR_FORMAT;
        }
    }

    req.skin_intervals = running_skin_intervals;
    req.skin_pixels = running_skin_pixels;

    rc = qmdl_size_add_mul(&req.encoded_bytes, (size_t)model->header.numverts, 12u);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_size_add_mul(&req.encoded_bytes, (size_t)model->header.numtris, 16u);
    if (rc != QMDL_OK) return rc;

    for (i = 0; i < model->header.numtris; ++i)
    {
        if (model->triangles[i].vertindex[0] < 0 ||
            model->triangles[i].vertindex[1] < 0 ||
            model->triangles[i].vertindex[2] < 0)
        {
            return QMDL_ERR_RANGE;
        }
        for (j = 0; j < 3u; ++j)
        {
            if ((qmdl_u32)model->triangles[i].vertindex[j] >= model->header.numverts)
            {
                return QMDL_ERR_RANGE;
            }
        }
    }

    running_frame_intervals = 0u;
    running_subframes = 0u;
    running_packed_verts = 0u;

    for (i = 0; i < model->header.numframes; ++i)
    {
        if (model->frames[i].first_subframe != running_subframes) return QMDL_ERR_FORMAT;
        rc = qmdl_size_add(&req.encoded_bytes, 4u);
        if (rc != QMDL_OK) return rc;

        if (model->frames[i].type == QMDL_FRAME_SINGLE)
        {
            if (model->frames[i].num_subframes != 1u) return QMDL_ERR_FORMAT;
            if (running_subframes >= model->num_subframes) return QMDL_ERR_RANGE;
            if (model->subframes[running_subframes].first_vert != running_packed_verts) return QMDL_ERR_FORMAT;

            running_subframes += 1u;
            rc = qmdl_u32_add(&running_packed_verts, model->header.numverts);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add(&req.encoded_bytes, 24u);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add_mul(&req.encoded_bytes, (size_t)model->header.numverts, 4u);
            if (rc != QMDL_OK) return rc;
        }
        else if (model->frames[i].type == QMDL_FRAME_GROUP)
        {
            if (model->frames[i].num_subframes == 0u) return QMDL_ERR_FORMAT;
            if (model->frames[i].first_interval != running_frame_intervals) return QMDL_ERR_FORMAT;
            rc = qmdl_group_times_are_strictly_increasing(
                    model->frame_intervals + model->frames[i].first_interval,
                    model->frames[i].num_subframes);
            if (rc != QMDL_OK)
            {
                return (rc == QMDL_ERR_FORMAT) ? QMDL_ERR_RANGE : rc;
            }

            rc = qmdl_u32_add(&running_frame_intervals, model->frames[i].num_subframes);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add(&req.encoded_bytes, 12u);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_size_add_mul(&req.encoded_bytes, (size_t)model->frames[i].num_subframes, 4u);
            if (rc != QMDL_OK) return rc;

            for (j = 0; j < model->frames[i].num_subframes; ++j)
            {
                if ((running_subframes + j) >= model->num_subframes) return QMDL_ERR_RANGE;
                if (model->subframes[running_subframes + j].first_vert !=
                    running_packed_verts + (j * model->header.numverts))
                {
                    return QMDL_ERR_FORMAT;
                }
                rc = qmdl_size_add(&req.encoded_bytes, 24u);
                if (rc != QMDL_OK) return rc;
                rc = qmdl_size_add_mul(&req.encoded_bytes, (size_t)model->header.numverts, 4u);
                if (rc != QMDL_OK) return rc;
            }

            running_subframes += model->frames[i].num_subframes;
            rc = qmdl_u32_mul(model->frames[i].num_subframes, model->header.numverts, &j);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_u32_add(&running_packed_verts, j);
            if (rc != QMDL_OK) return rc;
        }
        else
        {
            return QMDL_ERR_FORMAT;
        }
    }

    req.frame_intervals = running_frame_intervals;
    req.subframes = running_subframes;
    req.packed_verts = running_packed_verts;

    if (req.skin_intervals != model->num_skin_intervals) return QMDL_ERR_FORMAT;
    if (req.skin_pixels != model->num_skin_pixels) return QMDL_ERR_FORMAT;
    if (req.frame_intervals != model->num_frame_intervals) return QMDL_ERR_FORMAT;
    if (req.subframes != model->num_subframes) return QMDL_ERR_FORMAT;
    if (req.packed_verts != model->num_packed_verts) return QMDL_ERR_FORMAT;

    if (requirements != 0)
    {
        *requirements = req;
    }

    return QMDL_OK;
}

int qmdl_encode_measure(const qmdl_model *model,
                        size_t *encoded_size)
{
    qmdl_requirements requirements;
    int rc;

    if (encoded_size == 0)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_validate_model(model, &requirements);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    *encoded_size = requirements.encoded_bytes;
    return QMDL_OK;
}

int qmdl_encode(const qmdl_model *model,
                void *dst,
                size_t dst_size,
                size_t *encoded_size)
{
    qmdl_requirements requirements;
    qmdl_writer wr;
    qmdl_u32 pixels_per_image;
    qmdl_u32 i;
    qmdl_u32 j;
    int rc;

    if (model == 0 || dst == 0)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_validate_model(model, &requirements);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    if (dst_size < requirements.encoded_bytes)
    {
        return QMDL_ERR_SIZE;
    }

    rc = qmdl_u32_mul(model->header.skinwidth, model->header.skinheight, &pixels_per_image);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    wr.ptr = (qmdl_u8 *)dst;
    wr.end = wr.ptr + requirements.encoded_bytes;

    rc = qmdl_write_u32_le(&wr, QMDL_IDENT);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_u32_le(&wr, QMDL_VERSION);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_write_fx16_f32_le(&wr, model->header.scale[0]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_fx16_f32_le(&wr, model->header.scale[1]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_fx16_f32_le(&wr, model->header.scale[2]);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_write_fx16_f32_le(&wr, model->header.origin[0]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_fx16_f32_le(&wr, model->header.origin[1]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_fx16_f32_le(&wr, model->header.origin[2]);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_write_fx16_f32_le(&wr, model->header.bounding_radius);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_write_fx16_f32_le(&wr, model->header.eye_position[0]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_fx16_f32_le(&wr, model->header.eye_position[1]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_fx16_f32_le(&wr, model->header.eye_position[2]);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_write_u32_le(&wr, model->header.numskins);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_u32_le(&wr, model->header.skinwidth);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_u32_le(&wr, model->header.skinheight);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_u32_le(&wr, model->header.numverts);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_u32_le(&wr, model->header.numtris);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_u32_le(&wr, model->header.numframes);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_u32_le(&wr, model->header.synctype);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_u32_le(&wr, model->header.flags);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_write_fx16_f32_le(&wr, model->header.size);
    if (rc != QMDL_OK) return rc;

    for (i = 0; i < model->header.numskins; ++i)
    {
        rc = qmdl_write_u32_le(&wr, model->skins[i].type);
        if (rc != QMDL_OK) return rc;

        if (model->skins[i].type == QMDL_SKIN_SINGLE)
        {
            rc = qmdl_write_bytes(&wr,
                                  model->skin_pixels + model->skins[i].first_pixel,
                                  (size_t)(model->header.skinwidth * model->header.skinheight));
            if (rc != QMDL_OK) return rc;
        }
        else
        {
            rc = qmdl_write_u32_le(&wr, model->skins[i].num_images);
            if (rc != QMDL_OK) return rc;
            for (j = 0; j < model->skins[i].num_images; ++j)
            {
                rc = qmdl_write_fx16_f32_le(&wr,
                                            model->skin_intervals[model->skins[i].first_interval + j]);
                if (rc != QMDL_OK) return rc;
            }
            for (j = 0; j < model->skins[i].num_images; ++j)
            {
                rc = qmdl_write_bytes(&wr,
                                      model->skin_pixels +
                                      model->skins[i].first_pixel +
                                      (j * pixels_per_image),
                                      (size_t)pixels_per_image);
                if (rc != QMDL_OK) return rc;
            }
        }
    }

    for (i = 0; i < model->header.numverts; ++i)
    {
        rc = qmdl_write_i32_le(&wr, model->stverts[i].onseam);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_write_i32_le(&wr, model->stverts[i].s);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_write_i32_le(&wr, model->stverts[i].t);
        if (rc != QMDL_OK) return rc;
    }

    for (i = 0; i < model->header.numtris; ++i)
    {
        rc = qmdl_write_i32_le(&wr, model->triangles[i].facesfront);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_write_i32_le(&wr, model->triangles[i].vertindex[0]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_write_i32_le(&wr, model->triangles[i].vertindex[1]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_write_i32_le(&wr, model->triangles[i].vertindex[2]);
        if (rc != QMDL_OK) return rc;
    }

    for (i = 0; i < model->header.numframes; ++i)
    {
        rc = qmdl_write_u32_le(&wr, model->frames[i].type);
        if (rc != QMDL_OK) return rc;

        if (model->frames[i].type == QMDL_FRAME_SINGLE)
        {
            rc = qmdl_write_simple_subframe(&wr,
                                            &model->subframes[model->frames[i].first_subframe],
                                            model->packed_verts,
                                            model->header.numverts);
            if (rc != QMDL_OK) return rc;
        }
        else
        {
            rc = qmdl_write_u32_le(&wr, model->frames[i].num_subframes);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_write_trivertx(&wr, &model->frames[i].bboxmin);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_write_trivertx(&wr, &model->frames[i].bboxmax);
            if (rc != QMDL_OK) return rc;

            for (j = 0; j < model->frames[i].num_subframes; ++j)
            {
                rc = qmdl_write_fx16_f32_le(&wr,
                                            model->frame_intervals[model->frames[i].first_interval + j]);
                if (rc != QMDL_OK) return rc;
            }

            for (j = 0; j < model->frames[i].num_subframes; ++j)
            {
                rc = qmdl_write_simple_subframe(&wr,
                                                &model->subframes[model->frames[i].first_subframe + j],
                                                model->packed_verts,
                                                model->header.numverts);
                if (rc != QMDL_OK) return rc;
            }
        }
    }

    if (encoded_size != 0)
    {
        *encoded_size = requirements.encoded_bytes;
    }

    return QMDL_OK;
}
