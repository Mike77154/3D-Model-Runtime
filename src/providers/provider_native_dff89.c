/* RenderWare DFF native adapter: own decoded Dff3dModel, no GACKT-LANG. */
#include "../modelrt_internal.h"
#include "dff3d.h"
#include <string.h>
typedef struct dffrt_state_s {Dff3dModel*model;} dffrt_state;
static int dff_probe(const char*p,const modelrt_u8*h,size_t n)
{
 (void)h;(void)n;return modelrt_ext_eq(p,".dff")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;
}
static int dff_open(modelrt_runtime*r,modelrt_handle*h,const char*p)
{
 modelrt_u8*bytes;size_t n;int rc;Dff3dModel*m;Dff3dDiag*diag;dffrt_state*state;(void)r;
 rc=modelrt_read_file_arena(h,p,&bytes,&n);if(rc)return rc;
 if(!n||n>2147483647u)return MODELRT_ERR_FORMAT;
 m=(Dff3dModel*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));
 diag=(Dff3dDiag*)modelrt_arena_alloc(h,sizeof(*diag),sizeof(void*));
 state=(dffrt_state*)modelrt_arena_alloc(h,sizeof(*state),sizeof(void*));
 if(!m||!diag||!state)return MODELRT_ERR_NO_SPACE;
 dff3d_model_clear(m);dff3d_diag_clear(diag);
 rc=dff3d_decode_memory(bytes,(dff_u32)n,m,diag);
 if(rc!=DFF3D_OK||!m->vertex_count||!m->triangle_count||diag->overflow_vertices||diag->overflow_triangles)return MODELRT_ERR_FORMAT;
 state->model=m;h->native_state=state;
 h->info.mesh_count=m->geometry_count?m->geometry_count:1u;
 h->info.bone_count=m->skin_bone_count;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE;
 if(m->vertex_count&&m->vertices[0].has_uv0)h->info.capabilities|=MODELRT_CAP_UV0;
 if(m->skin_bone_count)h->info.capabilities|=MODELRT_CAP_SKELETON;
 return MODELRT_OK;
}
static int dff_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
 Dff3dModel*m=((dffrt_state*)h->native_state)->model;
 modelrt_triangle_view tr;modelrt_mesh_info mi;dff_u32 i,j,vid[3];(void)t;
 if(ai!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 if(s->on_model_begin&&s->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
 modelrt_zero(&mi,sizeof(mi));mi.vertex_count=m->vertex_count;mi.triangle_count=m->triangle_count;
 modelrt_copy_string(mi.name,sizeof(mi.name),"RenderWare DFF");
 if(s->on_mesh_begin&&s->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;
 for(i=0u;i<m->triangle_count;i++){
  const Dff3dTriangle*f=&m->triangles[i];modelrt_zero(&tr,sizeof(tr));
  vid[0]=f->a;vid[1]=f->b;vid[2]=f->c;tr.material_index=(modelrt_i32)f->material;
  tr.mesh_index=(modelrt_u32)f->geometry;
  for(j=0u;j<3u;j++){
   const Dff3dVertex*v;
   if(vid[j]>=m->vertex_count)return MODELRT_ERR_FORMAT;
   v=&m->vertices[vid[j]];
   tr.v[j].position.x=(modelrt_q16)v->pos.x;
   tr.v[j].position.y=(modelrt_q16)v->pos.y;
   tr.v[j].position.z=(modelrt_q16)v->pos.z;
   if(v->has_uv0){tr.v[j].uv0.x=(modelrt_q16)v->uv0.u;tr.v[j].uv0.y=(modelrt_q16)v->uv0.v;tr.v[j].flags|=MODELRT_VTX_UV0;}
   if(v->has_normal){tr.v[j].normal.x=(modelrt_q16)v->normal.x;tr.v[j].normal.y=(modelrt_q16)v->normal.y;tr.v[j].normal.z=(modelrt_q16)v->normal.z;tr.v[j].flags|=MODELRT_VTX_NORMAL;}
  }
  if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
 }
 if(s->on_mesh_end&&s->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;
 return s->on_model_end&&s->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
const Dff3dModel*modelrt_native_dff89(const modelrt_handle*h)
{return h&&h->is_open&&h->info.codec_id==96u?((dffrt_state*)h->native_state)->model:0;}
const modelrt_provider modelrt_provider_native_dff89={96u,"RenderWare DFF native",dff_probe,dff_open,0,dff_visit,0,0,0,0,0,0,0,0,0};
