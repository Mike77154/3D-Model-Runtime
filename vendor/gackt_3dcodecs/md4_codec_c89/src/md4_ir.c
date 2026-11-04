#include "md4_internal.h"

void md4_ir_init(md4_ir_model_t *ir)
{
    if (ir == 0) {
        return;
    }
    memset(ir, 0, sizeof(*ir));
}

md4_rc_t md4_ir_validate(const md4_ir_model_t *ir)
{
    md4_i32 i;
    md4_i32 expected_pose_bones;

    if (ir == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (ir->num_frames < 0 || ir->num_frames > MD4_CFG_MAX_FRAMES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (ir->num_bones < 0 || ir->num_bones > MD4_CFG_MAX_BONES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (ir->num_lods < 0 || ir->num_lods > MD4_CFG_MAX_LODS) {
        return MD4_RC_BAD_COUNTS;
    }
    if (ir->num_meshes < 0 || ir->num_meshes > MD4_CFG_MAX_SURFACES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (ir->num_vertices < 0 || ir->num_vertices > MD4_CFG_MAX_VERTICES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (ir->num_triangles < 0 || ir->num_triangles > MD4_CFG_MAX_TRIANGLES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (ir->num_weights < 0 || ir->num_weights > MD4_CFG_MAX_WEIGHTS) {
        return MD4_RC_BAD_COUNTS;
    }

    expected_pose_bones = ir->num_frames * ir->num_bones;
    if (ir->num_pose_bones != expected_pose_bones) {
        return MD4_RC_BAD_COUNTS;
    }

    for (i = 0; i < ir->num_frames; ++i) {
        if (ir->frames[i].first_pose_bone != i * ir->num_bones) {
            return MD4_RC_BAD_COUNTS;
        }
        if (ir->frames[i].first_pose_bone < 0 ||
            ir->frames[i].first_pose_bone + ir->num_bones > ir->num_pose_bones) {
            return MD4_RC_BAD_COUNTS;
        }
    }

    for (i = 0; i < ir->num_lods; ++i) {
        const md4_ir_lod_t *lod;
        lod = &ir->lods[i];
        if (lod->first_mesh < 0 || lod->num_meshes < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (lod->first_mesh + lod->num_meshes > ir->num_meshes) {
            return MD4_RC_BAD_COUNTS;
        }
    }

    for (i = 0; i < ir->num_meshes; ++i) {
        const md4_ir_mesh_t *mesh;
        md4_i32 j;
        mesh = &ir->meshes[i];
        if (mesh->first_vertex < 0 || mesh->num_vertices < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (mesh->first_triangle < 0 || mesh->num_triangles < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (mesh->first_vertex + mesh->num_vertices > ir->num_vertices) {
            return MD4_RC_BAD_COUNTS;
        }
        if (mesh->first_triangle + mesh->num_triangles > ir->num_triangles) {
            return MD4_RC_BAD_COUNTS;
        }
        for (j = 0; j < mesh->num_triangles; ++j) {
            const md4_ir_triangle_t *tri;
            tri = &ir->triangles[mesh->first_triangle + j];
            if (tri->indexes[0] < 0 || tri->indexes[1] < 0 || tri->indexes[2] < 0) {
                return MD4_RC_BAD_COUNTS;
            }
            if (tri->indexes[0] >= mesh->num_vertices ||
                tri->indexes[1] >= mesh->num_vertices ||
                tri->indexes[2] >= mesh->num_vertices) {
                return MD4_RC_BAD_COUNTS;
            }
        }
    }

    for (i = 0; i < ir->num_vertices; ++i) {
        const md4_ir_vertex_t *v;
        md4_i32 j;
        v = &ir->vertices[i];
        if (v->first_weight < 0 || v->num_weights < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (v->first_weight + v->num_weights > ir->num_weights) {
            return MD4_RC_BAD_COUNTS;
        }
        for (j = 0; j < v->num_weights; ++j) {
            const md4_ir_weight_t *w;
            w = &ir->weights[v->first_weight + j];
            if (w->bone_index < 0 || w->bone_index >= ir->num_bones) {
                return MD4_RC_BAD_COUNTS;
            }
        }
    }

    return MD4_RC_OK;
}

md4_rc_t md4_ir_from_model(const md4_model_t *model, md4_ir_model_t *out_ir)
{
    md4_rc_t rc;
    md4_i32 i;

    if (model == 0 || out_ir == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }

    rc = md4_validate_model(model);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    md4_ir_init(out_ir);
    md4_copy_qpath(out_ir->name, (const md4_u8 *)model->name);
    out_ir->has_frame_names = model->layout.frame_has_name;
    out_ir->has_bone_names = model->layout.has_bone_names;
    out_ir->has_rest_positions = model->layout.vertex_has_rest_pos;
    out_ir->has_weight_offsets = model->layout.weights_have_offset;

    out_ir->num_frames = model->num_frames;
    out_ir->num_bones = model->num_bones;
    out_ir->num_lods = model->num_lods;
    out_ir->num_meshes = model->num_surfaces;
    out_ir->num_vertices = model->num_vertices;
    out_ir->num_triangles = model->num_triangles;
    out_ir->num_weights = model->num_weights;
    out_ir->num_pose_bones = model->num_frame_bones;

    for (i = 0; i < model->num_bones; ++i) {
        md4_copy_qpath(out_ir->bones[i].name, (const md4_u8 *)model->bone_names[i]);
    }

    for (i = 0; i < model->num_frames; ++i) {
        md4_i32 j;
        out_ir->frames[i].radius = model->frames[i].radius;
        out_ir->frames[i].first_pose_bone = model->frames[i].first_bone;
        md4_copy_frame_name(out_ir->frames[i].name,
                            (const md4_u8 *)model->frames[i].name,
                            model->layout.frame_has_name);
        for (j = 0; j < 3; ++j) {
            out_ir->frames[i].bounds[0][j] = model->frames[i].bounds[0][j];
            out_ir->frames[i].bounds[1][j] = model->frames[i].bounds[1][j];
            out_ir->frames[i].local_origin[j] = model->frames[i].local_origin[j];
        }
    }

    for (i = 0; i < model->num_frame_bones; ++i) {
        out_ir->pose_bones[i].matrix[0][0] = model->frame_bones[i].matrix[0][0];
        out_ir->pose_bones[i].matrix[0][1] = model->frame_bones[i].matrix[0][1];
        out_ir->pose_bones[i].matrix[0][2] = model->frame_bones[i].matrix[0][2];
        out_ir->pose_bones[i].matrix[0][3] = model->frame_bones[i].matrix[0][3];
        out_ir->pose_bones[i].matrix[1][0] = model->frame_bones[i].matrix[1][0];
        out_ir->pose_bones[i].matrix[1][1] = model->frame_bones[i].matrix[1][1];
        out_ir->pose_bones[i].matrix[1][2] = model->frame_bones[i].matrix[1][2];
        out_ir->pose_bones[i].matrix[1][3] = model->frame_bones[i].matrix[1][3];
        out_ir->pose_bones[i].matrix[2][0] = model->frame_bones[i].matrix[2][0];
        out_ir->pose_bones[i].matrix[2][1] = model->frame_bones[i].matrix[2][1];
        out_ir->pose_bones[i].matrix[2][2] = model->frame_bones[i].matrix[2][2];
        out_ir->pose_bones[i].matrix[2][3] = model->frame_bones[i].matrix[2][3];
    }

    for (i = 0; i < model->num_lods; ++i) {
        out_ir->lods[i].first_mesh = model->lods[i].first_surface;
        out_ir->lods[i].num_meshes = model->lods[i].num_surfaces;
    }

    for (i = 0; i < model->num_surfaces; ++i) {
        out_ir->meshes[i].material_index = model->surfaces[i].shader_index;
        out_ir->meshes[i].first_vertex = model->surfaces[i].first_vertex;
        out_ir->meshes[i].num_vertices = model->surfaces[i].num_vertices;
        out_ir->meshes[i].first_triangle = model->surfaces[i].first_triangle;
        out_ir->meshes[i].num_triangles = model->surfaces[i].num_triangles;
        md4_copy_qpath(out_ir->meshes[i].name, (const md4_u8 *)model->surfaces[i].name);
        md4_copy_qpath(out_ir->meshes[i].material, (const md4_u8 *)model->surfaces[i].shader);
    }

    for (i = 0; i < model->num_vertices; ++i) {
        md4_i32 j;
        out_ir->vertices[i].first_weight = model->vertices[i].first_weight;
        out_ir->vertices[i].num_weights = model->vertices[i].num_weights;
        for (j = 0; j < 3; ++j) {
            out_ir->vertices[i].rest_pos[j] = model->vertices[i].rest_pos[j];
            out_ir->vertices[i].normal[j] = model->vertices[i].normal[j];
        }
        out_ir->vertices[i].uv[0] = model->vertices[i].uv[0];
        out_ir->vertices[i].uv[1] = model->vertices[i].uv[1];
    }

    for (i = 0; i < model->num_triangles; ++i) {
        out_ir->triangles[i].indexes[0] = model->triangles[i].indexes[0];
        out_ir->triangles[i].indexes[1] = model->triangles[i].indexes[1];
        out_ir->triangles[i].indexes[2] = model->triangles[i].indexes[2];
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
                const md4_weight_t *src_w;
                md4_ir_weight_t *dst_w;
                md4_i32 global_bone;
                wi = src_v->first_weight + w;
                src_w = &model->weights[wi];
                dst_w = &out_ir->weights[wi];
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

    return md4_ir_validate(out_ir);
}

static md4_header_mode_t md4_ir_choose_header_mode(const md4_ir_model_t *ir,
                                                   const md4_ir_export_opts_t *opts)
{
    if (opts != 0 && opts->header_mode != MD4_HEADER_AUTO) {
        return opts->header_mode;
    }
    if (ir->has_bone_names) {
        return MD4_HEADER_EXTENDED;
    }
    return MD4_HEADER_LEGACY;
}

static md4_vertex_mode_t md4_ir_choose_vertex_mode(const md4_ir_model_t *ir,
                                                   const md4_ir_export_opts_t *opts)
{
    if (opts != 0 && opts->vertex_mode != MD4_VERT_AUTO) {
        return opts->vertex_mode;
    }
    if (ir->has_weight_offsets) {
        if (ir->has_rest_positions) {
            return MD4_VERT_PADDED;
        }
        return MD4_VERT_RUNTIME;
    }
    return MD4_VERT_DRAFT;
}

md4_rc_t md4_model_from_ir(const md4_ir_model_t *ir, const md4_ir_export_opts_t *opts,
                           md4_model_t *out_model)
{
    md4_vertex_mode_t vertex_mode;
    md4_header_mode_t header_mode;
    md4_i32 write_frame_names;
    md4_i32 write_bone_names;
    md4_rc_t rc;
    md4_i32 i;

    if (ir == 0 || out_model == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }

    rc = md4_ir_validate(ir);
    if (rc != MD4_RC_OK) {
        return rc;
    }

    header_mode = md4_ir_choose_header_mode(ir, opts);
    vertex_mode = md4_ir_choose_vertex_mode(ir, opts);
    write_frame_names = (opts != 0) ? (opts->write_frame_names ? 1 : 0) : ir->has_frame_names;
    write_bone_names = (opts != 0) ? (opts->write_bone_names ? 1 : 0) : ir->has_bone_names;

    if (header_mode != MD4_HEADER_LEGACY && header_mode != MD4_HEADER_EXTENDED) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (vertex_mode != MD4_VERT_RUNTIME && vertex_mode != MD4_VERT_PADDED && vertex_mode != MD4_VERT_DRAFT) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if ((vertex_mode == MD4_VERT_RUNTIME || vertex_mode == MD4_VERT_PADDED) && !ir->has_weight_offsets) {
        return MD4_RC_UNSUPPORTED;
    }
    if (header_mode != MD4_HEADER_EXTENDED) {
        write_bone_names = 0;
    }

    md4_model_init(out_model);
    md4_copy_qpath(out_model->name, (const md4_u8 *)ir->name);

    out_model->layout.header_mode = header_mode;
    out_model->layout.vertex_mode = vertex_mode;
    out_model->layout.frame_has_name = write_frame_names;
    out_model->layout.has_bone_names = write_bone_names;
    out_model->layout.vertex_has_rest_pos = (vertex_mode == MD4_VERT_RUNTIME) ? 0 : 1;
    out_model->layout.weights_have_offset = (vertex_mode == MD4_VERT_DRAFT) ? 0 : 1;

    out_model->num_frames = ir->num_frames;
    out_model->num_bones = ir->num_bones;
    out_model->num_lods = ir->num_lods;
    out_model->num_surfaces = ir->num_meshes;
    out_model->num_vertices = ir->num_vertices;
    out_model->num_triangles = ir->num_triangles;
    out_model->num_weights = ir->num_weights;
    out_model->num_frame_bones = ir->num_pose_bones;
    out_model->num_bone_refs = 0;

    for (i = 0; i < ir->num_bones; ++i) {
        if (write_bone_names) {
            md4_copy_qpath(out_model->bone_names[i], (const md4_u8 *)ir->bones[i].name);
        }
    }

    for (i = 0; i < ir->num_frames; ++i) {
        md4_i32 j;
        out_model->frames[i].radius = ir->frames[i].radius;
        out_model->frames[i].first_bone = ir->frames[i].first_pose_bone;
        if (write_frame_names) {
            md4_copy_frame_name(out_model->frames[i].name,
                                (const md4_u8 *)ir->frames[i].name,
                                1);
        }
        for (j = 0; j < 3; ++j) {
            out_model->frames[i].bounds[0][j] = ir->frames[i].bounds[0][j];
            out_model->frames[i].bounds[1][j] = ir->frames[i].bounds[1][j];
            out_model->frames[i].local_origin[j] = ir->frames[i].local_origin[j];
        }
    }

    for (i = 0; i < ir->num_pose_bones; ++i) {
        out_model->frame_bones[i].matrix[0][0] = ir->pose_bones[i].matrix[0][0];
        out_model->frame_bones[i].matrix[0][1] = ir->pose_bones[i].matrix[0][1];
        out_model->frame_bones[i].matrix[0][2] = ir->pose_bones[i].matrix[0][2];
        out_model->frame_bones[i].matrix[0][3] = ir->pose_bones[i].matrix[0][3];
        out_model->frame_bones[i].matrix[1][0] = ir->pose_bones[i].matrix[1][0];
        out_model->frame_bones[i].matrix[1][1] = ir->pose_bones[i].matrix[1][1];
        out_model->frame_bones[i].matrix[1][2] = ir->pose_bones[i].matrix[1][2];
        out_model->frame_bones[i].matrix[1][3] = ir->pose_bones[i].matrix[1][3];
        out_model->frame_bones[i].matrix[2][0] = ir->pose_bones[i].matrix[2][0];
        out_model->frame_bones[i].matrix[2][1] = ir->pose_bones[i].matrix[2][1];
        out_model->frame_bones[i].matrix[2][2] = ir->pose_bones[i].matrix[2][2];
        out_model->frame_bones[i].matrix[2][3] = ir->pose_bones[i].matrix[2][3];
    }

    for (i = 0; i < ir->num_lods; ++i) {
        out_model->lods[i].first_surface = ir->lods[i].first_mesh;
        out_model->lods[i].num_surfaces = ir->lods[i].num_meshes;
    }

    for (i = 0; i < ir->num_meshes; ++i) {
        out_model->surfaces[i].shader_index = ir->meshes[i].material_index;
        out_model->surfaces[i].first_vertex = ir->meshes[i].first_vertex;
        out_model->surfaces[i].num_vertices = ir->meshes[i].num_vertices;
        out_model->surfaces[i].first_triangle = ir->meshes[i].first_triangle;
        out_model->surfaces[i].num_triangles = ir->meshes[i].num_triangles;
        out_model->surfaces[i].first_bone_ref = out_model->num_bone_refs;
        out_model->surfaces[i].num_bone_refs = 0;
        out_model->surfaces[i].ofs_header_raw = 0;
        md4_copy_qpath(out_model->surfaces[i].name, (const md4_u8 *)ir->meshes[i].name);
        md4_copy_qpath(out_model->surfaces[i].shader, (const md4_u8 *)ir->meshes[i].material);
    }

    for (i = 0; i < ir->num_vertices; ++i) {
        md4_i32 j;
        out_model->vertices[i].first_weight = ir->vertices[i].first_weight;
        out_model->vertices[i].num_weights = ir->vertices[i].num_weights;
        for (j = 0; j < 3; ++j) {
            out_model->vertices[i].rest_pos[j] = ir->vertices[i].rest_pos[j];
            out_model->vertices[i].normal[j] = ir->vertices[i].normal[j];
        }
        out_model->vertices[i].uv[0] = ir->vertices[i].uv[0];
        out_model->vertices[i].uv[1] = ir->vertices[i].uv[1];
    }

    for (i = 0; i < ir->num_triangles; ++i) {
        out_model->triangles[i].indexes[0] = ir->triangles[i].indexes[0];
        out_model->triangles[i].indexes[1] = ir->triangles[i].indexes[1];
        out_model->triangles[i].indexes[2] = ir->triangles[i].indexes[2];
    }

    for (i = 0; i < ir->num_meshes; ++i) {
        const md4_ir_mesh_t *mesh;
        md4_surface_t *surface;
        md4_i32 bone_local[MD4_CFG_MAX_BONES];
        md4_i32 v;
        md4_i32 b;

        mesh = &ir->meshes[i];
        surface = &out_model->surfaces[i];
        for (b = 0; b < ir->num_bones; ++b) {
            bone_local[b] = -1;
        }

        for (v = 0; v < mesh->num_vertices; ++v) {
            const md4_ir_vertex_t *src_v;
            md4_i32 w;
            src_v = &ir->vertices[mesh->first_vertex + v];
            for (w = 0; w < src_v->num_weights; ++w) {
                md4_i32 wi;
                const md4_ir_weight_t *src_w;
                md4_weight_t *dst_w;
                md4_i32 global_bone;
                wi = src_v->first_weight + w;
                src_w = &ir->weights[wi];
                dst_w = &out_model->weights[wi];
                global_bone = src_w->bone_index;
                if (global_bone < 0 || global_bone >= ir->num_bones) {
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

    rc = md4_validate_model(out_model);
    if (rc != MD4_RC_OK) {
        return rc;
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

    if (!ir->has_rest_positions) {
        if (vertex_mode == MD4_VERT_PADDED || vertex_mode == MD4_VERT_DRAFT) {
            for (i = 0; i < out_model->num_vertices; ++i) {
                out_model->vertices[i].rest_pos[0] = 0;
                out_model->vertices[i].rest_pos[1] = 0;
                out_model->vertices[i].rest_pos[2] = 0;
            }
        }
    }

    return MD4_RC_OK;
}
