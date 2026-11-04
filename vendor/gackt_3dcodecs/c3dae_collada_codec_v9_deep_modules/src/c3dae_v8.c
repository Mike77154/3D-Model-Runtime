#include "c3dae_v8.h"
#include <string.h>

#define V8_NIL ((const char *)0)

static unsigned long v8_strlen(const char *s)
{
    unsigned long n;
    n = 0UL;
    if (s == (const char *)0) return 0UL;
    while (s[n] != '\0') n++;
    return n;
}

static int v8_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int v8_streq(const char *a, const char *b)
{
    if (a == (const char *)0 || b == (const char *)0) return 0;
    return strcmp(a, b) == 0;
}

static void v8_trim_bounds(const char **a, const char **b)
{
    while (*a < *b && v8_is_space(**a)) (*a)++;
    while (*b > *a && v8_is_space((*b)[-1])) (*b)--;
}

static void v8_copy(char *dst, unsigned long cap, const char *a, const char *b)
{
    unsigned long n;
    if (cap == 0UL) return;
    if (a == V8_NIL || b == V8_NIL || b < a) { dst[0] = '\0'; return; }
    v8_trim_bounds(&a, &b);
    n = (unsigned long)(b - a);
    if (n >= cap) n = cap - 1UL;
    if (n > 0UL) memcpy(dst, a, (size_t)n);
    dst[n] = '\0';
}

static int v8_tag_local_matches(const char *p, const char *end, const char *name)
{
    const char *a;
    const char *b;
    const char *colon;
    const char *q;
    unsigned long n;
    unsigned long m;
    if (p >= end || *p != '<') return 0;
    a = p + 1;
    if (a < end && *a == '/') a++;
    if (a < end && (*a == '?' || *a == '!')) return 0;
    b = a;
    while (b < end && !v8_is_space(*b) && *b != '>' && *b != '/') b++;
    colon = V8_NIL;
    q = a;
    while (q < b) { if (*q == ':') colon = q; q++; }
    if (colon != V8_NIL) a = colon + 1;
    n = (unsigned long)(b - a);
    m = v8_strlen(name);
    if (n != m) return 0;
    return memcmp(a, name, (size_t)n) == 0;
}

static const char *v8_find_gt(const char *p, const char *end)
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
    return V8_NIL;
}

static const char *v8_find_start(const char *p, const char *end, const char *name, const char **tag_end)
{
    const char *gt;
    while (p < end) {
        while (p < end && *p != '<') p++;
        if (p >= end) break;
        if (p + 1 < end && p[1] == '/') { p++; continue; }
        if (v8_tag_local_matches(p, end, name)) {
            gt = v8_find_gt(p, end);
            if (gt == V8_NIL) return V8_NIL;
            if (tag_end != (const char **)0) *tag_end = gt;
            return p;
        }
        p++;
    }
    return V8_NIL;
}

static const char *v8_find_end(const char *p, const char *end, const char *name, const char **after)
{
    const char *gt;
    while (p < end) {
        while (p < end && *p != '<') p++;
        if (p >= end) break;
        if (p + 1 < end && p[1] == '/' && v8_tag_local_matches(p, end, name)) {
            gt = v8_find_gt(p, end);
            if (gt == V8_NIL) return V8_NIL;
            if (after != (const char **)0) *after = gt;
            return p;
        }
        p++;
    }
    return V8_NIL;
}

static int v8_is_self_close(const char *tag, const char *tag_end)
{
    const char *p;
    p = tag_end;
    while (p > tag) {
        p--;
        if (v8_is_space(*p) || *p == '>') continue;
        return *p == '/';
    }
    return 0;
}

static int v8_attr_get(const char *tag, const char *tag_end, const char *attr, char *out, unsigned long cap)
{
    const char *p;
    unsigned long alen;
    if (cap == 0UL) return 0;
    out[0] = '\0';
    alen = v8_strlen(attr);
    p = tag;
    while (p < tag_end) {
        while (p < tag_end && (v8_is_space(*p) || *p == '<' || *p == '/' || *p == '>')) p++;
        if (p + alen < tag_end && memcmp(p, attr, (size_t)alen) == 0) {
            const char *q;
            q = p + alen;
            if (q < tag_end && (*q == '=' || v8_is_space(*q))) {
                while (q < tag_end && v8_is_space(*q)) q++;
                if (q < tag_end && *q == '=') {
                    q++;
                    while (q < tag_end && v8_is_space(*q)) q++;
                    if (q < tag_end && (*q == '"' || *q == '\'')) {
                        char quote;
                        const char *a;
                        quote = *q;
                        q++;
                        a = q;
                        while (q < tag_end && *q != quote) q++;
                        v8_copy(out, cap, a, q);
                        return 1;
                    }
                }
            }
        }
        while (p < tag_end && !v8_is_space(*p) && *p != '>') p++;
    }
    return 0;
}

