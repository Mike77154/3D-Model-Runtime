/* ActorX PSK/PSA adapter. Native codec retained as vendor; no universal asset conversion. */
#include "../modelrt_internal.h"
#include "pskpsa3d.h"
#include "../../include/modelrt_native89.h"
#include <string.h>
#include <stdio.h>

#define AX89_PSK 1
#define AX89_PSA 2

typedef struct ax89_state_s {
    int kind;
    psk3d_model *mesh;
    psk3d_animset *anim;
    psk3d_skin_cache *cache;
    psk3d_skinned_points *points;
    psk3d_skinned_normals *normals;
    psk3d_pose *local_pose;
    psk3d_pose *world_pose;
} ax89_state;

typedef struct ax89_bind_s { psk3d_bone_map map; } ax89_bind_state;

static int ax89_probe(const char *path, const modelrt_u8 *head, size_t size)
{
    if (modelrt_ext_eq(path, ".psk") || modelrt_ext_eq(path, ".pskx") ||
        modelrt_ext_eq(path, ".psa")) {
        if (head != 0 && size >= 8u &&
            (memcmp(head, "ACTRHEAD", 8u) == 0 || memcmp(head, "ANIMHEAD", 8u) == 0))
            return MODELRT_PROBE_YES;
        return MODELRT_PROBE_MAYBE;
    }
    return MODELRT_PROBE_NO;
}

static int ax89_open(modelrt_runtime *rt, modelrt_handle *h, const char *path)
{
    ax89_state *s;
    modelrt_u8 *data;
    size_t n;
    psk3d_report *rep;
    psk3d_result e;
    int rc;
    s = (ax89_state *)modelrt_arena_alloc(h, sizeof(*s), 8u);
    rep = (psk3d_report *)modelrt_arena_alloc(h, sizeof(*rep), 8u);
    if (!s || !rep) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s, sizeof(*s));
    rc = modelrt_read_file_arena(h, path, &data, &n);
    if (rc != MODELRT_OK) return rc;
    if (n > 0xffffffffu) return MODELRT_ERR_LIMIT;
    if (modelrt_ext_eq(path, ".psa")) {
        s->anim = (psk3d_animset *)modelrt_arena_alloc(h, sizeof(psk3d_animset), 8u);
        if (!s->anim) return MODELRT_ERR_NO_SPACE;
        e = psk3d_parse_psa(data, (psk3d_u32)n, s->anim, rep);
        if (e != PSK3D_OK) { modelrt_set_error(rt, "ActorX PSA", MODELRT_ERR_CODEC, psk3d_result_name(e)); return MODELRT_ERR_CODEC; }
        s->kind = AX89_PSA;
        h->info.bone_count = s->anim->bone_count;
        h->info.animation_count = s->anim->sequence_count;
        h->info.capabilities = MODELRT_CAP_ANIMATION | MODELRT_CAP_SKELETON | MODELRT_CAP_ANIM_ATTACH_SOURCE;
    } else {
        s->mesh = (psk3d_model *)modelrt_arena_alloc(h, sizeof(psk3d_model), 8u);
        if (!s->mesh) return MODELRT_ERR_NO_SPACE;
        e = psk3d_parse_psk(data, (psk3d_u32)n, s->mesh, rep);
        if (e != PSK3D_OK) { modelrt_set_error(rt, "ActorX PSK", MODELRT_ERR_CODEC, psk3d_result_name(e)); return MODELRT_ERR_CODEC; }
        s->kind = AX89_PSK;
        h->info.mesh_count = 1u;
        h->info.material_count = s->mesh->material_count;
        h->info.texture_count = s->mesh->material_count;
        h->info.bone_count = s->mesh->bone_count;
        h->info.morph_count = s->mesh->morph_target_count;
        h->info.capabilities = MODELRT_CAP_GEOMETRY | MODELRT_CAP_UV0 | MODELRT_CAP_MATERIALS |
          MODELRT_CAP_TEXTURES | MODELRT_CAP_EXTERNAL_TEX | MODELRT_CAP_SKELETON |
          MODELRT_CAP_SKINNING | MODELRT_CAP_ANIM_ATTACH_TARGET;
        if (s->mesh->normal_count) h->info.capabilities |= MODELRT_CAP_NORMALS;
        if (s->mesh->vertex_color_count) h->info.capabilities |= MODELRT_CAP_VERTEX_COLOR;
        if (s->mesh->morph_target_count) h->info.capabilities |= MODELRT_CAP_MORPHS;
        s->cache = (psk3d_skin_cache *)modelrt_arena_alloc(h, sizeof(psk3d_skin_cache), 8u);
        s->points = (psk3d_skinned_points *)modelrt_arena_alloc(h, sizeof(psk3d_skinned_points), 8u);
        s->normals = (psk3d_skinned_normals *)modelrt_arena_alloc(h, sizeof(psk3d_skinned_normals), 8u);
        s->local_pose = (psk3d_pose *)modelrt_arena_alloc(h, sizeof(psk3d_pose), 8u);
        s->world_pose = (psk3d_pose *)modelrt_arena_alloc(h, sizeof(psk3d_pose), 8u);
        if (!s->cache || !s->points || !s->normals || !s->local_pose || !s->world_pose) return MODELRT_ERR_NO_SPACE;
        e = psk3d_build_skin_cache(s->mesh, s->cache);
        if (e != PSK3D_OK) {
            /* An unweighted mesh is still displayable in bind-pose. */
            s->cache = 0;
        }
    }
    h->native_state = s;
    modelrt_copy_string(h->info.name, sizeof(h->info.name), path);
    return MODELRT_OK;
}
static void ax89_close(modelrt_handle *h) { (void)h; }

