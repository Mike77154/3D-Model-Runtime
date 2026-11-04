/* Cinema4DCodec89 independent animer API. C89 / CC0. */
#ifndef C4D89_MODULE_ANIMER_H
#define C4D89_MODULE_ANIMER_H
#include "c4d89.h"
/* Track and rig evidence; skinning requires validated external weights. */
int c4d89_animer_index(const char *path,C4D89_AnimIndex *out);
int c4d89_animer_frame(const char *path,const C4D89_AnimIndex *idx,
    long seconds_q16,unsigned int rotation_shortarc,
    C4D89_FrameTrackFn cb,void *user,C4D89_FrameStats *stats);
#endif
