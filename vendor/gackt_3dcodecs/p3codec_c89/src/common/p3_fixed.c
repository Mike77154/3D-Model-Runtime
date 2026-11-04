#include <stdio.h>
#include <string.h>
#include "p3_fixed.h"

static p3_u32 p3_abs32(p3_s32 v) {
    if (v < 0) {
        return (p3_u32)(0 - v);
    }
    return (p3_u32)v;
}

p3_fixed p3_fixed_from_int(int v) {
    return (p3_fixed)((p3_s32)v * (p3_s32)P3_FIXED_ONE);
}

int p3_fixed_to_int(p3_fixed v) {
    if (v >= 0) {
        return (int)(v / P3_FIXED_ONE);
    }
    return (int)(-(((-v) + P3_FIXED_ONE - 1) / P3_FIXED_ONE));
}

int p3_fixed_from_str(const char *text, p3_fixed *out_value) {
    int sign;
    p3_u32 ipart;
    p3_u32 frac;
    p3_u32 scale;
    p3_u32 frac_fixed;
    const char *p;

    if (text == 0 || out_value == 0) {
        return -1;
    }

    sign = 1;
    p = text;
    if (*p == '-') {
        sign = -1;
        ++p;
    } else if (*p == '+') {
        ++p;
    }

    if (*p == 0) {
        return -1;
    }

    ipart = 0;
    while (*p >= '0' && *p <= '9') {
        p3_u32 digit;
        digit = (p3_u32)(*p - '0');
        if (ipart > 32767UL) {
            return -1;
        }
        ipart = (ipart * 10UL) + digit;
        ++p;
    }

    frac = 0;
    scale = 1;
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') {
            p3_u32 digit;
            if (scale < 1000000UL) {
                digit = (p3_u32)(*p - '0');
                frac = (frac * 10UL) + digit;
                scale *= 10UL;
            }
            ++p;
        }
    }

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        ++p;
    }
    if (*p != 0) {
        return -1;
    }

    if (ipart > 32767UL) {
        return -1;
    }

    frac_fixed = 0;
    if (scale > 1UL) {
        frac_fixed = (frac * (p3_u32)P3_FIXED_ONE + (scale / 2UL)) / scale;
    }

    *out_value = (p3_fixed)((ipart << 16) + frac_fixed);
    if (sign < 0) {
        *out_value = (p3_fixed)(0 - *out_value);
    }
    return 0;
}

void p3_fixed_to_str(p3_fixed value, char *dst) {
    p3_u32 abs_value;
    p3_u32 whole;
    p3_u32 frac;
    p3_u32 digits;

    if (dst == 0) {
        return;
    }

    abs_value = p3_abs32(value);
    whole = abs_value >> 16;
    frac = abs_value & 0xffffUL;
    digits = (frac * 10000UL + 32768UL) >> 16;
    if (digits >= 10000UL) {
        whole += 1UL;
        digits = 0UL;
    }

    if (value < 0) {
        sprintf(dst, "-%lu.%04lu", (unsigned long)whole, (unsigned long)digits);
    } else {
        sprintf(dst, "%lu.%04lu", (unsigned long)whole, (unsigned long)digits);
    }
}

p3_fixed p3_fixed_mul(p3_fixed a, p3_fixed b) {
    int sign;
    p3_u32 ua;
    p3_u32 ub;
    p3_u32 ahi;
    p3_u32 alo;
    p3_u32 bhi;
    p3_u32 blo;
    p3_u32 term0;
    p3_u32 term1;
    p3_u32 term2;
    p3_u32 term3;
    p3_u32 outv;

    sign = 1;
    if (a < 0) {
        sign = -sign;
        ua = (p3_u32)(0 - a);
    } else {
        ua = (p3_u32)a;
    }
    if (b < 0) {
        sign = -sign;
        ub = (p3_u32)(0 - b);
    } else {
        ub = (p3_u32)b;
    }

    ahi = ua >> 16;
    alo = ua & 0xffffUL;
    bhi = ub >> 16;
    blo = ub & 0xffffUL;

    term0 = (ahi * bhi) << 16;
    term1 = ahi * blo;
    term2 = bhi * alo;
    term3 = (alo * blo) >> 16;

    outv = term0 + term1 + term2 + term3;
    if (sign < 0) {
        return (p3_fixed)(0 - (p3_s32)outv);
    }
    return (p3_fixed)outv;
}

