#include "c3dae_v9.h"
#include <string.h>

#define V9_NIL ((const char *)0)

static unsigned long v9_strlen(const char *s)
{
    unsigned long n;
    n = 0UL;
    if (s == (const char *)0) return 0UL;
    while (s[n] != '\0') n++;
    return n;
}

static int v9_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int v9_is_name_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ':' || c == '.';
}

static int v9_streq(const char *a, const char *b)
{
    if (a == (const char *)0 || b == (const char *)0) return 0;
    return strcmp(a, b) == 0;
}

static void v9_trim_bounds(const char **a, const char **b)
{
    while (*a < *b && v9_is_space(**a)) (*a)++;
    while (*b > *a && v9_is_space((*b)[-1])) (*b)--;
}

static void v9_copy(char *dst, unsigned long cap, const char *a, const char *b)
{
    unsigned long n;
    if (cap == 0UL) return;
    if (a == V9_NIL || b == V9_NIL || b < a) {
        dst[0] = '\0';
        return;
    }
    v9_trim_bounds(&a, &b);
    n = (unsigned long)(b - a);
    if (n >= cap) n = cap - 1UL;
    if (n > 0UL) memcpy(dst, a, (size_t)n);
    dst[n] = '\0';
}

static int v9_name_eq(const char *a, const char *b, const char *name)
{
    const char *colon;
    const char *p;
    unsigned long n;
    unsigned long m;
    colon = V9_NIL;
    p = a;
    while (p < b) {
        if (*p == ':') colon = p;
        p++;
    }
    if (colon != V9_NIL) a = colon + 1;
    n = (unsigned long)(b - a);
    m = v9_strlen(name);
    if (n != m) return 0;
    return memcmp(a, name, (size_t)n) == 0;
}

static int v9_tag_name(const char *tag, const char *end, char *out, unsigned long cap)
{
    const char *a;
    const char *b;
    const char *p;
    const char *colon;
    if (cap == 0UL || tag >= end || *tag != '<') return 0;
    out[0] = '\0';
    a = tag + 1;
    if (a < end && *a == '/') a++;
    if (a < end && (*a == '?' || *a == '!')) return 0;
    b = a;
    while (b < end && v9_is_name_char(*b)) b++;
    colon = V9_NIL;
    p = a;
    while (p < b) {
        if (*p == ':') colon = p;
        p++;
    }
    if (colon != V9_NIL) a = colon + 1;
    v9_copy(out, cap, a, b);
    return out[0] != '\0';
}

static int v9_tag_local_matches(const char *p, const char *end, const char *name)
{
    const char *a;
    const char *b;
    if (p >= end || *p != '<') return 0;
    a = p + 1;
    if (a < end && *a == '/') a++;
    if (a < end && (*a == '?' || *a == '!')) return 0;
    b = a;
    while (b < end && v9_is_name_char(*b)) b++;
    return v9_name_eq(a, b, name);
}

static const char *v9_find_gt(const char *p, const char *end)
{
    int q;
    char quote;
    q = 0;
    quote = '\0';
    while (p < end) {
        if (q) {
            if (*p == quote) q = 0;
        } else {
            if (*p == '\'' || *p == '"') { q = 1; quote = *p; }
            else if (*p == '>') return p + 1;
        }
        p++;
    }
    return V9_NIL;
}

static int v9_is_self_close(const char *tag, const char *tag_end)
{
    const char *p;
    p = tag_end;
    while (p > tag) {
        p--;
        if (*p == '>' || v9_is_space(*p)) continue;
        return *p == '/';
    }
    return 0;
}

static const char *v9_find_start(const char *p, const char *end, const char *name, const char **tag_end)
{
    const char *gt;
    while (p < end) {
        while (p < end && *p != '<') p++;
        if (p >= end) break;
        if (p + 1 < end && p[1] == '/') { p++; continue; }
        if (v9_tag_local_matches(p, end, name)) {
            gt = v9_find_gt(p, end);
            if (gt == V9_NIL) return V9_NIL;
            if (tag_end != (const char **)0) *tag_end = gt;
            return p;
        }
        p++;
    }
    return V9_NIL;
}

static const char *v9_find_matching_end(const char *content_start,
                                        const char *end,
                                        const char *name,
                                        const char **after)
{
    const char *p;
    const char *gt;
    int depth;
    p = content_start;
    depth = 1;
    while (p < end) {
        while (p < end && *p != '<') p++;
        if (p >= end) break;
        if (p + 1 < end && p[1] == '!' ) { p++; continue; }
        if (p + 1 < end && p[1] == '?' ) { p++; continue; }
        if (p + 1 < end && p[1] == '/') {
            if (v9_tag_local_matches(p, end, name)) {
                gt = v9_find_gt(p, end);
                if (gt == V9_NIL) return V9_NIL;
                depth--;
                if (depth == 0) {
                    if (after != (const char **)0) *after = gt;
                    return p;
                }
                p = gt;
                continue;
            }
        } else {
            if (v9_tag_local_matches(p, end, name)) {
                gt = v9_find_gt(p, end);
                if (gt == V9_NIL) return V9_NIL;
                if (!v9_is_self_close(p, gt)) depth++;
                p = gt;
                continue;
            }
        }
        p++;
    }
    return V9_NIL;
}

typedef struct v9_elem_tag {
    const char *tag;
    const char *tag_end;
    const char *content;
    const char *content_end;
    const char *after;
    char name[C3DAE_V9_NAME_MAX];
    int self_close;
} v9_elem;

static int v9_next_child(const char *p, const char *end, v9_elem *el)
{
    const char *gt;
    const char *close;
    const char *after;
    if (el == (v9_elem *)0) return 0;
    memset(el, 0, sizeof(*el));
    while (p < end) {
        while (p < end && *p != '<') p++;
        if (p >= end) return 0;
        if (p + 1 < end && (p[1] == '/' || p[1] == '!' || p[1] == '?')) { p++; continue; }
        gt = v9_find_gt(p, end);
        if (gt == V9_NIL) return 0;
        if (!v9_tag_name(p, gt, el->name, sizeof(el->name))) { p = gt; continue; }
        el->tag = p;
        el->tag_end = gt;
        el->content = gt;
        el->self_close = v9_is_self_close(p, gt);
        if (el->self_close) {
            el->content_end = gt;
            el->after = gt;
            return 1;
        }
        close = v9_find_matching_end(gt, end, el->name, &after);
        if (close == V9_NIL) return 0;
        el->content_end = close;
        el->after = after;
        return 1;
    }
    return 0;
}

