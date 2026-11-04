#include "../gs3d_internal.h"
/* Critical order: blend local joint transforms FIRST, recompute world
   hierarchy, blend raw frame vertex corrections, SKIN ONLY ONCE.
   Never blend finished skinned meshes or accumulate previous output. */
#if GS3D_WITH_MDL7
static GS3D_Mat gs_blend_source_bones[GS3D_MAX_BONES];
static GS3D_Vec gs_blend_source_raw[GS3D_MAX_VERTS];
int gs3d_blend_skeletal(unsigned int group,const GS3D_ClipSample *a,
    const GS3D_ClipSample *b,long alpha){
    unsigned int i;unsigned long k,base,count;long inv;int r;
    GS3D_Group *g;
    if(!a||!b||group>=gs3d_model.groups||
       alpha<0L||alpha>GS3D_Q16_ONE)return GS3D_ERR_RANGE;
    if(alpha==0L)return gs3d_sample_skeletal_q16(group,a->frame_a,a->frame_b,a->alpha_q16);
    if(alpha==GS3D_Q16_ONE)return gs3d_sample_skeletal_q16(group,b->frame_a,b->frame_b,b->alpha_q16);
    if(!gs3d_model.bones)return GS3D_ERR_UNSUPPORTED;
    g=&gs3d_model.group[group];base=g->vertex_first;count=g->vertex_count;
    if(base>GS3D_MAX_VERTS||count>GS3D_MAX_VERTS-base)return GS3D_ERR_CAPACITY;
    gs3d_pose_ready=0;
    r=gs_pose_sample_keys(a->frame_a,a->frame_b,a->alpha_q16);if(r)return r;
    for(i=0U;i<gs3d_model.bones;i++)gs_blend_source_bones[i]=gs_pose_local[i];
    r=gs_pose_sample_keys(b->frame_a,b->frame_b,b->alpha_q16);if(r)return r;
    for(i=0U;i<gs3d_model.bones;i++){
        GS3D_Mat temp;
        r=gs3d_trs_interpolate(&gs_blend_source_bones[i],&gs_pose_local[i],alpha,&temp);
        if(r)return r;
        gs_pose_local[i]=temp;
    }
    r=gs_pose_hierarchy();if(r)return r;
    gs3d_pose_ready=1;
    r=gs3d_skeletal_sample_raw_vertices(group,a->frame_a,a->frame_b,a->alpha_q16);
    if(r){gs3d_pose_ready=0;return r;}
    for(k=0UL;k<count;k++)gs_blend_source_raw[k]=gs3d_model.v[base+k];
    r=gs3d_skeletal_sample_raw_vertices(group,b->frame_a,b->frame_b,b->alpha_q16);
    if(r){gs3d_pose_ready=0;return r;}
    inv=GS3D_Q16_ONE-alpha;
    for(k=0UL;k<count;k++){
        GS3D_Vec *dst=&gs3d_model.v[base+k];
        const GS3D_Vec *old=&gs_blend_source_raw[k];
        dst->x=gs_addsat(gs_qmul(old->x,inv),gs_qmul(dst->x,alpha));
        dst->y=gs_addsat(gs_qmul(old->y,inv),gs_qmul(dst->y,alpha));
        dst->z=gs_addsat(gs_qmul(old->z,inv),gs_qmul(dst->z,alpha));
        if(alpha<32768L)dst->normal=old->normal;
    }
    r=gs_weighted_skin_group(group);
    if(r)gs3d_pose_ready=0;
    return r;
}
#else
int gs3d_blend_skeletal(unsigned int group,const GS3D_ClipSample *a,
    const GS3D_ClipSample *b,long alpha){
    (void)group;(void)a;(void)b;(void)alpha;return GS3D_ERR_UNSUPPORTED;
}
#endif
