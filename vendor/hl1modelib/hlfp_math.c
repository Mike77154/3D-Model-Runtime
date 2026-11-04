#include "hlfp_internal.h"

#define HLFP_PI_FIXED ((hlfp_fixed)205887)
#define HLFP_HALF_PI_FIXED ((hlfp_fixed)102944)
#define HLFP_TWO_PI_FIXED ((hlfp_fixed)411775)
#define HLFP_TRIG_TABLE_SEGMENTS 256

static const hlfp_fixed hlfp__sin_quarter_table[HLFP_TRIG_TABLE_SEGMENTS + 1] = {
    0, 402, 804, 1206, 1608, 2010, 2412, 2814,
    3216, 3617, 4019, 4420, 4821, 5222, 5623, 6023,
    6424, 6824, 7224, 7623, 8022, 8421, 8820, 9218,
    9616, 10014, 10411, 10808, 11204, 11600, 11996, 12391,
    12785, 13180, 13573, 13966, 14359, 14751, 15143, 15534,
    15924, 16314, 16703, 17091, 17479, 17867, 18253, 18639,
    19024, 19409, 19792, 20175, 20557, 20939, 21320, 21699,
    22078, 22457, 22834, 23210, 23586, 23961, 24335, 24708,
    25080, 25451, 25821, 26190, 26558, 26925, 27291, 27656,
    28020, 28383, 28745, 29106, 29466, 29824, 30182, 30538,
    30893, 31248, 31600, 31952, 32303, 32652, 33000, 33347,
    33692, 34037, 34380, 34721, 35062, 35401, 35738, 36075,
    36410, 36744, 37076, 37407, 37736, 38064, 38391, 38716,
    39040, 39362, 39683, 40002, 40320, 40636, 40951, 41264,
    41576, 41886, 42194, 42501, 42806, 43110, 43412, 43713,
    44011, 44308, 44604, 44898, 45190, 45480, 45769, 46056,
    46341, 46624, 46906, 47186, 47464, 47741, 48015, 48288,
    48559, 48828, 49095, 49361, 49624, 49886, 50146, 50404,
    50660, 50914, 51166, 51417, 51665, 51911, 52156, 52398,
    52639, 52878, 53114, 53349, 53581, 53812, 54040, 54267,
    54491, 54714, 54934, 55152, 55368, 55582, 55794, 56004,
    56212, 56418, 56621, 56823, 57022, 57219, 57414, 57607,
    57798, 57986, 58172, 58356, 58538, 58718, 58896, 59071,
    59244, 59415, 59583, 59750, 59914, 60075, 60235, 60392,
    60547, 60700, 60851, 60999, 61145, 61288, 61429, 61568,
    61705, 61839, 61971, 62101, 62228, 62353, 62476, 62596,
    62714, 62830, 62943, 63054, 63162, 63268, 63372, 63473,
    63572, 63668, 63763, 63854, 63944, 64031, 64115, 64197,
    64277, 64354, 64429, 64501, 64571, 64639, 64704, 64766,
    64827, 64884, 64940, 64993, 65043, 65091, 65137, 65180,
    65220, 65259, 65294, 65328, 65358, 65387, 65413, 65436,
    65457, 65476, 65492, 65505, 65516, 65525, 65531, 65535,
    65536
};

