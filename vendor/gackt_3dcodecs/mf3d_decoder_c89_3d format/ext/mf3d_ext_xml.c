#include "mf3d_ext_xml.h"

extern int mf3d_parse_fix(const char *s, mf3d_fix *out);
extern int mf3d_parse_u32(const char *s, mf3d_u32 *out);

static int is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int local_after_prefix_eq(const char *p, const char *name, int *used)
{
    int i, j, best;
    best = -1;
    i = 0;
    while (p[i] && !is_ws(p[i]) && p[i] != '>' && p[i] != '/') {
        if (p[i] == ':') best = i + 1;
        i++;
    }
    if (best < 0) best = 0;
    j = 0;
    while (name[j]) {
        if (p[best + j] != name[j]) return 0;
        j++;
    }
    if (p[best + j] == 0 || is_ws(p[best + j]) || p[best + j] == '>' || p[best + j] == '/') {
        if (used) *used = best + j;
        return 1;
    }
    return 0;
}

int mf3d_ext_tag_matches(const char *p, const char *local_name)
{
    if (!p || !local_name || *p != '<') return 0;
    p++;
    if (*p == '/' || *p == '?' || *p == '!') return 0;
    return local_after_prefix_eq(p, local_name, 0);
}

const char *mf3d_ext_next_tag(const char *p, const char *local_name)
{
    if (!p || !local_name) return 0;
    while (*p) {
        if (*p == '<' && mf3d_ext_tag_matches(p, local_name)) return p;
        p++;
    }
    return 0;
}

const char *mf3d_ext_find_close(const char *p, const char *local_name)
{
    int depth, namelen;
    const char *q;
    if (!p || !local_name) return 0;
    namelen = 0;
    while (local_name[namelen]) namelen++;
    depth = 1;
    q = p + 1;
    while (*q) {
        if (*q == '<') {
            if (q[1] == '/') {
                if (local_after_prefix_eq(q + 2, local_name, 0)) {
                    depth--;
                    if (depth <= 0) return q;
                }
            } else if (mf3d_ext_tag_matches(q, local_name)) {
                const char *e;
                e = q;
                while (*e && *e != '>') e++;
                if (e > q && e[-1] != '/') depth++;
            }
        }
        q++;
    }
    return 0;
}

void mf3d_ext_attr(const char *tag, const char *attr, char *out, int outcap)
{
    int alen, i, start_ok;
    const char *p;
    char q;
    if (!out || outcap <= 0) return;
    out[0] = 0;
    if (!tag || !attr) return;
    alen = 0;
    while (attr[alen]) alen++;
    p = tag;
    while (*p && *p != '>') {
        start_ok = 0;
        if (p == tag) start_ok = 1;
        else if (is_ws(p[-1]) || p[-1] == '<') start_ok = 1;
        else if (p[-1] == ':') start_ok = 1;
        if (start_ok) {
            for (i = 0; i < alen && p[i] == attr[i]; ++i) ;
            if (i == alen && p[i] == '=') {
                q = p[i + 1];
                if (q != '\'' && q != '"') return;
                p += i + 2;
                i = 0;
                while (*p && *p != q && i < outcap - 1) out[i++] = *p++;
                out[i] = 0;
                return;
            }
        }
        p++;
    }
}

void mf3d_ext_text_between(const char *begin, const char *end, char *out, int outcap)
{
    int i;
    if (!out || outcap <= 0) return;
    out[0] = 0;
    if (!begin || !end || end <= begin) return;
    while (*begin && *begin != '>') begin++;
    if (*begin == '>') begin++;
    i = 0;
    while (begin < end && *begin && i < outcap - 1) {
        out[i++] = *begin++;
    }
    out[i] = 0;
}

static int hexv(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

int mf3d_ext_parse_hex_color(const char *s, mf3d_u8 *r, mf3d_u8 *g, mf3d_u8 *b, mf3d_u8 *a)
{
    int n[8], i;
    if (!s || !r || !g || !b || !a) return MF3D_E_BADARG;
    if (*s == '#') s++;
    for (i = 0; i < 8; ++i) n[i] = -1;
    i = 0;
    while (s[i] && i < 8) { n[i] = hexv(s[i]); if (n[i] < 0) break; i++; }
    if (i != 6 && i != 8) return MF3D_E_RANGE;
    *r = (mf3d_u8)((n[0] << 4) | n[1]);
    *g = (mf3d_u8)((n[2] << 4) | n[3]);
    *b = (mf3d_u8)((n[4] << 4) | n[5]);
    *a = (i == 8) ? (mf3d_u8)((n[6] << 4) | n[7]) : (mf3d_u8)255;
    return MF3D_OK;
}

void mf3d_ext_identity(mf3d_fix *t12)
{
    int i;
    if (!t12) return;
    for (i = 0; i < 12; ++i) t12[i] = 0;
    t12[0] = t12[5] = t12[10] = (mf3d_fix)(1L << 16);
}

void mf3d_ext_parse_transform(const char *s, mf3d_fix *t12)
{
    int i, n;
    const char *p;
    char tmp[64];
    if (!t12) return;
    mf3d_ext_identity(t12);
    if (!s || !s[0]) return;
    p = s;
    for (i = 0; i < 12; ++i) {
        while (is_ws(*p)) p++;
        n = 0;
        while (*p && !is_ws(*p) && n < 63) tmp[n++] = *p++;
        tmp[n] = 0;
        if (n > 0) mf3d_parse_fix(tmp, &t12[i]);
    }
}

mf3d_fix mf3d_ext_mul_fix(mf3d_fix a, mf3d_fix b)
{
    long sign;
    unsigned long ua, ub, ah, al, bh, bl, mid, hi, lo;
    sign = 1;
    if (a < 0) { sign = -sign; ua = (unsigned long)(-a); } else ua = (unsigned long)a;
    if (b < 0) { sign = -sign; ub = (unsigned long)(-b); } else ub = (unsigned long)b;
    ah = ua >> 16; al = ua & 65535UL;
    bh = ub >> 16; bl = ub & 65535UL;
    hi = ah * bh;
    mid = ah * bl + al * bh;
    lo = al * bl;
    /* (ua*ub)>>16 with reduced overflow risk for normal 16.16 model coords */
    ua = (hi << 16) + mid + (lo >> 16);
    return (mf3d_fix)(sign < 0 ? -(long)ua : (long)ua);
}

void mf3d_ext_apply_transform(const mf3d_fix *t12, const mf3d_vec3 *in_v, mf3d_vec3 *out_v)
{
    if (!t12 || !in_v || !out_v) return;
    out_v->x = mf3d_ext_mul_fix(t12[0], in_v->x) + mf3d_ext_mul_fix(t12[1], in_v->y) + mf3d_ext_mul_fix(t12[2], in_v->z) + t12[3];
    out_v->y = mf3d_ext_mul_fix(t12[4], in_v->x) + mf3d_ext_mul_fix(t12[5], in_v->y) + mf3d_ext_mul_fix(t12[6], in_v->z) + t12[7];
    out_v->z = mf3d_ext_mul_fix(t12[8], in_v->x) + mf3d_ext_mul_fix(t12[9], in_v->y) + mf3d_ext_mul_fix(t12[10], in_v->z) + t12[11];
}
