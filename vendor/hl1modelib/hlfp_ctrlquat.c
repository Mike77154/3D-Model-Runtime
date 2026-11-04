#include "hlfp_internal.h"

#include <limits.h>

#define HLFP_CTRLQUAT_PI_FIXED ((hlfp_fixed)205887)
#define HLFP_CTRLQUAT_HALF_PI_FIXED ((hlfp_fixed)102944)
#define HLFP_CTRLQUAT_EPSILON ((hlfp_fixed)1)
#define HLFP_CTRLQUAT_ANIMOFF_SIZE 12

static int hlfp__fixed_add_local(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out) {
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

static hlfp_fixed hlfp__fixed_clamp01(hlfp_fixed v) {
    if (v < 0) return 0;
    if (v > HLFP_FIXED_ONE) return HLFP_FIXED_ONE;
    return v;
}

static int hlfp__fixed_sqrt(hlfp_fixed x, hlfp_fixed *out) {
    if (out == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    if (x < 0) {
        *out = 0;
        return HLFP_ERR_BAD_ARGUMENT;
    }
    if (x == 0) {
        *out = 0;
        return HLFP_OK;
    }
#if ULONG_MAX > 0xFFFFFFFFUL
    {
        unsigned long lo;
        unsigned long hi;
        unsigned long mid;
        unsigned long best;
        unsigned long sq;
        unsigned long target;
        target = (unsigned long)(unsigned int)x;
        lo = 0UL;
        hi = (unsigned long)(target > (unsigned long)HLFP_FIXED_ONE ? target : (unsigned long)HLFP_FIXED_ONE);
        best = 0UL;
        while (lo <= hi) {
            mid = lo + ((hi - lo) >> 1);
            sq = (mid * mid) >> HLFP_FIXED_FRAC_BITS;
            if (sq <= target) {
                best = mid;
                lo = mid + 1UL;
            } else {
                if (mid == 0UL) {
                    break;
                }
                hi = mid - 1UL;
            }
        }
        if (best > (unsigned long)INT_MAX) {
            *out = INT_MAX;
            return HLFP_ERR_OVERFLOW;
        }
        *out = (hlfp_fixed)best;
        return HLFP_OK;
    }
#else
    {
        hlfp_fixed guess;
        hlfp_fixed divv;
        hlfp_fixed sum;
        int i;
        int rc;
        guess = (x > HLFP_FIXED_ONE) ? x : HLFP_FIXED_ONE;
        for (i = 0; i < 12; ++i) {
            rc = hlfp_fixed_div(x, guess, &divv);
            if (rc != HLFP_OK) {
                return rc;
            }
            rc = hlfp__fixed_add_local(guess, divv, &sum);
            if (rc != HLFP_OK) {
                return rc;
            }
            guess = sum >> 1;
        }
        *out = guess;
        return HLFP_OK;
    }
#endif
}

static void hlfp__quat_identity(hlfp_quat *q) {
    if (q == 0) {
        return;
    }
    q->x = 0;
    q->y = 0;
    q->z = 0;
    q->w = HLFP_FIXED_ONE;
}

static int hlfp__quat_negate_if_needed(const hlfp_quat *ref, hlfp_quat *q, hlfp_error *err) {
    hlfp_fixed d0;
    hlfp_fixed d1;
    hlfp_fixed d2;
    hlfp_fixed d3;
    hlfp_fixed dot;
    int rc;

    if (ref == 0 || q == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion hemisphere input");
    }
    rc = hlfp_fixed_mul(ref->x, q->x, &d0);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp_fixed_mul(ref->y, q->y, &d1);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp_fixed_mul(ref->z, q->z, &d2);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp_fixed_mul(ref->w, q->w, &d3);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp__fixed_add_local(d0, d1, &dot);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp__fixed_add_local(dot, d2, &dot);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp__fixed_add_local(dot, d3, &dot);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");

    if (dot < 0) {
        q->x = -q->x;
        q->y = -q->y;
        q->z = -q->z;
        q->w = -q->w;
    }
    return HLFP_OK;
}

static int hlfp__quat_normalize(hlfp_quat *q, hlfp_error *err) {
    hlfp_fixed s0;
    hlfp_fixed s1;
    hlfp_fixed s2;
    hlfp_fixed s3;
    hlfp_fixed len2;
    hlfp_fixed len;
    int rc;

    if (q == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion normalize input");
    }
    rc = hlfp_fixed_mul(q->x, q->x, &s0);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize overflow");
    rc = hlfp_fixed_mul(q->y, q->y, &s1);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize overflow");
    rc = hlfp_fixed_mul(q->z, q->z, &s2);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize overflow");
    rc = hlfp_fixed_mul(q->w, q->w, &s3);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize overflow");
    rc = hlfp__fixed_add_local(s0, s1, &len2);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize overflow");
    rc = hlfp__fixed_add_local(len2, s2, &len2);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize overflow");
    rc = hlfp__fixed_add_local(len2, s3, &len2);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize overflow");
    if (len2 == 0) {
        hlfp__quat_identity(q);
        return HLFP_OK;
    }
    rc = hlfp__fixed_sqrt(len2, &len);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "quaternion normalize sqrt failed");
    }
    rc = hlfp_fixed_div(q->x, len, &q->x);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize divide failed");
    rc = hlfp_fixed_div(q->y, len, &q->y);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize divide failed");
    rc = hlfp_fixed_div(q->z, len, &q->z);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize divide failed");
    rc = hlfp_fixed_div(q->w, len, &q->w);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion normalize divide failed");
    return HLFP_OK;
}


