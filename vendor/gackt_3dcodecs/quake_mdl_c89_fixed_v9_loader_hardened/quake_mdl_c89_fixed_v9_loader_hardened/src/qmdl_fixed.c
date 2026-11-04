
#include "qmdl_internal.h"

static qmdl_u32 qmdl_abs_i32(qmdl_i32 value)
{
    if (value == QMDL_I32_MIN)
    {
        return 0x80000000u;
    }
    if (value < 0)
    {
        return (qmdl_u32)(-value);
    }
    return (qmdl_u32)value;
}

static int qmdl_msb_index(qmdl_u32 value)
{
    int index;

    index = 0;
    while (value > 1u)
    {
        value >>= 1;
        index++;
    }
    return index;
}

void qmdl_storage_clear(qmdl_storage *storage)
{
    if (storage != 0)
    {
        memset(storage, 0, sizeof(*storage));
    }
}

void qmdl_requirements_clear(qmdl_requirements *requirements)
{
    if (requirements != 0)
    {
        memset(requirements, 0, sizeof(*requirements));
    }
}

void qmdl_model_clear(qmdl_model *model)
{
    if (model != 0)
    {
        memset(model, 0, sizeof(*model));
    }
}

void qmdl_blob_plan_clear(qmdl_blob_plan *plan)
{
    if (plan != 0)
    {
        memset(plan, 0, sizeof(*plan));
    }
}

void qmdl_inspect_info_clear(qmdl_inspect_info *inspect)
{
    if (inspect != 0)
    {
        memset(inspect, 0, sizeof(*inspect));
    }
}

void qmdl_quake_compat_info_clear(qmdl_quake_compat_info *compat)
{
    if (compat != 0)
    {
        memset(compat, 0, sizeof(*compat));
        compat->first_flagged_skin = QMDL_INVALID_INDEX;
        compat->first_flagged_frame = QMDL_INVALID_INDEX;
    }
}


void qmdl_indexed_image_storage_clear(qmdl_indexed_image_storage *storage)
{
    if (storage != 0)
    {
        memset(storage, 0, sizeof(*storage));
    }
}

void qmdl_indexed_image_clear(qmdl_indexed_image *image)
{
    if (image != 0)
    {
        memset(image, 0, sizeof(*image));
    }
}
const char *qmdl_strerror(int code)
{
    switch (code)
    {
        case QMDL_OK: return "ok";
        case QMDL_ERR_ARGS: return "invalid arguments";
        case QMDL_ERR_SIZE: return "buffer too small or truncated data";
        case QMDL_ERR_FORMAT: return "invalid mdl data";
        case QMDL_ERR_RANGE: return "value out of supported range";
        case QMDL_ERR_OVERFLOW: return "integer overflow";
        case QMDL_ERR_CAPACITY: return "caller supplied storage is too small";
        case QMDL_ERR_IO: return "i/o failure";
        default: return "unknown error";
    }
}

qmdl_fx16 qmdl_fx_from_int(int value)
{
    if (value > 32767)
    {
        return QMDL_I32_MAX;
    }
    if (value < -32768)
    {
        return QMDL_I32_MIN;
    }
    return (qmdl_fx16)(value << 16);
}

qmdl_fx16 qmdl_fx_from_ratio(int num, int den)
{
    qmdl_i32 q;
    qmdl_i32 r;
    int sign;
    qmdl_u32 anum;
    qmdl_u32 aden;
    qmdl_u32 result;

    if (den == 0)
    {
        return (num >= 0) ? QMDL_I32_MAX : QMDL_I32_MIN;
    }

    sign = 0;
    if (num < 0)
    {
        sign ^= 1;
        anum = qmdl_abs_i32((qmdl_i32)num);
    }
    else
    {
        anum = (qmdl_u32)num;
    }
    if (den < 0)
    {
        sign ^= 1;
        aden = qmdl_abs_i32((qmdl_i32)den);
    }
    else
    {
        aden = (qmdl_u32)den;
    }

    q = (qmdl_i32)(anum / aden);
    r = (qmdl_i32)(anum % aden);

    if (q > 32767)
    {
        return sign ? QMDL_I32_MIN : QMDL_I32_MAX;
    }

    result = ((qmdl_u32)q << 16) + (qmdl_u32)(((unsigned long)r << 16) / aden);
    if (sign)
    {
        if (result >= 0x80000000u)
        {
            return QMDL_I32_MIN;
        }
        return -(qmdl_i32)result;
    }
    if (result > 0x7FFFFFFFu)
    {
        return QMDL_I32_MAX;
    }
    return (qmdl_i32)result;
}

