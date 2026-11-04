#include "../modelrt_internal.h"
#include "../../vendor/mmd_codec_c89_fixed_v3/include/mmd_codec.h"

#include <string.h>

#define MMD89_KIND_MODEL 1
#define MMD89_KIND_MOTION 2
#define MMD89_KIND_POSE 3

typedef struct mmd89_state_s {
    int kind;
    MMD_Model *model;
    MMD_Motion *motion;
    MMD_Pose *pose;
    modelrt_u8 *bytes;
    size_t size;
} mmd89_state;

static int mmd89_probe(const char *path, const modelrt_u8 *head, size_t head_size)
{
    int fmt;
    if (head != 0 && head_size != 0u) {
        fmt = mmd_probe((const unsigned char *)head, (mmd_u32)head_size);
        if (fmt == MMD_FMT_PMD || fmt == MMD_FMT_PMX || fmt == MMD_FMT_VMD || fmt == MMD_FMT_VPD)
            return MODELRT_PROBE_YES;
    }
    if (modelrt_path_has_suffix(path, ".pmd") || modelrt_path_has_suffix(path, ".pmx") ||
        modelrt_path_has_suffix(path, ".vmd") || modelrt_path_has_suffix(path, ".vpd"))
        return MODELRT_PROBE_MAYBE;
    return MODELRT_PROBE_NO;
}

static int mmd89_open(modelrt_runtime *rt, modelrt_handle *handle, const char *path)
{
    mmd89_state *s;
    int fmt;
    int rc;
    s = (mmd89_state *)modelrt_arena_alloc(handle, sizeof(*s), 8u);
    if (s == 0) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s, sizeof(*s));
    rc = modelrt_read_file_arena(handle, path, &s->bytes, &s->size);
    if (rc != MODELRT_OK) return rc;
    fmt = mmd_probe((const unsigned char *)s->bytes, (mmd_u32)s->size);
    if (fmt == MMD_FMT_PMD || fmt == MMD_FMT_PMX) {
        s->model = (MMD_Model *)modelrt_arena_alloc(handle, sizeof(MMD_Model), 8u);
        if (s->model == 0) return MODELRT_ERR_NO_SPACE;
        mmd_model_init(s->model);
        rc = mmd_load_model((const unsigned char *)s->bytes, (mmd_u32)s->size, s->model);
        if (rc != MMD_OK) {
            modelrt_set_error(rt, "MMD", MODELRT_ERR_CODEC, "PMD/PMX parse failed");
            return MODELRT_ERR_CODEC;
        }
        s->kind = MMD89_KIND_MODEL;
        modelrt_copy_string(handle->info.name, sizeof(handle->info.name), s->model->name);
        handle->info.mesh_count = s->model->vertex_count > 0 ? 1u : 0u;
        handle->info.material_count = (modelrt_u32)s->model->material_count;
        handle->info.texture_count = (modelrt_u32)s->model->texture_count;
        handle->info.bone_count = (modelrt_u32)s->model->bone_count;
        handle->info.morph_count = (modelrt_u32)s->model->morph_count;
        handle->info.capabilities = MODELRT_CAP_GEOMETRY | MODELRT_CAP_NORMALS |
                                    MODELRT_CAP_UV0 | MODELRT_CAP_MATERIALS;
        if (s->model->texture_count > 0) handle->info.capabilities |= MODELRT_CAP_TEXTURES |
                                                                       MODELRT_CAP_EXTERNAL_TEX;
        if (s->model->bone_count > 0) handle->info.capabilities |= MODELRT_CAP_SKELETON |
                                                                    MODELRT_CAP_SKINNING;
        if (s->model->morph_count > 0) handle->info.capabilities |= MODELRT_CAP_MORPHS;
    } else if (fmt == MMD_FMT_VMD) {
        s->motion = (MMD_Motion *)modelrt_arena_alloc(handle, sizeof(MMD_Motion), 8u);
        if (s->motion == 0) return MODELRT_ERR_NO_SPACE;
        mmd_motion_init(s->motion);
        rc = mmd_load_vmd((const unsigned char *)s->bytes, (mmd_u32)s->size, s->motion);
        if (rc != MMD_OK) return MODELRT_ERR_CODEC;
        s->kind = MMD89_KIND_MOTION;
        modelrt_copy_string(handle->info.name, sizeof(handle->info.name), s->motion->model_name);
        handle->info.animation_count = 1u;
        handle->info.capabilities = MODELRT_CAP_ANIMATION;
        if (s->motion->camera_key_count) { handle->info.capabilities |= MODELRT_CAP_CAMERAS; handle->info.camera_count = 1u; }
        if (s->motion->light_key_count) { handle->info.capabilities |= MODELRT_CAP_LIGHTS; handle->info.light_count = 1u; }
        if (s->motion->morph_key_count) handle->info.capabilities |= MODELRT_CAP_MORPHS;
    } else if (fmt == MMD_FMT_VPD || modelrt_path_has_suffix(path, ".vpd")) {
        s->pose = (MMD_Pose *)modelrt_arena_alloc(handle, sizeof(MMD_Pose), 8u);
        if (s->pose == 0) return MODELRT_ERR_NO_SPACE;
        mmd_pose_init(s->pose);
        rc = mmd_load_vpd((const unsigned char *)s->bytes, (mmd_u32)s->size, s->pose);
        if (rc != MMD_OK) return MODELRT_ERR_CODEC;
        s->kind = MMD89_KIND_POSE;
        modelrt_copy_string(handle->info.name, sizeof(handle->info.name), s->pose->model_name);
        handle->info.bone_count = (modelrt_u32)s->pose->bone_count;
        handle->info.capabilities = MODELRT_CAP_SKELETON;
    } else {
        return MODELRT_ERR_FORMAT;
    }
    handle->native_state = s;
    return MODELRT_OK;
}

