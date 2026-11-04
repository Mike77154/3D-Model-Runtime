#include "../modelrt_internal.h"
#include "../../vendor/genesis3d_c89_codec/g3d_codec.h"

#include <string.h>

#define G3D89_KIND_BODY 1
#define G3D89_KIND_ACTOR 2
#define G3D89_KIND_MOTION 3

typedef struct g3d89_state_s {
    int kind;
    modelrt_u8 *bytes;
    size_t size;
    g3d_arena arena;
    g3d_body *body;
    g3d_actor *actor;
    g3d_motion *motion;
    g3d_skinned_mesh skin;
} g3d89_state;

static int g3d89_probe(const char *path, const modelrt_u8 *head, size_t head_size)
{
    (void)head; (void)head_size;
    if (modelrt_path_has_suffix(path, ".bdy") || modelrt_path_has_suffix(path, ".act") ||
        modelrt_path_has_suffix(path, ".mot")) return MODELRT_PROBE_MAYBE;
    return MODELRT_PROBE_NO;
}

static int g3d89_open(modelrt_runtime *rt, modelrt_handle *handle, const char *path)
{
    g3d89_state *s;
    modelrt_u8 *arena_mem;
    size_t arena_size;
    int rc;
    g3d_body *body;
    s = (g3d89_state *)modelrt_arena_alloc(handle, sizeof(*s), 8u);
    if (s == 0) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s, sizeof(*s));
    rc = modelrt_read_file_arena(handle, path, &s->bytes, &s->size);
    if (rc != MODELRT_OK) return rc;
    if (handle->workspace_used >= handle->workspace_size) return MODELRT_ERR_NO_SPACE;
    arena_mem = handle->workspace + handle->workspace_used;
    arena_size = handle->workspace_size - handle->workspace_used;
    if (arena_size > 0xffffffffu) arena_size = 0xffffffffu;
    g3d_arena_init(&s->arena, arena_mem, (g3d_u32)arena_size);
    handle->workspace_used = handle->workspace_size;

    if (modelrt_path_has_suffix(path, ".act")) {
        s->actor = (g3d_actor *)g3d_arena_alloc(&s->arena, (g3d_u32)sizeof(g3d_actor), 4u);
        if (s->actor == 0) return MODELRT_ERR_NO_SPACE;
        rc = g3d_actor_parse_file(s->actor, s->bytes, (g3d_u32)s->size, &s->arena);
        if (rc != G3D_OK) { modelrt_set_error(rt, "Genesis3D", MODELRT_ERR_CODEC, "ACT parse failed"); return MODELRT_ERR_CODEC; }
        s->kind = G3D89_KIND_ACTOR;
        body = s->actor->body;
    } else if (modelrt_path_has_suffix(path, ".mot")) {
        s->motion = (g3d_motion *)g3d_arena_alloc(&s->arena, (g3d_u32)sizeof(g3d_motion), 4u);
        if (s->motion == 0) return MODELRT_ERR_NO_SPACE;
        rc = g3d_motion_parse_binary(s->motion, s->bytes, (g3d_u32)s->size, &s->arena);
        if (rc != G3D_OK) { modelrt_set_error(rt, "Genesis3D", MODELRT_ERR_CODEC, "MOT parse failed"); return MODELRT_ERR_CODEC; }
        s->kind = G3D89_KIND_MOTION;
        body = 0;
    } else {
        s->body = (g3d_body *)g3d_arena_alloc(&s->arena, (g3d_u32)sizeof(g3d_body), 4u);
        if (s->body == 0) return MODELRT_ERR_NO_SPACE;
        rc = g3d_body_parse_file(s->body, s->bytes, (g3d_u32)s->size, &s->arena);
        if (rc != G3D_OK) { modelrt_set_error(rt, "Genesis3D", MODELRT_ERR_CODEC, "BDY parse failed"); return MODELRT_ERR_CODEC; }
        s->kind = G3D89_KIND_BODY;
        body = s->body;
    }

    handle->native_state = s;
    modelrt_copy_string(handle->info.name, sizeof(handle->info.name), path);
    if (body != 0) {
        if (g3d_body_alloc_skinned_mesh(&s->arena, body, &s->skin) != G3D_OK) return MODELRT_ERR_NO_SPACE;
        handle->info.mesh_count = body->lod_count ? body->lod_count : 1u;
        handle->info.material_count = (modelrt_u32)body->material_count;
        handle->info.texture_count = (modelrt_u32)body->material_count;
        handle->info.bone_count = (modelrt_u32)body->bone_count;
        handle->info.capabilities = MODELRT_CAP_GEOMETRY | MODELRT_CAP_NORMALS | MODELRT_CAP_UV0 |
                                    MODELRT_CAP_MATERIALS | MODELRT_CAP_SKELETON | MODELRT_CAP_SKINNING |
                                    MODELRT_CAP_LOD | MODELRT_CAP_ANIM_ATTACH_TARGET;
        if (g3d_body_count_textured_materials(body) != 0u)
            handle->info.capabilities |= MODELRT_CAP_TEXTURES | MODELRT_CAP_EMBEDDED_TEX;
    }
    if (s->kind == G3D89_KIND_ACTOR) {
        handle->info.animation_count = (modelrt_u32)s->actor->motion_count;
        if (s->actor->motion_count > 0)
            handle->info.capabilities |= MODELRT_CAP_ANIMATION | MODELRT_CAP_ANIM_EVENTS | MODELRT_CAP_ANIM_ATTACH_SOURCE;
    } else if (s->kind == G3D89_KIND_MOTION) {
        handle->info.animation_count = 1u;
        handle->info.capabilities = MODELRT_CAP_ANIMATION | MODELRT_CAP_ANIM_EVENTS | MODELRT_CAP_ANIM_ATTACH_SOURCE;
        if (s->motion->name) modelrt_copy_string(handle->info.name, sizeof(handle->info.name), s->motion->name);
    }
    return MODELRT_OK;
}

