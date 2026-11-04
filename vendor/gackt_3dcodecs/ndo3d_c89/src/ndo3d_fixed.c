#include "ndo3d_internal.h"

#define NDO3D_I32_MAX_VAL 2147483647L
#define NDO3D_I32_MIN_VAL (-2147483647L - 1L)

static ndo3d_fixed ndo3d_sat_pos(ndo3d_u32 v)
{
    if (v > (ndo3d_u32)NDO3D_I32_MAX_VAL) return (ndo3d_fixed)NDO3D_I32_MAX_VAL;
    return (ndo3d_fixed)v;
}

static ndo3d_fixed ndo3d_apply_sign(ndo3d_u32 mag, int sign)
{
    if (sign) {
        if (mag >= (ndo3d_u32)2147483648UL) return (ndo3d_fixed)NDO3D_I32_MIN_VAL;
        return (ndo3d_fixed)(-(ndo3d_i32)mag);
    }
    return ndo3d_sat_pos(mag);
}

ndo3d_fixed ndo3d_fixed_from_int(long v)
{
    if (v > (NDO3D_I32_MAX_VAL / NDO3D_Q_ONE)) return (ndo3d_fixed)NDO3D_I32_MAX_VAL;
    if (v < (NDO3D_I32_MIN_VAL / NDO3D_Q_ONE)) return (ndo3d_fixed)NDO3D_I32_MIN_VAL;
    return (ndo3d_fixed)(v * NDO3D_Q_ONE);
}

long ndo3d_fixed_to_int(ndo3d_fixed v)
{
    return (long)(v / NDO3D_Q_ONE);
}

ndo3d_fixed ndo3d_fixed_div_int(ndo3d_fixed v, long d)
{
    if (d == 0L) return 0L;
    return (ndo3d_fixed)(v / d);
}

ndo3d_fixed ndo3d_fixed_mul_int_sat(ndo3d_fixed v, long m)
{
    int neg;
    ndo3d_u32 mag;
    ndo3d_u32 limit;
    long i;
    ndo3d_u32 acc;
    ndo3d_u32 add;
    if (v == 0L || m == 0L) return 0L;
    neg = 0;
    if (v < 0L) {
        neg = !neg;
        if (v == NDO3D_I32_MIN_VAL) mag = 2147483648UL;
        else mag = (ndo3d_u32)(-v);
    } else {
        mag = (ndo3d_u32)v;
    }
    if (m < 0L) {
        neg = !neg;
        m = -m;
    }
    limit = neg ? 2147483648UL : 2147483647UL;
    acc = 0UL;
    add = mag;
    for (i = 0L; i < m; i++) {
        if (acc > limit - add) return neg ? (ndo3d_fixed)NDO3D_I32_MIN_VAL : (ndo3d_fixed)NDO3D_I32_MAX_VAL;
        acc += add;
    }
    return ndo3d_apply_sign(acc, neg);
}

ndo3d_fixed ndo3d__float32bits_to_fixed(ndo3d_u32 bits, int *ok)
{
    int sign;
    int exp;
    int e;
    int shift;
    ndo3d_u32 mant;
    ndo3d_u32 mag;
    ndo3d_u32 limit;
    int i;
    if (ok != 0) *ok = 1;
    sign = (int)((bits >> 31) & 1UL);
    exp = (int)((bits >> 23) & 255UL);
    mant = bits & 0x7fffffUL;
    if (exp == 255) {
        if (ok != 0) *ok = 0;
        return 0L;
    }
    if (exp == 0) {
        if (mant == 0UL) return 0L;
        /* Tiny subnormals underflow to zero in Q16.16 for this importer. */
        return 0L;
    }
    mant |= 0x800000UL;
    e = exp - 127;
    shift = e - 7; /* mant * 2^(e-23+16). */
    if (shift >= 0) {
        mag = mant;
        limit = sign ? 2147483648UL : 2147483647UL;
        for (i = 0; i < shift; i++) {
            if (mag > (limit >> 1)) {
                return sign ? (ndo3d_fixed)NDO3D_I32_MIN_VAL : (ndo3d_fixed)NDO3D_I32_MAX_VAL;
            }
            mag <<= 1;
        }
    } else {
        int rshift;
        ndo3d_u32 add;
        rshift = -shift;
        if (rshift >= 32) return 0L;
        add = 0UL;
        if (rshift > 0) add = (ndo3d_u32)1UL << (rshift - 1);
        mag = (mant + add) >> rshift;
    }
    return ndo3d_apply_sign(mag, sign);
}