static int v9_attr_get(const char *tag, const char *tag_end, const char *attr, char *out, unsigned long cap)
{
    const char *p;
    unsigned long alen;
    if (cap == 0UL) return 0;
    out[0] = '\0';
    alen = v9_strlen(attr);
    p = tag;
    while (p < tag_end) {
        while (p < tag_end && (v9_is_space(*p) || *p == '<' || *p == '/' || *p == '>')) p++;
        if (p + alen < tag_end && memcmp(p, attr, (size_t)alen) == 0) {
            const char *q;
            q = p + alen;
            if (q < tag_end && (*q == '=' || v9_is_space(*q))) {
                while (q < tag_end && v9_is_space(*q)) q++;
                if (q < tag_end && *q == '=') {
                    q++;
                    while (q < tag_end && v9_is_space(*q)) q++;
                    if (q < tag_end && (*q == '"' || *q == '\'')) {
                        char quote;
                        const char *a;
                        quote = *q;
                        q++;
                        a = q;
                        while (q < tag_end && *q != quote) q++;
                        v9_copy(out, cap, a, q);
                        return 1;
                    }
                }
            }
        }
        while (p < tag_end && !v9_is_space(*p) && *p != '>') p++;
    }
    return 0;
}

static c3dae_fixed v9_parse_fixed_range(const char *a, const char *b)
{
    long sign;
    long ip;
    long fp;
    long scale;
    int digits;
    sign = 1L;
    ip = 0L;
    fp = 0L;
    scale = 1L;
    digits = 0;
    v9_trim_bounds(&a, &b);
    if (a < b && *a == '-') { sign = -1L; a++; }
    else if (a < b && *a == '+') a++;
    while (a < b && *a >= '0' && *a <= '9') {
        ip = ip * 10L + (long)(*a - '0');
        a++;
    }
    if (a < b && *a == '.') {
        a++;
        while (a < b && *a >= '0' && *a <= '9' && digits < 6) {
            fp = fp * 10L + (long)(*a - '0');
            scale *= 10L;
            digits++;
            a++;
        }
    }
    return (c3dae_fixed)(sign * ((ip << C3DAE_FIXED_SHIFT) + ((fp << C3DAE_FIXED_SHIFT) / scale)));
}

static c3dae_u32 v9_count_start_tags(const char *a, const char *b, const char *name)
{
    c3dae_u32 n;
    const char *p;
    n = 0UL;
    p = a;
    while (p < b) {
        while (p < b && *p != '<') p++;
        if (p >= b) break;
        if (p + 1 < b && p[1] != '/' && v9_tag_local_matches(p, b, name)) n++;
        p++;
    }
    return n;
}

static c3dae_u32 v9_parse_first_uint_element(const char *a, const char *b, const char *name)
{
    const char *tag_end;
    const char *s;
    const char *e;
    c3dae_fixed f;
    s = v9_find_start(a, b, name, &tag_end);
    if (s == V9_NIL) return 0UL;
    e = v9_find_matching_end(tag_end, b, name, (const char **)0);
    if (e == V9_NIL) return 0UL;
    f = v9_parse_fixed_range(tag_end, e);
    if (f < 0) return 0UL;
    return (c3dae_u32)(f >> C3DAE_FIXED_SHIFT);
}

static const char *v9_strnstr_local(const char *hay, const char *end, const char *needle)
{
    unsigned long nlen;
    nlen = v9_strlen(needle);
    if (nlen == 0UL) return hay;
    while (hay + nlen <= end) {
        if (memcmp(hay, needle, (size_t)nlen) == 0) return hay;
        hay++;
    }
    return V9_NIL;
}

static int v9_word_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static c3dae_u32 v9_count_word(const char *a, const char *b, const char *word)
{
    c3dae_u32 count;
    const char *p;
    unsigned long n;
    count = 0UL;
    p = a;
    n = v9_strlen(word);
    while (p + n <= b) {
        if (memcmp(p, word, (size_t)n) == 0) {
            int left;
            int right;
            left = (p == a) || !v9_word_char(p[-1]);
            right = (p + n >= b) || !v9_word_char(p[n]);
            if (left && right) count++;
            p += n;
        } else {
            p++;
        }
    }
    return count;
}

c3dae_fixed c3dae_v9_fixed_mul(c3dae_fixed a, c3dae_fixed b)
{
    return (c3dae_fixed)((a >> 8) * (b >> 8));
}

c3dae_fixed c3dae_v9_fixed_div(c3dae_fixed a, c3dae_fixed b)
{
    long sign;
    unsigned long aa;
    unsigned long bb;
    unsigned long q;
    if (b == 0L) return 0L;
    sign = 1L;
    if (a < 0L) { sign = -sign; aa = (unsigned long)(-a); }
    else aa = (unsigned long)a;
    if (b < 0L) { sign = -sign; bb = (unsigned long)(-b); }
    else bb = (unsigned long)b;
    if (aa <= 65535UL) q = (aa << C3DAE_FIXED_SHIFT) / bb;
    else q = (aa / bb) << C3DAE_FIXED_SHIFT;
    return (c3dae_fixed)(sign < 0L ? -(long)q : (long)q);
}

static c3dae_fixed v9_weight_at(const c3dae_fixed *weights, c3dae_u32 i)
{
    if (weights == (const c3dae_fixed *)0) return C3DAE_FIXED_ONE;
    return weights[i];
}

