#include "../../gs3d_internal.h"
/* Explicit MDL7 deformation adapter, its own vendor extension point. */
int gs3d_mdl7_format_deform(unsigned int group,unsigned int frame){
#if GS3D_WITH_MDL7
    if(gs3d_model.format!=7)return GS3D_ERR_UNSUPPORTED;
    return gs3d_mdl7_deform(group,frame);
#else
    (void)group;(void)frame;return GS3D_ERR_UNSUPPORTED;
#endif
}
