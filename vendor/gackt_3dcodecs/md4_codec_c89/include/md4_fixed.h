#ifndef MD4_FIXED_H
#define MD4_FIXED_H

#include "md4_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MD4_FX_ONE ((md4_fx_t)MD4_CFG_FX_ONE)

md4_fx_t md4_fx_from_int(md4_i32 v);
md4_i32 md4_fx_to_int_floor(md4_fx_t v);
md4_i32 md4_fx_to_int_round(md4_fx_t v);
md4_fx_t md4_fx_mul(md4_fx_t a, md4_fx_t b);
md4_fx_t md4_fx_div(md4_fx_t a, md4_fx_t b);
md4_fx_t md4_fx_lerp(md4_fx_t a, md4_fx_t b, md4_fx_t t);
md4_fx_t md4_fx_abs(md4_fx_t v);
md4_fx_t md4_fx_dot3(const md4_fx_t a[3], const md4_fx_t b[3]);

md4_fx_t md4_fx_from_ieee754_bits(md4_u32 bits);
md4_u32 md4_fx_to_ieee754_bits(md4_fx_t v);

#ifdef __cplusplus
}
#endif

#endif
