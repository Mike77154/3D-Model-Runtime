#include "../modelrt_internal.h"

#include "../../vendor/3dfbxplus/3dfbx/3dfbx.h"

#include <string.h>

typedef struct modelrt_fbx_state_s {
    FbxArena arena;
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;
} modelrt_fbx_state;

static int fbx89_probe(const char *path, const modelrt_u8 *head, size_t head_size)
{
    static const char magic[] = "Kaydara FBX Binary";
    size_t i;
    if (modelrt_ext_eq(path, ".fbx")) return MODELRT_PROBE_YES;
    if (head != 0 && head_size >= sizeof(magic) - 1u) {
        for (i = 0u; i < sizeof(magic) - 1u; ++i) if (head[i] != (modelrt_u8)magic[i]) break;
        if (i == sizeof(magic) - 1u) return MODELRT_PROBE_YES;
    }
    return MODELRT_PROBE_NO;
}

static modelrt_i32 fbx89_find_material(const FbxScene *scene, fbx_i64 id)
{
    int i;
    if (scene == 0) return -1;
    for (i = 0; i < scene->material_count; ++i) {
        if (scene->materials[i] != 0 && scene->materials[i]->fbx_id == id) return (modelrt_i32)i;
    }
    return -1;
}

static modelrt_i32 fbx89_find_texture(const FbxScene *scene, fbx_i64 texture_id)
{
    int i;
    if (scene == 0) return -1;
    for (i = 0; i < scene->texture_count; ++i) {
        if (scene->textures[i].texture_id == texture_id) return (modelrt_i32)i;
    }
    return -1;
}

static modelrt_q16 fbx89_sat_add(modelrt_q16 a, modelrt_q16 b)
{
#if defined(_MSC_VER)
    __int64 x = (__int64)a + (__int64)b;
    if (x > 2147483647i64) return 2147483647;
    if (x < (-2147483647i64 - 1i64)) return (-2147483647 - 1);
    return (modelrt_q16)x;
#else
    __extension__ typedef long long fbx89_i64_local;
    fbx89_i64_local x;
    x = (fbx89_i64_local)a + (fbx89_i64_local)b;
    if (x > ((fbx89_i64_local)2147483647)) return 2147483647;
    if (x < (-((fbx89_i64_local)2147483647) - ((fbx89_i64_local)1))) return (-2147483647 - 1);
    return (modelrt_q16)x;
#endif
}

static void fbx89_transform_point(const fbx_real m[16], const fbx_real *p, modelrt_vec3 *out)
{
    modelrt_q16 x;
    modelrt_q16 y;
    modelrt_q16 z;
    x = fbx_real_mul(p[0], m[0]);
    x = fbx89_sat_add(x, fbx_real_mul(p[1], m[4]));
    x = fbx89_sat_add(x, fbx_real_mul(p[2], m[8]));
    x = fbx89_sat_add(x, m[12]);
    y = fbx_real_mul(p[0], m[1]);
    y = fbx89_sat_add(y, fbx_real_mul(p[1], m[5]));
    y = fbx89_sat_add(y, fbx_real_mul(p[2], m[9]));
    y = fbx89_sat_add(y, m[13]);
    z = fbx_real_mul(p[0], m[2]);
    z = fbx89_sat_add(z, fbx_real_mul(p[1], m[6]));
    z = fbx89_sat_add(z, fbx_real_mul(p[2], m[10]));
    z = fbx89_sat_add(z, m[14]);
    out->x = x; out->y = y; out->z = z;
}

static void fbx89_transform_normal(const fbx_real m[16], const fbx_real *p, modelrt_vec3 *out)
{
    modelrt_q16 x;
    modelrt_q16 y;
    modelrt_q16 z;
    x = fbx_real_mul(p[0], m[0]);
    x = fbx89_sat_add(x, fbx_real_mul(p[1], m[4]));
    x = fbx89_sat_add(x, fbx_real_mul(p[2], m[8]));
    y = fbx_real_mul(p[0], m[1]);
    y = fbx89_sat_add(y, fbx_real_mul(p[1], m[5]));
    y = fbx89_sat_add(y, fbx_real_mul(p[2], m[9]));
    z = fbx_real_mul(p[0], m[2]);
    z = fbx89_sat_add(z, fbx_real_mul(p[1], m[6]));
    z = fbx89_sat_add(z, fbx_real_mul(p[2], m[10]));
    out->x = x; out->y = y; out->z = z;
}

