#include "../gs3d_internal.h"
#define DECL(N) int gs3d_a##N##_blend_apply(const GS3D_Crossfade *,GS3D_BlendSample *);
DECL(4) DECL(5) DECL(6) DECL(7) DECL(8)
int gs3d_vendor_crossfade_apply(unsigned int generation,
    const GS3D_Crossfade *fade,GS3D_BlendSample *sample){
    if(!fade||generation!=fade->from.generation || generation!=fade->to.generation)
        return GS3D_ERR_UNSUPPORTED;
    switch(generation){
    case 4U:return gs3d_a4_blend_apply(fade,sample);
    case 5U:return gs3d_a5_blend_apply(fade,sample);
    case 6U:return gs3d_a6_blend_apply(fade,sample);
    case 7U:return gs3d_a7_blend_apply(fade,sample);
    case 8U:return gs3d_a8_blend_apply(fade,sample);
    default:return GS3D_ERR_UNSUPPORTED;
    }
}
