/* Additional real native importers: Blitz3D, Quick3D and 3D Gamestudio HMB.
 * Original vendor parse functions + direct triangle visitor, no GACKT bridge. */
#include "../modelrt_internal.h"
#include "b3d_c89.h"
#include "q3d_codec.h"
#include "hmb3d.h"
#include <string.h>
#define W3_B3D 1
#define W3_Q3D 2
#define W3_HMB 3
typedef struct w3_state_s {int kind;void *native;} w3_state;
static int w3_probe(const char*p,const modelrt_u8*h,size_t n,int kind){
 if(kind==W3_B3D)return modelrt_ext_eq(p,".b3d")?(n>=4u&&memcmp(h,"BB3D",4u)==0?100:25):0;
 if(kind==W3_Q3D)return (modelrt_ext_eq(p,".q3o")||modelrt_ext_eq(p,".q3s"))?100:0;
 if(kind==W3_HMB)return (modelrt_ext_eq(p,".hmb")||modelrt_ext_eq(p,".hmp"))?100:0;
 return 0;
}
static int w3_b3d_probe(const char*p,const modelrt_u8*h,size_t n){return w3_probe(p,h,n,W3_B3D);}
static int w3_q3d_probe(const char*p,const modelrt_u8*h,size_t n){return w3_probe(p,h,n,W3_Q3D);}
static int w3_hmb_probe(const char*p,const modelrt_u8*h,size_t n){return w3_probe(p,h,n,W3_HMB);}
static int w3_open(modelrt_runtime*r,modelrt_handle*h,const char*p,int kind){
 modelrt_u8*bytes;size_t len;w3_state*s;int rc;(void)r;
 s=(w3_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));if(!s)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(s,sizeof(*s));s->kind=kind;
 rc=modelrt_read_file_arena(h,p,&bytes,&len);if(rc)return rc;
 if(len<4u||len>0xffffffffUL)return MODELRT_ERR_FORMAT;
 if(kind==W3_B3D){
  b3d_model*m=(b3d_model*)modelrt_arena_alloc(h,sizeof(b3d_model),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
  b3d_model_clear(m);rc=b3d_load_memory((const b3d_u8*)bytes,(b3d_u32)len,m);if(rc)return MODELRT_ERR_FORMAT;
  s->native=m;h->info.mesh_count=(modelrt_u32)m->mesh_count;h->info.material_count=(modelrt_u32)m->brush_count;
  h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NORMALS|MODELRT_CAP_NATIVE_SCENE;
 }else if(kind==W3_Q3D){
  Q3D_Model*m=(Q3D_Model*)modelrt_arena_alloc(h,sizeof(Q3D_Model),sizeof(void*));Q3D_Diag dg;if(!m)return MODELRT_ERR_NO_SPACE;
  q3d_clear_model(m);q3d_clear_diag(&dg);rc=q3d_parse_buffer(m,&dg,bytes,(unsigned long)len);if(rc)return MODELRT_ERR_FORMAT;
  s->native=m;h->info.mesh_count=(modelrt_u32)m->mesh_count;h->info.material_count=(modelrt_u32)m->material_count;
  h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_MATERIALS;
 }else if(kind==W3_HMB){
  hmb3d_model*m=(hmb3d_model*)modelrt_arena_alloc(h,sizeof(hmb3d_model),sizeof(void*));hmb3d_diag dg;if(!m)return MODELRT_ERR_NO_SPACE;
  hmb3d_model_init(m);hmb3d_diag_init(&dg);rc=hmb3d_load_memory((const hmb3d_u8*)bytes,(hmb3d_u32)len,m,&dg);if(rc)return MODELRT_ERR_FORMAT;
  s->native=m;h->info.mesh_count=(modelrt_u32)m->object_count;
  h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NATIVE_SCENE;
 }else return MODELRT_ERR_UNSUPPORTED;
 h->native_state=s;modelrt_copy_string(h->info.name,sizeof(h->info.name),p);return MODELRT_OK;
}
static int w3_b3d_open(modelrt_runtime*r,modelrt_handle*h,const char*p){return w3_open(r,h,p,W3_B3D);}
static int w3_q3d_open(modelrt_runtime*r,modelrt_handle*h,const char*p){return w3_open(r,h,p,W3_Q3D);}
static int w3_hmb_open(modelrt_runtime*r,modelrt_handle*h,const char*p){return w3_open(r,h,p,W3_HMB);}
static void w3_pos(modelrt_vertex_view *v,long x,long y,long z){v->position.x=(modelrt_q16)x;v->position.y=(modelrt_q16)y;v->position.z=(modelrt_q16)z;}
static int w3_visit(modelrt_handle*h,int anim,modelrt_q16 time,const modelrt_visit_sink*sink,void*u){
 w3_state*s;modelrt_mesh_info mi;modelrt_triangle_view tri;unsigned long i,j,idx[3],first,count;int n;
 (void)time;
 if(!h||!sink||!h->native_state)return MODELRT_ERR_BAD_ARG;
 if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 s=(w3_state*)h->native_state;
 if(sink->on_model_begin&&sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
 modelrt_zero(&mi,sizeof(mi));modelrt_copy_string(mi.name,sizeof(mi.name),"Native vendor geometry");
 if(sink->on_mesh_begin&&sink->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;
 if(s->kind==W3_B3D){
  const b3d_model*m=(const b3d_model*)s->native;
  for(i=0;i<(unsigned long)m->tri_count;i++){
   const b3d_tri*f=&m->tris[i];const b3d_mesh*me;
   if(f->mesh_id<0||f->mesh_id>=m->mesh_count)return MODELRT_ERR_FORMAT;
   me=&m->meshes[f->mesh_id];
   modelrt_zero(&tri,sizeof(tri));tri.material_index=f->brush_id;tri.mesh_index=(modelrt_u32)f->mesh_id;
   for(n=0;n<3;n++){
    long id=(long)me->vertex_first+(long)f->v[n];const b3d_vertex*v;
    if(f->v[n]<0||f->v[n]>=me->vertex_count||id<0||id>=m->vertex_count)return MODELRT_ERR_FORMAT;
    v=&m->vertices[id];w3_pos(&tri.v[n],v->x,v->y,v->z);
    tri.v[n].normal.x=v->nx;tri.v[n].normal.y=v->ny;tri.v[n].normal.z=v->nz;
    tri.v[n].uv0.x=v->uv[0][0];tri.v[n].uv0.y=v->uv[0][1];
    tri.v[n].flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0;
   }
   if(sink->on_triangle&&sink->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
  }
 }else if(s->kind==W3_Q3D){
  const Q3D_Model*m=(const Q3D_Model*)s->native;
  for(i=0;i<m->face_count;i++){
   const Q3D_Face*f=&m->faces[i];if(f->corner_count<3u)continue;
   for(j=1u;j+1u<(unsigned long)f->corner_count;j++){
    idx[0]=f->first_corner;idx[1]=f->first_corner+j;idx[2]=f->first_corner+j+1u;
    modelrt_zero(&tri,sizeof(tri));tri.material_index=(modelrt_i32)f->material;
    for(n=0;n<3;n++){
     const Q3D_Corner*c;const Q3D_Vec3*v;
     if(idx[n]>=m->corner_count)return MODELRT_ERR_FORMAT;
     c=&m->corners[idx[n]];if(c->vertex>=m->vertex_count)return MODELRT_ERR_FORMAT;
     v=&m->vertices[c->vertex];w3_pos(&tri.v[n],v->x,v->y,v->z);
     if(c->uv<m->uv_count){tri.v[n].uv0.x=m->uvs[c->uv].u;tri.v[n].uv0.y=m->uvs[c->uv].v;tri.v[n].flags=MODELRT_VTX_UV0;}
    }
    if(sink->on_triangle&&sink->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
   }
  }
 }else if(s->kind==W3_HMB){
  const hmb3d_model*m=(const hmb3d_model*)s->native;
  for(i=0;i<m->face_count;i++){
   const hmb3d_face*f=&m->faces[i];if(f->corner_count<3u)continue;
   first=(unsigned long)f->first_corner;count=(unsigned long)f->corner_count;
   for(j=1u;j+1u<count;j++){
    idx[0]=first;idx[1]=first+j;idx[2]=first+j+1u;
    modelrt_zero(&tri,sizeof(tri));tri.material_index=f->material_index;tri.mesh_index=f->object_index;
    for(n=0;n<3;n++){
     const hmb3d_corner*c;const hmb3d_vec3*v;
     if(idx[n]>=m->corner_count)return MODELRT_ERR_FORMAT;
     c=&m->corners[idx[n]];if(c->vertex>=m->vertex_count)return MODELRT_ERR_FORMAT;
     v=&m->vertices[c->vertex];w3_pos(&tri.v[n],v->x,v->y,v->z);
     if(c->uv<m->uv_count){tri.v[n].uv0.x=m->uvs[c->uv].u;tri.v[n].uv0.y=m->uvs[c->uv].v;tri.v[n].flags=MODELRT_VTX_UV0;}
    }
    if(sink->on_triangle&&sink->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
   }
  }
 }else return MODELRT_ERR_UNSUPPORTED;
 if(sink->on_mesh_end&&sink->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;
 if(sink->on_model_end&&sink->on_model_end(u,&h->info))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
#define W3_DEF(ID,NAME,PR,OP) {ID,NAME,PR,OP,0,w3_visit,0,0,0,0,0,0,0,0,0}
const modelrt_provider modelrt_provider_gackt_b3d89=W3_DEF(52u,"GACKT B3D native",w3_b3d_probe,w3_b3d_open);
const modelrt_provider modelrt_provider_gackt_q3d89=W3_DEF(53u,"GACKT Q3D native",w3_q3d_probe,w3_q3d_open);
const modelrt_provider modelrt_provider_gackt_hmb89=W3_DEF(54u,"GACKT HMB native",w3_hmb_probe,w3_hmb_open);
const void *modelrt_native_gackt_wave3_89(const modelrt_handle *h){
 const w3_state*s;if(!h||!h->is_open||h->info.codec_id<52u||h->info.codec_id>54u)return 0;
 s=(const w3_state*)h->native_state;return s?s->native:0;
}