int qmdl_fx_floor_to_int(qmdl_fx16 value)
{
    if (value >= 0)
    {
        return (int)((qmdl_u32)value >> 16);
    }
    if ((value & 0xFFFF) == 0)
    {
        return (int)(value / QMDL_FX_ONE);
    }
    return (int)(value / QMDL_FX_ONE) - 1;
}

int qmdl_fx_round_to_int(qmdl_fx16 value)
{
    if (value >= 0)
    {
        return (int)((value + QMDL_FX_HALF) >> 16);
    }
    return -((int)(((-value) + QMDL_FX_HALF) >> 16));
}

qmdl_fx16 qmdl_fx_mul_int(qmdl_fx16 value, int factor)
{
    int sign;
    qmdl_u32 avalue;
    qmdl_u32 afactor;
    qmdl_u32 result;

    if (factor == 0 || value == 0)
    {
        return 0;
    }

    sign = 0;
    if (value < 0)
    {
        sign ^= 1;
        avalue = qmdl_abs_i32(value);
    }
    else
    {
        avalue = (qmdl_u32)value;
    }
    if (factor < 0)
    {
        sign ^= 1;
        afactor = qmdl_abs_i32((qmdl_i32)factor);
    }
    else
    {
        afactor = (qmdl_u32)factor;
    }

    if (avalue != 0u && afactor > 0xFFFFFFFFu / avalue)
    {
        return sign ? QMDL_I32_MIN : QMDL_I32_MAX;
    }

    result = avalue * afactor;
    if (sign)
    {
        if (result >= 0x80000000u)
        {
            return QMDL_I32_MIN;
        }
        return -(qmdl_i32)result;
    }
    if (result > 0x7FFFFFFFu)
    {
        return QMDL_I32_MAX;
    }
    return (qmdl_i32)result;
}

qmdl_fx16 qmdl_fx_div_int(qmdl_fx16 value, int divisor)
{
    if (divisor == 0)
    {
        return (value >= 0) ? QMDL_I32_MAX : QMDL_I32_MIN;
    }
    return value / divisor;
}

qmdl_fx16 qmdl_f32_bits_to_fx16(qmdl_u32 bits)
{
    qmdl_u32 sign;
    qmdl_u32 exponent;
    qmdl_u32 mantissa;
    qmdl_u32 significand;
    int shift;
    qmdl_u32 magnitude;
    qmdl_u32 remainder;
    qmdl_u32 half;

    sign = bits >> 31;
    exponent = (bits >> 23) & 0xFFu;
    mantissa = bits & 0x7FFFFFu;

    if (exponent == 0xFFu)
    {
        return sign ? QMDL_I32_MIN : QMDL_I32_MAX;
    }

    if (exponent == 0u)
    {
        return 0;
    }

    significand = 0x800000u | mantissa;
    shift = (int)exponent - 134;

    if (shift >= 0)
    {
        if (shift > 7)
        {
            return sign ? QMDL_I32_MIN : QMDL_I32_MAX;
        }
        if (sign)
        {
            if (significand > (0x80000000u >> shift))
            {
                return QMDL_I32_MIN;
            }
        }
        else
        {
            if (significand > (0x7FFFFFFFu >> shift))
            {
                return QMDL_I32_MAX;
            }
        }
        magnitude = significand << shift;
    }
    else
    {
        shift = -shift;
        if (shift >= 32)
        {
            magnitude = 0;
        }
        else
        {
            magnitude = significand >> shift;
            if (shift > 0)
            {
                remainder = significand & ((1u << shift) - 1u);
                half = 1u << (shift - 1);
                if (remainder > half || (remainder == half && (magnitude & 1u) != 0u))
                {
                    magnitude++;
                }
            }
        }
    }

    if (sign)
    {
        if (magnitude >= 0x80000000u)
        {
            return QMDL_I32_MIN;
        }
        return -(qmdl_i32)magnitude;
    }
    if (magnitude > 0x7FFFFFFFu)
    {
        return QMDL_I32_MAX;
    }
    return (qmdl_i32)magnitude;
}