static c3dae_fixed v8_parse_fixed_range(const char *a, const char *b)
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
    v8_trim_bounds(&a, &b);
    if (a < b && *a == '-') { sign = -1L; a++; }
    else if (a < b && *a == '+') a++;
    while (a < b && *a >= '0' && *a <= '9') { ip = ip * 10L + (long)(*a - '0'); a++; }
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

static c3dae_u32 v8_parse_fixed_values(const char *a, const char *b, c3dae_fixed *out, c3dae_u32 cap)
{
    c3dae_u32 count;
    const char *s;
    const char *n;
    count = 0UL;
    s = a;
    while (s < b && count < cap) {
        while (s < b && !((*s >= '0' && *s <= '9') || *s == '-' || *s == '+')) s++;
        if (s >= b) break;
        n = s;
        if (*n == '-' || *n == '+') n++;
        while (n < b && ((*n >= '0' && *n <= '9') || *n == '.')) n++;
        out[count] = v8_parse_fixed_range(s, n);
        count++;
        s = n;
    }
    return count;
}

static c3dae_u32 v8_parse_uint_attr(const char *tag, const char *tag_end, const char *name)
{
    char tmp[32];
    c3dae_fixed f;
    tmp[0] = '\0';
    if (!v8_attr_get(tag, tag_end, name, tmp, sizeof(tmp))) return 0UL;
    f = v8_parse_fixed_range(tmp, tmp + v8_strlen(tmp));
    if (f < 0) return 0UL;
    return (c3dae_u32)(f >> C3DAE_FIXED_SHIFT);
}

static const char *v8_child_text(const char *a, const char *b, const char *name, char *out, unsigned long cap)
{
    const char *tag;
    const char *tag_end;
    const char *end_tag;
    const char *after;
    if (cap > 0UL) out[0] = '\0';
    tag = v8_find_start(a, b, name, &tag_end);
    if (tag == V8_NIL) return V8_NIL;
    if (v8_is_self_close(tag, tag_end)) return tag_end;
    end_tag = v8_find_end(tag_end, b, name, &after);
    if (end_tag == V8_NIL) return V8_NIL;
    v8_copy(out, cap, tag_end, end_tag);
    return after;
}

static c3dae_fixed v8_fmul(c3dae_fixed a, c3dae_fixed b)
{
    return (c3dae_fixed)(((long)a * (long)b) >> C3DAE_FIXED_SHIFT);
}

static c3dae_fixed v8_fdiv(c3dae_fixed a, c3dae_fixed b)
{
    if (b == 0) return 0;
    return (c3dae_fixed)(((long)a << C3DAE_FIXED_SHIFT) / (long)b);
}

static c3dae_fixed v8_abs(c3dae_fixed a)
{
    if (a < 0) return -a;
    return a;
}

static c3dae_fixed v8_sqrt_fixed(c3dae_fixed x)
{
    c3dae_fixed g;
    int i;
    if (x <= 0) return 0;
    g = x;
    if (g < C3DAE_FIXED_ONE) g = C3DAE_FIXED_ONE;
    for (i = 0; i < 10; i++) g = (c3dae_fixed)((g + v8_fdiv(x, g)) >> 1);
    return g;
}

static void v8_vec3_cross(c3dae_fixed *o, const c3dae_fixed *a, const c3dae_fixed *b)
{
    o[0] = v8_fmul(a[1], b[2]) - v8_fmul(a[2], b[1]);
    o[1] = v8_fmul(a[2], b[0]) - v8_fmul(a[0], b[2]);
    o[2] = v8_fmul(a[0], b[1]) - v8_fmul(a[1], b[0]);
}

static c3dae_fixed v8_vec3_dot(const c3dae_fixed *a, const c3dae_fixed *b)
{
    return v8_fmul(a[0], b[0]) + v8_fmul(a[1], b[1]) + v8_fmul(a[2], b[2]);
}

