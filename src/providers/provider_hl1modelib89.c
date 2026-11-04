#include "../modelrt_internal.h"
#include "../../vendor/hl1modelib/hlfp.h"
#include "../../vendor/hl1modelib/hlfp_goldsrc.h"
#include "../../include/modelrt_native89.h"

#include <string.h>

#define HL189_KIND_MDL 1
#define HL189_KIND_SMD 2

typedef struct hl189_state_s {
    int kind;
    modelrt_u8 *bytes;
    size_t size;
    hlfp_mdl *mdl;
    hlfp_smd_model *smd;
    hlfp_mdl_mesh_expanded *scratch;
    hlfp_mat3x4 world[HLFP_MDL_MAX_BONES];
} hl189_state;

static int hl189_probe(const char *path, const modelrt_u8 *head, size_t head_size)
{
    /* GoldSrc IDST version 10; Source Studio IDST version >= 44.
       Never claim a Source MDL just because the first four bytes match. */
    if (head != 0 && head_size >= 8u && memcmp(head, "IDST", 4u) == 0) {
        modelrt_u32 version = (modelrt_u32)head[4] | ((modelrt_u32)head[5] << 8) |
                              ((modelrt_u32)head[6] << 16) | ((modelrt_u32)head[7] << 24);
        return version == 10u ? MODELRT_PROBE_YES : MODELRT_PROBE_NO;
    }
    if (head != 0 && head_size >= 4u && head[0] == 'I' && head[1] == 'D' && head[2] == 'S' && head[3] == 'T')
        return MODELRT_PROBE_YES;
    if (modelrt_path_has_suffix(path, ".mdl")) return MODELRT_PROBE_MAYBE;
    if (modelrt_path_has_suffix(path, ".smd")) return MODELRT_PROBE_MAYBE;
    return MODELRT_PROBE_NO;
}

static int hl189_open(modelrt_runtime *rt, modelrt_handle *handle, const char *path)
{
    hl189_state *s;
    hlfp_error err;
    int rc;
    s = (hl189_state *)modelrt_arena_alloc(handle, sizeof(*s), 8u);
    if (s == 0) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s, sizeof(*s));
    rc = modelrt_read_file_arena(handle, path, &s->bytes, &s->size);
    if (rc != MODELRT_OK) return rc;
    modelrt_zero(&err, sizeof(err));
    if (s->size >= 4u && s->bytes[0] == 'I' && s->bytes[1] == 'D' && s->bytes[2] == 'S' && s->bytes[3] == 'T') {
        s->mdl = (hlfp_mdl *)modelrt_arena_alloc(handle, sizeof(hlfp_mdl), 8u);
        s->scratch = (hlfp_mdl_mesh_expanded *)modelrt_arena_alloc(handle, sizeof(hlfp_mdl_mesh_expanded), 8u);
        if (s->mdl == 0 || s->scratch == 0) return MODELRT_ERR_NO_SPACE;
        hlfp_mdl_init(s->mdl);
        rc = hlfp_mdl_parse_main(s->bytes, s->size, s->mdl, &err);
        if (rc != HLFP_OK) {
            modelrt_set_error(rt, "GoldSrc", MODELRT_ERR_CODEC, err.message);
            return MODELRT_ERR_CODEC;
        }
        s->kind = HL189_KIND_MDL;
        modelrt_copy_string(handle->info.name, sizeof(handle->info.name), s->mdl->header.name);
        handle->info.mesh_count = (modelrt_u32)s->mdl->num_meshes;
        handle->info.material_count = (modelrt_u32)s->mdl->num_textures;
        handle->info.texture_count = (modelrt_u32)s->mdl->num_textures;
        handle->info.bone_count = (modelrt_u32)s->mdl->num_bones;
        handle->info.animation_count = (modelrt_u32)s->mdl->num_seqdesc;
        handle->info.capabilities = MODELRT_CAP_GEOMETRY | MODELRT_CAP_NORMALS |
                                    MODELRT_CAP_UV0 | MODELRT_CAP_MATERIALS |
                                    MODELRT_CAP_SKELETON | MODELRT_CAP_SKINNING;
        if (s->mdl->num_textures > 0) handle->info.capabilities |= MODELRT_CAP_TEXTURES |
                                                                  MODELRT_CAP_EMBEDDED_TEX;
        if (s->mdl->num_seqdesc > 0) handle->info.capabilities |= MODELRT_CAP_ANIMATION;
        if (s->mdl->num_events > 0) handle->info.capabilities |= MODELRT_CAP_ANIM_EVENTS;
    } else {
        s->smd = (hlfp_smd_model *)modelrt_arena_alloc(handle, sizeof(hlfp_smd_model), 8u);
        if (s->smd == 0) return MODELRT_ERR_NO_SPACE;
        hlfp_smd_init(s->smd);
        rc = hlfp_smd_parse_buffer((const char *)s->bytes, s->size, s->smd, &err);
        if (rc != HLFP_OK) {
            modelrt_set_error(rt, "GoldSrc/SMD", MODELRT_ERR_CODEC, err.message);
            return MODELRT_ERR_CODEC;
        }
        s->kind = HL189_KIND_SMD;
        modelrt_copy_string(handle->info.name, sizeof(handle->info.name), path);
        handle->info.mesh_count = s->smd->num_triangles ? 1u : 0u;
        handle->info.bone_count = (modelrt_u32)s->smd->num_nodes;
        handle->info.animation_count = s->smd->num_frames > 1 ? 1u : 0u;
        handle->info.capabilities = MODELRT_CAP_GEOMETRY | MODELRT_CAP_NORMALS |
                                    MODELRT_CAP_UV0 | MODELRT_CAP_SKELETON |
                                    MODELRT_CAP_SKINNING | MODELRT_CAP_EXTERNAL_TEX;
        if (s->smd->num_frames > 1) handle->info.capabilities |= MODELRT_CAP_ANIMATION;
    }
    handle->native_state = s;
    return MODELRT_OK;
}

