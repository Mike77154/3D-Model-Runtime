#include "c4d89_animer.h"

int c4d89_animer_index(const char *path,C4D89_AnimIndex *out)
{return c4d89_scan_animation(path,out);}
int c4d89_animer_frame(const char *path,const C4D89_AnimIndex *idx,
    long seconds_q16,unsigned int rotation_shortarc,
    C4D89_FrameTrackFn cb,void *user,C4D89_FrameStats *stats)
{return c4d89_sample_animation_frame(path,idx,seconds_q16,
    rotation_shortarc,cb,user,stats);}
