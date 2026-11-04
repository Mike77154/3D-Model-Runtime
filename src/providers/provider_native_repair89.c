/* Native codecs — COLLADA, PSX TMD, Source VTA.
 * No GACKT bridge and no cooked representation. Strict ISO C89.
 */
#include "../modelrt_internal.h"
#include "c3dae.h"
#include "tmd_codec3d.h"
#include "vta_decoder3d.h"
#include "md4_codec.h"
#include "md7_codec.h"
#include "uc3d.h"
#include "u3d3d.h"
#include "p3_egg_parser.h"
#include "p3_egg_model.h"
#include "mf3d.h"
#include "mpk_reader.h"
#include "zlib89.h"
#include <string.h>
#include <limits.h>

typedef struct repair_state_s {
    void *decoded;
    const modelrt_u8 *bytes;
    size_t size;
    void *extra;
} repair_state;

static int repair_read(modelrt_handle *h,const char *path,repair_state **out)
{
    repair_state *s;
    modelrt_u8 *buf;
    size_t n;
    int rc;
    s=(repair_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
    if(!s)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));
    rc=modelrt_read_file_arena(h,path,&buf,&n);
    if(rc!=MODELRT_OK)return rc;
    if(!n||n>2147483647u)return MODELRT_ERR_FORMAT;
    s->bytes=buf;s->size=n;h->native_state=s;*out=s;
    return MODELRT_OK;
}
static int repair_begin(modelrt_handle*h,const modelrt_visit_sink*s,void*u)
{
    return s->on_model_begin&&s->on_model_begin(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
static int repair_end(modelrt_handle*h,const modelrt_visit_sink*s,void*u)
{
    return s->on_model_end&&s->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
static int repair_mesh_begin(const modelrt_visit_sink*s,void*u,modelrt_u32 index,modelrt_u32 nt)
{
    modelrt_mesh_info mi;modelrt_zero(&mi,sizeof(mi));mi.index=index;mi.triangle_count=nt;
    modelrt_copy_string(mi.name,sizeof(mi.name),"Native mesh");
    return s->on_mesh_begin&&s->on_mesh_begin(u,&mi)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
static int repair_mesh_end(const modelrt_visit_sink*s,void*u,modelrt_u32 index,modelrt_u32 nt)
{
    modelrt_mesh_info mi;modelrt_zero(&mi,sizeof(mi));mi.index=index;mi.triangle_count=nt;
    modelrt_copy_string(mi.name,sizeof(mi.name),"Native mesh");
    return s->on_mesh_end&&s->on_mesh_end(u,&mi)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}

/* COLLADA .dae: native library creates bounded mesh/triangle arrays in arena. */
typedef struct dae_bundle_s {c3dae_model *model;c3dae_context *context;} dae_bundle;
static int dae_probe(const char*p,const modelrt_u8*h,size_t n)
{(void)h;(void)n;return modelrt_ext_eq(p,".dae")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int dae_open(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    repair_state*s;dae_bundle*b;c3dae_model*m;c3dae_context*c;
    c3dae_vertex*verts;c3dae_triangle*tris;c3dae_mesh*meshes;
    int rc;(void)rt;
    rc=repair_read(h,p,&s);if(rc)return rc;
    /* Conservative capacities. Exceeding them is an explicit parse failure. */
    b=(dae_bundle*)modelrt_arena_alloc(h,sizeof(*b),sizeof(void*));
    m=(c3dae_model*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));
    c=(c3dae_context*)modelrt_arena_alloc(h,sizeof(*c),sizeof(void*));
    verts=(c3dae_vertex*)modelrt_arena_alloc(h,16384u*sizeof(*verts),sizeof(void*));
    tris=(c3dae_triangle*)modelrt_arena_alloc(h,16384u*sizeof(*tris),sizeof(void*));
    meshes=(c3dae_mesh*)modelrt_arena_alloc(h,128u*sizeof(*meshes),sizeof(void*));
    if(!b||!m||!c||!verts||!tris||!meshes)return MODELRT_ERR_NO_SPACE;
    c3dae_model_init(m,verts,16384u,tris,16384u,meshes,128u);
    c3dae_context_init(c);
    rc=c3dae_load_memory(c,(const char*)s->bytes,(c3dae_u32)s->size,m);
    if(rc!=C3DAE_OK||!m->triangle_count||!m->vertex_count)return MODELRT_ERR_FORMAT;
    b->model=m;b->context=c;s->decoded=b;
    h->info.mesh_count=m->mesh_count?m->mesh_count:1u;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NORMALS|MODELRT_CAP_NATIVE_SCENE;
    return MODELRT_OK;
}
static int dae_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
    dae_bundle*b=(dae_bundle*)((repair_state*)h->native_state)->decoded;
    c3dae_model*m=b->model; modelrt_triangle_view tr; c3dae_u32 i,j,k,vid[3];int rc;
    (void)t;
    if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;
    rc=repair_begin(h,s,u);if(rc)return rc;
    /* One callback mesh preserves native mesh IDs on emitted triangles. */
    rc=repair_mesh_begin(s,u,0u,m->triangle_count);if(rc)return rc;
    for(i=0;i<m->triangle_count;++i){
        const c3dae_triangle*ct=&m->triangles[i];
        vid[0]=ct->a;vid[1]=ct->b;vid[2]=ct->c;
        modelrt_zero(&tr,sizeof(tr));tr.material_index=ct->material_index;
        tr.mesh_index=ct->mesh_index;
        for(j=0;j<3u;++j){
            const c3dae_vertex*v;
            k=vid[j];if(k>=m->vertex_count)return MODELRT_ERR_FORMAT;
            v=&m->vertices[k];
            tr.v[j].position.x=(modelrt_q16)v->x;
            tr.v[j].position.y=(modelrt_q16)v->y;
            tr.v[j].position.z=(modelrt_q16)v->z;
            tr.v[j].normal.x=(modelrt_q16)v->nx;
            tr.v[j].normal.y=(modelrt_q16)v->ny;
            tr.v[j].normal.z=(modelrt_q16)v->nz;
            tr.v[j].uv0.x=(modelrt_q16)v->u;
            tr.v[j].uv0.y=(modelrt_q16)v->v;
            tr.v[j].flags=MODELRT_VTX_UV0|MODELRT_VTX_NORMAL;
        }
        if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
    }
    rc=repair_mesh_end(s,u,0u,m->triangle_count);if(rc)return rc;
    return repair_end(h,s,u);
}

/* PSX TMD: preserve raw primitive/vertex decoding, reject unsupported packets. */
static int tmd_probe(const char*p,const modelrt_u8*b,size_t n)
{
    if(!modelrt_ext_eq(p,".tmd"))return MODELRT_PROBE_NO;
    return n>=4u&&b[0]==0x41u&&b[1]==0u&&b[2]==0u&&b[3]==0u?MODELRT_PROBE_YES:MODELRT_PROBE_NO;
}
static int tmd_open_native(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    repair_state*s;TMD_Context*ctx;TMD_Object*objs;int rc;(void)rt;
    rc=repair_read(h,p,&s);if(rc)return rc;
    ctx=(TMD_Context*)modelrt_arena_alloc(h,sizeof(*ctx),sizeof(void*));
    objs=(TMD_Object*)modelrt_arena_alloc(h,256u*sizeof(*objs),sizeof(void*));
    if(!ctx||!objs)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(ctx,sizeof(*ctx));
    rc=tmd_parse(ctx,s->bytes,(tmd_u32)s->size,objs,256u);
    if(rc!=TMD_ERR_OK||ctx->parsed_object_count==0u)return MODELRT_ERR_FORMAT;
    {
        tmd_u32 i,total=0u;
        for(i=0u;i<ctx->parsed_object_count;i++)total+=ctx->objects[i].n_primitive;
        if(total==0u)return MODELRT_ERR_UNSUPPORTED;
    }
    s->decoded=ctx;
    h->info.mesh_count=(modelrt_u32)ctx->parsed_object_count;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE;
    return MODELRT_OK;
}
static int tmd_visit_native(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
    TMD_Context*c=(TMD_Context*)((repair_state*)h->native_state)->decoded;
    TMD_Object obj;TMD_Primitive prim;TMD_Face face;TMD_Vertex pos;
    modelrt_triangle_view tr; tmd_u32 oi,pi;int rc,j,k,ix,steps,ind[3];
    (void)t;if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;
    rc=repair_begin(h,s,u);if(rc)return rc;
    for(oi=0u;oi<c->parsed_object_count;oi++){
        if(tmd_get_object(c,oi,&obj)!=TMD_ERR_OK)return MODELRT_ERR_FORMAT;
        rc=repair_mesh_begin(s,u,(modelrt_u32)oi,obj.n_primitive);if(rc)return rc;
        for(pi=0u;pi<obj.n_primitive;pi++){
            if(tmd_get_primitive(c,oi,pi,&prim)!=TMD_ERR_OK)return MODELRT_ERR_FORMAT;
            if(!tmd_primitive_is_polygon(&prim))continue;
            if(tmd_decode_primitive_face(c,oi,&prim,&face)!=TMD_ERR_OK)continue;
            if(face.vertex_count!=3u&&face.vertex_count!=4u)continue;
            steps=face.vertex_count==4u?2:1;
            for(j=0;j<steps;j++){
                ind[0]=0;ind[1]=j==0?1:2;ind[2]=j==0?2:3;
                modelrt_zero(&tr,sizeof(tr));tr.mesh_index=(modelrt_u32)oi;
                if(face.attrs&TMD_FACEF_DOUBLE_SIDED)tr.flags|=MODELRT_TRI_DOUBLE_SIDED;
                for(k=0;k<3;k++){
                    ix=ind[k];
                    if(face.v[ix]>=obj.n_vert)return MODELRT_ERR_FORMAT;
                    if(tmd_get_vertex(c,oi,face.v[ix],&pos)!=TMD_ERR_OK)return MODELRT_ERR_FORMAT;
                    tr.v[k].position.x=(modelrt_q16)((modelrt_i32)pos.x*65536L);
                    tr.v[k].position.y=(modelrt_q16)((modelrt_i32)pos.y*65536L);
                    tr.v[k].position.z=(modelrt_q16)((modelrt_i32)pos.z*65536L);
                    if(face.attrs&TMD_FACEF_TEXTURED){
                        tr.v[k].uv0.x=(modelrt_q16)((modelrt_u32)face.uv[ix].u*256u);
                        tr.v[k].uv0.y=(modelrt_q16)((modelrt_u32)face.uv[ix].v*256u);
                        tr.v[k].flags|=MODELRT_VTX_UV0;
                    }
                }
                if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
            }
        }
        rc=repair_mesh_end(s,u,(modelrt_u32)oi,obj.n_primitive);if(rc)return rc;
    }
    return repair_end(h,s,u);
}

/* Source VTA contains animation keys, not triangle topology. */
static int vta_probe(const char*p,const modelrt_u8*b,size_t n)
{(void)b;(void)n;return modelrt_ext_eq(p,".vta")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int vta_open_native(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    repair_state*s;VtaModel*m;int rc;(void)rt;
    rc=repair_read(h,p,&s);if(rc)return rc;
    m=(VtaModel*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));
    if(!m)return MODELRT_ERR_NO_SPACE;
    vta_model_init(m);
    rc=vta_decode(m,(const char*)s->bytes,s->size);
    if(rc!=VTA_OK||m->frame_count<=0)return MODELRT_ERR_FORMAT;
    s->decoded=m;
    h->info.animation_count=(modelrt_u32)m->frame_count;
    h->info.bone_count=(modelrt_u32)m->bone_count;
    h->info.capabilities=MODELRT_CAP_ANIMATION|MODELRT_CAP_MORPHS|MODELRT_CAP_NATIVE_SCENE|MODELRT_CAP_ANIM_ATTACH_SOURCE;
    return MODELRT_OK;
}
static int vta_visit_native(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
    VtaModel*m=(VtaModel*)((repair_state*)h->native_state)->decoded;
    modelrt_bone_view bv;int rc,i;(void)t;
    if(ai>=m->frame_count||ai<MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_BAD_ARG;
    rc=repair_begin(h,s,u);if(rc)return rc;
    for(i=0;i<m->bone_count;i++){
        modelrt_zero(&bv,sizeof(bv));bv.index=(modelrt_u32)i;
        bv.parent=(modelrt_i32)m->bones[i].parent;
        modelrt_copy_string(bv.name,sizeof(bv.name),m->bones[i].name);
        if(s->on_bone&&s->on_bone(u,&bv))return MODELRT_ERR_CALLBACK;
    }
    /* VTA morph vertex keys require a target mesh; native state retains them. */
    return repair_end(h,s,u);
}
static int vta_count_native(const modelrt_handle*h)
{VtaModel*m=(VtaModel*)((repair_state*)h->native_state)->decoded;return m->frame_count;}
static int vta_info_native(modelrt_handle*h,int index,modelrt_animation_info*out)
{
    VtaModel*m=(VtaModel*)((repair_state*)h->native_state)->decoded;
    if(index<0||index>=m->frame_count||!out)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)index;
    out->frame_count=1u;out->first_tick=m->frames[index].time;
    out->last_tick=m->frames[index].time;
    out->source_key_count=(modelrt_u32)m->frames[index].key_count;
    modelrt_copy_string(out->name,sizeof(out->name),m->frames[index].name);
    return MODELRT_OK;
}
/* MD4 and MD7 both expose caller-owned decoded structures. */
static int md4_probe_native(const char*p,const modelrt_u8*b,size_t n)
{(void)b;(void)n;return modelrt_ext_eq(p,".md4")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int md4_open_native(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    repair_state*s;md4_model_t*m;md4_rc_t rc;int rr;(void)rt;
    rr=repair_read(h,p,&s);if(rr)return rr;
    m=(md4_model_t*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));
    if(!m)return MODELRT_ERR_NO_SPACE;
    md4_model_init(m);
    rc=md4_decode(m,s->bytes,(md4_u32)s->size,0);
    if(rc!=MD4_RC_OK||m->num_lods<=0||m->num_triangles<=0)return MODELRT_ERR_FORMAT;
    s->decoded=m;
    /* Reused MD4 skinning workspace: frame visits must not exhaust the arena. */
    s->extra=modelrt_arena_alloc(h,(size_t)MD4_CFG_MAX_VERTICES*6u*sizeof(md4_fx_t),sizeof(void*));
    if(!s->extra)return MODELRT_ERR_NO_SPACE;
    h->info.mesh_count=(modelrt_u32)m->num_surfaces;
    h->info.bone_count=(modelrt_u32)m->num_bones;
    h->info.animation_count=(modelrt_u32)m->num_frames;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_SKELETON|MODELRT_CAP_ANIMATION|MODELRT_CAP_NATIVE_SCENE;
    return MODELRT_OK;
}
static int md4_visit_native(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
    repair_state*state=(repair_state*)h->native_state;
    md4_model_t*m=(md4_model_t*)state->decoded;
    md4_fx_t*xyz,*norm;modelrt_triangle_view tr;int rc,i,j,k,ni,index,vi;
    const md4_lod_t *lod;const md4_surface_t *surface;
    (void)t;if(ai<MODELRT_ANIM_BIND_POSE||ai>=m->num_frames)return MODELRT_ERR_BAD_ARG;
    rc=repair_begin(h,s,u);if(rc)return rc;
    lod=&m->lods[0];
    for(i=0;i<lod->num_surfaces;i++){
        int si=lod->first_surface+i;
        if(si<0||si>=m->num_surfaces)return MODELRT_ERR_FORMAT;
        surface=&m->surfaces[si];
        if(surface->num_vertices<=0||surface->num_triangles<=0)continue;
        if(surface->num_vertices>MD4_CFG_MAX_VERTICES)return MODELRT_ERR_LIMIT;
        xyz=(md4_fx_t*)state->extra;
        norm=xyz+MD4_CFG_MAX_VERTICES*3u;
        if(md4_skin_surface(m,0,i,ai<0?0:ai,xyz,norm,(md4_u32)surface->num_vertices)!=MD4_RC_OK)return MODELRT_ERR_CODEC;
        rc=repair_mesh_begin(s,u,(modelrt_u32)i,(modelrt_u32)surface->num_triangles);if(rc)return rc;
        for(j=0;j<surface->num_triangles;j++){
            int it=surface->first_triangle+j;
            if(it<0||it>=m->num_triangles)return MODELRT_ERR_FORMAT;
            modelrt_zero(&tr,sizeof(tr));tr.mesh_index=(modelrt_u32)i;
            for(k=0;k<3;k++){
                ni=m->triangles[it].indexes[k];
                if(ni<0||ni>=surface->num_vertices)return MODELRT_ERR_FORMAT;
                vi=surface->first_vertex+ni;
                if(vi<0||vi>=m->num_vertices)return MODELRT_ERR_FORMAT;
                index=ni*3;
                tr.v[k].position.x=xyz[index];tr.v[k].position.y=xyz[index+1];tr.v[k].position.z=xyz[index+2];
                tr.v[k].normal.x=norm[index];tr.v[k].normal.y=norm[index+1];tr.v[k].normal.z=norm[index+2];
                tr.v[k].uv0.x=m->vertices[vi].uv[0];tr.v[k].uv0.y=m->vertices[vi].uv[1];
                tr.v[k].flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0;
            }
            if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
        }
        rc=repair_mesh_end(s,u,(modelrt_u32)i,(modelrt_u32)surface->num_triangles);if(rc)return rc;
    }
    return repair_end(h,s,u);
}
static int md4_count_native(const modelrt_handle*h)
{return ((md4_model_t*)((repair_state*)h->native_state)->decoded)->num_frames;}
static int md4_info_native(modelrt_handle*h,int index,modelrt_animation_info*out)
{
    md4_model_t*m=(md4_model_t*)((repair_state*)h->native_state)->decoded;
    if(!out||index<0||index>=m->num_frames)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)index;out->frame_count=(modelrt_u32)m->num_frames;
    modelrt_copy_string(out->name,sizeof(out->name),m->frames[index].name);
    return MODELRT_OK;
}
/* MD7 decomposes model into groups with independent vertex/UV tables. */
static int md7_probe_native(const char*p,const modelrt_u8*b,size_t n)
{
    (void)p;return n>=4u&&memcmp(b,"MDL7",4u)==0?MODELRT_PROBE_YES:MODELRT_PROBE_NO;
}
#define MD7_ARENA_FIELD(M,F,CAP) do { \
 (M)->F=(struct md7_##F *)modelrt_arena_alloc(h,(CAP)*sizeof(*(M)->F),sizeof(void*)); \
 (M)->F##_cap=(CAP);if(!(M)->F)return MODELRT_ERR_NO_SPACE; \
}while(0)
static int md7_open_native(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    repair_state*s;struct md7_model*m;int rc;(void)rt;
    rc=repair_read(h,p,&s);if(rc)return rc;
    m=(struct md7_model*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));
    if(!m)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(m,sizeof(*m));
    m->bones=(struct md7_bone*)modelrt_arena_alloc(h,128u*sizeof(*m->bones),sizeof(void*));m->bone_cap=128u;
    m->groups=(struct md7_group*)modelrt_arena_alloc(h,128u*sizeof(*m->groups),sizeof(void*));m->group_cap=128u;
    m->skins=(struct md7_skin*)modelrt_arena_alloc(h,256u*sizeof(*m->skins),sizeof(void*));m->skin_cap=256u;
    m->materials=(struct md7_material*)modelrt_arena_alloc(h,256u*sizeof(*m->materials),sizeof(void*));m->material_cap=256u;
    m->uvs=(struct md7_uv*)modelrt_arena_alloc(h,32768u*sizeof(*m->uvs),sizeof(void*));m->uv_cap=32768u;
    m->triangles=(struct md7_triangle*)modelrt_arena_alloc(h,32768u*sizeof(*m->triangles),sizeof(void*));m->triangle_cap=32768u;
    m->vertices=(struct md7_vertex*)modelrt_arena_alloc(h,32768u*sizeof(*m->vertices),sizeof(void*));m->vertex_cap=32768u;
    m->frames=(struct md7_frame*)modelrt_arena_alloc(h,256u*sizeof(*m->frames),sizeof(void*));m->frame_cap=256u;
    m->framevertices=(struct md7_vertex*)modelrt_arena_alloc(h,65536u*sizeof(*m->framevertices),sizeof(void*));m->framevertex_cap=65536u;
    m->transforms=(struct md7_bonetransform*)modelrt_arena_alloc(h,8192u*sizeof(*m->transforms),sizeof(void*));m->transform_cap=8192u;
    m->deformers=(struct md7_deformer*)modelrt_arena_alloc(h,128u*sizeof(*m->deformers),sizeof(void*));m->deformer_cap=128u;
    m->deformer_elements=(struct md7_deformer_element*)modelrt_arena_alloc(h,1024u*sizeof(*m->deformer_elements),sizeof(void*));m->deformer_element_cap=1024u;
    m->deformer_weights=(struct md7_deformer_weight*)modelrt_arena_alloc(h,32768u*sizeof(*m->deformer_weights),sizeof(void*));m->deformer_weight_cap=32768u;
    if(!m->bones||!m->groups||!m->skins||!m->materials||!m->uvs||!m->triangles||!m->vertices||!m->frames||!m->framevertices||!m->transforms||!m->deformers||!m->deformer_elements||!m->deformer_weights)return MODELRT_ERR_NO_SPACE;
    if(md7_load_memory(s->bytes,(md7_u32)s->size,m)!=MD7_OK||!m->triangle_count)return MODELRT_ERR_FORMAT;
    s->decoded=m;h->info.mesh_count=(modelrt_u32)m->group_count;
    /* Only bind pose is emitted here. Frames remain in the borrowed native state. */
    h->info.animation_count=0u;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NATIVE_SCENE;
    return MODELRT_OK;
}
static int md7_visit_native(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
    struct md7_model*m=(struct md7_model*)((repair_state*)h->native_state)->decoded;
    const struct md7_group*g;const struct md7_triangle*tri;const struct md7_vertex*v;
    modelrt_triangle_view out;md7_u32 i,j,k,tid,vid,uid,vi;int rc;
    (void)t;if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;
    rc=repair_begin(h,s,u);if(rc)return rc;
    for(i=0u;i<m->group_count;i++){
        g=&m->groups[i];rc=repair_mesh_begin(s,u,(modelrt_u32)i,(modelrt_u32)g->numtris);if(rc)return rc;
        for(j=0u;j<g->numtris;j++){
            tid=g->first_triangle+j;if(tid>=m->triangle_count)return MODELRT_ERR_FORMAT;
            tri=&m->triangles[tid];modelrt_zero(&out,sizeof(out));out.mesh_index=(modelrt_u32)i;
            for(k=0u;k<3u;k++){
                vid=(md7_u32)tri->v_index[k];if(vid>=g->numverts)return MODELRT_ERR_FORMAT;
                vi=g->first_vertex+vid;if(vi>=m->vertex_count)return MODELRT_ERR_FORMAT;
                v=&m->vertices[vi];out.v[k].position.x=(modelrt_q16)v->x;out.v[k].position.y=(modelrt_q16)v->y;out.v[k].position.z=(modelrt_q16)v->z;
                if(v->normal_mode==2u){out.v[k].normal.x=(modelrt_q16)v->nx;out.v[k].normal.y=(modelrt_q16)v->ny;out.v[k].normal.z=(modelrt_q16)v->nz;out.v[k].flags|=MODELRT_VTX_NORMAL;}
                if(tri->has_uv1){uid=g->first_uv+tri->uv1_index[k];if(uid>=m->uv_count)return MODELRT_ERR_FORMAT;out.v[k].uv0.x=(modelrt_q16)m->uvs[uid].u;out.v[k].uv0.y=(modelrt_q16)m->uvs[uid].v;out.v[k].flags|=MODELRT_VTX_UV0;}
            }
            if(s->on_triangle&&s->on_triangle(u,&out))return MODELRT_ERR_CALLBACK;
        }
        rc=repair_mesh_end(s,u,(modelrt_u32)i,(modelrt_u32)g->numtris);if(rc)return rc;
    }
    return repair_end(h,s,u);
}
/* UC script: scene references and animation lists, not a triangle source. */
static int uc_probe_native(const char*p,const modelrt_u8*b,size_t n)
{(void)b;(void)n;return modelrt_ext_eq(p,".uc")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int uc_open_native(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    repair_state*s;uc3d_scene*scene;int rc;(void)rt;
    rc=repair_read(h,p,&s);if(rc)return rc;
    scene=(uc3d_scene*)modelrt_arena_alloc(h,sizeof(*scene),sizeof(void*));
    if(!scene)return MODELRT_ERR_NO_SPACE;
    uc3d_scene_init(scene);
    rc=uc3d_parse_uc_buffer((const char*)s->bytes,(unsigned long)s->size,scene);
    if(rc!=UC3D_OK||(!scene->has_class&&!scene->mesh_count&&!scene->sequence_count))return MODELRT_ERR_FORMAT;
    s->decoded=scene;h->info.animation_count=(modelrt_u32)scene->sequence_count;
    h->info.capabilities=MODELRT_CAP_NATIVE_SCENE|MODELRT_CAP_EXTERNAL_TEX;
    return MODELRT_OK;
}
static int uc_visit_native(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{(void)t;if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;return repair_begin(h,s,u)==MODELRT_OK?repair_end(h,s,u):MODELRT_ERR_CALLBACK;}
/* UE1 mesh: opening _d.3d automatically resolves the paired _a.3d frames. */
typedef struct ue_pair_s {
    u3d3d_model model;
    const modelrt_u8*anim_bytes;
    size_t anim_size;
    u3d3d_vertex*frame_vertices;
} ue_pair;
static int ue_probe_native(const char*p,const modelrt_u8*b,size_t n)
{(void)b;(void)n;return modelrt_path_has_suffix(p,"_d.3d")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int ue_open_native(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    repair_state*s;ue_pair*pair;u3d3d_triangle*tri;modelrt_u8*anim;size_t anim_size;
    char anim_path[MODELRT_PATH_MAX];size_t path_len;int rc;(void)rt;
    rc=repair_read(h,p,&s);if(rc)return rc;
    path_len=strlen(p);if(path_len<5u||path_len>=sizeof(anim_path))return MODELRT_ERR_BAD_ARG;
    modelrt_copy_string(anim_path,sizeof(anim_path),p);
    anim_path[path_len-4u]='a';
    rc=modelrt_read_file_arena(h,anim_path,&anim,&anim_size);if(rc)return rc;
    pair=(ue_pair*)modelrt_arena_alloc(h,sizeof(*pair),sizeof(void*));
    tri=(u3d3d_triangle*)modelrt_arena_alloc(h,65535u*sizeof(*tri),sizeof(void*));
    if(!pair||!tri)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(pair,sizeof(*pair));u3d3d_model_init(&pair->model,tri,65535u);
    if(u3d3d_decode_d(s->bytes,(u3d3d_u32)s->size,&pair->model)!=U3D3D_OK)return MODELRT_ERR_FORMAT;
    if(u3d3d_decode_a_header(anim,(u3d3d_u32)anim_size,&pair->model)!=U3D3D_OK)return MODELRT_ERR_FORMAT;
    if(pair->model.vertex_count==0u||pair->model.triangle_count==0u)return MODELRT_ERR_FORMAT;
    pair->frame_vertices=(u3d3d_vertex*)modelrt_arena_alloc(h,(size_t)pair->model.vertex_count*sizeof(u3d3d_vertex),sizeof(void*));
    if(!pair->frame_vertices)return MODELRT_ERR_NO_SPACE;
    pair->anim_bytes=anim;pair->anim_size=anim_size;s->decoded=pair;
    h->info.mesh_count=1u;h->info.animation_count=(modelrt_u32)pair->model.frame_count;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_ANIMATION|MODELRT_CAP_NATIVE_SCENE;
    return MODELRT_OK;
}
static int ue_visit_native(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
    ue_pair*p=(ue_pair*)((repair_state*)h->native_state)->decoded;
    u3d3d_model*m=&p->model;modelrt_triangle_view tr;unsigned int i,j,idx;int rc;(void)t;
    if(ai<MODELRT_ANIM_BIND_POSE||ai>=m->frame_count)return MODELRT_ERR_BAD_ARG;
    if(u3d3d_decode_a_frame(p->anim_bytes,(u3d3d_u32)p->anim_size,m,ai<0?0u:(u3d3d_u16)ai,p->frame_vertices,m->vertex_count)!=U3D3D_OK)return MODELRT_ERR_CODEC;
    rc=repair_begin(h,s,u);if(rc)return rc;
    rc=repair_mesh_begin(s,u,0u,m->triangle_count);if(rc)return rc;
    for(i=0u;i<m->triangle_count;i++){
        const u3d3d_triangle*ft=&m->triangles[i];modelrt_zero(&tr,sizeof(tr));
        for(j=0u;j<3u;j++){
            idx=(unsigned int)ft->v[j];if(idx>=m->vertex_count)return MODELRT_ERR_FORMAT;
            tr.v[j].position.x=(modelrt_q16)p->frame_vertices[idx].x;
            tr.v[j].position.y=(modelrt_q16)p->frame_vertices[idx].y;
            tr.v[j].position.z=(modelrt_q16)p->frame_vertices[idx].z;
            tr.v[j].uv0.x=(modelrt_q16)((modelrt_u32)ft->uv[j][0]*256u);
            tr.v[j].uv0.y=(modelrt_q16)((modelrt_u32)ft->uv[j][1]*256u);
            tr.v[j].flags=MODELRT_VTX_UV0;
        }
        if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
    }
    rc=repair_mesh_end(s,u,0u,m->triangle_count);if(rc)return rc;
    return repair_end(h,s,u);
}
static int ue_count_native(const modelrt_handle*h)
{return (int)((ue_pair*)((repair_state*)h->native_state)->decoded)->model.frame_count;}
static int ue_info_native(modelrt_handle*h,int index,modelrt_animation_info*out)
{
    ue_pair*p=(ue_pair*)((repair_state*)h->native_state)->decoded;
    if(index<0||!out||index>=p->model.frame_count)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)index;
    out->frame_count=p->model.frame_count;
    modelrt_copy_string(out->name,sizeof(out->name),"Unreal vertex frame");return MODELRT_OK;
}
/* Panda EGG is parsed with the vendor's AST+model, not any interchange file. */
static int egg_probe_native(const char*p,const modelrt_u8*b,size_t n)
{(void)b;(void)n;return modelrt_ext_eq(p,".egg")?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int egg_open_native(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    repair_state*s;p3_egg_ast*ast;p3_egg_model*m;char err[128];int rc;(void)rt;
    rc=repair_read(h,p,&s);if(rc)return rc;
    if(s->size>P3_MAX_FILE_BYTES)return MODELRT_ERR_LIMIT;
    ast=(p3_egg_ast*)modelrt_arena_alloc(h,sizeof(*ast),sizeof(void*));
    m=(p3_egg_model*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));
    if(!ast||!m)return MODELRT_ERR_NO_SPACE;
    p3_egg_ast_init(ast);p3_egg_model_init(m);
    if(p3_egg_parse(ast,(const char*)s->bytes,(p3_u32)s->size,err,sizeof(err))!=0)return MODELRT_ERR_FORMAT;
    if(p3_egg_model_from_ast(m,ast,err,sizeof(err))!=0)return MODELRT_ERR_FORMAT;
    if(m->polygon_count==0u||m->vertex_count==0u)return MODELRT_ERR_UNSUPPORTED;
    s->decoded=m;h->info.mesh_count=1u;
    h->info.bone_count=(modelrt_u32)m->joint_count;
    h->info.material_count=(modelrt_u32)m->material_count;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NORMALS|MODELRT_CAP_NATIVE_SCENE;
    return MODELRT_OK;
}
static int egg_visit_native(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
    p3_egg_model*m=(p3_egg_model*)((repair_state*)h->native_state)->decoded;
    modelrt_triangle_view tr;modelrt_u32 poly,k,j,idx;int rc;
    (void)t;if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;
    rc=repair_begin(h,s,u);if(rc)return rc;
    rc=repair_mesh_begin(s,u,0u,m->polygon_count);if(rc)return rc;
    for(poly=0u;poly<(modelrt_u32)m->polygon_count;poly++){
        const p3_egg_polygon*pg=&m->polygons[poly];
        if(pg->index_count<3u)continue;
        if((modelrt_u32)pg->first_index+pg->index_count>m->polygon_index_count)return MODELRT_ERR_FORMAT;
        for(k=1u;k+1u<(modelrt_u32)pg->index_count;k++){
            modelrt_zero(&tr,sizeof(tr));
            for(j=0u;j<3u;j++){
                const p3_egg_vertex*v;modelrt_u32 at=pg->first_index+(j==0?0u:(j==1?k:k+1u));
                idx=(modelrt_u32)m->polygon_indices[at];if(idx>=m->vertex_count)return MODELRT_ERR_FORMAT;
                v=&m->vertices[idx];
                tr.v[j].position.x=(modelrt_q16)v->pos[0];tr.v[j].position.y=(modelrt_q16)v->pos[1];tr.v[j].position.z=(modelrt_q16)v->pos[2];
                if(v->has_uv){tr.v[j].uv0.x=(modelrt_q16)v->uv[0];tr.v[j].uv0.y=(modelrt_q16)v->uv[1];tr.v[j].flags|=MODELRT_VTX_UV0;}
                if(v->has_normal){tr.v[j].normal.x=(modelrt_q16)v->normal[0];tr.v[j].normal.y=(modelrt_q16)v->normal[1];tr.v[j].normal.z=(modelrt_q16)v->normal[2];tr.v[j].flags|=MODELRT_VTX_NORMAL;}
            }
            if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
        }
    }
    rc=repair_mesh_end(s,u,0u,m->polygon_count);if(rc)return rc;
    return repair_end(h,s,u);
}

/* 3MF package: extract original model part using raw DEFLATE support. */
static int mf_inflate_native(void*u,const mf3d_u8*src,mf3d_u32 src_len,mf3d_u8*dst,mf3d_u32 cap,mf3d_u32*out_len)
{
    size_t got=0u;int rc;(void)u;
    rc=zlib89_inflate_raw_memory(src,(size_t)src_len,dst,(size_t)cap,&got);
    if(rc!=ZLIB89_OK||got>cap)return -1;
    *out_len=(mf3d_u32)got;return 0;
}
static int mf_probe_native(const char*p,const modelrt_u8*b,size_t n)
{return modelrt_ext_eq(p,".3mf")&&n>=4u&&b[0]=='P'&&b[1]=='K'?MODELRT_PROBE_YES:MODELRT_PROBE_NO;}
static int mf_open_native(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    repair_state*s;mf3d_model*m;mf3d_options opts;mf3d_u8*scratch;int rc;(void)rt;
    rc=repair_read(h,p,&s);if(rc)return rc;
    m=(mf3d_model*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));
    scratch=(mf3d_u8*)modelrt_arena_alloc(h,MF3D_XML_BUFFER_MAX,1u);
    if(!m||!scratch)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(&opts,sizeof(opts));opts.scratch=scratch;opts.scratch_size=MF3D_XML_BUFFER_MAX;
    opts.inflate_cb=mf_inflate_native;
    if(mf3d_decode(s->bytes,(mf3d_u32)s->size,m,&opts)!=MF3D_OK||m->triangle_count==0u)return MODELRT_ERR_FORMAT;
    s->decoded=m;h->info.mesh_count=(modelrt_u32)m->object_count;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE;
    return MODELRT_OK;
}
static int mf_visit_native(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
    mf3d_model*m=(mf3d_model*)((repair_state*)h->native_state)->decoded;
    modelrt_triangle_view tr;mf3d_u32 i,j,vid[3];int rc;(void)t;
    if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;
    rc=repair_begin(h,s,u);if(rc)return rc;
    rc=repair_mesh_begin(s,u,0u,(modelrt_u32)m->triangle_count);if(rc)return rc;
    for(i=0u;i<m->triangle_count;i++){
        const mf3d_tri*face=&m->triangles[i];modelrt_zero(&tr,sizeof(tr));
        vid[0]=face->v1;vid[1]=face->v2;vid[2]=face->v3;
        for(j=0u;j<3u;j++){
            if(vid[j]>=m->vertex_count)return MODELRT_ERR_FORMAT;
            tr.v[j].position.x=(modelrt_q16)m->vertices[vid[j]].x;
            tr.v[j].position.y=(modelrt_q16)m->vertices[vid[j]].y;
            tr.v[j].position.z=(modelrt_q16)m->vertices[vid[j]].z;
        }
        if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
    }
    rc=repair_mesh_end(s,u,0u,(modelrt_u32)m->triangle_count);if(rc)return rc;
    return repair_end(h,s,u);
}
/* Max Payne SKD/KF2/KFS decodes directly from its native file reader. */
static int mp_probe_native(const char*p,const modelrt_u8*b,size_t n)
{(void)b;(void)n;return (modelrt_ext_eq(p,".skd")||modelrt_ext_eq(p,".kf2")||modelrt_ext_eq(p,".kfs"))?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int mp_open_native(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    repair_state*s;mpk_model*m;int rc;(void)rt;
    rc=repair_read(h,p,&s);if(rc)return rc;
    m=(mpk_model*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;
    mpk_model_init(m);
    if(!mpk_read_file(p,m)||m->polygon_count==0u||m->vertex_count==0u)return MODELRT_ERR_FORMAT;
    s->decoded=m;h->info.mesh_count=(modelrt_u32)m->mesh_count;
    h->info.bone_count=(modelrt_u32)m->bone_count;
    h->info.animation_count=(modelrt_u32)m->animation_count;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_SKELETON|MODELRT_CAP_NATIVE_SCENE;
    return MODELRT_OK;
}
static int mp_visit_native(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
    mpk_model*m=(mpk_model*)((repair_state*)h->native_state)->decoded;
    modelrt_triangle_view tr;mpk_u32 i,j,k,ind[3],vid,uv;int rc;
    (void)t;if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;
    rc=repair_begin(h,s,u);if(rc)return rc;
    rc=repair_mesh_begin(s,u,0u,(modelrt_u32)m->polygon_count);if(rc)return rc;
    for(i=0u;i<m->polygon_count;i++){
        const mpk_polygon *pg=&m->polygons[i];
        if(pg->count<3||pg->count>MPK_MAX_POLY_VERTS)return MODELRT_ERR_FORMAT;
        for(j=1u;j+1u<(mpk_u32)pg->count;j++){
            ind[0]=0u;ind[1]=j;ind[2]=j+1u;modelrt_zero(&tr,sizeof(tr));
            for(k=0u;k<3u;k++){
                vid=pg->v[ind[k]];if(vid>=m->vertex_count)return MODELRT_ERR_FORMAT;
                tr.v[k].position.x=(modelrt_q16)m->vertices[vid].x;
                tr.v[k].position.y=(modelrt_q16)m->vertices[vid].y;
                tr.v[k].position.z=(modelrt_q16)m->vertices[vid].z;
                uv=pg->uv[ind[k]];
                if(uv<m->uv_count){tr.v[k].uv0.x=(modelrt_q16)m->uvs[uv].u;tr.v[k].uv0.y=(modelrt_q16)m->uvs[uv].v;tr.v[k].flags=MODELRT_VTX_UV0;}
            }
            if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
        }
    }
    rc=repair_mesh_end(s,u,0u,(modelrt_u32)m->polygon_count);if(rc)return rc;
    return repair_end(h,s,u);
}

#define REPAIR_PROVIDER(ID,NAME,PROBE,OPEN,VISIT,CNT,INFO) \
 {ID,NAME,PROBE,OPEN,0,VISIT,CNT,INFO,0,0,0,0,0,0,0}
const modelrt_provider modelrt_provider_native_3mf89=REPAIR_PROVIDER(91u,"3MF native",mf_probe_native,mf_open_native,mf_visit_native,0,0);
const modelrt_provider modelrt_provider_native_mpkf89=REPAIR_PROVIDER(90u,"Max Payne MP/KF native",mp_probe_native,mp_open_native,mp_visit_native,0,0);
const modelrt_provider modelrt_provider_native_uc89=REPAIR_PROVIDER(87u,"Unreal UC scene native",uc_probe_native,uc_open_native,uc_visit_native,0,0);
const modelrt_provider modelrt_provider_native_ue1_89=REPAIR_PROVIDER(88u,"Unreal UE1 mesh native",ue_probe_native,ue_open_native,ue_visit_native,ue_count_native,ue_info_native);
const modelrt_provider modelrt_provider_native_panda89=REPAIR_PROVIDER(85u,"Panda EGG native",egg_probe_native,egg_open_native,egg_visit_native,0,0);
const modelrt_provider modelrt_provider_native_md4_89=REPAIR_PROVIDER(83u,"Quake MD4 native",md4_probe_native,md4_open_native,md4_visit_native,md4_count_native,md4_info_native);
const modelrt_provider modelrt_provider_native_md7_89=REPAIR_PROVIDER(84u,"MDL7 native",md7_probe_native,md7_open_native,md7_visit_native,0,0);
const modelrt_provider modelrt_provider_native_dae89=REPAIR_PROVIDER(81u,"COLLADA native",dae_probe,dae_open,dae_visit,0,0);
const modelrt_provider modelrt_provider_native_tmd89=REPAIR_PROVIDER(86u,"PSX TMD native",tmd_probe,tmd_open_native,tmd_visit_native,0,0);
const modelrt_provider modelrt_provider_native_vta89=REPAIR_PROVIDER(89u,"Source VTA native",vta_probe,vta_open_native,vta_visit_native,vta_count_native,vta_info_native);
const c3dae_model *modelrt_native_dae89(const modelrt_handle*h)
{const repair_state*s;if(!h||!h->is_open||h->info.codec_id!=81u)return 0;s=(const repair_state*)h->native_state;return ((const dae_bundle*)s->decoded)->model;}
const TMD_Context *modelrt_native_tmd89(const modelrt_handle*h)
{const repair_state*s;if(!h||!h->is_open||h->info.codec_id!=86u)return 0;s=(const repair_state*)h->native_state;return (const TMD_Context*)s->decoded;}
const VtaModel *modelrt_native_vta89(const modelrt_handle*h)
{const repair_state*s;if(!h||!h->is_open||h->info.codec_id!=89u)return 0;s=(const repair_state*)h->native_state;return (const VtaModel*)s->decoded;}

/* Borrowed native model state, valid until modelrt_close(). */
const void *modelrt_native_batch7_payload89(const modelrt_handle *h)
{
    const repair_state*s;
    if(!h||!h->is_open)return 0;
    if(h->info.codec_id<81u||h->info.codec_id>91u||h->info.codec_id==82u)return 0;
    s=(const repair_state*)h->native_state;
    return s?s->decoded:0;
}
