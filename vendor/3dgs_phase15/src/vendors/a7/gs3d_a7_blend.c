#include "../../gs3d_internal.h"
int gs3d_a7_blend_apply(const GS3D_Crossfade *fade,GS3D_BlendSample *sample){
    if(!fade||fade->from.generation!=7U||fade->to.generation!=7U)
        return GS3D_ERR_UNSUPPORTED;
    return gs3d_crossfade_apply(fade,sample);
}
