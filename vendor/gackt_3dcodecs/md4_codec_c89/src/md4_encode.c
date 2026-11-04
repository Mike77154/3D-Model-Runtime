#include "md4_internal.h"

static void md4_write_zero(md4_u8 *dst, md4_u32 count)
{
    md4_u32 i;
    for (i = 0; i < count; ++i) {
        dst[i] = 0;
    }
}

static void md4_write_qpath(md4_u8 *dst, const char *src)
{
    md4_u32 i;
    md4_write_zero(dst, MD4_CFG_MAX_QPATH);
    if (src == 0) {
        return;
    }
    for (i = 0; i < MD4_CFG_MAX_QPATH - 1; ++i) {
        if (src[i] == 0) {
            break;
        }
        dst[i] = (md4_u8)src[i];
    }
}

static void md4_write_frame_name(md4_u8 *dst, const char *src)
{
    md4_u32 i;
    md4_write_zero(dst, MD4_CFG_FRAME_NAME_LEN);
    if (src == 0) {
        return;
    }
    for (i = 0; i < MD4_CFG_FRAME_NAME_LEN - 1; ++i) {
        if (src[i] == 0) {
            break;
        }
        dst[i] = (md4_u8)src[i];
    }
}

static md4_header_mode_t md4_encode_header_mode(const md4_model_t *model, const md4_encode_opts_t *opts)
{
    if (opts != 0 && opts->header_mode != MD4_HEADER_AUTO) {
        return opts->header_mode;
    }
    if (model->layout.header_mode != MD4_HEADER_AUTO) {
        return model->layout.header_mode;
    }
    return MD4_HEADER_LEGACY;
}

static md4_vertex_mode_t md4_encode_vertex_mode(const md4_model_t *model, const md4_encode_opts_t *opts)
{
    if (opts != 0 && opts->vertex_mode != MD4_VERT_AUTO) {
        return opts->vertex_mode;
    }
    if (model->layout.vertex_mode != MD4_VERT_AUTO) {
        return model->layout.vertex_mode;
    }
    return MD4_VERT_RUNTIME;
}

static md4_i32 md4_encode_write_frame_names(const md4_model_t *model, const md4_encode_opts_t *opts)
{
    if (opts != 0) {
        return opts->write_frame_names ? 1 : 0;
    }
    return model->layout.frame_has_name ? 1 : 0;
}

static md4_i32 md4_encode_write_bone_names(const md4_model_t *model, const md4_encode_opts_t *opts, md4_header_mode_t header_mode)
{
    if (header_mode != MD4_HEADER_EXTENDED) {
        return 0;
    }
    if (opts != 0) {
        return opts->write_bone_names ? 1 : 0;
    }
    return model->layout.has_bone_names ? 1 : 0;
}

static md4_i32 md4_vertex_size_for_mode(const md4_model_t *model, md4_vertex_mode_t mode, md4_i32 vertex_index)
{
    const md4_vertex_t *v;
    v = &model->vertices[vertex_index];
    if (mode == MD4_VERT_RUNTIME) {
        return MD4_RUNTIME_VERTEX_BASE + v->num_weights * MD4_RUNTIME_WEIGHT_SIZE;
    }
    if (mode == MD4_VERT_PADDED) {
        return MD4_PADDED_VERTEX_BASE + v->num_weights * MD4_RUNTIME_WEIGHT_SIZE;
    }
    return MD4_DRAFT_VERTEX_BASE + v->num_weights * MD4_DRAFT_WEIGHT_SIZE;
}

