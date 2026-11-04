/* Native VRML97 XML-free WRL decoder; direct triangle visitor, C89. */
#include "../modelrt_internal.h"
#include "vrml97_codec.h"
#include <string.h>
typedef struct vr97_state_s {vrml97_scene *scene;} vr97_state;
static int vr97_probe(const char*p,const modelrt_u8*h,size_t n)
{
    if(!modelrt_ext_eq(p,".wrl"))return MODELRT_PROBE_NO;
    if(n>=15u&&memcmp(h,"#VRML V2.0",10u)==0)return MODELRT_PROBE_YES;
    return MODELRT_PROBE_NO;
}
static int vr97_open(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    modelrt_u8*data;size_t len;int rc;vr97_state*s;(void)rt;
    rc=modelrt_read_file_arena(h,p,&data,&len);if(rc)return rc;
    if(len<15u||len>0x7fffffffUL||memcmp(data,"#VRML V2.0",10u)!=0)return MODELRT_ERR_FORMAT;
    s=(vr97_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));if(!s)return MODELRT_ERR_NO_SPACE;
    s->scene=(vrml97_scene*)modelrt_arena_alloc(h,sizeof(vrml97_scene),sizeof(void*));
    if(!s->scene)return MODELRT_ERR_NO_SPACE;
    rc=vrml97_parse(s->scene,(const char*)data,(unsigned long)len);
    if(rc!=VRML97_OK)return rc==VRML97_ERR_OVERFLOW?MODELRT_ERR_LIMIT:MODELRT_ERR_FORMAT;
    if(s->scene->warnings&VRML97_WARN_LIMIT_REACHED)return MODELRT_ERR_LIMIT;
    h->native_state=s;
    h->info.mesh_count=(modelrt_u32)s->scene->mesh_count;
    h->info.material_count=(modelrt_u32)s->scene->material_count;
    h->info.texture_count=(modelrt_u32)s->scene->texture_count;
    h->info.capabilities=MODELRT_CAP_NATIVE_SCENE;
    if(s->scene->triangles_count)h->info.capabilities|=MODELRT_CAP_GEOMETRY;
    if(s->scene->material_count)h->info.capabilities|=MODELRT_CAP_MATERIALS;
    if(s->scene->texture_count)h->info.capabilities|=MODELRT_CAP_TEXTURES|MODELRT_CAP_EXTERNAL_TEX;
    if(s->scene->viewpoint_count)h->info.capabilities|=MODELRT_CAP_CAMERAS;
    if(s->scene->light_count)h->info.capabilities|=MODELRT_CAP_LIGHTS;
    modelrt_copy_string(h->info.name,sizeof(h->info.name),p);
    return MODELRT_OK;
}
static int vr97_visit(modelrt_handle*h,int anim,modelrt_q16 time,const modelrt_visit_sink*sink,void*u)
{
    const vr97_state*s;const vrml97_scene*m;int mesh;unsigned long i;
    (void)time;
    if(!h||!h->native_state||!sink)return MODELRT_ERR_BAD_ARG;
    if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
    s=(const vr97_state*)h->native_state;m=s->scene;
    if(sink->on_model_begin&&sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
    for(mesh=0;mesh<m->mesh_count;mesh++){
        const vrml97_mesh*me=&m->meshes[mesh];modelrt_mesh_info mi;
        if(me->first_triangle>m->triangles_count||me->triangle_count>m->triangles_count-me->first_triangle)
            return MODELRT_ERR_FORMAT;
        modelrt_zero(&mi,sizeof(mi));mi.index=(modelrt_u32)mesh;mi.triangle_count=(modelrt_u32)me->triangle_count;
        mi.vertex_count=mi.triangle_count*3u;
        modelrt_copy_string(mi.name,sizeof(mi.name),me->name);
        if(sink->on_mesh_begin&&sink->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;
        for(i=me->first_triangle;i<me->first_triangle+me->triangle_count;i++){
            const vrml97_triangle*f=&m->triangles[i];modelrt_triangle_view t;int j;
            modelrt_zero(&t,sizeof(t));t.material_index=f->material;t.mesh_index=(modelrt_u32)mesh;
            if(!f->solid)t.flags|=MODELRT_TRI_DOUBLE_SIDED;
            for(j=0;j<3;j++){
                t.v[j].position.x=(modelrt_q16)f->p[j].x;
                t.v[j].position.y=(modelrt_q16)f->p[j].y;
                t.v[j].position.z=(modelrt_q16)f->p[j].z;
                if(f->has_n){t.v[j].normal.x=(modelrt_q16)f->n[j].x;
                    t.v[j].normal.y=(modelrt_q16)f->n[j].y;
                    t.v[j].normal.z=(modelrt_q16)f->n[j].z;
                    t.v[j].flags|=MODELRT_VTX_NORMAL;}
                if(f->has_tc){t.v[j].uv0.x=(modelrt_q16)f->tc[j].x;
                    t.v[j].uv0.y=(modelrt_q16)f->tc[j].y;
                    t.v[j].flags|=MODELRT_VTX_UV0;}
            }
            if(sink->on_triangle&&sink->on_triangle(u,&t))return MODELRT_ERR_CALLBACK;
        }
        if(sink->on_mesh_end&&sink->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;
    }
    if(sink->on_model_end&&sink->on_model_end(u,&h->info))return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
static int vr97_tcount(const modelrt_handle*h){return (int)h->info.texture_count;}
static int vr97_tinfo(modelrt_handle*h,int idx,modelrt_texture_info*out)
{
    const vr97_state*s;
    if(!h||!h->native_state||!out||idx<0||idx>=(int)h->info.texture_count)return MODELRT_ERR_BAD_ARG;
    s=(const vr97_state*)h->native_state;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)idx;out->kind=MODELRT_TEXTURE_EXTERNAL;
    modelrt_copy_string(out->name,sizeof(out->name),s->scene->textures[idx].name);
    modelrt_copy_string(out->path,sizeof(out->path),s->scene->textures[idx].url);
    modelrt_copy_string(out->relative_path,sizeof(out->relative_path),s->scene->textures[idx].url);
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_native_vrml97_89={
    58u,"VRML97 native",vr97_probe,vr97_open,0,vr97_visit,0,0,vr97_tcount,vr97_tinfo,0,0,0,0,0
};
const void *modelrt_native_vrml97_89(const modelrt_handle*h)
{
    const vr97_state*s;
    if(!h||!h->is_open||h->info.codec_id!=58u)return 0;
    s=(const vr97_state*)h->native_state;return s?s->scene:0;
}
