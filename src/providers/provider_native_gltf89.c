/* Native glTF/GLB importer: original accessors, no GACKT-LANG, C89.
 * Supports embedded GLB BIN and sidecar URI buffers, plain triangles,
 * triangle strips and fans. Unsupported compression fails closed.
 * Scene transforms remain accessible in original gltf3d_asset structure. */
#include "../modelrt_internal.h"
#include "gltf3d.h"
#include "gltf3d_util.h"
#include <string.h>

typedef struct gltf_rt_state_s {
    gltf3d_asset *asset;
    modelrt_handle *handle;
} gltf_rt_state;

static int gltf_probe(const char*p,const modelrt_u8*h,size_t n)
{
    if(modelrt_ext_eq(p,".glb"))
        return (n>=4u&&memcmp(h,"glTF",4u)==0)?MODELRT_PROBE_YES:MODELRT_PROBE_MAYBE;
    if(modelrt_ext_eq(p,".gltf"))
        return (n>=1u&&(h[0]=='{'||h[0]==' '||h[0]=='\n'))?MODELRT_PROBE_YES:MODELRT_PROBE_MAYBE;
    return MODELRT_PROBE_NO;
}

/* No archive extraction, network fetching or heap allocation in this path. */
static int gltf_load_uri(void*user,const char*uri,const unsigned char**out,unsigned long*size)
{
    gltf_rt_state *s=(gltf_rt_state*)user;
    modelrt_handle *h;
    size_t plen,ulen,base=0u,n;
    char path[MODELRT_PATH_MAX];modelrt_u8*data;int rc;
    if(!s||!uri||!out||!size)return -1;
    if(strncmp(uri,"data:",5u)==0)return -1; /* vendor URI buffer has 256 chars */
    if(strstr(uri,"://")||uri[0]=='/'||uri[0]=='\\')return -1;
    h=s->handle;plen=strlen(h->path);ulen=strlen(uri);
    for(n=0u;n<plen;n++)if(h->path[n]=='/'||h->path[n]=='\\')base=n+1u;
    if(base+ulen+1u>=sizeof(path))return -1;
    memcpy(path,h->path,base);memcpy(path+base,uri,ulen+1u);
    rc=modelrt_read_file_arena(h,path,&data,&n);
    if(rc!=MODELRT_OK||n>0xffffffffUL)return -1;
    *out=data;*size=(unsigned long)n;
    return 0;
}
static int gltf_open(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    modelrt_u8*data;size_t size;int rc;int i;gltf_rt_state*s;gltf3d_io io;
    (void)rt;
    rc=modelrt_read_file_arena(h,p,&data,&size);if(rc)return rc;
    if(size<4u||size>0x7fffffffUL)return MODELRT_ERR_FORMAT;
    s=(gltf_rt_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
    if(!s)return MODELRT_ERR_NO_SPACE;
    s->asset=(gltf3d_asset*)modelrt_arena_alloc(h,sizeof(gltf3d_asset),sizeof(void*));
    if(!s->asset)return MODELRT_ERR_NO_SPACE;
    s->handle=h;io.user=s;io.load_uri=gltf_load_uri;
    rc=gltf3d_parse_memory(s->asset,data,(unsigned long)size,&io);
    if(rc!=GLTF3D_OK)return rc==GLTF3D_ERR_LIMIT?MODELRT_ERR_LIMIT:MODELRT_ERR_FORMAT;
    if(s->asset->has_required_unsupported)return MODELRT_ERR_UNSUPPORTED;
    for(i=0;i<s->asset->primitive_count;i++){
        const gltf3d_primitive*pr=&s->asset->primitives[i];
        int ai=pr->attributes[GLTF3D_ATTR_POSITION];gltf3d_fx v;
        if(pr->has_draco)return MODELRT_ERR_UNSUPPORTED;
        if(pr->mode==GLTF3D_MODE_TRIANGLES||pr->mode==GLTF3D_MODE_TRIANGLE_STRIP||pr->mode==GLTF3D_MODE_TRIANGLE_FAN){
            if(ai<0||ai>=s->asset->accessor_count)return MODELRT_ERR_FORMAT;
            if(s->asset->accessors[ai].count && gltf3d_accessor_read_fixed(s->asset,ai,0u,0,&v)!=GLTF3D_OK)
                return MODELRT_ERR_FORMAT; /* prevent metadata-only "successful" opens */
        }
    }
    h->native_state=s;
    h->info.mesh_count=(modelrt_u32)s->asset->mesh_count;
    h->info.material_count=(modelrt_u32)s->asset->material_count;
    h->info.texture_count=(modelrt_u32)s->asset->texture_count;
    h->info.capabilities=MODELRT_CAP_NATIVE_SCENE|MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0;
    if(h->info.material_count)h->info.capabilities|=MODELRT_CAP_MATERIALS;
    if(h->info.texture_count)h->info.capabilities|=MODELRT_CAP_TEXTURES|MODELRT_CAP_EXTERNAL_TEX;
    if(s->asset->node_count)h->info.capabilities|=MODELRT_CAP_RIGID_HIERARCHY;
    modelrt_copy_string(h->info.name,sizeof(h->info.name),p);
    return MODELRT_OK;
}
static int gltf_vertex(const gltf3d_asset*a,const gltf3d_primitive*pr,unsigned long vi,modelrt_vertex_view*v)
{
    gltf3d_fx fx;int j,r;int ai=pr->attributes[GLTF3D_ATTR_POSITION];
    modelrt_zero(v,sizeof(*v));
    if(ai<0||ai>=a->accessor_count||vi>=a->accessors[ai].count)return MODELRT_ERR_FORMAT;
    for(j=0;j<3;j++){
        r=gltf3d_accessor_read_fixed(a,ai,vi,j,&fx);
        if(r!=GLTF3D_OK)return MODELRT_ERR_FORMAT;
        if(j==0)v->position.x=(modelrt_q16)fx;
        if(j==1)v->position.y=(modelrt_q16)fx;
        if(j==2)v->position.z=(modelrt_q16)fx;
    }
    ai=pr->attributes[GLTF3D_ATTR_TEXCOORD_0];
    if(ai>=0){
        if(gltf3d_accessor_read_fixed(a,ai,vi,0,&fx)!=GLTF3D_OK)return MODELRT_ERR_FORMAT;
        v->uv0.x=(modelrt_q16)fx;
        if(gltf3d_accessor_read_fixed(a,ai,vi,1,&fx)!=GLTF3D_OK)return MODELRT_ERR_FORMAT;
        v->uv0.y=(modelrt_q16)fx;v->flags|=MODELRT_VTX_UV0;
    }
    ai=pr->attributes[GLTF3D_ATTR_NORMAL];
    if(ai>=0){
        for(j=0;j<3;j++){
            if(gltf3d_accessor_read_fixed(a,ai,vi,j,&fx)!=GLTF3D_OK)return MODELRT_ERR_FORMAT;
            if(j==0)v->normal.x=(modelrt_q16)fx;
            if(j==1)v->normal.y=(modelrt_q16)fx;
            if(j==2)v->normal.z=(modelrt_q16)fx;
        }v->flags|=MODELRT_VTX_NORMAL;
    }
    return MODELRT_OK;
}
static int gltf_visit(modelrt_handle*h,int anim,modelrt_q16 time,const modelrt_visit_sink*sink,void*u)
{
    const gltf_rt_state*s;const gltf3d_asset*a;int i,j;unsigned long t,count,ids[3];
    (void)time;
    if(!h||!h->native_state||!sink)return MODELRT_ERR_BAD_ARG;
    if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
    s=(const gltf_rt_state*)h->native_state;a=s->asset;
    if(sink->on_model_begin&&sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
    for(i=0;i<a->primitive_count;i++){
        const gltf3d_primitive*pr=&a->primitives[i];modelrt_mesh_info mi;
        if(pr->mode!=GLTF3D_MODE_TRIANGLES&&pr->mode!=GLTF3D_MODE_TRIANGLE_STRIP&&pr->mode!=GLTF3D_MODE_TRIANGLE_FAN)
            continue; /* lines and points are natively available, not triangles */
        count=gltf3d_primitive_triangle_count(a,i);
        modelrt_zero(&mi,sizeof(mi));mi.index=(modelrt_u32)i;
        mi.triangle_count=(modelrt_u32)count;
        mi.vertex_count=(modelrt_u32)gltf3d_primitive_vertex_count(a,i);
        modelrt_copy_string(mi.name,sizeof(mi.name),"glTF primitive");
        if(sink->on_mesh_begin&&sink->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;
        for(t=0u;t<count;t++){
            modelrt_triangle_view tv;
            if(gltf3d_primitive_read_triangle(a,i,t,ids)!=GLTF3D_OK)return MODELRT_ERR_FORMAT;
            modelrt_zero(&tv,sizeof(tv));tv.mesh_index=(modelrt_u32)i;tv.material_index=pr->material;
            for(j=0;j<3;j++)if(gltf_vertex(a,pr,ids[j],&tv.v[j])!=MODELRT_OK)return MODELRT_ERR_FORMAT;
            if(sink->on_triangle&&sink->on_triangle(u,&tv))return MODELRT_ERR_CALLBACK;
        }
        if(sink->on_mesh_end&&sink->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;
    }
    if(sink->on_model_end&&sink->on_model_end(u,&h->info))return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_native_gltf89={
  60u,"glTF/GLB native",gltf_probe,gltf_open,0,gltf_visit,0,0,0,0,0,0,0,0,0
};
const void *modelrt_native_gltf89(const modelrt_handle*h)
{
    const gltf_rt_state*s;if(!h||!h->is_open||h->info.codec_id!=60u)return 0;
    s=(const gltf_rt_state*)h->native_state;return s?s->asset:0;
}