#define P3_FIXED_DEG90  ((p3_fixed)5898240L)
#define P3_FIXED_DEG360 ((p3_fixed)23592960L)

static const p3_fixed p3_fixed_sin90_table[91] = {
    0,
    1144,
    2287,
    3430,
    4572,
    5712,
    6850,
    7987,
    9121,
    10252,
    11380,
    12505,
    13626,
    14742,
    15855,
    16962,
    18064,
    19161,
    20252,
    21336,
    22415,
    23486,
    24550,
    25607,
    26656,
    27697,
    28729,
    29753,
    30767,
    31772,
    32768,
    33754,
    34729,
    35693,
    36647,
    37590,
    38521,
    39441,
    40348,
    41243,
    42126,
    42995,
    43852,
    44695,
    45525,
    46341,
    47143,
    47930,
    48703,
    49461,
    50203,
    50931,
    51643,
    52339,
    53020,
    53684,
    54332,
    54963,
    55578,
    56175,
    56756,
    57319,
    57865,
    58393,
    58903,
    59396,
    59870,
    60326,
    60764,
    61183,
    61584,
    61966,
    62328,
    62672,
    62997,
    63303,
    63589,
    63856,
    64104,
    64332,
    64540,
    64729,
    64898,
    65048,
    65177,
    65287,
    65376,
    65446,
    65496,
    65526,
    65536
};

static p3_fixed p3_fixed_norm_deg(p3_fixed angle_deg) {
    p3_s32 a;
    a = (p3_s32)(angle_deg % P3_FIXED_DEG360);
    if (a < 0) {
        a = (p3_s32)(a + P3_FIXED_DEG360);
    }
    return (p3_fixed)a;
}

static p3_fixed p3_fixed_sin_0_90(p3_fixed angle_deg) {
    p3_u16 whole;
    p3_u32 frac;
    p3_s32 base;
    p3_s32 next;
    p3_s32 delta;
    p3_s32 interp;
    if (angle_deg <= 0) {
        return 0;
    }
    if (angle_deg >= P3_FIXED_DEG90) {
        return P3_FIXED_ONE;
    }
    whole = (p3_u16)((p3_u32)angle_deg >> 16);
    frac = (p3_u32)angle_deg & 0xffffUL;
    base = p3_fixed_sin90_table[whole];
    next = p3_fixed_sin90_table[(p3_u16)(whole + 1U)];
    delta = (p3_s32)(next - base);
    interp = (p3_s32)((delta * (p3_s32)frac + 32768L) >> 16);
    return (p3_fixed)(base + interp);
}

static p3_fixed p3_fixed_sin_norm(p3_fixed angle_deg) {
    p3_u16 quadrant;
    p3_fixed rem;
    quadrant = (p3_u16)(angle_deg / P3_FIXED_DEG90);
    rem = (p3_fixed)(angle_deg - (p3_fixed)((p3_s32)quadrant * P3_FIXED_DEG90));
    if (quadrant == 0U) {
        return p3_fixed_sin_0_90(rem);
    }
    if (quadrant == 1U) {
        return p3_fixed_sin_0_90((p3_fixed)(P3_FIXED_DEG90 - rem));
    }
    if (quadrant == 2U) {
        return (p3_fixed)(0 - p3_fixed_sin_0_90(rem));
    }
    return (p3_fixed)(0 - p3_fixed_sin_0_90((p3_fixed)(P3_FIXED_DEG90 - rem)));
}

p3_fixed p3_fixed_sin_deg(p3_fixed angle_deg) {
    return p3_fixed_sin_norm(p3_fixed_norm_deg(angle_deg));
}

p3_fixed p3_fixed_cos_deg(p3_fixed angle_deg) {
    p3_fixed a;
    a = p3_fixed_norm_deg(angle_deg);
    a = (p3_fixed)(a + P3_FIXED_DEG90);
    if (a >= P3_FIXED_DEG360) {
        a = (p3_fixed)(a - P3_FIXED_DEG360);
    }
    return p3_fixed_sin_norm(a);
}
