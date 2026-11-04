#include "md3_internal.h"

static const md3_fixed_t g_md3_sin_table_q16[256] =
{
    0, 1608, 3216, 4821, 6424, 8022, 9616, 11204,
    12785, 14359, 15924, 17479, 19024, 20557, 22078, 23586,
    25080, 26558, 28020, 29466, 30893, 32303, 33692, 35062,
    36410, 37736, 39040, 40320, 41576, 42806, 44011, 45190,
    46341, 47464, 48559, 49624, 50660, 51665, 52639, 53581,
    54491, 55368, 56212, 57022, 57798, 58538, 59244, 59914,
    60547, 61145, 61705, 62228, 62714, 63162, 63572, 63944,
    64277, 64571, 64827, 65043, 65220, 65358, 65457, 65516,
    65536, 65516, 65457, 65358, 65220, 65043, 64827, 64571,
    64277, 63944, 63572, 63162, 62714, 62228, 61705, 61145,
    60547, 59914, 59244, 58538, 57798, 57022, 56212, 55368,
    54491, 53581, 52639, 51665, 50660, 49624, 48559, 47464,
    46341, 45190, 44011, 42806, 41576, 40320, 39040, 37736,
    36410, 35062, 33692, 32303, 30893, 29466, 28020, 26558,
    25080, 23586, 22078, 20557, 19024, 17479, 15924, 14359,
    12785, 11204, 9616, 8022, 6424, 4821, 3216, 1608,
    0, -1608, -3216, -4821, -6424, -8022, -9616, -11204,
    -12785, -14359, -15924, -17479, -19024, -20557, -22078, -23586,
    -25080, -26558, -28020, -29466, -30893, -32303, -33692, -35062,
    -36410, -37736, -39040, -40320, -41576, -42806, -44011, -45190,
    -46341, -47464, -48559, -49624, -50660, -51665, -52639, -53581,
    -54491, -55368, -56212, -57022, -57798, -58538, -59244, -59914,
    -60547, -61145, -61705, -62228, -62714, -63162, -63572, -63944,
    -64277, -64571, -64827, -65043, -65220, -65358, -65457, -65516,
    -65536, -65516, -65457, -65358, -65220, -65043, -64827, -64571,
    -64277, -63944, -63572, -63162, -62714, -62228, -61705, -61145,
    -60547, -59914, -59244, -58538, -57798, -57022, -56212, -55368,
    -54491, -53581, -52639, -51665, -50660, -49624, -48559, -47464,
    -46341, -45190, -44011, -42806, -41576, -40320, -39040, -37736,
    -36410, -35062, -33692, -32303, -30893, -29466, -28020, -26558,
    -25080, -23586, -22078, -20557, -19024, -17479, -15924, -14359,
    -12785, -11204, -9616, -8022, -6424, -4821, -3216, -1608
};

const md3_fixed_t *md3_sin_table_q16(void)
{
    return g_md3_sin_table_q16;
}

md3_fixed_t md3_sin_step_q16(md3_u8 angle_index)
{
    return g_md3_sin_table_q16[(unsigned int) angle_index];
}

md3_fixed_t md3_cos_step_q16(md3_u8 angle_index)
{
    return g_md3_sin_table_q16[((unsigned int) angle_index + 64U) & 255U];
}

md3_fixed_t md3_fixed_from_int(int value)
{
    return (md3_fixed_t) (value * MD3_FIXED_ONE);
}

int md3_fixed_to_int_floor(md3_fixed_t value)
{
    return (int) (value >> 16);
}

int md3_fixed_to_int_nearest(md3_fixed_t value)
{
    if (value >= 0)
    {
        return (int) ((value + MD3_FIXED_HALF) >> 16);
    }
    return (int) (-(((-value) + MD3_FIXED_HALF) >> 16));
}

md3_fixed_t md3_fixed_abs(md3_fixed_t value)
{
    return (value < 0) ? -value : value;
}

