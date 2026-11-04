#include "bravure_bvh_internal.h"

static const bbvh_fixed bbvh_sin_0_90[91] = {
    0L, 1144L, 2287L, 3430L, 4572L, 5712L, 6850L, 7987L,
    9121L, 10252L, 11380L, 12505L, 13626L, 14742L, 15855L, 16962L,
    18064L, 19161L, 20252L, 21336L, 22415L, 23486L, 24550L, 25607L,
    26656L, 27697L, 28729L, 29753L, 30767L, 31772L, 32768L, 33754L,
    34729L, 35693L, 36647L, 37590L, 38521L, 39441L, 40348L, 41243L,
    42126L, 42995L, 43852L, 44695L, 45525L, 46341L, 47143L, 47930L,
    48703L, 49461L, 50203L, 50931L, 51643L, 52339L, 53020L, 53684L,
    54332L, 54963L, 55578L, 56175L, 56756L, 57319L, 57865L, 58393L,
    58903L, 59396L, 59870L, 60326L, 60764L, 61183L, 61584L, 61966L,
    62328L, 62672L, 62997L, 63303L, 63589L, 63856L, 64104L, 64332L,
    64540L, 64729L, 64898L, 65048L, 65177L, 65287L, 65376L, 65446L,
    65496L, 65526L, 65536L
};

bbvh_fixed bbvh_fixed_add_sat(bbvh_fixed a, bbvh_fixed b)
{
    if (b > 0L && a > BBVH_FIXED_MAX - b) {
        return BBVH_FIXED_MAX;
    }
    if (b < 0L && a < BBVH_FIXED_MIN - b) {
        return BBVH_FIXED_MIN;
    }
    return a + b;
}

bbvh_fixed bbvh_fixed_sub_sat(bbvh_fixed a, bbvh_fixed b)
{
    if (b == BBVH_FIXED_MIN) {
        return BBVH_FIXED_MAX;
    }
    return bbvh_fixed_add_sat(a, -b);
}

bbvh_fixed bbvh_fixed_abs_sat(bbvh_fixed a)
{
    if (a == BBVH_FIXED_MIN) {
        return BBVH_FIXED_MAX;
    }
    return (a < 0L) ? -a : a;
}

static bbvh_fixed bbvh_sat_neg(bbvh_fixed a)
{
    if (a == BBVH_FIXED_MIN) {
        return BBVH_FIXED_MAX;
    }
    return -a;
}

static unsigned long bbvh_abs_to_ulong(bbvh_fixed a)
{
    if (a == BBVH_FIXED_MIN) {
        return (unsigned long)BBVH_FIXED_MAX;
    }
    if (a < 0L) {
        return (unsigned long)(-a);
    }
    return (unsigned long)a;
}

static unsigned long bbvh_ul_lshift_sat(unsigned long v, int shift)
{
    int i;
    unsigned long maxv;

    maxv = (unsigned long)BBVH_FIXED_MAX;
    for (i = 0; i < shift; ++i) {
        if (v > (maxv >> 1)) {
            return maxv;
        }
        v <<= 1;
    }
    return v;
}

static unsigned long bbvh_ul_add_sat(unsigned long a, unsigned long b)
{
    unsigned long maxv;

    maxv = (unsigned long)BBVH_FIXED_MAX;
    if (a > maxv - b) {
        return maxv;
    }
    return a + b;
}

static bbvh_fixed bbvh_sat_mul_int(bbvh_fixed a, long b)
{
    bbvh_fixed r;
    long i;
    int neg;

    if (b == 0L || a == 0L) {
        return 0L;
    }
    neg = BBVH_FALSE;
    if (a < 0L) {
        a = bbvh_sat_neg(a);
        neg = !neg;
    }
    if (b < 0L) {
        b = -b;
        neg = !neg;
    }
    r = 0L;
    for (i = 0L; i < b; ++i) {
        r = bbvh_fixed_add_sat(r, a);
        if (r == BBVH_FIXED_MAX) {
            break;
        }
    }
    return neg ? bbvh_sat_neg(r) : r;
}

bbvh_fixed bbvh_fixed_from_int(long v)
{
    if (v > (BBVH_FIXED_MAX >> BBVH_FIXED_SHIFT)) {
        return BBVH_FIXED_MAX;
    }
    if (v < (BBVH_FIXED_MIN >> BBVH_FIXED_SHIFT)) {
        return BBVH_FIXED_MIN;
    }
    return (bbvh_fixed)(v << BBVH_FIXED_SHIFT);
}