static void hl189_close(modelrt_handle *handle) { (void)handle; }

/* GoldSrc/Studio space is Z-up.  model_runtime89 views are Y-up.
 * Convert with a +90 degree rotation around X:
 *     runtime X = native X
 *     runtime Y = native Z
 *     runtime Z = -native Y
 * This is a proper rotation (determinant +1), so triangle winding is preserved.
 * Keep the codec state, bind pose and animation math in native Studio space;
 * only the emitted runtime view crosses coordinate bases. */
static void hl189_to_runtime_vec(modelrt_q16 x, modelrt_q16 y, modelrt_q16 z,
                                 modelrt_vec3 *out)
{
    out->x = x;
    out->y = z;
    out->z = -y;
}

static void hl189_expanded_vertex(const hlfp_mdl_expanded_vertex *v, modelrt_vertex_view *out)
{
    modelrt_zero(out, sizeof(*out));
    hl189_to_runtime_vec(v->pos.x, v->pos.y, v->pos.z, &out->position);
    hl189_to_runtime_vec(v->normal.x, v->normal.y, v->normal.z, &out->normal);
    out->uv0.x = v->u; out->uv0.y = v->v;
    out->rgba[0] = out->rgba[1] = out->rgba[2] = out->rgba[3] = 255u;
    out->bone[0] = (modelrt_i16)v->bone;
    out->weight[0] = MODELRT_Q16_ONE;
    out->weight_count = 1u;
    out->flags = MODELRT_VTX_NORMAL | MODELRT_VTX_UV0 | MODELRT_VTX_SKIN;
}

static void hl189_smd_vertex(const hlfp_smd_vertex *v, modelrt_vertex_view *out)
{
    int i;
    modelrt_zero(out, sizeof(*out));
    hl189_to_runtime_vec(v->pos.x, v->pos.y, v->pos.z, &out->position);
    hl189_to_runtime_vec(v->normal.x, v->normal.y, v->normal.z, &out->normal);
    out->uv0.x = v->u; out->uv0.y = v->v;
    out->rgba[0] = out->rgba[1] = out->rgba[2] = out->rgba[3] = 255u;
    out->flags = MODELRT_VTX_NORMAL | MODELRT_VTX_UV0;
    if (v->num_links > 0) {
        out->weight_count = (modelrt_u8)(v->num_links > 4 ? 4 : v->num_links);
        for (i = 0; i < (int)out->weight_count; ++i) {
            out->bone[i] = (modelrt_i16)v->links[i].bone;
            out->weight[i] = v->links[i].weight;
        }
        out->flags |= MODELRT_VTX_SKIN;
    } else {
        out->bone[0] = (modelrt_i16)v->bone;
        out->weight[0] = MODELRT_Q16_ONE;
        out->weight_count = 1u;
        out->flags |= MODELRT_VTX_SKIN;
    }
}

