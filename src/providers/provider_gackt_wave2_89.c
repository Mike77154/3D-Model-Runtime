/* Native adapters for original GACKT decoder libraries; ISO C89, caller arena.
 * Each registered provider calls its own vendor API and streams real triangles.
 * No GACKT router, intermediate geometry file or malloc. */
#include "../modelrt_internal.h"
#include "ac3d_decoder.h"
#include "g3ds_fixed.h"
#include "lwo3d.h"
#include "gif_ogex_fixed.h"
#include "rawtri89.h"
#include "nff89.h"
#include <string.h>
#include <limits.h>
#define W2_AC 1
#define W2_3DS 2
#define W2_LWO 3
#define W2_OGX 4
#define W2_RAW 5
#define W2_NFF 6

typedef struct w2_state_s { int kind; void *native; modelrt_u8 *file; size_t len; unsigned long triangles; } w2_state;
typedef struct w2_emit_s { const modelrt_visit_sink *sink; void *user; int failed; unsigned long count; const ogx_model *ogx; } w2_emit;
static modelrt_q16 w2_q(long x) {
 if(x>2147483647L)return (modelrt_q16)2147483647L;
 if(x<(-2147483647L-1L))return (modelrt_q16)(-2147483647L-1L);
 return (modelrt_q16)x;
}
static void w2_pos(modelrt_vertex_view *v,long x,long y,long z){
 v->position.x=w2_q(x);v->position.y=w2_q(y);v->position.z=w2_q(z);
}
static int w2_emit_tri(w2_emit *e,modelrt_triangle_view *tri) {
 ++e->count;
 if(e->sink->on_triangle && e->sink->on_triangle(e->user,tri)) {e->failed=1;return 1;}
 return 0;
}
static int w2_ac_cb(const struct ac3d_triangle_ex *a,void *user){
 w2_emit *e=(w2_emit*)user;modelrt_triangle_view t;int j;modelrt_zero(&t,sizeof(t));
 t.material_index=a->material_index;t.mesh_index=0u;
 for(j=0;j<3;j++){
  w2_pos(&t.v[j],a->p[j].x,a->p[j].y,a->p[j].z);
  t.v[j].normal.x=w2_q(a->n[j].x);t.v[j].normal.y=w2_q(a->n[j].y);t.v[j].normal.z=w2_q(a->n[j].z);
  t.v[j].uv0.x=w2_q(a->uv[j].u);t.v[j].uv0.y=w2_q(a->uv[j].v);
  t.v[j].flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0;
 }
 return w2_emit_tri(e,&t);
}
static void w2_lwo_cb(void *user,const lwo3d_triangle *a){
 w2_emit *e=(w2_emit*)user;modelrt_triangle_view t;int j;
 if(e->failed)return;
 modelrt_zero(&t,sizeof(t));t.material_index=(modelrt_i32)a->material;
 for(j=0;j<3;j++){
  w2_pos(&t.v[j],a->p[j].x,a->p[j].y,a->p[j].z);
  t.v[j].normal.x=w2_q(a->raw_normal.x);t.v[j].normal.y=w2_q(a->raw_normal.y);t.v[j].normal.z=w2_q(a->raw_normal.z);
  t.v[j].flags=MODELRT_VTX_NORMAL;
  if(a->has_uv[j]){t.v[j].uv0.x=w2_q(a->uv[j].x);t.v[j].uv0.y=w2_q(a->uv[j].y);t.v[j].flags|=MODELRT_VTX_UV0;}
 }
 (void)w2_emit_tri(e,&t);
}
static int w2_nff_cb(void *user,const nff89_poly *poly,const nff89_vec3 *a,const nff89_vec3 *b,const nff89_vec3 *c){
 w2_emit *e=(w2_emit*)user;modelrt_triangle_view t;(void)poly;
 modelrt_zero(&t,sizeof(t));t.material_index=-1;
 w2_pos(&t.v[0],a->x,a->y,a->z);w2_pos(&t.v[1],b->x,b->y,b->z);w2_pos(&t.v[2],c->x,c->y,c->z);
 return w2_emit_tri(e,&t)?1:0;
}
static void w2_ogx_cb(void *user,const ogx_triangle *a){
 w2_emit *e=(w2_emit*)user;modelrt_triangle_view t;unsigned int ids[3];int j;
 if(e->failed)return;
 ids[0]=a->a;ids[1]=a->b;ids[2]=a->c;
 if(ids[0]>=e->ogx->vertex_count ||ids[1]>=e->ogx->vertex_count||ids[2]>=e->ogx->vertex_count){e->failed=1;return;}
 modelrt_zero(&t,sizeof(t));t.material_index=(modelrt_i32)a->material_index;
 for(j=0;j<3;j++){
  const ogx_vertex *v=&e->ogx->vertices[ids[j]];
  w2_pos(&t.v[j],v->pos[0],v->pos[1],v->pos[2]);
  if(v->flags&OGX_ATTR_NORMAL){t.v[j].normal.x=w2_q(v->normal[0]);t.v[j].normal.y=w2_q(v->normal[1]);t.v[j].normal.z=w2_q(v->normal[2]);t.v[j].flags|=MODELRT_VTX_NORMAL;}
  if(v->flags&OGX_ATTR_TEXCOORD){t.v[j].uv0.x=w2_q(v->texcoord[0]);t.v[j].uv0.y=w2_q(v->texcoord[1]);t.v[j].flags|=MODELRT_VTX_UV0;}
 }
 (void)w2_emit_tri(e,&t);
}
static int w2_probe(const char *p,const modelrt_u8 *h,size_t n,int kind){
 if(kind==W2_AC) return (modelrt_ext_eq(p,".ac")||modelrt_ext_eq(p,".ac3d"))?(n>=4u&&memcmp(h,"AC3D",4u)==0?100:25):0;
 if(kind==W2_3DS) return modelrt_ext_eq(p,".3ds")?(n>=2u&&h[0]==0x4du&&h[1]==0x4du?100:25):0;
 if(kind==W2_LWO) return modelrt_ext_eq(p,".lwo")?(n>=4u&&memcmp(h,"FORM",4u)==0?100:25):0;
 if(kind==W2_OGX) return modelrt_ext_eq(p,".ogex")?100:0;
 if(kind==W2_RAW) return modelrt_ext_eq(p,".raw")?100:0;
 if(kind==W2_NFF) return (modelrt_ext_eq(p,".nff")||modelrt_ext_eq(p,".enff"))?100:0;
 return 0;
}
static int w2_ac_probe(const char*p,const modelrt_u8*h,size_t n){return w2_probe(p,h,n,W2_AC);}
static int w2_3ds_probe(const char*p,const modelrt_u8*h,size_t n){return w2_probe(p,h,n,W2_3DS);}
static int w2_lwo_probe(const char*p,const modelrt_u8*h,size_t n){return w2_probe(p,h,n,W2_LWO);}
static int w2_ogx_probe(const char*p,const modelrt_u8*h,size_t n){return w2_probe(p,h,n,W2_OGX);}
static int w2_raw_probe(const char*p,const modelrt_u8*h,size_t n){return w2_probe(p,h,n,W2_RAW);}
static int w2_nff_probe(const char*p,const modelrt_u8*h,size_t n){return w2_probe(p,h,n,W2_NFF);}
static int w2_open(modelrt_runtime *rt,modelrt_handle *h,const char *path,int kind){
 w2_state *s;int rc;size_t extra;
 (void)rt;
 s=(w2_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));if(!s)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(s,sizeof(*s));s->kind=kind;
 rc=modelrt_read_file_arena(h,path,&s->file,&s->len);if(rc)return rc;
 if(s->len<4u||s->len>0xffffffffUL)return MODELRT_ERR_FORMAT;
 if(kind==W2_AC){
  struct ac3d_model *m=(struct ac3d_model*)modelrt_arena_alloc(h,sizeof(struct ac3d_model),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
  ac3d_model_init(m);rc=ac3d_decode_memory(m,(const char*)s->file,(unsigned long)s->len);if(rc)return MODELRT_ERR_FORMAT;
  if(ac3d_build_world_matrices(m))return MODELRT_ERR_FORMAT;
  s->native=m;h->info.mesh_count=(modelrt_u32)m->object_count;h->info.material_count=(modelrt_u32)m->material_count;
  h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NORMALS|MODELRT_CAP_NATIVE_SCENE;
 }else if(kind==W2_3DS){
  G3DS_Model *m=(G3DS_Model*)modelrt_arena_alloc(h,sizeof(G3DS_Model),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
  g3ds_model_init(m);rc=g3ds_load_memory((const G3DS_U8*)s->file,(G3DS_U32)s->len,m);if(rc)return MODELRT_ERR_FORMAT;
  s->native=m;h->info.mesh_count=m->object_count;h->info.material_count=m->material_count;
  h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_MATERIALS|MODELRT_CAP_UV0|MODELRT_CAP_NATIVE_SCENE;
 }else if(kind==W2_LWO){
  lwo3d_model *m=(lwo3d_model*)modelrt_arena_alloc(h,sizeof(lwo3d_model),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
  lwo3d_init(m);rc=lwo3d_decode_memory((const lwo3d_u8*)s->file,(lwo3d_u32)s->len,m);if(rc)return MODELRT_ERR_FORMAT;
  s->native=m;h->info.mesh_count=m->layer_count?m->layer_count:1u;h->info.material_count=m->surface_count;
  h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NORMALS|MODELRT_CAP_UV0|MODELRT_CAP_NATIVE_SCENE;
 }else if(kind==W2_OGX){
  ogx_model *m=(ogx_model*)modelrt_arena_alloc(h,sizeof(ogx_model),sizeof(void*));ogx_report rep;if(!m)return MODELRT_ERR_NO_SPACE;
  ogx_model_clear(m);ogx_report_clear(&rep);rc=ogx_decode_memory((const char*)s->file,(unsigned long)s->len,m,&rep);if(rc)return MODELRT_ERR_FORMAT;
  rc=ogx_resolve_references(m);if(rc)return MODELRT_ERR_FORMAT;
  s->native=m;h->info.mesh_count=m->mesh_count;h->info.material_count=m->material_count;
  h->info.capabilities=(m->mesh_count?MODELRT_CAP_GEOMETRY:0u)|MODELRT_CAP_UV0|MODELRT_CAP_NORMALS|MODELRT_CAP_NATIVE_SCENE;
 }else if(kind==W2_RAW){
  rw3d_mesh *m=(rw3d_mesh*)modelrt_arena_alloc(h,sizeof(rw3d_mesh),sizeof(void*));rw3d_arena a;rw3d_report rep;
  if(!m)return MODELRT_ERR_NO_SPACE;
  rw3d_mesh_zero(m);rw3d_report_zero(&rep);extra=0u;
  rc=rw3d_estimate_raw_arena((const char*)s->file,s->len,RW3D_RAW_DEFAULT_FLAGS,&extra,&rep);
  if(rc!=RW3D_OK)return MODELRT_ERR_FORMAT;
  if(extra==0u||extra>h->workspace_size-h->workspace_used)return MODELRT_ERR_NO_SPACE;
  {void* buf=modelrt_arena_alloc(h,extra,8u);if(!buf)return MODELRT_ERR_NO_SPACE;
   rw3d_arena_init(&a,buf,extra);
   rc=rw3d_parse_raw_memory((const char*)s->file,s->len,&a,RW3D_RAW_DEFAULT_FLAGS,m,&rep);}
  if(rc!=RW3D_OK)return MODELRT_ERR_FORMAT;
  s->native=m;h->info.mesh_count=m->triangle_count?1u:0u;h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NORMALS;
 }else if(kind==W2_NFF){
  nff89_scene *m=(nff89_scene*)modelrt_arena_alloc(h,sizeof(nff89_scene),sizeof(void*));
  size_t n=s->len/16u+16u;
  nff89_poly *polys; nff89_light *lights;nff89_sphere *spheres;nff89_cone *cones;
  nff89_bezier *beziers;nff89_nurbs *nurbs;nff89_object *objects;
  if(!m)return MODELRT_ERR_NO_SPACE;
  if(n>3000u)n=3000u;
  polys=(nff89_poly*)modelrt_arena_alloc(h,n*sizeof(nff89_poly),8u);
  lights=(nff89_light*)modelrt_arena_alloc(h,128u*sizeof(nff89_light),8u);
  spheres=(nff89_sphere*)modelrt_arena_alloc(h,128u*sizeof(nff89_sphere),8u);
  cones=(nff89_cone*)modelrt_arena_alloc(h,128u*sizeof(nff89_cone),8u);
  beziers=(nff89_bezier*)modelrt_arena_alloc(h,32u*sizeof(nff89_bezier),8u);
  nurbs=(nff89_nurbs*)modelrt_arena_alloc(h,16u*sizeof(nff89_nurbs),8u);
  objects=(nff89_object*)modelrt_arena_alloc(h,128u*sizeof(nff89_object),8u);
  if(!polys||!lights||!spheres||!cones||!beziers||!nurbs||!objects)return MODELRT_ERR_NO_SPACE;
  nff89_init_scene(m);nff89_set_buffers(m,lights,128u,spheres,128u,cones,128u,polys,(unsigned)n,beziers,32u,nurbs,16u,objects,128u);
  rc=nff89_parse(m,(const char*)s->file,(long)s->len);if(rc)return MODELRT_ERR_FORMAT;
  s->native=m;h->info.mesh_count=m->polys_count?1u:0u;h->info.capabilities=(m->polys_count?MODELRT_CAP_GEOMETRY:0u)|MODELRT_CAP_NATIVE_SCENE;
 }else return MODELRT_ERR_UNSUPPORTED;
 h->native_state=s;modelrt_copy_string(h->info.name,sizeof(h->info.name),path);return MODELRT_OK;
}
static int w2_ac_open(modelrt_runtime*r,modelrt_handle*h,const char*p){return w2_open(r,h,p,W2_AC);}
static int w2_3ds_open(modelrt_runtime*r,modelrt_handle*h,const char*p){return w2_open(r,h,p,W2_3DS);}
static int w2_lwo_open(modelrt_runtime*r,modelrt_handle*h,const char*p){return w2_open(r,h,p,W2_LWO);}
static int w2_ogx_open(modelrt_runtime*r,modelrt_handle*h,const char*p){return w2_open(r,h,p,W2_OGX);}
static int w2_raw_open(modelrt_runtime*r,modelrt_handle*h,const char*p){return w2_open(r,h,p,W2_RAW);}
static int w2_nff_open(modelrt_runtime*r,modelrt_handle*h,const char*p){return w2_open(r,h,p,W2_NFF);}
static int w2_visit(modelrt_handle*h,int anim,modelrt_q16 time,const modelrt_visit_sink*sink,void*user){
 w2_state *s;w2_emit e;modelrt_mesh_info mi;modelrt_triangle_view t;unsigned long i;int k,rc;
 (void)time;
 if(!h||!h->native_state||!sink)return MODELRT_ERR_BAD_ARG;
 if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 s=(w2_state*)h->native_state;e.sink=sink;e.user=user;e.failed=0;e.count=0u;e.ogx=0;
 if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
 modelrt_zero(&mi,sizeof(mi));mi.index=0u;modelrt_copy_string(mi.name,sizeof(mi.name),"Native GACKT codec");
 if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&mi))return MODELRT_ERR_CALLBACK;
 if(s->kind==W2_AC){rc=ac3d_for_each_triangle_ex((const struct ac3d_model*)s->native,0,w2_ac_cb,&e);if(rc&&!e.failed)return MODELRT_ERR_FORMAT;}
 else if(s->kind==W2_LWO){lwo3d_emit_triangles((const lwo3d_model*)s->native,w2_lwo_cb,&e);}
 else if(s->kind==W2_OGX){
  const ogx_model *m=(const ogx_model*)s->native;unsigned int idx;e.ogx=m;
  for(idx=0u;idx<m->mesh_count&&!e.failed;idx++){
   if(ogx_mesh_emit_triangles(m,idx,w2_ogx_cb,&e)!=OGX_OK)return MODELRT_ERR_FORMAT;
  }
 }else if(s->kind==W2_3DS){
  const G3DS_Model *m=(const G3DS_Model*)s->native;
  for(i=0;i<m->face_count;i++){
   const G3DS_Face *f=&m->faces[i];unsigned int ids[3];
   ids[0]=f->a;ids[1]=f->b;ids[2]=f->c;
   if(ids[0]>=m->vertex_count||ids[1]>=m->vertex_count||ids[2]>=m->vertex_count)return MODELRT_ERR_FORMAT;
   modelrt_zero(&t,sizeof(t));t.material_index=(modelrt_i32)f->material;
   for(k=0;k<3;k++){
    const G3DS_Vertex *v=&m->vertices[ids[k]];
    w2_pos(&t.v[k],v->x,v->y,v->z);
    if(v->has_uv){t.v[k].uv0.x=w2_q(v->u);t.v[k].uv0.y=w2_q(v->v);t.v[k].flags|=MODELRT_VTX_UV0;}
   }
   if(w2_emit_tri(&e,&t))break;
  }
 }else if(s->kind==W2_RAW){
  const rw3d_mesh *m=(const rw3d_mesh*)s->native;
  for(i=0;i<m->triangle_count;i++){
   const rw3d_tri *f=&m->triangles[i];unsigned long ids[3];ids[0]=f->a;ids[1]=f->b;ids[2]=f->c;
   if(ids[0]>=m->vertex_count||ids[1]>=m->vertex_count||ids[2]>=m->vertex_count)return MODELRT_ERR_FORMAT;
   modelrt_zero(&t,sizeof(t));t.material_index=-1;
   for(k=0;k<3;k++){const rw3d_vec3q *v=&m->vertices[ids[k]];
    w2_pos(&t.v[k],v->x,v->y,v->z);t.v[k].normal.x=w2_q(f->normal.x);t.v[k].normal.y=w2_q(f->normal.y);t.v[k].normal.z=w2_q(f->normal.z);t.v[k].flags=MODELRT_VTX_NORMAL;
   }if(w2_emit_tri(&e,&t))break;
  }
 }else if(s->kind==W2_NFF){
  rc=nff89_emit_triangles((const nff89_scene*)s->native,w2_nff_cb,&e);if(rc&&!e.failed)return MODELRT_ERR_FORMAT;
 }else return MODELRT_ERR_UNSUPPORTED;
 if(e.failed)return MODELRT_ERR_CALLBACK;
 if(sink->on_mesh_end&&sink->on_mesh_end(user,&mi))return MODELRT_ERR_CALLBACK;
 if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
#define W2_PROVIDER(ID,NAME,PROBE,OPEN) {ID,NAME,PROBE,OPEN,0,w2_visit,0,0,0,0,0,0,0,0,0}
const modelrt_provider modelrt_provider_gackt_ac3d89=W2_PROVIDER(46u,"GACKT AC3D native",w2_ac_probe,w2_ac_open);
const modelrt_provider modelrt_provider_gackt_3ds89=W2_PROVIDER(47u,"GACKT 3DS native",w2_3ds_probe,w2_3ds_open);
const modelrt_provider modelrt_provider_gackt_lwo89=W2_PROVIDER(48u,"GACKT LWO native",w2_lwo_probe,w2_lwo_open);
const modelrt_provider modelrt_provider_gackt_ogex89=W2_PROVIDER(49u,"GACKT OGEX native",w2_ogx_probe,w2_ogx_open);
const modelrt_provider modelrt_provider_gackt_raw89=W2_PROVIDER(50u,"GACKT RAW native",w2_raw_probe,w2_raw_open);
const modelrt_provider modelrt_provider_gackt_nff89=W2_PROVIDER(51u,"GACKT NFF native",w2_nff_probe,w2_nff_open);
/* Typed source structs remain reachable as const via this optional native API. */
const void *modelrt_native_gackt_wave2_89(const modelrt_handle *h){
 const w2_state *s;
 if(!h||!h->is_open||h->info.codec_id<46u||h->info.codec_id>51u)return 0;
 s=(const w2_state*)h->native_state;return s?s->native:0;
}
