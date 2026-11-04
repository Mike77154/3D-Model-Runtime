#include "../gs3d_internal.h"
int gs3d_player_init(GS3D_ClipPlayer *p,const GS3D_Clip *c){
    if(!p||!c||!c->ticks_per_frame||!c->count)return GS3D_ERR_RANGE;
    p->clip=*c;p->elapsed_ticks=0UL;p->playing=1U;return GS3D_OK;
}
int gs3d_player_seek(GS3D_ClipPlayer *p,unsigned long ticks){
    if(!p)return GS3D_ERR_RANGE;
    p->elapsed_ticks=ticks;return GS3D_OK;
}
int gs3d_player_advance(GS3D_ClipPlayer *p,unsigned long dt){
    if(!p)return GS3D_ERR_RANGE;
    if(!p->playing)return GS3D_OK;
    if(dt>ULONG_MAX-p->elapsed_ticks)return GS3D_ERR_RANGE;
    p->elapsed_ticks+=dt;
    return GS3D_OK;
}
int gs3d_player_sample(const GS3D_ClipPlayer *p,GS3D_ClipSample *sample){
    if(!p)return GS3D_ERR_RANGE;
    return gs3d_clip_resolve(&p->clip,p->elapsed_ticks,sample);
}
int gs3d_player_apply(const GS3D_ClipPlayer *p,GS3D_ClipSample *sample){
    if(!p)return GS3D_ERR_RANGE;
    return gs3d_clip_apply(&p->clip,p->elapsed_ticks,sample);
}