static c3dae_fixed v9_nurbs_basis(const c3dae_fixed *knots,
                                  c3dae_u32 knot_count,
                                  c3dae_u32 i,
                                  c3dae_u32 p,
                                  c3dae_fixed u)
{
    c3dae_fixed left;
    c3dae_fixed right;
    c3dae_fixed den;
    c3dae_fixed term;
    if (knots == (const c3dae_fixed *)0 || i + p + 1UL >= knot_count) return 0L;
    if (p == 0UL) {
        if ((knots[i] <= u && u < knots[i + 1UL]) ||
            (u == knots[knot_count - 1UL] && knots[i] <= u && u == knots[i + 1UL])) return C3DAE_FIXED_ONE;
        return 0L;
    }
    left = 0L;
    right = 0L;
    den = (c3dae_fixed)(knots[i + p] - knots[i]);
    if (den != 0L) {
        term = c3dae_v9_fixed_div((c3dae_fixed)(u - knots[i]), den);
        left = c3dae_v9_fixed_mul(term, v9_nurbs_basis(knots, knot_count, i, p - 1UL, u));
    }
    den = (c3dae_fixed)(knots[i + p + 1UL] - knots[i + 1UL]);
    if (den != 0L) {
        term = c3dae_v9_fixed_div((c3dae_fixed)(knots[i + p + 1UL] - u), den);
        right = c3dae_v9_fixed_mul(term, v9_nurbs_basis(knots, knot_count, i + 1UL, p - 1UL, u));
    }
    return (c3dae_fixed)(left + right);
}

int c3dae_v9_nurbs_eval_curve(const c3dae_v9_nurbs_curve *curve, c3dae_fixed u, c3dae_v9_vec3 *out_point)
{
    c3dae_u32 i;
    c3dae_fixed b;
    c3dae_fixed w;
    c3dae_fixed bw;
    c3dae_fixed sx;
    c3dae_fixed sy;
    c3dae_fixed sz;
    c3dae_fixed sw;
    if (curve == (const c3dae_v9_nurbs_curve *)0 || out_point == (c3dae_v9_vec3 *)0) return C3DAE_V9_ERR_NULL;
    if (curve->control_points == (const c3dae_v9_vec3 *)0 || curve->knots == (const c3dae_fixed *)0) return C3DAE_V9_ERR_NULL;
    if (curve->degree == 0UL || curve->control_count == 0UL || curve->knot_count < curve->control_count + curve->degree + 1UL) return C3DAE_V9_ERR_BAD_NURBS;
    sx = 0L; sy = 0L; sz = 0L; sw = 0L;
    for (i = 0UL; i < curve->control_count; i++) {
        b = v9_nurbs_basis(curve->knots, curve->knot_count, i, curve->degree, u);
        w = v9_weight_at(curve->weights, i);
        bw = c3dae_v9_fixed_mul(b, w);
        sx = (c3dae_fixed)(sx + c3dae_v9_fixed_mul(curve->control_points[i].x, bw));
        sy = (c3dae_fixed)(sy + c3dae_v9_fixed_mul(curve->control_points[i].y, bw));
        sz = (c3dae_fixed)(sz + c3dae_v9_fixed_mul(curve->control_points[i].z, bw));
        sw = (c3dae_fixed)(sw + bw);
    }
    if (sw == 0L) return C3DAE_V9_ERR_BAD_NURBS;
    out_point->x = c3dae_v9_fixed_div(sx, sw);
    out_point->y = c3dae_v9_fixed_div(sy, sw);
    out_point->z = c3dae_v9_fixed_div(sz, sw);
    return C3DAE_V9_OK;
}

int c3dae_v9_nurbs_tessellate_curve(const c3dae_v9_nurbs_curve *curve,
                                    c3dae_v9_vec3 *out_points,
                                    c3dae_u32 out_cap,
                                    c3dae_u32 steps,
                                    c3dae_u32 *out_count)
{
    c3dae_u32 i;
    c3dae_fixed u0;
    c3dae_fixed u1;
    c3dae_fixed t;
    c3dae_fixed u;
    int rc;
    if (out_count != (c3dae_u32 *)0) *out_count = 0UL;
    if (curve == (const c3dae_v9_nurbs_curve *)0 || out_points == (c3dae_v9_vec3 *)0) return C3DAE_V9_ERR_NULL;
    if (steps < 1UL) steps = 1UL;
    if (out_cap < steps + 1UL) return C3DAE_V9_ERR_CAPACITY;
    if (curve->knot_count <= curve->degree * 2UL + 1UL) return C3DAE_V9_ERR_BAD_NURBS;
    u0 = curve->knots[curve->degree];
    u1 = curve->knots[curve->knot_count - curve->degree - 1UL];
    for (i = 0UL; i <= steps; i++) {
        t = c3dae_v9_fixed_div((c3dae_fixed)i, (c3dae_fixed)steps);
        u = (c3dae_fixed)(u0 + c3dae_v9_fixed_mul((c3dae_fixed)(u1 - u0), t));
        if (i == steps) u = u1;
        rc = c3dae_v9_nurbs_eval_curve(curve, u, &out_points[i]);
        if (rc != C3DAE_V9_OK) return rc;
    }
    if (out_count != (c3dae_u32 *)0) *out_count = steps + 1UL;
    return C3DAE_V9_OK;
}

int c3dae_v9_nurbs_eval_surface(const c3dae_v9_nurbs_surface *surface,
                                c3dae_fixed u,
                                c3dae_fixed v,
                                c3dae_v9_vec3 *out_point)
{
    c3dae_u32 iu;
    c3dae_u32 iv;
    c3dae_u32 idx;
    c3dae_fixed bu;
    c3dae_fixed bv;
    c3dae_fixed b;
    c3dae_fixed w;
    c3dae_fixed bw;
    c3dae_fixed sx;
    c3dae_fixed sy;
    c3dae_fixed sz;
    c3dae_fixed sw;
    if (surface == (const c3dae_v9_nurbs_surface *)0 || out_point == (c3dae_v9_vec3 *)0) return C3DAE_V9_ERR_NULL;
    if (surface->control_points == (const c3dae_v9_vec3 *)0 || surface->knots_u == (const c3dae_fixed *)0 || surface->knots_v == (const c3dae_fixed *)0) return C3DAE_V9_ERR_NULL;
    if (surface->control_count_u == 0UL || surface->control_count_v == 0UL) return C3DAE_V9_ERR_BAD_NURBS;
    sx = 0L; sy = 0L; sz = 0L; sw = 0L;
    for (iv = 0UL; iv < surface->control_count_v; iv++) {
        bv = v9_nurbs_basis(surface->knots_v, surface->knot_count_v, iv, surface->degree_v, v);
        for (iu = 0UL; iu < surface->control_count_u; iu++) {
            bu = v9_nurbs_basis(surface->knots_u, surface->knot_count_u, iu, surface->degree_u, u);
            b = c3dae_v9_fixed_mul(bu, bv);
            idx = iv * surface->control_count_u + iu;
            w = v9_weight_at(surface->weights, idx);
            bw = c3dae_v9_fixed_mul(b, w);
            sx = (c3dae_fixed)(sx + c3dae_v9_fixed_mul(surface->control_points[idx].x, bw));
            sy = (c3dae_fixed)(sy + c3dae_v9_fixed_mul(surface->control_points[idx].y, bw));
            sz = (c3dae_fixed)(sz + c3dae_v9_fixed_mul(surface->control_points[idx].z, bw));
            sw = (c3dae_fixed)(sw + bw);
        }
    }
    if (sw == 0L) return C3DAE_V9_ERR_BAD_NURBS;
    out_point->x = c3dae_v9_fixed_div(sx, sw);
    out_point->y = c3dae_v9_fixed_div(sy, sw);
    out_point->z = c3dae_v9_fixed_div(sz, sw);
    return C3DAE_V9_OK;
}

