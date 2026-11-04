#include "../gs3d_internal.h"
#define DECL(G) int gs3d_a##G##_clip_apply(const GS3D_Clip *,unsigned long,GS3D_ClipSample *);
DECL(4) DECL(5) DECL(6) DECL(7) DECL(8)
int gs3d_vendor_clip_apply(unsigned int generation,const GS3D_Clip *clip,
                           unsigned long ticks,GS3D_ClipSample *sample){
    switch(generation){
    case 4U:return gs3d_a4_clip_apply(clip,ticks,sample);
    case 5U:return gs3d_a5_clip_apply(clip,ticks,sample);
    case 6U:return gs3d_a6_clip_apply(clip,ticks,sample);
    case 7U:return gs3d_a7_clip_apply(clip,ticks,sample);
    case 8U:return gs3d_a8_clip_apply(clip,ticks,sample);
    default:return GS3D_ERR_UNSUPPORTED;
    }
}
