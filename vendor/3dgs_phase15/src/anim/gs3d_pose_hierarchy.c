#include "../gs3d_internal.h"
static long safe_negate(long x){
    return x==(-2147483647L-1L)?2147483647L:-x;
}
/* Compose rest offsets with per-frame local joint matrices.
   SkinMatrix = AnimatedWorld * inverse(RestWorld).
   For MDL7 models with weighted deformers this is INSUFFICIENT: such groups
   are rejected by the reader instead of rendering a misleading deformation. */
int gs_pose_hierarchy(void){
    static unsigned char done[GS3D_MAX_BONES];
    static long bind[GS3D_MAX_BONES][3];
    unsigned int n=gs3d_model.bones,i,p,iteration,progress=0U,remaining=n;
    GS3D_Mat t,local,inv;GS3D_Bone *bone;
    if(gs3d_get_pose_mode()==GS3D_POSE_FILE_SPACE)
        return gs_pose_file_space();
    memset(done,0,sizeof(done));
    for(iteration=0U;remaining && iteration<n;iteration++){
        progress=0U;
        for(i=0U;i<n;i++){
            if(done[i])continue;
            bone=&gs3d_model.bone[i];p=bone->parent;
            if(p!=65535U && (p>=n||!done[p]))continue;
            if(p==65535U){
                bind[i][0]=bone->xyz[0];bind[i][1]=bone->xyz[1];
                bind[i][2]=bone->xyz[2];
            }else{
                bind[i][0]=gs_addsat(bind[p][0],bone->xyz[0]);
                bind[i][1]=gs_addsat(bind[p][1],bone->xyz[1]);
                bind[i][2]=gs_addsat(bind[p][2],bone->xyz[2]);
            }
            gs_mat_translate(&t,bone->xyz[0],bone->xyz[1],bone->xyz[2]);
            gs_mat_mul(&local,&t,&gs_pose_local[i]);
            if(p==65535U)gs_pose_world[i]=local;
            else gs_mat_mul(&gs_pose_world[i],&gs_pose_world[p],&local);
            gs_mat_translate(&inv,
                safe_negate(bind[i][0]),
                safe_negate(bind[i][1]),
                safe_negate(bind[i][2]));
            gs_mat_mul(&gs3d_skin_matrix[i],&gs_pose_world[i],&inv);
            done[i]=1U;remaining--;progress++;
        }
        if(!progress)return GS3D_ERR_CORRUPT;
    }
    return remaining?GS3D_ERR_CORRUPT:GS3D_OK;
}
