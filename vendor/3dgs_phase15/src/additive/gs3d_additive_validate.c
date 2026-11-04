#include "../gs3d_internal.h"
/* All source clips share a skeleton and serialized frame indices in group 0.
   Cross-model retargeting is deliberately out of scope. */
int gs3d_additive_validate(const GS3D_AdditiveRequest *req,
                           const GS3D_BoneMask *mask){
    const GS3D_Clip *c[3];unsigned int k;int v;
    if(!req||!mask||!gs_is_open)return GS3D_ERR_RANGE;
    if(req->strength_q16<0L||req->strength_q16>65536L)return GS3D_ERR_RANGE;
    v=gs3d_mask_validate(mask);if(v)return v;
    if(gs3d_model.format!=7||gs3d_model.bones==0U)return GS3D_ERR_UNSUPPORTED;
    c[0]=&req->base;c[1]=&req->additive;c[2]=&req->reference;
    for(k=0U;k<3U;k++){
        if(c[k]->generation<6U||c[k]->generation>8U ||
           c[k]->generation!=c[0]->generation ||
           c[k]->group!=c[0]->group ||
           c[k]->group>=gs3d_model.groups ||
           c[k]->track!=GS3D_CLIP_TRACK_SKELETAL ||
           c[k]->count==0U || c[k]->ticks_per_frame==0UL ||
           c[k]->mode>GS3D_CLIP_PINGPONG ||
           (unsigned long)c[k]->first+c[k]->count>
               gs3d_model.group[0].frame_count ||
           (unsigned long)c[k]->first+c[k]->count>
               gs3d_model.group[c[0]->group].frame_count)
            return GS3D_ERR_UNSUPPORTED;
    }
    return GS3D_OK;
}