static void v8_vec3_norm(c3dae_fixed *v)
{
    c3dae_fixed len2;
    c3dae_fixed len;
    len2 = v8_vec3_dot(v, v);
    len = v8_sqrt_fixed(v8_abs(len2));
    if (len == 0) return;
    v[0] = v8_fdiv(v[0], len);
    v[1] = v8_fdiv(v[1], len);
    v[2] = v8_fdiv(v[2], len);
}

static void v8_matrix_identity(c3dae_fixed *m)
{
    int i;
    for (i = 0; i < 16; i++) m[i] = 0;
    m[0] = C3DAE_FIXED_ONE;
    m[5] = C3DAE_FIXED_ONE;
    m[10] = C3DAE_FIXED_ONE;
    m[15] = C3DAE_FIXED_ONE;
}

void c3dae_v8_matrix_from_lookat(c3dae_fixed *m, const c3dae_fixed *raw9)
{
    c3dae_fixed eye[3];
    c3dae_fixed f[3];
    c3dae_fixed up[3];
    c3dae_fixed s[3];
    c3dae_fixed u[3];
    int i;
    if (m == (c3dae_fixed *)0 || raw9 == (const c3dae_fixed *)0) return;
    for (i = 0; i < 3; i++) eye[i] = raw9[i];
    for (i = 0; i < 3; i++) f[i] = raw9[3 + i] - raw9[i];
    for (i = 0; i < 3; i++) up[i] = raw9[6 + i];
    v8_vec3_norm(f);
    v8_vec3_norm(up);
    v8_vec3_cross(s, f, up);
    v8_vec3_norm(s);
    v8_vec3_cross(u, s, f);
    v8_matrix_identity(m);
    m[0] = s[0]; m[1] = s[1]; m[2] = s[2]; m[3] = -v8_vec3_dot(s, eye);
    m[4] = u[0]; m[5] = u[1]; m[6] = u[2]; m[7] = -v8_vec3_dot(u, eye);
    m[8] = -f[0]; m[9] = -f[1]; m[10] = -f[2]; m[11] = v8_vec3_dot(f, eye);
    m[12] = 0; m[13] = 0; m[14] = 0; m[15] = C3DAE_FIXED_ONE;
}

void c3dae_v8_matrix_from_skew(c3dae_fixed *m, const c3dae_fixed *raw7)
{
    c3dae_fixed angle;
    c3dae_fixed r[3];
    c3dae_fixed t[3];
    c3dae_fixed sn;
    c3dae_fixed cs;
    c3dae_fixed k;
    int i;
    int j;
    if (m == (c3dae_fixed *)0 || raw7 == (const c3dae_fixed *)0) return;
    angle = raw7[0];
    r[0] = raw7[1]; r[1] = raw7[2]; r[2] = raw7[3];
    t[0] = raw7[4]; t[1] = raw7[5]; t[2] = raw7[6];
    v8_vec3_norm(r);
    v8_vec3_norm(t);
    sn = c3dae_fixed_sin_deg(angle);
    cs = c3dae_fixed_cos_deg(angle);
    k = v8_fdiv(sn, cs);
    v8_matrix_identity(m);
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            m[(i * 4) + j] = m[(i * 4) + j] + v8_fmul(k, v8_fmul(t[i], r[j]));
        }
    }
}

void c3dae_v8_document_init(c3dae_v8_document *doc)
{
    if (doc == (c3dae_v8_document *)0) return;
    memset(doc, 0, sizeof(*doc));
}

void c3dae_v8_bind_transforms(c3dae_v8_document *doc, c3dae_v8_transform *items, c3dae_u32 cap)
{
    if (doc == (c3dae_v8_document *)0) return;
    doc->transforms = items;
    doc->transform_cap = cap;
    doc->transform_count = 0UL;
}

void c3dae_v8_bind_sources(c3dae_v8_document *doc, c3dae_v8_source_info *items, c3dae_u32 cap)
{
    if (doc == (c3dae_v8_document *)0) return;
    doc->sources = items;
    doc->source_cap = cap;
    doc->source_count = 0UL;
}

void c3dae_v8_bind_render_passes(c3dae_v8_document *doc, c3dae_v8_render_pass *items, c3dae_u32 cap)
{
    if (doc == (c3dae_v8_document *)0) return;
    doc->render_passes = items;
    doc->render_pass_cap = cap;
    doc->render_pass_count = 0UL;
}

void c3dae_v8_bind_images(c3dae_v8_document *doc, c3dae_v8_image_info *items, c3dae_u32 cap)
{
    if (doc == (c3dae_v8_document *)0) return;
    doc->images = items;
    doc->image_cap = cap;
    doc->image_count = 0UL;
}

