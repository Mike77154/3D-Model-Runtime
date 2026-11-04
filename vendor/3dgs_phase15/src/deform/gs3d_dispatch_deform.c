#include "../gs3d_internal.h"
int gs3d_apply_deformer(unsigned int group,unsigned int frame){
    if(!gs_is_open)return GS3D_ERR_RANGE;
    if(group>=gs3d_model.groups||frame>=gs3d_model.group[group].frame_count)return GS3D_ERR_RANGE;
    return gs3d_format_deform(group,frame);
}
