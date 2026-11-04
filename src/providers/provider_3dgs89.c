/* GameStudio A4-A8 Fase15 model bridge. Distinct native MDL3/4/5/7 signatures.
 * Single static gs3d_model: visits and texture reads re-open the selected
 * file; NOT reentrant/thread-safe. Frames are addressed by native indices;
 * animation FPS are unknown, no invented timing. No heap, no float math.
 */
#include "../modelrt_internal.h"
#include "../../vendor/3dgs_phase15/include/gs3d.h"
#include <string.h>
#include <limits.h>
#include <stdio.h>

/* Native gs3d state is process-global; preserve the legacy default for
 * compatibility and offer experimental file-space poses by explicit choice. */
static unsigned int gsrt_pose_mode=GS3D_POSE_HIERARCHY;
int modelrt_3dgs_set_pose_mode(modelrt_u32 mode)
{
    if(mode!=MODELRT_3DGS_POSE_HIERARCHY &&
       mode!=MODELRT_3DGS_POSE_FILE_SPACE)return MODELRT_ERR_BAD_ARG;
    gsrt_pose_mode=(unsigned int)mode;
    return MODELRT_OK;
}
modelrt_u32 modelrt_3dgs_get_pose_mode(void)
{
    return (modelrt_u32)gsrt_pose_mode;
}
static void gsrt_apply_pose_mode(void)
{
    (void)gs3d_set_pose_mode(gsrt_pose_mode);
}

typedef struct gsrt_state_s {
    modelrt_u32 group_count;
    modelrt_u32 texture_count;
    modelrt_u32 bone_count;
    modelrt_u32 format;
} gsrt_state;

