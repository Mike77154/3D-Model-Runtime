/* fbx_types.h - "C89 style" fixed-width integer types
 *
 * FBX needs 32-bit and 64-bit integers (IDs, offsets). C89 itself doesn't
 * define exact-width types, so this header derives them from <limits.h> and
 * uses common compiler extensions for 64-bit.
 */
#ifndef FBX_TYPES_H
#define FBX_TYPES_H

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 8-bit ---- */
#if UCHAR_MAX != 0xFF
# error "3dfbx requires 8-bit unsigned char"
#endif

typedef signed char   fbx_i8;
typedef unsigned char fbx_u8;

/* ---- 16-bit ---- */
#if USHRT_MAX == 0xFFFF
typedef signed short   fbx_i16;
typedef unsigned short fbx_u16;
#else
# error "3dfbx requires 16-bit unsigned short"
#endif

/* ---- 32-bit ---- */
#if UINT_MAX == 0xFFFFFFFFu
typedef signed int   fbx_i32;
typedef unsigned int fbx_u32;
#elif ULONG_MAX == 0xFFFFFFFFul
typedef signed long   fbx_i32;
typedef unsigned long fbx_u32;
#else
# error "3dfbx: could not find a 32-bit integer type on this platform"
#endif

/* ---- 64-bit ----
 * C89 has no standard 64-bit integer. We require a compiler extension.
 */
#if defined(_MSC_VER)
typedef signed __int64   fbx_i64;
typedef unsigned __int64 fbx_u64;
#else
/* "long long" is not part of ISO C90, but it is required for FBX offsets.
   Use __extension__ to silence -pedantic on GCC/Clang. */
#if defined(__GNUC__) || defined(__clang__)
__extension__ typedef signed long long   fbx_i64;
__extension__ typedef unsigned long long fbx_u64;
#else
typedef signed long long   fbx_i64;
typedef unsigned long long fbx_u64;
#endif
#endif

#define FBX_U32_MAX ((fbx_u32)0xFFFFFFFFu)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_TYPES_H */