static int hlfp__fixed_abs_to_unsigned(hlfp_fixed v, unsigned int *out, int *sign) {
    if (out == 0 || sign == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    *sign = 1;
    if (v < 0) {
        *sign = -1;
        if (v == INT_MIN) {
            *out = ((unsigned int)INT_MAX) + 1U;
        } else {
            *out = (unsigned int)(-v);
        }
    } else {
        *out = (unsigned int)v;
    }
    return HLFP_OK;
}

static int hlfp__fixed_apply_sign(unsigned int mag, int sign, hlfp_fixed *out) {
    if (out == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    if (sign < 0) {
        if (mag > ((unsigned int)INT_MAX) + 1U) {
            *out = INT_MIN;
            return HLFP_ERR_OVERFLOW;
        }
        if (mag == ((unsigned int)INT_MAX) + 1U) {
            *out = INT_MIN;
        } else {
            *out = -(hlfp_fixed)mag;
        }
    } else {
        if (mag > (unsigned int)INT_MAX) {
            *out = INT_MAX;
            return HLFP_ERR_OVERFLOW;
        }
        *out = (hlfp_fixed)mag;
    }
    return HLFP_OK;
}

static int hlfp__fixed_add(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out) {
    if (out == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        *out = (b > 0) ? INT_MAX : INT_MIN;
        return HLFP_ERR_OVERFLOW;
    }
    *out = a + b;
    return HLFP_OK;
}

static int hlfp__fixed_mul_local(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out) {
    if (out == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }

#if LONG_MAX > 2147483647L
    {
        long tmp;
        tmp = ((long)a * (long)b);
        if (tmp >= 0) {
            tmp += (1L << (HLFP_FIXED_FRAC_BITS - 1));
        } else {
            tmp -= (1L << (HLFP_FIXED_FRAC_BITS - 1));
        }
        tmp >>= HLFP_FIXED_FRAC_BITS;
        if (tmp > (long)INT_MAX) {
            *out = INT_MAX;
            return HLFP_ERR_OVERFLOW;
        }
        if (tmp < (long)INT_MIN) {
            *out = INT_MIN;
            return HLFP_ERR_OVERFLOW;
        }
        *out = (hlfp_fixed)tmp;
        return HLFP_OK;
    }
#else
    {
        int sign_a;
        int sign_b;
        int sign;
        unsigned int ua;
        unsigned int ub;
        unsigned int ai;
        unsigned int af;
        unsigned int bi;
        unsigned int bf;
        unsigned int whole;
        unsigned int frac1;
        unsigned int frac2;
        unsigned int frac3;
        unsigned int mag;
        int rc;

        rc = hlfp__fixed_abs_to_unsigned(a, &ua, &sign_a);
        if (rc != HLFP_OK) {
            return rc;
        }
        rc = hlfp__fixed_abs_to_unsigned(b, &ub, &sign_b);
        if (rc != HLFP_OK) {
            return rc;
        }
        sign = sign_a * sign_b;
        ai = ua >> HLFP_FIXED_FRAC_BITS;
        af = ua & 0xFFFFU;
        bi = ub >> HLFP_FIXED_FRAC_BITS;
        bf = ub & 0xFFFFU;

        if (ai != 0U && bi > (((unsigned int)INT_MAX) >> HLFP_FIXED_FRAC_BITS) / ai) {
            *out = sign < 0 ? INT_MIN : INT_MAX;
            return HLFP_ERR_OVERFLOW;
        }
        whole = (ai * bi) << HLFP_FIXED_FRAC_BITS;
        frac1 = ai * bf;
        frac2 = bi * af;
        frac3 = (unsigned int)((af * bf + 0x8000U) >> HLFP_FIXED_FRAC_BITS);

        mag = whole;
        if (mag > (unsigned int)INT_MAX - frac1) {
            *out = sign < 0 ? INT_MIN : INT_MAX;
            return HLFP_ERR_OVERFLOW;
        }
        mag += frac1;
        if (mag > (unsigned int)INT_MAX - frac2) {
            *out = sign < 0 ? INT_MIN : INT_MAX;
            return HLFP_ERR_OVERFLOW;
        }
        mag += frac2;
        if (mag > (unsigned int)INT_MAX - frac3) {
            *out = sign < 0 ? INT_MIN : INT_MAX;
            return HLFP_ERR_OVERFLOW;
        }
        mag += frac3;
        return hlfp__fixed_apply_sign(mag, sign, out);
    }
#endif
}

int hlfp_fixed_mul(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out) {
    return hlfp__fixed_mul_local(a, b, out);
}

int hlfp_fixed_div(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out) {
    unsigned int ua;
    unsigned int ub;
    int sign_a;
    int sign_b;
    int sign;
    unsigned int q_int;
    unsigned int q_frac;
    unsigned int rem;
    unsigned int mag;
    int i;
    int rc;

    if (out == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    if (b == 0) {
        *out = (a < 0) ? INT_MIN : INT_MAX;
        return HLFP_ERR_DIV_BY_ZERO;
    }

    rc = hlfp__fixed_abs_to_unsigned(a, &ua, &sign_a);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__fixed_abs_to_unsigned(b, &ub, &sign_b);
    if (rc != HLFP_OK) {
        return rc;
    }
    sign = sign_a * sign_b;

    q_int = ua / ub;
    rem = ua % ub;
    if (q_int > ((unsigned int)INT_MAX >> HLFP_FIXED_FRAC_BITS)) {
        *out = sign < 0 ? INT_MIN : INT_MAX;
        return HLFP_ERR_OVERFLOW;
    }

    q_frac = 0U;
    for (i = 0; i < HLFP_FIXED_FRAC_BITS; ++i) {
        rem <<= 1;
        q_frac <<= 1;
        if (rem >= ub) {
            rem -= ub;
            q_frac |= 1U;
        }
    }

    mag = (q_int << HLFP_FIXED_FRAC_BITS) | q_frac;
    return hlfp__fixed_apply_sign(mag, sign, out);
}

static hlfp_fixed hlfp__sin_quarter(hlfp_fixed x) {
    unsigned int scaled;
    unsigned int index;
    unsigned int rem;
    unsigned int t256;
    hlfp_fixed a;
    hlfp_fixed b;
    hlfp_fixed delta;

    if (x <= 0) {
        return 0;
    }
    if (x >= HLFP_HALF_PI_FIXED) {
        return HLFP_FIXED_ONE;
    }

    scaled = ((unsigned int)x) * HLFP_TRIG_TABLE_SEGMENTS;
    index = scaled / (unsigned int)HLFP_HALF_PI_FIXED;
    if (index >= HLFP_TRIG_TABLE_SEGMENTS) {
        return HLFP_FIXED_ONE;
    }
    rem = scaled - index * (unsigned int)HLFP_HALF_PI_FIXED;
    t256 = (rem * 256U) / (unsigned int)HLFP_HALF_PI_FIXED;
    a = hlfp__sin_quarter_table[index];
    b = hlfp__sin_quarter_table[index + 1U];
    delta = b - a;
    return a + (hlfp_fixed)((delta * (hlfp_fixed)t256) >> 8);
}

int hlfp_fixed_sin_cos(hlfp_fixed radians, hlfp_fixed *out_sin, hlfp_fixed *out_cos) {
    hlfp_fixed x;
    hlfp_fixed s;
    hlfp_fixed c;

    if (out_sin == 0 || out_cos == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }

    x = radians;
    while (x < 0) {
        x += HLFP_TWO_PI_FIXED;
    }
    while (x >= HLFP_TWO_PI_FIXED) {
        x -= HLFP_TWO_PI_FIXED;
    }

    if (x < HLFP_HALF_PI_FIXED) {
        s = hlfp__sin_quarter(x);
        c = hlfp__sin_quarter(HLFP_HALF_PI_FIXED - x);
    } else if (x < HLFP_PI_FIXED) {
        x = HLFP_PI_FIXED - x;
        s = hlfp__sin_quarter(x);
        c = -hlfp__sin_quarter(HLFP_HALF_PI_FIXED - x);
    } else if (x < HLFP_PI_FIXED + HLFP_HALF_PI_FIXED) {
        x = x - HLFP_PI_FIXED;
        s = -hlfp__sin_quarter(x);
        c = -hlfp__sin_quarter(HLFP_HALF_PI_FIXED - x);
    } else {
        x = HLFP_TWO_PI_FIXED - x;
        s = -hlfp__sin_quarter(x);
        c = hlfp__sin_quarter(HLFP_HALF_PI_FIXED - x);
    }

    *out_sin = s;
    *out_cos = c;
    return HLFP_OK;
}

void hlfp_mat3x4_identity(hlfp_mat3x4 *out) {
    if (out == 0) {
        return;
    }
    hlfp__zero(out, sizeof(*out));
    out->m[0][0] = HLFP_FIXED_ONE;
    out->m[1][1] = HLFP_FIXED_ONE;
    out->m[2][2] = HLFP_FIXED_ONE;
}

int hlfp_angles_to_mat3x4(const hlfp_vec3 *rot, const hlfp_vec3 *pos, hlfp_mat3x4 *out, hlfp_error *err) {
    hlfp_fixed sp;
    hlfp_fixed cp;
    hlfp_fixed sy;
    hlfp_fixed cy;
    hlfp_fixed sr;
    hlfp_fixed cr;
    hlfp_fixed crcy;
    hlfp_fixed crsy;
    hlfp_fixed srcy;
    hlfp_fixed srsy;
    hlfp_fixed t1;
    hlfp_fixed t2;
    int rc;

    if (rot == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null matrix build input");
    }

    rc = hlfp_fixed_sin_cos(rot->x, &sp, &cp);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "failed to evaluate pitch sine/cosine");
    }
    rc = hlfp_fixed_sin_cos(rot->y, &sy, &cy);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "failed to evaluate yaw sine/cosine");
    }
    rc = hlfp_fixed_sin_cos(rot->z, &sr, &cr);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "failed to evaluate roll sine/cosine");
    }

    rc = hlfp_fixed_mul(cp, cy, &out->m[0][0]);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
    }
    rc = hlfp_fixed_mul(cp, sy, &out->m[1][0]);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
    }
    out->m[2][0] = -sp;

    rc = hlfp_fixed_mul(cr, cy, &crcy);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
    rc = hlfp_fixed_mul(cr, sy, &crsy);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
    rc = hlfp_fixed_mul(sr, cy, &srcy);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
    rc = hlfp_fixed_mul(sr, sy, &srsy);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");

    rc = hlfp_fixed_mul(sp, srcy, &t1);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
    rc = hlfp__fixed_add(t1, -crsy, &out->m[0][1]);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");

    rc = hlfp_fixed_mul(sp, srsy, &t1);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
    rc = hlfp__fixed_add(t1, crcy, &out->m[1][1]);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");

    rc = hlfp_fixed_mul(sr, cp, &out->m[2][1]);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");

    rc = hlfp_fixed_mul(sp, crcy, &t1);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
    rc = hlfp__fixed_add(t1, srsy, &out->m[0][2]);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");

    rc = hlfp_fixed_mul(sp, crsy, &t1);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
    rc = hlfp__fixed_add(t1, -srcy, &out->m[1][2]);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");

    rc = hlfp_fixed_mul(cr, cp, &out->m[2][2]);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");

    out->m[0][3] = pos != 0 ? pos->x : 0;
    out->m[1][3] = pos != 0 ? pos->y : 0;
    out->m[2][3] = pos != 0 ? pos->z : 0;
    HLFP_UNUSED(t2);
    return HLFP_OK;
}


