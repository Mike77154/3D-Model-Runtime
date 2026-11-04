#ifndef FBX_FIXED_H
#define FBX_FIXED_H

#include "fbx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FBX_REAL_SHIFT
#define FBX_REAL_SHIFT 16
#endif

#define FBX_REAL_ONE      ((fbx_real)(1L << FBX_REAL_SHIFT))
#define FBX_REAL_ZERO     ((fbx_real)0)
#define FBX_REAL_HALF     ((fbx_real)(1L << (FBX_REAL_SHIFT - 1)))
#define FBX_REAL_EPSILON  ((fbx_real)1)
#define FBX_REAL_PI       ((fbx_real)205887L)
#define FBX_REAL_TWO_PI   ((fbx_real)411775L)
#define FBX_REAL_HALF_PI  ((fbx_real)102944L)
#define FBX_REAL_DEG2RAD  ((fbx_real)1144L)
#define FBX_REAL_MAX      ((fbx_real)0x7fffffffL)
#define FBX_REAL_MIN      ((fbx_real)(-0x7fffffffL - 1L))

typedef fbx_i32 fbx_real; /* signed Q16.16 by default */

fbx_real fbx_real_from_int(fbx_i32 v);
fbx_real fbx_real_from_i64(fbx_i64 v);
fbx_real fbx_real_from_ratio(fbx_i64 num, fbx_i64 den);
fbx_real fbx_real_from_ascii(const char *text);
fbx_real fbx_real_from_ieee32_bits(fbx_u32 bits);
fbx_real fbx_real_from_ieee64_bits(fbx_u64 bits);
fbx_u32  fbx_real_to_ieee32_bits(fbx_real v);
fbx_u64  fbx_real_to_ieee64_bits(fbx_real v);

fbx_i32  fbx_real_to_int_round(fbx_real v);
fbx_real fbx_real_abs(fbx_real v);
fbx_real fbx_real_mul(fbx_real a, fbx_real b);
fbx_real fbx_real_div(fbx_real a, fbx_real b);
fbx_real fbx_real_sqrt(fbx_real v);
fbx_real fbx_real_sin_rad(fbx_real rad);
fbx_real fbx_real_cos_rad(fbx_real rad);
fbx_real fbx_real_tan_rad(fbx_real rad);
fbx_real fbx_real_deg_to_rad(fbx_real deg);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_FIXED_H */
