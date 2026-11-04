#include "mf3d.h"

mf3d_fix mf3d_fix_from_int(mf3d_i32 v) { return (mf3d_fix)(v << 16); }
mf3d_i32 mf3d_fix_to_int(mf3d_fix v) { return (mf3d_i32)(v >> 16); }

/* Parses decimal ASCII into 16.16 fixed point. No float/double. */
int mf3d_parse_fix(const char *s, mf3d_fix *out)
{
    mf3d_i32 sign, ip, fp, scale, n;
    const char *p;
    if (!s || !out) return MF3D_E_BADARG;
    p = s; sign = 1; ip = 0; fp = 0; scale = 1; n = 0;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p == '-') { sign = -1; p++; } else if (*p == '+') p++;
    while (*p >= '0' && *p <= '9') { ip = ip * 10 + (*p - '0'); p++; }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9' && n < 6) {
            fp = fp * 10 + (*p - '0'); scale *= 10; n++; p++;
        }
        while (*p >= '0' && *p <= '9') p++;
    }
    /* simple exponent support: e-3/e+3 via repeated decimal shift */
    if (*p == 'e' || *p == 'E') {
        mf3d_i32 esign, exp, i;
        p++; esign = 1; exp = 0;
        if (*p == '-') { esign = -1; p++; } else if (*p == '+') p++;
        while (*p >= '0' && *p <= '9') { exp = exp * 10 + (*p - '0'); p++; }
        if (exp > 12) exp = 12;
        if (esign > 0) { for (i = 0; i < exp; ++i) ip *= 10; }
        else { for (i = 0; i < exp; ++i) { scale *= 10; } }
    }
    *out = (mf3d_fix)(sign * ((ip << 16) + ((fp << 16) / (scale ? scale : 1))));
    return MF3D_OK;
}

int mf3d_parse_u32(const char *s, mf3d_u32 *out)
{
    mf3d_u32 v;
    const char *p;
    if (!s || !out) return MF3D_E_BADARG;
    p = s; v = 0;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    while (*p >= '0' && *p <= '9') { v = v * 10UL + (mf3d_u32)(*p - '0'); p++; }
    *out = v;
    return MF3D_OK;
}