static int hlfp__mul3(hlfp_fixed a, hlfp_fixed b, hlfp_fixed c, hlfp_fixed *out) {
    hlfp_fixed ab;
    int rc;
    rc = hlfp_fixed_mul(a, b, &ab);
    if (rc != HLFP_OK) return rc;
    return hlfp_fixed_mul(ab, c, out);
}

int hlfp_goldsrc_angles_to_quat(const hlfp_vec3 *rot, hlfp_quat *out, hlfp_error *err) {
    hlfp_fixed sx;
    hlfp_fixed cx;
    hlfp_fixed sy;
    hlfp_fixed cy;
    hlfp_fixed sz;
    hlfp_fixed cz;
    hlfp_fixed a;
    hlfp_fixed b;
    int rc;

    if (rot == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null GoldSrc quaternion input");
    }
    rc = hlfp_fixed_sin_cos(rot->z >> 1, &sz, &cz);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc Z half-angle failed");
    rc = hlfp_fixed_sin_cos(rot->y >> 1, &sy, &cy);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc Y half-angle failed");
    rc = hlfp_fixed_sin_cos(rot->x >> 1, &sx, &cx);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc X half-angle failed");

    rc = hlfp__mul3(sx, cy, cz, &a);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__mul3(cx, sy, sz, &b);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__fixed_add(a, -b, &out->x);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");

    rc = hlfp__mul3(cx, sy, cz, &a);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__mul3(sx, cy, sz, &b);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__fixed_add(a, b, &out->y);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");

    rc = hlfp__mul3(cx, cy, sz, &a);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__mul3(sx, sy, cz, &b);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__fixed_add(a, -b, &out->z);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");

    rc = hlfp__mul3(cx, cy, cz, &a);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__mul3(sx, sy, sz, &b);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__fixed_add(a, b, &out->w);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    return HLFP_OK;
}

