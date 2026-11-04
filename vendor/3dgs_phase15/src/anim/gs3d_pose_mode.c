#include "../gs3d_internal.h"
/* No heap, no hidden matrix interpretation changes for existing callers. */
static unsigned int gs_pose_mode=GS3D_POSE_HIERARCHY;
int gs3d_set_pose_mode(unsigned int mode){
    if(mode!=GS3D_POSE_HIERARCHY && mode!=GS3D_POSE_FILE_SPACE)
        return GS3D_ERR_RANGE;
    gs_pose_mode=mode;
    gs3d_pose_ready=0;
    return GS3D_OK;
}
unsigned int gs3d_get_pose_mode(void){return gs_pose_mode;}