static modelrt_u8 fbx89_q16_to_u8(fbx_real v)
{
    modelrt_i32 n;
    if (v <= 0) return 0u;
    if (v >= FBX_REAL_ONE) return 255u;
    n = (modelrt_i32)(((v >> 8) * 255) >> 8);
    if (n < 0) n = 0;
    if (n > 255) n = 255;
    return (modelrt_u8)n;
}

static modelrt_i32 fbx89_triangle_material(const modelrt_fbx_state *st, const FbxMesh *mesh, fbx_u32 first_index)
{
    fbx_u32 s;
    int slot;
    if (mesh == 0 || st == 0 || st->scene == 0) return -1;
    slot = -1;
    for (s = 0u; s < mesh->submesh_count; ++s) {
        const FbxSubMesh *sm;
        sm = &mesh->submeshes[s];
        if (first_index >= sm->first_index && first_index < sm->first_index + sm->index_count) {
            slot = sm->material_index;
            break;
        }
    }
    if (slot < 0 && mesh->material_slot_count == 1) slot = 0;
    if (slot >= 0 && slot < mesh->material_slot_count && mesh->material_ids != 0)
        return fbx89_find_material(st->scene, mesh->material_ids[slot]);
    return -1;
}

static int fbx89_open(modelrt_runtime *rt, modelrt_handle *handle, const char *path)
{
    modelrt_fbx_state *st;
    modelrt_u8 *arena_base;
    size_t remain;
    FbxModel *m;
    int meshes;
    st = (modelrt_fbx_state *)modelrt_arena_alloc(handle, sizeof(*st), 8u);
    if (st == 0) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(st, sizeof(*st));
    if (handle->workspace_used >= handle->workspace_size) return MODELRT_ERR_NO_SPACE;
    arena_base = handle->workspace + handle->workspace_used;
    remain = handle->workspace_size - handle->workspace_used;
    fbx_arena_init(&st->arena, arena_base, remain);
    fbx_alloc_set_arena(&st->arena);
    st->ctx = fbx_open(path);
    if (st->ctx == 0) {
        modelrt_set_error(rt, "3dfbx", MODELRT_ERR_CODEC, "fbx_open failed");
        return MODELRT_ERR_CODEC;
    }
    st->root = fbx_read_scene(st->ctx);
    if (st->root == 0) {
        modelrt_set_error(rt, "3dfbx", MODELRT_ERR_CODEC, fbx_last_error(st->ctx));
        return MODELRT_ERR_CODEC;
    }
    st->scene = fbx_scene_build(st->root);
    if (st->scene == 0) {
        modelrt_set_error(rt, "3dfbx", MODELRT_ERR_CODEC, "fbx_scene_build failed");
        return MODELRT_ERR_CODEC;
    }
    fbx_scene_compute_global_transforms(st->scene);
    handle->native_state = st;
    handle->workspace_used = handle->workspace_size; /* rest belongs to FBX arena */
    handle->info.capabilities = MODELRT_CAP_GEOMETRY | MODELRT_CAP_MATERIALS | MODELRT_CAP_NATIVE_SCENE;
    meshes = 0;
    for (m = st->scene->models; m != 0; m = m->next) {
        if (m->mesh != 0) {
            ++meshes;
            if (m->mesh->normals != 0) handle->info.capabilities |= MODELRT_CAP_NORMALS;
            if (m->mesh->uv_set_count > 0u) handle->info.capabilities |= MODELRT_CAP_UV0;
            if (m->mesh->colors != 0) handle->info.capabilities |= MODELRT_CAP_VERTEX_COLOR;
            if (m->mesh->skin != 0) handle->info.capabilities |= MODELRT_CAP_SKINNING;
            if (m->mesh->morph_target_count > 0) handle->info.capabilities |= MODELRT_CAP_MORPHS;
        }
    }
    if (st->scene->skeleton != 0 && st->scene->skeleton->bone_count > 0) {
        handle->info.capabilities |= MODELRT_CAP_SKELETON;
        handle->info.capabilities |= MODELRT_CAP_ANIM_ATTACH_TARGET;
    }
    if (st->scene->anim_clip_count > 0)
        handle->info.capabilities |= MODELRT_CAP_ANIMATION | MODELRT_CAP_ANIM_ATTACH_SOURCE;
    if (st->scene->texture_count > 0) handle->info.capabilities |= MODELRT_CAP_TEXTURES;
    if (st->scene->camera_count > 0) handle->info.capabilities |= MODELRT_CAP_CAMERAS;
    if (st->scene->light_count > 0) handle->info.capabilities |= MODELRT_CAP_LIGHTS;
    handle->info.mesh_count = (modelrt_u32)meshes;
    handle->info.material_count = (modelrt_u32)st->scene->material_count;
    handle->info.texture_count = (modelrt_u32)st->scene->texture_count;
    handle->info.bone_count = st->scene->skeleton ? (modelrt_u32)st->scene->skeleton->bone_count : 0u;
    handle->info.animation_count = (modelrt_u32)st->scene->anim_clip_count;
    handle->info.camera_count = (modelrt_u32)st->scene->camera_count;
    handle->info.light_count = (modelrt_u32)st->scene->light_count;
    handle->info.morph_count = 0u;
    for (m = st->scene->models; m != 0; m = m->next)
        if (m->mesh != 0 && m->mesh->morph_target_count > 0) handle->info.morph_count += (modelrt_u32)m->mesh->morph_target_count;
    modelrt_copy_string(handle->info.name, sizeof(handle->info.name), path);
    return MODELRT_OK;
}

