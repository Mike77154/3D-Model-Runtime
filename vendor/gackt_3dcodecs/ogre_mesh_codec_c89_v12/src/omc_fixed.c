#include "omc_fixed.h"
#include <stdio.h>
#include <string.h>

static int omc_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

OMC_Fixed omc_fixed_from_int(long v)
{
    return (OMC_Fixed)(v << OMC_FIXED_SHIFT);
}

OMC_Fixed omc_fixed_abs(OMC_Fixed v)
{
    if (v < 0) return -v;
    return v;
}

OMC_Fixed omc_fixed_min(OMC_Fixed a, OMC_Fixed b)
{
    return a < b ? a : b;
}

OMC_Fixed omc_fixed_max(OMC_Fixed a, OMC_Fixed b)
{
    return a > b ? a : b;
}

static OMC_Fixed omc_fixed_mul10_checked(OMC_Fixed v)
{
    if (v > 107374182L) return 2147483647L;
    if (v < -107374182L) return -2147483647L;
    return (OMC_Fixed)(v * 10L);
}

static OMC_Fixed omc_fixed_div10(OMC_Fixed v)
{
    return (OMC_Fixed)(v / 10L);
}

OMC_Fixed omc_fixed_from_decimal(const char *text)
{
    const char *p;
    int sign;
    long whole;
    long frac_fixed;
    unsigned long frac_num;
    unsigned long frac_den;
    unsigned long rem;
    int frac_digits;
    int bit;
    int exp_sign;
    long exp_value;
    OMC_Fixed value;

    if (!text) return 0;

    p = text;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;

    sign = 1;
    if (*p == '-') { sign = -1; p++; }
    else if (*p == '+') { p++; }

    whole = 0;
    while (omc_is_digit(*p)) {
        if (whole < 107374182L) whole = whole * 10L + (long)(*p - '0');
        p++;
    }

    value = (OMC_Fixed)(whole << OMC_FIXED_SHIFT);

    frac_fixed = 0;
    frac_num = 0UL;
    frac_den = 1UL;
    frac_digits = 0;
    if (*p == '.') {
        p++;
        while (omc_is_digit(*p)) {
            if (frac_digits < 8) {
                frac_num = frac_num * 10UL + (unsigned long)(*p - '0');
                frac_den *= 10UL;
                frac_digits++;
            }
            p++;
        }
    }
    if (frac_digits > 0) {
        rem = frac_num;
        bit = OMC_FIXED_SHIFT - 1;
        while (bit >= 0) {
            rem *= 2UL;
            if (rem >= frac_den) {
                rem -= frac_den;
                frac_fixed |= (1L << bit);
            }
            bit--;
        }
        rem *= 2UL;
        if (rem >= frac_den && frac_fixed < OMC_FIXED_ONE) frac_fixed++;
        if (frac_fixed >= OMC_FIXED_ONE) {
            value += (OMC_Fixed)OMC_FIXED_ONE;
            frac_fixed -= OMC_FIXED_ONE;
        }
    }
    value += (OMC_Fixed)frac_fixed;

    exp_sign = 1;
    exp_value = 0;
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '-') { exp_sign = -1; p++; }
        else if (*p == '+') { p++; }
        while (omc_is_digit(*p)) {
            if (exp_value < 16L) exp_value = exp_value * 10L + (long)(*p - '0');
            p++;
        }
        while (exp_value > 0) {
            if (exp_sign > 0) value = omc_fixed_mul10_checked(value);
            else value = omc_fixed_div10(value);
            exp_value--;
        }
    }

    if (sign < 0) value = -value;
    return value;
}

OMC_Fixed omc_fixed_from_ieee754(unsigned long bits32)
{
    unsigned long sign;
    unsigned long exp;
    unsigned long mant;
    long shift;
    unsigned long mag;
    OMC_Fixed out;

    bits32 &= 0xffffffffUL;
    sign = (bits32 >> 31) & 1UL;
    exp = (bits32 >> 23) & 0xffUL;
    mant = bits32 & 0x7fffffUL;

    if (exp == 0xffUL) {
        if (sign) return (OMC_Fixed)-2147483647L;
        return (OMC_Fixed)2147483647L;
    }

    if (exp == 0UL) {
        return 0;
    }

    mant |= 0x800000UL;
    shift = (long)exp - 134L;

    if (shift >= 0L) {
        if (shift > 7L) mag = 0x7fffffffUL;
        else mag = (mant << shift) & 0xffffffffUL;
    } else {
        shift = -shift;
        if (shift > 31L) mag = 0UL;
        else mag = mant >> shift;
    }

    if (mag > 0x7fffffffUL) mag = 0x7fffffffUL;
    out = (OMC_Fixed)mag;
    if (sign) out = -out;
    return out;
}

void omc_fixed_to_string(OMC_Fixed v, char *out, unsigned long out_cap, int decimals)
{
    OMC_Fixed av;
    long whole;
    long frac;
    long scale;
    long frac_scaled;
    char tmp[48];
    char fmt[16];
    int neg;
    unsigned long i;

    if (!out || out_cap == 0UL) return;
    out[0] = '\0';

    if (decimals < 0) decimals = 0;
    if (decimals > 5) decimals = 5;

    neg = 0;
    av = v;
    if (v < 0) { neg = 1; av = -v; }

    whole = (long)(av >> OMC_FIXED_SHIFT);
    frac = (long)(av & 0xffffL);
    scale = 1L;
    i = 0UL;
    while ((int)i < decimals) { scale *= 10L; i++; }
    frac_scaled = (frac * scale + OMC_FIXED_HALF) >> OMC_FIXED_SHIFT;
    if (frac_scaled >= scale && decimals > 0) {
        whole++;
        frac_scaled = 0;
    }

    if (decimals == 0) {
        if (neg) sprintf(tmp, "-%ld", whole);
        else sprintf(tmp, "%ld", whole);
    } else {
        sprintf(fmt, "%%s%%ld.%%0%dld", decimals);
        sprintf(tmp, fmt, neg ? "-" : "", whole, frac_scaled);
    }

    i = 0UL;
    while (tmp[i] && i + 1UL < out_cap) {
        out[i] = tmp[i];
        i++;
    }
    out[i] = '\0';
}
