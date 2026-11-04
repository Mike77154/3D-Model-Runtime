#include "../gs3d_internal.h"
int gs3d_vendor_additive_apply(unsigned int generation,
    const GS3D_AdditiveRequest *request,const GS3D_BoneMask *mask){
    if(!request||request->base.generation!=generation)return GS3D_ERR_RANGE;
    switch(generation){
    case 4U:return gs3d_a4_additive(request,mask);
    case 5U:return gs3d_a5_additive(request,mask);
    case 6U:return gs3d_a6_additive(request,mask);
    case 7U:return gs3d_a7_additive(request,mask);
    case 8U:return gs3d_a8_additive(request,mask);
    default:return GS3D_ERR_UNSUPPORTED;
    }
}
