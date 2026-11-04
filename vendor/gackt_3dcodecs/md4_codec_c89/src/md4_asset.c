#include "md4_internal.h"

static md4_header_mode_t md4_asset_choose_header_mode(const md4_asset_ir_model_t *asset,
                                                      const md4_asset_export_opts_t *opts)
{
    if (opts != 0 && opts->header_mode != MD4_HEADER_AUTO) {
        return opts->header_mode;
    }
    if (asset->has_bone_names) {
        return MD4_HEADER_EXTENDED;
    }
    return MD4_HEADER_LEGACY;
}

static md4_vertex_mode_t md4_asset_choose_vertex_mode(const md4_asset_ir_model_t *asset,
                                                      const md4_asset_export_opts_t *opts)
{
    if (opts != 0 && opts->vertex_mode != MD4_VERT_AUTO) {
        return opts->vertex_mode;
    }
    if (asset->has_weight_offsets) {
        if (asset->has_rest_positions) {
            return MD4_VERT_PADDED;
        }
        return MD4_VERT_RUNTIME;
    }
    return MD4_VERT_DRAFT;
}

void md4_asset_ir_init(md4_asset_ir_model_t *asset)
{
    if (asset == 0) {
        return;
    }
    memset(asset, 0, sizeof(*asset));
}

