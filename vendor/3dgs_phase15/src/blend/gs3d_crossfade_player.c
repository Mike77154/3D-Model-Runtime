#include "../gs3d_internal.h"
int gs3d_crossfade_init(GS3D_Crossfade *out,const GS3D_Clip *from,
    const GS3D_Clip *to,unsigned long from_ticks,unsigned long to_ticks,
    unsigned long fade_ticks){
    GS3D_Crossfade tmp;unsigned int track;int r;
    if(!out||!from||!to||!fade_ticks)return GS3D_ERR_RANGE;
    memset(&tmp,0,sizeof(tmp));
    tmp.from=*from;tmp.to=*to;
    tmp.from_ticks=from_ticks;tmp.to_ticks=to_ticks;
    tmp.fade_ticks=fade_ticks;tmp.playing=1U;
    r=gs3d_blend_validate(&tmp,&track);if(r)return r;
    *out=tmp;return GS3D_OK;
}
int gs3d_crossfade_seek(GS3D_Crossfade *f,unsigned long from_ticks,
    unsigned long to_ticks,unsigned long fade_elapsed){
    if(!f||!f->fade_ticks)return GS3D_ERR_RANGE;
    f->from_ticks=from_ticks;f->to_ticks=to_ticks;
    f->fade_elapsed=fade_elapsed>f->fade_ticks?f->fade_ticks:fade_elapsed;
    return GS3D_OK;
}
int gs3d_crossfade_advance(GS3D_Crossfade *f,unsigned long delta){
    if(!f||!f->fade_ticks)return GS3D_ERR_RANGE;
    if(!f->playing)return GS3D_OK;
    /* Transactional clock update: no partial increment on overflow. */
    if(delta>ULONG_MAX-f->from_ticks||delta>ULONG_MAX-f->to_ticks)
        return GS3D_ERR_RANGE;
    f->from_ticks+=delta;f->to_ticks+=delta;
    if(f->fade_elapsed>=f->fade_ticks||delta>=f->fade_ticks-f->fade_elapsed)
        f->fade_elapsed=f->fade_ticks;
    else f->fade_elapsed+=delta;
    return GS3D_OK;
}
int gs3d_crossfade_sample(const GS3D_Crossfade *f,GS3D_BlendSample *out){
    unsigned int track;int r;
    if(!out)return GS3D_ERR_RANGE;
    r=gs3d_blend_validate(f,&track);if(r)return r;
    r=gs3d_clip_resolve(&f->from,f->from_ticks,&out->from);if(r)return r;
    r=gs3d_clip_resolve(&f->to,f->to_ticks,&out->to);if(r)return r;
    out->weight_q16=f->fade_elapsed>=f->fade_ticks?GS3D_Q16_ONE:
        gs3d_fraction_q16(f->fade_elapsed,f->fade_ticks);
    return GS3D_OK;
}
