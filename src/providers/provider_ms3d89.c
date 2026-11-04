/* Native MilkShape3D v1.7 adapter. C89; no heap or floating point. */
#include "../modelrt_internal.h"
#include "gif_ms3d_uv.h"
#include <string.h>

typedef struct msrt_state_s {
    ms3d_model *model;
    ms3d_uv_bindings *bindings;
    ms3d_skin_pose *pose;
    ms3d_skin_vertices *verts;
    modelrt_u32 orphan_count;
} msrt_state;

static int msrt_probe(const char *path,const modelrt_u8 *head,size_t n)
{
    if (n>=10u && memcmp(head,MS3D_MAGIC_TEXT,10u)==0) return MODELRT_PROBE_YES;
    return modelrt_ext_eq(path,".ms3d") ? MODELRT_PROBE_MAYBE : MODELRT_PROBE_NO;
}
static int msrt_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
    msrt_state *s; modelrt_u8 *bytes; size_t count; int rc; unsigned int i;
    s=(msrt_state*)modelrt_arena_alloc(h,sizeof(*s),8u);
    if(!s)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));
    s->model=(ms3d_model*)modelrt_arena_alloc(h,sizeof(ms3d_model),8u);
    s->bindings=(ms3d_uv_bindings*)modelrt_arena_alloc(h,sizeof(ms3d_uv_bindings),8u);
    if(!s->model||!s->bindings)return MODELRT_ERR_NO_SPACE;
    rc=modelrt_read_file_arena(h,path,&bytes,&count);
    if(rc)return rc;
    rc=ms3d_parse(s->model,bytes,count);
    if(rc!=MS3D_OK){modelrt_set_error(rt,"MilkShape3D",MODELRT_ERR_CODEC,ms3d_error_string(rc));return MODELRT_ERR_CODEC;}
    if(ms3d_uv_build_bindings(s->model,s->bindings)!=MS3D_OK)return MODELRT_ERR_CODEC;
    if(s->model->num_joints){
        s->pose=(ms3d_skin_pose*)modelrt_arena_alloc(h,sizeof(ms3d_skin_pose),8u);
        s->verts=(ms3d_skin_vertices*)modelrt_arena_alloc(h,sizeof(ms3d_skin_vertices),8u);
        if(!s->pose||!s->verts)return MODELRT_ERR_NO_SPACE;
        if(ms3d_skin_prepare(s->model,s->pose)!=MS3D_OK)return MODELRT_ERR_CODEC;
    }
    h->native_state=s;
    for(i=0u;i<s->model->num_triangles;i++)
        if(s->bindings->group[i]<0 && s->model->num_groups)s->orphan_count++;
    h->info.mesh_count=s->model->num_groups
        ?(modelrt_u32)s->model->num_groups+(s->orphan_count?1u:0u):1u;
    h->info.material_count=s->model->num_materials;
    h->info.bone_count=s->model->num_joints;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NORMALS|MODELRT_CAP_UV0|MODELRT_CAP_MATERIALS;
    for(i=0u;i<s->model->num_materials;i++){
        if(s->model->materials[i].texture[0])h->info.texture_count++;
    }
    if(h->info.texture_count)h->info.capabilities|=MODELRT_CAP_TEXTURES|MODELRT_CAP_EXTERNAL_TEX;
    if(s->model->num_joints)h->info.capabilities|=MODELRT_CAP_SKELETON|MODELRT_CAP_SKINNING;
    if(s->model->num_joints && s->model->total_frames>0 && s->model->animation_fps>0)
        h->info.capabilities|=MODELRT_CAP_ANIMATION,h->info.animation_count=1u;
    modelrt_copy_string(h->info.name,sizeof(h->info.name),path);
    return MODELRT_OK;
}
static int msrt_visit(modelrt_handle *h,int anim,modelrt_q16 time,const modelrt_visit_sink *sink,void *user)
{
    msrt_state *s;const ms3d_model *m;modelrt_mesh_info mesh;modelrt_triangle_view tri;
    modelrt_material_view mat;modelrt_bone_view bone;modelrt_u32 g,i,j,k;int mi,group_count;
    if(!h||!sink)return MODELRT_ERR_BAD_ARG;
    s=(msrt_state*)h->native_state;if(!s||!s->model)return MODELRT_ERR_BAD_ARG;
    m=s->model;
    if(anim>=0 && (unsigned)anim>=h->info.animation_count)return MODELRT_ERR_BAD_ARG;
    if(s->pose){
        if(ms3d_skin_evaluate_mode(m,s->pose,anim>=0?(ms3d_fx)time:0L,MS3D_ROTATION_QUAT_SLERP)!=MS3D_OK)return MODELRT_ERR_CODEC;
        if(ms3d_skin_positions(m,s->pose,s->verts)!=MS3D_OK)return MODELRT_ERR_CODEC;
    }
    if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
    for(i=0;i<m->num_materials;i++){
        const ms3d_material *src=&m->materials[i];
        modelrt_zero(&mat,sizeof(mat));mat.index=i;
        modelrt_copy_string(mat.name,sizeof(mat.name),src->name);
        mat.rgba[0]=(modelrt_u8)(src->diffuse[0]<=0?0:src->diffuse[0]>=65536L?255:src->diffuse[0]*255L/65536L);
        mat.rgba[1]=(modelrt_u8)(src->diffuse[1]<=0?0:src->diffuse[1]>=65536L?255:src->diffuse[1]*255L/65536L);
        mat.rgba[2]=(modelrt_u8)(src->diffuse[2]<=0?0:src->diffuse[2]>=65536L?255:src->diffuse[2]*255L/65536L);
        mat.rgba[3]=(modelrt_u8)(src->transparency<=0?0:src->transparency>=65536L?255:src->transparency*255L/65536L);
        mat.texture_index=-1;
        if(src->texture[0]){
            modelrt_u32 ti;
            int prior=0;
            for(ti=0;ti<i;ti++)if(m->materials[ti].texture[0])prior++;
            mat.texture_index=prior;
        }
        if(mat.rgba[3]<255)mat.flags|=MODELRT_MAT_ALPHA_BLEND;
        if(src->alphamap[0])mat.flags|=MODELRT_MAT_ALPHA_MASK;
        if(sink->on_material&&sink->on_material(user,&mat))return MODELRT_ERR_CALLBACK;
    }
    group_count=(int)h->info.mesh_count;
    for(g=0u;g<(modelrt_u32)group_count;g++){
        modelrt_zero(&mesh,sizeof(mesh));mesh.index=g;mesh.vertex_count=m->num_vertices;
        mesh.triangle_count=m->num_groups
            ?(g<m->num_groups?m->groups[g].triangle_count:s->orphan_count):m->num_triangles;
        mesh.material_count=m->num_materials;
        modelrt_copy_string(mesh.name,sizeof(mesh.name),
            m->num_groups?(g<m->num_groups?m->groups[g].name:"Ungrouped triangles"):"MilkShape mesh");
        if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&mesh))return MODELRT_ERR_CALLBACK;
        for(i=0u;i<m->num_triangles;i++){
            const ms3d_triangle *t=&m->triangles[i];
            if(m->num_groups){
                if(g<m->num_groups){if(s->bindings->group[i]!=(int)g)continue;}
                else if(s->bindings->group[i]>=0)continue;
            }
            modelrt_zero(&tri,sizeof(tri));tri.mesh_index=g;mi=s->bindings->material[i];
            tri.material_index=mi;
            for(j=0;j<3u;j++){
                const ms3d_vertex *v;modelrt_vertex_view *out;ms3d_u16 vi=t->vertex_indices[j];
                ms3d_fx norm[3],outnorm[3];ms3d_influence inf[4];ms3d_u8 ct;
                if(vi>=m->num_vertices)return MODELRT_ERR_FORMAT;
                v=&m->vertices[vi];out=&tri.v[j];
                for(k=0;k<3u;k++)norm[k]=t->vertex_normals[j][k];
                out->position.x=(modelrt_q16)(s->verts?s->verts->xyz[vi][0]:v->position[0]);
                out->position.y=(modelrt_q16)(s->verts?s->verts->xyz[vi][1]:v->position[1]);
                out->position.z=(modelrt_q16)(s->verts?s->verts->xyz[vi][2]:v->position[2]);
                if(s->pose && ms3d_skin_normal(m,s->pose,vi,norm,outnorm)==MS3D_OK){
                    out->normal.x=(modelrt_q16)outnorm[0];out->normal.y=(modelrt_q16)outnorm[1];out->normal.z=(modelrt_q16)outnorm[2];
                }else{
                    out->normal.x=(modelrt_q16)norm[0];out->normal.y=(modelrt_q16)norm[1];out->normal.z=(modelrt_q16)norm[2];
                }
                /* Runtime view targets OpenGL bottom-left; native MS3D t stays intact in vendor. */
                out->uv0.x=(modelrt_q16)t->s[j];out->uv0.y=MODELRT_Q16_ONE-(modelrt_q16)t->t[j];
                out->rgba[0]=out->rgba[1]=out->rgba[2]=out->rgba[3]=255u;
                out->flags=MODELRT_VTX_UV0|MODELRT_VTX_NORMAL;
                if(ms3d_vertex_get_influences(m,vi,inf,&ct)==MS3D_OK && ct){
                    out->weight_count=(modelrt_u8)ct;out->flags|=MODELRT_VTX_SKIN;
                    for(k=0;k<ct && k<4u;k++){out->bone[k]=(modelrt_i16)inf[k].bone_id;out->weight[k]=(modelrt_q16)inf[k].weight;}
                }
            }
            if(sink->on_triangle&&sink->on_triangle(user,&tri))return MODELRT_ERR_CALLBACK;
        }
        if(sink->on_mesh_end&&sink->on_mesh_end(user,&mesh))return MODELRT_ERR_CALLBACK;
    }
    for(i=0;i<m->num_joints;i++){
        modelrt_zero(&bone,sizeof(bone));bone.index=i;bone.parent=m->joints[i].parent_index;
        modelrt_copy_string(bone.name,sizeof(bone.name),m->joints[i].name);
        bone.position.x=(modelrt_q16)m->joints[i].position[0];
        bone.position.y=(modelrt_q16)m->joints[i].position[1];
        bone.position.z=(modelrt_q16)m->joints[i].position[2];
        if(s->pose){bone.position.x=(modelrt_q16)s->pose->global[i].m[0][3];bone.position.y=(modelrt_q16)s->pose->global[i].m[1][3];bone.position.z=(modelrt_q16)s->pose->global[i].m[2][3];}
        if(sink->on_bone&&sink->on_bone(user,&bone))return MODELRT_ERR_CALLBACK;
    }
    if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
