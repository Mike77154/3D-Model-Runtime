#ifndef DBO_FIXED_H
#define DBO_FIXED_H

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char dbo_u8;
typedef unsigned short dbo_u16;

#if UINT_MAX >= 0xFFFFFFFFUL
typedef unsigned int dbo_u32;
typedef int dbo_s32;
#elif ULONG_MAX >= 0xFFFFFFFFUL
typedef unsigned long dbo_u32;
typedef long dbo_s32;
#else
#error "Need a 32-bit unsigned integer type."
#endif

typedef int dbo_bool;
typedef dbo_s32 dbo_fx32;

#define DBO_TRUE 1
#define DBO_FALSE 0

#define DBO_FX_SHIFT 16
#define DBO_FX_ONE   ((dbo_fx32)65536L)
#define DBO_FX_HALF  ((dbo_fx32)32768L)

dbo_fx32 dbo_fx_from_int(int v);
int dbo_fx_to_int(dbo_fx32 v);
dbo_fx32 dbo_fx_from_float_bits(dbo_u32 bits);
dbo_u32 dbo_fx_to_float_bits(dbo_fx32 value);
void dbo_fx_identity4(dbo_fx32 out16[16]);

#ifdef __cplusplus
}
#endif

#endif
