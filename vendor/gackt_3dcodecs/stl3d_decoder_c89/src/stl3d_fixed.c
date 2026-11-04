#include "stl3d_fixed.h"

static int stl3d_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static stl3d_fixed stl3d_sat_mul10(stl3d_fixed v, int *sat)
{
    if (v > STL3D_FIXED_MAX / 10L) {
        if (sat) *sat = 1;
        return STL3D_FIXED_MAX;
    }
    if (v < STL3D_FIXED_MIN / 10L) {
        if (sat) *sat = 1;
        return STL3D_FIXED_MIN;
    }
    return (stl3d_fixed)(v * 10L);
}

static stl3d_fixed stl3d_sat_pos_add(stl3d_fixed a, stl3d_fixed b, int *sat)
{
    if (a > STL3D_FIXED_MAX - b) {
        if (sat) *sat = 1;
        return STL3D_FIXED_MAX;
    }
    return (stl3d_fixed)(a + b);
}

stl3d_fixed stl3d_fixed_from_ascii(const char *text, int *ok)
{
    int sign;
    int saw_digit;
    int exp_sign;
    long exp_val;
    stl3d_fixed whole;
    stl3d_fixed frac;
    stl3d_fixed frac_unit;
    stl3d_fixed value;
    const char *p;
    int sat;

    sat = 0;
    if (ok) *ok = 0;
    if (!text) return 0;

    p = text;
    sign = 1;
    saw_digit = 0;
    whole = 0;
    frac = 0;
    frac_unit = STL3D_FIXED_ONE / 10L;

    if (*p == '+') {
        ++p;
    } else if (*p == '-') {
        sign = -1;
        ++p;
    }

    while (stl3d_is_digit(*p)) {
        int d;
        saw_digit = 1;
        d = (int)(*p - '0');
        if (whole > (STL3D_FIXED_MAX >> STL3D_FIXED_SHIFT)) {
            whole = (STL3D_FIXED_MAX >> STL3D_FIXED_SHIFT);
            sat = 1;
        } else {
            whole = whole * 10L + (stl3d_fixed)d;
            if (whole > (STL3D_FIXED_MAX >> STL3D_FIXED_SHIFT)) {
                whole = (STL3D_FIXED_MAX >> STL3D_FIXED_SHIFT);
                sat = 1;
            }
        }
        ++p;
    }

    if (*p == '.') {
        ++p;
        while (stl3d_is_digit(*p)) {
            int d2;
            saw_digit = 1;
            d2 = (int)(*p - '0');
            if (frac_unit > 0) {
                frac = stl3d_sat_pos_add(frac, (stl3d_fixed)(d2 * frac_unit), &sat);
                frac_unit /= 10L;
            }
            ++p;
        }
    }

    value = (stl3d_fixed)(whole << STL3D_FIXED_SHIFT);
    value = stl3d_sat_pos_add(value, frac, &sat);

    if (*p == 'e' || *p == 'E') {
        ++p;
        exp_sign = 1;
        exp_val = 0;
        if (*p == '+') {
            ++p;
        } else if (*p == '-') {
            exp_sign = -1;
            ++p;
        }
        if (!stl3d_is_digit(*p)) return 0;
        while (stl3d_is_digit(*p)) {
            if (exp_val < 64L) exp_val = exp_val * 10L + (long)(*p - '0');
            ++p;
        }
        while (exp_val > 0L) {
            if (exp_sign > 0) {
                value = stl3d_sat_mul10(value, &sat);
            } else {
                value = (stl3d_fixed)(value / 10L);
            }
            --exp_val;
        }
    }

    if (*p != '\0') return 0;
    if (!saw_digit) return 0;

    if (sign < 0) value = (stl3d_fixed)(-value);
    if (ok) *ok = 1;
    (void)sat;
    return value;
}

static stl3d_u32 stl3d_u32le(const stl3d_u8 b[4])
{
    stl3d_u32 v;
    v = ((stl3d_u32)b[0]) |
        ((stl3d_u32)b[1] << 8) |
        ((stl3d_u32)b[2] << 16) |
        ((stl3d_u32)b[3] << 24);
    return v;
}

static stl3d_u32 stl3d_sat_lshift_u32(stl3d_u32 v, int sh, int *saturated)
{
    while (sh > 0) {
        if (v > ((stl3d_u32)STL3D_FIXED_MAX / 2UL)) {
            if (saturated) *saturated = 1;
            return (stl3d_u32)STL3D_FIXED_MAX;
        }
        v <<= 1;
        --sh;
    }
    return v;
}

