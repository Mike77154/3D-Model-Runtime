#include "md4_internal.h"
#include <limits.h>

static md4_fx_t md4_fx_sat_from_u32(md4_u32 v, md4_i32 negative)
{
    if (!negative) {
        if (v > 0x7fffffffU) {
            return (md4_fx_t)0x7fffffff;
        }
        return (md4_fx_t)v;
    }
    if (v >= 0x80000000U) {
        return (md4_fx_t)0x80000000U;
    }
    return (md4_fx_t)(-(md4_i32)v);
}

static md4_u32 md4_abs_u32(md4_fx_t v)
{
    if (v >= 0) {
        return (md4_u32)v;
    }
    if (v == (md4_fx_t)0x80000000U) {
        return 0x80000000U;
    }
    return (md4_u32)(-v);
}

md4_fx_t md4_fx_from_int(md4_i32 v)
{
    if (v > (0x7fffffff >> MD4_CFG_FX_FRAC_BITS)) {
        return (md4_fx_t)0x7fffffff;
    }
    if (v < -(0x7fffffff >> MD4_CFG_FX_FRAC_BITS) - 1) {
        return (md4_fx_t)0x80000000U;
    }
    return (md4_fx_t)(v << MD4_CFG_FX_FRAC_BITS);
}

md4_i32 md4_fx_to_int_floor(md4_fx_t v)
{
    if (v >= 0) {
        return (md4_i32)((md4_u32)v >> MD4_CFG_FX_FRAC_BITS);
    }
    return (md4_i32)(-((md4_i32)((md4_abs_u32(v) + (MD4_FX_ONE - 1)) >> MD4_CFG_FX_FRAC_BITS)));
}

md4_i32 md4_fx_to_int_round(md4_fx_t v)
{
    if (v >= 0) {
        return (md4_i32)(((md4_u32)v + (MD4_FX_ONE >> 1)) >> MD4_CFG_FX_FRAC_BITS);
    }
    return -((md4_i32)((md4_abs_u32(v) + (MD4_FX_ONE >> 1)) >> MD4_CFG_FX_FRAC_BITS));
}

md4_fx_t md4_fx_abs(md4_fx_t v)
{
    if (v >= 0) {
        return v;
    }
    if (v == (md4_fx_t)0x80000000U) {
        return (md4_fx_t)0x7fffffff;
    }
    return (md4_fx_t)(-v);
}

md4_fx_t md4_fx_mul(md4_fx_t a, md4_fx_t b)
{
    md4_u32 ua;
    md4_u32 ub;
    md4_u32 ah;
    md4_u32 al;
    md4_u32 bh;
    md4_u32 bl;
    md4_u32 part0;
    md4_u32 part1;
    md4_u32 part2;
    md4_u32 part3;
    md4_u32 res;
    md4_i32 neg;

    if (a == 0 || b == 0) {
        return 0;
    }

    neg = ((a < 0) ^ (b < 0)) ? 1 : 0;
    ua = md4_abs_u32(a);
    ub = md4_abs_u32(b);

    ah = ua >> 16;
    al = ua & 0xffffU;
    bh = ub >> 16;
    bl = ub & 0xffffU;

    if (ah != 0U && bh != 0U && ah * bh > 0x7fffU) {
        return neg ? (md4_fx_t)0x80000000U : (md4_fx_t)0x7fffffff;
    }

    part0 = (ah * bh) << 16;
    part1 = ah * bl;
    part2 = al * bh;
    part3 = (al * bl) >> 16;

    if (part0 > 0xffffffffU - part1) {
        return neg ? (md4_fx_t)0x80000000U : (md4_fx_t)0x7fffffff;
    }
    res = part0 + part1;
    if (res > 0xffffffffU - part2) {
        return neg ? (md4_fx_t)0x80000000U : (md4_fx_t)0x7fffffff;
    }
    res += part2;
    if (res > 0xffffffffU - part3) {
        return neg ? (md4_fx_t)0x80000000U : (md4_fx_t)0x7fffffff;
    }
    res += part3;

    return md4_fx_sat_from_u32(res, neg);
}

