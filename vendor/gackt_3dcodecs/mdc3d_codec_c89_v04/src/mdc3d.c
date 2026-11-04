#include "mdc3d.h"

#define MDC3D_HDR_SIZE 112UL
#define MDC3D_FRAME_SIZE 56UL
#define MDC3D_TAG_NAME_SIZE 64UL
#define MDC3D_TAG_FRAME_SIZE 12UL
#define MDC3D_SURF_HDR_SIZE 128UL
#define MDC3D_TRI_SIZE 12UL
#define MDC3D_SHADER_SIZE 68UL
#define MDC3D_TEX_SIZE 8UL
#define MDC3D_BASEVERT_SIZE 8UL
#define MDC3D_COMPVERT_SIZE 4UL

static const MDC3D_S16 mdc3d_sin256[256] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804,
    0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804,
};
static const MDC3D_S16 mdc3d_cos256[256] = {
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804,
    0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804,
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
};
static const MDC3D_S16 mdc3d_comp_norm256[256][3] = {
    { 32767, 0, 0 },
    { 32137, 6393, 0 },
    { 30273, 12539, 0 },
    { 27245, 18204, 0 },
    { 23170, 23170, 0 },
    { 18204, 27245, 0 },
    { 12539, 30273, 0 },
    { 6393, 32137, 0 },
    { 0, 32767, 0 },
    { -6393, 32137, 0 },
    { -12539, 30273, 0 },
    { -18204, 27245, 0 },
    { -23170, 23170, 0 },
    { -27245, 18204, 0 },
    { -30273, 12539, 0 },
    { -32137, 6393, 0 },
    { -32767, 0, 0 },
    { -32137, -6393, 0 },
    { -30273, -12539, 0 },
    { -27245, -18204, 0 },
    { -23170, -23170, 0 },
    { -18204, -27245, 0 },
    { -12539, -30273, 0 },
    { -6393, -32137, 0 },
    { 0, -32767, 0 },
    { 6393, -32137, 0 },
    { 12539, -30273, 0 },
    { 18204, -27245, 0 },
    { 23170, -23170, 0 },
    { 27245, -18204, 0 },
    { 30273, -12539, 0 },
    { 32137, -6393, 0 },
    { 32137, 0, -6393 },
    { 31332, 7151, -6393 },
    { 28955, 13944, -6393 },
    { 25126, 20037, -6393 },
    { 20037, 25126, -6393 },
    { 13944, 28955, -6393 },
    { 7151, 31332, -6393 },
    { 0, 32137, -6393 },
    { -7151, 31332, -6393 },
    { -13944, 28955, -6393 },
    { -20037, 25126, -6393 },
    { -25126, 20037, -6393 },
    { -28955, 13944, -6393 },
    { -31332, 7151, -6393 },
    { -32137, 0, -6393 },
    { -31332, -7151, -6393 },
    { -28955, -13944, -6393 },
    { -25126, -20037, -6393 },
    { -20037, -25126, -6393 },
    { -13944, -28955, -6393 },
    { -7151, -31332, -6393 },
    { 0, -32137, -6393 },
    { 7151, -31332, -6393 },
    { 13944, -28955, -6393 },
    { 20037, -25126, -6393 },
    { 25126, -20037, -6393 },
    { 28955, -13944, -6393 },
    { 31332, -7151, -6393 },
    { 30273, 0, -12539 },
    { 29241, 7835, -12539 },
    { 26217, 15136, -12539 },
    { 21406, 21406, -12539 },
    { 15136, 26217, -12539 },
    { 7835, 29241, -12539 },
    { 0, 30273, -12539 },
    { -7835, 29241, -12539 },
    { -15136, 26217, -12539 },
    { -21406, 21406, -12539 },
    { -26217, 15136, -12539 },
    { -29241, 7835, -12539 },
    { -30273, 0, -12539 },
    { -29241, -7835, -12539 },
    { -26217, -15136, -12539 },
    { -21406, -21406, -12539 },
    { -15136, -26217, -12539 },
    { -7835, -29241, -12539 },
    { 0, -30273, -12539 },
    { 7835, -29241, -12539 },
    { 15136, -26217, -12539 },
    { 21406, -21406, -12539 },
    { 26217, -15136, -12539 },
    { 29241, -7835, -12539 },
    { 27245, 0, -18204 },
    { 25911, 8419, -18204 },
    { 22041, 16014, -18204 },
    { 16014, 22041, -18204 },
    { 8419, 25911, -18204 },
    { 0, 27245, -18204 },
    { -8419, 25911, -18204 },
    { -16014, 22041, -18204 },
    { -22041, 16014, -18204 },
    { -25911, 8419, -18204 },
    { -27245, 0, -18204 },
    { -25911, -8419, -18204 },
    { -22041, -16014, -18204 },
    { -16014, -22041, -18204 },
    { -8419, -25911, -18204 },
    { 0, -27245, -18204 },
    { 8419, -25911, -18204 },
    { 16014, -22041, -18204 },
    { 22041, -16014, -18204 },
    { 25911, -8419, -18204 },
    { 23170, 0, -23170 },
    { 21406, 8867, -23170 },
    { 16384, 16384, -23170 },
    { 8867, 21406, -23170 },
    { 0, 23170, -23170 },
    { -8867, 21406, -23170 },
    { -16384, 16384, -23170 },
    { -21406, 8867, -23170 },
    { -23170, 0, -23170 },
    { -21406, -8867, -23170 },
    { -16384, -16384, -23170 },
    { -8867, -21406, -23170 },
    { 0, -23170, -23170 },
    { 8867, -21406, -23170 },
    { 16383, -16384, -23170 },
    { 21406, -8867, -23170 },
    { 18204, 0, -27245 },
    { 15765, 9102, -27245 },
    { 9102, 15765, -27245 },
    { 0, 18204, -27245 },
    { -9102, 15765, -27245 },
    { -15765, 9102, -27245 },
    { -18204, 0, -27245 },
    { -15765, -9102, -27245 },
    { -9102, -15765, -27245 },
    { 0, -18204, -27245 },
    { 9102, -15765, -27245 },
    { 15765, -9102, -27245 },
    { 12539, 0, -30273 },
    { 8867, 8867, -30273 },
    { 0, 12539, -30273 },
    { -8867, 8867, -30273 },
    { -12539, 0, -30273 },
    { -8867, -8867, -30273 },
    { 0, -12539, -30273 },
    { 8867, -8867, -30273 },
    { 6393, 0, -32137 },
    { 0, 6393, -32137 },
    { -6393, 0, -32137 },
    { 0, -6393, -32137 },
    { 32137, 0, 6393 },
    { 31332, 7151, 6393 },
    { 28955, 13944, 6393 },
    { 25126, 20037, 6393 },
    { 20037, 25126, 6393 },
    { 13944, 28955, 6393 },
    { 7151, 31332, 6393 },
    { 0, 32137, 6393 },
    { -7151, 31332, 6393 },
    { -13944, 28955, 6393 },
    { -20037, 25126, 6393 },
    { -25126, 20037, 6393 },
    { -28955, 13944, 6393 },
    { -31332, 7151, 6393 },
    { -32137, 0, 6393 },
    { -31332, -7151, 6393 },
    { -28955, -13944, 6393 },
    { -25126, -20037, 6393 },
    { -20037, -25126, 6393 },
    { -13944, -28955, 6393 },
    { -7151, -31332, 6393 },
    { 0, -32137, 6393 },
    { 7151, -31332, 6393 },
    { 13944, -28955, 6393 },
    { 20037, -25126, 6393 },
    { 25126, -20037, 6393 },
    { 28955, -13944, 6393 },
    { 31332, -7151, 6393 },
    { 30273, 0, 12539 },
    { 29241, 7835, 12539 },
    { 26217, 15136, 12539 },
    { 21406, 21406, 12539 },
    { 15136, 26217, 12539 },
    { 7835, 29241, 12539 },
    { 0, 30273, 12539 },
    { -7835, 29241, 12539 },
    { -15136, 26217, 12539 },
    { -21406, 21406, 12539 },
    { -26217, 15136, 12539 },
    { -29241, 7835, 12539 },
    { -30273, 0, 12539 },
    { -29241, -7835, 12539 },
    { -26217, -15136, 12539 },
    { -21406, -21406, 12539 },
    { -15136, -26217, 12539 },
    { -7835, -29241, 12539 },
    { 0, -30273, 12539 },
    { 7835, -29241, 12539 },
    { 15136, -26217, 12539 },
    { 21406, -21406, 12539 },
    { 26217, -15136, 12539 },
    { 29241, -7835, 12539 },
    { 27245, 0, 18204 },
    { 25911, 8419, 18204 },
    { 22041, 16014, 18204 },
    { 16014, 22041, 18204 },
    { 8419, 25911, 18204 },
    { 0, 27245, 18204 },
    { -8419, 25911, 18204 },
    { -16014, 22041, 18204 },
    { -22041, 16014, 18204 },
    { -25911, 8419, 18204 },
    { -27245, 0, 18204 },
    { -25911, -8419, 18204 },
    { -22041, -16014, 18204 },
    { -16014, -22041, 18204 },
    { -8419, -25911, 18204 },
    { 0, -27245, 18204 },
    { 8419, -25911, 18204 },
    { 16014, -22041, 18204 },
    { 22041, -16014, 18204 },
    { 25911, -8419, 18204 },
    { 23170, 0, 23170 },
    { 21406, 8867, 23170 },
    { 16384, 16383, 23170 },
    { 8867, 21406, 23170 },
    { 0, 23170, 23170 },
    { -8867, 21406, 23170 },
    { -16383, 16384, 23170 },
    { -21406, 8867, 23170 },
    { -23170, 0, 23170 },
    { -21406, -8867, 23170 },
    { -16384, -16383, 23170 },
    { -8867, -21406, 23170 },
    { 0, -23170, 23170 },
    { 8867, -21406, 23170 },
    { 16383, -16384, 23170 },
    { 21406, -8867, 23170 },
    { 18204, 0, 27245 },
    { 15765, 9102, 27245 },
    { 9102, 15765, 27245 },
    { 0, 18204, 27245 },
    { -9102, 15765, 27245 },
    { -15765, 9102, 27245 },
    { -18204, 0, 27245 },
    { -15765, -9102, 27245 },
    { -9102, -15765, 27245 },
    { 0, -18204, 27245 },
    { 9102, -15765, 27245 },
    { 15765, -9102, 27245 },
    { 12539, 0, 30273 },
    { 8867, 8867, 30273 },
    { 0, 12539, 30273 },
    { -8867, 8867, 30273 },
    { -12539, 0, 30273 },
    { -8867, -8867, 30273 },
    { 0, -12539, 30273 },
    { 8867, -8867, 30273 },
    { 6393, 0, 32137 },
    { 0, 6393, 32137 },
    { -6393, 0, 32137 },
    { 0, -6393, 32137 },
};

