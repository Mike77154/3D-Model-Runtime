
#include <ctype.h>
#include <string.h>
#include "vr1_fixed.h"

static const vr1_fx vr1_sin_deg_0_90[91] = {
    0L, 71L, 143L, 214L, 286L, 357L, 428L, 499L, 570L, 641L, 711L, 782L, 852L, 921L, 991L, 1060L, 1129L, 1198L, 1266L, 1334L, 1401L, 1468L, 1534L, 1600L, 1666L, 1731L, 1796L, 1860L, 1923L, 1986L, 2048L, 2110L, 2171L, 2231L, 2290L, 2349L, 2408L, 2465L, 2522L, 2578L, 2633L, 2687L, 2741L, 2793L, 2845L, 2896L, 2946L, 2996L, 3044L, 3091L, 3138L, 3183L, 3228L, 3271L, 3314L, 3355L, 3396L, 3435L, 3474L, 3511L, 3547L, 3582L, 3617L, 3650L, 3681L, 3712L, 3742L, 3770L, 3798L, 3824L, 3849L, 3873L, 3896L, 3917L, 3937L, 3956L, 3974L, 3991L, 4006L, 4021L, 4034L, 4046L, 4056L, 4065L, 4074L, 4080L, 4086L, 4090L, 4094L, 4095L, 4096L
};

vr1_fx vr1_fx_from_int(long v)
{
    if (v > (VR1_FX_MAX / VR1_FX_ONE)) return VR1_FX_MAX;
    if (v < (VR1_FX_MIN / VR1_FX_ONE)) return VR1_FX_MIN;
    return (vr1_fx)(v * VR1_FX_ONE);
}

long vr1_fx_to_int(vr1_fx v)
{
    return v / VR1_FX_ONE;
}

vr1_fx vr1_fx_abs(vr1_fx v)
{
    if (v < 0) return -v;
    return v;
}

vr1_fx vr1_fx_clamp_long(long v)
{
    if (v > VR1_FX_MAX) return VR1_FX_MAX;
    if (v < VR1_FX_MIN) return VR1_FX_MIN;
    return (vr1_fx)v;
}

static vr1_fx vr1_fx_mul_abs(vr1_fx a, vr1_fx b)
{
    long aq;
    long ar;
    long bq;
    long br;
    long out;
    long term;

    aq = a / VR1_FX_ONE;
    ar = a % VR1_FX_ONE;
    bq = b / VR1_FX_ONE;
    br = b % VR1_FX_ONE;

    out = 0;
    if (aq != 0 && bq != 0) {
        if (aq > (VR1_FX_MAX / VR1_FX_ONE) / bq) return VR1_FX_MAX;
        out += aq * bq * VR1_FX_ONE;
    }
    term = aq * br;
    if (term > VR1_FX_MAX - out) return VR1_FX_MAX;
    out += term;
    term = bq * ar;
    if (term > VR1_FX_MAX - out) return VR1_FX_MAX;
    out += term;
    term = (ar * br) / VR1_FX_ONE;
    if (term > VR1_FX_MAX - out) return VR1_FX_MAX;
    out += term;
    return (vr1_fx)out;
}

vr1_fx vr1_fx_mul(vr1_fx a, vr1_fx b)
{
    int neg;
    vr1_fx ua;
    vr1_fx ub;
    vr1_fx out;

    neg = 0;
    if (a < 0) { neg = !neg; ua = -a; } else { ua = a; }
    if (b < 0) { neg = !neg; ub = -b; } else { ub = b; }
    out = vr1_fx_mul_abs(ua, ub);
    if (neg) out = -out;
    return out;
}

vr1_fx vr1_fx_div(vr1_fx a, vr1_fx b)
{
    int neg;
    vr1_fx ua;
    vr1_fx ub;
    long q;
    long r;
    long out;

    if (b == 0) {
        if (a < 0) return VR1_FX_MIN;
        return VR1_FX_MAX;
    }
    neg = 0;
    if (a < 0) { neg = !neg; ua = -a; } else { ua = a; }
    if (b < 0) { neg = !neg; ub = -b; } else { ub = b; }

    q = ua / ub;
    r = ua % ub;
    if (q > VR1_FX_MAX / VR1_FX_ONE) out = VR1_FX_MAX;
    else out = q * VR1_FX_ONE + (r * VR1_FX_ONE) / ub;
    if (out > VR1_FX_MAX) out = VR1_FX_MAX;
    if (neg) out = -out;
    return (vr1_fx)out;
}