bbvh_fixed bbvh_fixed_mul(bbvh_fixed a, bbvh_fixed b)
{
    unsigned long ua;
    unsigned long ub;
    unsigned long acc;
    unsigned long term;
    unsigned long bitmask;
    int bit;
    int neg;

    if (a == 0L || b == 0L) {
        return 0L;
    }

    neg = BBVH_FALSE;
    if (a < 0L) {
        neg = !neg;
    }
    if (b < 0L) {
        neg = !neg;
    }

    ua = bbvh_abs_to_ulong(a);
    ub = bbvh_abs_to_ulong(b);
    acc = 0UL;

    for (bit = 0; bit < 31; ++bit) {
        bitmask = ((unsigned long)1UL) << bit;
        if ((ub & bitmask) != 0UL) {
            if (bit < BBVH_FIXED_SHIFT) {
                term = ua >> (BBVH_FIXED_SHIFT - bit);
            } else {
                term = bbvh_ul_lshift_sat(ua, bit - BBVH_FIXED_SHIFT);
            }
            acc = bbvh_ul_add_sat(acc, term);
            if (acc >= (unsigned long)BBVH_FIXED_MAX) {
                break;
            }
        }
    }

    if (neg) {
        if (acc >= (unsigned long)BBVH_FIXED_MAX) {
            return BBVH_FIXED_MIN;
        }
        return (bbvh_fixed)(-(long)acc);
    }
    if (acc >= (unsigned long)BBVH_FIXED_MAX) {
        return BBVH_FIXED_MAX;
    }
    return (bbvh_fixed)acc;
}

bbvh_fixed bbvh_fixed_div_int(bbvh_fixed a, long b)
{
    if (b == 0L) {
        return (a < 0L) ? BBVH_FIXED_MIN : BBVH_FIXED_MAX;
    }
    return (bbvh_fixed)(a / b);
}

bbvh_fixed bbvh_fixed_div(bbvh_fixed a, bbvh_fixed b)
{
    int neg;
    bbvh_fixed aa;
    bbvh_fixed bb;
    bbvh_fixed lo;
    bbvh_fixed hi;
    bbvh_fixed mid;
    bbvh_fixed prod;
    bbvh_fixed best;
    int i;

    if (b == 0L) {
        return (a < 0L) ? BBVH_FIXED_MIN : BBVH_FIXED_MAX;
    }
    if (a == 0L) {
        return 0L;
    }

    neg = BBVH_FALSE;
    if (a < 0L) {
        neg = !neg;
    }
    if (b < 0L) {
        neg = !neg;
    }

    aa = bbvh_fixed_abs_sat(a);
    bb = bbvh_fixed_abs_sat(b);
    lo = 0L;
    hi = BBVH_FIXED_MAX;
    best = 0L;

    for (i = 0; i < 31; ++i) {
        mid = lo + ((hi - lo) / 2L);
        prod = bbvh_fixed_mul(mid, bb);
        if (prod <= aa && prod >= 0L) {
            best = mid;
            if (mid == BBVH_FIXED_MAX) {
                break;
            }
            lo = mid + 1L;
        } else {
            if (mid == 0L) {
                break;
            }
            hi = mid - 1L;
        }
    }

    return neg ? bbvh_sat_neg(best) : best;
}

bbvh_fixed bbvh_fixed_sqrt(bbvh_fixed value)
{
    bbvh_fixed lo;
    bbvh_fixed hi;
    bbvh_fixed mid;
    bbvh_fixed sq;
    bbvh_fixed best;
    int i;

    if (value <= 0L) {
        return 0L;
    }

    lo = 0L;
    hi = (value > BBVH_FIXED_ONE) ? value : BBVH_FIXED_ONE;
    best = 0L;

    for (i = 0; i < 31; ++i) {
        mid = lo + ((hi - lo) / 2L);
        sq = bbvh_fixed_mul(mid, mid);
        if (sq <= value && sq >= 0L) {
            best = mid;
            if (mid == BBVH_FIXED_MAX) {
                break;
            }
            lo = mid + 1L;
        } else {
            if (mid == 0L) {
                break;
            }
            hi = mid - 1L;
        }
    }
    return best;
}

bbvh_fixed bbvh_vec3_length(const bbvh_fixed xyz[3])
{
    bbvh_fixed sum;

    if (xyz == (const bbvh_fixed *)0) {
        return 0L;
    }
    sum = bbvh_fixed_mul(xyz[0], xyz[0]);
    sum = bbvh_fixed_add_sat(sum, bbvh_fixed_mul(xyz[1], xyz[1]));
    sum = bbvh_fixed_add_sat(sum, bbvh_fixed_mul(xyz[2], xyz[2]));
    return bbvh_fixed_sqrt(sum);
}