static MDC3D_U32 mdc3d_u32_le(const MDC3D_U8 *p) {
    return ((MDC3D_U32)p[0]) | ((MDC3D_U32)p[1] << 8) | ((MDC3D_U32)p[2] << 16) | ((MDC3D_U32)p[3] << 24);
}
static MDC3D_U16 mdc3d_u16_le(const MDC3D_U8 *p) {
    return (MDC3D_U16)(((MDC3D_U16)p[0]) | ((MDC3D_U16)p[1] << 8));
}
static MDC3D_S16 mdc3d_s16_le(const MDC3D_U8 *p) { return (MDC3D_S16)mdc3d_u16_le(p); }

static int mdc3d_range_ok(MDC3D_U32 size, MDC3D_U32 off, MDC3D_U32 need) {
    if (off > size) return 0;
    if (need > size) return 0;
    if (off > size - need) return 0;
    return 1;
}
static int mdc3d_mul_ok(MDC3D_U32 a, MDC3D_U32 b, MDC3D_U32 *out) {
    if (a != 0UL && b > (MDC3D_MAX_BYTES / a)) return 0;
    *out = a * b;
    return 1;
}
static int mdc3d_add_ok(MDC3D_U32 a, MDC3D_U32 b, MDC3D_U32 *out) {
    if (b > MDC3D_MAX_BYTES - a) return 0;
    *out = a + b;
    return 1;
}
static void mdc3d_copy_name(char *dst, const MDC3D_U8 *src, MDC3D_U32 n, MDC3D_U32 dstn) {
    MDC3D_U32 i;
    if (dstn == 0UL) return;
    for (i = 0; i + 1UL < dstn && i < n; ++i) {
        dst[i] = (char)src[i];
        if (src[i] == 0U) { return; }
    }
    dst[i] = 0;
}

/* IEEE754 binary32 -> Q16.16 without using float/double. Handles normal/subnormal/inf/nan conservatively. */
static mdc3d_fixed mdc3d_f32le_to_fixed(const MDC3D_U8 *p) {
    MDC3D_U32 u, frac, mant;
    MDC3D_S32 sign, exp, shift;
    MDC3D_U32 val;
    u = mdc3d_u32_le(p);
    sign = (u & 0x80000000UL) ? -1L : 1L;
    exp = (MDC3D_S32)((u >> 23) & 255UL);
    frac = u & 0x7FFFFFUL;
    if (exp == 255L) {
        return (sign < 0L) ? (MDC3D_S32)0x80000000UL : (MDC3D_S32)0x7FFFFFFFUL;
    }
    if (exp == 0L) {
        /* Subnormals are far below one Q16.16 step in normal model data. */
        return 0L;
    }
    mant = frac | 0x800000UL;
    /* fixed = mant * 2^(exp-127) / 2^23 * 2^16 = mant * 2^(exp-134) */
    shift = exp - 134L;
    if (shift >= 8L) {
        return (sign < 0L) ? (MDC3D_S32)0x80000000UL : (MDC3D_S32)0x7FFFFFFFUL;
    }
    if (shift >= 0L) val = mant << shift;
    else {
        MDC3D_S32 rshift = -shift;
        if (rshift >= 32L) val = 0UL;
        else val = mant >> rshift;
    }
    if (sign < 0L) return -(MDC3D_S32)val;
    return (MDC3D_S32)val;
}

static MDC3D_U32 mdc3d_isqrt_u32(MDC3D_U32 x) {
    MDC3D_U32 op, res, one;
    op = x; res = 0UL; one = 1UL << 30;
    while (one > op) one >>= 2;
    while (one != 0UL) {
        if (op >= res + one) { op -= res + one; res = res + (one << 1); }
        res >>= 1; one >>= 2;
    }
    return res;
}

static MDC3D_S16 mdc3d_clamp_s16(MDC3D_S32 v) {
    if (v > 32767L) return 32767;
    if (v < -32768L) return (MDC3D_S16)-32768;
    return (MDC3D_S16)v;
}

static void mdc3d_base_normal(MDC3D_U16 enc, MDC3D_S16 out[3]) {
    MDC3D_U8 rho, sigma;
    MDC3D_S32 sr, x, y, z;
    rho = (MDC3D_U8)(enc & 255U);
    sigma = (MDC3D_U8)((enc >> 8) & 255U);
    sr = (MDC3D_S32)mdc3d_sin256[rho];
    x = ((MDC3D_S32)mdc3d_cos256[sigma] * sr) >> 15;
    y = ((MDC3D_S32)mdc3d_sin256[sigma] * sr) >> 15;
    z = (MDC3D_S32)mdc3d_cos256[rho];
    out[0] = mdc3d_clamp_s16(x); out[1] = mdc3d_clamp_s16(y); out[2] = mdc3d_clamp_s16(z);
}

static void mdc3d_final_normal(MDC3D_U16 base, MDC3D_U8 has_comp, MDC3D_U8 comp, MDC3D_S16 out[3]) {
    MDC3D_S16 b[3];
    MDC3D_S32 x, y, z, ax, ay, az, len;
    MDC3D_U32 sum;
    mdc3d_base_normal(base, b);
    x = (MDC3D_S32)b[0]; y = (MDC3D_S32)b[1]; z = (MDC3D_S32)b[2];
    if (has_comp) {
        x += (MDC3D_S32)mdc3d_comp_norm256[comp][0];
        y += (MDC3D_S32)mdc3d_comp_norm256[comp][1];
        z += (MDC3D_S32)mdc3d_comp_norm256[comp][2];
    }
    ax = x >> 1; ay = y >> 1; az = z >> 1;
    sum = (MDC3D_U32)(ax * ax) + (MDC3D_U32)(ay * ay) + (MDC3D_U32)(az * az);
    len = (MDC3D_S32)mdc3d_isqrt_u32(sum);
    if (len <= 0L) { out[0] = 0; out[1] = 0; out[2] = 32767; return; }
    /* len is half-scale, so x/(2*len)*32767 */
    out[0] = mdc3d_clamp_s16((x * 16384L) / len);
    out[1] = mdc3d_clamp_s16((y * 16384L) / len);
    out[2] = mdc3d_clamp_s16((z * 16384L) / len);
}