static int hl189_prepare_world(hl189_state *s, int animation_index, modelrt_q16 time)
{
    hlfp_error err;
    hlfp_mdl_pose_set pose;
    hlfp_mdl_quat_pose_set qpose;
    int rc;
    modelrt_zero(&err, sizeof(err));
    if (animation_index >= 0 && animation_index < s->mdl->num_seqdesc) {
        /*
         * GoldSrc Studio does not interpolate animated Euler angles directly.
         * It builds the two sampled rotations as quaternions, slerps them, and
         * also clears STUDIO_X/Y/Z motion-bone channels before constructing the
         * hierarchy.  hl1modelib's quaternion decoder mirrors that path; the
         * older generic time decoder did neither, which could both warp limbs
         * slightly and translate the entire character far from the viewer.
         */
        rc = hlfp_mdl_decode_sequence_time_quat_ctrl(s->mdl, animation_index,
                                                      (hlfp_fixed)time, 0,
                                                      &qpose, &err);
        if (rc != HLFP_OK) return MODELRT_ERR_CODEC;
        rc = hlfp_mdl_quat_pose_to_world_matrices(s->mdl, &qpose, s->world,
                                                   HLFP_MDL_MAX_BONES, &err);
        return rc == HLFP_OK ? MODELRT_OK : MODELRT_ERR_CODEC;
    }
    rc = hlfp_goldsrc_make_bind_pose(s->mdl, &pose, &err);
    if (rc != HLFP_OK) return MODELRT_ERR_CODEC;
    rc = hlfp_mdl_pose_to_world_matrices(s->mdl, &pose, s->world, HLFP_MDL_MAX_BONES, &err);
    return rc == HLFP_OK ? MODELRT_OK : MODELRT_ERR_CODEC;
}

static modelrt_u32 hl189_material_flags(int native_flags)
{
    modelrt_u32 flags;
    flags = 0u;
    if ((native_flags & HLFP_STUDIO_NF_FLATSHADE) != 0) flags |= MODELRT_MAT_FLATSHADE;
    if ((native_flags & HLFP_STUDIO_NF_CHROME) != 0) flags |= MODELRT_MAT_CHROME;
    if ((native_flags & HLFP_STUDIO_NF_FULLBRIGHT) != 0) flags |= MODELRT_MAT_FULLBRIGHT;
    if ((native_flags & HLFP_STUDIO_NF_NOMIPS) != 0) flags |= MODELRT_MAT_NOMIPS;
    if ((native_flags & HLFP_STUDIO_NF_ALPHA) != 0) flags |= MODELRT_MAT_ALPHA_BLEND;
    if ((native_flags & HLFP_STUDIO_NF_ADDITIVE) != 0) flags |= MODELRT_MAT_ADDITIVE;
    if ((native_flags & HLFP_STUDIO_NF_MASKED) != 0) flags |= MODELRT_MAT_ALPHA_MASK;
    return flags;
}

