#include "../../gs3d_internal.h"
/* Generation 6: delegates to format-aware independent deformer. */
int gs3d_a6_deform(unsigned int group,unsigned int frame){
    if(!gs_is_open||!gs3d_family_supports(6U,gs3d_model.format))return GS3D_ERR_UNSUPPORTED;
    return gs3d_apply_deformer(group,frame);
}
