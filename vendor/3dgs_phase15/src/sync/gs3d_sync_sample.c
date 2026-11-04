#include "../gs3d_internal.h"
/* Follower can have a different frame count and tick rate. It receives
   the same cyclic phase, not the same absolute frame or wall clock. */
int gs3d_sync_sample(const GS3D_Clip *master,unsigned long master_ticks,
    const GS3D_Clip *follower,long offset,GS3D_ClipSample *out){
    long phase;unsigned long scaled,idx;int status;
    if(!master||!follower||!out||offset<0L||offset>=65536L||
       follower->mode!=GS3D_CLIP_LOOP||follower->count<2U||
       follower->count>GS3D_MAX_FRAMES||!follower->ticks_per_frame||
       (unsigned long)follower->first+follower->count>65535UL)
        return GS3D_ERR_RANGE;
    status=gs3d_sync_phase(master,master_ticks,&phase);if(status)return status;
    phase=(phase+offset)&65535L;
    scaled=(unsigned long)phase*(unsigned long)follower->count;
    idx=scaled>>16;
    if(idx>=(unsigned long)follower->count)return GS3D_ERR_CORRUPT;
    out->frame_a=(unsigned short)((unsigned long)follower->first+idx);
    out->frame_b=(unsigned short)((unsigned long)follower->first+
       (idx+1UL)%(unsigned long)follower->count);
    out->alpha_q16=(long)(scaled&65535UL);
    out->direction=0U;out->at_end=0U;
    return GS3D_OK;
}
