#ifndef ASK3D_FIXED_H
#define ASK3D_FIXED_H

#ifdef __cplusplus
extern "C" {
#endif

#define ASK3D_FIXED_SHIFT 16
#define ASK3D_FIXED_ONE   65536L
#define ASK3D_FIXED_HALF  32768L

typedef long ask3d_i32;
typedef unsigned long ask3d_u32;
typedef long ask3d_fixed;

ask3d_fixed ask3d_fixed_from_int(long v);
ask3d_fixed ask3d_fixed_mul(ask3d_fixed a, ask3d_fixed b);
ask3d_fixed ask3d_fixed_div(ask3d_fixed a, ask3d_fixed b);
ask3d_fixed ask3d_fixed_parse(const char *text, int *ok);
void ask3d_fixed_to_ascii(ask3d_fixed v, char *out, unsigned int out_cap, unsigned int frac_digits);
void ask3d_i32_to_ascii(long v, char *out, unsigned int out_cap);

#ifdef __cplusplus
}
#endif

#endif
