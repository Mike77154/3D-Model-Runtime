#include "fbx_fixed.h"

#include <limits.h>

static fbx_real fbx_real_saturate_i64(fbx_i64 v)
{
    if (v > (fbx_i64)FBX_REAL_MAX) return FBX_REAL_MAX;
    if (v < (fbx_i64)FBX_REAL_MIN) return FBX_REAL_MIN;
    return (fbx_real)v;
}

fbx_real fbx_real_from_int(fbx_i32 v)
{
    return fbx_real_saturate_i64(((fbx_i64)v) * ((fbx_i64)1 << FBX_REAL_SHIFT));
}

static fbx_u64 fbx_abs_i64_to_u64(fbx_i64 v)
{
    if (v < 0) return (fbx_u64)(-(v + 1)) + (fbx_u64)1u;
    return (fbx_u64)v;
}

static fbx_real fbx_real_apply_sign_u64(fbx_u64 q, int sign)
{
    if (sign) {
        if (q >= ((fbx_u64)FBX_REAL_MAX + (fbx_u64)1u)) return FBX_REAL_MIN;
        return (fbx_real)(-((fbx_i64)q));
    }
    if (q > (fbx_u64)FBX_REAL_MAX) return FBX_REAL_MAX;
    return (fbx_real)q;
}

static fbx_u64 fbx_div_q16_abs(fbx_u64 num, fbx_u64 den, fbx_u64 limit)
{
    fbx_u64 whole;
    fbx_u64 rem;
    fbx_u64 q;
    fbx_u64 frac;
    int i;

    if (den == 0u) return limit;
    whole = num / den;
    rem = num % den;

    if (whole > (limit >> FBX_REAL_SHIFT)) return limit;
    q = whole << FBX_REAL_SHIFT;

    frac = 0u;
    for (i = 0; i < FBX_REAL_SHIFT; ++i) {
        frac <<= 1;
        if (rem >= den - rem) {
            rem = rem - (den - rem);
            frac |= 1u;
        } else {
            rem += rem;
        }
    }

    if (q > limit - frac) return limit;
    return q + frac;
}

fbx_real fbx_real_from_i64(fbx_i64 v)
{
    if (v > ((fbx_i64)FBX_REAL_MAX >> FBX_REAL_SHIFT)) return FBX_REAL_MAX;
    if (v < ((fbx_i64)FBX_REAL_MIN >> FBX_REAL_SHIFT)) return FBX_REAL_MIN;
    return (fbx_real)(v * ((fbx_i64)1 << FBX_REAL_SHIFT));
}

fbx_real fbx_real_from_ratio(fbx_i64 num, fbx_i64 den)
{
    int sign;
    fbx_u64 unum;
    fbx_u64 uden;
    fbx_u64 limit;
    fbx_u64 q;

    if (den == 0) return FBX_REAL_ZERO;
    sign = 0;
    if (num < 0) sign = !sign;
    if (den < 0) sign = !sign;
    unum = fbx_abs_i64_to_u64(num);
    uden = fbx_abs_i64_to_u64(den);
    limit = sign ? ((fbx_u64)FBX_REAL_MAX + (fbx_u64)1u) : (fbx_u64)FBX_REAL_MAX;
    q = fbx_div_q16_abs(unum, uden, limit);
    return fbx_real_apply_sign_u64(q, sign);
}

fbx_i32 fbx_real_to_int_round(fbx_real v)
{
    if (v >= 0) return (fbx_i32)((v + FBX_REAL_HALF) >> FBX_REAL_SHIFT);
    {
        fbx_i64 nv = -((fbx_i64)v);
        return (fbx_i32)(-((nv + (fbx_i64)FBX_REAL_HALF) >> FBX_REAL_SHIFT));
    }
}

fbx_real fbx_real_abs(fbx_real v)
{
    if (v == FBX_REAL_MIN) return FBX_REAL_MAX;
    return v < 0 ? -v : v;
}

fbx_real fbx_real_mul(fbx_real a, fbx_real b)
{
    return fbx_real_saturate_i64(((fbx_i64)a * (fbx_i64)b) >> FBX_REAL_SHIFT);
}

fbx_real fbx_real_div(fbx_real a, fbx_real b)
{
    int sign;
    fbx_u64 ua;
    fbx_u64 ub;
    fbx_u64 limit;
    fbx_u64 q;

    if (b == 0) return a < 0 ? FBX_REAL_MIN : FBX_REAL_MAX;
    sign = 0;
    if (a < 0) sign = !sign;
    if (b < 0) sign = !sign;
    ua = fbx_abs_i64_to_u64((fbx_i64)a);
    ub = fbx_abs_i64_to_u64((fbx_i64)b);
    limit = sign ? ((fbx_u64)FBX_REAL_MAX + (fbx_u64)1u) : (fbx_u64)FBX_REAL_MAX;
    q = fbx_div_q16_abs(ua, ub, limit);
    return fbx_real_apply_sign_u64(q, sign);
}