int c3dae_v9_nurbs_tessellate_surface(const c3dae_v9_nurbs_surface *surface,
                                      c3dae_v9_vec3 *out_points,
                                      c3dae_u32 out_cap,
                                      c3dae_u32 steps_u,
                                      c3dae_u32 steps_v,
                                      c3dae_u32 *out_count)
{
    c3dae_u32 u_i;
    c3dae_u32 v_i;
    c3dae_u32 idx;
    c3dae_fixed u0;
    c3dae_fixed u1;
    c3dae_fixed v0;
    c3dae_fixed v1;
    c3dae_fixed tu;
    c3dae_fixed tv;
    c3dae_fixed u;
    c3dae_fixed v;
    int rc;
    if (out_count != (c3dae_u32 *)0) *out_count = 0UL;
    if (surface == (const c3dae_v9_nurbs_surface *)0 || out_points == (c3dae_v9_vec3 *)0) return C3DAE_V9_ERR_NULL;
    if (steps_u < 1UL) steps_u = 1UL;
    if (steps_v < 1UL) steps_v = 1UL;
    if (out_cap < (steps_u + 1UL) * (steps_v + 1UL)) return C3DAE_V9_ERR_CAPACITY;
    if (surface->knot_count_u <= surface->degree_u * 2UL + 1UL || surface->knot_count_v <= surface->degree_v * 2UL + 1UL) return C3DAE_V9_ERR_BAD_NURBS;
    u0 = surface->knots_u[surface->degree_u];
    u1 = surface->knots_u[surface->knot_count_u - surface->degree_u - 1UL];
    v0 = surface->knots_v[surface->degree_v];
    v1 = surface->knots_v[surface->knot_count_v - surface->degree_v - 1UL];
    idx = 0UL;
    for (v_i = 0UL; v_i <= steps_v; v_i++) {
        tv = c3dae_v9_fixed_div((c3dae_fixed)v_i, (c3dae_fixed)steps_v);
        v = (c3dae_fixed)(v0 + c3dae_v9_fixed_mul((c3dae_fixed)(v1 - v0), tv));
        if (v_i == steps_v) v = v1;
        for (u_i = 0UL; u_i <= steps_u; u_i++) {
            tu = c3dae_v9_fixed_div((c3dae_fixed)u_i, (c3dae_fixed)steps_u);
            u = (c3dae_fixed)(u0 + c3dae_v9_fixed_mul((c3dae_fixed)(u1 - u0), tu));
            if (u_i == steps_u) u = u1;
            rc = c3dae_v9_nurbs_eval_surface(surface, u, v, &out_points[idx]);
            if (rc != C3DAE_V9_OK) return rc;
            idx++;
        }
    }
    if (out_count != (c3dae_u32 *)0) *out_count = idx;
    return C3DAE_V9_OK;
}

void c3dae_v9_math_program_init(c3dae_v9_math_program *program,
                                c3dae_v9_math_op *ops,
                                c3dae_u32 op_cap)
{
    if (program == (c3dae_v9_math_program *)0) return;
    memset(program, 0, sizeof(*program));
    program->ops = ops;
    program->op_cap = op_cap;
}

static int v9_math_emit(c3dae_v9_math_program *p, int op, c3dae_fixed value, const char *name_a, const char *name_b)
{
    c3dae_v9_math_op *dst;
    if (p == (c3dae_v9_math_program *)0 || p->ops == (c3dae_v9_math_op *)0) return C3DAE_V9_ERR_NULL;
    if (p->op_count >= p->op_cap) return C3DAE_V9_ERR_CAPACITY;
    dst = &p->ops[p->op_count++];
    memset(dst, 0, sizeof(*dst));
    dst->opcode = op;
    dst->value = value;
    if (name_a != V9_NIL && name_b != V9_NIL) v9_copy(dst->name, sizeof(dst->name), name_a, name_b);
    return C3DAE_V9_OK;
}

static int v9_math_op_name_to_code(const char *name)
{
    if (v9_streq(name, "plus")) return C3DAE_V9_MATH_ADD;
    if (v9_streq(name, "minus")) return C3DAE_V9_MATH_SUB;
    if (v9_streq(name, "times")) return C3DAE_V9_MATH_MUL;
    if (v9_streq(name, "divide")) return C3DAE_V9_MATH_DIV;
    if (v9_streq(name, "power")) return C3DAE_V9_MATH_POWI;
    if (v9_streq(name, "sin")) return C3DAE_V9_MATH_SIN_DEG;
    if (v9_streq(name, "cos")) return C3DAE_V9_MATH_COS_DEG;
    if (v9_streq(name, "min")) return C3DAE_V9_MATH_MIN;
    if (v9_streq(name, "max")) return C3DAE_V9_MATH_MAX;
    return 0;
}

static int v9_compile_expr(const char *a, const char *b, c3dae_v9_math_program *program);

