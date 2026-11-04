#include "dff3d_fixed.h"

static dff_fixed dff3d_saturate_signed(unsigned long mag, int neg)
{
    if (neg) {
        if (mag >= 0x80000000UL) {
            return DFF3D_FIXED_MIN;
        }
        return (dff_fixed)(-(dff_i32)mag);
    }
    if (mag > 0x7fffffffUL) {
        return DFF3D_FIXED_MAX;
    }
    return (dff_fixed)mag;
}

dff_fixed dff3d_fixed_from_int(dff_i32 x)
{
    if (x > 32767) {
        return DFF3D_FIXED_MAX;
    }
    if (x < -32768) {
        return DFF3D_FIXED_MIN;
    }
    return (dff_fixed)(x << DFF3D_FIXED_SHIFT);
}

dff_i32 dff3d_fixed_to_int_floor(dff_fixed x)
{
    return (dff_i32)(x >> DFF3D_FIXED_SHIFT);
}

dff_fixed dff3d_fixed_add_sat(dff_fixed a, dff_fixed b)
{
    if (b > 0 && a > (DFF3D_FIXED_MAX - b)) {
        return DFF3D_FIXED_MAX;
    }
    if (b < 0 && a < (DFF3D_FIXED_MIN - b)) {
        return DFF3D_FIXED_MIN;
    }
    return (dff_fixed)(a + b);
}

dff_fixed dff3d_fixed_mul(dff_fixed a, dff_fixed b)
{
    unsigned long ua;
    unsigned long ub;
    unsigned long ah;
    unsigned long al;
    unsigned long bh;
    unsigned long bl;
    unsigned long mag;
    int neg;

    neg = 0;
    if (a < 0) {
        neg = !neg;
        ua = (unsigned long)(-(a + 1));
        ua += 1UL;
    } else {
        ua = (unsigned long)a;
    }
    if (b < 0) {
        neg = !neg;
        ub = (unsigned long)(-(b + 1));
        ub += 1UL;
    } else {
        ub = (unsigned long)b;
    }

    /* C89-safe Q16.16 multiply without long long.
       It is exact for the high 16x16 term and keeps the useful cross terms.
       Good for matrices, normals, UV transforms and GTA-era coordinate ranges. */
    ah = ua >> 16;
    al = ua & 0xffffUL;
    bh = ub >> 16;
    bl = ub & 0xffffUL;

    if (ah != 0UL && bh != 0UL) {
        if (ah > 32767UL || bh > 32767UL || (ah * bh) > 32767UL) {
            return neg ? DFF3D_FIXED_MIN : DFF3D_FIXED_MAX;
        }
    }

    mag = (ah * bh) << 16;
    mag += ah * bl;
    mag += bh * al;
    mag += (al >> 8) * (bl >> 8);

    return dff3d_saturate_signed(mag, neg);
}

dff_fixed dff3d_fixed_from_f32_bits(dff_u32 bits)
{
    dff_u32 sign;
    dff_u32 exp;
    dff_u32 frac;
    unsigned long mant;
    unsigned long mag;
    int shift;
    int neg;

    sign = (bits >> 31) & 1U;
    exp = (bits >> 23) & 255U;
    frac = bits & 0x007fffffUL;
    neg = sign ? 1 : 0;

    if (exp == 0U) {
        return 0;
    }
    if (exp == 255U) {
        return neg ? DFF3D_FIXED_MIN : DFF3D_FIXED_MAX;
    }

    mant = (unsigned long)(frac | 0x00800000UL);
    shift = (int)exp - 134;

    if (shift >= 0) {
        if (shift > 7) {
            return neg ? DFF3D_FIXED_MIN : DFF3D_FIXED_MAX;
        }
        mag = mant << shift;
    } else {
        shift = -shift;
        if (shift >= 31) {
            mag = 0UL;
        } else {
            unsigned long rounder;
            rounder = (shift > 0) ? (1UL << (shift - 1)) : 0UL;
            mag = (mant + rounder) >> shift;
        }
    }

    return dff3d_saturate_signed(mag, neg);
}