static int hl189_visit_mdl(modelrt_handle *handle, hl189_state *s, int animation_index,
                           modelrt_q16 time, const modelrt_visit_sink *sink, void *user)
{
    int i, j, b, cb, model_index, rc;
    hlfp_error err;
    (void)handle;
    rc = hl189_prepare_world(s, animation_index, time);
    if (rc != MODELRT_OK) return rc;
    if (sink->on_material) {
        for (i = 0; i < s->mdl->num_textures; ++i) {
            modelrt_material_view mv;
            modelrt_zero(&mv, sizeof(mv));
            mv.index = (modelrt_u32)i;
            modelrt_copy_string(mv.name, sizeof(mv.name), s->mdl->textures[i].name);
            mv.rgba[0] = mv.rgba[1] = mv.rgba[2] = mv.rgba[3] = 255u;
            mv.texture_index = i;
            mv.flags = hl189_material_flags(s->mdl->textures[i].flags);
            if (sink->on_material(user, &mv)) return MODELRT_ERR_CALLBACK;
        }
    }
    for (b = 0; b < s->mdl->num_bodyparts; ++b) {
        modelrt_zero(&err, sizeof(err));
        if (hlfp_goldsrc_model_for_bodypart(s->mdl, 0, b, &model_index, &err) != HLFP_OK) continue;
        if (model_index < 0 || model_index >= s->mdl->num_models) continue;
        for (i = 0; i < s->mdl->models[model_index].nummesh; ++i) {
            int mesh_index;
            modelrt_mesh_info mi;
            hlfp_mdl_mesh_expanded *em;
            mesh_index = s->mdl->models[model_index].first_mesh + i;
            if (mesh_index < 0 || mesh_index >= s->mdl->num_meshes) continue;
            modelrt_zero(&err, sizeof(err));
            em = s->scratch;
            if (hlfp_goldsrc_bake_mesh_with_matrices(s->mdl, model_index, mesh_index, 0,
                                                     s->world, s->mdl->num_bones, em, &err) != HLFP_OK)
                return MODELRT_ERR_CODEC;
            modelrt_zero(&mi, sizeof(mi));
            mi.index = (modelrt_u32)mesh_index;
            modelrt_copy_string(mi.name, sizeof(mi.name), s->mdl->models[model_index].name);
            mi.vertex_count = (modelrt_u32)em->num_vertices;
            mi.triangle_count = (modelrt_u32)em->num_triangles;
            mi.material_count = 1u;
            if (sink->on_mesh_begin && sink->on_mesh_begin(user, &mi)) return MODELRT_ERR_CALLBACK;
            for (j = 0; j < em->num_triangles; ++j) {
                modelrt_triangle_view tv;
                int ids[3];
                int k;
                modelrt_zero(&tv, sizeof(tv));
                tv.mesh_index = (modelrt_u32)mesh_index;
                tv.material_index = em->texture_index;
                ids[0] = em->triangles[j].v0; ids[1] = em->triangles[j].v1; ids[2] = em->triangles[j].v2;
                for (k = 0; k < 3; ++k) {
                    if (ids[k] < 0 || ids[k] >= em->num_vertices) return MODELRT_ERR_FORMAT;
                    hl189_expanded_vertex(&em->vertices[ids[k]], &tv.v[k]);
                }
                if (sink->on_triangle && sink->on_triangle(user, &tv)) return MODELRT_ERR_CALLBACK;
            }
            if (sink->on_mesh_end && sink->on_mesh_end(user, &mi)) return MODELRT_ERR_CALLBACK;
        }
    }
    if (sink->on_bone) {
        for (i = 0; i < s->mdl->num_bones; ++i) {
            modelrt_bone_view bv;
            modelrt_zero(&bv, sizeof(bv));
            bv.index = (modelrt_u32)i;
            bv.parent = s->mdl->bones[i].parent;
            modelrt_copy_string(bv.name, sizeof(bv.name), s->mdl->bones[i].name);
            hl189_to_runtime_vec(s->world[i].m[0][3], s->world[i].m[1][3],
                                 s->world[i].m[2][3], &bv.position);
            if (sink->on_bone(user, &bv)) return MODELRT_ERR_CALLBACK;
        }
    }
    (void)cb;
    return MODELRT_OK;
}

