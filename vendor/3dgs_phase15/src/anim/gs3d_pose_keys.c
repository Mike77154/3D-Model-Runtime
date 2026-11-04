#include "../gs3d_internal.h"
GS3D_Mat gs_pose_local[GS3D_MAX_BONES];
GS3D_Mat gs_pose_world[GS3D_MAX_BONES];
GS3D_Mat gs3d_skin_matrix[GS3D_MAX_BONES];
/* MDL7 stores bone animation in group 0; subsequent groups can share it. */
int gs_pose_load_keys(unsigned int frame){
    unsigned int i,j; GS3D_BoneKey key; GS3D_Group *g;
    static unsigned char seen[GS3D_MAX_BONES];
    int r;
    if(gs3d_model.format!=7||!gs_is_open||!gs3d_model.groups)
        return GS3D_ERR_UNSUPPORTED;
    g=&gs3d_model.group[0];if(frame>=g->frame_count)return GS3D_ERR_RANGE;
    if(g->frames[frame].trans_count>(unsigned long)gs3d_model.bones)return GS3D_ERR_CORRUPT;
    memset(seen,0,sizeof(seen));
    for(i=0U;i<gs3d_model.bones;i++)gs_mat_identity(&gs_pose_local[i]);
    for(j=0U;j<g->frames[frame].trans_count;j++){
        r=gs3d_read_bone_key(0U,frame,(unsigned long)j,&key);if(r)return r;
        if(seen[key.bone])return GS3D_ERR_CORRUPT;
        seen[key.bone]=1U;
        for(i=0U;i<16U;i++)gs_pose_local[key.bone].m[i]=key.matrix[i];
        /* The 4th row is not used for affine joint transforms. */
        gs_pose_local[key.bone].m[3]=0L;
        gs_pose_local[key.bone].m[7]=0L;
        gs_pose_local[key.bone].m[11]=0L;
        gs_pose_local[key.bone].m[15]=65536L;
    }
    return GS3D_OK;
}