static void mdc3d_normalize_q15(MDC3D_S32 x, MDC3D_S32 y, MDC3D_S32 z, MDC3D_S16 out[3]) {
    MDC3D_S32 ax, ay, az, len;
    MDC3D_U32 sum;
    ax = x >> 1; ay = y >> 1; az = z >> 1;
    sum = (MDC3D_U32)(ax * ax) + (MDC3D_U32)(ay * ay) + (MDC3D_U32)(az * az);
    len = (MDC3D_S32)mdc3d_isqrt_u32(sum);
    if (len <= 0L) { out[0] = 0; out[1] = 0; out[2] = 32767; return; }
    out[0] = mdc3d_clamp_s16((x * 16384L) / len);
    out[1] = mdc3d_clamp_s16((y * 16384L) / len);
    out[2] = mdc3d_clamp_s16((z * 16384L) / len);
}

void mdc3d_decode_base_normal(MDC3D_U16 normal, mdc3d_normal out_normal[3]) {
    if (!out_normal) return;
    mdc3d_base_normal(normal, out_normal);
}

void mdc3d_decode_final_normal(MDC3D_U16 base_normal, MDC3D_U8 has_comp, MDC3D_U8 comp_normal, mdc3d_normal out_normal[3]) {
    if (!out_normal) return;
    mdc3d_final_normal(base_normal, has_comp, comp_normal, out_normal);
}


static MDC3D_S32 mdc3d_q15_mul(MDC3D_S32 a, MDC3D_S32 b) {
    return (MDC3D_S32)((a * b) >> 15);
}

static MDC3D_U8 mdc3d_angle_to_lut(MDC3D_S16 raw) {
    MDC3D_S32 v;
    /* MDC tags use raw degrees scaled by 360/32700. Convert that to 0..255 turns. */
    v = ((MDC3D_S32)raw * 256L) / 32700L;
    while (v < 0L) v += 256L;
    while (v > 255L) v -= 256L;
    return (MDC3D_U8)v;
}

static void mdc3d_build_tag_axis(MDC3D_S16 pitch_raw, MDC3D_S16 yaw_raw, MDC3D_S16 roll_raw, MDC3D_S16 axis[3][3]) {
    MDC3D_U8 pi, yi, ri;
    MDC3D_S32 cp, sp, cy, sy, cr, sr;
    MDC3D_S32 r00, r01, r02, r10, r11, r12, r20, r21, r22;
    MDC3D_S32 t0, t1;
    /* Intrinsic XYZ described by the spec as roll, then pitch, then yaw.
       Matrix used here: Rz(yaw) * Ry(pitch) * Rx(roll), Q15 integer math. */
    pi = mdc3d_angle_to_lut(pitch_raw);
    yi = mdc3d_angle_to_lut(yaw_raw);
    ri = mdc3d_angle_to_lut(roll_raw);
    cp = (MDC3D_S32)mdc3d_cos256[pi]; sp = (MDC3D_S32)mdc3d_sin256[pi];
    cy = (MDC3D_S32)mdc3d_cos256[yi]; sy = (MDC3D_S32)mdc3d_sin256[yi];
    cr = (MDC3D_S32)mdc3d_cos256[ri]; sr = (MDC3D_S32)mdc3d_sin256[ri];

    r00 = mdc3d_q15_mul(cy, cp);
    t0 = mdc3d_q15_mul(cy, sp);
    t1 = mdc3d_q15_mul(t0, sr);
    r01 = t1 - mdc3d_q15_mul(sy, cr);
    t1 = mdc3d_q15_mul(t0, cr);
    r02 = t1 + mdc3d_q15_mul(sy, sr);

    r10 = mdc3d_q15_mul(sy, cp);
    t0 = mdc3d_q15_mul(sy, sp);
    t1 = mdc3d_q15_mul(t0, sr);
    r11 = t1 + mdc3d_q15_mul(cy, cr);
    t1 = mdc3d_q15_mul(t0, cr);
    r12 = t1 - mdc3d_q15_mul(cy, sr);

    r20 = -sp;
    r21 = mdc3d_q15_mul(cp, sr);
    r22 = mdc3d_q15_mul(cp, cr);

    axis[0][0] = mdc3d_clamp_s16(r00); axis[0][1] = mdc3d_clamp_s16(r01); axis[0][2] = mdc3d_clamp_s16(r02);
    axis[1][0] = mdc3d_clamp_s16(r10); axis[1][1] = mdc3d_clamp_s16(r11); axis[1][2] = mdc3d_clamp_s16(r12);
    axis[2][0] = mdc3d_clamp_s16(r20); axis[2][1] = mdc3d_clamp_s16(r21); axis[2][2] = mdc3d_clamp_s16(r22);
}

const char *mdc3d_version_string(void) { return "mdc3d 0.4"; }

const char *mdc3d_error_string(int code) {
    switch (code) {
    case MDC3D_OK: return "ok";
    case MDC3D_E_ARG: return "bad argument";
    case MDC3D_E_TRUNCATED: return "truncated file";
    case MDC3D_E_BAD_IDENT: return "bad ident";
    case MDC3D_E_BAD_VERSION: return "bad version";
    case MDC3D_E_BAD_OFFSET: return "bad offset/range";
    case MDC3D_E_BAD_COUNT: return "bad count/index";
    case MDC3D_E_UNSUPPORTED: return "unsupported feature";
    case MDC3D_E_INTERNAL: return "internal error";
    case MDC3D_E_PARSE: return "parse error";
    default: return "unknown error";
    }
}

void mdc3d_init(mdc3d_ctx *ctx, const void *data, MDC3D_U32 size) {
    if (!ctx) return;
    ctx->data = (const MDC3D_U8 *)data;
    ctx->size = size;
    ctx->parsed_header = 0;
}

mdc3d_fixed mdc3d_fixed_from_i32(MDC3D_S32 v) { return (mdc3d_fixed)(v << 16); }
MDC3D_S32 mdc3d_fixed_to_i32_floor(mdc3d_fixed v) { return (v >> 16); }

void mdc3d_fixed_to_decimal(mdc3d_fixed v, char *out, MDC3D_U32 out_size, MDC3D_U32 decimals) {
    MDC3D_U32 i, pos, scale, frac;
    MDC3D_S32 whole;
    char tmp[16];
    if (!out || out_size == 0UL) return;
    out[0] = 0;
    pos = 0UL;
    if (v < 0L) { if (pos + 1UL < out_size) out[pos++] = '-'; v = -v; }
    whole = v >> 16;
    i = 0UL;
    do { tmp[i++] = (char)('0' + (whole % 10L)); whole /= 10L; } while (whole && i < sizeof(tmp));
    while (i && pos + 1UL < out_size) out[pos++] = tmp[--i];
    if (decimals && pos + 1UL < out_size) out[pos++] = '.';
    scale = 1UL; for (i = 0UL; i < decimals; ++i) scale *= 10UL;
    frac = (MDC3D_U32)(((v & 0xFFFFL) * (MDC3D_S32)scale) >> 16);
    for (i = decimals; i > 0UL; --i) {
        MDC3D_U32 div = 1UL, j;
        for (j = 1UL; j < i; ++j) div *= 10UL;
        if (pos + 1UL < out_size) out[pos++] = (char)('0' + ((frac / div) % 10UL));
    }
    out[pos < out_size ? pos : out_size - 1UL] = 0;
}

int mdc3d_parse_header(mdc3d_ctx *ctx) {
    const MDC3D_U8 *p;
    if (!ctx || !ctx->data) return MDC3D_E_ARG;
    if (!mdc3d_range_ok(ctx->size, 0UL, MDC3D_HDR_SIZE)) return MDC3D_E_TRUNCATED;
    p = ctx->data;
    ctx->header.ident = mdc3d_u32_le(p + 0);
    ctx->header.version = mdc3d_u32_le(p + 4);
    mdc3d_copy_name(ctx->header.name, p + 8, 64UL, 65UL);
    ctx->header.flags = mdc3d_u32_le(p + 72);
    ctx->header.num_frames = mdc3d_u32_le(p + 76);
    ctx->header.num_tags = mdc3d_u32_le(p + 80);
    ctx->header.num_surfaces = mdc3d_u32_le(p + 84);
    ctx->header.num_skins = mdc3d_u32_le(p + 88);
    ctx->header.ofs_frames = mdc3d_u32_le(p + 92);
    ctx->header.ofs_tag_names = mdc3d_u32_le(p + 96);
    ctx->header.ofs_tag_frames = mdc3d_u32_le(p + 100);
    ctx->header.ofs_surfaces = mdc3d_u32_le(p + 104);
    ctx->header.ofs_end = mdc3d_u32_le(p + 108);
    ctx->parsed_header = 1;
    if (ctx->header.ident != MDC3D_IDENT_FILE) return MDC3D_E_BAD_IDENT;
    if (ctx->header.version != MDC3D_FILE_VERSION) return MDC3D_E_BAD_VERSION;
    return MDC3D_OK;
}

