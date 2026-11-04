#include "../../gs3d_internal.h"
/* Explicit MDL7 weight adapter. Legacy returns implicit morph influence. */
int gs3d_mdl7_format_weights(unsigned long vertex,GS3D_WeightSet *out){
#if GS3D_WITH_MDL7
    if(gs3d_model.format!=7)return GS3D_ERR_UNSUPPORTED;
    return gs_mdl7_weight_info(vertex,out);
#else
    (void)vertex;(void)out;return GS3D_ERR_UNSUPPORTED;
#endif
}