static int ax89_emit(modelrt_handle *h, const psk3d_skinned_points *points,
                     const psk3d_skinned_normals *normals,
                     const psk3d_pose *scene_pose,
                     const modelrt_visit_sink *sink, void *user)
{
    ax89_state *s;
    const psk3d_model *m;
    modelrt_mesh_info mesh;
    modelrt_material_view mat;
    modelrt_triangle_view tri;
    modelrt_bone_view bone;
    modelrt_u32 j, k, wi, pi, ni;
    const psk3d_wedge *w;
    const psk3d_vec3 *p;
    if (!h || !sink) return MODELRT_ERR_BAD_ARG;
    s = (ax89_state *)h->native_state;
    if (!s || !s->mesh) return MODELRT_ERR_UNSUPPORTED;
    m = s->mesh;
    if (sink->on_model_begin && sink->on_model_begin(user, &h->info)) return MODELRT_ERR_CALLBACK;
    modelrt_zero(&mesh, sizeof(mesh));
    mesh.index = 0u; mesh.vertex_count = m->wedge_count;
    mesh.triangle_count = m->face_count; mesh.material_count = m->material_count;
    modelrt_copy_string(mesh.name, sizeof(mesh.name), "ActorX mesh");
    if (sink->on_mesh_begin && sink->on_mesh_begin(user, &mesh)) return MODELRT_ERR_CALLBACK;
    for (j=0u;j<m->material_count;++j) {
        modelrt_zero(&mat, sizeof(mat)); mat.index=j;
        mat.texture_index=(modelrt_i32)j;
        modelrt_copy_string(mat.name, sizeof(mat.name), m->materials[j].name);
        mat.rgba[0]=255;mat.rgba[1]=255;mat.rgba[2]=255;mat.rgba[3]=255;
        if (m->materials[j].poly_flags & 0x00000002u) mat.flags |= MODELRT_MAT_ALPHA_MASK;
        if (sink->on_material && sink->on_material(user, &mat)) return MODELRT_ERR_CALLBACK;
    }
    for (j=0u;j<m->face_count;++j) {
        modelrt_zero(&tri, sizeof(tri));
        tri.mesh_index=0u;
        tri.material_index=(modelrt_i32)m->faces[j].mat_index;
        for (k=0u;k<3u;++k) {
            wi=m->faces[j].wedge[k];
            if (wi>=m->wedge_count) return MODELRT_ERR_FORMAT;
            w=&m->wedges[wi];pi=w->point_index;
            if (pi>=m->point_count) return MODELRT_ERR_FORMAT;
            p=points ? &points->points[pi] : &m->points[pi].p;
            /* PSK native is Z-up. Scene convention uses Y-up, handedness preserved. */
            tri.v[k].position.x=p->x;tri.v[k].position.y=p->z;tri.v[k].position.z=-p->y;
            tri.v[k].uv0.x=w->u;tri.v[k].uv0.y=w->v;
            tri.v[k].flags=MODELRT_VTX_UV0;
            ni = (m->normal_count == m->point_count) ? pi : wi;
            if (ni<m->normal_count) {
                const psk3d_vec3 *nv = (normals && ni<normals->normal_count) ?
                    &normals->normals[ni] : &m->normals[ni];
                tri.v[k].normal.x=nv->x;
                tri.v[k].normal.y=nv->z;
                tri.v[k].normal.z=-nv->y;
                tri.v[k].flags |= MODELRT_VTX_NORMAL;
            }
            if (wi<m->vertex_color_count) {
                tri.v[k].rgba[0]=m->vertex_colors[wi].r;
                tri.v[k].rgba[1]=m->vertex_colors[wi].g;
                tri.v[k].rgba[2]=m->vertex_colors[wi].b;
                tri.v[k].rgba[3]=m->vertex_colors[wi].a;
                tri.v[k].flags |= MODELRT_VTX_COLOR;
            } else {
                tri.v[k].rgba[0]=255;tri.v[k].rgba[1]=255;tri.v[k].rgba[2]=255;tri.v[k].rgba[3]=255;
            }
        }
        if (sink->on_triangle && sink->on_triangle(user, &tri)) return MODELRT_ERR_CALLBACK;
    }
    for (j=0u;j<m->bone_count;++j) {
        modelrt_zero(&bone,sizeof(bone)); bone.index=j;
        bone.parent=m->bones[j].parent_index;
        if (j==0u && bone.parent==0) bone.parent=-1; /* ActorX root sentinel */
        modelrt_copy_string(bone.name,sizeof(bone.name),m->bones[j].name);
        /* Viewer skeleton links require world bone positions, not local bind offsets. */
        {
            const psk3d_vec3 *bp = (scene_pose && j<scene_pose->bone_count) ?
                 &scene_pose->positions[j] : &m->bones[j].position;
            bone.position.x=bp->x;
            bone.position.y=bp->z;
            bone.position.z=-bp->y;
        }
        if (sink->on_bone && sink->on_bone(user,&bone)) return MODELRT_ERR_CALLBACK;
    }
    if (sink->on_mesh_end && sink->on_mesh_end(user,&mesh)) return MODELRT_ERR_CALLBACK;
    if (sink->on_model_end && sink->on_model_end(user,&h->info)) return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}