static void fbx89_close(modelrt_handle *handle)
{
    modelrt_fbx_state *st;
    if (handle == 0) return;
    st = (modelrt_fbx_state *)handle->native_state;
    if (st == 0) return;
    fbx_alloc_set_arena(&st->arena);
    if (st->scene != 0) fbx_scene_destroy(st->scene);
    if (st->ctx != 0) fbx_close(st->ctx, st->root);
    fbx_alloc_set_arena(0);
}

static int fbx89_emit_materials(modelrt_fbx_state *st, const modelrt_visit_sink *sink, void *user)
{
    int i;
    if (sink->on_material == 0 || st->scene == 0) return MODELRT_OK;
    for (i = 0; i < st->scene->material_count; ++i) {
        FbxMaterial *m;
        modelrt_material_view mv;
        int t;
        m = st->scene->materials[i];
        if (m == 0) continue;
        modelrt_zero(&mv, sizeof(mv));
        mv.index = (modelrt_u32)i;
        modelrt_copy_string(mv.name, sizeof(mv.name), m->name ? m->name : "fbx_material");
        mv.rgba[0] = fbx89_q16_to_u8(m->diffuse_color[0]);
        mv.rgba[1] = fbx89_q16_to_u8(m->diffuse_color[1]);
        mv.rgba[2] = fbx89_q16_to_u8(m->diffuse_color[2]);
        mv.rgba[3] = fbx89_q16_to_u8(m->opacity);
        mv.texture_index = -1;
        for (t = 0; t < m->texture_count; ++t) {
            if (m->textures[t].usage == FBX_TEX_USAGE_DIFFUSE) {
                mv.texture_index = fbx89_find_texture(st->scene, m->textures[t].texture_id);
                break;
            }
        }
        if (sink->on_material(user, &mv) != 0) return MODELRT_ERR_CALLBACK;
    }
    return MODELRT_OK;
}

static FbxModel *fbx89_model_by_id(FbxScene *scene, fbx_i64 id);

static void fbx89_mat_mul(const fbx_real a[16], const fbx_real b[16], fbx_real out[16])
{
    fbx_real t[16];
    int r;
    int c;
    int k;
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            fbx_real v;
            v = FBX_REAL_ZERO;
            for (k = 0; k < 4; ++k)
                v = fbx89_sat_add(v, fbx_real_mul(a[r * 4 + k], b[k * 4 + c]));
            t[r * 4 + c] = v;
        }
    }
    memcpy(out, t, sizeof(t));
}