md4_rc_t md4_asset_ir_validate(const md4_asset_ir_model_t *asset)
{
    md4_i32 i;
    md4_i32 expected_pose_bones;

    if (asset == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (asset->skeleton.num_bones < 0 || asset->skeleton.num_bones > MD4_CFG_MAX_BONES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (asset->mesh.num_lods < 0 || asset->mesh.num_lods > MD4_CFG_MAX_LODS) {
        return MD4_RC_BAD_COUNTS;
    }
    if (asset->mesh.num_surfaces < 0 || asset->mesh.num_surfaces > MD4_CFG_MAX_SURFACES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (asset->mesh.num_vertices < 0 || asset->mesh.num_vertices > MD4_CFG_MAX_VERTICES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (asset->mesh.num_triangles < 0 || asset->mesh.num_triangles > MD4_CFG_MAX_TRIANGLES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (asset->mesh.num_weights < 0 || asset->mesh.num_weights > MD4_CFG_MAX_WEIGHTS) {
        return MD4_RC_BAD_COUNTS;
    }
    if (asset->mesh.num_bone_palette < 0 || asset->mesh.num_bone_palette > MD4_CFG_MAX_BONE_REFS) {
        return MD4_RC_BAD_COUNTS;
    }
    if (asset->animations.num_clips < 0 || asset->animations.num_clips > MD4_CFG_MAX_ANIM_CLIPS) {
        return MD4_RC_BAD_COUNTS;
    }
    if (asset->animations.num_frames < 0 || asset->animations.num_frames > MD4_CFG_MAX_FRAMES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (asset->animations.num_pose_bones < 0 || asset->animations.num_pose_bones > MD4_CFG_MAX_FRAME_BONES) {
        return MD4_RC_BAD_COUNTS;
    }

    for (i = 0; i < asset->skeleton.num_bones; ++i) {
        md4_i32 parent;
        parent = asset->skeleton.bones[i].parent_index;
        if (parent >= asset->skeleton.num_bones) {
            return MD4_RC_BAD_COUNTS;
        }
        if (parent >= i) {
            if (parent != -1) {
                return MD4_RC_BAD_COUNTS;
            }
        }
    }

    for (i = 0; i < asset->mesh.num_lods; ++i) {
        const md4_asset_mesh_lod_t *lod;
        lod = &asset->mesh.lods[i];
        if (lod->first_surface < 0 || lod->num_surfaces < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (lod->first_surface + lod->num_surfaces > asset->mesh.num_surfaces) {
            return MD4_RC_BAD_COUNTS;
        }
    }

    for (i = 0; i < asset->mesh.num_surfaces; ++i) {
        const md4_asset_mesh_surface_t *surface;
        md4_i32 j;
        surface = &asset->mesh.surfaces[i];
        if (surface->first_vertex < 0 || surface->num_vertices < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (surface->first_triangle < 0 || surface->num_triangles < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (surface->first_bone_palette < 0 || surface->num_bone_palette < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (surface->first_vertex + surface->num_vertices > asset->mesh.num_vertices) {
            return MD4_RC_BAD_COUNTS;
        }
        if (surface->first_triangle + surface->num_triangles > asset->mesh.num_triangles) {
            return MD4_RC_BAD_COUNTS;
        }
        if (surface->first_bone_palette + surface->num_bone_palette > asset->mesh.num_bone_palette) {
            return MD4_RC_BAD_COUNTS;
        }
        for (j = 0; j < surface->num_triangles; ++j) {
            const md4_asset_mesh_triangle_t *tri;
            tri = &asset->mesh.triangles[surface->first_triangle + j];
            if (tri->indexes[0] < 0 || tri->indexes[1] < 0 || tri->indexes[2] < 0) {
                return MD4_RC_BAD_COUNTS;
            }
            if (tri->indexes[0] >= surface->num_vertices ||
                tri->indexes[1] >= surface->num_vertices ||
                tri->indexes[2] >= surface->num_vertices) {
                return MD4_RC_BAD_COUNTS;
            }
        }
        for (j = 0; j < surface->num_bone_palette; ++j) {
            md4_i32 b;
            b = asset->mesh.bone_palette[surface->first_bone_palette + j];
            if (b < 0 || b >= asset->skeleton.num_bones) {
                return MD4_RC_BAD_COUNTS;
            }
        }
    }

    for (i = 0; i < asset->mesh.num_vertices; ++i) {
        const md4_asset_mesh_vertex_t *v;
        md4_i32 j;
        v = &asset->mesh.vertices[i];
        if (v->first_weight < 0 || v->num_weights < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (v->first_weight + v->num_weights > asset->mesh.num_weights) {
            return MD4_RC_BAD_COUNTS;
        }
        for (j = 0; j < v->num_weights; ++j) {
            const md4_asset_mesh_weight_t *w;
            w = &asset->mesh.weights[v->first_weight + j];
            if (w->bone_index < 0 || w->bone_index >= asset->skeleton.num_bones) {
                return MD4_RC_BAD_COUNTS;
            }
        }
    }

    expected_pose_bones = asset->animations.num_frames * asset->skeleton.num_bones;
    if (asset->animations.num_pose_bones != expected_pose_bones) {
        return MD4_RC_BAD_COUNTS;
    }
    for (i = 0; i < asset->animations.num_frames; ++i) {
        const md4_asset_anim_frame_t *frame;
        frame = &asset->animations.frames[i];
        if (frame->first_pose_bone != i * asset->skeleton.num_bones) {
            return MD4_RC_BAD_COUNTS;
        }
        if (frame->first_pose_bone < 0 ||
            frame->first_pose_bone + asset->skeleton.num_bones > asset->animations.num_pose_bones) {
            return MD4_RC_BAD_COUNTS;
        }
    }
    for (i = 0; i < asset->animations.num_clips; ++i) {
        const md4_asset_anim_clip_t *clip;
        clip = &asset->animations.clips[i];
        if (clip->first_frame < 0 || clip->num_frames < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (clip->first_frame + clip->num_frames > asset->animations.num_frames) {
            return MD4_RC_BAD_COUNTS;
        }
    }

    return MD4_RC_OK;
}

md4_rc_t md4_asset_ir_from_model(const md4_model_t *model, md4_asset_ir_model_t *out_asset)
{
    md4_rc_t rc;
    md4_i32 i;

    if (model == 0 || out_asset == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    rc = md4_validate_model(model);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    md4_asset_ir_init(out_asset);
    md4_copy_qpath(out_asset->name, (const md4_u8 *)model->name);
    out_asset->has_frame_names = model->layout.frame_has_name;
    out_asset->has_bone_names = model->layout.has_bone_names;
    out_asset->has_rest_positions = model->layout.vertex_has_rest_pos;
    out_asset->has_weight_offsets = model->layout.weights_have_offset;

    out_asset->skeleton.num_bones = model->num_bones;
    for (i = 0; i < model->num_bones; ++i) {
        md4_copy_qpath(out_asset->skeleton.bones[i].name, (const md4_u8 *)model->bone_names[i]);
        out_asset->skeleton.bones[i].parent_index = -1;
    }

    out_asset->mesh.num_lods = model->num_lods;
    out_asset->mesh.num_surfaces = model->num_surfaces;
    out_asset->mesh.num_vertices = model->num_vertices;
    out_asset->mesh.num_triangles = model->num_triangles;
    out_asset->mesh.num_weights = model->num_weights;
    out_asset->mesh.num_bone_palette = model->num_bone_refs;

    for (i = 0; i < model->num_lods; ++i) {
        out_asset->mesh.lods[i].first_surface = model->lods[i].first_surface;
        out_asset->mesh.lods[i].num_surfaces = model->lods[i].num_surfaces;
    }
    for (i = 0; i < model->num_surfaces; ++i) {
        out_asset->mesh.surfaces[i].material_index = model->surfaces[i].shader_index;
        out_asset->mesh.surfaces[i].first_vertex = model->surfaces[i].first_vertex;
        out_asset->mesh.surfaces[i].num_vertices = model->surfaces[i].num_vertices;
        out_asset->mesh.surfaces[i].first_triangle = model->surfaces[i].first_triangle;
        out_asset->mesh.surfaces[i].num_triangles = model->surfaces[i].num_triangles;
        out_asset->mesh.surfaces[i].first_bone_palette = model->surfaces[i].first_bone_ref;
        out_asset->mesh.surfaces[i].num_bone_palette = model->surfaces[i].num_bone_refs;
        md4_copy_qpath(out_asset->mesh.surfaces[i].name, (const md4_u8 *)model->surfaces[i].name);
        md4_copy_qpath(out_asset->mesh.surfaces[i].material, (const md4_u8 *)model->surfaces[i].shader);
    }
    for (i = 0; i < model->num_vertices; ++i) {
        md4_i32 j;
        out_asset->mesh.vertices[i].first_weight = model->vertices[i].first_weight;
        out_asset->mesh.vertices[i].num_weights = model->vertices[i].num_weights;
        for (j = 0; j < 3; ++j) {
            out_asset->mesh.vertices[i].rest_pos[j] = model->vertices[i].rest_pos[j];
            out_asset->mesh.vertices[i].normal[j] = model->vertices[i].normal[j];
        }
        out_asset->mesh.vertices[i].uv[0] = model->vertices[i].uv[0];
        out_asset->mesh.vertices[i].uv[1] = model->vertices[i].uv[1];
    }
    for (i = 0; i < model->num_triangles; ++i) {
        out_asset->mesh.triangles[i].indexes[0] = model->triangles[i].indexes[0];
        out_asset->mesh.triangles[i].indexes[1] = model->triangles[i].indexes[1];
        out_asset->mesh.triangles[i].indexes[2] = model->triangles[i].indexes[2];
    }
    for (i = 0; i < model->num_bone_refs; ++i) {
        out_asset->mesh.bone_palette[i] = model->bone_refs[i];
    }
    for (i = 0; i < model->num_surfaces; ++i) {
        const md4_surface_t *surface;
        md4_i32 v;
        surface = &model->surfaces[i];
        for (v = 0; v < surface->num_vertices; ++v) {
            const md4_vertex_t *src_v;
            md4_i32 w;
            src_v = &model->vertices[surface->first_vertex + v];
            for (w = 0; w < src_v->num_weights; ++w) {
                md4_i32 wi;
                md4_i32 global_bone;
                const md4_weight_t *src_w;
                md4_asset_mesh_weight_t *dst_w;
                wi = src_v->first_weight + w;
                src_w = &model->weights[wi];
                dst_w = &out_asset->mesh.weights[wi];
                global_bone = md4_resolve_bone_index(model, surface, src_w->bone_index);
                if (global_bone < 0 || global_bone >= model->num_bones) {
                    return MD4_RC_BAD_COUNTS;
                }
                dst_w->bone_index = global_bone;
                dst_w->bone_weight = src_w->bone_weight;
                dst_w->offset[0] = src_w->offset[0];
                dst_w->offset[1] = src_w->offset[1];
                dst_w->offset[2] = src_w->offset[2];
            }
        }
    }

    out_asset->animations.num_clips = 1;
    out_asset->animations.num_frames = model->num_frames;
    out_asset->animations.num_pose_bones = model->num_frame_bones;
    md4_copy_qpath(out_asset->animations.clips[0].name, (const md4_u8 *)"default");
    out_asset->animations.clips[0].first_frame = 0;
    out_asset->animations.clips[0].num_frames = model->num_frames;
    out_asset->animations.clips[0].frame_rate = 0;

    for (i = 0; i < model->num_frames; ++i) {
        md4_i32 j;
        out_asset->animations.frames[i].radius = model->frames[i].radius;
        out_asset->animations.frames[i].first_pose_bone = i * model->num_bones;
        md4_copy_frame_name(out_asset->animations.frames[i].name,
                            (const md4_u8 *)model->frames[i].name,
                            model->layout.frame_has_name);
        for (j = 0; j < 3; ++j) {
            out_asset->animations.frames[i].bounds[0][j] = model->frames[i].bounds[0][j];
            out_asset->animations.frames[i].bounds[1][j] = model->frames[i].bounds[1][j];
            out_asset->animations.frames[i].local_origin[j] = model->frames[i].local_origin[j];
        }
    }
    for (i = 0; i < model->num_frame_bones; ++i) {
        out_asset->animations.pose_bones[i].matrix[0][0] = model->frame_bones[i].matrix[0][0];
        out_asset->animations.pose_bones[i].matrix[0][1] = model->frame_bones[i].matrix[0][1];
        out_asset->animations.pose_bones[i].matrix[0][2] = model->frame_bones[i].matrix[0][2];
        out_asset->animations.pose_bones[i].matrix[0][3] = model->frame_bones[i].matrix[0][3];
        out_asset->animations.pose_bones[i].matrix[1][0] = model->frame_bones[i].matrix[1][0];
        out_asset->animations.pose_bones[i].matrix[1][1] = model->frame_bones[i].matrix[1][1];
        out_asset->animations.pose_bones[i].matrix[1][2] = model->frame_bones[i].matrix[1][2];
        out_asset->animations.pose_bones[i].matrix[1][3] = model->frame_bones[i].matrix[1][3];
        out_asset->animations.pose_bones[i].matrix[2][0] = model->frame_bones[i].matrix[2][0];
        out_asset->animations.pose_bones[i].matrix[2][1] = model->frame_bones[i].matrix[2][1];
        out_asset->animations.pose_bones[i].matrix[2][2] = model->frame_bones[i].matrix[2][2];
        out_asset->animations.pose_bones[i].matrix[2][3] = model->frame_bones[i].matrix[2][3];
    }

    return md4_asset_ir_validate(out_asset);
}

md4_rc_t md4_model_from_asset_ir(const md4_asset_ir_model_t *asset,
                                 const md4_asset_export_opts_t *opts,
                                 md4_model_t *out_model)
{
    md4_rc_t rc;
    md4_header_mode_t header_mode;
    md4_vertex_mode_t vertex_mode;
    md4_i32 write_frame_names;
    md4_i32 write_bone_names;
    md4_i32 clip_index;
    md4_i32 i;
    const md4_asset_anim_clip_t *clip;

    if (asset == 0 || out_model == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    rc = md4_asset_ir_validate(asset);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    header_mode = md4_asset_choose_header_mode(asset, opts);
    vertex_mode = md4_asset_choose_vertex_mode(asset, opts);
    write_frame_names = (opts != 0) ? (opts->write_frame_names ? 1 : 0) : asset->has_frame_names;
    write_bone_names = (opts != 0) ? (opts->write_bone_names ? 1 : 0) : asset->has_bone_names;
    clip_index = (opts != 0) ? opts->clip_index : 0;

    if (header_mode != MD4_HEADER_LEGACY && header_mode != MD4_HEADER_EXTENDED) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (vertex_mode != MD4_VERT_RUNTIME && vertex_mode != MD4_VERT_PADDED && vertex_mode != MD4_VERT_DRAFT) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if ((vertex_mode == MD4_VERT_RUNTIME || vertex_mode == MD4_VERT_PADDED) && !asset->has_weight_offsets) {
        return MD4_RC_UNSUPPORTED;
    }
    if (header_mode != MD4_HEADER_EXTENDED) {
        write_bone_names = 0;
    }
    if (clip_index < 0 || clip_index >= asset->animations.num_clips) {
        return MD4_RC_BAD_ARGUMENT;
    }

    clip = &asset->animations.clips[clip_index];

    md4_model_init(out_model);
    md4_copy_qpath(out_model->name, (const md4_u8 *)asset->name);
    out_model->layout.header_mode = header_mode;
    out_model->layout.vertex_mode = vertex_mode;
    out_model->layout.frame_has_name = write_frame_names;
    out_model->layout.has_bone_names = write_bone_names;
    out_model->layout.vertex_has_rest_pos = (vertex_mode == MD4_VERT_RUNTIME) ? 0 : 1;
    out_model->layout.weights_have_offset = (vertex_mode == MD4_VERT_DRAFT) ? 0 : 1;

    out_model->num_bones = asset->skeleton.num_bones;
    out_model->num_frames = clip->num_frames;
    out_model->num_lods = asset->mesh.num_lods;
    out_model->num_surfaces = asset->mesh.num_surfaces;
    out_model->num_vertices = asset->mesh.num_vertices;
    out_model->num_triangles = asset->mesh.num_triangles;
    out_model->num_weights = asset->mesh.num_weights;
    out_model->num_frame_bones = clip->num_frames * asset->skeleton.num_bones;
    out_model->num_bone_refs = 0;

    for (i = 0; i < asset->skeleton.num_bones; ++i) {
        if (write_bone_names) {
            md4_copy_qpath(out_model->bone_names[i], (const md4_u8 *)asset->skeleton.bones[i].name);
        }
    }

    for (i = 0; i < clip->num_frames; ++i) {
        const md4_asset_anim_frame_t *src_frame;
        md4_i32 src_frame_index;
        md4_i32 src_pose_base;
        md4_i32 j;

        src_frame_index = clip->first_frame + i;
        src_frame = &asset->animations.frames[src_frame_index];
        src_pose_base = src_frame->first_pose_bone;
        if (src_pose_base < 0 || src_pose_base + asset->skeleton.num_bones > asset->animations.num_pose_bones) {
            return MD4_RC_BAD_COUNTS;
        }

        out_model->frames[i].radius = src_frame->radius;
        out_model->frames[i].first_bone = i * asset->skeleton.num_bones;
        if (write_frame_names) {
            md4_copy_frame_name(out_model->frames[i].name,
                                (const md4_u8 *)src_frame->name,
                                1);
        }
        for (j = 0; j < 3; ++j) {
            out_model->frames[i].bounds[0][j] = src_frame->bounds[0][j];
            out_model->frames[i].bounds[1][j] = src_frame->bounds[1][j];
            out_model->frames[i].local_origin[j] = src_frame->local_origin[j];
        }
        for (j = 0; j < asset->skeleton.num_bones; ++j) {
            const md4_asset_anim_pose_bone_t *src_bone;
            md4_bone_t *dst_bone;
            src_bone = &asset->animations.pose_bones[src_pose_base + j];
            dst_bone = &out_model->frame_bones[i * asset->skeleton.num_bones + j];
            dst_bone->matrix[0][0] = src_bone->matrix[0][0];
            dst_bone->matrix[0][1] = src_bone->matrix[0][1];
            dst_bone->matrix[0][2] = src_bone->matrix[0][2];
            dst_bone->matrix[0][3] = src_bone->matrix[0][3];
            dst_bone->matrix[1][0] = src_bone->matrix[1][0];
            dst_bone->matrix[1][1] = src_bone->matrix[1][1];
            dst_bone->matrix[1][2] = src_bone->matrix[1][2];
            dst_bone->matrix[1][3] = src_bone->matrix[1][3];
            dst_bone->matrix[2][0] = src_bone->matrix[2][0];
            dst_bone->matrix[2][1] = src_bone->matrix[2][1];
            dst_bone->matrix[2][2] = src_bone->matrix[2][2];
            dst_bone->matrix[2][3] = src_bone->matrix[2][3];
        }
    }

    for (i = 0; i < asset->mesh.num_lods; ++i) {
        out_model->lods[i].first_surface = asset->mesh.lods[i].first_surface;
        out_model->lods[i].num_surfaces = asset->mesh.lods[i].num_surfaces;
    }
    for (i = 0; i < asset->mesh.num_surfaces; ++i) {
        out_model->surfaces[i].shader_index = asset->mesh.surfaces[i].material_index;
        out_model->surfaces[i].first_vertex = asset->mesh.surfaces[i].first_vertex;
        out_model->surfaces[i].num_vertices = asset->mesh.surfaces[i].num_vertices;
        out_model->surfaces[i].first_triangle = asset->mesh.surfaces[i].first_triangle;
        out_model->surfaces[i].num_triangles = asset->mesh.surfaces[i].num_triangles;
        out_model->surfaces[i].first_bone_ref = 0;
        out_model->surfaces[i].num_bone_refs = 0;
        out_model->surfaces[i].ofs_header_raw = 0;
        md4_copy_qpath(out_model->surfaces[i].name, (const md4_u8 *)asset->mesh.surfaces[i].name);
        md4_copy_qpath(out_model->surfaces[i].shader, (const md4_u8 *)asset->mesh.surfaces[i].material);
    }
    for (i = 0; i < asset->mesh.num_vertices; ++i) {
        md4_i32 j;
        out_model->vertices[i].first_weight = asset->mesh.vertices[i].first_weight;
        out_model->vertices[i].num_weights = asset->mesh.vertices[i].num_weights;
        for (j = 0; j < 3; ++j) {
            out_model->vertices[i].rest_pos[j] = asset->mesh.vertices[i].rest_pos[j];
            out_model->vertices[i].normal[j] = asset->mesh.vertices[i].normal[j];
        }
        out_model->vertices[i].uv[0] = asset->mesh.vertices[i].uv[0];
        out_model->vertices[i].uv[1] = asset->mesh.vertices[i].uv[1];
    }
    for (i = 0; i < asset->mesh.num_triangles; ++i) {
        out_model->triangles[i].indexes[0] = asset->mesh.triangles[i].indexes[0];
        out_model->triangles[i].indexes[1] = asset->mesh.triangles[i].indexes[1];
        out_model->triangles[i].indexes[2] = asset->mesh.triangles[i].indexes[2];
    }

    for (i = 0; i < asset->mesh.num_surfaces; ++i) {
        const md4_asset_mesh_surface_t *mesh_surface;
        md4_surface_t *surface;
        md4_i32 bone_local[MD4_CFG_MAX_BONES];
        md4_i32 p;
        md4_i32 v;
        md4_i32 b;

        mesh_surface = &asset->mesh.surfaces[i];
        surface = &out_model->surfaces[i];
        surface->first_bone_ref = out_model->num_bone_refs;
        surface->num_bone_refs = 0;
        for (b = 0; b < asset->skeleton.num_bones; ++b) {
            bone_local[b] = -1;
        }

        for (p = 0; p < mesh_surface->num_bone_palette; ++p) {
            md4_i32 global_bone;
            global_bone = asset->mesh.bone_palette[mesh_surface->first_bone_palette + p];
            if (global_bone < 0 || global_bone >= asset->skeleton.num_bones) {
                return MD4_RC_BAD_COUNTS;
            }
            if (bone_local[global_bone] < 0) {
                if (out_model->num_bone_refs >= MD4_CFG_MAX_BONE_REFS) {
                    return MD4_RC_CAPACITY;
                }
                bone_local[global_bone] = surface->num_bone_refs;
                out_model->bone_refs[out_model->num_bone_refs] = global_bone;
                ++out_model->num_bone_refs;
                ++surface->num_bone_refs;
            }
        }

        for (v = 0; v < mesh_surface->num_vertices; ++v) {
            const md4_asset_mesh_vertex_t *src_v;
            md4_i32 w;
            src_v = &asset->mesh.vertices[mesh_surface->first_vertex + v];
            for (w = 0; w < src_v->num_weights; ++w) {
                md4_i32 wi;
                md4_i32 global_bone;
                const md4_asset_mesh_weight_t *src_w;
                md4_weight_t *dst_w;
                wi = src_v->first_weight + w;
                src_w = &asset->mesh.weights[wi];
                dst_w = &out_model->weights[wi];
                global_bone = src_w->bone_index;
                if (global_bone < 0 || global_bone >= asset->skeleton.num_bones) {
                    return MD4_RC_BAD_COUNTS;
                }
                if (bone_local[global_bone] < 0) {
                    if (out_model->num_bone_refs >= MD4_CFG_MAX_BONE_REFS) {
                        return MD4_RC_CAPACITY;
                    }
                    bone_local[global_bone] = surface->num_bone_refs;
                    out_model->bone_refs[out_model->num_bone_refs] = global_bone;
                    ++out_model->num_bone_refs;
                    ++surface->num_bone_refs;
                }
                dst_w->bone_index = bone_local[global_bone];
                dst_w->bone_weight = src_w->bone_weight;
                dst_w->offset[0] = src_w->offset[0];
                dst_w->offset[1] = src_w->offset[1];
                dst_w->offset[2] = src_w->offset[2];
            }
        }
    }

    if (!asset->has_rest_positions) {
        if (vertex_mode == MD4_VERT_PADDED || vertex_mode == MD4_VERT_DRAFT) {
            for (i = 0; i < out_model->num_vertices; ++i) {
                out_model->vertices[i].rest_pos[0] = 0;
                out_model->vertices[i].rest_pos[1] = 0;
                out_model->vertices[i].rest_pos[2] = 0;
            }
        }
    }
    if (!write_frame_names) {
        for (i = 0; i < out_model->num_frames; ++i) {
            out_model->frames[i].name[0] = 0;
        }
    }
    if (!write_bone_names) {
        for (i = 0; i < out_model->num_bones; ++i) {
            out_model->bone_names[i][0] = 0;
        }
    }

    return md4_validate_model(out_model);
}
