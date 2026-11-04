#include "../gs3d_internal.h"
/* MDL3/4/5 frame-indexed per-vertex deformation. No invented skeleton. */
int gs3d_legacy_deform(unsigned int group,unsigned int frame){
    if(gs3d_model.format==7)return GS3D_ERR_UNSUPPORTED;
    if(group>=gs3d_model.groups||frame>=gs3d_model.group[group].frame_count)return GS3D_ERR_RANGE;
    return gs_legacy_frame(group,frame);
}