static void mmd89_close(modelrt_handle *handle) { (void)handle; }

static modelrt_u8 mmd89_color_u8(mmd_fp v)
{
    modelrt_i32 x;
    if (v <= 0) return 0u;
    if (v >= MMD_FP_ONE) return 255u;
    x = (modelrt_i32)(((modelrt_u32)v * 255u) >> 16);
    if (x < 0) x = 0;
    if (x > 255) x = 255;
    return (modelrt_u8)x;
}

static void mmd89_vertex(const MMD_Vertex *v, modelrt_vertex_view *out)
{
    int i;
    modelrt_zero(out, sizeof(*out));
    out->position.x = v->position.x;
    out->position.y = v->position.y;
    out->position.z = v->position.z;
    out->normal.x = v->normal.x;
    out->normal.y = v->normal.y;
    out->normal.z = v->normal.z;
    out->uv0.x = v->uv.x;
    out->uv0.y = v->uv.y;
    out->rgba[0] = out->rgba[1] = out->rgba[2] = out->rgba[3] = 255u;
    out->flags = MODELRT_VTX_NORMAL | MODELRT_VTX_UV0;
    out->weight_count = 4u;
    for (i = 0; i < 4; ++i) {
        out->bone[i] = (modelrt_i16)v->bone[i];
        out->weight[i] = v->weight[i];
    }
    while (out->weight_count > 0u && out->weight[out->weight_count - 1u] == 0)
        --out->weight_count;
    if (out->weight_count) out->flags |= MODELRT_VTX_SKIN;
}

