#include "anima_biovisionbvh_internal.h"

static int anima_bvh_char_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static long anima_bvh_sat_mul10(long v)
{
    if (v > ANIMA_BVH_FIXED_MAX / 10L) {
        return ANIMA_BVH_FIXED_MAX;
    }
    return v * 10L;
}

static long anima_bvh_sat_add(long a, long b)
{
    if (b > 0L && a > ANIMA_BVH_FIXED_MAX - b) {
        return ANIMA_BVH_FIXED_MAX;
    }
    if (b < 0L && a < ANIMA_BVH_FIXED_MIN - b) {
        return ANIMA_BVH_FIXED_MIN;
    }
    return a + b;
}

anima_bvh_fixed anima_bvh_fixed_from_int(long value)
{
    if (value > (ANIMA_BVH_FIXED_MAX >> ANIMA_BVH_FIXED_SHIFT)) {
        return ANIMA_BVH_FIXED_MAX;
    }
    if (value < (ANIMA_BVH_FIXED_MIN >> ANIMA_BVH_FIXED_SHIFT)) {
        return ANIMA_BVH_FIXED_MIN;
    }
    return (anima_bvh_fixed)(value << ANIMA_BVH_FIXED_SHIFT);
}

anima_bvh_fixed anima_bvh_fixed_add_sat(anima_bvh_fixed a, anima_bvh_fixed b)
{
    return anima_bvh_sat_add(a, b);
}

anima_bvh_fixed anima_bvh_parse_fixed_cstr(const char *text, int *ok)
{
    long int_part;
    long frac_part;
    long denom;
    long value;
    int sign;
    int any_digit;
    int exp_sign;
    int exp_value;
    const char *p;

    if (ok != 0) {
        *ok = 0;
    }
    if (text == 0) {
        return 0L;
    }

    p = text;
    sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    int_part = 0L;
    any_digit = 0;
    while (anima_bvh_char_is_digit(*p)) {
        any_digit = 1;
        if (int_part > (ANIMA_BVH_FIXED_MAX / 10L)) {
            int_part = ANIMA_BVH_FIXED_MAX;
        } else {
            int_part = int_part * 10L + (long)(*p - '0');
        }
        p++;
    }

    frac_part = 0L;
    denom = 1L;
    if (*p == '.') {
        p++;
        while (anima_bvh_char_is_digit(*p)) {
            any_digit = 1;
            if (denom < 100000000L) {
                denom = denom * 10L;
                frac_part = frac_part + ((long)(*p - '0') * ANIMA_BVH_FIXED_ONE) / denom;
            }
            p++;
        }
    }

    if (!any_digit) {
        return 0L;
    }

    if (*p == 'e' || *p == 'E') {
        p++;
        exp_sign = 1;
        if (*p == '-') {
            exp_sign = -1;
            p++;
        } else if (*p == '+') {
            p++;
        }
        exp_value = 0;
        while (anima_bvh_char_is_digit(*p)) {
            if (exp_value < 1000) {
                exp_value = exp_value * 10 + (*p - '0');
            }
            p++;
        }
    } else {
        exp_sign = 1;
        exp_value = 0;
    }

    if (*p != '\0') {
        return 0L;
    }

    if (int_part > (ANIMA_BVH_FIXED_MAX >> ANIMA_BVH_FIXED_SHIFT)) {
        value = ANIMA_BVH_FIXED_MAX;
    } else {
        value = (int_part << ANIMA_BVH_FIXED_SHIFT);
        value = anima_bvh_sat_add(value, frac_part);
    }

    while (exp_value > 0) {
        if (exp_sign > 0) {
            value = anima_bvh_sat_mul10(value);
        } else {
            value = value / 10L;
        }
        exp_value--;
    }

    if (sign < 0) {
        if (value == ANIMA_BVH_FIXED_MAX) {
            value = ANIMA_BVH_FIXED_MIN + 1L;
        } else {
            value = -value;
        }
    }

    if (ok != 0) {
        *ok = 1;
    }
    return (anima_bvh_fixed)value;
}

static void anima_bvh_append_char(char *out, unsigned int cap, unsigned int *pos, char c)
{
    if (*pos + 1U < cap) {
        out[*pos] = c;
        *pos = *pos + 1U;
        out[*pos] = '\0';
    }
}

static void anima_bvh_append_ulong(char *out, unsigned int cap, unsigned int *pos, unsigned long v)
{
    char tmp[32];
    unsigned int n;

    n = 0U;
    if (v == 0UL) {
        anima_bvh_append_char(out, cap, pos, '0');
        return;
    }
    while (v > 0UL && n < (unsigned int)sizeof(tmp)) {
        tmp[n] = (char)('0' + (char)(v % 10UL));
        n++;
        v = v / 10UL;
    }
    while (n > 0U) {
        n--;
        anima_bvh_append_char(out, cap, pos, tmp[n]);
    }
}

