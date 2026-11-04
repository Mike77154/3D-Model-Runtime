/* xp_semantics.c - DirectX .x runtime semantics helpers
   C89, fixed-point only, no heap.
*/
#include "xp.h"

#if ULONG_MAX == 0xffffffffUL
typedef unsigned long xp_sem_u32;
#define XP_SEM_U32_MAX 0xffffffffUL
#else
typedef unsigned int xp_sem_u32;
#define XP_SEM_U32_MAX 0xffffffffU
#endif

static xp_sem_u32 xp_fx_mag(xp_fx32 v) {
    if (v < 0) return (xp_sem_u32)(-(v + (xp_fx32)1)) + (xp_sem_u32)1;
    return (xp_sem_u32)v;
}

static xp_fx32 xp_fx_neg_sat(xp_fx32 v) {
    if (v == XP_FX_MIN) return XP_FX_MAX;
    return (xp_fx32)(-v);
}

static xp_fx32 xp_fx_add_sat(xp_fx32 a, xp_fx32 b) {
    if (b > 0 && a > (xp_fx32)(XP_FX_MAX - b)) return XP_FX_MAX;
    if (b < 0 && a < (xp_fx32)(XP_FX_MIN - b)) return XP_FX_MIN;
    return (xp_fx32)(a + b);
}

static xp_fx32 xp_fx_mul_sat(xp_fx32 a, xp_fx32 b) {
    xp_sem_u32 ua, ub, ahi, alo, bhi, blo;
    xp_sem_u32 hh, term, mag, limit;
    int neg;
    if (a == 0 || b == 0) return (xp_fx32)0;
    neg = ((a < 0) != (b < 0));
    ua = xp_fx_mag(a);
    ub = xp_fx_mag(b);
    ahi = ua >> 16;
    alo = ua & (xp_sem_u32)0xffffU;
    bhi = ub >> 16;
    blo = ub & (xp_sem_u32)0xffffU;
    limit = neg ? (xp_sem_u32)0x80000000UL : (xp_sem_u32)0x7fffffffUL;

    hh = ahi * bhi;
    if (hh > (limit >> 16)) return neg ? XP_FX_MIN : XP_FX_MAX;
    mag = hh << 16;

    term = ahi * blo;
    if (term > limit - mag) return neg ? XP_FX_MIN : XP_FX_MAX;
    mag += term;
    term = alo * bhi;
    if (term > limit - mag) return neg ? XP_FX_MIN : XP_FX_MAX;
    mag += term;
    term = (alo * blo) >> 16;
    if (term > limit - mag) return neg ? XP_FX_MIN : XP_FX_MAX;
    mag += term;

    if (neg) {
        if (mag == (xp_sem_u32)0x80000000UL) return XP_FX_MIN;
        return (xp_fx32)(-(xp_fx32)mag);
    }
    return (xp_fx32)mag;
}

xp_anim_key_semantic xp_animation_key_semantic_kind(int raw_key_type, int n_values,
                                                     unsigned int* out_flags) {
    unsigned int flags;
    xp_anim_key_semantic sem;
    flags = XP_ANIM_KEY_FLAG_NONE;
    sem = XP_ANIM_SEM_UNKNOWN;
    if (raw_key_type == 0 && n_values == 4) sem = XP_ANIM_SEM_ROTATION;
    else if (raw_key_type == 1 && n_values == 3) sem = XP_ANIM_SEM_SCALE;
    else if (raw_key_type == 2 && n_values == 3) sem = XP_ANIM_SEM_POSITION;
    else if (raw_key_type == 3 && n_values == 16) sem = XP_ANIM_SEM_MATRIX;
    else if (raw_key_type == 4 && n_values == 16) {
        sem = XP_ANIM_SEM_MATRIX;
        flags |= XP_ANIM_KEY_FLAG_LEGACY_TYPE4;
    }
    if (sem != XP_ANIM_SEM_UNKNOWN) flags |= XP_ANIM_KEY_FLAG_ABSOLUTE_LOCAL;
    if (out_flags) *out_flags = flags;
    return sem;
}

void xp_mat4_identity(xp_fx32 out16[16]) {
    int i;
    if (!out16) return;
    for (i = 0; i < 16; ++i) out16[i] = (xp_fx32)0;
    out16[0] = XP_FX_ONE;
    out16[5] = XP_FX_ONE;
    out16[10] = XP_FX_ONE;
    out16[15] = XP_FX_ONE;
}

void xp_mat4_mul_dx(const xp_fx32 a16[16], const xp_fx32 b16[16], xp_fx32 out16[16]) {
    xp_fx32 t[16];
    int r, c, k;
    if (!a16 || !b16 || !out16) return;
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            xp_fx32 acc;
            acc = (xp_fx32)0;
            for (k = 0; k < 4; ++k) {
                acc = xp_fx_add_sat(acc, xp_fx_mul_sat(a16[r * 4 + k], b16[k * 4 + c]));
            }
            t[r * 4 + c] = acc;
        }
    }
    for (r = 0; r < 16; ++r) out16[r] = t[r];
}

void xp_skin_final_matrix(const xp_fx32 inverse_bind16[16],
                          const xp_fx32 bone_combined16[16], xp_fx32 out16[16]) {
    xp_mat4_mul_dx(inverse_bind16, bone_combined16, out16);
}

void xp_transform_point_dx(const xp_fx32 m16[16], xp_fx32 x, xp_fx32 y, xp_fx32 z,
                           xp_fx32* out_x, xp_fx32* out_y, xp_fx32* out_z) {
    xp_fx32 tx, ty, tz;
    if (!m16) return;
    tx = xp_fx_add_sat(xp_fx_add_sat(xp_fx_mul_sat(x, m16[0]), xp_fx_mul_sat(y, m16[4])),
                       xp_fx_add_sat(xp_fx_mul_sat(z, m16[8]), m16[12]));
    ty = xp_fx_add_sat(xp_fx_add_sat(xp_fx_mul_sat(x, m16[1]), xp_fx_mul_sat(y, m16[5])),
                       xp_fx_add_sat(xp_fx_mul_sat(z, m16[9]), m16[13]));
    tz = xp_fx_add_sat(xp_fx_add_sat(xp_fx_mul_sat(x, m16[2]), xp_fx_mul_sat(y, m16[6])),
                       xp_fx_add_sat(xp_fx_mul_sat(z, m16[10]), m16[14]));
    if (out_x) *out_x = tx;
    if (out_y) *out_y = ty;
    if (out_z) *out_z = tz;
}

void xp_uv_dx_to_gl(xp_fx32 u, xp_fx32 v, xp_fx32* out_u, xp_fx32* out_v) {
    if (out_u) *out_u = u;
    if (out_v) *out_v = xp_fx_add_sat(XP_FX_ONE, xp_fx_neg_sat(v));
}
