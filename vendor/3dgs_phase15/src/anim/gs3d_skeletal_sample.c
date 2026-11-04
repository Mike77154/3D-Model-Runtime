#include "../gs3d_internal.h"
/* Absolute fractional pose evaluation. Accepts one adjacent or arbitrary
   pair of MDL7 frames. Timing / FPS belongs to downstream clip scheduler. */
int gs3d_sample_skeletal_q16(unsigned int group,unsigned int frame_a,
                             unsigned int frame_b,long alpha_q16){
#if GS3D_WITH_MDL7
    int r;GS3D_Group *g;
    if(!gs_is_open||gs3d_model.format!=7||group>=gs3d_model.groups)
        return GS3D_ERR_UNSUPPORTED;
    g=&gs3d_model.group[group];
    if(frame_a>=g->frame_count||frame_b>=g->frame_count||
       alpha_q16<0L||alpha_q16>65536L)return GS3D_ERR_RANGE;
    /* Shared bone tracks in group 0, validated independently. */
    if(!gs3d_model.bones || frame_a>=gs3d_model.group[0].frame_count ||
       frame_b>=gs3d_model.group[0].frame_count)return GS3D_ERR_UNSUPPORTED;
    gs3d_pose_ready=0;
    r=gs_pose_sample_keys(frame_a,frame_b,alpha_q16);if(r)return r;
    r=gs_pose_hierarchy();if(r)return r;
    gs3d_pose_ready=1;
    r=gs3d_skeletal_sample_raw_vertices(group,frame_a,frame_b,alpha_q16);
    if(r){gs3d_pose_ready=0;return r;}
    r=gs_weighted_skin_group(group);
    if(r)gs3d_pose_ready=0;
    return r;
#else
    (void)group;(void)frame_a;(void)frame_b;(void)alpha_q16;
    return GS3D_ERR_UNSUPPORTED;
#endif
}
