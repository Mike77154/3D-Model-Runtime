/* Quake II MD2 direct native vendor. Frame data remains in md2_model_t.
   Mesh visitor samples authored frames (no invented interpolation FPS). */
#include "../modelrt_internal.h"
#include "md2/md2_codec.h"
#include "md2/md2_runtime.h"
#include <string.h>
typedef struct modelrt_md2_state_s {
 md2_model_t *model;
 md2_runtime_mesh_t *mesh;
 md2_runtime_vertex_t *pose;
} modelrt_md2_state;
static int md2probe(const char*p,const modelrt_u8*h,size_t n){
 if(!modelrt_ext_eq(p,".md2"))return 0;
 return n>=8u&&h[0]=='I'&&h[1]=='D'&&h[2]=='P'&&h[3]=='2' ? MODELRT_PROBE_YES:MODELRT_PROBE_MAYBE;
}
static int md2open(modelrt_runtime *rt,modelrt_handle *h,const char *path){
 modelrt_md2_state*s;int rc;(void)rt;
 s=(modelrt_md2_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));if(!s)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(s,sizeof(*s));
 s->model=(md2_model_t*)modelrt_arena_alloc(h,sizeof(md2_model_t),sizeof(void*));
 s->mesh=(md2_runtime_mesh_t*)modelrt_arena_alloc(h,sizeof(md2_runtime_mesh_t),sizeof(void*));
 s->pose=(md2_runtime_vertex_t*)modelrt_arena_alloc(h,sizeof(md2_runtime_vertex_t)*(size_t)MD2_RUNTIME_MAX_VERTICES,sizeof(void*));
 if(!s->model||!s->mesh||!s->pose)return MODELRT_ERR_NO_SPACE;
 md2_model_init(s->model);
 rc=md2_decode_file(path,s->model);if(rc!=0)return MODELRT_ERR_FORMAT;
 md2_runtime_mesh_init(s->mesh);
 rc=md2_runtime_build_triangle_mesh(s->model,s->mesh);
 if(rc!=0)return MODELRT_ERR_CODEC;
 h->native_state=s;h->info.mesh_count=s->mesh->num_indices?1u:0u;
 h->info.texture_count=s->model->num_skins>0?(modelrt_u32)s->model->num_skins:0u;
 h->info.animation_count=s->model->num_frames>0?1u:0u;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0;
 if(h->info.texture_count)h->info.capabilities|=MODELRT_CAP_TEXTURES|MODELRT_CAP_EXTERNAL_TEX;
 if(h->info.animation_count)h->info.capabilities|=MODELRT_CAP_ANIMATION;
 modelrt_copy_string(h->info.name,sizeof(h->info.name),path);
 return MODELRT_OK;
}
static int md2visit_frame(modelrt_handle*h,modelrt_u32 frame,const modelrt_visit_sink *sink,void *user){
 modelrt_md2_state*s;modelrt_mesh_info mi;modelrt_u32 i;
 if(!h||!h->native_state||!sink)return MODELRT_ERR_BAD_ARG;
 s=(modelrt_md2_state*)h->native_state;
 if(frame>=(modelrt_u32)s->model->num_frames)return MODELRT_ERR_BAD_ARG;
 if(md2_runtime_pose_frame(s->model,s->mesh,(md2_i32)frame,s->pose)!=0)return MODELRT_ERR_CODEC;
 if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
 if(h->info.mesh_count){
  modelrt_zero(&mi,sizeof(mi));mi.index=0u;mi.vertex_count=(modelrt_u32)s->mesh->num_vertices;mi.triangle_count=(modelrt_u32)(s->mesh->num_indices/3);
  modelrt_copy_string(mi.name,sizeof(mi.name),"Quake II MD2");
  if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&mi))return MODELRT_ERR_CALLBACK;
  for(i=0u;i+2u<(modelrt_u32)s->mesh->num_indices;i+=3u){
   modelrt_triangle_view tv;int k;modelrt_zero(&tv,sizeof(tv));tv.material_index=-1;
   for(k=0;k<3;k++){
    md2_u16 idx=s->mesh->indices[i+(modelrt_u32)k];md2_runtime_vertex_t *v;
    if(idx>=(md2_u16)s->mesh->num_vertices)return MODELRT_ERR_FORMAT;
    v=&s->pose[idx];tv.v[k].position.x=v->x;tv.v[k].position.y=v->y;tv.v[k].position.z=v->z;
    tv.v[k].uv0.x=v->u;tv.v[k].uv0.y=v->v;tv.v[k].flags=MODELRT_VTX_UV0;
   }
   if(sink->on_triangle&&sink->on_triangle(user,&tv))return MODELRT_ERR_CALLBACK;
  }
  if(sink->on_mesh_end&&sink->on_mesh_end(user,&mi))return MODELRT_ERR_CALLBACK;
 }
 if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
static int md2visit(modelrt_handle*h,int anim,modelrt_q16 t,const modelrt_visit_sink*sink,void*user){
 (void)t;
 if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 return md2visit_frame(h,0u,sink,user);
}
static int md2frame(modelrt_handle*h,int anim,modelrt_u32 frame,const modelrt_visit_sink*sink,void*user){
 if(anim!=0)return MODELRT_ERR_BAD_ARG;
 return md2visit_frame(h,frame,sink,user);
}
static int md2count(const modelrt_handle*h){return (int)h->info.animation_count;}
static int md2info(modelrt_handle*h,int idx,modelrt_animation_info*out){
 modelrt_md2_state*s;if(!h||!out||idx!=0||!h->native_state)return MODELRT_ERR_BAD_ARG;
 s=(modelrt_md2_state*)h->native_state;
 modelrt_zero(out,sizeof(*out));out->index=0u;
 modelrt_copy_string(out->name,sizeof(out->name),"MD2 vertex frames");
 out->frame_count=(modelrt_u32)s->model->num_frames;
 out->first_tick=0;out->last_tick=out->frame_count?out->frame_count-1u:0u;
 out->flags=MODELRT_ANIM_DENSE_FRAMES;
 return MODELRT_OK;
}
static int md2texcount(const modelrt_handle*h){return (int)h->info.texture_count;}
static int md2texinfo(modelrt_handle*h,int idx,modelrt_texture_info*out){
 modelrt_md2_state*s;if(!h||!out||!h->native_state||idx<0||idx>=(int)h->info.texture_count)return MODELRT_ERR_BAD_ARG;
 s=(modelrt_md2_state*)h->native_state;modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)idx;out->kind=MODELRT_TEXTURE_EXTERNAL;
 modelrt_copy_string(out->name,sizeof(out->name),s->model->skins[idx]);
 modelrt_copy_string(out->path,sizeof(out->path),s->model->skins[idx]);
 modelrt_copy_string(out->relative_path,sizeof(out->relative_path),s->model->skins[idx]);return MODELRT_OK;
}
const modelrt_provider modelrt_provider_gackt_md289={MODELRT_CODEC_GACKT_MD2,"GACKT Quake2 MD2 (native)",md2probe,md2open,0,md2visit,md2count,md2info,md2texcount,md2texinfo,0,0,0,0,md2frame};

const md2_model_t *modelrt_native_md289(const modelrt_handle *h){
 const modelrt_md2_state *s;
 if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_GACKT_MD2)return 0;
 s=(const modelrt_md2_state*)h->native_state;return s?s->model:0;
}
