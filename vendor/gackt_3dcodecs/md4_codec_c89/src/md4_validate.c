#include "md4_internal.h"

md4_rc_t md4_validate_model(const md4_model_t *model)
{
    md4_i32 i;
    md4_i32 expected_frame_bones;

    if (model == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (model->num_frames < 0 || model->num_frames > MD4_CFG_MAX_FRAMES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (model->num_bones < 0 || model->num_bones > MD4_CFG_MAX_BONES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (model->num_lods < 0 || model->num_lods > MD4_CFG_MAX_LODS) {
        return MD4_RC_BAD_COUNTS;
    }
    if (model->num_surfaces < 0 || model->num_surfaces > MD4_CFG_MAX_SURFACES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (model->num_vertices < 0 || model->num_vertices > MD4_CFG_MAX_VERTICES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (model->num_triangles < 0 || model->num_triangles > MD4_CFG_MAX_TRIANGLES) {
        return MD4_RC_BAD_COUNTS;
    }
    if (model->num_weights < 0 || model->num_weights > MD4_CFG_MAX_WEIGHTS) {
        return MD4_RC_BAD_COUNTS;
    }
    if (model->num_bone_refs < 0 || model->num_bone_refs > MD4_CFG_MAX_BONE_REFS) {
        return MD4_RC_BAD_COUNTS;
    }

    expected_frame_bones = model->num_frames * model->num_bones;
    if (model->num_frame_bones != expected_frame_bones) {
        return MD4_RC_BAD_COUNTS;
    }

    for (i = 0; i < model->num_frames; ++i) {
        if (model->frames[i].first_bone != i * model->num_bones) {
            return MD4_RC_BAD_COUNTS;
        }
        if (model->frames[i].first_bone < 0 || model->frames[i].first_bone + model->num_bones > model->num_frame_bones) {
            return MD4_RC_BAD_COUNTS;
        }
    }

    for (i = 0; i < model->num_lods; ++i) {
        const md4_lod_t *lod;
        lod = &model->lods[i];
        if (lod->first_surface < 0 || lod->num_surfaces < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (lod->first_surface + lod->num_surfaces > model->num_surfaces) {
            return MD4_RC_BAD_COUNTS;
        }
    }

    for (i = 0; i < model->num_surfaces; ++i) {
        const md4_surface_t *surface;
        md4_i32 j;
        surface = &model->surfaces[i];
        if (surface->first_vertex < 0 || surface->num_vertices < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (surface->first_triangle < 0 || surface->num_triangles < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (surface->first_bone_ref < 0 || surface->num_bone_refs < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (surface->first_vertex + surface->num_vertices > model->num_vertices) {
            return MD4_RC_BAD_COUNTS;
        }
        if (surface->first_triangle + surface->num_triangles > model->num_triangles) {
            return MD4_RC_BAD_COUNTS;
        }
        if (surface->first_bone_ref + surface->num_bone_refs > model->num_bone_refs) {
            return MD4_RC_BAD_COUNTS;
        }
        for (j = 0; j < surface->num_triangles; ++j) {
            const md4_triangle_t *tri;
            tri = &model->triangles[surface->first_triangle + j];
            if (tri->indexes[0] < 0 || tri->indexes[1] < 0 || tri->indexes[2] < 0) {
                return MD4_RC_BAD_COUNTS;
            }
            if (tri->indexes[0] >= surface->num_vertices || tri->indexes[1] >= surface->num_vertices || tri->indexes[2] >= surface->num_vertices) {
                return MD4_RC_BAD_COUNTS;
            }
        }
        for (j = 0; j < surface->num_bone_refs; ++j) {
            md4_i32 bref;
            bref = model->bone_refs[surface->first_bone_ref + j];
            if (bref < 0 || bref >= model->num_bones) {
                return MD4_RC_BAD_COUNTS;
            }
        }
    }

    for (i = 0; i < model->num_vertices; ++i) {
        const md4_vertex_t *v;
        md4_i32 j;
        v = &model->vertices[i];
        if (v->first_weight < 0 || v->num_weights < 0) {
            return MD4_RC_BAD_COUNTS;
        }
        if (v->first_weight + v->num_weights > model->num_weights) {
            return MD4_RC_BAD_COUNTS;
        }
        for (j = 0; j < v->num_weights; ++j) {
            const md4_weight_t *w;
            w = &model->weights[v->first_weight + j];
            if (w->bone_index < 0) {
                return MD4_RC_BAD_COUNTS;
            }
        }
    }

    return MD4_RC_OK;
}