bbvh_fixed bbvh_fixed_ratio_clamped(bbvh_fixed numerator, bbvh_fixed denominator)
{
    unsigned long n;
    unsigned long d;
    unsigned long r;
    unsigned long q;
    int bit;

    if (denominator <= 0L) {
        return (numerator <= 0L) ? 0L : BBVH_FIXED_ONE;
    }
    if (numerator <= 0L) {
        return 0L;
    }
    if (numerator >= denominator) {
        return BBVH_FIXED_ONE;
    }

    n = (unsigned long)numerator;
    d = (unsigned long)denominator;
    r = n;
    q = 0UL;
    for (bit = BBVH_FIXED_SHIFT - 1; bit >= 0; --bit) {
        r <<= 1;
        if (r >= d) {
            q |= ((unsigned long)1UL) << bit;
            r -= d;
        }
    }
    if (q > (unsigned long)BBVH_FIXED_ONE) {
        q = (unsigned long)BBVH_FIXED_ONE;
    }
    return (bbvh_fixed)q;
}

bbvh_fixed bbvh_fixed_lerp(bbvh_fixed a, bbvh_fixed b, bbvh_fixed t_q16)
{
    bbvh_fixed delta;
    bbvh_fixed step;

    if (t_q16 <= 0L) {
        return a;
    }
    if (t_q16 >= BBVH_FIXED_ONE) {
        return b;
    }
    delta = bbvh_fixed_sub_sat(b, a);
    step = bbvh_fixed_mul(delta, t_q16);
    return bbvh_fixed_add_sat(a, step);
}

static bbvh_fixed bbvh_sin_lookup_0_90(bbvh_fixed degrees_q16)
{
    long deg;
    bbvh_fixed frac;
    bbvh_fixed a;
    bbvh_fixed b;
    bbvh_fixed delta;
    bbvh_fixed part;

    if (degrees_q16 <= 0L) {
        return 0L;
    }
    if (degrees_q16 >= 90L * BBVH_FIXED_ONE) {
        return BBVH_FIXED_ONE;
    }
    deg = degrees_q16 >> BBVH_FIXED_SHIFT;
    frac = degrees_q16 & (BBVH_FIXED_ONE - 1L);
    a = bbvh_sin_0_90[(int)deg];
    b = bbvh_sin_0_90[(int)deg + 1];
    delta = b - a;
    part = (delta * (frac >> 8)) >> 8;
    return a + part;
}

bbvh_fixed bbvh_fixed_sin_deg(bbvh_fixed degrees_q16)
{
    bbvh_fixed full;
    bbvh_fixed half;
    bbvh_fixed q1;
    bbvh_fixed q2;
    bbvh_fixed d;

    full = 360L * BBVH_FIXED_ONE;
    half = 180L * BBVH_FIXED_ONE;
    q1 = 90L * BBVH_FIXED_ONE;
    q2 = 270L * BBVH_FIXED_ONE;
    d = degrees_q16 % full;
    if (d < 0L) {
        d += full;
    }
    if (d <= q1) {
        return bbvh_sin_lookup_0_90(d);
    }
    if (d <= half) {
        return bbvh_sin_lookup_0_90(half - d);
    }
    if (d <= q2) {
        return bbvh_sat_neg(bbvh_sin_lookup_0_90(d - half));
    }
    return bbvh_sat_neg(bbvh_sin_lookup_0_90(full - d));
}

bbvh_fixed bbvh_fixed_cos_deg(bbvh_fixed degrees_q16)
{
    return bbvh_fixed_sin_deg(90L * BBVH_FIXED_ONE - degrees_q16);
}