static int fbx89_skin_position(modelrt_fbx_state *st, FbxModel *model, FbxMesh *mesh,
                               const FbxAnimClip *clip, modelrt_q16 time, fbx_u32 vi, modelrt_vec3 *out)
{
    FbxSkin *skin;
    fbx_u32 first;
    fbx_u16 count;
    int wi;
    fbx_real sumw;
    modelrt_vec3 accum;
    const fbx_real *src;
    if (st == 0 || model == 0 || mesh == 0 || out == 0) return 0;
    skin = mesh->skin;
    if (skin == 0 || skin->vertex_first_weight == 0 || skin->vertex_weight_count == 0 ||
        skin->weights == 0 || vi >= mesh->vertex_count) return 0;
    first = skin->vertex_first_weight[vi];
    count = skin->vertex_weight_count[vi];
    if (count == 0u) return 0;
    src = mesh->positions + (size_t)vi * 3u;
    accum.x = 0; accum.y = 0; accum.z = 0;
    sumw = FBX_REAL_ZERO;
    for (wi = 0; wi < (int)count; ++wi) {
        const FbxVertexWeight *vw;
        FbxBone *bone;
        FbxModel *bm;
        fbx_real current[16];
        fbx_real tmp[16];
        fbx_real deform[16];
        modelrt_vec3 p;
        vw = &skin->weights[first + (fbx_u32)wi];
        if (vw->bone >= (fbx_u16)st->scene->skeleton->bone_count) continue;
        bone = &st->scene->skeleton->bones[vw->bone];
        bm = fbx89_model_by_id(st->scene, bone->fbx_id);
        if (bm == 0 || !bone->has_bind_pose) continue;
        if (clip != 0) fbx_model_eval_global_matrix(bm, clip, time, current);
        else memcpy(current, bm->global_matrix, sizeof(current));
        /* Row-vector convention: p * meshBind * inverse(boneBind) * boneCurrent. */
        if (skin->has_mesh_bind_pose) fbx89_mat_mul(skin->mesh_bind_pose, bone->inv_bind_pose, tmp);
        else fbx89_mat_mul(model->global_matrix, bone->inv_bind_pose, tmp);
        fbx89_mat_mul(tmp, current, deform);
        fbx89_transform_point(deform, src, &p);
        accum.x = fbx89_sat_add(accum.x, fbx_real_mul(p.x, vw->weight));
        accum.y = fbx89_sat_add(accum.y, fbx_real_mul(p.y, vw->weight));
        accum.z = fbx89_sat_add(accum.z, fbx_real_mul(p.z, vw->weight));
        sumw = fbx89_sat_add(sumw, vw->weight);
    }
    if (sumw <= FBX_REAL_EPSILON) return 0;
    if (sumw != FBX_REAL_ONE) {
        accum.x = fbx_real_div(accum.x, sumw);
        accum.y = fbx_real_div(accum.y, sumw);
        accum.z = fbx_real_div(accum.z, sumw);
    }
    *out = accum;
    return 1;
}

static void fbx89_fill_vertex(const FbxMesh *mesh, fbx_u32 vi, const fbx_real mat[16], modelrt_vertex_view *v)
{
    int w;
    modelrt_zero(v, sizeof(*v));
    v->rgba[0] = 255u; v->rgba[1] = 255u; v->rgba[2] = 255u; v->rgba[3] = 255u;
    if (vi >= mesh->vertex_count) return;
    fbx89_transform_point(mat, mesh->positions + (size_t)vi * 3u, &v->position);
    if (mesh->normals != 0) {
        fbx89_transform_normal(mat, mesh->normals + (size_t)vi * 3u, &v->normal);
        v->flags |= MODELRT_VTX_NORMAL;
    }
    if (mesh->uvs != 0) {
        v->uv0.x = mesh->uvs[(size_t)vi * 2u + 0u];
        v->uv0.y = mesh->uvs[(size_t)vi * 2u + 1u];
        v->flags |= MODELRT_VTX_UV0;
    }
    if (mesh->colors != 0) {
        v->rgba[0] = fbx89_q16_to_u8(mesh->colors[(size_t)vi * 4u + 0u]);
        v->rgba[1] = fbx89_q16_to_u8(mesh->colors[(size_t)vi * 4u + 1u]);
        v->rgba[2] = fbx89_q16_to_u8(mesh->colors[(size_t)vi * 4u + 2u]);
        v->rgba[3] = fbx89_q16_to_u8(mesh->colors[(size_t)vi * 4u + 3u]);
        v->flags |= MODELRT_VTX_COLOR;
    }
    if (mesh->skin != 0 && mesh->skin->vertex_first_weight != 0 && mesh->skin->vertex_weight_count != 0) {
        fbx_u32 first;
        fbx_u16 count;
        first = mesh->skin->vertex_first_weight[vi];
        count = mesh->skin->vertex_weight_count[vi];
        if (count > 4u) count = 4u;
        v->weight_count = (modelrt_u8)count;
        for (w = 0; w < (int)count; ++w) {
            const FbxVertexWeight *fw;
            fw = &mesh->skin->weights[first + (fbx_u32)w];
            v->bone[w] = (modelrt_i16)fw->bone;
            v->weight[w] = fw->weight;
        }
        if (count > 0u) v->flags |= MODELRT_VTX_SKIN;
    }
}

