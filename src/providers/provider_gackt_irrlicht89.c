/* Direct Irrlicht .irrmesh and .irr parser.  Source codec: irrfxlib_v3.
 * No GACKT adapter or intermediate serialization; caller supplies arena.
 * .irr scenes retain native node hierarchy and attributes (no fabricated mesh).
 */
#include "../modelrt_internal.h"
#include "ir_mesh.h"
#include "ir_scene.h"
#include "ir_validate.h"
#include <string.h>

typedef struct exp_ir_state_s {
 int scene_mode;
 unsigned char *data; size_t size;
 ir_mesh_doc mesh;
 ir_scene_doc scene;
} exp_ir_state;
static size_t ircap(size_t n,size_t d,size_t min,size_t max){size_t k=n/d+32u;if(k<min)k=min;if(k>max)k=max;return k;}
static void *ira(modelrt_handle*h,size_t cnt,size_t sz){if(sz==0u||cnt>((size_t)-1)/sz)return 0;return modelrt_arena_alloc(h,cnt*sz,sizeof(void*));}
static modelrt_q16 irq16(long q){if(q>2147483647L)return 2147483647;if(q<(-2147483647L-1L))return (-2147483647-1);return (modelrt_q16)q;}
static int ir_probe(const char*path,const modelrt_u8 *head,size_t n){
 (void)head;(void)n;return (modelrt_ext_eq(path,".irrmesh")||modelrt_ext_eq(path,".irr"))?MODELRT_PROBE_YES:0;
}
static int ir_open(modelrt_runtime*rt,modelrt_handle*h,const char*path){
 exp_ir_state*s;int rc;char error[256];(void)rt;
 s=(exp_ir_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
 if(!s)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(s,sizeof(*s));
 rc=modelrt_read_file_arena(h,path,&s->data,&s->size);if(rc)return rc;
 s->scene_mode=modelrt_ext_eq(path,".irr")?1:0;
 if(s->scene_mode){
  ir_scene_storage st;size_t n=s->size;modelrt_zero(&st,sizeof(st));
  st.node_cap=ircap(n,200,32,32768);st.material_cap=ircap(n,160,32,32768);
  st.animator_cap=ircap(n,160,32,32768);st.attr_cap=ircap(n,35,64,262144);
  st.string_cap=ircap(n,1,512,16777216);
  st.nodes=(ir_scene_node*)ira(h,st.node_cap,sizeof(ir_scene_node));
  st.materials=(ir_scene_material*)ira(h,st.material_cap,sizeof(ir_scene_material));
  st.animators=(ir_scene_animator*)ira(h,st.animator_cap,sizeof(ir_scene_animator));
  st.attrs=(ir_attr*)ira(h,st.attr_cap,sizeof(ir_attr));
  st.string_pool=(char*)ira(h,st.string_cap,1u);
  if(!st.nodes||!st.materials||!st.animators||!st.attrs||!st.string_pool)return MODELRT_ERR_NO_SPACE;
  ir_scene_doc_init(&s->scene,&st);
  rc=ir_scene_parse_memory(&s->scene,s->data,s->size,error,sizeof(error));
  if(rc!=IR_OK)return rc==IR_ERR_CAPACITY?MODELRT_ERR_LIMIT:MODELRT_ERR_FORMAT;
  h->info.capabilities=MODELRT_CAP_NATIVE_SCENE;
  h->info.material_count=(modelrt_u32)s->scene.material_count;
  /* A .irr is a scene description; referenced .irrmesh files are external.
     We expose native scene nodes rather than falsely report triangles. */
 }else{
  ir_mesh_storage st;size_t n=s->size;ir_u32 tri_count=0;size_t i;
  modelrt_zero(&st,sizeof(st));
  st.buffer_cap=ircap(n,512,16,65536);st.material_cap=ircap(n,256,16,65536);
  st.attr_cap=ircap(n,50,64,262144);st.vertex_cap=ircap(n,28,32,400000);
  st.index_cap=ircap(n,5,128,1200000);st.string_cap=ircap(n,1,512,16777216);
  st.buffers=(ir_mesh_buffer*)ira(h,st.buffer_cap,sizeof(ir_mesh_buffer));
  st.materials=(ir_material_block*)ira(h,st.material_cap,sizeof(ir_material_block));
  st.attrs=(ir_attr*)ira(h,st.attr_cap,sizeof(ir_attr));
  st.vertices=(ir_vertex*)ira(h,st.vertex_cap,sizeof(ir_vertex));
  st.indices=(ir_u32*)ira(h,st.index_cap,sizeof(ir_u32));
  st.string_pool=(char*)ira(h,st.string_cap,1u);
  if(!st.buffers||!st.materials||!st.attrs||!st.vertices||!st.indices||!st.string_pool)return MODELRT_ERR_NO_SPACE;
  ir_mesh_doc_init(&s->mesh,&st);
  rc=ir_mesh_parse_memory(&s->mesh,s->data,s->size,error,sizeof(error));
  if(rc!=IR_OK)return rc==IR_ERR_CAPACITY?MODELRT_ERR_LIMIT:MODELRT_ERR_FORMAT;
  for(i=0;i<s->mesh.buffer_count;i++){
    if(ir_mesh_buffer_indices_in_range(&s->mesh,&s->mesh.buffers[i])!=IR_OK)return MODELRT_ERR_FORMAT;
    if(ir_mesh_buffer_triangle_count(&s->mesh.buffers[i],&tri_count)!=IR_OK)return MODELRT_ERR_FORMAT;
  }
  h->info.mesh_count=(modelrt_u32)s->mesh.buffer_count;
  h->info.material_count=(modelrt_u32)s->mesh.material_count;
  h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NORMALS|MODELRT_CAP_VERTEX_COLOR|MODELRT_CAP_MATERIALS|MODELRT_CAP_NATIVE_SCENE;
 }
 h->native_state=s;modelrt_copy_string(h->info.name,sizeof(h->info.name),path);
 return MODELRT_OK;
}
static int ir_visit(modelrt_handle*h,int anim,modelrt_q16 time,const modelrt_visit_sink*sink,void*user){
 exp_ir_state*s;size_t i,j; (void)time;
 if(!h||!h->native_state||!sink)return MODELRT_ERR_BAD_ARG;
 if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 s=(exp_ir_state*)h->native_state;
 if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
 if(!s->scene_mode){
  for(i=0;i<s->mesh.buffer_count;i++){
   const ir_mesh_buffer*b=&s->mesh.buffers[i];modelrt_mesh_info mi;
   modelrt_zero(&mi,sizeof(mi));mi.index=(modelrt_u32)i;mi.vertex_count=(modelrt_u32)b->vertex_count;mi.triangle_count=(modelrt_u32)(b->index_count/3u);mi.material_count=b->material_index==IR_MESH_NO_MATERIAL?0u:1u;
   modelrt_copy_string(mi.name,sizeof(mi.name),"Irrlicht mesh buffer");
   if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&mi))return MODELRT_ERR_CALLBACK;
   for(j=0;j+2u<(size_t)b->index_count;j+=3u){
    modelrt_triangle_view tv;int k;modelrt_zero(&tv,sizeof(tv));tv.mesh_index=(modelrt_u32)i;
    tv.material_index=b->material_index==IR_MESH_NO_MATERIAL?-1:(modelrt_i32)b->material_index;
    for(k=0;k<3;k++){
      ir_u32 ind;const ir_vertex*v;modelrt_vertex_view*dst=&tv.v[k];ir_u32 c;
      if(ir_mesh_buffer_index_at(&s->mesh,b,j+(size_t)k,&ind)!=IR_OK)return MODELRT_ERR_FORMAT;
      v=ir_mesh_buffer_vertex_at(&s->mesh,b,(size_t)ind);if(!v)return MODELRT_ERR_FORMAT;
      dst->position.x=irq16(v->pos.x);dst->position.y=irq16(v->pos.y);dst->position.z=irq16(v->pos.z);
      dst->normal.x=irq16(v->normal.x);dst->normal.y=irq16(v->normal.y);dst->normal.z=irq16(v->normal.z);
      dst->uv0.x=irq16(v->uv1.x);dst->uv0.y=irq16(v->uv1.y);
      c=v->color_argb;dst->rgba[0]=(modelrt_u8)((c>>16)&255u);dst->rgba[1]=(modelrt_u8)((c>>8)&255u);
      dst->rgba[2]=(modelrt_u8)(c&255u);dst->rgba[3]=(modelrt_u8)((c>>24)&255u);
      dst->flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0|MODELRT_VTX_COLOR;
    }
    if(sink->on_triangle&&sink->on_triangle(user,&tv))return MODELRT_ERR_CALLBACK;
   }
   if(sink->on_mesh_end&&sink->on_mesh_end(user,&mi))return MODELRT_ERR_CALLBACK;
  }
 }
 if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
const modelrt_provider modelrt_provider_gackt_irr89={MODELRT_CODEC_GACKT_IRRLICHT,"GACKT Irrlicht (native)",ir_probe,ir_open,0,ir_visit,0,0,0,0,0,0,0,0,0};

const ir_mesh_doc *modelrt_native_irrmesh89(const modelrt_handle *h){
 const exp_ir_state *s;
 if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_GACKT_IRRLICHT)return 0;
 s=(const exp_ir_state*)h->native_state;return s&&!s->scene_mode?&s->mesh:0;
}
const ir_scene_doc *modelrt_native_irrscene89(const modelrt_handle *h){
 const exp_ir_state *s;
 if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_GACKT_IRRLICHT)return 0;
 s=(const exp_ir_state*)h->native_state;return s&&s->scene_mode?&s->scene:0;
}
