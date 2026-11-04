/* VRML 1.0 native scene parser. Uses its original fixed-point scene layout. */
#include "../modelrt_internal.h"
#include "vr1_parser.h"
#include <string.h>
typedef struct vr1_rt_state_s {vr1_scene*scene;vr1_parser_context*parser;} vr1_rt_state;
static int vr1_probe(const char*p,const modelrt_u8*h,size_t n)
{
    if(!modelrt_ext_eq(p,".wrl"))return MODELRT_PROBE_NO;
    if(n>=12u && memcmp(h,"#VRML V1.0",10u)==0)return MODELRT_PROBE_YES;
    return MODELRT_PROBE_NO; /* do not steal VRML97 files */
}
static int vr1_open(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    modelrt_u8*file;size_t len;vr1_rt_state*s;int rc;(void)rt;
    rc=modelrt_read_file_arena(h,p,&file,&len);if(rc)return rc;
    if(len<12u||len>VR1_MAX_INPUT_BYTES||memcmp(file,"#VRML V1.0",10u)!=0)
        return MODELRT_ERR_FORMAT;
    s=(vr1_rt_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
    if(!s)return MODELRT_ERR_NO_SPACE;
    s->scene=(vr1_scene*)modelrt_arena_alloc(h,sizeof(vr1_scene),sizeof(void*));
    s->parser=(vr1_parser_context*)modelrt_arena_alloc(h,sizeof(vr1_parser_context),sizeof(void*));
    if(!s->scene||!s->parser)return MODELRT_ERR_NO_SPACE;
    rc=vr1_parse_buffer(s->parser,s->scene,(const char*)file,(long)len);
    if(rc!=0||s->scene->error_count>0)return MODELRT_ERR_FORMAT;
    h->native_state=s;
    h->info.mesh_count=s->scene->triangle_count?1u:0u;
    h->info.material_count=(modelrt_u32)s->scene->material_count;
    h->info.capabilities=MODELRT_CAP_NATIVE_SCENE;
    if(s->scene->triangle_count)h->info.capabilities|=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NORMALS;
    if(s->scene->material_count)h->info.capabilities|=MODELRT_CAP_MATERIALS;
    if(s->scene->camera_count)h->info.capabilities|=MODELRT_CAP_CAMERAS;
    if(s->scene->light_count)h->info.capabilities|=MODELRT_CAP_LIGHTS;
    modelrt_copy_string(h->info.name,sizeof(h->info.name),p);
    return MODELRT_OK;
}
static int vr1_visit(modelrt_handle*h,int anim,modelrt_q16 time,const modelrt_visit_sink*sink,void*u)
{
    const vr1_rt_state*s;const vr1_scene*m;modelrt_mesh_info mi;long i;int k;
    (void)time;
    if(!h||!h->native_state||!sink)return MODELRT_ERR_BAD_ARG;
    if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
    s=(const vr1_rt_state*)h->native_state;m=s->scene;
    if(sink->on_model_begin&&sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
    if(m->triangle_count){
        modelrt_zero(&mi,sizeof(mi));mi.vertex_count=(modelrt_u32)m->vertex_count;
        mi.triangle_count=(modelrt_u32)m->triangle_count;
        mi.material_count=(modelrt_u32)m->material_count;
        modelrt_copy_string(mi.name,sizeof(mi.name),"VRML 1.0 mesh");
        if(sink->on_mesh_begin&&sink->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;
        for(i=0;i<m->triangle_count;i++){
            const vr1_triangle*f=&m->triangles[i];long idx[3];modelrt_triangle_view t;
            idx[0]=f->a;idx[1]=f->b;idx[2]=f->c;
            modelrt_zero(&t,sizeof(t));t.material_index=f->material;
            for(k=0;k<3;k++){
                const vr1_vertex*v;
                if(idx[k]<0||idx[k]>=m->vertex_count)return MODELRT_ERR_FORMAT;
                v=&m->vertices[idx[k]];
                t.v[k].position.x=v->pos.x;t.v[k].position.y=v->pos.y;t.v[k].position.z=v->pos.z;
                t.v[k].normal.x=v->normal.x;t.v[k].normal.y=v->normal.y;t.v[k].normal.z=v->normal.z;
                t.v[k].uv0.x=v->uv.x;t.v[k].uv0.y=v->uv.y;
                t.v[k].flags=MODELRT_VTX_UV0|MODELRT_VTX_NORMAL;
            }
            if(sink->on_triangle&&sink->on_triangle(u,&t))return MODELRT_ERR_CALLBACK;
        }
        if(sink->on_mesh_end&&sink->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;
    }
    if(sink->on_model_end&&sink->on_model_end(u,&h->info))return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_native_vrml1_89={
    57u,"VRML1 native",vr1_probe,vr1_open,0,vr1_visit,0,0,0,0,0,0,0,0,0
};
const void *modelrt_native_vrml1_89(const modelrt_handle*h)
{
    const vr1_rt_state*s;
    if(!h||!h->is_open||h->info.codec_id!=57u)return 0;
    s=(const vr1_rt_state*)h->native_state;return s?s->scene:0;
}