static int mdc3d_check_header_counts(const mdc3d_header *h) {
    if (h->num_frames == 0UL) return MDC3D_E_BAD_COUNT;
    if (h->num_frames > MDC3D_MAX_FRAMES) return MDC3D_E_BAD_COUNT;
    if (h->num_tags > MDC3D_MAX_TAGS) return MDC3D_E_BAD_COUNT;
    if (h->num_surfaces > MDC3D_MAX_SURFACES) return MDC3D_E_BAD_COUNT;
    return MDC3D_OK;
}

int mdc3d_validate(mdc3d_ctx *ctx) {
    MDC3D_U32 bytes, bytes2, off, i;
    int r;
    mdc3d_surface s;
    if (!ctx) return MDC3D_E_ARG;
    if (!ctx->parsed_header) { r = mdc3d_parse_header(ctx); if (r != MDC3D_OK) return r; }
    r = mdc3d_check_header_counts(&ctx->header); if (r != MDC3D_OK) return r;
    if (ctx->header.ofs_end > ctx->size) return MDC3D_E_BAD_OFFSET;
    if (ctx->header.ofs_frames < MDC3D_HDR_SIZE) return MDC3D_E_BAD_OFFSET;
    if (ctx->header.ofs_tag_names < ctx->header.ofs_frames) return MDC3D_E_BAD_OFFSET;
    if (ctx->header.ofs_tag_frames < ctx->header.ofs_tag_names) return MDC3D_E_BAD_OFFSET;
    if (ctx->header.ofs_surfaces < ctx->header.ofs_tag_frames) return MDC3D_E_BAD_OFFSET;
    if (ctx->header.ofs_end < ctx->header.ofs_surfaces) return MDC3D_E_BAD_OFFSET;
    if (!mdc3d_mul_ok(ctx->header.num_frames, MDC3D_FRAME_SIZE, &bytes)) return MDC3D_E_BAD_COUNT;
    if (!mdc3d_range_ok(ctx->size, ctx->header.ofs_frames, bytes)) return MDC3D_E_BAD_OFFSET;
    if (!mdc3d_mul_ok(ctx->header.num_tags, MDC3D_TAG_NAME_SIZE, &bytes)) return MDC3D_E_BAD_COUNT;
    if (!mdc3d_range_ok(ctx->size, ctx->header.ofs_tag_names, bytes)) return MDC3D_E_BAD_OFFSET;
    if (!mdc3d_mul_ok(ctx->header.num_frames, ctx->header.num_tags, &bytes)) return MDC3D_E_BAD_COUNT;
    if (!mdc3d_mul_ok(bytes, MDC3D_TAG_FRAME_SIZE, &bytes2)) return MDC3D_E_BAD_COUNT;
    if (!mdc3d_range_ok(ctx->size, ctx->header.ofs_tag_frames, bytes2)) return MDC3D_E_BAD_OFFSET;
    off = ctx->header.ofs_surfaces;
    for (i = 0UL; i < ctx->header.num_surfaces; ++i) {
        r = mdc3d_read_surface(ctx, i, &s); if (r != MDC3D_OK) return r;
        if (s.file_offset != off) return MDC3D_E_BAD_OFFSET;
        if (!mdc3d_add_ok(off, s.ofs_end, &off)) return MDC3D_E_BAD_OFFSET;
    }
    return MDC3D_OK;
}

int mdc3d_read_frame(const mdc3d_ctx *ctx, MDC3D_U32 frame_index, mdc3d_frame *out_frame) {
    MDC3D_U32 off; const MDC3D_U8 *p;
    if (!ctx || !out_frame || !ctx->parsed_header) return MDC3D_E_ARG;
    if (frame_index >= ctx->header.num_frames) return MDC3D_E_BAD_COUNT;
    off = ctx->header.ofs_frames + frame_index * MDC3D_FRAME_SIZE;
    if (!mdc3d_range_ok(ctx->size, off, MDC3D_FRAME_SIZE)) return MDC3D_E_BAD_OFFSET;
    p = ctx->data + off;
    out_frame->bbox_min[0] = mdc3d_f32le_to_fixed(p + 0);
    out_frame->bbox_min[1] = mdc3d_f32le_to_fixed(p + 4);
    out_frame->bbox_min[2] = mdc3d_f32le_to_fixed(p + 8);
    out_frame->bbox_max[0] = mdc3d_f32le_to_fixed(p + 12);
    out_frame->bbox_max[1] = mdc3d_f32le_to_fixed(p + 16);
    out_frame->bbox_max[2] = mdc3d_f32le_to_fixed(p + 20);
    out_frame->local_origin[0] = mdc3d_f32le_to_fixed(p + 24);
    out_frame->local_origin[1] = mdc3d_f32le_to_fixed(p + 28);
    out_frame->local_origin[2] = mdc3d_f32le_to_fixed(p + 32);
    out_frame->radius = mdc3d_f32le_to_fixed(p + 36);
    mdc3d_copy_name(out_frame->name, p + 40, 16UL, 17UL);
    return MDC3D_OK;
}

int mdc3d_read_surface(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, mdc3d_surface *s) {
    MDC3D_U32 i, off, step; const MDC3D_U8 *p;
    if (!ctx || !s || !ctx->parsed_header) return MDC3D_E_ARG;
    if (surface_index >= ctx->header.num_surfaces) return MDC3D_E_BAD_COUNT;
    off = ctx->header.ofs_surfaces;
    for (i = 0UL; i <= surface_index; ++i) {
        if (!mdc3d_range_ok(ctx->size, off, MDC3D_SURF_HDR_SIZE)) return MDC3D_E_BAD_OFFSET;
        p = ctx->data + off;
        s->file_offset = off;
        s->ident = mdc3d_u32_le(p + 0);
        mdc3d_copy_name(s->name, p + 4, 64UL, 65UL);
        s->flags = mdc3d_u32_le(p + 68);
        s->num_comp_frames = mdc3d_u32_le(p + 72);
        s->num_base_frames = mdc3d_u32_le(p + 76);
        s->num_shaders = mdc3d_u32_le(p + 80);
        s->num_vertices = mdc3d_u32_le(p + 84);
        s->num_triangles = mdc3d_u32_le(p + 88);
        s->ofs_triangles = mdc3d_u32_le(p + 92);
        s->ofs_shaders = mdc3d_u32_le(p + 96);
        s->ofs_texcoords = mdc3d_u32_le(p + 100);
        s->ofs_base_verts = mdc3d_u32_le(p + 104);
        s->ofs_comp_verts = mdc3d_u32_le(p + 108);
        s->ofs_frame_base_frames = mdc3d_u32_le(p + 112);
        s->ofs_frame_comp_frames = mdc3d_u32_le(p + 116);
        s->ofs_end = mdc3d_u32_le(p + 120);
        if (s->ident != MDC3D_IDENT_SURFACE) return MDC3D_E_BAD_IDENT;
        if (s->num_vertices > MDC3D_MAX_VERTICES || s->num_triangles > MDC3D_MAX_TRIANGLES || s->num_shaders > MDC3D_MAX_SHADERS) return MDC3D_E_BAD_COUNT;
        if (s->ofs_end < MDC3D_SURF_HDR_SIZE) return MDC3D_E_BAD_OFFSET;
        if (!mdc3d_range_ok(ctx->size, off, s->ofs_end)) return MDC3D_E_BAD_OFFSET;
        if (i == surface_index) return MDC3D_OK;
        step = s->ofs_end;
        if (!mdc3d_add_ok(off, step, &off)) return MDC3D_E_BAD_OFFSET;
    }
    return MDC3D_E_INTERNAL;
}