static int gsrt_probe(const char *path,const modelrt_u8 *head,size_t n)
{
    if (!modelrt_ext_eq(path,".mdl") || n<4u) return MODELRT_PROBE_NO;
    if (memcmp(head,"MDL3",4u)==0 || memcmp(head,"MDL4",4u)==0 ||
        memcmp(head,"MDL5",4u)==0 || memcmp(head,"MDL7",4u)==0) return 115;
    return MODELRT_PROBE_NO;
}
static int gsrt_reload(modelrt_handle *h)
{
    gs3d_set_external_texture_decoder(0,0); /* Never use stale callbacks. */
    if(gs3d_open(h->path)!=GS3D_OK)return MODELRT_ERR_FORMAT;
    gsrt_apply_pose_mode();
    return MODELRT_OK;
}
static int gsrt_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
    gsrt_state *s;
    modelrt_u32 nskins=0u,i,nfaces=0u;
    (void)rt;
    if(gs3d_open(path)!=GS3D_OK)return MODELRT_ERR_FORMAT;
    gsrt_apply_pose_mode();
    if(gs3d_model.groups==0u){gs3d_close();return MODELRT_ERR_FORMAT;}
    s=(gsrt_state *)modelrt_arena_alloc(h,sizeof(*s),8u);
    if(!s){gs3d_close();return MODELRT_ERR_NO_SPACE;}
    modelrt_zero(s,sizeof(*s));
    s->group_count=(modelrt_u32)gs3d_model.groups;
    s->bone_count=(modelrt_u32)gs3d_model.bones;
    s->format=(modelrt_u32)gs3d_model.format;
    for(i=0u;i<s->group_count;++i){
        nskins+=(modelrt_u32)gs3d_model.group[i].skin_count;
        nfaces+=(modelrt_u32)gs3d_model.group[i].face_count;
    }
    s->texture_count=nskins;
    h->native_state=s;
    h->info.mesh_count=s->group_count;
    h->info.material_count=nskins;
    h->info.texture_count=nskins;
    h->info.bone_count=s->bone_count;
    h->info.animation_count=s->group_count;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_ANIMATION|
                         MODELRT_CAP_MATERIALS|MODELRT_CAP_UV0;
    if(nskins)h->info.capabilities|=MODELRT_CAP_TEXTURES|MODELRT_CAP_EMBEDDED_TEX|MODELRT_CAP_EXTERNAL_TEX;
    if(s->bone_count)h->info.capabilities|=MODELRT_CAP_SKELETON|MODELRT_CAP_SKINNING;
    if(s->format<7u)h->info.capabilities|=MODELRT_CAP_MORPHS;
    modelrt_copy_string(h->info.name,sizeof(h->info.name),path);
    (void)nfaces;
    gs3d_close();
    return MODELRT_OK;
}
static void gsrt_close(modelrt_handle *h){(void)h;gs3d_close();}
static int gsrt_q16(long x,modelrt_q16 *out)
{
    /* On Win32 long is 32-bit, on many Linux builds it is 64-bit. */
    if(x>2147483647L || x<(-2147483647L-1L))return 0;
    *out=(modelrt_q16)x;return 1;
}
static int gsrt_vertex(unsigned long vi,unsigned long uvi,modelrt_vertex_view *out)
{
    GS3D_Vec *v;
    GS3D_UV *uv;
    GS3D_WeightSet w;
    unsigned int j;
    if(vi>=gs3d_model.vertices)return 0;
    v=&gs3d_model.v[vi];uv=(uvi<gs3d_model.uvs)?&gs3d_model.uv[uvi]:0;
    modelrt_zero(out,sizeof(*out));
    if(!gsrt_q16(v->x,&out->position.x)||!gsrt_q16(v->y,&out->position.y)||
       !gsrt_q16(v->z,&out->position.z))return 0;
    if(uv){
        if(!gsrt_q16(uv->u,&out->uv0.x)||!gsrt_q16(uv->v,&out->uv0.y))return 0;
        out->flags=MODELRT_VTX_UV0;
    }
    out->rgba[0]=out->rgba[1]=out->rgba[2]=out->rgba[3]=255u;
    if(gs3d_get_vertex_weights(vi,&w)==GS3D_OK && w.count && w.count<=4u &&
       w.kind!=GS3D_DEFORM_VERTEX_FRAME && w.item[0].bone!=GS3D_WEIGHT_NO_BONE){
        for(j=0u;j<w.count;++j){
            if(w.item[j].bone>=gs3d_model.bones ||
               !gsrt_q16(w.item[j].weight_q16,&out->weight[j]))return 0;
            out->bone[j]=(modelrt_i16)w.item[j].bone;
        }
        out->weight_count=w.count;
        out->flags|=MODELRT_VTX_SKIN;
    }
    return 1;
}
static int gsrt_offset_skin(unsigned int group,unsigned int slot)
{
    unsigned int i;
    int n=0;
    for(i=0u;i<group;++i)n+=(int)gs3d_model.group[i].skin_count;
    return n+(int)slot;
}
static int gsrt_emit(modelrt_handle *h,unsigned int active_group,unsigned int frame,
                     const modelrt_visit_sink *sink,void *user)
{
    gsrt_state *s=(gsrt_state *)h->native_state;
    unsigned int i,k,corner;
    unsigned long j;
    modelrt_model_info info;
    if(!s||!sink)return MODELRT_ERR_BAD_ARG;
    if(gsrt_reload(h)!=MODELRT_OK)return MODELRT_ERR_FORMAT;
    if(gs3d_model.groups!=s->group_count || gs3d_model.format!=(int)s->format)return MODELRT_ERR_FORMAT;
    if(active_group>=s->group_count)return MODELRT_ERR_BAD_ARG;
    /* Each group may have its own independent timeline. Frame changes are
       applied only to the selected group; all other groups use bind frame. */
    for(i=0u;i<s->group_count;++i){
        GS3D_Group *g=&gs3d_model.group[i];
        unsigned int target=(i==active_group)?frame:0u;
        int rc;
        if(!g->frame_count)continue;
        if(target>=g->frame_count)return MODELRT_ERR_BAD_ARG;
        /* Native Fase15 deformer handles legacy morph and weighted MDL7.
           Fallback to authored raw vertex frame where rig is unsupported. */
        rc=gs3d_apply_deformer(i,target);
        if(rc!=GS3D_OK){
            if(gs3d_apply_frame(i,target)!=GS3D_OK)return MODELRT_ERR_CODEC;
        }
    }
    info=h->info;
    if(sink->on_model_begin && sink->on_model_begin(user,&info))return MODELRT_ERR_CALLBACK;
    for(i=0u;i<s->bone_count;++i){
        modelrt_bone_view bone;
        GS3D_Bone *src=&gs3d_model.bone[i];
        modelrt_zero(&bone,sizeof(bone));
        bone.index=(modelrt_u32)i;
        bone.parent=(src->parent==65535u)?-1:(modelrt_i32)src->parent;
        modelrt_copy_string(bone.name,sizeof(bone.name),src->name);
        if(!gsrt_q16(src->xyz[0],&bone.position.x)||
           !gsrt_q16(src->xyz[1],&bone.position.y)||
           !gsrt_q16(src->xyz[2],&bone.position.z))return MODELRT_ERR_FORMAT;
        if(sink->on_bone&&sink->on_bone(user,&bone))return MODELRT_ERR_CALLBACK;
    }
    for(i=0u;i<s->group_count;++i){
        GS3D_Group *g=&gs3d_model.group[i];
        modelrt_mesh_info mesh;
        modelrt_zero(&mesh,sizeof(mesh));
        mesh.index=(modelrt_u32)i;
        mesh.vertex_count=(modelrt_u32)g->vertex_count;
        mesh.triangle_count=(modelrt_u32)g->face_count;
        mesh.material_count=(modelrt_u32)g->skin_count;
        modelrt_copy_string(mesh.name,sizeof(mesh.name),g->name);
        for(k=0u;k<g->skin_count;++k){
            modelrt_material_view mat;
            GS3D_MaterialInfo native;
            modelrt_zero(&mat,sizeof(mat));
            mat.index=(modelrt_u32)gsrt_offset_skin(i,k);
            mat.texture_index=(modelrt_i32)mat.index;
            mat.rgba[0]=mat.rgba[1]=mat.rgba[2]=mat.rgba[3]=255u;
            if(gs3d_get_material(i,k,&native)==GS3D_OK && native.is_explicit){
                int c;
                for(c=0;c<3;++c){
                    long v=native.diffuse[c]>>8;
                    mat.rgba[c]=(modelrt_u8)(v<0?0:v>255?255:v);
                }
            }
            modelrt_copy_string(mat.name,sizeof(mat.name),g->skins[k].external);
            if(sink->on_material && sink->on_material(user,&mat))return MODELRT_ERR_CALLBACK;
        }
        if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&mesh))return MODELRT_ERR_CALLBACK;
        for(j=0UL;j<g->face_count;++j){
            GS3D_Face *f=&gs3d_model.face[g->face_first+j];
            modelrt_triangle_view tri;
            int mat=-1;
            modelrt_zero(&tri,sizeof(tri));
            tri.mesh_index=(modelrt_u32)i;
            if(f->material<g->skin_count)mat=gsrt_offset_skin(i,(unsigned int)f->material);
            tri.material_index=(modelrt_i32)mat;
            for(corner=0u;corner<3u;++corner){
                unsigned long vi=g->vertex_first+(unsigned long)f->vi[corner];
                unsigned long uv=g->uv_count?g->uv_first+(unsigned long)f->ti[corner]:gs3d_model.uvs;
                if(f->vi[corner]>=g->vertex_count ||
                   (g->uv_count && f->ti[corner]>=g->uv_count) ||
                   !gsrt_vertex(vi,uv,&tri.v[corner]))return MODELRT_ERR_FORMAT;
            }
            if(sink->on_triangle&&sink->on_triangle(user,&tri))return MODELRT_ERR_CALLBACK;
        }
        if(sink->on_mesh_end&&sink->on_mesh_end(user,&mesh))return MODELRT_ERR_CALLBACK;
    }
    if(sink->on_model_end&&sink->on_model_end(user,&info))return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