static void g3d89_close(modelrt_handle *handle) { (void)handle; }

static modelrt_u8 g3d89_color(g3d_u32 bits)
{
    g3d_fix16 q;
    modelrt_i32 v;
    q = g3d_f32bits_to_fix16(bits);
    if (q <= 0) return 0u;
    if (q >= 65536) return 255u;
    v = (modelrt_i32)(((modelrt_u32)q * 255u) >> 16);
    return (modelrt_u8)v;
}

static int g3d89_visit_with_motion(modelrt_handle *handle, int animation_index,
                                   const g3d_motion *external_motion, modelrt_q16 time,
                                   const modelrt_visit_sink *sink, void *user)
{
    g3d89_state *s;
    g3d_body *body;
    int rc;
    modelrt_u32 i, j;
    s = (g3d89_state *)handle->native_state;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    if (sink->on_model_begin && sink->on_model_begin(user, &handle->info)) return MODELRT_ERR_CALLBACK;
    body = s->kind == G3D89_KIND_ACTOR ? s->actor->body : s->body;
    if (body != 0) {
        if (external_motion != 0)
            rc = g3d_body_sample_skinned_fix16(body, external_motion, (g3d_fix16)time, &s->skin);
        else if (s->kind == G3D89_KIND_ACTOR)
            rc = g3d_actor_sample_skinned_fix16(s->actor, animation_index, (g3d_fix16)time, &s->skin);
        else
            rc = g3d_body_sample_skinned_fix16(body, 0, 0, &s->skin);
        if (rc != G3D_OK) return MODELRT_ERR_CODEC;
        if (sink->on_material) {
            for (i = 0u; i < (modelrt_u32)body->material_count; ++i) {
                modelrt_material_view mv;
                const char *name;
                modelrt_zero(&mv, sizeof(mv));
                mv.index = i;
                name = g3d_body_get_material_name(body, (g3d_s32)i);
                if (name) modelrt_copy_string(mv.name, sizeof(mv.name), name);
                mv.rgba[0] = g3d89_color(body->materials[i].red_bits);
                mv.rgba[1] = g3d89_color(body->materials[i].green_bits);
                mv.rgba[2] = g3d89_color(body->materials[i].blue_bits);
                mv.rgba[3] = 255u;
                mv.texture_index = body->materials[i].has_bitmap ? (modelrt_i32)i : -1;
                if (sink->on_material(user, &mv)) return MODELRT_ERR_CALLBACK;
            }
        }
        for (i = 0u; i < body->lod_count; ++i) {
            modelrt_mesh_info mi;
            g3d_body_triangle_list *tl;
            tl = &body->lods[i];
            modelrt_zero(&mi, sizeof(mi));
            mi.index = i;
            modelrt_copy_string(mi.name, sizeof(mi.name), "Genesis3D LOD");
            mi.vertex_count = (modelrt_u32)body->xskin_vertex_count;
            mi.triangle_count = tl->face_count;
            mi.material_count = (modelrt_u32)body->material_count;
            if (sink->on_mesh_begin && sink->on_mesh_begin(user, &mi)) return MODELRT_ERR_CALLBACK;
            for (j = 0u; j < tl->face_count; ++j) {
                modelrt_triangle_view tv;
                int k;
                modelrt_zero(&tv, sizeof(tv));
                tv.mesh_index = i;
                tv.material_index = tl->faces[j].material_index;
                for (k = 0; k < 3; ++k) {
                    g3d_s16 vi, ni;
                    g3d_body_xskin_vertex *sv;
                    modelrt_vertex_view *v;
                    vi = tl->faces[j].vtx_index[k];
                    ni = tl->faces[j].normal_index[k];
                    if (vi < 0 || vi >= body->xskin_vertex_count) return MODELRT_ERR_FORMAT;
                    sv = &body->xskin_vertices[(g3d_u32)vi];
                    v = &tv.v[k];
                    v->position.x = s->skin.vertices[(g3d_u32)vi].x;
                    v->position.y = s->skin.vertices[(g3d_u32)vi].y;
                    v->position.z = s->skin.vertices[(g3d_u32)vi].z;
                    if (ni >= 0 && ni < body->normal_count) {
                        v->normal.x = s->skin.normals[(g3d_u32)ni].x;
                        v->normal.y = s->skin.normals[(g3d_u32)ni].y;
                        v->normal.z = s->skin.normals[(g3d_u32)ni].z;
                        v->flags |= MODELRT_VTX_NORMAL;
                    }
                    v->uv0.x = g3d_f32bits_to_fix16(sv->u_bits);
                    v->uv0.y = g3d_f32bits_to_fix16(sv->v_bits);
                    v->flags |= MODELRT_VTX_UV0 | MODELRT_VTX_SKIN;
                    v->bone[0] = sv->bone_index;
                    v->weight[0] = MODELRT_Q16_ONE;
                    v->weight_count = 1u;
                    v->rgba[0] = v->rgba[1] = v->rgba[2] = v->rgba[3] = 255u;
                }
                if (sink->on_triangle && sink->on_triangle(user, &tv)) return MODELRT_ERR_CALLBACK;
            }
            if (sink->on_mesh_end && sink->on_mesh_end(user, &mi)) return MODELRT_ERR_CALLBACK;
        }
        if (sink->on_bone) {
            for (i = 0u; i < (modelrt_u32)body->bone_count; ++i) {
                modelrt_bone_view bv;
                const char *name;
                modelrt_zero(&bv, sizeof(bv));
                bv.index = i;
                bv.parent = body->bones[i].parent_bone_index;
                name = g3d_body_get_bone_name(body, (g3d_s32)i);
                if (name) modelrt_copy_string(bv.name, sizeof(bv.name), name);
                bv.position.x = s->skin.bone_matrices[i].m[3];
                bv.position.y = s->skin.bone_matrices[i].m[7];
                bv.position.z = s->skin.bone_matrices[i].m[11];
                if (sink->on_bone(user, &bv)) return MODELRT_ERR_CALLBACK;
            }
        }
    }
    if (sink->on_model_end && sink->on_model_end(user, &handle->info)) return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}

