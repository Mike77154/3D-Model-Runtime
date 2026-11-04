#include "ter3d.h"

#define TER3D_S32_MAX ((ter3d_s32)2147483647L)
#define TER3D_S32_MIN ((ter3d_s32)(-2147483647L - 1L))

ter3d_fixed ter3d_fixed_from_int(ter3d_s32 v)
{
    if (v > (TER3D_S32_MAX >> TER3D_Q)) return TER3D_S32_MAX;
    if (v < (TER3D_S32_MIN >> TER3D_Q)) return TER3D_S32_MIN;
    return (ter3d_fixed)(v << TER3D_Q);
}

ter3d_s32 ter3d_fixed_to_int_floor(ter3d_fixed v)
{
    return (ter3d_s32)(v >> TER3D_Q);
}

ter3d_fixed ter3d_fixed_add_sat(ter3d_fixed a, ter3d_fixed b)
{
    if (b > 0 && a > TER3D_S32_MAX - b) return TER3D_S32_MAX;
    if (b < 0 && a < TER3D_S32_MIN - b) return TER3D_S32_MIN;
    return a + b;
}

static ter3d_fixed ter3d_neg_sat(ter3d_fixed v)
{
    if (v == TER3D_S32_MIN) return TER3D_S32_MAX;
    return -v;
}

static ter3d_fixed ter3d_abs_sat(ter3d_fixed v)
{
    if (v < 0) return ter3d_neg_sat(v);
    return v;
}

static ter3d_fixed ter3d_shift_left_sat(ter3d_fixed a, int sh)
{
    int i;
    ter3d_fixed v;
    v = a;
    if (sh <= 0) return v;
    for (i = 0; i < sh; ++i) {
        if (v > (TER3D_S32_MAX / 2L)) return TER3D_S32_MAX;
        v = (ter3d_fixed)(v << 1);
    }
    return v;
}

/* C89-safe fixed multiply without 64-bit math. Saturating and deterministic. */
ter3d_fixed ter3d_fixed_mul_sat(ter3d_fixed a, ter3d_fixed b)
{
    int neg;
    int i;
    ter3d_fixed aa;
    ter3d_fixed bb;
    ter3d_fixed part;
    ter3d_fixed res;

    neg = 0;
    if (a < 0) { neg = !neg; a = ter3d_abs_sat(a); }
    if (b < 0) { neg = !neg; b = ter3d_abs_sat(b); }
    aa = a;
    bb = b;
    res = 0;

    for (i = 0; i < 31; ++i) {
        if ((bb & ((ter3d_fixed)1L << i)) != 0) {
            if (i >= TER3D_Q) part = ter3d_shift_left_sat(aa, i - TER3D_Q);
            else part = (ter3d_fixed)(aa >> (TER3D_Q - i));
            res = ter3d_fixed_add_sat(res, part);
        }
    }
    if (neg) res = ter3d_neg_sat(res);
    return res;
}

ter3d_fixed ter3d_ieee754_le_to_q16(const ter3d_u8 *p)
{
    ter3d_u32 bits;
    ter3d_u32 mant;
    ter3d_u32 exp;
    int sign;
    int shift;
    ter3d_fixed out;

    if (p == 0) return 0;
    bits = ter3d_read_le32u(p);
    sign = ((bits >> 31) != 0UL);
    exp = (bits >> 23) & 255UL;
    mant = bits & 0x7fffffUL;

    if (exp == 255UL) {
        return sign ? TER3D_S32_MIN : TER3D_S32_MAX;
    }
    if (exp == 0UL) {
        if (mant == 0UL) return 0;
        /* subnormal: too tiny for terrain scale precision in Q16; keep monotonic tiny. */
        return sign ? -1L : 1L;
    }

    mant |= 0x800000UL;
    shift = (int)exp - 134; /* mant * 2^(exp-127-23+16) */

    if (shift >= 0) {
        if (shift > 7) out = TER3D_S32_MAX;
        else {
            ter3d_u32 tmp;
            tmp = mant << shift;
            if ((tmp >> shift) != mant || tmp > (ter3d_u32)TER3D_S32_MAX) out = TER3D_S32_MAX;
            else out = (ter3d_fixed)tmp;
        }
    } else {
        if (shift < -31) out = 0;
        else out = (ter3d_fixed)(mant >> (-shift));
    }

    if (sign) out = ter3d_neg_sat(out);
    return out;
}

ter3d_u32 ter3d_q16_to_ieee754_bits(ter3d_fixed q)
{
    ter3d_u32 sign;
    ter3d_u32 absq;
    ter3d_u32 msb;
    ter3d_u32 tmp;
    int pos;
    int exp;
    ter3d_u32 mant;

    sign = 0UL;
    if (q < 0) {
        sign = 0x80000000UL;
        if (q == TER3D_S32_MIN) absq = (ter3d_u32)TER3D_S32_MAX;
        else absq = (ter3d_u32)(-q);
    } else {
        absq = (ter3d_u32)q;
    }
    if (absq == 0UL) return sign;

    msb = absq;
    pos = 0;
    while (msb > 1UL) {
        msb >>= 1;
        ++pos;
    }

    exp = pos - TER3D_Q + 127;
    if (exp <= 0) return sign;
    if (exp >= 255) return sign | 0x7f800000UL;

    if (pos >= 23) tmp = absq >> (pos - 23);
    else tmp = absq << (23 - pos);
    mant = tmp & 0x7fffffUL;
    return sign | ((ter3d_u32)exp << 23) | mant;
}
