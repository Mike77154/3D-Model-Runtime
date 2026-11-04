/* Native OGRE XML/binary decoder. Keeps the original OMC_Mesh as authority. */
#include "../modelrt_internal.h"
#include "ogre_mesh_codec.h"
#include <string.h>
typedef struct ogre_rt_state_s {OMC_Mesh *mesh;} ogre_rt_state;
static int ogre_probe(const char*p,const modelrt_u8*h,size_t n)
{
    if(modelrt_path_has_suffix(p,".mesh.xml"))return MODELRT_PROBE_YES;
    if(modelrt_ext_eq(p,".mesh")){
        if(n>=3u && h[0]==0x00u && h[1]==0x10u && h[2]==(modelrt_u8)'[')return MODELRT_PROBE_YES+10;
        return MODELRT_PROBE_MAYBE; /* XPS also uses .mesh */
    }
    return MODELRT_PROBE_NO;
}
static int ogre_open(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    ogre_rt_state*s;modelrt_u8*bytes;size_t len;int rc;(void)rt;
    rc=modelrt_read_file_arena(h,p,&bytes,&len);if(rc)return rc;
    if(len<4u||len>0x7fffffffUL)return MODELRT_ERR_FORMAT;
    s=(ogre_rt_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));if(!s)return MODELRT_ERR_NO_SPACE;
    s->mesh=(OMC_Mesh*)modelrt_arena_alloc(h,sizeof(OMC_Mesh),sizeof(void*));if(!s->mesh)return MODELRT_ERR_NO_SPACE;
    omc_mesh_init(s->mesh);
    if(modelrt_path_has_suffix(p,".mesh.xml"))
        rc=omc_load_mesh_xml_from_memory(s->mesh,(const char*)bytes,(unsigned long)len);
    else rc=omc_load_mesh_binary_from_memory(s->mesh,bytes,(unsigned long)len);
    if(rc!=OMC_OK)return rc==OMC_ERR_CAPACITY?MODELRT_ERR_LIMIT:MODELRT_ERR_FORMAT;
    /* Fail closed if submesh triangle indexes are impossible. Unsupported
       primitive types remain available via native scene only. */
    h->native_state=s;
    h->info.mesh_count=(modelrt_u32)s->mesh->submesh_count;
    h->info.material_count=(modelrt_u32)s->mesh->submesh_count;
    h->info.animation_count=0u; /* no invented skeleton frame sampler */
    h->info.capabilities=MODELRT_CAP_NATIVE_SCENE|MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NORMALS;
    if(s->mesh->submesh_count)h->info.capabilities|=MODELRT_CAP_MATERIALS;
    if(s->mesh->lod_level_count)h->info.capabilities|=MODELRT_CAP_LOD;
    modelrt_copy_string(h->info.name,sizeof(h->info.name),p);
    return MODELRT_OK;
}
static int ogre_visit(modelrt_handle*h,int anim,modelrt_q16 time,const modelrt_visit_sink*sink,void*u)
{
    const ogre_rt_state*s;const OMC_Mesh*m;unsigned int i;OMC_U32 j;int k;
    (void)time;
    if(!h||!h->native_state||!sink)return MODELRT_ERR_BAD_ARG;
    if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
    s=(const ogre_rt_state*)h->native_state;m=s->mesh;
    if(sink->on_model_begin&&sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
    for(i=0u;i<m->submesh_count;i++){
        const OMC_Submesh*sm=&m->submeshes[i];modelrt_mesh_info mi;
        modelrt_zero(&mi,sizeof(mi));mi.index=(modelrt_u32)i;mi.material_count=1u;
        mi.vertex_count=sm->vertex_count;mi.triangle_count=sm->index_count/3u;
        modelrt_copy_string(mi.name,sizeof(mi.name),sm->name[0]?sm->name:sm->material);
        if(sink->on_mesh_begin&&sink->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;
        if(sm->index_start>m->index_count || sm->index_count>m->index_count-sm->index_start)
            return MODELRT_ERR_FORMAT;
        if(sm->operation_type!=OMC_OP_TRIANGLE_LIST && sm->operation_type!=OMC_OP_UNKNOWN)
            return MODELRT_ERR_UNSUPPORTED; /* don't fake strip/fan topology */
        if((sm->index_count%3u)!=0u)return MODELRT_ERR_FORMAT;
        for(j=0u;j+2u<sm->index_count;j+=3u){
            modelrt_triangle_view tv;modelrt_zero(&tv,sizeof(tv));
            tv.mesh_index=(modelrt_u32)i;tv.material_index=(modelrt_i32)i;
            for(k=0;k<3;k++){
                OMC_U32 vi=m->indices[sm->index_start+j+(OMC_U32)k];
                const OMC_Vertex*v;
                if(!sm->use_shared_vertices)vi+=sm->vertex_start;
                if(vi>=m->vertex_count)return MODELRT_ERR_FORMAT;
                v=&m->vertices[vi];
                tv.v[k].position.x=(modelrt_q16)v->position.x;
                tv.v[k].position.y=(modelrt_q16)v->position.y;
                tv.v[k].position.z=(modelrt_q16)v->position.z;
                if(v->flags&OMC_VTX_HAS_NORMAL){
                    tv.v[k].normal.x=(modelrt_q16)v->normal.x;
                    tv.v[k].normal.y=(modelrt_q16)v->normal.y;
                    tv.v[k].normal.z=(modelrt_q16)v->normal.z;
                    tv.v[k].flags|=MODELRT_VTX_NORMAL;}
                if(v->flags&OMC_VTX_HAS_TEXCOORD){
                    tv.v[k].uv0.x=(modelrt_q16)v->texcoord[0].u;
                    tv.v[k].uv0.y=(modelrt_q16)v->texcoord[0].v;
                    tv.v[k].flags|=MODELRT_VTX_UV0;}
            }
            if(sink->on_triangle&&sink->on_triangle(u,&tv))return MODELRT_ERR_CALLBACK;
        }
        if(sink->on_mesh_end&&sink->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;
    }
    if(sink->on_model_end&&sink->on_model_end(u,&h->info))return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_native_ogre89={
 59u,"Ogre Mesh native",ogre_probe,ogre_open,0,ogre_visit,0,0,0,0,0,0,0,0,0
};
const void *modelrt_native_ogre89(const modelrt_handle*h)
{
    const ogre_rt_state*s;
    if(!h||!h->is_open||h->info.codec_id!=59u)return 0;
    s=(const ogre_rt_state*)h->native_state;return s?s->mesh:0;
}
