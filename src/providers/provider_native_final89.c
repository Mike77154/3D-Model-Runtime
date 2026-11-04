/* Native C89 providers. No cooked/interchange layer and no heap.
   Each library owns format semantics; runtime owns only the temporary views. */
#include "../modelrt_internal.h"
#include "mot89.h"
#include "step89.h"
#include "scn3d.h"
#include <string.h>
#include <limits.h>

#define FINAL_MOT_ID 97u
#define FINAL_STEP_ID 98u
#define FINAL_SCN_ID 99u
#define FINAL_SCN_MAX_VERTICES 65536UL

typedef struct final_mot_state_s {
    mot89_motion *motion;
    mot89_report report;
} final_mot_state;
static int final_mot_probe(const char *p, const modelrt_u8 *b, size_t n)
{
    (void)b; (void)n;
    return modelrt_ext_eq(p,".mot")||modelrt_ext_eq(p,".env") ? MODELRT_PROBE_MAYBE : MODELRT_PROBE_NO;
}
static int final_mot_open(modelrt_runtime *rt, modelrt_handle *h, const char *path)
{
    modelrt_u8 *bytes; size_t n; int rc;
    mot89_arena a; final_mot_state *s; void *mem;
    (void)rt;
    rc=modelrt_read_file_arena(h,path,&bytes,&n);if(rc)return rc;
    if(!n || n>1048576u)return MODELRT_ERR_LIMIT;
    s=(final_mot_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
    mem=modelrt_arena_alloc(h,2u*1024u*1024u,sizeof(void*));
    if(!s||!mem)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));mot89_arena_init(&a,mem,2u*1024u*1024u);
    if(mot89_parse((const char*)bytes,n,&a,&s->motion)!=MOT89_OK || !s->motion)return MODELRT_ERR_FORMAT;
    if(mot89_validate(s->motion,&s->report)!=MOT89_OK || s->motion->key_count<=0)return MODELRT_ERR_FORMAT;
    h->native_state=s;h->info.animation_count=1u;
    h->info.capabilities=MODELRT_CAP_ANIMATION|MODELRT_CAP_NATIVE_SCENE;
    return MODELRT_OK;
}
static int final_mot_visit(modelrt_handle *h,int ai,modelrt_q16 time,const modelrt_visit_sink *sink,void *u)
{
    long values[MOT89_MAX_CHANNELS];final_mot_state *s=(final_mot_state*)h->native_state;
    if(ai!=MODELRT_ANIM_BIND_POSE && ai!=0)return MODELRT_ERR_BAD_ARG;
    if(ai==0 && mot89_sample_all(s->motion,(long)time,values)!=MOT89_OK)return MODELRT_ERR_CODEC;
    if(sink->on_model_begin&&sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
    /* MOT/ENV are channels, not a mesh or skeleton. Source keys are exposed through native accessor. */
    return sink->on_model_end&&sink->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
static int final_mot_count(const modelrt_handle *h){(void)h;return 1;}
static int final_mot_info(modelrt_handle *h,int i,modelrt_animation_info *out)
{
    const final_mot_state *s=(const final_mot_state*)h->native_state;
    if(i!=0 || !out)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=0;out->frame_count=(modelrt_u32)s->motion->key_count;
    out->source_key_count=(modelrt_u32)s->motion->key_count;
    out->start_time=(modelrt_q16)s->report.first_time;out->end_time=(modelrt_q16)s->report.last_time;
    out->flags=MODELRT_ANIM_UNBOUND;
    modelrt_copy_string(out->name,sizeof(out->name),"LightWave MOT/ENV channels");return MODELRT_OK;
}
const mot89_motion *modelrt_native_mot89(const modelrt_handle *h)
{return h&&h->is_open&&h->info.codec_id==FINAL_MOT_ID?((final_mot_state*)h->native_state)->motion:0;}
const modelrt_provider modelrt_provider_native_mot89={FINAL_MOT_ID,"LightWave MOT/ENV native",final_mot_probe,final_mot_open,0,final_mot_visit,final_mot_count,final_mot_info,0,0,0,0,0,0,0};

typedef struct final_step_state_s {step89_model *model;} final_step_state;
static int final_step_probe(const char*p,const modelrt_u8*b,size_t n)
{
    if(!modelrt_ext_eq(p,".step")&&!modelrt_ext_eq(p,".stp")&&!modelrt_ext_eq(p,".exp"))return MODELRT_PROBE_NO;
    if(n>=13u && !memcmp(b,"ISO-10303-21;",13u))return MODELRT_PROBE_YES;
    return MODELRT_PROBE_MAYBE;
}
static int final_step_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
    modelrt_u8 *bytes;size_t n;int rc;step89_model *model;final_step_state*s;(void)rt;
    rc=modelrt_read_file_arena(h,path,&bytes,&n);if(rc)return rc;
    if(!n||n>10485760u||n>(size_t)INT_MAX)return MODELRT_ERR_LIMIT;
    model=(step89_model*)modelrt_arena_alloc(h,sizeof(*model),sizeof(void*));
    s=(final_step_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
    if(!model||!s)return MODELRT_ERR_NO_SPACE;
    step89_init(model);rc=step89_parse(model,(const char*)bytes,(int)n);
    if(rc!=STEP89_OK||model->entity_count<=0)return MODELRT_ERR_FORMAT;
    /* B-rep/topology remains native; no fabricated triangle tessellation. */
    (void)step89_extract_geometry(model);
    s->model=model;h->native_state=s;h->info.capabilities=MODELRT_CAP_NATIVE_SCENE;
    return MODELRT_OK;
}
static int final_step_visit(modelrt_handle *h,int ai,modelrt_q16 t,const modelrt_visit_sink *sink,void*u)
{
    (void)t;if(ai!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
    if(sink->on_model_begin&&sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
    return sink->on_model_end&&sink->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
const step89_model *modelrt_native_step89(const modelrt_handle *h)
{return h&&h->is_open&&h->info.codec_id==FINAL_STEP_ID?((final_step_state*)h->native_state)->model:0;}
const modelrt_provider modelrt_provider_native_step89={FINAL_STEP_ID,"STEP ISO10303 native BREP",final_step_probe,final_step_open,0,final_step_visit,0,0,0,0,0,0,0,0,0};

typedef struct final_memio_s {const modelrt_u8 *bytes;size_t size;size_t pos;} final_memio;
static long final_read(void*u,void*dst,long n)
{
    final_memio *m=(final_memio*)u;size_t sz;
    if(n<=0)return 0;sz=(size_t)n;
    if(sz>m->size-m->pos)sz=m->size-m->pos;
    if(sz)memcpy(dst,m->bytes+m->pos,sz);m->pos+=sz;return (long)sz;
}
static int final_seek(void*u,long at){final_memio*m=(final_memio*)u;if(at<0||(size_t)at>m->size)return 0;m->pos=(size_t)at;return 1;}
static long final_tell(void*u){return (long)((final_memio*)u)->pos;}
static long final_size(void*u){return (long)((final_memio*)u)->size;}
typedef struct final_scn_state_s {
    modelrt_u8 *bytes;size_t n;
    scn3d_fix (*vertices)[3];
    modelrt_u32 capacity;
    modelrt_u32 vertex_count;modelrt_u32 triangle_count;
    long object_id;int has_object;int overflow;
    const modelrt_visit_sink *sink;void *user;
} final_scn_state;
static void final_scn_vert(void *u,long obj,long index,scn3d_fix x,scn3d_fix y,scn3d_fix z)
{
    final_scn_state*s=(final_scn_state*)u;
    if(index<0||(unsigned long)index>=s->capacity){s->overflow=1;return;}
    if(!s->has_object||s->object_id!=obj){s->vertex_count=0u;s->object_id=obj;s->has_object=1;}
    s->vertices[index][0]=x;s->vertices[index][1]=y;s->vertices[index][2]=z;
    if((modelrt_u32)index>=s->vertex_count)s->vertex_count=(modelrt_u32)index+1u;
}
static void final_scn_triangle(void *u,long obj,long mat,long a,long b,long c)
{
    final_scn_state*s=(final_scn_state*)u;
    modelrt_triangle_view tr;long ix[3];int i;
    ix[0]=a;ix[1]=b;ix[2]=c;
    if(!s->has_object||s->object_id!=obj){s->overflow=1;return;}
    for(i=0;i<3;i++)if(ix[i]<0||(unsigned long)ix[i]>=s->vertex_count){s->overflow=1;return;}
    s->triangle_count++;
    if(!s->sink||!s->sink->on_triangle)return;
    modelrt_zero(&tr,sizeof(tr));tr.material_index=(modelrt_i32)mat;
    for(i=0;i<3;i++){
        tr.v[i].position.x=(modelrt_q16)s->vertices[ix[i]][0];
        tr.v[i].position.y=(modelrt_q16)s->vertices[ix[i]][1];
        tr.v[i].position.z=(modelrt_q16)s->vertices[ix[i]][2];
    }
    if(s->sink->on_triangle(s->user,&tr))s->overflow=1;
}
static int final_scn_scan(final_scn_state *s)
{
    final_memio mem;scn3d_io io;scn3d_options opt;scn3d_callbacks cb;int rc;
    mem.bytes=s->bytes;mem.size=s->n;mem.pos=0u;
    modelrt_zero(&io,sizeof(io));io.user=&mem;io.read=final_read;io.seek=final_seek;io.tell=final_tell;io.size=final_size;
    scn3d_default_options(&opt);opt.triangulate_faces=1;opt.emit_volume_cubes=1;opt.max_volume_voxels=8192;
    modelrt_zero(&cb,sizeof(cb));cb.user=s;cb.on_vertex=final_scn_vert;cb.on_triangle=final_scn_triangle;
    s->has_object=0;s->vertex_count=0u;s->triangle_count=0u;s->overflow=0;
    rc=scn3d_decode(&io,&opt,&cb);
    if(s->overflow)return MODELRT_ERR_LIMIT;
    return rc==SCN3D_OK?MODELRT_OK:MODELRT_ERR_FORMAT;
}
static int final_scn_probe(const char*p,const modelrt_u8*b,size_t n)
{(void)b;(void)n;return modelrt_ext_eq(p,".scn")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int final_scn_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
    final_scn_state *s;modelrt_u8 *data;size_t n;int rc;(void)rt;
    rc=modelrt_read_file_arena(h,path,&data,&n);if(rc)return rc;
    if(n==0u||n>(size_t)LONG_MAX)return MODELRT_ERR_FORMAT;
    s=(final_scn_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
    if(!s)return MODELRT_ERR_NO_SPACE;modelrt_zero(s,sizeof(*s));
    s->bytes=data;s->n=n;s->capacity=FINAL_SCN_MAX_VERTICES;
    s->vertices=(scn3d_fix(*)[3])modelrt_arena_alloc(h,(size_t)FINAL_SCN_MAX_VERTICES*3u*sizeof(scn3d_fix),sizeof(void*));
    if(!s->vertices)return MODELRT_ERR_NO_SPACE;
    rc=final_scn_scan(s);if(rc)return rc;
    if(!s->triangle_count)return MODELRT_ERR_UNSUPPORTED;
    h->native_state=s;h->info.mesh_count=1u;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE;
    return MODELRT_OK;
}
static int final_scn_visit(modelrt_handle *h,int ai,modelrt_q16 t,const modelrt_visit_sink*sink,void*u)
{
    final_scn_state*s=(final_scn_state*)h->native_state;modelrt_mesh_info mi;int rc;(void)t;
    if(ai!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
    if(sink->on_model_begin&&sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
    modelrt_zero(&mi,sizeof(mi));mi.triangle_count=s->triangle_count;modelrt_copy_string(mi.name,sizeof(mi.name),"SCN native mesh");
    if(sink->on_mesh_begin&&sink->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;
    s->sink=sink;s->user=u;rc=final_scn_scan(s);s->sink=0;s->user=0;
    if(rc)return rc;
    if(sink->on_mesh_end&&sink->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;
    return sink->on_model_end&&sink->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
const modelrt_provider modelrt_provider_native_scn89={FINAL_SCN_ID,"SCN native mesh",final_scn_probe,final_scn_open,0,final_scn_visit,0,0,0,0,0,0,0,0,0};
