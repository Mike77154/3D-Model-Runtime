#include "ss89fxpasses.h"
int ss89_fx_clamp(int a) { return a<0?0:a>255?255:a; }
/* GL_DST_COLOR/GL_SRC_COLOR: approx 2 * base * detail / 255.
   The 128 gray level behaves like identity. */
int ss89_fx_detail_channel(int base,int detail,int tint)
{
    int mixed=128+((detail-128)*tint)/255;
    return ss89_fx_clamp((2*base*mixed+127)/255);
}
/* Luminance-difference approximation of SE1's signed two-offset bump passes.
   Exact tangent frames, projected light and hardware texture environment
   are renderer-dependent and NOT reproduced by this scalar helper. */
int ss89_fx_bump_channel(int base,int plus,int minus,int strength)
{
    int d=(minus-plus)*strength/255;
    return ss89_fx_clamp(base+d);
}
int ss89_fx_haze_channel(int base,int haze,int amt)
{
    amt=ss89_fx_clamp(amt);
    return ss89_fx_clamp((base*(255-amt)+haze*amt+127)/255);
}
int ss89_fx_decal_channel(int base,int color,int coverage)
{
    coverage=ss89_fx_clamp(coverage);
    return ss89_fx_clamp((base*(255-coverage)+color*coverage+127)/255);
}
int ss89_fx_shadow_channel(int base,int intensity)
{
    intensity=ss89_fx_clamp(intensity);
    return ss89_fx_clamp(base*(255-intensity)/255);
}
