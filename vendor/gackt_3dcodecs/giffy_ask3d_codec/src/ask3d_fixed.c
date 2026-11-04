#include "ask3d_fixed.h"

ask3d_fixed ask3d_fixed_from_int(long v)
{
    return (ask3d_fixed)(v << ASK3D_FIXED_SHIFT);
}

ask3d_fixed ask3d_fixed_mul(ask3d_fixed a, ask3d_fixed b)
{
    return (ask3d_fixed)((a * b) >> ASK3D_FIXED_SHIFT);
}

ask3d_fixed ask3d_fixed_div(ask3d_fixed a, ask3d_fixed b)
{
    if (b == 0) {
        return 0;
    }
    return (ask3d_fixed)((a << ASK3D_FIXED_SHIFT) / b);
}

static ask3d_fixed ask3d_fixed_apply_exp10(ask3d_fixed v, long exp10)
{
    long i;
    i = 0L;
    if (exp10 > 12L) {
        exp10 = 12L;
    }
    if (exp10 < -12L) {
        exp10 = -12L;
    }
    while (i < exp10) {
        v = v * 10L;
        i++;
    }
    while (i > exp10) {
        v = v / 10L;
        i--;
    }
    return v;
}

ask3d_fixed ask3d_fixed_parse(const char *text, int *ok)
{
    long sign;
    long ipart;
    long frac;
    long place;
    long exp_sign;
    long exp_value;
    unsigned int i;
    int saw_digit;
    int saw_exp_digit;
    char c;
    ask3d_fixed out;

    if (ok != 0) {
        *ok = 0;
    }
    if (text == 0) {
        return 0;
    }

    sign = 1L;
    ipart = 0L;
    frac = 0L;
    place = 10L;
    i = 0U;
    saw_digit = 0;
    exp_sign = 1L;
    exp_value = 0L;
    saw_exp_digit = 0;

    if (text[i] == '-') {
        sign = -1L;
        i++;
    } else if (text[i] == '+') {
        i++;
    }

    while (text[i] >= '0' && text[i] <= '9') {
        saw_digit = 1;
        ipart = (ipart * 10L) + (long)(text[i] - '0');
        i++;
    }

    if (text[i] == '.') {
        i++;
        while (text[i] >= '0' && text[i] <= '9' && place <= 1000000L) {
            saw_digit = 1;
            frac += ((long)(text[i] - '0') * ASK3D_FIXED_ONE) / place;
            place *= 10L;
            i++;
        }
        while (text[i] >= '0' && text[i] <= '9') {
            saw_digit = 1;
            i++;
        }
    }

    if (text[i] == 'e' || text[i] == 'E') {
        i++;
        if (text[i] == '-') {
            exp_sign = -1L;
            i++;
        } else if (text[i] == '+') {
            i++;
        }
        while (text[i] >= '0' && text[i] <= '9') {
            saw_exp_digit = 1;
            if (exp_value < 1000L) {
                exp_value = (exp_value * 10L) + (long)(text[i] - '0');
            }
            i++;
        }
        if (saw_exp_digit == 0) {
            return 0;
        }
    }

    c = text[i];
    if (c == ':' || c == ',' || c == ';') {
        c = '\0';
    }

    if (saw_digit == 0) {
        return 0;
    }
    if (!(c == '\0' || c == '\r' || c == '\n' || c == '\t' || c == ' ')) {
        return 0;
    }

    out = (ask3d_fixed)(sign * ((ipart << ASK3D_FIXED_SHIFT) + frac));
    if (exp_value != 0L) {
        out = ask3d_fixed_apply_exp10(out, exp_sign * exp_value);
    }
    if (ok != 0) {
        *ok = 1;
    }
    return out;
}

void ask3d_i32_to_ascii(long v, char *out, unsigned int out_cap)
{
    char tmp[32];
    unsigned int n;
    unsigned int i;
    unsigned long x;
    int neg;

    if (out == 0 || out_cap == 0U) {
        return;
    }

    n = 0U;
    neg = 0;
    if (v < 0) {
        neg = 1;
        x = (unsigned long)(-v);
    } else {
        x = (unsigned long)v;
    }

    if (x == 0UL) {
        tmp[n++] = '0';
    } else {
        while (x != 0UL && n < (unsigned int)sizeof(tmp)) {
            tmp[n++] = (char)('0' + (x % 10UL));
            x /= 10UL;
        }
    }

    i = 0U;
    if (neg != 0 && i + 1U < out_cap) {
        out[i++] = '-';
    }
    while (n != 0U && i + 1U < out_cap) {
        out[i++] = tmp[--n];
    }
    out[i] = '\0';
}

void ask3d_fixed_to_ascii(ask3d_fixed v, char *out, unsigned int out_cap, unsigned int frac_digits)
{
    unsigned int i;
    unsigned int p;
    unsigned int d;
    unsigned long frac;
    long ip;
    int neg;
    char intbuf[32];
    char fracbuf[16];
    unsigned int pos;

    if (out == 0 || out_cap == 0U) {
        return;
    }
    if (frac_digits > 6U) {
        frac_digits = 6U;
    }

    neg = 0;
    if (v < 0) {
        neg = 1;
        v = -v;
    }

    ip = (long)(v >> ASK3D_FIXED_SHIFT);
    frac = (unsigned long)(v & (ASK3D_FIXED_ONE - 1L));
    ask3d_i32_to_ascii(ip, intbuf, (unsigned int)sizeof(intbuf));

    pos = 0U;
    if (neg != 0 && pos + 1U < out_cap) {
        out[pos++] = '-';
    }
    i = 0U;
    while (intbuf[i] != '\0' && pos + 1U < out_cap) {
        out[pos++] = intbuf[i++];
    }

    if (frac_digits != 0U && pos + 1U < out_cap) {
        out[pos++] = '.';
        p = 0U;
        while (p < frac_digits && p < (unsigned int)sizeof(fracbuf)) {
            frac *= 10UL;
            d = (unsigned int)(frac >> ASK3D_FIXED_SHIFT);
            frac = frac & (ASK3D_FIXED_ONE - 1L);
            fracbuf[p++] = (char)('0' + d);
        }
        i = 0U;
        while (i < p && pos + 1U < out_cap) {
            out[pos++] = fracbuf[i++];
        }
    }
    out[pos] = '\0';
}