static int v9_compile_apply(const char *a, const char *b, c3dae_v9_math_program *program)
{
    v9_elem opel;
    v9_elem arg;
    const char *p;
    int op;
    int rc;
    c3dae_u32 arg_count;
    if (!v9_next_child(a, b, &opel)) return C3DAE_V9_ERR_BAD_MATH;
    op = v9_math_op_name_to_code(opel.name);
    if (op == 0) return C3DAE_V9_ERR_UNSUPPORTED;
    p = opel.after;
    arg_count = 0UL;
    if (op == C3DAE_V9_MATH_SIN_DEG || op == C3DAE_V9_MATH_COS_DEG) {
        if (!v9_next_child(p, b, &arg)) return C3DAE_V9_ERR_BAD_MATH;
        rc = v9_compile_expr(arg.tag, arg.after, program);
        if (rc != C3DAE_V9_OK) return rc;
        return v9_math_emit(program, op, 0L, V9_NIL, V9_NIL);
    }
    while (v9_next_child(p, b, &arg)) {
        rc = v9_compile_expr(arg.tag, arg.after, program);
        if (rc != C3DAE_V9_OK) return rc;
        arg_count++;
        if (arg_count >= 2UL) {
            rc = v9_math_emit(program, op, 0L, V9_NIL, V9_NIL);
            if (rc != C3DAE_V9_OK) return rc;
        }
        p = arg.after;
    }
    if (arg_count == 1UL && op == C3DAE_V9_MATH_SUB) return v9_math_emit(program, C3DAE_V9_MATH_NEG, 0L, V9_NIL, V9_NIL);
    if (arg_count == 0UL) return C3DAE_V9_ERR_BAD_MATH;
    return C3DAE_V9_OK;
}

static int v9_compile_expr(const char *a, const char *b, c3dae_v9_math_program *program)
{
    v9_elem el;
    if (!v9_next_child(a, b, &el)) return C3DAE_V9_ERR_BAD_MATH;
    if (v9_streq(el.name, "math") || v9_streq(el.name, "semantics") || v9_streq(el.name, "formula")) {
        return v9_compile_expr(el.content, el.content_end, program);
    }
    if (v9_streq(el.name, "cn")) {
        return v9_math_emit(program, C3DAE_V9_MATH_CONST, v9_parse_fixed_range(el.content, el.content_end), V9_NIL, V9_NIL);
    }
    if (v9_streq(el.name, "ci")) {
        return v9_math_emit(program, C3DAE_V9_MATH_VAR, 0L, el.content, el.content_end);
    }
    if (v9_streq(el.name, "apply")) {
        return v9_compile_apply(el.content, el.content_end, program);
    }
    return C3DAE_V9_ERR_UNSUPPORTED;
}

int c3dae_v9_mathml_compile_first(const char *xml,
                                  c3dae_u32 xml_len,
                                  c3dae_v9_math_program *program)
{
    const char *a;
    const char *b;
    const char *tag_end;
    const char *end_tag;
    const char *after;
    int rc;
    if (xml == (const char *)0 || program == (c3dae_v9_math_program *)0) return C3DAE_V9_ERR_NULL;
    program->op_count = 0UL;
    program->max_stack = 0UL;
    a = xml;
    b = xml + xml_len;
    a = v9_find_start(a, b, "math", &tag_end);
    if (a == V9_NIL) return C3DAE_V9_ERR_BAD_MATH;
    end_tag = v9_find_matching_end(tag_end, b, "math", &after);
    if (end_tag == V9_NIL) return C3DAE_V9_ERR_XML;
    rc = v9_compile_expr(a, after, program);
    if (rc == C3DAE_V9_OK) program->max_stack = program->op_count;
    return rc;
}

static int v9_math_pop(c3dae_fixed *stack, c3dae_u32 *sp, c3dae_fixed *v)
{
    if (*sp == 0UL) return C3DAE_V9_ERR_STACK;
    *sp = *sp - 1UL;
    *v = stack[*sp];
    return C3DAE_V9_OK;
}

static int v9_math_push(c3dae_fixed *stack, c3dae_u32 stack_cap, c3dae_u32 *sp, c3dae_fixed v)
{
    if (*sp >= stack_cap) return C3DAE_V9_ERR_STACK;
    stack[*sp] = v;
    *sp = *sp + 1UL;
    return C3DAE_V9_OK;
}

static c3dae_fixed v9_fixed_powi(c3dae_fixed a, c3dae_fixed b)
{
    c3dae_s32 e;
    c3dae_fixed r;
    int neg;
    e = b >> C3DAE_FIXED_SHIFT;
    neg = 0;
    if (e < 0L) { neg = 1; e = -e; }
    r = C3DAE_FIXED_ONE;
    while (e > 0L) {
        r = c3dae_v9_fixed_mul(r, a);
        e--;
    }
    if (neg) r = c3dae_v9_fixed_div(C3DAE_FIXED_ONE, r);
    return r;
}