static int mmd89_visit(modelrt_handle *handle, int animation_index, modelrt_q16 time,
                       const modelrt_visit_sink *sink, void *user)
{
    mmd89_state *s;
    int cb;
    int i;
    (void)animation_index;
    (void)time;
    s = (mmd89_state *)handle->native_state;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    if (sink->on_model_begin) { cb = sink->on_model_begin(user, &handle->info); if (cb) return MODELRT_ERR_CALLBACK; }
    if (s->kind == MMD89_KIND_MODEL) {
        MMD_Model *m;
        mmd_i32 cumulative;
        m = s->model;
        if (sink->on_material) {
            for (i = 0; i < m->material_count; ++i) {
                modelrt_material_view mv;
                MMD_Material *mat;
                mat = &m->materials[i];
                modelrt_zero(&mv, sizeof(mv));
                mv.index = (modelrt_u32)i;
                modelrt_copy_string(mv.name, sizeof(mv.name), mat->name);
                mv.rgba[0] = mmd89_color_u8(mat->diffuse.x);
                mv.rgba[1] = mmd89_color_u8(mat->diffuse.y);
                mv.rgba[2] = mmd89_color_u8(mat->diffuse.z);
                mv.rgba[3] = mmd89_color_u8(mat->diffuse.w);
                mv.texture_index = mat->texture_index;
                if (sink->on_material(user, &mv)) return MODELRT_ERR_CALLBACK;
            }
        }
        if (m->vertex_count > 0) {
            modelrt_mesh_info mi;
            modelrt_zero(&mi, sizeof(mi));
            mi.index = 0u;
            modelrt_copy_string(mi.name, sizeof(mi.name), m->name);
            mi.vertex_count = (modelrt_u32)m->vertex_count;
            mi.triangle_count = (modelrt_u32)(m->index_count / 3);
            mi.material_count = (modelrt_u32)m->material_count;
            if (sink->on_mesh_begin && sink->on_mesh_begin(user, &mi)) return MODELRT_ERR_CALLBACK;
            cumulative = 0;
            for (i = 0; i + 2 < m->index_count; i += 3) {
                modelrt_triangle_view tv;
                mmd_i32 material_index;
                int j;
                material_index = 0;
                while (material_index < m->material_count && i >= cumulative + m->materials[material_index].index_count) {
                    cumulative += m->materials[material_index].index_count;
                    ++material_index;
                }
                modelrt_zero(&tv, sizeof(tv));
                tv.mesh_index = 0u;
                tv.material_index = material_index < m->material_count ? material_index : -1;
                for (j = 0; j < 3; ++j) {
                    mmd_u32 ix;
                    ix = m->indices[i + j];
                    if (ix >= (mmd_u32)m->vertex_count) return MODELRT_ERR_FORMAT;
                    mmd89_vertex(&m->vertices[ix], &tv.v[j]);
                }
                if (sink->on_triangle && sink->on_triangle(user, &tv)) return MODELRT_ERR_CALLBACK;
            }
            if (sink->on_mesh_end && sink->on_mesh_end(user, &mi)) return MODELRT_ERR_CALLBACK;
        }
        if (sink->on_bone) {
            for (i = 0; i < m->bone_count; ++i) {
                modelrt_bone_view bv;
                modelrt_zero(&bv, sizeof(bv));
                bv.index = (modelrt_u32)i;
                bv.parent = m->bones[i].parent;
                modelrt_copy_string(bv.name, sizeof(bv.name), m->bones[i].name);
                bv.position.x = m->bones[i].position.x;
                bv.position.y = m->bones[i].position.y;
                bv.position.z = m->bones[i].position.z;
                if (sink->on_bone(user, &bv)) return MODELRT_ERR_CALLBACK;
            }
        }
    } else if (s->kind == MMD89_KIND_POSE && sink->on_bone) {
        for (i = 0; i < s->pose->bone_count; ++i) {
            modelrt_bone_view bv;
            modelrt_zero(&bv, sizeof(bv));
            bv.index = (modelrt_u32)i;
            bv.parent = -1;
            modelrt_copy_string(bv.name, sizeof(bv.name), s->pose->bones[i].name);
            bv.position.x = s->pose->bones[i].translation.x;
            bv.position.y = s->pose->bones[i].translation.y;
            bv.position.z = s->pose->bones[i].translation.z;
            if (sink->on_bone(user, &bv)) return MODELRT_ERR_CALLBACK;
        }
    }
    if (sink->on_model_end) { cb = sink->on_model_end(user, &handle->info); if (cb) return MODELRT_ERR_CALLBACK; }
    return MODELRT_OK;
}