qmdl_u32 qmdl_fx16_to_f32_bits(qmdl_fx16 value)
{
    qmdl_u32 sign;
    qmdl_u32 magnitude;
    int msb;
    qmdl_u32 exponent;
    qmdl_u32 mantissa;
    qmdl_u32 normalized;
    qmdl_u32 remainder;
    qmdl_u32 half;
    int shift;

    if (value == 0)
    {
        return 0u;
    }

    sign = (value < 0) ? 0x80000000u : 0u;
    magnitude = qmdl_abs_i32(value);
    msb = qmdl_msb_index(magnitude);
    exponent = (qmdl_u32)(msb - 16 + 127);

    if (msb <= 23)
    {
        normalized = magnitude << (23 - msb);
    }
    else
    {
        shift = msb - 23;
        normalized = magnitude >> shift;
        remainder = magnitude & ((1u << shift) - 1u);
        half = 1u << (shift - 1);
        if (remainder > half || (remainder == half && (normalized & 1u) != 0u))
        {
            normalized++;
        }
        if (normalized == 0x01000000u)
        {
            normalized >>= 1;
            exponent++;
        }
    }

    mantissa = normalized & 0x7FFFFFu;
    return sign | (exponent << 23) | mantissa;
}

qmdl_fx16 qmdl_decode_coord(qmdl_fx16 scale, qmdl_fx16 origin, qmdl_u8 packed)
{
    qmdl_fx16 scaled;

    scaled = qmdl_fx_mul_int(scale, (int)packed);
    if (scale >= 0 && scaled == QMDL_I32_MAX)
    {
        return QMDL_I32_MAX;
    }
    if (scale < 0 && scaled == QMDL_I32_MIN)
    {
        return QMDL_I32_MIN;
    }

    if (origin > 0 && scaled > QMDL_I32_MAX - origin)
    {
        return QMDL_I32_MAX;
    }
    if (origin < 0 && scaled < QMDL_I32_MIN - origin)
    {
        return QMDL_I32_MIN;
    }
    return scaled + origin;
}

qmdl_u8 qmdl_encode_coord(qmdl_fx16 scale, qmdl_fx16 origin, qmdl_fx16 position)
{
    qmdl_i32 delta;
    qmdl_i32 quotient;
    qmdl_i32 remainder;
    qmdl_i32 abs_scale;

    if (scale == 0)
    {
        return 0;
    }

    delta = position - origin;
    abs_scale = (scale < 0) ? -scale : scale;
    if (scale < 0)
    {
        delta = -delta;
    }

    quotient = delta / abs_scale;
    remainder = delta % abs_scale;

    if (remainder < 0)
    {
        remainder = -remainder;
    }

    if (remainder <= (QMDL_I32_MAX / 2) && (remainder * 2) >= abs_scale)
    {
        if (delta >= 0)
        {
            quotient++;
        }
        else
        {
            quotient--;
        }
    }

    if (quotient < 0)
    {
        return 0;
    }
    if (quotient > 255)
    {
        return 255;
    }
    return (qmdl_u8)quotient;
}

void qmdl_decode_vertex(const qmdl_header *header,
                        const qmdl_trivertx *packed,
                        qmdl_fx16 out_xyz[3])
{
    if (header == 0 || packed == 0 || out_xyz == 0)
    {
        return;
    }
    out_xyz[0] = qmdl_decode_coord(header->scale[0], header->origin[0], packed->v[0]);
    out_xyz[1] = qmdl_decode_coord(header->scale[1], header->origin[1], packed->v[1]);
    out_xyz[2] = qmdl_decode_coord(header->scale[2], header->origin[2], packed->v[2]);
}

void qmdl_encode_vertex(const qmdl_header *header,
                        const qmdl_fx16 in_xyz[3],
                        qmdl_u8 normal_index,
                        qmdl_trivertx *packed_out)
{
    if (header == 0 || in_xyz == 0 || packed_out == 0)
    {
        return;
    }
    packed_out->v[0] = qmdl_encode_coord(header->scale[0], header->origin[0], in_xyz[0]);
    packed_out->v[1] = qmdl_encode_coord(header->scale[1], header->origin[1], in_xyz[1]);
    packed_out->v[2] = qmdl_encode_coord(header->scale[2], header->origin[2], in_xyz[2]);
    packed_out->lightnormalindex = normal_index;
}

qmdl_i32 qmdl_resolve_seam_s(const qmdl_header *header,
                            const qmdl_stvert *stvert,
                            const qmdl_triangle *triangle)
{
    qmdl_i32 s;

    if (header == 0 || stvert == 0)
    {
        return 0;
    }

    s = stvert->s;
    if (triangle != 0 && stvert->onseam != 0 && triangle->facesfront == 0)
    {
        s += (qmdl_i32)(header->skinwidth / 2u);
    }
    return s;
}
