#include "qmdl_internal.h"

static int qmdl_skip_bytes_compat(qmdl_cursor *cur, size_t count)
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

static int qmdl_read_trivertx_compat(qmdl_cursor *cur, qmdl_trivertx *out_value)
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

static int qmdl_parse_header_compat(qmdl_cursor *cur, qmdl_header *header)
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

    return QMDL_OK;
}

static void qmdl_compat_add_flag(qmdl_quake_compat_info *compat, qmdl_u32 flag)
{
    if (compat != 0)
    {
        compat->flags |= flag;
        if ((flag & QMDL_QCOMPAT_TRIANGLE_INDEX_OUT_OF_RANGE) != 0u &&
            compat->invalid_triangle_index_count != 0xFFFFFFFFu)
        {
            compat->invalid_triangle_index_count++;
        }
        if ((flag & QMDL_QCOMPAT_ST_OUT_OF_RANGE) != 0u &&
            compat->out_of_range_st_count != 0xFFFFFFFFu)
        {
            compat->out_of_range_st_count++;
        }
    }
}

static int qmdl_compat_i32_is_negative_bits(qmdl_u32 value)
{
    return (value & 0x80000000u) != 0u;
}

static int qmdl_compat_name16_has_nul(const char name[16])
{
    qmdl_u32 i;

    if (name == 0)
    {
        return 0;
    }
    for (i = 0u; i < 16u; ++i)
    {
        if (name[i] == '\0')
        {
            return 1;
        }
    }
    return 0;
}

static void qmdl_compat_add_flag2(qmdl_quake_compat_info *compat, qmdl_u32 flag)
{
    if (compat == 0)
    {
        return;
    }
    compat->flags2 |= flag;
    if ((flag & QMDL_QCOMPAT2_SIGNED_HEADER_FIELD_NEGATIVE) != 0u &&
        compat->signed_negative_header_count != 0xFFFFFFFFu)
    {
        compat->signed_negative_header_count++;
    }
    if ((flag & QMDL_QCOMPAT2_SIGNED_GROUP_COUNT_NEGATIVE) != 0u &&
        compat->signed_negative_group_count != 0xFFFFFFFFu)
    {
        compat->signed_negative_group_count++;
    }
    if ((flag & QMDL_QCOMPAT2_ONSEAM_NONCANONICAL) != 0u &&
        compat->noncanonical_onseam_count != 0xFFFFFFFFu)
    {
        compat->noncanonical_onseam_count++;
    }
    if ((flag & QMDL_QCOMPAT2_FACESFRONT_NONCANONICAL) != 0u &&
        compat->noncanonical_facesfront_count != 0xFFFFFFFFu)
    {
        compat->noncanonical_facesfront_count++;
    }
}

static void qmdl_compat_note_error(qmdl_quake_compat_info *compat,
                                   const qmdl_cursor *cur,
                                   const void *base,
                                   int rc)
{
    if (compat == 0)
    {
        return;
    }
    compat->error_code = (qmdl_i32)rc;
    if (cur != 0 && base != 0 && cur->ptr >= (const qmdl_u8 *)base)
    {
        compat->error_offset = (size_t)(cur->ptr - (const qmdl_u8 *)base);
    }
}

static void qmdl_compat_add_skin_flag2(qmdl_quake_compat_info *compat,
                                       qmdl_u32 skin_index,
                                       qmdl_u32 flag)
{
    if (compat == 0)
    {
        return;
    }
    qmdl_compat_add_flag2(compat, flag);
    if (compat->first_flagged_skin == QMDL_INVALID_INDEX)
    {
        compat->first_flagged_skin = skin_index;
    }
}

static void qmdl_compat_add_frame_flag2(qmdl_quake_compat_info *compat,
                                        qmdl_u32 frame_index,
                                        qmdl_u32 flag)
{
    if (compat == 0)
    {
        return;
    }
    qmdl_compat_add_flag2(compat, flag);
    if (compat->first_flagged_frame == QMDL_INVALID_INDEX)
    {
        compat->first_flagged_frame = frame_index;
    }
}

static void qmdl_compat_add_skin_flag(qmdl_quake_compat_info *compat,
                                      qmdl_u32 skin_index,
                                      qmdl_u32 flag)
{
    if (compat != 0)
    {
        compat->flags |= flag;
        if ((flag & QMDL_QCOMPAT_NONCANONICAL_TYPE) != 0u &&
            compat->noncanonical_type_count != 0xFFFFFFFFu)
        {
            compat->noncanonical_type_count++;
        }
        if (compat->first_flagged_skin == QMDL_INVALID_INDEX)
        {
            compat->first_flagged_skin = skin_index;
        }
    }
}

static void qmdl_compat_add_frame_flag(qmdl_quake_compat_info *compat,
                                       qmdl_u32 frame_index,
                                       qmdl_u32 flag)
{
    if (compat != 0)
    {
        compat->flags |= flag;
        if ((flag & QMDL_QCOMPAT_NONCANONICAL_TYPE) != 0u &&
            compat->noncanonical_type_count != 0xFFFFFFFFu)
        {
            compat->noncanonical_type_count++;
        }
        if ((flag & QMDL_QCOMPAT_LIGHTNORMAL_OUT_OF_RANGE) != 0u &&
            compat->bad_normal_count != 0xFFFFFFFFu)
        {
            compat->bad_normal_count++;
        }
        if (compat->first_flagged_frame == QMDL_INVALID_INDEX)
        {
            compat->first_flagged_frame = frame_index;
        }
    }
}


static int qmdl_compat_delta_differs(qmdl_fx16 a, qmdl_fx16 b)
{
    if (a < 0 || b < 0)
    {
        return 0;
    }
    if (a > b)
    {
        return (a - b) > 1;
    }
    return (b - a) > 1;
}