static int hlfp__quat_from_euler(const hlfp_vec3 *rot, hlfp_quat *out, hlfp_error *err) {
    hlfp_fixed sx, cx, sy, cy, sz, cz;
    hlfp_fixed a, b, t;
    int rc;
    if (rot == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion euler input");
    }
    rc = hlfp_fixed_sin_cos(rot->z / 2, &sz, &cz);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion ZR sin/cos failed");
    rc = hlfp_fixed_sin_cos(rot->y / 2, &sy, &cy);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion YR sin/cos failed");
    rc = hlfp_fixed_sin_cos(rot->x / 2, &sx, &cx);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion XR sin/cos failed");

    rc = hlfp_fixed_mul(sx, cy, &t); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(t, cz, &a); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(cx, sy, &t); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(t, sz, &b); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__fixed_add_local(a, -b, &out->x); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");

    rc = hlfp_fixed_mul(cx, sy, &t); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(t, cz, &a); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(sx, cy, &t); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(t, sz, &b); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__fixed_add_local(a, b, &out->y); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");

    rc = hlfp_fixed_mul(cx, cy, &t); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(t, sz, &a); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(sx, sy, &t); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(t, cz, &b); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__fixed_add_local(a, -b, &out->z); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");

    rc = hlfp_fixed_mul(cx, cy, &t); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(t, cz, &a); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(sx, sy, &t); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp_fixed_mul(t, sz, &b); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    rc = hlfp__fixed_add_local(a, b, &out->w); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "GoldSrc quaternion overflow");
    return hlfp__quat_normalize(out, err);
}