static md4_rc_t md4_compute_size_internal(const md4_model_t *model,
                                          const md4_encode_opts_t *opts,
                                          md4_u32 *out_size,
                                          md4_u32 *out_frames_off,
                                          md4_u32 *out_lods_off)
{
    md4_header_mode_t header_mode;
    md4_vertex_mode_t vertex_mode;
    md4_i32 write_frame_names;
    md4_i32 write_bone_names;
    md4_u32 offset;
    md4_i32 i;

    md4_rc_t rc;
    rc = md4_validate_model(model);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    header_mode = md4_encode_header_mode(model, opts);
    vertex_mode = md4_encode_vertex_mode(model, opts);
    write_frame_names = md4_encode_write_frame_names(model, opts);
    write_bone_names = md4_encode_write_bone_names(model, opts, header_mode);

    if (header_mode != MD4_HEADER_LEGACY && header_mode != MD4_HEADER_EXTENDED) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (vertex_mode != MD4_VERT_RUNTIME && vertex_mode != MD4_VERT_PADDED && vertex_mode != MD4_VERT_DRAFT) {
        return MD4_RC_BAD_ARGUMENT;
    }

    offset = (header_mode == MD4_HEADER_EXTENDED) ? MD4_HEADER_EXTENDED_SIZE : MD4_HEADER_LEGACY_SIZE;
    if (write_bone_names) {
        offset += (md4_u32)(model->num_bones * MD4_CFG_MAX_QPATH);
    }
    if (out_frames_off != 0) {
        *out_frames_off = offset;
    }

    offset += (md4_u32)(model->num_frames * ((write_frame_names ? MD4_FRAME_BASE_WITH_NAME : MD4_FRAME_BASE_NO_NAME) + model->num_bones * MD4_BONE_SIZE));
    if (out_lods_off != 0) {
        *out_lods_off = offset;
    }

    for (i = 0; i < model->num_lods; ++i) {
        const md4_lod_t *lod;
        md4_i32 s;
        lod = &model->lods[i];
        offset += MD4_LOD_SIZE;
        for (s = 0; s < lod->num_surfaces; ++s) {
            const md4_surface_t *surface;
            md4_i32 v;
            surface = &model->surfaces[lod->first_surface + s];
            offset += MD4_SURFACE_HEADER_SIZE;
            offset += (md4_u32)(surface->num_triangles * MD4_TRIANGLE_SIZE);
            for (v = 0; v < surface->num_vertices; ++v) {
                offset += (md4_u32)md4_vertex_size_for_mode(model, vertex_mode, surface->first_vertex + v);
            }
            offset += (md4_u32)(surface->num_bone_refs * 4);
        }
    }

    *out_size = offset;
    return MD4_RC_OK;
}

md4_rc_t md4_encode_size(const md4_model_t *model, const md4_encode_opts_t *opts, md4_u32 *out_size)
{
    return md4_compute_size_internal(model, opts, out_size, 0, 0);
}

