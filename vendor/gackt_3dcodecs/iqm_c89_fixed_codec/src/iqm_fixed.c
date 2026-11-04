#include "iqm_fixed.h"

static iqm_fixed iqm_fixed_sat_i64(iqm_i64 v)
{
    if(v > (iqm_i64)IQM_FIXED_MAX) return IQM_FIXED_MAX;
    if(v < (iqm_i64)IQM_FIXED_MIN) return IQM_FIXED_MIN;
    return (iqm_fixed)v;
}

static iqm_fixed iqm_fixed_from_unsigned_shifted(iqm_u64 mant, int shift, int neg)
{
    iqm_u64 u;
    iqm_i64 s;

    if(mant == (iqm_u64)0) return (iqm_fixed)0;

    if(shift >= 0)
    {
        if(shift >= 63) u = (iqm_u64)0x7fffffffffffffffULL;
        else u = mant << shift;
    }
    else
    {
        int rshift;
        iqm_u64 add;
        rshift = -shift;
        if(rshift >= 64) u = (iqm_u64)0;
        else
        {
            if(rshift > 0 && rshift < 64)
            {
                add = ((iqm_u64)1 << (rshift - 1));
                u = (mant + add) >> rshift;
            }
            else u = mant;
        }
    }

    if(u > (iqm_u64)0x7fffffffUL)
    {
        return neg ? IQM_FIXED_MIN : IQM_FIXED_MAX;
    }

    s = (iqm_i64)u;
    if(neg) s = -s;
    return (iqm_fixed)s;
}

iqm_fixed iqm_fixed_from_i32(iqm_i32 v)
{
    iqm_i64 r;
    r = ((iqm_i64)v) << IQM_FP_SHIFT;
    return iqm_fixed_sat_i64(r);
}

iqm_fixed iqm_fixed_from_raw_i64(iqm_i64 v)
{
    return iqm_fixed_sat_i64(v);
}

iqm_fixed iqm_fixed_mul(iqm_fixed a, iqm_fixed b)
{
    iqm_i64 r;
    r = ((iqm_i64)a * (iqm_i64)b) >> IQM_FP_SHIFT;
    return iqm_fixed_sat_i64(r);
}

iqm_fixed iqm_fixed_mul_u16(iqm_fixed a, iqm_u16 b)
{
    iqm_i64 r;
    r = (iqm_i64)a * (iqm_i64)b;
    return iqm_fixed_sat_i64(r);
}

iqm_fixed iqm_fixed_from_f32_bits(iqm_u32 bits)
{
    int neg;
    int exp;
    iqm_u32 frac;
    iqm_u64 mant;
    int shift;

    neg = (bits >> 31) ? 1 : 0;
    exp = (int)((bits >> 23) & 255u);
    frac = bits & 0x7fffffu;

    if(exp == 255)
    {
        if(frac != 0u) return (iqm_fixed)0;
        return neg ? IQM_FIXED_MIN : IQM_FIXED_MAX;
    }

    if(exp == 0)
    {
        if(frac == 0u) return (iqm_fixed)0;
        mant = (iqm_u64)frac;
        shift = -126 - 23 + IQM_FP_SHIFT;
        return iqm_fixed_from_unsigned_shifted(mant, shift, neg);
    }

    mant = ((iqm_u64)1 << 23) | (iqm_u64)frac;
    shift = exp - 127 - 23 + IQM_FP_SHIFT;
    return iqm_fixed_from_unsigned_shifted(mant, shift, neg);
}

iqm_fixed iqm_fixed_from_f16_bits(iqm_u16 bits)
{
    int neg;
    int exp;
    iqm_u16 frac;
    iqm_u64 mant;
    int shift;

    neg = (bits & 0x8000u) ? 1 : 0;
    exp = (int)((bits >> 10) & 31u);
    frac = (iqm_u16)(bits & 0x03ffu);

    if(exp == 31)
    {
        if(frac != 0u) return (iqm_fixed)0;
        return neg ? IQM_FIXED_MIN : IQM_FIXED_MAX;
    }

    if(exp == 0)
    {
        if(frac == 0u) return (iqm_fixed)0;
        mant = (iqm_u64)frac;
        shift = -14 - 10 + IQM_FP_SHIFT;
        return iqm_fixed_from_unsigned_shifted(mant, shift, neg);
    }

    mant = ((iqm_u64)1 << 10) | (iqm_u64)frac;
    shift = exp - 15 - 10 + IQM_FP_SHIFT;
    return iqm_fixed_from_unsigned_shifted(mant, shift, neg);
}

static iqm_u32 iqm_u32_to_ascii(iqm_u32 v, char *dst, iqm_u32 dst_size)
{
    char tmp[16];
    iqm_u32 n;
    iqm_u32 i;
    iqm_u32 out;

    if(dst_size == 0u) return 0u;
    if(v == 0u)
    {
        if(dst_size > 1u)
        {
            dst[0] = '0';
            dst[1] = 0;
            return 1u;
        }
        dst[0] = 0;
        return 0u;
    }

    n = 0u;
    while(v != 0u && n < (iqm_u32)sizeof(tmp))
    {
        tmp[n] = (char)('0' + (v % 10u));
        v /= 10u;
        n++;
    }

    out = 0u;
    i = 0u;
    while(i < n && out + 1u < dst_size)
    {
        dst[out] = tmp[n - 1u - i];
        out++;
        i++;
    }
    dst[out] = 0;
    return out;
}

int iqm_fixed_to_ascii(iqm_fixed v, char *dst, iqm_u32 dst_size, iqm_u32 frac_digits)
{
    iqm_u32 pos;
    iqm_u32 whole;
    iqm_u32 frac;
    iqm_u32 scale;
    iqm_u32 i;
    iqm_i32 signed_v;

    if(dst == 0 || dst_size == 0u) return 0;
    dst[0] = 0;
    if(dst_size < 2u) return 0;

    signed_v = v;
    pos = 0u;
    if(signed_v < 0)
    {
        dst[pos++] = '-';
        if(signed_v == IQM_FIXED_MIN) signed_v = IQM_FIXED_MAX;
        else signed_v = -signed_v;
    }

    whole = ((iqm_u32)signed_v) >> IQM_FP_SHIFT;
    pos += iqm_u32_to_ascii(whole, dst + pos, dst_size - pos);
    if(pos + 1u >= dst_size) return 1;

    if(frac_digits > 0u)
    {
        dst[pos++] = '.';
        frac = ((iqm_u32)signed_v) & ((1u << IQM_FP_SHIFT) - 1u);
        scale = 1u;
        i = 0u;
        while(i < frac_digits && scale <= 100000000u)
        {
            scale *= 10u;
            i++;
        }
        frac = (iqm_u32)(((iqm_u64)frac * (iqm_u64)scale) >> IQM_FP_SHIFT);
        i = frac_digits;
        while(i > 0u && pos + i < dst_size)
        {
            dst[pos + i - 1u] = (char)('0' + (frac % 10u));
            frac /= 10u;
            i--;
        }
        pos += frac_digits;
        if(pos >= dst_size) pos = dst_size - 1u;
        dst[pos] = 0;
    }
    return 1;
}
