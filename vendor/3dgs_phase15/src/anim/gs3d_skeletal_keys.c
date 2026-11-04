#include "../gs3d_internal.h"
/* Per-key local TRS interpolation, leaving each key's matrix intact at endpoints.
   Missing bone keys use the existing identity local-pose convention. */
#if GS3D_WITH_MDL7
static GS3D_Mat gs_sample_key_a[GS3D_MAX_BONES];
int gs_pose_sample_keys(unsigned int frame_a,unsigned int frame_b,long alpha){
    unsigned int i;int r;
    if(gs3d_model.format!=7||!gs3d_model.groups||!gs3d_model.bones)
        return GS3D_ERR_UNSUPPORTED;
    if(alpha<0L||alpha>65536L)return GS3D_ERR_RANGE;
    r=gs_pose_load_keys(frame_a);if(r)return r;
    for(i=0U;i<gs3d_model.bones;i++)gs_sample_key_a[i]=gs_pose_local[i];
    if(frame_a==frame_b||alpha==0L)return GS3D_OK;
    r=gs_pose_load_keys(frame_b);if(r)return r;
    if(alpha==65536L)return GS3D_OK;
    for(i=0U;i<gs3d_model.bones;i++){
        GS3D_Mat m;
        r=gs3d_trs_interpolate(&gs_sample_key_a[i],&gs_pose_local[i],alpha,&m);
        if(r)return r;
        gs_pose_local[i]=m;
    }
    return GS3D_OK;
}
#else
int gs_pose_sample_keys(unsigned int a,unsigned int b,long alpha){
    (void)a;(void)b;(void)alpha;return GS3D_ERR_UNSUPPORTED;
}
#endif