static void qmdl_compat_add_variable_interval(qmdl_quake_compat_info *compat,
                                              qmdl_u32 skin_index,
                                              qmdl_u32 frame_index)
{
    if (compat == 0)
    {
        return;
    }

    if (skin_index != QMDL_INVALID_INDEX)
    {
        qmdl_compat_add_skin_flag(compat, skin_index, QMDL_QCOMPAT_SKIN_INTERVALS_VARIABLE);
        if (compat->variable_skin_interval_count != 0xFFFFFFFFu)
        {
            compat->variable_skin_interval_count++;
        }
    }
    else
    {
        qmdl_compat_add_frame_flag(compat, frame_index, QMDL_QCOMPAT_FRAME_INTERVALS_VARIABLE);
        if (compat->variable_frame_interval_count != 0xFFFFFFFFu)
        {
            compat->variable_frame_interval_count++;
        }
    }
}

static void qmdl_compat_add_skin_group_over_4(qmdl_quake_compat_info *compat,
                                             qmdl_u32 skin_index,
                                             qmdl_u32 image_count)
{
    if (compat == 0)
    {
        return;
    }
    qmdl_compat_add_skin_flag(compat, skin_index, QMDL_QCOMPAT_SKIN_GROUP_IMAGES_OVER_4);
    if (compat->skin_group_over_4_count != 0xFFFFFFFFu)
    {
        compat->skin_group_over_4_count++;
    }
    if (image_count > 4u && compat->gl_skin_slot_alias_count <= 0xFFFFFFFFu - (image_count - 4u))
    {
        compat->gl_skin_slot_alias_count += image_count - 4u;
    }
}

static void qmdl_compat_apply_header(qmdl_quake_compat_info *compat,
                                     const qmdl_header *header)
{
    qmdl_u32 skin_pixels;

    if (compat == 0 || header == 0)
    {
        return;
    }

    if (qmdl_compat_i32_is_negative_bits(header->numskins))
    {
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_SIGNED_HEADER_FIELD_NEGATIVE);
    }
    if (qmdl_compat_i32_is_negative_bits(header->skinwidth))
    {
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_SIGNED_HEADER_FIELD_NEGATIVE);
    }
    if (qmdl_compat_i32_is_negative_bits(header->skinheight))
    {
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_SIGNED_HEADER_FIELD_NEGATIVE);
    }
    if (qmdl_compat_i32_is_negative_bits(header->numverts))
    {
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_SIGNED_HEADER_FIELD_NEGATIVE);
    }
    if (qmdl_compat_i32_is_negative_bits(header->numtris))
    {
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_SIGNED_HEADER_FIELD_NEGATIVE);
    }
    if (qmdl_compat_i32_is_negative_bits(header->numframes))
    {
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_SIGNED_HEADER_FIELD_NEGATIVE);
    }

    if (header->numskins == 0u)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_ZERO_SKINS);
    }
    if (header->numskins > QMDL_QUAKE_MAX_SKINS)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_SKINS_OVER_32);
    }
    if (header->skinwidth == 0u)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_SKINWIDTH_ZERO);
    }
    if (header->skinheight == 0u)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_SKINHEIGHT_ZERO);
    }
    if ((header->skinwidth & 3u) != 0u)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_SKINWIDTH_NOT_MULTIPLE_OF_4);
    }
    if (qmdl_u32_mul(header->skinwidth, header->skinheight, &skin_pixels) == QMDL_OK)
    {
        if ((skin_pixels & 3u) != 0u)
        {
            qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_SKINSIZE_NOT_MULTIPLE_OF_4);
        }
        if (skin_pixels > 0x7FFFFFFFu)
        {
            qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_SKINSIZE_SIGNED_OVERFLOW);
        }
    }
    else
    {
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_SKINSIZE_NOT_MULTIPLE_OF_4);
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_SKINSIZE_U32_OVERFLOW);
    }
    if (header->skinheight > QMDL_QUAKE_MAX_SKIN_HEIGHT)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_SKINHEIGHT_OVER_480);
    }
    if (header->skinheight > QMDL_QUAKEFORGE_MAX_SKIN_HEIGHT)
    {
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_SKINHEIGHT_OVER_QUAKEFORGE);
    }
    if (header->numverts == 0u)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_ZERO_VERTS);
    }
    if (header->numverts > QMDL_ID1_GL_MAX_VERTS)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_VERTS_OVER_ID1_GL);
    }
    if (header->numverts > QMDL_ID1_SW_MAX_VERTS)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_VERTS_OVER_ID1_SW_OR_QS);
    }
    if (header->numverts > QMDL_EZQUAKE_MAX_VERTS)
    {
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_VERTS_OVER_EZQUAKE);
    }
    if (header->numverts > QMDL_IRONWAIL_MAX_VERTS)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_VERTS_OVER_IRONWAIL);
    }
    if (header->numtris == 0u)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_ZERO_TRIS);
    }
    if (header->numtris > QMDL_ID1_GL_MAX_TRIS)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_TRIS_OVER_ID1_GL);
    }
    if (header->numtris > QMDL_EZQUAKE_MAX_TRIS)
    {
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_TRIS_OVER_EZQUAKE);
    }
    if (header->numtris > QMDL_QUAKESPASM_MAX_TRIS)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_TRIS_OVER_QS);
    }
    if (header->numframes == 0u)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_ZERO_FRAMES);
    }
    if (header->synctype != QMDL_SYNC_SYNC && header->synctype != QMDL_SYNC_RAND)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_BAD_SYNCTYPE);
    }
    if (header->numskins > QMDL_DARKPLACES_MAX_FIELD ||
        header->skinwidth > QMDL_DARKPLACES_MAX_FIELD ||
        header->skinheight > QMDL_DARKPLACES_MAX_FIELD ||
        header->numverts > QMDL_DARKPLACES_MAX_FIELD ||
        header->numtris > QMDL_DARKPLACES_MAX_FIELD ||
        header->numframes > QMDL_DARKPLACES_MAX_FIELD)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_DARKPLACES_FIELD_OVER_65535);
    }
    if (header->scale[0] <= 0 || header->scale[1] <= 0 || header->scale[2] <= 0)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_SCALE_NON_POSITIVE);
    }
}

static void qmdl_compat_apply_pose_limits(qmdl_quake_compat_info *compat)
{
    if (compat == 0)
    {
        return;
    }
    if (compat->num_poses > QMDL_ID1_GL_MAX_POSES)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_POSES_OVER_ID1_GL);
    }
    if (compat->num_poses > QMDL_EZQUAKE_MAX_POSES)
    {
        qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_POSES_OVER_EZQUAKE);
    }
    if (compat->num_poses > QMDL_QUAKESPASM_MAX_POSES)
    {
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_POSES_OVER_QS);
    }
}