md4_rc_t md4_encode(const md4_model_t *model, const md4_encode_opts_t *opts,
                    void *out_buf, md4_u32 out_buf_size, md4_u32 *out_written)
{
    md4_rc_t rc;
    md4_u32 total_size;
    md4_u32 frames_off;
    md4_u32 lods_off;
    md4_header_mode_t header_mode;
    md4_vertex_mode_t vertex_mode;
    md4_i32 write_frame_names;
    md4_i32 write_bone_names;
    md4_u8 *out;
    md4_u32 offset;
    md4_i32 i;

    if (model == 0 || out_buf == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }

    rc = md4_compute_size_internal(model, opts, &total_size, &frames_off, &lods_off);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    if (out_buf_size < total_size) {
        return MD4_RC_BUFFER_TOO_SMALL;
    }

    out = (md4_u8 *)out_buf;
    md4_write_zero(out, total_size);

    header_mode = md4_encode_header_mode(model, opts);
    vertex_mode = md4_encode_vertex_mode(model, opts);
    write_frame_names = md4_encode_write_frame_names(model, opts);
    write_bone_names = md4_encode_write_bone_names(model, opts, header_mode);

    md4_write_u32_le(out + 0U, MD4_IDENT_U32);
    md4_write_i32_le(out + 4U, MD4_VERSION_I32);
    md4_write_qpath(out + 8U, model->name);
    md4_write_i32_le(out + 72U, model->num_frames);
    md4_write_i32_le(out + 76U, model->num_bones);
    if (header_mode == MD4_HEADER_EXTENDED) {
        md4_write_i32_le(out + 80U, write_bone_names ? (md4_i32)((md4_u32)MD4_HEADER_EXTENDED_SIZE) : 0);
        md4_write_i32_le(out + 84U, (md4_i32)frames_off);
        md4_write_i32_le(out + 88U, model->num_lods);
        md4_write_i32_le(out + 92U, (md4_i32)lods_off);
        md4_write_i32_le(out + 96U, (md4_i32)total_size);
    } else {
        md4_write_i32_le(out + 80U, (md4_i32)frames_off);
        md4_write_i32_le(out + 84U, model->num_lods);
        md4_write_i32_le(out + 88U, (md4_i32)lods_off);
        md4_write_i32_le(out + 92U, (md4_i32)total_size);
    }

    if (write_bone_names) {
        for (i = 0; i < model->num_bones; ++i) {
            md4_write_qpath(out + ((header_mode == MD4_HEADER_EXTENDED) ? MD4_HEADER_EXTENDED_SIZE : MD4_HEADER_LEGACY_SIZE) + (md4_u32)(i * MD4_CFG_MAX_QPATH),
                            model->bone_names[i]);
        }
    }

    offset = frames_off;
    for (i = 0; i < model->num_frames; ++i) {
        const md4_frame_t *frame;
        md4_u32 frame_base;
        md4_i32 j;
        frame = &model->frames[i];
        for (j = 0; j < 3; ++j) {
            md4_write_f32_le_from_fx32(out + offset + (md4_u32)(j * 4), frame->bounds[0][j]);
            md4_write_f32_le_from_fx32(out + offset + 12U + (md4_u32)(j * 4), frame->bounds[1][j]);
            md4_write_f32_le_from_fx32(out + offset + 24U + (md4_u32)(j * 4), frame->local_origin[j]);
        }
        md4_write_f32_le_from_fx32(out + offset + 36U, frame->radius);
        frame_base = write_frame_names ? MD4_FRAME_BASE_WITH_NAME : MD4_FRAME_BASE_NO_NAME;
        if (write_frame_names) {
            md4_write_frame_name(out + offset + 40U, frame->name);
        }
        for (j = 0; j < model->num_bones; ++j) {
            md4_i32 r;
            md4_i32 c;
            md4_u32 boff;
            const md4_bone_t *bone;
            bone = &model->frame_bones[frame->first_bone + j];
            boff = offset + frame_base + (md4_u32)(j * MD4_BONE_SIZE);
            for (r = 0; r < 3; ++r) {
                for (c = 0; c < 4; ++c) {
                    md4_write_f32_le_from_fx32(out + boff + (md4_u32)((r * 4 + c) * 4), bone->matrix[r][c]);
                }
            }
        }
        offset += frame_base + (md4_u32)(model->num_bones * MD4_BONE_SIZE);
    }

    for (i = 0; i < model->num_lods; ++i) {
        const md4_lod_t *lod;
        md4_u32 lod_start;
        md4_u32 surf_off;
        md4_i32 s;
        lod = &model->lods[i];
        lod_start = offset;
        md4_write_i32_le(out + lod_start + 0U, lod->num_surfaces);
        md4_write_i32_le(out + lod_start + 4U, MD4_LOD_SIZE);
        surf_off = lod_start + MD4_LOD_SIZE;
        for (s = 0; s < lod->num_surfaces; ++s) {
            const md4_surface_t *surface;
            md4_u32 surf_start;
            md4_u32 tri_off;
            md4_u32 vert_off;
            md4_u32 bone_ref_off;
            md4_u32 surf_end;
            md4_i32 j;

            surface = &model->surfaces[lod->first_surface + s];
            surf_start = surf_off;
            tri_off = surf_start + MD4_SURFACE_HEADER_SIZE;
            vert_off = tri_off + (md4_u32)(surface->num_triangles * MD4_TRIANGLE_SIZE);
            bone_ref_off = vert_off;
            for (j = 0; j < surface->num_vertices; ++j) {
                bone_ref_off += (md4_u32)md4_vertex_size_for_mode(model, vertex_mode, surface->first_vertex + j);
            }
            surf_end = bone_ref_off + (md4_u32)(surface->num_bone_refs * 4);

            md4_write_u32_le(out + surf_start + 0U, MD4_IDENT_U32);
            md4_write_qpath(out + surf_start + 4U, surface->name);
            md4_write_qpath(out + surf_start + 68U, surface->shader);
            md4_write_i32_le(out + surf_start + 132U, surface->shader_index);
            md4_write_i32_le(out + surf_start + 136U, -(md4_i32)surf_start);
            md4_write_i32_le(out + surf_start + 140U, surface->num_vertices);
            md4_write_i32_le(out + surf_start + 144U, (md4_i32)(vert_off - surf_start));
            md4_write_i32_le(out + surf_start + 148U, surface->num_triangles);
            md4_write_i32_le(out + surf_start + 152U, (md4_i32)(tri_off - surf_start));
            md4_write_i32_le(out + surf_start + 156U, surface->num_bone_refs);
            md4_write_i32_le(out + surf_start + 160U, (md4_i32)(bone_ref_off - surf_start));
            md4_write_i32_le(out + surf_start + 164U, (md4_i32)(surf_end - surf_start));

            for (j = 0; j < surface->num_triangles; ++j) {
                const md4_triangle_t *tri;
                tri = &model->triangles[surface->first_triangle + j];
                md4_write_i32_le(out + tri_off + (md4_u32)(j * MD4_TRIANGLE_SIZE) + 0U, tri->indexes[0]);
                md4_write_i32_le(out + tri_off + (md4_u32)(j * MD4_TRIANGLE_SIZE) + 4U, tri->indexes[1]);
                md4_write_i32_le(out + tri_off + (md4_u32)(j * MD4_TRIANGLE_SIZE) + 8U, tri->indexes[2]);
            }

            offset = vert_off;
            for (j = 0; j < surface->num_vertices; ++j) {
                const md4_vertex_t *v;
                md4_i32 w;
                v = &model->vertices[surface->first_vertex + j];
                if (vertex_mode == MD4_VERT_PADDED || vertex_mode == MD4_VERT_DRAFT) {
                    md4_write_f32_le_from_fx32(out + offset + 0U, v->rest_pos[0]);
                    md4_write_f32_le_from_fx32(out + offset + 4U, v->rest_pos[1]);
                    md4_write_f32_le_from_fx32(out + offset + 8U, v->rest_pos[2]);
                    md4_write_f32_le_from_fx32(out + offset + 12U, v->normal[0]);
                    md4_write_f32_le_from_fx32(out + offset + 16U, v->normal[1]);
                    md4_write_f32_le_from_fx32(out + offset + 20U, v->normal[2]);
                    md4_write_f32_le_from_fx32(out + offset + 24U, v->uv[0]);
                    md4_write_f32_le_from_fx32(out + offset + 28U, v->uv[1]);
                    md4_write_i32_le(out + offset + 32U, v->num_weights);
                    offset += MD4_DRAFT_VERTEX_BASE;
                } else {
                    md4_write_f32_le_from_fx32(out + offset + 0U, v->normal[0]);
                    md4_write_f32_le_from_fx32(out + offset + 4U, v->normal[1]);
                    md4_write_f32_le_from_fx32(out + offset + 8U, v->normal[2]);
                    md4_write_f32_le_from_fx32(out + offset + 12U, v->uv[0]);
                    md4_write_f32_le_from_fx32(out + offset + 16U, v->uv[1]);
                    md4_write_i32_le(out + offset + 20U, v->num_weights);
                    offset += MD4_RUNTIME_VERTEX_BASE;
                }

                for (w = 0; w < v->num_weights; ++w) {
                    const md4_weight_t *weight;
                    weight = &model->weights[v->first_weight + w];
                    md4_write_i32_le(out + offset + 0U, weight->bone_index);
                    md4_write_f32_le_from_fx32(out + offset + 4U, weight->bone_weight);
                    if (vertex_mode != MD4_VERT_DRAFT) {
                        md4_write_f32_le_from_fx32(out + offset + 8U, weight->offset[0]);
                        md4_write_f32_le_from_fx32(out + offset + 12U, weight->offset[1]);
                        md4_write_f32_le_from_fx32(out + offset + 16U, weight->offset[2]);
                        offset += MD4_RUNTIME_WEIGHT_SIZE;
                    } else {
                        offset += MD4_DRAFT_WEIGHT_SIZE;
                    }
                }
            }

            for (j = 0; j < surface->num_bone_refs; ++j) {
                md4_write_i32_le(out + bone_ref_off + (md4_u32)(j * 4), model->bone_refs[surface->first_bone_ref + j]);
            }

            surf_off = surf_end;
        }
        md4_write_i32_le(out + lod_start + 8U, (md4_i32)(surf_off - lod_start));
        offset = surf_off;
    }

    if (out_written != 0) {
        *out_written = total_size;
    }
    return MD4_RC_OK;
}