void c3dae_v8_bind_breps(c3dae_v8_document *doc, c3dae_v8_brep_info *items, c3dae_u32 cap)
{
    if (doc == (c3dae_v8_document *)0) return;
    doc->breps = items;
    doc->brep_cap = cap;
    doc->brep_count = 0UL;
}

void c3dae_v8_bind_splines(c3dae_v8_document *doc, c3dae_v8_spline_info *items, c3dae_u32 cap)
{
    if (doc == (c3dae_v8_document *)0) return;
    doc->splines = items;
    doc->spline_cap = cap;
    doc->spline_count = 0UL;
}

void c3dae_v8_bind_issues(c3dae_v8_document *doc, c3dae_v8_issue *items, c3dae_u32 cap)
{
    if (doc == (c3dae_v8_document *)0) return;
    doc->issues = items;
    doc->issue_cap = cap;
    doc->issue_count = 0UL;
}

static void v8_issue(c3dae_v8_document *doc, int code, const char *where, const char *msg)
{
    c3dae_v8_issue *it;
    if (doc->issues == (c3dae_v8_issue *)0) return;
    if (doc->issue_count >= doc->issue_cap) { doc->stats.truncated_count++; return; }
    it = &doc->issues[doc->issue_count];
    memset(it, 0, sizeof(*it));
    it->code = code;
    if (where != (const char *)0) v8_copy(it->where_id, C3DAE_ID_MAX, where, where + v8_strlen(where));
    if (msg != (const char *)0) v8_copy(it->message, C3DAE_V8_TEXT_MAX, msg, msg + v8_strlen(msg));
    doc->issue_count++;
}

static void v8_push_transform(c3dae_v8_document *doc, const char *node, const char *node_end, const char *tag, const char *tag_end, const char *elem_end, int kind)
{
    c3dae_v8_transform *t;
    c3dae_u32 want;
    if (doc->transforms == (c3dae_v8_transform *)0) return;
    if (doc->transform_count >= doc->transform_cap) { doc->stats.truncated_count++; return; }
    t = &doc->transforms[doc->transform_count];
    memset(t, 0, sizeof(*t));
    v8_attr_get(node, node_end, "id", t->node_id, C3DAE_ID_MAX);
    v8_attr_get(node, node_end, "sid", t->node_sid, C3DAE_ID_MAX);
    v8_attr_get(node, node_end, "name", t->node_name, C3DAE_ID_MAX);
    v8_attr_get(tag, tag_end, "sid", t->sid, C3DAE_ID_MAX);
    t->kind = kind;
    want = kind == C3DAE_V8_TRANS_LOOKAT ? 9UL : 7UL;
    v8_parse_fixed_values(tag_end, elem_end, t->raw, want);
    if (kind == C3DAE_V8_TRANS_LOOKAT) {
        c3dae_v8_matrix_from_lookat(t->matrix, t->raw);
        doc->stats.lookat_count++;
    } else {
        c3dae_v8_matrix_from_skew(t->matrix, t->raw);
        doc->stats.skew_count++;
    }
    doc->transform_count++;
}

static void v8_parse_node_transforms(const char *xml, const char *end, c3dae_v8_document *doc)
{
    const char *node;
    const char *node_end_tag;
    const char *node_end;
    const char *after_node;
    const char *tag;
    const char *tag_end;
    const char *elem_end;
    const char *after;
    node = xml;
    while (1) {
        node = v8_find_start(node, end, "node", &node_end_tag);
        if (node == V8_NIL) break;
        node_end = v8_find_end(node_end_tag, end, "node", &after_node);
        if (node_end == V8_NIL) break;
        tag = node_end_tag;
        while (1) {
            tag = v8_find_start(tag, node_end, "lookat", &tag_end);
            if (tag == V8_NIL) break;
            elem_end = v8_find_end(tag_end, node_end, "lookat", &after);
            if (elem_end == V8_NIL) break;
            v8_push_transform(doc, node, node_end_tag, tag, tag_end, elem_end, C3DAE_V8_TRANS_LOOKAT);
            tag = after;
        }
        tag = node_end_tag;
        while (1) {
            tag = v8_find_start(tag, node_end, "skew", &tag_end);
            if (tag == V8_NIL) break;
            elem_end = v8_find_end(tag_end, node_end, "skew", &after);
            if (elem_end == V8_NIL) break;
            v8_push_transform(doc, node, node_end_tag, tag, tag_end, elem_end, C3DAE_V8_TRANS_SKEW);
            tag = after;
        }
        node = after_node;
    }
}