static int g3d89_visit(modelrt_handle *handle, int animation_index, modelrt_q16 time,
                       const modelrt_visit_sink *sink, void *user)
{
    return g3d89_visit_with_motion(handle, animation_index, 0, time, sink, user);
}

static int g3d89_animation_count(const modelrt_handle *handle)
{
    const g3d89_state *s;
    s = (const g3d89_state *)handle->native_state;
    if (!s) return 0;
    if (s->kind == G3D89_KIND_ACTOR) return s->actor->motion_count;
    return s->kind == G3D89_KIND_MOTION ? 1 : 0;
}

static int g3d89_animation_info(modelrt_handle *handle, int animation_index,
                                modelrt_animation_info *out_info)
{
    g3d89_state *s;
    g3d_motion *m;
    g3d_fix16 start, end;
    if (!handle || !out_info) return MODELRT_ERR_BAD_ARG;
    s = (g3d89_state *)handle->native_state;
    if (!s) return MODELRT_ERR_BAD_ARG;
    if (s->kind == G3D89_KIND_ACTOR) {
        if (animation_index < 0 || animation_index >= s->actor->motion_count) return MODELRT_ERR_BAD_ARG;
        m = &s->actor->motions[animation_index];
    } else if (s->kind == G3D89_KIND_MOTION && animation_index == 0) m = s->motion;
    else return MODELRT_ERR_UNSUPPORTED;
    start = end = 0;
    if (g3d_motion_get_time_extents_fix16(m, &start, &end) != G3D_OK) return MODELRT_ERR_CODEC;
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)animation_index;
    modelrt_copy_string(out_info->name, sizeof(out_info->name), m->name ? m->name : "motion");
    out_info->start_time = start;
    out_info->end_time = end;
    return MODELRT_OK;
}