md3_fixed_t md3_fixed_mul(md3_fixed_t a, md3_fixed_t b)
{
    md3_u32 ua;
    md3_u32 ub;
    md3_u32 ahi;
    md3_u32 bhi;
    md3_u32 alo;
    md3_u32 blo;
    md3_u32 result;
    int negative;

    negative = 0;
    ua = (a < 0) ? (md3_u32) (0U - (md3_u32) a) : (md3_u32) a;
    ub = (b < 0) ? (md3_u32) (0U - (md3_u32) b) : (md3_u32) b;
    if ((a < 0) != (b < 0))
    {
        negative = 1;
    }

    ahi = ua >> 16;
    bhi = ub >> 16;
    alo = ua & 0xFFFFU;
    blo = ub & 0xFFFFU;

    result = ((ahi * bhi) << 16) +
        (ahi * blo) +
        (bhi * alo) +
        (((alo * blo) + 0x8000U) >> 16);

    if (negative != 0)
    {
        return -(md3_fixed_t) result;
    }
    return (md3_fixed_t) result;
}

md3_fixed_t md3_fixed_div(md3_fixed_t a, md3_fixed_t b)
{
    md3_u32 ua;
    md3_u32 ub;
    md3_u32 integer_part;
    md3_u32 remainder;
    md3_u32 fraction;
    md3_u32 result;
    int negative;
    int i;

    if (b == 0)
    {
        return (a >= 0) ? 0x7FFFFFFF : (md3_fixed_t) 0x80000000U;
    }

    negative = 0;
    ua = (a < 0) ? (md3_u32) (0U - (md3_u32) a) : (md3_u32) a;
    ub = (b < 0) ? (md3_u32) (0U - (md3_u32) b) : (md3_u32) b;
    if ((a < 0) != (b < 0))
    {
        negative = 1;
    }

    integer_part = ua / ub;
    remainder = ua % ub;
    if (integer_part > 0x7FFFU)
    {
        return negative ? (md3_fixed_t) 0x80000000U : 0x7FFFFFFF;
    }

    fraction = 0U;
    for (i = 0; i < 16; ++i)
    {
        fraction <<= 1;
        if (remainder >= ub - remainder)
        {
            remainder = remainder - (ub - remainder);
            fraction |= 1U;
        }
        else
        {
            remainder = remainder + remainder;
        }
    }

    result = (integer_part << 16) | fraction;
    if (negative != 0)
    {
        return -(md3_fixed_t) result;
    }
    return (md3_fixed_t) result;
}

md3_u32 md3_isqrt_u32(md3_u32 value)
{
    md3_u32 op;
    md3_u32 res;
    md3_u32 one;

    op = value;
    res = 0U;
    one = 1UL << 30;

    while (one > op)
    {
        one >>= 2;
    }

    while (one != 0U)
    {
        if (op >= res + one)
        {
            op -= res + one;
            res = (res >> 1) + one;
        }
        else
        {
            res >>= 1;
        }
        one >>= 2;
    }

    return res;
}

md3_fixed_t md3_fixed_sqrt(md3_fixed_t value)
{
    md3_u32 scaled;
    md3_u32 root;

    if (value <= 0)
    {
        return 0;
    }

    scaled = (md3_u32) value >> 8;
    root = md3_isqrt_u32(scaled);
    return (md3_fixed_t) (root << 12);
}

md3_fixed_t md3_fixed_lerp(md3_fixed_t a, md3_fixed_t b, md3_fixed_t t)
{
    return a + md3_fixed_mul((md3_fixed_t) (b - a), t);
}

void md3_vec3_clear(md3_vec3_t *out_vec)
{
    if (out_vec == 0)
    {
        return;
    }

    out_vec->x = 0;
    out_vec->y = 0;
    out_vec->z = 0;
}

void md3_vec3_copy(const md3_vec3_t *in_vec, md3_vec3_t *out_vec)
{
    if (in_vec == 0 || out_vec == 0)
    {
        return;
    }

    *out_vec = *in_vec;
}

void md3_vec3_add(const md3_vec3_t *a, const md3_vec3_t *b,
    md3_vec3_t *out_vec)
{
    if (a == 0 || b == 0 || out_vec == 0)
    {
        return;
    }

    out_vec->x = a->x + b->x;
    out_vec->y = a->y + b->y;
    out_vec->z = a->z + b->z;
}

