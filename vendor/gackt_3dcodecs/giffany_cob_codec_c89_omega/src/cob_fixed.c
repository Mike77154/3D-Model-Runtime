#include "cob_private.h"

static cob_fixed cob_saturate_positive(unsigned long v)
{
    if (v > (unsigned long)COB_FIXED_MAX) {
        return COB_FIXED_MAX;
    }
    return (cob_fixed)v;
}

static cob_fixed cob_apply_sign_sat(unsigned long v, int sign)
{
    if (sign < 0) {
        if (v > 2147483648UL) {
            return COB_FIXED_MIN;
        }
        if (v == 2147483648UL) {
            return COB_FIXED_MIN;
        }
        return -(cob_fixed)v;
    }
    return cob_saturate_positive(v);
}

cob_fixed cob_fixed_from_int(long v)
{
    if (v > 32767L) {
        return COB_FIXED_MAX;
    }
    if (v < -32768L) {
        return COB_FIXED_MIN;
    }
    return (cob_fixed)(v * COB_FIXED_ONE);
}

long cob_fixed_to_int(cob_fixed v)
{
    return v / COB_FIXED_ONE;
}

int cob_fixed_to_cstr(cob_fixed v, char *out, unsigned long out_size, int decimals)
{
    unsigned long mag;
    unsigned long whole;
    unsigned long rem;
    unsigned long pos;
    int i;
    char tmp[32];
    unsigned long n;
    unsigned long digit;

    if (out == 0 || out_size == 0) {
        return 0;
    }
    if (decimals < 0) {
        decimals = 0;
    }
    if (decimals > 6) {
        decimals = 6;
    }
    pos = 0;
    if (v < 0) {
        if (pos + 1 >= out_size) {
            return 0;
        }
        out[pos++] = '-';
        if (v == COB_FIXED_MIN) {
            mag = 2147483648UL;
        } else {
            mag = (unsigned long)(-v);
        }
    } else {
        mag = (unsigned long)v;
    }
    whole = mag >> 16;
    rem = mag & 65535UL;

    sprintf(tmp, "%lu", whole);
    n = cob_strlen_ul(tmp);
    if (pos + n + 1UL >= out_size) {
        return 0;
    }
    memcpy(out + pos, tmp, n);
    pos += n;
    if (decimals > 0) {
        if (pos + 1UL >= out_size) {
            return 0;
        }
        out[pos++] = '.';
        for (i = 0; i < decimals; ++i) {
            rem *= 10UL;
            digit = rem >> 16;
            rem &= 65535UL;
            if (pos + 1UL >= out_size) {
                return 0;
            }
            out[pos++] = (char)('0' + digit);
        }
        while (pos > 0 && out[pos - 1] == '0') {
            --pos;
        }
        if (pos > 0 && out[pos - 1] == '.') {
            --pos;
        }
    }
    out[pos] = '\0';
    return 1;
}

static cob_fixed cob_fixed_mul10(cob_fixed v)
{
    if (v > COB_FIXED_MAX / 10L) {
        return COB_FIXED_MAX;
    }
    if (v < COB_FIXED_MIN / 10L) {
        return COB_FIXED_MIN;
    }
    return v * 10L;
}

static cob_fixed cob_fixed_div10(cob_fixed v)
{
    return v / 10L;
}