static int ndo3d_msb_u32(ndo3d_u32 v)
{
    int p;
    p = 31;
    while (p > 0 && ((v >> p) & 1UL) == 0UL) p--;
    return p;
}

ndo3d_u32 ndo3d__fixed_to_float32bits(ndo3d_fixed q)
{
    int sign;
    ndo3d_u32 u;
    int p;
    int exp;
    ndo3d_u32 full;
    ndo3d_u32 mant;
    int shift;
    ndo3d_u32 add;
    if (q == 0L) return 0UL;
    sign = 0;
    if (q < 0L) {
        sign = 1;
        if (q == NDO3D_I32_MIN_VAL) u = 2147483648UL;
        else u = (ndo3d_u32)(-q);
    } else {
        u = (ndo3d_u32)q;
    }
    p = ndo3d_msb_u32(u);
    exp = p - 16 + 127;
    if (exp <= 0) return sign ? 0x80000000UL : 0UL;
    if (exp >= 255) return sign ? 0xff800000UL : 0x7f800000UL;
    if (p >= 23) {
        shift = p - 23;
        add = 0UL;
        if (shift > 0) add = 1UL << (shift - 1);
        full = (u + add) >> shift;
    } else {
        full = u << (23 - p);
    }
    if (full >= 0x1000000UL) {
        full >>= 1;
        exp++;
        if (exp >= 255) return sign ? 0xff800000UL : 0x7f800000UL;
    }
    mant = full & 0x7fffffUL;
    return ((ndo3d_u32)sign << 31) | ((ndo3d_u32)exp << 23) | mant;
}

int ndo3d_fixed_to_ascii(ndo3d_fixed v, char *out, unsigned int out_cap, unsigned int frac_digits)
{
    char tmp[32];
    unsigned int ti;
    unsigned int oi;
    int neg;
    ndo3d_u32 mag;
    ndo3d_u32 whole;
    ndo3d_u32 frac;
    unsigned int i;
    ndo3d_u32 digit;
    if (out == 0 || out_cap == 0U) return NDO3D_ERR_NULL;
    if (frac_digits > 6U) frac_digits = 6U;
    neg = 0;
    if (v < 0L) {
        neg = 1;
        if (v == NDO3D_I32_MIN_VAL) mag = 2147483648UL;
        else mag = (ndo3d_u32)(-v);
    } else {
        mag = (ndo3d_u32)v;
    }
    whole = mag >> 16;
    frac = mag & 65535UL;
    oi = 0U;
    if (neg) {
        if (oi + 1U >= out_cap) return NDO3D_ERR_BOUNDS;
        out[oi++] = '-';
    }
    ti = 0U;
    if (whole == 0UL) {
        tmp[ti++] = '0';
    } else {
        while (whole != 0UL && ti < (unsigned int)sizeof(tmp)) {
            tmp[ti++] = (char)('0' + (char)(whole % 10UL));
            whole /= 10UL;
        }
    }
    while (ti > 0U) {
        if (oi + 1U >= out_cap) return NDO3D_ERR_BOUNDS;
        out[oi++] = tmp[--ti];
    }
    if (frac_digits != 0U) {
        if (oi + 1U >= out_cap) return NDO3D_ERR_BOUNDS;
        out[oi++] = '.';
        for (i = 0U; i < frac_digits; i++) {
            frac *= 10UL;
            digit = frac >> 16;
            frac &= 65535UL;
            if (oi + 1U >= out_cap) return NDO3D_ERR_BOUNDS;
            out[oi++] = (char)('0' + (char)digit);
        }
    }
    if (oi >= out_cap) return NDO3D_ERR_BOUNDS;
    out[oi] = '\0';
    return NDO3D_OK;
}
