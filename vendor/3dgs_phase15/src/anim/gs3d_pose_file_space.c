#include "../gs3d_internal.h"
/* Some authentic MDL7 animations contain model-space pivot-compensated
 * matrices: the translation is P - (R*P) for the joint pivot P.
 * Adding T(rest_bone) and recomposing the parent chain then applies
 * the joint offset twice. Use the decoded affine matrix AS STORED here.
 * This mode is experimental, explicitly selectable, and independent
 * of the legacy skeleton hierarchy / interpolation modules.
 */
int gs_pose_file_space(void){
    unsigned int i,j;
    if(!gs_is_open || gs3d_model.format!=7)return GS3D_ERR_UNSUPPORTED;
    if(gs3d_model.bones>GS3D_MAX_BONES)return GS3D_ERR_CAPACITY;
    for(i=0U;i<gs3d_model.bones;i++){
        for(j=0U;j<16U;j++)
            gs3d_skin_matrix[i].m[j]=gs_pose_local[i].m[j];
        /* All supported transforms are affine. Preserve the translation
           stored in indices 12-14; do NOT inject rest-space translations. */
        gs3d_skin_matrix[i].m[3]=0L;
        gs3d_skin_matrix[i].m[7]=0L;
        gs3d_skin_matrix[i].m[11]=0L;
        gs3d_skin_matrix[i].m[15]=GS3D_Q16_ONE;
        gs_pose_world[i]=gs3d_skin_matrix[i];
    }
    return GS3D_OK;
}