static int msrt_count(const modelrt_handle *h){return h->info.animation_count?(int)h->info.animation_count:0;}
static int msrt_info(modelrt_handle *h,int idx,modelrt_animation_info *out)
{
    msrt_state *s=(msrt_state*)h->native_state;const ms3d_model *m=s?s->model:0;
    if(!m||!out||idx!=0||!h->info.animation_count)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=0;modelrt_copy_string(out->name,sizeof(out->name),"MS3D animation");
    out->frame_count=(modelrt_u32)m->total_frames;out->source_key_count=m->num_rot_keys+m->num_pos_keys;
    out->first_tick=0;out->last_tick=m->total_frames-1;
    if(m->animation_fps>0){
        /* Clamp before multiplication; do not overflow signed Q16 on 32-bit. */
        ms3d_fx duration_frames=m->total_frames<=32767L?m->total_frames*65536L:2147483647L;
        out->end_time=ms3d_fx_div(duration_frames,m->animation_fps);
        out->ticks_per_second=(modelrt_u32)(m->animation_fps/65536L);out->flags=MODELRT_ANIM_DENSE_FRAMES|MODELRT_ANIM_TICK_RATE_KNOWN|MODELRT_ANIM_POSE_SAMPLER;}
    return MODELRT_OK;
}
static int msrt_tex_count(const modelrt_handle *h){return (int)h->info.texture_count;}
static int msrt_tex_info(modelrt_handle *h,int idx,modelrt_texture_info *out)
{
    msrt_state *s=(msrt_state*)h->native_state;int i,n=0;
    if(!s||!out||idx<0)return MODELRT_ERR_BAD_ARG;
    for(i=0;i<s->model->num_materials;i++)if(s->model->materials[i].texture[0]){
        if(n++==idx){modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)idx;out->kind=MODELRT_TEXTURE_EXTERNAL;
            modelrt_copy_string(out->name,sizeof(out->name),s->model->materials[i].name);
            modelrt_copy_string(out->path,sizeof(out->path),s->model->materials[i].texture);
            modelrt_copy_string(out->relative_path,sizeof(out->relative_path),s->model->materials[i].texture);
            return MODELRT_OK;}
    }
    return MODELRT_ERR_BAD_ARG;
}
const modelrt_provider modelrt_provider_ms3d89={
 MODELRT_CODEC_MILKSHAPE3D,"MilkShape3D v1.7 SLERP",msrt_probe,msrt_open,0,
 msrt_visit,msrt_count,msrt_info,msrt_tex_count,msrt_tex_info,0,0,0,0,0
};

const ms3d_model *modelrt_native_ms3d(const modelrt_handle *h)
{
    const msrt_state *s;
    if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_MILKSHAPE3D)return 0;
    s=(const msrt_state*)h->native_state;return s?s->model:0;
}
