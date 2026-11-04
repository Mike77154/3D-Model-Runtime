#ifndef P3_IEEE_H
#define P3_IEEE_H

#include "p3_fixed.h"
#include "p3_types.h"

p3_fixed p3_fixed_from_f32_bits(p3_u32 bits);
p3_fixed p3_fixed_from_f64_bits(p3_u32 lo_bits, p3_u32 hi_bits);
p3_u32 p3_f32_bits_from_fixed(p3_fixed value);

#endif