cob_fixed cob_fixed_parse_token(const char **cursor, const char *end, int *ok)
{
    const char *p;
    int sign;
    int any;
    int exp_sign;
    int exp_val;
    unsigned long whole;
    unsigned long mag;
    unsigned long place;
    unsigned long frac;
    cob_fixed val;
    int i;

    if (ok != 0) {
        *ok = 0;
    }
    if (cursor == 0 || *cursor == 0 || end == 0) {
        return 0;
    }
    p = cob_skip_spaces(*cursor, end);
    sign = 1;
    any = 0;
    whole = 0;
    frac = 0;
    if (p < end && *p == '-') {
        sign = -1;
        ++p;
    } else if (p < end && *p == '+') {
        ++p;
    }
    while (p < end && *p >= '0' && *p <= '9') {
        any = 1;
        if (whole < 32768UL) {
            whole = whole * 10UL + (unsigned long)(*p - '0');
        }
        ++p;
    }
    if (p < end && *p == '.') {
        ++p;
        place = 6553UL; /* floor(65536 / 10) */
        while (p < end && *p >= '0' && *p <= '9') {
            any = 1;
            if (place > 0UL) {
                frac += (unsigned long)(*p - '0') * place;
                place /= 10UL;
            }
            ++p;
        }
    }
    if (!any) {
        *cursor = p;
        return 0;
    }
    if (whole > 32767UL) {
        mag = 2147483647UL;
    } else {
        mag = (whole << 16) + frac;
    }
    val = cob_apply_sign_sat(mag, sign);
    if (p < end && (*p == 'e' || *p == 'E')) {
        ++p;
        exp_sign = 1;
        exp_val = 0;
        if (p < end && *p == '-') {
            exp_sign = -1;
            ++p;
        } else if (p < end && *p == '+') {
            ++p;
        }
        while (p < end && *p >= '0' && *p <= '9') {
            if (exp_val < 64) {
                exp_val = exp_val * 10 + (*p - '0');
            }
            ++p;
        }
        for (i = 0; i < exp_val; ++i) {
            if (exp_sign > 0) {
                val = cob_fixed_mul10(val);
            } else {
                val = cob_fixed_div10(val);
            }
        }
    }
    *cursor = p;
    if (ok != 0) {
        *ok = 1;
    }
    return val;
}

cob_fixed cob_fixed_from_ieee754_le(const unsigned char *p, int *ok)
{
    unsigned long bits;
    unsigned long mant;
    int sign;
    int exp;
    int e;
    int sh;
    unsigned long mag;

    if (ok != 0) {
        *ok = 1;
    }
    if (p == 0) {
        if (ok != 0) {
            *ok = 0;
        }
        return 0;
    }
    bits = cob_get_u32_le(p);
    sign = (bits & 0x80000000UL) ? -1 : 1;
    exp = (int)((bits >> 23) & 255UL);
    mant = bits & 0x7fffffUL;
    if (exp == 255) {
        if (ok != 0) {
            *ok = 0;
        }
        return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    }
    if (exp == 0) {
        return 0;
    }
    mant |= 0x800000UL;
    e = exp - 127;
    if (e >= 15) {
        if (ok != 0) {
            *ok = 0;
        }
        return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    }
    if (e < -32) {
        return 0;
    }
    sh = e - 7;
    if (sh >= 0) {
        mag = mant << sh;
    } else {
        sh = -sh;
        if (sh >= 31) {
            mag = 0;
        } else {
            mag = mant >> sh;
        }
    }
    return cob_apply_sign_sat(mag, sign);
}

static int cob_highest_bit_u32(unsigned long v)
{
    int i;
    for (i = 31; i >= 0; --i) {
        if ((v & (1UL << i)) != 0UL) {
            return i;
        }
    }
    return -1;
}

void cob_fixed_to_ieee754_le(cob_fixed v, unsigned char *p)
{
    unsigned long sign;
    unsigned long mag;
    int n;
    int exponent;
    unsigned long mant_full;
    unsigned long mant;
    unsigned long bits;

    if (p == 0) {
        return;
    }
    if (v == 0) {
        p[0] = p[1] = p[2] = p[3] = 0;
        return;
    }
    sign = 0UL;
    if (v < 0) {
        sign = 0x80000000UL;
        if (v == COB_FIXED_MIN) {
            mag = 2147483648UL;
        } else {
            mag = (unsigned long)(-v);
        }
    } else {
        mag = (unsigned long)v;
    }
    n = cob_highest_bit_u32(mag);
    if (n < 0) {
        p[0] = p[1] = p[2] = p[3] = 0;
        return;
    }
    exponent = n - 16 + 127;
    if (exponent <= 0) {
        p[0] = p[1] = p[2] = p[3] = 0;
        return;
    }
    if (exponent >= 255) {
        bits = sign | 0x7f7fffffUL;
        cob_put_u32_le(p, bits);
        return;
    }
    if (n > 23) {
        mant_full = mag >> (n - 23);
    } else {
        mant_full = mag << (23 - n);
    }
    mant = mant_full & 0x7fffffUL;
    bits = sign | (((unsigned long)exponent & 255UL) << 23) | mant;
    cob_put_u32_le(p, bits);
}