static void qmdl_compat_apply_engine_matrix(qmdl_quake_compat_info *compat)
{
    qmdl_u32 flags;
    qmdl_u32 flags2;
    qmdl_u32 all_fatal;
    qmdl_u32 all_fatal2;
    qmdl_u32 ok;

    if (compat == 0)
    {
        return;
    }

    flags = compat->flags;
    flags2 = compat->flags2;
    ok = QMDL_ENGINE_ALL;

    all_fatal = QMDL_QCOMPAT_ZERO_SKINS |
                QMDL_QCOMPAT_SKINWIDTH_ZERO |
                QMDL_QCOMPAT_SKINHEIGHT_ZERO |
                QMDL_QCOMPAT_ZERO_VERTS |
                QMDL_QCOMPAT_ZERO_TRIS |
                QMDL_QCOMPAT_ZERO_FRAMES |
                QMDL_QCOMPAT_GROUP_COUNT_ZERO |
                QMDL_QCOMPAT_TRIANGLE_INDEX_OUT_OF_RANGE |
                QMDL_QCOMPAT_TRUNCATED_DATA |
                QMDL_QCOMPAT_BAD_IDENT_VERSION;

    all_fatal2 = QMDL_QCOMPAT2_SIGNED_HEADER_FIELD_NEGATIVE |
                 QMDL_QCOMPAT2_SIGNED_GROUP_COUNT_NEGATIVE |
                 QMDL_QCOMPAT2_SKINSIZE_SIGNED_OVERFLOW |
                 QMDL_QCOMPAT2_SKINSIZE_U32_OVERFLOW;

    if ((flags & all_fatal) != 0u || (flags2 & all_fatal2) != 0u)
    {
        ok = 0u;
    }
    if ((flags & QMDL_QCOMPAT_SKINS_OVER_32) != 0u)
    {
        ok &= ~(QMDL_ENGINE_ID1_SOFTWARE |
                QMDL_ENGINE_ID1_GLQUAKE |
                QMDL_ENGINE_QUAKESPASM |
                QMDL_ENGINE_IRONWAIL |
                QMDL_ENGINE_EZQUAKE |
                QMDL_ENGINE_QUAKEFORGE);
    }
    if ((flags & QMDL_QCOMPAT_SKINWIDTH_NOT_MULTIPLE_OF_4) != 0u)
    {
        ok &= ~QMDL_ENGINE_ID1_SOFTWARE;
    }
    if ((flags2 & QMDL_QCOMPAT2_SKINSIZE_NOT_MULTIPLE_OF_4) != 0u)
    {
        ok &= ~QMDL_ENGINE_QUAKEFORGE;
    }
    if ((flags & QMDL_QCOMPAT_SKINHEIGHT_OVER_480) != 0u)
    {
        ok &= ~(QMDL_ENGINE_ID1_SOFTWARE |
                QMDL_ENGINE_ID1_GLQUAKE |
                QMDL_ENGINE_QUAKESPASM |
                QMDL_ENGINE_IRONWAIL |
                QMDL_ENGINE_EZQUAKE);
    }
    if ((flags2 & QMDL_QCOMPAT2_SKINHEIGHT_OVER_QUAKEFORGE) != 0u)
    {
        ok &= ~QMDL_ENGINE_QUAKEFORGE;
    }
    if ((flags & QMDL_QCOMPAT_VERTS_OVER_ID1_GL) != 0u)
    {
        ok &= ~QMDL_ENGINE_ID1_GLQUAKE;
    }
    if ((flags & QMDL_QCOMPAT_VERTS_OVER_ID1_SW_OR_QS) != 0u)
    {
        ok &= ~(QMDL_ENGINE_ID1_SOFTWARE | QMDL_ENGINE_QUAKESPASM);
    }
    if ((flags2 & QMDL_QCOMPAT2_VERTS_OVER_EZQUAKE) != 0u)
    {
        ok &= ~QMDL_ENGINE_EZQUAKE;
    }
    if ((flags & QMDL_QCOMPAT_VERTS_OVER_IRONWAIL) != 0u)
    {
        ok &= ~QMDL_ENGINE_IRONWAIL;
    }
    if ((flags & QMDL_QCOMPAT_TRIS_OVER_ID1_GL) != 0u)
    {
        ok &= ~QMDL_ENGINE_ID1_GLQUAKE;
    }
    if ((flags2 & QMDL_QCOMPAT2_TRIS_OVER_EZQUAKE) != 0u)
    {
        ok &= ~QMDL_ENGINE_EZQUAKE;
    }
    if ((flags & QMDL_QCOMPAT_TRIS_OVER_QS) != 0u)
    {
        ok &= ~QMDL_ENGINE_QUAKESPASM;
    }
    if ((flags & QMDL_QCOMPAT_POSES_OVER_ID1_GL) != 0u)
    {
        ok &= ~(QMDL_ENGINE_ID1_SOFTWARE | QMDL_ENGINE_ID1_GLQUAKE);
    }
    if ((flags2 & QMDL_QCOMPAT2_POSES_OVER_EZQUAKE) != 0u)
    {
        ok &= ~QMDL_ENGINE_EZQUAKE;
    }
    if ((flags & QMDL_QCOMPAT_POSES_OVER_QS) != 0u)
    {
        ok &= ~(QMDL_ENGINE_QUAKESPASM | QMDL_ENGINE_IRONWAIL);
    }
    if ((flags & QMDL_QCOMPAT_SKIN_GROUP_IMAGES_OVER_4) != 0u)
    {
        ok &= ~(QMDL_ENGINE_ID1_GLQUAKE |
                QMDL_ENGINE_QUAKESPASM |
                QMDL_ENGINE_IRONWAIL |
                QMDL_ENGINE_EZQUAKE);
    }
    if ((flags & QMDL_QCOMPAT_GROUP_TIME_NON_POSITIVE) != 0u)
    {
        ok &= ~(QMDL_ENGINE_ID1_SOFTWARE |
                QMDL_ENGINE_ID1_GLQUAKE |
                QMDL_ENGINE_QUAKESPASM |
                QMDL_ENGINE_IRONWAIL |
                QMDL_ENGINE_EZQUAKE);
    }
    if ((flags & QMDL_QCOMPAT_BAD_SYNCTYPE) != 0u)
    {
        ok &= ~QMDL_ENGINE_DARKPLACES;
    }
    if ((flags & QMDL_QCOMPAT_DARKPLACES_FIELD_OVER_65535) != 0u)
    {
        ok &= ~QMDL_ENGINE_DARKPLACES;
    }

    compat->ok_engines = ok;
    compat->fatal_flags = flags & all_fatal;
    compat->fatal_flags2 = flags2 & all_fatal2;
    if (ok == 0u && (flags != 0u || flags2 != 0u))
    {
        compat->fatal_flags |= flags;
        compat->fatal_flags2 |= flags2;
    }
    compat->warning_flags = flags & ~compat->fatal_flags;
    compat->warning_flags2 = flags2 & ~compat->fatal_flags2;
}