static int mdc3d_check_surface_ranges(const mdc3d_ctx *ctx, const mdc3d_surface *s) {
    MDC3D_U32 b;
    if (!mdc3d_mul_ok(s->num_triangles, MDC3D_TRI_SIZE, &b)) return MDC3D_E_BAD_COUNT;
    if (s->ofs_triangles > s->ofs_end || b > s->ofs_end - s->ofs_triangles) return MDC3D_E_BAD_OFFSET;
    if (!mdc3d_mul_ok(s->num_shaders, MDC3D_SHADER_SIZE, &b)) return MDC3D_E_BAD_COUNT;
    if (s->ofs_shaders > s->ofs_end || b > s->ofs_end - s->ofs_shaders) return MDC3D_E_BAD_OFFSET;
    if (!mdc3d_mul_ok(s->num_vertices, MDC3D_TEX_SIZE, &b)) return MDC3D_E_BAD_COUNT;
    if (s->ofs_texcoords > s->ofs_end || b > s->ofs_end - s->ofs_texcoords) return MDC3D_E_BAD_OFFSET;
    if (!mdc3d_mul_ok(s->num_base_frames, s->num_vertices, &b)) return MDC3D_E_BAD_COUNT;
    if (!mdc3d_mul_ok(b, MDC3D_BASEVERT_SIZE, &b)) return MDC3D_E_BAD_COUNT;
    if (s->ofs_base_verts > s->ofs_end || b > s->ofs_end - s->ofs_base_verts) return MDC3D_E_BAD_OFFSET;
    if (!mdc3d_mul_ok(s->num_comp_frames, s->num_vertices, &b)) return MDC3D_E_BAD_COUNT;
    if (!mdc3d_mul_ok(b, MDC3D_COMPVERT_SIZE, &b)) return MDC3D_E_BAD_COUNT;
    if (s->ofs_comp_verts > s->ofs_end || b > s->ofs_end - s->ofs_comp_verts) return MDC3D_E_BAD_OFFSET;
    if (!mdc3d_mul_ok(ctx->header.num_frames, 2UL, &b)) return MDC3D_E_BAD_COUNT;
    if (s->ofs_frame_base_frames > s->ofs_end || b > s->ofs_end - s->ofs_frame_base_frames) return MDC3D_E_BAD_OFFSET;
    if (s->ofs_frame_comp_frames > s->ofs_end || b > s->ofs_end - s->ofs_frame_comp_frames) return MDC3D_E_BAD_OFFSET;
    return MDC3D_OK;
}


int mdc3d_read_tag_name(const mdc3d_ctx *ctx, MDC3D_U32 tag_index, mdc3d_tag_name *out_tag) {
    MDC3D_U32 off;
    if (!ctx || !out_tag || !ctx->parsed_header) return MDC3D_E_ARG;
    if (tag_index >= ctx->header.num_tags) return MDC3D_E_BAD_COUNT;
    off = ctx->header.ofs_tag_names + tag_index * MDC3D_TAG_NAME_SIZE;
    if (!mdc3d_range_ok(ctx->size, off, MDC3D_TAG_NAME_SIZE)) return MDC3D_E_BAD_OFFSET;
    mdc3d_copy_name(out_tag->name, ctx->data + off, 64UL, 65UL);
    return MDC3D_OK;
}

int mdc3d_read_tag_frame(const mdc3d_ctx *ctx, MDC3D_U32 frame_index, MDC3D_U32 tag_index, mdc3d_tag_frame *out_tag_frame) {
    MDC3D_U32 off;
    const MDC3D_U8 *p;
    if (!ctx || !out_tag_frame || !ctx->parsed_header) return MDC3D_E_ARG;
    if (frame_index >= ctx->header.num_frames || tag_index >= ctx->header.num_tags) return MDC3D_E_BAD_COUNT;
    off = ctx->header.ofs_tag_frames + (frame_index * ctx->header.num_tags + tag_index) * MDC3D_TAG_FRAME_SIZE;
    if (!mdc3d_range_ok(ctx->size, off, MDC3D_TAG_FRAME_SIZE)) return MDC3D_E_BAD_OFFSET;
    p = ctx->data + off;
    out_tag_frame->frame_index = frame_index;
    out_tag_frame->tag_index = tag_index;
    out_tag_frame->raw_pos[0] = mdc3d_s16_le(p + 0);
    out_tag_frame->raw_pos[1] = mdc3d_s16_le(p + 2);
    out_tag_frame->raw_pos[2] = mdc3d_s16_le(p + 4);
    out_tag_frame->raw_angle[0] = mdc3d_s16_le(p + 6);
    out_tag_frame->raw_angle[1] = mdc3d_s16_le(p + 8);
    out_tag_frame->raw_angle[2] = mdc3d_s16_le(p + 10);
    out_tag_frame->pos[0] = ((MDC3D_S32)out_tag_frame->raw_pos[0]) << 10;
    out_tag_frame->pos[1] = ((MDC3D_S32)out_tag_frame->raw_pos[1]) << 10;
    out_tag_frame->pos[2] = ((MDC3D_S32)out_tag_frame->raw_pos[2]) << 10;
    mdc3d_build_tag_axis(out_tag_frame->raw_angle[0], out_tag_frame->raw_angle[1], out_tag_frame->raw_angle[2], out_tag_frame->axis);
    return MDC3D_OK;
}

int mdc3d_read_shader(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 shader_index, mdc3d_shader *out_shader) {
    mdc3d_surface s;
    const MDC3D_U8 *p;
    int r;
    if (!ctx || !out_shader || !ctx->parsed_header) return MDC3D_E_ARG;
    r = mdc3d_read_surface(ctx, surface_index, &s); if (r != MDC3D_OK) return r;
    r = mdc3d_check_surface_ranges(ctx, &s); if (r != MDC3D_OK) return r;
    if (shader_index >= s.num_shaders) return MDC3D_E_BAD_COUNT;
    p = ctx->data + s.file_offset + s.ofs_shaders + shader_index * MDC3D_SHADER_SIZE;
    mdc3d_copy_name(out_shader->name, p, 64UL, 65UL);
    out_shader->flags = mdc3d_u32_le(p + 64);
    return MDC3D_OK;
}

int mdc3d_read_triangle(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 tri_index, mdc3d_triangle *out_tri) {
    mdc3d_surface s;
    const MDC3D_U8 *p;
    int r;
    if (!ctx || !out_tri || !ctx->parsed_header) return MDC3D_E_ARG;
    r = mdc3d_read_surface(ctx, surface_index, &s); if (r != MDC3D_OK) return r;
    r = mdc3d_check_surface_ranges(ctx, &s); if (r != MDC3D_OK) return r;
    if (tri_index >= s.num_triangles) return MDC3D_E_BAD_COUNT;
    p = ctx->data + s.file_offset + s.ofs_triangles + tri_index * MDC3D_TRI_SIZE;
    out_tri->index[0] = mdc3d_u32_le(p + 0);
    out_tri->index[1] = mdc3d_u32_le(p + 4);
    out_tri->index[2] = mdc3d_u32_le(p + 8);
    if (out_tri->index[0] >= s.num_vertices || out_tri->index[1] >= s.num_vertices || out_tri->index[2] >= s.num_vertices) return MDC3D_E_BAD_COUNT;
    return MDC3D_OK;
}

int mdc3d_read_texcoord(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 vertex_index, mdc3d_texcoord *out_uv) {
    mdc3d_surface s;
    const MDC3D_U8 *p;
    int r;
    if (!ctx || !out_uv || !ctx->parsed_header) return MDC3D_E_ARG;
    r = mdc3d_read_surface(ctx, surface_index, &s); if (r != MDC3D_OK) return r;
    r = mdc3d_check_surface_ranges(ctx, &s); if (r != MDC3D_OK) return r;
    if (vertex_index >= s.num_vertices) return MDC3D_E_BAD_COUNT;
    p = ctx->data + s.file_offset + s.ofs_texcoords + vertex_index * MDC3D_TEX_SIZE;
    out_uv->s = mdc3d_f32le_to_fixed(p + 0);
    out_uv->t = mdc3d_f32le_to_fixed(p + 4);
    return MDC3D_OK;
}

