/* fbx_stdint.h
   Minimal fixed-width integer types for C89/C90.

   The library targets "portable C89" but still needs 64-bit integers
   for FBX 7.5+ (node headers use 64-bit offsets).

   Strategy:
   - On modern compilers, include <stdint.h> when it is known to exist.
   - On older MSVC, use the __intX built-ins.
   - Otherwise, fall back to common C89 assumptions:
       char  = 8-bit
       short = 16-bit
       int   = 32-bit
       long long / unsigned long long available as an extension.
*/

#ifndef FBX_STDINT_H
#define FBX_STDINT_H

#include <limits.h>

/* Prefer the system <stdint.h> when available */
#if defined(_MSC_VER)
  /* Visual Studio 2010+ ships <stdint.h> */
  #if (_MSC_VER >= 1600)
    #include <stdint.h>
  #else
    typedef signed __int8    int8_t;
    typedef unsigned __int8  uint8_t;
    typedef signed __int16   int16_t;
    typedef unsigned __int16 uint16_t;
    typedef signed __int32   int32_t;
    typedef unsigned __int32 uint32_t;
    typedef signed __int64   int64_t;
    typedef unsigned __int64 uint64_t;
  #endif
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
  #include <stdint.h>
#else
  /* C89 fallback */
  typedef signed char        int8_t;
  typedef unsigned char      uint8_t;
  typedef signed short       int16_t;
  typedef unsigned short     uint16_t;
  typedef signed int         int32_t;
  typedef unsigned int       uint32_t;

  /* 64-bit integers are not standard in C89, but are widely supported */
  #if defined(_MSC_VER)
    typedef signed __int64   int64_t;
    typedef unsigned __int64 uint64_t;
  #else
    typedef long long            int64_t;
    typedef unsigned long long   uint64_t;
  #endif
#endif

/* ---- Compile-time sanity checks ---- */
typedef char fbx_static_assert_int16_size[(sizeof(int16_t)  == 2) ? 1 : -1];
typedef char fbx_static_assert_uint16_size[(sizeof(uint16_t) == 2) ? 1 : -1];
typedef char fbx_static_assert_int32_size[(sizeof(int32_t)  == 4) ? 1 : -1];
typedef char fbx_static_assert_uint32_size[(sizeof(uint32_t) == 4) ? 1 : -1];
typedef char fbx_static_assert_int64_size[(sizeof(int64_t)  == 8) ? 1 : -1];
typedef char fbx_static_assert_uint64_size[(sizeof(uint64_t) == 8) ? 1 : -1];

#endif /* FBX_STDINT_H */
