#ifndef P3_FIXED_H
#define P3_FIXED_H

#include "p3_types.h"

#define P3_FIXED_ONE  ((p3_fixed)65536L)
#define P3_FIXED_HALF ((p3_fixed)32768L)

p3_fixed p3_fixed_from_int(int v);
int p3_fixed_to_int(p3_fixed v);
int p3_fixed_from_str(const char *text, p3_fixed *out_value);
void p3_fixed_to_str(p3_fixed value, char *dst);
p3_fixed p3_fixed_mul(p3_fixed a, p3_fixed b);
p3_fixed p3_fixed_sin_deg(p3_fixed angle_deg);
p3_fixed p3_fixed_cos_deg(p3_fixed angle_deg);

#endif