static int qmdl_compat_note_time(qmdl_quake_compat_info *compat,
                                 qmdl_fx16 value,
                                 qmdl_u32 index,
                                 qmdl_fx16 *prev,
                                 qmdl_fx16 *prev_delta,
                                 qmdl_u32 skin_index,
                                 qmdl_u32 frame_index)
{
    qmdl_fx16 delta;

    if (compat == 0 || prev == 0 || prev_delta == 0)
    {
        return QMDL_ERR_ARGS;
    }

    if (value <= 0)
    {
        if (skin_index != QMDL_INVALID_INDEX)
        {
            qmdl_compat_add_skin_flag(compat, skin_index, QMDL_QCOMPAT_GROUP_TIME_NON_POSITIVE);
        }
        else
        {
            qmdl_compat_add_frame_flag(compat, frame_index, QMDL_QCOMPAT_GROUP_TIME_NON_POSITIVE);
        }
    }
    if (index == 0u && value > 0 && value < qmdl_fx_from_ratio(1, 100))
    {
        if (skin_index != QMDL_INVALID_INDEX)
        {
            qmdl_compat_add_skin_flag2(compat, skin_index, QMDL_QCOMPAT2_FIRST_INTERVAL_BELOW_0_01);
        }
        else
        {
            qmdl_compat_add_frame_flag2(compat, frame_index, QMDL_QCOMPAT2_FIRST_INTERVAL_BELOW_0_01);
        }
    }
    if (index != 0u && value <= *prev)
    {
        if (skin_index != QMDL_INVALID_INDEX)
        {
            qmdl_compat_add_skin_flag(compat, skin_index, QMDL_QCOMPAT_GROUP_TIME_NON_MONOTONIC);
        }
        else
        {
            qmdl_compat_add_frame_flag(compat, frame_index, QMDL_QCOMPAT_GROUP_TIME_NON_MONOTONIC);
        }
    }

    if (value > 0 && (index == 0u || value > *prev))
    {
        if (index == 0u)
        {
            *prev_delta = value;
        }
        else
        {
            delta = value - *prev;
            if (skin_index == QMDL_INVALID_INDEX &&
                qmdl_compat_delta_differs(delta, *prev_delta))
            {
                qmdl_compat_add_variable_interval(compat, skin_index, frame_index);
            }
        }
    }

    *prev = value;
    return QMDL_OK;
}

static void qmdl_compat_note_gl_skin_time(qmdl_quake_compat_info *compat,
                                          qmdl_fx16 value,
                                          qmdl_u32 index,
                                          qmdl_u32 skin_index)
{
    qmdl_fx16 expected;

    if (compat == 0)
    {
        return;
    }
    if (index > 2147483636u)
    {
        return;
    }
    expected = qmdl_fx_from_ratio((int)(index + 1u), 10);
    if (value != expected)
    {
        qmdl_compat_add_variable_interval(compat, skin_index, QMDL_INVALID_INDEX);
    }
}

static void qmdl_compat_mark_raw_end(qmdl_quake_compat_info *compat,
                                     const qmdl_cursor *cur,
                                     const void *base,
                                     qmdl_u32 flag,
                                     int rc)
{
    if (compat == 0)
    {
        return;
    }
    qmdl_compat_add_flag(compat, flag);
    qmdl_compat_note_error(compat, cur, base, rc);
    if (cur != 0 && base != 0 && cur->ptr >= (const qmdl_u8 *)base)
    {
        compat->bytes_consumed = (size_t)(cur->ptr - (const qmdl_u8 *)base);
    }
}

static void qmdl_compat_note_bbox_declared(qmdl_quake_compat_info *compat,
                                           const qmdl_trivertx *bbox_min,
                                           const qmdl_trivertx *bbox_max)
{
    qmdl_u32 i;

    if (compat == 0 || bbox_min == 0 || bbox_max == 0)
    {
        return;
    }
    for (i = 0u; i < 3u; ++i)
    {
        if (bbox_min->v[i] > bbox_max->v[i])
        {
            if (compat->bbox_inverted_count != 0xFFFFFFFFu)
            {
                compat->bbox_inverted_count++;
            }
            return;
        }
    }
}

static void qmdl_compat_box_begin(qmdl_trivertx *box_min,
                                  qmdl_trivertx *box_max,
                                  qmdl_u32 *have_box)
{
    if (have_box != 0)
    {
        *have_box = 0u;
    }
    if (box_min != 0)
    {
        box_min->v[0] = 0u;
        box_min->v[1] = 0u;
        box_min->v[2] = 0u;
        box_min->lightnormalindex = 0u;
    }
    if (box_max != 0)
    {
        box_max->v[0] = 0u;
        box_max->v[1] = 0u;
        box_max->v[2] = 0u;
        box_max->lightnormalindex = 0u;
    }
}

static void qmdl_compat_box_include(qmdl_trivertx *box_min,
                                    qmdl_trivertx *box_max,
                                    qmdl_u32 *have_box,
                                    const qmdl_trivertx *vert)
{
    qmdl_u32 i;

    if (box_min == 0 || box_max == 0 || have_box == 0 || vert == 0)
    {
        return;
    }
    if (*have_box == 0u)
    {
        *box_min = *vert;
        *box_max = *vert;
        *have_box = 1u;
        return;
    }
    for (i = 0u; i < 3u; ++i)
    {
        if (vert->v[i] < box_min->v[i])
        {
            box_min->v[i] = vert->v[i];
        }
        if (vert->v[i] > box_max->v[i])
        {
            box_max->v[i] = vert->v[i];
        }
    }
}