static int mmd89_animation_count(const modelrt_handle *handle)
{
    const mmd89_state *s;
    s = (const mmd89_state *)handle->native_state;
    return (s && s->kind == MMD89_KIND_MOTION) ? 1 : 0;
}

static int mmd89_animation_info(modelrt_handle *handle, int animation_index,
                                modelrt_animation_info *out_info)
{
    mmd89_state *s;
    mmd_u32 max_frame;
    int i;
    if (handle == 0 || out_info == 0 || animation_index != 0) return MODELRT_ERR_BAD_ARG;
    s = (mmd89_state *)handle->native_state;
    if (s == 0 || s->kind != MMD89_KIND_MOTION) return MODELRT_ERR_UNSUPPORTED;
    max_frame = 0u;
    for (i = 0; i < s->motion->bone_key_count; ++i) if (s->motion->bone_keys[i].frame > max_frame) max_frame = s->motion->bone_keys[i].frame;
    for (i = 0; i < s->motion->morph_key_count; ++i) if (s->motion->morph_keys[i].frame > max_frame) max_frame = s->motion->morph_keys[i].frame;
    for (i = 0; i < s->motion->camera_key_count; ++i) if (s->motion->camera_keys[i].frame > max_frame) max_frame = s->motion->camera_keys[i].frame;
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = 0u;
    modelrt_copy_string(out_info->name, sizeof(out_info->name), "VMD motion");
    out_info->start_time = 0;
    out_info->end_time = (modelrt_q16)((max_frame * (modelrt_u32)MODELRT_Q16_ONE) / 30u);
    return MODELRT_OK;
}

static int mmd89_texture_count(const modelrt_handle *handle)
{
    const mmd89_state *s;
    s = (const mmd89_state *)handle->native_state;
    return (s && s->kind == MMD89_KIND_MODEL) ? s->model->texture_count : 0;
}

static int mmd89_texture_info(modelrt_handle *handle, int texture_index,
                              modelrt_texture_info *out_info)
{
    mmd89_state *s;
    if (handle == 0 || out_info == 0) return MODELRT_ERR_BAD_ARG;
    s = (mmd89_state *)handle->native_state;
    if (s == 0 || s->kind != MMD89_KIND_MODEL || texture_index < 0 || texture_index >= s->model->texture_count)
        return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)texture_index;
    out_info->kind = MODELRT_TEXTURE_EXTERNAL;
    modelrt_copy_string(out_info->name, sizeof(out_info->name), s->model->textures[texture_index].path);
    modelrt_copy_string(out_info->path, sizeof(out_info->path), s->model->textures[texture_index].path);
    return MODELRT_OK;
}

static int mmd89_texture_decode(modelrt_handle *handle, int texture_index,
                                modelrt_u8 *dst, size_t dst_capacity,
                                modelrt_u32 *out_width, modelrt_u32 *out_height,
                                size_t *out_bytes)
{
    (void)handle; (void)texture_index; (void)dst; (void)dst_capacity;
    (void)out_width; (void)out_height; (void)out_bytes;
    return MODELRT_ERR_UNSUPPORTED;
}

const modelrt_provider modelrt_provider_mmd89 = {
    MODELRT_CODEC_MMD, "MMD", mmd89_probe, mmd89_open, mmd89_close, mmd89_visit,
    mmd89_animation_count, mmd89_animation_info,
    mmd89_texture_count, mmd89_texture_info, mmd89_texture_decode, 0, 0, 0,
    0
};
