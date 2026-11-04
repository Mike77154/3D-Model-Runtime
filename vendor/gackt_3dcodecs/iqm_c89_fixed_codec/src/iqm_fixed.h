#ifndef IQM_FIXED_H
#define IQM_FIXED_H

#include "iqm_fixed_config.h"

/* C89-friendly integer assumptions: 8-bit char, 16-bit short, 32-bit int. */
typedef unsigned char iqm_u8;
typedef signed char iqm_i8;
typedef unsigned short iqm_u16;
typedef signed short iqm_i16;
typedef unsigned int iqm_u32;
typedef signed int iqm_i32;
typedef iqm_i32 iqm_fixed;

#if defined(_MSC_VER)
typedef signed __int64 iqm_i64;
typedef unsigned __int64 iqm_u64;
#else
typedef signed long long iqm_i64;
typedef unsigned long long iqm_u64;
#endif

#define IQM_FIXED_MAX ((iqm_fixed)0x7fffffff)
#define IQM_FIXED_MIN ((iqm_fixed)(-2147483647 - 1))

#ifdef __cplusplus
extern "C" {
#endif

iqm_fixed iqm_fixed_from_i32(iqm_i32 v);
iqm_fixed iqm_fixed_mul(iqm_fixed a, iqm_fixed b);
iqm_fixed iqm_fixed_mul_u16(iqm_fixed a, iqm_u16 b);
iqm_fixed iqm_fixed_from_raw_i64(iqm_i64 v);
iqm_fixed iqm_fixed_from_f32_bits(iqm_u32 bits);
iqm_fixed iqm_fixed_from_f16_bits(iqm_u16 bits);
int iqm_fixed_to_ascii(iqm_fixed v, char *dst, iqm_u32 dst_size, iqm_u32 frac_digits);

#ifdef __cplusplus
}
#endif

#endif
