#include "mpk_reader.h"

#define MPK_FIXED_ONE ((mpk_fixed)65536L)
#define MPK_FIXED_MAX ((mpk_fixed)0x7fffffffL)
#define MPK_FIXED_MIN ((mpk_fixed)(-2147483647L-1L))

static mpk_fixed mpk_sat_signed(mpk_u32 mag, int neg)
{
    if (neg) {
        if (mag > 0x80000000UL) return MPK_FIXED_MIN;
        if (mag == 0x80000000UL) return MPK_FIXED_MIN;
        return (mpk_fixed)(-(mpk_i32)mag);
    }
    if (mag > 0x7fffffffUL) return MPK_FIXED_MAX;
    return (mpk_fixed)mag;
}

mpk_fixed mpk_fixed_from_float_bits(mpk_u32 bits)
{
    mpk_u32 sign;
    mpk_u32 exp;
    mpk_u32 frac;
    mpk_u32 mant;
    int shift;
    mpk_u32 mag;

    sign = (bits >> 31) & 1UL;
    exp = (bits >> 23) & 255UL;
    frac = bits & 0x7fffffUL;

    if (exp == 0UL) {
        return 0;
    }
    if (exp == 255UL) {
        return sign ? MPK_FIXED_MIN : MPK_FIXED_MAX;
    }

    mant = frac | 0x800000UL;
    shift = (int)exp - 134;

    if (shift >= 0) {
        if (shift > 7) return sign ? MPK_FIXED_MIN : MPK_FIXED_MAX;
        mag = mant << shift;
    } else {
        int rshift;
        rshift = -shift;
        if (rshift >= 32) mag = 0UL;
        else mag = mant >> rshift;
    }

    return mpk_sat_signed(mag, (int)sign);
}

mpk_fixed mpk_fixed_from_half_bits(mpk_u32 hbits)
{
    mpk_u32 sign;
    mpk_u32 exp;
    mpk_u32 frac;
    mpk_u32 mant;
    int shift;
    mpk_u32 mag;

    sign = (hbits >> 15) & 1UL;
    exp = (hbits >> 10) & 31UL;
    frac = hbits & 1023UL;

    if (exp == 0UL) {
        return 0;
    }
    if (exp == 31UL) {
        return sign ? MPK_FIXED_MIN : MPK_FIXED_MAX;
    }

    mant = frac | 1024UL;
    shift = (int)exp - 9;
    if (shift >= 0) {
        if (shift > 20) return sign ? MPK_FIXED_MIN : MPK_FIXED_MAX;
        mag = mant << shift;
    } else {
        int rshift;
        rshift = -shift;
        if (rshift >= 32) mag = 0UL;
        else mag = mant >> rshift;
    }
    return mpk_sat_signed(mag, (int)sign);
}

void mpk_fixed_to_string(mpk_fixed v, char *buf, int buf_size)
{
    mpk_i32 whole;
    mpk_u32 frac;
    mpk_u32 f1000;
    int neg;

    if (buf_size <= 0) return;
    neg = 0;
    if (v < 0) {
        neg = 1;
        if (v == MPK_FIXED_MIN) {
            whole = 32768L;
            frac = 0UL;
            if (buf_size > 0) sprintf(buf, "-32768.000");
            return;
        }
        v = -v;
    }
    whole = v >> 16;
    frac = (mpk_u32)(v & 0xffffL);
    f1000 = (frac * 1000UL + 32768UL) >> 16;
    if (f1000 >= 1000UL) {
        whole += 1;
        f1000 -= 1000UL;
    }
    if (neg) sprintf(buf, "-%ld.%03lu", (long)whole, (unsigned long)f1000);
    else sprintf(buf, "%ld.%03lu", (long)whole, (unsigned long)f1000);
}
