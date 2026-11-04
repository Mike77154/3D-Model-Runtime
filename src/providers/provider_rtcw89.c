/* RTCW native MD3/MDC/MDS v19 bridge: C89, no malloc and no FP arithmetic.
 * The imported decoder keeps global scratch storage. Every visit reopens the
 * selected file, so interleaved handles work when calls are serialized. This
 * provider is NOT thread-safe; RTCW shaders/skins/PK3 are in the standalone
 * codec toolkit, not automatically wired to this geometry visitor.
 */
#include "../modelrt_internal.h"
#include "../../vendor/rtcw_natsuki_v19/rtcw_codec.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef struct rtcw_rt_state_s {
    modelrt_u32 frames;
    modelrt_u32 mesh_count;
    modelrt_u32 triangle_count;
    modelrt_u32 bone_count;
    modelrt_u32 kind;
} rtcw_rt_state;

static int rtcw_rt_probe(const char *path,const modelrt_u8 *head,size_t n)
{
    if(n<4u)return MODELRT_PROBE_NO;
    if(modelrt_ext_eq(path,".mds") && memcmp(head,"MDSW",4u)==0)return MODELRT_PROBE_YES;
    /* RTCW MDC v2 gets priority for exact native frames. MD3 preserves
       the established importer first, with RTCW as a fallback. Unsupported
       variants retain alternative providers for safe format fallback. */
    if(modelrt_ext_eq(path,".md3") && n>=8u && memcmp(head,"IDP3",4u)==0 && head[4]==15u && head[5]==0u && head[6]==0u && head[7]==0u)return MODELRT_PROBE_MAYBE;
    if(modelrt_ext_eq(path,".mdc") && n>=8u && memcmp(head,"IDPC",4u)==0 && head[4]==2u && head[5]==0u && head[6]==0u && head[7]==0u)return 110;
    return MODELRT_PROBE_NO;
}
static int rtcw_rt_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
    rtcw_rt_state *s;
    (void)rt;
    s=(rtcw_rt_state *)modelrt_arena_alloc(h,sizeof(*s),8u);
    if(!s)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));
    if(!rtcw_open(path))return MODELRT_ERR_FORMAT;
    if(!rtcw_decode(0UL))return MODELRT_ERR_FORMAT;
    if(rtcw_current.frames==0UL || rtcw_current.frames>2147483647UL)return MODELRT_ERR_LIMIT;
    s->frames=(modelrt_u32)rtcw_current.frames;
    s->mesh_count=(modelrt_u32)rtcw_current.surface_count;
    s->triangle_count=(modelrt_u32)rtcw_current.triangle_count;
    s->bone_count=(modelrt_u32)rtcw_current.bone_count;
    s->kind=rtcw_current.format[2]=='S'?3u:(rtcw_current.format[2]=='C'?2u:1u);
    h->native_state=s;
    h->info.mesh_count=s->mesh_count;
    h->info.material_count=s->mesh_count;
    h->info.texture_count=s->mesh_count;
    h->info.bone_count=s->bone_count;
    h->info.animation_count=1u;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|
        MODELRT_CAP_MATERIALS|MODELRT_CAP_ANIMATION|MODELRT_CAP_EXTERNAL_TEX;
    if(s->bone_count)h->info.capabilities|=MODELRT_CAP_SKELETON|MODELRT_CAP_SKINNING;
    modelrt_copy_string(h->info.name,sizeof(h->info.name),rtcw_current.name);
    return MODELRT_OK;
}
static void rtcw_rt_close(modelrt_handle *h){(void)h;}
static int rtcw_rt_q6(long v,modelrt_q16 *out)
{
    if(v>2097151L || v< -2097152L)return 0;
    *out=(modelrt_q16)(v*1024L);
    return 1;
}
static int rtcw_rt_uv(long v,modelrt_q16 *out)
{
    if(v>134217727L || v< -134217728L)return 0;
    *out=(modelrt_q16)(v*16L);
    return 1;
}
static int rtcw_rt_vertex(const rtcw_vertex *v,modelrt_vertex_view *dst)
{
    modelrt_zero(dst,sizeof(*dst));
    if(!rtcw_rt_q6(v->x,&dst->position.x) || !rtcw_rt_q6(v->y,&dst->position.y) ||
       !rtcw_rt_q6(v->z,&dst->position.z) || !rtcw_rt_uv(v->u,&dst->uv0.x) ||
       !rtcw_rt_uv(v->v,&dst->uv0.y))return 0;
    dst->flags=MODELRT_VTX_UV0;
    dst->rgba[0]=dst->rgba[1]=dst->rgba[2]=dst->rgba[3]=255u;
    return 1;
}
static int rtcw_rt_emit(modelrt_handle *h,modelrt_u32 frame,
                        const modelrt_visit_sink *sink,void *user)
{
    rtcw_rt_state *s=(rtcw_rt_state *)h->native_state;
    unsigned int i;
    unsigned long j;
    modelrt_model_info info;
    if(!s||frame>=s->frames||!sink)return MODELRT_ERR_BAD_ARG;
    /* No cached mutable global state shared between open handles. */
    if(!rtcw_open(h->path) || !rtcw_decode((unsigned long)frame))return MODELRT_ERR_FORMAT;
    if(rtcw_current.surface_count!=s->mesh_count || rtcw_current.bone_count!=s->bone_count)
        return MODELRT_ERR_FORMAT;
    info=h->info;
    if(sink->on_model_begin&&sink->on_model_begin(user,&info))return MODELRT_ERR_CALLBACK;
    for(i=0u;i<rtcw_current.bone_count;i++){
        const rtcw_bone *src=&rtcw_current.bones[i];
        modelrt_bone_view b;
        modelrt_zero(&b,sizeof(b));
        b.index=(modelrt_u32)i;b.parent=(modelrt_i32)src->parent;
        modelrt_copy_string(b.name,sizeof(b.name),src->name);
        /* RTCW stores joint distance but not a resolved world origin here. */
        if(sink->on_bone && sink->on_bone(user,&b))return MODELRT_ERR_CALLBACK;
    }
    for(i=0u;i<rtcw_current.surface_count;i++){
        const rtcw_surface *surf=&rtcw_current.surfaces[i];
        modelrt_mesh_info mesh;
        modelrt_material_view mat;
        modelrt_zero(&mesh,sizeof(mesh));
        mesh.index=(modelrt_u32)i;
        mesh.vertex_count=(modelrt_u32)surf->vertices;
        mesh.triangle_count=(modelrt_u32)surf->triangles;
        mesh.material_count=1u;
        modelrt_copy_string(mesh.name,sizeof(mesh.name),surf->name);
        modelrt_zero(&mat,sizeof(mat));
        mat.index=(modelrt_u32)i;
        modelrt_copy_string(mat.name,sizeof(mat.name),surf->shader);
        mat.texture_index=surf->shader[0]?(modelrt_i32)i:-1;
        mat.rgba[0]=mat.rgba[1]=mat.rgba[2]=mat.rgba[3]=255u;
        if(sink->on_material&&sink->on_material(user,&mat))return MODELRT_ERR_CALLBACK;
        if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&mesh))return MODELRT_ERR_CALLBACK;
        for(j=0UL;j<surf->triangles;j++){
            const rtcw_triangle *tr=&rtcw_current.tris[surf->first_triangle+j];
            modelrt_triangle_view tri;
            unsigned long idx[3];unsigned int k;
            idx[0]=tr->a;idx[1]=tr->b;idx[2]=tr->c;
            if(tr->surface!=(unsigned int)i)return MODELRT_ERR_FORMAT;
            modelrt_zero(&tri,sizeof(tri));
            tri.mesh_index=(modelrt_u32)i;tri.material_index=(modelrt_i32)i;
            for(k=0u;k<3u;k++){
                if(idx[k]<surf->first_vertex || idx[k]-surf->first_vertex>=surf->vertices ||
                   idx[k]>=rtcw_current.vertex_count ||
                   !rtcw_rt_vertex(&rtcw_current.verts[idx[k]],&tri.v[k]))return MODELRT_ERR_FORMAT;
            }
            if(sink->on_triangle&&sink->on_triangle(user,&tri))return MODELRT_ERR_CALLBACK;
        }
        if(sink->on_mesh_end&&sink->on_mesh_end(user,&mesh))return MODELRT_ERR_CALLBACK;
    }
    if(sink->on_model_end&&sink->on_model_end(user,&info))return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
