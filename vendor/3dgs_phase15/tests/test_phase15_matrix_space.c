#include "../src/gs3d_internal.h"
#include <stdio.h>
int main(void){
    GS3D_Vec in,out;
    unsigned int i;
    gs3d_model.format=7;gs3d_model.bones=2;
    gs_is_open=1;
    gs3d_model.bone[0].parent=65535U;
    gs3d_model.bone[1].parent=0U;
    gs3d_model.bone[0].xyz[0]=0L;
    gs3d_model.bone[0].xyz[1]=0L;
    gs3d_model.bone[0].xyz[2]=0L;
    gs3d_model.bone[1].xyz[0]=655360L;
    gs3d_model.bone[1].xyz[1]=0L;
    gs3d_model.bone[1].xyz[2]=0L;
    for(i=0U;i<2U;i++)gs_mat_identity(&gs_pose_local[i]);
    /* Rotation about the absolute pivot (10,0,0), not about origin. */
    gs_pose_local[1].m[0]=0L;
    gs_pose_local[1].m[1]=65536L;
    gs_pose_local[1].m[4]=-65536L;
    gs_pose_local[1].m[5]=0L;
    gs_pose_local[1].m[12]=655360L;
    gs_pose_local[1].m[13]=-655360L;
    if(gs3d_set_pose_mode(GS3D_POSE_FILE_SPACE)!=0)return 1;
    if(gs_pose_hierarchy()!=0)return 2;
    in.x=786432L;in.y=0L;in.z=0L;in.bone=1U;in.normal=0U;
    gs_mat_point(&gs3d_skin_matrix[1],&in,&out);
    if(out.x!=655360L||out.y!=131072L||out.z!=0L)return 3;
    if(gs3d_set_pose_mode(7U)!=GS3D_ERR_RANGE)return 4;
    if(gs3d_get_pose_mode()!=GS3D_POSE_FILE_SPACE)return 5;
    if(gs3d_set_pose_mode(GS3D_POSE_HIERARCHY)!=0)return 6;
    if(gs_pose_hierarchy()!=0)return 7;
    gs_mat_point(&gs3d_skin_matrix[1],&in,&out);
    if(out.x==655360L && out.y==131072L)return 8;
    puts("phase15 matrix-space PASS");
    return 0;
}
