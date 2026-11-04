/* Poser textual scene/embedded OBJ native bridge. C89, no heap. */
#include "../modelrt_internal.h"
#include "codec3d_poser.h"
#include <string.h>
typedef struct poser_state_s { P3D_Context *scene; } poser_state;
static int poser_probe(const char*p,const modelrt_u8*h,size_t n)
{(void)h;(void)n;return modelrt_ext_eq(p,".pp2")||modelrt_ext_eq(p,".pz3")||modelrt_ext_eq(p,".cr2")||modelrt_ext_eq(p,".pz2")||modelrt_ext_eq(p,".poser")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int poser_open(modelrt_runtime*r,modelrt_handle*h,const char*p)
{
 modelrt_u8 *b;size_t n;int rc;poser_state*s;P3D_Context*c;(void)r;
 rc=modelrt_read_file_arena(h,p,&b,&n);if(rc)return rc;
 if(!n||n>2147483647u)return MODELRT_ERR_FORMAT;
 c=(P3D_Context*)modelrt_arena_alloc(h,sizeof(*c),sizeof(void*));
 s=(poser_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
 if(!c||!s)return MODELRT_ERR_NO_SPACE;
 p3d_init(c);rc=p3d_decode_memory(c,b,(unsigned long)n);
 if(rc!=P3D_OK||c->flags&(P3D_FLAG_FACE_OVERFLOW|P3D_FLAG_VERTEX_OVERFLOW|P3D_FLAG_TEX_OVERFLOW|P3D_FLAG_NORMAL_OVERFLOW|P3D_FLAG_LINE_TRUNCATED))return MODELRT_ERR_FORMAT;
 /* Metadata-only Poser files without an embedded mesh are valid source scenes,
    but must have recognized objects/channels/references. */
 if(!c->face_count&&!c->object_count&&!c->ref_count&&!c->channel_count)return MODELRT_ERR_FORMAT;
 s->scene=c;h->native_state=s;
 h->info.mesh_count=c->face_count?1u:0u;
 h->info.capabilities=MODELRT_CAP_NATIVE_SCENE;
 if(c->face_count)h->info.capabilities|=MODELRT_CAP_GEOMETRY;
 if(c->texcoord_count)h->info.capabilities|=MODELRT_CAP_UV0;
 if(c->normal_count)h->info.capabilities|=MODELRT_CAP_NORMALS;
 return MODELRT_OK;
}
static int poser_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
 const P3D_Context*c=((poser_state*)h->native_state)->scene;
 modelrt_mesh_info mi;modelrt_triangle_view tr;unsigned long i;int j;(void)t;
 if(ai!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 if(s->on_model_begin&&s->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
 if(c->face_count){
  modelrt_zero(&mi,sizeof(mi));mi.triangle_count=(modelrt_u32)c->face_count;mi.vertex_count=(modelrt_u32)c->vertex_count;
  modelrt_copy_string(mi.name,sizeof(mi.name),"Poser embedded mesh");
  if(s->on_mesh_begin&&s->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;
  for(i=0;i<c->face_count;i++){
   const P3D_Face*f=&c->faces[i];
   modelrt_zero(&tr,sizeof(tr));tr.material_index=(modelrt_i32)f->material;
   for(j=0;j<3;j++){
    unsigned short vi=f->v[j],uv=f->vt[j],norm=f->vn[j];
    if(vi>=c->vertex_count)return MODELRT_ERR_FORMAT;
    tr.v[j].position.x=(modelrt_q16)c->vertices[vi].x;
    tr.v[j].position.y=(modelrt_q16)c->vertices[vi].y;
    tr.v[j].position.z=(modelrt_q16)c->vertices[vi].z;
    if(uv!=P3D_NO_INDEX&&uv<c->texcoord_count){
     tr.v[j].uv0.x=(modelrt_q16)c->texcoords[uv].u;
     tr.v[j].uv0.y=(modelrt_q16)c->texcoords[uv].v;
     tr.v[j].flags|=MODELRT_VTX_UV0;
    }
    if(norm!=P3D_NO_INDEX&&norm<c->normal_count){
     tr.v[j].normal.x=(modelrt_q16)c->normals[norm].x;
     tr.v[j].normal.y=(modelrt_q16)c->normals[norm].y;
     tr.v[j].normal.z=(modelrt_q16)c->normals[norm].z;
     tr.v[j].flags|=MODELRT_VTX_NORMAL;
    }
   }
   if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
  }
  if(s->on_mesh_end&&s->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;
 }
 return s->on_model_end&&s->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
const P3D_Context *modelrt_native_poser89(const modelrt_handle*h)
{return h&&h->is_open&&h->info.codec_id==95u?((poser_state*)h->native_state)->scene:0;}
const modelrt_provider modelrt_provider_native_poser89={95u,"Poser native",poser_probe,poser_open,0,poser_visit,0,0,0,0,0,0,0,0,0};
