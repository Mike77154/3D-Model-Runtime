#include "md2/md2_error.h"
#include "md2/md2_fixed.h"

#define MD2_I32_MAX_VALUE ((md2_i32)2147483647)
#define MD2_I32_MIN_VALUE ((md2_i32)(-2147483647 - 1))

static md2_u32 md2_fx_mag_i32(md2_i32 value)
{
    if (value < 0)
    {
        return (md2_u32)((md2_u32)(-(value + 1)) + 1U);
    }
    return (md2_u32)value;
}

static md2_fx32_t md2_fx_from_mag_sign(md2_u32 mag, md2_i32 sign)
{
    if (sign < 0)
    {
        if (mag >= 0x80000000U)
        {
            return MD2_I32_MIN_VALUE;
        }
        return (md2_fx32_t)(-(md2_i32)mag);
    }

    if (mag > 0x7FFFFFFFU)
    {
        return MD2_I32_MAX_VALUE;
    }
    return (md2_fx32_t)mag;
}

md2_fx32_t md2_fx_abs(md2_fx32_t v)
{
    if (v == MD2_I32_MIN_VALUE)
    {
        return MD2_I32_MAX_VALUE;
    }
    return (v < 0) ? (md2_fx32_t)(-v) : v;
}

md2_fx32_t md2_fx_div_i32(md2_fx32_t numer, md2_i32 denom)
{
    md2_i32 sign;
    md2_u32 anumer;
    md2_u32 adenom;
    md2_u32 q;
    md2_u32 r;

    if (denom == 0)
    {
        return 0;
    }

    sign = 1;
    if (numer < 0)
    {
        sign = -sign;
    }
    if (denom < 0)
    {
        sign = -sign;
    }

    anumer = md2_fx_mag_i32(numer);
    adenom = md2_fx_mag_i32(denom);
    if (adenom == 0U)
    {
        return 0;
    }

    q = anumer / adenom;
    r = anumer % adenom;

    if (r != 0U)
    {
        if (r >= (md2_u32)(adenom - r))
        {
            ++q;
        }
    }

    return md2_fx_from_mag_sign(q, sign);
}

