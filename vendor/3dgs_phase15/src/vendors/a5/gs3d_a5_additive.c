#include "../../gs3d_internal.h"
/* Vendor A5: format-specific parser is chosen by the common dispatcher. */
int gs3d_a5_additive(const GS3D_AdditiveRequest *r,const GS3D_BoneMask *m){
    if(!r||r->base.generation!=5U)return GS3D_ERR_RANGE;
    if(!gs_is_open || !gs3d_family_supports(5U,gs3d_model.format))
        return GS3D_ERR_UNSUPPORTED;
    return gs3d_additive_apply(r,m);
}
