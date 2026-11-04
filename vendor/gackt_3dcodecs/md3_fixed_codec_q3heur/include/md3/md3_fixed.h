#ifndef MD3_FIXED_H
#define MD3_FIXED_H

#include "md3_types.h"

md3_fixed_t md3_fixed_from_int(int value);
int md3_fixed_to_int_floor(md3_fixed_t value);
int md3_fixed_to_int_nearest(md3_fixed_t value);
md3_fixed_t md3_fixed_abs(md3_fixed_t value);
md3_fixed_t md3_fixed_mul(md3_fixed_t a, md3_fixed_t b);
md3_fixed_t md3_fixed_div(md3_fixed_t a, md3_fixed_t b);
md3_fixed_t md3_fixed_sqrt(md3_fixed_t value);
md3_fixed_t md3_fixed_lerp(md3_fixed_t a, md3_fixed_t b, md3_fixed_t t);

md3_fixed_t md3_fixed_from_md3_xyz(md3_i16 raw_value);
md3_i16 md3_fixed_to_md3_xyz(md3_fixed_t value);

void md3_vec3_clear(md3_vec3_t *out_vec);
void md3_vec3_copy(const md3_vec3_t *in_vec, md3_vec3_t *out_vec);
void md3_vec3_add(const md3_vec3_t *a, const md3_vec3_t *b,
    md3_vec3_t *out_vec);
void md3_vec3_sub(const md3_vec3_t *a, const md3_vec3_t *b,
    md3_vec3_t *out_vec);
void md3_vec3_scale(const md3_vec3_t *in_vec, md3_fixed_t scale,
    md3_vec3_t *out_vec);
md3_fixed_t md3_vec3_dot(const md3_vec3_t *a, const md3_vec3_t *b);
void md3_vec3_cross(const md3_vec3_t *a, const md3_vec3_t *b,
    md3_vec3_t *out_vec);
void md3_vec3_lerp(const md3_vec3_t *a, const md3_vec3_t *b,
    md3_fixed_t t, md3_vec3_t *out_vec);
md3_fixed_t md3_fixed_length3(md3_fixed_t x, md3_fixed_t y, md3_fixed_t z);
int md3_vec3_normalize(const md3_vec3_t *in_vec, md3_vec3_t *out_vec);

void md3_decode_packed_normal(md3_u16 packed_normal, md3_vec3_t *out_normal);
md3_u16 md3_encode_packed_normal(const md3_vec3_t *normal);

#endif