int c3dae_v9_math_eval(const c3dae_v9_math_program *program,
                       c3dae_fixed *stack,
                       c3dae_u32 stack_cap,
                       c3dae_v9_math_var_cb var_cb,
                       void *user,
                       c3dae_fixed *out_value)
{
    c3dae_u32 i;
    c3dae_u32 sp;
    c3dae_fixed a;
    c3dae_fixed b;
    c3dae_fixed v;
    int rc;
    const c3dae_v9_math_op *op;
    if (program == (const c3dae_v9_math_program *)0 || stack == (c3dae_fixed *)0 || out_value == (c3dae_fixed *)0) return C3DAE_V9_ERR_NULL;
    sp = 0UL;
    for (i = 0UL; i < program->op_count; i++) {
        op = &program->ops[i];
        switch (op->opcode) {
        case C3DAE_V9_MATH_CONST:
            rc = v9_math_push(stack, stack_cap, &sp, op->value);
            if (rc != C3DAE_V9_OK) return rc;
            break;
        case C3DAE_V9_MATH_VAR:
            if (var_cb == (c3dae_v9_math_var_cb)0) return C3DAE_V9_ERR_BAD_MATH;
            if (!var_cb(user, op->name, &v)) return C3DAE_V9_ERR_BAD_MATH;
            rc = v9_math_push(stack, stack_cap, &sp, v);
            if (rc != C3DAE_V9_OK) return rc;
            break;
        case C3DAE_V9_MATH_ADD:
            if (v9_math_pop(stack, &sp, &b) != C3DAE_V9_OK || v9_math_pop(stack, &sp, &a) != C3DAE_V9_OK) return C3DAE_V9_ERR_STACK;
            rc = v9_math_push(stack, stack_cap, &sp, (c3dae_fixed)(a + b));
            if (rc != C3DAE_V9_OK) return rc;
            break;
        case C3DAE_V9_MATH_SUB:
            if (v9_math_pop(stack, &sp, &b) != C3DAE_V9_OK || v9_math_pop(stack, &sp, &a) != C3DAE_V9_OK) return C3DAE_V9_ERR_STACK;
            rc = v9_math_push(stack, stack_cap, &sp, (c3dae_fixed)(a - b));
            if (rc != C3DAE_V9_OK) return rc;
            break;
        case C3DAE_V9_MATH_MUL:
            if (v9_math_pop(stack, &sp, &b) != C3DAE_V9_OK || v9_math_pop(stack, &sp, &a) != C3DAE_V9_OK) return C3DAE_V9_ERR_STACK;
            rc = v9_math_push(stack, stack_cap, &sp, c3dae_v9_fixed_mul(a, b));
            if (rc != C3DAE_V9_OK) return rc;
            break;
        case C3DAE_V9_MATH_DIV:
            if (v9_math_pop(stack, &sp, &b) != C3DAE_V9_OK || v9_math_pop(stack, &sp, &a) != C3DAE_V9_OK) return C3DAE_V9_ERR_STACK;
            rc = v9_math_push(stack, stack_cap, &sp, c3dae_v9_fixed_div(a, b));
            if (rc != C3DAE_V9_OK) return rc;
            break;
        case C3DAE_V9_MATH_NEG:
            if (v9_math_pop(stack, &sp, &a) != C3DAE_V9_OK) return C3DAE_V9_ERR_STACK;
            rc = v9_math_push(stack, stack_cap, &sp, (c3dae_fixed)(-a));
            if (rc != C3DAE_V9_OK) return rc;
            break;
        case C3DAE_V9_MATH_SIN_DEG:
            if (v9_math_pop(stack, &sp, &a) != C3DAE_V9_OK) return C3DAE_V9_ERR_STACK;
            rc = v9_math_push(stack, stack_cap, &sp, c3dae_fixed_sin_deg(a));
            if (rc != C3DAE_V9_OK) return rc;
            break;
        case C3DAE_V9_MATH_COS_DEG:
            if (v9_math_pop(stack, &sp, &a) != C3DAE_V9_OK) return C3DAE_V9_ERR_STACK;
            rc = v9_math_push(stack, stack_cap, &sp, c3dae_fixed_cos_deg(a));
            if (rc != C3DAE_V9_OK) return rc;
            break;
        case C3DAE_V9_MATH_MIN:
            if (v9_math_pop(stack, &sp, &b) != C3DAE_V9_OK || v9_math_pop(stack, &sp, &a) != C3DAE_V9_OK) return C3DAE_V9_ERR_STACK;
            rc = v9_math_push(stack, stack_cap, &sp, a < b ? a : b);
            if (rc != C3DAE_V9_OK) return rc;
            break;
        case C3DAE_V9_MATH_MAX:
            if (v9_math_pop(stack, &sp, &b) != C3DAE_V9_OK || v9_math_pop(stack, &sp, &a) != C3DAE_V9_OK) return C3DAE_V9_ERR_STACK;
            rc = v9_math_push(stack, stack_cap, &sp, a > b ? a : b);
            if (rc != C3DAE_V9_OK) return rc;
            break;
        case C3DAE_V9_MATH_POWI:
            if (v9_math_pop(stack, &sp, &b) != C3DAE_V9_OK || v9_math_pop(stack, &sp, &a) != C3DAE_V9_OK) return C3DAE_V9_ERR_STACK;
            rc = v9_math_push(stack, stack_cap, &sp, v9_fixed_powi(a, b));
            if (rc != C3DAE_V9_OK) return rc;
            break;
        default:
            return C3DAE_V9_ERR_UNSUPPORTED;
        }
    }
    if (sp != 1UL) return C3DAE_V9_ERR_STACK;
    *out_value = stack[0];
    return C3DAE_V9_OK;
}

int c3dae_v9_shader_preflight(const char *source,
                              c3dae_u32 source_len,
                              int profile,
                              c3dae_v9_shader_unit *unit)
{
    const char *a;
    const char *b;
    const char *p;
    c3dae_s32 braces;
    unsigned int warns;
    if (source == (const char *)0 || unit == (c3dae_v9_shader_unit *)0) return C3DAE_V9_ERR_NULL;
    a = source;
    b = source + source_len;
    braces = 0L;
    warns = 0U;
    p = a;
    while (p < b) {
        if (*p == '{') braces++;
        else if (*p == '}') braces--;
        p++;
    }
    if (braces != 0L) warns |= C3DAE_V9_SHADER_WARN_BAD_BRACES;
    if (v9_strnstr_local(a, b, "main") == V9_NIL) warns |= C3DAE_V9_SHADER_WARN_NO_MAIN;
    if (profile == C3DAE_V9_SHADER_CG) warns |= C3DAE_V9_SHADER_WARN_LEGACY_CG;
    if (profile == C3DAE_V9_SHADER_GLSL || profile == C3DAE_V9_SHADER_CG || profile == C3DAE_V9_SHADER_GLES || profile == C3DAE_V9_SHADER_GLES2) warns |= C3DAE_V9_SHADER_WARN_NEEDS_BACKEND;
    if (v9_strnstr_local(a, b, "texture2D") != V9_NIL || v9_strnstr_local(a, b, "texture(") != V9_NIL) warns |= C3DAE_V9_SHADER_WARN_TEXTURE_FN;
    if (v9_strnstr_local(a, b, "dFdx") != V9_NIL || v9_strnstr_local(a, b, "dFdy") != V9_NIL) warns |= C3DAE_V9_SHADER_WARN_DERIVATIVES;
    unit->profile = profile;
    unit->warning_flags |= warns;
    unit->uniform_count += v9_count_word(a, b, "uniform");
    unit->attribute_count += v9_count_word(a, b, "attribute");
    unit->attribute_count += v9_count_word(a, b, "in");
    unit->varying_count += v9_count_word(a, b, "varying");
    unit->sampler_count += v9_count_word(a, b, "sampler2D");
    unit->sampler_count += v9_count_word(a, b, "samplerCube");
    if (v9_strnstr_local(a, b, "gl_Position") != V9_NIL || v9_strnstr_local(a, b, "POSITION") != V9_NIL) unit->stage = C3DAE_V9_SHADER_STAGE_VERTEX;
    if (v9_strnstr_local(a, b, "gl_FragColor") != V9_NIL || v9_strnstr_local(a, b, "FRAG") != V9_NIL || v9_strnstr_local(a, b, "COLOR") != V9_NIL) unit->stage = C3DAE_V9_SHADER_STAGE_FRAGMENT;
    p = v9_strnstr_local(a, b, "#version");
    if (p != V9_NIL) {
        p += 8;
        while (p < b && v9_is_space(*p)) p++;
        while (p < b && *p >= '0' && *p <= '9') {
            unit->version_number = unit->version_number * 10UL + (c3dae_u32)(*p - '0');
            p++;
        }
    }
    return C3DAE_V9_OK;
}