vr1_fx vr1_fx_sqrt(vr1_fx v)
{
    long x;
    long bit;
    long res;

    if (v <= 0) return 0;
    x = v;
    bit = 1L << 30;
    res = 0;
    while (bit > x) bit >>= 2;
    while (bit != 0) {
        if (x >= res + bit) {
            x -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (vr1_fx)(res << (VR1_FX_SHIFT / 2));
}

static int vr1_angle_to_deg(vr1_fx radians)
{
    long a;
    int deg;

    a = radians % VR1_FX_TWO_PI;
    if (a < 0) a += VR1_FX_TWO_PI;
    deg = (int)((a * 180L + (VR1_FX_PI / 2)) / VR1_FX_PI);
    while (deg >= 360) deg -= 360;
    while (deg < 0) deg += 360;
    return deg;
}

vr1_fx vr1_fx_sin(vr1_fx radians)
{
    int deg;
    int q;
    int d;
    vr1_fx v;

    deg = vr1_angle_to_deg(radians);
    q = deg / 90;
    d = deg % 90;
    if (q == 0) v = vr1_sin_deg_0_90[d];
    else if (q == 1) v = vr1_sin_deg_0_90[90 - d];
    else if (q == 2) v = -vr1_sin_deg_0_90[d];
    else v = -vr1_sin_deg_0_90[90 - d];
    return v;
}

vr1_fx vr1_fx_cos(vr1_fx radians)
{
    return vr1_fx_sin(radians + VR1_FX_HALF_PI);
}

vr1_fx vr1_fx_from_ascii(const char *text, int len, int *ok)
{
    int i;
    int sign;
    long int_part;
    long out;
    long frac_place;
    int exp_sign;
    int exp_val;
    int saw_digit;

    if (ok) *ok = 0;
    if (text == 0 || len <= 0) return 0;

    i = 0;
    sign = 1;
    if (text[i] == '-') { sign = -1; i++; }
    else if (text[i] == '+') { i++; }

    int_part = 0;
    saw_digit = 0;
    while (i < len && isdigit((unsigned char)text[i])) {
        saw_digit = 1;
        if (int_part < 100000L) int_part = int_part * 10L + (long)(text[i] - '0');
        i++;
    }
    if (int_part > (VR1_FX_MAX / VR1_FX_ONE)) out = VR1_FX_MAX;
    else out = int_part * VR1_FX_ONE;

    if (i < len && text[i] == '.') {
        i++;
        frac_place = VR1_FX_ONE / 10L;
        while (i < len && isdigit((unsigned char)text[i])) {
            saw_digit = 1;
            if (frac_place > 0 && out < VR1_FX_MAX) {
                out += (long)(text[i] - '0') * frac_place;
            }
            frac_place /= 10L;
            i++;
        }
    }

    exp_sign = 1;
    exp_val = 0;
    if (i < len && (text[i] == 'e' || text[i] == 'E')) {
        i++;
        if (i < len && text[i] == '-') { exp_sign = -1; i++; }
        else if (i < len && text[i] == '+') { i++; }
        while (i < len && isdigit((unsigned char)text[i])) {
            if (exp_val < 32) exp_val = exp_val * 10 + (text[i] - '0');
            i++;
        }
        while (exp_val > 0) {
            if (exp_sign > 0) {
                if (out > VR1_FX_MAX / 10L) out = VR1_FX_MAX;
                else out *= 10L;
            } else {
                out /= 10L;
            }
            exp_val--;
        }
    }

    if (!saw_digit) return 0;
    if (sign < 0) out = -out;
    if (ok) *ok = 1;
    return vr1_fx_clamp_long(out);
}

void vr1_fx_to_ascii(vr1_fx v, char *out, int out_cap, int decimals)
{
    long ip;
    long rem;
    long scale10;
    long fp;
    int neg;
    int i;
    char tmp[32];
    char *p;

    if (out == 0 || out_cap <= 0) return;
    out[0] = '\0';
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;
    neg = 0;
    if (v < 0) { neg = 1; v = -v; }
    ip = v / VR1_FX_ONE;
    rem = v % VR1_FX_ONE;
    scale10 = 1;
    for (i = 0; i < decimals; i++) scale10 *= 10L;
    fp = (rem * scale10 + VR1_FX_HALF) / VR1_FX_ONE;
    if (fp >= scale10 && decimals > 0) { ip++; fp -= scale10; }

    p = tmp;
    if (neg) { *p++ = '-'; }
    {
        char buf[20];
        int n;
        int j;
        n = 0;
        if (ip == 0) buf[n++] = '0';
        while (ip > 0 && n < 19) {
            buf[n++] = (char)('0' + (ip % 10L));
            ip /= 10L;
        }
        for (j = n - 1; j >= 0; j--) *p++ = buf[j];
    }
    if (decimals > 0) {
        char fbuf[10];
        int j;
        *p++ = '.';
        for (j = decimals - 1; j >= 0; j--) {
            fbuf[j] = (char)('0' + (fp % 10L));
            fp /= 10L;
        }
        for (j = 0; j < decimals; j++) *p++ = fbuf[j];
    }
    *p = '\0';
    strncpy(out, tmp, (size_t)(out_cap - 1));
    out[out_cap - 1] = '\0';
}