void md3_vec3_sub(const md3_vec3_t *a, const md3_vec3_t *b,
    md3_vec3_t *out_vec)
{
    if (a == 0 || b == 0 || out_vec == 0)
    {
        return;
    }

    out_vec->x = a->x - b->x;
    out_vec->y = a->y - b->y;
    out_vec->z = a->z - b->z;
}

void md3_vec3_scale(const md3_vec3_t *in_vec, md3_fixed_t scale,
    md3_vec3_t *out_vec)
{
    if (in_vec == 0 || out_vec == 0)
    {
        return;
    }

    out_vec->x = md3_fixed_mul(in_vec->x, scale);
    out_vec->y = md3_fixed_mul(in_vec->y, scale);
    out_vec->z = md3_fixed_mul(in_vec->z, scale);
}

md3_fixed_t md3_vec3_dot(const md3_vec3_t *a, const md3_vec3_t *b)
{
    if (a == 0 || b == 0)
    {
        return 0;
    }

    return md3_fixed_mul(a->x, b->x) +
        md3_fixed_mul(a->y, b->y) +
        md3_fixed_mul(a->z, b->z);
}

void md3_vec3_cross(const md3_vec3_t *a, const md3_vec3_t *b,
    md3_vec3_t *out_vec)
{
    md3_vec3_t temp;

    if (a == 0 || b == 0 || out_vec == 0)
    {
        return;
    }

    temp.x = md3_fixed_mul(a->y, b->z) - md3_fixed_mul(a->z, b->y);
    temp.y = md3_fixed_mul(a->z, b->x) - md3_fixed_mul(a->x, b->z);
    temp.z = md3_fixed_mul(a->x, b->y) - md3_fixed_mul(a->y, b->x);
    *out_vec = temp;
}

void md3_vec3_lerp(const md3_vec3_t *a, const md3_vec3_t *b,
    md3_fixed_t t, md3_vec3_t *out_vec)
{
    if (a == 0 || b == 0 || out_vec == 0)
    {
        return;
    }

    out_vec->x = md3_fixed_lerp(a->x, b->x, t);
    out_vec->y = md3_fixed_lerp(a->y, b->y, t);
    out_vec->z = md3_fixed_lerp(a->z, b->z, t);
}

md3_fixed_t md3_fixed_from_md3_xyz(md3_i16 raw_value)
{
    return (md3_fixed_t) ((md3_i32) raw_value * MD3_XYZ_SCALE_Q16);
}

md3_i16 md3_fixed_to_md3_xyz(md3_fixed_t value)
{
    md3_i32 shifted;

    if (value >= 0)
    {
        shifted = (value + 512) >> 10;
    }
    else
    {
        shifted = -(((-value) + 512) >> 10);
    }

    if (shifted > 32767)
    {
        shifted = 32767;
    }
    if (shifted < -32768)
    {
        shifted = -32768;
    }

    return (md3_i16) shifted;
}

static md3_u32 md3_abs_component_u32(md3_fixed_t value)
{
    if (value < 0)
    {
        return 0U - (md3_u32) value;
    }
    return (md3_u32) value;
}

md3_fixed_t md3_fixed_length3(md3_fixed_t x, md3_fixed_t y, md3_fixed_t z)
{
    md3_u32 ax;
    md3_u32 ay;
    md3_u32 az;
    md3_u32 max_value;
    md3_u32 sx;
    md3_u32 sy;
    md3_u32 sz;
    md3_u32 sum;
    md3_u32 root;
    unsigned int shift;

    ax = md3_abs_component_u32(x);
    ay = md3_abs_component_u32(y);
    az = md3_abs_component_u32(z);

    max_value = ax;
    if (ay > max_value) max_value = ay;
    if (az > max_value) max_value = az;

    if (max_value == 0U)
    {
        return 0;
    }

    shift = 0U;
    while (shift < 31U && (max_value >> shift) > 32767U)
    {
        ++shift;
    }

    sx = ax >> shift;
    sy = ay >> shift;
    sz = az >> shift;

    sum = sx * sx + sy * sy + sz * sz;
    root = md3_isqrt_u32(sum);

    if (shift >= 31U)
    {
        return 0x7FFFFFFF;
    }
    if (root > (0x7FFFFFFFUL >> shift))
    {
        return 0x7FFFFFFF;
    }

    return (md3_fixed_t) (root << shift);
}