static int hlfp__quat_dot(const hlfp_quat *a, const hlfp_quat *b, hlfp_fixed *out, hlfp_error *err) {
    hlfp_fixed t0, t1, t2, t3;
    int rc;
    if (a == 0 || b == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion dot input");
    }
    rc = hlfp_fixed_mul(a->x, b->x, &t0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp_fixed_mul(a->y, b->y, &t1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp_fixed_mul(a->z, b->z, &t2); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp_fixed_mul(a->w, b->w, &t3); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp__fixed_add_local(t0, t1, out); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp__fixed_add_local(*out, t2, out); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    rc = hlfp__fixed_add_local(*out, t3, out); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion dot overflow");
    return HLFP_OK;
}

static int hlfp__fixed_acos(hlfp_fixed c, hlfp_fixed *out, hlfp_error *err) {
    hlfp_fixed lo;
    hlfp_fixed hi;
    int i;
    if (out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null fixed acos output");
    }
    if (c > HLFP_FIXED_ONE) c = HLFP_FIXED_ONE;
    if (c < -HLFP_FIXED_ONE) c = -HLFP_FIXED_ONE;
    lo = 0;
    hi = HLFP_CTRLQUAT_PI_FIXED;
    for (i = 0; i < 18; ++i) {
        hlfp_fixed mid;
        hlfp_fixed s;
        hlfp_fixed cm;
        int rc;
        mid = lo + ((hi - lo) >> 1);
        rc = hlfp_fixed_sin_cos(mid, &s, &cm);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "fixed acos cos failed");
        if (cm > c) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    *out = lo + ((hi - lo) >> 1);
    return HLFP_OK;
}

static int hlfp__quat_scale_add(const hlfp_quat *a, hlfp_fixed sa, const hlfp_quat *b, hlfp_fixed sb, hlfp_quat *out, hlfp_error *err) {
    hlfp_fixed ax, ay, az, aw, bx, by, bz, bw;
    int rc;
    rc = hlfp_fixed_mul(a->x, sa, &ax); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion scale overflow");
    rc = hlfp_fixed_mul(a->y, sa, &ay); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion scale overflow");
    rc = hlfp_fixed_mul(a->z, sa, &az); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion scale overflow");
    rc = hlfp_fixed_mul(a->w, sa, &aw); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion scale overflow");
    rc = hlfp_fixed_mul(b->x, sb, &bx); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion scale overflow");
    rc = hlfp_fixed_mul(b->y, sb, &by); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion scale overflow");
    rc = hlfp_fixed_mul(b->z, sb, &bz); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion scale overflow");
    rc = hlfp_fixed_mul(b->w, sb, &bw); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion scale overflow");
    rc = hlfp__fixed_add_local(ax, bx, &out->x); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion add overflow");
    rc = hlfp__fixed_add_local(ay, by, &out->y); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion add overflow");
    rc = hlfp__fixed_add_local(az, bz, &out->z); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion add overflow");
    rc = hlfp__fixed_add_local(aw, bw, &out->w); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion add overflow");
    return hlfp__quat_normalize(out, err);
}

static int hlfp__quat_slerp(const hlfp_quat *p, const hlfp_quat *q_in, hlfp_fixed t, hlfp_quat *out, hlfp_error *err) {
    hlfp_quat q;
    hlfp_fixed cosom = 0;
    hlfp_fixed one_minus;
    hlfp_fixed one_plus;
    int rc;
    if (p == 0 || q_in == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion slerp input");
    }
    if (t < 0) t = 0;
    if (t > HLFP_FIXED_ONE) t = HLFP_FIXED_ONE;
    q = *q_in;
    rc = hlfp__quat_dot(p, &q, &cosom, err);
    if (rc != HLFP_OK) return rc;
    if (cosom < 0) {
        q.x = -q.x; q.y = -q.y; q.z = -q.z; q.w = -q.w;
        cosom = -cosom;
    }
    one_plus = HLFP_FIXED_ONE + cosom;
    one_minus = HLFP_FIXED_ONE - cosom;
    if (one_plus > HLFP_CTRLQUAT_EPSILON) {
        hlfp_fixed sclp;
        hlfp_fixed sclq;
        if (one_minus > HLFP_CTRLQUAT_EPSILON) {
            hlfp_fixed omega = 0;
            hlfp_fixed sinom;
            hlfp_fixed angle;
            hlfp_fixed ss;
            hlfp_fixed cc;
            rc = hlfp__fixed_acos(cosom, &omega, err);
            if (rc != HLFP_OK) return rc;
            rc = hlfp_fixed_sin_cos(omega, &sinom, &cc);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp sinom failed");
            if (sinom == 0) {
                sclp = HLFP_FIXED_ONE - t;
                sclq = t;
            } else {
                rc = hlfp_fixed_mul(HLFP_FIXED_ONE - t, omega, &angle);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp angle overflow");
                rc = hlfp_fixed_sin_cos(angle, &ss, &cc);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp sine failed");
                rc = hlfp_fixed_div(ss, sinom, &sclp);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp divide failed");
                rc = hlfp_fixed_mul(t, omega, &angle);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp angle overflow");
                rc = hlfp_fixed_sin_cos(angle, &ss, &cc);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp sine failed");
                rc = hlfp_fixed_div(ss, sinom, &sclq);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp divide failed");
            }
        } else {
            sclp = HLFP_FIXED_ONE - t;
            sclq = t;
        }
        return hlfp__quat_scale_add(p, sclp, &q, sclq, out, err);
    } else {
        hlfp_quat ortho;
        hlfp_fixed angle;
        hlfp_fixed sclp;
        hlfp_fixed sclq;
        hlfp_fixed ss;
        hlfp_fixed cc;
        ortho.x = -q.y;
        ortho.y = q.x;
        ortho.z = -q.w;
        ortho.w = q.z;
        rc = hlfp_fixed_mul(HLFP_FIXED_ONE - t, HLFP_CTRLQUAT_HALF_PI_FIXED, &angle);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp 180 angle overflow");
        rc = hlfp_fixed_sin_cos(angle, &sclp, &cc);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp 180 sine failed");
        rc = hlfp_fixed_mul(t, HLFP_CTRLQUAT_HALF_PI_FIXED, &angle);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp 180 angle overflow");
        rc = hlfp_fixed_sin_cos(angle, &sclq, &ss);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion slerp 180 sine failed");
        return hlfp__quat_scale_add(p, sclp, &ortho, sclq, out, err);
    }
}



static int hlfp__quat_mul(const hlfp_quat *a, const hlfp_quat *b, hlfp_quat *out, hlfp_error *err) {
    hlfp_fixed t0, t1, t2, t3;
    hlfp_quat q;
    int rc;
    if (a == 0 || b == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion multiply input");
    }
    rc = hlfp_fixed_mul(a->w, b->x, &t0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(a->x, b->w, &t1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(a->y, b->z, &t2); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(a->z, b->y, &t3); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(t0, t1, &q.x); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(q.x, t2, &q.x); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(q.x, -t3, &q.x); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");

    rc = hlfp_fixed_mul(a->w, b->y, &t0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(-a->x, b->z, &t1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(a->y, b->w, &t2); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(a->z, b->x, &t3); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(t0, t1, &q.y); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(q.y, t2, &q.y); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(q.y, t3, &q.y); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");

    rc = hlfp_fixed_mul(a->w, b->z, &t0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(a->x, b->y, &t1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(-a->y, b->x, &t2); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(a->z, b->w, &t3); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(t0, t1, &q.z); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(q.z, t2, &q.z); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(q.z, t3, &q.z); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");

    rc = hlfp_fixed_mul(a->w, b->w, &t0); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(-a->x, b->x, &t1); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(-a->y, b->y, &t2); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp_fixed_mul(-a->z, b->z, &t3); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(t0, t1, &q.w); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(q.w, t2, &q.w); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");
    rc = hlfp__fixed_add_local(q.w, t3, &q.w); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion multiply overflow");

    *out = q;
    return hlfp__quat_normalize(out, err);
}

static int hlfp__quat_to_mat3x4(const hlfp_quat *q, const hlfp_vec3 *pos, hlfp_mat3x4 *out, hlfp_error *err) {
    hlfp_fixed xx, yy, zz, xy, xz, yz, wx, wy, wz;
    hlfp_fixed two_xx, two_yy, two_zz, two_xy, two_xz, two_yz, two_wx, two_wy, two_wz;
    int rc;

    if (q == 0 || pos == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion matrix output");
    }
    rc = hlfp_fixed_mul(q->x, q->x, &xx); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion matrix overflow");
    rc = hlfp_fixed_mul(q->y, q->y, &yy); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion matrix overflow");
    rc = hlfp_fixed_mul(q->z, q->z, &zz); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion matrix overflow");
    rc = hlfp_fixed_mul(q->x, q->y, &xy); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion matrix overflow");
    rc = hlfp_fixed_mul(q->x, q->z, &xz); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion matrix overflow");
    rc = hlfp_fixed_mul(q->y, q->z, &yz); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion matrix overflow");
    rc = hlfp_fixed_mul(q->w, q->x, &wx); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion matrix overflow");
    rc = hlfp_fixed_mul(q->w, q->y, &wy); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion matrix overflow");
    rc = hlfp_fixed_mul(q->w, q->z, &wz); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion matrix overflow");

    two_xx = xx << 1; two_yy = yy << 1; two_zz = zz << 1;
    two_xy = xy << 1; two_xz = xz << 1; two_yz = yz << 1;
    two_wx = wx << 1; two_wy = wy << 1; two_wz = wz << 1;

    out->m[0][0] = HLFP_FIXED_ONE - two_yy - two_zz;
    out->m[0][1] = two_xy - two_wz;
    out->m[0][2] = two_xz + two_wy;
    out->m[0][3] = pos->x;

    out->m[1][0] = two_xy + two_wz;
    out->m[1][1] = HLFP_FIXED_ONE - two_xx - two_zz;
    out->m[1][2] = two_yz - two_wx;
    out->m[1][3] = pos->y;

    out->m[2][0] = two_xz - two_wy;
    out->m[2][1] = two_yz + two_wx;
    out->m[2][2] = HLFP_FIXED_ONE - two_xx - two_yy;
    out->m[2][3] = pos->z;
    return HLFP_OK;
}

static int hlfp__controller_slot(const hlfp_mdl *mdl, int bonecontroller_index) {
    int slot;
    if (mdl == 0) {
        return -1;
    }
    if (bonecontroller_index < 0 || bonecontroller_index >= mdl->num_bonecontrollers) {
        return -1;
    }
    slot = mdl->bonecontrollers[bonecontroller_index].index;
    if (slot > 0) {
        slot -= 1;
    } else {
        slot = bonecontroller_index;
    }
    if (slot < 0 || slot >= HLFP_MDL_MAX_USER_CONTROLLERS) {
        return -1;
    }
    return slot;
}

static int hlfp__apply_bone_controllers_quat(const hlfp_mdl *mdl,
                                             const hlfp_mdl_controller_state *controllers,
                                             hlfp_mdl_quat_pose_set *poses,
                                             hlfp_error *err) {
    int bone;
    if (mdl == 0 || poses == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null bone controller input");
    }
    if (controllers == 0) {
        return HLFP_OK;
    }
    for (bone = 0; bone < poses->num_bones; ++bone) {
        hlfp_vec3 delta_rot;
        int any_rot;
        int ch;
        delta_rot.x = 0;
        delta_rot.y = 0;
        delta_rot.z = 0;
        any_rot = 0;
        for (ch = 0; ch < 6; ++ch) {
            int bcidx = mdl->bones[bone].bonecontroller[ch];
            int slot = hlfp__controller_slot(mdl, bcidx);
            if (slot >= 0) {
                hlfp_fixed v = controllers->bone[slot];
                if (v != 0) {
                    if (ch == 0) poses->poses[bone].pos.x += v;
                    else if (ch == 1) poses->poses[bone].pos.y += v;
                    else if (ch == 2) poses->poses[bone].pos.z += v;
                    else if (ch == 3) { delta_rot.x += v; any_rot = 1; }
                    else if (ch == 4) { delta_rot.y += v; any_rot = 1; }
                    else { delta_rot.z += v; any_rot = 1; }
                }
            }
        }
        if (any_rot) {
            hlfp_quat delta_q;
            hlfp_quat final_q;
            int rc = hlfp__quat_from_euler(&delta_rot, &delta_q, err);
            if (rc != HLFP_OK) {
                return rc;
            }
            rc = hlfp__quat_mul(&poses->poses[bone].rot, &delta_q, &final_q, err);
            if (rc != HLFP_OK) {
                return rc;
            }
            poses->poses[bone].rot = final_q;
        }
    }
    return HLFP_OK;
}

static int hlfp__sequence_blend_value(const hlfp_mdl_seqdesc *seq,
                                      const hlfp_mdl_controller_state *controllers,
                                      int axis,
                                      hlfp_fixed *out_t,
                                      hlfp_error *err) {
    hlfp_fixed num;
    hlfp_fixed den;
    int rc;
    if (out_t == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null blend controller output");
    }
    *out_t = 0;
    if (seq == 0 || axis < 0 || axis >= HLFP_MDL_MAX_BLEND_AXES) {
        return HLFP_OK;
    }
    if (controllers == 0) {
        return HLFP_OK;
    }
    num = controllers->blend[axis] - seq->blendstart[axis];
    den = seq->blendend[axis] - seq->blendstart[axis];
    if (den == 0) {
        *out_t = 0;
        return HLFP_OK;
    }
    rc = hlfp_fixed_div(num, den, out_t);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "blend controller divide failed");
    }
    *out_t = hlfp__fixed_clamp01(*out_t);
    return HLFP_OK;
}

void hlfp_mdl_controller_state_init(hlfp_mdl_controller_state *out) {
    if (out != 0) {
        hlfp__zero(out, sizeof(*out));
    }
}

int hlfp_mdl_pose_to_quat_pose_set(const hlfp_mdl_pose_set *src,
                                   hlfp_mdl_quat_pose_set *out,
                                   hlfp_error *err) {
    int i;
    int rc;
    if (src == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion pose conversion input");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));
    out->num_bones = src->num_bones;
    for (i = 0; i < src->num_bones; ++i) {
        out->poses[i].bone = src->poses[i].bone;
        out->poses[i].pos = src->poses[i].pos;
        rc = hlfp__quat_from_euler(&src->poses[i].rot, &out->poses[i].rot, err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    return HLFP_OK;
}

int hlfp_mdl_quat_pose_to_local_matrices(const hlfp_mdl_quat_pose_set *poses,
                                         hlfp_mat3x4 *out,
                                         int out_cap,
                                         hlfp_error *err) {
    int i;
    int rc;
    if (poses == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion local matrix input");
    }
    hlfp_error_clear(err);
    if (poses->num_bones < 0 || poses->num_bones > out_cap) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "local matrix output capacity too small for quaternion poses");
    }
    for (i = 0; i < out_cap; ++i) {
        hlfp_mat3x4_identity(&out[i]);
    }
    for (i = 0; i < poses->num_bones; ++i) {
        int bone = poses->poses[i].bone;
        if (bone < 0 || bone >= out_cap) {
            return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "quaternion pose bone index out of range");
        }
        rc = hlfp__quat_to_mat3x4(&poses->poses[i].rot, &poses->poses[i].pos, &out[bone], err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    return HLFP_OK;
}

int hlfp_mdl_quat_pose_to_world_matrices(const hlfp_mdl *mdl,
                                         const hlfp_mdl_quat_pose_set *poses,
                                         hlfp_mat3x4 *out,
                                         int out_cap,
                                         hlfp_error *err) {
    hlfp_mat3x4 local[HLFP_MDL_MAX_BONES];
    int bone;
    int rc;
    if (mdl == 0 || poses == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion world matrix input");
    }
    hlfp_error_clear(err);
    if (mdl->num_bones > out_cap || mdl->num_bones > HLFP_MDL_MAX_BONES) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "world matrix output capacity too small");
    }
    rc = hlfp_mdl_quat_pose_to_local_matrices(poses, local, HLFP_MDL_MAX_BONES, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    for (bone = 0; bone < mdl->num_bones; ++bone) {
        int parent = mdl->bones[bone].parent;
        if (parent < 0) {
            out[bone] = local[bone];
        } else {
            rc = hlfp_mat3x4_concat(&out[parent], &local[bone], &out[bone], err);
            if (rc != HLFP_OK) {
                return rc;
            }
        }
    }
    return HLFP_OK;
}

int hlfp_mdl_blend_quat_pose_sets(const hlfp_mdl_quat_pose_set *pose_sets,
                                  const hlfp_fixed *weights,
                                  int count,
                                  hlfp_mdl_quat_pose_set *out,
                                  hlfp_error *err) {
    int i;
    int bone;
    int num_bones;
    hlfp_fixed total_weight;
    int rc;

    if (pose_sets == 0 || weights == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion blend input");
    }
    hlfp_error_clear(err);
    if (count <= 0 || count > HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "quaternion blend count out of range");
    }
    num_bones = pose_sets[0].num_bones;
    if (num_bones < 0 || num_bones > HLFP_MDL_MAX_BONES) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "quaternion blend bone count exceeds fixed capacity");
    }
    hlfp__zero(out, sizeof(*out));
    out->num_bones = num_bones;
    total_weight = 0;
    for (i = 0; i < count; ++i) {
        total_weight += weights[i];
    }
    if (total_weight == 0) {
        return hlfp__set_error(err, HLFP_ERR_DIV_BY_ZERO, 0, 0, "quaternion blend total weight is zero");
    }
    for (bone = 0; bone < num_bones; ++bone) {
        hlfp_quat ref_q;
        hlfp_quat acc_q;
        hlfp_fixed acc_px, acc_py, acc_pz;
        ref_q = pose_sets[0].poses[bone].rot;
        acc_q.x = 0;
        acc_q.y = 0;
        acc_q.z = 0;
        acc_q.w = 0;
        acc_px = 0;
        acc_py = 0;
        acc_pz = 0;
        out->poses[bone].bone = pose_sets[0].poses[bone].bone;
        for (i = 0; i < count; ++i) {
            hlfp_quat q;
            hlfp_fixed t;
            if (pose_sets[i].num_bones != num_bones || pose_sets[i].poses[bone].bone != out->poses[bone].bone) {
                return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "quaternion pose sets are not bone-ordered for blending");
            }
            q = pose_sets[i].poses[bone].rot;
            rc = hlfp__quat_negate_if_needed(&ref_q, &q, err);
            if (rc != HLFP_OK) {
                return rc;
            }
            rc = hlfp_fixed_mul(pose_sets[i].poses[bone].pos.x, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion position blend overflow");
            acc_px += t;
            rc = hlfp_fixed_mul(pose_sets[i].poses[bone].pos.y, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion position blend overflow");
            acc_py += t;
            rc = hlfp_fixed_mul(pose_sets[i].poses[bone].pos.z, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion position blend overflow");
            acc_pz += t;
            rc = hlfp_fixed_mul(q.x, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion rotation blend overflow");
            acc_q.x += t;
            rc = hlfp_fixed_mul(q.y, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion rotation blend overflow");
            acc_q.y += t;
            rc = hlfp_fixed_mul(q.z, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion rotation blend overflow");
            acc_q.z += t;
            rc = hlfp_fixed_mul(q.w, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion rotation blend overflow");
            acc_q.w += t;
        }
        rc = hlfp_fixed_div(acc_px, total_weight, &out->poses[bone].pos.x);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion blend normalization failed");
        rc = hlfp_fixed_div(acc_py, total_weight, &out->poses[bone].pos.y);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion blend normalization failed");
        rc = hlfp_fixed_div(acc_pz, total_weight, &out->poses[bone].pos.z);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion blend normalization failed");
        out->poses[bone].rot = acc_q;
        rc = hlfp__quat_normalize(&out->poses[bone].rot, err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    return HLFP_OK;
}


static int hlfp__quat_lerp_fixed(hlfp_fixed a, hlfp_fixed b, hlfp_fixed t, hlfp_fixed *out, hlfp_error *err);

static int hlfp__quat_pose_slerp_pair(const hlfp_mdl_quat_pose_set *a,
                                      const hlfp_mdl_quat_pose_set *b,
                                      hlfp_fixed t,
                                      hlfp_mdl_quat_pose_set *out,
                                      hlfp_error *err) {
    int bone;
    int rc;
    if (a == 0 || b == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion pose slerp input");
    }
    if (a->num_bones != b->num_bones || a->num_bones < 0 || a->num_bones > HLFP_MDL_MAX_BONES) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "quaternion pose slerp requires matching bone counts");
    }
    hlfp__zero(out, sizeof(*out));
    out->num_bones = a->num_bones;
    for (bone = 0; bone < a->num_bones; ++bone) {
        if (a->poses[bone].bone != b->poses[bone].bone) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "quaternion pose slerp bone mismatch");
        }
        out->poses[bone].bone = a->poses[bone].bone;
        rc = hlfp__quat_lerp_fixed(a->poses[bone].pos.x, b->poses[bone].pos.x, t, &out->poses[bone].pos.x, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_lerp_fixed(a->poses[bone].pos.y, b->poses[bone].pos.y, t, &out->poses[bone].pos.y, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_lerp_fixed(a->poses[bone].pos.z, b->poses[bone].pos.z, t, &out->poses[bone].pos.z, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_slerp(&a->poses[bone].rot, &b->poses[bone].rot, t, &out->poses[bone].rot, err);
        if (rc != HLFP_OK) return rc;
    }
    return HLFP_OK;
}


static int hlfp__quat_normalize_frame_position(const hlfp_mdl_seqdesc *seq,
                                               hlfp_fixed frame_position,
                                               int *frame,
                                               hlfp_fixed *frac,
                                               hlfp_error *err) {
    hlfp_fixed pos;
    hlfp_fixed range;
    hlfp_fixed max_pos;
    if (seq == 0 || frame == 0 || frac == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion frame normalize input");
    }
    if (seq->numframes <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "sequence has no frames");
    }
    if (seq->numframes > INT_MAX / HLFP_FIXED_ONE) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence frame count exceeds 16.16 range");
    }
    if (seq->numframes == 1) {
        *frame = 0;
        *frac = 0;
        return HLFP_OK;
    }
    pos = frame_position;
    if ((seq->flags & HLFP_STUDIO_LOOPING) != 0) {
        range = (hlfp_fixed)((seq->numframes - 1) * HLFP_FIXED_ONE);
        if (range <= 0) {
            *frame = 0;
            *frac = 0;
            return HLFP_OK;
        }
        pos %= range;
        if (pos < 0) pos += range;
        *frame = (int)(pos >> HLFP_FIXED_FRAC_BITS);
        *frac = pos & 0xFFFF;
        return HLFP_OK;
    }
    if (pos <= 0) {
        *frame = 0;
        *frac = 0;
        return HLFP_OK;
    }
    max_pos = (hlfp_fixed)((seq->numframes - 1) * HLFP_FIXED_ONE);
    if (pos >= max_pos) {
        *frame = seq->numframes - 1;
        *frac = 0;
        return HLFP_OK;
    }
    *frame = (int)(pos >> HLFP_FIXED_FRAC_BITS);
    *frac = pos & 0xFFFF;
    return HLFP_OK;
}

static int hlfp__quat_sequence_buffer(const hlfp_mdl *mdl,
                                      const hlfp_mdl_seqdesc *seq,
                                      hlfp_buffer *buf,
                                      size_t *base_off,
                                      hlfp_error *err) {
    const hlfp_mdl_seqgroup *group;
    if (mdl == 0 || seq == 0 || buf == 0 || base_off == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion sequence buffer input");
    }
    if (seq->seqgroup < 0 || seq->seqgroup >= mdl->num_seqgroups) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence group index out of range");
    }
    group = &mdl->seqgroups[seq->seqgroup];
    if (seq->seqgroup == 0) {
        *buf = mdl->main_buffer;
        *base_off = (size_t)group->data + (size_t)seq->animindex;
    } else {
        *buf = mdl->seqgroup_buffers[seq->seqgroup];
        if (buf->data == 0 || buf->size == 0) {
            return hlfp__set_error(err, HLFP_ERR_MISSING_DATA, 0, 0, "external sequence group buffer not attached");
        }
        *base_off = (size_t)seq->animindex;
    }
    return HLFP_OK;
}

static int hlfp__quat_anim_sample_pair(const hlfp_buffer *buf,
                                       const hlfp_u8 *anim_entry,
                                       hlfp_u16 offset,
                                       int frame,
                                       int clamp_next,
                                       hlfp_i16 *sample1,
                                       hlfp_i16 *sample2,
                                       hlfp_error *err) {
    const hlfp_u8 *p;
    int k;
    int valid;
    int total;
    int idx1;
    int idx2;
    if (buf == 0 || anim_entry == 0 || sample1 == 0 || sample2 == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion anim sample input");
    }
    *sample1 = 0;
    *sample2 = 0;
    if (offset == 0) {
        return HLFP_OK;
    }
    p = hlfp__checked_range(buf, (size_t)(anim_entry - buf->data) + (size_t)offset, 2U, err, "quaternion animation value span");
    if (p == 0) return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    k = frame;
    for (;;) {
        p = hlfp__checked_range(buf, (size_t)(p - buf->data), 2U, err, "quaternion animation value span");
        if (p == 0) return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        valid = (int)p[0];
        total = (int)p[1];
        if (total <= 0) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "animation span has non-positive total");
        }
        if (total < valid) {
            k = 0;
        }
        if (total > k) {
            if (valid > k) {
                idx1 = k + 1;
                if (valid > k + 1) {
                    idx2 = k + 2;
                } else {
                    if (total > k + 1) idx2 = idx1;
                    else idx2 = valid + 2;
                }
            } else {
                idx1 = valid;
                if (total > k + 1) idx2 = idx1;
                else idx2 = valid + 2;
            }
            {
                const hlfp_u8 *v1;
                const hlfp_u8 *v2;
                /* Some final frames have no following span/sample.
                   At the final non-looping key the interpolation fraction
                   is zero, so the next key must not be read. */
                if (clamp_next) idx2 = idx1;
                v1 = hlfp__checked_range(buf, (size_t)(p - buf->data) + (size_t)idx1 * 2U, 2U, err, "quaternion animation sample");
                if (v1 == 0) return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
                v2 = hlfp__checked_range(buf, (size_t)(p - buf->data) + (size_t)idx2 * 2U, 2U, err, "quaternion animation sample");
                if (v2 == 0) return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
                *sample1 = hlfp__read_i16le(v1);
                *sample2 = hlfp__read_i16le(v2);
                return HLFP_OK;
            }
        }
        k -= total;
        p = hlfp__checked_range(buf, (size_t)(p - buf->data) + (size_t)(valid + 1) * 2U, 2U, err, "next quaternion animation span");
        if (p == 0) return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }
}

static int hlfp__quat_lerp_fixed(hlfp_fixed a, hlfp_fixed b, hlfp_fixed t, hlfp_fixed *out, hlfp_error *err) {
    hlfp_fixed delta;
    hlfp_fixed scaled;
    int rc;
    rc = hlfp__fixed_add_local(b, -a, &delta);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "fixed lerp delta overflow");
    rc = hlfp_fixed_mul(delta, t, &scaled);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "fixed lerp scale overflow");
    rc = hlfp__fixed_add_local(a, scaled, out);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "fixed lerp add overflow");
    return HLFP_OK;
}

static int hlfp__quat_component_values(const hlfp_buffer *buf,
                                       const hlfp_u8 *entry,
                                       hlfp_u16 offset,
                                       int frame,
                                       int clamp_next,
                                       hlfp_fixed base,
                                       hlfp_fixed scale,
                                       hlfp_fixed *v1,
                                       hlfp_fixed *v2,
                                       hlfp_error *err) {
    hlfp_i16 s1;
    hlfp_i16 s2;
    hlfp_fixed d1;
    hlfp_fixed d2;
    int rc;
    if (v1 == 0 || v2 == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion component output");
    }
    rc = hlfp__quat_anim_sample_pair(buf, entry, offset, frame, clamp_next, &s1, &s2, err);
    if (rc != HLFP_OK) return rc;
    rc = hlfp__fixed_mul_i16(scale, s1, &d1);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion sample scale overflow");
    rc = hlfp__fixed_mul_i16(scale, s2, &d2);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion sample scale overflow");
    rc = hlfp__fixed_add_local(base, d1, v1);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion component add overflow");
    rc = hlfp__fixed_add_local(base, d2, v2);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "quaternion component add overflow");
    return HLFP_OK;
}

static int hlfp__quat_decode_sequence_slot(const hlfp_mdl *mdl,
                                           int sequence_index,
                                           int blend_index,
                                           hlfp_fixed frame_position,
                                           hlfp_mdl_quat_pose_set *out,
                                           hlfp_error *err) {
    const hlfp_mdl_seqdesc *seq;
    hlfp_buffer buf;
    size_t base_off = 0;
    int frame = 0;
    hlfp_fixed frac = 0;
    int bone;
    int rc;
    int clamp_next;
    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion sequence slot input");
    }
    if (sequence_index < 0 || sequence_index >= mdl->num_seqdesc) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence index out of range");
    }
    seq = &mdl->seqdesc[sequence_index];
    if (blend_index < 0 || blend_index >= seq->numblends) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "blend index out of range");
    }
    rc = hlfp__quat_normalize_frame_position(seq, frame_position, &frame, &frac, err);
    if (rc != HLFP_OK) return rc;
    clamp_next = frame >= seq->numframes - 1;
    rc = hlfp__quat_sequence_buffer(mdl, seq, &buf, &base_off, err);
    if (rc != HLFP_OK) return rc;
    hlfp__zero(out, sizeof(*out));
    out->num_bones = mdl->num_bones;
    for (bone = 0; bone < mdl->num_bones; ++bone) {
        const hlfp_u8 *entry;
        size_t entry_off;
        hlfp_fixed px1, py1, pz1, px2, py2, pz2;
        hlfp_vec3 a1;
        hlfp_vec3 a2;
        hlfp_quat q1;
        hlfp_quat q2;
        entry_off = base_off + ((size_t)blend_index * (size_t)mdl->num_bones + (size_t)bone) * (size_t)HLFP_CTRLQUAT_ANIMOFF_SIZE;
        entry = hlfp__checked_range(&buf, entry_off, (size_t)HLFP_CTRLQUAT_ANIMOFF_SIZE, err, "quaternion animation offset entry");
        if (entry == 0) return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        out->poses[bone].bone = bone;
        rc = hlfp__quat_component_values(&buf, entry, hlfp__read_u16le(entry + 0), frame, clamp_next, mdl->bones[bone].value[0], mdl->bones[bone].scale[0], &px1, &px2, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_component_values(&buf, entry, hlfp__read_u16le(entry + 2), frame, clamp_next, mdl->bones[bone].value[1], mdl->bones[bone].scale[1], &py1, &py2, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_component_values(&buf, entry, hlfp__read_u16le(entry + 4), frame, clamp_next, mdl->bones[bone].value[2], mdl->bones[bone].scale[2], &pz1, &pz2, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_lerp_fixed(px1, px2, frac, &out->poses[bone].pos.x, err); if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_lerp_fixed(py1, py2, frac, &out->poses[bone].pos.y, err); if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_lerp_fixed(pz1, pz2, frac, &out->poses[bone].pos.z, err); if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_component_values(&buf, entry, hlfp__read_u16le(entry + 6), frame, clamp_next, mdl->bones[bone].value[3], mdl->bones[bone].scale[3], &a1.x, &a2.x, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_component_values(&buf, entry, hlfp__read_u16le(entry + 8), frame, clamp_next, mdl->bones[bone].value[4], mdl->bones[bone].scale[4], &a1.y, &a2.y, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_component_values(&buf, entry, hlfp__read_u16le(entry + 10), frame, clamp_next, mdl->bones[bone].value[5], mdl->bones[bone].scale[5], &a1.z, &a2.z, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_from_euler(&a1, &q1, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_from_euler(&a2, &q2, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__quat_slerp(&q1, &q2, frac, &out->poses[bone].rot, err);
        if (rc != HLFP_OK) return rc;
    }
    if (seq->motionbone >= 0 && seq->motionbone < out->num_bones) {
        if ((seq->motiontype & HLFP_STUDIO_X) != 0) out->poses[seq->motionbone].pos.x = 0;
        if ((seq->motiontype & HLFP_STUDIO_Y) != 0) out->poses[seq->motionbone].pos.y = 0;
        if ((seq->motiontype & HLFP_STUDIO_Z) != 0) out->poses[seq->motionbone].pos.z = 0;
    }
    return HLFP_OK;
}

int hlfp_mdl_decode_sequence_frame_fixed_quat_ctrl(const hlfp_mdl *mdl,
                                                   int sequence_index,
                                                   hlfp_fixed frame_position,
                                                   const hlfp_mdl_controller_state *controllers,
                                                   hlfp_mdl_quat_pose_set *out,
                                                   hlfp_error *err) {
    const hlfp_mdl_seqdesc *seq;
    hlfp_mdl_quat_pose_set quat_sets[4];
    hlfp_fixed blend_u;
    hlfp_fixed blend_v;
    hlfp_fixed weights[4];
    int slots[4];
    int slot_count;
    int i;
    int rc;

    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null controller quaternion decode input");
    }
    hlfp_error_clear(err);
    if (sequence_index < 0 || sequence_index >= mdl->num_seqdesc) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence index out of range");
    }
    seq = &mdl->seqdesc[sequence_index];
    if (seq->numblends <= 1) {
        slot_count = 1;
        slots[0] = 0;
        weights[0] = HLFP_FIXED_ONE;
    } else if (seq->numblends == 2) {
        rc = hlfp__sequence_blend_value(seq, controllers, 0, &blend_u, err);
        if (rc != HLFP_OK) return rc;
        slot_count = 2;
        slots[0] = 0;
        slots[1] = 1;
        weights[1] = blend_u;
        weights[0] = HLFP_FIXED_ONE - blend_u;
    } else {
        rc = hlfp__sequence_blend_value(seq, controllers, 0, &blend_u, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__sequence_blend_value(seq, controllers, 1, &blend_v, err);
        if (rc != HLFP_OK) return rc;
        slot_count = 4;
        slots[0] = 0;
        slots[1] = 1;
        slots[2] = 2;
        slots[3] = 3;
        rc = hlfp_fixed_mul(HLFP_FIXED_ONE - blend_u, HLFP_FIXED_ONE - blend_v, &weights[0]);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "controller bilerp overflow");
        rc = hlfp_fixed_mul(blend_u, HLFP_FIXED_ONE - blend_v, &weights[1]);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "controller bilerp overflow");
        rc = hlfp_fixed_mul(HLFP_FIXED_ONE - blend_u, blend_v, &weights[2]);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "controller bilerp overflow");
        rc = hlfp_fixed_mul(blend_u, blend_v, &weights[3]);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "controller bilerp overflow");
    }

    for (i = 0; i < slot_count; ++i) {
        rc = hlfp__quat_decode_sequence_slot(mdl, sequence_index, slots[i], frame_position, &quat_sets[i], err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    if (slot_count == 1) {
        *out = quat_sets[0];
    } else if (slot_count == 2) {
        rc = hlfp__quat_pose_slerp_pair(&quat_sets[0], &quat_sets[1], blend_u, out, err);
        if (rc != HLFP_OK) {
            return rc;
        }
    } else {
        hlfp_mdl_quat_pose_set tmp01;
        hlfp_mdl_quat_pose_set tmp23;
        rc = hlfp__quat_pose_slerp_pair(&quat_sets[0], &quat_sets[1], blend_u, &tmp01, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        rc = hlfp__quat_pose_slerp_pair(&quat_sets[2], &quat_sets[3], blend_u, &tmp23, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        rc = hlfp__quat_pose_slerp_pair(&tmp01, &tmp23, blend_v, out, err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    HLFP_UNUSED(weights);
    return hlfp__apply_bone_controllers_quat(mdl, controllers, out, err);
}


int hlfp_mdl_decode_sequence_frame_fixed_quat(const hlfp_mdl *mdl,
                                             int sequence_index,
                                             int blend_index,
                                             hlfp_fixed frame_position,
                                             hlfp_mdl_quat_pose_set *out,
                                             hlfp_error *err) {
    if (blend_index != 0) {
        return hlfp__quat_decode_sequence_slot(mdl, sequence_index, blend_index, frame_position, out, err);
    }
    return hlfp_mdl_decode_sequence_frame_fixed_quat_ctrl(mdl, sequence_index, frame_position, 0, out, err);
}

int hlfp_mdl_decode_sequence_world_matrices_fixed_quat(const hlfp_mdl *mdl,
                                                       int sequence_index,
                                                       int blend_index,
                                                       hlfp_fixed frame_position,
                                                       hlfp_mat3x4 *out,
                                                       int out_cap,
                                                       hlfp_error *err) {
    hlfp_mdl_quat_pose_set poses;
    int rc;
    rc = hlfp_mdl_decode_sequence_frame_fixed_quat(mdl, sequence_index, blend_index, frame_position, &poses, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_quat_pose_to_world_matrices(mdl, &poses, out, out_cap, err);
}

int hlfp_mdl_decode_sequence_time_quat_ctrl(const hlfp_mdl *mdl,
                                            int sequence_index,
                                            hlfp_fixed seconds,
                                            const hlfp_mdl_controller_state *controllers,
                                            hlfp_mdl_quat_pose_set *out,
                                            hlfp_error *err) {
    hlfp_fixed frame_position;
    int rc;
    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion time decode input");
    }
    hlfp_error_clear(err);
    if (sequence_index < 0 || sequence_index >= mdl->num_seqdesc) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence index out of range");
    }
    rc = hlfp_fixed_mul(seconds, mdl->seqdesc[sequence_index].fps, &frame_position);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "time to frame conversion overflow");
    }
    return hlfp_mdl_decode_sequence_frame_fixed_quat_ctrl(mdl, sequence_index, frame_position, controllers, out, err);
}

int hlfp_mdl_blend_sequence_times_quat_ctrl(const hlfp_mdl *mdl,
                                            const int *sequence_indices,
                                            const hlfp_fixed *seconds,
                                            const hlfp_mdl_controller_state *controllers,
                                            const hlfp_fixed *weights,
                                            int count,
                                            hlfp_mdl_quat_pose_set *out,
                                            hlfp_error *err) {
    hlfp_mdl_quat_pose_set decoded[HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS];
    int i;
    int rc;
    if (mdl == 0 || sequence_indices == 0 || seconds == 0 || weights == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion sequence blend input");
    }
    hlfp_error_clear(err);
    if (count <= 0 || count > HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "quaternion sequence blend count out of range");
    }
    for (i = 0; i < count; ++i) {
        const hlfp_mdl_controller_state *ctrl = 0;
        if (controllers != 0) {
            ctrl = &controllers[i];
        }
        rc = hlfp_mdl_decode_sequence_time_quat_ctrl(mdl, sequence_indices[i], seconds[i], ctrl, &decoded[i], err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    return hlfp_mdl_blend_quat_pose_sets(decoded, weights, count, out, err);
}

int hlfp_mdl_blend_sequence_world_matrices_times_quat_ctrl(const hlfp_mdl *mdl,
                                                           const int *sequence_indices,
                                                           const hlfp_fixed *seconds,
                                                           const hlfp_mdl_controller_state *controllers,
                                                           const hlfp_fixed *weights,
                                                           int count,
                                                           hlfp_mat3x4 *out,
                                                           int out_cap,
                                                           hlfp_error *err) {
    hlfp_mdl_quat_pose_set poses;
    int rc;
    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quaternion world blend output");
    }
    rc = hlfp_mdl_blend_sequence_times_quat_ctrl(mdl, sequence_indices, seconds, controllers, weights, count, &poses, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_quat_pose_to_world_matrices(mdl, &poses, out, out_cap, err);
}
