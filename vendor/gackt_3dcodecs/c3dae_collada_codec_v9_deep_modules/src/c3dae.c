#include "c3dae.h"

#include <stddef.h>
#include <string.h>

#define C3DAE_XML_NIL ((const char *)0)
#define C3DAE_S32_MAX_VAL ((c3dae_s32)2147483647L)
#define C3DAE_S32_MIN_VAL ((c3dae_s32)(-2147483647L - 1L))

typedef struct c3dae_input_tag {
    char semantic[24];
    int source_index;
    unsigned int offset;
    unsigned int set_index;
} c3dae_input;

typedef struct c3dae_sampler_refs_tag {
    char id[C3DAE_ID_MAX];
    int input_source;
    int output_source;
    int interpolation_source;
    int in_tangent_source;
    int out_tangent_source;
} c3dae_sampler_refs;

static int c3dae_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int c3dae_is_name_char(char c)
{
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= '0' && c <= '9') return 1;
    if (c == '_' || c == '-' || c == ':' || c == '.' || c == '/') return 1;
    return 0;
}

static unsigned int c3dae_strlen_lit(const char *s)
{
    unsigned int n;
    n = 0U;
    while (s[n] != '\0') n++;
    return n;
}

static int c3dae_mem_eq(const char *a, const char *b, unsigned int n)
{
    unsigned int i;
    i = 0U;
    while (i < n) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return 1;
}

static void c3dae_copy_text(char *dst, unsigned int dst_cap, const char *a, const char *b)
{
    unsigned int n;
    unsigned int i;
    if (dst_cap == 0U) return;
    if (a == (const char *)0 || b == (const char *)0 || b < a) {
        dst[0] = '\0';
        return;
    }
    n = (unsigned int)(b - a);
    if (n >= dst_cap) n = dst_cap - 1U;
    i = 0U;
    while (i < n) {
        dst[i] = a[i];
        i++;
    }
    dst[i] = '\0';
}

static int c3dae_local_name_eq(const char *name_start, const char *name_end, const char *want)
{
    const char *p;
    const char *local;
    unsigned int want_len;
    p = name_start;
    local = name_start;
    while (p < name_end) {
        if (*p == ':') local = p + 1;
        p++;
    }
    want_len = c3dae_strlen_lit(want);
    if ((unsigned int)(name_end - local) != want_len) return 0;
    return c3dae_mem_eq(local, want, want_len);
}

static const char *c3dae_find_char(const char *p, const char *end, char c)
{
    while (p < end) {
        if (*p == c) return p;
        p++;
    }
    return C3DAE_XML_NIL;
}

static int c3dae_tag_is_self_closing(const char *tag_start, const char *tag_end)
{
    const char *p;
    if (tag_end <= tag_start) return 0;
    p = tag_end;
    while (p > tag_start) {
        p--;
        if (c3dae_is_space(*p) || *p == '>') continue;
        if (*p == '/') return 1;
        return 0;
    }
    return 0;
}

static int c3dae_start_tag_at(const char *p, const char *end, const char *name, const char **tag_end)
{
    const char *q;
    const char *name_start;
    const char *name_end;
    if (p >= end || *p != '<') return 0;
    q = p + 1;
    if (q >= end) return 0;
    if (*q == '/' || *q == '!' || *q == '?') return 0;
    name_start = q;
    while (q < end && c3dae_is_name_char(*q) && *q != '/') q++;
    name_end = q;
    if (name_start == name_end) return 0;
    if (!c3dae_local_name_eq(name_start, name_end, name)) return 0;
    if (q < end && !c3dae_is_space(*q) && *q != '>' && *q != '/') return 0;
    q = c3dae_find_char(q, end, '>');
    if (q == C3DAE_XML_NIL) return 0;
    if (tag_end != (const char **)0) *tag_end = q + 1;
    return 1;
}

static int c3dae_end_tag_at(const char *p, const char *end, const char *name, const char **after_tag)
{
    const char *q;
    const char *name_start;
    const char *name_end;
    if (p >= end || *p != '<') return 0;
    q = p + 1;
    if (q >= end || *q != '/') return 0;
    q++;
    name_start = q;
    while (q < end && c3dae_is_name_char(*q) && *q != '/') q++;
    name_end = q;
    if (!c3dae_local_name_eq(name_start, name_end, name)) return 0;
    q = c3dae_find_char(q, end, '>');
    if (q == C3DAE_XML_NIL) return 0;
    if (after_tag != (const char **)0) *after_tag = q + 1;
    return 1;
}

static const char *c3dae_find_start_tag(const char *p, const char *end, const char *name, const char **tag_end)
{
    while (p < end) {
        if (*p == '<') {
            if (c3dae_start_tag_at(p, end, name, tag_end)) return p;
        }
        p++;
    }
    return C3DAE_XML_NIL;
}

static const char *c3dae_find_end_tag(const char *p, const char *end, const char *name, const char **after_tag)
{
    while (p < end) {
        if (*p == '<') {
            if (c3dae_end_tag_at(p, end, name, after_tag)) return p;
        }
        p++;
    }
    return C3DAE_XML_NIL;
}

static const char *c3dae_find_matching_end_tag(const char *tag_start,
                                               const char *tag_end,
                                               const char *limit,
                                               const char *name,
                                               const char **after_tag)
{
    const char *p;
    const char *te;
    int depth;
    if (c3dae_tag_is_self_closing(tag_start, tag_end)) {
        if (after_tag != (const char **)0) *after_tag = tag_end;
        return tag_end;
    }
    depth = 1;
    p = tag_end;
    while (p < limit) {
        if (*p == '<') {
            if (c3dae_start_tag_at(p, limit, name, &te)) {
                if (!c3dae_tag_is_self_closing(p, te)) depth++;
                p = te;
                continue;
            }
            if (c3dae_end_tag_at(p, limit, name, &te)) {
                depth--;
                if (depth == 0) {
                    if (after_tag != (const char **)0) *after_tag = te;
                    return p;
                }
                p = te;
                continue;
            }
        }
        p++;
    }
    return C3DAE_XML_NIL;
}

static int c3dae_attr_get(const char *tag_start,
                          const char *tag_end,
                          const char *attr,
                          char *out,
                          unsigned int out_cap)
{
    const char *p;
    const char *name_start;
    const char *name_end;
    const char *val_start;
    char quote;
    unsigned int attr_len;
    p = tag_start;
    attr_len = c3dae_strlen_lit(attr);
    if (out_cap > 0U) out[0] = '\0';
    while (p < tag_end) {
        while (p < tag_end && !c3dae_is_name_char(*p)) p++;
        if (p >= tag_end) break;
        name_start = p;
        while (p < tag_end && c3dae_is_name_char(*p)) p++;
        name_end = p;
        while (p < tag_end && c3dae_is_space(*p)) p++;
        if (p >= tag_end || *p != '=') continue;
        p++;
        while (p < tag_end && c3dae_is_space(*p)) p++;
        if (p >= tag_end) break;
        if (*p != '\'' && *p != '"') continue;
        quote = *p;
        p++;
        val_start = p;
        while (p < tag_end && *p != quote) p++;
        if ((unsigned int)(name_end - name_start) == attr_len &&
            c3dae_mem_eq(name_start, attr, attr_len)) {
            c3dae_copy_text(out, out_cap, val_start, p);
            return 1;
        }
        if (p < tag_end) p++;
    }
    return 0;
}

static int c3dae_attr_get_u32(const char *tag_start,
                              const char *tag_end,
                              const char *attr,
                              c3dae_u32 *out)
{
    char tmp[32];
    const char *p;
    c3dae_u32 v;
    if (!c3dae_attr_get(tag_start, tag_end, attr, tmp, sizeof(tmp))) return 0;
    p = tmp;
    v = 0UL;
    while (*p >= '0' && *p <= '9') {
        v = (v * 10UL) + (c3dae_u32)(*p - '0');
        p++;
    }
    *out = v;
    return 1;
}

static void c3dae_set_error(c3dae_context *ctx, int code, const char *msg)
{
    unsigned int i;
    if (ctx == (c3dae_context *)0) return;
    ctx->error_code = code;
    i = 0U;
    while (msg != (const char *)0 && msg[i] != '\0' && i + 1U < sizeof(ctx->error_message)) {
        ctx->error_message[i] = msg[i];
        i++;
    }
    ctx->error_message[i] = '\0';
}

static c3dae_s32 c3dae_sat_mul10(c3dae_s32 v)
{
    if (v > C3DAE_S32_MAX_VAL / 10L) return C3DAE_S32_MAX_VAL;
    if (v < C3DAE_S32_MIN_VAL / 10L) return C3DAE_S32_MIN_VAL;
    return v * 10L;
}

static int c3dae_parse_fixed_token(const char **pp, const char *end, c3dae_fixed *out)
{
    const char *p;
    int sign;
    c3dae_s32 ip;
    c3dae_s32 frac;
    c3dae_s32 scale;
    c3dae_s32 val;
    int have_digit;
    int frac_digits;
    int exp_sign;
    int exp_val;
    p = *pp;
    while (p < end && c3dae_is_space(*p)) p++;
    if (p >= end) return 0;
    sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    ip = 0L;
    have_digit = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        if (ip < 32767L) ip = (ip * 10L) + (c3dae_s32)(*p - '0');
        have_digit = 1;
        p++;
    }
    frac = 0L;
    scale = 1L;
    frac_digits = 0;
    if (p < end && *p == '.') {
        p++;
        while (p < end && *p >= '0' && *p <= '9') {
            if (frac_digits < 6) {
                frac = (frac * 10L) + (c3dae_s32)(*p - '0');
                scale = scale * 10L;
                frac_digits++;
            }
            have_digit = 1;
            p++;
        }
    }
    if (!have_digit) return 0;
    val = (c3dae_s32)(ip << C3DAE_FIXED_SHIFT);
    if (frac_digits > 0) val += (c3dae_s32)((frac << C3DAE_FIXED_SHIFT) / scale);
    if (p < end && (*p == 'e' || *p == 'E')) {
        p++;
        exp_sign = 1;
        if (p < end && *p == '-') { exp_sign = -1; p++; }
        else if (p < end && *p == '+') p++;
        exp_val = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            if (exp_val < 12) exp_val = (exp_val * 10) + (int)(*p - '0');
            p++;
        }
        if (exp_sign > 0) {
            while (exp_val > 0) { val = c3dae_sat_mul10(val); exp_val--; }
        } else {
            while (exp_val > 0) { val = val / 10L; exp_val--; }
        }
    }
    if (sign < 0) val = -val;
    *out = val;
    *pp = p;
    return 1;
}

static int c3dae_attr_get_fixed(const char *tag_start,
                                const char *tag_end,
                                const char *attr,
                                c3dae_fixed *out)
{
    char tmp[48];
    const char *p;
    const char *e;
    if (!c3dae_attr_get(tag_start, tag_end, attr, tmp, sizeof(tmp))) return 0;
    p = tmp;
    e = tmp + c3dae_strlen_lit(tmp);
    return c3dae_parse_fixed_token(&p, e, out);
}

static int c3dae_parse_u32_token(const char **pp, const char *end, c3dae_u32 *out)
{
    const char *p;
    c3dae_u32 v;
    int have;
    p = *pp;
    while (p < end && c3dae_is_space(*p)) p++;
    if (p >= end) return 0;
    v = 0UL;
    have = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        v = (v * 10UL) + (c3dae_u32)(*p - '0');
        have = 1;
        p++;
    }
    if (!have) return 0;
    *out = v;
    *pp = p;
    return 1;
}

static int c3dae_parse_s32_token(const char **pp, const char *end, c3dae_s32 *out)
{
    const char *p;
    c3dae_s32 v;
    int sign;
    int have;
    p = *pp;
    while (p < end && c3dae_is_space(*p)) p++;
    if (p >= end) return 0;
    sign = 1;
    if (*p == '-') { sign = -1; p++; }
    else if (*p == '+') p++;
    v = 0L;
    have = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        v = (v * 10L) + (c3dae_s32)(*p - '0');
        have = 1;
        p++;
    }
    if (!have) return 0;
    if (sign < 0) v = -v;
    *out = v;
    *pp = p;
    return 1;
}

static int c3dae_parse_name_token(const char **pp, const char *end, char *out, unsigned int cap)
{
    const char *p;
    const char *a;
    p = *pp;
    while (p < end && c3dae_is_space(*p)) p++;
    if (p >= end) return 0;
    a = p;
    while (p < end && !c3dae_is_space(*p) && *p != '<' && *p != '>') p++;
    if (a == p) return 0;
    c3dae_copy_text(out, cap, a, p);
    *pp = p;
    return 1;
}

static int c3dae_ref_equals_id(const char *ref, const char *id)
{
    const char *r;
    r = ref;
    if (*r == '#') r++;
    return strcmp(r, id) == 0;
}

static void c3dae_ref_to_id(char *out, unsigned int cap, const char *ref)
{
    const char *r;
    r = ref;
    if (r != (const char *)0 && *r == '#') r++;
    if (r == (const char *)0) r = "";
    c3dae_copy_text(out, cap, r, r + c3dae_strlen_lit(r));
}

static void c3dae_matrix_identity(c3dae_fixed *m)
{
    unsigned int i;
    i = 0U;
    while (i < 16U) { m[i] = 0L; i++; }
    m[0] = C3DAE_FIXED_ONE;
    m[5] = C3DAE_FIXED_ONE;
    m[10] = C3DAE_FIXED_ONE;
    m[15] = C3DAE_FIXED_ONE;
}

static void c3dae_matrix_copy(c3dae_fixed *dst, const c3dae_fixed *src)
{
    unsigned int i;
    i = 0U;
    while (i < 16U) { dst[i] = src[i]; i++; }
}

static c3dae_fixed c3dae_fixed_mul(c3dae_fixed a, c3dae_fixed b)
{
    /* C89-friendly approximate Q16.16 multiply avoiding 64-bit requirements. */
    return (c3dae_fixed)((a >> 8) * (b >> 8));
}

static c3dae_fixed c3dae_fixed_lerp(c3dae_fixed a, c3dae_fixed b, c3dae_fixed t)
{
    return a + c3dae_fixed_mul((c3dae_fixed)(b - a), t);
}

static c3dae_fixed c3dae_fixed_bezier(c3dae_fixed p0,
                                      c3dae_fixed c0,
                                      c3dae_fixed c1,
                                      c3dae_fixed p1,
                                      c3dae_fixed t)
{
    c3dae_fixed one_minus;
    c3dae_fixed t2;
    c3dae_fixed t3;
    c3dae_fixed u2;
    c3dae_fixed u3;
    c3dae_fixed three;
    c3dae_fixed r;
    one_minus = C3DAE_FIXED_ONE - t;
    t2 = c3dae_fixed_mul(t, t);
    t3 = c3dae_fixed_mul(t2, t);
    u2 = c3dae_fixed_mul(one_minus, one_minus);
    u3 = c3dae_fixed_mul(u2, one_minus);
    three = (c3dae_fixed)(3L << C3DAE_FIXED_SHIFT);
    r = c3dae_fixed_mul(u3, p0);
    r += c3dae_fixed_mul(c3dae_fixed_mul(c3dae_fixed_mul(three, u2), t), c0);
    r += c3dae_fixed_mul(c3dae_fixed_mul(c3dae_fixed_mul(three, one_minus), t2), c1);
    r += c3dae_fixed_mul(t3, p1);
    return r;
}

static c3dae_fixed c3dae_fixed_div_safe(c3dae_fixed a, c3dae_fixed b)
{
    c3dae_fixed aa;
    c3dae_fixed bb;
    c3dae_fixed sign;
    c3dae_fixed q;
    if (b == 0L) return 0L;
    sign = 1L;
    aa = a;
    bb = b;
    if (aa < 0L) { aa = -aa; sign = -sign; }
    if (bb < 0L) { bb = -bb; sign = -sign; }
    if (aa <= 65535L) q = (c3dae_fixed)(((unsigned long)aa << C3DAE_FIXED_SHIFT) / (unsigned long)bb);
    else q = (aa / bb) << C3DAE_FIXED_SHIFT;
    return sign < 0L ? -q : q;
}

static void c3dae_matrix_mul(c3dae_fixed *out, const c3dae_fixed *a, const c3dae_fixed *b)
{
    c3dae_fixed r[16];
    unsigned int row;
    unsigned int col;
    unsigned int k;
    c3dae_fixed sum;
    row = 0U;
    while (row < 4U) {
        col = 0U;
        while (col < 4U) {
            sum = 0L;
            k = 0U;
            while (k < 4U) {
                sum += c3dae_fixed_mul(a[row * 4U + k], b[k * 4U + col]);
                k++;
            }
            r[row * 4U + col] = sum;
            col++;
        }
        row++;
    }
    c3dae_matrix_copy(out, r);
}

