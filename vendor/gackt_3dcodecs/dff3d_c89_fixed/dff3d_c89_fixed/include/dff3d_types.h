#ifndef DFF3D_TYPES_H
#define DFF3D_TYPES_H

#include <limits.h>

#if UINT_MAX == 0xffffffffU
typedef unsigned int dff_u32;
typedef int dff_i32;
#elif ULONG_MAX == 0xffffffffUL
typedef unsigned long dff_u32;
typedef long dff_i32;
#else
#error "dff3d needs a 32-bit unsigned int or unsigned long"
#endif

#if USHRT_MAX == 0xffffU
typedef unsigned short dff_u16;
typedef short dff_i16;
#else
#error "dff3d needs a 16-bit unsigned short"
#endif

typedef unsigned char dff_u8;
typedef signed char dff_i8;

typedef dff_i32 dff_fixed;

#ifndef DFF3D_NULL
#define DFF3D_NULL ((void*)0)
#endif

#endif