static FbxModel *fbx89_model_by_id(FbxScene *scene, fbx_i64 id)
{
    FbxModel *m;
    if (scene == 0) return 0;
    for (m = scene->models; m != 0; m = m->next) if (m->id == id) return m;
    return 0;
}

static int fbx89_visit_clip(modelrt_handle *handle, const FbxAnimClip *clip, modelrt_q16 time,
                            const modelrt_visit_sink *sink, void *user)
{
    modelrt_fbx_state *st;
    FbxModel *m;
    modelrt_u32 mesh_index;
    int rc;
    if (handle == 0 || sink == 0) return MODELRT_ERR_BAD_ARG;
    st = (modelrt_fbx_state *)handle->native_state;
    if (st == 0 || st->scene == 0) return MODELRT_ERR_CODEC;
    if (sink->on_model_begin != 0 && sink->on_model_begin(user, &handle->info) != 0) return MODELRT_ERR_CALLBACK;
    rc = fbx89_emit_materials(st, sink, user);
    if (rc != MODELRT_OK) return rc;
    mesh_index = 0u;
    for (m = st->scene->models; m != 0; m = m->next) {
        FbxMesh *mesh;
        modelrt_mesh_info mi;
        fbx_real mat[16];
        fbx_u32 ii;
        if (m->mesh == 0) continue;
        mesh = m->mesh;
        modelrt_zero(&mi, sizeof(mi));
        mi.index = mesh_index;
        modelrt_copy_string(mi.name, sizeof(mi.name), m->name ? m->name : "fbx_mesh");
        mi.vertex_count = mesh->vertex_count;
        mi.triangle_count = mesh->index_count / 3u;
        mi.material_count = (modelrt_u32)((mesh->material_slot_count > 0) ? mesh->material_slot_count : 0);
        if (sink->on_mesh_begin != 0 && sink->on_mesh_begin(user, &mi) != 0) return MODELRT_ERR_CALLBACK;
        {
            fbx_real node_global[16];
            if (clip != 0) fbx_model_eval_global_matrix(m, clip, time, node_global);
            else memcpy(node_global, m->global_matrix, sizeof(node_global));
            /* Geometric transform affects attached geometry but is not inherited. */
            fbx89_mat_mul(m->geom_matrix, node_global, mat);
        }
        for (ii = 0u; ii + 2u < mesh->index_count; ii += 3u) {
            modelrt_triangle_view tri;
            fbx_u32 a;
            fbx_u32 b;
            fbx_u32 c;
            modelrt_zero(&tri, sizeof(tri));
            a = mesh->indices[ii + 0u]; b = mesh->indices[ii + 1u]; c = mesh->indices[ii + 2u];
            if (a >= mesh->vertex_count || b >= mesh->vertex_count || c >= mesh->vertex_count) continue;
            tri.mesh_index = mesh_index;
            tri.material_index = fbx89_triangle_material(st, mesh, ii);
            fbx89_fill_vertex(mesh, a, mat, &tri.v[0]);
            fbx89_fill_vertex(mesh, b, mat, &tri.v[1]);
            fbx89_fill_vertex(mesh, c, mat, &tri.v[2]);
            if (mesh->skin != 0 && st->scene->skeleton != 0) {
                modelrt_vec3 skinned;
                if (fbx89_skin_position(st, m, mesh, clip, time, a, &skinned)) tri.v[0].position = skinned;
                if (fbx89_skin_position(st, m, mesh, clip, time, b, &skinned)) tri.v[1].position = skinned;
                if (fbx89_skin_position(st, m, mesh, clip, time, c, &skinned)) tri.v[2].position = skinned;
            }
            if (sink->on_triangle != 0 && sink->on_triangle(user, &tri) != 0) return MODELRT_ERR_CALLBACK;
        }
        if (sink->on_mesh_end != 0 && sink->on_mesh_end(user, &mi) != 0) return MODELRT_ERR_CALLBACK;
        ++mesh_index;
    }
    if (sink->on_bone != 0 && st->scene->skeleton != 0) {
        int bi;
        for (bi = 0; bi < st->scene->skeleton->bone_count; ++bi) {
            FbxBone *b;
            FbxModel *bm;
            modelrt_bone_view bv;
            fbx_real bmat[16];
            b = &st->scene->skeleton->bones[bi];
            modelrt_zero(&bv, sizeof(bv));
            bv.index = (modelrt_u32)bi;
            bv.parent = b->parent_index;
            modelrt_copy_string(bv.name, sizeof(bv.name), b->name ? b->name : "bone");
            bm = fbx89_model_by_id(st->scene, b->fbx_id);
            if (bm != 0) {
                if (clip != 0) fbx_model_eval_global_matrix(bm, clip, time, bmat);
                else memcpy(bmat, bm->global_matrix, sizeof(bmat));
                bv.position.x = bmat[12]; bv.position.y = bmat[13]; bv.position.z = bmat[14];
            } else {
                bv.position.x = b->bind_pose[12]; bv.position.y = b->bind_pose[13]; bv.position.z = b->bind_pose[14];
            }
            if (sink->on_bone(user, &bv) != 0) return MODELRT_ERR_CALLBACK;
        }
    }
    if (sink->on_model_end != 0 && sink->on_model_end(user, &handle->info) != 0) return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}

