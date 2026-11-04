#include "md3_internal.h"

static md3_u32 md3_abs_u32_from_i32(md3_i32 value)
{
    if (value < 0)
    {
        return (md3_u32) (0U - (md3_u32) value);
    }
    return (md3_u32) value;
}

int md3_fixed_from_ieee754_u32(md3_u32 bits, md3_fixed_t *out_value)
{
    md3_u32 sign;
    md3_u32 exponent_bits;
    md3_u32 fraction;
    md3_u32 mantissa;
    int shift;
    md3_u32 magnitude;

    if (out_value == 0)
    {
        return MD3_EINVAL;
    }

    sign = (bits >> 31) & 1U;
    exponent_bits = (bits >> 23) & 0xFFU;
    fraction = bits & 0x7FFFFFU;

    if (exponent_bits == 0xFFU)
    {
        return MD3_ERANGE;
    }

    if (exponent_bits == 0U)
    {
        if (fraction == 0U)
        {
            *out_value = 0;
            return MD3_OK;
        }

        *out_value = 0;
        return MD3_OK;
    }

    mantissa = 0x800000UL | fraction;
    shift = (int) exponent_bits - 134;

    if (shift >= 0)
    {
        if (shift > 7)
        {
            return MD3_ERANGE;
        }
        magnitude = mantissa << shift;
    }
    else
    {
        if (shift < -31)
        {
            magnitude = 0U;
        }
        else
        {
            magnitude = mantissa >> (-shift);
        }
    }

    if (sign != 0U)
    {
        if (magnitude > 0x80000000UL)
        {
            return MD3_ERANGE;
        }
        *out_value = -(md3_i32) magnitude;
    }
    else
    {
        if (magnitude > 0x7FFFFFFFUL)
        {
            return MD3_ERANGE;
        }
        *out_value = (md3_i32) magnitude;
    }

    return MD3_OK;
}

md3_u32 md3_fixed_to_ieee754_u32(md3_fixed_t value)
{
    md3_u32 sign;
    md3_u32 magnitude;
    int msb;
    int exponent_unbiased;
    md3_u32 exponent_bits;
    md3_u32 mantissa24;
    md3_u32 mantissa_field;

    if (value == 0)
    {
        return 0U;
    }

    sign = (value < 0) ? 1U : 0U;
    magnitude = md3_abs_u32_from_i32(value);

    msb = 31;
    while (msb > 0 && ((magnitude & ((md3_u32) 1U << msb)) == 0U))
    {
        --msb;
    }

    exponent_unbiased = msb - 16;
    exponent_bits = (md3_u32) (exponent_unbiased + 127);

    if (msb > 23)
    {
        int shift;
        md3_u32 remainder;
        md3_u32 halfway;

        shift = msb - 23;
        mantissa24 = magnitude >> shift;
        remainder = magnitude & ((((md3_u32) 1U) << shift) - 1U);
        halfway = ((md3_u32) 1U) << (shift - 1);

        if (remainder > halfway ||
            (remainder == halfway && (mantissa24 & 1U) != 0U))
        {
            ++mantissa24;
            if (mantissa24 == 0x1000000UL)
            {
                mantissa24 >>= 1;
                ++exponent_bits;
            }
        }
    }
    else
    {
        mantissa24 = magnitude << (23 - msb);
    }

    if (exponent_bits >= 255U)
    {
        exponent_bits = 254U;
        mantissa24 = 0xFFFFFFUL;
    }

    mantissa_field = mantissa24 & 0x7FFFFFUL;

    return (sign << 31) | (exponent_bits << 23) | mantissa_field;
}
