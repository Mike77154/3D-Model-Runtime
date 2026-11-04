#include "../gs3d_internal.h"
int gs3d_layer_validate(const GS3D_LayerRequest *req,const GS3D_BoneMask *mask){
    GS3D_Crossfade test;unsigned int track;int r;
    if(!req||!mask||!gs_is_open)return GS3D_ERR_RANGE;
    if(req->overlay_weight_q16<0L||req->overlay_weight_q16>GS3D_Q16_ONE)
        return GS3D_ERR_RANGE;
    r=gs3d_mask_validate(mask);if(r)return r;
    memset(&test,0,sizeof(test));
    test.from=req->base;test.to=req->overlay;test.fade_ticks=1UL;
    r=gs3d_blend_validate(&test,&track);if(r)return r;
    if(track!=GS3D_CLIP_TRACK_SKELETAL)return GS3D_ERR_UNSUPPORTED;
    if(req->consume_root_motion>1U)return GS3D_ERR_RANGE;
    if(req->consume_root_motion){
        if(req->root_bone>=gs3d_model.bones||
           gs3d_model.bone[req->root_bone].parent!=65535U)
            return GS3D_ERR_RANGE;
        if(mask->weight_q16[req->root_bone]!=0L)
            return GS3D_ERR_UNSUPPORTED;
        if(req->base.mode!=GS3D_CLIP_CLAMP||
           req->previous_base_ticks>req->base_ticks)
            return GS3D_ERR_UNSUPPORTED;
    }
    return GS3D_OK;
}
