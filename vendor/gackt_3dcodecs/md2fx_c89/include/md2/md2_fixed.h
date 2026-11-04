#ifndef MD2_FIXED_H
#define MD2_FIXED_H

#include "md2_types.h"

typedef md2_i32 md2_fx32_t;

#define MD2_FX_SHIFT 16
#define MD2_FX_ONE   ((md2_fx32_t)1 << MD2_FX_SHIFT)
#define MD2_FX_HALF  ((md2_fx32_t)1 << (MD2_FX_SHIFT - 1))

#define MD2_FX_FROM_INT(v) ((md2_fx32_t)((md2_i32)(v) << MD2_FX_SHIFT))
#define MD2_FX_TO_INT_FLOOR(v) ((md2_i32)((v) >> MD2_FX_SHIFT))
#define MD2_FX_TO_INT_ROUND(v) ((md2_i32)(((v) >= 0) ? (((v) + MD2_FX_HALF) >> MD2_FX_SHIFT) : -(((-(v)) + MD2_FX_HALF) >> MD2_FX_SHIFT)))

typedef struct md2_fx_vec3_s
{
    md2_fx32_t x;
    md2_fx32_t y;
    md2_fx32_t z;
} md2_fx_vec3_t;

md2_fx32_t md2_fx_abs(md2_fx32_t v);
md2_fx32_t md2_fx_div_i32(md2_fx32_t numer, md2_i32 denom);
int md2_fx_texel_center_div_i32(md2_i16 texel, md2_i32 size, md2_fx32_t *out_value);
md2_fx32_t md2_fx_ceil_div_i32(md2_fx32_t numer, md2_i32 denom);
md2_fx32_t md2_fx_clamp_unit(md2_fx32_t t);
md2_fx32_t md2_fx_mul_unit(md2_fx32_t value, md2_fx32_t unit_q16);
md2_fx32_t md2_fx_lerp(md2_fx32_t a, md2_fx32_t b, md2_fx32_t t);
md2_i32 md2_fx_quantize_byte(md2_fx32_t value, md2_fx32_t translate, md2_fx32_t scale);
md2_fx32_t md2_fx_dequantize_byte(md2_u8 q, md2_fx32_t translate, md2_fx32_t scale);

int md2_fx_from_f32_bits(md2_u32 bits, md2_fx32_t *out_value);
int md2_fx_to_f32_bits(md2_fx32_t value, md2_u32 *out_bits);

#endif /* MD2_FIXED_H */
