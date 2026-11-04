#include "../include/dbo_fixed.h"

static dbo_u32 dbo__msb_index(dbo_u32 v)
{
    dbo_u32 i;
    for (i = 31UL; i > 0UL; --i)
    {
        if ((v >> i) & 1UL)
        {
            return i;
        }
    }
    return 0UL;
}

dbo_fx32 dbo_fx_from_int(int v)
{
    return (dbo_fx32)(((dbo_s32)v) << DBO_FX_SHIFT);
}

int dbo_fx_to_int(dbo_fx32 v)
{
    if (v >= 0)
    {
        return (int)(v >> DBO_FX_SHIFT);
    }
    return (int)(-(((-v) >> DBO_FX_SHIFT)));
}

dbo_fx32 dbo_fx_from_float_bits(dbo_u32 bits)
{
    dbo_u32 sign;
    dbo_u32 exp;
    dbo_u32 mant;
    dbo_u32 sig;
    dbo_s32 shift;
    dbo_u32 value;

    if ((bits & 0x7FFFFFFFUL) == 0UL)
    {
        return 0;
    }

    sign = bits >> 31;
    exp = (bits >> 23) & 0xFFUL;
    mant = bits & 0x7FFFFFUL;

    if (exp == 255UL)
    {
        if (sign != 0UL)
        {
            return (dbo_fx32)0x80000000UL;
        }
        return (dbo_fx32)0x7FFFFFFFUL;
    }

    if (exp == 0UL)
    {
        return 0;
    }

    sig = 0x800000UL | mant;
    shift = (dbo_s32)exp - 134L;

    if (shift >= 8)
    {
        value = 0x7FFFFFFFUL;
    }
    else if (shift >= 0)
    {
        if (sig > (0x7FFFFFFFUL >> shift))
        {
            value = 0x7FFFFFFFUL;
        }
        else
        {
            value = sig << shift;
        }
    }
    else if (shift <= -31)
    {
        value = 0UL;
    }
    else
    {
        value = sig >> (dbo_u32)(-shift);
    }

    if (sign != 0UL)
    {
        if (value >= 0x80000000UL)
        {
            return (dbo_fx32)0x80000000UL;
        }
        return -(dbo_fx32)value;
    }

    if (value > 0x7FFFFFFFUL)
    {
        value = 0x7FFFFFFFUL;
    }
    return (dbo_fx32)value;
}

dbo_u32 dbo_fx_to_float_bits(dbo_fx32 value)
{
    dbo_u32 sign;
    dbo_u32 abs_v;
    dbo_u32 msb;
    dbo_s32 exponent;
    dbo_u32 mantissa_full;
    dbo_u32 mantissa;

    if (value == 0)
    {
        return 0UL;
    }

    sign = 0UL;
    if (value < 0)
    {
        sign = 0x80000000UL;
        abs_v = (dbo_u32)(-value);
    }
    else
    {
        abs_v = (dbo_u32)value;
    }

    msb = dbo__msb_index(abs_v);
    exponent = (dbo_s32)msb - 16L + 127L;

    if (exponent <= 0L)
    {
        return sign;
    }
    if (exponent >= 255L)
    {
        return sign | 0x7F7FFFFFUL;
    }

    if (msb > 23UL)
    {
        mantissa_full = abs_v >> (msb - 23UL);
    }
    else
    {
        mantissa_full = abs_v << (23UL - msb);
    }

    mantissa = mantissa_full & 0x7FFFFFUL;
    return sign | (((dbo_u32)exponent & 0xFFUL) << 23) | mantissa;
}

void dbo_fx_identity4(dbo_fx32 out16[16])
{
    dbo_u32 i;
    for (i = 0UL; i < 16UL; ++i)
    {
        out16[i] = 0;
    }
    out16[0] = DBO_FX_ONE;
    out16[5] = DBO_FX_ONE;
    out16[10] = DBO_FX_ONE;
    out16[15] = DBO_FX_ONE;
}
