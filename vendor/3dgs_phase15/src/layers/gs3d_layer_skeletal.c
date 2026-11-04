#include "../gs3d_internal.h"
/* Evaluate both sampled skeletons in LOCAL space before one skinning pass.
   Keep the base clip's vertex morph corrections: unlike joint transforms,
   they cannot be sensibly separated by an individual joint mask. */
#if GS3D_WITH_MDL7
static GS3D_Mat layer_base[GS3D_MAX_BONES];
int gs3d_layer_skeletal(unsigned int group,const GS3D_LayerRequest *req,
    const GS3D_BoneMask *mask,const GS3D_ClipSample *a,
    const GS3D_ClipSample *b){
    unsigned int i;int r;long w;
    if(!req||!mask||!a||!b||group>=gs3d_model.groups)
        return GS3D_ERR_RANGE;
    gs3d_pose_ready=0;
    r=gs_pose_sample_keys(a->frame_a,a->frame_b,a->alpha_q16);
    if(r)return r;
    for(i=0U;i<gs3d_model.bones;i++)layer_base[i]=gs_pose_local[i];
    r=gs_pose_sample_keys(b->frame_a,b->frame_b,b->alpha_q16);
    if(r)return r;
    for(i=0U;i<gs3d_model.bones;i++){
        GS3D_Mat temp;
        w=gs_qmul(mask->weight_q16[i],req->overlay_weight_q16);
        if(w<=0L){gs_pose_local[i]=layer_base[i];continue;}
        if(w>=GS3D_Q16_ONE)continue;
        r=gs3d_trs_interpolate(&layer_base[i],&gs_pose_local[i],w,&temp);
        if(r)return r;
        gs_pose_local[i]=temp;
    }
    if(req->consume_root_motion){
        gs_pose_local[req->root_bone].m[12]=0L;
        gs_pose_local[req->root_bone].m[13]=0L;
        gs_pose_local[req->root_bone].m[14]=0L;
    }
    r=gs_pose_hierarchy();if(r)return r;
    gs3d_pose_ready=1;
    r=gs3d_skeletal_sample_raw_vertices(group,a->frame_a,a->frame_b,a->alpha_q16);
    if(r){gs3d_pose_ready=0;return r;}
    r=gs_weighted_skin_group(group);
    if(r)gs3d_pose_ready=0;
    return r;
}
#else
int gs3d_layer_skeletal(unsigned int group,const GS3D_LayerRequest *req,
    const GS3D_BoneMask *mask,const GS3D_ClipSample *a,
    const GS3D_ClipSample *b){
    (void)group;(void)req;(void)mask;(void)a;(void)b;
    return GS3D_ERR_UNSUPPORTED;
}
#endif