static int hl189_visit_smd(modelrt_handle *handle, hl189_state *s,
                           const modelrt_visit_sink *sink, void *user)
{
    int i, j;
    if (s->smd->num_triangles > 0) {
        modelrt_mesh_info mi;
        modelrt_zero(&mi, sizeof(mi));
        mi.index = 0u;
        modelrt_copy_string(mi.name, sizeof(mi.name), handle->info.name);
        mi.vertex_count = (modelrt_u32)(s->smd->num_triangles * 3);
        mi.triangle_count = (modelrt_u32)s->smd->num_triangles;
        if (sink->on_mesh_begin && sink->on_mesh_begin(user, &mi)) return MODELRT_ERR_CALLBACK;
        for (i = 0; i < s->smd->num_triangles; ++i) {
            modelrt_triangle_view tv;
            modelrt_zero(&tv, sizeof(tv));
            tv.mesh_index = 0u;
            tv.material_index = -1;
            for (j = 0; j < 3; ++j) hl189_smd_vertex(&s->smd->triangles[i].v[j], &tv.v[j]);
            if (sink->on_triangle && sink->on_triangle(user, &tv)) return MODELRT_ERR_CALLBACK;
        }
        if (sink->on_mesh_end && sink->on_mesh_end(user, &mi)) return MODELRT_ERR_CALLBACK;
    }
    if (sink->on_bone) {
        int frame_index;
        frame_index = 0;
        for (i = 0; i < s->smd->num_nodes; ++i) {
            modelrt_bone_view bv;
            int p;
            modelrt_zero(&bv, sizeof(bv));
            bv.index = (modelrt_u32)i;
            bv.parent = s->smd->nodes[i].parent;
            modelrt_copy_string(bv.name, sizeof(bv.name), s->smd->nodes[i].name);
            if (s->smd->num_frames > frame_index) {
                hlfp_smd_frame *fr;
                fr = &s->smd->frames[frame_index];
                for (p = 0; p < fr->pose_count; ++p) {
                    hlfp_pose *pose;
                    pose = &s->smd->poses[fr->pose_start + p];
                    if (pose->bone == s->smd->nodes[i].id) {
                        hl189_to_runtime_vec(pose->pos.x, pose->pos.y, pose->pos.z,
                                             &bv.position);
                        break;
                    }
                }
            }
            if (sink->on_bone(user, &bv)) return MODELRT_ERR_CALLBACK;
        }
    }
    return MODELRT_OK;
}

static int hl189_visit(modelrt_handle *handle, int animation_index, modelrt_q16 time,
                       const modelrt_visit_sink *sink, void *user)
{
    hl189_state *s;
    int rc;
    s = (hl189_state *)handle->native_state;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    if (sink->on_model_begin && sink->on_model_begin(user, &handle->info)) return MODELRT_ERR_CALLBACK;
    rc = (s->kind == HL189_KIND_MDL) ? hl189_visit_mdl(handle, s, animation_index, time, sink, user)
                                     : hl189_visit_smd(handle, s, sink, user);
    if (rc != MODELRT_OK) return rc;
    if (sink->on_model_end && sink->on_model_end(user, &handle->info)) return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}

static int hl189_animation_count(const modelrt_handle *handle)
{
    const hl189_state *s;
    s = (const hl189_state *)handle->native_state;
    if (s == 0) return 0;
    if (s->kind == HL189_KIND_MDL) return s->mdl->num_seqdesc;
    return s->smd->num_frames > 1 ? 1 : 0;
}

static int hl189_animation_info(modelrt_handle *handle, int animation_index,
                                modelrt_animation_info *out_info)
{
    hl189_state *s;
    if (handle == 0 || out_info == 0) return MODELRT_ERR_BAD_ARG;
    s = (hl189_state *)handle->native_state;
    if (s == 0 || s->kind != HL189_KIND_MDL || animation_index < 0 || animation_index >= s->mdl->num_seqdesc)
        return MODELRT_ERR_UNSUPPORTED;
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)animation_index;
    modelrt_copy_string(out_info->name, sizeof(out_info->name), s->mdl->seqdesc[animation_index].label);
    out_info->start_time = 0;
    if (s->mdl->seqdesc[animation_index].numframes > 0) {
        modelrt_i32 fps_int;
        out_info->frame_count = (modelrt_u32)s->mdl->seqdesc[animation_index].numframes;
        out_info->first_tick = 0;
        out_info->last_tick = s->mdl->seqdesc[animation_index].numframes - 1;
        out_info->flags |= MODELRT_ANIM_DENSE_FRAMES;
        fps_int = (modelrt_i32)((s->mdl->seqdesc[animation_index].fps + 32768L) / 65536L);
        if (fps_int > 0) {
            out_info->ticks_per_second = (modelrt_u32)fps_int;
            out_info->flags |= MODELRT_ANIM_TICK_RATE_KNOWN;
        }
    }
    if (s->mdl->seqdesc[animation_index].fps > 0 &&
        s->mdl->seqdesc[animation_index].numframes > 1)
        out_info->end_time = modelrt_q16_div(
            modelrt_q16_from_int(s->mdl->seqdesc[animation_index].numframes - 1),
            s->mdl->seqdesc[animation_index].fps);
    return MODELRT_OK;
}

