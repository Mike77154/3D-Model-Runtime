#include "md4_internal.h"

#if MD4_CFG_STREAM_CHUNK < 168
#error "MD4_CFG_STREAM_CHUNK must be at least 168 bytes"
#endif

static void md4_sw_write_zero(md4_u8 *dst, md4_u32 count)
{
    md4_u32 i;
    for (i = 0; i < count; ++i) {
        dst[i] = 0;
    }
}

static void md4_sw_write_qpath(md4_u8 *dst, const char *src)
{
    md4_u32 i;
    md4_sw_write_zero(dst, MD4_CFG_MAX_QPATH);
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

static void md4_sw_write_frame_name(md4_u8 *dst, const char *src)
{
    md4_u32 i;
    md4_sw_write_zero(dst, MD4_CFG_FRAME_NAME_LEN);
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

static md4_header_mode_t md4_sw_header_mode(const md4_model_t *model, const md4_encode_opts_t *opts)
{
    if (opts != 0 && opts->header_mode != MD4_HEADER_AUTO) {
        return opts->header_mode;
    }
    if (model->layout.header_mode != MD4_HEADER_AUTO) {
        return model->layout.header_mode;
    }
    return MD4_HEADER_LEGACY;
}

static md4_vertex_mode_t md4_sw_vertex_mode(const md4_model_t *model, const md4_encode_opts_t *opts)
{
    if (opts != 0 && opts->vertex_mode != MD4_VERT_AUTO) {
        return opts->vertex_mode;
    }
    if (model->layout.vertex_mode != MD4_VERT_AUTO) {
        return model->layout.vertex_mode;
    }
    return MD4_VERT_RUNTIME;
}

static md4_i32 md4_sw_write_frame_names(const md4_model_t *model, const md4_encode_opts_t *opts)
{
    if (opts != 0) {
        return opts->write_frame_names ? 1 : 0;
    }
    return model->layout.frame_has_name ? 1 : 0;
}

static md4_i32 md4_sw_write_bone_names(const md4_model_t *model,
                                       const md4_encode_opts_t *opts,
                                       md4_header_mode_t header_mode)
{
    if (header_mode != MD4_HEADER_EXTENDED) {
        return 0;
    }
    if (opts != 0) {
        return opts->write_bone_names ? 1 : 0;
    }
    return model->layout.has_bone_names ? 1 : 0;
}

static md4_i32 md4_sw_vertex_size_for_mode(const md4_model_t *model,
                                           md4_vertex_mode_t mode,
                                           md4_i32 vertex_index)
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

static md4_rc_t md4_writer_build_layout(md4_writer_layout_t *layout,
                                        const md4_model_t *model,
                                        const md4_encode_opts_t *opts)
{
    md4_rc_t rc;
    md4_u32 offset;
    md4_i32 i;
    md4_i32 global_surface_index;

    if (layout == 0 || model == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }

    rc = md4_validate_model(model);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    memset(layout, 0, sizeof(*layout));
    layout->header_mode = md4_sw_header_mode(model, opts);
    layout->vertex_mode = md4_sw_vertex_mode(model, opts);
    layout->write_frame_names = md4_sw_write_frame_names(model, opts);
    layout->write_bone_names = md4_sw_write_bone_names(model, opts, layout->header_mode);

    if (layout->header_mode != MD4_HEADER_LEGACY && layout->header_mode != MD4_HEADER_EXTENDED) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (layout->vertex_mode != MD4_VERT_RUNTIME &&
        layout->vertex_mode != MD4_VERT_PADDED &&
        layout->vertex_mode != MD4_VERT_DRAFT) {
        return MD4_RC_BAD_ARGUMENT;
    }

    offset = (layout->header_mode == MD4_HEADER_EXTENDED) ? MD4_HEADER_EXTENDED_SIZE : MD4_HEADER_LEGACY_SIZE;
    if (layout->write_bone_names) {
        offset += (md4_u32)(model->num_bones * MD4_CFG_MAX_QPATH);
    }
    layout->frames_off = offset;

    offset += (md4_u32)(model->num_frames *
             ((layout->write_frame_names ? MD4_FRAME_BASE_WITH_NAME : MD4_FRAME_BASE_NO_NAME) +
              model->num_bones * MD4_BONE_SIZE));
    layout->lods_off = offset;

    global_surface_index = 0;
    for (i = 0; i < model->num_lods; ++i) {
        const md4_lod_t *lod;
        md4_i32 s;
        lod = &model->lods[i];
        layout->lod_starts[i] = offset;
        offset += MD4_LOD_SIZE;
        for (s = 0; s < lod->num_surfaces; ++s) {
            const md4_surface_t *surface;
            md4_i32 v;
            md4_u32 tri_abs;
            md4_u32 vert_abs;
            md4_u32 bref_abs;
            md4_u32 surf_end;
            global_surface_index = lod->first_surface + s;
            if (global_surface_index < 0 || global_surface_index >= model->num_surfaces) {
                return MD4_RC_BAD_COUNTS;
            }
            surface = &model->surfaces[global_surface_index];
            layout->surface_starts[global_surface_index] = offset;
            tri_abs = offset + MD4_SURFACE_HEADER_SIZE;
            vert_abs = tri_abs + (md4_u32)(surface->num_triangles * MD4_TRIANGLE_SIZE);
            bref_abs = vert_abs;
            for (v = 0; v < surface->num_vertices; ++v) {
                bref_abs += (md4_u32)md4_sw_vertex_size_for_mode(model, layout->vertex_mode,
                                                                 surface->first_vertex + v);
            }
            surf_end = bref_abs + (md4_u32)(surface->num_bone_refs * 4);
            layout->surface_triangles[global_surface_index] = tri_abs;
            layout->surface_vertices[global_surface_index] = vert_abs;
            layout->surface_bone_refs[global_surface_index] = bref_abs;
            layout->surface_ends[global_surface_index] = surf_end;
            offset = surf_end;
        }
        layout->lod_ends[i] = offset;
    }

    layout->total_size = offset;
    return MD4_RC_OK;
}

void md4_writer_init(md4_writer_t *writer)
{
    if (writer == 0) {
        return;
    }
    memset(writer, 0, sizeof(*writer));
    writer->phase = MD4_WR_PHASE_IDLE;
}

static void md4_writer_finish_frames(md4_writer_t *writer)
{
    writer->lod_index = 0;
    writer->surface_index_in_lod = 0;
    writer->triangle_index = 0;
    writer->vertex_index = 0;
    writer->weight_index_in_vertex = 0;
    writer->bone_ref_index = 0;
    writer->current_surface_global = -1;
    if (writer->model->num_lods > 0) {
        writer->phase = MD4_WR_PHASE_LOD_HEADER;
    } else {
        writer->phase = MD4_WR_PHASE_DONE;
    }
}

static void md4_writer_advance_after_frame_base(md4_writer_t *writer)
{
    if (writer->model->num_bones > 0) {
        writer->bone_index_in_frame = 0;
        writer->phase = MD4_WR_PHASE_FRAME_BONE;
    } else {
        ++writer->frame_index;
        if (writer->frame_index < writer->model->num_frames) {
            writer->phase = MD4_WR_PHASE_FRAME_BASE;
        } else {
            md4_writer_finish_frames(writer);
        }
    }
}

static void md4_writer_advance_after_frame_bone(md4_writer_t *writer)
{
    ++writer->bone_index_in_frame;
    if (writer->bone_index_in_frame < writer->model->num_bones) {
        writer->phase = MD4_WR_PHASE_FRAME_BONE;
    } else {
        ++writer->frame_index;
        if (writer->frame_index < writer->model->num_frames) {
            writer->phase = MD4_WR_PHASE_FRAME_BASE;
        } else {
            md4_writer_finish_frames(writer);
        }
    }
}

static void md4_writer_goto_after_surface(md4_writer_t *writer)
{
    const md4_lod_t *lod;
    ++writer->surface_index_in_lod;
    lod = &writer->model->lods[writer->lod_index];
    if (writer->surface_index_in_lod < lod->num_surfaces) {
        writer->phase = MD4_WR_PHASE_SURFACE_HEADER;
    } else {
        ++writer->lod_index;
        writer->surface_index_in_lod = 0;
        if (writer->lod_index < writer->model->num_lods) {
            writer->phase = MD4_WR_PHASE_LOD_HEADER;
        } else {
            writer->phase = MD4_WR_PHASE_DONE;
        }
    }
    writer->triangle_index = 0;
    writer->vertex_index = 0;
    writer->weight_index_in_vertex = 0;
    writer->bone_ref_index = 0;
    writer->current_surface_global = -1;
}

static void md4_writer_goto_after_surface_header(md4_writer_t *writer)
{
    const md4_surface_t *surface;
    surface = &writer->model->surfaces[writer->current_surface_global];
    writer->triangle_index = 0;
    writer->vertex_index = 0;
    writer->weight_index_in_vertex = 0;
    writer->bone_ref_index = 0;
    if (surface->num_triangles > 0) {
        writer->phase = MD4_WR_PHASE_SURFACE_TRIANGLE;
    } else if (surface->num_vertices > 0) {
        writer->phase = MD4_WR_PHASE_SURFACE_VERTEX_BASE;
    } else if (surface->num_bone_refs > 0) {
        writer->phase = MD4_WR_PHASE_SURFACE_BONE_REF;
    } else {
        md4_writer_goto_after_surface(writer);
    }
}

static void md4_writer_advance_after_triangle(md4_writer_t *writer)
{
    const md4_surface_t *surface;
    ++writer->triangle_index;
    surface = &writer->model->surfaces[writer->current_surface_global];
    if (writer->triangle_index < surface->num_triangles) {
        writer->phase = MD4_WR_PHASE_SURFACE_TRIANGLE;
    } else if (surface->num_vertices > 0) {
        writer->vertex_index = 0;
        writer->weight_index_in_vertex = 0;
        writer->phase = MD4_WR_PHASE_SURFACE_VERTEX_BASE;
    } else if (surface->num_bone_refs > 0) {
        writer->bone_ref_index = 0;
        writer->phase = MD4_WR_PHASE_SURFACE_BONE_REF;
    } else {
        md4_writer_goto_after_surface(writer);
    }
}

static void md4_writer_advance_after_vertex_base(md4_writer_t *writer)
{
    const md4_surface_t *surface;
    const md4_vertex_t *vertex;
    surface = &writer->model->surfaces[writer->current_surface_global];
    vertex = &writer->model->vertices[surface->first_vertex + writer->vertex_index];
    if (vertex->num_weights > 0) {
        writer->weight_index_in_vertex = 0;
        writer->phase = MD4_WR_PHASE_SURFACE_VERTEX_WEIGHT;
    } else {
        ++writer->vertex_index;
        if (writer->vertex_index < surface->num_vertices) {
            writer->phase = MD4_WR_PHASE_SURFACE_VERTEX_BASE;
        } else if (surface->num_bone_refs > 0) {
            writer->bone_ref_index = 0;
            writer->phase = MD4_WR_PHASE_SURFACE_BONE_REF;
        } else {
            md4_writer_goto_after_surface(writer);
        }
    }
}

static void md4_writer_advance_after_vertex_weight(md4_writer_t *writer)
{
    const md4_surface_t *surface;
    const md4_vertex_t *vertex;
    surface = &writer->model->surfaces[writer->current_surface_global];
    vertex = &writer->model->vertices[surface->first_vertex + writer->vertex_index];
    ++writer->weight_index_in_vertex;
    if (writer->weight_index_in_vertex < vertex->num_weights) {
        writer->phase = MD4_WR_PHASE_SURFACE_VERTEX_WEIGHT;
    } else {
        ++writer->vertex_index;
        if (writer->vertex_index < surface->num_vertices) {
            writer->phase = MD4_WR_PHASE_SURFACE_VERTEX_BASE;
        } else if (surface->num_bone_refs > 0) {
            writer->bone_ref_index = 0;
            writer->phase = MD4_WR_PHASE_SURFACE_BONE_REF;
        } else {
            md4_writer_goto_after_surface(writer);
        }
    }
}

static void md4_writer_advance_after_bone_ref(md4_writer_t *writer)
{
    const md4_surface_t *surface;
    surface = &writer->model->surfaces[writer->current_surface_global];
    ++writer->bone_ref_index;
    if (writer->bone_ref_index < surface->num_bone_refs) {
        writer->phase = MD4_WR_PHASE_SURFACE_BONE_REF;
    } else {
        md4_writer_goto_after_surface(writer);
    }
}

static md4_rc_t md4_writer_fill_pending(md4_writer_t *writer)
{
    const md4_model_t *model;
    md4_u8 *out;

    if (writer == 0 || writer->model == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (writer->pending_size != 0U) {
        return MD4_RC_OK;
    }
    if (writer->phase == MD4_WR_PHASE_DONE) {
        return MD4_RC_OK;
    }
    if (writer->phase == MD4_WR_PHASE_IDLE) {
        return MD4_RC_BAD_ARGUMENT;
    }

    model = writer->model;
    out = writer->pending;
    writer->pending_offset = 0U;
    writer->pending_size = 0U;

    switch (writer->phase) {
    case MD4_WR_PHASE_HEADER:
    {
        md4_u32 header_size;
        header_size = (writer->layout.header_mode == MD4_HEADER_EXTENDED) ?
                      MD4_HEADER_EXTENDED_SIZE : MD4_HEADER_LEGACY_SIZE;
        md4_sw_write_zero(out, header_size);
        md4_write_u32_le(out + 0U, MD4_IDENT_U32);
        md4_write_i32_le(out + 4U, MD4_VERSION_I32);
        md4_sw_write_qpath(out + 8U, model->name);
        md4_write_i32_le(out + 72U, model->num_frames);
        md4_write_i32_le(out + 76U, model->num_bones);
        if (writer->layout.header_mode == MD4_HEADER_EXTENDED) {
            md4_write_i32_le(out + 80U, writer->layout.write_bone_names ? (md4_i32)MD4_HEADER_EXTENDED_SIZE : 0);
            md4_write_i32_le(out + 84U, (md4_i32)writer->layout.frames_off);
            md4_write_i32_le(out + 88U, model->num_lods);
            md4_write_i32_le(out + 92U, (md4_i32)writer->layout.lods_off);
            md4_write_i32_le(out + 96U, (md4_i32)writer->layout.total_size);
        } else {
            md4_write_i32_le(out + 80U, (md4_i32)writer->layout.frames_off);
            md4_write_i32_le(out + 84U, model->num_lods);
            md4_write_i32_le(out + 88U, (md4_i32)writer->layout.lods_off);
            md4_write_i32_le(out + 92U, (md4_i32)writer->layout.total_size);
        }
        writer->pending_size = header_size;
        if (writer->layout.write_bone_names && model->num_bones > 0) {
            writer->bone_name_index = 0;
            writer->phase = MD4_WR_PHASE_BONE_NAME;
        } else {
            writer->frame_index = 0;
            writer->phase = MD4_WR_PHASE_FRAME_BASE;
        }
        return MD4_RC_OK;
    }
    case MD4_WR_PHASE_BONE_NAME:
        md4_sw_write_qpath(out, model->bone_names[writer->bone_name_index]);
        writer->pending_size = MD4_CFG_MAX_QPATH;
        ++writer->bone_name_index;
        if (writer->bone_name_index >= model->num_bones) {
            writer->frame_index = 0;
            writer->phase = MD4_WR_PHASE_FRAME_BASE;
        }
        return MD4_RC_OK;
    case MD4_WR_PHASE_FRAME_BASE:
    {
        const md4_frame_t *frame;
        md4_u32 frame_base;
        md4_i32 j;
        frame = &model->frames[writer->frame_index];
        frame_base = writer->layout.write_frame_names ? MD4_FRAME_BASE_WITH_NAME : MD4_FRAME_BASE_NO_NAME;
        md4_sw_write_zero(out, frame_base);
        for (j = 0; j < 3; ++j) {
            md4_write_f32_le_from_fx32(out + (md4_u32)(j * 4), frame->bounds[0][j]);
            md4_write_f32_le_from_fx32(out + 12U + (md4_u32)(j * 4), frame->bounds[1][j]);
            md4_write_f32_le_from_fx32(out + 24U + (md4_u32)(j * 4), frame->local_origin[j]);
        }
        md4_write_f32_le_from_fx32(out + 36U, frame->radius);
        if (writer->layout.write_frame_names) {
            md4_sw_write_frame_name(out + 40U, frame->name);
        }
        writer->pending_size = frame_base;
        md4_writer_advance_after_frame_base(writer);
        return MD4_RC_OK;
    }
    case MD4_WR_PHASE_FRAME_BONE:
    {
        const md4_frame_t *frame;
        const md4_bone_t *bone;
        md4_i32 r;
        md4_i32 c;
        frame = &model->frames[writer->frame_index];
        bone = &model->frame_bones[frame->first_bone + writer->bone_index_in_frame];
        md4_sw_write_zero(out, MD4_BONE_SIZE);
        for (r = 0; r < 3; ++r) {
            for (c = 0; c < 4; ++c) {
                md4_write_f32_le_from_fx32(out + (md4_u32)((r * 4 + c) * 4), bone->matrix[r][c]);
            }
        }
        writer->pending_size = MD4_BONE_SIZE;
        md4_writer_advance_after_frame_bone(writer);
        return MD4_RC_OK;
    }
    case MD4_WR_PHASE_LOD_HEADER:
    {
        const md4_lod_t *lod;
        lod = &model->lods[writer->lod_index];
        md4_sw_write_zero(out, MD4_LOD_SIZE);
        md4_write_i32_le(out + 0U, lod->num_surfaces);
        md4_write_i32_le(out + 4U, MD4_LOD_SIZE);
        md4_write_i32_le(out + 8U, (md4_i32)(writer->layout.lod_ends[writer->lod_index] -
                                            writer->layout.lod_starts[writer->lod_index]));
        writer->pending_size = MD4_LOD_SIZE;
        if (lod->num_surfaces > 0) {
            writer->surface_index_in_lod = 0;
            writer->phase = MD4_WR_PHASE_SURFACE_HEADER;
        } else {
            ++writer->lod_index;
            if (writer->lod_index < model->num_lods) {
                writer->phase = MD4_WR_PHASE_LOD_HEADER;
            } else {
                writer->phase = MD4_WR_PHASE_DONE;
            }
        }
        return MD4_RC_OK;
    }
    case MD4_WR_PHASE_SURFACE_HEADER:
    {
        const md4_surface_t *surface;
        md4_u32 surf_start;
        writer->current_surface_global = md4_get_global_surface_index(model,
                                                                      writer->lod_index,
                                                                      writer->surface_index_in_lod);
        if (writer->current_surface_global < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        surface = &model->surfaces[writer->current_surface_global];
        surf_start = writer->layout.surface_starts[writer->current_surface_global];
        md4_sw_write_zero(out, MD4_SURFACE_HEADER_SIZE);
        md4_write_u32_le(out + 0U, MD4_IDENT_U32);
        md4_sw_write_qpath(out + 4U, surface->name);
        md4_sw_write_qpath(out + 68U, surface->shader);
        md4_write_i32_le(out + 132U, surface->shader_index);
        md4_write_i32_le(out + 136U, -(md4_i32)surf_start);
        md4_write_i32_le(out + 140U, surface->num_vertices);
        md4_write_i32_le(out + 144U, (md4_i32)(writer->layout.surface_vertices[writer->current_surface_global] - surf_start));
        md4_write_i32_le(out + 148U, surface->num_triangles);
        md4_write_i32_le(out + 152U, (md4_i32)(writer->layout.surface_triangles[writer->current_surface_global] - surf_start));
        md4_write_i32_le(out + 156U, surface->num_bone_refs);
        md4_write_i32_le(out + 160U, (md4_i32)(writer->layout.surface_bone_refs[writer->current_surface_global] - surf_start));
        md4_write_i32_le(out + 164U, (md4_i32)(writer->layout.surface_ends[writer->current_surface_global] - surf_start));
        writer->pending_size = MD4_SURFACE_HEADER_SIZE;
        md4_writer_goto_after_surface_header(writer);
        return MD4_RC_OK;
    }
    case MD4_WR_PHASE_SURFACE_TRIANGLE:
    {
        const md4_surface_t *surface;
        const md4_triangle_t *tri;
        surface = &model->surfaces[writer->current_surface_global];
        tri = &model->triangles[surface->first_triangle + writer->triangle_index];
        md4_sw_write_zero(out, MD4_TRIANGLE_SIZE);
        md4_write_i32_le(out + 0U, tri->indexes[0]);
        md4_write_i32_le(out + 4U, tri->indexes[1]);
        md4_write_i32_le(out + 8U, tri->indexes[2]);
        writer->pending_size = MD4_TRIANGLE_SIZE;
        md4_writer_advance_after_triangle(writer);
        return MD4_RC_OK;
    }
    case MD4_WR_PHASE_SURFACE_VERTEX_BASE:
    {
        const md4_surface_t *surface;
        const md4_vertex_t *v;
        surface = &model->surfaces[writer->current_surface_global];
        v = &model->vertices[surface->first_vertex + writer->vertex_index];
        if (writer->layout.vertex_mode == MD4_VERT_PADDED || writer->layout.vertex_mode == MD4_VERT_DRAFT) {
            md4_sw_write_zero(out, MD4_DRAFT_VERTEX_BASE);
            md4_write_f32_le_from_fx32(out + 0U, v->rest_pos[0]);
            md4_write_f32_le_from_fx32(out + 4U, v->rest_pos[1]);
            md4_write_f32_le_from_fx32(out + 8U, v->rest_pos[2]);
            md4_write_f32_le_from_fx32(out + 12U, v->normal[0]);
            md4_write_f32_le_from_fx32(out + 16U, v->normal[1]);
            md4_write_f32_le_from_fx32(out + 20U, v->normal[2]);
            md4_write_f32_le_from_fx32(out + 24U, v->uv[0]);
            md4_write_f32_le_from_fx32(out + 28U, v->uv[1]);
            md4_write_i32_le(out + 32U, v->num_weights);
            writer->pending_size = MD4_DRAFT_VERTEX_BASE;
        } else {
            md4_sw_write_zero(out, MD4_RUNTIME_VERTEX_BASE);
            md4_write_f32_le_from_fx32(out + 0U, v->normal[0]);
            md4_write_f32_le_from_fx32(out + 4U, v->normal[1]);
            md4_write_f32_le_from_fx32(out + 8U, v->normal[2]);
            md4_write_f32_le_from_fx32(out + 12U, v->uv[0]);
            md4_write_f32_le_from_fx32(out + 16U, v->uv[1]);
            md4_write_i32_le(out + 20U, v->num_weights);
            writer->pending_size = MD4_RUNTIME_VERTEX_BASE;
        }
        md4_writer_advance_after_vertex_base(writer);
        return MD4_RC_OK;
    }
    case MD4_WR_PHASE_SURFACE_VERTEX_WEIGHT:
    {
        const md4_surface_t *surface;
        const md4_vertex_t *v;
        const md4_weight_t *weight;
        surface = &model->surfaces[writer->current_surface_global];
        v = &model->vertices[surface->first_vertex + writer->vertex_index];
        weight = &model->weights[v->first_weight + writer->weight_index_in_vertex];
        if (writer->layout.vertex_mode == MD4_VERT_DRAFT) {
            md4_sw_write_zero(out, MD4_DRAFT_WEIGHT_SIZE);
            md4_write_i32_le(out + 0U, weight->bone_index);
            md4_write_f32_le_from_fx32(out + 4U, weight->bone_weight);
            writer->pending_size = MD4_DRAFT_WEIGHT_SIZE;
        } else {
            md4_sw_write_zero(out, MD4_RUNTIME_WEIGHT_SIZE);
            md4_write_i32_le(out + 0U, weight->bone_index);
            md4_write_f32_le_from_fx32(out + 4U, weight->bone_weight);
            md4_write_f32_le_from_fx32(out + 8U, weight->offset[0]);
            md4_write_f32_le_from_fx32(out + 12U, weight->offset[1]);
            md4_write_f32_le_from_fx32(out + 16U, weight->offset[2]);
            writer->pending_size = MD4_RUNTIME_WEIGHT_SIZE;
        }
        md4_writer_advance_after_vertex_weight(writer);
        return MD4_RC_OK;
    }
    case MD4_WR_PHASE_SURFACE_BONE_REF:
    {
        const md4_surface_t *surface;
        md4_i32 bref;
        surface = &model->surfaces[writer->current_surface_global];
        bref = model->bone_refs[surface->first_bone_ref + writer->bone_ref_index];
        md4_sw_write_zero(out, 4U);
        md4_write_i32_le(out + 0U, bref);
        writer->pending_size = 4U;
        md4_writer_advance_after_bone_ref(writer);
        return MD4_RC_OK;
    }
    default:
        return MD4_RC_INTERNAL;
    }
}

md4_rc_t md4_writer_begin(md4_writer_t *writer, const md4_model_t *model, const md4_encode_opts_t *opts)
{
    md4_rc_t rc;

    if (writer == 0 || model == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }

    md4_writer_init(writer);
    writer->model = model;
    rc = md4_writer_build_layout(&writer->layout, model, opts);
    if (rc != MD4_RC_OK) {
        writer->model = 0;
        return rc;
    }
    writer->phase = MD4_WR_PHASE_HEADER;
    writer->current_surface_global = -1;
    return MD4_RC_OK;
}

md4_i32 md4_writer_is_done(const md4_writer_t *writer)
{
    if (writer == 0) {
        return 1;
    }
    return (writer->phase == MD4_WR_PHASE_DONE && writer->pending_size == 0U) ? 1 : 0;
}

md4_u32 md4_writer_total_size(const md4_writer_t *writer)
{
    if (writer == 0) {
        return 0U;
    }
    return writer->layout.total_size;
}

md4_u32 md4_writer_bytes_written(const md4_writer_t *writer)
{
    if (writer == 0) {
        return 0U;
    }
    return writer->bytes_written;
}

md4_rc_t md4_writer_step(md4_writer_t *writer, md4_stream_sink_fn sink, void *user,
                         md4_u32 max_bytes, md4_u32 *out_emitted)
{
    md4_u32 emitted;
    md4_rc_t rc;

    if (writer == 0 || sink == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }

    emitted = 0U;
    while (!md4_writer_is_done(writer)) {
        md4_u32 chunk_size;
        md4_u32 remaining_budget;
        if (writer->pending_size == 0U) {
            rc = md4_writer_fill_pending(writer);
            if (rc != MD4_RC_OK) {
                return rc;
            }
            if (writer->pending_size == 0U && writer->phase == MD4_WR_PHASE_DONE) {
                break;
            }
        }

        remaining_budget = (max_bytes == 0U) ? 0xffffffffU : (max_bytes - emitted);
        if (max_bytes != 0U && emitted >= max_bytes) {
            break;
        }

        chunk_size = writer->pending_size - writer->pending_offset;
        if (max_bytes != 0U && chunk_size > remaining_budget) {
            chunk_size = remaining_budget;
        }
        if (chunk_size == 0U) {
            break;
        }

        rc = sink(user, writer->pending + writer->pending_offset, chunk_size);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        writer->pending_offset += chunk_size;
        writer->bytes_written += chunk_size;
        emitted += chunk_size;
        if (writer->pending_offset >= writer->pending_size) {
            writer->pending_offset = 0U;
            writer->pending_size = 0U;
        }
    }

    if (out_emitted != 0) {
        *out_emitted = emitted;
    }
    return MD4_RC_OK;
}

md4_rc_t md4_writer_write_all(md4_writer_t *writer, md4_stream_sink_fn sink, void *user)
{
    md4_rc_t rc;
    md4_u32 emitted;

    if (writer == 0 || sink == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }

    while (!md4_writer_is_done(writer)) {
        rc = md4_writer_step(writer, sink, user, 0U, &emitted);
        if (rc != MD4_RC_OK) {
            return rc;
        }
        if (emitted == 0U && !md4_writer_is_done(writer)) {
            return MD4_RC_INTERNAL;
        }
    }

    return MD4_RC_OK;
}

md4_rc_t md4_encode_stream(const md4_model_t *model, const md4_encode_opts_t *opts,
                           md4_stream_sink_fn sink, void *user)
{
    md4_writer_t writer;
    md4_rc_t rc;

    if (model == 0 || sink == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }

    md4_writer_init(&writer);
    rc = md4_writer_begin(&writer, model, opts);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    return md4_writer_write_all(&writer, sink, user);
}
