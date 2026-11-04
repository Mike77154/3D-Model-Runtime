#include "md4_internal.h"

#define MD4_SP_STAGE_IDLE 0
#define MD4_SP_STAGE_HEADER 1
#define MD4_SP_STAGE_BONE_NAME 2
#define MD4_SP_STAGE_FRAME_BASE 3
#define MD4_SP_STAGE_FRAME_BONE 4
#define MD4_SP_STAGE_LOD_HEADER 5
#define MD4_SP_STAGE_SURFACE_HEADER 6
#define MD4_SP_STAGE_SURFACE_TRIANGLE 7
#define MD4_SP_STAGE_SURFACE_VERTEX_BASE 8
#define MD4_SP_STAGE_SURFACE_VERTEX_WEIGHT 9
#define MD4_SP_STAGE_SURFACE_BONE_REF 10
#define MD4_SP_STAGE_SKIP_TO_END 11
#define MD4_SP_STAGE_DONE 12

static void md4_stream_default_opts(md4_decode_opts_t *opts)
{
    if (opts == 0) {
        return;
    }
    opts->header_mode = MD4_HEADER_AUTO;
    opts->vertex_mode = MD4_VERT_RUNTIME;
    opts->strict = 1;
}

static md4_i32 md4_stream_strict(const md4_stream_parser_t *parser)
{
    if (parser != 0 && parser->opts.strict == 0) {
        return 0;
    }
    return 1;
}

static md4_i32 md4_sp_try_header_prefix(const md4_u8 *data, md4_u32 size,
                                        md4_header_mode_t mode,
                                        md4_i32 *out_num_frames,
                                        md4_i32 *out_num_bones,
                                        md4_i32 *out_num_lods,
                                        md4_i32 *out_ofs_bone_names,
                                        md4_i32 *out_ofs_frames,
                                        md4_i32 *out_ofs_lods,
                                        md4_i32 *out_ofs_end,
                                        md4_i32 *out_frame_stride,
                                        md4_i32 *out_frame_has_name)
{
    md4_i32 header_size;
    md4_i32 num_frames;
    md4_i32 num_bones;
    md4_i32 num_lods;
    md4_i32 ofs_bone_names;
    md4_i32 ofs_frames;
    md4_i32 ofs_lods;
    md4_i32 ofs_end;
    md4_i32 frames_block_size;
    md4_i32 frame_stride;
    md4_i32 frame_base;
    md4_u32 ident;
    md4_i32 version;

    header_size = (mode == MD4_HEADER_EXTENDED) ? MD4_HEADER_EXTENDED_SIZE : MD4_HEADER_LEGACY_SIZE;
    if (size < (md4_u32)header_size) {
        return 0;
    }

    ident = md4_read_u32_le(data + 0U);
    version = md4_read_i32_le(data + 4U);
    if (ident != MD4_IDENT_U32 || version != MD4_VERSION_I32) {
        return 0;
    }

    num_frames = md4_read_i32_le(data + 72U);
    num_bones = md4_read_i32_le(data + 76U);
    if (mode == MD4_HEADER_EXTENDED) {
        ofs_bone_names = md4_read_i32_le(data + 80U);
        ofs_frames = md4_read_i32_le(data + 84U);
        num_lods = md4_read_i32_le(data + 88U);
        ofs_lods = md4_read_i32_le(data + 92U);
        ofs_end = md4_read_i32_le(data + 96U);
    } else {
        ofs_bone_names = 0;
        ofs_frames = md4_read_i32_le(data + 80U);
        num_lods = md4_read_i32_le(data + 84U);
        ofs_lods = md4_read_i32_le(data + 88U);
        ofs_end = md4_read_i32_le(data + 92U);
    }

    if (num_frames <= 0 || num_frames > MD4_CFG_MAX_FRAMES) {
        return 0;
    }
    if (num_bones < 0 || num_bones > MD4_CFG_MAX_BONES) {
        return 0;
    }
    if (num_lods <= 0 || num_lods > MD4_CFG_MAX_LODS) {
        return 0;
    }
    if (ofs_frames < header_size || ofs_lods <= ofs_frames || ofs_end <= ofs_lods) {
        return 0;
    }
    if (mode == MD4_HEADER_EXTENDED && ofs_bone_names != 0) {
        if (ofs_bone_names < header_size || ofs_bone_names >= ofs_frames) {
            return 0;
        }
    }

    frames_block_size = ofs_lods - ofs_frames;
    if (frames_block_size <= 0 || (frames_block_size % num_frames) != 0) {
        return 0;
    }
    frame_stride = frames_block_size / num_frames;
    frame_base = frame_stride - (num_bones * MD4_BONE_SIZE);
    if (frame_base != MD4_FRAME_BASE_NO_NAME && frame_base != MD4_FRAME_BASE_WITH_NAME) {
        return 0;
    }

    *out_num_frames = num_frames;
    *out_num_bones = num_bones;
    *out_num_lods = num_lods;
    *out_ofs_bone_names = ofs_bone_names;
    *out_ofs_frames = ofs_frames;
    *out_ofs_lods = ofs_lods;
    *out_ofs_end = ofs_end;
    *out_frame_stride = frame_stride;
    *out_frame_has_name = (frame_base == MD4_FRAME_BASE_WITH_NAME) ? 1 : 0;
    return 1;
}

static md4_rc_t md4_sp_emit_header(md4_stream_parser_t *parser)
{
    md4_stream_header_info_t info;

    memset(&info, 0, sizeof(info));
    md4_copy_qpath(info.name, parser->scratch + 8U);
    info.layout.header_mode = parser->detected_header_mode;
    info.layout.vertex_mode = parser->selected_vertex_mode;
    info.layout.frame_has_name = parser->frame_has_name;
    info.layout.has_bone_names = (parser->ofs_bone_names != 0) ? 1 : 0;
    info.layout.vertex_has_rest_pos = (parser->selected_vertex_mode == MD4_VERT_PADDED ||
                                       parser->selected_vertex_mode == MD4_VERT_DRAFT) ? 1 : 0;
    info.layout.weights_have_offset = (parser->selected_vertex_mode != MD4_VERT_DRAFT) ? 1 : 0;
    info.num_frames = parser->num_frames;
    info.num_bones = parser->num_bones;
    info.num_lods = parser->num_lods;
    info.ofs_bone_names = parser->ofs_bone_names;
    info.ofs_frames = parser->ofs_frames;
    info.ofs_lods = parser->ofs_lods;
    info.ofs_end = parser->ofs_end;
    if (parser->callbacks.on_header != 0) {
        return parser->callbacks.on_header(parser->user, &info);
    }
    return MD4_RC_OK;
}

