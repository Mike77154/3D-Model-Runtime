#include "acc3d.h"

static int acc3d_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static void acc3d_reverse(char *s, long n)
{
    long i;
    for (i = 0; i < n / 2; ++i) {
        char t;
        t = s[i];
        s[i] = s[n - 1 - i];
        s[n - 1 - i] = t;
    }
}

int acc3d_parse_fixed(const char *s, size_t n, acc3d_fix *out_value)
{
    size_t i;
    long sign;
    long whole;
    long frac;
    long div;
    long saw_digit;
    long max_before_mul;

    if (s == (const char *)0 || out_value == (acc3d_fix *)0) return ACC3D_ERR_ARG;

    i = 0;
    sign = 1;
    whole = 0;
    frac = 0;
    div = 1;
    saw_digit = 0;
    max_before_mul = 1073741823L / 10L;

    while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
    if (i < n && (s[i] == '-' || s[i] == '+')) {
        if (s[i] == '-') sign = -1;
        ++i;
    }
    while (i < n && acc3d_is_digit(s[i])) {
        if (whole > max_before_mul) return ACC3D_ERR_RANGE;
        whole = whole * 10L + (long)(s[i] - '0');
        saw_digit = 1;
        ++i;
    }
    if (i < n && s[i] == '.') {
        ++i;
        while (i < n && acc3d_is_digit(s[i])) {
            if (div < 1000000L) {
                frac = frac * 10L + (long)(s[i] - '0');
                div = div * 10L;
            }
            saw_digit = 1;
            ++i;
        }
    }
    if (!saw_digit) return ACC3D_ERR_PARSE;
    *out_value = (acc3d_fix)(sign * ((whole << ACC3D_FIXED_SHIFT) + ((frac << ACC3D_FIXED_SHIFT) / div)));
    return ACC3D_OK;
}

void acc3d_fixed_to_ascii(acc3d_fix v, char *out, size_t out_cap, int decimals)
{
    char tmp[48];
    long pos;
    long neg;
    long whole;
    long frac;
    long scale;
    int d;
    size_t w;

    if (out == (char *)0 || out_cap == 0) return;
    out[0] = '\0';
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;

    neg = 0;
    if (v < 0) {
        neg = 1;
        v = -v;
    }

    whole = v >> ACC3D_FIXED_SHIFT;
    frac = v & (ACC3D_FIXED_ONE - 1L);
    scale = 1L;
    for (d = 0; d < decimals; ++d) scale *= 10L;
    frac = (frac * scale + (ACC3D_FIXED_ONE / 2L)) >> ACC3D_FIXED_SHIFT;
    if (frac >= scale && decimals > 0) {
        frac -= scale;
        whole += 1L;
    }

    pos = 0;
    if (whole == 0) {
        tmp[pos++] = '0';
    } else {
        while (whole > 0 && pos < (long)(sizeof(tmp) - 1)) {
            tmp[pos++] = (char)('0' + (whole % 10L));
            whole /= 10L;
        }
        acc3d_reverse(tmp, pos);
    }

    w = 0;
    if (neg && w + 1 < out_cap) out[w++] = '-';
    for (d = 0; d < pos && w + 1 < out_cap; ++d) out[w++] = tmp[d];
    if (decimals > 0 && w + 1 < out_cap) {
        out[w++] = '.';
        for (d = decimals - 1; d >= 0 && w + 1 < out_cap; --d) {
            long pow10;
            int k;
            pow10 = 1L;
            for (k = 0; k < d; ++k) pow10 *= 10L;
            out[w++] = (char)('0' + ((frac / pow10) % 10L));
        }
    }
    out[w] = '\0';
}

const char *acc3d_result_string(int code)
{
    switch (code) {
    case ACC3D_OK: return "ok";
    case ACC3D_ERR_ARG: return "bad argument";
    case ACC3D_ERR_HEADER: return "bad AC3D/ACC header";
    case ACC3D_ERR_LIMIT: return "fixed storage limit hit";
    case ACC3D_ERR_PARSE: return "parse error";
    case ACC3D_ERR_RANGE: return "number out of range";
    default: return "unknown";
    }
}