static void c3dae_matrix_make_translate(c3dae_fixed *m, c3dae_fixed x, c3dae_fixed y, c3dae_fixed z)
{
    c3dae_matrix_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

static void c3dae_matrix_make_scale(c3dae_fixed *m, c3dae_fixed x, c3dae_fixed y, c3dae_fixed z)
{
    c3dae_matrix_identity(m);
    m[0] = x;
    m[5] = y;
    m[10] = z;
}

static c3dae_fixed c3dae_fixed_abs(c3dae_fixed v)
{
    return v < 0L ? -v : v;
}

static c3dae_fixed c3dae_sin_deg_milli(c3dae_s32 deg_milli)
{
    static const c3dae_fixed tab[19] = {
        0L, 5712L, 11380L, 16962L, 22415L,
        27697L, 32768L, 37590L, 42126L, 46341L,
        50203L, 53684L, 56756L, 59396L, 61584L,
        63303L, 64540L, 65287L, 65536L
    };
    c3dae_s32 a;
    c3dae_s32 q;
    c3dae_s32 rem;
    int sign;
    unsigned int idx;
    c3dae_fixed t;
    while (deg_milli < 0L) deg_milli += 360000L;
    while (deg_milli >= 360000L) deg_milli -= 360000L;
    sign = 1;
    a = deg_milli;
    if (a >= 180000L) { sign = -1; a -= 180000L; }
    if (a > 90000L) a = 180000L - a;
    q = a / 5000L;
    rem = a - (q * 5000L);
    if (q >= 18L) return sign > 0 ? C3DAE_FIXED_ONE : -C3DAE_FIXED_ONE;
    idx = (unsigned int)q;
    t = (c3dae_fixed)((rem << C3DAE_FIXED_SHIFT) / 5000L);
    return sign > 0 ? c3dae_fixed_lerp(tab[idx], tab[idx + 1U], t) : -c3dae_fixed_lerp(tab[idx], tab[idx + 1U], t);
}

c3dae_fixed c3dae_fixed_sin_deg(c3dae_fixed degrees)
{
    c3dae_s32 milli;
    milli = (c3dae_s32)((degrees * 1000L) >> C3DAE_FIXED_SHIFT);
    return c3dae_sin_deg_milli(milli);
}

c3dae_fixed c3dae_fixed_cos_deg(c3dae_fixed degrees)
{
    c3dae_s32 milli;
    milli = (c3dae_s32)((degrees * 1000L) >> C3DAE_FIXED_SHIFT);
    return c3dae_sin_deg_milli(milli + 90000L);
}

void c3dae_matrix_from_rotate(c3dae_fixed *m,
                              c3dae_fixed x,
                              c3dae_fixed y,
                              c3dae_fixed z,
                              c3dae_fixed degrees)
{
    c3dae_fixed c;
    c3dae_fixed s;
    c3dae_fixed t;
    c3dae_fixed xx;
    c3dae_fixed yy;
    c3dae_fixed zz;
    c3dae_fixed xy;
    c3dae_fixed xz;
    c3dae_fixed yz;
    if (m == (c3dae_fixed *)0) return;
    c3dae_matrix_identity(m);
    if (c3dae_fixed_abs(x) >= C3DAE_FIXED_ONE - 64L && y == 0L && z == 0L) {
        c = c3dae_fixed_cos_deg(degrees);
        s = c3dae_fixed_sin_deg(degrees);
        if (x < 0L) s = -s;
        m[5] = c; m[6] = s; m[9] = -s; m[10] = c;
        return;
    }
    if (x == 0L && c3dae_fixed_abs(y) >= C3DAE_FIXED_ONE - 64L && z == 0L) {
        c = c3dae_fixed_cos_deg(degrees);
        s = c3dae_fixed_sin_deg(degrees);
        if (y < 0L) s = -s;
        m[0] = c; m[2] = -s; m[8] = s; m[10] = c;
        return;
    }
    if (x == 0L && y == 0L && c3dae_fixed_abs(z) >= C3DAE_FIXED_ONE - 64L) {
        c = c3dae_fixed_cos_deg(degrees);
        s = c3dae_fixed_sin_deg(degrees);
        if (z < 0L) s = -s;
        m[0] = c; m[1] = s; m[4] = -s; m[5] = c;
        return;
    }
    c = c3dae_fixed_cos_deg(degrees);
    s = c3dae_fixed_sin_deg(degrees);
    t = C3DAE_FIXED_ONE - c;
    xx = c3dae_fixed_mul(x, x);
    yy = c3dae_fixed_mul(y, y);
    zz = c3dae_fixed_mul(z, z);
    xy = c3dae_fixed_mul(x, y);
    xz = c3dae_fixed_mul(x, z);
    yz = c3dae_fixed_mul(y, z);
    m[0] = c + c3dae_fixed_mul(t, xx);
    m[1] = c3dae_fixed_mul(t, xy) + c3dae_fixed_mul(s, z);
    m[2] = c3dae_fixed_mul(t, xz) - c3dae_fixed_mul(s, y);
    m[4] = c3dae_fixed_mul(t, xy) - c3dae_fixed_mul(s, z);
    m[5] = c + c3dae_fixed_mul(t, yy);
    m[6] = c3dae_fixed_mul(t, yz) + c3dae_fixed_mul(s, x);
    m[8] = c3dae_fixed_mul(t, xz) + c3dae_fixed_mul(s, y);
    m[9] = c3dae_fixed_mul(t, yz) - c3dae_fixed_mul(s, x);
    m[10] = c + c3dae_fixed_mul(t, zz);
}

static int c3dae_find_source(c3dae_context *ctx, const char *ref)
{
    unsigned int i;
    i = 0U;
    while (i < ctx->source_count) {
        if (c3dae_ref_equals_id(ref, ctx->sources[i].id)) return (int)i;
        i++;
    }
    return -1;
}

static int c3dae_find_vbinding(c3dae_context *ctx, const char *ref)
{
    unsigned int i;
    i = 0U;
    while (i < ctx->vbinding_count) {
        if (c3dae_ref_equals_id(ref, ctx->vbindings[i].id)) return (int)i;
        i++;
    }
    return -1;
}

static int c3dae_find_mesh_by_id(c3dae_model *m, const char *ref)
{
    c3dae_u32 i;
    char id[C3DAE_ID_MAX];
    c3dae_ref_to_id(id, sizeof(id), ref);
    i = 0UL;
    while (i < m->mesh_count) {
        if (strcmp(m->meshes[i].id, id) == 0) return (int)i;
        i++;
    }
    return -1;
}

static int c3dae_find_controller_by_id(c3dae_model *m, const char *ref)
{
    c3dae_u32 i;
    char id[C3DAE_ID_MAX];
    c3dae_ref_to_id(id, sizeof(id), ref);
    i = 0UL;
    while (i < m->controller_count) {
        if (strcmp(m->controllers[i].id, id) == 0) return (int)i;
        i++;
    }
    return -1;
}

static int c3dae_find_material_by_id(c3dae_model *m, const char *ref)
{
    c3dae_u32 i;
    char id[C3DAE_ID_MAX];
    c3dae_ref_to_id(id, sizeof(id), ref);
    i = 0UL;
    while (i < m->material_count) {
        if (strcmp(m->materials[i].id, id) == 0) return (int)i;
        if (strcmp(m->materials[i].name, id) == 0) return (int)i;
        i++;
    }
    return -1;
}

static int c3dae_find_camera_by_id(c3dae_model *m, const char *ref)
{
    c3dae_u32 i;
    char id[C3DAE_ID_MAX];
    c3dae_ref_to_id(id, sizeof(id), ref);
    i = 0UL;
    while (i < m->camera_count) {
        if (strcmp(m->cameras[i].id, id) == 0) return (int)i;
        i++;
    }
    return -1;
}

static int c3dae_find_light_by_id(c3dae_model *m, const char *ref)
{
    c3dae_u32 i;
    char id[C3DAE_ID_MAX];
    c3dae_ref_to_id(id, sizeof(id), ref);
    i = 0UL;
    while (i < m->light_count) {
        if (strcmp(m->lights[i].id, id) == 0) return (int)i;
        i++;
    }
    return -1;
}

static int c3dae_find_node_by_id_or_sid(const c3dae_model *m, const char *name)
{
    c3dae_u32 i;
    const char *r;
    r = name;
    if (r != (const char *)0 && *r == '#') r++;
    i = 0UL;
    while (i < m->node_count) {
        if (strcmp(m->nodes[i].id, r) == 0) return (int)i;
        if (strcmp(m->nodes[i].sid, r) == 0) return (int)i;
        i++;
    }
    return -1;
}

static void c3dae_reset_temp_sources(c3dae_context *ctx)
{
    ctx->source_data_count = 0UL;
    ctx->source_name_count = 0UL;
    ctx->source_count = 0U;
    ctx->vbinding_count = 0U;
}

static int c3dae_parse_sources(c3dae_context *ctx, const char *mesh_a, const char *mesh_b)
{
    const char *p;
    const char *tag;
    const char *tag_end;
    const char *src_end;
    const char *after_src;
    const char *fa;
    const char *fa_tag_end;
    const char *fa_end;
    const char *after_fa;
    const char *na;
    const char *na_tag_end;
    const char *na_end;
    const char *after_na;
    const char *acc;
    const char *acc_tag_end;
    c3dae_source *s;
    c3dae_fixed val;
    c3dae_u32 stride;
    c3dae_u32 off;
    c3dae_u32 raw_start;
    c3dae_u32 raw_count;
    char tok[C3DAE_ID_MAX];
    p = mesh_a;
    while (1) {
        tag = c3dae_find_start_tag(p, mesh_b, "source", &tag_end);
        if (tag == C3DAE_XML_NIL) break;
        src_end = c3dae_find_end_tag(tag_end, mesh_b, "source", &after_src);
        if (src_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        if (ctx->source_count >= C3DAE_MAX_SOURCES) return C3DAE_ERR_CAPACITY;
        s = &ctx->sources[ctx->source_count];
        memset(s, 0, sizeof(*s));
        if (!c3dae_attr_get(tag, tag_end, "id", s->id, C3DAE_ID_MAX)) c3dae_attr_get(tag, tag_end, "name", s->id, C3DAE_ID_MAX);
        s->first = ctx->source_data_count;
        s->stride = 1UL;
        s->offset = 0UL;
        s->name_first = ctx->source_name_count;
        s->is_name_array = 0;
        fa = c3dae_find_start_tag(tag_end, src_end, "float_array", &fa_tag_end);
        if (fa != C3DAE_XML_NIL) {
            fa_end = c3dae_find_end_tag(fa_tag_end, src_end, "float_array", &after_fa);
            if (fa_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
            raw_start = ctx->source_data_count;
            p = fa_tag_end;
            while (p < fa_end) {
                while (p < fa_end && c3dae_is_space(*p)) p++;
                if (p >= fa_end) break;
                if (ctx->source_data_count >= C3DAE_MAX_SOURCE_FLOATS) return C3DAE_ERR_CAPACITY;
                if (!c3dae_parse_fixed_token(&p, fa_end, &val)) return C3DAE_ERR_BAD_NUMBER;
                ctx->source_data[ctx->source_data_count] = val;
                ctx->source_data_count++;
            }
            raw_count = ctx->source_data_count - raw_start;
            s->raw_count = raw_count;
        } else {
            na = c3dae_find_start_tag(tag_end, src_end, "Name_array", &na_tag_end);
            if (na == C3DAE_XML_NIL) na = c3dae_find_start_tag(tag_end, src_end, "IDREF_array", &na_tag_end);
            if (na == C3DAE_XML_NIL) na = c3dae_find_start_tag(tag_end, src_end, "SIDREF_array", &na_tag_end);
            if (na != C3DAE_XML_NIL) {
                na_end = c3dae_find_end_tag(na_tag_end, src_end, "Name_array", &after_na);
                if (na_end == C3DAE_XML_NIL) na_end = c3dae_find_end_tag(na_tag_end, src_end, "IDREF_array", &after_na);
                if (na_end == C3DAE_XML_NIL) na_end = c3dae_find_end_tag(na_tag_end, src_end, "SIDREF_array", &after_na);
                if (na_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
                p = na_tag_end;
                s->is_name_array = 1;
                while (p < na_end) {
                    while (p < na_end && c3dae_is_space(*p)) p++;
                    if (p >= na_end) break;
                    if (ctx->source_name_count >= (c3dae_u32)C3DAE_MAX_SOURCE_NAMES) return C3DAE_ERR_CAPACITY;
                    if (!c3dae_parse_name_token(&p, na_end, tok, sizeof(tok))) return C3DAE_ERR_XML;
                    c3dae_copy_text(ctx->source_names[ctx->source_name_count], C3DAE_ID_MAX, tok, tok + c3dae_strlen_lit(tok));
                    ctx->source_name_count++;
                }
                s->name_count = ctx->source_name_count - s->name_first;
                s->raw_count = s->name_count;
            }
        }
        acc = c3dae_find_start_tag(tag_end, src_end, "accessor", &acc_tag_end);
        if (acc != C3DAE_XML_NIL) {
            stride = 1UL;
            off = 0UL;
            c3dae_attr_get_u32(acc, acc_tag_end, "stride", &stride);
            c3dae_attr_get_u32(acc, acc_tag_end, "offset", &off);
            if (stride == 0UL) stride = 1UL;
            s->stride = stride;
            s->offset = off;
        } else {
            if (s->raw_count >= 16UL && (s->raw_count % 16UL) == 0UL) s->stride = 16UL;
            else if (s->raw_count >= 4UL && (s->raw_count % 4UL) == 0UL) s->stride = 4UL;
            else if (s->raw_count >= 3UL && (s->raw_count % 3UL) == 0UL) s->stride = 3UL;
            else if (s->raw_count >= 2UL && (s->raw_count % 2UL) == 0UL) s->stride = 2UL;
            else s->stride = 1UL;
        }
        if (s->stride > 0UL && s->raw_count >= s->offset) s->item_count = (s->raw_count - s->offset) / s->stride;
        ctx->source_count++;
        p = after_src;
    }
    return C3DAE_OK;
}

static int c3dae_source_name_at(c3dae_context *ctx, int source_index, c3dae_u32 item, char *out, unsigned int cap)
{
    c3dae_source *s;
    if (source_index < 0) return C3DAE_ERR_NOT_FOUND;
    s = &ctx->sources[source_index];
    if (!s->is_name_array) return C3DAE_ERR_XML;
    if (item >= s->name_count) return C3DAE_ERR_XML;
    c3dae_copy_text(out, cap, ctx->source_names[s->name_first + item], ctx->source_names[s->name_first + item] + c3dae_strlen_lit(ctx->source_names[s->name_first + item]));
    return C3DAE_OK;
}

static int c3dae_sample_source(c3dae_context *ctx,
                               int source_index,
                               c3dae_u32 item_index,
                               c3dae_fixed *a,
                               c3dae_fixed *b,
                               c3dae_fixed *c)
{
    c3dae_source *s;
    c3dae_u32 base;
    if (source_index < 0) return C3DAE_ERR_NOT_FOUND;
    s = &ctx->sources[source_index];
    if (item_index >= s->item_count) return C3DAE_ERR_XML;
    base = s->first + s->offset + (item_index * s->stride);
    if (base >= C3DAE_MAX_SOURCE_FLOATS) return C3DAE_ERR_XML;
    *a = ctx->source_data[base];
    if (s->stride > 1UL && base + 1UL < C3DAE_MAX_SOURCE_FLOATS) *b = ctx->source_data[base + 1UL];
    else *b = 0L;
    if (s->stride > 2UL && base + 2UL < C3DAE_MAX_SOURCE_FLOATS) *c = ctx->source_data[base + 2UL];
    else *c = 0L;
    return C3DAE_OK;
}

static int c3dae_sample_matrix_source(c3dae_context *ctx, int source_index, c3dae_u32 item_index, c3dae_fixed *m)
{
    c3dae_source *s;
    c3dae_u32 base;
    unsigned int i;
    if (source_index < 0) return C3DAE_ERR_NOT_FOUND;
    s = &ctx->sources[source_index];
    if (item_index >= s->item_count || s->stride < 16UL) return C3DAE_ERR_XML;
    base = s->first + s->offset + (item_index * s->stride);
    if (base + 15UL >= C3DAE_MAX_SOURCE_FLOATS) return C3DAE_ERR_XML;
    i = 0U;
    while (i < 16U) { m[i] = ctx->source_data[base + (c3dae_u32)i]; i++; }
    return C3DAE_OK;
}

static c3dae_fixed c3dae_source_value_at(c3dae_context *ctx, int source_index, c3dae_u32 item_index, c3dae_u32 component)
{
    c3dae_source *s;
    c3dae_u32 base;
    if (source_index < 0) return 0L;
    s = &ctx->sources[source_index];
    if (item_index >= s->item_count) return 0L;
    if (component >= s->stride) return 0L;
    base = s->first + s->offset + (item_index * s->stride) + component;
    if (base >= C3DAE_MAX_SOURCE_FLOATS) return 0L;
    return ctx->source_data[base];
}

static void c3dae_source_sample_tangent(c3dae_context *ctx,
                                        int source_index,
                                        c3dae_u32 item_index,
                                        unsigned int value_count,
                                        c3dae_fixed *out_time,
                                        c3dae_fixed *out_value)
{
    c3dae_source *s;
    c3dae_u32 base;
    unsigned int j;
    if (out_time != (c3dae_fixed *)0) *out_time = 0L;
    if (source_index < 0) return;
    s = &ctx->sources[source_index];
    if (item_index >= s->item_count) return;
    base = s->first + s->offset + (item_index * s->stride);
    if (base >= C3DAE_MAX_SOURCE_FLOATS) return;
    if (s->stride >= (c3dae_u32)(value_count * 2U)) {
        if (out_time != (c3dae_fixed *)0) *out_time = ctx->source_data[base];
        j = 0U;
        while (j < value_count) {
            out_value[j] = ctx->source_data[base + 1UL + (c3dae_u32)(j * 2U)];
            j++;
        }
    } else if (s->stride >= (c3dae_u32)value_count) {
        j = 0U;
        while (j < value_count) {
            out_value[j] = ctx->source_data[base + (c3dae_u32)j];
            j++;
        }
    }
}

static int c3dae_parse_vertex_bindings(c3dae_context *ctx, const char *mesh_a, const char *mesh_b)
{
    const char *p;
    const char *tag;
    const char *tag_end;
    const char *v_end;
    const char *after_v;
    const char *inp;
    const char *inp_end;
    char sem[32];
    char src[96];
    c3dae_vertex_binding *b;
    p = mesh_a;
    while (1) {
        tag = c3dae_find_start_tag(p, mesh_b, "vertices", &tag_end);
        if (tag == C3DAE_XML_NIL) break;
        v_end = c3dae_find_end_tag(tag_end, mesh_b, "vertices", &after_v);
        if (v_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        if (ctx->vbinding_count >= C3DAE_MAX_VERTEX_BINDINGS) return C3DAE_ERR_CAPACITY;
        b = &ctx->vbindings[ctx->vbinding_count];
        b->id[0] = '\0';
        b->position_source = -1;
        c3dae_attr_get(tag, tag_end, "id", b->id, C3DAE_ID_MAX);
        inp = tag_end;
        while (1) {
            inp = c3dae_find_start_tag(inp, v_end, "input", &inp_end);
            if (inp == C3DAE_XML_NIL) break;
            sem[0] = '\0';
            src[0] = '\0';
            c3dae_attr_get(inp, inp_end, "semantic", sem, sizeof(sem));
            c3dae_attr_get(inp, inp_end, "source", src, sizeof(src));
            if (strcmp(sem, "POSITION") == 0) b->position_source = c3dae_find_source(ctx, src);
            inp = inp_end;
        }
        ctx->vbinding_count++;
        p = after_v;
    }
    return C3DAE_OK;
}

static int c3dae_read_primitive_inputs(c3dae_context *ctx,
                                       const char *prim_a,
                                       const char *prim_b,
                                       c3dae_input *inputs,
                                       unsigned int *input_count,
                                       unsigned int *max_offset)
{
    const char *p;
    const char *inp;
    const char *inp_end;
    char sem[32];
    char src[96];
    c3dae_u32 off;
    c3dae_u32 setv;
    int si;
    int vi;
    unsigned int n;
    n = 0U;
    *max_offset = 0U;
    p = prim_a;
    while (1) {
        inp = c3dae_find_start_tag(p, prim_b, "input", &inp_end);
        if (inp == C3DAE_XML_NIL) break;
        if (n >= C3DAE_MAX_INPUTS) return C3DAE_ERR_CAPACITY;
        sem[0] = '\0';
        src[0] = '\0';
        off = 0UL;
        setv = 0UL;
        c3dae_attr_get(inp, inp_end, "semantic", sem, sizeof(sem));
        c3dae_attr_get(inp, inp_end, "source", src, sizeof(src));
        c3dae_attr_get_u32(inp, inp_end, "offset", &off);
        c3dae_attr_get_u32(inp, inp_end, "set", &setv);
        si = -1;
        if (strcmp(sem, "VERTEX") == 0) {
            vi = c3dae_find_vbinding(ctx, src);
            if (vi >= 0) si = ctx->vbindings[vi].position_source;
        } else if (strcmp(sem, "POSITION") == 0 || strcmp(sem, "NORMAL") == 0 || strcmp(sem, "TEXCOORD") == 0 || strcmp(sem, "UV") == 0) {
            si = c3dae_find_source(ctx, src);
        }
        inputs[n].semantic[0] = '\0';
        c3dae_copy_text(inputs[n].semantic, sizeof(inputs[n].semantic), sem, sem + c3dae_strlen_lit(sem));
        inputs[n].source_index = si;
        inputs[n].offset = (unsigned int)off;
        inputs[n].set_index = (unsigned int)setv;
        if (inputs[n].offset > *max_offset) *max_offset = inputs[n].offset;
        n++;
        p = inp_end;
    }
    *input_count = n;
    if (*max_offset + 1U > 16U) return C3DAE_ERR_UNSUPPORTED;
    return C3DAE_OK;
}

static int c3dae_emit_vertex(c3dae_context *ctx,
                             c3dae_model *model,
                             c3dae_input *inputs,
                             unsigned int input_count,
                             c3dae_u32 *offset_values,
                             c3dae_u32 *out_index)
{
    c3dae_vertex v;
    unsigned int i;
    c3dae_fixed a;
    c3dae_fixed b;
    c3dae_fixed c;
    int rc;
    if (model->vertex_count >= model->vertex_cap) return C3DAE_ERR_CAPACITY;
    memset(&v, 0, sizeof(v));
    v.source_vertex = 0UL;
    i = 0U;
    while (i < input_count) {
        if (inputs[i].source_index >= 0) {
            if (strcmp(inputs[i].semantic, "VERTEX") == 0 || strcmp(inputs[i].semantic, "POSITION") == 0) {
                v.source_vertex = offset_values[inputs[i].offset];
                rc = c3dae_sample_source(ctx, inputs[i].source_index, offset_values[inputs[i].offset], &a, &b, &c);
                if (rc != C3DAE_OK) return rc;
                v.x = a; v.y = b; v.z = c; v.flags |= C3DAE_VERTEX_HAS_POS;
            } else if (strcmp(inputs[i].semantic, "NORMAL") == 0) {
                rc = c3dae_sample_source(ctx, inputs[i].source_index, offset_values[inputs[i].offset], &a, &b, &c);
                if (rc != C3DAE_OK) return rc;
                v.nx = a; v.ny = b; v.nz = c; v.flags |= C3DAE_VERTEX_HAS_NORMAL;
            } else if ((strcmp(inputs[i].semantic, "TEXCOORD") == 0 || strcmp(inputs[i].semantic, "UV") == 0) && inputs[i].set_index == 0U) {
                rc = c3dae_sample_source(ctx, inputs[i].source_index, offset_values[inputs[i].offset], &a, &b, &c);
                if (rc != C3DAE_OK) return rc;
                v.u = a; v.v = b; v.flags |= C3DAE_VERTEX_HAS_UV;
            }
        }
        i++;
    }
    *out_index = model->vertex_count;
    model->vertices[model->vertex_count] = v;
    model->vertex_count++;
    return C3DAE_OK;
}

static int c3dae_emit_tri(c3dae_model *model, c3dae_u32 a, c3dae_u32 b, c3dae_u32 c, c3dae_u32 mesh_index, const char *material_symbol)
{
    c3dae_triangle t;
    if (model->triangle_count >= model->triangle_cap) return C3DAE_ERR_CAPACITY;
    memset(&t, 0, sizeof(t));
    t.a = a; t.b = b; t.c = c; t.mesh_index = mesh_index;
    t.material_index = -1;
    if (material_symbol != (const char *)0) {
        c3dae_copy_text(t.material_symbol, C3DAE_ID_MAX, material_symbol, material_symbol + c3dae_strlen_lit(material_symbol));
    }
    model->triangles[model->triangle_count] = t;
    model->triangle_count++;
    return C3DAE_OK;
}

static int c3dae_read_vertex_tuple(const char **pp, const char *end, unsigned int max_offset, c3dae_u32 *offset_values)
{
    unsigned int i;
    i = 0U;
    while (i <= max_offset) {
        if (!c3dae_parse_u32_token(pp, end, &offset_values[i])) return C3DAE_ERR_XML;
        i++;
    }
    return C3DAE_OK;
}

static int c3dae_parse_triangles(c3dae_context *ctx, c3dae_model *model, const char *mesh_a, const char *mesh_b, c3dae_u32 mesh_index)
{
    const char *p;
    const char *tag;
    const char *tag_end;
    const char *prim_end;
    const char *after_prim;
    const char *p_tag;
    const char *p_tag_end;
    const char *p_end;
    const char *after_p;
    c3dae_input inputs[C3DAE_MAX_INPUTS];
    unsigned int input_count;
    unsigned int max_offset;
    c3dae_u32 offvals[16];
    c3dae_u32 vi[3];
    char material_symbol[C3DAE_ID_MAX];
    int rc;
    p = mesh_a;
    while (1) {
        tag = c3dae_find_start_tag(p, mesh_b, "triangles", &tag_end);
        if (tag == C3DAE_XML_NIL) break;
        prim_end = c3dae_find_end_tag(tag_end, mesh_b, "triangles", &after_prim);
        if (prim_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        material_symbol[0] = '\0';
        c3dae_attr_get(tag, tag_end, "material", material_symbol, sizeof(material_symbol));
        rc = c3dae_read_primitive_inputs(ctx, tag_end, prim_end, inputs, &input_count, &max_offset);
        if (rc != C3DAE_OK) return rc;
        p_tag = c3dae_find_start_tag(tag_end, prim_end, "p", &p_tag_end);
        if (p_tag != C3DAE_XML_NIL) {
            p_end = c3dae_find_end_tag(p_tag_end, prim_end, "p", &after_p);
            if (p_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
            p = p_tag_end;
            while (p < p_end) {
                while (p < p_end && c3dae_is_space(*p)) p++;
                if (p >= p_end) break;
                rc = c3dae_read_vertex_tuple(&p, p_end, max_offset, offvals); if (rc != C3DAE_OK) return rc;
                rc = c3dae_emit_vertex(ctx, model, inputs, input_count, offvals, &vi[0]); if (rc != C3DAE_OK) return rc;
                rc = c3dae_read_vertex_tuple(&p, p_end, max_offset, offvals); if (rc != C3DAE_OK) return rc;
                rc = c3dae_emit_vertex(ctx, model, inputs, input_count, offvals, &vi[1]); if (rc != C3DAE_OK) return rc;
                rc = c3dae_read_vertex_tuple(&p, p_end, max_offset, offvals); if (rc != C3DAE_OK) return rc;
                rc = c3dae_emit_vertex(ctx, model, inputs, input_count, offvals, &vi[2]); if (rc != C3DAE_OK) return rc;
                rc = c3dae_emit_tri(model, vi[0], vi[1], vi[2], mesh_index, material_symbol); if (rc != C3DAE_OK) return rc;
            }
        }
        p = after_prim;
    }
    return C3DAE_OK;
}

static int c3dae_parse_polylist(c3dae_context *ctx, c3dae_model *model, const char *mesh_a, const char *mesh_b, c3dae_u32 mesh_index)
{
    const char *p;
    const char *tag;
    const char *tag_end;
    const char *prim_end;
    const char *after_prim;
    const char *vc_tag;
    const char *vc_tag_end;
    const char *vc_end;
    const char *after_vc;
    const char *p_tag;
    const char *p_tag_end;
    const char *p_end;
    const char *after_p;
    const char *vcursor;
    const char *icursor;
    c3dae_input inputs[C3DAE_MAX_INPUTS];
    unsigned int input_count;
    unsigned int max_offset;
    c3dae_u32 offvals[16];
    c3dae_u32 poly_indices[C3DAE_MAX_POLY_VERTS];
    c3dae_u32 n;
    c3dae_u32 i;
    char material_symbol[C3DAE_ID_MAX];
    int rc;
    p = mesh_a;
    while (1) {
        tag = c3dae_find_start_tag(p, mesh_b, "polylist", &tag_end);
        if (tag == C3DAE_XML_NIL) break;
        prim_end = c3dae_find_end_tag(tag_end, mesh_b, "polylist", &after_prim);
        if (prim_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        material_symbol[0] = '\0';
        c3dae_attr_get(tag, tag_end, "material", material_symbol, sizeof(material_symbol));
        rc = c3dae_read_primitive_inputs(ctx, tag_end, prim_end, inputs, &input_count, &max_offset);
        if (rc != C3DAE_OK) return rc;
        vc_tag = c3dae_find_start_tag(tag_end, prim_end, "vcount", &vc_tag_end);
        p_tag = c3dae_find_start_tag(tag_end, prim_end, "p", &p_tag_end);
        if (vc_tag == C3DAE_XML_NIL || p_tag == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        vc_end = c3dae_find_end_tag(vc_tag_end, prim_end, "vcount", &after_vc);
        p_end = c3dae_find_end_tag(p_tag_end, prim_end, "p", &after_p);
        if (vc_end == C3DAE_XML_NIL || p_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        vcursor = vc_tag_end;
        icursor = p_tag_end;
        while (vcursor < vc_end) {
            while (vcursor < vc_end && c3dae_is_space(*vcursor)) vcursor++;
            if (vcursor >= vc_end) break;
            if (!c3dae_parse_u32_token(&vcursor, vc_end, &n)) return C3DAE_ERR_XML;
            if (n > (c3dae_u32)C3DAE_MAX_POLY_VERTS) return C3DAE_ERR_UNSUPPORTED;
            i = 0UL;
            while (i < n) {
                rc = c3dae_read_vertex_tuple(&icursor, p_end, max_offset, offvals); if (rc != C3DAE_OK) return rc;
                rc = c3dae_emit_vertex(ctx, model, inputs, input_count, offvals, &poly_indices[i]); if (rc != C3DAE_OK) return rc;
                i++;
            }
            if (n >= 3UL) {
                i = 1UL;
                while (i + 1UL < n) {
                    rc = c3dae_emit_tri(model, poly_indices[0], poly_indices[i], poly_indices[i + 1UL], mesh_index, material_symbol); if (rc != C3DAE_OK) return rc;
                    i++;
                }
            }
        }
        p = after_prim;
    }
    return C3DAE_OK;
}


static int c3dae_parse_tristrips(c3dae_context *ctx, c3dae_model *model, const char *mesh_a, const char *mesh_b, c3dae_u32 mesh_index)
{
    const char *p;
    const char *tag;
    const char *tag_end;
    const char *prim_end;
    const char *after_prim;
    const char *p_tag;
    const char *p_tag_end;
    const char *p_end;
    const char *after_p;
    c3dae_input inputs[C3DAE_MAX_INPUTS];
    unsigned int input_count;
    unsigned int max_offset;
    c3dae_u32 offvals[16];
    c3dae_u32 last0;
    c3dae_u32 last1;
    c3dae_u32 cur;
    c3dae_u32 idx;
    char material_symbol[C3DAE_ID_MAX];
    int rc;
    p = mesh_a;
    while (1) {
        tag = c3dae_find_start_tag(p, mesh_b, "tristrips", &tag_end);
        if (tag == C3DAE_XML_NIL) break;
        prim_end = c3dae_find_end_tag(tag_end, mesh_b, "tristrips", &after_prim);
        if (prim_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        material_symbol[0] = '\0';
        c3dae_attr_get(tag, tag_end, "material", material_symbol, sizeof(material_symbol));
        rc = c3dae_read_primitive_inputs(ctx, tag_end, prim_end, inputs, &input_count, &max_offset);
        if (rc != C3DAE_OK) return rc;
        p_tag = tag_end;
        while (1) {
            p_tag = c3dae_find_start_tag(p_tag, prim_end, "p", &p_tag_end);
            if (p_tag == C3DAE_XML_NIL) break;
            p_end = c3dae_find_end_tag(p_tag_end, prim_end, "p", &after_p);
            if (p_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
            p = p_tag_end;
            idx = 0UL;
            last0 = 0UL; last1 = 0UL;
            while (p < p_end) {
                while (p < p_end && c3dae_is_space(*p)) p++;
                if (p >= p_end) break;
                rc = c3dae_read_vertex_tuple(&p, p_end, max_offset, offvals); if (rc != C3DAE_OK) return rc;
                rc = c3dae_emit_vertex(ctx, model, inputs, input_count, offvals, &cur); if (rc != C3DAE_OK) return rc;
                if (idx >= 2UL) {
                    if ((idx & 1UL) == 0UL) rc = c3dae_emit_tri(model, last0, last1, cur, mesh_index, material_symbol);
                    else rc = c3dae_emit_tri(model, last1, last0, cur, mesh_index, material_symbol);
                    if (rc != C3DAE_OK) return rc;
                }
                last0 = last1;
                last1 = cur;
                idx++;
            }
            p_tag = after_p;
        }
        p = after_prim;
    }
    return C3DAE_OK;
}

static int c3dae_parse_trifans(c3dae_context *ctx, c3dae_model *model, const char *mesh_a, const char *mesh_b, c3dae_u32 mesh_index)
{
    const char *p;
    const char *tag;
    const char *tag_end;
    const char *prim_end;
    const char *after_prim;
    const char *p_tag;
    const char *p_tag_end;
    const char *p_end;
    const char *after_p;
    c3dae_input inputs[C3DAE_MAX_INPUTS];
    unsigned int input_count;
    unsigned int max_offset;
    c3dae_u32 offvals[16];
    c3dae_u32 first;
    c3dae_u32 prev;
    c3dae_u32 cur;
    c3dae_u32 idx;
    char material_symbol[C3DAE_ID_MAX];
    int rc;
    p = mesh_a;
    while (1) {
        tag = c3dae_find_start_tag(p, mesh_b, "trifans", &tag_end);
        if (tag == C3DAE_XML_NIL) break;
        prim_end = c3dae_find_end_tag(tag_end, mesh_b, "trifans", &after_prim);
        if (prim_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        material_symbol[0] = '\0';
        c3dae_attr_get(tag, tag_end, "material", material_symbol, sizeof(material_symbol));
        rc = c3dae_read_primitive_inputs(ctx, tag_end, prim_end, inputs, &input_count, &max_offset);
        if (rc != C3DAE_OK) return rc;
        p_tag = tag_end;
        while (1) {
            p_tag = c3dae_find_start_tag(p_tag, prim_end, "p", &p_tag_end);
            if (p_tag == C3DAE_XML_NIL) break;
            p_end = c3dae_find_end_tag(p_tag_end, prim_end, "p", &after_p);
            if (p_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
            p = p_tag_end;
            idx = 0UL;
            first = 0UL; prev = 0UL;
            while (p < p_end) {
                while (p < p_end && c3dae_is_space(*p)) p++;
                if (p >= p_end) break;
                rc = c3dae_read_vertex_tuple(&p, p_end, max_offset, offvals); if (rc != C3DAE_OK) return rc;
                rc = c3dae_emit_vertex(ctx, model, inputs, input_count, offvals, &cur); if (rc != C3DAE_OK) return rc;
                if (idx == 0UL) first = cur;
                else if (idx == 1UL) prev = cur;
                else { rc = c3dae_emit_tri(model, first, prev, cur, mesh_index, material_symbol); if (rc != C3DAE_OK) return rc; prev = cur; }
                idx++;
            }
            p_tag = after_p;
        }
        p = after_prim;
    }
    return C3DAE_OK;
}

static int c3dae_parse_polygons(c3dae_context *ctx, c3dae_model *model, const char *mesh_a, const char *mesh_b, c3dae_u32 mesh_index)
{
    const char *p;
    const char *tag;
    const char *tag_end;
    const char *prim_end;
    const char *after_prim;
    const char *p_tag;
    const char *p_tag_end;
    const char *p_end;
    const char *after_p;
    c3dae_input inputs[C3DAE_MAX_INPUTS];
    unsigned int input_count;
    unsigned int max_offset;
    c3dae_u32 offvals[16];
    c3dae_u32 poly_indices[C3DAE_MAX_POLY_VERTS];
    c3dae_u32 n;
    c3dae_u32 i;
    char material_symbol[C3DAE_ID_MAX];
    int rc;
    p = mesh_a;
    while (1) {
        tag = c3dae_find_start_tag(p, mesh_b, "polygons", &tag_end);
        if (tag == C3DAE_XML_NIL) break;
        prim_end = c3dae_find_end_tag(tag_end, mesh_b, "polygons", &after_prim);
        if (prim_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        material_symbol[0] = '\0';
        c3dae_attr_get(tag, tag_end, "material", material_symbol, sizeof(material_symbol));
        rc = c3dae_read_primitive_inputs(ctx, tag_end, prim_end, inputs, &input_count, &max_offset);
        if (rc != C3DAE_OK) return rc;
        p_tag = tag_end;
        while (1) {
            p_tag = c3dae_find_start_tag(p_tag, prim_end, "p", &p_tag_end);
            if (p_tag == C3DAE_XML_NIL) break;
            p_end = c3dae_find_end_tag(p_tag_end, prim_end, "p", &after_p);
            if (p_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
            p = p_tag_end;
            n = 0UL;
            while (p < p_end) {
                while (p < p_end && c3dae_is_space(*p)) p++;
                if (p >= p_end) break;
                if (n >= (c3dae_u32)C3DAE_MAX_POLY_VERTS) return C3DAE_ERR_UNSUPPORTED;
                rc = c3dae_read_vertex_tuple(&p, p_end, max_offset, offvals); if (rc != C3DAE_OK) return rc;
                rc = c3dae_emit_vertex(ctx, model, inputs, input_count, offvals, &poly_indices[n]); if (rc != C3DAE_OK) return rc;
                n++;
            }
            if (n >= 3UL) {
                i = 1UL;
                while (i + 1UL < n) {
                    rc = c3dae_emit_tri(model, poly_indices[0], poly_indices[i], poly_indices[i + 1UL], mesh_index, material_symbol);
                    if (rc != C3DAE_OK) return rc;
                    i++;
                }
            }
            p_tag = after_p;
        }
        p = after_prim;
    }
    return C3DAE_OK;
}

static int c3dae_parse_matrix_text(const char *a, const char *b, c3dae_fixed *m)
{
    unsigned int i;
    c3dae_fixed v;
    const char *p;
    p = a;
    i = 0U;
    while (i < 16U) {
        if (!c3dae_parse_fixed_token(&p, b, &v)) return 0;
        m[i] = v;
        i++;
    }
    return 1;
}

static int c3dae_parse_geometry(c3dae_context *ctx, c3dae_model *model, const char *geom_tag, const char *geom_tag_end, const char *geom_end)
{
    const char *mesh;
    const char *mesh_tag_end;
    const char *mesh_end;
    const char *after_mesh;
    c3dae_mesh *m;
    c3dae_u32 mesh_index;
    int rc;
    mesh = c3dae_find_start_tag(geom_tag_end, geom_end, "mesh", &mesh_tag_end);
    if (mesh == C3DAE_XML_NIL) return C3DAE_OK;
    mesh_end = c3dae_find_end_tag(mesh_tag_end, geom_end, "mesh", &after_mesh);
    if (mesh_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
    if (model->mesh_count >= model->mesh_cap) return C3DAE_ERR_CAPACITY;
    c3dae_reset_temp_sources(ctx);
    mesh_index = model->mesh_count;
    m = &model->meshes[model->mesh_count];
    memset(m, 0, sizeof(*m));
    c3dae_attr_get(geom_tag, geom_tag_end, "id", m->id, C3DAE_ID_MAX);
    c3dae_attr_get(geom_tag, geom_tag_end, "name", m->name, C3DAE_ID_MAX);
    if (m->id[0] == '\0') c3dae_copy_text(m->id, C3DAE_ID_MAX, m->name, m->name + c3dae_strlen_lit(m->name));
    m->first_vertex = model->vertex_count;
    m->first_triangle = model->triangle_count;
    model->mesh_count++;
    rc = c3dae_parse_sources(ctx, mesh_tag_end, mesh_end); if (rc != C3DAE_OK) return rc;
    rc = c3dae_parse_vertex_bindings(ctx, mesh_tag_end, mesh_end); if (rc != C3DAE_OK) return rc;
    rc = c3dae_parse_triangles(ctx, model, mesh_tag_end, mesh_end, mesh_index); if (rc != C3DAE_OK) return rc;
    rc = c3dae_parse_polylist(ctx, model, mesh_tag_end, mesh_end, mesh_index); if (rc != C3DAE_OK) return rc;
    rc = c3dae_parse_tristrips(ctx, model, mesh_tag_end, mesh_end, mesh_index); if (rc != C3DAE_OK) return rc;
    rc = c3dae_parse_trifans(ctx, model, mesh_tag_end, mesh_end, mesh_index); if (rc != C3DAE_OK) return rc;
    rc = c3dae_parse_polygons(ctx, model, mesh_tag_end, mesh_end, mesh_index); if (rc != C3DAE_OK) return rc;
    m->vertex_count = model->vertex_count - m->first_vertex;
    m->triangle_count = model->triangle_count - m->first_triangle;
    return C3DAE_OK;
}

static void c3dae_parse_asset(c3dae_model *model, const char *xml, const char *end)
{
    const char *asset;
    const char *asset_tag_end;
    const char *asset_end;
    const char *after_asset;
    const char *unit;
    const char *unit_end;
    const char *up;
    const char *up_tag_end;
    const char *up_end;
    const char *after_up;
    char tmp[32];
    c3dae_fixed meter;
    model->unit_meter = C3DAE_FIXED_ONE;
    model->up_axis = C3DAE_UP_UNKNOWN;
    asset = c3dae_find_start_tag(xml, end, "asset", &asset_tag_end);
    if (asset == C3DAE_XML_NIL) return;
    asset_end = c3dae_find_end_tag(asset_tag_end, end, "asset", &after_asset);
    if (asset_end == C3DAE_XML_NIL) return;
    unit = c3dae_find_start_tag(asset_tag_end, asset_end, "unit", &unit_end);
    if (unit != C3DAE_XML_NIL) {
        meter = C3DAE_FIXED_ONE;
        if (c3dae_attr_get_fixed(unit, unit_end, "meter", &meter)) model->unit_meter = meter;
    }
    up = c3dae_find_start_tag(asset_tag_end, asset_end, "up_axis", &up_tag_end);
    if (up != C3DAE_XML_NIL) {
        up_end = c3dae_find_end_tag(up_tag_end, asset_end, "up_axis", &after_up);
        if (up_end != C3DAE_XML_NIL) {
            tmp[0] = '\0';
            c3dae_copy_text(tmp, sizeof(tmp), up_tag_end, up_end);
            if (strstr(tmp, "X_UP") != (char *)0) model->up_axis = C3DAE_UP_X;
            else if (strstr(tmp, "Y_UP") != (char *)0) model->up_axis = C3DAE_UP_Y;
            else if (strstr(tmp, "Z_UP") != (char *)0) model->up_axis = C3DAE_UP_Z;
        }
    }
}

static int c3dae_parse_skin_joints_block(c3dae_context *ctx,
                                         c3dae_model *model,
                                         c3dae_controller *ctl,
                                         const char *skin_a,
                                         const char *skin_b)
{
    const char *jtag;
    const char *jtag_end;
    const char *jend;
    const char *after_j;
    const char *inp;
    const char *inp_end;
    char sem[32];
    char src[96];
    int joints_src;
    int inv_src;
    c3dae_u32 i;
    c3dae_skin_joint *sj;
    int rc;
    joints_src = -1;
    inv_src = -1;
    jtag = c3dae_find_start_tag(skin_a, skin_b, "joints", &jtag_end);
    if (jtag == C3DAE_XML_NIL) return C3DAE_OK;
    jend = c3dae_find_end_tag(jtag_end, skin_b, "joints", &after_j);
    if (jend == C3DAE_XML_NIL) return C3DAE_ERR_XML;
    inp = jtag_end;
    while (1) {
        inp = c3dae_find_start_tag(inp, jend, "input", &inp_end);
        if (inp == C3DAE_XML_NIL) break;
        sem[0] = '\0'; src[0] = '\0';
        c3dae_attr_get(inp, inp_end, "semantic", sem, sizeof(sem));
        c3dae_attr_get(inp, inp_end, "source", src, sizeof(src));
        if (strcmp(sem, "JOINT") == 0) joints_src = c3dae_find_source(ctx, src);
        else if (strcmp(sem, "INV_BIND_MATRIX") == 0) inv_src = c3dae_find_source(ctx, src);
        inp = inp_end;
    }
    if (joints_src < 0) return C3DAE_OK;
    ctl->first_joint = model->skin_joint_count;
    i = 0UL;
    while (i < ctx->sources[joints_src].name_count) {
        if (model->skin_joint_count >= model->skin_joint_cap) return C3DAE_ERR_CAPACITY;
        sj = &model->skin_joints[model->skin_joint_count];
        memset(sj, 0, sizeof(*sj));
        rc = c3dae_source_name_at(ctx, joints_src, i, sj->sid, C3DAE_ID_MAX);
        if (rc != C3DAE_OK) return rc;
        c3dae_matrix_identity(sj->inv_bind_matrix);
        if (inv_src >= 0 && ctx->sources[inv_src].stride >= 16UL && i < ctx->sources[inv_src].item_count) {
            rc = c3dae_sample_matrix_source(ctx, inv_src, i, sj->inv_bind_matrix);
            if (rc != C3DAE_OK) return rc;
        }
        sj->node_index = -1;
        model->skin_joint_count++;
        i++;
    }
    ctl->joint_count = model->skin_joint_count - ctl->first_joint;
    return C3DAE_OK;
}

static int c3dae_read_weight_inputs(c3dae_context *ctx,
                                    const char *vw_a,
                                    const char *vw_b,
                                    c3dae_input *inputs,
                                    unsigned int *input_count,
                                    unsigned int *max_offset,
                                    int *joint_source,
                                    int *weight_source)
{
    const char *p;
    const char *inp;
    const char *inp_end;
    char sem[32];
    char src[96];
    c3dae_u32 off;
    unsigned int n;
    n = 0U;
    *max_offset = 0U;
    *joint_source = -1;
    *weight_source = -1;
    p = vw_a;
    while (1) {
        inp = c3dae_find_start_tag(p, vw_b, "input", &inp_end);
        if (inp == C3DAE_XML_NIL) break;
        if (n >= C3DAE_MAX_INPUTS) return C3DAE_ERR_CAPACITY;
        sem[0] = '\0'; src[0] = '\0'; off = 0UL;
        c3dae_attr_get(inp, inp_end, "semantic", sem, sizeof(sem));
        c3dae_attr_get(inp, inp_end, "source", src, sizeof(src));
        c3dae_attr_get_u32(inp, inp_end, "offset", &off);
        c3dae_copy_text(inputs[n].semantic, sizeof(inputs[n].semantic), sem, sem + c3dae_strlen_lit(sem));
        inputs[n].offset = (unsigned int)off;
        inputs[n].source_index = c3dae_find_source(ctx, src);
        inputs[n].set_index = 0U;
        if (strcmp(sem, "JOINT") == 0) *joint_source = inputs[n].source_index;
        if (strcmp(sem, "WEIGHT") == 0) *weight_source = inputs[n].source_index;
        if (inputs[n].offset > *max_offset) *max_offset = inputs[n].offset;
        n++;
        p = inp_end;
    }
    *input_count = n;
    return C3DAE_OK;
}

static int c3dae_parse_vertex_weights_block(c3dae_context *ctx,
                                            c3dae_model *model,
                                            c3dae_controller *ctl,
                                            const char *skin_a,
                                            const char *skin_b)
{
    const char *vw;
    const char *vw_tag_end;
    const char *vw_end;
    const char *after_vw;
    const char *vc_tag;
    const char *vc_tag_end;
    const char *vc_end;
    const char *after_vc;
    const char *v_tag;
    const char *v_tag_end;
    const char *v_end;
    const char *after_v;
    c3dae_input inputs[C3DAE_MAX_INPUTS];
    unsigned int input_count;
    unsigned int max_offset;
    int joint_source;
    int weight_source;
    const char *vcursor;
    const char *icursor;
    c3dae_u32 vertex_count;
    c3dae_u32 vi;
    c3dae_u32 ninf;
    c3dae_u32 k;
    c3dae_s32 idx;
    c3dae_s32 joint_idx;
    c3dae_s32 weight_idx;
    c3dae_skin_vertex *sv;
    c3dae_fixed a;
    c3dae_fixed b;
    c3dae_fixed c;
    unsigned int out_i;
    int rc;
    (void)joint_source;
    vw = c3dae_find_start_tag(skin_a, skin_b, "vertex_weights", &vw_tag_end);
    if (vw == C3DAE_XML_NIL) return C3DAE_OK;
    vw_end = c3dae_find_end_tag(vw_tag_end, skin_b, "vertex_weights", &after_vw);
    if (vw_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
    vertex_count = 0UL;
    c3dae_attr_get_u32(vw, vw_tag_end, "count", &vertex_count);
    rc = c3dae_read_weight_inputs(ctx, vw_tag_end, vw_end, inputs, &input_count, &max_offset, &joint_source, &weight_source);
    if (rc != C3DAE_OK) return rc;
    if (weight_source < 0) return C3DAE_OK;
    vc_tag = c3dae_find_start_tag(vw_tag_end, vw_end, "vcount", &vc_tag_end);
    v_tag = c3dae_find_start_tag(vw_tag_end, vw_end, "v", &v_tag_end);
    if (vc_tag == C3DAE_XML_NIL || v_tag == C3DAE_XML_NIL) return C3DAE_ERR_XML;
    vc_end = c3dae_find_end_tag(vc_tag_end, vw_end, "vcount", &after_vc);
    v_end = c3dae_find_end_tag(v_tag_end, vw_end, "v", &after_v);
    if (vc_end == C3DAE_XML_NIL || v_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
    ctl->first_vertex_weight = model->skin_vertex_count;
    ctl->vertex_weight_count = vertex_count;
    if (model->skin_vertex_count + vertex_count > model->skin_vertex_cap) return C3DAE_ERR_CAPACITY;
    vcursor = vc_tag_end;
    icursor = v_tag_end;
    vi = 0UL;
    while (vi < vertex_count) {
        if (!c3dae_parse_u32_token(&vcursor, vc_end, &ninf)) return C3DAE_ERR_XML;
        sv = &model->skin_vertices[model->skin_vertex_count + vi];
        memset(sv, 0, sizeof(*sv));
        out_i = 0U;
        k = 0UL;
        while (k < ninf) {
            joint_idx = -1L;
            weight_idx = -1L;
            idx = 0L;
            while ((unsigned int)idx <= max_offset) {
                if (!c3dae_parse_s32_token(&icursor, v_end, &idx)) return C3DAE_ERR_XML;
                if ((unsigned int)(idx < 0L ? 0U : 0U) == 1U) { }
                if ((unsigned int)inputs[0].offset == (unsigned int)idx) { }
                /* handled below with slot scan */
                break;
            }
            /* Re-read this influence as an offset tuple using a small local pass. */
            {
                c3dae_s32 tuple[16];
                unsigned int t;
                tuple[0] = idx;
                t = 1U;
                while (t <= max_offset) {
                    if (!c3dae_parse_s32_token(&icursor, v_end, &tuple[t])) return C3DAE_ERR_XML;
                    t++;
                }
                t = 0U;
                while (t < input_count) {
                    if (strcmp(inputs[t].semantic, "JOINT") == 0) joint_idx = tuple[inputs[t].offset];
                    else if (strcmp(inputs[t].semantic, "WEIGHT") == 0) weight_idx = tuple[inputs[t].offset];
                    t++;
                }
            }
            if (joint_idx >= 0L && weight_idx >= 0L && out_i < C3DAE_MAX_SKIN_INFLUENCES) {
                rc = c3dae_sample_source(ctx, weight_source, (c3dae_u32)weight_idx, &a, &b, &c);
                if (rc != C3DAE_OK) return rc;
                sv->joint[out_i] = (c3dae_u16)joint_idx;
                sv->weight[out_i] = a;
                out_i++;
            }
            k++;
        }
        sv->count = out_i;
        vi++;
    }
    model->skin_vertex_count += vertex_count;
    return C3DAE_OK;
}

static int c3dae_parse_morph_block(c3dae_context *ctx,
                                     c3dae_model *model,
                                     c3dae_controller *ctl,
                                     const char *morph_a,
                                     const char *morph_b)
{
    const char *targets;
    const char *targets_tag_end;
    const char *targets_end;
    const char *after_targets;
    const char *inp;
    const char *inp_end;
    char sem[32];
    char src[96];
    int target_source;
    int weight_source;
    c3dae_source *ts;
    c3dae_source *ws;
    c3dae_u32 count;
    c3dae_u32 i;
    c3dae_morph_target *mt;
    target_source = -1;
    weight_source = -1;
    targets = c3dae_find_start_tag(morph_a, morph_b, "targets", &targets_tag_end);
    if (targets == C3DAE_XML_NIL) return C3DAE_OK;
    targets_end = c3dae_find_end_tag(targets_tag_end, morph_b, "targets", &after_targets);
    if (targets_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
    inp = targets_tag_end;
    while (1) {
        inp = c3dae_find_start_tag(inp, targets_end, "input", &inp_end);
        if (inp == C3DAE_XML_NIL) break;
        sem[0] = '\0'; src[0] = '\0';
        c3dae_attr_get(inp, inp_end, "semantic", sem, sizeof(sem));
        c3dae_attr_get(inp, inp_end, "source", src, sizeof(src));
        if (strcmp(sem, "MORPH_TARGET") == 0) target_source = c3dae_find_source(ctx, src);
        else if (strcmp(sem, "MORPH_WEIGHT") == 0) weight_source = c3dae_find_source(ctx, src);
        inp = inp_end;
    }
    if (target_source < 0 || weight_source < 0) return C3DAE_OK;
    ts = &ctx->sources[target_source];
    ws = &ctx->sources[weight_source];
    count = ts->name_count;
    if (ws->item_count < count) count = ws->item_count;
    ctl->first_morph_target = model->morph_target_count;
    ctl->morph_target_count = count;
    if (model->morph_target_cap == 0UL) return C3DAE_OK;
    if (model->morph_target_count + count > model->morph_target_cap) return C3DAE_ERR_CAPACITY;
    i = 0UL;
    while (i < count) {
        mt = &model->morph_targets[model->morph_target_count];
        memset(mt, 0, sizeof(*mt));
        c3dae_source_name_at(ctx, target_source, i, mt->target, C3DAE_ID_MAX);
        mt->mesh_index = c3dae_find_mesh_by_id(model, mt->target);
        mt->weight = c3dae_source_value_at(ctx, weight_source, i, 0UL);
        model->morph_target_count++;
        i++;
    }
    return C3DAE_OK;
}

static int c3dae_parse_controllers(c3dae_context *ctx, c3dae_model *model, const char *xml, const char *end)
{
    const char *p;
    const char *tag;
    const char *tag_end;
    const char *ctl_end;
    const char *after_ctl;
    const char *skin;
    const char *skin_tag_end;
    const char *skin_end;
    const char *after_skin;
    const char *morph;
    const char *morph_tag_end;
    const char *morph_end;
    const char *after_morph;
    const char *bsm;
    const char *bsm_tag_end;
    const char *bsm_end;
    const char *after_bsm;
    c3dae_controller *ctl;
    int rc;
    char method[32];
    p = xml;
    while (1) {
        tag = c3dae_find_start_tag(p, end, "controller", &tag_end);
        if (tag == C3DAE_XML_NIL) break;
        ctl_end = c3dae_find_end_tag(tag_end, end, "controller", &after_ctl);
        if (ctl_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        skin = c3dae_find_start_tag(tag_end, ctl_end, "skin", &skin_tag_end);
        morph = c3dae_find_start_tag(tag_end, ctl_end, "morph", &morph_tag_end);
        if (skin != C3DAE_XML_NIL || morph != C3DAE_XML_NIL) {
            if (model->controller_count >= model->controller_cap) return C3DAE_ERR_CAPACITY;
            ctl = &model->controllers[model->controller_count];
            memset(ctl, 0, sizeof(*ctl));
            ctl->kind = C3DAE_CONTROLLER_UNKNOWN;
            ctl->source_mesh_index = -1;
            ctl->morph_source_mesh_index = -1;
            c3dae_matrix_identity(ctl->bind_shape_matrix);
            c3dae_attr_get(tag, tag_end, "id", ctl->id, C3DAE_ID_MAX);
            c3dae_attr_get(tag, tag_end, "name", ctl->name, C3DAE_ID_MAX);
            if (skin != C3DAE_XML_NIL) {
                skin_end = c3dae_find_end_tag(skin_tag_end, ctl_end, "skin", &after_skin);
                if (skin_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
                ctl->kind = C3DAE_CONTROLLER_SKIN;
                c3dae_attr_get(skin, skin_tag_end, "source", ctl->skin_source, C3DAE_ID_MAX);
                ctl->source_mesh_index = c3dae_find_mesh_by_id(model, ctl->skin_source);
                c3dae_reset_temp_sources(ctx);
                rc = c3dae_parse_sources(ctx, skin_tag_end, skin_end); if (rc != C3DAE_OK) return rc;
                bsm = c3dae_find_start_tag(skin_tag_end, skin_end, "bind_shape_matrix", &bsm_tag_end);
                if (bsm != C3DAE_XML_NIL) {
                    bsm_end = c3dae_find_end_tag(bsm_tag_end, skin_end, "bind_shape_matrix", &after_bsm);
                    if (bsm_end != C3DAE_XML_NIL) c3dae_parse_matrix_text(bsm_tag_end, bsm_end, ctl->bind_shape_matrix);
                }
                model->controller_count++;
                rc = c3dae_parse_skin_joints_block(ctx, model, ctl, skin_tag_end, skin_end); if (rc != C3DAE_OK) return rc;
                rc = c3dae_parse_vertex_weights_block(ctx, model, ctl, skin_tag_end, skin_end); if (rc != C3DAE_OK) return rc;
            } else {
                morph_end = c3dae_find_end_tag(morph_tag_end, ctl_end, "morph", &after_morph);
                if (morph_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
                ctl->kind = C3DAE_CONTROLLER_MORPH;
                ctl->morph_method = C3DAE_MORPH_NORMALIZED;
                method[0] = '\0';
                c3dae_attr_get(morph, morph_tag_end, "method", method, sizeof(method));
                if (strcmp(method, "RELATIVE") == 0) ctl->morph_method = C3DAE_MORPH_RELATIVE;
                c3dae_attr_get(morph, morph_tag_end, "source", ctl->morph_source, C3DAE_ID_MAX);
                ctl->morph_source_mesh_index = c3dae_find_mesh_by_id(model, ctl->morph_source);
                c3dae_reset_temp_sources(ctx);
                rc = c3dae_parse_sources(ctx, morph_tag_end, morph_end); if (rc != C3DAE_OK) return rc;
                model->controller_count++;
                rc = c3dae_parse_morph_block(ctx, model, ctl, morph_tag_end, morph_end); if (rc != C3DAE_OK) return rc;
            }
        }
        p = after_ctl;
    }
    return C3DAE_OK;
}

static void c3dae_apply_skins_to_emitted_vertices(c3dae_model *model)
{
    c3dae_u32 ci;
    c3dae_u32 vi;
    c3dae_u32 k;
    c3dae_controller *ctl;
    c3dae_mesh *mesh;
    c3dae_vertex *v;
    c3dae_skin_vertex *sv;
    ci = 0UL;
    while (ci < model->controller_count) {
        ctl = &model->controllers[ci];
        if (ctl->kind == C3DAE_CONTROLLER_SKIN && ctl->source_mesh_index >= 0 && (c3dae_u32)ctl->source_mesh_index < model->mesh_count) {
            mesh = &model->meshes[ctl->source_mesh_index];
            vi = mesh->first_vertex;
            while (vi < mesh->first_vertex + mesh->vertex_count) {
                v = &model->vertices[vi];
                if (v->source_vertex < ctl->vertex_weight_count) {
                    sv = &model->skin_vertices[ctl->first_vertex_weight + v->source_vertex];
                    v->skin_count = sv->count;
                    k = 0UL;
                    while (k < (c3dae_u32)sv->count && k < (c3dae_u32)C3DAE_MAX_SKIN_INFLUENCES) {
                        v->joint[k] = (c3dae_u16)(ctl->first_joint + (c3dae_u32)sv->joint[k]);
                        v->weight[k] = sv->weight[k];
                        k++;
                    }
                    if (v->skin_count > 0U) v->flags |= C3DAE_VERTEX_HAS_SKIN;
                }
                vi++;
            }
        }
        ci++;
    }
}

static void c3dae_parse_anim_target(c3dae_anim_channel *ch, const char *target)
{
    const char *slash;
    const char *dot;
    const char *end;
    end = target + c3dae_strlen_lit(target);
    slash = target;
    while (slash < end && *slash != '/') slash++;
    c3dae_copy_text(ch->target_node, C3DAE_ID_MAX, target, slash);
    ch->target_sid[0] = '\0';
    ch->target_kind = C3DAE_ANIM_TARGET_UNKNOWN;
    if (slash < end) {
        dot = slash + 1;
        while (dot < end && *dot != '.') dot++;
        c3dae_copy_text(ch->target_sid, C3DAE_ID_MAX, slash + 1, dot);
        if (strstr(ch->target_sid, "matrix") != (char *)0 || strstr(ch->target_sid, "transform") != (char *)0) ch->target_kind = C3DAE_ANIM_TARGET_MATRIX;
        else if (strstr(ch->target_sid, "translate") != (char *)0 || strstr(ch->target_sid, "location") != (char *)0) ch->target_kind = C3DAE_ANIM_TARGET_TRANSLATE;
        else if (strstr(ch->target_sid, "rotate") != (char *)0 || strstr(ch->target_sid, "rotation") != (char *)0) ch->target_kind = C3DAE_ANIM_TARGET_ROTATE;
        else if (strstr(ch->target_sid, "scale") != (char *)0) ch->target_kind = C3DAE_ANIM_TARGET_SCALE;
        else ch->target_kind = C3DAE_ANIM_TARGET_SCALAR;
    }
}

static int c3dae_read_sampler_refs(c3dae_context *ctx,
                                   const char *anim_a,
                                   const char *anim_b,
                                   const char *sampler_id,
                                   c3dae_sampler_refs *refs)
{
    const char *p;
    const char *samp;
    const char *samp_tag_end;
    const char *samp_end;
    const char *after_samp;
    const char *inp;
    const char *inp_end;
    char id[C3DAE_ID_MAX];
    char sem[32];
    char src[96];
    memset(refs, 0, sizeof(*refs));
    refs->input_source = -1;
    refs->output_source = -1;
    refs->interpolation_source = -1;
    refs->in_tangent_source = -1;
    refs->out_tangent_source = -1;
    p = anim_a;
    while (1) {
        samp = c3dae_find_start_tag(p, anim_b, "sampler", &samp_tag_end);
        if (samp == C3DAE_XML_NIL) break;
        samp_end = c3dae_find_end_tag(samp_tag_end, anim_b, "sampler", &after_samp);
        if (samp_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        id[0] = '\0';
        c3dae_attr_get(samp, samp_tag_end, "id", id, sizeof(id));
        if (c3dae_ref_equals_id(sampler_id, id)) {
            c3dae_copy_text(refs->id, C3DAE_ID_MAX, id, id + c3dae_strlen_lit(id));
            inp = samp_tag_end;
            while (1) {
                inp = c3dae_find_start_tag(inp, samp_end, "input", &inp_end);
                if (inp == C3DAE_XML_NIL) break;
                sem[0] = '\0'; src[0] = '\0';
                c3dae_attr_get(inp, inp_end, "semantic", sem, sizeof(sem));
                c3dae_attr_get(inp, inp_end, "source", src, sizeof(src));
                if (strcmp(sem, "INPUT") == 0) refs->input_source = c3dae_find_source(ctx, src);
                else if (strcmp(sem, "OUTPUT") == 0) refs->output_source = c3dae_find_source(ctx, src);
                else if (strcmp(sem, "INTERPOLATION") == 0) refs->interpolation_source = c3dae_find_source(ctx, src);
                else if (strcmp(sem, "IN_TANGENT") == 0) refs->in_tangent_source = c3dae_find_source(ctx, src);
                else if (strcmp(sem, "OUT_TANGENT") == 0) refs->out_tangent_source = c3dae_find_source(ctx, src);
                inp = inp_end;
            }
            return C3DAE_OK;
        }
        p = after_samp;
    }
    return C3DAE_ERR_NOT_FOUND;
}

static int c3dae_emit_animation_channel(c3dae_context *ctx,
                                        c3dae_model *model,
                                        const char *anim_id,
                                        c3dae_sampler_refs *refs,
                                        const char *target)
{
    c3dae_anim_channel *ch;
    c3dae_source *in_src;
    c3dae_source *out_src;
    c3dae_u32 kc;
    c3dae_u32 i;
    c3dae_u32 j;
    c3dae_u32 base;
    c3dae_anim_key *key;
    if (model->anim_channel_cap == 0UL || model->anim_key_cap == 0UL) return C3DAE_OK;
    if (refs->input_source < 0 || refs->output_source < 0) return C3DAE_ERR_NOT_FOUND;
    in_src = &ctx->sources[refs->input_source];
    out_src = &ctx->sources[refs->output_source];
    kc = in_src->item_count;
    if (out_src->item_count < kc) kc = out_src->item_count;
    if (kc == 0UL) return C3DAE_OK;
    if (model->anim_channel_count >= model->anim_channel_cap) return C3DAE_ERR_CAPACITY;
    if (model->anim_key_count + kc > model->anim_key_cap) return C3DAE_ERR_CAPACITY;
    ch = &model->anim_channels[model->anim_channel_count];
    memset(ch, 0, sizeof(*ch));
    c3dae_copy_text(ch->id, C3DAE_ID_MAX, anim_id, anim_id + c3dae_strlen_lit(anim_id));
    c3dae_parse_anim_target(ch, target);
    ch->target_node_index = -1;
    ch->first_key = model->anim_key_count;
    ch->key_count = kc;
    ch->interpolation = C3DAE_INTERP_LINEAR;
    if (refs->interpolation_source >= 0 && ctx->sources[refs->interpolation_source].is_name_array && ctx->sources[refs->interpolation_source].name_count > 0UL) {
        if (strcmp(ctx->source_names[ctx->sources[refs->interpolation_source].name_first], "STEP") == 0) ch->interpolation = C3DAE_INTERP_STEP;
        else if (strcmp(ctx->source_names[ctx->sources[refs->interpolation_source].name_first], "BEZIER") == 0) ch->interpolation = C3DAE_INTERP_BEZIER;
    }
    if (refs->in_tangent_source >= 0 || refs->out_tangent_source >= 0) ch->interpolation = C3DAE_INTERP_BEZIER;
    i = 0UL;
    while (i < kc) {
        key = &model->anim_keys[model->anim_key_count];
        memset(key, 0, sizeof(*key));
        base = in_src->first + in_src->offset + (i * in_src->stride);
        key->time = ctx->source_data[base];
        key->value_count = (unsigned int)out_src->stride;
        if (key->value_count > 16U) key->value_count = 16U;
        base = out_src->first + out_src->offset + (i * out_src->stride);
        j = 0UL;
        while (j < (c3dae_u32)key->value_count) {
            key->value[j] = ctx->source_data[base + j];
            key->in_tangent[j] = key->value[j];
            key->out_tangent[j] = key->value[j];
            j++;
        }
        if (refs->in_tangent_source >= 0) {
            c3dae_source_sample_tangent(ctx, refs->in_tangent_source, i, key->value_count, &key->in_tangent_time, key->in_tangent);
            key->tangent_flags |= 1U;
        }
        if (refs->out_tangent_source >= 0) {
            c3dae_source_sample_tangent(ctx, refs->out_tangent_source, i, key->value_count, &key->out_tangent_time, key->out_tangent);
            key->tangent_flags |= 2U;
        }
        model->anim_key_count++;
        i++;
    }
    model->anim_channel_count++;
    return C3DAE_OK;
}

static int c3dae_parse_animation_leaf(c3dae_context *ctx, c3dae_model *model, const char *anim_tag, const char *anim_tag_end, const char *anim_end)
{
    const char *chan;
    const char *chan_end;
    char anim_id[C3DAE_ID_MAX];
    char source_ref[C3DAE_ID_MAX];
    char target[128];
    c3dae_sampler_refs refs;
    int rc;
    c3dae_reset_temp_sources(ctx);
    rc = c3dae_parse_sources(ctx, anim_tag_end, anim_end);
    if (rc != C3DAE_OK) return rc;
    anim_id[0] = '\0';
    c3dae_attr_get(anim_tag, anim_tag_end, "id", anim_id, sizeof(anim_id));
    if (anim_id[0] == '\0') c3dae_attr_get(anim_tag, anim_tag_end, "name", anim_id, sizeof(anim_id));
    chan = anim_tag_end;
    while (1) {
        chan = c3dae_find_start_tag(chan, anim_end, "channel", &chan_end);
        if (chan == C3DAE_XML_NIL) break;
        source_ref[0] = '\0'; target[0] = '\0';
        c3dae_attr_get(chan, chan_end, "source", source_ref, sizeof(source_ref));
        c3dae_attr_get(chan, chan_end, "target", target, sizeof(target));
        rc = c3dae_read_sampler_refs(ctx, anim_tag_end, anim_end, source_ref, &refs);
        if (rc == C3DAE_OK) {
            rc = c3dae_emit_animation_channel(ctx, model, anim_id, &refs, target);
            if (rc != C3DAE_OK) return rc;
        }
        chan = chan_end;
    }
    return C3DAE_OK;
}

static int c3dae_parse_animation_recursive(c3dae_context *ctx, c3dae_model *model, const char *anim_tag, const char *anim_tag_end, const char *anim_end)
{
    const char *child;
    const char *child_tag_end;
    const char *child_end;
    const char *after_child;
    int rc;
    int had_child;
    had_child = 0;
    child = anim_tag_end;
    while (1) {
        child = c3dae_find_start_tag(child, anim_end, "animation", &child_tag_end);
        if (child == C3DAE_XML_NIL) break;
        child_end = c3dae_find_matching_end_tag(child, child_tag_end, anim_end, "animation", &after_child);
        if (child_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        had_child = 1;
        rc = c3dae_parse_animation_recursive(ctx, model, child, child_tag_end, child_end);
        if (rc != C3DAE_OK) return rc;
        child = after_child;
    }
    if (!had_child) return c3dae_parse_animation_leaf(ctx, model, anim_tag, anim_tag_end, anim_end);
    return C3DAE_OK;
}

static int c3dae_parse_animations(c3dae_context *ctx, c3dae_model *model, const char *xml, const char *end)
{
    const char *lib;
    const char *lib_tag_end;
    const char *lib_end;
    const char *after_lib;
    const char *anim;
    const char *anim_tag_end;
    const char *anim_end;
    const char *after_anim;
    int rc;
    lib = c3dae_find_start_tag(xml, end, "library_animations", &lib_tag_end);
    if (lib == C3DAE_XML_NIL) return C3DAE_OK;
    lib_end = c3dae_find_end_tag(lib_tag_end, end, "library_animations", &after_lib);
    if (lib_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
    anim = lib_tag_end;
    while (1) {
        anim = c3dae_find_start_tag(anim, lib_end, "animation", &anim_tag_end);
        if (anim == C3DAE_XML_NIL) break;
        anim_end = c3dae_find_matching_end_tag(anim, anim_tag_end, lib_end, "animation", &after_anim);
        if (anim_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        rc = c3dae_parse_animation_recursive(ctx, model, anim, anim_tag_end, anim_end);
        if (rc != C3DAE_OK) return rc;
        anim = after_anim;
    }
    return C3DAE_OK;
}

static void c3dae_parse_node_own_transform(c3dae_node *n, const char *a, const char *b)
{
    const char *p;
    const char *tag_end;
    const char *elem_end;
    const char *after_elem;
    const char *q;
    c3dae_fixed x;
    c3dae_fixed y;
    c3dae_fixed z;
    c3dae_fixed w;
    c3dae_fixed tmp[16];
    c3dae_matrix_identity(n->local_matrix);
    p = a;
    while (p < b) {
        if (*p != '<') { p++; continue; }
        if (c3dae_start_tag_at(p, b, "matrix", &tag_end)) {
            elem_end = c3dae_find_end_tag(tag_end, b, "matrix", &after_elem);
            if (elem_end != C3DAE_XML_NIL) {
                if (c3dae_parse_matrix_text(tag_end, elem_end, tmp)) c3dae_matrix_mul(n->local_matrix, n->local_matrix, tmp);
                p = after_elem;
                continue;
            }
        } else if (c3dae_start_tag_at(p, b, "translate", &tag_end)) {
            elem_end = c3dae_find_end_tag(tag_end, b, "translate", &after_elem);
            if (elem_end != C3DAE_XML_NIL) {
                q = tag_end;
                x = 0L; y = 0L; z = 0L;
                c3dae_parse_fixed_token(&q, elem_end, &x);
                c3dae_parse_fixed_token(&q, elem_end, &y);
                c3dae_parse_fixed_token(&q, elem_end, &z);
                c3dae_matrix_make_translate(tmp, x, y, z);
                c3dae_matrix_mul(n->local_matrix, n->local_matrix, tmp);
                p = after_elem;
                continue;
            }
        } else if (c3dae_start_tag_at(p, b, "rotate", &tag_end)) {
            elem_end = c3dae_find_end_tag(tag_end, b, "rotate", &after_elem);
            if (elem_end != C3DAE_XML_NIL) {
                q = tag_end;
                x = 0L; y = 0L; z = C3DAE_FIXED_ONE; w = 0L;
                c3dae_parse_fixed_token(&q, elem_end, &x);
                c3dae_parse_fixed_token(&q, elem_end, &y);
                c3dae_parse_fixed_token(&q, elem_end, &z);
                c3dae_parse_fixed_token(&q, elem_end, &w);
                c3dae_matrix_from_rotate(tmp, x, y, z, w);
                c3dae_matrix_mul(n->local_matrix, n->local_matrix, tmp);
                p = after_elem;
                continue;
            }
        } else if (c3dae_start_tag_at(p, b, "scale", &tag_end)) {
            elem_end = c3dae_find_end_tag(tag_end, b, "scale", &after_elem);
            if (elem_end != C3DAE_XML_NIL) {
                q = tag_end;
                x = C3DAE_FIXED_ONE; y = C3DAE_FIXED_ONE; z = C3DAE_FIXED_ONE;
                c3dae_parse_fixed_token(&q, elem_end, &x);
                c3dae_parse_fixed_token(&q, elem_end, &y);
                c3dae_parse_fixed_token(&q, elem_end, &z);
                c3dae_matrix_make_scale(tmp, x, y, z);
                c3dae_matrix_mul(n->local_matrix, n->local_matrix, tmp);
                p = after_elem;
                continue;
            }
        }
        p++;
    }
}


static void c3dae_parse_instance_materials(c3dae_node *n, const char *a, const char *b)
{
    const char *p;
    const char *im;
    const char *im_end;
    char sym[C3DAE_ID_MAX];
    char target[C3DAE_ID_MAX];
    unsigned int k;
    p = a;
    while (1) {
        im = c3dae_find_start_tag(p, b, "instance_material", &im_end);
        if (im == C3DAE_XML_NIL) break;
        if (n->material_bind_count < C3DAE_MAX_NODE_MATERIAL_BINDINGS) {
            k = n->material_bind_count;
            sym[0] = '\0';
            target[0] = '\0';
            c3dae_attr_get(im, im_end, "symbol", sym, sizeof(sym));
            c3dae_attr_get(im, im_end, "target", target, sizeof(target));
            c3dae_copy_text(n->material_symbol[k], C3DAE_ID_MAX, sym, sym + c3dae_strlen_lit(sym));
            c3dae_ref_to_id(n->material_target[k], C3DAE_ID_MAX, target);
            n->material_index[k] = -1;
            n->material_bind_count++;
        }
        p = im_end;
    }
}

static void c3dae_parse_node_instances(c3dae_node *n, const char *a, const char *b)
{
    const char *ig;
    const char *ig_end;
    const char *ig_close;
    const char *after_ig;
    const char *ic;
    const char *ic_tag_end;
    const char *ic_end;
    const char *after_ic;
    const char *sk;
    const char *sk_tag_end;
    const char *sk_end;
    const char *after_sk;
    const char *icam;
    const char *icam_end;
    const char *ilight;
    const char *ilight_end;
    char tmp[C3DAE_ID_MAX];
    ig = c3dae_find_start_tag(a, b, "instance_geometry", &ig_end);
    if (ig != C3DAE_XML_NIL) {
        tmp[0] = '\0';
        c3dae_attr_get(ig, ig_end, "url", tmp, sizeof(tmp));
        c3dae_ref_to_id(n->instance_geometry, C3DAE_ID_MAX, tmp);
        ig_close = c3dae_find_end_tag(ig_end, b, "instance_geometry", &after_ig);
        if (ig_close != C3DAE_XML_NIL) c3dae_parse_instance_materials(n, ig_end, ig_close);
    }
    ic = c3dae_find_start_tag(a, b, "instance_controller", &ic_tag_end);
    if (ic != C3DAE_XML_NIL) {
        tmp[0] = '\0';
        c3dae_attr_get(ic, ic_tag_end, "url", tmp, sizeof(tmp));
        c3dae_ref_to_id(n->instance_controller, C3DAE_ID_MAX, tmp);
        ic_end = c3dae_find_end_tag(ic_tag_end, b, "instance_controller", &after_ic);
        if (ic_end != C3DAE_XML_NIL) {
            c3dae_parse_instance_materials(n, ic_tag_end, ic_end);
            sk = c3dae_find_start_tag(ic_tag_end, ic_end, "skeleton", &sk_tag_end);
            if (sk != C3DAE_XML_NIL) {
                sk_end = c3dae_find_end_tag(sk_tag_end, ic_end, "skeleton", &after_sk);
                if (sk_end != C3DAE_XML_NIL) {
                    tmp[0] = '\0';
                    c3dae_copy_text(tmp, sizeof(tmp), sk_tag_end, sk_end);
                    c3dae_ref_to_id(n->skeleton, C3DAE_ID_MAX, tmp);
                }
            }
        }
    }
    icam = c3dae_find_start_tag(a, b, "instance_camera", &icam_end);
    if (icam != C3DAE_XML_NIL) {
        tmp[0] = '\0';
        c3dae_attr_get(icam, icam_end, "url", tmp, sizeof(tmp));
        c3dae_ref_to_id(n->instance_camera, C3DAE_ID_MAX, tmp);
    }
    ilight = c3dae_find_start_tag(a, b, "instance_light", &ilight_end);
    if (ilight != C3DAE_XML_NIL) {
        tmp[0] = '\0';
        c3dae_attr_get(ilight, ilight_end, "url", tmp, sizeof(tmp));
        c3dae_ref_to_id(n->instance_light, C3DAE_ID_MAX, tmp);
    }
}

static int c3dae_parse_node_recursive(c3dae_model *model, const char *node_tag, const char *node_tag_end, const char *node_end, int parent)
{
    c3dae_node *n;
    c3dae_u32 my_index;
    const char *own_end;
    const char *child;
    const char *child_tag_end;
    const char *child_end;
    const char *after_child;
    char type[32];
    int rc;
    if (model->node_count >= model->node_cap) return C3DAE_ERR_CAPACITY;
    my_index = model->node_count;
    n = &model->nodes[model->node_count];
    memset(n, 0, sizeof(*n));
    n->parent = parent;
    n->type = C3DAE_NODE_NODE;
    n->mesh_index = -1;
    n->controller_index = -1;
    n->camera_index = -1;
    n->light_index = -1;
    {
        unsigned int mbi;
        mbi = 0U;
        while (mbi < C3DAE_MAX_NODE_MATERIAL_BINDINGS) { n->material_index[mbi] = -1; mbi++; }
    }
    c3dae_matrix_identity(n->local_matrix);
    c3dae_matrix_identity(n->base_local_matrix);
    c3dae_matrix_identity(n->world_matrix);
    c3dae_attr_get(node_tag, node_tag_end, "id", n->id, C3DAE_ID_MAX);
    c3dae_attr_get(node_tag, node_tag_end, "sid", n->sid, C3DAE_ID_MAX);
    c3dae_attr_get(node_tag, node_tag_end, "name", n->name, C3DAE_ID_MAX);
    type[0] = '\0';
    c3dae_attr_get(node_tag, node_tag_end, "type", type, sizeof(type));
    if (strcmp(type, "JOINT") == 0) n->type = C3DAE_NODE_JOINT;
    own_end = c3dae_find_start_tag(node_tag_end, node_end, "node", &child_tag_end);
    if (own_end == C3DAE_XML_NIL) own_end = node_end;
    c3dae_parse_node_own_transform(n, node_tag_end, own_end);
    c3dae_matrix_copy(n->base_local_matrix, n->local_matrix);
    c3dae_parse_node_instances(n, node_tag_end, own_end);
    model->node_count++;
    child = node_tag_end;
    while (1) {
        child = c3dae_find_start_tag(child, node_end, "node", &child_tag_end);
        if (child == C3DAE_XML_NIL) break;
        child_end = c3dae_find_matching_end_tag(child, child_tag_end, node_end, "node", &after_child);
        if (child_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        rc = c3dae_parse_node_recursive(model, child, child_tag_end, child_end, (int)my_index);
        if (rc != C3DAE_OK) return rc;
        child = after_child;
    }
    return C3DAE_OK;
}

static int c3dae_parse_visual_scenes(c3dae_model *model, const char *xml, const char *end)
{
    const char *lib;
    const char *lib_tag_end;
    const char *lib_end;
    const char *after_lib;
    const char *node;
    const char *node_tag_end;
    const char *node_end;
    const char *after_node;
    int rc;
    if (model->node_cap == 0UL) return C3DAE_OK;
    lib = c3dae_find_start_tag(xml, end, "library_visual_scenes", &lib_tag_end);
    if (lib == C3DAE_XML_NIL) return C3DAE_OK;
    lib_end = c3dae_find_end_tag(lib_tag_end, end, "library_visual_scenes", &after_lib);
    if (lib_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
    node = lib_tag_end;
    while (1) {
        node = c3dae_find_start_tag(node, lib_end, "node", &node_tag_end);
        if (node == C3DAE_XML_NIL) break;
        node_end = c3dae_find_matching_end_tag(node, node_tag_end, lib_end, "node", &after_node);
        if (node_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        rc = c3dae_parse_node_recursive(model, node, node_tag_end, node_end, -1);
        if (rc != C3DAE_OK) return rc;
        node = after_node;
    }
    return C3DAE_OK;
}

static void c3dae_resolve_nodes_and_joints(c3dae_model *model)
{
    c3dae_u32 i;
    c3dae_u32 j;
    unsigned int mbi;
    c3dae_node *n;
    i = 0UL;
    while (i < model->node_count) {
        n = &model->nodes[i];
        if (n->instance_geometry[0] != '\0') n->mesh_index = c3dae_find_mesh_by_id(model, n->instance_geometry);
        if (n->instance_controller[0] != '\0') n->controller_index = c3dae_find_controller_by_id(model, n->instance_controller);
        if (n->instance_camera[0] != '\0' && model->cameras != (c3dae_camera *)0) n->camera_index = c3dae_find_camera_by_id(model, n->instance_camera);
        if (n->instance_light[0] != '\0' && model->lights != (c3dae_light *)0) n->light_index = c3dae_find_light_by_id(model, n->instance_light);
        mbi = 0U;
        while (mbi < n->material_bind_count && mbi < C3DAE_MAX_NODE_MATERIAL_BINDINGS) {
            n->material_index[mbi] = c3dae_find_material_by_id(model, n->material_target[mbi]);
            mbi++;
        }
        i++;
    }
    i = 0UL;
    while (i < model->skin_joint_count) {
        model->skin_joints[i].node_index = c3dae_find_node_by_id_or_sid(model, model->skin_joints[i].sid);
        i++;
    }
    i = 0UL;
    while (i < model->anim_channel_count) {
        model->anim_channels[i].target_node_index = c3dae_find_node_by_id_or_sid(model, model->anim_channels[i].target_node);
        i++;
    }
    (void)j;
}

void c3dae_build_node_world_matrices(c3dae_model *model)
{
    c3dae_u32 i;
    c3dae_node *n;
    if (model == (c3dae_model *)0 || model->nodes == (c3dae_node *)0) return;
    i = 0UL;
    while (i < model->node_count) {
        n = &model->nodes[i];
        if (n->parent >= 0 && (c3dae_u32)n->parent < model->node_count) c3dae_matrix_mul(n->world_matrix, model->nodes[n->parent].world_matrix, n->local_matrix);
        else c3dae_matrix_copy(n->world_matrix, n->local_matrix);
        i++;
    }
}

static int c3dae_parse_images(c3dae_model *model, const char *xml, const char *end)
{
    const char *p;
    const char *img;
    const char *img_tag_end;
    const char *img_end;
    const char *after_img;
    const char *init;
    const char *init_tag_end;
    const char *init_end;
    const char *after_init;
    c3dae_image *im;
    if (model->image_cap == 0UL) return C3DAE_OK;
    p = xml;
    while (1) {
        img = c3dae_find_start_tag(p, end, "image", &img_tag_end);
        if (img == C3DAE_XML_NIL) break;
        img_end = c3dae_find_end_tag(img_tag_end, end, "image", &after_img);
        if (img_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        if (model->image_count >= model->image_cap) return C3DAE_ERR_CAPACITY;
        im = &model->images[model->image_count];
        memset(im, 0, sizeof(*im));
        c3dae_attr_get(img, img_tag_end, "id", im->id, C3DAE_ID_MAX);
        init = c3dae_find_start_tag(img_tag_end, img_end, "init_from", &init_tag_end);
        if (init != C3DAE_XML_NIL) {
            init_end = c3dae_find_end_tag(init_tag_end, img_end, "init_from", &after_init);
            if (init_end != C3DAE_XML_NIL) c3dae_copy_text(im->path, C3DAE_PATH_MAX, init_tag_end, init_end);
        }
        model->image_count++;
        p = after_img;
    }
    return C3DAE_OK;
}

typedef struct c3dae_fx_newparam_tag {
    char sid[C3DAE_ID_MAX];
    char value[C3DAE_ID_MAX];
    int kind;
} c3dae_fx_newparam;

static int c3dae_parse_fixed_text4(const char *a, const char *b, c3dae_fixed *out)
{
    const char *p;
    unsigned int i;
    p = a;
    i = 0U;
    while (i < 4U) { out[i] = (i == 3U) ? C3DAE_FIXED_ONE : 0L; i++; }
    i = 0U;
    while (i < 4U && p < b) {
        if (!c3dae_parse_fixed_token(&p, b, &out[i])) break;
        i++;
    }
    return i > 0U;
}

static int c3dae_parse_fixed_text3(const char *a, const char *b, c3dae_fixed *out)
{
    const char *p;
    unsigned int i;
    p = a;
    i = 0U;
    while (i < 3U) { out[i] = 0L; i++; }
    i = 0U;
    while (i < 3U && p < b) {
        if (!c3dae_parse_fixed_token(&p, b, &out[i])) break;
        i++;
    }
    return i > 0U;
}

static int c3dae_parse_color_child(const char *a, const char *b, const char *name, c3dae_fixed *out)
{
    const char *tag;
    const char *tag_end;
    const char *elem_end;
    const char *after_elem;
    const char *c;
    const char *c_end_tag;
    const char *c_end;
    const char *after_c;
    tag = c3dae_find_start_tag(a, b, name, &tag_end);
    if (tag == C3DAE_XML_NIL) return 0;
    elem_end = c3dae_find_end_tag(tag_end, b, name, &after_elem);
    if (elem_end == C3DAE_XML_NIL) return 0;
    c = c3dae_find_start_tag(tag_end, elem_end, "color", &c_end_tag);
    if (c == C3DAE_XML_NIL) return 0;
    c_end = c3dae_find_end_tag(c_end_tag, elem_end, "color", &after_c);
    if (c_end == C3DAE_XML_NIL) return 0;
    return c3dae_parse_fixed_text4(c_end_tag, c_end, out);
}


static int c3dae_parse_scalar_text_child(const char *a, const char *b, const char *name, c3dae_fixed *out)
{
    const char *tag;
    const char *tag_end;
    const char *elem_end;
    const char *after_elem;
    const char *p;
    tag = c3dae_find_start_tag(a, b, name, &tag_end);
    if (tag == C3DAE_XML_NIL) return 0;
    elem_end = c3dae_find_end_tag(tag_end, b, name, &after_elem);
    if (elem_end == C3DAE_XML_NIL) return 0;
    p = tag_end;
    return c3dae_parse_fixed_token(&p, elem_end, out);
}

static int c3dae_parse_vec3_text_child(const char *a, const char *b, const char *name, c3dae_fixed *out)
{
    const char *tag;
    const char *tag_end;
    const char *elem_end;
    const char *after_elem;
    tag = c3dae_find_start_tag(a, b, name, &tag_end);
    if (tag == C3DAE_XML_NIL) return 0;
    elem_end = c3dae_find_end_tag(tag_end, b, name, &after_elem);
    if (elem_end == C3DAE_XML_NIL) return 0;
    return c3dae_parse_fixed_text3(tag_end, elem_end, out);
}

static int c3dae_parse_float_child(const char *a, const char *b, const char *name, c3dae_fixed *out)
{
    const char *tag;
    const char *tag_end;
    const char *elem_end;
    const char *after_elem;
    const char *f;
    const char *f_end_tag;
    const char *f_end;
    const char *after_f;
    const char *p;
    tag = c3dae_find_start_tag(a, b, name, &tag_end);
    if (tag == C3DAE_XML_NIL) return 0;
    elem_end = c3dae_find_end_tag(tag_end, b, name, &after_elem);
    if (elem_end == C3DAE_XML_NIL) return 0;
    f = c3dae_find_start_tag(tag_end, elem_end, "float", &f_end_tag);
    if (f == C3DAE_XML_NIL) return 0;
    f_end = c3dae_find_end_tag(f_end_tag, elem_end, "float", &after_f);
    if (f_end == C3DAE_XML_NIL) return 0;
    p = f_end_tag;
    return c3dae_parse_fixed_token(&p, f_end, out);
}

static unsigned int c3dae_fx_find_param(c3dae_fx_newparam *params, unsigned int count, const char *sid)
{
    unsigned int i;
    i = 0U;
    while (i < count) {
        if (strcmp(params[i].sid, sid) == 0) return i;
        i++;
    }
    return 9999U;
}

static void c3dae_effect_resolve_texture_to(char *tex_out, char *img_out, unsigned int *flags, unsigned int flag_bit, c3dae_fx_newparam *params, unsigned int count, const char *sampler)
{
    unsigned int si;
    unsigned int surf;
    c3dae_copy_text(tex_out, C3DAE_ID_MAX, sampler, sampler + c3dae_strlen_lit(sampler));
    *flags |= flag_bit;
    si = c3dae_fx_find_param(params, count, sampler);
    if (si != 9999U && params[si].kind == 2) {
        surf = c3dae_fx_find_param(params, count, params[si].value);
        if (surf != 9999U && params[surf].kind == 1) {
            c3dae_copy_text(img_out, C3DAE_ID_MAX, params[surf].value, params[surf].value + c3dae_strlen_lit(params[surf].value));
        }
    }
}

static void c3dae_parse_texture_child(const char *a, const char *b, const char *name, c3dae_effect *e, c3dae_fx_newparam *params, unsigned int count)
{
    const char *tag;
    const char *tag_end;
    const char *elem_end;
    const char *after_elem;
    const char *tex;
    const char *tex_end;
    char sampler[C3DAE_ID_MAX];
    tag = c3dae_find_start_tag(a, b, name, &tag_end);
    if (tag == C3DAE_XML_NIL) return;
    elem_end = c3dae_find_end_tag(tag_end, b, name, &after_elem);
    if (elem_end == C3DAE_XML_NIL) return;
    tex = c3dae_find_start_tag(tag_end, elem_end, "texture", &tex_end);
    if (tex == C3DAE_XML_NIL) return;
    sampler[0] = '\0';
    c3dae_attr_get(tex, tex_end, "texture", sampler, sizeof(sampler));
    if (sampler[0] != '\0') {
        if (strcmp(name, "diffuse") == 0) c3dae_effect_resolve_texture_to(e->diffuse_texture, e->diffuse_image, &e->flags, C3DAE_EFFECT_HAS_DIFFUSE_TEXTURE, params, count, sampler);
        else if (strcmp(name, "specular") == 0) c3dae_effect_resolve_texture_to(e->specular_texture, e->specular_image, &e->flags, C3DAE_EFFECT_HAS_SPECULAR_TEXTURE, params, count, sampler);
        else if (strcmp(name, "emission") == 0) c3dae_effect_resolve_texture_to(e->emission_texture, e->emission_image, &e->flags, C3DAE_EFFECT_HAS_EMISSION_TEXTURE, params, count, sampler);
        else if (strcmp(name, "bump") == 0 || strcmp(name, "normal") == 0) c3dae_effect_resolve_texture_to(e->normal_texture, e->normal_image, &e->flags, C3DAE_EFFECT_HAS_NORMAL_TEXTURE, params, count, sampler);
    }
}

static int c3dae_parse_effects(c3dae_model *model, const char *xml, const char *end)
{
    const char *p;
    const char *eff;
    const char *eff_tag_end;
    const char *eff_end;
    const char *after_eff;
    const char *prof;
    const char *prof_tag_end;
    const char *prof_end;
    const char *after_prof;
    const char *np;
    const char *np_tag_end;
    const char *np_end;
    const char *after_np;
    const char *surface;
    const char *sampler2d;
    const char *init;
    const char *source;
    const char *txt_end;
    const char *after_txt;
    const char *tech;
    const char *tech_tag_end;
    const char *tech_end;
    const char *after_tech;
    const char *shader;
    const char *shader_tag_end;
    const char *shader_end;
    const char *after_shader;
    c3dae_effect *e;
    c3dae_fx_newparam params[32];
    unsigned int param_count;
    if (model->effect_cap == 0UL) return C3DAE_OK;
    p = xml;
    while (1) {
        eff = c3dae_find_start_tag(p, end, "effect", &eff_tag_end);
        if (eff == C3DAE_XML_NIL) break;
        eff_end = c3dae_find_end_tag(eff_tag_end, end, "effect", &after_eff);
        if (eff_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        if (model->effect_count >= model->effect_cap) return C3DAE_ERR_CAPACITY;
        e = &model->effects[model->effect_count];
        memset(e, 0, sizeof(*e));
        e->shader_model = C3DAE_FX_SHADER_NONE;
        e->diffuse[0] = C3DAE_FIXED_ONE; e->diffuse[1] = C3DAE_FIXED_ONE; e->diffuse[2] = C3DAE_FIXED_ONE; e->diffuse[3] = C3DAE_FIXED_ONE;
        e->transparent[0] = C3DAE_FIXED_ONE; e->transparent[1] = C3DAE_FIXED_ONE; e->transparent[2] = C3DAE_FIXED_ONE; e->transparent[3] = C3DAE_FIXED_ONE;
        e->transparency = C3DAE_FIXED_ONE;
        e->index_of_refraction = C3DAE_FIXED_ONE;
        c3dae_attr_get(eff, eff_tag_end, "id", e->id, C3DAE_ID_MAX);
        c3dae_attr_get(eff, eff_tag_end, "name", e->name, C3DAE_ID_MAX);
        param_count = 0U;
        prof = c3dae_find_start_tag(eff_tag_end, eff_end, "profile_COMMON", &prof_tag_end);
        if (prof != C3DAE_XML_NIL) prof_end = c3dae_find_end_tag(prof_tag_end, eff_end, "profile_COMMON", &after_prof);
        else { prof_tag_end = eff_tag_end; prof_end = eff_end; }
        if (prof_end == C3DAE_XML_NIL) prof_end = eff_end;
        np = prof_tag_end;
        while (1) {
            np = c3dae_find_start_tag(np, prof_end, "newparam", &np_tag_end);
            if (np == C3DAE_XML_NIL) break;
            np_end = c3dae_find_end_tag(np_tag_end, prof_end, "newparam", &after_np);
            if (np_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
            if (param_count < 32U) {
                memset(&params[param_count], 0, sizeof(params[param_count]));
                c3dae_attr_get(np, np_tag_end, "sid", params[param_count].sid, C3DAE_ID_MAX);
                surface = c3dae_find_start_tag(np_tag_end, np_end, "surface", &txt_end);
                sampler2d = c3dae_find_start_tag(np_tag_end, np_end, "sampler2D", &txt_end);
                if (surface != C3DAE_XML_NIL) {
                    init = c3dae_find_start_tag(np_tag_end, np_end, "init_from", &txt_end);
                    if (init != C3DAE_XML_NIL) {
                        source = c3dae_find_end_tag(txt_end, np_end, "init_from", &after_txt);
                        if (source != C3DAE_XML_NIL) c3dae_copy_text(params[param_count].value, C3DAE_ID_MAX, txt_end, source);
                        params[param_count].kind = 1;
                    }
                } else if (sampler2d != C3DAE_XML_NIL) {
                    source = c3dae_find_start_tag(np_tag_end, np_end, "source", &txt_end);
                    if (source != C3DAE_XML_NIL) {
                        init = c3dae_find_end_tag(txt_end, np_end, "source", &after_txt);
                        if (init != C3DAE_XML_NIL) c3dae_copy_text(params[param_count].value, C3DAE_ID_MAX, txt_end, init);
                        params[param_count].kind = 2;
                    }
                }
                if (params[param_count].sid[0] != '\0') param_count++;
            }
            np = after_np;
        }
        tech = c3dae_find_start_tag(prof_tag_end, prof_end, "technique", &tech_tag_end);
        if (tech != C3DAE_XML_NIL) tech_end = c3dae_find_end_tag(tech_tag_end, prof_end, "technique", &after_tech);
        else { tech_tag_end = prof_tag_end; tech_end = prof_end; }
        if (tech_end == C3DAE_XML_NIL) tech_end = prof_end;
        shader = c3dae_find_start_tag(tech_tag_end, tech_end, "phong", &shader_tag_end);
        e->shader_model = C3DAE_FX_SHADER_PHONG;
        if (shader == C3DAE_XML_NIL) { shader = c3dae_find_start_tag(tech_tag_end, tech_end, "blinn", &shader_tag_end); e->shader_model = C3DAE_FX_SHADER_BLINN; }
        if (shader == C3DAE_XML_NIL) { shader = c3dae_find_start_tag(tech_tag_end, tech_end, "lambert", &shader_tag_end); e->shader_model = C3DAE_FX_SHADER_LAMBERT; }
        if (shader == C3DAE_XML_NIL) { shader = c3dae_find_start_tag(tech_tag_end, tech_end, "constant", &shader_tag_end); e->shader_model = C3DAE_FX_SHADER_CONSTANT; }
        if (shader != C3DAE_XML_NIL) {
            if (e->shader_model == C3DAE_FX_SHADER_PHONG) shader_end = c3dae_find_end_tag(shader_tag_end, tech_end, "phong", &after_shader);
            else if (e->shader_model == C3DAE_FX_SHADER_BLINN) shader_end = c3dae_find_end_tag(shader_tag_end, tech_end, "blinn", &after_shader);
            else if (e->shader_model == C3DAE_FX_SHADER_LAMBERT) shader_end = c3dae_find_end_tag(shader_tag_end, tech_end, "lambert", &after_shader);
            else shader_end = c3dae_find_end_tag(shader_tag_end, tech_end, "constant", &after_shader);
            if (shader_end != C3DAE_XML_NIL) {
                c3dae_parse_color_child(shader_tag_end, shader_end, "emission", e->emission);
                c3dae_parse_color_child(shader_tag_end, shader_end, "ambient", e->ambient);
                c3dae_parse_color_child(shader_tag_end, shader_end, "diffuse", e->diffuse);
                c3dae_parse_color_child(shader_tag_end, shader_end, "specular", e->specular);
                c3dae_parse_color_child(shader_tag_end, shader_end, "reflective", e->reflective);
                c3dae_parse_color_child(shader_tag_end, shader_end, "transparent", e->transparent);
                c3dae_parse_float_child(shader_tag_end, shader_end, "shininess", &e->shininess);
                c3dae_parse_float_child(shader_tag_end, shader_end, "transparency", &e->transparency);
                c3dae_parse_float_child(shader_tag_end, shader_end, "index_of_refraction", &e->index_of_refraction);
                c3dae_parse_texture_child(shader_tag_end, shader_end, "diffuse", e, params, param_count);
                c3dae_parse_texture_child(shader_tag_end, shader_end, "specular", e, params, param_count);
                c3dae_parse_texture_child(shader_tag_end, shader_end, "emission", e, params, param_count);
                c3dae_parse_texture_child(shader_tag_end, shader_end, "bump", e, params, param_count);
            }
        }
        model->effect_count++;
        p = after_eff;
    }
    return C3DAE_OK;
}

static int c3dae_find_effect_by_id(c3dae_model *model, const char *ref)
{
    c3dae_u32 i;
    char id[C3DAE_ID_MAX];
    c3dae_ref_to_id(id, sizeof(id), ref);
    i = 0UL;
    while (i < model->effect_count) {
        if (strcmp(model->effects[i].id, id) == 0) return (int)i;
        i++;
    }
    return -1;
}

static void c3dae_resolve_material_effects(c3dae_model *model)
{
    c3dae_u32 i;
    i = 0UL;
    while (i < model->material_count) {
        model->materials[i].effect_index = c3dae_find_effect_by_id(model, model->materials[i].effect);
        if (model->materials[i].effect_index >= 0) {
            model->materials[i].diffuse[0] = model->effects[model->materials[i].effect_index].diffuse[0];
            model->materials[i].diffuse[1] = model->effects[model->materials[i].effect_index].diffuse[1];
            model->materials[i].diffuse[2] = model->effects[model->materials[i].effect_index].diffuse[2];
            model->materials[i].diffuse[3] = model->effects[model->materials[i].effect_index].diffuse[3];
        }
        i++;
    }
}

static int c3dae_parse_physics_scenes(c3dae_model *model, const char *xml, const char *end)
{
    const char *p;
    const char *ps;
    const char *ps_tag_end;
    const char *ps_end;
    const char *after_ps;
    const char *g;
    const char *g_tag_end;
    const char *g_end;
    const char *after_g;
    const char *ts;
    const char *ts_tag_end;
    const char *ts_end;
    const char *after_ts;
    const char *q;
    c3dae_physics_scene *s;
    if (model->physics_scene_cap == 0UL) return C3DAE_OK;
    p = xml;
    while (1) {
        ps = c3dae_find_start_tag(p, end, "physics_scene", &ps_tag_end);
        if (ps == C3DAE_XML_NIL) break;
        ps_end = c3dae_find_end_tag(ps_tag_end, end, "physics_scene", &after_ps);
        if (ps_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        if (model->physics_scene_count >= model->physics_scene_cap) return C3DAE_ERR_CAPACITY;
        s = &model->physics_scenes[model->physics_scene_count];
        memset(s, 0, sizeof(*s));
        s->gravity[1] = (c3dae_fixed)(-10L << C3DAE_FIXED_SHIFT);
        s->time_step = (c3dae_fixed)(1092L); /* approx 1/60 */
        c3dae_attr_get(ps, ps_tag_end, "id", s->id, C3DAE_ID_MAX);
        c3dae_attr_get(ps, ps_tag_end, "name", s->name, C3DAE_ID_MAX);
        g = c3dae_find_start_tag(ps_tag_end, ps_end, "gravity", &g_tag_end);
        if (g != C3DAE_XML_NIL) {
            g_end = c3dae_find_end_tag(g_tag_end, ps_end, "gravity", &after_g);
            if (g_end != C3DAE_XML_NIL) c3dae_parse_fixed_text3(g_tag_end, g_end, s->gravity);
        }
        ts = c3dae_find_start_tag(ps_tag_end, ps_end, "time_step", &ts_tag_end);
        if (ts != C3DAE_XML_NIL) {
            ts_end = c3dae_find_end_tag(ts_tag_end, ps_end, "time_step", &after_ts);
            if (ts_end != C3DAE_XML_NIL) { q = ts_tag_end; c3dae_parse_fixed_token(&q, ts_end, &s->time_step); }
        }
        model->physics_scene_count++;
        p = after_ps;
    }
    return C3DAE_OK;
}

static int c3dae_parse_physics_models(c3dae_model *model, const char *xml, const char *end)
{
    const char *p;
    const char *pm;
    const char *pm_tag_end;
    const char *pm_end;
    const char *after_pm;
    const char *rb;
    const char *rb_tag_end;
    const char *rb_end;
    const char *after_rb;
    const char *tc;
    const char *tc_tag_end;
    const char *tc_end;
    const char *after_tc;
    const char *tag;
    const char *tag_end;
    const char *elem_end;
    const char *after_elem;
    const char *q;
    c3dae_physics_model *pmx;
    c3dae_rigid_body *body;
    if (model->physics_model_cap == 0UL) return C3DAE_OK;
    p = xml;
    while (1) {
        pm = c3dae_find_start_tag(p, end, "physics_model", &pm_tag_end);
        if (pm == C3DAE_XML_NIL) break;
        pm_end = c3dae_find_end_tag(pm_tag_end, end, "physics_model", &after_pm);
        if (pm_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        if (model->physics_model_count >= model->physics_model_cap) return C3DAE_ERR_CAPACITY;
        pmx = &model->physics_models[model->physics_model_count];
        memset(pmx, 0, sizeof(*pmx));
        c3dae_attr_get(pm, pm_tag_end, "id", pmx->id, C3DAE_ID_MAX);
        c3dae_attr_get(pm, pm_tag_end, "name", pmx->name, C3DAE_ID_MAX);
        pmx->first_rigid_body = model->rigid_body_count;
        rb = pm_tag_end;
        while (1) {
            rb = c3dae_find_start_tag(rb, pm_end, "rigid_body", &rb_tag_end);
            if (rb == C3DAE_XML_NIL) break;
            rb_end = c3dae_find_end_tag(rb_tag_end, pm_end, "rigid_body", &after_rb);
            if (rb_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
            if (model->rigid_body_count >= model->rigid_body_cap) return C3DAE_ERR_CAPACITY;
            body = &model->rigid_bodies[model->rigid_body_count];
            memset(body, 0, sizeof(*body));
            body->dynamic = 1;
            body->mass = C3DAE_FIXED_ONE;
            c3dae_attr_get(rb, rb_tag_end, "sid", body->sid, C3DAE_ID_MAX);
            c3dae_attr_get(rb, rb_tag_end, "name", body->name, C3DAE_ID_MAX);
            tc = c3dae_find_start_tag(rb_tag_end, rb_end, "technique_common", &tc_tag_end);
            if (tc != C3DAE_XML_NIL) tc_end = c3dae_find_end_tag(tc_tag_end, rb_end, "technique_common", &after_tc);
            else { tc_tag_end = rb_tag_end; tc_end = rb_end; }
            if (tc_end == C3DAE_XML_NIL) tc_end = rb_end;
            c3dae_parse_float_child(tc_tag_end, tc_end, "mass", &body->mass);
            tag = c3dae_find_start_tag(tc_tag_end, tc_end, "dynamic", &tag_end);
            if (tag != C3DAE_XML_NIL) {
                elem_end = c3dae_find_end_tag(tag_end, tc_end, "dynamic", &after_elem);
                if (elem_end != C3DAE_XML_NIL && tag_end < elem_end && *tag_end == 'f') body->dynamic = 0;
            }
            tag = c3dae_find_start_tag(tc_tag_end, tc_end, "inertia", &tag_end);
            if (tag != C3DAE_XML_NIL) { elem_end = c3dae_find_end_tag(tag_end, tc_end, "inertia", &after_elem); if (elem_end != C3DAE_XML_NIL) c3dae_parse_fixed_text3(tag_end, elem_end, body->inertia); }
            tag = c3dae_find_start_tag(tc_tag_end, tc_end, "velocity", &tag_end);
            if (tag != C3DAE_XML_NIL) { elem_end = c3dae_find_end_tag(tag_end, tc_end, "velocity", &after_elem); if (elem_end != C3DAE_XML_NIL) c3dae_parse_fixed_text3(tag_end, elem_end, body->velocity); }
            tag = c3dae_find_start_tag(tc_tag_end, tc_end, "angular_velocity", &tag_end);
            if (tag != C3DAE_XML_NIL) { elem_end = c3dae_find_end_tag(tag_end, tc_end, "angular_velocity", &after_elem); if (elem_end != C3DAE_XML_NIL) c3dae_parse_fixed_text3(tag_end, elem_end, body->angular_velocity); }
            if (c3dae_find_start_tag(tc_tag_end, tc_end, "box", &tag_end) != C3DAE_XML_NIL) body->shape_kind = C3DAE_PHYS_SHAPE_BOX;
            else if (c3dae_find_start_tag(tc_tag_end, tc_end, "sphere", &tag_end) != C3DAE_XML_NIL) body->shape_kind = C3DAE_PHYS_SHAPE_SPHERE;
            else if (c3dae_find_start_tag(tc_tag_end, tc_end, "plane", &tag_end) != C3DAE_XML_NIL) body->shape_kind = C3DAE_PHYS_SHAPE_PLANE;
            else if (c3dae_find_start_tag(tc_tag_end, tc_end, "instance_geometry", &tag_end) != C3DAE_XML_NIL) body->shape_kind = C3DAE_PHYS_SHAPE_MESH;
            tag = c3dae_find_start_tag(tc_tag_end, tc_end, "half_extents", &tag_end);
            if (tag != C3DAE_XML_NIL) { elem_end = c3dae_find_end_tag(tag_end, tc_end, "half_extents", &after_elem); if (elem_end != C3DAE_XML_NIL) c3dae_parse_fixed_text3(tag_end, elem_end, body->shape_size); }
            tag = c3dae_find_start_tag(tc_tag_end, tc_end, "radius", &tag_end);
            if (tag != C3DAE_XML_NIL) { elem_end = c3dae_find_end_tag(tag_end, tc_end, "radius", &after_elem); if (elem_end != C3DAE_XML_NIL) { q = tag_end; c3dae_parse_fixed_token(&q, elem_end, &body->shape_size[0]); } }
            model->rigid_body_count++;
            pmx->rigid_body_count++;
            rb = after_rb;
        }
        model->physics_model_count++;
        p = after_pm;
    }
    return C3DAE_OK;
}

static int c3dae_parse_kinematics(c3dae_model *model, const char *xml, const char *end)
{
    const char *p;
    const char *km;
    const char *km_tag_end;
    const char *km_end;
    const char *after_km;
    const char *jt;
    const char *jt_tag_end;
    const char *jt_end;
    const char *after_jt;
    const char *sub;
    const char *sub_tag_end;
    const char *sub_end;
    const char *after_sub;
    const char *axis;
    const char *axis_tag_end;
    const char *axis_end;
    const char *after_axis;
    const char *lim;
    const char *lim_tag_end;
    const char *lim_end;
    const char *after_lim;
    const char *minv;
    const char *maxv;
    const char *val_end;
    const char *q;
    c3dae_kinematic_model *mk;
    c3dae_kinematic_joint *j;
    if (model->kinematic_model_cap == 0UL) return C3DAE_OK;
    p = xml;
    while (1) {
        km = c3dae_find_start_tag(p, end, "kinematics_model", &km_tag_end);
        if (km == C3DAE_XML_NIL) break;
        km_end = c3dae_find_end_tag(km_tag_end, end, "kinematics_model", &after_km);
        if (km_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        if (model->kinematic_model_count >= model->kinematic_model_cap) return C3DAE_ERR_CAPACITY;
        mk = &model->kinematic_models[model->kinematic_model_count];
        memset(mk, 0, sizeof(*mk));
        c3dae_attr_get(km, km_tag_end, "id", mk->id, C3DAE_ID_MAX);
        c3dae_attr_get(km, km_tag_end, "name", mk->name, C3DAE_ID_MAX);
        mk->first_joint = model->kinematic_joint_count;
        jt = km_tag_end;
        while (1) {
            jt = c3dae_find_start_tag(jt, km_end, "joint", &jt_tag_end);
            if (jt == C3DAE_XML_NIL) break;
            jt_end = c3dae_find_end_tag(jt_tag_end, km_end, "joint", &after_jt);
            if (jt_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
            if (model->kinematic_joint_count >= model->kinematic_joint_cap) return C3DAE_ERR_CAPACITY;
            j = &model->kinematic_joints[model->kinematic_joint_count];
            memset(j, 0, sizeof(*j));
            j->axis[0] = C3DAE_FIXED_ONE;
            j->limit_min = (c3dae_fixed)(-32767L << C3DAE_FIXED_SHIFT);
            j->limit_max = (c3dae_fixed)(32767L << C3DAE_FIXED_SHIFT);
            c3dae_attr_get(jt, jt_tag_end, "sid", j->sid, C3DAE_ID_MAX);
            c3dae_attr_get(jt, jt_tag_end, "name", j->name, C3DAE_ID_MAX);
            sub = c3dae_find_start_tag(jt_tag_end, jt_end, "revolute", &sub_tag_end);
            j->type = C3DAE_KIN_JOINT_REVOLUTE;
            if (sub == C3DAE_XML_NIL) { sub = c3dae_find_start_tag(jt_tag_end, jt_end, "prismatic", &sub_tag_end); j->type = C3DAE_KIN_JOINT_PRISMATIC; }
            if (sub != C3DAE_XML_NIL) {
                if (j->type == C3DAE_KIN_JOINT_REVOLUTE) sub_end = c3dae_find_end_tag(sub_tag_end, jt_end, "revolute", &after_sub);
                else sub_end = c3dae_find_end_tag(sub_tag_end, jt_end, "prismatic", &after_sub);
                if (sub_end != C3DAE_XML_NIL) {
                    axis = c3dae_find_start_tag(sub_tag_end, sub_end, "axis", &axis_tag_end);
                    if (axis != C3DAE_XML_NIL) { axis_end = c3dae_find_end_tag(axis_tag_end, sub_end, "axis", &after_axis); if (axis_end != C3DAE_XML_NIL) c3dae_parse_fixed_text3(axis_tag_end, axis_end, j->axis); }
                    lim = c3dae_find_start_tag(sub_tag_end, sub_end, "limits", &lim_tag_end);
                    if (lim != C3DAE_XML_NIL) {
                        lim_end = c3dae_find_end_tag(lim_tag_end, sub_end, "limits", &after_lim);
                        if (lim_end != C3DAE_XML_NIL) {
                            minv = c3dae_find_start_tag(lim_tag_end, lim_end, "min", &axis_tag_end);
                            if (minv != C3DAE_XML_NIL) { val_end = c3dae_find_end_tag(axis_tag_end, lim_end, "min", &after_axis); if (val_end != C3DAE_XML_NIL) { q = axis_tag_end; c3dae_parse_fixed_token(&q, val_end, &j->limit_min); } }
                            maxv = c3dae_find_start_tag(lim_tag_end, lim_end, "max", &axis_tag_end);
                            if (maxv != C3DAE_XML_NIL) { val_end = c3dae_find_end_tag(axis_tag_end, lim_end, "max", &after_axis); if (val_end != C3DAE_XML_NIL) { q = axis_tag_end; c3dae_parse_fixed_token(&q, val_end, &j->limit_max); } }
                        }
                    }
                }
            }
            model->kinematic_joint_count++;
            mk->joint_count++;
            jt = after_jt;
        }
        model->kinematic_model_count++;
        p = after_km;
    }
    return C3DAE_OK;
}

static int c3dae_parse_materials(c3dae_model *model, const char *xml, const char *end)
{
    const char *p;
    const char *mat;
    const char *mat_tag_end;
    const char *mat_end;
    const char *after_mat;
    const char *inst;
    const char *inst_end;
    c3dae_material *ma;
    char tmp[C3DAE_ID_MAX];
    if (model->material_cap == 0UL) return C3DAE_OK;
    p = xml;
    while (1) {
        mat = c3dae_find_start_tag(p, end, "material", &mat_tag_end);
        if (mat == C3DAE_XML_NIL) break;
        mat_end = c3dae_find_end_tag(mat_tag_end, end, "material", &after_mat);
        if (mat_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        if (model->material_count >= model->material_cap) return C3DAE_ERR_CAPACITY;
        ma = &model->materials[model->material_count];
        memset(ma, 0, sizeof(*ma));
        c3dae_attr_get(mat, mat_tag_end, "id", ma->id, C3DAE_ID_MAX);
        c3dae_attr_get(mat, mat_tag_end, "name", ma->name, C3DAE_ID_MAX);
        ma->effect_index = -1;
        ma->diffuse[0] = C3DAE_FIXED_ONE; ma->diffuse[1] = C3DAE_FIXED_ONE; ma->diffuse[2] = C3DAE_FIXED_ONE; ma->diffuse[3] = C3DAE_FIXED_ONE;
        inst = c3dae_find_start_tag(mat_tag_end, mat_end, "instance_effect", &inst_end);
        if (inst != C3DAE_XML_NIL) {
            tmp[0] = '\0';
            c3dae_attr_get(inst, inst_end, "url", tmp, sizeof(tmp));
            c3dae_ref_to_id(ma->effect, C3DAE_ID_MAX, tmp);
        }
        model->material_count++;
        p = after_mat;
    }
    return C3DAE_OK;
}


static int c3dae_parse_cameras(c3dae_model *model, const char *xml, const char *end)
{
    const char *p;
    const char *cam;
    const char *cam_tag_end;
    const char *cam_end;
    const char *after_cam;
    const char *opt;
    const char *opt_tag_end;
    const char *opt_end;
    const char *after_opt;
    c3dae_camera *ca;
    if (model->camera_cap == 0UL) return C3DAE_OK;
    p = xml;
    while (1) {
        cam = c3dae_find_start_tag(p, end, "camera", &cam_tag_end);
        if (cam == C3DAE_XML_NIL) break;
        cam_end = c3dae_find_end_tag(cam_tag_end, end, "camera", &after_cam);
        if (cam_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        if (model->camera_count >= model->camera_cap) return C3DAE_ERR_CAPACITY;
        ca = &model->cameras[model->camera_count];
        memset(ca, 0, sizeof(*ca));
        ca->kind = C3DAE_CAMERA_UNKNOWN;
        ca->znear = C3DAE_FIXED_ONE;
        ca->zfar = c3dae_fixed_from_int(1000L);
        ca->aspect_ratio = C3DAE_FIXED_ONE;
        c3dae_attr_get(cam, cam_tag_end, "id", ca->id, C3DAE_ID_MAX);
        c3dae_attr_get(cam, cam_tag_end, "name", ca->name, C3DAE_ID_MAX);
        opt = c3dae_find_start_tag(cam_tag_end, cam_end, "perspective", &opt_tag_end);
        if (opt != C3DAE_XML_NIL) {
            opt_end = c3dae_find_end_tag(opt_tag_end, cam_end, "perspective", &after_opt);
            if (opt_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
            ca->kind = C3DAE_CAMERA_PERSPECTIVE;
            c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "xfov", &ca->xfov);
            c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "yfov", &ca->yfov);
            c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "aspect_ratio", &ca->aspect_ratio);
            c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "znear", &ca->znear);
            c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "zfar", &ca->zfar);
        }
        opt = c3dae_find_start_tag(cam_tag_end, cam_end, "orthographic", &opt_tag_end);
        if (opt != C3DAE_XML_NIL) {
            opt_end = c3dae_find_end_tag(opt_tag_end, cam_end, "orthographic", &after_opt);
            if (opt_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
            ca->kind = C3DAE_CAMERA_ORTHOGRAPHIC;
            c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "xmag", &ca->xmag);
            c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "ymag", &ca->ymag);
            c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "aspect_ratio", &ca->aspect_ratio);
            c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "znear", &ca->znear);
            c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "zfar", &ca->zfar);
        }
        model->camera_count++;
        p = after_cam;
    }
    return C3DAE_OK;
}

static int c3dae_parse_lights(c3dae_model *model, const char *xml, const char *end)
{
    const char *p;
    const char *li;
    const char *li_tag_end;
    const char *li_end;
    const char *after_li;
    const char *opt;
    const char *opt_tag_end;
    const char *opt_end;
    const char *after_opt;
    c3dae_light *l;
    if (model->light_cap == 0UL) return C3DAE_OK;
    p = xml;
    while (1) {
        li = c3dae_find_start_tag(p, end, "light", &li_tag_end);
        if (li == C3DAE_XML_NIL) break;
        li_end = c3dae_find_end_tag(li_tag_end, end, "light", &after_li);
        if (li_end == C3DAE_XML_NIL) return C3DAE_ERR_XML;
        if (model->light_count >= model->light_cap) return C3DAE_ERR_CAPACITY;
        l = &model->lights[model->light_count];
        memset(l, 0, sizeof(*l));
        l->kind = C3DAE_LIGHT_UNKNOWN;
        l->color[0] = C3DAE_FIXED_ONE; l->color[1] = C3DAE_FIXED_ONE; l->color[2] = C3DAE_FIXED_ONE;
        l->constant_attenuation = C3DAE_FIXED_ONE;
        c3dae_attr_get(li, li_tag_end, "id", l->id, C3DAE_ID_MAX);
        c3dae_attr_get(li, li_tag_end, "name", l->name, C3DAE_ID_MAX);
        opt = c3dae_find_start_tag(li_tag_end, li_end, "ambient", &opt_tag_end);
        if (opt != C3DAE_XML_NIL) { opt_end = c3dae_find_end_tag(opt_tag_end, li_end, "ambient", &after_opt); if (opt_end == C3DAE_XML_NIL) return C3DAE_ERR_XML; l->kind = C3DAE_LIGHT_AMBIENT; c3dae_parse_vec3_text_child(opt_tag_end, opt_end, "color", l->color); }
        opt = c3dae_find_start_tag(li_tag_end, li_end, "directional", &opt_tag_end);
        if (opt != C3DAE_XML_NIL) { opt_end = c3dae_find_end_tag(opt_tag_end, li_end, "directional", &after_opt); if (opt_end == C3DAE_XML_NIL) return C3DAE_ERR_XML; l->kind = C3DAE_LIGHT_DIRECTIONAL; c3dae_parse_vec3_text_child(opt_tag_end, opt_end, "color", l->color); }
        opt = c3dae_find_start_tag(li_tag_end, li_end, "point", &opt_tag_end);
        if (opt != C3DAE_XML_NIL) { opt_end = c3dae_find_end_tag(opt_tag_end, li_end, "point", &after_opt); if (opt_end == C3DAE_XML_NIL) return C3DAE_ERR_XML; l->kind = C3DAE_LIGHT_POINT; c3dae_parse_vec3_text_child(opt_tag_end, opt_end, "color", l->color); c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "constant_attenuation", &l->constant_attenuation); c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "linear_attenuation", &l->linear_attenuation); c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "quadratic_attenuation", &l->quadratic_attenuation); }
        opt = c3dae_find_start_tag(li_tag_end, li_end, "spot", &opt_tag_end);
        if (opt != C3DAE_XML_NIL) { opt_end = c3dae_find_end_tag(opt_tag_end, li_end, "spot", &after_opt); if (opt_end == C3DAE_XML_NIL) return C3DAE_ERR_XML; l->kind = C3DAE_LIGHT_SPOT; c3dae_parse_vec3_text_child(opt_tag_end, opt_end, "color", l->color); c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "constant_attenuation", &l->constant_attenuation); c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "linear_attenuation", &l->linear_attenuation); c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "quadratic_attenuation", &l->quadratic_attenuation); c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "falloff_angle", &l->falloff_angle); c3dae_parse_scalar_text_child(opt_tag_end, opt_end, "falloff_exponent", &l->falloff_exponent); }
        model->light_count++;
        p = after_li;
    }
    return C3DAE_OK;
}

void c3dae_resolve_triangle_materials(c3dae_model *model)
{
    c3dae_u32 i;
    int idx;
    if (model == (c3dae_model *)0 || model->triangles == (c3dae_triangle *)0) return;
    i = 0UL;
    while (i < model->triangle_count) {
        model->triangles[i].material_index = -1;
        if (model->triangles[i].material_symbol[0] != '\0' && model->materials != (c3dae_material *)0) {
            idx = c3dae_find_material_by_id(model, model->triangles[i].material_symbol);
            model->triangles[i].material_index = idx;
        }
        i++;
    }
}

int c3dae_node_resolve_triangle_material(const c3dae_model *model, c3dae_u32 node_index, c3dae_u32 triangle_index)
{
    const c3dae_node *n;
    const c3dae_triangle *t;
    unsigned int i;
    if (model == (const c3dae_model *)0 || model->nodes == (const c3dae_node *)0 || model->triangles == (const c3dae_triangle *)0) return -1;
    if (node_index >= model->node_count || triangle_index >= model->triangle_count) return -1;
    n = &model->nodes[node_index];
    t = &model->triangles[triangle_index];
    i = 0U;
    while (i < n->material_bind_count && i < C3DAE_MAX_NODE_MATERIAL_BINDINGS) {
        if (strcmp(n->material_symbol[i], t->material_symbol) == 0) return n->material_index[i];
        i++;
    }
    return t->material_index;
}

void c3dae_context_init(c3dae_context *ctx)
{
    if (ctx == (c3dae_context *)0) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->error_code = C3DAE_OK;
}

void c3dae_model_init(c3dae_model *m,
                      c3dae_vertex *verts,
                      c3dae_u32 vert_cap,
                      c3dae_triangle *tris,
                      c3dae_u32 tri_cap,
                      c3dae_mesh *meshes,
                      c3dae_u32 mesh_cap)
{
    if (m == (c3dae_model *)0) return;
    memset(m, 0, sizeof(*m));
    m->vertices = verts;
    m->vertex_cap = vert_cap;
    m->triangles = tris;
    m->triangle_cap = tri_cap;
    m->meshes = meshes;
    m->mesh_cap = mesh_cap;
    m->unit_meter = C3DAE_FIXED_ONE;
    m->up_axis = C3DAE_UP_UNKNOWN;
}

void c3dae_model_bind_nodes(c3dae_model *m, c3dae_node *nodes, c3dae_u32 cap)
{
    if (m == (c3dae_model *)0) return;
    m->nodes = nodes;
    m->node_cap = cap;
    m->node_count = 0UL;
}

void c3dae_model_bind_skin(c3dae_model *m,
                           c3dae_controller *controllers,
                           c3dae_u32 controller_cap,
                           c3dae_skin_joint *joints,
                           c3dae_u32 joint_cap,
                           c3dae_skin_vertex *skin_vertices,
                           c3dae_u32 skin_vertex_cap)
{
    if (m == (c3dae_model *)0) return;
    m->controllers = controllers;
    m->controller_cap = controller_cap;
    m->controller_count = 0UL;
    m->skin_joints = joints;
    m->skin_joint_cap = joint_cap;
    m->skin_joint_count = 0UL;
    m->skin_vertices = skin_vertices;
    m->skin_vertex_cap = skin_vertex_cap;
    m->skin_vertex_count = 0UL;
}

void c3dae_model_bind_morphs(c3dae_model *m,
                             c3dae_morph_target *targets,
                             c3dae_u32 target_cap)
{
    if (m == (c3dae_model *)0) return;
    m->morph_targets = targets;
    m->morph_target_cap = target_cap;
    m->morph_target_count = 0UL;
}

void c3dae_model_bind_animations(c3dae_model *m,
                                 c3dae_anim_channel *channels,
                                 c3dae_u32 channel_cap,
                                 c3dae_anim_key *keys,
                                 c3dae_u32 key_cap)
{
    if (m == (c3dae_model *)0) return;
    m->anim_channels = channels;
    m->anim_channel_cap = channel_cap;
    m->anim_channel_count = 0UL;
    m->anim_keys = keys;
    m->anim_key_cap = key_cap;
    m->anim_key_count = 0UL;
}

void c3dae_model_bind_effects(c3dae_model *m,
                              c3dae_effect *effects,
                              c3dae_u32 effect_cap)
{
    if (m == (c3dae_model *)0) return;
    m->effects = effects;
    m->effect_cap = effect_cap;
    m->effect_count = 0UL;
}


void c3dae_model_bind_cameras_lights(c3dae_model *m,
                                     c3dae_camera *cameras,
                                     c3dae_u32 camera_cap,
                                     c3dae_light *lights,
                                     c3dae_u32 light_cap)
{
    if (m == (c3dae_model *)0) return;
    m->cameras = cameras;
    m->camera_cap = camera_cap;
    m->camera_count = 0UL;
    m->lights = lights;
    m->light_cap = light_cap;
    m->light_count = 0UL;
}

void c3dae_model_bind_physics(c3dae_model *m,
                              c3dae_physics_scene *scenes,
                              c3dae_u32 scene_cap,
                              c3dae_physics_model *models,
                              c3dae_u32 model_cap,
                              c3dae_rigid_body *bodies,
                              c3dae_u32 body_cap)
{
    if (m == (c3dae_model *)0) return;
    m->physics_scenes = scenes;
    m->physics_scene_cap = scene_cap;
    m->physics_scene_count = 0UL;
    m->physics_models = models;
    m->physics_model_cap = model_cap;
    m->physics_model_count = 0UL;
    m->rigid_bodies = bodies;
    m->rigid_body_cap = body_cap;
    m->rigid_body_count = 0UL;
}

void c3dae_model_bind_kinematics(c3dae_model *m,
                                 c3dae_kinematic_model *models,
                                 c3dae_u32 model_cap,
                                 c3dae_kinematic_joint *joints,
                                 c3dae_u32 joint_cap)
{
    if (m == (c3dae_model *)0) return;
    m->kinematic_models = models;
    m->kinematic_model_cap = model_cap;
    m->kinematic_model_count = 0UL;
    m->kinematic_joints = joints;
    m->kinematic_joint_cap = joint_cap;
    m->kinematic_joint_count = 0UL;
}

void c3dae_model_bind_materials(c3dae_model *m,
                                c3dae_material *materials,
                                c3dae_u32 material_cap,
                                c3dae_image *images,
                                c3dae_u32 image_cap)
{
    if (m == (c3dae_model *)0) return;
    m->materials = materials;
    m->material_cap = material_cap;
    m->material_count = 0UL;
    m->images = images;
    m->image_cap = image_cap;
    m->image_count = 0UL;
}

int c3dae_load_memory(c3dae_context *ctx, const char *xml, c3dae_u32 xml_len, c3dae_model *out_model)
{
    const char *end;
    const char *p;
    const char *geom;
    const char *geom_tag_end;
    const char *geom_end;
    const char *after_geom;
    int rc;
    if (ctx == (c3dae_context *)0 || xml == (const char *)0 || out_model == (c3dae_model *)0) return C3DAE_ERR_NULL;
    c3dae_set_error(ctx, C3DAE_OK, "ok");
    end = xml + xml_len;
    c3dae_parse_asset(out_model, xml, end);
    p = xml;
    while (1) {
        geom = c3dae_find_start_tag(p, end, "geometry", &geom_tag_end);
        if (geom == C3DAE_XML_NIL) break;
        geom_end = c3dae_find_end_tag(geom_tag_end, end, "geometry", &after_geom);
        if (geom_end == C3DAE_XML_NIL) { c3dae_set_error(ctx, C3DAE_ERR_XML, "geometry end tag missing"); return C3DAE_ERR_XML; }
        rc = c3dae_parse_geometry(ctx, out_model, geom, geom_tag_end, geom_end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
        p = after_geom;
    }
    if (out_model->controllers != (c3dae_controller *)0) {
        rc = c3dae_parse_controllers(ctx, out_model, xml, end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
        c3dae_apply_skins_to_emitted_vertices(out_model);
    }
    if (out_model->nodes != (c3dae_node *)0) {
        rc = c3dae_parse_visual_scenes(out_model, xml, end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
    }
    if (out_model->anim_channels != (c3dae_anim_channel *)0) {
        rc = c3dae_parse_animations(ctx, out_model, xml, end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
    }
    if (out_model->images != (c3dae_image *)0) {
        rc = c3dae_parse_images(out_model, xml, end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
    }
    if (out_model->effects != (c3dae_effect *)0) {
        rc = c3dae_parse_effects(out_model, xml, end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
    }
    if (out_model->materials != (c3dae_material *)0) {
        rc = c3dae_parse_materials(out_model, xml, end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
        if (out_model->effects != (c3dae_effect *)0) c3dae_resolve_material_effects(out_model);
        c3dae_resolve_triangle_materials(out_model);
    }
    if (out_model->cameras != (c3dae_camera *)0) {
        rc = c3dae_parse_cameras(out_model, xml, end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
    }
    if (out_model->lights != (c3dae_light *)0) {
        rc = c3dae_parse_lights(out_model, xml, end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
    }
    if (out_model->physics_scenes != (c3dae_physics_scene *)0) {
        rc = c3dae_parse_physics_scenes(out_model, xml, end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
    }
    if (out_model->physics_models != (c3dae_physics_model *)0) {
        rc = c3dae_parse_physics_models(out_model, xml, end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
    }
    if (out_model->kinematic_models != (c3dae_kinematic_model *)0) {
        rc = c3dae_parse_kinematics(out_model, xml, end);
        if (rc != C3DAE_OK) { c3dae_set_error(ctx, rc, c3dae_error_string(rc)); return rc; }
    }
    c3dae_resolve_nodes_and_joints(out_model);
    c3dae_build_node_world_matrices(out_model);
    return C3DAE_OK;
}

int c3dae_animation_eval_channel(const c3dae_model *model,
                                 c3dae_u32 channel_index,
                                 c3dae_fixed time,
                                 c3dae_fixed *out_values,
                                 unsigned int out_cap,
                                 unsigned int *out_count)
{
    const c3dae_anim_channel *ch;
    const c3dae_anim_key *k0;
    const c3dae_anim_key *k1;
    c3dae_u32 i;
    unsigned int j;
    c3dae_fixed span;
    c3dae_fixed t;
    if (out_count != (unsigned int *)0) *out_count = 0U;
    if (model == (const c3dae_model *)0 || out_values == (c3dae_fixed *)0) return C3DAE_ERR_NULL;
    if (channel_index >= model->anim_channel_count) return C3DAE_ERR_NOT_FOUND;
    ch = &model->anim_channels[channel_index];
    if (ch->key_count == 0UL) return C3DAE_ERR_NOT_FOUND;
    k0 = &model->anim_keys[ch->first_key];
    if (time <= k0->time || ch->key_count == 1UL) {
        j = 0U;
        while (j < k0->value_count && j < out_cap) { out_values[j] = k0->value[j]; j++; }
        if (out_count != (unsigned int *)0) *out_count = j;
        return C3DAE_OK;
    }
    i = 0UL;
    while (i + 1UL < ch->key_count) {
        k0 = &model->anim_keys[ch->first_key + i];
        k1 = &model->anim_keys[ch->first_key + i + 1UL];
        if (time <= k1->time) {
            j = 0U;
            if (ch->interpolation == C3DAE_INTERP_STEP) {
                while (j < k0->value_count && j < out_cap) { out_values[j] = k0->value[j]; j++; }
            } else {
                span = k1->time - k0->time;
                t = c3dae_fixed_div_safe(time - k0->time, span);
                while (j < k0->value_count && j < k1->value_count && j < out_cap) {
                    if (ch->interpolation == C3DAE_INTERP_BEZIER) out_values[j] = c3dae_fixed_bezier(k0->value[j], k0->out_tangent[j], k1->in_tangent[j], k1->value[j], t);
                    else out_values[j] = c3dae_fixed_lerp(k0->value[j], k1->value[j], t);
                    j++;
                }
            }
            if (out_count != (unsigned int *)0) *out_count = j;
            return C3DAE_OK;
        }
        i++;
    }
    k0 = &model->anim_keys[ch->first_key + ch->key_count - 1UL];
    j = 0U;
    while (j < k0->value_count && j < out_cap) { out_values[j] = k0->value[j]; j++; }
    if (out_count != (unsigned int *)0) *out_count = j;
    return C3DAE_OK;
}


int c3dae_apply_animations(c3dae_model *model, c3dae_fixed time)
{
    c3dae_u32 i;
    c3dae_anim_channel *ch;
    c3dae_node *n;
    c3dae_fixed vals[16];
    c3dae_fixed tmp[16];
    unsigned int count;
    int rc;
    if (model == (c3dae_model *)0) return C3DAE_ERR_NULL;
    if (model->nodes == (c3dae_node *)0) return C3DAE_OK;
    i = 0UL;
    while (i < model->node_count) {
        c3dae_matrix_copy(model->nodes[i].local_matrix, model->nodes[i].base_local_matrix);
        i++;
    }
    i = 0UL;
    while (i < model->anim_channel_count) {
        ch = &model->anim_channels[i];
        if (ch->target_node_index >= 0 && (c3dae_u32)ch->target_node_index < model->node_count) {
            n = &model->nodes[ch->target_node_index];
            count = 0U;
            rc = c3dae_animation_eval_channel(model, i, time, vals, 16U, &count);
            if (rc != C3DAE_OK) return rc;
            if (ch->target_kind == C3DAE_ANIM_TARGET_MATRIX && count >= 16U) {
                c3dae_matrix_copy(n->local_matrix, vals);
            } else if (ch->target_kind == C3DAE_ANIM_TARGET_TRANSLATE && count >= 3U) {
                c3dae_matrix_make_translate(tmp, vals[0], vals[1], vals[2]);
                c3dae_matrix_mul(n->local_matrix, n->local_matrix, tmp);
            } else if (ch->target_kind == C3DAE_ANIM_TARGET_ROTATE && count >= 4U) {
                c3dae_matrix_from_rotate(tmp, vals[0], vals[1], vals[2], vals[3]);
                c3dae_matrix_mul(n->local_matrix, n->local_matrix, tmp);
            } else if (ch->target_kind == C3DAE_ANIM_TARGET_SCALE && count >= 3U) {
                c3dae_matrix_make_scale(tmp, vals[0], vals[1], vals[2]);
                c3dae_matrix_mul(n->local_matrix, n->local_matrix, tmp);
            }
        }
        i++;
    }
    c3dae_build_node_world_matrices(model);
    return C3DAE_OK;
}

int c3dae_build_skin_palette(const c3dae_model *model,
                             c3dae_u32 controller_index,
                             c3dae_fixed *out_matrices,
                             c3dae_u32 matrix_cap)
{
    const c3dae_controller *ctl;
    const c3dae_skin_joint *sj;
    c3dae_fixed tmp[16];
    c3dae_fixed tmp2[16];
    c3dae_u32 i;
    c3dae_u32 out_pos;
    if (model == (const c3dae_model *)0 || out_matrices == (c3dae_fixed *)0) return C3DAE_ERR_NULL;
    if (controller_index >= model->controller_count) return C3DAE_ERR_NOT_FOUND;
    ctl = &model->controllers[controller_index];
    if (ctl->kind != C3DAE_CONTROLLER_SKIN) return C3DAE_ERR_UNSUPPORTED;
    if (matrix_cap < ctl->joint_count) return C3DAE_ERR_CAPACITY;
    i = 0UL;
    while (i < ctl->joint_count) {
        sj = &model->skin_joints[ctl->first_joint + i];
        if (sj->node_index >= 0 && (c3dae_u32)sj->node_index < model->node_count) {
            c3dae_matrix_mul(tmp, model->nodes[sj->node_index].world_matrix, sj->inv_bind_matrix);
            c3dae_matrix_mul(tmp2, tmp, ctl->bind_shape_matrix);
            c3dae_matrix_copy(&out_matrices[i * 16UL], tmp2);
        } else {
            c3dae_matrix_identity(&out_matrices[i * 16UL]);
        }
        out_pos = i * 16UL;
        (void)out_pos;
        i++;
    }
    return C3DAE_OK;
}

c3dae_fixed c3dae_fixed_from_int(c3dae_s32 v)
{
    return (c3dae_fixed)(v << C3DAE_FIXED_SHIFT);
}

c3dae_s32 c3dae_fixed_to_int(c3dae_fixed v)
{
    return (c3dae_s32)(v >> C3DAE_FIXED_SHIFT);
}

c3dae_s32 c3dae_fixed_to_milli(c3dae_fixed v)
{
    c3dae_s32 sign;
    c3dae_s32 av;
    sign = 1L;
    av = v;
    if (av < 0L) { sign = -1L; av = -av; }
    return sign * (c3dae_s32)((av * 1000L) >> C3DAE_FIXED_SHIFT);
}

const char *c3dae_error_string(int code)
{
    if (code == C3DAE_OK) return "ok";
    if (code == C3DAE_ERR_NULL) return "null pointer";
    if (code == C3DAE_ERR_CAPACITY) return "caller buffer capacity exceeded";
    if (code == C3DAE_ERR_XML) return "invalid or unsupported XML structure";
    if (code == C3DAE_ERR_UNSUPPORTED) return "unsupported COLLADA feature in this importer";
    if (code == C3DAE_ERR_BAD_NUMBER) return "bad numeric token";
    if (code == C3DAE_ERR_NOT_FOUND) return "referenced source not found";
    return "unknown error";
}