md4_fx_t md4_fx_div(md4_fx_t a, md4_fx_t b)
{
    md4_i32 neg;
    md4_u32 ua;
    md4_u32 ub;
    md4_u32 int_part;
    md4_u32 rem;
    md4_u32 frac;
    md4_u32 bit;
    md4_u32 res;

    if (b == 0) {
        return (a < 0) ? (md4_fx_t)0x80000000U : (md4_fx_t)0x7fffffff;
    }
    if (a == 0) {
        return 0;
    }

    neg = ((a < 0) ^ (b < 0)) ? 1 : 0;
    ua = md4_abs_u32(a);
    ub = md4_abs_u32(b);

    int_part = ua / ub;
    rem = ua % ub;

    if (int_part > (0x7fffffffU >> MD4_CFG_FX_FRAC_BITS)) {
        return neg ? (md4_fx_t)0x80000000U : (md4_fx_t)0x7fffffff;
    }

    frac = 0U;
    bit = 1U << (MD4_CFG_FX_FRAC_BITS - 1);
    while (bit != 0U) {
        rem <<= 1;
        if (rem >= ub) {
            rem -= ub;
            frac |= bit;
        }
        bit >>= 1;
    }

    res = (int_part << MD4_CFG_FX_FRAC_BITS) | frac;
    return md4_fx_sat_from_u32(res, neg);
}

md4_fx_t md4_fx_lerp(md4_fx_t a, md4_fx_t b, md4_fx_t t)
{
    md4_fx_t diff;
    diff = b - a;
    return a + md4_fx_mul(diff, t);
}

md4_fx_t md4_fx_dot3(const md4_fx_t a[3], const md4_fx_t b[3])
{
    md4_fx_t r;
    r = md4_fx_mul(a[0], b[0]);
    r += md4_fx_mul(a[1], b[1]);
    r += md4_fx_mul(a[2], b[2]);
    return r;
}

md4_fx_t md4_fx_from_ieee754_bits(md4_u32 bits)
{
    md4_u32 sign;
    md4_u32 exp;
    md4_u32 mant;
    md4_u32 sig;
    md4_i32 shift;
    md4_u32 val;

    sign = bits >> 31;
    exp = (bits >> 23) & 0xffU;
    mant = bits & 0x7fffffU;

    if (exp == 255U) {
        return sign ? (md4_fx_t)0x80000000U : (md4_fx_t)0x7fffffff;
    }
    if (exp == 0U) {
        if (mant == 0U) {
            return 0;
        }
        return 0;
    }

    sig = 0x800000U | mant;
    shift = (md4_i32)exp - 134;

    if (shift >= 0) {
        if (shift > 7) {
            return sign ? (md4_fx_t)0x80000000U : (md4_fx_t)0x7fffffff;
        }
        if (sig > (0x7fffffffU >> shift)) {
            return sign ? (md4_fx_t)0x80000000U : (md4_fx_t)0x7fffffff;
        }
        val = sig << shift;
    } else {
        md4_u32 rshift;
        md4_u32 add;
        rshift = (md4_u32)(-shift);
        if (rshift >= 32U) {
            val = 0U;
        } else {
            add = (rshift == 0U) ? 0U : (1U << (rshift - 1));
            val = (sig + add) >> rshift;
        }
    }

    return md4_fx_sat_from_u32(val, (md4_i32)sign);
}

md4_u32 md4_fx_to_ieee754_bits(md4_fx_t v)
{
    md4_u32 sign;
    md4_u32 abs_v;
    md4_i32 p;
    md4_i32 exp;
    md4_u32 sig;
    md4_u32 mant;

    if (v == 0) {
        return 0U;
    }

    sign = (v < 0) ? 0x80000000U : 0U;
    abs_v = md4_abs_u32(v);
    p = md4_count_bits_u32(abs_v);
    if (p < 0) {
        return 0U;
    }

    exp = p - MD4_CFG_FX_FRAC_BITS + 127;
    if (exp <= 0) {
        return 0U;
    }
    if (exp >= 255) {
        return sign | 0x7f7fffffU;
    }

    if (p > 23) {
        md4_u32 shift;
        md4_u32 round_bit;
        shift = (md4_u32)(p - 23);
        sig = abs_v >> shift;
        if (shift > 0U) {
            round_bit = 1U << (shift - 1U);
            if ((abs_v & round_bit) != 0U) {
                sig += 1U;
            }
        }
    } else {
        sig = abs_v << (23 - p);
    }

    if ((sig & 0x01000000U) != 0U) {
        sig >>= 1;
        exp += 1;
        if (exp >= 255) {
            return sign | 0x7f7fffffU;
        }
    }

    mant = sig & 0x7fffffU;
    return sign | ((md4_u32)exp << 23) | mant;
}
