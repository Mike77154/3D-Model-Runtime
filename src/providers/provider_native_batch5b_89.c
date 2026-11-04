/* Native Batch 05B: TER, NDO, ASK, DBO, AMJ.
 * No GACKT runtime, no cooked intermediary, no synthetic success.
 */
#include "../modelrt_internal.h"
#include "ter3d.h"
#include "ndo3d.h"
#include "ask3d_scene.h"
#include "ask3d_parse.h"
#include "dbo.h"
#include "amj3d.h"
#include <string.h>
#include <limits.h>

typedef struct b5b_s {void*native;void*other;modelrt_u8*data;size_t size;} b5b;
#define P(NAME,EXT) static int NAME(const char*p,const modelrt_u8*h,size_t n){(void)h;(void)n;return modelrt_ext_eq(p,EXT)?MODELRT_PROBE_MAYBE:0;}
P(ter_probe,".ter") P(ndo_probe,".ndo") P(ask_probe,".ask") P(dbo_probe,".dbo") P(amj_probe,".amj")
static int b5b_load(modelrt_handle*h,const char*p,b5b**out){
 b5b*s;int rc;s=(b5b*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));if(!s)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(s,sizeof(*s));rc=modelrt_read_file_arena(h,p,&s->data,&s->size);if(rc)return rc;
 if(s->size<4u||s->size>0x7fffffffUL)return MODELRT_ERR_FORMAT;
 h->native_state=s;*out=s;return MODELRT_OK;
}
static int b5b_begin(modelrt_handle*h,const modelrt_visit_sink*s,void*u){
 modelrt_mesh_info m;modelrt_zero(&m,sizeof(m));modelrt_copy_string(m.name,sizeof(m.name),"Native source mesh");
 if(s->on_model_begin&&s->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
 if(s->on_mesh_begin&&s->on_mesh_begin(u,&m))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
static int b5b_end(modelrt_handle*h,const modelrt_visit_sink*s,void*u){
 modelrt_mesh_info m;modelrt_zero(&m,sizeof(m));modelrt_copy_string(m.name,sizeof(m.name),"Native source mesh");
 if(s->on_mesh_end&&s->on_mesh_end(u,&m))return MODELRT_ERR_CALLBACK;
 if(s->on_model_end&&s->on_model_end(u,&h->info))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
static int ter_open(modelrt_runtime*r,modelrt_handle*h,const char*p){
 b5b*s;ter3d_info*m;int rc;(void)r;rc=b5b_load(h,p,&s);if(rc)return rc;
 m=(ter3d_info*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(m,sizeof(*m));if(ter3d_read_info(s->data,(ter3d_u32)s->size,m)!=TER3D_OK)return MODELRT_ERR_FORMAT;
 if(m->family!=TER3D_FAMILY_TERRAGEN||!m->terragen.has_altw||
    m->terragen.xpts<2u||m->terragen.ypts<2u)return MODELRT_ERR_UNSUPPORTED;
 s->native=m;h->info.mesh_count=1u;h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE;
 return MODELRT_OK;
}
static int ter_visit(modelrt_handle*h,int a,modelrt_q16 t,const modelrt_visit_sink*s,void*u){
 b5b*st=(b5b*)h->native_state;const ter3d_terragen_info*m=&((ter3d_info*)st->native)->terragen;
 ter3d_u16 x,y;int j,rc;modelrt_triangle_view tri;ter3d_fixed z[4];(void)t;
 if(a!=-1)return MODELRT_ERR_UNSUPPORTED;rc=b5b_begin(h,s,u);if(rc)return rc;
 for(y=0;y+1u<m->ypts;y++)for(x=0;x+1u<m->xpts;x++){
  if(ter3d_terragen_get_height_q16(st->data,(ter3d_u32)st->size,m,x,y,&z[0])!=TER3D_OK||
     ter3d_terragen_get_height_q16(st->data,(ter3d_u32)st->size,m,x+1u,y,&z[1])!=TER3D_OK||
     ter3d_terragen_get_height_q16(st->data,(ter3d_u32)st->size,m,x,y+1u,&z[2])!=TER3D_OK||
     ter3d_terragen_get_height_q16(st->data,(ter3d_u32)st->size,m,x+1u,y+1u,&z[3])!=TER3D_OK)return MODELRT_ERR_FORMAT;
  for(j=0;j<2;j++){
   int idx[3],k;idx[0]=0;idx[1]=j?3:1;idx[2]=j?2:3;
   modelrt_zero(&tri,sizeof(tri));
   for(k=0;k<3;k++){
    int i=idx[k];tri.v[k].position.x=((modelrt_q16)(x+(i&1)))<<16;
    tri.v[k].position.y=((modelrt_q16)(y+(i>>1)))<<16;
    tri.v[k].position.z=(modelrt_q16)z[i];
   }
   if(s->on_triangle&&s->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
  }
 }
 return b5b_end(h,s,u);
}
static int ndo_open(modelrt_runtime*r,modelrt_handle*h,const char*p){
 b5b*s;NDO3D_Model*m;NDO3D_Diag dg;int rc;(void)r;rc=b5b_load(h,p,&s);if(rc)return rc;
 m=(NDO3D_Model*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(m,sizeof(*m));modelrt_zero(&dg,sizeof(dg));
 if(ndo3d_decode(m,s->data,(unsigned long)s->size,&dg)!=NDO3D_OK||m->object_count==0u)return MODELRT_ERR_FORMAT;
 s->native=m;h->info.mesh_count=m->object_count;h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE;
 return MODELRT_OK;
}
static int ndo_visit(modelrt_handle*h,int a,modelrt_q16 t,const modelrt_visit_sink*s,void*u){
 b5b*st=(b5b*)h->native_state;NDO3D_Model*m=(NDO3D_Model*)st->native;
 unsigned int i,j,k,c,n;ndo3d_u16 indices[1024];NDO3D_Diag dg;int rc;modelrt_triangle_view tri;(void)t;
 if(a!=-1)return MODELRT_ERR_UNSUPPORTED;rc=b5b_begin(h,s,u);if(rc)return rc;
 for(i=0;i<m->object_count;i++){
  NDO3D_Object*o=&m->objects[i];
  for(j=0;j<o->face_count;j++){
   modelrt_zero(&dg,sizeof(dg));if(ndo3d_face_vertices(o,j,indices,1024u,&c,&dg)!=NDO3D_OK)return MODELRT_ERR_FORMAT;
   for(k=1u;k+1u<c;k++){
    unsigned int vi[3];vi[0]=indices[0];vi[1]=indices[k];vi[2]=indices[k+1u];modelrt_zero(&tri,sizeof(tri));tri.mesh_index=i;
    for(n=0;n<3;n++){
     const NDO3D_Vertex*v;if(vi[n]>=o->vertex_count)return MODELRT_ERR_FORMAT;
     v=&o->vertices[vi[n]];tri.v[n].position.x=(modelrt_q16)v->x;
     tri.v[n].position.y=(modelrt_q16)v->y;tri.v[n].position.z=(modelrt_q16)v->z;
    }
    if(s->on_triangle&&s->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
   }
  }
 }
 return b5b_end(h,s,u);
}
static int ask_open(modelrt_runtime*r,modelrt_handle*h,const char*p){
 b5b*s;Ask3dScene*m;Ask3dWork*w;int rc;(void)r;rc=b5b_load(h,p,&s);if(rc)return rc;
 m=(Ask3dScene*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));w=(Ask3dWork*)modelrt_arena_alloc(h,sizeof(*w),sizeof(void*));
 if(!m||!w)return MODELRT_ERR_NO_SPACE;ask3d_scene_clear(m);ask3d_work_clear(w);
 if(ask3d_parse_memory(m,w,(const char*)s->data,(unsigned long)s->size)!=ASK3D_OK||m->face_count==0u||m->overflowed)return MODELRT_ERR_FORMAT;
 s->native=m;h->info.mesh_count=m->object_count;h->info.material_count=m->material_count;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_MATERIALS|MODELRT_CAP_NATIVE_SCENE;
 return MODELRT_OK;
}
static int ask_visit(modelrt_handle*h,int a,modelrt_q16 t,const modelrt_visit_sink*s,void*u){
 b5b*st=(b5b*)h->native_state;Ask3dScene*m=(Ask3dScene*)st->native;unsigned int i;int j,rc;modelrt_triangle_view tri;(void)t;
 if(a!=-1)return MODELRT_ERR_UNSUPPORTED;rc=b5b_begin(h,s,u);if(rc)return rc;
 for(i=0;i<m->face_count;i++){
  const Ask3dFace*f=&m->faces[i];modelrt_zero(&tri,sizeof(tri));tri.material_index=f->material_id;
  for(j=0;j<3;j++){
   unsigned int ix=f->v[j];const Ask3dVec3*v;
   if(ix>=m->vertex_count)return MODELRT_ERR_FORMAT;
   v=&m->vertices[ix];tri.v[j].position.x=v->x;tri.v[j].position.y=v->y;tri.v[j].position.z=v->z;
   if(f->has_tv){if(f->tv[j]>=m->tvertex_count)return MODELRT_ERR_FORMAT;
    tri.v[j].uv0.x=m->tvertices[f->tv[j]].u;tri.v[j].uv0.y=m->tvertices[f->tv[j]].v;tri.v[j].flags|=MODELRT_VTX_UV0;}
  }
  if(s->on_triangle&&s->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
 }
 return b5b_end(h,s,u);
}
static int dbo_open(modelrt_runtime*r,modelrt_handle*h,const char*p){
 b5b*s;dbo_scene*m;int rc;(void)r;rc=b5b_load(h,p,&s);if(rc)return rc;
 m=(dbo_scene*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(m,sizeof(*m));if(!dbo_parse_memory(m,s->data,(dbo_u32)s->size)||m->mesh_count==0u)return MODELRT_ERR_FORMAT;
 s->native=m;h->info.mesh_count=m->mesh_count;h->info.material_count=m->mesh_count;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE|MODELRT_CAP_UV0;
 return MODELRT_OK;
}
static int dbo_visit(modelrt_handle*h,int a,modelrt_q16 t,const modelrt_visit_sink*s,void*u){
 b5b*st=(b5b*)h->native_state;dbo_scene*m=(dbo_scene*)st->native;dbo_u32 i,j;int k,rc;modelrt_triangle_view tri;(void)t;
 if(a!=-1)return MODELRT_ERR_UNSUPPORTED;rc=b5b_begin(h,s,u);if(rc)return rc;
 for(i=0;i<m->mesh_count;i++){
  const dbo_mesh*me=&m->meshes[i];dbo_u32 ct=me->index_count?me->index_count:me->vertex_count;
  if(ct%3u)return MODELRT_ERR_UNSUPPORTED;
  if(me->index_count && (me->index_data.offset>m->blob_used||me->index_data.size>m->blob_used-me->index_data.offset))return MODELRT_ERR_FORMAT;
  for(j=0;j<ct;j+=3u){
   modelrt_zero(&tri,sizeof(tri));tri.mesh_index=i;tri.material_index=i;
   for(k=0;k<3;k++){
    dbo_u32 ix=j+(dbo_u32)k;dbo_fx32 xyz[3];dbo_fx32 uv[4];
    if(me->index_count){
     const dbo_u8*bb=m->blob+me->index_data.offset;
     if(me->index_data.size>=ct*4u){size_t q=(size_t)ix*4u;
      ix=((dbo_u32)bb[q])|((dbo_u32)bb[q+1u]<<8)|((dbo_u32)bb[q+2u]<<16)|((dbo_u32)bb[q+3u]<<24);
     }else if(me->index_data.size>=ct*2u){size_t q=(size_t)ix*2u;ix=(dbo_u32)bb[q]|((dbo_u32)bb[q+1u]<<8);}
     else return MODELRT_ERR_FORMAT;
    }
    if(ix>=me->vertex_count||!dbo_fvf_decode_position(m,me,ix,xyz))return MODELRT_ERR_FORMAT;
    tri.v[k].position.x=xyz[0];tri.v[k].position.y=xyz[1];tri.v[k].position.z=xyz[2];
    if(dbo_fvf_decode_texcoord(m,me,ix,0u,uv)){tri.v[k].uv0.x=uv[0];tri.v[k].uv0.y=uv[1];tri.v[k].flags|=MODELRT_VTX_UV0;}
   }
   if(s->on_triangle&&s->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
  }
 }
 return b5b_end(h,s,u);
}
/* AMJ's callbacks collect actual decoded vertices/faces; proprietary binary
 * signatures without validated geometry are explicitly rejected. */
typedef struct b5b_amj_s {modelrt_vec3 *v;modelrt_u32 nv,maxv;modelrt_u32 *tris;modelrt_u32 nt,maxt;int overflow;} b5b_amj;
static void amj_vertex(void*u,amj3d_fixed x,amj3d_fixed y,amj3d_fixed z){
 b5b_amj*a=(b5b_amj*)u;if(a->nv>=a->maxv){a->overflow=1;return;}
 a->v[a->nv].x=(modelrt_q16)x;a->v[a->nv].y=(modelrt_q16)y;a->v[a->nv].z=(modelrt_q16)z;a->nv++;
}
static void amj_face(void*u,const amj3d_i32*ind,unsigned int count){
 b5b_amj*a=(b5b_amj*)u;unsigned int i;
 if(count<3u)return;
 for(i=1u;i+1u<count;i++){
  if(a->nt>=a->maxt){a->overflow=1;return;}
  if(ind[0]<0||ind[i]<0||ind[i+1u]<0){a->overflow=1;return;}
  a->tris[a->nt*3u]=(modelrt_u32)ind[0];a->tris[a->nt*3u+1u]=(modelrt_u32)ind[i];a->tris[a->nt*3u+2u]=(modelrt_u32)ind[i+1u];a->nt++;
 }
}
static int amj_open(modelrt_runtime*r,modelrt_handle*h,const char*p){
 b5b*s;b5b_amj*a;AMJ3D_Config cfg;AMJ3D_Callbacks cb;int rc;(void)r;rc=b5b_load(h,p,&s);if(rc)return rc;
 a=(b5b_amj*)modelrt_arena_alloc(h,sizeof(*a),sizeof(void*));if(!a)return MODELRT_ERR_NO_SPACE;modelrt_zero(a,sizeof(*a));
 a->maxv=65536u;a->maxt=65536u;
 a->v=(modelrt_vec3*)modelrt_arena_alloc(h,a->maxv*sizeof(*a->v),sizeof(void*));
 a->tris=(modelrt_u32*)modelrt_arena_alloc(h,a->maxt*3u*sizeof(*a->tris),sizeof(void*));
 if(!a->v||!a->tris)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(&cb,sizeof(cb));amj3d_default_config(&cfg);
 cfg.flags=AMJ3D_FLAG_STRICT|AMJ3D_FLAG_EMIT_TRIANGULATED|AMJ3D_FLAG_ACCEPT_OBJ_LIKE;
 cb.on_vertex=amj_vertex;cb.on_face=amj_face;
 rc=amj3d_decode(s->data,(unsigned long)s->size,&cfg,&cb,a);
 if(rc!=AMJ3D_OK||a->overflow||a->nt==0u)return MODELRT_ERR_FORMAT;
 s->native=a;h->info.mesh_count=1u;h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE;
 return MODELRT_OK;
}
static int amj_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u){
 b5b*st=(b5b*)h->native_state;b5b_amj*a=(b5b_amj*)st->native;modelrt_u32 i;int j,rc;modelrt_triangle_view tri;(void)t;
 if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;rc=b5b_begin(h,s,u);if(rc)return rc;
 for(i=0;i<a->nt;i++){
  modelrt_zero(&tri,sizeof(tri));for(j=0;j<3;j++){
   modelrt_u32 ix=a->tris[i*3u+j];if(ix>=a->nv)return MODELRT_ERR_FORMAT;
   tri.v[j].position=a->v[ix];
  }
  if(s->on_triangle&&s->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
 }
 return b5b_end(h,s,u);
}
#define B5B_DEF(ID,NAME,P,O,V) {ID,NAME,P,O,0,V,0,0,0,0,0,0,0,0,0}
const modelrt_provider modelrt_provider_native_ter89=B5B_DEF(66u,"TER native",ter_probe,ter_open,ter_visit);
const modelrt_provider modelrt_provider_native_ndo89=B5B_DEF(67u,"NDO native",ndo_probe,ndo_open,ndo_visit);
const modelrt_provider modelrt_provider_native_ask89=B5B_DEF(68u,"ASK native",ask_probe,ask_open,ask_visit);
const modelrt_provider modelrt_provider_native_dbo89=B5B_DEF(69u,"DBO native",dbo_probe,dbo_open,dbo_visit);
const modelrt_provider modelrt_provider_native_amj89=B5B_DEF(70u,"AMJ native",amj_probe,amj_open,amj_visit);

const void *modelrt_native_batch5b_89(const modelrt_handle*h){
 const b5b*s;
 if(!h||!h->is_open||h->info.codec_id<66u||h->info.codec_id>70u)return 0;
 s=(const b5b*)h->native_state;return s?s->native:0;
}
