#ifndef STL3D_FIXED_H
#define STL3D_FIXED_H

/*
   stl3d_fixed.h - tiny Q16.16 fixed-point helpers, C89 friendly.
   No malloc, no free, no realloc, no floating-point operations.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STL3D_FIXED_SHIFT
#define STL3D_FIXED_SHIFT 16
#endif

#define STL3D_FIXED_ONE     (1L << STL3D_FIXED_SHIFT)
#define STL3D_FIXED_HALF    (1L << (STL3D_FIXED_SHIFT - 1))
#define STL3D_FIXED_MAX     (2147483647L)
#define STL3D_FIXED_MIN     (-2147483647L)

typedef signed long    stl3d_fixed;
typedef unsigned char  stl3d_u8;
typedef unsigned short stl3d_u16;
typedef unsigned long  stl3d_u32;
typedef signed long    stl3d_s32;

stl3d_fixed stl3d_fixed_from_ascii(const char *text, int *ok);
stl3d_fixed stl3d_fixed_from_f32le(const stl3d_u8 b[4]);
stl3d_fixed stl3d_fixed_from_f32le_ex(const stl3d_u8 b[4], int *non_finite, int *saturated);
int stl3d_f32le_is_nonfinite(const stl3d_u8 b[4]);
void stl3d_fixed_to_ascii(stl3d_fixed v, char *out, unsigned long out_cap, unsigned int decimals);

#ifdef __cplusplus
}
#endif

#endif /* STL3D_FIXED_H */
