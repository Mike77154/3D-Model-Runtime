#include "md4_internal.h"

static md4_rc_t md4_engine_close_skeleton(md4_engine_stream_parser_t *parser);
static md4_rc_t md4_engine_close_animation(md4_engine_stream_parser_t *parser);
static md4_rc_t md4_engine_close_surface(md4_engine_stream_parser_t *parser);
static md4_rc_t md4_engine_close_lod(md4_engine_stream_parser_t *parser);
static md4_rc_t md4_engine_close_mesh(md4_engine_stream_parser_t *parser);

static md4_rc_t md4_engine_emit_missing_bone_defs(md4_engine_stream_parser_t *parser, md4_i32 upto)
{
    md4_engine_bone_def_info_t info;
    md4_rc_t rc;

    while (parser->next_bone_def_to_emit < upto) {
        memset(&info, 0, sizeof(info));
        info.bone_index = parser->next_bone_def_to_emit;
        info.parent_index = -1;
        if (parser->callbacks.on_bone_def != 0) {
            rc = parser->callbacks.on_bone_def(parser->user, &info);
            if (rc != MD4_RC_OK) {
                return rc;
            }
        }
        parser->next_bone_def_to_emit += 1;
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_close_skeleton(md4_engine_stream_parser_t *parser)
{
    md4_rc_t rc;

    if (parser->skeleton_closed) {
        return MD4_RC_OK;
    }

    rc = md4_engine_emit_missing_bone_defs(parser, parser->skeleton_info.num_bones);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    if (parser->skeleton_open && parser->callbacks.on_skeleton_end != 0) {
        rc = parser->callbacks.on_skeleton_end(parser->user, &parser->skeleton_info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->skeleton_open = 0;
    parser->skeleton_closed = 1;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_open_animation(md4_engine_stream_parser_t *parser)
{
    md4_rc_t rc;

    if (parser->animation_open || parser->animation_closed) {
        return MD4_RC_OK;
    }

    rc = md4_engine_close_skeleton(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    parser->animation_info.num_clips = 1;
    parser->animation_info.num_frames = parser->asset_info.num_frames;
    parser->animation_info.num_bones = parser->asset_info.num_bones;

    if (parser->callbacks.on_animation_begin != 0) {
        rc = parser->callbacks.on_animation_begin(parser->user, &parser->animation_info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    memset(&parser->clip_info, 0, sizeof(parser->clip_info));
    parser->clip_info.clip_index = 0;
    strcpy(parser->clip_info.name, "default");
    parser->clip_info.first_frame = 0;
    parser->clip_info.num_frames = parser->asset_info.num_frames;
    parser->clip_info.frame_rate = 0;

    if (parser->callbacks.on_anim_clip != 0) {
        rc = parser->callbacks.on_anim_clip(parser->user, &parser->clip_info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->animation_open = 1;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_close_frame(md4_engine_stream_parser_t *parser)
{
    md4_rc_t rc;

    if (!parser->frame_open) {
        return MD4_RC_OK;
    }

    if (parser->callbacks.on_anim_frame_end != 0) {
        rc = parser->callbacks.on_anim_frame_end(parser->user, &parser->current_frame);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->frame_open = 0;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_close_animation(md4_engine_stream_parser_t *parser)
{
    md4_rc_t rc;

    if (parser->animation_closed) {
        return MD4_RC_OK;
    }
    if (!parser->animation_open) {
        return MD4_RC_OK;
    }

    rc = md4_engine_close_frame(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    if (parser->callbacks.on_animation_end != 0) {
        rc = parser->callbacks.on_animation_end(parser->user, &parser->animation_info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->animation_open = 0;
    parser->animation_closed = 1;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_open_mesh(md4_engine_stream_parser_t *parser)
{
    md4_rc_t rc;

    if (parser->mesh_open || parser->mesh_closed) {
        return MD4_RC_OK;
    }

    rc = md4_engine_close_animation(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    rc = md4_engine_close_skeleton(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    parser->mesh_info.num_lods = parser->asset_info.num_lods;
    if (parser->callbacks.on_mesh_begin != 0) {
        rc = parser->callbacks.on_mesh_begin(parser->user, &parser->mesh_info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->mesh_open = 1;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_close_surface(md4_engine_stream_parser_t *parser)
{
    md4_rc_t rc;

    if (!parser->surface_open) {
        return MD4_RC_OK;
    }

    if (parser->callbacks.on_surface_end != 0) {
        rc = parser->callbacks.on_surface_end(parser->user, &parser->current_surface);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->surface_open = 0;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_close_lod(md4_engine_stream_parser_t *parser)
{
    md4_rc_t rc;

    if (!parser->lod_open) {
        return MD4_RC_OK;
    }

    rc = md4_engine_close_surface(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    if (parser->callbacks.on_lod_end != 0) {
        rc = parser->callbacks.on_lod_end(parser->user, &parser->current_lod);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->lod_open = 0;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_close_mesh(md4_engine_stream_parser_t *parser)
{
    md4_rc_t rc;

    if (parser->mesh_closed) {
        return MD4_RC_OK;
    }
    if (!parser->mesh_open) {
        return MD4_RC_OK;
    }

    rc = md4_engine_close_lod(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    if (parser->callbacks.on_mesh_end != 0) {
        rc = parser->callbacks.on_mesh_end(parser->user, &parser->mesh_info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->mesh_open = 0;
    parser->mesh_closed = 1;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_on_header(void *user, const md4_stream_header_info_t *info)
{
    md4_engine_stream_parser_t *parser;
    md4_rc_t rc;

    parser = (md4_engine_stream_parser_t *)user;
    memset(&parser->asset_info, 0, sizeof(parser->asset_info));
    strcpy(parser->asset_info.name, info->name);
    parser->asset_info.layout = info->layout;
    parser->asset_info.num_frames = info->num_frames;
    parser->asset_info.num_bones = info->num_bones;
    parser->asset_info.num_lods = info->num_lods;

    parser->skeleton_info.num_bones = info->num_bones;
    parser->skeleton_info.has_bone_names = info->layout.has_bone_names;
    parser->next_bone_def_to_emit = 0;

    if (parser->callbacks.on_asset_begin != 0) {
        rc = parser->callbacks.on_asset_begin(parser->user, &parser->asset_info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    if (parser->callbacks.on_skeleton_begin != 0) {
        rc = parser->callbacks.on_skeleton_begin(parser->user, &parser->skeleton_info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->skeleton_open = 1;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_on_bone_name(void *user, const md4_stream_bone_name_info_t *info)
{
    md4_engine_stream_parser_t *parser;
    md4_engine_bone_def_info_t def;
    md4_rc_t rc;

    parser = (md4_engine_stream_parser_t *)user;
    rc = md4_engine_emit_missing_bone_defs(parser, info->bone_index);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    memset(&def, 0, sizeof(def));
    def.bone_index = info->bone_index;
    def.parent_index = -1;
    strcpy(def.name, info->name);

    if (parser->callbacks.on_bone_def != 0) {
        rc = parser->callbacks.on_bone_def(parser->user, &def);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    if (info->bone_index >= parser->next_bone_def_to_emit) {
        parser->next_bone_def_to_emit = info->bone_index + 1;
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_on_frame(void *user, const md4_stream_frame_info_t *info)
{
    md4_engine_stream_parser_t *parser;
    md4_rc_t rc;

    parser = (md4_engine_stream_parser_t *)user;
    rc = md4_engine_open_animation(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    rc = md4_engine_close_frame(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    parser->current_frame = *info;
    if (parser->callbacks.on_anim_frame_begin != 0) {
        rc = parser->callbacks.on_anim_frame_begin(parser->user, &parser->current_frame);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->frame_open = 1;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_on_bone(void *user, const md4_stream_bone_info_t *info)
{
    md4_engine_stream_parser_t *parser;
    md4_rc_t rc;

    parser = (md4_engine_stream_parser_t *)user;
    rc = md4_engine_open_animation(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    if (parser->callbacks.on_anim_frame_bone != 0) {
        rc = parser->callbacks.on_anim_frame_bone(parser->user, info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_on_lod(void *user, const md4_stream_lod_info_t *info)
{
    md4_engine_stream_parser_t *parser;
    md4_rc_t rc;

    parser = (md4_engine_stream_parser_t *)user;
    rc = md4_engine_open_mesh(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    rc = md4_engine_close_lod(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    parser->current_lod = *info;
    if (parser->callbacks.on_lod_begin != 0) {
        rc = parser->callbacks.on_lod_begin(parser->user, &parser->current_lod);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->lod_open = 1;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_on_surface(void *user, const md4_stream_surface_info_t *info)
{
    md4_engine_stream_parser_t *parser;
    md4_rc_t rc;

    parser = (md4_engine_stream_parser_t *)user;
    rc = md4_engine_open_mesh(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    rc = md4_engine_close_surface(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    parser->current_surface = *info;
    if (parser->callbacks.on_surface_begin != 0) {
        rc = parser->callbacks.on_surface_begin(parser->user, &parser->current_surface);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }

    parser->surface_open = 1;
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_on_triangle(void *user, const md4_stream_triangle_info_t *info)
{
    md4_engine_stream_parser_t *parser;
    md4_rc_t rc;

    parser = (md4_engine_stream_parser_t *)user;
    if (parser->callbacks.on_surface_triangle != 0) {
        rc = parser->callbacks.on_surface_triangle(parser->user, info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_on_vertex(void *user, const md4_stream_vertex_info_t *info)
{
    md4_engine_stream_parser_t *parser;
    md4_rc_t rc;

    parser = (md4_engine_stream_parser_t *)user;
    if (parser->callbacks.on_surface_vertex != 0) {
        rc = parser->callbacks.on_surface_vertex(parser->user, info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_on_weight(void *user, const md4_stream_weight_info_t *info)
{
    md4_engine_stream_parser_t *parser;
    md4_rc_t rc;

    parser = (md4_engine_stream_parser_t *)user;
    if (parser->callbacks.on_surface_weight != 0) {
        rc = parser->callbacks.on_surface_weight(parser->user, info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_on_bone_ref(void *user, const md4_stream_bone_ref_info_t *info)
{
    md4_engine_stream_parser_t *parser;
    md4_rc_t rc;

    parser = (md4_engine_stream_parser_t *)user;
    if (parser->callbacks.on_surface_bone_ref != 0) {
        rc = parser->callbacks.on_surface_bone_ref(parser->user, info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_engine_on_end(void *user)
{
    md4_engine_stream_parser_t *parser;
    md4_rc_t rc;

    parser = (md4_engine_stream_parser_t *)user;
    rc = md4_engine_close_mesh(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    rc = md4_engine_close_animation(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    rc = md4_engine_close_skeleton(parser);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    if (!parser->asset_closed && parser->callbacks.on_asset_end != 0) {
        rc = parser->callbacks.on_asset_end(parser->user, &parser->asset_info);
        if (rc != MD4_RC_OK) {
            return rc;
        }
    }
    parser->asset_closed = 1;
    return MD4_RC_OK;
}

void md4_engine_stream_parser_init(md4_engine_stream_parser_t *parser)
{
    if (parser == 0) {
        return;
    }
    memset(parser, 0, sizeof(*parser));
    md4_stream_parser_init(&parser->parser);
}

md4_rc_t md4_engine_stream_parser_begin(md4_engine_stream_parser_t *parser,
                                        const md4_decode_opts_t *opts,
                                        const md4_engine_stream_callbacks_t *callbacks,
                                        void *user)
{
    md4_stream_callbacks_t inner;

    if (parser == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }

    md4_engine_stream_parser_init(parser);
    if (callbacks != 0) {
        parser->callbacks = *callbacks;
    }
    parser->user = user;

    memset(&inner, 0, sizeof(inner));
    inner.on_header = md4_engine_on_header;
    inner.on_bone_name = md4_engine_on_bone_name;
    inner.on_frame = md4_engine_on_frame;
    inner.on_bone = md4_engine_on_bone;
    inner.on_lod = md4_engine_on_lod;
    inner.on_surface = md4_engine_on_surface;
    inner.on_triangle = md4_engine_on_triangle;
    inner.on_vertex = md4_engine_on_vertex;
    inner.on_weight = md4_engine_on_weight;
    inner.on_bone_ref = md4_engine_on_bone_ref;
    inner.on_end = md4_engine_on_end;

    return md4_stream_parser_begin(&parser->parser, opts, &inner, parser);
}

md4_rc_t md4_engine_stream_parser_push(md4_engine_stream_parser_t *parser,
                                       const void *chunk,
                                       md4_u32 chunk_size)
{
    if (parser == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    return md4_stream_parser_push(&parser->parser, chunk, chunk_size);
}

md4_i32 md4_engine_stream_parser_header_ready(const md4_engine_stream_parser_t *parser)
{
    if (parser == 0) {
        return 0;
    }
    return md4_stream_parser_header_ready(&parser->parser);
}

md4_i32 md4_engine_stream_parser_is_complete(const md4_engine_stream_parser_t *parser)
{
    if (parser == 0) {
        return 0;
    }
    return md4_stream_parser_is_complete(&parser->parser);
}

md4_u32 md4_engine_stream_parser_bytes_consumed(const md4_engine_stream_parser_t *parser)
{
    if (parser == 0) {
        return 0U;
    }
    return md4_stream_parser_bytes_consumed(&parser->parser);
}

md4_u32 md4_engine_stream_parser_expected_size(const md4_engine_stream_parser_t *parser)
{
    if (parser == 0) {
        return 0U;
    }
    return md4_stream_parser_expected_size(&parser->parser);
}

md4_rc_t md4_engine_stream_parser_finish(md4_engine_stream_parser_t *parser)
{
    if (parser == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    return md4_stream_parser_finish(&parser->parser);
}