static int fbx89_visit(modelrt_handle *handle, int animation_index, modelrt_q16 time,
                       const modelrt_visit_sink *sink, void *user)
{
    modelrt_fbx_state *st;
    const FbxAnimClip *clip;
    if (handle == 0 || sink == 0) return MODELRT_ERR_BAD_ARG;
    st = (modelrt_fbx_state *)handle->native_state;
    if (st == 0 || st->scene == 0) return MODELRT_ERR_CODEC;
    clip = 0;
    if (animation_index >= 0) {
        if (animation_index >= st->scene->anim_clip_count) return MODELRT_ERR_BAD_ARG;
        clip = st->scene->anim_clips[animation_index];
    }
    return fbx89_visit_clip(handle, clip, time, sink, user);
}

static int fbx89_animation_count(const modelrt_handle *handle)
{
    const modelrt_fbx_state *st;
    st = (const modelrt_fbx_state *)handle->native_state;
    return (st && st->scene) ? st->scene->anim_clip_count : 0;
}

static int fbx89_animation_info(modelrt_handle *handle, int animation_index, modelrt_animation_info *out_info)
{
    modelrt_fbx_state *st;
    FbxAnimClip *clip;
    if (handle == 0 || out_info == 0) return MODELRT_ERR_BAD_ARG;
    st = (modelrt_fbx_state *)handle->native_state;
    if (st == 0 || st->scene == 0 || animation_index < 0 || animation_index >= st->scene->anim_clip_count)
        return MODELRT_ERR_BAD_ARG;
    clip = st->scene->anim_clips[animation_index];
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)animation_index;
    modelrt_copy_string(out_info->name, sizeof(out_info->name), clip && clip->name ? clip->name : "FBX Animation");
    if (clip != 0) { out_info->start_time = clip->start_time; out_info->end_time = clip->end_time; }
    return MODELRT_OK;
}

static int fbx89_texture_count(const modelrt_handle *handle)
{
    const modelrt_fbx_state *st;
    st = (const modelrt_fbx_state *)handle->native_state;
    return (st && st->scene) ? st->scene->texture_count : 0;
}

static int fbx89_texture_info(modelrt_handle *handle, int texture_index, modelrt_texture_info *out_info)
{
    modelrt_fbx_state *st;
    FbxTextureSlot *t;
    if (handle == 0 || out_info == 0) return MODELRT_ERR_BAD_ARG;
    st = (modelrt_fbx_state *)handle->native_state;
    if (st == 0 || st->scene == 0 || texture_index < 0 || texture_index >= st->scene->texture_count) return MODELRT_ERR_BAD_ARG;
    t = &st->scene->textures[texture_index];
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)texture_index;
    modelrt_copy_string(out_info->name, sizeof(out_info->name), t->name ? t->name : "fbx_texture");
    /* Keep both media references.  Autodesk FBX media resolution checks the
       stored Filename first and then RelativeFilename; exposing both lets a
       host reproduce that behavior and inspect what the asset actually says. */
    if (t->file_path != 0 && t->file_path[0] != '\0') {
        modelrt_copy_string(out_info->source_path, sizeof(out_info->source_path), t->file_path);
        modelrt_copy_string(out_info->path, sizeof(out_info->path), t->file_path);
    }
    if (t->relative_path != 0 && t->relative_path[0] != '\0') {
        modelrt_copy_string(out_info->relative_path, sizeof(out_info->relative_path), t->relative_path);
        if (out_info->path[0] == '\0')
            modelrt_copy_string(out_info->path, sizeof(out_info->path), t->relative_path);
    }
    if (t->embedded) {
        out_info->kind = MODELRT_TEXTURE_EMBEDDED;
        out_info->rgba_bytes = t->embedded_size; /* encoded payload bytes, not RGBA */
    } else {
        out_info->kind = MODELRT_TEXTURE_EXTERNAL;
    }
    return MODELRT_OK;
}

