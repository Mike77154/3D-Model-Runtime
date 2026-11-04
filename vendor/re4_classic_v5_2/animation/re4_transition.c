/* Time-varying crossfade weights for RE4 animation, white-room implementation.
   C89, deterministic 32-bit fixed point; no float, heap or unsafe shift. */
#include "re4c3d/re4_transition.h"
/* Exact floor((a*b)/65536) for [0,65536] using 32-bit partial products. */
static re4_u32 multiply_q16(re4_u32 a, re4_u32 b)
{
    re4_u32 ah=a>>8, bh=b>>8, al=a&255U, bl=b&255U;
    re4_u32 cross=ah*bl+bh*al;
    return ah*bh+((cross*256U+al*bl)>>16);
}
re4_err re4_transition_weight(const re4_transition *t,re4_u16 frame,
    re4_q16 *out_weight)
{
    re4_u32 span,elapsed,x,p[6],n,step,i;
    if(!t||!out_weight)return RE4_ERR_NULL;
    if(t->last<=t->first||t->curve>RE4_TRANSITION_SMOOTHERSTEP)
        return RE4_ERR_BAD_FORMAT;
    if(frame<=t->first){*out_weight=0;return RE4_OK;}
    if(frame>=t->last){*out_weight=65536;return RE4_OK;}
    span=(re4_u32)t->last-(re4_u32)t->first;
    elapsed=(re4_u32)frame-(re4_u32)t->first;
    x=(elapsed*65536U)/span;
    if(t->curve==RE4_TRANSITION_LINEAR){*out_weight=(re4_q16)x;return RE4_OK;}
    /* Monotone Bezier control points (smoothstep: cubic, smootherstep:
       quintic). Integer de Casteljau uses convex combinations only, so
       quantization cannot produce a backward step in the transition. */
    n=(t->curve==RE4_TRANSITION_SMOOTHSTEP)?4U:6U;
    for(i=0;i<n;++i)p[i]=(i>=n/2U)?65536U:0U;
    for(step=n-1U;step>0U;--step){
        for(i=0;i<step;++i)
            p[i]+=multiply_q16(x,p[i+1U]-p[i]);
    }
    *out_weight=(re4_q16)p[0];
    return RE4_OK;
}