static void qmdl_compat_check_bbox_contains(qmdl_quake_compat_info *compat,
                                            const qmdl_trivertx *decl_min,
                                            const qmdl_trivertx *decl_max,
                                            const qmdl_trivertx *actual_min,
                                            const qmdl_trivertx *actual_max,
                                            qmdl_u32 have_actual)
{
    qmdl_u32 i;

    if (compat == 0 || decl_min == 0 || decl_max == 0 ||
        actual_min == 0 || actual_max == 0 || have_actual == 0u)
    {
        return;
    }
    for (i = 0u; i < 3u; ++i)
    {
        if (actual_min->v[i] < decl_min->v[i] || actual_max->v[i] > decl_max->v[i])
        {
            if (compat->bbox_mismatch_count != 0xFFFFFFFFu)
            {
                compat->bbox_mismatch_count++;
            }
            return;
        }
    }
}

int qmdl_quake_compat_analyze(const void *data,
                              size_t data_size,
                              qmdl_quake_compat_info *compat)
{
    qmdl_cursor cur;
    qmdl_header header;
    qmdl_trivertx tmp_vert;
    qmdl_trivertx bbox_min;
    qmdl_trivertx bbox_max;
    qmdl_trivertx group_bbox_min;
    qmdl_trivertx group_bbox_max;
    qmdl_trivertx actual_min;
    qmdl_trivertx actual_max;
    qmdl_u32 have_actual;
    qmdl_u32 i;
    qmdl_u32 j;
    qmdl_u32 k;
    qmdl_u32 raw_type;
    qmdl_u32 count;
    qmdl_u32 skin_pixels_per_image;
    qmdl_u32 tmp_u32;
    qmdl_i32 tmp_i32;
    qmdl_fx16 prev_time;
    qmdl_fx16 prev_delta;
    qmdl_fx16 time_value;
    size_t per_image_size;
    size_t skip_size;
    char tmp_name[16];
    int rc;

#define QMDL_COMPAT_TRY(EXPR) do { \
        rc = (EXPR); \
        if (rc != QMDL_OK) { \
            if (rc == QMDL_ERR_SIZE) { \
                qmdl_compat_mark_raw_end(compat, &cur, data, QMDL_QCOMPAT_TRUNCATED_DATA, rc); \
                qmdl_compat_apply_pose_limits(compat); \
                qmdl_compat_apply_engine_matrix(compat); \
                return QMDL_OK; \
            } \
            qmdl_compat_note_error(compat, &cur, data, rc); \
            return rc; \
        } \
    } while (0)

    if (data == 0 || compat == 0)
    {
        return QMDL_ERR_ARGS;
    }

    qmdl_quake_compat_info_clear(compat);

    cur.ptr = (const qmdl_u8 *)data;
    cur.end = cur.ptr + data_size;

    QMDL_COMPAT_TRY(qmdl_parse_header_compat(&cur, &header));

    if (header.ident != QMDL_IDENT || header.version != QMDL_VERSION)
    {
        qmdl_compat_mark_raw_end(compat, &cur, data, QMDL_QCOMPAT_BAD_IDENT_VERSION, QMDL_ERR_FORMAT);
        qmdl_compat_apply_engine_matrix(compat);
        return QMDL_OK;
    }

    qmdl_compat_apply_header(compat, &header);
    if ((compat->flags2 & (QMDL_QCOMPAT2_SIGNED_HEADER_FIELD_NEGATIVE |
                            QMDL_QCOMPAT2_SKINSIZE_SIGNED_OVERFLOW |
                            QMDL_QCOMPAT2_SKINSIZE_U32_OVERFLOW)) != 0u)
    {
        qmdl_compat_note_error(compat, &cur, data, QMDL_ERR_FORMAT);
        compat->bytes_consumed = (size_t)(cur.ptr - (const qmdl_u8 *)data);
        qmdl_compat_apply_pose_limits(compat);
        qmdl_compat_apply_engine_matrix(compat);
        return QMDL_OK;
    }

    QMDL_COMPAT_TRY(qmdl_u32_mul(header.skinwidth, header.skinheight, &skin_pixels_per_image));
    QMDL_COMPAT_TRY(qmdl_size_mul((size_t)header.skinwidth, (size_t)header.skinheight, &per_image_size));

    for (i = 0u; i < header.numskins; ++i)
    {
        QMDL_COMPAT_TRY(qmdl_read_u32_le(&cur, &raw_type));

        if (raw_type == QMDL_SKIN_SINGLE)
        {
            QMDL_COMPAT_TRY(qmdl_u32_add(&compat->num_skin_images, 1u));
            QMDL_COMPAT_TRY(qmdl_skip_bytes_compat(&cur, per_image_size));
        }
        else
        {
            if (raw_type != QMDL_SKIN_GROUP)
            {
                qmdl_compat_add_skin_flag(compat, i, QMDL_QCOMPAT_NONCANONICAL_TYPE);
            }
            QMDL_COMPAT_TRY(qmdl_read_u32_le(&cur, &count));
            if (qmdl_compat_i32_is_negative_bits(count))
            {
                qmdl_compat_add_skin_flag2(compat, i, QMDL_QCOMPAT2_SIGNED_GROUP_COUNT_NEGATIVE);
                qmdl_compat_note_error(compat, &cur, data, QMDL_ERR_FORMAT);
                compat->bytes_consumed = (size_t)(cur.ptr - (const qmdl_u8 *)data);
                qmdl_compat_apply_pose_limits(compat);
                qmdl_compat_apply_engine_matrix(compat);
                return QMDL_OK;
            }
            if (count == 0u)
            {
                qmdl_compat_add_skin_flag(compat, i, QMDL_QCOMPAT_GROUP_COUNT_ZERO);
                compat->bytes_consumed = (size_t)(cur.ptr - (const qmdl_u8 *)data);
                qmdl_compat_apply_pose_limits(compat);
                qmdl_compat_apply_engine_matrix(compat);
                return QMDL_OK;
            }

            QMDL_COMPAT_TRY(qmdl_u32_add(&compat->num_skin_images, count));
            if (count > compat->max_skin_group_images)
            {
                compat->max_skin_group_images = count;
            }
            if (count > 4u)
            {
                qmdl_compat_add_skin_group_over_4(compat, i, count);
            }

            prev_time = 0;
            prev_delta = 0;
            for (j = 0u; j < count; ++j)
            {
                QMDL_COMPAT_TRY(qmdl_read_fx16_f32_le(&cur, &time_value));
                QMDL_COMPAT_TRY(qmdl_compat_note_time(compat, time_value, j, &prev_time, &prev_delta, i, QMDL_INVALID_INDEX));
                qmdl_compat_note_gl_skin_time(compat, time_value, j, i);
            }

            QMDL_COMPAT_TRY(qmdl_size_mul((size_t)count, per_image_size, &skip_size));
            QMDL_COMPAT_TRY(qmdl_skip_bytes_compat(&cur, skip_size));
        }
    }

    for (i = 0u; i < header.numverts; ++i)
    {
        QMDL_COMPAT_TRY(qmdl_read_i32_le(&cur, &tmp_i32));
        if (tmp_i32 != 0 && tmp_i32 != (qmdl_i32)QMDL_ONSEAM_FLAG)
        {
            qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_ONSEAM_NONCANONICAL);
        }
        QMDL_COMPAT_TRY(qmdl_read_i32_le(&cur, &tmp_i32));
        if (tmp_i32 < 0 || (header.skinwidth != 0u && (qmdl_u32)tmp_i32 >= header.skinwidth))
        {
            qmdl_compat_add_flag(compat, QMDL_QCOMPAT_ST_OUT_OF_RANGE);
        }
        QMDL_COMPAT_TRY(qmdl_read_i32_le(&cur, &tmp_i32));
        if (tmp_i32 < 0 || (header.skinheight != 0u && (qmdl_u32)tmp_i32 >= header.skinheight))
        {
            qmdl_compat_add_flag(compat, QMDL_QCOMPAT_ST_OUT_OF_RANGE);
        }
    }

    for (i = 0u; i < header.numtris; ++i)
    {
        QMDL_COMPAT_TRY(qmdl_read_i32_le(&cur, &tmp_i32));
        if (tmp_i32 != 0 && tmp_i32 != 1)
        {
            qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_FACESFRONT_NONCANONICAL);
        }
        for (j = 0u; j < 3u; ++j)
        {
            QMDL_COMPAT_TRY(qmdl_read_i32_le(&cur, &tmp_i32));
            if (tmp_i32 < 0 || (qmdl_u32)tmp_i32 >= header.numverts)
            {
                qmdl_compat_add_flag(compat, QMDL_QCOMPAT_TRIANGLE_INDEX_OUT_OF_RANGE);
            }
        }
    }

    for (i = 0u; i < header.numframes; ++i)
    {
        QMDL_COMPAT_TRY(qmdl_read_u32_le(&cur, &raw_type));

        if (raw_type == QMDL_FRAME_SINGLE)
        {
            count = 1u;
        }
        else
        {
            if (raw_type != QMDL_FRAME_GROUP)
            {
                qmdl_compat_add_frame_flag(compat, i, QMDL_QCOMPAT_NONCANONICAL_TYPE);
            }
            QMDL_COMPAT_TRY(qmdl_read_u32_le(&cur, &count));
            if (qmdl_compat_i32_is_negative_bits(count))
            {
                qmdl_compat_add_frame_flag2(compat, i, QMDL_QCOMPAT2_SIGNED_GROUP_COUNT_NEGATIVE);
                qmdl_compat_note_error(compat, &cur, data, QMDL_ERR_FORMAT);
                compat->bytes_consumed = (size_t)(cur.ptr - (const qmdl_u8 *)data);
                qmdl_compat_apply_pose_limits(compat);
                qmdl_compat_apply_engine_matrix(compat);
                return QMDL_OK;
            }
            if (count == 0u)
            {
                qmdl_compat_add_frame_flag(compat, i, QMDL_QCOMPAT_GROUP_COUNT_ZERO);
                compat->bytes_consumed = (size_t)(cur.ptr - (const qmdl_u8 *)data);
                qmdl_compat_apply_pose_limits(compat);
                qmdl_compat_apply_engine_matrix(compat);
                return QMDL_OK;
            }
            QMDL_COMPAT_TRY(qmdl_read_trivertx_compat(&cur, &group_bbox_min));
            QMDL_COMPAT_TRY(qmdl_read_trivertx_compat(&cur, &group_bbox_max));
            qmdl_compat_note_bbox_declared(compat, &group_bbox_min, &group_bbox_max);

            prev_time = 0;
            prev_delta = 0;
            for (j = 0u; j < count; ++j)
            {
                QMDL_COMPAT_TRY(qmdl_read_fx16_f32_le(&cur, &time_value));
                QMDL_COMPAT_TRY(qmdl_compat_note_time(compat, time_value, j, &prev_time, &prev_delta, QMDL_INVALID_INDEX, i));
            }
        }

        QMDL_COMPAT_TRY(qmdl_u32_add(&compat->num_poses, count));
        if (raw_type != QMDL_FRAME_SINGLE && count > compat->max_frame_group_poses)
        {
            compat->max_frame_group_poses = count;
        }

        for (j = 0u; j < count; ++j)
        {
            QMDL_COMPAT_TRY(qmdl_read_trivertx_compat(&cur, &bbox_min));
            QMDL_COMPAT_TRY(qmdl_read_trivertx_compat(&cur, &bbox_max));
            qmdl_compat_note_bbox_declared(compat, &bbox_min, &bbox_max);
            qmdl_compat_box_begin(&actual_min, &actual_max, &have_actual);
            QMDL_COMPAT_TRY(qmdl_read_bytes(&cur, tmp_name, 16u));
            if (!qmdl_compat_name16_has_nul(tmp_name))
            {
                QMDL_COMPAT_TRY(qmdl_u32_add(&compat->nonnul_frame_name_count, 1u));
            }
            for (k = 0u; k < header.numverts; ++k)
            {
                QMDL_COMPAT_TRY(qmdl_read_trivertx_compat(&cur, &tmp_vert));
                qmdl_compat_box_include(&actual_min, &actual_max, &have_actual, &tmp_vert);
                if (!qmdl_vertex_normal_is_valid(tmp_vert.lightnormalindex))
                {
                    qmdl_compat_add_frame_flag(compat, i, QMDL_QCOMPAT_LIGHTNORMAL_OUT_OF_RANGE);
                }
            }
            qmdl_compat_check_bbox_contains(compat, &bbox_min, &bbox_max,
                                            &actual_min, &actual_max, have_actual);
            if (raw_type != QMDL_FRAME_SINGLE)
            {
                qmdl_compat_check_bbox_contains(compat, &group_bbox_min, &group_bbox_max,
                                                &actual_min, &actual_max, have_actual);
            }
        }
    }

    compat->bytes_consumed = (size_t)(cur.ptr - (const qmdl_u8 *)data);
    if (data_size > compat->bytes_consumed)
    {
        compat->trailing_bytes = data_size - compat->bytes_consumed;
        qmdl_compat_add_flag(compat, QMDL_QCOMPAT_TRAILING_BYTES);
    }

    qmdl_compat_apply_pose_limits(compat);
    qmdl_compat_apply_engine_matrix(compat);

    tmp_u32 = skin_pixels_per_image;
    (void)tmp_u32;