int mdc3d_read_frame_map(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 frame_index, mdc3d_frame_map *out_map) {
    mdc3d_surface s;
    MDC3D_U32 fb_off, fc_off;
    int r;
    if (!ctx || !out_map || !ctx->parsed_header) return MDC3D_E_ARG;
    if (frame_index >= ctx->header.num_frames) return MDC3D_E_BAD_COUNT;
    r = mdc3d_read_surface(ctx, surface_index, &s); if (r != MDC3D_OK) return r;
    r = mdc3d_check_surface_ranges(ctx, &s); if (r != MDC3D_OK) return r;
    fb_off = s.file_offset + s.ofs_frame_base_frames + frame_index * 2UL;
    fc_off = s.file_offset + s.ofs_frame_comp_frames + frame_index * 2UL;
    out_map->base_frame_index = mdc3d_u16_le(ctx->data + fb_off);
    out_map->comp_frame_index = mdc3d_u16_le(ctx->data + fc_off);
    out_map->has_comp = (out_map->comp_frame_index != MDC3D_NO_COMP_FRAME) ? 1U : 0U;
    if ((MDC3D_U32)out_map->base_frame_index >= s.num_base_frames) return MDC3D_E_BAD_COUNT;
    if (out_map->has_comp && (MDC3D_U32)out_map->comp_frame_index >= s.num_comp_frames) return MDC3D_E_BAD_COUNT;
    return MDC3D_OK;
}

int mdc3d_read_base_vertex(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 base_frame_index, MDC3D_U32 vertex_index, mdc3d_raw_base_vertex *out_base) {
    mdc3d_surface s;
    MDC3D_U32 off;
    const MDC3D_U8 *p;
    int r;
    if (!ctx || !out_base || !ctx->parsed_header) return MDC3D_E_ARG;
    r = mdc3d_read_surface(ctx, surface_index, &s); if (r != MDC3D_OK) return r;
    r = mdc3d_check_surface_ranges(ctx, &s); if (r != MDC3D_OK) return r;
    if (base_frame_index >= s.num_base_frames || vertex_index >= s.num_vertices) return MDC3D_E_BAD_COUNT;
    off = s.file_offset + s.ofs_base_verts + (base_frame_index * s.num_vertices + vertex_index) * MDC3D_BASEVERT_SIZE;
    if (!mdc3d_range_ok(ctx->size, off, MDC3D_BASEVERT_SIZE)) return MDC3D_E_BAD_OFFSET;
    p = ctx->data + off;
    out_base->x = mdc3d_s16_le(p + 0);
    out_base->y = mdc3d_s16_le(p + 2);
    out_base->z = mdc3d_s16_le(p + 4);
    out_base->normal = mdc3d_u16_le(p + 6);
    return MDC3D_OK;
}

int mdc3d_read_comp_vertex(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 comp_frame_index, MDC3D_U32 vertex_index, mdc3d_raw_comp_vertex *out_comp) {
    mdc3d_surface s;
    MDC3D_U32 off;
    const MDC3D_U8 *p;
    int r;
    if (!ctx || !out_comp || !ctx->parsed_header) return MDC3D_E_ARG;
    r = mdc3d_read_surface(ctx, surface_index, &s); if (r != MDC3D_OK) return r;
    r = mdc3d_check_surface_ranges(ctx, &s); if (r != MDC3D_OK) return r;
    if (comp_frame_index >= s.num_comp_frames || vertex_index >= s.num_vertices) return MDC3D_E_BAD_COUNT;
    off = s.file_offset + s.ofs_comp_verts + (comp_frame_index * s.num_vertices + vertex_index) * MDC3D_COMPVERT_SIZE;
    if (!mdc3d_range_ok(ctx->size, off, MDC3D_COMPVERT_SIZE)) return MDC3D_E_BAD_OFFSET;
    p = ctx->data + off;
    out_comp->x = p[0]; out_comp->y = p[1]; out_comp->z = p[2]; out_comp->normal = p[3];
    return MDC3D_OK;
}

int mdc3d_read_surface_frame_vertex(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 frame_index, MDC3D_U32 vertex_index, mdc3d_vertex *out_vertex) {
    mdc3d_surface s;
    mdc3d_frame fr;
    mdc3d_frame_map fm;
    mdc3d_raw_base_vertex bv;
    mdc3d_raw_comp_vertex cv;
    MDC3D_S32 dx, dy, dz;
    int r;
    if (!ctx || !out_vertex || !ctx->parsed_header) return MDC3D_E_ARG;
    if (frame_index >= ctx->header.num_frames) return MDC3D_E_BAD_COUNT;
    r = mdc3d_read_surface(ctx, surface_index, &s); if (r != MDC3D_OK) return r;
    r = mdc3d_check_surface_ranges(ctx, &s); if (r != MDC3D_OK) return r;
    if (vertex_index >= s.num_vertices) return MDC3D_E_BAD_COUNT;
    r = mdc3d_read_frame(ctx, frame_index, &fr); if (r != MDC3D_OK) return r;
    r = mdc3d_read_frame_map(ctx, surface_index, frame_index, &fm); if (r != MDC3D_OK) return r;
    r = mdc3d_read_base_vertex(ctx, surface_index, fm.base_frame_index, vertex_index, &bv); if (r != MDC3D_OK) return r;
    out_vertex->surface_index = surface_index;
    out_vertex->frame_index = frame_index;
    out_vertex->vertex_index = vertex_index;
    out_vertex->pos[0] = fr.local_origin[0] + ((MDC3D_S32)bv.x << 10);
    out_vertex->pos[1] = fr.local_origin[1] + ((MDC3D_S32)bv.y << 10);
    out_vertex->pos[2] = fr.local_origin[2] + ((MDC3D_S32)bv.z << 10);
    out_vertex->base_frame_index = fm.base_frame_index;
    out_vertex->comp_frame_index = fm.comp_frame_index;
    out_vertex->base_normal_raw = bv.normal;
    out_vertex->comp_normal_raw = 0U;
    out_vertex->has_comp = 0U;
    if (fm.has_comp) {
        r = mdc3d_read_comp_vertex(ctx, surface_index, fm.comp_frame_index, vertex_index, &cv); if (r != MDC3D_OK) return r;
        dx = ((MDC3D_S32)cv.x - 127L) << 12;
        dy = ((MDC3D_S32)cv.y - 127L) << 12;
        dz = ((MDC3D_S32)cv.z - 127L) << 12;
        out_vertex->pos[0] += dx; out_vertex->pos[1] += dy; out_vertex->pos[2] += dz;
        out_vertex->comp_normal_raw = cv.normal;
        out_vertex->has_comp = 1U;
    }
    mdc3d_final_normal(bv.normal, out_vertex->has_comp, out_vertex->comp_normal_raw, out_vertex->normal);
    return MDC3D_OK;
}

int mdc3d_audit_model(mdc3d_ctx *ctx, mdc3d_audit *out_audit) {
    MDC3D_U32 i, j, off, tmp;
    mdc3d_surface s;
    mdc3d_frame_map fm;
    int r;
    if (!ctx || !out_audit) return MDC3D_E_ARG;
    if (!ctx->parsed_header) { r = mdc3d_parse_header(ctx); if (r != MDC3D_OK) return r; }
    r = mdc3d_validate(ctx); if (r != MDC3D_OK) return r;
    out_audit->flags = 0UL;
    out_audit->first_surface_offset = ctx->header.ofs_surfaces;
    out_audit->computed_surface_end = ctx->header.ofs_surfaces;
    out_audit->header_ofs_end = ctx->header.ofs_end;
    out_audit->trailing_bytes = 0UL;
    out_audit->compressed_frame_refs = 0UL;
    out_audit->uncompressed_frame_refs = 0UL;
    if (ctx->header.ofs_frames > MDC3D_HDR_SIZE) out_audit->flags |= MDC3D_AUDIT_PADDED_HEADER;
    if (ctx->header.num_skins != 0UL) out_audit->flags |= MDC3D_AUDIT_USES_SKIN_COUNT;
    off = ctx->header.ofs_surfaces;
    for (i = 0UL; i < ctx->header.num_surfaces; ++i) {
        r = mdc3d_read_surface(ctx, i, &s); if (r != MDC3D_OK) return r;
        if (s.num_comp_frames != 0UL) out_audit->flags |= MDC3D_AUDIT_USES_COMPRESSION;
        for (j = 0UL; j < ctx->header.num_frames; ++j) {
            r = mdc3d_read_frame_map(ctx, i, j, &fm); if (r != MDC3D_OK) return r;
            if (fm.has_comp) out_audit->compressed_frame_refs++; else out_audit->uncompressed_frame_refs++;
        }
        if (!mdc3d_add_ok(off, s.ofs_end, &tmp)) return MDC3D_E_BAD_OFFSET;
        off = tmp;
    }
    out_audit->computed_surface_end = off;
    if (ctx->header.ofs_end > off) {
        out_audit->trailing_bytes = ctx->header.ofs_end - off;
        out_audit->flags |= MDC3D_AUDIT_TRAILING_BYTES;
    }
    return MDC3D_OK;
}

