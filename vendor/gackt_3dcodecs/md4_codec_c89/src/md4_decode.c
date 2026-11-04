#include "md4_internal.h"

static md4_i32 md4_try_header_mode(const md4_u8 *data, md4_u32 size,
                                   md4_header_mode_t mode,
                                   md4_raw_header_t *out_raw)
{
    md4_i32 header_size;
    md4_i32 num_frames;
    md4_i32 num_bones;
    md4_i32 num_lods;
    md4_i32 ofs_bone_names;
    md4_i32 ofs_frames;
    md4_i32 ofs_lods;
    md4_i32 ofs_end;
    md4_i32 frame_stride;
    md4_i32 frame_base;
    md4_u32 ident;
    md4_i32 version;
    md4_i32 frames_block_size;

    header_size = (mode == MD4_HEADER_EXTENDED) ? MD4_HEADER_EXTENDED_SIZE : MD4_HEADER_LEGACY_SIZE;
    if (size < (md4_u32)header_size) {
        return 0;
    }

    ident = md4_read_u32_le(data + 0);
    version = md4_read_i32_le(data + 4);
    if (ident != MD4_IDENT_U32) {
        return 0;
    }
    if (version != MD4_VERSION_I32) {
        return 0;
    }

    num_frames = md4_read_i32_le(data + 72);
    num_bones = md4_read_i32_le(data + 76);
    if (mode == MD4_HEADER_EXTENDED) {
        ofs_bone_names = md4_read_i32_le(data + 80);
        ofs_frames = md4_read_i32_le(data + 84);
        num_lods = md4_read_i32_le(data + 88);
        ofs_lods = md4_read_i32_le(data + 92);
        ofs_end = md4_read_i32_le(data + 96);
    } else {
        ofs_bone_names = 0;
        ofs_frames = md4_read_i32_le(data + 80);
        num_lods = md4_read_i32_le(data + 84);
        ofs_lods = md4_read_i32_le(data + 88);
        ofs_end = md4_read_i32_le(data + 92);
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
    if ((md4_u32)ofs_end > size) {
        return 0;
    }
    if (mode == MD4_HEADER_EXTENDED && ofs_bone_names != 0) {
        if (ofs_bone_names < header_size) {
            return 0;
        }
        if (ofs_bone_names >= ofs_frames) {
            return 0;
        }
        if (!md4_range_ok(size, (md4_u32)ofs_bone_names,
                          (md4_u32)(num_bones * MD4_CFG_MAX_QPATH))) {
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

    out_raw->mode = mode;
    out_raw->num_frames = num_frames;
    out_raw->num_bones = num_bones;
    out_raw->num_lods = num_lods;
    out_raw->ofs_bone_names = ofs_bone_names;
    out_raw->ofs_frames = ofs_frames;
    out_raw->ofs_lods = ofs_lods;
    out_raw->ofs_end = ofs_end;
    out_raw->frame_stride = frame_stride;
    out_raw->frame_has_name = (frame_base == MD4_FRAME_BASE_WITH_NAME) ? 1 : 0;
    return 1;
}

md4_i32 md4_choose_header(const md4_u8 *data, md4_u32 size, const md4_decode_opts_t *opts, md4_raw_header_t *out_raw)
{
    md4_raw_header_t legacy;
    md4_raw_header_t ext;
    md4_i32 have_legacy;
    md4_i32 have_ext;

    have_legacy = md4_try_header_mode(data, size, MD4_HEADER_LEGACY, &legacy);
    have_ext = md4_try_header_mode(data, size, MD4_HEADER_EXTENDED, &ext);

    if (opts != 0) {
        if (opts->header_mode == MD4_HEADER_LEGACY) {
            if (!have_legacy) {
                return 0;
            }
            *out_raw = legacy;
            return 1;
        }
        if (opts->header_mode == MD4_HEADER_EXTENDED) {
            if (!have_ext) {
                return 0;
            }
            *out_raw = ext;
            return 1;
        }
    }

    if (have_legacy) {
        *out_raw = legacy;
        return 1;
    }
    if (have_ext) {
        *out_raw = ext;
        return 1;
    }
    return 0;
}


static md4_i32 md4_decode_strict_flag(const md4_decode_opts_t *opts)
{
    if (opts != 0 && !opts->strict) {
        return 0;
    }
    return 1;
}

static md4_i32 md4_parse_vertex_block_runtime_ex(const md4_u8 *base,
                                                 md4_u32 block_size,
                                                 md4_i32 num_vertices,
                                                 md4_i32 strict,
                                                 md4_i32 include_rest_pos,
                                                 md4_i32 *out_used_bytes,
                                                 md4_model_t *model,
                                                 md4_surface_t *surface)
{
    md4_u32 cursor;
    md4_i32 i;
    md4_i32 vbase;
    md4_i32 windex;

    cursor = 0U;
    vbase = include_rest_pos ? MD4_PADDED_VERTEX_BASE : MD4_RUNTIME_VERTEX_BASE;

    if (surface != 0) {
        surface->first_vertex = model->num_vertices;
    }

    for (i = 0; i < num_vertices; ++i) {
        md4_i32 j;
        md4_i32 num_weights;
        md4_i32 vertex_index;
        md4_vertex_t *vertex;

        if (cursor + (md4_u32)vbase > block_size) {
            return 0;
        }

        num_weights = md4_read_i32_le(base + cursor + (include_rest_pos ? 32U : 20U));
        if (num_weights < 0) {
            return 0;
        }
        if (cursor + (md4_u32)vbase + (md4_u32)(num_weights * MD4_RUNTIME_WEIGHT_SIZE) > block_size) {
            return 0;
        }

        if (model != 0) {
            vertex_index = md4_append_vertex(model);
            if (vertex_index < 0) {
                return 0;
            }
            vertex = &model->vertices[vertex_index];
            memset(vertex, 0, sizeof(*vertex));
            if (include_rest_pos) {
                vertex->rest_pos[0] = md4_read_fx32_from_f32_le(base + cursor + 0U);
                vertex->rest_pos[1] = md4_read_fx32_from_f32_le(base + cursor + 4U);
                vertex->rest_pos[2] = md4_read_fx32_from_f32_le(base + cursor + 8U);
                vertex->normal[0] = md4_read_fx32_from_f32_le(base + cursor + 12U);
                vertex->normal[1] = md4_read_fx32_from_f32_le(base + cursor + 16U);
                vertex->normal[2] = md4_read_fx32_from_f32_le(base + cursor + 20U);
                vertex->uv[0] = md4_read_fx32_from_f32_le(base + cursor + 24U);
                vertex->uv[1] = md4_read_fx32_from_f32_le(base + cursor + 28U);
            } else {
                vertex->normal[0] = md4_read_fx32_from_f32_le(base + cursor + 0U);
                vertex->normal[1] = md4_read_fx32_from_f32_le(base + cursor + 4U);
                vertex->normal[2] = md4_read_fx32_from_f32_le(base + cursor + 8U);
                vertex->uv[0] = md4_read_fx32_from_f32_le(base + cursor + 12U);
                vertex->uv[1] = md4_read_fx32_from_f32_le(base + cursor + 16U);
            }
            vertex->num_weights = num_weights;
            vertex->first_weight = model->num_weights;
        }

        for (j = 0; j < num_weights; ++j) {
            md4_u32 woff;
            woff = cursor + (md4_u32)vbase + (md4_u32)(j * MD4_RUNTIME_WEIGHT_SIZE);
            if (model != 0) {
                windex = md4_append_weight(model);
                if (windex < 0) {
                    return 0;
                }
                model->weights[windex].bone_index = md4_read_i32_le(base + woff + 0U);
                model->weights[windex].bone_weight = md4_read_fx32_from_f32_le(base + woff + 4U);
                model->weights[windex].offset[0] = md4_read_fx32_from_f32_le(base + woff + 8U);
                model->weights[windex].offset[1] = md4_read_fx32_from_f32_le(base + woff + 12U);
                model->weights[windex].offset[2] = md4_read_fx32_from_f32_le(base + woff + 16U);
            }
        }

        cursor += (md4_u32)vbase + (md4_u32)(num_weights * MD4_RUNTIME_WEIGHT_SIZE);
    }

    if (strict) {
        if (cursor != block_size) {
            return 0;
        }
    } else if (cursor > block_size) {
        return 0;
    }

    if (surface != 0) {
        surface->num_vertices = num_vertices;
    }
    if (out_used_bytes != 0) {
        *out_used_bytes = (md4_i32)cursor;
    }
    return 1;
}

md4_i32 md4_parse_vertex_block_runtime(const md4_u8 *base, md4_u32 block_size, md4_i32 num_vertices,
                                       md4_i32 strict, md4_i32 *out_used_bytes,
                                       md4_i32 count_only, md4_model_t *model, md4_surface_t *surface)
{
    md4_model_t *use_model;
    md4_surface_t *use_surface;
    use_model = count_only ? 0 : model;
    use_surface = count_only ? 0 : surface;
    return md4_parse_vertex_block_runtime_ex(base, block_size, num_vertices, strict, 0,
                                             out_used_bytes, use_model, use_surface);
}

static md4_i32 md4_parse_vertex_block_padded(const md4_u8 *base, md4_u32 block_size, md4_i32 num_vertices,
                                             md4_i32 strict, md4_i32 *out_used_bytes,
                                             md4_i32 count_only, md4_model_t *model, md4_surface_t *surface)
{
    md4_model_t *use_model;
    md4_surface_t *use_surface;
    use_model = count_only ? 0 : model;
    use_surface = count_only ? 0 : surface;
    return md4_parse_vertex_block_runtime_ex(base, block_size, num_vertices, strict, 1,
                                             out_used_bytes, use_model, use_surface);
}

md4_i32 md4_parse_vertex_block_draft(const md4_u8 *base, md4_u32 block_size, md4_i32 num_vertices,
                                     md4_i32 strict, md4_i32 *out_used_bytes,
                                     md4_i32 count_only, md4_model_t *model, md4_surface_t *surface)
{
    md4_u32 cursor;
    md4_i32 i;
    md4_model_t *use_model;
    md4_surface_t *use_surface;

    use_model = count_only ? 0 : model;
    use_surface = count_only ? 0 : surface;

    cursor = 0U;
    if (use_surface != 0) {
        use_surface->first_vertex = use_model->num_vertices;
    }

    for (i = 0; i < num_vertices; ++i) {
        md4_i32 num_weights;
        md4_i32 j;
        md4_i32 vertex_index;

        if (cursor + MD4_DRAFT_VERTEX_BASE > block_size) {
            return 0;
        }
        num_weights = md4_read_i32_le(base + cursor + 32U);
        if (num_weights < 0) {
            return 0;
        }
        if (cursor + MD4_DRAFT_VERTEX_BASE + (md4_u32)(num_weights * MD4_DRAFT_WEIGHT_SIZE) > block_size) {
            return 0;
        }

        if (use_model != 0) {
            vertex_index = md4_append_vertex(use_model);
            if (vertex_index < 0) {
                return 0;
            }
            memset(&use_model->vertices[vertex_index], 0, sizeof(use_model->vertices[vertex_index]));
            use_model->vertices[vertex_index].rest_pos[0] = md4_read_fx32_from_f32_le(base + cursor + 0U);
            use_model->vertices[vertex_index].rest_pos[1] = md4_read_fx32_from_f32_le(base + cursor + 4U);
            use_model->vertices[vertex_index].rest_pos[2] = md4_read_fx32_from_f32_le(base + cursor + 8U);
            use_model->vertices[vertex_index].normal[0] = md4_read_fx32_from_f32_le(base + cursor + 12U);
            use_model->vertices[vertex_index].normal[1] = md4_read_fx32_from_f32_le(base + cursor + 16U);
            use_model->vertices[vertex_index].normal[2] = md4_read_fx32_from_f32_le(base + cursor + 20U);
            use_model->vertices[vertex_index].uv[0] = md4_read_fx32_from_f32_le(base + cursor + 24U);
            use_model->vertices[vertex_index].uv[1] = md4_read_fx32_from_f32_le(base + cursor + 28U);
            use_model->vertices[vertex_index].num_weights = num_weights;
            use_model->vertices[vertex_index].first_weight = use_model->num_weights;
        }

        for (j = 0; j < num_weights; ++j) {
            md4_u32 woff;
            woff = cursor + MD4_DRAFT_VERTEX_BASE + (md4_u32)(j * MD4_DRAFT_WEIGHT_SIZE);
            if (use_model != 0) {
                md4_i32 windex;
                windex = md4_append_weight(use_model);
                if (windex < 0) {
                    return 0;
                }
                use_model->weights[windex].bone_index = md4_read_i32_le(base + woff + 0U);
                use_model->weights[windex].bone_weight = md4_read_fx32_from_f32_le(base + woff + 4U);
                use_model->weights[windex].offset[0] = 0;
                use_model->weights[windex].offset[1] = 0;
                use_model->weights[windex].offset[2] = 0;
            }
        }

        cursor += MD4_DRAFT_VERTEX_BASE + (md4_u32)(num_weights * MD4_DRAFT_WEIGHT_SIZE);
    }

    if (strict) {
        if (cursor != block_size) {
            return 0;
        }
    } else if (cursor > block_size) {
        return 0;
    }

    if (use_surface != 0) {
        use_surface->num_vertices = num_vertices;
    }
    if (out_used_bytes != 0) {
        *out_used_bytes = (md4_i32)cursor;
    }
    return 1;
}

static md4_i32 md4_probe_vertex_mode(const md4_u8 *base,
                                     md4_u32 block_size,
                                     md4_i32 num_vertices,
                                     md4_i32 strict,
                                     md4_vertex_mode_t requested_mode,
                                     md4_i32 *out_mode,
                                     md4_i32 *out_used_bytes)
{
    md4_i32 used_bytes;

    used_bytes = 0;
    if ((requested_mode == MD4_VERT_AUTO || requested_mode == MD4_VERT_RUNTIME) &&
        md4_parse_vertex_block_runtime(base, block_size, num_vertices,
                                       strict, &used_bytes,
                                       1, 0, 0)) {
        if (out_mode != 0) {
            *out_mode = MD4_VERT_RUNTIME;
        }
        if (out_used_bytes != 0) {
            *out_used_bytes = used_bytes;
        }
        return 1;
    }

    used_bytes = 0;
    if ((requested_mode == MD4_VERT_AUTO || requested_mode == MD4_VERT_PADDED) &&
        md4_parse_vertex_block_padded(base, block_size, num_vertices,
                                      strict, &used_bytes,
                                      1, 0, 0)) {
        if (out_mode != 0) {
            *out_mode = MD4_VERT_PADDED;
        }
        if (out_used_bytes != 0) {
            *out_used_bytes = used_bytes;
        }
        return 1;
    }

    used_bytes = 0;
    if ((requested_mode == MD4_VERT_AUTO || requested_mode == MD4_VERT_DRAFT) &&
        md4_parse_vertex_block_draft(base, block_size, num_vertices,
                                     strict, &used_bytes,
                                     1, 0, 0)) {
        if (out_mode != 0) {
            *out_mode = MD4_VERT_DRAFT;
        }
        if (out_used_bytes != 0) {
            *out_used_bytes = used_bytes;
        }
        return 1;
    }

    return 0;
}

static md4_i32 md4_parse_vertex_block_for_mode(const md4_u8 *base,
                                               md4_u32 block_size,
                                               md4_i32 num_vertices,
                                               md4_i32 strict,
                                               md4_vertex_mode_t mode,
                                               md4_i32 *out_used_bytes,
                                               md4_model_t *model,
                                               md4_surface_t *surface)
{
    if (mode == MD4_VERT_RUNTIME) {
        return md4_parse_vertex_block_runtime(base, block_size, num_vertices,
                                              strict, out_used_bytes,
                                              0, model, surface);
    }
    if (mode == MD4_VERT_PADDED) {
        return md4_parse_vertex_block_padded(base, block_size, num_vertices,
                                             strict, out_used_bytes,
                                             0, model, surface);
    }
    if (mode == MD4_VERT_DRAFT) {
        return md4_parse_vertex_block_draft(base, block_size, num_vertices,
                                            strict, out_used_bytes,
                                            0, model, surface);
    }
    return 0;
}

static md4_rc_t md4_decode_frames(md4_model_t *model, const md4_u8 *data, md4_u32 size, const md4_raw_header_t *raw)
{
    md4_i32 i;
    for (i = 0; i < raw->num_frames; ++i) {
        md4_u32 frame_off;
        md4_i32 frame_idx;
        md4_i32 j;
        md4_frame_t *frame;

        frame_off = (md4_u32)(raw->ofs_frames + i * raw->frame_stride);
        if (!md4_range_ok(size, frame_off, (md4_u32)raw->frame_stride)) {
            return MD4_RC_TRUNCATED;
        }

        frame_idx = md4_append_frame(model);
        if (frame_idx < 0) {
            return MD4_RC_CAPACITY;
        }
        frame = &model->frames[frame_idx];
        memset(frame, 0, sizeof(*frame));
        frame->first_bone = model->num_frame_bones;

        for (j = 0; j < 3; ++j) {
            frame->bounds[0][j] = md4_read_fx32_from_f32_le(data + frame_off + (md4_u32)(j * 4));
            frame->bounds[1][j] = md4_read_fx32_from_f32_le(data + frame_off + 12U + (md4_u32)(j * 4));
            frame->local_origin[j] = md4_read_fx32_from_f32_le(data + frame_off + 24U + (md4_u32)(j * 4));
        }
        frame->radius = md4_read_fx32_from_f32_le(data + frame_off + 36U);
        md4_copy_frame_name(frame->name, data + frame_off + 40U, raw->frame_has_name);

        for (j = 0; j < raw->num_bones; ++j) {
            md4_u32 bone_off;
            md4_i32 bone_index;
            md4_i32 r;
            md4_i32 c;

            bone_index = md4_append_bone(model);
            if (bone_index < 0) {
                return MD4_RC_CAPACITY;
            }
            bone_off = frame_off + (md4_u32)(raw->frame_has_name ? MD4_FRAME_BASE_WITH_NAME : MD4_FRAME_BASE_NO_NAME) + (md4_u32)(j * MD4_BONE_SIZE);
            for (r = 0; r < 3; ++r) {
                for (c = 0; c < 4; ++c) {
                    model->frame_bones[bone_index].matrix[r][c] = md4_read_fx32_from_f32_le(data + bone_off + (md4_u32)((r * 4 + c) * 4));
                }
            }
        }
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_decode_surfaces(md4_model_t *model, const md4_u8 *data, md4_u32 size, const md4_raw_header_t *raw, const md4_decode_opts_t *opts)
{
    md4_u32 lod_off;
    md4_i32 lod_i;
    md4_i32 strict;
    lod_off = (md4_u32)raw->ofs_lods;
    strict = md4_decode_strict_flag(opts);

    for (lod_i = 0; lod_i < raw->num_lods; ++lod_i) {
        md4_i32 num_surfaces;
        md4_i32 ofs_surfaces;
        md4_i32 ofs_end;
        md4_i32 lod_index;
        md4_i32 surf_i;
        md4_u32 surf_off;
        md4_lod_t *lod;

        if (!md4_range_ok(size, lod_off, MD4_LOD_SIZE)) {
            return MD4_RC_TRUNCATED;
        }

        num_surfaces = md4_read_i32_le(data + lod_off + 0U);
        ofs_surfaces = md4_read_i32_le(data + lod_off + 4U);
        ofs_end = md4_read_i32_le(data + lod_off + 8U);
        if (num_surfaces < 0 || ofs_surfaces < MD4_LOD_SIZE || ofs_end < ofs_surfaces) {
            return MD4_RC_BAD_OFFSETS;
        }
        if (!md4_range_ok(size, lod_off, (md4_u32)ofs_end)) {
            return MD4_RC_TRUNCATED;
        }

        lod_index = md4_append_lod(model);
        if (lod_index < 0) {
            return MD4_RC_CAPACITY;
        }
        lod = &model->lods[lod_index];
        lod->first_surface = model->num_surfaces;
        lod->num_surfaces = num_surfaces;

        surf_off = lod_off + (md4_u32)ofs_surfaces;
        for (surf_i = 0; surf_i < num_surfaces; ++surf_i) {
            md4_i32 surface_index;
            md4_surface_t *surface;
            md4_i32 num_triangles;
            md4_i32 ofs_triangles;
            md4_i32 num_vertices;
            md4_i32 ofs_verts;
            md4_i32 num_bone_refs;
            md4_i32 ofs_bone_refs;
            md4_i32 surf_end;
            md4_i32 vertex_block_end;
            md4_i32 used_bytes;
            md4_i32 parsed;
            md4_i32 chosen_mode;
            md4_i32 k;

            md4_u32 surface_ident;
            md4_u32 triangle_bytes;
            md4_u32 bone_ref_bytes;
            md4_u32 vertex_block_size;

            if (!md4_range_ok(size, surf_off, MD4_SURFACE_HEADER_SIZE)) {
                return MD4_RC_TRUNCATED;
            }
            surface_ident = md4_read_u32_le(data + surf_off + 0U);
            num_vertices = md4_read_i32_le(data + surf_off + 140U);
            ofs_verts = md4_read_i32_le(data + surf_off + 144U);
            num_triangles = md4_read_i32_le(data + surf_off + 148U);
            ofs_triangles = md4_read_i32_le(data + surf_off + 152U);
            num_bone_refs = md4_read_i32_le(data + surf_off + 156U);
            ofs_bone_refs = md4_read_i32_le(data + surf_off + 160U);
            surf_end = md4_read_i32_le(data + surf_off + 164U);

            if (strict && surface_ident != MD4_IDENT_U32) {
                return MD4_RC_BAD_LAYOUT;
            }
            if (num_vertices < 0 || num_triangles < 0 || num_bone_refs < 0) {
                return MD4_RC_BAD_COUNTS;
            }
            if (num_vertices > MD4_CFG_MAX_VERTICES || num_triangles > MD4_CFG_MAX_TRIANGLES || num_bone_refs > MD4_CFG_MAX_BONE_REFS) {
                return MD4_RC_CAPACITY;
            }
            if (ofs_triangles < MD4_SURFACE_HEADER_SIZE || ofs_verts < MD4_SURFACE_HEADER_SIZE || surf_end < MD4_SURFACE_HEADER_SIZE) {
                return MD4_RC_BAD_OFFSETS;
            }
            if (!md4_range_ok(size, surf_off, (md4_u32)surf_end)) {
                return MD4_RC_TRUNCATED;
            }
            triangle_bytes = (md4_u32)num_triangles * MD4_TRIANGLE_SIZE;
            if (!md4_range_ok((md4_u32)surf_end, (md4_u32)ofs_triangles, triangle_bytes)) {
                return MD4_RC_BAD_OFFSETS;
            }
            if (ofs_bone_refs > 0) {
                bone_ref_bytes = (md4_u32)num_bone_refs * 4U;
                if (!md4_range_ok((md4_u32)surf_end, (md4_u32)ofs_bone_refs, bone_ref_bytes)) {
                    return MD4_RC_BAD_OFFSETS;
                }
            }

            vertex_block_end = md4_surface_vertex_block_end(surf_end, ofs_bone_refs, num_bone_refs);
            if (ofs_verts > vertex_block_end) {
                return MD4_RC_BAD_OFFSETS;
            }
            vertex_block_size = (md4_u32)(vertex_block_end - ofs_verts);
            chosen_mode = model->layout.vertex_mode;
            parsed = md4_probe_vertex_mode(data + surf_off + (md4_u32)ofs_verts,
                                           vertex_block_size,
                                           num_vertices,
                                           strict,
                                           model->layout.vertex_mode,
                                           &chosen_mode,
                                           &used_bytes);
            if (!parsed) {
                return MD4_RC_BAD_LAYOUT;
            }

            surface_index = md4_append_surface(model);
            if (surface_index < 0) {
                return MD4_RC_CAPACITY;
            }
            surface = &model->surfaces[surface_index];
            memset(surface, 0, sizeof(*surface));
            md4_copy_qpath(surface->name, data + surf_off + 4U);
            md4_copy_qpath(surface->shader, data + surf_off + 68U);
            surface->shader_index = md4_read_i32_le(data + surf_off + 132U);
            surface->ofs_header_raw = md4_read_i32_le(data + surf_off + 136U);
            if (strict && surface->ofs_header_raw > 0) {
                return MD4_RC_BAD_OFFSETS;
            }
            surface->first_triangle = model->num_triangles;
            surface->num_triangles = num_triangles;
            surface->first_bone_ref = model->num_bone_refs;
            surface->num_bone_refs = num_bone_refs;

            for (k = 0; k < num_triangles; ++k) {
                md4_i32 tri_idx;
                md4_u32 toff;
                tri_idx = md4_append_triangle(model);
                if (tri_idx < 0) {
                    return MD4_RC_CAPACITY;
                }
                toff = surf_off + (md4_u32)ofs_triangles + (md4_u32)(k * MD4_TRIANGLE_SIZE);
                model->triangles[tri_idx].indexes[0] = md4_read_i32_le(data + toff + 0U);
                model->triangles[tri_idx].indexes[1] = md4_read_i32_le(data + toff + 4U);
                model->triangles[tri_idx].indexes[2] = md4_read_i32_le(data + toff + 8U);
            }

            for (k = 0; k < num_bone_refs; ++k) {
                md4_i32 bref_idx;
                bref_idx = md4_append_bone_ref(model);
                if (bref_idx < 0) {
                    return MD4_RC_CAPACITY;
                }
                model->bone_refs[bref_idx] = md4_read_i32_le(data + surf_off + (md4_u32)ofs_bone_refs + (md4_u32)(k * 4));
            }

            parsed = md4_parse_vertex_block_for_mode(data + surf_off + (md4_u32)ofs_verts,
                                                     vertex_block_size,
                                                     num_vertices,
                                                     strict,
                                                     (md4_vertex_mode_t)chosen_mode,
                                                     &used_bytes,
                                                     model,
                                                     surface);
            if (!parsed) {
                return MD4_RC_BAD_LAYOUT;
            }

            if ((md4_vertex_mode_t)chosen_mode == MD4_VERT_RUNTIME) {
                model->layout.vertex_has_rest_pos = 0;
                model->layout.weights_have_offset = 1;
            } else if ((md4_vertex_mode_t)chosen_mode == MD4_VERT_PADDED) {
                model->layout.vertex_has_rest_pos = 1;
                model->layout.weights_have_offset = 1;
            } else {
                model->layout.vertex_has_rest_pos = 1;
                model->layout.weights_have_offset = 0;
            }

            if (model->layout.vertex_mode == MD4_VERT_AUTO) {
                model->layout.vertex_mode = (md4_vertex_mode_t)chosen_mode;
            } else if ((md4_i32)model->layout.vertex_mode != chosen_mode) {
                return MD4_RC_BAD_LAYOUT;
            }

            surf_off += (md4_u32)surf_end;
        }

        lod_off += (md4_u32)ofs_end;
    }

    return MD4_RC_OK;
}

md4_rc_t md4_decode(md4_model_t *model, const void *data_ptr, md4_u32 size, const md4_decode_opts_t *opts)
{
    const md4_u8 *data;
    md4_raw_header_t raw;
    md4_rc_t rc;
    md4_i32 i;

    if (model == 0 || data_ptr == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    data = (const md4_u8 *)data_ptr;
    md4_model_init(model);

    if (size < 8U) {
        return MD4_RC_TRUNCATED;
    }
    if (md4_read_u32_le(data) != MD4_IDENT_U32) {
        return MD4_RC_BAD_IDENT;
    }
    if (md4_read_i32_le(data + 4U) != MD4_VERSION_I32) {
        return MD4_RC_BAD_VERSION;
    }
    if (!md4_choose_header(data, size, opts, &raw)) {
        return MD4_RC_BAD_LAYOUT;
    }

    model->layout.header_mode = raw.mode;
    model->layout.frame_has_name = raw.frame_has_name;
    model->layout.vertex_mode = (opts != 0) ? opts->vertex_mode : MD4_VERT_AUTO;
    model->layout.has_bone_names = 0;
    model->layout.vertex_has_rest_pos = 0;
    model->layout.weights_have_offset = 0;
    md4_copy_qpath(model->name, data + 8U);
    model->num_bones = raw.num_bones;

    if (raw.mode == MD4_HEADER_EXTENDED && raw.ofs_bone_names != 0) {
        if (!md4_range_ok(size, (md4_u32)raw.ofs_bone_names, (md4_u32)(raw.num_bones * MD4_CFG_MAX_QPATH))) {
            return MD4_RC_TRUNCATED;
        }
        for (i = 0; i < raw.num_bones; ++i) {
            md4_copy_qpath(model->bone_names[i], data + (md4_u32)raw.ofs_bone_names + (md4_u32)(i * MD4_CFG_MAX_QPATH));
        }
        model->layout.has_bone_names = 1;
    }

    rc = md4_decode_frames(model, data, size, &raw);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    rc = md4_decode_surfaces(model, data, size, &raw, opts);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    return md4_validate_model(model);
}
