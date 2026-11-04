#include "../gs3d_internal.h"
/* Sample each local skeleton before blend. Skin only once after composition.
   As phase10, vertex-frame corrections are taken from BASE exclusively. */
#if GS3D_WITH_MDL7
static GS3D_Mat add_local[GS3D_MAX_BONES];
static GS3D_Mat ref_local[GS3D_MAX_BONES];
static GS3D_Mat base_local[GS3D_MAX_BONES];
int gs3d_additive_skeletal(unsigned int group,const GS3D_AdditiveRequest *req,
    const GS3D_BoneMask *mask,const GS3D_ClipSample *base,
    const GS3D_ClipSample *add,const GS3D_ClipSample *ref){
    unsigned int i;int status;long weight;GS3D_Mat combined;
    if(!req||!mask||!base||!add||!ref||group>=gs3d_model.groups)
        return GS3D_ERR_RANGE;
    gs3d_pose_ready=0;
    status=gs_pose_sample_keys(base->frame_a,base->frame_b,base->alpha_q16);
    if(status)return status;
    for(i=0U;i<gs3d_model.bones;i++)base_local[i]=gs_pose_local[i];
    status=gs_pose_sample_keys(add->frame_a,add->frame_b,add->alpha_q16);
    if(status)return status;
    for(i=0U;i<gs3d_model.bones;i++)add_local[i]=gs_pose_local[i];
    status=gs_pose_sample_keys(ref->frame_a,ref->frame_b,ref->alpha_q16);
    if(status)return status;
    for(i=0U;i<gs3d_model.bones;i++)ref_local[i]=gs_pose_local[i];
    for(i=0U;i<gs3d_model.bones;i++){
        weight=gs_qmul(req->strength_q16,mask->weight_q16[i]);
        status=gs3d_additive_matrix(&base_local[i],&add_local[i],
                                    &ref_local[i],weight,&combined);
        if(status)return status;
        gs_pose_local[i]=combined;
    }
    status=gs_pose_hierarchy();if(status)return status;
    gs3d_pose_ready=1;
    status=gs3d_skeletal_sample_raw_vertices(group,base->frame_a,
                  base->frame_b,base->alpha_q16);
    if(status){gs3d_pose_ready=0;return status;}
    status=gs_weighted_skin_group(group);
    if(status)gs3d_pose_ready=0;
    return status;
}
#else
int gs3d_additive_skeletal(unsigned int group,const GS3D_AdditiveRequest *req,
    const GS3D_BoneMask *mask,const GS3D_ClipSample *base,
    const GS3D_ClipSample *add,const GS3D_ClipSample *ref){
    (void)group;(void)req;(void)mask;(void)base;(void)add;(void)ref;
    return GS3D_ERR_UNSUPPORTED;
}
#endif