int md2_fx_texel_center_div_i32(md2_i16 texel, md2_i32 size, md2_fx32_t *out_value)
{
    md2_i32 ti;
    md2_i32 mag;
    md2_fx32_t numer;

    if (out_value == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (size <= 0)
    {
        return MD2_ERR_BAD_LAYOUT;
    }

    ti = (md2_i32)texel;
    if (ti == (md2_i32)(-32767 - 1))
    {
        numer = (md2_fx32_t)(MD2_I32_MIN_VALUE + (md2_i32)MD2_FX_HALF);
    }
    else if (ti < 0)
    {
        mag = (md2_i32)(-ti);
        numer = (md2_fx32_t)(-(mag * (md2_i32)MD2_FX_ONE));
        numer = (md2_fx32_t)(numer + (md2_i32)MD2_FX_HALF);
    }
    else
    {
        numer = (md2_fx32_t)(ti * (md2_i32)MD2_FX_ONE);
        if (numer > MD2_I32_MAX_VALUE - (md2_i32)MD2_FX_HALF)
        {
            return MD2_ERR_OVERFLOW;
        }
        numer = (md2_fx32_t)(numer + (md2_i32)MD2_FX_HALF);
    }

    *out_value = md2_fx_div_i32(numer, size);
    return MD2_OK;
}

md2_fx32_t md2_fx_ceil_div_i32(md2_fx32_t numer, md2_i32 denom)
{
    md2_i32 q;

    if (denom <= 0)
    {
        return 0;
    }

    if (numer <= 0)
    {
        return 0;
    }

    q = numer / denom;
    if ((numer % denom) != 0)
    {
        ++q;
    }
    return (md2_fx32_t)q;
}

md2_fx32_t md2_fx_clamp_unit(md2_fx32_t t)
{
    if (t <= 0)
    {
        return 0;
    }
    if (t >= MD2_FX_ONE)
    {
        return MD2_FX_ONE;
    }
    return t;
}

md2_fx32_t md2_fx_mul_unit(md2_fx32_t value, md2_fx32_t unit_q16)
{
    md2_u32 mag;
    md2_u32 t;
    md2_u32 whole;
    md2_u32 frac;
    md2_u32 part_whole;
    md2_u32 part_frac;
    md2_u32 result;
    int negative;

    t = (md2_u32)md2_fx_clamp_unit(unit_q16);
    if (t == 0U)
    {
        return 0;
    }
    if (t == (md2_u32)MD2_FX_ONE)
    {
        return value;
    }

    negative = 0;
    if (value < 0)
    {
        negative = 1;
    }
    mag = md2_fx_mag_i32(value);

    whole = mag >> 16;
    frac = mag & 0xFFFFU;

    part_whole = whole * t;
    part_frac = ((frac * t) + 0x8000U) >> 16;
    result = part_whole + part_frac;

    if (negative)
    {
        return md2_fx_from_mag_sign(result, -1);
    }
    return md2_fx_from_mag_sign(result, 1);
}

md2_fx32_t md2_fx_lerp(md2_fx32_t a, md2_fx32_t b, md2_fx32_t t)
{
    md2_fx32_t clamped;
    md2_fx32_t omt;
    md2_fx32_t part_a;
    md2_fx32_t part_b;

    clamped = md2_fx_clamp_unit(t);
    if (clamped == 0)
    {
        return a;
    }
    if (clamped == MD2_FX_ONE)
    {
        return b;
    }

    omt = (md2_fx32_t)(MD2_FX_ONE - clamped);
    part_a = md2_fx_mul_unit(a, omt);
    part_b = md2_fx_mul_unit(b, clamped);
    return (md2_fx32_t)(part_a + part_b);
}

md2_i32 md2_fx_quantize_byte(md2_fx32_t value, md2_fx32_t translate, md2_fx32_t scale)
{
    md2_fx32_t rel;
    md2_i32 q;

    if (scale <= 0)
    {
        return 0;
    }

    rel = (md2_fx32_t)(value - translate);
    if (rel <= 0)
    {
        return 0;
    }

    q = (md2_i32)((rel + (scale >> 1)) / scale);

    if (q < 0)
    {
        q = 0;
    }
    if (q > 255)
    {
        q = 255;
    }
    return q;
}

md2_fx32_t md2_fx_dequantize_byte(md2_u8 q, md2_fx32_t translate, md2_fx32_t scale)
{
    md2_i32 qi;
    qi = (md2_i32)q;
    return (md2_fx32_t)(translate + qi * scale);
}

int md2_fx_from_f32_bits(md2_u32 bits, md2_fx32_t *out_value)
{
    md2_u32 sign;
    md2_u32 exp;
    md2_u32 frac;
    md2_u32 mant;
    md2_i32 shift;
    md2_i32 value;

    if (out_value == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    sign = bits >> 31;
    exp = (bits >> 23) & 0xFFU;
    frac = bits & 0x7FFFFFU;

    if (exp == 255U)
    {
        return MD2_ERR_BAD_FLOAT;
    }

    if (exp == 0U)
    {
        if (frac == 0U)
        {
            *out_value = 0;
            return MD2_OK;
        }

        /*
         * Denormals are much smaller than 1/65536 for practical MD2 usage.
         * Returning zero keeps the code C89-only and deterministic.
         */
        *out_value = 0;
        return MD2_OK;
    }

    mant = (1U << 23) | frac;
    shift = (md2_i32)exp - 134; /* (exp - 127) - 7 */

    if (shift >= 0)
    {
        if (shift > 7)
        {
            return MD2_ERR_OVERFLOW;
        }
        if (mant > (0x7FFFFFFFU >> shift))
        {
            return MD2_ERR_OVERFLOW;
        }
        value = (md2_i32)(mant << shift);
    }
    else
    {
        md2_i32 rshift;
        md2_u32 round;
        md2_u32 rounded;

        rshift = (md2_i32)(-shift);
        if (rshift >= 32)
        {
            value = 0;
        }
        else
        {
            round = (rshift > 0) ? (1U << (rshift - 1)) : 0U;
            rounded = mant + round;
            value = (md2_i32)(rounded >> rshift);
        }
    }

    if (sign != 0U)
    {
        value = (md2_i32)(-value);
    }

    *out_value = value;
    return MD2_OK;
}

int md2_fx_to_f32_bits(md2_fx32_t value, md2_u32 *out_bits)
{
    md2_u32 sign;
    md2_u32 mag;
    md2_i32 p;
    md2_u32 sig;
    md2_u32 frac;
    md2_u32 exp_bits;

    if (out_bits == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    if (value == 0)
    {
        *out_bits = 0U;
        return MD2_OK;
    }

    sign = (value < 0) ? 1U : 0U;
    mag = (md2_u32)((value < 0) ? (md2_u32)(-value) : (md2_u32)value);

    p = 31;
    while (p > 0 && ((mag & (1U << p)) == 0U))
    {
        --p;
    }

    if (p > 23)
    {
        md2_i32 rshift;
        md2_u32 round;
        rshift = p - 23;
        round = 1U << (rshift - 1);
        sig = (mag + round) >> rshift;
    }
    else
    {
        sig = mag << (23 - p);
    }

    if (sig >= (1U << 24))
    {
        sig >>= 1;
        ++p;
    }

    exp_bits = (md2_u32)(p - 16 + 127);
    if (exp_bits >= 255U)
    {
        return MD2_ERR_OVERFLOW;
    }

    frac = sig & 0x7FFFFFU;
    *out_bits = (sign << 31) | (exp_bits << 23) | frac;
    return MD2_OK;
}
