/* Blender .blend Phase18 standalone C89 vendor -> native Runtime75 triangle sink.
 * This adapter is intentionally STATIC GEOMETRY ONLY: no misleading constraints
 * / animation frames or guessed UV/material bindings. No OBJ intermediary.
 * Workspace needs sizeof(BC_File) ~19.2 MiB. Compression deliberately OFF.
 */
#include "../modelrt_internal.h"
#include "bc_runtime_stream.h"
#include <string.h>

typedef struct blend_rt_state_s {
 BC_File *file;
 modelrt_u32 meshes, triangles, has_uv;
} blend_rt_state;
typedef struct blend_rt_visit_s {
 const modelrt_visit_sink *sink;
 void *user;
 int callback_error;
} blend_rt_visit;

static int blend_preflight(void *p,const BC_RT_Mesh *m){
 blend_rt_state *s=(blend_rt_state*)p;
 if(m->has_uv)s->has_uv=1u;
 return 0;
}
static int blend_probe(const char *path,const modelrt_u8 *head,size_t n){
 if(n>=7u && memcmp(head,"BLENDER",7u)==0)return 140;
 if(modelrt_ext_eq(path,".blend") && n>=4u){
  if(head[0]==0x1fu&&head[1]==0x8bu)return 140;
  if(head[0]==0x28u&&head[1]==0xb5u&&head[2]==0x2fu&&head[3]==0xfdu)return 140;
 }
 return modelrt_ext_eq(path,".blend")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;
}
static int blend_open(modelrt_runtime *rt,modelrt_handle *h,const char *path){
 blend_rt_state *st;BC_File *b;int rc;BC_RT_Sink pre;unsigned int meshes=0u,tris=0u;
 modelrt_u8 head[12];size_t n=0u;
 if(modelrt_read_file_head(path,head,sizeof(head),&n)!=MODELRT_OK)return MODELRT_ERR_IO;
 if(n>=4u && (n<7u || memcmp(head,"BLENDER",7u)!=0) &&
    ((head[0]==0x1fu&&head[1]==0x8bu)||
     (head[0]==0x28u&&head[1]==0xb5u&&head[2]==0x2fu&&head[3]==0xfdu))){
  modelrt_set_error(rt,"BLEND Phase18",MODELRT_ERR_UNSUPPORTED,
      "compressed .blend: requires external unpack; no bundled non-C89 libraries linked");
  return MODELRT_ERR_UNSUPPORTED;
 }
 st=(blend_rt_state*)modelrt_arena_alloc(h,sizeof(*st),sizeof(void*));
 if(!st)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(st,sizeof(*st));
 b=(BC_File*)modelrt_arena_alloc(h,sizeof(BC_File),sizeof(void*));
 if(!b){modelrt_set_error(rt,"BLEND Phase18",MODELRT_ERR_NO_SPACE,
    "provide >=20 MiB workspace for BLEND SDNA index");return MODELRT_ERR_NO_SPACE;}
 st->file=b;
 if(bc_open(b,path)!=BC_OK){bc_close(b);return MODELRT_ERR_FORMAT;}
 if(bc_scan(b)!=BC_OK||bc_dna_parse(b)!=BC_OK){
  bc_close(b);return MODELRT_ERR_FORMAT;
 }
 modelrt_zero(&pre,sizeof(pre));pre.begin=blend_preflight;
 rc=bc_rt_stream(b,&pre,st,&meshes,&tris);
 if(rc!=BC_OK){bc_close(b);return rc==BC_UNSUPPORTED?MODELRT_ERR_UNSUPPORTED:MODELRT_ERR_FORMAT;}
 st->meshes=(modelrt_u32)meshes;st->triangles=(modelrt_u32)tris;
 h->native_state=st;
 h->info.mesh_count=st->meshes;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE;
 if(st->has_uv)h->info.capabilities|=MODELRT_CAP_UV0;
 modelrt_copy_string(h->info.name,sizeof(h->info.name),path);
 return MODELRT_OK;
}
static void blend_close(modelrt_handle *h){
 blend_rt_state *s=(blend_rt_state*)h->native_state;
 if(s&&s->file)bc_close(s->file);
}
static int blend_begin(void *p,const BC_RT_Mesh *m){
 blend_rt_visit *v=(blend_rt_visit*)p;modelrt_mesh_info info;
 modelrt_zero(&info,sizeof(info));info.index=(modelrt_u32)m->index;
 info.vertex_count=(modelrt_u32)m->vertices;info.triangle_count=(modelrt_u32)m->triangles;
 modelrt_copy_string(info.name,sizeof(info.name),"Blender Mesh datablock");
 if(v->sink->on_mesh_begin&&v->sink->on_mesh_begin(v->user,&info)){
  v->callback_error=1;return 1;
 }return 0;
}
static int blend_triangle(void *p,const BC_RT_Tri *t){
 blend_rt_visit *v=(blend_rt_visit*)p;modelrt_triangle_view out;unsigned int i,j;
 modelrt_zero(&out,sizeof(out));out.mesh_index=(modelrt_u32)t->mesh_index;out.material_index=-1;
 for(i=0u;i<3u;i++){
  modelrt_vertex_view *vv=&out.v[i];
  vv->position.x=(modelrt_q16)t->v[i].pos[0];
  vv->position.y=(modelrt_q16)t->v[i].pos[1];
  vv->position.z=(modelrt_q16)t->v[i].pos[2];
  if(t->v[i].has_uv){
   vv->uv0.x=(modelrt_q16)t->v[i].uv[0];
   vv->uv0.y=(modelrt_q16)t->v[i].uv[1];
   vv->flags|=MODELRT_VTX_UV0;
  }
  for(j=0u;j<4u;j++)vv->rgba[j]=255u;
 }
 if(v->sink->on_triangle&&v->sink->on_triangle(v->user,&out)){
  v->callback_error=1;return 1;
 }return 0;
}
static int blend_end(void *p,const BC_RT_Mesh *m){
 blend_rt_visit *v=(blend_rt_visit*)p;modelrt_mesh_info info;
 modelrt_zero(&info,sizeof(info));info.index=(modelrt_u32)m->index;
 info.vertex_count=(modelrt_u32)m->vertices;info.triangle_count=(modelrt_u32)m->triangles;
 modelrt_copy_string(info.name,sizeof(info.name),"Blender Mesh datablock");
 if(v->sink->on_mesh_end&&v->sink->on_mesh_end(v->user,&info)){
  v->callback_error=1;return 1;
 }return 0;
}
static int blend_visit(modelrt_handle *h,int animation,modelrt_q16 time,
                       const modelrt_visit_sink *sink,void *user){
 blend_rt_state *s=(blend_rt_state*)h->native_state;
 blend_rt_visit context;BC_RT_Sink cb;unsigned int n=0u,t=0u;int rc;
 (void)time;
 if(!s||!sink)return MODELRT_ERR_BAD_ARG;
 if(animation!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
 modelrt_zero(&context,sizeof(context));context.sink=sink;context.user=user;
 cb.begin=blend_begin;cb.triangle=blend_triangle;cb.end=blend_end;
 rc=bc_rt_stream(s->file,&cb,&context,&n,&t);
 if(context.callback_error)return MODELRT_ERR_CALLBACK;
 if(rc!=BC_OK||n!=s->meshes||t!=s->triangles)return MODELRT_ERR_CODEC;
 if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
const modelrt_provider modelrt_provider_blend89={
 MODELRT_CODEC_BLEND,"BLEND Phase18 static SDNA",blend_probe,blend_open,blend_close,
 blend_visit,0,0,0,0,0,0,0,0,0
};
