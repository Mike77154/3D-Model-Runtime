/* Quake III MD3 native surfaces and exact vertex frames (C89). */
#include "../modelrt_internal.h"
#include "md3/md3.h"
#include <string.h>
#include <stdio.h>
typedef struct md3_rt_state_s {
    FILE *file;
    md3_decoder_t decoder;
    md3_triangle_t *tris;
    md3_st_t *st;
    md3_vertex_t *vertices;
} md3_rt_state;
static int md3_probe(const char *p,const modelrt_u8 *h,size_t n)
{
    if(!modelrt_ext_eq(p,".md3"))return MODELRT_PROBE_NO;
    return (n>=8u && memcmp(h,"IDP3",4u)==0 && h[4]==15u &&
        h[5]==0u && h[6]==0u && h[7]==0u)?MODELRT_PROBE_YES:MODELRT_PROBE_MAYBE;
}
static int md3_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
    md3_rt_state *s;int rc;int i;(void)rt;
    s=(md3_rt_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
    if(!s)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));
    s->file=fopen(path,"rb");if(!s->file)return MODELRT_ERR_IO;
    rc=md3_decoder_open(&s->decoder,s->file);
    if(rc!=MD3_OK){fclose(s->file);s->file=0;return MODELRT_ERR_FORMAT;}
    s->tris=(md3_triangle_t*)modelrt_arena_alloc(h,sizeof(md3_triangle_t)*MD3_MAX_TRIANGLES,sizeof(void*));
    s->st=(md3_st_t*)modelrt_arena_alloc(h,sizeof(md3_st_t)*MD3_MAX_VERTS,sizeof(void*));
    s->vertices=(md3_vertex_t*)modelrt_arena_alloc(h,sizeof(md3_vertex_t)*MD3_MAX_VERTS,sizeof(void*));
    if(!s->tris||!s->st||!s->vertices){fclose(s->file);s->file=0;return MODELRT_ERR_NO_SPACE;}
    h->native_state=s;
    h->info.mesh_count=(modelrt_u32)s->decoder.header.num_surfaces;
    h->info.animation_count=s->decoder.header.num_frames>0?1u:0u;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NORMALS|MODELRT_CAP_NATIVE_SCENE;
    if(h->info.animation_count)h->info.capabilities|=MODELRT_CAP_ANIMATION;
    /* MD3 surfaces contain named shader slots; preserve them natively. */
    for(i=0;i<s->decoder.header.num_surfaces;i++)
        if(s->decoder.surfaces[i].num_shaders>0){h->info.capabilities|=MODELRT_CAP_MATERIALS;break;}
    modelrt_copy_string(h->info.name,sizeof(h->info.name),path);
    return MODELRT_OK;
}
static void md3_close(modelrt_handle*h)
{
    md3_rt_state*s;
    if(!h||!h->native_state)return;
    s=(md3_rt_state*)h->native_state;
    if(s->file){fclose(s->file);s->file=0;}
    md3_decoder_close(&s->decoder);
}
static int md3_visit_frame(modelrt_handle*h,modelrt_u32 frame,const modelrt_visit_sink *sink,void *user)
{
    md3_rt_state*s;int i,j,k;modelrt_mesh_info mi;
    if(!h||!h->native_state||!sink)return MODELRT_ERR_BAD_ARG;
    s=(md3_rt_state*)h->native_state;
    if(frame>=(modelrt_u32)s->decoder.header.num_frames)return MODELRT_ERR_BAD_ARG;
    if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
    for(i=0;i<s->decoder.header.num_surfaces;i++){
        const md3_surface_info_t*surf=&s->decoder.surfaces[i];
        if(surf->num_verts<=0 || surf->num_verts>MD3_MAX_VERTS ||
           surf->num_triangles<0 || surf->num_triangles>MD3_MAX_TRIANGLES)return MODELRT_ERR_LIMIT;
        if(md3_decoder_read_surface_triangles(&s->decoder,i,s->tris,surf->num_triangles)!=MD3_OK ||
           md3_decoder_read_surface_st(&s->decoder,i,s->st,surf->num_verts)!=MD3_OK ||
           md3_decoder_read_surface_vertices_for_frame(&s->decoder,i,(md3_i32)frame,s->vertices,surf->num_verts)!=MD3_OK)
            return MODELRT_ERR_CODEC;
        modelrt_zero(&mi,sizeof(mi));mi.index=(modelrt_u32)i;
        mi.vertex_count=(modelrt_u32)surf->num_verts;mi.triangle_count=(modelrt_u32)surf->num_triangles;
        modelrt_copy_string(mi.name,sizeof(mi.name),surf->name);
        if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&mi))return MODELRT_ERR_CALLBACK;
        for(j=0;j<surf->num_triangles;j++){
            modelrt_triangle_view tri;
            modelrt_zero(&tri,sizeof(tri));tri.mesh_index=(modelrt_u32)i;tri.material_index=-1;
            for(k=0;k<3;k++){
                int vi=s->tris[j].index[k];
                const md3_vertex_t *v;
                if(vi<0||vi>=surf->num_verts)return MODELRT_ERR_FORMAT;
                v=&s->vertices[vi];
                tri.v[k].position.x=v->position.x;
                tri.v[k].position.y=v->position.y;
                tri.v[k].position.z=v->position.z;
                tri.v[k].normal.x=v->normal.x;
                tri.v[k].normal.y=v->normal.y;
                tri.v[k].normal.z=v->normal.z;
                tri.v[k].uv0.x=s->st[vi].s;tri.v[k].uv0.y=s->st[vi].t;
                tri.v[k].flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0;
            }
            if(sink->on_triangle&&sink->on_triangle(user,&tri))return MODELRT_ERR_CALLBACK;
        }
        if(sink->on_mesh_end&&sink->on_mesh_end(user,&mi))return MODELRT_ERR_CALLBACK;
    }
    if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
static int md3_visit(modelrt_handle*h,int anim,modelrt_q16 time,const modelrt_visit_sink*s,void*u)
{(void)time;if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;return md3_visit_frame(h,0u,s,u);}
static int md3_count(const modelrt_handle*h){return (int)h->info.animation_count;}
static int md3_anim_info(modelrt_handle*h,int index,modelrt_animation_info*out)
{
    md3_rt_state*s;if(!h||!h->native_state||!out||index!=0)return MODELRT_ERR_BAD_ARG;
    s=(md3_rt_state*)h->native_state;
    modelrt_zero(out,sizeof(*out));out->index=0u;
    out->frame_count=(modelrt_u32)s->decoder.header.num_frames;
    out->last_tick=(modelrt_i32)out->frame_count-1;
    out->flags=MODELRT_ANIM_DENSE_FRAMES;
    modelrt_copy_string(out->name,sizeof(out->name),"MD3 authored vertex frames");
    return MODELRT_OK;
}
static int md3_frame(modelrt_handle*h,int index,modelrt_u32 frame,const modelrt_visit_sink*s,void*u)
{if(index!=0)return MODELRT_ERR_BAD_ARG;return md3_visit_frame(h,frame,s,u);}
const modelrt_provider modelrt_provider_native_md389={
    56u,"Quake III MD3 native",md3_probe,md3_open,md3_close,md3_visit,
    md3_count,md3_anim_info,0,0,0,0,0,0,md3_frame
};
const void *modelrt_native_md389(const modelrt_handle *h)
{
    const md3_rt_state*s;
    if(!h||!h->is_open||h->info.codec_id!=56u)return 0;
    s=(const md3_rt_state*)h->native_state;return s?&s->decoder:0;
}
