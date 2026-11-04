#include "../../gs3d_internal.h"
int gs3d_a7_clip_apply(const GS3D_Clip *c,unsigned long ticks,GS3D_ClipSample *sample){
    if(!c || c->generation!=7U)return GS3D_ERR_UNSUPPORTED;
    return gs3d_clip_apply(c,ticks,sample);
}