static int v8_array_kind_from_name(const char *name)
{
    if (v8_streq(name, "float_array")) return C3DAE_V8_ARRAY_FLOAT;
    if (v8_streq(name, "Name_array")) return C3DAE_V8_ARRAY_NAME;
    if (v8_streq(name, "IDREF_array")) return C3DAE_V8_ARRAY_IDREF;
    if (v8_streq(name, "SIDREF_array")) return C3DAE_V8_ARRAY_SIDREF;
    if (v8_streq(name, "bool_array")) return C3DAE_V8_ARRAY_BOOL;
    if (v8_streq(name, "int_array")) return C3DAE_V8_ARRAY_INT;
    return C3DAE_V8_ARRAY_UNKNOWN;
}

static int v8_find_first_array(const char *a, const char *b, const char **out_tag, const char **out_end, char *kind_name, unsigned long kind_cap)
{
    static const char *names[6] = { "float_array", "Name_array", "IDREF_array", "SIDREF_array", "bool_array", "int_array" };
    const char *best;
    const char *best_end;
    const char *cur;
    const char *cur_end;
    int best_i;
    int i;
    best = V8_NIL;
    best_end = V8_NIL;
    best_i = -1;
    for (i = 0; i < 6; i++) {
        cur = v8_find_start(a, b, names[i], &cur_end);
        if (cur != V8_NIL && (best == V8_NIL || cur < best)) {
            best = cur;
            best_end = cur_end;
            best_i = i;
        }
    }
    if (best == V8_NIL) return 0;
    *out_tag = best;
    *out_end = best_end;
    v8_copy(kind_name, kind_cap, names[best_i], names[best_i] + v8_strlen(names[best_i]));
    return 1;
}

static void v8_parse_sources(const char *xml, const char *end, c3dae_v8_document *doc)
{
    const char *src;
    const char *src_end_tag;
    const char *src_end;
    const char *after_src;
    const char *arr;
    const char *arr_end;
    const char *acc;
    const char *acc_end;
    const char *param;
    const char *param_end;
    char kind_name[32];
    c3dae_v8_source_info *s;
    src = xml;
    while (1) {
        src = v8_find_start(src, end, "source", &src_end_tag);
        if (src == V8_NIL) break;
        src_end = v8_find_end(src_end_tag, end, "source", &after_src);
        if (src_end == V8_NIL) break;
        doc->stats.source_count++;
        if (doc->sources == (c3dae_v8_source_info *)0) { src = after_src; continue; }
        if (doc->source_count >= doc->source_cap) { doc->stats.truncated_count++; return; }
        s = &doc->sources[doc->source_count];
        memset(s, 0, sizeof(*s));
        v8_attr_get(src, src_end_tag, "id", s->source_id, C3DAE_ID_MAX);
        if (v8_find_first_array(src_end_tag, src_end, &arr, &arr_end, kind_name, sizeof(kind_name))) {
            v8_attr_get(arr, arr_end, "id", s->array_id, C3DAE_ID_MAX);
            s->array_kind = v8_array_kind_from_name(kind_name);
            s->count = v8_parse_uint_attr(arr, arr_end, "count");
        }
        s->stride = 1UL;
        acc = v8_find_start(src_end_tag, src_end, "accessor", &acc_end);
        if (acc != V8_NIL) {
            s->stride = v8_parse_uint_attr(acc, acc_end, "stride");
            s->offset = v8_parse_uint_attr(acc, acc_end, "offset");
            if (s->stride == 0UL) s->stride = 1UL;
            param = acc_end;
            while (1) {
                param = v8_find_start(param, src_end, "param", &param_end);
                if (param == V8_NIL) break;
                if (s->param_count == 0UL) {
                    if (!v8_attr_get(param, param_end, "name", s->first_param, C3DAE_ID_MAX)) v8_attr_get(param, param_end, "semantic", s->first_param, C3DAE_ID_MAX);
                }
                s->param_count++;
                param = param_end;
            }
        }
        doc->source_count++;
        src = after_src;
    }
}

