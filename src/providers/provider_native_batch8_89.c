/* GACKT native expansion: Model3D (.m3d) + Character Studio CSM.
   No GACKT-LANG, no intermediate file, no heap. C89. */
#include "../modelrt_internal.h"
#include "m3d89.h"
#include "CSM_chiquisumother3d.h"
#include "lws3d.h"
#include "zlib89.h"
#include <string.h>

typedef struct m3drt_state_s {m3d89_model*model;} m3drt_state;
static int m3d_probe(const char *p,const modelrt_u8 *h,size_t n)
{return modelrt_ext_eq(p,".m3d")&&n>=8u&&!memcmp(h,"3DMO",4u)?MODELRT_PROBE_YES:MODELRT_PROBE_NO;}
static int m3d_open(modelrt_runtime *r,modelrt_handle *h,const char *path)
{
 modelrt_u8 *buf,*payload;size_t len=0u,outlen=0u;int rc;
 m3drt_state *s;m3d89_model*m;(void)r;
 rc=modelrt_read_file_arena(h,path,&buf,&len);if(rc)return rc;
 if(len<16u || memcmp(buf,"3DMO",4u))return MODELRT_ERR_FORMAT;
 s=(m3drt_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
 m=(m3d89_model*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));
 if(!s||!m)return MODELRT_ERR_NO_SPACE;
 rc=m3d89_decode(m,buf,(m3d89_u32)len);
 if(rc==M3D89_ERR_STREAM_PACKED){
   /* Stream begins immediately after 8-byte 3DMO size header; zlib-wrapped HEAD chunk. */
   payload=(modelrt_u8*)modelrt_arena_alloc(h,16u*1024u*1024u,1u);
   if(!payload)return MODELRT_ERR_NO_SPACE;
   if(zlib89_inflate_memory(buf+8u,len-8u,payload,16u*1024u*1024u,&outlen)!=ZLIB89_OK)return MODELRT_ERR_FORMAT;
   m3d89_zero_model(m);
   rc=m3d89_decode_payload(m,payload,(m3d89_u32)outlen,(m3d89_u32)len,8u);
 }
 if(rc!=M3D89_OK||m->face_count==0u||m->vertex_count==0u)return MODELRT_ERR_FORMAT;
 s->model=m;h->native_state=s;
 h->info.mesh_count=1u;
 h->info.bone_count=(modelrt_u32)m->bone_count;
 h->info.animation_count=(modelrt_u32)m->action_count;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE;
 if(m->texcoord_count)h->info.capabilities|=MODELRT_CAP_UV0;
 if(m->bone_count)h->info.capabilities|=MODELRT_CAP_SKELETON;
 if(m->action_count)h->info.capabilities|=MODELRT_CAP_ANIMATION;
 return MODELRT_OK;
}
static int m3d_visit(modelrt_handle*h,int ai,modelrt_q16 time,const modelrt_visit_sink*s,void*u)
{
 m3d89_model*m=((m3drt_state*)h->native_state)->model;
 modelrt_triangle_view tr;modelrt_mesh_info mi;modelrt_bone_view bv;
 m3d89_u32 i,j,k,idx,uv;
 (void)time;
 if(ai!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 if(s->on_model_begin&&s->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
 modelrt_zero(&mi,sizeof(mi));mi.triangle_count=m->face_count;mi.vertex_count=m->vertex_count;
 modelrt_copy_string(mi.name,sizeof(mi.name),"M3D native mesh");
 if(s->on_mesh_begin&&s->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;
 for(i=0u;i<m->face_count;i++){
  const m3d89_face*f=&m->faces[i];modelrt_zero(&tr,sizeof(tr));
  tr.material_index=(modelrt_i32)f->material_index;
  for(j=0u;j<3u;j++){
   idx=f->vertex[j];if(idx>=m->vertex_count)return MODELRT_ERR_FORMAT;
   tr.v[j].position.x=(modelrt_q16)m->vertices[idx].x;
   tr.v[j].position.y=(modelrt_q16)m->vertices[idx].y;
   tr.v[j].position.z=(modelrt_q16)m->vertices[idx].z;
   uv=f->texcoord[j];
   if((f->flags&M3D89_FACE_HAS_UV)&&uv<m->texcoord_count){
    tr.v[j].uv0.x=(modelrt_q16)m->texcoords[uv].u;
    tr.v[j].uv0.y=(modelrt_q16)m->texcoords[uv].v;
    tr.v[j].flags|=MODELRT_VTX_UV0;
   }
  }
  if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
 }
 if(s->on_mesh_end&&s->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;
 for(k=0;k<(m3d89_u32)m->bone_count;k++){
  modelrt_zero(&bv,sizeof(bv));bv.index=(modelrt_u32)k;bv.parent=-1;
  if(s->on_bone&&s->on_bone(u,&bv))return MODELRT_ERR_CALLBACK;
 }
 return s->on_model_end&&s->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
static int m3d_anim_count(const modelrt_handle*h)
{return ((m3drt_state*)h->native_state)->model->action_count;}
static int m3d_anim_info(modelrt_handle*h,int i,modelrt_animation_info*out)
{
 m3d89_model*m=((m3drt_state*)h->native_state)->model;
 if(!out||i<0||i>=m->action_count)return MODELRT_ERR_BAD_ARG;
 modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)i;
 out->flags=MODELRT_ANIM_UNBOUND;return MODELRT_OK;
}

typedef struct csmrt_state_s {CSM3D_Doc*doc;} csmrt_state;
static int csm_probe(const char*p,const modelrt_u8*h,size_t n)
{(void)h;(void)n;return modelrt_ext_eq(p,".csm")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int csm_open(modelrt_runtime*r,modelrt_handle*h,const char*path)
{
 modelrt_u8*bytes;size_t len;int rc;CSM3D_Doc*doc;CSM3D_Diagnostic*diag;
 csmrt_state*st;(void)r;
 rc=modelrt_read_file_arena(h,path,&bytes,&len);if(rc)return rc;
 if(!len||len>2147483647u)return MODELRT_ERR_FORMAT;
 doc=(CSM3D_Doc*)modelrt_arena_alloc(h,sizeof(*doc),sizeof(void*));
 diag=(CSM3D_Diagnostic*)modelrt_arena_alloc(h,sizeof(*diag),sizeof(void*));
 st=(csmrt_state*)modelrt_arena_alloc(h,sizeof(*st),sizeof(void*));
 if(!doc||!diag||!st)return MODELRT_ERR_NO_SPACE;
 csm3d_doc_init(doc);csm3d_diag_init(diag);
 rc=csm3d_decode((const char*)bytes,(csm3d_u32)len,doc,diag);
 if(rc!=CSM3D_OK||doc->frame_count<1||doc->marker_count<1)return MODELRT_ERR_FORMAT;
 st->doc=doc;h->native_state=st;
 h->info.animation_count=1u;
 h->info.capabilities=MODELRT_CAP_ANIMATION|MODELRT_CAP_NATIVE_SCENE;
 return MODELRT_OK;
}
static int csm_visit(modelrt_handle*h,int ai,modelrt_q16 time,const modelrt_visit_sink*s,void*u)
{
 (void)time;if(ai!=MODELRT_ANIM_BIND_POSE&&ai!=0)return MODELRT_ERR_BAD_ARG;
 if(s->on_model_begin&&s->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
 /* CSM stores optical motion markers, NOT meshes and NOT skeleton bones.
    The complete source marker/frame arrays remain accessible via native_state. */
 return s->on_model_end&&s->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
static int csm_anim_count(const modelrt_handle*h)
{(void)h;return 1;}
static int csm_anim_info(modelrt_handle*h,int index,modelrt_animation_info*out)
{
 const CSM3D_Doc*d=((csmrt_state*)h->native_state)->doc;
 if(!out||index!=0)return MODELRT_ERR_BAD_ARG;
 modelrt_zero(out,sizeof(*out));out->index=0u;
 out->frame_count=(modelrt_u32)d->frame_count;
 out->flags=MODELRT_ANIM_DENSE_FRAMES|MODELRT_ANIM_UNBOUND;
 modelrt_copy_string(out->name,sizeof(out->name),"CSM optical markers");
 return MODELRT_OK;
}
const m3d89_model*modelrt_native_m3d89(const modelrt_handle*h)
{return h&&h->is_open&&h->info.codec_id==92u?((m3drt_state*)h->native_state)->model:0;}
const CSM3D_Doc*modelrt_native_csm89(const modelrt_handle*h)
{return h&&h->is_open&&h->info.codec_id==93u?((csmrt_state*)h->native_state)->doc:0;}
const modelrt_provider modelrt_provider_native_m3d89={92u,"Model3D native",m3d_probe,m3d_open,0,m3d_visit,m3d_anim_count,m3d_anim_info,0,0,0,0,0,0,0};
const modelrt_provider modelrt_provider_native_csm89={93u,"CSM optical motion native",csm_probe,csm_open,0,csm_visit,csm_anim_count,csm_anim_info,0,0,0,0,0,0,0};

/* LWS is a scene reference/transform and animation format; it is not itself
 * an LWO mesh. Preserve the native scene and never emit fake triangles. */
typedef struct lwsrt_state_s {LWS3D_Scene*scene;} lwsrt_state;
static int lws_probe(const char*p,const modelrt_u8*h,size_t n)
{(void)h;(void)n;return modelrt_ext_eq(p,".lws")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int lws_open(modelrt_runtime*r,modelrt_handle*h,const char*p)
{
 modelrt_u8*b;size_t n;int rc;LWS3D_Scene*scene;LWS3D_ParseStats stats;lwsrt_state*state;
 (void)r;
 rc=modelrt_read_file_arena(h,p,&b,&n);if(rc)return rc;
 if(n==0u||n>2147483647u)return MODELRT_ERR_FORMAT;
 scene=(LWS3D_Scene*)modelrt_arena_alloc(h,sizeof(*scene),sizeof(void*));
 state=(lwsrt_state*)modelrt_arena_alloc(h,sizeof(*state),sizeof(void*));
 if(!scene||!state)return MODELRT_ERR_NO_SPACE;
 lws3d_scene_init(scene);modelrt_zero(&stats,sizeof(stats));
 rc=lws3d_parse_buffer(scene,(const char*)b,(unsigned long)n,&stats);
 if(rc!=LWS3D_OK||scene->overflowed)return MODELRT_ERR_FORMAT;
 state->scene=scene;h->native_state=state;
 h->info.capabilities=MODELRT_CAP_NATIVE_SCENE;
 if(scene->first_frame<scene->last_frame){
    h->info.animation_count=1u;h->info.capabilities|=MODELRT_CAP_ANIMATION;
 }
 return MODELRT_OK;
}
static int lws_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
 (void)t;
 if(ai!=MODELRT_ANIM_BIND_POSE&&ai!=0)return MODELRT_ERR_BAD_ARG;
 if(s->on_model_begin&&s->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
 return s->on_model_end&&s->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
static int lws_anim_count(const modelrt_handle*h)
{return (int)h->info.animation_count;}
static int lws_anim_info(modelrt_handle*h,int index,modelrt_animation_info*out)
{
 const LWS3D_Scene*s=((lwsrt_state*)h->native_state)->scene;
 if(!out||index!=0||!h->info.animation_count)return MODELRT_ERR_BAD_ARG;
 modelrt_zero(out,sizeof(*out));out->index=0u;
 if(s->last_frame<s->first_frame)return MODELRT_ERR_FORMAT;
 out->frame_count=(modelrt_u32)(s->last_frame-s->first_frame+1L);
 out->flags=MODELRT_ANIM_UNBOUND;
 modelrt_copy_string(out->name,sizeof(out->name),"LightWave scene timeline");
 return MODELRT_OK;
}
const LWS3D_Scene*modelrt_native_lws89(const modelrt_handle*h)
{return h&&h->is_open&&h->info.codec_id==94u?((lwsrt_state*)h->native_state)->scene:0;}
const modelrt_provider modelrt_provider_native_lws89={94u,"LightWave LWS native",lws_probe,lws_open,0,lws_visit,lws_anim_count,lws_anim_info,0,0,0,0,0,0,0};
