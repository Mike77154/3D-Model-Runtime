#ifndef MD2_TYPES_H
#define MD2_TYPES_H

#include <limits.h>

#if UCHAR_MAX != 255
#error "md2fx requires 8-bit unsigned char"
#endif

#if USHRT_MAX != 65535U
#error "md2fx requires 16-bit unsigned short"
#endif

#if UINT_MAX < 0xFFFFFFFFUL
#error "md2fx requires an unsigned int with at least 32 bits"
#endif

typedef signed char md2_i8;
typedef unsigned char md2_u8;
typedef signed short md2_i16;
typedef unsigned short md2_u16;
typedef signed int md2_i32;
typedef unsigned int md2_u32;

#endif /* MD2_TYPES_H */