int bbvh_fixed_from_ascii(const char *text, bbvh_fixed *out_value)
{
    int i;
    int sign;
    int any;
    long ip;
    long frac_num;
    long frac_den;
    bbvh_fixed frac;
    bbvh_fixed val;
    int exp_sign;
    long exp_val;
    int frac_digits;

    if (text == (const char *)0 || out_value == (bbvh_fixed *)0) {
        return BBVH_FALSE;
    }

    i = 0;
    sign = 1;
    if (text[i] == '-') {
        sign = -1;
        ++i;
    } else if (text[i] == '+') {
        ++i;
    }

    ip = 0L;
    any = BBVH_FALSE;
    while (bbvh_is_digit(text[i])) {
        any = BBVH_TRUE;
        if (ip < 32760L) {
            ip = ip * 10L + (long)(text[i] - '0');
        }
        ++i;
    }

    frac = 0L;
    frac_num = 0L;
    frac_den = 1L;
    frac_digits = 0;
    if (text[i] == '.') {
        ++i;
        while (bbvh_is_digit(text[i])) {
            any = BBVH_TRUE;
            if (frac_digits < 9) {
                frac_num = frac_num * 10L + (long)(text[i] - '0');
                frac_den = frac_den * 10L;
                frac_digits += 1;
            }
            ++i;
        }
    }
    if (frac_digits > 0) {
        int bit;
        long rem;
        rem = frac_num;
        frac = 0L;
        for (bit = 15; bit >= 0; --bit) {
            rem = rem * 2L;
            if (rem >= frac_den) {
                frac |= (bbvh_fixed)(1L << bit);
                rem -= frac_den;
            }
        }
        if (rem > 0L && rem <= 1073741823L && rem * 2L >= frac_den) {
            if (frac < BBVH_FIXED_ONE - 1L) {
                frac += 1L;
            } else {
                ip += 1L;
                frac = 0L;
            }
        }
    }

    if (!any) {
        return BBVH_FALSE;
    }

    exp_sign = 1;
    exp_val = 0L;
    if (text[i] == 'e' || text[i] == 'E') {
        ++i;
        if (text[i] == '-') {
            exp_sign = -1;
            ++i;
        } else if (text[i] == '+') {
            ++i;
        }
        if (!bbvh_is_digit(text[i])) {
            return BBVH_FALSE;
        }
        while (bbvh_is_digit(text[i])) {
            if (exp_val < 8L) {
                exp_val = exp_val * 10L + (long)(text[i] - '0');
            }
            ++i;
        }
    }

    if (text[i] != '\0') {
        return BBVH_FALSE;
    }

    val = bbvh_fixed_from_int(ip);
    val = bbvh_fixed_add_sat(val, frac);
    if (exp_val > 0L) {
        long k;
        if (exp_sign > 0) {
            for (k = 0L; k < exp_val; ++k) {
                val = bbvh_sat_mul_int(val, 10L);
            }
        } else {
            for (k = 0L; k < exp_val; ++k) {
                val = val / 10L;
            }
        }
    }
    if (sign < 0) {
        val = bbvh_sat_neg(val);
    }
    *out_value = val;
    return BBVH_TRUE;
}

static int bbvh_write_char_local(char *out_text, int out_cap, int *pos, char c)
{
    if (*pos >= out_cap - 1) {
        return BBVH_FALSE;
    }
    out_text[*pos] = c;
    *pos = *pos + 1;
    out_text[*pos] = '\0';
    return BBVH_TRUE;
}

int bbvh_fixed_to_ascii(bbvh_fixed value, char *out_text, int out_cap, int decimals)
{
    char tmp[32];
    int tp;
    int op;
    long ip;
    bbvh_fixed frac;
    int d;

    if (out_text == (char *)0 || out_cap <= 0) {
        return BBVH_FALSE;
    }
    out_text[0] = '\0';
    op = 0;
    if (decimals < 0) {
        decimals = 0;
    }
    if (decimals > 8) {
        decimals = 8;
    }
    if (value < 0L) {
        if (!bbvh_write_char_local(out_text, out_cap, &op, '-')) {
            return BBVH_FALSE;
        }
        value = bbvh_sat_neg(value);
    }

    ip = value >> BBVH_FIXED_SHIFT;
    frac = value & (BBVH_FIXED_ONE - 1L);

    tp = 0;
    if (ip == 0L) {
        tmp[tp++] = '0';
    } else {
        while (ip > 0L && tp < (int)sizeof(tmp)) {
            tmp[tp++] = (char)('0' + (int)(ip % 10L));
            ip = ip / 10L;
        }
    }
    while (tp > 0) {
        --tp;
        if (!bbvh_write_char_local(out_text, out_cap, &op, tmp[tp])) {
            return BBVH_FALSE;
        }
    }

    if (decimals > 0) {
        if (!bbvh_write_char_local(out_text, out_cap, &op, '.')) {
            return BBVH_FALSE;
        }
        for (d = 0; d < decimals; ++d) {
            frac = frac * 10L;
            if (!bbvh_write_char_local(out_text, out_cap, &op,
                                       (char)('0' + (int)(frac >> BBVH_FIXED_SHIFT)))) {
                return BBVH_FALSE;
            }
            frac = frac & (BBVH_FIXED_ONE - 1L);
        }
    }
    return BBVH_TRUE;
}

long bbvh_fixed_floor_int(bbvh_fixed value)
{
    if (value >= 0L) {
        return value >> BBVH_FIXED_SHIFT;
    }
    return -((bbvh_fixed_abs_sat(value) + BBVH_FIXED_ONE - 1L) >> BBVH_FIXED_SHIFT);
}