static int hlfp__quat_dot_math(const hlfp_quat *a, const hlfp_quat *b, hlfp_fixed *out) {
    hlfp_fixed t;
    hlfp_fixed sum;
    int rc;
    rc = hlfp_fixed_mul(a->x, b->x, &sum);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_fixed_mul(a->y, b->y, &t);
    if (rc != HLFP_OK) return rc;
    rc = hlfp__fixed_add(sum, t, &sum);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_fixed_mul(a->z, b->z, &t);
    if (rc != HLFP_OK) return rc;
    rc = hlfp__fixed_add(sum, t, &sum);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_fixed_mul(a->w, b->w, &t);
    if (rc != HLFP_OK) return rc;
    return hlfp__fixed_add(sum, t, out);
}

static hlfp_fixed hlfp__clamp_unit_math(hlfp_fixed v) {
    if (v < -HLFP_FIXED_ONE) return -HLFP_FIXED_ONE;
    if (v > HLFP_FIXED_ONE) return HLFP_FIXED_ONE;
    return v;
}

static int hlfp__fixed_sqrt_math(hlfp_fixed x, hlfp_fixed *out) {
    hlfp_fixed guess;
    hlfp_fixed divv;
    hlfp_fixed sum;
    int i;
    int rc;
    if (out == 0) return HLFP_ERR_BAD_ARGUMENT;
    if (x < 0) return HLFP_ERR_BAD_ARGUMENT;
    if (x == 0) { *out = 0; return HLFP_OK; }
    guess = (x > HLFP_FIXED_ONE) ? x : HLFP_FIXED_ONE;
    for (i = 0; i < 12; ++i) {
        rc = hlfp_fixed_div(x, guess, &divv);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__fixed_add(guess, divv, &sum);
        if (rc != HLFP_OK) return rc;
        guess = sum >> 1;
        if (guess == 0) guess = 1;
    }
    *out = guess;
    return HLFP_OK;
}

