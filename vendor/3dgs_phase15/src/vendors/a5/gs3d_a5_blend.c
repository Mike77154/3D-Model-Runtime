#include "../../gs3d_internal.h"
int gs3d_a5_blend_apply(const GS3D_Crossfade *fade,GS3D_BlendSample *sample){
    if(!fade||fade->from.generation!=5U||fade->to.generation!=5U)
        return GS3D_ERR_UNSUPPORTED;
    return gs3d_crossfade_apply(fade,sample);
}
