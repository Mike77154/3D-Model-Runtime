/* Native Batch 05A: ACC, SIB, COB, MD5, AMF.
   All calls go straight to the vendored codec, never through GACKT-LANG. */
#include "../modelrt_internal.h"
#include "acc3d.h"
#include "sib3d.h"
#include "cob_chiqui3d.h"
#include "md5fx.h"
#include "chiqui_amf.h"
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct b5_state_s { void *native; void *extra; modelrt_u32 count; } b5_state;
static int b5_probe_ext(const char *p, const modelrt_u8 *h, size_t n, const char *e)
{(void)h;(void)n;return modelrt_ext_eq(p,e)?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
#define B5_PROBE(NAME,EXT) static int NAME(const char*p,const modelrt_u8*h,size_t n){return b5_probe_ext(p,h,n,EXT);}
B5_PROBE(acc_probe,".acc")
B5_PROBE(sib_probe,".sib")
B5_PROBE(cob_probe,".cob")
B5_PROBE(md5_probe,".md5mesh")
B5_PROBE(amf_probe,".amf")
static int b5_begin(modelrt_handle*h,const modelrt_visit_sink*s,void*u){
 modelrt_mesh_info m;modelrt_zero(&m,sizeof(m));modelrt_copy_string(m.name,sizeof(m.name),"Native source mesh");
 if(s->on_model_begin&&s->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
 if(s->on_mesh_begin&&s->on_mesh_begin(u,&m))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
static int b5_end(modelrt_handle*h,const modelrt_visit_sink*s,void*u){
 modelrt_mesh_info m;modelrt_zero(&m,sizeof(m));modelrt_copy_string(m.name,sizeof(m.name),"Native source mesh");
 if(s->on_mesh_end&&s->on_mesh_end(u,&m))return MODELRT_ERR_CALLBACK;
 if(s->on_model_end&&s->on_model_end(u,&h->info))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
static int b5_load(modelrt_handle*h,const char*p,b5_state**out,modelrt_u8**data,size_t*n){
 b5_state *s;int r;
 s=(b5_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));if(!s)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(s,sizeof(*s));r=modelrt_read_file_arena(h,p,data,n);if(r)return r;
 if(*n==0u || *n>0x7fffffffUL)return MODELRT_ERR_FORMAT;
 h->native_state=s;*out=s;return MODELRT_OK;
}
static int acc_open(modelrt_runtime*r,modelrt_handle*h,const char*p){
 b5_state*s;modelrt_u8*d;size_t n;struct acc3d_model*m;int rc;(void)r;
 rc=b5_load(h,p,&s,&d,&n);if(rc)return rc;
 m=(struct acc3d_model*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
 acc3d_model_init(m);rc=acc3d_decode_memory((const char*)d,n,ACC3D_OPT_NONE,m);
 if(rc!=ACC3D_OK)return MODELRT_ERR_FORMAT;
 rc=acc3d_triangulate(m,ACC3D_OPT_SKIP_NON_POLY);if(rc!=ACC3D_OK)return MODELRT_ERR_FORMAT;
 if(m->triangle_count<=0)return MODELRT_ERR_FORMAT;
 s->native=m;h->info.mesh_count=(modelrt_u32)m->object_count;h->info.material_count=(modelrt_u32)m->material_count;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_MATERIALS|MODELRT_CAP_NATIVE_SCENE;
 return MODELRT_OK;
}
static int acc_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u){
 b5_state*st=(b5_state*)h->native_state;struct acc3d_model*m=(struct acc3d_model*)st->native;
 long i;int j,rc;modelrt_triangle_view tri;(void)t;
 if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;rc=b5_begin(h,s,u);if(rc)return rc;
 for(i=0;i<m->triangle_count;i++){
  const struct acc3d_triangle*f=&m->triangles[i];modelrt_zero(&tri,sizeof(tri));tri.material_index=(modelrt_i32)f->material_index;
  for(j=0;j<3;j++){
   long vi=f->v[j],ri=f->ref_index[j];const struct acc3d_vec3*v;
   if(vi<0||vi>=m->vertex_count||ri<0||ri>=m->ref_count)return MODELRT_ERR_FORMAT;
   v=&m->vertices[vi];tri.v[j].position.x=(modelrt_q16)v->x;tri.v[j].position.y=(modelrt_q16)v->y;tri.v[j].position.z=(modelrt_q16)v->z;
   tri.v[j].uv0.x=(modelrt_q16)m->refs[ri].uv.u;tri.v[j].uv0.y=(modelrt_q16)m->refs[ri].uv.v;tri.v[j].flags=MODELRT_VTX_UV0;
  }
  if(s->on_triangle&&s->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
 }
 return b5_end(h,s,u);
}
static int sib_open(modelrt_runtime*r,modelrt_handle*h,const char*p){
 b5_state*s;modelrt_u8*d;size_t n;SIB_Scene*m;SIB_Diagnostics dg;int rc;(void)r;
 rc=b5_load(h,p,&s,&d,&n);if(rc)return rc;
 m=(SIB_Scene*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
 sib_scene_init(m);sib_diagnostics_init(&dg);
 if(sib_decode_memory(d,(SIB_U32)n,m,&dg)!=SIB_OK||m->face_count==0u)return MODELRT_ERR_FORMAT;
 s->native=m;h->info.mesh_count=m->mesh_count;h->info.material_count=m->material_count;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NORMALS|MODELRT_CAP_MATERIALS|MODELRT_CAP_NATIVE_SCENE;
 return MODELRT_OK;
}
static int sib_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u){
 b5_state*st=(b5_state*)h->native_state;SIB_Scene*m=(SIB_Scene*)st->native;SIB_U32 i,j,k,ci[3];int n,rc;modelrt_triangle_view tri;(void)t;
 if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;rc=b5_begin(h,s,u);if(rc)return rc;
 for(i=0;i<m->face_count;i++){
  const SIB_Face*f=&m->faces[i];if(f->corner_count<3u)continue;
  if(f->first_corner>m->corner_count||f->corner_count>m->corner_count-f->first_corner)return MODELRT_ERR_FORMAT;
  for(j=1u;j+1u<f->corner_count;j++){
   ci[0]=f->first_corner;ci[1]=f->first_corner+j;ci[2]=f->first_corner+j+1u;
   modelrt_zero(&tri,sizeof(tri));tri.material_index=(modelrt_i32)f->material;
   for(n=0;n<3;n++){
    const SIB_Corner*c=&m->corners[ci[n]];const SIB_Vec3*v;
    if(c->vertex>=m->vertex_count)return MODELRT_ERR_FORMAT;
    v=&m->vertices[c->vertex];tri.v[n].position.x=v->x;tri.v[n].position.y=v->y;tri.v[n].position.z=v->z;
    tri.v[n].uv0.x=c->uv.x;tri.v[n].uv0.y=c->uv.y;tri.v[n].normal.x=c->normal.x;
    tri.v[n].normal.y=c->normal.y;tri.v[n].normal.z=c->normal.z;
    tri.v[n].flags=MODELRT_VTX_UV0|MODELRT_VTX_NORMAL;
   }
   if(s->on_triangle&&s->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
  }
 }
 return b5_end(h,s,u);
}
static int cob_open(modelrt_runtime*r,modelrt_handle*h,const char*p){
 b5_state*s;modelrt_u8*d;size_t n;cob_scene*m;int rc;(void)r;
 rc=b5_load(h,p,&s,&d,&n);if(rc)return rc;
 m=(cob_scene*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
 cob_scene_clear(m);rc=cob_decode_memory(m,d,(unsigned long)n);if(rc!=COB_OK||m->face_count==0u)return MODELRT_ERR_FORMAT;
 {
  cob_triangle*cache;unsigned long cap=(unsigned long)m->index_count,ntri=0u;
  if(!cap||cap>1500000u)return MODELRT_ERR_LIMIT;
  cache=(cob_triangle*)modelrt_arena_alloc(h,(size_t)cap*sizeof(*cache),sizeof(void*));
  if(!cache)return MODELRT_ERR_NO_SPACE;
  if(cob_scene_build_triangles(m,cache,cap,&ntri)!=COB_OK||!ntri)return MODELRT_ERR_FORMAT;
  s->extra=cache;s->count=(modelrt_u32)ntri;
 }
 s->native=m;h->info.mesh_count=m->mesh_count;h->info.material_count=m->material_count;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_MATERIALS|MODELRT_CAP_NATIVE_SCENE;
 return MODELRT_OK;
}
static int cob_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u){
 b5_state*st=(b5_state*)h->native_state;cob_scene*m=(cob_scene*)st->native;
 const cob_triangle *faces=(const cob_triangle*)st->extra;unsigned long count=(unsigned long)st->count;int rc,j;unsigned long i;modelrt_triangle_view tri;(void)t;
 if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;
 (void)m;rc=b5_begin(h,s,u);if(rc)return rc;
 for(i=0;i<count;i++){
  modelrt_zero(&tri,sizeof(tri));tri.material_index=faces[i].material;tri.mesh_index=faces[i].mesh_index;
  for(j=0;j<3;j++){
   tri.v[j].position.x=(modelrt_q16)faces[i].p[j].x;
   tri.v[j].position.y=(modelrt_q16)faces[i].p[j].y;
   tri.v[j].position.z=(modelrt_q16)faces[i].p[j].z;
   tri.v[j].uv0.x=(modelrt_q16)faces[i].uv[j].u;
   tri.v[j].uv0.y=(modelrt_q16)faces[i].uv[j].v;tri.v[j].flags=MODELRT_VTX_UV0;
  }
  if(s->on_triangle&&s->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
 }
 return b5_end(h,s,u);
}
/* The upstream reader ignores huge textual triangle IDs: reject them here. */
static int md5_tri_headers_valid(const char *path){
 FILE*f=fopen(path,"rb");char line[512];int ok=1;
 if(!f)return 0;
 while(fgets(line,sizeof(line),f)){
  const char *p=line;char*end;unsigned long index;
  while(*p&&isspace((unsigned char)*p))p++;
  if(p[0]=='t'&&p[1]=='r'&&p[2]=='i'&&isspace((unsigned char)p[3])){
   p+=3;while(*p&&isspace((unsigned char)*p))p++;
   if(!isdigit((unsigned char)*p)){ok=0;break;}
   index=strtoul(p,&end,10);
   if(end==p||index>=MD5FX_MAX_TRIS){ok=0;break;}
  }
 }
 fclose(f);return ok;
}
static int md5_open(modelrt_runtime*r,modelrt_handle*h,const char*p){
 md5fx_mesh_file*m;md5fx_status st;b5_state*s;(void)r;
 s=(b5_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));if(!s)return MODELRT_ERR_NO_SPACE;modelrt_zero(s,sizeof(*s));
 m=(md5fx_mesh_file*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
 md5fx_mesh_init(m);md5fx_status_init(&st);if(md5fx_mesh_load_file(m,p,&st)!=MD5FX_OK||m->num_meshes<=0)return MODELRT_ERR_FORMAT;
 {
  int mi,ti;
  for(mi=0;mi<m->num_meshes;mi++){
    const md5fx_mesh*mesh=&m->meshes[mi];
    if(mesh->num_tris<0||mesh->num_verts<0||mesh->first_tri<0||mesh->first_vert<0||
       mesh->first_tri>MD5FX_MAX_TRIS-mesh->num_tris||mesh->first_vert>MD5FX_MAX_VERTS-mesh->num_verts)return MODELRT_ERR_FORMAT;
    for(ti=0;ti<mesh->num_tris;ti++){
      const md5fx_tri*tr=&m->tris[mesh->first_tri+ti];
      if(tr->v0<0||tr->v0>=mesh->num_verts||tr->v1<0||tr->v1>=mesh->num_verts||
         tr->v2<0||tr->v2>=mesh->num_verts)return MODELRT_ERR_FORMAT;
    }
  }
 }
 if(!md5_tri_headers_valid(p))return MODELRT_ERR_FORMAT;
 s->native=m;h->native_state=s;h->info.mesh_count=m->num_meshes;h->info.bone_count=m->num_joints;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_SKELETON|MODELRT_CAP_SKINNING|MODELRT_CAP_NATIVE_SCENE;
 return MODELRT_OK;
}
static int md5_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u){
 b5_state*st=(b5_state*)h->native_state;md5fx_mesh_file*m=(md5fx_mesh_file*)st->native;
 md5fx_pose pose;md5fx_status status;md5fx_vec3*pos;int rc,i,j,k,vid[3];modelrt_triangle_view tri;(void)t;
 if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;
 md5fx_pose_init(&pose);md5fx_status_init(&status);
 rc=md5fx_mesh_bind_pose(m,&pose,&status);if(rc!=MD5FX_OK)return MODELRT_ERR_FORMAT;
 pos=(md5fx_vec3*)st->extra;
 if(!pos){pos=(md5fx_vec3*)modelrt_arena_alloc(h,sizeof(*pos)*MD5FX_MAX_VERTS,sizeof(void*));
 if(!pos)return MODELRT_ERR_NO_SPACE;st->extra=pos;}
 rc=md5fx_mesh_build_vertices(m,&pose,pos,MD5FX_MAX_VERTS,&status);if(rc<0)return MODELRT_ERR_FORMAT;
 rc=b5_begin(h,s,u);if(rc)return rc;
 for(i=0;i<m->num_meshes;i++){
  const md5fx_mesh*mesh=&m->meshes[i];
  for(j=0;j<mesh->num_tris;j++){
   const md5fx_tri*f=&m->tris[mesh->first_tri+j];
   vid[0]=f->v0;vid[1]=f->v1;vid[2]=f->v2;
   modelrt_zero(&tri,sizeof(tri));tri.mesh_index=i;tri.material_index=i;
   for(k=0;k<3;k++){
    int v=mesh->first_vert+vid[k];
    if(vid[k]<0||vid[k]>=mesh->num_verts||v<0||v>=MD5FX_MAX_VERTS)return MODELRT_ERR_FORMAT;
    tri.v[k].position.x=(modelrt_q16)pos[v].x;tri.v[k].position.y=(modelrt_q16)pos[v].y;tri.v[k].position.z=(modelrt_q16)pos[v].z;
    tri.v[k].uv0.x=(modelrt_q16)m->verts[v].uv.x;tri.v[k].uv0.y=(modelrt_q16)m->verts[v].uv.y;
    tri.v[k].flags=MODELRT_VTX_UV0;
   }
   if(s->on_triangle&&s->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
  }
 }
 return b5_end(h,s,u);
}
static int amf_open(modelrt_runtime*r,modelrt_handle*h,const char*p){
 b5_state*s;modelrt_u8*d;size_t n;amf_document*doc;amf_arena a;amf_diag dg;void*scratch;size_t need;int rc;(void)r;
 rc=b5_load(h,p,&s,&d,&n);if(rc)return rc;
 if(n<5u||d[0]!='<'||n>50000000u)return MODELRT_ERR_FORMAT; /* ZIP AMF pending */
 doc=(amf_document*)modelrt_arena_alloc(h,sizeof(*doc),sizeof(void*));if(!doc)return MODELRT_ERR_NO_SPACE;
 need=n*10u+4096u;scratch=modelrt_arena_alloc(h,need,sizeof(void*));if(!scratch)return MODELRT_ERR_NO_SPACE;
 amf_arena_init(&a,scratch,need);amf_document_zero(doc);amf_diag_clear(&dg);
 rc=amf_parse_xml(doc,&a,(const char*)d,n,0,&dg);if(rc!=AMF_OK||doc->triangle_count==0u)return MODELRT_ERR_FORMAT;
 s->native=doc;h->info.mesh_count=(modelrt_u32)doc->object_count;
 h->info.material_count=(modelrt_u32)doc->material_count;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_MATERIALS|MODELRT_CAP_NATIVE_SCENE;
 return MODELRT_OK;
}
static int amf_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u){
 b5_state*st=(b5_state*)h->native_state;amf_document*m=(amf_document*)st->native;
 size_t i;int j,rc;modelrt_triangle_view tri;(void)t;
 if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;rc=b5_begin(h,s,u);if(rc)return rc;
 for(i=0;i<m->triangle_count;i++){
  const amf_triangle*f=&m->triangles[i];const amf_object*o;amf_u32 idx[3];
  if(f->object_index>=m->object_count)return MODELRT_ERR_FORMAT;
  o=&m->objects[f->object_index];idx[0]=f->v1;idx[1]=f->v2;idx[2]=f->v3;
  modelrt_zero(&tri,sizeof(tri));tri.mesh_index=(modelrt_u32)f->object_index;tri.material_index=(modelrt_i32)o->material_id;
  for(j=0;j<3;j++){
   const amf_vec3*v;size_t ix;
   if(idx[j]>=o->vertex_count)return MODELRT_ERR_FORMAT;
   ix=o->vertex_first+(size_t)idx[j];if(ix>=m->vertex_count)return MODELRT_ERR_FORMAT;
   v=&m->vertices[ix].p;tri.v[j].position.x=(modelrt_q16)v->x;tri.v[j].position.y=(modelrt_q16)v->y;tri.v[j].position.z=(modelrt_q16)v->z;
  }
  if(s->on_triangle&&s->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
 }
 return b5_end(h,s,u);
}
#define B5_DEF(ID,NAME,P,O,V) {ID,NAME,P,O,0,V,0,0,0,0,0,0,0,0,0}
const modelrt_provider modelrt_provider_native_acc89=B5_DEF(61u,"ACC native",acc_probe,acc_open,acc_visit);
const modelrt_provider modelrt_provider_native_sib89=B5_DEF(62u,"SIB native",sib_probe,sib_open,sib_visit);
const modelrt_provider modelrt_provider_native_cob89=B5_DEF(63u,"COB native",cob_probe,cob_open,cob_visit);
const modelrt_provider modelrt_provider_native_md5_89=B5_DEF(64u,"MD5 native",md5_probe,md5_open,md5_visit);
const modelrt_provider modelrt_provider_native_amf89=B5_DEF(65u,"AMF XML native",amf_probe,amf_open,amf_visit);

const void *modelrt_native_batch5a_89(const modelrt_handle*h){
 const b5_state*s;
 if(!h||!h->is_open||h->info.codec_id<61u||h->info.codec_id>65u)return 0;
 s=(const b5_state*)h->native_state;return s?s->native:0;
}