static int fbx89_texture_encoded_view(modelrt_handle *handle, int texture_index,
                                      modelrt_texture_blob_view *out_view)
{
    modelrt_fbx_state *st;
    FbxTextureSlot *t;
    if (handle == 0 || out_view == 0) return MODELRT_ERR_BAD_ARG;
    st = (modelrt_fbx_state *)handle->native_state;
    if (st == 0 || st->scene == 0 || texture_index < 0 ||
        texture_index >= st->scene->texture_count) return MODELRT_ERR_BAD_ARG;
    t = &st->scene->textures[texture_index];
    out_view->data = 0;
    out_view->size = 0u;
    out_view->flags = 0u;
    if (!t->embedded || t->embedded_data == 0 || t->embedded_size == 0u)
        return MODELRT_ERR_UNSUPPORTED;
    out_view->data = (const modelrt_u8 *)t->embedded_data;
    out_view->size = (size_t)t->embedded_size;
    return MODELRT_OK;
}

typedef struct fbx89_anim_bind_state_s {
    FbxAnimClip mapped_clip;
    FbxNodeAnimTrack *mapped_tracks;
    int track_capacity;
} fbx89_anim_bind_state;

static const char *fbx89_name_leaf(const char *name)
{
    const char *p;
    const char *leaf;
    if (name == 0) return "";
    leaf = name;
    for (p = name; *p != '\0'; ++p) {
        if (*p == ':' || *p == '|' || *p == '/' || *p == '\\') leaf = p + 1;
    }
    return leaf;
}

static int fbx89_name_equiv(const char *a, const char *b)
{
    if (a == 0 || b == 0) return 0;
    if (strcmp(a, b) == 0) return 1;
    return strcmp(fbx89_name_leaf(a), fbx89_name_leaf(b)) == 0;
}

static FbxModel *fbx89_model_by_name(FbxScene *scene, const char *name)
{
    FbxModel *m;
    FbxModel *fallback;
    if (scene == 0 || name == 0) return 0;
    fallback = 0;
    for (m = scene->models; m != 0; m = m->next) {
        if (m->name == 0) continue;
        if (strcmp(m->name, name) == 0) return m;
        if (fallback == 0 && fbx89_name_equiv(m->name, name)) fallback = m;
    }
    return fallback;
}

static FbxModel *fbx89_source_model_for_track(FbxScene *scene, const FbxNodeAnimTrack *track)
{
    if (scene == 0 || track == 0) return 0;
    return fbx89_model_by_id(scene, track->fbx_id);
}

static int fbx89_prepare_mapped_clip(modelrt_animation_binding *binding, int animation_index,
                                     const FbxAnimClip **out_clip)
{
    modelrt_fbx_state *target_st;
    modelrt_fbx_state *source_st;
    fbx89_anim_bind_state *bs;
    FbxAnimClip *src;
    int i;
    int n;
    if (binding == 0 || out_clip == 0 || binding->target == 0 || binding->source == 0)
        return MODELRT_ERR_BAD_ARG;
    target_st = (modelrt_fbx_state *)binding->target->native_state;
    source_st = (modelrt_fbx_state *)binding->source->native_state;
    bs = (fbx89_anim_bind_state *)binding->provider_state;
    if (target_st == 0 || source_st == 0 || bs == 0 || target_st->scene == 0 || source_st->scene == 0)
        return MODELRT_ERR_CODEC;
    if (animation_index < 0 || animation_index >= source_st->scene->anim_clip_count)
        return MODELRT_ERR_BAD_ARG;
    src = source_st->scene->anim_clips[animation_index];
    if (src == 0) return MODELRT_ERR_CODEC;
    bs->mapped_clip = *src;
    bs->mapped_clip.tracks = bs->mapped_tracks;
    bs->mapped_clip.track_count = 0;
    /* Baked tracks and morph IDs belong to the source scene. Keep this first
       attachment path strictly skeletal/TRS and use the live native curves. */
    bs->mapped_clip.baked_fps = 0;
    bs->mapped_clip.baked_frame_count = 0;
    bs->mapped_clip.baked_times = 0;
    bs->mapped_clip.baked_tracks = 0;
    bs->mapped_clip.morph_track_count = 0;
    bs->mapped_clip.morph_tracks = 0;
    n = 0;
    for (i = 0; i < src->track_count; ++i) {
        FbxModel *sm;
        FbxModel *tm;
        sm = fbx89_source_model_for_track(source_st->scene, &src->tracks[i]);
        if (sm == 0 || sm->name == 0) continue;
        tm = fbx89_model_by_name(target_st->scene, sm->name);
        if (tm == 0) continue;
        if (n >= bs->track_capacity) return MODELRT_ERR_NO_SPACE;
        bs->mapped_tracks[n] = src->tracks[i];
        bs->mapped_tracks[n].fbx_id = tm->id;
        ++n;
    }
    bs->mapped_clip.track_count = n;
    if (n <= 0) return MODELRT_ERR_UNSUPPORTED;
    *out_clip = &bs->mapped_clip;
    return MODELRT_OK;
}