int mdc3d_get_info(mdc3d_ctx *ctx, mdc3d_info *out_info) {
    MDC3D_U32 i, tmp;
    mdc3d_surface s;
    int r;
    if (!ctx || !out_info) return MDC3D_E_ARG;
    if (!ctx->parsed_header) { r = mdc3d_parse_header(ctx); if (r != MDC3D_OK) return r; }
    r = mdc3d_validate(ctx); if (r != MDC3D_OK) return r;
    out_info->num_frames = ctx->header.num_frames;
    out_info->num_tags = ctx->header.num_tags;
    out_info->num_surfaces = ctx->header.num_surfaces;
    out_info->total_vertices_per_frame = 0UL;
    out_info->total_triangles = 0UL;
    out_info->total_shaders = 0UL;
    out_info->total_base_frames = 0UL;
    out_info->total_comp_frames = 0UL;
    out_info->total_emitted_vertices = 0UL;
    out_info->has_compression = 0U;
    for (i = 0UL; i < ctx->header.num_surfaces; ++i) {
        r = mdc3d_read_surface(ctx, i, &s); if (r != MDC3D_OK) return r;
        r = mdc3d_check_surface_ranges(ctx, &s); if (r != MDC3D_OK) return r;
        if (!mdc3d_add_ok(out_info->total_vertices_per_frame, s.num_vertices, &tmp)) return MDC3D_E_BAD_COUNT;
        out_info->total_vertices_per_frame = tmp;
        if (!mdc3d_add_ok(out_info->total_triangles, s.num_triangles, &tmp)) return MDC3D_E_BAD_COUNT;
        out_info->total_triangles = tmp;
        if (!mdc3d_add_ok(out_info->total_shaders, s.num_shaders, &tmp)) return MDC3D_E_BAD_COUNT;
        out_info->total_shaders = tmp;
        if (!mdc3d_add_ok(out_info->total_base_frames, s.num_base_frames, &tmp)) return MDC3D_E_BAD_COUNT;
        out_info->total_base_frames = tmp;
        if (!mdc3d_add_ok(out_info->total_comp_frames, s.num_comp_frames, &tmp)) return MDC3D_E_BAD_COUNT;
        out_info->total_comp_frames = tmp;
        if (s.num_comp_frames != 0UL) out_info->has_compression = 1U;
    }
    if (!mdc3d_mul_ok(out_info->total_vertices_per_frame, ctx->header.num_frames, &tmp)) return MDC3D_E_BAD_COUNT;
    out_info->total_emitted_vertices = tmp;
    return MDC3D_OK;
}

static int mdc3d_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static void mdc3d_copy_token(char *dst, MDC3D_U32 dstn, const char *src, MDC3D_U32 len) {
    MDC3D_U32 i;
    if (dstn == 0UL) return;
    while (len && mdc3d_is_space(src[0])) { ++src; --len; }
    while (len && mdc3d_is_space(src[len - 1UL])) { --len; }
    for (i = 0UL; i + 1UL < dstn && i < len; ++i) dst[i] = src[i];
    dst[i] = 0;
}

int mdc3d_skin_walk(const char *text, MDC3D_U32 size, const mdc3d_skin_callbacks *cb) {
    MDC3D_U32 i, line_start, comma, line_end;
    mdc3d_skin_entry e;
    int r;
    if (!text || !cb || !cb->on_entry) return MDC3D_E_ARG;
    i = 0UL;
    while (i < size) {
        line_start = i;
        while (i < size && text[i] != '\n' && text[i] != '\r') ++i;
        line_end = i;
        while (i < size && (text[i] == '\n' || text[i] == '\r')) ++i;
        while (line_start < line_end && mdc3d_is_space(text[line_start])) ++line_start;
        if (line_start >= line_end) continue;
        if (text[line_start] == '/' && line_start + 1UL < line_end && text[line_start + 1UL] == '/') continue;
        if (text[line_start] == '#') continue;
        comma = line_start;
        while (comma < line_end && text[comma] != ',') ++comma;
        if (comma >= line_end) continue;
        mdc3d_copy_token(e.surface, 65UL, text + line_start, comma - line_start);
        mdc3d_copy_token(e.shader, 65UL, text + comma + 1UL, line_end - comma - 1UL);
        e.is_tag = (e.surface[0] == 't' && e.surface[1] == 'a' && e.surface[2] == 'g' && e.surface[3] == '_') ? 1U : 0U;
        r = cb->on_entry(cb->user, &e); if (r) return r;
    }
    return MDC3D_OK;
}

typedef struct mdc3d_skin_resolve_state_s {
    const char *surface;
    char *out_shader;
    MDC3D_U32 out_shader_size;
    MDC3D_U8 found;
} mdc3d_skin_resolve_state;

static int mdc3d_streq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; ++a; ++b; }
    return *a == 0 && *b == 0;
}

static int mdc3d_skin_resolve_cb(void *user, const mdc3d_skin_entry *entry) {
    mdc3d_skin_resolve_state *st;
    MDC3D_U32 i;
    st = (mdc3d_skin_resolve_state *)user;
    if (st->found) return 0;
    if (!mdc3d_streq(st->surface, entry->surface)) return 0;
    if (st->out_shader_size == 0UL) return MDC3D_E_ARG;
    for (i = 0UL; i + 1UL < st->out_shader_size && entry->shader[i] != 0; ++i) st->out_shader[i] = entry->shader[i];
    st->out_shader[i] = 0;
    st->found = 1U;
    return 0;
}

int mdc3d_skin_resolve_surface(const char *text, MDC3D_U32 size, const char *surface, char *out_shader, MDC3D_U32 out_shader_size) {
    mdc3d_skin_callbacks cb;
    mdc3d_skin_resolve_state st;
    int r;
    if (!text || !surface || !out_shader || out_shader_size == 0UL) return MDC3D_E_ARG;
    out_shader[0] = 0;
    st.surface = surface;
    st.out_shader = out_shader;
    st.out_shader_size = out_shader_size;
    st.found = 0U;
    cb.user = &st;
    cb.on_entry = mdc3d_skin_resolve_cb;
    r = mdc3d_skin_walk(text, size, &cb);
    if (r != MDC3D_OK) return r;
    return st.found ? MDC3D_OK : MDC3D_E_PARSE;
}

static mdc3d_fixed mdc3d_fixed_lerp_u16(mdc3d_fixed a, mdc3d_fixed b, MDC3D_U16 alpha) {
    MDC3D_S32 diff;
    MDC3D_U32 mag, whole, frac, part;
    int neg;
    if (alpha == 0U) return a;
    if (alpha == 65535U) return b;
    diff = b - a;
    neg = 0;
    if (diff < 0L) { neg = 1; mag = (MDC3D_U32)(-diff); }
    else mag = (MDC3D_U32)diff;
    whole = mag >> 16;
    frac = mag & 0xFFFFUL;
    if (whole > 32767UL) return neg ? (a - (mdc3d_fixed)0x7FFFFFFFL) : (a + (mdc3d_fixed)0x7FFFFFFFL);
    part = whole * (MDC3D_U32)alpha;
    part += (MDC3D_U32)(((frac * (MDC3D_U32)alpha) >> 16) & 0xFFFFUL);
    if (neg) return a - (mdc3d_fixed)part;
    return a + (mdc3d_fixed)part;
}

static MDC3D_S16 mdc3d_normal_lerp_component(MDC3D_S16 a, MDC3D_S16 b, MDC3D_U16 alpha) {
    MDC3D_S32 diff, v;
    diff = (MDC3D_S32)b - (MDC3D_S32)a;
    v = (MDC3D_S32)a + (MDC3D_S32)((diff * (MDC3D_S32)alpha) / 65535L);
    return mdc3d_clamp_s16(v);
}