static int rtcw_rt_visit(modelrt_handle *h,int animation_index,modelrt_q16 time,
                         const modelrt_visit_sink *sink,void *user)
{
    if(animation_index!=MODELRT_ANIM_BIND_POSE && (animation_index!=0 || time!=0))
        return MODELRT_ERR_UNSUPPORTED; /* No assumed playback FPS. */
    return rtcw_rt_emit(h,0u,sink,user);
}
static int rtcw_rt_anim_count(const modelrt_handle *h)
{
    return h->native_state?1:0;
}
static int rtcw_rt_anim_info(modelrt_handle *h,int index,modelrt_animation_info *out)
{
    rtcw_rt_state *s=(rtcw_rt_state *)h->native_state;
    if(!s || !out || index!=0)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=0u;
    modelrt_copy_string(out->name,sizeof(out->name),"RTCW_native_frames");
    out->frame_count=s->frames;out->source_key_count=s->frames;
    out->first_tick=0;out->last_tick=(modelrt_i32)(s->frames-1u);
    out->flags=MODELRT_ANIM_DENSE_FRAMES|MODELRT_ANIM_POSE_SAMPLER;
    return MODELRT_OK;
}
static int rtcw_rt_frame(modelrt_handle *h,int index,modelrt_u32 frame,
                         const modelrt_visit_sink *sink,void *user)
{
    if(index!=0)return MODELRT_ERR_BAD_ARG;
    return rtcw_rt_emit(h,frame,sink,user);
}
static int rtcw_rt_tex_count(const modelrt_handle *h)
{
    const rtcw_rt_state *s=(const rtcw_rt_state *)h->native_state;
    return s?(int)s->mesh_count:0;
}
static int rtcw_rt_tex_info(modelrt_handle *h,int index,modelrt_texture_info *out)
{
    rtcw_rt_state *s=(rtcw_rt_state *)h->native_state;
    if(!s || !out || index<0 || (modelrt_u32)index>=s->mesh_count)return MODELRT_ERR_BAD_ARG;
    if(!rtcw_open(h->path)||!rtcw_decode(0UL))return MODELRT_ERR_FORMAT;
    modelrt_zero(out,sizeof(*out));
    out->index=(modelrt_u32)index;
    out->kind=MODELRT_TEXTURE_EXTERNAL;
    modelrt_copy_string(out->name,sizeof(out->name),rtcw_current.surfaces[index].shader);
    modelrt_copy_string(out->path,sizeof(out->path),rtcw_current.surfaces[index].shader);
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_rtcw89={
 MODELRT_CODEC_RTCW,"RTCW v19 MDS/MDC/MD3 (native frames C89)",
 rtcw_rt_probe,rtcw_rt_open,rtcw_rt_close,rtcw_rt_visit,
 rtcw_rt_anim_count,rtcw_rt_anim_info,
 rtcw_rt_tex_count,rtcw_rt_tex_info,0,0,0,0,rtcw_rt_frame
};
