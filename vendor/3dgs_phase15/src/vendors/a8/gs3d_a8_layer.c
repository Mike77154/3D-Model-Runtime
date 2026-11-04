#include "../../gs3d_internal.h"
/* Vendor A8: runtime bone masks exist only if the source is MDL7. */
int gs3d_a8_layer_apply(const GS3D_LayerRequest *req,
    const GS3D_BoneMask *mask,GS3D_RootDelta *root){
    if(!req||req->base.generation!=8U||
       req->overlay.generation!=8U)
        return GS3D_ERR_UNSUPPORTED;
    return gs3d_layer_apply(req,mask,root);
}
