/* Direct STL decoder; reuses the original C89 vendor streaming API. */
#include "../modelrt_internal.h"
#include "stl3d_decoder.h"
#include <string.h>

typedef struct exp_stl_file_s {
 const stl3d_u8 *data;size_t size;size_t pos;
} exp_stl_file;
typedef struct exp_stl_state_s {
 modelrt_u8 *bytes;size_t size;modelrt_u32 triangles;
} exp_stl_state;
typedef struct exp_stl_visit_s {
 const modelrt_visit_sink *sink;void *user;int failed;modelrt_u32 count;
} exp_stl_visit;
static int stl_probe(const char*p,const modelrt_u8*h,size_t n){(void)h;(void)n;return modelrt_ext_eq(p,".stl")?MODELRT_PROBE_YES:0;}
static int stl_read(void*u,stl3d_u8*dst,unsigned long cap,unsigned long*got){
 exp_stl_file*s=(exp_stl_file*)u;size_t n=s->size-s->pos;
 if(n>(size_t)cap)n=(size_t)cap;
 if(n)memcpy(dst,s->data+s->pos,n);
 s->pos+=n;*got=(unsigned long)n;return STL3D_OK;
}
static int stl_open(modelrt_runtime*rt,modelrt_handle*h,const char*path){
 exp_stl_state*s;exp_stl_file f;stl3d_ctx ctx;stl3d_stats stats;stl3d_u8 buf[4096];int rc;
 (void)rt;s=(exp_stl_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));if(!s)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(s,sizeof(*s));rc=modelrt_read_file_arena(h,path,&s->bytes,&s->size);if(rc)return rc;
 if(s->size>0xffffffffUL||s->size<6u)return MODELRT_ERR_FORMAT;
 f.data=s->bytes;f.size=s->size;f.pos=0u;
 rc=stl3d_init(&ctx,stl_read,&f,buf,sizeof(buf));if(rc!=STL3D_OK)return MODELRT_ERR_CODEC;
 stl3d_set_total_size(&ctx,(stl3d_u32)s->size);
 rc=stl3d_decode_all(&ctx,0,0,&stats);if(rc!=STL3D_OK)return MODELRT_ERR_FORMAT;
 if(stats.triangles>0xffffffffUL)return MODELRT_ERR_LIMIT;
 s->triangles=(modelrt_u32)stats.triangles;
 h->info.mesh_count=s->triangles?1u:0u;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NORMALS;
 if(stats.viscam_color_facets||stats.magics_color_facets)h->info.capabilities|=MODELRT_CAP_VERTEX_COLOR;
 h->native_state=s;modelrt_copy_string(h->info.name,sizeof(h->info.name),path);return MODELRT_OK;
}
static modelrt_q16 stl_q(long x){
 if(x>2147483647L)return 2147483647L;
 if(x<(-2147483647L-1L))return (-2147483647-1);
 return (modelrt_q16)x;
}
static int stl_tri_cb(void*u,const stl3d_triangle*src){
 exp_stl_visit*v=(exp_stl_visit*)u;modelrt_triangle_view tv;int k;
 modelrt_zero(&tv,sizeof(tv));tv.material_index=-1;
 for(k=0;k<3;k++){
  tv.v[k].position.x=stl_q(src->v[k].x);
  tv.v[k].position.y=stl_q(src->v[k].y);
  tv.v[k].position.z=stl_q(src->v[k].z);
  tv.v[k].normal.x=stl_q(src->normal.x);
  tv.v[k].normal.y=stl_q(src->normal.y);
  tv.v[k].normal.z=stl_q(src->normal.z);
  tv.v[k].flags=MODELRT_VTX_NORMAL;
  if(src->color_magics.valid){
   tv.v[k].rgba[0]=src->color_magics.r;tv.v[k].rgba[1]=src->color_magics.g;
   tv.v[k].rgba[2]=src->color_magics.b;tv.v[k].rgba[3]=src->color_magics.a;
   tv.v[k].flags|=MODELRT_VTX_COLOR;
  }else if(src->color_viscam.valid){
   tv.v[k].rgba[0]=src->color_viscam.r;tv.v[k].rgba[1]=src->color_viscam.g;
   tv.v[k].rgba[2]=src->color_viscam.b;tv.v[k].rgba[3]=src->color_viscam.a;
   tv.v[k].flags|=MODELRT_VTX_COLOR;
  }
 }
 v->count++;
 if(v->sink->on_triangle&&v->sink->on_triangle(v->user,&tv)){v->failed=1;return STL3D_ERR_ARG;}
 return STL3D_OK;
}
static int stl_visit(modelrt_handle*h,int anim,modelrt_q16 t,const modelrt_visit_sink*sink,void*user){
 exp_stl_state*s;exp_stl_file f;stl3d_ctx ctx;stl3d_u8 buf[4096];exp_stl_visit st;modelrt_mesh_info mi;int rc;
 (void)t;if(!h||!sink||!h->native_state)return MODELRT_ERR_BAD_ARG;
 if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 s=(exp_stl_state*)h->native_state;
 if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
 if(s->triangles){
  modelrt_zero(&mi,sizeof(mi));mi.index=0u;mi.vertex_count=s->triangles*3u;mi.triangle_count=s->triangles;
  modelrt_copy_string(mi.name,sizeof(mi.name),"STL native");
  if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&mi))return MODELRT_ERR_CALLBACK;
  f.data=s->bytes;f.size=s->size;f.pos=0u;
  rc=stl3d_init(&ctx,stl_read,&f,buf,sizeof(buf));if(rc!=STL3D_OK)return MODELRT_ERR_CODEC;
  stl3d_set_total_size(&ctx,(stl3d_u32)s->size);
  st.sink=sink;st.user=user;st.count=0u;st.failed=0;
  rc=stl3d_decode_all(&ctx,stl_tri_cb,&st,0);
  if(st.failed)return MODELRT_ERR_CALLBACK;
  if(rc!=STL3D_OK||st.count!=s->triangles)return MODELRT_ERR_FORMAT;
  if(sink->on_mesh_end&&sink->on_mesh_end(user,&mi))return MODELRT_ERR_CALLBACK;
 }
 if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
const modelrt_provider modelrt_provider_gackt_stl89={MODELRT_CODEC_GACKT_STL,"GACKT STL (native)",stl_probe,stl_open,0,stl_visit,0,0,0,0,0,0,0,0,0};

const unsigned char *modelrt_native_stl_bytes89(const modelrt_handle *h,size_t *bytes){
 const exp_stl_state *s;
 if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_GACKT_STL)return 0;
 s=(const exp_stl_state*)h->native_state;if(!s)return 0;
 if(bytes)*bytes=s->size;
 return s->bytes;
}
