#ifndef DFF3D_FIXED_H
#define DFF3D_FIXED_H

#include "dff3d_types.h"

#define DFF3D_FIXED_SHIFT 16
#define DFF3D_FIXED_ONE ((dff_fixed)65536)
#define DFF3D_FIXED_HALF ((dff_fixed)32768)
#define DFF3D_FIXED_MAX ((dff_fixed)0x7fffffffL)
#define DFF3D_FIXED_MIN ((dff_fixed)(-2147483647L - 1L))

dff_fixed dff3d_fixed_from_f32_bits(dff_u32 bits);
dff_fixed dff3d_fixed_from_int(dff_i32 x);
dff_i32 dff3d_fixed_to_int_floor(dff_fixed x);
dff_fixed dff3d_fixed_mul(dff_fixed a, dff_fixed b);
dff_fixed dff3d_fixed_add_sat(dff_fixed a, dff_fixed b);

#endif