void anima_bvh_fixed_to_cstr(anima_bvh_fixed value, char *out, unsigned int out_cap, int decimals)
{
    unsigned int pos;
    unsigned long u;
    unsigned long whole;
    unsigned long frac;
    int i;

    if (out == 0 || out_cap == 0U) {
        return;
    }
    out[0] = '\0';
    pos = 0U;

    if (decimals < 0) {
        decimals = 0;
    }
    if (decimals > 8) {
        decimals = 8;
    }

    if (value < 0L) {
        anima_bvh_append_char(out, out_cap, &pos, '-');
        if (value == ANIMA_BVH_FIXED_MIN) {
            u = (unsigned long)ANIMA_BVH_FIXED_MAX;
        } else {
            u = (unsigned long)(-value);
        }
    } else {
        u = (unsigned long)value;
    }

    whole = u >> ANIMA_BVH_FIXED_SHIFT;
    frac = u & 65535UL;
    anima_bvh_append_ulong(out, out_cap, &pos, whole);

    if (decimals > 0) {
        anima_bvh_append_char(out, out_cap, &pos, '.');
        i = 0;
        while (i < decimals) {
            frac = frac * 10UL;
            anima_bvh_append_char(out, out_cap, &pos, (char)('0' + (char)(frac >> ANIMA_BVH_FIXED_SHIFT)));
            frac = frac & 65535UL;
            i++;
        }
    }
}

anima_bvh_fixed anima_bvh_fixed_mul_q15(anima_bvh_fixed value, anima_bvh_q15 q15)
{
    long v;
    long q;
    long whole;
    long rem;
    long result;
    long part;
    int neg;

    if (value == 0L || q15 == 0L) {
        return 0L;
    }

    neg = 0;
    v = value;
    q = q15;
    if (v < 0L) {
        neg = !neg;
        if (v == ANIMA_BVH_FIXED_MIN) {
            v = ANIMA_BVH_FIXED_MAX;
        } else {
            v = -v;
        }
    }
    if (q < 0L) {
        neg = !neg;
        q = -q;
    }

    whole = v / 32768L;
    rem = v % 32768L;

    if (q != 0L && whole > ANIMA_BVH_FIXED_MAX / q) {
        result = ANIMA_BVH_FIXED_MAX;
    } else {
        result = whole * q;
        part = (rem * q) / 32768L;
        if (part > ANIMA_BVH_FIXED_MAX - result) {
            result = ANIMA_BVH_FIXED_MAX;
        } else {
            result = result + part;
        }
    }

    if (neg) {
        result = -result;
    }
    return result;
}

static const anima_bvh_q15 anima_bvh_sin90_q15[91] = {
    0L, 572L, 1144L, 1715L, 2286L, 2856L, 3425L, 3993L, 4560L, 5126L, 5690L, 6252L, 6813L,
    7371L, 7927L, 8481L, 9032L, 9580L, 10126L, 10668L, 11207L, 11743L, 12275L, 12803L, 13328L, 13848L,
    14364L, 14876L, 15383L, 15886L, 16383L, 16876L, 17364L, 17846L, 18323L, 18794L, 19260L, 19720L, 20173L,
    20621L, 21062L, 21497L, 21925L, 22347L, 22762L, 23170L, 23571L, 23964L, 24351L, 24730L, 25101L, 25465L,
    25821L, 26169L, 26509L, 26841L, 27165L, 27481L, 27788L, 28087L, 28377L, 28659L, 28932L, 29196L, 29451L,
    29697L, 29934L, 30162L, 30381L, 30591L, 30791L, 30982L, 31163L, 31335L, 31498L, 31650L, 31794L, 31927L,
    32051L, 32165L, 32269L, 32364L, 32448L, 32523L, 32587L, 32642L, 32687L, 32722L, 32747L, 32762L, 32767L
};

static anima_bvh_q15 anima_bvh_sin_0_90_interp_q15(long deg_q16)
{
    long whole;
    long frac;
    long a;
    long b;
    long inv;
    long out;

    if (deg_q16 <= 0L) {
        return 0L;
    }
    if (deg_q16 >= 90L * ANIMA_BVH_FIXED_ONE) {
        return ANIMA_BVH_Q15_ONE;
    }

    whole = deg_q16 >> ANIMA_BVH_FIXED_SHIFT;
    frac = deg_q16 & 65535L;
    if (whole < 0L) {
        whole = 0L;
    }
    if (whole >= 90L) {
        return ANIMA_BVH_Q15_ONE;
    }

    a = anima_bvh_sin90_q15[(int)whole];
    b = anima_bvh_sin90_q15[(int)whole + 1];
    inv = 65536L - frac;
    out = (a * inv + b * frac) >> ANIMA_BVH_FIXED_SHIFT;
    return out;
}

static long anima_bvh_mod_360_q16(anima_bvh_fixed deg)
{
    long full;

    full = 360L * ANIMA_BVH_FIXED_ONE;
    while (deg < 0L) {
        deg = deg + full;
    }
    while (deg >= full) {
        deg = deg - full;
    }
    return deg;
}

anima_bvh_q15 anima_bvh_sin_deg_q15(anima_bvh_fixed degrees_q16)
{
    long d;
    int neg;
    anima_bvh_q15 r;

    d = anima_bvh_mod_360_q16(degrees_q16);
    neg = 0;

    if (d > 180L * ANIMA_BVH_FIXED_ONE) {
        d = d - 180L * ANIMA_BVH_FIXED_ONE;
        neg = 1;
    }
    if (d > 90L * ANIMA_BVH_FIXED_ONE) {
        d = 180L * ANIMA_BVH_FIXED_ONE - d;
    }

    r = anima_bvh_sin_0_90_interp_q15(d);
    if (neg) {
        r = -r;
    }
    return r;
}

anima_bvh_q15 anima_bvh_cos_deg_q15(anima_bvh_fixed degrees_q16)
{
    return anima_bvh_sin_deg_q15(90L * ANIMA_BVH_FIXED_ONE - degrees_q16);
}
