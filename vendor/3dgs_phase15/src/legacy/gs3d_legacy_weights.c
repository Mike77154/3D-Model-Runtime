#include "../gs3d_internal.h"
/* Implicit full-strength frame morph; no skeletal weight records in MDL3-5.
   Packed 16-bit vertex byte 7 is an historical bone tag, not verified LBS. */
int gs_legacy_weight_info(unsigned long v,GS3D_WeightSet *out){
    if(!out||v>=gs3d_model.vertices)return GS3D_ERR_RANGE;
    memset(out,0,sizeof(*out));out->kind=GS3D_DEFORM_VERTEX_FRAME;
    out->count=1U;out->item[0].bone=GS3D_WEIGHT_NO_BONE;
    out->item[0].weight_q16=65536L;
    return GS3D_OK;
}
