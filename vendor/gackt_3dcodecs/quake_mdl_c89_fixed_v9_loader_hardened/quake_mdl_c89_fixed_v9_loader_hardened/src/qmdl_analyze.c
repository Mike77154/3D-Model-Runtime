#include "qmdl_internal.h"

static int qmdl_skip_bytes_analyze(qmdl_cursor *cur, size_t count)
{
    if (cur == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if ((size_t)(cur->end - cur->ptr) < count)
    {
        return QMDL_ERR_SIZE;
    }
    cur->ptr += count;
    return QMDL_OK;
}

static int qmdl_read_trivertx_analyze(qmdl_cursor *cur, qmdl_trivertx *out_value)
{
    int rc;

    if (cur == 0 || out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_read_u8(cur, &out_value->v[0]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_u8(cur, &out_value->v[1]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_u8(cur, &out_value->v[2]);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_read_u8(cur, &out_value->lightnormalindex);
    if (rc != QMDL_OK) return rc;

    return QMDL_OK;
}

static int qmdl_parse_header_analyze(qmdl_cursor *cur, qmdl_header *header)
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

    return QMDL_OK;
}

static void qmdl_warn_group_time_value(qmdl_fx16 value,
                                       qmdl_u32 index,
                                       qmdl_fx16 *prev,
                                       qmdl_inspect_info *inspect)
{
    if (prev == 0 || inspect == 0)
    {
        return;
    }

    if (value <= 0)
    {
        inspect->warning_flags |= QMDL_WARN_GROUP_TIME_NON_POSITIVE;
        inspect->nonpositive_group_time_count += 1u;
    }
    if (index != 0u && value <= *prev)
    {
        inspect->warning_flags |= QMDL_WARN_GROUP_TIME_NON_MONOTONIC;
        inspect->nonmonotonic_group_time_count += 1u;
    }

    *prev = value;
}

int qmdl_analyze(const void *data,
                 size_t data_size,
                 qmdl_requirements *requirements,
                 qmdl_inspect_info *inspect)
{
    qmdl_requirements local_req;
    qmdl_cursor cur;
    qmdl_header header;
    qmdl_u32 i;
    qmdl_u32 j;
    qmdl_u32 raw_type;
    qmdl_u32 count;
    qmdl_i32 ivalue;
    qmdl_trivertx tmp_vert;
    char tmp_name[16];
    int rc;

    if (data == 0)
    {
        return QMDL_ERR_ARGS;
    }

    if (requirements == 0)
    {
        requirements = &local_req;
    }
    qmdl_requirements_clear(requirements);

    if (inspect != 0)
    {
        qmdl_inspect_info_clear(inspect);
    }

    rc = qmdl_measure(data, data_size, requirements);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    if (inspect != 0)
    {
        inspect->bytes_consumed = requirements->encoded_bytes;
        if (data_size > requirements->encoded_bytes)
        {
            inspect->trailing_bytes = data_size - requirements->encoded_bytes;
            inspect->warning_flags |= QMDL_WARN_TRAILING_BYTES;
        }
    }

    cur.ptr = (const qmdl_u8 *)data;
    cur.end = cur.ptr + data_size;

    rc = qmdl_parse_header_analyze(&cur, &header);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    if (inspect != 0)
    {
        if (header.synctype != QMDL_SYNC_SYNC && header.synctype != QMDL_SYNC_RAND)
        {
            inspect->warning_flags |= QMDL_WARN_INVALID_SYNCTYPE;
        }
        if ((header.skinwidth & 3u) != 0u)
        {
            inspect->warning_flags |= QMDL_WARN_SKINWIDTH_NOT_MULTIPLE_OF_4;
        }
        if ((header.skinwidth & 7u) != 0u)
        {
            inspect->warning_flags |= QMDL_WARN_SKINWIDTH_NOT_MULTIPLE_OF_8;
        }
        if ((header.skinheight & 7u) != 0u)
        {
            inspect->warning_flags |= QMDL_WARN_SKINHEIGHT_NOT_MULTIPLE_OF_8;
        }
    }

    for (i = 0; i < header.numskins; ++i)
    {
        rc = qmdl_read_u32_le(&cur, &raw_type);
        if (rc != QMDL_OK) return rc;

        if (raw_type == 0u)
        {
            size_t pixel_count;
            rc = qmdl_size_mul((size_t)header.skinwidth, (size_t)header.skinheight, &pixel_count);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_skip_bytes_analyze(&cur, pixel_count);
            if (rc != QMDL_OK) return rc;
        }
        else
        {
            if (inspect != 0 && raw_type != QMDL_SKIN_GROUP)
            {
                inspect->warning_flags |= QMDL_WARN_NONCANONICAL_SKIN_TYPE;
                inspect->noncanonical_skin_type_count += 1u;
            }

            rc = qmdl_read_u32_le(&cur, &count);
            if (rc != QMDL_OK) return rc;
            if (count == 0u) return QMDL_ERR_FORMAT;
            {
                qmdl_fx16 prev_time;
                qmdl_fx16 time_value;
                prev_time = 0;
                for (j = 0; j < count; ++j)
                {
                    rc = qmdl_read_fx16_f32_le(&cur, &time_value);
                    if (rc != QMDL_OK) return rc;
                    if (inspect != 0)
                    {
                        qmdl_warn_group_time_value(time_value, j, &prev_time, inspect);
                    }
                }
            }
            for (j = 0; j < count; ++j)
            {
                size_t pixel_count;
                rc = qmdl_size_mul((size_t)header.skinwidth, (size_t)header.skinheight, &pixel_count);
                if (rc != QMDL_OK) return rc;
                rc = qmdl_skip_bytes_analyze(&cur, pixel_count);
                if (rc != QMDL_OK) return rc;
            }
        }
    }

    for (i = 0; i < header.numverts; ++i)
    {
        rc = qmdl_read_i32_le(&cur, &ivalue);
        if (rc != QMDL_OK) return rc;
        if (inspect != 0 && ivalue != 0 && ivalue != (qmdl_i32)QMDL_ONSEAM_FLAG)
        {
            inspect->warning_flags |= QMDL_WARN_ONSEAM_NONCANONICAL;
            inspect->noncanonical_onseam_count += 1u;
        }

        rc = qmdl_read_i32_le(&cur, &ivalue);
        if (rc != QMDL_OK) return rc;
        if (inspect != 0 && (ivalue < 0 || (header.skinwidth != 0u && (qmdl_u32)ivalue >= header.skinwidth)))
        {
            inspect->warning_flags |= QMDL_WARN_STVERT_OUT_OF_RANGE;
            inspect->out_of_range_st_count += 1u;
        }

        rc = qmdl_read_i32_le(&cur, &ivalue);
        if (rc != QMDL_OK) return rc;
        if (inspect != 0 && (ivalue < 0 || (header.skinheight != 0u && (qmdl_u32)ivalue >= header.skinheight)))
        {
            inspect->warning_flags |= QMDL_WARN_STVERT_OUT_OF_RANGE;
            inspect->out_of_range_st_count += 1u;
        }
    }

    for (i = 0; i < header.numtris; ++i)
    {
        rc = qmdl_read_i32_le(&cur, &ivalue);
        if (rc != QMDL_OK) return rc;
        if (inspect != 0 && ivalue != 0 && ivalue != 1)
        {
            inspect->warning_flags |= QMDL_WARN_FACESFRONT_NONCANONICAL;
            inspect->noncanonical_facesfront_count += 1u;
        }
        for (j = 0; j < 3u; ++j)
        {
            rc = qmdl_read_i32_le(&cur, &ivalue);
            if (rc != QMDL_OK) return rc;
            if (inspect != 0 && (ivalue < 0 || (qmdl_u32)ivalue >= header.numverts))
            {
                inspect->warning_flags |= QMDL_WARN_TRIANGLE_INDEX_OUT_OF_RANGE;
                inspect->invalid_triangle_index_count += 1u;
            }
        }
    }

    for (i = 0; i < header.numframes; ++i)
    {
        rc = qmdl_read_u32_le(&cur, &raw_type);
        if (rc != QMDL_OK) return rc;

        if (raw_type == 0u)
        {
            count = 1u;
        }
        else
        {
            if (inspect != 0 && raw_type != QMDL_FRAME_GROUP)
            {
                inspect->warning_flags |= QMDL_WARN_NONCANONICAL_FRAME_TYPE;
                inspect->noncanonical_frame_type_count += 1u;
            }
            rc = qmdl_read_u32_le(&cur, &count);
            if (rc != QMDL_OK) return rc;
            if (count == 0u) return QMDL_ERR_FORMAT;
            rc = qmdl_read_trivertx_analyze(&cur, &tmp_vert);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_read_trivertx_analyze(&cur, &tmp_vert);
            if (rc != QMDL_OK) return rc;
            {
                qmdl_fx16 prev_time;
                qmdl_fx16 time_value;
                prev_time = 0;
                for (j = 0; j < count; ++j)
                {
                    rc = qmdl_read_fx16_f32_le(&cur, &time_value);
                    if (rc != QMDL_OK) return rc;
                    if (inspect != 0)
                    {
                        qmdl_warn_group_time_value(time_value, j, &prev_time, inspect);
                    }
                }
            }
        }

        for (j = 0; j < count; ++j)
        {
            qmdl_u32 k;
            rc = qmdl_read_trivertx_analyze(&cur, &tmp_vert);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_read_trivertx_analyze(&cur, &tmp_vert);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_read_bytes(&cur, tmp_name, 16u);
            if (rc != QMDL_OK) return rc;
            for (k = 0u; k < header.numverts; ++k)
            {
                rc = qmdl_read_trivertx_analyze(&cur, &tmp_vert);
                if (rc != QMDL_OK) return rc;
                if (inspect != 0)
                {
                    if (tmp_vert.lightnormalindex == 255u)
                    {
                        inspect->warning_flags |= QMDL_WARN_LIGHTNORMAL_BUG_255;
                        inspect->bug255_normal_count += 1u;
                    }
                    else if (!qmdl_vertex_normal_is_valid(tmp_vert.lightnormalindex))
                    {
                        inspect->warning_flags |= QMDL_WARN_LIGHTNORMAL_OUT_OF_RANGE;
                        inspect->invalid_normal_count += 1u;
                    }
                }
            }
        }
    }

    return QMDL_OK;
}
