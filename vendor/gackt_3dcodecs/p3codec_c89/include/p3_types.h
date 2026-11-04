#ifndef P3_TYPES_H
#define P3_TYPES_H

#include <limits.h>

#if UCHAR_MAX != 0xffU
#error "Unsupported platform: unsigned char is not 8-bit."
#endif

typedef signed char p3_s8;
typedef unsigned char p3_u8;

#if USHRT_MAX == 0xffffU
typedef signed short p3_s16;
typedef unsigned short p3_u16;
#else
#error "Unsupported platform: unsigned short is not 16-bit."
#endif

#if UINT_MAX == 0xffffffffUL
typedef signed int p3_s32;
typedef unsigned int p3_u32;
#elif ULONG_MAX == 0xffffffffUL
typedef signed long p3_s32;
typedef unsigned long p3_u32;
#else
#error "Unsupported platform: no 32-bit unsigned integer type found."
#endif

typedef p3_s32 p3_fixed;

#define P3_TRUE  1
#define P3_FALSE 0

#endif
