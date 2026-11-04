#include "p3_ieee.h"

static p3_u8 p3_msb_index_u32(p3_u32 v) {
    p3_u8 i;
    i = 0U;
    while (v > 1UL) {
        v >>= 1;
        i = (p3_u8)(i + 1U);
    }
    return i;
}


static p3_u32 p3_f32_bits_from_f64_bits(p3_u32 lo_bits, p3_u32 hi_bits) {
    p3_u32 sign;
    p3_u32 exp64;
    p3_u32 mant_hi;
    p3_s32 e;
    p3_s32 exp32;
    p3_u32 top24;
    p3_u32 round_bit;
    p3_u32 sticky_bits;

    sign = hi_bits & 0x80000000UL;
    exp64 = (hi_bits >> 20) & 0x7ffUL;
    mant_hi = hi_bits & 0x000fffffUL;

    if (exp64 == 0UL) {
        return sign;
    }
    if (exp64 == 0x7ffUL) {
        return sign | 0x7f800000UL;
    }

    e = (p3_s32)exp64 - 1023;
    exp32 = e + 127;
    if (exp32 <= 0) {
        return sign;
    }
    if (exp32 >= 255) {
        return sign | 0x7f800000UL;
    }

    top24 = 0x800000UL | (mant_hi << 3) | (lo_bits >> 29);
    round_bit = lo_bits & 0x10000000UL;
    sticky_bits = lo_bits & 0x0fffffffUL;
    if (round_bit != 0UL && (sticky_bits != 0UL || (top24 & 1UL) != 0UL)) {
        top24 += 1UL;
    }
    if (top24 >= 0x1000000UL) {
        top24 >>= 1;
        exp32 += 1;
        if (exp32 >= 255) {
            return sign | 0x7f800000UL;
        }
    }

    return sign | ((p3_u32)exp32 << 23) | (top24 & 0x7fffffUL);
}

p3_fixed p3_fixed_from_f64_bits(p3_u32 lo_bits, p3_u32 hi_bits) {
    return p3_fixed_from_f32_bits(p3_f32_bits_from_f64_bits(lo_bits, hi_bits));
}

p3_fixed p3_fixed_from_f32_bits(p3_u32 bits) {
    p3_u32 sign;
    p3_u32 exp;
    p3_u32 mant;
    p3_u32 sig;
    p3_s32 shift;
    p3_u32 mag;

    sign = (bits >> 31) & 1UL;
    exp = (bits >> 23) & 0xffUL;
    mant = bits & 0x7fffffUL;

    if (exp == 0UL) {
        return 0;
    }
    if (exp == 0xffUL) {
        return sign ? (p3_fixed)0x80000000UL : (p3_fixed)0x7fffffffUL;
    }

    sig = 0x800000UL | mant;
    shift = (p3_s32)exp - 134;

    if (shift >= 24) {
        mag = 0x7fffffffUL;
    } else if (shift >= 0) {
        if (sig > (0xffffffffUL >> (p3_u32)shift)) {
            mag = 0x7fffffffUL;
        } else {
            mag = sig << (p3_u32)shift;
        }
    } else {
        p3_u32 rshift;
        rshift = (p3_u32)(0 - shift);
        if (rshift >= 32UL) {
            mag = 0UL;
        } else {
            mag = sig >> rshift;
            if (rshift > 0UL) {
                p3_u32 round_mask;
                round_mask = 1UL << (rshift - 1UL);
                if ((sig & round_mask) != 0UL) {
                    mag += 1UL;
                }
            }
        }
    }

    if (mag > 0x7fffffffUL) {
        mag = 0x7fffffffUL;
    }
    if (sign != 0UL) {
        return (p3_fixed)(0 - (p3_s32)mag);
    }
    return (p3_fixed)mag;
}

p3_u32 p3_f32_bits_from_fixed(p3_fixed value) {
    p3_u32 sign;
    p3_u32 mag;
    p3_u8 pos;
    p3_s32 e;
    p3_s32 exponent;
    p3_u32 sig;
    p3_s32 shift;

    if (value == 0) {
        return 0UL;
    }

    sign = 0UL;
    if (value < 0) {
        sign = 0x80000000UL;
        mag = (p3_u32)(0 - value);
    } else {
        mag = (p3_u32)value;
    }

    pos = p3_msb_index_u32(mag);
    e = (p3_s32)pos - 16;
    exponent = e + 127;
    if (exponent <= 0) {
        return sign;
    }
    if (exponent >= 255) {
        return sign | 0x7f800000UL;
    }

    shift = 23 - (p3_s32)pos;
    if (shift >= 0) {
        sig = mag << (p3_u32)shift;
    } else {
        p3_u32 rshift;
        p3_u32 remainder_mask;
        rshift = (p3_u32)(0 - shift);
        sig = mag >> rshift;
        if (rshift > 0UL) {
            remainder_mask = (1UL << rshift) - 1UL;
            if ((mag & remainder_mask) > (1UL << (rshift - 1UL))) {
                sig += 1UL;
            }
        }
    }

    if (sig >= 0x1000000UL) {
        sig >>= 1;
        exponent += 1;
        if (exponent >= 255) {
            return sign | 0x7f800000UL;
        }
    }

    return sign | ((p3_u32)exponent << 23) | (sig & 0x7fffffUL);
}
