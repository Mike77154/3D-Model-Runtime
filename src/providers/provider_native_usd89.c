/* Direct OpenUSD parser bridge, ISO C89 / Q16; no GACKT language. */
#include "../modelrt_internal.h"
#include "usd3d.h"
#include <string.h>
#define USD_RT_MAX_VERTICES 65536UL
#define USD_RT_CODEC_ID 100u
typedef struct usd_rt_state_s {
    usd3d_vec3 *points;
    usd3d_u32 used;
    usd3d_stats stats;
    modelrt_u32 triangle_count;
    int failed;
    int visiting;
    const modelrt_visit_sink *sink;
    void *user;
} usd_rt_state;
static int usd_begin_mesh(void *u,const char *name)
{
    usd_rt_state*s=(usd_rt_state*)u;
    modelrt_mesh_info mi;s->used=0u;
    if(!s->visiting||!s->sink->on_mesh_begin)return 0;
    modelrt_zero(&mi,sizeof(mi));modelrt_copy_string(mi.name,sizeof(mi.name),name);
    return s->sink->on_mesh_begin(s->user,&mi);
}
static int usd_point(void*u,usd3d_u32 index,usd3d_fixed x,usd3d_fixed y,usd3d_fixed z)
{
    usd_rt_state*s=(usd_rt_state*)u;
    if(index>=USD_RT_MAX_VERTICES){s->failed=1;return 1;}
    s->points[index].x=x;s->points[index].y=y;s->points[index].z=z;
    if(index>=s->used)s->used=index+1u;return 0;
}
static int usd_face(void*u,usd3d_u32 fi,const usd3d_u32 *indices,usd3d_u16 count)
{
    usd_rt_state*s=(usd_rt_state*)u;
    modelrt_triangle_view tr;int j,k;usd3d_u32 v[3];(void)fi;
    if(count<3u || count>USD3D_MAX_NGON){s->failed=1;return 1;}
    for(j=1;j<(int)count-1;j++){
        v[0]=indices[0];v[1]=indices[j];v[2]=indices[j+1];
        if(v[0]>=s->used||v[1]>=s->used||v[2]>=s->used){s->failed=1;return 1;}
        s->triangle_count++;
        if(s->visiting&&s->sink->on_triangle){
            modelrt_zero(&tr,sizeof(tr));
            for(k=0;k<3;k++){
                tr.v[k].position.x=(modelrt_q16)s->points[v[k]].x;
                tr.v[k].position.y=(modelrt_q16)s->points[v[k]].y;
                tr.v[k].position.z=(modelrt_q16)s->points[v[k]].z;
            }
            if(s->sink->on_triangle(s->user,&tr))return 1;
        }
    }
    return 0;
}
static int usd_end_mesh(void*u,const char *name)
{
    usd_rt_state*s=(usd_rt_state*)u;modelrt_mesh_info mi;
    if(!s->visiting||!s->sink->on_mesh_end)return 0;
    modelrt_zero(&mi,sizeof(mi));modelrt_copy_string(mi.name,sizeof(mi.name),name);
    mi.vertex_count=(modelrt_u32)s->used;
    return s->sink->on_mesh_end(s->user,&mi);
}
static int usd_parse(modelrt_handle *h,usd_rt_state *s)
{
    usd3d_callbacks cb;usd3d_config cfg;int rc;
    modelrt_zero(&cb,sizeof(cb));usd3d_default_config(&cfg);
    cfg.triangulate_ngons=0;cfg.emit_points=1;cfg.emit_prims=1;
    cb.begin_mesh=usd_begin_mesh;cb.point=usd_point;cb.face=usd_face;cb.end_mesh=usd_end_mesh;
    s->failed=0;s->triangle_count=0u;modelrt_zero(&s->stats,sizeof(s->stats));
    rc=usd3d_decode_file(h->path,&cfg,&cb,s,&s->stats);
    if(s->failed)return MODELRT_ERR_LIMIT;
    return rc==USD3D_OK?MODELRT_OK:MODELRT_ERR_FORMAT;
}
static int usd_probe(const char*p,const modelrt_u8*b,size_t n)
{
    (void)b;(void)n;return modelrt_ext_eq(p,".usd")||modelrt_ext_eq(p,".usda")||modelrt_ext_eq(p,".usdc")||modelrt_ext_eq(p,".usdz")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;
}
static int usd_open(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    usd_rt_state*s;int rc;(void)rt;(void)p;
    s=(usd_rt_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));if(!s)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));
    s->points=(usd3d_vec3*)modelrt_arena_alloc(h,USD_RT_MAX_VERTICES*sizeof(usd3d_vec3),sizeof(void*));
    if(!s->points)return MODELRT_ERR_NO_SPACE;
    h->native_state=s;rc=usd_parse(h,s);if(rc)return rc;
    if(!s->stats.prims)return MODELRT_ERR_FORMAT;
    h->info.mesh_count=(modelrt_u32)s->stats.meshes;
    h->info.capabilities=MODELRT_CAP_NATIVE_SCENE;
    if(s->triangle_count)h->info.capabilities|=MODELRT_CAP_GEOMETRY;
    return MODELRT_OK;
}
static int usd_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*sink,void*u)
{
    usd_rt_state*s=(usd_rt_state*)h->native_state;int rc;(void)t;
    if(ai!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
    if(sink->on_model_begin&&sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
    s->sink=sink;s->user=u;s->visiting=1;
    rc=usd_parse(h,s);s->visiting=0;s->sink=0;s->user=0;
    if(rc)return rc;
    return sink->on_model_end&&sink->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
const modelrt_provider modelrt_provider_native_usd89={USD_RT_CODEC_ID,"OpenUSD native",usd_probe,usd_open,0,usd_visit,0,0,0,0,0,0,0,0,0};
int modelrt_native_usd_stats89(const modelrt_handle *h,usd3d_stats *out)
{
    if(!h||!h->is_open||h->info.codec_id!=USD_RT_CODEC_ID||!out)return MODELRT_ERR_BAD_ARG;
    *out=((const usd_rt_state*)h->native_state)->stats;
    return MODELRT_OK;
}