int md3_vec3_normalize(const md3_vec3_t *in_vec, md3_vec3_t *out_vec)
{
    md3_fixed_t length;

    if (in_vec == 0 || out_vec == 0)
    {
        return MD3_EINVAL;
    }

    length = md3_fixed_length3(in_vec->x, in_vec->y, in_vec->z);
    if (length <= 0)
    {
        out_vec->x = 0;
        out_vec->y = 0;
        out_vec->z = 0;
        return MD3_EINVAL;
    }

    out_vec->x = md3_fixed_div(in_vec->x, length);
    out_vec->y = md3_fixed_div(in_vec->y, length);
    out_vec->z = md3_fixed_div(in_vec->z, length);
    return MD3_OK;
}

void md3_decode_packed_normal(md3_u16 packed_normal, md3_vec3_t *out_normal)
{
    md3_u8 lat;
    md3_u8 lng;
    md3_fixed_t sin_lng;
    md3_fixed_t cos_lng;
    md3_fixed_t sin_lat;
    md3_fixed_t cos_lat;

    if (out_normal == 0)
    {
        return;
    }

    lat = (md3_u8) ((packed_normal >> 8) & 0xFFU);
    lng = (md3_u8) (packed_normal & 0xFFU);

    sin_lng = md3_sin_step_q16(lng);
    cos_lng = md3_cos_step_q16(lng);
    sin_lat = md3_sin_step_q16(lat);
    cos_lat = md3_cos_step_q16(lat);

    out_normal->x = md3_fixed_mul(cos_lat, sin_lng);
    out_normal->y = md3_fixed_mul(sin_lat, sin_lng);
    out_normal->z = cos_lng;
}

md3_u16 md3_encode_packed_normal(const md3_vec3_t *normal)
{
    md3_vec3_t unit_normal;
    md3_i32 sx;
    md3_i32 sy;
    md3_i32 sz;
    md3_i32 best_score;
    md3_i32 best_sin_abs;
    md3_u16 best_packed;
    int lng;
    int lat;
    int rc;

    if (normal == 0)
    {
        return 0U;
    }

    rc = md3_vec3_normalize(normal, &unit_normal);
    if (rc != MD3_OK)
    {
        return 0U;
    }

    sx = unit_normal.x >> 8;
    sy = unit_normal.y >> 8;
    sz = unit_normal.z >> 8;

    /* Match q3's singularities explicitly: +Z -> 0x0000, -Z -> 0x0080. */
    if (sx == 0 && sy == 0)
    {
        return (sz >= 0) ? 0U : 128U;
    }

    best_score = -2147483647 - 1;
    best_sin_abs = 2147483647;
    best_packed = 0U;

    for (lng = 0; lng < 256; ++lng)
    {
        md3_i32 sin_lng8;
        md3_i32 cos_lng8;

        sin_lng8 = (md3_i32) (md3_sin_step_q16((md3_u8) lng) >> 8);
        cos_lng8 = (md3_i32) (md3_cos_step_q16((md3_u8) lng) >> 8);

        for (lat = 0; lat < 256; ++lat)
        {
            md3_i32 sin_lat8;
            md3_i32 cos_lat8;
            md3_i32 xy_score;
            md3_i32 score;

            sin_lat8 = (md3_i32) (md3_sin_step_q16((md3_u8) lat) >> 8);
            cos_lat8 = (md3_i32) (md3_cos_step_q16((md3_u8) lat) >> 8);

            xy_score = sx * cos_lat8 + sy * sin_lat8;
            score = sin_lng8 * xy_score + sz * cos_lng8;

            if (score > best_score ||
                (score == best_score &&
                ((sin_lng8 < 0 ? -sin_lng8 : sin_lng8) < best_sin_abs)))
            {
                best_score = score;
                best_sin_abs = (sin_lng8 < 0 ? -sin_lng8 : sin_lng8);
                best_packed = (md3_u16) ((((md3_u16) lat) << 8) |
                    ((md3_u16) lng));
            }
        }
    }

    return best_packed;
}
