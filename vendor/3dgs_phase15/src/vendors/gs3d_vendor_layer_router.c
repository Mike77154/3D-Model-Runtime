#include "../gs3d_internal.h"
#define DECL(N) int gs3d_a##N##_layer_apply(const GS3D_LayerRequest *,    const GS3D_BoneMask *,GS3D_RootDelta *);
DECL(4) DECL(5) DECL(6) DECL(7) DECL(8)
int gs3d_vendor_layer_apply(unsigned int generation,
    const GS3D_LayerRequest *req,const GS3D_BoneMask *mask,
    GS3D_RootDelta *out){
    if(!req||req->base.generation!=generation||
       req->overlay.generation!=generation)return GS3D_ERR_UNSUPPORTED;
    switch(generation){
    case 4U:return gs3d_a4_layer_apply(req,mask,out);
    case 5U:return gs3d_a5_layer_apply(req,mask,out);
    case 6U:return gs3d_a6_layer_apply(req,mask,out);
    case 7U:return gs3d_a7_layer_apply(req,mask,out);
    case 8U:return gs3d_a8_layer_apply(req,mask,out);
    default:return GS3D_ERR_UNSUPPORTED;
    }
}
