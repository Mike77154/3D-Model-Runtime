#include "../gs3d_internal.h"
/* Unsigned 16-step long division. Computes floor(rem * 2^16 / den)
   without overflow, even for a full-width 32-bit Win32 tick counter. */
long gs3d_fraction_q16(unsigned long rem,unsigned long den){
    unsigned int i;unsigned long result=0UL;
    for(i=0U;i<16U;i++){
        result<<=1;
        if(rem>=den-rem){rem-=den-rem;result|=1UL;}
        else rem+=rem;
    }
    return (long)result;
}
int gs3d_clip_resolve(const GS3D_Clip *c,unsigned long elapsed,
    GS3D_ClipSample *out){
    unsigned long step,remainder,pos,period,idx,idx2;
    if(!c||!out||!c->ticks_per_frame||!c->count||
       c->mode>GS3D_CLIP_PINGPONG)return GS3D_ERR_RANGE;
    if((unsigned long)c->first+(unsigned long)c->count>65535UL)
        return GS3D_ERR_RANGE;
    step=elapsed/c->ticks_per_frame;
    remainder=elapsed%c->ticks_per_frame;
    out->at_end=0U;out->direction=0U;
    if(c->count==1U){idx=idx2=0UL;remainder=0UL;out->at_end=1U;}
    else if(c->mode==GS3D_CLIP_CLAMP){
        if(step>=(unsigned long)c->count-1UL){
            idx=idx2=(unsigned long)c->count-1UL;
            remainder=0UL;out->at_end=1U;
        }else{idx=step;idx2=idx+1UL;}
    }else if(c->mode==GS3D_CLIP_LOOP){
        period=(unsigned long)c->count;
        idx=step%period;idx2=(idx+1UL)%period;
    }else{
        period=((unsigned long)c->count-1UL)*2UL;
        pos=step%period;
        if(pos<(unsigned long)c->count-1UL){idx=pos;idx2=idx+1UL;}
        else{idx=period-pos;idx2=idx-1UL;out->direction=1U;}
    }
    out->frame_a=(unsigned short)((unsigned long)c->first+idx);
    out->frame_b=(unsigned short)((unsigned long)c->first+idx2);
    out->alpha_q16=gs3d_fraction_q16(remainder,c->ticks_per_frame);
    return GS3D_OK;
}