static void v8_parse_render_passes(const char *xml, const char *end, c3dae_v8_document *doc)
{
    const char *vs;
    const char *vs_end_tag;
    const char *vs_end;
    const char *after_vs;
    const char *eval;
    const char *eval_end_tag;
    const char *eval_end;
    const char *after_eval;
    const char *render;
    const char *render_end_tag;
    const char *render_end;
    const char *after_render;
    c3dae_v8_render_pass *rp;
    char vsid[C3DAE_ID_MAX];
    vs = xml;
    while (1) {
        vs = v8_find_start(vs, end, "visual_scene", &vs_end_tag);
        if (vs == V8_NIL) break;
        vs_end = v8_find_end(vs_end_tag, end, "visual_scene", &after_vs);
        if (vs_end == V8_NIL) break;
        vsid[0] = '\0';
        v8_attr_get(vs, vs_end_tag, "id", vsid, sizeof(vsid));
        eval = vs_end_tag;
        while (1) {
            eval = v8_find_start(eval, vs_end, "evaluate_scene", &eval_end_tag);
            if (eval == V8_NIL) break;
            eval_end = v8_find_end(eval_end_tag, vs_end, "evaluate_scene", &after_eval);
            if (eval_end == V8_NIL) break;
            render = eval_end_tag;
            while (1) {
                render = v8_find_start(render, eval_end, "render", &render_end_tag);
                if (render == V8_NIL) break;
                render_end = v8_find_end(render_end_tag, eval_end, "render", &after_render);
                if (render_end == V8_NIL) break;
                doc->stats.render_pass_count++;
                if (doc->render_passes != (c3dae_v8_render_pass *)0) {
                    if (doc->render_pass_count >= doc->render_pass_cap) { doc->stats.truncated_count++; return; }
                    rp = &doc->render_passes[doc->render_pass_count];
                    memset(rp, 0, sizeof(*rp));
                    v8_copy(rp->visual_scene, C3DAE_ID_MAX, vsid, vsid + v8_strlen(vsid));
                    v8_attr_get(eval, eval_end_tag, "name", rp->evaluate_name, C3DAE_ID_MAX);
                    v8_attr_get(render, render_end_tag, "sid", rp->render_sid, C3DAE_ID_MAX);
                    v8_child_text(render_end_tag, render_end, "camera_node", rp->camera_node, C3DAE_ID_MAX);
                    v8_child_text(render_end_tag, render_end, "layer", rp->layer, C3DAE_ID_MAX);
                    doc->render_pass_count++;
                }
                render = after_render;
            }
            eval = after_eval;
        }
        vs = after_vs;
    }
}

static c3dae_u32 v8_non_space_len(const char *a, const char *b)
{
    c3dae_u32 n;
    n = 0UL;
    while (a < b) {
        if (!v8_is_space(*a)) n++;
        a++;
    }
    return n;
}

static void v8_parse_images(const char *xml, const char *end, c3dae_v8_document *doc)
{
    const char *img;
    const char *img_end_tag;
    const char *img_end;
    const char *after_img;
    const char *data;
    const char *data_end_tag;
    const char *data_end;
    const char *after_data;
    c3dae_v8_image_info *ii;
    img = xml;
    while (1) {
        img = v8_find_start(img, end, "image", &img_end_tag);
        if (img == V8_NIL) break;
        img_end = v8_find_end(img_end_tag, end, "image", &after_img);
        if (img_end == V8_NIL) break;
        if (doc->images == (c3dae_v8_image_info *)0) { img = after_img; continue; }
        if (doc->image_count >= doc->image_cap) { doc->stats.truncated_count++; return; }
        ii = &doc->images[doc->image_count];
        memset(ii, 0, sizeof(*ii));
        v8_attr_get(img, img_end_tag, "id", ii->image_id, C3DAE_ID_MAX);
        if (v8_child_text(img_end_tag, img_end, "init_from", ii->path, C3DAE_PATH_MAX) != V8_NIL) {
            ii->flags |= C3DAE_V8_IMAGE_EXTERNAL;
            doc->stats.external_image_count++;
        }
        data = v8_find_start(img_end_tag, img_end, "data", &data_end_tag);
        if (data != V8_NIL) {
            data_end = v8_find_end(data_end_tag, img_end, "data", &after_data);
            if (data_end != V8_NIL) {
                ii->flags |= C3DAE_V8_IMAGE_EMBEDDED;
                v8_attr_get(data, data_end_tag, "format", ii->format, C3DAE_ID_MAX);
                ii->embedded_bytes = v8_non_space_len(data_end_tag, data_end);
                doc->stats.embedded_image_count++;
                v8_issue(doc, C3DAE_V8_WARN_EMBEDDED_IMAGE, ii->image_id, "embedded image data detected; route to texture extractor");
            }
        }
        if (ii->flags != 0U) doc->image_count++;
        img = after_img;
    }
}

