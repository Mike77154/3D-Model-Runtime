#include "../../gs3d_internal.h"
int gs3d_a4_blend_apply(const GS3D_Crossfade *fade,GS3D_BlendSample *sample){
    if(!fade||fade->from.generation!=4U||fade->to.generation!=4U)
        return GS3D_ERR_UNSUPPORTED;
    return gs3d_crossfade_apply(fade,sample);
}
