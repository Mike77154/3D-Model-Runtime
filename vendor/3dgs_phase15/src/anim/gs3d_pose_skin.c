#include "../gs3d_internal.h"
int gs3d_build_pose(unsigned int group,unsigned int frame){
    int r;
    if(!gs_is_open||gs3d_model.format!=7||group>=gs3d_model.groups)
        return GS3D_ERR_UNSUPPORTED;
    if(frame>=gs3d_model.group[group].frame_count)return GS3D_ERR_RANGE;
    gs3d_pose_ready=0;
    r=gs_pose_load_keys(frame);if(r)return r;
    r=gs_pose_hierarchy();
    if(!r)gs3d_pose_ready=1;
    return r;
}
int gs3d_transform_point(unsigned int bone,const GS3D_Vec *in,GS3D_Vec *out){
    if(!in||!out||!gs_is_open||!gs3d_pose_ready||bone>=gs3d_model.bones)return GS3D_ERR_RANGE;
    gs_mat_point(&gs3d_skin_matrix[bone],in,out);return GS3D_OK;
}
int gs3d_apply_pose(unsigned int group,unsigned int frame){
    return gs3d_mdl7_deform(group,frame);
}
