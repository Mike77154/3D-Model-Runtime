#ifndef VR1_FIXED_H
#define VR1_FIXED_H

#include "vr1_config.h"

typedef long vr1_fx;

#define VR1_FX_SHIFT 12
#define VR1_FX_ONE   (1L << VR1_FX_SHIFT)
#define VR1_FX_HALF  (1L << (VR1_FX_SHIFT - 1))
#define VR1_FX_MAX   0x3fffffffL
#define VR1_FX_MIN   (-0x3fffffffL)
#define VR1_FX_PI    12868L
#define VR1_FX_TWO_PI 25736L
#define VR1_FX_HALF_PI 6434L

vr1_fx vr1_fx_from_int(long v);
long   vr1_fx_to_int(vr1_fx v);
vr1_fx vr1_fx_abs(vr1_fx v);
vr1_fx vr1_fx_clamp_long(long v);
vr1_fx vr1_fx_mul(vr1_fx a, vr1_fx b);
vr1_fx vr1_fx_div(vr1_fx a, vr1_fx b);
vr1_fx vr1_fx_sqrt(vr1_fx v);
vr1_fx vr1_fx_sin(vr1_fx radians);
vr1_fx vr1_fx_cos(vr1_fx radians);
vr1_fx vr1_fx_from_ascii(const char *text, int len, int *ok);
void   vr1_fx_to_ascii(vr1_fx v, char *out, int out_cap, int decimals);

#endif
