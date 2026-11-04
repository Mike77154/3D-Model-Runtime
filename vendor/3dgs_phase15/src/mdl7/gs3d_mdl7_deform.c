#include "../gs3d_internal.h"
int gs3d_mdl7_deform(unsigned int group,unsigned int frame){
    int r;
    if(gs3d_model.format!=7||group>=gs3d_model.groups)return GS3D_ERR_UNSUPPORTED;
    if(frame>=gs3d_model.group[group].frame_count)return GS3D_ERR_RANGE;
    r=gs3d_build_pose(group,frame);if(r)return r;
    r=gs_mdl7_frame(group,frame);if(r)return r;
    return gs_weighted_skin_group(group);
}