static c3dae_u32 v8_count_tag(const char *a, const char *b, const char *name)
{
    c3dae_u32 n;
    const char *p;
    const char *tag_end;
    n = 0UL;
    p = a;
    while (1) {
        p = v8_find_start(p, b, name, &tag_end);
        if (p == V8_NIL) break;
        n++;
        p = tag_end;
    }
    return n;
}

static void v8_parse_brep_spline(const char *xml, const char *end, c3dae_v8_document *doc)
{
    const char *geom;
    const char *geom_end_tag;
    const char *geom_end;
    const char *after_geom;
    const char *brep;
    const char *brep_end_tag;
    const char *brep_end;
    const char *after_brep;
    const char *spline;
    const char *spline_end_tag;
    const char *spline_end;
    const char *after_spline;
    const char *tag;
    const char *tag_end;
    c3dae_v8_brep_info *bi;
    c3dae_v8_spline_info *si;
    char gid[C3DAE_ID_MAX];
    char tmp[C3DAE_ID_MAX];
    geom = xml;
    while (1) {
        geom = v8_find_start(geom, end, "geometry", &geom_end_tag);
        if (geom == V8_NIL) break;
        geom_end = v8_find_end(geom_end_tag, end, "geometry", &after_geom);
        if (geom_end == V8_NIL) break;
        gid[0] = '\0';
        v8_attr_get(geom, geom_end_tag, "id", gid, sizeof(gid));
        brep = v8_find_start(geom_end_tag, geom_end, "brep", &brep_end_tag);
        if (brep != V8_NIL) {
            brep_end = v8_find_end(brep_end_tag, geom_end, "brep", &after_brep);
            if (brep_end != V8_NIL) {
                doc->stats.brep_count++;
                v8_issue(doc, C3DAE_V8_WARN_BREP_ROUTE, gid, "B-rep detected; send to CAD/tessellation bridge");
                if (doc->breps != (c3dae_v8_brep_info *)0) {
                    if (doc->brep_count >= doc->brep_cap) { doc->stats.truncated_count++; return; }
                    bi = &doc->breps[doc->brep_count];
                    memset(bi, 0, sizeof(*bi));
                    v8_copy(bi->geometry_id, C3DAE_ID_MAX, gid, gid + v8_strlen(gid));
                    bi->source_count = v8_count_tag(brep_end_tag, brep_end, "source");
                    bi->curve_count = v8_count_tag(brep_end_tag, brep_end, "curve");
                    bi->surface_count = v8_count_tag(brep_end_tag, brep_end, "surface");
                    bi->edge_count = v8_count_tag(brep_end_tag, brep_end, "edge");
                    bi->wire_count = v8_count_tag(brep_end_tag, brep_end, "wire");
                    bi->face_count = v8_count_tag(brep_end_tag, brep_end, "face");
                    bi->shell_count = v8_count_tag(brep_end_tag, brep_end, "shell");
                    bi->solid_count = v8_count_tag(brep_end_tag, brep_end, "solid");
                    if (bi->curve_count != 0UL) bi->flags |= C3DAE_V8_BREP_HAS_CURVES;
                    if (bi->surface_count != 0UL) bi->flags |= C3DAE_V8_BREP_HAS_SURFACES;
                    if (bi->edge_count != 0UL || bi->wire_count != 0UL || bi->face_count != 0UL) bi->flags |= C3DAE_V8_BREP_HAS_TOPOLOGY;
                    if (bi->shell_count != 0UL || bi->solid_count != 0UL) bi->flags |= C3DAE_V8_BREP_HAS_SOLIDS;
                    doc->brep_count++;
                }
            }
        }
        spline = v8_find_start(geom_end_tag, geom_end, "spline", &spline_end_tag);
        if (spline != V8_NIL) {
            spline_end = v8_find_end(spline_end_tag, geom_end, "spline", &after_spline);
            if (spline_end != V8_NIL) {
                doc->stats.spline_count++;
                v8_issue(doc, C3DAE_V8_WARN_SPLINE_ROUTE, gid, "spline detected; route to curve tessellator");
                if (doc->splines != (c3dae_v8_spline_info *)0) {
                    if (doc->spline_count >= doc->spline_cap) { doc->stats.truncated_count++; return; }
                    si = &doc->splines[doc->spline_count];
                    memset(si, 0, sizeof(*si));
                    v8_copy(si->geometry_id, C3DAE_ID_MAX, gid, gid + v8_strlen(gid));
                    si->closed = C3DAE_V8_SPLINE_UNKNOWN;
                    si->source_count = v8_count_tag(spline_end_tag, spline_end, "source");
                    tag = v8_find_start(spline_end_tag, spline_end, "control_vertices", &tag_end);
                    if (tag != V8_NIL) si->control_vertex_input_count = v8_count_tag(tag_end, spline_end, "input");
                    tmp[0] = '\0';
                    if (v8_attr_get(spline, spline_end_tag, "closed", tmp, sizeof(tmp))) {
                        if (v8_streq(tmp, "true") || v8_streq(tmp, "1") || v8_streq(tmp, "TRUE")) si->closed = C3DAE_V8_SPLINE_CLOSED;
                        else si->closed = C3DAE_V8_SPLINE_OPEN;
                    }
                    tag = v8_find_start(spline_end_tag, spline_end, "degree", &tag_end);
                    if (tag != V8_NIL) {
                        const char *deg_end;
                        const char *after_deg;
                        deg_end = v8_find_end(tag_end, spline_end, "degree", &after_deg);
                        if (deg_end != V8_NIL) si->degree = (c3dae_u32)(v8_parse_fixed_range(tag_end, deg_end) >> C3DAE_FIXED_SHIFT);
                    }
                    doc->spline_count++;
                }
            }
        }
        geom = after_geom;
    }
}

