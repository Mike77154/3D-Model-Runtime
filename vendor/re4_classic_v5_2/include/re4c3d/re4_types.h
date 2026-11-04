#ifndef RE4_TYPES_H
#define RE4_TYPES_H

#include <limits.h>

#if UCHAR_MAX != 0xFF
#error "re4c3d needs 8-bit unsigned char"
#endif
#if USHRT_MAX != 0xFFFF
#error "re4c3d needs 16-bit unsigned short"
#endif
#if UINT_MAX != 0xFFFFFFFFU
#error "re4c3d needs 32-bit unsigned int"
#endif

typedef unsigned char  re4_u8;
typedef signed char    re4_i8;
typedef unsigned short re4_u16;
typedef signed short   re4_i16;
typedef unsigned int   re4_u32;
typedef signed int     re4_i32;
typedef unsigned long  re4_size;
typedef int            re4_bool;
typedef re4_i32        re4_q16;

#define RE4_TRUE  1
#define RE4_FALSE 0

#define RE4_U32_MAX ((re4_u32)0xFFFFFFFFU)
#define RE4_I32_MAX ((re4_i32)0x7FFFFFFF)
#define RE4_I32_MIN ((re4_i32)(-2147483647 - 1))

typedef enum re4_err_e {
    RE4_OK = 0,
    RE4_ERR_NULL = 1,
    RE4_ERR_RANGE = 2,
    RE4_ERR_BAD_MAGIC = 3,
    RE4_ERR_BAD_FORMAT = 4,
    RE4_ERR_NO_SPACE = 5,
    RE4_ERR_UNSUPPORTED = 6,
    RE4_ERR_NEED_EXTERNAL_CODEC = 7,
    RE4_ERR_TODO = 8
} re4_err;

typedef enum re4_endian_e {
    RE4_ENDIAN_LITTLE = 0,
    RE4_ENDIAN_BIG = 1
} re4_endian;

typedef struct re4_span_s {
    const re4_u8 *data;
    re4_size size;
} re4_span;

typedef struct re4_strref_s {
    const char *ptr;
    re4_size len;
} re4_strref;

typedef re4_err (*re4_emit_fn)(void *user, const char *text, re4_size len);

#endif