static g3d_body *g3d89_body(g3d89_state *s)
{
    if (!s) return 0;
    if (s->kind == G3D89_KIND_ACTOR) return s->actor->body;
    return s->kind == G3D89_KIND_BODY ? s->body : 0;
}

static int g3d89_texture_count(const modelrt_handle *handle)
{
    g3d89_state *s;
    g3d_body *b;
    s = (g3d89_state *)handle->native_state; b = g3d89_body(s);
    return b ? b->material_count : 0;
}

static int g3d89_texture_info(modelrt_handle *handle, int texture_index,
                              modelrt_texture_info *out_info)
{
    g3d89_state *s;
    g3d_body *b;
    g3d_bitmap_info bi;
    const char *name;
    if (!handle || !out_info) return MODELRT_ERR_BAD_ARG;
    s = (g3d89_state *)handle->native_state; b = g3d89_body(s);
    if (!b || texture_index < 0 || texture_index >= b->material_count) return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)texture_index;
    name = g3d_body_get_material_name(b, texture_index);
    if (name) modelrt_copy_string(out_info->name, sizeof(out_info->name), name);
    if (!b->materials[texture_index].has_bitmap) return MODELRT_OK;
    out_info->kind = MODELRT_TEXTURE_EMBEDDED;
    if (g3d_body_get_material_bitmap_info(b, texture_index, &bi) == G3D_OK) {
        out_info->width = bi.width; out_info->height = bi.height;
        out_info->rgba_bytes = bi.width * bi.height * 4u;
    }
    return MODELRT_OK;
}

