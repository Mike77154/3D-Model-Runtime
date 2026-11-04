#include "../../gs3d_internal.h"
/* Explicit MDL3 deformation adapter, its own vendor extension point. */
int gs3d_mdl3_format_deform(unsigned int group,unsigned int frame){
#if GS3D_WITH_LEGACY
    if(gs3d_model.format!=3)return GS3D_ERR_UNSUPPORTED;
    return gs3d_legacy_deform(group,frame);
#else
    (void)group;(void)frame;return GS3D_ERR_UNSUPPORTED;
#endif
}
