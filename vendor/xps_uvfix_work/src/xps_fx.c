#include "xps_internal.h"

static int xps_internal_mul10_checked(xps_fx value, int digit, xps_fx *out)
{
    xps_i32 maxv;
    xps_i32 addv;

    maxv = 0x7FFFFFFFL;
    addv = ((xps_i32)digit << 16);

    if (value > (maxv - addv) / 10L) {
        return XPS_ERR_OVERFLOW;
    }
    *out = (xps_fx)(value * 10L + addv);
    return XPS_OK;
}

static int xps_internal_mul10_exp(xps_fx *value)
{
    xps_i32 maxv;

    maxv = 0x7FFFFFFFL;
    if (*value > maxv / 10L) {
        return XPS_ERR_OVERFLOW;
    }
    *value = (xps_fx)(*value * 10L);
    return XPS_OK;
}

static void xps_internal_div10_exp(xps_fx *value)
{
    xps_fx v;

    v = *value;
    *value = (xps_fx)((v + 5L) / 10L);
}

int xps_internal_fx_from_ascii(const char *text, xps_fx *out)
{
    const char *p;
    xps_fx value;
    int sign;
    int saw_digit;
    unsigned long denom;
    int exp_sign;
    int exp_value;
    int rc;

    if (text == NULL || out == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    p = text;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }

    sign = 1;
    if (*p == '+') {
        ++p;
    } else if (*p == '-') {
        sign = -1;
        ++p;
    }

    value = 0;
    saw_digit = 0;
    while (*p >= '0' && *p <= '9') {
        saw_digit = 1;
        rc = xps_internal_mul10_checked(value, (int)(*p - '0'), &value);
        if (rc != XPS_OK) {
            return rc;
        }
        ++p;
    }

    if (*p == '.') {
        ++p;
        denom = 10UL;
        while (*p >= '0' && *p <= '9') {
            xps_i32 addv;
            int digit;

            saw_digit = 1;
            digit = (int)(*p - '0');
            addv = (xps_i32)((((unsigned long)digit << 16) + (denom / 2UL)) / denom);
            if (value > 0x7FFFFFFFL - addv) {
                return XPS_ERR_OVERFLOW;
            }
            value = (xps_fx)(value + addv);
            if (denom <= ULONG_MAX / 10UL) {
                denom *= 10UL;
            }
            ++p;
        }
    }

    if (!saw_digit) {
        return XPS_ERR_PARSE;
    }

    exp_sign = 1;
    exp_value = 0;
    if (*p == 'e' || *p == 'E') {
        ++p;
        if (*p == '+') {
            ++p;
        } else if (*p == '-') {
            exp_sign = -1;
            ++p;
        }
        if (!(*p >= '0' && *p <= '9')) {
            return XPS_ERR_PARSE;
        }
        while (*p >= '0' && *p <= '9') {
            if (exp_value > 1000) {
                return XPS_ERR_OVERFLOW;
            }
            exp_value = exp_value * 10 + (int)(*p - '0');
            ++p;
        }
    }

    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }
    if (*p != '\0') {
        return XPS_ERR_PARSE;
    }

    if (exp_sign > 0) {
        while (exp_value > 0) {
            rc = xps_internal_mul10_exp(&value);
            if (rc != XPS_OK) {
                return rc;
            }
            --exp_value;
        }
    } else {
        while (exp_value > 0) {
            xps_internal_div10_exp(&value);
            --exp_value;
        }
    }

    if (sign < 0) {
        value = (xps_fx)(-value);
    }

    *out = value;
    return XPS_OK;
}

int xps_internal_f32_bits_to_fx(xps_u32 bits, xps_fx *out)
{
    xps_u32 sign;
    xps_u32 exp;
    xps_u32 mant;
    xps_i32 q;
    xps_i32 e;
    xps_i32 shift;
    xps_u32 sig;

    if (out == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    sign = (bits >> 31) & 1U;
    exp = (bits >> 23) & 0xFFU;
    mant = bits & 0x7FFFFFU;

    if (exp == 0xFFU) {
        return XPS_ERR_PARSE;
    }

    if (exp == 0U) {
        if (mant == 0U) {
            *out = 0;
            return XPS_OK;
        }
        e = -126;
        sig = mant;
    } else {
        e = (xps_i32)exp - 127;
        sig = mant | 0x800000U;
    }

    shift = e - 7;
    if (shift > 7) {
        return XPS_ERR_OVERFLOW;
    }

    if (shift >= 0) {
        if (sig > (xps_u32)(0x7FFFFFFFUL >> shift)) {
            return XPS_ERR_OVERFLOW;
        }
        q = (xps_i32)(sig << shift);
    } else {
        xps_i32 rshift;
        xps_u32 roundv;

        rshift = (xps_i32)(-shift);
        if (rshift >= 31) {
            q = 0;
        } else {
            roundv = 1U << (rshift - 1);
            q = (xps_i32)((sig + roundv) >> rshift);
        }
    }

    if (sign != 0U) {
        q = -q;
    }
    *out = q;
    return XPS_OK;
}