static int fbx89_animation_bind(modelrt_handle *target, modelrt_handle *source,
                                modelrt_animation_binding *binding)
{
    modelrt_fbx_state *target_st;
    modelrt_fbx_state *source_st;
    fbx89_anim_bind_state *bs;
    int max_tracks;
    int total_tracks;
    int matched_tracks;
    int ci;
    if (target == 0 || source == 0 || binding == 0) return MODELRT_ERR_BAD_ARG;
    if (source->info.codec_id != MODELRT_CODEC_FBX) return MODELRT_ERR_UNSUPPORTED;
    target_st = (modelrt_fbx_state *)target->native_state;
    source_st = (modelrt_fbx_state *)source->native_state;
    if (target_st == 0 || source_st == 0 || target_st->scene == 0 || source_st->scene == 0)
        return MODELRT_ERR_CODEC;
    if (source_st->scene->anim_clip_count <= 0) return MODELRT_ERR_UNSUPPORTED;
    max_tracks = 0;
    total_tracks = 0;
    matched_tracks = 0;
    for (ci = 0; ci < source_st->scene->anim_clip_count; ++ci) {
        FbxAnimClip *clip;
        int ti;
        clip = source_st->scene->anim_clips[ci];
        if (clip == 0) continue;
        if (clip->track_count > max_tracks) max_tracks = clip->track_count;
        total_tracks += clip->track_count;
        for (ti = 0; ti < clip->track_count; ++ti) {
            FbxModel *sm;
            sm = fbx89_source_model_for_track(source_st->scene, &clip->tracks[ti]);
            if (sm != 0 && sm->name != 0 && fbx89_model_by_name(target_st->scene, sm->name) != 0)
                ++matched_tracks;
        }
    }
    if (max_tracks <= 0 || matched_tracks <= 0) return MODELRT_ERR_UNSUPPORTED;
    bs = (fbx89_anim_bind_state *)modelrt_binding_alloc(binding, sizeof(*bs), 8u);
    if (bs == 0) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(bs, sizeof(*bs));
    bs->mapped_tracks = (FbxNodeAnimTrack *)modelrt_binding_alloc(binding,
        sizeof(FbxNodeAnimTrack) * (size_t)max_tracks, 8u);
    if (bs->mapped_tracks == 0) return MODELRT_ERR_NO_SPACE;
    bs->track_capacity = max_tracks;
    binding->provider_state = bs;
    binding->info.source_track_count = (modelrt_u32)total_tracks;
    binding->info.matched_track_count = (modelrt_u32)matched_tracks;
    binding->info.flags = 1u; /* name-based native FBX track mapping */
    return MODELRT_OK;
}

static int fbx89_visit_bound_animation(modelrt_animation_binding *binding,
                                       int animation_index, modelrt_q16 time,
                                       const modelrt_visit_sink *sink, void *user)
{
    const FbxAnimClip *clip;
    int rc;
    if (binding == 0 || binding->target == 0) return MODELRT_ERR_BAD_ARG;
    clip = 0;
    rc = fbx89_prepare_mapped_clip(binding, animation_index, &clip);
    if (rc != MODELRT_OK) return rc;
    return fbx89_visit_clip(binding->target, clip, time, sink, user);
}

const modelrt_provider modelrt_provider_fbx89 = {
    MODELRT_CODEC_FBX,
    "3dfbx",
    fbx89_probe,
    fbx89_open,
    fbx89_close,
    fbx89_visit,
    fbx89_animation_count,
    fbx89_animation_info,
    fbx89_texture_count,
    fbx89_texture_info,
    0,
    fbx89_texture_encoded_view,
    fbx89_animation_bind,
    fbx89_visit_bound_animation,
    0
};
