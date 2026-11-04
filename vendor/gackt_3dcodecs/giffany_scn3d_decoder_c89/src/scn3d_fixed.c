#include "scn3d.h"

static int scn3d_is_digit(int c)
{
    return c >= '0' && c <= '9';
}

scn3d_fix scn3d_fix_from_int(long v)
{
    return (scn3d_fix)(v * SCN3D_FIX_ONE);
}

long scn3d_fix_to_int(scn3d_fix v)
{
    return (long)(v / SCN3D_FIX_ONE);
}

scn3d_fix scn3d_fix_mul(scn3d_fix a, scn3d_fix b)
{
    int sign;
    unsigned long ua;
    unsigned long ub;
    unsigned long hi;
    unsigned long lo;
    unsigned long res;

    sign = 0;
    if (a < 0) {
        ua = (unsigned long)(-a);
        sign = !sign;
    } else {
        ua = (unsigned long)a;
    }
    if (b < 0) {
        ub = (unsigned long)(-b);
        sign = !sign;
    } else {
        ub = (unsigned long)b;
    }

    /* C89-safe 16.16 multiply split into 16-bit parts. */
    hi = (ua >> 16) * (ub >> 16);
    lo = (ua & 0xffffUL) * (ub & 0xffffUL);
    res = (hi << 16) + ((ua >> 16) * (ub & 0xffffUL)) + ((ub >> 16) * (ua & 0xffffUL)) + (lo >> 16);
    if (sign) {
        return -(scn3d_fix)res;
    }
    return (scn3d_fix)res;
}

scn3d_fix scn3d_fix_from_decimal_text(const char *s, const char **endp)
{
    int neg;
    unsigned long ip;
    unsigned long fp;
    unsigned long div;
    scn3d_fix out;

    neg = 0;
    ip = 0;
    fp = 0;
    div = 1;

    while (*s == ' ' || *s == '\t') {
        ++s;
    }
    if (*s == '-') {
        neg = 1;
        ++s;
    } else if (*s == '+') {
        ++s;
    }
    while (scn3d_is_digit((unsigned char)*s)) {
        ip = ip * 10UL + (unsigned long)(*s - '0');
        ++s;
    }
    if (*s == '.') {
        ++s;
        while (scn3d_is_digit((unsigned char)*s) && div < 1000000UL) {
            fp = fp * 10UL + (unsigned long)(*s - '0');
            div = div * 10UL;
            ++s;
        }
        while (scn3d_is_digit((unsigned char)*s)) {
            ++s;
        }
    }

    out = (scn3d_fix)(ip * 65536UL);
    if (div > 1UL) {
        out += (scn3d_fix)((fp * 65536UL) / div);
    }
    if (endp) {
        *endp = s;
    }
    return neg ? -out : out;
}

scn3d_fix scn3d_fix_from_ieee754_bits(scn3d_u32 bits)
{
    int sign;
    int exp;
    scn3d_u32 mant;
    int shift;
    unsigned long value;

    sign = (int)((bits >> 31) & 1UL);
    exp = (int)((bits >> 23) & 255UL);
    mant = bits & 0x7fffffUL;

    if (exp == 0 && mant == 0UL) {
        return 0;
    }
    if (exp == 255) {
        return sign ? (scn3d_fix)0x80000000L : (scn3d_fix)0x7fffffffL;
    }
    if (exp == 0) {
        /* Denormals are tiny; keep deterministic and avoid real-number hardware ops. */
        return 0;
    }

    mant |= 0x800000UL;
    shift = exp - 134; /* exp - 127 - 23 + 16 */

    if (shift >= 0) {
        if (shift > 7) {
            value = 0x7fffffffUL;
        } else {
            value = mant << shift;
        }
    } else {
        if (shift < -31) {
            value = 0UL;
        } else {
            value = mant >> (-shift);
        }
    }

    if (value > 0x7fffffffUL) {
        value = 0x7fffffffUL;
    }
    if (sign) {
        return -(scn3d_fix)value;
    }
    return (scn3d_fix)value;
}

void scn3d_fix_to_text(scn3d_fix v, char *out, int out_size, int decimals)
{
    long ip;
    unsigned long frac;
    unsigned long scale;
    int i;
    int pos;
    char tmp[32];
    int ti;
    int neg;

    if (!out || out_size <= 0) {
        return;
    }
    out[0] = '\0';
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;

    neg = 0;
    if (v < 0) {
        neg = 1;
        v = -v;
    }
    ip = v / 65536L;
    frac = (unsigned long)(v % 65536L);
    scale = 1UL;
    for (i = 0; i < decimals; ++i) {
        scale *= 10UL;
    }
    frac = (frac * scale + 32768UL) / 65536UL;
    if (frac >= scale && scale > 0UL) {
        frac -= scale;
        ++ip;
    }

    pos = 0;
    if (neg && pos < out_size - 1) {
        out[pos++] = '-';
    }

    ti = 0;
    if (ip == 0) {
        tmp[ti++] = '0';
    } else {
        while (ip > 0 && ti < (int)sizeof(tmp)) {
            tmp[ti++] = (char)('0' + (ip % 10));
            ip /= 10;
        }
    }
    while (ti > 0 && pos < out_size - 1) {
        out[pos++] = tmp[--ti];
    }

    if (decimals > 0 && pos < out_size - 1) {
        out[pos++] = '.';
        scale /= 10UL;
        while (scale > 0UL && pos < out_size - 1) {
            out[pos++] = (char)('0' + (frac / scale) % 10UL);
            scale /= 10UL;
        }
    }
    out[pos] = '\0';
}
