#ifndef RTCW_STAGEFX_H
#define RTCW_STAGEFX_H
/* Independent preview-only shader-stage subset. CC0-1.0; ISO C89, no heap/FP. */
#define RTCW_FX_MAX_STAGES 4U
#define RTCW_FX_Q 4096L
/* Blend & alpha constants remain compatible with rtcw_shaderstyle.h. */
#include "rtcw_shaderstyle.h"
typedef struct rtcw_fx_stage_s {
    char map[256];
    int blend,alpha_func;
    long scale_s,scale_t;          /* Q12 */
    long scroll_s,scroll_t;        /* Q12 texcoords/second */
    unsigned int rgb[3],alpha;   /* modulation, 0..255 */
    unsigned int unhandled;
} rtcw_fx_stage;
typedef struct rtcw_fx_s {
    rtcw_fx_stage stage[RTCW_FX_MAX_STAGES];
    unsigned int stages,unhandled;
} rtcw_fx;
/* 1 material found, 0 not found, -1 invalid/truncated source. */
int rtcw_fx_lookup(const char *filename,const char *material,unsigned long time_ms,rtcw_fx *out);
const char *rtcw_fx_error(void);
/* Q12 value from a UV at time in milliseconds; no floating point. */
long rtcw_fx_uv(long uv,long scale,long scroll,unsigned long time_ms);
#endif