int stl3d_f32le_is_nonfinite(const stl3d_u8 b[4])
{
    stl3d_u32 bits;
    bits = stl3d_u32le(b);
    return ((bits >> 23) & 255UL) == 255UL;
}

stl3d_fixed stl3d_fixed_from_f32le_ex(const stl3d_u8 b[4], int *non_finite, int *saturated)
{
    stl3d_u32 bits;
    stl3d_u32 mant;
    int sign;
    int exp;
    int shift;
    stl3d_u32 mag;

    if (non_finite) *non_finite = 0;
    if (saturated) *saturated = 0;

    bits = stl3d_u32le(b);
    sign = (bits & 0x80000000UL) ? -1 : 1;
    exp = (int)((bits >> 23) & 255UL);
    mant = bits & 0x7fffffUL;

    if (exp == 255) {
        if (non_finite) *non_finite = 1;
        if (saturated) *saturated = 1;
        return sign < 0 ? STL3D_FIXED_MIN : STL3D_FIXED_MAX;
    }
    if (exp == 0) {
        return 0;
    }

    mant |= 0x800000UL;
    shift = exp - 134; /* exp-127-23+16 */
    if (shift >= 0) {
        mag = stl3d_sat_lshift_u32(mant, shift, saturated);
    } else {
        int rshift;
        rshift = -shift;
        if (rshift >= 31) {
            mag = 0;
        } else {
            stl3d_u32 rounder;
            rounder = (rshift > 0) ? (1UL << (rshift - 1)) : 0UL;
            mag = (mant + rounder) >> rshift;
        }
    }

    if (mag > (stl3d_u32)STL3D_FIXED_MAX) {
        mag = (stl3d_u32)STL3D_FIXED_MAX;
        if (saturated) *saturated = 1;
    }
    if (sign < 0) return (stl3d_fixed)(-(stl3d_fixed)mag);
    return (stl3d_fixed)mag;
}

stl3d_fixed stl3d_fixed_from_f32le(const stl3d_u8 b[4])
{
    return stl3d_fixed_from_f32le_ex(b, 0, 0);
}

static void stl3d_rev(char *s, unsigned long n)
{
    unsigned long i;
    i = 0;
    while (i < n / 2UL) {
        char t;
        t = s[i];
        s[i] = s[n - 1UL - i];
        s[n - 1UL - i] = t;
        ++i;
    }
}

static unsigned long stl3d_write_ulong(char *out, unsigned long cap, unsigned long v)
{
    char tmp[24];
    unsigned long n;
    unsigned long i;

    n = 0;
    if (v == 0UL) {
        if (cap > 1UL) {
            out[0] = '0';
            out[1] = '\0';
            return 1UL;
        }
        if (cap > 0UL) out[0] = '\0';
        return 0UL;
    }

    while (v > 0UL && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10UL));
        v /= 10UL;
    }
    stl3d_rev(tmp, n);
    i = 0;
    while (i < n && i + 1UL < cap) {
        out[i] = tmp[i];
        ++i;
    }
    if (cap > 0UL) out[i] = '\0';
    return i;
}

void stl3d_fixed_to_ascii(stl3d_fixed v, char *out, unsigned long out_cap, unsigned int decimals)
{
    unsigned long pos;
    unsigned long whole;
    unsigned long frac;
    unsigned int i;
    int neg;

    if (!out || out_cap == 0UL) return;
    out[0] = '\0';
    pos = 0;
    neg = 0;
    if (v < 0) {
        neg = 1;
        if (v == STL3D_FIXED_MIN) v = STL3D_FIXED_MAX;
        else v = (stl3d_fixed)(-v);
    }
    if (neg && pos + 1UL < out_cap) out[pos++] = '-';
    whole = ((unsigned long)v) >> STL3D_FIXED_SHIFT;
    pos += stl3d_write_ulong(out + pos, out_cap - pos, whole);
    if (decimals > 0U && pos + 1UL < out_cap) {
        out[pos++] = '.';
        out[pos] = '\0';
        frac = ((unsigned long)v) & ((1UL << STL3D_FIXED_SHIFT) - 1UL);
        i = 0U;
        while (i < decimals && pos + 1UL < out_cap) {
            frac *= 10UL;
            out[pos++] = (char)('0' + (frac >> STL3D_FIXED_SHIFT));
            frac &= ((1UL << STL3D_FIXED_SHIFT) - 1UL);
            out[pos] = '\0';
            ++i;
        }
    }
    if (pos < out_cap) out[pos] = '\0';
    else out[out_cap - 1UL] = '\0';
}
