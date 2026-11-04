#include "../gs3d_internal.h"
#include <limits.h>
#if GS3D_WITH_MDL7
static long safe_sub(long a,long b){
    if(b==LONG_MIN){
        if(a>=0L)return LONG_MAX;
        return gs_addsat(gs_addsat(a,LONG_MAX),1L);
    }
    return gs_addsat(a,-b);
}
static int root_position(const GS3D_Clip *clip,unsigned long tick,
    unsigned int root,long position[3]){
    GS3D_ClipSample s;int r;
    r=gs3d_clip_resolve(clip,tick,&s);if(r)return r;
    r=gs_pose_sample_keys(s.frame_a,s.frame_b,s.alpha_q16);
    if(r)return r;
    position[0]=gs_pose_local[root].m[12];
    position[1]=gs_pose_local[root].m[13];
    position[2]=gs_pose_local[root].m[14];
    return GS3D_OK;
}
int gs3d_root_motion_delta(const GS3D_Clip *clip,unsigned long previous_ticks,
    unsigned long current_ticks,unsigned int root,GS3D_RootDelta *out){
    GS3D_RootDelta temp;long prev[3];unsigned int i;int r;
    if(!out||!clip||!gs_is_open)return GS3D_ERR_RANGE;
    if(gs3d_model.format!=7||!gs3d_model.bones)
        return GS3D_ERR_UNSUPPORTED;
    if(root>=gs3d_model.bones ||gs3d_model.bone[root].parent!=65535U)
        return GS3D_ERR_RANGE;
    if(clip->mode!=GS3D_CLIP_CLAMP||previous_ticks>current_ticks ||
       clip->track==GS3D_CLIP_TRACK_MORPH)
        return GS3D_ERR_UNSUPPORTED;
    if(!gs3d_family_supports(clip->generation,gs3d_model.format) ||
       clip->group>=gs3d_model.groups ||
       clip->first>=gs3d_model.group[clip->group].frame_count ||
       clip->count>gs3d_model.group[clip->group].frame_count-clip->first ||
       clip->first>=gs3d_model.group[0].frame_count ||
       clip->count>gs3d_model.group[0].frame_count-clip->first)
        return GS3D_ERR_RANGE;
    gs3d_pose_ready=0;
    r=root_position(clip,previous_ticks,root,prev);if(r)return r;
    r=root_position(clip,current_ticks,root,temp.position_q16);if(r)return r;
    for(i=0U;i<3U;i++)temp.delta_q16[i]=safe_sub(temp.position_q16[i],prev[i]);
    *out=temp;return GS3D_OK;
}
#else
int gs3d_root_motion_delta(const GS3D_Clip *clip,unsigned long previous_ticks,
    unsigned long current_ticks,unsigned int root,GS3D_RootDelta *out){
    (void)clip;(void)previous_ticks;(void)current_ticks;(void)root;(void)out;
    return GS3D_ERR_UNSUPPORTED;
}
#endif
