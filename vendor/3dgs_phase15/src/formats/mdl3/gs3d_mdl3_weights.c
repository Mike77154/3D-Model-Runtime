#include "../../gs3d_internal.h"
/* Explicit MDL3 weight adapter. Legacy returns implicit morph influence. */
int gs3d_mdl3_format_weights(unsigned long vertex,GS3D_WeightSet *out){
#if GS3D_WITH_LEGACY
    if(gs3d_model.format!=3)return GS3D_ERR_UNSUPPORTED;
    return gs_legacy_weight_info(vertex,out);
#else
    (void)vertex;(void)out;return GS3D_ERR_UNSUPPORTED;
#endif
}