static int ax89_visit(modelrt_handle *h, int animation_index, modelrt_q16 time,
                      const modelrt_visit_sink *sink, void *user)
{
    const ax89_state *s = (const ax89_state *)h->native_state;
    (void)time;
    if (animation_index != MODELRT_ANIM_BIND_POSE && animation_index != 0) return MODELRT_ERR_BAD_ARG;
    return ax89_emit(h, 0, 0, s->cache ? &s->cache->bind_global_pose : 0, sink, user);
}
static int ax89_count(const modelrt_handle *h)
{
    const ax89_state *s=(const ax89_state *)h->native_state;
    if (!s || !s->anim) return 0;
    return (int)s->anim->sequence_count;
}
static int ax89_anim_info(modelrt_handle *h,int index,modelrt_animation_info *out)
{
    ax89_state *s;
    const psk3d_anim_info *a;
    modelrt_u32 rate;
    if (!h || !out || index<0) return MODELRT_ERR_BAD_ARG;
    s=(ax89_state *)h->native_state;
    if (!s || !s->anim || (modelrt_u32)index>=s->anim->sequence_count) return MODELRT_ERR_BAD_ARG;
    a=&s->anim->sequences[index];modelrt_zero(out,sizeof(*out));
    out->index=(modelrt_u32)index;
    modelrt_copy_string(out->name,sizeof(out->name),a->name);
    rate=(a->anim_rate>0) ? (modelrt_u32)(a->anim_rate >> 16) : 0u;
    if (rate==0u) rate=30u; /* Rate unknown: preview fallback, not a source claim */
    out->frame_count=a->num_raw_frames>0?(modelrt_u32)a->num_raw_frames:0u;
    out->source_key_count=out->frame_count*s->anim->bone_count;
    out->first_tick=0;out->last_tick=out->frame_count? (modelrt_i32)(out->frame_count-1u):0;
    out->ticks_per_second=rate;
    out->flags=MODELRT_ANIM_DENSE_FRAMES|MODELRT_ANIM_POSE_SAMPLER;
    out->end_time=(modelrt_q16)((out->frame_count/(modelrt_u32)rate)*65536u+
        ((out->frame_count%rate)*65536u)/rate);
    return MODELRT_OK;
}
static int ax89_texture_count(const modelrt_handle *h)
{
    const ax89_state *s = (const ax89_state *)h->native_state;
    return s && s->mesh ? (int)s->mesh->material_count:0;
}
static int ax89_texture_info(modelrt_handle *h,int i,modelrt_texture_info *out)
{
    ax89_state *s;
    size_t n;
    if (!h || !out || i<0) return MODELRT_ERR_BAD_ARG;
    s=(ax89_state *)h->native_state;
    if (!s || !s->mesh || (modelrt_u32)i>=s->mesh->material_count) return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)i;
    out->kind=MODELRT_TEXTURE_EXTERNAL;
    modelrt_copy_string(out->name,sizeof(out->name),s->mesh->materials[i].name);
    modelrt_copy_string(out->path,sizeof(out->path),s->mesh->materials[i].name);
    n=strlen(out->path);
    if (n>0u && n<sizeof(out->path)-5u && !strchr(out->path,'.')) strcat(out->path,".png");
    modelrt_copy_string(out->relative_path,sizeof(out->relative_path),out->path);
    return MODELRT_OK;
}
static int ax89_bind(modelrt_handle *target,modelrt_handle *source,modelrt_animation_binding *b)
{
    ax89_state *t, *s;
    ax89_bind_state *map;
    psk3d_result rc;
    t=(ax89_state *)target->native_state;s=(ax89_state *)source->native_state;
    if (!t || !s || !t->mesh || !s->anim || !t->cache) return MODELRT_ERR_UNSUPPORTED;
    map=(ax89_bind_state *)modelrt_binding_alloc(b,sizeof(*map),8u);
    if (!map) return MODELRT_ERR_NO_SPACE;
    rc=psk3d_build_bone_map(t->mesh,s->anim,PSK3D_BONEMAP_CASE_INSENSITIVE | PSK3D_BONEMAP_ALLOW_MISSING,&map->map);
    if (rc != PSK3D_OK) return MODELRT_ERR_CODEC;
    b->provider_state=map;
    b->info.source_track_count=s->anim->bone_count;
    b->info.matched_track_count=t->mesh->bone_count-map->map.missing_count;
    b->info.flags=1u;
    return MODELRT_OK;
}
static int ax89_visit_bound(modelrt_animation_binding *b,int index,modelrt_q16 time,
                            const modelrt_visit_sink *sink,void *user)
{
    ax89_state *t,*s;
    ax89_bind_state *bm;
    modelrt_animation_info info;
    modelrt_u32 frame;
    modelrt_q16 fraction;
    psk3d_result rc;
    if (!b || !b->target || !b->source || !b->provider_state) return MODELRT_ERR_BAD_ARG;
    t=(ax89_state *)b->target->native_state;s=(ax89_state *)b->source->native_state;
    bm=(ax89_bind_state *)b->provider_state;
    if (!t || !s || !t->mesh || !s->anim || index<0 || (modelrt_u32)index>=s->anim->sequence_count) return MODELRT_ERR_BAD_ARG;
    if (ax89_anim_info(b->source,index,&info)!=MODELRT_OK || info.ticks_per_second==0u) return MODELRT_ERR_CODEC;
    if (time<0) time=0;
    frame=(modelrt_u32)(time/65536)*info.ticks_per_second;
    frame+=(modelrt_u32)(((modelrt_u32)(time%65536)*info.ticks_per_second)/65536u);
    fraction=(modelrt_q16)(((modelrt_u32)(time%65536)*info.ticks_per_second)%65536u);
    if (info.frame_count==0u) return MODELRT_ERR_FORMAT;
    if (frame>=info.frame_count) frame%=info.frame_count;
    rc=psk3d_sample_sequence_mapped_scaled_ex(t->mesh,s->anim,&bm->map,(psk3d_u32)index,
        (psk3d_u32)frame,(psk3d_i32)fraction,PSK3D_ROT_SLERP,t->local_pose);
    if (rc!=PSK3D_OK) return MODELRT_ERR_CODEC;
    rc=psk3d_globalize_pose(t->mesh->bones,t->mesh->bone_count,t->local_pose,t->world_pose);
    if (rc!=PSK3D_OK) return MODELRT_ERR_CODEC;
    rc=psk3d_skin_points_lbs_cached(t->mesh,t->cache,t->world_pose,t->points);
    if (rc!=PSK3D_OK) return MODELRT_ERR_CODEC;
    if (t->mesh->normal_count > 0u) {
        rc=psk3d_skin_normals_lbs_cached(t->mesh,t->cache,t->world_pose,t->normals);
        if(rc!=PSK3D_OK) return MODELRT_ERR_CODEC;
    }
    return ax89_emit(b->target,t->points,t->mesh->normal_count ? t->normals : 0,
                     t->world_pose,sink,user);
}

const modelrt_provider modelrt_provider_pskpsa89={
 MODELRT_CODEC_ACTORX,"ActorX PSK/PSA v3",ax89_probe,ax89_open,ax89_close,
 ax89_visit,ax89_count,ax89_anim_info,ax89_texture_count,ax89_texture_info,
 0,0,ax89_bind,ax89_visit_bound,0
};

const psk3d_model *modelrt_native_psk(const modelrt_handle *handle)
{
    const ax89_state *s;
    if (!handle || !handle->is_open || handle->info.codec_id != MODELRT_CODEC_ACTORX) return 0;
    s=(const ax89_state *)handle->native_state;
    return s?s->mesh:0;
}
const psk3d_animset *modelrt_native_psa(const modelrt_handle *handle)
{
    const ax89_state *s;
    if (!handle || !handle->is_open || handle->info.codec_id != MODELRT_CODEC_ACTORX) return 0;
    s=(const ax89_state *)handle->native_state;
    return s?s->anim:0;
}
