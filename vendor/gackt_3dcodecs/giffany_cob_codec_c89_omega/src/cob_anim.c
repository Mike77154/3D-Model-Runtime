#include "cob_private.h"

/* Generic trueSpace Bone/Chan animation helpers. */

static unsigned long an_abs_fixed(cob_fixed v)
{
    if (v == COB_FIXED_MIN) return 2147483648UL;
    if (v < 0) return (unsigned long)(-v);
    return (unsigned long)v;
}

static cob_fixed an_from_mag_sign(unsigned long mag, int sign)
{
    if (sign < 0) {
        if (mag >= 2147483648UL) return COB_FIXED_MIN;
        return -(cob_fixed)mag;
    }
    if (mag > (unsigned long)COB_FIXED_MAX) return COB_FIXED_MAX;
    return (cob_fixed)mag;
}

static cob_fixed an_mul(cob_fixed a, cob_fixed b)
{
    unsigned long am;
    unsigned long bm;
    unsigned long aw;
    unsigned long af;
    unsigned long bw;
    unsigned long bf;
    unsigned long out;
    int sign;
    sign = 1;
    if (a < 0) sign = -sign;
    if (b < 0) sign = -sign;
    am = an_abs_fixed(a);
    bm = an_abs_fixed(b);
    aw = am >> 16;
    af = am & 65535UL;
    bw = bm >> 16;
    bf = bm & 65535UL;
    if (aw != 0UL && bw > 32767UL / aw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out = (aw * bw) << 16;
    if (aw != 0UL && bf > ((unsigned long)COB_FIXED_MAX - out) / aw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out += aw * bf;
    if (bw != 0UL && af > ((unsigned long)COB_FIXED_MAX - out) / bw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out += bw * af;
    out += (af * bf) >> 16;
    return an_from_mag_sign(out, sign);
}

static cob_fixed an_div(cob_fixed num, cob_fixed den)
{
    unsigned long n;
    unsigned long d;
    unsigned long whole;
    unsigned long rem;
    unsigned long out;
    int sign;
    if (den == 0) return 0;
    sign = 1;
    if (num < 0) sign = -sign;
    if (den < 0) sign = -sign;
    n = an_abs_fixed(num);
    d = an_abs_fixed(den);
    whole = n / d;
    rem = n % d;
    if (whole > 32767UL) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out = (whole << 16) + ((rem << 16) / d);
    return an_from_mag_sign(out, sign);
}

static cob_fixed an_lerp(cob_fixed a, cob_fixed b, cob_fixed t)
{
    return a + an_mul(b - a, t);
}

int cob_anim_sample_channel(const cob_scene *scene, unsigned long channel_index, cob_fixed time, cob_fixed out_value[4])
{
    const cob_anim_channel *ch;
    const cob_anim_key *k0;
    const cob_anim_key *k1;
    unsigned long i;
    unsigned long local;
    cob_fixed t;
    int c;

    if (scene == 0 || out_value == 0) return COB_ERR_ARGUMENT;
    if (channel_index >= scene->anim_channel_count) return COB_ERR_ARGUMENT;
    ch = &scene->anim_channels[channel_index];
    if (ch->first_key + ch->key_count > scene->anim_key_count) return COB_ERR_PARSE;
    for (c = 0; c < 4; ++c) out_value[c] = 0;
    if (ch->key_count == 0UL) return COB_ERR_PARSE;
    k0 = &scene->anim_keys[ch->first_key];
    if (time <= k0->time || ch->key_count == 1UL) {
        for (c = 0; c < 4; ++c) out_value[c] = k0->value[c];
        return COB_OK;
    }
    for (local = 1UL; local < ch->key_count; ++local) {
        k1 = &scene->anim_keys[ch->first_key + local];
        if (time <= k1->time) {
            k0 = &scene->anim_keys[ch->first_key + local - 1UL];
            t = an_div(time - k0->time, k1->time - k0->time);
            for (i = 0; i < 4UL; ++i) {
                out_value[i] = an_lerp(k0->value[i], k1->value[i], t);
            }
            return COB_OK;
        }
    }
    k1 = &scene->anim_keys[ch->first_key + ch->key_count - 1UL];
    for (c = 0; c < 4; ++c) out_value[c] = k1->value[c];
    return COB_OK;
}

int cob_anim_apply_sample_to_transform(cob_fixed transform[16], cob_u8 semantic, const cob_fixed value[4])
{
    if (transform == 0 || value == 0) return COB_ERR_ARGUMENT;
    if (semantic == 1U) {
        transform[3] = value[0];
        transform[7] = value[1];
        transform[11] = value[2];
    } else if (semantic == 3U) {
        transform[0] = value[0];
        transform[5] = value[1];
        transform[10] = value[2];
    } else if (semantic == 4U) {
        transform[3] = value[0];
    } else if (semantic == 2U) {
        cob_fixed rot[16];
        int i;
        if (value[3] != 0) {
            if (cob_anim_make_quat_matrix(value, rot) != COB_OK) return COB_ERR_UNSUPPORTED_FORMAT;
        } else {
            if (cob_anim_make_euler_xyz_matrix(value[0], value[1], value[2], rot) != COB_OK) return COB_ERR_UNSUPPORTED_FORMAT;
        }
        for (i = 0; i < 3; ++i) {
            transform[i] = rot[i];
            transform[4 + i] = rot[4 + i];
            transform[8 + i] = rot[8 + i];
        }
    }
    return COB_OK;
}
