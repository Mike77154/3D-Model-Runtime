#ifndef RTCW_SHADERSTYLE_H
#define RTCW_SHADERSTYLE_H
/* CC0-1.0. Strict C89 first-stage blend/alpha preview (not full Q3 shaders). */
#define RTCW_BLEND_AUTO 0
#define RTCW_BLEND_ALPHA 1
#define RTCW_BLEND_ADD 2
#define RTCW_BLEND_FILTER 3
#define RTCW_ALPHA_NONE 0
#define RTCW_ALPHA_GT0 1
#define RTCW_ALPHA_GE128 2
#define RTCW_ALPHA_LT128 3
typedef struct rtcw_shaderstyle_s {
    int blend;
    int alpha_func;
    unsigned int stage_count;
    unsigned int unhandled;
} rtcw_shaderstyle;
/* 1: material found, 0: no matching shader, -1: invalid source.
   Parse only first texture stage; count later stages as unhandled. */
int rtcw_shaderstyle_lookup(const char *filename,const char *material,
                            rtcw_shaderstyle *out);
const char *rtcw_shaderstyle_error(void);
#endif