static int gsrt_visit(modelrt_handle *h,int animation_index,modelrt_q16 time,
                      const modelrt_visit_sink *sink,void *user)
{
    (void)time;
    if(animation_index!=MODELRT_ANIM_BIND_POSE && animation_index!=0)return MODELRT_ERR_UNSUPPORTED;
    if(animation_index==0 && time!=0)return MODELRT_ERR_UNSUPPORTED;/* no FPS */
    return gsrt_emit(h,0u,0u,sink,user);
}
static int gsrt_anim_count(const modelrt_handle *h)
{
    gsrt_state *s=(gsrt_state *)h->native_state;
    return s?(int)s->group_count:0;
}
static int gsrt_anim_info(modelrt_handle *h,int index,modelrt_animation_info *out)
{
    gsrt_state *s=(gsrt_state *)h->native_state;
    unsigned long frames;
    if(!s||!out||index<0||(modelrt_u32)index>=s->group_count)return MODELRT_ERR_BAD_ARG;
    if(gsrt_reload(h)!=MODELRT_OK)return MODELRT_ERR_FORMAT;
    frames=gs3d_model.group[index].frame_count;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)index;
    modelrt_copy_string(out->name,sizeof(out->name),gs3d_model.group[index].name);
    out->frame_count=(modelrt_u32)frames;
    out->source_key_count=(modelrt_u32)frames;
    out->last_tick=frames?(modelrt_i32)(frames-1u):0;
    out->flags=MODELRT_ANIM_DENSE_FRAMES|MODELRT_ANIM_POSE_SAMPLER;
    return MODELRT_OK;
}
static int gsrt_frame(modelrt_handle *h,int index,modelrt_u32 frame,
                      const modelrt_visit_sink *sink,void *user)
{
    gsrt_state *s=(gsrt_state *)h->native_state;
    if(!s||index<0||(modelrt_u32)index>=s->group_count)return MODELRT_ERR_BAD_ARG;
    return gsrt_emit(h,(unsigned int)index,(unsigned int)frame,sink,user);
}
static int gsrt_tex_count(const modelrt_handle *h)
{
    gsrt_state *s=(gsrt_state *)h->native_state;
    return s?(int)s->texture_count:0;
}
static int gsrt_tex_map(int index,unsigned int *group,unsigned int *slot)
{
    unsigned int i;
    if(index<0)return 0;
    for(i=0u;i<gs3d_model.groups;++i){
        if((unsigned int)index<gs3d_model.group[i].skin_count){
            *group=i;*slot=(unsigned int)index;return 1;
        }
        index-=(int)gs3d_model.group[i].skin_count;
    }
    return 0;
}
static int gsrt_tex_info(modelrt_handle *h,int index,modelrt_texture_info *out)
{
    gsrt_state *s=(gsrt_state *)h->native_state;
    unsigned int group,slot;
    GS3D_TextureInfo native;
    if(!s||!out||index<0||(modelrt_u32)index>=s->texture_count)return MODELRT_ERR_BAD_ARG;
    if(gsrt_reload(h)!=MODELRT_OK || !gsrt_tex_map(index,&group,&slot))return MODELRT_ERR_FORMAT;
    if(gs3d_texture_info(group,slot,&native)!=GS3D_OK)return MODELRT_ERR_FORMAT;
    modelrt_zero(out,sizeof(*out));
    out->index=(modelrt_u32)index;
    out->kind=native.storage==GS3D_TEXTURE_EXTERNAL?MODELRT_TEXTURE_EXTERNAL:MODELRT_TEXTURE_EMBEDDED;
    out->width=(modelrt_u32)native.width;
    out->height=(modelrt_u32)native.height;
    modelrt_copy_string(out->name,sizeof(out->name),native.external);
    modelrt_copy_string(out->path,sizeof(out->path),native.external);
    return MODELRT_OK;
}
static int gsrt_tex_decode(modelrt_handle *h,int index,modelrt_u8 *dst,
                           size_t cap,modelrt_u32 *w,modelrt_u32 *ht,size_t *bytes)
{
    gsrt_state *s=(gsrt_state *)h->native_state;
    unsigned int group,slot;
    unsigned long ww=0UL,hh=0UL;
    size_t n;
    if(!s||!dst||!w||!ht||!bytes||index<0||(modelrt_u32)index>=s->texture_count)
        return MODELRT_ERR_BAD_ARG;
    if(gsrt_reload(h)!=MODELRT_OK||!gsrt_tex_map(index,&group,&slot))return MODELRT_ERR_FORMAT;
    if(gs3d_decode_skin(group,slot,&ww,&hh)!=GS3D_OK)return MODELRT_ERR_UNSUPPORTED;
    if(!ww||!hh||ww>65535UL||hh>65535UL||ww>GS3D_MAX_TEXELS/hh)return MODELRT_ERR_LIMIT;
    n=(size_t)(ww*hh*4UL);
    if(n>cap)return MODELRT_ERR_NO_SPACE;
    memcpy(dst,gs3d_rgba,n);
    *w=(modelrt_u32)ww;*ht=(modelrt_u32)hh;*bytes=n;
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_3dgs89={
    MODELRT_CODEC_3DGS,"3DGS A4-A8 Fase15 MDL3/4/5/7 C89",
    gsrt_probe,gsrt_open,gsrt_close,gsrt_visit,
    gsrt_anim_count,gsrt_anim_info,
    gsrt_tex_count,gsrt_tex_info,gsrt_tex_decode,0,0,0,gsrt_frame
};