static int hlfp__quat_normalize_math(hlfp_quat *q, hlfp_error *err) {
    hlfp_fixed len2;
    hlfp_fixed len;
    int rc;
    rc = hlfp__quat_dot_math(q, q, &len2);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize overflow");
    if (len2 <= 0) {
        q->x = 0;
        q->y = 0;
        q->z = 0;
        q->w = HLFP_FIXED_ONE;
        return HLFP_OK;
    }
    rc = hlfp__fixed_sqrt_math(len2, &len);
    if (rc != HLFP_OK || len == 0) return hlfp__set_error(err, rc != HLFP_OK ? rc : HLFP_ERR_DIV_BY_ZERO, 0, 0, "quaternion normalize sqrt failed");
    rc = hlfp_fixed_div(q->x, len, &q->x); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize divide failed");
    rc = hlfp_fixed_div(q->y, len, &q->y); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize divide failed");
    rc = hlfp_fixed_div(q->z, len, &q->z); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize divide failed");
    rc = hlfp_fixed_div(q->w, len, &q->w); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize divide failed");
    return HLFP_OK;
}

static int hlfp__fixed_acos_math(hlfp_fixed x, hlfp_fixed *out) {
    hlfp_fixed lo;
    hlfp_fixed hi;
    hlfp_fixed mid;
    hlfp_fixed sn;
    hlfp_fixed cs;
    int i;
    int rc;
    if (out == 0) return HLFP_ERR_BAD_ARGUMENT;
    x = hlfp__clamp_unit_math(x);
    lo = 0;
    hi = HLFP_PI_FIXED;
    for (i = 0; i < 24; ++i) {
        mid = lo + ((hi - lo) >> 1);
        rc = hlfp_fixed_sin_cos(mid, &sn, &cs);
        if (rc != HLFP_OK) return rc;
        if (cs > x) lo = mid;
        else hi = mid;
    }
    *out = lo + ((hi - lo) >> 1);
    return HLFP_OK;
}

int hlfp_quat_slerp(const hlfp_quat *a, const hlfp_quat *b, hlfp_fixed t, hlfp_quat *out, hlfp_error *err) {
    hlfp_quat qb;
    hlfp_quat alt;
    hlfp_fixed d;
    hlfp_fixed sum_minus;
    hlfp_fixed sum_plus;
    hlfp_fixed cosom;
    hlfp_fixed omega;
    hlfp_fixed sinom;
    hlfp_fixed sinv;
    hlfp_fixed cosv;
    hlfp_fixed sclp;
    hlfp_fixed sclq;
    hlfp_fixed tmp0;
    hlfp_fixed tmp1;
    int rc;

    if (a == 0 || b == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion slerp input");
    }
    if (t < 0) t = 0;
    if (t > HLFP_FIXED_ONE) t = HLFP_FIXED_ONE;
    qb = *b;
    sum_minus = 0;
    sum_plus = 0;
#define HLFP_QDIST(comp) \
    rc = hlfp__fixed_add(a->comp, -qb.comp, &d); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow"); \
    rc = hlfp_fixed_mul(d, d, &d); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow"); \
    rc = hlfp__fixed_add(sum_minus, d, &sum_minus); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow"); \
    rc = hlfp__fixed_add(a->comp, qb.comp, &d); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow"); \
    rc = hlfp_fixed_mul(d, d, &d); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow"); \
    rc = hlfp__fixed_add(sum_plus, d, &sum_plus); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow")
    HLFP_QDIST(x);
    HLFP_QDIST(y);
    HLFP_QDIST(z);
    HLFP_QDIST(w);