static int g3d89_texture_decode(modelrt_handle *handle, int texture_index,
                                modelrt_u8 *dst, size_t dst_capacity,
                                modelrt_u32 *out_width, modelrt_u32 *out_height,
                                size_t *out_bytes)
{
    g3d89_state *s; g3d_body *b; g3d_u32 w,h,bytes; int rc;
    if (!handle || !dst) return MODELRT_ERR_BAD_ARG;
    s = (g3d89_state *)handle->native_state; b = g3d89_body(s);
    if (!b) return MODELRT_ERR_UNSUPPORTED;
    w=h=bytes=0u;
    rc = g3d_body_decode_material_rgba32(b, texture_index, 0u, 0u, dst, (g3d_u32)dst_capacity, &w, &h, &bytes);
    if (rc != G3D_OK) return MODELRT_ERR_CODEC;
    if (out_width) *out_width = w;
    if (out_height) *out_height = h;
    if (out_bytes) *out_bytes = (size_t)bytes;
    return MODELRT_OK;
}

static g3d_motion *g3d89_source_motion(g3d89_state *s, int animation_index)
{
    if (s == 0) return 0;
    if (s->kind == G3D89_KIND_MOTION) return animation_index == 0 ? s->motion : 0;
    if (s->kind == G3D89_KIND_ACTOR && animation_index >= 0 && animation_index < s->actor->motion_count)
        return &s->actor->motions[animation_index];
    return 0;
}

static int g3d89_animation_bind(modelrt_handle *target, modelrt_handle *source,
                                modelrt_animation_binding *binding)
{
    g3d89_state *ts;
    g3d89_state *ss;
    g3d_body *body;
    int ci;
    int total;
    int matched;
    if (target == 0 || source == 0 || binding == 0) return MODELRT_ERR_BAD_ARG;
    if (source->info.codec_id != MODELRT_CODEC_GENESIS3D) return MODELRT_ERR_UNSUPPORTED;
    ts = (g3d89_state *)target->native_state;
    ss = (g3d89_state *)source->native_state;
    body = g3d89_body(ts);
    if (body == 0 || ss == 0) return MODELRT_ERR_UNSUPPORTED;
    total = 0;
    matched = 0;
    for (ci = 0; ci < g3d89_animation_count(source); ++ci) {
        g3d_motion *m;
        int pi;
        m = g3d89_source_motion(ss, ci);
        if (m == 0) continue;
        for (pi = 0; pi < m->leaf.path_count; ++pi) {
            const char *name;
            g3d_s32 bone_index;
            name = g3d_motion_get_path_name(m, pi);
            ++total;
            bone_index = -1;
            if (name != 0 && g3d_body_find_bone_index(body, name, &bone_index) == G3D_OK)
                ++matched;
        }
    }
    if (total <= 0 || matched <= 0) return MODELRT_ERR_UNSUPPORTED;
    binding->provider_state = ss; /* source stays independently owned/open */
    binding->info.source_track_count = (modelrt_u32)total;
    binding->info.matched_track_count = (modelrt_u32)matched;
    binding->info.flags = 1u; /* Genesis path/bone-name compatibility */
    return MODELRT_OK;
}

static int g3d89_visit_bound_animation(modelrt_animation_binding *binding,
                                       int animation_index, modelrt_q16 time,
                                       const modelrt_visit_sink *sink, void *user)
{
    g3d89_state *ss;
    g3d_motion *motion;
    if (binding == 0 || binding->target == 0 || binding->source == 0) return MODELRT_ERR_BAD_ARG;
    ss = (g3d89_state *)binding->source->native_state;
    motion = g3d89_source_motion(ss, animation_index);
    if (motion == 0) return MODELRT_ERR_BAD_ARG;
    return g3d89_visit_with_motion(binding->target, MODELRT_ANIM_BIND_POSE, motion, time, sink, user);
}

const modelrt_provider modelrt_provider_g3d89 = {
    MODELRT_CODEC_GENESIS3D, "Genesis3D", g3d89_probe, g3d89_open, g3d89_close, g3d89_visit,
    g3d89_animation_count, g3d89_animation_info,
    g3d89_texture_count, g3d89_texture_info, g3d89_texture_decode, 0,
    g3d89_animation_bind, g3d89_visit_bound_animation,
    0
};