int c3dae_v9_shader_compile_with_backend(const c3dae_v9_shader_compile_request *req,
                                         c3dae_v9_shader_compile_result *res,
                                         c3dae_v9_shader_backend_cb backend,
                                         void *user)
{
    if (req == (const c3dae_v9_shader_compile_request *)0 || res == (c3dae_v9_shader_compile_result *)0) return C3DAE_V9_ERR_NULL;
    if (backend == (c3dae_v9_shader_backend_cb)0) {
        const char *msg;
        unsigned long n;
        msg = "no shader backend bound; use runtime GL, cgc, glslang, or engine-specific compiler callback";
        if (res->log != (char *)0 && res->log_cap > 0UL) {
            n = v9_strlen(msg);
            if (n >= res->log_cap) n = res->log_cap - 1UL;
            memcpy(res->log, msg, (size_t)n);
            res->log[n] = '\0';
            res->log_len = (c3dae_u32)n;
        }
        res->ok = 0;
        return C3DAE_V9_ERR_UNSUPPORTED;
    }
    return backend(user, req, res);
}

void c3dae_v9_document_init(c3dae_v9_document *doc)
{
    if (doc == (c3dae_v9_document *)0) return;
    memset(doc, 0, sizeof(*doc));
}

void c3dae_v9_bind_breps(c3dae_v9_document *doc, c3dae_v9_brep_deep *items, c3dae_u32 cap)
{
    if (doc == (c3dae_v9_document *)0) return;
    doc->breps = items;
    doc->brep_cap = cap;
    doc->brep_count = 0UL;
}

void c3dae_v9_bind_formulas(c3dae_v9_document *doc, c3dae_v9_formula_info *items, c3dae_u32 cap)
{
    if (doc == (c3dae_v9_document *)0) return;
    doc->formulas = items;
    doc->formula_cap = cap;
    doc->formula_count = 0UL;
}

void c3dae_v9_bind_shaders(c3dae_v9_document *doc, c3dae_v9_shader_unit *items, c3dae_u32 cap)
{
    if (doc == (c3dae_v9_document *)0) return;
    doc->shaders = items;
    doc->shader_cap = cap;
    doc->shader_count = 0UL;
}

static void v9_scan_breps(const char *xml, const char *end, c3dae_v9_document *doc)
{
    const char *p;
    const char *ge;
    const char *gclose;
    const char *gafter;
    const char *bt;
    const char *be;
    const char *bclose;
    const char *bafter;
    c3dae_v9_brep_deep *br;
    p = xml;
    while ((ge = v9_find_start(p, end, "geometry", &gclose)) != V9_NIL) {
        gclose = v9_find_gt(ge, end);
        if (gclose == V9_NIL) break;
        gafter = V9_NIL;
        bt = v9_find_matching_end(gclose, end, "geometry", &gafter);
        if (bt == V9_NIL) break;
        be = v9_find_start(gclose, bt, "brep", &bclose);
        if (be != V9_NIL) {
            bafter = V9_NIL;
            bclose = v9_find_gt(be, bt);
            if (bclose != V9_NIL) {
                bclose = v9_find_matching_end(bclose, bt, "brep", &bafter);
                if (bclose != V9_NIL) {
                    if (doc->brep_count < doc->brep_cap && doc->breps != (c3dae_v9_brep_deep *)0) {
                        br = &doc->breps[doc->brep_count++];
                        memset(br, 0, sizeof(*br));
                        v9_attr_get(ge, gclose, "id", br->geometry_id, sizeof(br->geometry_id));
                        br->curve_count = v9_count_start_tags(be, bclose, "curve");
                        br->surface_count = v9_count_start_tags(be, bclose, "surface");
                        br->vertex_count = v9_count_start_tags(be, bclose, "vertices");
                        br->edge_count = v9_count_start_tags(be, bclose, "edges");
                        br->wire_count = v9_count_start_tags(be, bclose, "wires");
                        br->face_count = v9_count_start_tags(be, bclose, "faces");
                        br->shell_count = v9_count_start_tags(be, bclose, "shells");
                        br->solid_count = v9_count_start_tags(be, bclose, "solids");
                        br->spline_count = v9_count_start_tags(be, bclose, "spline");
                        br->nurbs_count = v9_count_start_tags(be, bclose, "nurbs");
                        br->max_degree = v9_parse_first_uint_element(be, bclose, "degree");
                        if (br->vertex_count) br->flags |= C3DAE_V9_BREP_HAS_VERTICES;
                        if (br->edge_count) br->flags |= C3DAE_V9_BREP_HAS_EDGES;
                        if (br->wire_count) br->flags |= C3DAE_V9_BREP_HAS_WIRES;
                        if (br->face_count) br->flags |= C3DAE_V9_BREP_HAS_FACES;
                        if (br->shell_count) br->flags |= C3DAE_V9_BREP_HAS_SHELLS;
                        if (br->solid_count) br->flags |= C3DAE_V9_BREP_HAS_SOLIDS;
                        if (br->curve_count) br->flags |= C3DAE_V9_BREP_HAS_CURVES;
                        if (br->surface_count) br->flags |= C3DAE_V9_BREP_HAS_SURFACES;
                        if (br->spline_count || br->nurbs_count) br->flags |= C3DAE_V9_BREP_HAS_NURBS;
                    } else {
                        doc->truncated_count++;
                    }
                }
            }
        }
        p = gafter != V9_NIL ? gafter : bt + 1;
    }
}