#undef HLFP_QDIST
    if (sum_minus > sum_plus) {
        qb.x = -qb.x;
        qb.y = -qb.y;
        qb.z = -qb.z;
        qb.w = -qb.w;
    }
    rc = hlfp__quat_dot_math(a, &qb, &cosom);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp dot overflow");
    cosom = hlfp__clamp_unit_math(cosom);
    if (HLFP_FIXED_ONE + cosom > 1) {
        if (HLFP_FIXED_ONE - cosom > 1) {
            rc = hlfp__fixed_acos_math(cosom, &omega);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp acos failed");
            rc = hlfp_fixed_sin_cos(omega, &sinom, &cosv);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp sin failed");
            if (sinom == 0) {
                sclp = HLFP_FIXED_ONE - t;
                sclq = t;
            } else {
                rc = hlfp_fixed_mul(HLFP_FIXED_ONE - t, omega, &tmp0);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
                rc = hlfp_fixed_sin_cos(tmp0, &sinv, &cosv);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp sin failed");
                rc = hlfp_fixed_div(sinv, sinom, &sclp);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp divide failed");
                rc = hlfp_fixed_mul(t, omega, &tmp0);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
                rc = hlfp_fixed_sin_cos(tmp0, &sinv, &cosv);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp sin failed");
                rc = hlfp_fixed_div(sinv, sinom, &sclq);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp divide failed");
            }
        } else {
            sclp = HLFP_FIXED_ONE - t;
            sclq = t;
        }
        rc = hlfp_fixed_mul(a->x, sclp, &tmp0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(qb.x, sclq, &tmp1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp__fixed_add(tmp0, tmp1, &out->x); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(a->y, sclp, &tmp0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(qb.y, sclq, &tmp1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp__fixed_add(tmp0, tmp1, &out->y); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(a->z, sclp, &tmp0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(qb.z, sclq, &tmp1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp__fixed_add(tmp0, tmp1, &out->z); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(a->w, sclp, &tmp0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(qb.w, sclq, &tmp1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp__fixed_add(tmp0, tmp1, &out->w); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
    } else {
        alt.x = -qb.y;
        alt.y = qb.x;
        alt.z = -qb.w;
        alt.w = qb.z;
        rc = hlfp_fixed_mul(HLFP_FIXED_ONE - t, HLFP_HALF_PI_FIXED, &tmp0);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_sin_cos(tmp0, &sclp, &cosv);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp sin failed");
        rc = hlfp_fixed_mul(t, HLFP_HALF_PI_FIXED, &tmp0);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_sin_cos(tmp0, &sclq, &cosv);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp sin failed");
        rc = hlfp_fixed_mul(a->x, sclp, &tmp0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(alt.x, sclq, &tmp1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp__fixed_add(tmp0, tmp1, &out->x); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(a->y, sclp, &tmp0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(alt.y, sclq, &tmp1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp__fixed_add(tmp0, tmp1, &out->y); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(a->z, sclp, &tmp0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp_fixed_mul(alt.z, sclq, &tmp1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        rc = hlfp__fixed_add(tmp0, tmp1, &out->z); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp overflow");
        out->w = a->w;
    }
    return hlfp__quat_normalize_math(out, err);
}

int hlfp_quat_to_mat3x4(const hlfp_quat *q, const hlfp_vec3 *pos, hlfp_mat3x4 *out, hlfp_error *err) {
    hlfp_fixed xx;
    hlfp_fixed yy;
    hlfp_fixed zz;
    hlfp_fixed xy;
    hlfp_fixed xz;
    hlfp_fixed yz;
    hlfp_fixed wx;
    hlfp_fixed wy;
    hlfp_fixed wz;
    hlfp_fixed two_xx;
    hlfp_fixed two_yy;
    hlfp_fixed two_zz;
    hlfp_fixed two_xy;
    hlfp_fixed two_xz;
    hlfp_fixed two_yz;
    hlfp_fixed two_wx;
    hlfp_fixed two_wy;
    hlfp_fixed two_wz;
    if (q == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion matrix output");
    }
    if (hlfp_fixed_mul(q->x, q->x, &xx) != HLFP_OK) return hlfp__set_error(err, HLFP_ERR_OVERFLOW, 0, 0, "quaternion matrix overflow");
    if (hlfp_fixed_mul(q->y, q->y, &yy) != HLFP_OK) return hlfp__set_error(err, HLFP_ERR_OVERFLOW, 0, 0, "quaternion matrix overflow");
    if (hlfp_fixed_mul(q->z, q->z, &zz) != HLFP_OK) return hlfp__set_error(err, HLFP_ERR_OVERFLOW, 0, 0, "quaternion matrix overflow");
    if (hlfp_fixed_mul(q->x, q->y, &xy) != HLFP_OK) return hlfp__set_error(err, HLFP_ERR_OVERFLOW, 0, 0, "quaternion matrix overflow");
    if (hlfp_fixed_mul(q->x, q->z, &xz) != HLFP_OK) return hlfp__set_error(err, HLFP_ERR_OVERFLOW, 0, 0, "quaternion matrix overflow");
    if (hlfp_fixed_mul(q->y, q->z, &yz) != HLFP_OK) return hlfp__set_error(err, HLFP_ERR_OVERFLOW, 0, 0, "quaternion matrix overflow");
    if (hlfp_fixed_mul(q->w, q->x, &wx) != HLFP_OK) return hlfp__set_error(err, HLFP_ERR_OVERFLOW, 0, 0, "quaternion matrix overflow");
    if (hlfp_fixed_mul(q->w, q->y, &wy) != HLFP_OK) return hlfp__set_error(err, HLFP_ERR_OVERFLOW, 0, 0, "quaternion matrix overflow");
    if (hlfp_fixed_mul(q->w, q->z, &wz) != HLFP_OK) return hlfp__set_error(err, HLFP_ERR_OVERFLOW, 0, 0, "quaternion matrix overflow");
    two_xx = xx << 1;
    two_yy = yy << 1;
    two_zz = zz << 1;
    two_xy = xy << 1;
    two_xz = xz << 1;
    two_yz = yz << 1;
    two_wx = wx << 1;
    two_wy = wy << 1;
    two_wz = wz << 1;
    out->m[0][0] = HLFP_FIXED_ONE - two_yy - two_zz;
    out->m[1][0] = two_xy + two_wz;
    out->m[2][0] = two_xz - two_wy;
    out->m[0][1] = two_xy - two_wz;
    out->m[1][1] = HLFP_FIXED_ONE - two_xx - two_zz;
    out->m[2][1] = two_yz + two_wx;
    out->m[0][2] = two_xz + two_wy;
    out->m[1][2] = two_yz - two_wx;
    out->m[2][2] = HLFP_FIXED_ONE - two_xx - two_yy;
    out->m[0][3] = pos != 0 ? pos->x : 0;
    out->m[1][3] = pos != 0 ? pos->y : 0;
    out->m[2][3] = pos != 0 ? pos->z : 0;
    return HLFP_OK;
}

int hlfp_goldsrc_angles_to_bone_mat3x4(const hlfp_vec3 *rot, const hlfp_vec3 *pos, hlfp_mat3x4 *out, hlfp_error *err) {
    hlfp_quat q;
    int rc;
    rc = hlfp_goldsrc_angles_to_quat(rot, &q, err);
    if (rc != HLFP_OK) return rc;
    return hlfp_quat_to_mat3x4(&q, pos, out, err);
}

int hlfp_mat3x4_concat(const hlfp_mat3x4 *parent, const hlfp_mat3x4 *child, hlfp_mat3x4 *out, hlfp_error *err) {
    hlfp_mat3x4 tmp;
    int i;
    int j;
    int rc;
    hlfp_fixed p0;
    hlfp_fixed p1;
    hlfp_fixed p2;
    hlfp_fixed sum;
    hlfp_fixed t;

    if (parent == 0 || child == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null matrix concat input");
    }

    hlfp__zero(&tmp, sizeof(tmp));
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            rc = hlfp_fixed_mul(parent->m[i][0], child->m[0][j], &p0);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
            rc = hlfp_fixed_mul(parent->m[i][1], child->m[1][j], &p1);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
            rc = hlfp_fixed_mul(parent->m[i][2], child->m[2][j], &p2);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
            rc = hlfp__fixed_add(p0, p1, &sum);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
            rc = hlfp__fixed_add(sum, p2, &tmp.m[i][j]);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
        }
        rc = hlfp_fixed_mul(parent->m[i][0], child->m[0][3], &p0);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
        rc = hlfp_fixed_mul(parent->m[i][1], child->m[1][3], &p1);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
        rc = hlfp_fixed_mul(parent->m[i][2], child->m[2][3], &p2);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
        rc = hlfp__fixed_add(p0, p1, &sum);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
        rc = hlfp__fixed_add(sum, p2, &t);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
        rc = hlfp__fixed_add(t, parent->m[i][3], &tmp.m[i][3]);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix overflow");
    }

    *out = tmp;
    return HLFP_OK;
}

int hlfp_mdl_pose_to_local_matrices(const hlfp_mdl_pose_set *poses, hlfp_mat3x4 *out, int out_cap, hlfp_error *err) {
    int i;
    int rc;

    if (poses == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null pose matrix output");
    }
    if (poses->num_bones < 0 || poses->num_bones > out_cap) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "matrix output capacity exceeded");
    }

    for (i = 0; i < poses->num_bones; ++i) {
        hlfp_mat3x4_identity(&out[i]);
    }
    for (i = 0; i < poses->num_bones; ++i) {
        if (poses->poses[i].bone < 0 || poses->poses[i].bone >= poses->num_bones) {
            return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "pose bone index out of range");
        }
        rc = hlfp_goldsrc_angles_to_bone_mat3x4(&poses->poses[i].rot, &poses->poses[i].pos, &out[poses->poses[i].bone], err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    return HLFP_OK;
}

static int hlfp__mdl_world_recursive(const hlfp_mdl *mdl,
                                     const hlfp_mat3x4 *local,
                                     hlfp_mat3x4 *world,
                                     int *state,
                                     int bone,
                                     hlfp_error *err) {
    int parent;
    int rc;

    if (state[bone] == 2) {
        return HLFP_OK;
    }
    if (state[bone] == 1) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "bone parent cycle detected");
    }
    state[bone] = 1;
    parent = mdl->bones[bone].parent;
    if (parent < 0) {
        world[bone] = local[bone];
    } else {
        if (parent >= mdl->num_bones) {
            return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "bone parent index out of range");
        }
        rc = hlfp__mdl_world_recursive(mdl, local, world, state, parent, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        rc = hlfp_mat3x4_concat(&world[parent], &local[bone], &world[bone], err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    state[bone] = 2;
    return HLFP_OK;
}

int hlfp_mdl_pose_to_world_matrices(const hlfp_mdl *mdl, const hlfp_mdl_pose_set *poses, hlfp_mat3x4 *out, int out_cap, hlfp_error *err) {
    hlfp_mat3x4 local[HLFP_MDL_MAX_BONES];
    int state[HLFP_MDL_MAX_BONES];
    int i;
    int rc;

    if (mdl == 0 || poses == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null world matrix request");
    }
    if (mdl->num_bones != poses->num_bones) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "pose set bone count does not match model");
    }
    if (mdl->num_bones > out_cap) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "world matrix output capacity exceeded");
    }

    rc = hlfp_mdl_pose_to_local_matrices(poses, local, HLFP_MDL_MAX_BONES, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    for (i = 0; i < mdl->num_bones; ++i) {
        state[i] = 0;
        hlfp_mat3x4_identity(&out[i]);
    }
    for (i = 0; i < mdl->num_bones; ++i) {
        rc = hlfp__mdl_world_recursive(mdl, local, out, state, i, err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    return HLFP_OK;
}

int hlfp_mdl_decode_sequence_world_matrices(const hlfp_mdl *mdl, int sequence_index, int blend_index, int frame,
                                            hlfp_mat3x4 *out, int out_cap, hlfp_error *err) {
    hlfp_mdl_pose_set poses;
    int rc;

    rc = hlfp_mdl_decode_sequence_frame(mdl, sequence_index, blend_index, frame, &poses, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_pose_to_world_matrices(mdl, &poses, out, out_cap, err);
}


int hlfp_mdl_decode_sequence_world_matrices_fixed(const hlfp_mdl *mdl, int sequence_index, int blend_index,
                                                  hlfp_fixed frame_position,
                                                  hlfp_mat3x4 *out, int out_cap, hlfp_error *err) {
    hlfp_mdl_pose_set poses;
    int rc;

    rc = hlfp_mdl_decode_sequence_frame_fixed(mdl, sequence_index, blend_index, frame_position, &poses, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_pose_to_world_matrices(mdl, &poses, out, out_cap, err);
}

int hlfp_mdl_decode_sequence_world_matrices_time(const hlfp_mdl *mdl, int sequence_index, int blend_index,
                                                 hlfp_fixed seconds,
                                                 hlfp_mat3x4 *out, int out_cap, hlfp_error *err) {
    hlfp_mdl_pose_set poses;
    int rc;

    rc = hlfp_mdl_decode_sequence_time(mdl, sequence_index, blend_index, seconds, &poses, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_pose_to_world_matrices(mdl, &poses, out, out_cap, err);
}