static fbx_u64 fbx_isqrt_u64(fbx_u64 x)
{
    fbx_u64 op;
    fbx_u64 res;
    fbx_u64 one;

    op = x;
    res = 0u;
    one = ((fbx_u64)1u) << 62;
    while (one > op) one >>= 2;
    while (one != 0u) {
        if (op >= res + one) {
            op -= res + one;
            res = res + (one << 1);
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

fbx_real fbx_real_sqrt(fbx_real v)
{
    fbx_u64 x;
    if (v <= 0) return FBX_REAL_ZERO;
    x = ((fbx_u64)(fbx_u32)v) << FBX_REAL_SHIFT;
    return (fbx_real)fbx_isqrt_u64(x);
}

fbx_real fbx_real_deg_to_rad(fbx_real deg)
{
    return fbx_real_mul(deg, FBX_REAL_DEG2RAD);
}

static fbx_real fbx_real_wrap_pi(fbx_real x)
{
    while (x > FBX_REAL_PI) x -= FBX_REAL_TWO_PI;
    while (x < -FBX_REAL_PI) x += FBX_REAL_TWO_PI;
    return x;
}

fbx_real fbx_real_sin_rad(fbx_real rad)
{
    fbx_real x;
    fbx_real x2;
    fbx_real x3;
    fbx_real x5;
    fbx_real x7;
    fbx_real term;

    x = fbx_real_wrap_pi(rad);
    x2 = fbx_real_mul(x, x);
    x3 = fbx_real_mul(x2, x);
    x5 = fbx_real_mul(x3, x2);
    x7 = fbx_real_mul(x5, x2);

    term = x;
    term -= fbx_real_div(x3, fbx_real_from_int(6));
    term += fbx_real_div(x5, fbx_real_from_int(120));
    term -= fbx_real_div(x7, fbx_real_from_int(5040));
    return term;
}

fbx_real fbx_real_cos_rad(fbx_real rad)
{
    return fbx_real_sin_rad(rad + FBX_REAL_HALF_PI);
}

fbx_real fbx_real_tan_rad(fbx_real rad)
{
    return fbx_real_div(fbx_real_sin_rad(rad), fbx_real_cos_rad(rad));
}

static fbx_real fbx_real_shift_to_q16(fbx_u64 mant, int shift, int sign)
{
    fbx_u64 u;
    fbx_i64 s;

    if (shift >= 0) {
        if (shift >= 63) return sign ? FBX_REAL_MIN : FBX_REAL_MAX;
        if (mant > (((fbx_u64)FBX_REAL_MAX) >> shift)) {
            return sign ? FBX_REAL_MIN : FBX_REAL_MAX;
        }
        u = mant << shift;
    } else {
        int rshift;
        rshift = -shift;
        if (rshift >= 64) u = 0u;
        else u = mant >> rshift;
    }

    if (u > (fbx_u64)FBX_REAL_MAX) return sign ? FBX_REAL_MIN : FBX_REAL_MAX;
    s = (fbx_i64)u;
    if (sign) s = -s;
    return (fbx_real)s;
}

fbx_real fbx_real_from_ieee32_bits(fbx_u32 bits)
{
    int sign;
    int exp;
    fbx_u32 frac;
    fbx_u64 mant;
    int shift;

    sign = (int)((bits >> 31) & 1u);
    exp = (int)((bits >> 23) & 0xffu);
    frac = bits & 0x7fffffu;

    if (exp == 255) return sign ? FBX_REAL_MIN : FBX_REAL_MAX;
    if (exp == 0) {
        if (frac == 0u) return FBX_REAL_ZERO;
        mant = (fbx_u64)frac;
        shift = -126 - 23 + FBX_REAL_SHIFT;
    } else {
        mant = (fbx_u64)(frac | 0x800000u);
        shift = exp - 127 - 23 + FBX_REAL_SHIFT;
    }
    return fbx_real_shift_to_q16(mant, shift, sign);
}

fbx_real fbx_real_from_ieee64_bits(fbx_u64 bits)
{
    int sign;
    int exp;
    fbx_u64 frac;
    fbx_u64 mant;
    int shift;

    sign = (int)((bits >> 63) & 1u);
    exp = (int)((bits >> 52) & 0x7ffu);
    frac = bits & ((((fbx_u64)1u) << 52) - 1u);

    if (exp == 2047) return sign ? FBX_REAL_MIN : FBX_REAL_MAX;
    if (exp == 0) {
        if (frac == 0u) return FBX_REAL_ZERO;
        mant = frac;
        shift = -1022 - 52 + FBX_REAL_SHIFT;
    } else {
        mant = frac | (((fbx_u64)1u) << 52);
        shift = exp - 1023 - 52 + FBX_REAL_SHIFT;
    }
    return fbx_real_shift_to_q16(mant, shift, sign);
}

static int fbx_ascii_lower_char(int c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int fbx_ascii_contains_word_ci(const char *s, const char *word)
{
    const char *p;
    if (!s || !word || !word[0]) return 0;
    for (p = s; *p; ++p) {
        const char *a;
        const char *b;
        a = p;
        b = word;
        while (*a && *b && fbx_ascii_lower_char((unsigned char)*a) == fbx_ascii_lower_char((unsigned char)*b)) {
            ++a;
            ++b;
        }
        if (!*b) return 1;
    }
    return 0;
}

fbx_real fbx_real_from_ascii(const char *text)
{
    int sign;
    fbx_i64 ip;
    fbx_i64 fp;
    fbx_i64 scale;
    fbx_real out;
    int exp_sign;
    int exp_value;
    const char *orig;

    if (!text) return FBX_REAL_ZERO;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;

    sign = 0;
    if (*text == '-') { sign = 1; text++; }
    else if (*text == '+') text++;
    orig = text;

    /* FBX ASCII may contain SDK spellings such as 1.#INF, -1.#INF,
       1.#NAN, NaN, Infinity.  The fixed-point core cannot preserve IEEE
       NaN payloads, so we follow a deterministic import policy: NaN maps to
       zero, infinities saturate by sign.  This keeps malformed authoring data
       parseable without introducing C floating-point types. */
    if (fbx_ascii_contains_word_ci(orig, "nan")) return FBX_REAL_ZERO;
    if (fbx_ascii_contains_word_ci(orig, "ind")) return FBX_REAL_ZERO;
    if (fbx_ascii_contains_word_ci(orig, "inf")) return sign ? FBX_REAL_MIN : FBX_REAL_MAX;

    ip = 0;
    while (*text >= '0' && *text <= '9') {
        if (ip < (fbx_i64)1000000000) ip = ip * 10 + (fbx_i64)(*text - '0');
        text++;
    }

    fp = 0;
    scale = 1;
    if (*text == '.') {
        text++;
        while (*text >= '0' && *text <= '9' && scale < (fbx_i64)1000000000) {
            fp = fp * 10 + (fbx_i64)(*text - '0');
            scale *= 10;
            text++;
        }
        while (*text >= '0' && *text <= '9') text++;
    }

    out = fbx_real_from_i64(ip);
    if (fp != 0 && scale > 1) out += fbx_real_from_ratio(fp, scale);

    exp_sign = 0;
    exp_value = 0;
    if (*text == 'e' || *text == 'E') {
        text++;
        if (*text == '-') { exp_sign = 1; text++; }
        else if (*text == '+') text++;
        while (*text >= '0' && *text <= '9' && exp_value < 9) {
            exp_value = exp_value * 10 + (int)(*text - '0');
            text++;
        }
        while (exp_value > 0) {
            if (exp_sign) out = fbx_real_div(out, fbx_real_from_int(10));
            else out = fbx_real_mul(out, fbx_real_from_int(10));
            exp_value--;
        }
    }

    return sign ? -out : out;
}


static int fbx_u32_highest_bit(fbx_u32 v)
{
    int b;
    b = -1;
    while (v != 0u) {
        v >>= 1;
        b++;
    }
    return b;
}

fbx_u32 fbx_real_to_ieee32_bits(fbx_real v)
{
    fbx_u32 sign;
    fbx_u32 av;
    int hb;
    int exp;
    fbx_u32 sig;
    fbx_u32 mant;
    int shift;

    if (v == FBX_REAL_ZERO) return 0u;
    sign = 0u;
    if (v < 0) {
        sign = 0x80000000u;
        av = (fbx_u32)(~(fbx_u32)v) + 1u;
    } else {
        av = (fbx_u32)v;
    }
    hb = fbx_u32_highest_bit(av);
    if (hb < 0) return sign;
    exp = hb - FBX_REAL_SHIFT + 127;
    if (exp <= 0) return sign;
    if (exp >= 255) return sign | 0x7f800000u;
    shift = 23 - hb;
    if (shift >= 0) sig = av << shift;
    else sig = av >> (-shift);
    mant = sig & 0x007fffffu;
    return sign | ((fbx_u32)exp << 23) | mant;
}

fbx_u64 fbx_real_to_ieee64_bits(fbx_real v)
{
    fbx_u64 sign;
    fbx_u32 av32;
    fbx_u64 av;
    int hb;
    int exp;
    fbx_u64 sig;
    fbx_u64 mant;
    int shift;

    if (v == FBX_REAL_ZERO) return (fbx_u64)0u;
    sign = (fbx_u64)0u;
    if (v < 0) {
        sign = (fbx_u64)1u << 63;
        av32 = (fbx_u32)(~(fbx_u32)v) + 1u;
    } else {
        av32 = (fbx_u32)v;
    }
    av = (fbx_u64)av32;
    hb = fbx_u32_highest_bit(av32);
    if (hb < 0) return sign;
    exp = hb - FBX_REAL_SHIFT + 1023;
    if (exp <= 0) return sign;
    if (exp >= 2047) return sign | ((fbx_u64)0x7ffu << 52);
    shift = 52 - hb;
    if (shift >= 0) sig = av << shift;
    else sig = av >> (-shift);
    mant = sig & (((fbx_u64)1u << 52) - 1u);
    return sign | ((fbx_u64)exp << 52) | mant;
}