static int v9_profile_from_name(const char *name)
{
    if (v9_streq(name, "profile_GLSL")) return C3DAE_V9_SHADER_GLSL;
    if (v9_streq(name, "profile_CG")) return C3DAE_V9_SHADER_CG;
    if (v9_streq(name, "profile_GLES")) return C3DAE_V9_SHADER_GLES;
    if (v9_streq(name, "profile_GLES2")) return C3DAE_V9_SHADER_GLES2;
    return C3DAE_V9_SHADER_UNKNOWN;
}

static void v9_scan_shaders(const char *xml, const char *end, c3dae_v9_document *doc)
{
    const char *p;
    const char *et;
    const char *egte;
    const char *eend;
    const char *eafter;
    v9_elem prof;
    v9_elem code;
    c3dae_v9_shader_unit *su;
    int profile;
    p = xml;
    while ((et = v9_find_start(p, end, "effect", &egte)) != V9_NIL) {
        eafter = V9_NIL;
        eend = v9_find_matching_end(egte, end, "effect", &eafter);
        if (eend == V9_NIL) break;
        p = egte;
        while (v9_next_child(p, eend, &prof)) {
            profile = v9_profile_from_name(prof.name);
            if (profile != C3DAE_V9_SHADER_UNKNOWN) {
                const char *cp;
                cp = prof.content;
                while (v9_next_child(cp, prof.content_end, &code)) {
                    if (v9_streq(code.name, "code")) {
                        doc->shader_code_count++;
                        if (doc->shader_count < doc->shader_cap && doc->shaders != (c3dae_v9_shader_unit *)0) {
                            su = &doc->shaders[doc->shader_count++];
                            memset(su, 0, sizeof(*su));
                            v9_attr_get(et, egte, "id", su->effect_id, sizeof(su->effect_id));
                            v9_attr_get(code.tag, code.tag_end, "sid", su->code_id, sizeof(su->code_id));
                            if (su->code_id[0] == '\0') v9_attr_get(code.tag, code.tag_end, "id", su->code_id, sizeof(su->code_id));
                            su->profile = profile;
                            su->source_offset = (c3dae_u32)(code.content - xml);
                            su->source_len = (c3dae_u32)(code.content_end - code.content);
                            c3dae_v9_shader_preflight(code.content, su->source_len, profile, su);
                        } else {
                            doc->truncated_count++;
                        }
                    }
                    cp = code.after;
                }
            }
            p = prof.after;
        }
        p = eafter != V9_NIL ? eafter : eend + 1;
    }
}

static void v9_scan_formulas(const char *xml, const char *end, c3dae_v9_document *doc)
{
    const char *p;
    const char *ft;
    const char *fte;
    const char *fend;
    const char *fafter;
    c3dae_v9_formula_info *fi;
    p = xml;
    while ((ft = v9_find_start(p, end, "formula", &fte)) != V9_NIL) {
        fafter = V9_NIL;
        fend = v9_find_matching_end(fte, end, "formula", &fafter);
        if (fend == V9_NIL) break;
        doc->mathml_count += v9_count_start_tags(fte, fend, "math");
        if (doc->formula_count < doc->formula_cap && doc->formulas != (c3dae_v9_formula_info *)0) {
            fi = &doc->formulas[doc->formula_count++];
            memset(fi, 0, sizeof(*fi));
            v9_attr_get(ft, fte, "id", fi->formula_id, sizeof(fi->formula_id));
            v9_attr_get(ft, fte, "target", fi->target, sizeof(fi->target));
            fi->math_node_count = v9_count_start_tags(fte, fend, "math");
        } else {
            doc->truncated_count++;
        }
        p = fafter != V9_NIL ? fafter : fend + 1;
    }
}

int c3dae_v9_scan_memory(const char *xml, c3dae_u32 xml_len, c3dae_v9_document *doc)
{
    const char *end;
    if (xml == (const char *)0 || doc == (c3dae_v9_document *)0) return C3DAE_V9_ERR_NULL;
    doc->brep_count = 0UL;
    doc->formula_count = 0UL;
    doc->shader_count = 0UL;
    doc->mathml_count = 0UL;
    doc->shader_code_count = 0UL;
    doc->truncated_count = 0UL;
    doc->issue_count = 0UL;
    end = xml + xml_len;
    v9_scan_breps(xml, end, doc);
    v9_scan_formulas(xml, end, doc);
    v9_scan_shaders(xml, end, doc);
    return C3DAE_V9_OK;
}

const char *c3dae_v9_error_string(int code)
{
    switch (code) {
    case C3DAE_V9_OK: return "ok";
    case C3DAE_V9_ERR_NULL: return "null argument";
    case C3DAE_V9_ERR_XML: return "xml parse error";
    case C3DAE_V9_ERR_CAPACITY: return "capacity exceeded";
    case C3DAE_V9_ERR_BAD_MATH: return "bad MathML expression";
    case C3DAE_V9_ERR_STACK: return "MathML VM stack error";
    case C3DAE_V9_ERR_UNSUPPORTED: return "unsupported feature";
    case C3DAE_V9_ERR_BAD_NURBS: return "bad NURBS data";
    default: return "unknown";
    }
}

const char *c3dae_v9_shader_profile_name(int profile)
{
    switch (profile) {
    case C3DAE_V9_SHADER_GLSL: return "GLSL";
    case C3DAE_V9_SHADER_CG: return "CG";
    case C3DAE_V9_SHADER_GLES: return "GLES";
    case C3DAE_V9_SHADER_GLES2: return "GLES2";
    default: return "unknown";
    }
}

const char *c3dae_v9_shader_stage_name(int stage)
{
    switch (stage) {
    case C3DAE_V9_SHADER_STAGE_VERTEX: return "vertex";
    case C3DAE_V9_SHADER_STAGE_FRAGMENT: return "fragment";
    case C3DAE_V9_SHADER_STAGE_GEOMETRY: return "geometry";
    case C3DAE_V9_SHADER_STAGE_COMPUTE: return "compute";
    default: return "unknown";
    }
}