static int hl189_texture_count(const modelrt_handle *handle)
{
    const hl189_state *s;
    s = (const hl189_state *)handle->native_state;
    return (s && s->kind == HL189_KIND_MDL) ? s->mdl->num_textures : 0;
}

static int hl189_texture_info(modelrt_handle *handle, int texture_index,
                              modelrt_texture_info *out_info)
{
    hl189_state *s;
    hlfp_mdl_texture *t;
    if (handle == 0 || out_info == 0) return MODELRT_ERR_BAD_ARG;
    s = (hl189_state *)handle->native_state;
    if (s == 0 || s->kind != HL189_KIND_MDL || texture_index < 0 || texture_index >= s->mdl->num_textures)
        return MODELRT_ERR_BAD_ARG;
    t = &s->mdl->textures[texture_index];
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)texture_index;
    out_info->kind = MODELRT_TEXTURE_EMBEDDED;
    modelrt_copy_string(out_info->name, sizeof(out_info->name), t->name);
    out_info->width = (modelrt_u32)t->width;
    out_info->height = (modelrt_u32)t->height;
    out_info->rgba_bytes = out_info->width * out_info->height * 4u;
    out_info->flags = hl189_material_flags(t->flags);
    return MODELRT_OK;
}

static int hl189_texture_decode(modelrt_handle *handle, int texture_index,
                                modelrt_u8 *dst, size_t dst_capacity,
                                modelrt_u32 *out_width, modelrt_u32 *out_height,
                                size_t *out_bytes)
{
    hl189_state *s;
    hlfp_error err;
    size_t used;
    int w, h, rc;
    if (handle == 0 || dst == 0) return MODELRT_ERR_BAD_ARG;
    s = (hl189_state *)handle->native_state;
    if (s == 0 || s->kind != HL189_KIND_MDL) return MODELRT_ERR_UNSUPPORTED;
    modelrt_zero(&err, sizeof(err));
    used = 0u; w = h = 0;
    rc = hlfp_goldsrc_texture_to_rgba32(s->mdl, texture_index, 0, dst, dst_capacity,
                                        &used, &w, &h, &err);
    if (rc != HLFP_OK) return MODELRT_ERR_CODEC;
    if (out_width) *out_width = (modelrt_u32)w;
    if (out_height) *out_height = (modelrt_u32)h;
    if (out_bytes) *out_bytes = used;
    return MODELRT_OK;
}

const modelrt_provider modelrt_provider_hl1modelib89 = {
    MODELRT_CODEC_GOLDSRC, "GoldSrc/HL1 (hl1modelib v7)", hl189_probe, hl189_open, hl189_close, hl189_visit,
    hl189_animation_count, hl189_animation_info,
    hl189_texture_count, hl189_texture_info, hl189_texture_decode, 0, 0, 0,
    0
};

const hlfp_mdl *modelrt_native_goldsrc_mdl(const modelrt_handle *h)
{
    const hl189_state *s;
    if(!h || !h->is_open || h->info.codec_id!=MODELRT_CODEC_GOLDSRC)return 0;
    s=(const hl189_state *)h->native_state;
    return s?s->mdl:0;
}
const hlfp_smd_model *modelrt_native_goldsrc_smd(const modelrt_handle *h)
{
    const hl189_state *s;
    if(!h || !h->is_open || h->info.codec_id!=MODELRT_CODEC_GOLDSRC)return 0;
    s=(const hl189_state *)h->native_state;
    return s?s->smd:0;
}