static void v8_lint_schema(const char *xml, const char *end, c3dae_v8_document *doc)
{
    const char *tag_end;
    const char *p;
    if (v8_find_start(xml, end, "asset", &tag_end) == V8_NIL) v8_issue(doc, C3DAE_V8_WARN_NO_ASSET, "COLLADA", "missing asset metadata");
    if (v8_find_start(xml, end, "scene", &tag_end) == V8_NIL) v8_issue(doc, C3DAE_V8_WARN_NO_SCENE, "COLLADA", "missing active scene");
    p = xml;
    while (1) {
        p = v8_find_start(p, end, "profile_GLSL", &tag_end);
        if (p == V8_NIL) break;
        doc->stats.unknown_profile_count++;
        v8_issue(doc, C3DAE_V8_WARN_SHADER_PROFILE, "profile_GLSL", "shader profile detected; convert or delegate to renderer backend");
        p = tag_end;
    }
    p = xml;
    while (1) {
        p = v8_find_start(p, end, "profile_CG", &tag_end);
        if (p == V8_NIL) break;
        doc->stats.unknown_profile_count++;
        v8_issue(doc, C3DAE_V8_WARN_SHADER_PROFILE, "profile_CG", "Cg shader profile detected; convert or ignore for fixed-function targets");
        p = tag_end;
    }
    p = xml;
    while (1) {
        p = v8_find_start(p, end, "h", &tag_end);
        if (p == V8_NIL) break;
        v8_issue(doc, C3DAE_V8_WARN_POLYGON_HOLES, "polygons/ph", "polygon hole detected; needs robust tessellation path");
        p = tag_end;
    }
    p = xml;
    while (1) {
        p = v8_find_start(p, end, "math", &tag_end);
        if (p == V8_NIL) break;
        doc->stats.mathml_count++;
        v8_issue(doc, C3DAE_V8_WARN_FORMULA_MATHML, "math", "MathML formula payload detected; scanner reports it but does not evaluate it");
        p = tag_end;
    }
}

int c3dae_v8_scan_memory(const char *xml, c3dae_u32 xml_len, c3dae_v8_document *doc)
{
    const char *end;
    if (xml == (const char *)0 || doc == (c3dae_v8_document *)0) return C3DAE_V8_ERR_NULL;
    end = xml + xml_len;
    v8_parse_node_transforms(xml, end, doc);
    v8_parse_sources(xml, end, doc);
    v8_parse_render_passes(xml, end, doc);
    v8_parse_images(xml, end, doc);
    v8_parse_brep_spline(xml, end, doc);
    v8_lint_schema(xml, end, doc);
    doc->stats.truncated_count += doc->stats.truncated_count;
    return C3DAE_V8_OK;
}

const char *c3dae_v8_error_string(int code)
{
    switch (code) {
    case C3DAE_V8_OK: return "ok";
    case C3DAE_V8_ERR_NULL: return "null argument";
    case C3DAE_V8_ERR_XML: return "xml parse error";
    default: return "unknown c3dae_v8 error";
    }
}
