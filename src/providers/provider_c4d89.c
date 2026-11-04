/* Cinema4D C4D6 native scene adapter, 5 modules all retained; C89 no heap. */
#include "../modelrt_internal.h"
#include "c4d89.h"
#include <string.h>

typedef struct c4drt_state_s {
    C4D89_MeshTable *meshes;
    C4D89_BitmapCatalog *bitmaps;
    modelrt_vec3 *points;
    modelrt_vec2 *uv;
    unsigned long point_capacity,uv_capacity;
} c4drt_state;

typedef struct c4drt_visit_s {
    c4drt_state *state;
    modelrt_handle *handle;
    const modelrt_visit_sink *sink;
    void *user;
    unsigned int mesh_index;
    modelrt_mesh_info info;
    int failed;
} c4drt_visit;

static int c4drt_probe(const char *path,const modelrt_u8 *head,size_t n)
{
    if(n>=8 && (!memcmp(head,"XC4DC4D6",8)||!memcmp(head,"QC4DC4D6",8)))return MODELRT_PROBE_YES;
    return modelrt_ext_eq(path,".c4d")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;
}
static int c4drt_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
    c4drt_state *s;int rc;unsigned int i;
    unsigned long maxv=0,maxuv=0;
    (void)rt;
    s=(c4drt_state*)modelrt_arena_alloc(h,sizeof(*s),8u);
    if(!s)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));
    s->meshes=(C4D89_MeshTable*)modelrt_arena_alloc(h,sizeof(C4D89_MeshTable),8u);
    s->bitmaps=(C4D89_BitmapCatalog*)modelrt_arena_alloc(h,sizeof(C4D89_BitmapCatalog),8u);
    if(!s->meshes||!s->bitmaps)return MODELRT_ERR_NO_SPACE;
    if(c4d89_probe(path)==0)return MODELRT_ERR_FORMAT;
    rc=c4d89_find_meshes(path,s->meshes);
    if(rc!=C4D89_OK)return MODELRT_ERR_CODEC;
    for(i=0;i<s->meshes->count;i++){
        const C4D89_MeshDesc *m=&s->meshes->meshes[i];
        if(m->vertex_count>maxv)maxv=m->vertex_count;
        if(m->uv_count>maxuv)maxuv=m->uv_count;
    }
    if(maxv){s->points=(modelrt_vec3*)modelrt_arena_alloc(h,(size_t)maxv*sizeof(modelrt_vec3),8u);if(!s->points)return MODELRT_ERR_NO_SPACE;}
    if(maxuv){s->uv=(modelrt_vec2*)modelrt_arena_alloc(h,(size_t)maxuv*sizeof(modelrt_vec2),8u);if(!s->uv)return MODELRT_ERR_NO_SPACE;}
    s->point_capacity=maxv;s->uv_capacity=maxuv;
    modelrt_zero(s->bitmaps,sizeof(*s->bitmaps));
    /* Paths recovered from typed bitmap parameters are references, NOT exact material bindings. */
    if(c4d89_scan_material_bitmaps(path,s->bitmaps)!=C4D89_OK)s->bitmaps->count=0u;
    h->native_state=s;
    h->info.mesh_count=s->meshes->count;
    h->info.material_count=0u; /* TextureTag->material BaseLink unresolved by vendor. */
    h->info.texture_count=s->bitmaps->count;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NATIVE_SCENE;
    if(s->bitmaps->count)h->info.capabilities|=MODELRT_CAP_EXTERNAL_TEX|MODELRT_CAP_TEXTURES;
    modelrt_copy_string(h->info.name,sizeof(h->info.name),path);
    return MODELRT_OK;
}
static int c4drt_begin(void *p,unsigned int i,const C4D89_MeshDesc *m)
{
    c4drt_visit *v=(c4drt_visit*)p;
    modelrt_zero(&v->info,sizeof(v->info));v->mesh_index=i;
    v->info.index=i;v->info.vertex_count=(modelrt_u32)m->vertex_count;
    v->info.triangle_count=(modelrt_u32)(m->triangle_count+m->quad_count*2ul);
    modelrt_copy_string(v->info.name,sizeof(v->info.name),"Cinema4D native polygon mesh");
    if(v->sink->on_mesh_begin&&v->sink->on_mesh_begin(v->user,&v->info)){v->failed=1;return 0;}
    return 1;
}
static int c4drt_vertex(void *p,unsigned long i,long x,long y,long z)
{
    c4drt_visit *v=(c4drt_visit*)p;
    if(i>=v->state->point_capacity)return 0;
    v->state->points[i].x=(modelrt_q16)x;
    v->state->points[i].y=(modelrt_q16)y;
    v->state->points[i].z=(modelrt_q16)z;
    return 1;
}
static int c4drt_uv(void *p,unsigned long i,long u,long vv)
{
    c4drt_visit *v=(c4drt_visit*)p;
    if(i>=v->state->uv_capacity)return 0;
    v->state->uv[i].x=(modelrt_q16)u;v->state->uv[i].y=(modelrt_q16)vv;
    return 1;
}
static int c4drt_face(void *p,unsigned long face_index,const C4D89_Face *f)
{
    c4drt_visit *v=(c4drt_visit*)p;
    modelrt_triangle_view tri;
    unsigned int a[4],corners,t,k,corner;
    unsigned long uv_idx;
    a[0]=f->a;a[1]=f->b;a[2]=f->c;a[3]=f->d;
    corners=f->corners;
    if(corners!=3u && corners!=4u)return 0;
    for(t=0;t<(corners==4u?2u:1u);t++){
        modelrt_zero(&tri,sizeof(tri));tri.mesh_index=v->mesh_index;tri.material_index=-1;
        for(k=0;k<3;k++){
            corner=t==0u?k:(k==0u?0u:k==1u?2u:3u);
            if(a[corner]>=v->state->point_capacity)return 0;
            uv_idx=face_index*4ul+corner;
            if(uv_idx>=v->state->uv_capacity)return 0;
            tri.v[k].position=v->state->points[a[corner]];
            tri.v[k].uv0=v->state->uv[uv_idx];
            tri.v[k].uv0.y=MODELRT_Q16_ONE-tri.v[k].uv0.y; /* one top-left -> GL UV flip */
            tri.v[k].flags=MODELRT_VTX_UV0;
            tri.v[k].rgba[0]=tri.v[k].rgba[1]=tri.v[k].rgba[2]=tri.v[k].rgba[3]=255u;
        }
        if(v->sink->on_triangle&&v->sink->on_triangle(v->user,&tri)){v->failed=1;return 0;}
    }
    return 1;
}
static int c4drt_end(void *p,unsigned int i)
{
    c4drt_visit *v=(c4drt_visit*)p;(void)i;
    if(v->sink->on_mesh_end&&v->sink->on_mesh_end(v->user,&v->info)){v->failed=1;return 0;}
    return 1;
}
static int c4drt_visit_scene(modelrt_handle *h,int animation,modelrt_q16 time,const modelrt_visit_sink *sink,void *user)
{
    c4drt_state *s=(c4drt_state*)h->native_state;C4D89_MeshSink callbacks;
    c4drt_visit v;int rc;unsigned int i;
    (void)time;
    if(!s||!sink)return MODELRT_ERR_BAD_ARG;
    if(animation>=0)return MODELRT_ERR_UNSUPPORTED; /* Vendor has scalar tracks, no full pose. */
    if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
    modelrt_zero(&v,sizeof(v));v.state=s;v.handle=h;v.sink=sink;v.user=user;
    modelrt_zero(&callbacks,sizeof(callbacks));callbacks.begin=c4drt_begin;callbacks.vertex=c4drt_vertex;
    callbacks.uv=c4drt_uv;callbacks.face=c4drt_face;callbacks.end=c4drt_end;
    for(i=0;i<s->meshes->count;i++){
        rc=c4d89_stream_mesh(h->path,i,&s->meshes->meshes[i],&callbacks,&v);
        if(rc!=C4D89_OK)return v.failed?MODELRT_ERR_CALLBACK:MODELRT_ERR_CODEC;
    }
    if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
static int c4drt_tex_count(const modelrt_handle *h){return (int)h->info.texture_count;}
static int c4drt_tex_info(modelrt_handle *h,int idx,modelrt_texture_info *out)
{
    c4drt_state *s=(c4drt_state*)h->native_state;
    if(!s||!out||idx<0||(unsigned)idx>=s->bitmaps->count)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)idx;out->kind=MODELRT_TEXTURE_EXTERNAL;
    modelrt_copy_string(out->name,sizeof(out->name),s->bitmaps->paths[idx].name);
    modelrt_copy_string(out->path,sizeof(out->path),s->bitmaps->paths[idx].name);
    modelrt_copy_string(out->relative_path,sizeof(out->relative_path),s->bitmaps->paths[idx].name);
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_c4d89={
 MODELRT_CODEC_CINEMA4D,"Cinema4DCodec89 v0.14",c4drt_probe,c4drt_open,0,
 c4drt_visit_scene,0,0,c4drt_tex_count,c4drt_tex_info,0,0,0,0,0
};