void mdc3d_transform_point_by_tag(const mdc3d_tag_frame *tag, const mdc3d_fixed local_pos[3], mdc3d_fixed out_pos[3]) {
    MDC3D_S32 x, y, z;
    if (!tag || !local_pos || !out_pos) return;
    x = tag->pos[0]; y = tag->pos[1]; z = tag->pos[2];
    x += (MDC3D_S32)(((MDC3D_S32)local_pos[0] * (MDC3D_S32)tag->axis[0][0]) >> 15);
    x += (MDC3D_S32)(((MDC3D_S32)local_pos[1] * (MDC3D_S32)tag->axis[0][1]) >> 15);
    x += (MDC3D_S32)(((MDC3D_S32)local_pos[2] * (MDC3D_S32)tag->axis[0][2]) >> 15);
    y += (MDC3D_S32)(((MDC3D_S32)local_pos[0] * (MDC3D_S32)tag->axis[1][0]) >> 15);
    y += (MDC3D_S32)(((MDC3D_S32)local_pos[1] * (MDC3D_S32)tag->axis[1][1]) >> 15);
    y += (MDC3D_S32)(((MDC3D_S32)local_pos[2] * (MDC3D_S32)tag->axis[1][2]) >> 15);
    z += (MDC3D_S32)(((MDC3D_S32)local_pos[0] * (MDC3D_S32)tag->axis[2][0]) >> 15);
    z += (MDC3D_S32)(((MDC3D_S32)local_pos[1] * (MDC3D_S32)tag->axis[2][1]) >> 15);
    z += (MDC3D_S32)(((MDC3D_S32)local_pos[2] * (MDC3D_S32)tag->axis[2][2]) >> 15);
    out_pos[0] = x; out_pos[1] = y; out_pos[2] = z;
}

int mdc3d_emit_surface_frame_vertices(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 frame_index, const mdc3d_callbacks *cb) {
    mdc3d_surface s;
    mdc3d_vertex v;
    MDC3D_U32 i;
    int r;
    if (!ctx || !ctx->parsed_header || !cb || !cb->on_vertex) return MDC3D_E_ARG;
    r = mdc3d_read_surface(ctx, surface_index, &s); if (r != MDC3D_OK) return r;
    r = mdc3d_check_surface_ranges(ctx, &s); if (r != MDC3D_OK) return r;
    for (i = 0UL; i < s.num_vertices; ++i) {
        r = mdc3d_read_surface_frame_vertex(ctx, surface_index, frame_index, i, &v); if (r != MDC3D_OK) return r;
        r = cb->on_vertex(cb->user, &v); if (r) return r;
    }
    return MDC3D_OK;
}

int mdc3d_emit_surface_frame_vertices_lerp(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 frame_a, MDC3D_U32 frame_b, MDC3D_U16 alpha_q16, const mdc3d_callbacks *cb) {
    mdc3d_surface s;
    mdc3d_vertex va, vb, vo;
    MDC3D_U32 i;
    MDC3D_S16 nx, ny, nz;
    int r;
    if (!ctx || !ctx->parsed_header || !cb || !cb->on_vertex) return MDC3D_E_ARG;
    r = mdc3d_read_surface(ctx, surface_index, &s); if (r != MDC3D_OK) return r;
    r = mdc3d_check_surface_ranges(ctx, &s); if (r != MDC3D_OK) return r;
    for (i = 0UL; i < s.num_vertices; ++i) {
        r = mdc3d_read_surface_frame_vertex(ctx, surface_index, frame_a, i, &va); if (r != MDC3D_OK) return r;
        r = mdc3d_read_surface_frame_vertex(ctx, surface_index, frame_b, i, &vb); if (r != MDC3D_OK) return r;
        vo = va;
        vo.frame_index = frame_a;
        vo.pos[0] = mdc3d_fixed_lerp_u16(va.pos[0], vb.pos[0], alpha_q16);
        vo.pos[1] = mdc3d_fixed_lerp_u16(va.pos[1], vb.pos[1], alpha_q16);
        vo.pos[2] = mdc3d_fixed_lerp_u16(va.pos[2], vb.pos[2], alpha_q16);
        nx = mdc3d_normal_lerp_component(va.normal[0], vb.normal[0], alpha_q16);
        ny = mdc3d_normal_lerp_component(va.normal[1], vb.normal[1], alpha_q16);
        nz = mdc3d_normal_lerp_component(va.normal[2], vb.normal[2], alpha_q16);
        mdc3d_normalize_q15((MDC3D_S32)nx, (MDC3D_S32)ny, (MDC3D_S32)nz, vo.normal);
        r = cb->on_vertex(cb->user, &vo); if (r) return r;
    }
    return MDC3D_OK;
}

int mdc3d_emit_frame_vertices(const mdc3d_ctx *ctx, MDC3D_U32 frame_index, const mdc3d_callbacks *cb) {
    MDC3D_U32 i;
    int r;
    if (!ctx || !ctx->parsed_header) return MDC3D_E_ARG;
    for (i = 0UL; i < ctx->header.num_surfaces; ++i) {
        r = mdc3d_emit_surface_frame_vertices(ctx, i, frame_index, cb); if (r != MDC3D_OK) return r;
    }
    return MDC3D_OK;
}

int mdc3d_walk(mdc3d_ctx *ctx, const mdc3d_callbacks *cb, int emit_vertices) {
    MDC3D_U32 i, j, off; int r; mdc3d_frame fr; mdc3d_tag_name tn; mdc3d_tag_frame tf; mdc3d_surface s;
    if (!ctx) return MDC3D_E_ARG;
    if (!ctx->parsed_header) { r = mdc3d_parse_header(ctx); if (r != MDC3D_OK) return r; }
    r = mdc3d_validate(ctx); if (r != MDC3D_OK) return r;
    if (cb && cb->on_model) { r = cb->on_model(cb->user, &ctx->header); if (r) return r; }
    for (i = 0UL; i < ctx->header.num_frames; ++i) {
        r = mdc3d_read_frame(ctx, i, &fr); if (r != MDC3D_OK) return r;
        if (cb && cb->on_frame) { r = cb->on_frame(cb->user, i, &fr); if (r) return r; }
    }
    for (i = 0UL; i < ctx->header.num_tags; ++i) {
        off = ctx->header.ofs_tag_names + i * MDC3D_TAG_NAME_SIZE;
        mdc3d_copy_name(tn.name, ctx->data + off, 64UL, 65UL);
        if (cb && cb->on_tag_name) { r = cb->on_tag_name(cb->user, i, &tn); if (r) return r; }
    }
    for (i = 0UL; i < ctx->header.num_frames; ++i) {
        for (j = 0UL; j < ctx->header.num_tags; ++j) {
            r = mdc3d_read_tag_frame(ctx, i, j, &tf); if (r != MDC3D_OK) return r;
            if (cb && cb->on_tag_frame) { r = cb->on_tag_frame(cb->user, &tf); if (r) return r; }
        }
    }
    for (i = 0UL; i < ctx->header.num_surfaces; ++i) {
        r = mdc3d_read_surface(ctx, i, &s); if (r != MDC3D_OK) return r;
        r = mdc3d_check_surface_ranges(ctx, &s); if (r != MDC3D_OK) return r;
        if (cb && cb->on_surface) { r = cb->on_surface(cb->user, i, &s); if (r) return r; }
        for (j = 0UL; j < s.num_shaders; ++j) {
            mdc3d_shader sh; const MDC3D_U8 *p = ctx->data + s.file_offset + s.ofs_shaders + j * MDC3D_SHADER_SIZE;
            mdc3d_copy_name(sh.name, p, 64UL, 65UL); sh.flags = mdc3d_u32_le(p + 64);
            if (cb && cb->on_shader) { r = cb->on_shader(cb->user, i, j, &sh); if (r) return r; }
        }
        for (j = 0UL; j < s.num_triangles; ++j) {
            mdc3d_triangle tri; const MDC3D_U8 *p = ctx->data + s.file_offset + s.ofs_triangles + j * MDC3D_TRI_SIZE;
            tri.index[0] = mdc3d_u32_le(p + 0); tri.index[1] = mdc3d_u32_le(p + 4); tri.index[2] = mdc3d_u32_le(p + 8);
            if (tri.index[0] >= s.num_vertices || tri.index[1] >= s.num_vertices || tri.index[2] >= s.num_vertices) return MDC3D_E_BAD_COUNT;
            if (cb && cb->on_triangle) { r = cb->on_triangle(cb->user, i, j, &tri); if (r) return r; }
        }
        for (j = 0UL; j < s.num_vertices; ++j) {
            mdc3d_texcoord uv; const MDC3D_U8 *p = ctx->data + s.file_offset + s.ofs_texcoords + j * MDC3D_TEX_SIZE;
            uv.s = mdc3d_f32le_to_fixed(p + 0); uv.t = mdc3d_f32le_to_fixed(p + 4);
            if (cb && cb->on_texcoord) { r = cb->on_texcoord(cb->user, i, j, &uv); if (r) return r; }
        }
        if (emit_vertices) {
            for (j = 0UL; j < ctx->header.num_frames; ++j) {
                r = mdc3d_emit_surface_frame_vertices(ctx, i, j, cb); if (r != MDC3D_OK) return r;
            }
        }
    }
    return MDC3D_OK;
}