#undef QMDL_COMPAT_TRY

    return QMDL_OK;
}

int qmdl_model_quake_compat(const qmdl_model *model,
                            qmdl_quake_compat_info *compat)
{
    qmdl_u32 i;
    qmdl_u32 j;
    qmdl_u32 k;
    qmdl_fx16 prev_time;
    qmdl_fx16 prev_delta;
    qmdl_fx16 time_value;
    const qmdl_skin *skin;
    const qmdl_frame *frame;
    const qmdl_subframe *subframe;
    const qmdl_trivertx *vert;
    qmdl_trivertx actual_min;
    qmdl_trivertx actual_max;
    qmdl_u32 have_actual;
    int rc;

    if (model == 0 || compat == 0)
    {
        return QMDL_ERR_ARGS;
    }

    qmdl_quake_compat_info_clear(compat);
    qmdl_compat_apply_header(compat, &model->header);
    if ((compat->flags2 & (QMDL_QCOMPAT2_SIGNED_HEADER_FIELD_NEGATIVE |
                            QMDL_QCOMPAT2_SKINSIZE_SIGNED_OVERFLOW |
                            QMDL_QCOMPAT2_SKINSIZE_U32_OVERFLOW)) != 0u)
    {
        qmdl_compat_apply_engine_matrix(compat);
        return QMDL_OK;
    }

    if (model->num_subframes != 0u && model->subframes == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (model->num_packed_verts != 0u && model->packed_verts == 0)
    {
        return QMDL_ERR_ARGS;
    }

    if (model->header.numskins != 0u && model->skins == 0)
    {
        return QMDL_ERR_ARGS;
    }
    for (i = 0u; i < model->header.numskins; ++i)
    {
        skin = &model->skins[i];
        if (skin->type == QMDL_SKIN_SINGLE)
        {
            rc = qmdl_u32_add(&compat->num_skin_images, 1u);
            if (rc != QMDL_OK) return rc;
        }
        else
        {
            if (skin->type != QMDL_SKIN_GROUP)
            {
                qmdl_compat_add_skin_flag(compat, i, QMDL_QCOMPAT_NONCANONICAL_TYPE);
            }
            if (qmdl_compat_i32_is_negative_bits(skin->num_images))
            {
                qmdl_compat_add_skin_flag2(compat, i, QMDL_QCOMPAT2_SIGNED_GROUP_COUNT_NEGATIVE);
                continue;
            }
            if (skin->num_images == 0u)
            {
                qmdl_compat_add_skin_flag(compat, i, QMDL_QCOMPAT_GROUP_COUNT_ZERO);
                continue;
            }
            if (model->skin_intervals == 0 || skin->first_interval > model->num_skin_intervals ||
                skin->num_images > model->num_skin_intervals - skin->first_interval)
            {
                return QMDL_ERR_RANGE;
            }
            rc = qmdl_u32_add(&compat->num_skin_images, skin->num_images);
            if (rc != QMDL_OK) return rc;
            if (skin->num_images > compat->max_skin_group_images)
            {
                compat->max_skin_group_images = skin->num_images;
            }
            if (skin->num_images > 4u)
            {
                qmdl_compat_add_skin_group_over_4(compat, i, skin->num_images);
            }
            prev_time = 0;
            prev_delta = 0;
            for (j = 0u; j < skin->num_images; ++j)
            {
                time_value = model->skin_intervals[skin->first_interval + j];
                rc = qmdl_compat_note_time(compat, time_value, j, &prev_time, &prev_delta, i, QMDL_INVALID_INDEX);
                if (rc != QMDL_OK) return rc;
                qmdl_compat_note_gl_skin_time(compat, time_value, j, i);
            }
        }
    }

    if (model->header.numverts != 0u && model->stverts == 0)
    {
        return QMDL_ERR_ARGS;
    }
    for (i = 0u; i < model->header.numverts; ++i)
    {
        if (model->stverts[i].onseam != 0 &&
            model->stverts[i].onseam != (qmdl_i32)QMDL_ONSEAM_FLAG)
        {
            qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_ONSEAM_NONCANONICAL);
        }
        if (model->stverts[i].s < 0 ||
            (model->header.skinwidth != 0u && (qmdl_u32)model->stverts[i].s >= model->header.skinwidth) ||
            model->stverts[i].t < 0 ||
            (model->header.skinheight != 0u && (qmdl_u32)model->stverts[i].t >= model->header.skinheight))
        {
            qmdl_compat_add_flag(compat, QMDL_QCOMPAT_ST_OUT_OF_RANGE);
        }
    }

    if (model->header.numtris != 0u && model->triangles == 0)
    {
        return QMDL_ERR_ARGS;
    }
    for (i = 0u; i < model->header.numtris; ++i)
    {
        if (model->triangles[i].facesfront != 0 &&
            model->triangles[i].facesfront != 1)
        {
            qmdl_compat_add_flag2(compat, QMDL_QCOMPAT2_FACESFRONT_NONCANONICAL);
        }
        for (j = 0u; j < 3u; ++j)
        {
            if (model->triangles[i].vertindex[j] < 0 ||
                (qmdl_u32)model->triangles[i].vertindex[j] >= model->header.numverts)
            {
                qmdl_compat_add_flag(compat, QMDL_QCOMPAT_TRIANGLE_INDEX_OUT_OF_RANGE);
            }
        }
    }

    if (model->header.numframes != 0u && model->frames == 0)
    {
        return QMDL_ERR_ARGS;
    }
    for (i = 0u; i < model->header.numframes; ++i)
    {
        frame = &model->frames[i];
        if (frame->type == QMDL_FRAME_SINGLE)
        {
            rc = qmdl_u32_add(&compat->num_poses, 1u);
            if (rc != QMDL_OK) return rc;
            if (frame->first_subframe >= model->num_subframes)
            {
                return QMDL_ERR_RANGE;
            }
            qmdl_compat_note_bbox_declared(compat, &frame->bboxmin, &frame->bboxmax);
            subframe = &model->subframes[frame->first_subframe];
            if (!qmdl_compat_name16_has_nul(subframe->name))
            {
                rc = qmdl_u32_add(&compat->nonnul_frame_name_count, 1u);
                if (rc != QMDL_OK) return rc;
            }
            qmdl_compat_note_bbox_declared(compat, &subframe->bboxmin, &subframe->bboxmax);
            if (subframe->first_vert > model->num_packed_verts ||
                model->header.numverts > model->num_packed_verts - subframe->first_vert)
            {
                return QMDL_ERR_RANGE;
            }
            qmdl_compat_box_begin(&actual_min, &actual_max, &have_actual);
            for (k = 0u; k < model->header.numverts; ++k)
            {
                vert = &model->packed_verts[subframe->first_vert + k];
                qmdl_compat_box_include(&actual_min, &actual_max, &have_actual, vert);
                if (!qmdl_vertex_normal_is_valid(vert->lightnormalindex))
                {
                    qmdl_compat_add_frame_flag(compat, i, QMDL_QCOMPAT_LIGHTNORMAL_OUT_OF_RANGE);
                }
            }
            qmdl_compat_check_bbox_contains(compat, &subframe->bboxmin, &subframe->bboxmax,
                                            &actual_min, &actual_max, have_actual);
            qmdl_compat_check_bbox_contains(compat, &frame->bboxmin, &frame->bboxmax,
                                            &actual_min, &actual_max, have_actual);
        }
        else
        {
            if (frame->type != QMDL_FRAME_GROUP)
            {
                qmdl_compat_add_frame_flag(compat, i, QMDL_QCOMPAT_NONCANONICAL_TYPE);
            }
            if (qmdl_compat_i32_is_negative_bits(frame->num_subframes))
            {
                qmdl_compat_add_frame_flag2(compat, i, QMDL_QCOMPAT2_SIGNED_GROUP_COUNT_NEGATIVE);
                continue;
            }
            if (frame->num_subframes == 0u)
            {
                qmdl_compat_add_frame_flag(compat, i, QMDL_QCOMPAT_GROUP_COUNT_ZERO);
                continue;
            }
            if (model->frame_intervals == 0 || frame->first_interval > model->num_frame_intervals ||
                frame->num_subframes > model->num_frame_intervals - frame->first_interval)
            {
                return QMDL_ERR_RANGE;
            }
            if (frame->first_subframe > model->num_subframes ||
                frame->num_subframes > model->num_subframes - frame->first_subframe)
            {
                return QMDL_ERR_RANGE;
            }
            qmdl_compat_note_bbox_declared(compat, &frame->bboxmin, &frame->bboxmax);
            rc = qmdl_u32_add(&compat->num_poses, frame->num_subframes);
            if (rc != QMDL_OK) return rc;
            if (frame->num_subframes > compat->max_frame_group_poses)
            {
                compat->max_frame_group_poses = frame->num_subframes;
            }
            prev_time = 0;
            prev_delta = 0;
            for (j = 0u; j < frame->num_subframes; ++j)
            {
                time_value = model->frame_intervals[frame->first_interval + j];
                rc = qmdl_compat_note_time(compat, time_value, j, &prev_time, &prev_delta, QMDL_INVALID_INDEX, i);
                if (rc != QMDL_OK) return rc;
                subframe = &model->subframes[frame->first_subframe + j];
                if (!qmdl_compat_name16_has_nul(subframe->name))
                {
                    rc = qmdl_u32_add(&compat->nonnul_frame_name_count, 1u);
                    if (rc != QMDL_OK) return rc;
                }
                qmdl_compat_note_bbox_declared(compat, &subframe->bboxmin, &subframe->bboxmax);
                if (subframe->first_vert > model->num_packed_verts ||
                    model->header.numverts > model->num_packed_verts - subframe->first_vert)
                {
                    return QMDL_ERR_RANGE;
                }
                qmdl_compat_box_begin(&actual_min, &actual_max, &have_actual);
                for (k = 0u; k < model->header.numverts; ++k)
                {
                    vert = &model->packed_verts[subframe->first_vert + k];
                    qmdl_compat_box_include(&actual_min, &actual_max, &have_actual, vert);
                    if (!qmdl_vertex_normal_is_valid(vert->lightnormalindex))
                    {
                        qmdl_compat_add_frame_flag(compat, i, QMDL_QCOMPAT_LIGHTNORMAL_OUT_OF_RANGE);
                    }
                }
                qmdl_compat_check_bbox_contains(compat, &subframe->bboxmin, &subframe->bboxmax,
                                                &actual_min, &actual_max, have_actual);
                qmdl_compat_check_bbox_contains(compat, &frame->bboxmin, &frame->bboxmax,
                                                &actual_min, &actual_max, have_actual);
            }
        }
    }

    qmdl_compat_apply_pose_limits(compat);
    qmdl_compat_apply_engine_matrix(compat);
    return QMDL_OK;
}
