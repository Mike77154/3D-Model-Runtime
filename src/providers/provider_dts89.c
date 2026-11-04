/* Torque DTS v41 polygon/LOD adapter: native shape, primitive expansion and UVs.
 * Full vendored dts89 APIs (including DSQ, poses, shading) remain public for engines.
 * Runtime visitor conservatively emits bind/static geometry only. C89/no heap. */
#include "../modelrt_internal.h"
#include "dts89.h"
#include "dts89_modules.h"
#include "dts89_shaderer.h"
#include <string.h>

typedef struct dtsrt_state_s {
    dts89_shape *shape;
    dts89_runtime_shape *runtime;
} dtsrt_state;

static int dtsrt_probe(const char *path,const modelrt_u8 *head,size_t n)
{
    dts89_probe_result r;
    int rc;
    if(!modelrt_ext_eq(path,".dts"))return MODELRT_PROBE_NO;
    /* Probe head may be shorter than the full header; native decoder validates. */
    if(n>=48u){rc=dts89_probe(head,(dts89_u32)n,&r);
        if(rc==DTS89_OK && r.kind==DTS89_PROBE_KIND_DTS)return MODELRT_PROBE_YES;}
    return MODELRT_PROBE_MAYBE;
}
static int dtsrt_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
    dtsrt_state *s;modelrt_u8 *bytes;size_t bytes_n,base,cap;dts89_arena arena;
    dts89_runtime_options opt;int rc;dts89_s32 i;
    s=(dtsrt_state*)modelrt_arena_alloc(h,sizeof(*s),8u);
    if(!s)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));
    rc=modelrt_read_file_arena(h,path,&bytes,&bytes_n);if(rc)return rc;
    if(bytes_n>0xffffffffu)return MODELRT_ERR_LIMIT;
    s->shape=(dts89_shape*)modelrt_arena_alloc(h,sizeof(*s->shape),8u);
    s->runtime=(dts89_runtime_shape*)modelrt_arena_alloc(h,sizeof(*s->runtime),8u);
    if(!s->shape||!s->runtime)return MODELRT_ERR_NO_SPACE;
    base=(h->workspace_used+7u)&~(size_t)7u;
    if(base>=h->workspace_size)return MODELRT_ERR_NO_SPACE;
    cap=h->workspace_size-base;
    if(cap>0xffffffffu)cap=0xffffffffu;
    dts89_arena_init(&arena,h->workspace+base,(dts89_u32)cap);
    rc=dts89_decode_shape(bytes,(dts89_u32)bytes_n,&arena,s->shape);
    if(rc!=DTS89_OK){modelrt_set_error(rt,"Torque DTS",MODELRT_ERR_CODEC,"native DTS parse failed");return MODELRT_ERR_CODEC;}
    dts89_runtime_options_default(&opt);
    rc=dts89_runtime_build(s->shape,&opt,&arena,s->runtime);
    if(rc!=DTS89_OK){modelrt_set_error(rt,"Torque DTS",MODELRT_ERR_CODEC,"native LOD setup failed");return MODELRT_ERR_CODEC;}
    h->workspace_used=base+(size_t)arena.used;
    h->native_state=s;
    h->info.mesh_count=(modelrt_u32)s->shape->num_meshes;
    h->info.material_count=(modelrt_u32)s->shape->num_materials;
    h->info.texture_count=(modelrt_u32)s->shape->num_materials;
    h->info.bone_count=(modelrt_u32)s->shape->num_nodes;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_MATERIALS|MODELRT_CAP_UV0|
      MODELRT_CAP_LOD|MODELRT_CAP_NATIVE_SCENE|MODELRT_CAP_EXTERNAL_TEX|MODELRT_CAP_TEXTURES;
    /* Sequencer and skinning are available through native API only until
     * world-object transforms and DSQ pose sampling are verified in visitor. */
    if(s->shape->num_nodes)h->info.capabilities|=MODELRT_CAP_SKELETON;
    for(i=0;i<s->shape->num_meshes;i++){
        if(s->shape->meshes[i].norms_bits){h->info.capabilities|=MODELRT_CAP_NORMALS;break;}
    }
    modelrt_copy_string(h->info.name,sizeof(h->info.name),path);
    return MODELRT_OK;
}
static int dtsrt_visit(modelrt_handle *h,int anim,modelrt_q16 time,const modelrt_visit_sink *sink,void *user)
{
    dtsrt_state *s=(dtsrt_state*)h->native_state;
    dts89_shape *shape;dts89_render_span spans[8];dts89_detail_choice choice;
    dts89_draw_command *draws;dts89_triangle *triangles;dts89_u32 span_count,draw_count,tri_count;
    dts89_u32 a,b,c;dts89_s32 i;size_t mark;int rc,result;
    modelrt_mesh_info mesh;modelrt_triangle_view tri;modelrt_material_view mat;modelrt_bone_view bone;
    dts89_mod_vertex sample;dts89_s32 idx[3];const dts89_mesh_view *native_mesh;
    (void)time;
    if(!s||!sink)return MODELRT_ERR_BAD_ARG;
    if(anim>=0)return MODELRT_ERR_UNSUPPORTED;
    shape=s->shape;mark=h->workspace_used;result=MODELRT_OK;
    if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
    for(i=0;i<shape->num_materials;i++){
        modelrt_zero(&mat,sizeof(mat));mat.index=(modelrt_u32)i;mat.texture_index=i;
        if(shape->materials[i].name.ptr){
            size_t n=(size_t)shape->materials[i].name.len;
            if(n>=sizeof(mat.name))n=sizeof(mat.name)-1u;
            memcpy(mat.name,shape->materials[i].name.ptr,n);mat.name[n]=0;
        }
        mat.rgba[0]=mat.rgba[1]=mat.rgba[2]=mat.rgba[3]=255u;
        if(shape->materials[i].flags&DTS89_MATF_TRANSLUCENT)mat.flags|=MODELRT_MAT_ALPHA_BLEND;
        if(shape->materials[i].flags&DTS89_MATF_ADDITIVE)mat.flags|=MODELRT_MAT_ADDITIVE;
        if(shape->materials[i].flags&DTS89_MATF_SELF_ILLUMINATING)mat.flags|=MODELRT_MAT_FULLBRIGHT;
        if(shape->materials[i].flags&DTS89_MATF_NO_MIPMAP)mat.flags|=MODELRT_MAT_NOMIPS;
        if(shape->materials[i].bump_map!=0xffffffffu)mat.flags|=MODELRT_MAT_BUMP;
        if(shape->materials[i].detail_map!=0xffffffffu)mat.flags|=MODELRT_MAT_DETAIL;
        if(sink->on_material&&sink->on_material(user,&mat)){result=MODELRT_ERR_CALLBACK;goto finish;}
    }
    if(shape->num_details<=0)goto bones;
    choice.detail_level=0;choice.intra=255u;choice.pixel_size=dts89_fx_from_int(512);
    rc=dts89_runtime_make_render_spans(shape,s->runtime,&choice,0u,spans,8u,&span_count);
    if(rc!=DTS89_OK){result=MODELRT_ERR_CODEC;goto finish;}
    for(a=0u;a<span_count;a++){
        rc=dts89_runtime_emit_draws_for_span(shape,&spans[a],0,0u,&draw_count);
        if(rc){result=MODELRT_ERR_CODEC;goto finish;}
        if(draw_count==0u)continue;
        draws=(dts89_draw_command*)modelrt_arena_alloc(h,(size_t)draw_count*sizeof(*draws),8u);
        if(!draws){result=MODELRT_ERR_NO_SPACE;goto finish;}
        rc=dts89_runtime_emit_draws_for_span(shape,&spans[a],draws,draw_count,&draw_count);
        if(rc){result=MODELRT_ERR_CODEC;goto finish;}
        for(b=0u;b<draw_count;b++){
            if(draws[b].mesh_index<0||draws[b].mesh_index>=shape->num_meshes){result=MODELRT_ERR_FORMAT;goto finish;}
            native_mesh=&shape->meshes[draws[b].mesh_index];
            rc=dts89_runtime_count_triangles_for_draw(shape,&draws[b],&tri_count);
            if(rc){result=MODELRT_ERR_CODEC;goto finish;}
            if(!tri_count)continue;
            /* Temporary triangles are rewound between primitives. */
            {size_t tmark=h->workspace_used;
             triangles=(dts89_triangle*)modelrt_arena_alloc(h,(size_t)tri_count*sizeof(*triangles),8u);
             if(!triangles){result=MODELRT_ERR_NO_SPACE;goto finish;}
             rc=dts89_runtime_emit_triangles_for_draw(shape,&draws[b],triangles,tri_count,&tri_count);
             if(rc){result=MODELRT_ERR_CODEC;goto finish;}
             modelrt_zero(&mesh,sizeof(mesh));mesh.index=(modelrt_u32)draws[b].mesh_index;
             mesh.vertex_count=(modelrt_u32)native_mesh->num_verts;mesh.triangle_count=tri_count;
             mesh.material_count=h->info.material_count;
             modelrt_copy_string(mesh.name,sizeof(mesh.name),"Torque DTS LOD0 primitive");
             if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&mesh)){result=MODELRT_ERR_CALLBACK;goto finish;}
             for(c=0u;c<tri_count;c++){
                modelrt_zero(&tri,sizeof(tri));tri.mesh_index=mesh.index;
                tri.material_index=triangles[c].material_index;
                idx[0]=triangles[c].i0;idx[1]=triangles[c].i1;idx[2]=triangles[c].i2;
                for(i=0;i<3;i++){
                    if(dts89_mesher_sample(native_mesh,idx[i],&sample)!=DTS89_OK){result=MODELRT_ERR_CODEC;goto finish;}
                    tri.v[i].position.x=sample.position.x;tri.v[i].position.y=sample.position.y;tri.v[i].position.z=sample.position.z;
                    tri.v[i].uv0.x=sample.uv.x;tri.v[i].uv0.y=MODELRT_Q16_ONE-sample.uv.y; /* top-left Torque -> GL */
                    tri.v[i].flags=MODELRT_VTX_UV0;
                    if(native_mesh->norms_bits&&idx[i]>=0&&idx[i]<native_mesh->num_verts){
                        dts89_fx_vec3 normal;
                        if(dts89_fx_vec3_from_bits3(native_mesh->norms_bits+(dts89_u32)idx[i]*3u,&normal)==DTS89_OK){
                            tri.v[i].normal.x=normal.x;tri.v[i].normal.y=normal.y;tri.v[i].normal.z=normal.z;
                            tri.v[i].flags|=MODELRT_VTX_NORMAL;
                        }
                    }
                    tri.v[i].rgba[0]=tri.v[i].rgba[1]=tri.v[i].rgba[2]=tri.v[i].rgba[3]=255u;
                }
                if(sink->on_triangle&&sink->on_triangle(user,&tri)){result=MODELRT_ERR_CALLBACK;goto finish;}
             }
             if(sink->on_mesh_end&&sink->on_mesh_end(user,&mesh)){result=MODELRT_ERR_CALLBACK;goto finish;}
             h->workspace_used=tmark;
            }
        }
    }
 bones:
    for(i=0;i<shape->num_nodes;i++){
        modelrt_zero(&bone,sizeof(bone));bone.index=(modelrt_u32)i;
        bone.parent=shape->nodes[i].parent_index;
        if(shape->nodes[i].name_index>=0 && shape->nodes[i].name_index<shape->num_names){
            dts89_string name=shape->names[shape->nodes[i].name_index];
            size_t n=(size_t)name.len;if(n>=sizeof(bone.name))n=sizeof(bone.name)-1u;
            if(name.ptr){memcpy(bone.name,name.ptr,n);bone.name[n]=0;}
        }
        if(sink->on_bone&&sink->on_bone(user,&bone)){result=MODELRT_ERR_CALLBACK;goto finish;}
    }
    if(sink->on_model_end&&sink->on_model_end(user,&h->info))result=MODELRT_ERR_CALLBACK;
 finish:
    h->workspace_used=mark;
    return result;
}
static int dtsrt_tex_count(const modelrt_handle *h){return (int)h->info.texture_count;}
static int dtsrt_tex_info(modelrt_handle *h,int idx,modelrt_texture_info *out)
{
    dtsrt_state *s=(dtsrt_state*)h->native_state;dts89_string name;size_t n;
    if(!s||!out||idx<0||idx>=s->shape->num_materials)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)idx;out->kind=MODELRT_TEXTURE_EXTERNAL;
    name=s->shape->materials[idx].name;n=(size_t)name.len;
    if(n>=sizeof(out->name))n=sizeof(out->name)-1u;
    if(name.ptr){memcpy(out->name,name.ptr,n);out->name[n]=0;}
    modelrt_copy_string(out->path,sizeof(out->path),out->name);
    modelrt_copy_string(out->relative_path,sizeof(out->relative_path),out->name);
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_dts89={
 MODELRT_CODEC_TORQUE_DTS,"Torque DTS89 v41 LOD",dtsrt_probe,dtsrt_open,0,
 dtsrt_visit,0,0,dtsrt_tex_count,dtsrt_tex_info,0,0,0,0,0
};

const dts89_shape *modelrt_native_torque_dts(const modelrt_handle *h)
{
    const dtsrt_state *s;
    if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_TORQUE_DTS)return 0;
    s=(const dtsrt_state*)h->native_state;return s?s->shape:0;
}
