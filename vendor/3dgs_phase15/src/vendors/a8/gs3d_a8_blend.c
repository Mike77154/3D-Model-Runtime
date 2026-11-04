#include "../../gs3d_internal.h"
int gs3d_a8_blend_apply(const GS3D_Crossfade *fade,GS3D_BlendSample *sample){
    if(!fade||fade->from.generation!=8U||fade->to.generation!=8U)
        return GS3D_ERR_UNSUPPORTED;
    return gs3d_crossfade_apply(fade,sample);
}
