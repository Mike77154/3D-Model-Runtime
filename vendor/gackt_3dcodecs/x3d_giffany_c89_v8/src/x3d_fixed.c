#include "x3d_codec.h"

x3d_fx x3d_fx_from_int(long v)
{
    return (x3d_fx)(v * X3D_ONE);
}

long x3d_fx_to_int(x3d_fx v)
{
    return v / X3D_ONE;
}

x3d_fx x3d_fx_mul(x3d_fx a, x3d_fx b)
{
    return (x3d_fx)((a * b) / X3D_ONE);
}

x3d_fx x3d_fx_div(x3d_fx a, x3d_fx b)
{
    if (b == 0) {
        return 0;
    }
    return (x3d_fx)((a * X3D_ONE) / b);
}

static int x3d_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static long x3d_pow10_small(int e)
{
    long v;
    int i;
    v = 1;
    for (i = 0; i < e; ++i) {
        v = v * 10;
    }
    return v;
}

int x3d_fx_parse(const char *s, int len, x3d_fx *out)
{
    int i;
    int sign;
    long whole;
    long frac;
    long frac_scale;
    int frac_digits;
    int exp_sign;
    int exp_val;
    x3d_fx v;
    long factor;

    if (!s || !out || len <= 0) {
        return X3D_STATUS_BAD_ARG;
    }
    i = 0;
    while (i < len && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' || s[i] == ',')) {
        ++i;
    }
    sign = 1;
    if (i < len && s[i] == '-') {
        sign = -1;
        ++i;
    } else if (i < len && s[i] == '+') {
        ++i;
    }
    whole = 0;
    while (i < len && x3d_is_digit(s[i])) {
        whole = whole * 10 + (long)(s[i] - '0');
        ++i;
    }
    frac = 0;
    frac_scale = 1;
    frac_digits = 0;
    if (i < len && s[i] == '.') {
        ++i;
        while (i < len && x3d_is_digit(s[i])) {
            if (frac_digits < 8) {
                frac = frac * 10 + (long)(s[i] - '0');
                frac_scale = frac_scale * 10;
                ++frac_digits;
            }
            ++i;
        }
    }
    v = (x3d_fx)(whole * X3D_ONE);
    if (frac_scale > 1) {
        v = v + (x3d_fx)((frac * X3D_ONE) / frac_scale);
    }
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        exp_sign = 1;
        if (i < len && s[i] == '-') {
            exp_sign = -1;
            ++i;
        } else if (i < len && s[i] == '+') {
            ++i;
        }
        exp_val = 0;
        while (i < len && x3d_is_digit(s[i])) {
            if (exp_val < 8) {
                exp_val = exp_val * 10 + (s[i] - '0');
            }
            ++i;
        }
        if (exp_val > 6) {
            exp_val = 6;
        }
        factor = x3d_pow10_small(exp_val);
        if (exp_sign > 0) {
            v = (x3d_fx)(v * factor);
        } else if (factor != 0) {
            v = (x3d_fx)(v / factor);
        }
    }
    if (sign < 0) {
        v = -v;
    }
    *out = v;
    return X3D_STATUS_OK;
}

static int x3d_append_char(char *out, int cap, int *pos, char c)
{
    if (*pos + 1 >= cap) {
        return X3D_STATUS_OUTPUT_FULL;
    }
    out[*pos] = c;
    *pos = *pos + 1;
    out[*pos] = 0;
    return X3D_STATUS_OK;
}

static int x3d_append_uint(char *out, int cap, int *pos, unsigned long v)
{
    char buf[32];
    int n;
    int i;
    n = 0;
    if (v == 0UL) {
        return x3d_append_char(out, cap, pos, '0');
    }
    while (v > 0UL && n < (int)sizeof(buf)) {
        buf[n++] = (char)('0' + (int)(v % 10UL));
        v = v / 10UL;
    }
    for (i = n - 1; i >= 0; --i) {
        if (x3d_append_char(out, cap, pos, buf[i]) != X3D_STATUS_OK) {
            return X3D_STATUS_OUTPUT_FULL;
        }
    }
    return X3D_STATUS_OK;
}

int x3d_fx_to_text(x3d_fx v, char *out, int cap)
{
    int pos;
    unsigned long whole;
    unsigned long frac;
    int i;
    int trail;
    char fracbuf[6];
    x3d_fx av;

    if (!out || cap <= 0) {
        return X3D_STATUS_BAD_ARG;
    }
    pos = 0;
    out[0] = 0;
    av = v;
    if (v < 0) {
        if (x3d_append_char(out, cap, &pos, '-') != X3D_STATUS_OK) {
            return X3D_STATUS_OUTPUT_FULL;
        }
        av = -v;
    }
    whole = (unsigned long)(av / X3D_ONE);
    frac = (unsigned long)(((av % X3D_ONE) * 10000L + (X3D_ONE / 2)) / X3D_ONE);
    if (frac >= 10000UL) {
        whole = whole + 1UL;
        frac = 0UL;
    }
    if (x3d_append_uint(out, cap, &pos, whole) != X3D_STATUS_OK) {
        return X3D_STATUS_OUTPUT_FULL;
    }
    if (frac == 0UL) {
        return X3D_STATUS_OK;
    }
    if (x3d_append_char(out, cap, &pos, '.') != X3D_STATUS_OK) {
        return X3D_STATUS_OUTPUT_FULL;
    }
    for (i = 3; i >= 0; --i) {
        fracbuf[i] = (char)('0' + (int)(frac % 10UL));
        frac = frac / 10UL;
    }
    fracbuf[4] = 0;
    trail = 3;
    while (trail > 0 && fracbuf[trail] == '0') {
        --trail;
    }
    for (i = 0; i <= trail; ++i) {
        if (x3d_append_char(out, cap, &pos, fracbuf[i]) != X3D_STATUS_OK) {
            return X3D_STATUS_OUTPUT_FULL;
        }
    }
    return X3D_STATUS_OK;
}
