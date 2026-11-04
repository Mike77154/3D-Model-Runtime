#include "../gs3d_internal.h"
/* Loop phase is normalized over count frames; each period has N spans,
   including the wrap from last to first. Tick products never overflow. */
int gs3d_sync_phase(const GS3D_Clip *c,unsigned long ticks,long *out){
    unsigned long step,rem,idx;long frac;
    if(!c||!out||c->mode!=GS3D_CLIP_LOOP||c->count<2U||
       c->count>GS3D_MAX_FRAMES||!c->ticks_per_frame)
        return GS3D_ERR_RANGE;
    step=ticks/c->ticks_per_frame;rem=ticks%c->ticks_per_frame;
    idx=step%(unsigned long)c->count;
    frac=gs3d_fraction_q16(rem,c->ticks_per_frame);
    *out=(long)((idx*65536UL+(unsigned long)frac)/(unsigned long)c->count);
    return GS3D_OK;
}