static void md4_sp_prepare_after_header(md4_stream_parser_t *parser)
{
    parser->bone_name_index = 0;
    parser->frame_index = 0;
    parser->bone_index_in_frame = 0;
    parser->lod_index = 0;
    parser->lod_abs_start = (md4_u32)parser->ofs_lods;
    parser->surface_index_in_lod = 0;
    parser->surface_index_global = 0;
    parser->triangle_index = 0;
    parser->vertex_index_in_surface = 0;
    parser->weight_index_in_vertex = 0;
    parser->bone_ref_index = 0;
    if (parser->ofs_bone_names != 0 && parser->num_bones > 0) {
        parser->stage = MD4_SP_STAGE_BONE_NAME;
    } else {
        parser->stage = MD4_SP_STAGE_FRAME_BASE;
    }
}

static md4_rc_t md4_sp_complete(md4_stream_parser_t *parser)
{
    md4_rc_t rc;

    parser->complete = 1;
    parser->stage = MD4_SP_STAGE_DONE;
    if (parser->callbacks.on_end != 0) {
        rc = parser->callbacks.on_end(parser->user);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_sp_process_header(md4_stream_parser_t *parser,
                                      const md4_u8 **chunk,
                                      md4_u32 *chunk_size)
{
    md4_u32 to_copy;
    md4_i32 num_frames;
    md4_i32 num_bones;
    md4_i32 num_lods;
    md4_i32 ofs_bone_names;
    md4_i32 ofs_frames;
    md4_i32 ofs_lods;
    md4_i32 ofs_end;
    md4_i32 frame_stride;
    md4_i32 frame_has_name;
    md4_i32 have_legacy;
    md4_i32 have_ext;
    md4_rc_t rc;

    while (parser->scratch_used < MD4_HEADER_LEGACY_SIZE && *chunk_size > 0U) {
        to_copy = (md4_u32)MD4_HEADER_LEGACY_SIZE - parser->scratch_used;
        if (to_copy > *chunk_size) {
            to_copy = *chunk_size;
        }
        memcpy(parser->scratch + parser->scratch_used, *chunk, (size_t)to_copy);
        parser->scratch_used += to_copy;
        parser->bytes_consumed += to_copy;
        *chunk += to_copy;
        *chunk_size -= to_copy;
    }
    if (parser->scratch_used < MD4_HEADER_LEGACY_SIZE) {
        return MD4_RC_OK;
    }

    if (md4_read_u32_le(parser->scratch + 0U) != MD4_IDENT_U32) {
        return MD4_RC_BAD_IDENT;
    }
    if (md4_read_i32_le(parser->scratch + 4U) != MD4_VERSION_I32) {
        return MD4_RC_BAD_VERSION;
    }

    have_legacy = md4_sp_try_header_prefix(parser->scratch, parser->scratch_used,
                                           MD4_HEADER_LEGACY,
                                           &num_frames, &num_bones, &num_lods,
                                           &ofs_bone_names, &ofs_frames, &ofs_lods,
                                           &ofs_end, &frame_stride, &frame_has_name);
    if (parser->opts.header_mode == MD4_HEADER_LEGACY ||
        (parser->opts.header_mode == MD4_HEADER_AUTO && have_legacy)) {
        if (!have_legacy) {
            return MD4_RC_BAD_OFFSETS;
        }
        parser->detected_header_mode = MD4_HEADER_LEGACY;
        parser->num_frames = num_frames;
        parser->num_bones = num_bones;
        parser->num_lods = num_lods;
        parser->ofs_bone_names = ofs_bone_names;
        parser->ofs_frames = ofs_frames;
        parser->ofs_lods = ofs_lods;
        parser->ofs_end = ofs_end;
        parser->expected_size = (md4_u32)ofs_end;
        parser->frame_stride = frame_stride;
        parser->frame_has_name = frame_has_name;
        parser->frame_base_size = frame_has_name ? MD4_FRAME_BASE_WITH_NAME : MD4_FRAME_BASE_NO_NAME;
        parser->header_ready = 1;
        rc = md4_sp_emit_header(parser);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        md4_sp_prepare_after_header(parser);
        parser->scratch_used = 0U;
        return MD4_RC_OK;
    }

    while (parser->scratch_used < MD4_HEADER_EXTENDED_SIZE && *chunk_size > 0U) {
        to_copy = (md4_u32)MD4_HEADER_EXTENDED_SIZE - parser->scratch_used;
        if (to_copy > *chunk_size) {
            to_copy = *chunk_size;
        }
        memcpy(parser->scratch + parser->scratch_used, *chunk, (size_t)to_copy);
        parser->scratch_used += to_copy;
        parser->bytes_consumed += to_copy;
        *chunk += to_copy;
        *chunk_size -= to_copy;
    }
    if (parser->scratch_used < MD4_HEADER_EXTENDED_SIZE) {
        return MD4_RC_OK;
    }

    have_ext = md4_sp_try_header_prefix(parser->scratch, parser->scratch_used,
                                        MD4_HEADER_EXTENDED,
                                        &num_frames, &num_bones, &num_lods,
                                        &ofs_bone_names, &ofs_frames, &ofs_lods,
                                        &ofs_end, &frame_stride, &frame_has_name);
    if (!have_ext) {
        return MD4_RC_BAD_OFFSETS;
    }

    parser->detected_header_mode = MD4_HEADER_EXTENDED;
    parser->num_frames = num_frames;
    parser->num_bones = num_bones;
    parser->num_lods = num_lods;
    parser->ofs_bone_names = ofs_bone_names;
    parser->ofs_frames = ofs_frames;
    parser->ofs_lods = ofs_lods;
    parser->ofs_end = ofs_end;
    parser->expected_size = (md4_u32)ofs_end;
    parser->frame_stride = frame_stride;
    parser->frame_has_name = frame_has_name;
    parser->frame_base_size = frame_has_name ? MD4_FRAME_BASE_WITH_NAME : MD4_FRAME_BASE_NO_NAME;
    parser->header_ready = 1;
    rc = md4_sp_emit_header(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    md4_sp_prepare_after_header(parser);
    parser->scratch_used = 0U;
    return MD4_RC_OK;
}

static md4_u32 md4_sp_item_abs(const md4_stream_parser_t *parser)
{
    switch (parser->stage) {
    case MD4_SP_STAGE_BONE_NAME:
        return (md4_u32)parser->ofs_bone_names + (md4_u32)(parser->bone_name_index * MD4_CFG_MAX_QPATH);
    case MD4_SP_STAGE_FRAME_BASE:
        return (md4_u32)parser->ofs_frames + (md4_u32)(parser->frame_index * parser->frame_stride);
    case MD4_SP_STAGE_FRAME_BONE:
        return (md4_u32)parser->ofs_frames + (md4_u32)(parser->frame_index * parser->frame_stride) +
               (md4_u32)parser->frame_base_size + (md4_u32)(parser->bone_index_in_frame * MD4_BONE_SIZE);
    case MD4_SP_STAGE_LOD_HEADER:
        return parser->lod_abs_start;
    case MD4_SP_STAGE_SURFACE_HEADER:
        return parser->surface_abs_start;
    case MD4_SP_STAGE_SURFACE_TRIANGLE:
        return parser->tri_abs_start + (md4_u32)(parser->triangle_index * MD4_TRIANGLE_SIZE);
    case MD4_SP_STAGE_SURFACE_VERTEX_BASE:
        return parser->vertex_abs_start;
    case MD4_SP_STAGE_SURFACE_VERTEX_WEIGHT:
        return parser->vertex_abs_start + (md4_u32)parser->vertex_base_size +
               (md4_u32)(parser->weight_index_in_vertex * parser->weight_size);
    case MD4_SP_STAGE_SURFACE_BONE_REF:
        return parser->bone_ref_abs_start + (md4_u32)(parser->bone_ref_index * 4);
    default:
        return parser->bytes_consumed;
    }
}

static md4_u32 md4_sp_item_size(const md4_stream_parser_t *parser)
{
    switch (parser->stage) {
    case MD4_SP_STAGE_BONE_NAME:
        return MD4_CFG_MAX_QPATH;
    case MD4_SP_STAGE_FRAME_BASE:
        return (md4_u32)parser->frame_base_size;
    case MD4_SP_STAGE_FRAME_BONE:
        return MD4_BONE_SIZE;
    case MD4_SP_STAGE_LOD_HEADER:
        return MD4_LOD_SIZE;
    case MD4_SP_STAGE_SURFACE_HEADER:
        return MD4_SURFACE_HEADER_SIZE;
    case MD4_SP_STAGE_SURFACE_TRIANGLE:
        return MD4_TRIANGLE_SIZE;
    case MD4_SP_STAGE_SURFACE_VERTEX_BASE:
        return (md4_u32)parser->vertex_base_size;
    case MD4_SP_STAGE_SURFACE_VERTEX_WEIGHT:
        return (md4_u32)parser->weight_size;
    case MD4_SP_STAGE_SURFACE_BONE_REF:
        return 4U;
    default:
        return 0U;
    }
}

static void md4_sp_next_lod_or_end(md4_stream_parser_t *parser)
{
    parser->lod_index += 1;
    parser->lod_abs_start = parser->lod_abs_end;
    if (parser->lod_index < parser->num_lods) {
        parser->stage = MD4_SP_STAGE_LOD_HEADER;
    } else {
        parser->stage = MD4_SP_STAGE_SKIP_TO_END;
    }
}

static void md4_sp_next_surface_or_lod(md4_stream_parser_t *parser)
{
    parser->surface_index_in_lod += 1;
    parser->surface_index_global += 1;
    parser->surface_abs_start = parser->surface_abs_end;
    if (parser->surface_index_in_lod < parser->lod_num_surfaces) {
        parser->stage = MD4_SP_STAGE_SURFACE_HEADER;
    } else {
        md4_sp_next_lod_or_end(parser);
    }
}

static void md4_sp_next_vertex_or_surface_tail(md4_stream_parser_t *parser)
{
    parser->vertex_abs_start += (md4_u32)parser->vertex_base_size +
                                (md4_u32)(parser->vertex_num_weights * parser->weight_size);
    parser->vertex_index_in_surface += 1;
    parser->weight_index_in_vertex = 0;
    if (parser->vertex_index_in_surface < parser->surface_num_vertices) {
        parser->stage = MD4_SP_STAGE_SURFACE_VERTEX_BASE;
    } else if (parser->surface_num_bone_refs > 0) {
        parser->stage = MD4_SP_STAGE_SURFACE_BONE_REF;
    } else {
        md4_sp_next_surface_or_lod(parser);
    }
}

static md4_rc_t md4_sp_emit_bone_name(md4_stream_parser_t *parser)
{
    md4_stream_bone_name_info_t info;

    memset(&info, 0, sizeof(info));
    info.bone_index = parser->bone_name_index;
    md4_copy_qpath(info.name, parser->scratch);
    if (parser->callbacks.on_bone_name != 0) {
        return parser->callbacks.on_bone_name(parser->user, &info);
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_sp_emit_frame(md4_stream_parser_t *parser)
{
    md4_stream_frame_info_t info;
    md4_u32 base_off;
    md4_i32 i;

    memset(&info, 0, sizeof(info));
    info.frame_index = parser->frame_index;
    for (i = 0; i < 3; ++i) {
        info.bounds[0][i] = md4_read_fx32_from_f32_le(parser->scratch + (md4_u32)(i * 4));
        info.bounds[1][i] = md4_read_fx32_from_f32_le(parser->scratch + 12U + (md4_u32)(i * 4));
        info.local_origin[i] = md4_read_fx32_from_f32_le(parser->scratch + 24U + (md4_u32)(i * 4));
    }
    info.radius = md4_read_fx32_from_f32_le(parser->scratch + 36U);
    base_off = parser->frame_has_name ? 40U : 0U;
    if (parser->frame_has_name) {
        md4_copy_frame_name(info.name, parser->scratch + base_off, 1);
    }
    if (parser->callbacks.on_frame != 0) {
        return parser->callbacks.on_frame(parser->user, &info);
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_sp_emit_bone(md4_stream_parser_t *parser)
{
    md4_stream_bone_info_t info;
    md4_i32 r;
    md4_i32 c;

    memset(&info, 0, sizeof(info));
    info.frame_index = parser->frame_index;
    info.bone_index = parser->bone_index_in_frame;
    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 4; ++c) {
            info.matrix[r][c] = md4_read_fx32_from_f32_le(parser->scratch + (md4_u32)((r * 4 + c) * 4));
        }
    }
    if (parser->callbacks.on_bone != 0) {
        return parser->callbacks.on_bone(parser->user, &info);
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_sp_emit_lod(md4_stream_parser_t *parser)
{
    md4_stream_lod_info_t info;

    memset(&info, 0, sizeof(info));
    info.lod_index = parser->lod_index;
    info.num_surfaces = parser->lod_num_surfaces;
    if (parser->callbacks.on_lod != 0) {
        return parser->callbacks.on_lod(parser->user, &info);
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_sp_emit_surface(md4_stream_parser_t *parser)
{
    md4_stream_surface_info_t info;

    memset(&info, 0, sizeof(info));
    info.lod_index = parser->lod_index;
    info.surface_index_in_lod = parser->surface_index_in_lod;
    info.surface_index_global = parser->surface_index_global;
    md4_copy_qpath(info.name, parser->scratch + 4U);
    md4_copy_qpath(info.shader, parser->scratch + 68U);
    info.shader_index = md4_read_i32_le(parser->scratch + 132U);
    info.ofs_header_raw = md4_read_i32_le(parser->scratch + 136U);
    info.num_vertices = parser->surface_num_vertices;
    info.num_triangles = parser->surface_num_triangles;
    info.num_bone_refs = parser->surface_num_bone_refs;
    if (parser->callbacks.on_surface != 0) {
        return parser->callbacks.on_surface(parser->user, &info);
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_sp_emit_triangle(md4_stream_parser_t *parser)
{
    md4_stream_triangle_info_t info;

    memset(&info, 0, sizeof(info));
    info.lod_index = parser->lod_index;
    info.surface_index_in_lod = parser->surface_index_in_lod;
    info.surface_index_global = parser->surface_index_global;
    info.triangle_index = parser->triangle_index;
    info.indexes[0] = md4_read_i32_le(parser->scratch + 0U);
    info.indexes[1] = md4_read_i32_le(parser->scratch + 4U);
    info.indexes[2] = md4_read_i32_le(parser->scratch + 8U);
    if (parser->callbacks.on_triangle != 0) {
        return parser->callbacks.on_triangle(parser->user, &info);
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_sp_emit_vertex(md4_stream_parser_t *parser)
{
    md4_stream_vertex_info_t info;

    memset(&info, 0, sizeof(info));
    info.lod_index = parser->lod_index;
    info.surface_index_in_lod = parser->surface_index_in_lod;
    info.surface_index_global = parser->surface_index_global;
    info.vertex_index_in_surface = parser->vertex_index_in_surface;
    if (parser->selected_vertex_mode == MD4_VERT_RUNTIME) {
        info.normal[0] = md4_read_fx32_from_f32_le(parser->scratch + 0U);
        info.normal[1] = md4_read_fx32_from_f32_le(parser->scratch + 4U);
        info.normal[2] = md4_read_fx32_from_f32_le(parser->scratch + 8U);
        info.uv[0] = md4_read_fx32_from_f32_le(parser->scratch + 12U);
        info.uv[1] = md4_read_fx32_from_f32_le(parser->scratch + 16U);
        info.num_weights = md4_read_i32_le(parser->scratch + 20U);
    } else {
        info.rest_pos[0] = md4_read_fx32_from_f32_le(parser->scratch + 0U);
        info.rest_pos[1] = md4_read_fx32_from_f32_le(parser->scratch + 4U);
        info.rest_pos[2] = md4_read_fx32_from_f32_le(parser->scratch + 8U);
        info.normal[0] = md4_read_fx32_from_f32_le(parser->scratch + 12U);
        info.normal[1] = md4_read_fx32_from_f32_le(parser->scratch + 16U);
        info.normal[2] = md4_read_fx32_from_f32_le(parser->scratch + 20U);
        info.uv[0] = md4_read_fx32_from_f32_le(parser->scratch + 24U);
        info.uv[1] = md4_read_fx32_from_f32_le(parser->scratch + 28U);
        info.num_weights = md4_read_i32_le(parser->scratch + 32U);
    }
    if (parser->callbacks.on_vertex != 0) {
        return parser->callbacks.on_vertex(parser->user, &info);
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_sp_emit_weight(md4_stream_parser_t *parser)
{
    md4_stream_weight_info_t info;

    memset(&info, 0, sizeof(info));
    info.lod_index = parser->lod_index;
    info.surface_index_in_lod = parser->surface_index_in_lod;
    info.surface_index_global = parser->surface_index_global;
    info.vertex_index_in_surface = parser->vertex_index_in_surface;
    info.weight_index_in_vertex = parser->weight_index_in_vertex;
    info.bone_index = md4_read_i32_le(parser->scratch + 0U);
    info.bone_weight = md4_read_fx32_from_f32_le(parser->scratch + 4U);
    if (parser->selected_vertex_mode != MD4_VERT_DRAFT) {
        info.offset[0] = md4_read_fx32_from_f32_le(parser->scratch + 8U);
        info.offset[1] = md4_read_fx32_from_f32_le(parser->scratch + 12U);
        info.offset[2] = md4_read_fx32_from_f32_le(parser->scratch + 16U);
    }
    if (parser->callbacks.on_weight != 0) {
        return parser->callbacks.on_weight(parser->user, &info);
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_sp_emit_bone_ref(md4_stream_parser_t *parser)
{
    md4_stream_bone_ref_info_t info;

    memset(&info, 0, sizeof(info));
    info.lod_index = parser->lod_index;
    info.surface_index_in_lod = parser->surface_index_in_lod;
    info.surface_index_global = parser->surface_index_global;
    info.bone_ref_index = parser->bone_ref_index;
    info.bone_index = md4_read_i32_le(parser->scratch + 0U);
    if (parser->callbacks.on_bone_ref != 0) {
        return parser->callbacks.on_bone_ref(parser->user, &info);
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_sp_process_item(md4_stream_parser_t *parser)
{
    md4_rc_t rc;
    md4_i32 val0;
    md4_i32 val1;
    md4_i32 val2;
    md4_i32 val3;
    md4_i32 surf_ofs_triangles;
    md4_i32 surf_ofs_verts;
    md4_i32 surf_ofs_bone_refs;
    md4_i32 surf_ofs_end;

    rc = MD4_RC_OK;
    switch (parser->stage) {
    case MD4_SP_STAGE_BONE_NAME:
        rc = md4_sp_emit_bone_name(parser);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        parser->bone_name_index += 1;
        if (parser->bone_name_index < parser->num_bones) {
            parser->stage = MD4_SP_STAGE_BONE_NAME;
        } else {
            parser->stage = MD4_SP_STAGE_FRAME_BASE;
        }
        break;

    case MD4_SP_STAGE_FRAME_BASE:
        rc = md4_sp_emit_frame(parser);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        parser->bone_index_in_frame = 0;
        if (parser->num_bones > 0) {
            parser->stage = MD4_SP_STAGE_FRAME_BONE;
        } else {
            parser->frame_index += 1;
            if (parser->frame_index < parser->num_frames) {
                parser->stage = MD4_SP_STAGE_FRAME_BASE;
            } else {
                parser->stage = MD4_SP_STAGE_LOD_HEADER;
            }
        }
        break;

    case MD4_SP_STAGE_FRAME_BONE:
        rc = md4_sp_emit_bone(parser);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        parser->bone_index_in_frame += 1;
        if (parser->bone_index_in_frame < parser->num_bones) {
            parser->stage = MD4_SP_STAGE_FRAME_BONE;
        } else {
            parser->frame_index += 1;
            if (parser->frame_index < parser->num_frames) {
                parser->stage = MD4_SP_STAGE_FRAME_BASE;
            } else {
                parser->stage = MD4_SP_STAGE_LOD_HEADER;
            }
        }
        break;

    case MD4_SP_STAGE_LOD_HEADER:
        parser->lod_num_surfaces = md4_read_i32_le(parser->scratch + 0U);
        val0 = md4_read_i32_le(parser->scratch + 4U);
        val1 = md4_read_i32_le(parser->scratch + 8U);
        if (parser->lod_num_surfaces < 0 || parser->lod_num_surfaces > MD4_CFG_MAX_SURFACES) {
            return MD4_RC_BAD_COUNTS;
        }
        if (val0 < MD4_LOD_SIZE || val1 < val0) {
            return MD4_RC_BAD_OFFSETS;
        }
        parser->surface_abs_start = parser->lod_abs_start + (md4_u32)val0;
        parser->lod_abs_end = parser->lod_abs_start + (md4_u32)val1;
        if (parser->lod_abs_end > parser->expected_size || parser->lod_abs_end < parser->surface_abs_start) {
            return MD4_RC_BAD_OFFSETS;
        }
        rc = md4_sp_emit_lod(parser);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        parser->surface_index_in_lod = 0;
        if (parser->lod_num_surfaces > 0) {
            parser->stage = MD4_SP_STAGE_SURFACE_HEADER;
        } else {
            md4_sp_next_lod_or_end(parser);
        }
        break;

    case MD4_SP_STAGE_SURFACE_HEADER:
        if (md4_read_u32_le(parser->scratch + 0U) != MD4_IDENT_U32) {
            return MD4_RC_BAD_IDENT;
        }
        parser->surface_num_vertices = md4_read_i32_le(parser->scratch + 140U);
        surf_ofs_verts = md4_read_i32_le(parser->scratch + 144U);
        parser->surface_num_triangles = md4_read_i32_le(parser->scratch + 148U);
        surf_ofs_triangles = md4_read_i32_le(parser->scratch + 152U);
        parser->surface_num_bone_refs = md4_read_i32_le(parser->scratch + 156U);
        surf_ofs_bone_refs = md4_read_i32_le(parser->scratch + 160U);
        surf_ofs_end = md4_read_i32_le(parser->scratch + 164U);
        if (parser->surface_num_vertices < 0 || parser->surface_num_vertices > MD4_CFG_MAX_VERTICES) {
            return MD4_RC_BAD_COUNTS;
        }
        if (parser->surface_num_triangles < 0 || parser->surface_num_triangles > MD4_CFG_MAX_TRIANGLES) {
            return MD4_RC_BAD_COUNTS;
        }
        if (parser->surface_num_bone_refs < 0 || parser->surface_num_bone_refs > MD4_CFG_MAX_BONE_REFS) {
            return MD4_RC_BAD_COUNTS;
        }
        if (surf_ofs_end < MD4_SURFACE_HEADER_SIZE) {
            return MD4_RC_BAD_OFFSETS;
        }
        if (surf_ofs_triangles < MD4_SURFACE_HEADER_SIZE || surf_ofs_verts < MD4_SURFACE_HEADER_SIZE) {
            return MD4_RC_BAD_OFFSETS;
        }
        parser->surface_abs_end = parser->surface_abs_start + (md4_u32)surf_ofs_end;
        parser->tri_abs_start = parser->surface_abs_start + (md4_u32)surf_ofs_triangles;
        parser->vert_abs_start = parser->surface_abs_start + (md4_u32)surf_ofs_verts;
        parser->vertex_block_end_abs = parser->surface_abs_start +
            (md4_u32)md4_surface_vertex_block_end(surf_ofs_end, surf_ofs_bone_refs, parser->surface_num_bone_refs);
        parser->bone_ref_abs_start = parser->surface_abs_start + (md4_u32)surf_ofs_bone_refs;
        if (parser->surface_abs_end > parser->lod_abs_end || parser->surface_abs_end > parser->expected_size) {
            return MD4_RC_BAD_OFFSETS;
        }
        if (parser->tri_abs_start > parser->surface_abs_end || parser->vert_abs_start > parser->surface_abs_end) {
            return MD4_RC_BAD_OFFSETS;
        }
        if (parser->vertex_block_end_abs < parser->vert_abs_start || parser->vertex_block_end_abs > parser->surface_abs_end) {
            return MD4_RC_BAD_OFFSETS;
        }
        if (parser->surface_num_bone_refs > 0) {
            if (parser->bone_ref_abs_start < parser->vertex_block_end_abs || parser->bone_ref_abs_start > parser->surface_abs_end) {
                return MD4_RC_BAD_OFFSETS;
            }
        }
        rc = md4_sp_emit_surface(parser);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        parser->triangle_index = 0;
        parser->vertex_index_in_surface = 0;
        parser->vertex_abs_start = parser->vert_abs_start;
        parser->bone_ref_index = 0;
        if (parser->surface_num_triangles > 0) {
            parser->stage = MD4_SP_STAGE_SURFACE_TRIANGLE;
        } else if (parser->surface_num_vertices > 0) {
            parser->stage = MD4_SP_STAGE_SURFACE_VERTEX_BASE;
        } else if (parser->surface_num_bone_refs > 0) {
            parser->stage = MD4_SP_STAGE_SURFACE_BONE_REF;
        } else {
            md4_sp_next_surface_or_lod(parser);
        }
        break;

    case MD4_SP_STAGE_SURFACE_TRIANGLE:
        rc = md4_sp_emit_triangle(parser);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        if (md4_stream_strict(parser)) {
            val0 = md4_read_i32_le(parser->scratch + 0U);
            val1 = md4_read_i32_le(parser->scratch + 4U);
            val2 = md4_read_i32_le(parser->scratch + 8U);
            if (val0 < 0 || val1 < 0 || val2 < 0) {
                return MD4_RC_BAD_COUNTS;
            }
            if (val0 >= parser->surface_num_vertices || val1 >= parser->surface_num_vertices ||
                val2 >= parser->surface_num_vertices) {
                return MD4_RC_BAD_COUNTS;
            }
        }
        parser->triangle_index += 1;
        if (parser->triangle_index < parser->surface_num_triangles) {
            parser->stage = MD4_SP_STAGE_SURFACE_TRIANGLE;
        } else if (parser->surface_num_vertices > 0) {
            parser->stage = MD4_SP_STAGE_SURFACE_VERTEX_BASE;
        } else if (parser->surface_num_bone_refs > 0) {
            parser->stage = MD4_SP_STAGE_SURFACE_BONE_REF;
        } else {
            md4_sp_next_surface_or_lod(parser);
        }
        break;

    case MD4_SP_STAGE_SURFACE_VERTEX_BASE:
        if (parser->selected_vertex_mode == MD4_VERT_RUNTIME) {
            parser->vertex_num_weights = md4_read_i32_le(parser->scratch + 20U);
        } else {
            parser->vertex_num_weights = md4_read_i32_le(parser->scratch + 32U);
        }
        if (parser->vertex_num_weights < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (parser->vertex_abs_start + (md4_u32)parser->vertex_base_size +
            (md4_u32)(parser->vertex_num_weights * parser->weight_size) > parser->vertex_block_end_abs) {
            return MD4_RC_BAD_OFFSETS;
        }
        rc = md4_sp_emit_vertex(parser);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        parser->weight_index_in_vertex = 0;
        if (parser->vertex_num_weights > 0) {
            parser->stage = MD4_SP_STAGE_SURFACE_VERTEX_WEIGHT;
        } else {
            md4_sp_next_vertex_or_surface_tail(parser);
        }
        break;

    case MD4_SP_STAGE_SURFACE_VERTEX_WEIGHT:
        if (md4_stream_strict(parser)) {
            val0 = md4_read_i32_le(parser->scratch + 0U);
            if (val0 < 0) {
                return MD4_RC_BAD_COUNTS;
            }
        }
        rc = md4_sp_emit_weight(parser);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        parser->weight_index_in_vertex += 1;
        if (parser->weight_index_in_vertex < parser->vertex_num_weights) {
            parser->stage = MD4_SP_STAGE_SURFACE_VERTEX_WEIGHT;
        } else {
            md4_sp_next_vertex_or_surface_tail(parser);
        }
        break;

    case MD4_SP_STAGE_SURFACE_BONE_REF:
        val3 = md4_read_i32_le(parser->scratch + 0U);
        if (val3 < 0 || val3 >= parser->num_bones) {
            return MD4_RC_BAD_COUNTS;
        }
        rc = md4_sp_emit_bone_ref(parser);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        parser->bone_ref_index += 1;
        if (parser->bone_ref_index < parser->surface_num_bone_refs) {
            parser->stage = MD4_SP_STAGE_SURFACE_BONE_REF;
        } else {
            md4_sp_next_surface_or_lod(parser);
        }
        break;

    default:
        return MD4_RC_INTERNAL;
    }
    return MD4_RC_OK;
}

void md4_stream_parser_init(md4_stream_parser_t *parser)
{
    if (parser == 0) {
        return;
    }
    memset(parser, 0, sizeof(*parser));
    md4_stream_default_opts(&parser->opts);
    parser->stage = MD4_SP_STAGE_IDLE;
}

md4_rc_t md4_stream_parser_begin(md4_stream_parser_t *parser,
                                 const md4_decode_opts_t *opts,
                                 const md4_stream_callbacks_t *callbacks,
                                 void *user)
{
    if (parser == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    md4_stream_parser_init(parser);
    if (opts != 0) {
        parser->opts = *opts;
    }
    if (callbacks != 0) {
        parser->callbacks = *callbacks;
    }
    parser->user = user;
    if (parser->opts.vertex_mode == MD4_VERT_AUTO) {
        return MD4_RC_UNSUPPORTED;
    }
    parser->selected_vertex_mode = parser->opts.vertex_mode;
    if (parser->selected_vertex_mode == MD4_VERT_RUNTIME) {
        parser->vertex_base_size = MD4_RUNTIME_VERTEX_BASE;
        parser->weight_size = MD4_RUNTIME_WEIGHT_SIZE;
    } else if (parser->selected_vertex_mode == MD4_VERT_PADDED) {
        parser->vertex_base_size = MD4_PADDED_VERTEX_BASE;
        parser->weight_size = MD4_RUNTIME_WEIGHT_SIZE;
    } else if (parser->selected_vertex_mode == MD4_VERT_DRAFT) {
        parser->vertex_base_size = MD4_DRAFT_VERTEX_BASE;
        parser->weight_size = MD4_DRAFT_WEIGHT_SIZE;
    } else {
        return MD4_RC_BAD_ARGUMENT;
    }
    parser->stage = MD4_SP_STAGE_HEADER;
    return MD4_RC_OK;
}

md4_rc_t md4_stream_parser_push(md4_stream_parser_t *parser,
                                const void *chunk,
                                md4_u32 chunk_size)
{
    const md4_u8 *src;
    md4_u32 avail;
    md4_rc_t rc;
    md4_u32 target_abs;
    md4_u32 need;
    md4_u32 to_copy;
    md4_u32 to_skip;

    if (parser == 0 || (chunk == 0 && chunk_size != 0U)) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (parser->stage == MD4_SP_STAGE_IDLE) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (parser->complete && chunk_size > 0U) {
        return MD4_RC_BAD_OFFSETS;
    }

    src = (const md4_u8 *)chunk;
    avail = chunk_size;
    while (avail > 0U) {
        if (parser->stage == MD4_SP_STAGE_DONE) {
            return MD4_RC_BAD_OFFSETS;
        }
        if (parser->stage == MD4_SP_STAGE_HEADER) {
            rc = md4_sp_process_header(parser, &src, &avail);
            if (rc != MD4_RC_OK) {
                return rc;
            }
            continue;
        }
        if (parser->stage == MD4_SP_STAGE_SKIP_TO_END) {
            if (parser->bytes_consumed > parser->expected_size) {
                return MD4_RC_BAD_OFFSETS;
            }
            to_skip = parser->expected_size - parser->bytes_consumed;
            if (to_skip > avail) {
                to_skip = avail;
            }
            parser->bytes_consumed += to_skip;
            src += to_skip;
            avail -= to_skip;
            if (parser->bytes_consumed == parser->expected_size) {
                rc = md4_sp_complete(parser);
                if (rc != MD4_RC_OK) {
                    return rc;
                }
            }
            continue;
        }

        target_abs = md4_sp_item_abs(parser);
        if (parser->bytes_consumed < target_abs) {
            to_skip = target_abs - parser->bytes_consumed;
            if (to_skip > avail) {
                to_skip = avail;
            }
            parser->bytes_consumed += to_skip;
            src += to_skip;
            avail -= to_skip;
            continue;
        }

        need = md4_sp_item_size(parser);
        if (need == 0U || need > (md4_u32)sizeof(parser->scratch)) {
            return MD4_RC_INTERNAL;
        }
        to_copy = need - parser->scratch_used;
        if (to_copy > avail) {
            to_copy = avail;
        }
        memcpy(parser->scratch + parser->scratch_used, src, (size_t)to_copy);
        parser->scratch_used += to_copy;
        parser->bytes_consumed += to_copy;
        src += to_copy;
        avail -= to_copy;
        if (parser->scratch_used == need) {
            rc = md4_sp_process_item(parser);
            parser->scratch_used = 0U;
            if (rc != MD4_RC_OK) {
                return rc;
            }
            if (parser->stage == MD4_SP_STAGE_SKIP_TO_END && parser->bytes_consumed == parser->expected_size) {
                rc = md4_sp_complete(parser);
                if (rc != MD4_RC_OK) {
                    return rc;
                }
            }
        }
    }
    return MD4_RC_OK;
}

md4_i32 md4_stream_parser_header_ready(const md4_stream_parser_t *parser)
{
    if (parser == 0) {
        return 0;
    }
    return parser->header_ready;
}

md4_i32 md4_stream_parser_is_complete(const md4_stream_parser_t *parser)
{
    if (parser == 0) {
        return 0;
    }
    return parser->complete;
}

md4_u32 md4_stream_parser_bytes_consumed(const md4_stream_parser_t *parser)
{
    if (parser == 0) {
        return 0U;
    }
    return parser->bytes_consumed;
}

md4_u32 md4_stream_parser_expected_size(const md4_stream_parser_t *parser)
{
    if (parser == 0) {
        return 0U;
    }
    return parser->expected_size;
}

md4_rc_t md4_stream_parser_finish(md4_stream_parser_t *parser)
{
    if (parser == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (!parser->header_ready) {
        return MD4_RC_TRUNCATED;
    }
    if (!parser->complete) {
        return MD4_RC_TRUNCATED;
    }
    if (parser->bytes_consumed != parser->expected_size) {
        return MD4_RC_TRUNCATED;
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_stream_model_on_header(void *user, const md4_stream_header_info_t *info)
{
    md4_stream_decode_model_t *decoder;

    decoder = (md4_stream_decode_model_t *)user;
    if (decoder == 0 || decoder->model == 0 || info == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    md4_model_init(decoder->model);
    memcpy(decoder->model->name, info->name, (size_t)MD4_CFG_MAX_QPATH);
    decoder->model->layout = info->layout;
    decoder->model->num_bones = info->num_bones;
    return MD4_RC_OK;
}

static md4_rc_t md4_stream_model_on_bone_name(void *user, const md4_stream_bone_name_info_t *info)
{
    md4_stream_decode_model_t *decoder;

    decoder = (md4_stream_decode_model_t *)user;
    if (decoder == 0 || decoder->model == 0 || info == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (info->bone_index < 0 || info->bone_index >= MD4_CFG_MAX_BONES) {
        return MD4_RC_CAPACITY;
    }
    memcpy(decoder->model->bone_names[info->bone_index], info->name, (size_t)MD4_CFG_MAX_QPATH);
    return MD4_RC_OK;
}

static md4_rc_t md4_stream_model_on_frame(void *user, const md4_stream_frame_info_t *info)
{
    md4_stream_decode_model_t *decoder;
    md4_i32 index;
    md4_i32 i;

    decoder = (md4_stream_decode_model_t *)user;
    if (decoder == 0 || decoder->model == 0 || info == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    index = md4_append_frame(decoder->model);
    if (index < 0) {
        return MD4_RC_CAPACITY;
    }
    decoder->model->frames[index].first_bone = decoder->model->num_frame_bones;
    decoder->model->frames[index].radius = info->radius;
    memcpy(decoder->model->frames[index].name, info->name, (size_t)MD4_CFG_FRAME_NAME_LEN);
    for (i = 0; i < 3; ++i) {
        decoder->model->frames[index].bounds[0][i] = info->bounds[0][i];
        decoder->model->frames[index].bounds[1][i] = info->bounds[1][i];
        decoder->model->frames[index].local_origin[i] = info->local_origin[i];
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_stream_model_on_bone(void *user, const md4_stream_bone_info_t *info)
{
    md4_stream_decode_model_t *decoder;
    md4_i32 index;
    md4_i32 r;
    md4_i32 c;

    decoder = (md4_stream_decode_model_t *)user;
    if (decoder == 0 || decoder->model == 0 || info == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    index = md4_append_bone(decoder->model);
    if (index < 0) {
        return MD4_RC_CAPACITY;
    }
    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 4; ++c) {
            decoder->model->frame_bones[index].matrix[r][c] = info->matrix[r][c];
        }
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_stream_model_on_lod(void *user, const md4_stream_lod_info_t *info)
{
    md4_stream_decode_model_t *decoder;
    md4_i32 index;

    decoder = (md4_stream_decode_model_t *)user;
    if (decoder == 0 || decoder->model == 0 || info == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    index = md4_append_lod(decoder->model);
    if (index < 0) {
        return MD4_RC_CAPACITY;
    }
    decoder->model->lods[index].first_surface = decoder->model->num_surfaces;
    decoder->model->lods[index].num_surfaces = info->num_surfaces;
    return MD4_RC_OK;
}

static md4_rc_t md4_stream_model_on_surface(void *user, const md4_stream_surface_info_t *info)
{
    md4_stream_decode_model_t *decoder;
    md4_i32 index;

    decoder = (md4_stream_decode_model_t *)user;
    if (decoder == 0 || decoder->model == 0 || info == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    index = md4_append_surface(decoder->model);
    if (index < 0) {
        return MD4_RC_CAPACITY;
    }
    memcpy(decoder->model->surfaces[index].name, info->name, (size_t)MD4_CFG_MAX_QPATH);
    memcpy(decoder->model->surfaces[index].shader, info->shader, (size_t)MD4_CFG_MAX_QPATH);
    decoder->model->surfaces[index].shader_index = info->shader_index;
    decoder->model->surfaces[index].first_vertex = decoder->model->num_vertices;
    decoder->model->surfaces[index].num_vertices = info->num_vertices;
    decoder->model->surfaces[index].first_triangle = decoder->model->num_triangles;
    decoder->model->surfaces[index].num_triangles = info->num_triangles;
    decoder->model->surfaces[index].first_bone_ref = decoder->model->num_bone_refs;
    decoder->model->surfaces[index].num_bone_refs = info->num_bone_refs;
    decoder->model->surfaces[index].ofs_header_raw = info->ofs_header_raw;
    return MD4_RC_OK;
}

static md4_rc_t md4_stream_model_on_triangle(void *user, const md4_stream_triangle_info_t *info)
{
    md4_stream_decode_model_t *decoder;
    md4_i32 index;

    decoder = (md4_stream_decode_model_t *)user;
    if (decoder == 0 || decoder->model == 0 || info == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    index = md4_append_triangle(decoder->model);
    if (index < 0) {
        return MD4_RC_CAPACITY;
    }
    decoder->model->triangles[index].indexes[0] = info->indexes[0];
    decoder->model->triangles[index].indexes[1] = info->indexes[1];
    decoder->model->triangles[index].indexes[2] = info->indexes[2];
    return MD4_RC_OK;
}

static md4_rc_t md4_stream_model_on_vertex(void *user, const md4_stream_vertex_info_t *info)
{
    md4_stream_decode_model_t *decoder;
    md4_i32 index;

    decoder = (md4_stream_decode_model_t *)user;
    if (decoder == 0 || decoder->model == 0 || info == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    index = md4_append_vertex(decoder->model);
    if (index < 0) {
        return MD4_RC_CAPACITY;
    }
    decoder->model->vertices[index].rest_pos[0] = info->rest_pos[0];
    decoder->model->vertices[index].rest_pos[1] = info->rest_pos[1];
    decoder->model->vertices[index].rest_pos[2] = info->rest_pos[2];
    decoder->model->vertices[index].normal[0] = info->normal[0];
    decoder->model->vertices[index].normal[1] = info->normal[1];
    decoder->model->vertices[index].normal[2] = info->normal[2];
    decoder->model->vertices[index].uv[0] = info->uv[0];
    decoder->model->vertices[index].uv[1] = info->uv[1];
    decoder->model->vertices[index].first_weight = decoder->model->num_weights;
    decoder->model->vertices[index].num_weights = info->num_weights;
    return MD4_RC_OK;
}

static md4_rc_t md4_stream_model_on_weight(void *user, const md4_stream_weight_info_t *info)
{
    md4_stream_decode_model_t *decoder;
    md4_i32 index;

    decoder = (md4_stream_decode_model_t *)user;
    if (decoder == 0 || decoder->model == 0 || info == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    index = md4_append_weight(decoder->model);
    if (index < 0) {
        return MD4_RC_CAPACITY;
    }
    decoder->model->weights[index].bone_index = info->bone_index;
    decoder->model->weights[index].bone_weight = info->bone_weight;
    decoder->model->weights[index].offset[0] = info->offset[0];
    decoder->model->weights[index].offset[1] = info->offset[1];
    decoder->model->weights[index].offset[2] = info->offset[2];
    return MD4_RC_OK;
}

static md4_rc_t md4_stream_model_on_bone_ref(void *user, const md4_stream_bone_ref_info_t *info)
{
    md4_stream_decode_model_t *decoder;
    md4_i32 index;

    decoder = (md4_stream_decode_model_t *)user;
    if (decoder == 0 || decoder->model == 0 || info == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    index = md4_append_bone_ref(decoder->model);
    if (index < 0) {
        return MD4_RC_CAPACITY;
    }
    decoder->model->bone_refs[index] = info->bone_index;
    return MD4_RC_OK;
}

static md4_rc_t md4_stream_model_on_end(void *user)
{
    md4_stream_decode_model_t *decoder;

    decoder = (md4_stream_decode_model_t *)user;
    if (decoder == 0 || decoder->model == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    decoder->model->num_bones = decoder->parser.num_bones;
    return md4_validate_model(decoder->model);
}

void md4_stream_decode_model_init(md4_stream_decode_model_t *decoder, md4_model_t *model)
{
    if (decoder == 0) {
        return;
    }
    memset(decoder, 0, sizeof(*decoder));
    decoder->model = model;
    md4_stream_parser_init(&decoder->parser);
}

md4_rc_t md4_stream_decode_model_begin(md4_stream_decode_model_t *decoder,
                                       const md4_decode_opts_t *opts)
{
    md4_stream_callbacks_t callbacks;

    if (decoder == 0 || decoder->model == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.on_header = md4_stream_model_on_header;
    callbacks.on_bone_name = md4_stream_model_on_bone_name;
    callbacks.on_frame = md4_stream_model_on_frame;
    callbacks.on_bone = md4_stream_model_on_bone;
    callbacks.on_lod = md4_stream_model_on_lod;
    callbacks.on_surface = md4_stream_model_on_surface;
    callbacks.on_triangle = md4_stream_model_on_triangle;
    callbacks.on_vertex = md4_stream_model_on_vertex;
    callbacks.on_weight = md4_stream_model_on_weight;
    callbacks.on_bone_ref = md4_stream_model_on_bone_ref;
    callbacks.on_end = md4_stream_model_on_end;
    return md4_stream_parser_begin(&decoder->parser, opts, &callbacks, decoder);
}

md4_rc_t md4_stream_decode_model_push(md4_stream_decode_model_t *decoder,
                                      const void *chunk,
                                      md4_u32 chunk_size)
{
    if (decoder == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    return md4_stream_parser_push(&decoder->parser, chunk, chunk_size);
}

md4_i32 md4_stream_decode_model_header_ready(const md4_stream_decode_model_t *decoder)
{
    if (decoder == 0) {
        return 0;
    }
    return md4_stream_parser_header_ready(&decoder->parser);
}

md4_i32 md4_stream_decode_model_is_complete(const md4_stream_decode_model_t *decoder)
{
    if (decoder == 0) {
        return 0;
    }
    return md4_stream_parser_is_complete(&decoder->parser);
}

md4_u32 md4_stream_decode_model_bytes_consumed(const md4_stream_decode_model_t *decoder)
{
    if (decoder == 0) {
        return 0U;
    }
    return md4_stream_parser_bytes_consumed(&decoder->parser);
}

md4_u32 md4_stream_decode_model_expected_size(const md4_stream_decode_model_t *decoder)
{
    if (decoder == 0) {
        return 0U;
    }
    return md4_stream_parser_expected_size(&decoder->parser);
}

md4_rc_t md4_stream_decode_model_finish(md4_stream_decode_model_t *decoder)
{
    if (decoder == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    return md4_stream_parser_finish(&decoder->parser);
}
