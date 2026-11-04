#include "amj3d.h"

#ifndef AMJ3D_NULL
#define AMJ3D_NULL ((void*)0)
#endif

#define AMJ3D_LINE_MAX 512U
#define AMJ3D_DEFAULT_COORD_LIMIT (16384L * AMJ3D_FIXED_ONE)

static int amj3d_is_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static int amj3d_is_digit(unsigned char c)
{
    return c >= (unsigned char)'0' && c <= (unsigned char)'9';
}

static unsigned char amj3d_lower(unsigned char c)
{
    if (c >= (unsigned char)'A' && c <= (unsigned char)'Z') {
        return (unsigned char)(c - (unsigned char)'A' + (unsigned char)'a');
    }
    return c;
}

static int amj3d_streq_ci_len(const char *a, unsigned int alen, const char *b)
{
    unsigned int i;
    i = 0U;
    while (b[i] != '\0') {
        if (i >= alen) {
            return 0;
        }
        if (amj3d_lower((unsigned char)a[i]) != amj3d_lower((unsigned char)b[i])) {
            return 0;
        }
        i++;
    }
    return i == alen;
}

static void amj3d_warn(const AMJ3D_Callbacks *cb, void *user, AMJ3D_Info *info, int code, const char *message)
{
    if (info != AMJ3D_NULL) {
        info->warning_count++;
    }
    if (cb != AMJ3D_NULL && cb->on_warning != AMJ3D_NULL) {
        cb->on_warning(user, code, message);
    }
}

static amj3d_u32 amj3d_read_u32_le(const amj3d_u8 *p)
{
    amj3d_u32 v;
    v = (amj3d_u32)p[0];
    v |= ((amj3d_u32)p[1]) << 8;
    v |= ((amj3d_u32)p[2]) << 16;
    v |= ((amj3d_u32)p[3]) << 24;
    return v;
}

static amj3d_u32 amj3d_read_u32_be(const amj3d_u8 *p)
{
    amj3d_u32 v;
    v = (amj3d_u32)p[3];
    v |= ((amj3d_u32)p[2]) << 8;
    v |= ((amj3d_u32)p[1]) << 16;
    v |= ((amj3d_u32)p[0]) << 24;
    return v;
}

static int amj3d_mem_contains_ci(const amj3d_u8 *data, unsigned long size, const char *needle)
{
    unsigned long i;
    unsigned int n;
    unsigned int j;
    n = 0U;
    while (needle[n] != '\0') {
        n++;
    }
    if (n == 0U || size < (unsigned long)n) {
        return 0;
    }
    i = 0UL;
    while (i + (unsigned long)n <= size) {
        j = 0U;
        while (j < n) {
            if (amj3d_lower(data[i + (unsigned long)j]) != amj3d_lower((unsigned char)needle[j])) {
                break;
            }
            j++;
        }
        if (j == n) {
            return 1;
        }
        i++;
    }
    return 0;
}

static int amj3d_looks_text(const amj3d_u8 *data, unsigned long size)
{
    unsigned long i;
    unsigned long scan;
    unsigned long printable;
    unsigned char c;
    scan = size;
    if (scan > 4096UL) {
        scan = 4096UL;
    }
    if (scan == 0UL) {
        return 0;
    }
    printable = 0UL;
    i = 0UL;
    while (i < scan) {
        c = data[i];
        if ((c >= 32U && c <= 126U) || c == 9U || c == 10U || c == 13U) {
            printable++;
        }
        if (c == 0U) {
            return 0;
        }
        i++;
    }
    return printable * 100UL >= scan * 85UL;
}

static void amj3d_skip_spaces_line(const char **pp, const char *end)
{
    const char *p;
    p = *pp;
    while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) {
        p++;
    }
    *pp = p;
}

static unsigned int amj3d_read_token(const char **pp, const char *end, const char **tok)
{
    const char *p;
    const char *s;
    p = *pp;
    amj3d_skip_spaces_line(&p, end);
    s = p;
    while (p < end && !amj3d_is_space((unsigned char)*p) && *p != ',' && *p != ';') {
        p++;
    }
    *tok = s;
    *pp = p;
    return (unsigned int)(p - s);
}

static int amj3d_parse_index_token(const char *tok, unsigned int len, amj3d_i32 *out)
{
    unsigned int i;
    int sign;
    long v;
    int any;
    i = 0U;
    sign = 1;
    v = 0L;
    any = 0;
    if (i < len && tok[i] == '-') {
        sign = -1;
        i++;
    } else if (i < len && tok[i] == '+') {
        i++;
    }
    while (i < len) {
        if (tok[i] == '/') {
            break;
        }
        if (!amj3d_is_digit((unsigned char)tok[i])) {
            return 0;
        }
        any = 1;
        if (v < 107374182L) {
            v = v * 10L + (long)(tok[i] - '0');
        }
        i++;
    }
    if (!any) {
        return 0;
    }
    if (sign < 0) {
        v = -v;
    }
    *out = v;
    return 1;
}

static int amj3d_parse_fixed_token(const char *tok, unsigned int len, amj3d_fixed *out)
{
    int ok;
    ok = 0;
    *out = amj3d_fixed_from_text(tok, len, &ok);
    return ok;
}

static int amj3d_parse_vec3(const char **pp, const char *end, amj3d_fixed *a, amj3d_fixed *b, amj3d_fixed *c)
{
    const char *tok;
    unsigned int len;
    len = amj3d_read_token(pp, end, &tok);
    if (len == 0U || !amj3d_parse_fixed_token(tok, len, a)) {
        return 0;
    }
    len = amj3d_read_token(pp, end, &tok);
    if (len == 0U || !amj3d_parse_fixed_token(tok, len, b)) {
        return 0;
    }
    len = amj3d_read_token(pp, end, &tok);
    if (len == 0U || !amj3d_parse_fixed_token(tok, len, c)) {
        return 0;
    }
    return 1;
}

static int amj3d_parse_vec2(const char **pp, const char *end, amj3d_fixed *a, amj3d_fixed *b)
{
    const char *tok;
    unsigned int len;
    len = amj3d_read_token(pp, end, &tok);
    if (len == 0U || !amj3d_parse_fixed_token(tok, len, a)) {
        return 0;
    }
    len = amj3d_read_token(pp, end, &tok);
    if (len == 0U || !amj3d_parse_fixed_token(tok, len, b)) {
        return 0;
    }
    return 1;
}

static void amj3d_trim_line(const char *line, unsigned int line_len, const char **out_begin, const char **out_end)
{
    const char *b;
    const char *e;
    b = line;
    e = line + line_len;
    while (b < e && amj3d_is_space((unsigned char)*b)) {
        b++;
    }
    while (e > b && amj3d_is_space((unsigned char)e[-1])) {
        e--;
    }
    *out_begin = b;
    *out_end = e;
}

static int amj3d_parse_face_record(const char *p,
                                   const char *end,
                                   const AMJ3D_Config *cfg,
                                   const AMJ3D_Callbacks *cb,
                                   void *user,
                                   AMJ3D_Info *info)
{
    amj3d_i32 idx[AMJ3D_MAX_FACE_INDICES];
    unsigned int count;
    const char *tok;
    unsigned int len;
    int ok;
    unsigned int i;
    amj3d_i32 tri[3];
    (void)cfg;
    count = 0U;
    while (p < end) {
        len = amj3d_read_token(&p, end, &tok);
        if (len == 0U) {
            break;
        }
        if (tok[0] == '#') {
            break;
        }
        if (count < (unsigned int)AMJ3D_MAX_FACE_INDICES) {
            ok = amj3d_parse_index_token(tok, len, &idx[count]);
            if (!ok) {
                amj3d_warn(cb, user, info, AMJ3D_WARN_BAD_NUMBER, "bad face index");
                if ((cfg->flags & AMJ3D_FLAG_STRICT) != 0UL) {
                    return 0;
                }
            } else {
                count++;
            }
        } else {
            amj3d_warn(cb, user, info, AMJ3D_WARN_FACE_TRUNCATED, "face has more indices than AMJ3D_MAX_FACE_INDICES");
        }
    }
    if (count < 3U) {
        return 0;
    }
    if ((cfg->flags & AMJ3D_FLAG_EMIT_TRIANGULATED) != 0UL && count > 3U) {
        i = 1U;
        while (i + 1U < count) {
            tri[0] = idx[0];
            tri[1] = idx[i];
            tri[2] = idx[i + 1U];
            if (cb != AMJ3D_NULL && cb->on_face != AMJ3D_NULL) {
                cb->on_face(user, tri, 3U);
            }
            info->face_count++;
            i++;
        }
    } else {
        if (cb != AMJ3D_NULL && cb->on_face != AMJ3D_NULL) {
            cb->on_face(user, idx, count);
        }
        info->face_count++;
    }
    return 1;
}

static void amj3d_emit_name(void (*fn)(void *, const char *, unsigned int),
                            void *user,
                            const char *p,
                            const char *end)
{
    const char *b;
    const char *e;
    b = p;
    e = end;
    while (b < e && amj3d_is_space((unsigned char)*b)) {
        b++;
    }
    while (e > b && amj3d_is_space((unsigned char)e[-1])) {
        e--;
    }
    if (fn != AMJ3D_NULL && e > b) {
        fn(user, b, (unsigned int)(e - b));
    }
}

static int amj3d_parse_text_line(const char *line,
                                 unsigned int line_len,
                                 const AMJ3D_Config *cfg,
                                 const AMJ3D_Callbacks *cb,
                                 void *user,
                                 AMJ3D_Info *info)
{
    const char *p;
    const char *end;
    const char *tok;
    unsigned int len;
    amj3d_fixed a;
    amj3d_fixed b;
    amj3d_fixed c;
    int consumed;
    amj3d_trim_line(line, line_len, &p, &end);
    if (p >= end) {
        return 1;
    }
    if (*p == '#' || *p == ';') {
        return 1;
    }
    if (*p == '/' && (p + 1) < end && p[1] == '/') {
        return 1;
    }

    len = amj3d_read_token(&p, end, &tok);
    if (len == 0U) {
        return 1;
    }

    consumed = 0;
    if (len == 1U && tok[0] == 'v') {
        if (amj3d_parse_vec3(&p, end, &a, &b, &c)) {
            if (cb != AMJ3D_NULL && cb->on_vertex != AMJ3D_NULL) {
                cb->on_vertex(user, a, b, c);
            }
            info->vertex_count++;
            consumed = 1;
        }
    } else if (len == 2U && tok[0] == 'v' && tok[1] == 'n') {
        if (amj3d_parse_vec3(&p, end, &a, &b, &c)) {
            if (cb != AMJ3D_NULL && cb->on_normal != AMJ3D_NULL) {
                cb->on_normal(user, a, b, c);
            }
            info->normal_count++;
            consumed = 1;
        }
    } else if (len == 2U && tok[0] == 'v' && tok[1] == 't') {
        if (amj3d_parse_vec2(&p, end, &a, &b)) {
            if (cb != AMJ3D_NULL && cb->on_texcoord != AMJ3D_NULL) {
                cb->on_texcoord(user, a, b);
            }
            info->texcoord_count++;
            consumed = 1;
        }
    } else if (len == 1U && tok[0] == 'f') {
        consumed = amj3d_parse_face_record(p, end, cfg, cb, user, info);
    } else if (amj3d_streq_ci_len(tok, len, "vertex") ||
               amj3d_streq_ci_len(tok, len, "point") ||
               amj3d_streq_ci_len(tok, len, "position") ||
               amj3d_streq_ci_len(tok, len, "p")) {
        if (amj3d_parse_vec3(&p, end, &a, &b, &c)) {
            if (cb != AMJ3D_NULL && cb->on_vertex != AMJ3D_NULL) {
                cb->on_vertex(user, a, b, c);
            }
            info->vertex_count++;
            consumed = 1;
        }
    } else if (amj3d_streq_ci_len(tok, len, "normal")) {
        if (amj3d_parse_vec3(&p, end, &a, &b, &c)) {
            if (cb != AMJ3D_NULL && cb->on_normal != AMJ3D_NULL) {
                cb->on_normal(user, a, b, c);
            }
            info->normal_count++;
            consumed = 1;
        }
    } else if (amj3d_streq_ci_len(tok, len, "texcoord") ||
               amj3d_streq_ci_len(tok, len, "uv")) {
        if (amj3d_parse_vec2(&p, end, &a, &b)) {
            if (cb != AMJ3D_NULL && cb->on_texcoord != AMJ3D_NULL) {
                cb->on_texcoord(user, a, b);
            }
            info->texcoord_count++;
            consumed = 1;
        }
    } else if (amj3d_streq_ci_len(tok, len, "face") ||
               amj3d_streq_ci_len(tok, len, "polygon") ||
               amj3d_streq_ci_len(tok, len, "tri") ||
               amj3d_streq_ci_len(tok, len, "quad")) {
        consumed = amj3d_parse_face_record(p, end, cfg, cb, user, info);
    } else if (amj3d_streq_ci_len(tok, len, "usemtl") ||
               amj3d_streq_ci_len(tok, len, "material")) {
        if (cb != AMJ3D_NULL) {
            amj3d_emit_name(cb->on_material, user, p, end);
        }
        consumed = 1;
    } else if (amj3d_streq_ci_len(tok, len, "o") ||
               amj3d_streq_ci_len(tok, len, "g") ||
               amj3d_streq_ci_len(tok, len, "object") ||
               amj3d_streq_ci_len(tok, len, "mesh")) {
        if (cb != AMJ3D_NULL) {
            amj3d_emit_name(cb->on_object, user, p, end);
        }
        consumed = 1;
    }

    if (!consumed && (cfg->flags & AMJ3D_FLAG_STRICT) != 0UL) {
        amj3d_warn(cb, user, info, AMJ3D_WARN_UNKNOWN_RECORD, "unknown text record");
        return 0;
    }
    return 1;
}

static int amj3d_decode_text(const amj3d_u8 *data,
                             unsigned long size,
                             const AMJ3D_Config *cfg,
                             const AMJ3D_Callbacks *cb,
                             void *user,
                             AMJ3D_Info *info)
{
    unsigned long pos;
    unsigned long start;
    unsigned int line_len;
    char line[AMJ3D_LINE_MAX];
    unsigned int i;
    int ok;
    pos = 0UL;
    ok = 1;
    while (pos < size) {
        start = pos;
        while (pos < size && data[pos] != (amj3d_u8)'\n' && data[pos] != (amj3d_u8)'\r') {
            pos++;
        }
        line_len = (unsigned int)(pos - start);
        if (line_len >= AMJ3D_LINE_MAX) {
            line_len = AMJ3D_LINE_MAX - 1U;
        }
        i = 0U;
        while (i < line_len) {
            line[i] = (char)data[start + (unsigned long)i];
            i++;
        }
        line[line_len] = '\0';
        if (!amj3d_parse_text_line(line, line_len, cfg, cb, user, info)) {
            ok = 0;
            if ((cfg->flags & AMJ3D_FLAG_STRICT) != 0UL) {
                return AMJ3D_ERR_PARSE;
            }
        }
        while (pos < size && (data[pos] == (amj3d_u8)'\n' || data[pos] == (amj3d_u8)'\r')) {
            pos++;
        }
    }
    if (!ok) {
        return AMJ3D_ERR_PARSE;
    }
    return AMJ3D_OK;
}

static int amj3d_is_plausible_fixed(amj3d_fixed v, amj3d_fixed limit)
{
    if (v < 0L) {
        if (v < -limit) {
            return 0;
        }
    } else {
        if (v > limit) {
            return 0;
        }
    }
    return 1;
}

static int amj3d_binary_scan_triplets(const amj3d_u8 *data,
                                      unsigned long size,
                                      const AMJ3D_Config *cfg,
                                      const AMJ3D_Callbacks *cb,
                                      void *user,
                                      AMJ3D_Info *info)
{
    unsigned long off;
    int ok0;
    int ok1;
    int ok2;
    int cl0;
    int cl1;
    int cl2;
    amj3d_fixed x;
    amj3d_fixed y;
    amj3d_fixed z;
    amj3d_u32 b0;
    amj3d_u32 b1;
    amj3d_u32 b2;
    unsigned int emitted;
    amj3d_fixed limit;
    limit = cfg->coord_abs_limit;
    if (limit <= 0L) {
        limit = AMJ3D_DEFAULT_COORD_LIMIT;
    }
    emitted = 0U;
    if (size < 12UL) {
        return AMJ3D_OK;
    }
    off = 0UL;
    while (off + 12UL <= size) {
        b0 = amj3d_read_u32_le(data + off);
        b1 = amj3d_read_u32_le(data + off + 4UL);
        b2 = amj3d_read_u32_le(data + off + 8UL);
        x = amj3d_fixed_from_ieee754_u32(b0, &ok0, &cl0);
        y = amj3d_fixed_from_ieee754_u32(b1, &ok1, &cl1);
        z = amj3d_fixed_from_ieee754_u32(b2, &ok2, &cl2);
        if (ok0 && ok1 && ok2 &&
            amj3d_is_plausible_fixed(x, limit) &&
            amj3d_is_plausible_fixed(y, limit) &&
            amj3d_is_plausible_fixed(z, limit)) {
            if (cb != AMJ3D_NULL && cb->on_vertex != AMJ3D_NULL) {
                cb->on_vertex(user, x, y, z);
            }
            info->vertex_count++;
            emitted++;
            off += 12UL;
        } else {
            b0 = amj3d_read_u32_be(data + off);
            b1 = amj3d_read_u32_be(data + off + 4UL);
            b2 = amj3d_read_u32_be(data + off + 8UL);
            x = amj3d_fixed_from_ieee754_u32(b0, &ok0, &cl0);
            y = amj3d_fixed_from_ieee754_u32(b1, &ok1, &cl1);
            z = amj3d_fixed_from_ieee754_u32(b2, &ok2, &cl2);
            if (ok0 && ok1 && ok2 &&
                amj3d_is_plausible_fixed(x, limit) &&
                amj3d_is_plausible_fixed(y, limit) &&
                amj3d_is_plausible_fixed(z, limit)) {
                if (cb != AMJ3D_NULL && cb->on_vertex != AMJ3D_NULL) {
                    cb->on_vertex(user, x, y, z);
                }
                info->vertex_count++;
                emitted++;
                off += 12UL;
            } else {
                off++;
            }
        }
        if (emitted == 1U) {
            amj3d_warn(cb, user, info, AMJ3D_WARN_POSSIBLE_FALSE_POS,
                       "binary scan emits candidate float triplets; verify with real AMJ samples");
        }
    }
    return AMJ3D_OK;
}

void amj3d_default_config(AMJ3D_Config *cfg)
{
    if (cfg == AMJ3D_NULL) {
        return;
    }
    cfg->flags = AMJ3D_FLAG_ACCEPT_OBJ_LIKE;
    cfg->coord_abs_limit = AMJ3D_DEFAULT_COORD_LIMIT;
    cfg->binary_stride_min = 12U;
    cfg->binary_stride_max = 64U;
}

int amj3d_probe(const amj3d_u8 *data, unsigned long size)
{
    if (data == AMJ3D_NULL || size == 0UL) {
        return AMJ3D_KIND_UNKNOWN;
    }
    if (amj3d_mem_contains_ci(data, size, "AMJ") ||
        amj3d_mem_contains_ci(data, size, "Amapi") ||
        amj3d_mem_contains_ci(data, size, "AMAPI") ||
        amj3d_mem_contains_ci(data, size, "Eovia") ||
        amj3d_mem_contains_ci(data, size, "Yonowat")) {
        if (amj3d_looks_text(data, size)) {
            return AMJ3D_KIND_AMJ_TEXT;
        }
        return AMJ3D_KIND_AMJ_BINARY;
    }
    if (amj3d_looks_text(data, size)) {
        if (amj3d_mem_contains_ci(data, size, "\nv ") ||
            amj3d_mem_contains_ci(data, size, "\nf ") ||
            amj3d_mem_contains_ci(data, size, "vertex") ||
            amj3d_mem_contains_ci(data, size, "face")) {
            return AMJ3D_KIND_OBJ_LIKE_TEXT;
        }
        return AMJ3D_KIND_TEXT;
    }
    return AMJ3D_KIND_UNKNOWN;
}

int amj3d_decode(const amj3d_u8 *data,
                 unsigned long size,
                 const AMJ3D_Config *cfg_in,
                 const AMJ3D_Callbacks *cb,
                 void *user)
{
    AMJ3D_Config cfg_stack;
    const AMJ3D_Config *cfg;
    AMJ3D_Info info;
    int kind;
    int status;

    if (data == AMJ3D_NULL) {
        return AMJ3D_ERR_BAD_ARGUMENT;
    }
    if (size == 0UL) {
        return AMJ3D_ERR_EMPTY;
    }
    if (cfg_in == AMJ3D_NULL) {
        amj3d_default_config(&cfg_stack);
        cfg = &cfg_stack;
    } else {
        cfg = cfg_in;
    }

    kind = amj3d_probe(data, size);
    info.kind = kind;
    info.size = size;
    info.flags_used = cfg->flags;
    info.vertex_count = 0UL;
    info.normal_count = 0UL;
    info.texcoord_count = 0UL;
    info.face_count = 0UL;
    info.warning_count = 0UL;

    if (cb != AMJ3D_NULL && cb->on_begin != AMJ3D_NULL) {
        cb->on_begin(user, &info);
    }

    status = AMJ3D_OK;
    if (kind == AMJ3D_KIND_AMJ_BINARY || kind == AMJ3D_KIND_UNKNOWN) {
        if (kind == AMJ3D_KIND_AMJ_BINARY) {
            amj3d_warn(cb, user, &info, AMJ3D_WARN_PROPRIETARY_BINARY,
                       "AMJ/Amapi binary appears proprietary; public full spec not available");
        }
        if ((cfg->flags & AMJ3D_FLAG_REPORT_RAW_CHUNKS) != 0UL && cb != AMJ3D_NULL && cb->on_raw_chunk != AMJ3D_NULL) {
            cb->on_raw_chunk(user, 0UL, data, size);
        }
        if ((cfg->flags & AMJ3D_FLAG_BINARY_SCAN) != 0UL) {
            amj3d_warn(cb, user, &info, AMJ3D_WARN_BINARY_SCAN_ACTIVE,
                       "forensic binary scan enabled");
            status = amj3d_binary_scan_triplets(data, size, cfg, cb, user, &info);
        } else if (kind == AMJ3D_KIND_AMJ_BINARY) {
            status = AMJ3D_ERR_UNSUPPORTED_BINARY;
        } else {
            status = AMJ3D_ERR_PARSE;
        }
    } else {
        if (kind == AMJ3D_KIND_TEXT || kind == AMJ3D_KIND_AMJ_TEXT) {
            amj3d_warn(cb, user, &info, AMJ3D_WARN_TEXT_FALLBACK,
                       "text fallback decoder active");
        }
        status = amj3d_decode_text(data, size, cfg, cb, user, &info);
    }

    if (cb != AMJ3D_NULL && cb->on_end != AMJ3D_NULL) {
        cb->on_end(user, &info, status);
    }
    return status;
}

amj3d_fixed amj3d_fixed_from_i32(amj3d_i32 v)
{
    if (v > 32767L) {
        return 2147418112L;
    }
    if (v < -32768L) {
        return -2147483647L;
    }
    return v * AMJ3D_FIXED_ONE;
}

amj3d_i32 amj3d_fixed_to_i32(amj3d_fixed v)
{
    if (v >= 0L) {
        return v / AMJ3D_FIXED_ONE;
    }
    return -((-v) / AMJ3D_FIXED_ONE);
}

amj3d_fixed amj3d_fixed_mul(amj3d_fixed a, amj3d_fixed b)
{
    /* Portable C89 fallback: intentionally avoids long long.
       Good enough for small fixed values. For heavy math, replace with
       target-specific 64-bit or assembly backend outside this decoder. */
    amj3d_i32 ai;
    amj3d_i32 af;
    amj3d_i32 bi;
    amj3d_i32 bf;
    amj3d_fixed r;
    int neg;
    neg = 0;
    if (a < 0L) {
        a = -a;
        neg = !neg;
    }
    if (b < 0L) {
        b = -b;
        neg = !neg;
    }
    ai = a / AMJ3D_FIXED_ONE;
    af = a % AMJ3D_FIXED_ONE;
    bi = b / AMJ3D_FIXED_ONE;
    bf = b % AMJ3D_FIXED_ONE;
    r = ai * bi * AMJ3D_FIXED_ONE;
    r += ai * bf;
    r += bi * af;
    r += (af / 256L) * (bf / 256L);
    if (neg) {
        r = -r;
    }
    return r;
}

amj3d_fixed amj3d_fixed_div(amj3d_fixed a, amj3d_fixed b)
{
    amj3d_fixed q;
    amj3d_fixed rem;
    int neg;
    int i;
    if (b == 0L) {
        if (a < 0L) {
            return -2147483647L;
        }
        return 2147483647L;
    }
    neg = 0;
    if (a < 0L) {
        a = -a;
        neg = !neg;
    }
    if (b < 0L) {
        b = -b;
        neg = !neg;
    }
    q = a / b;
    rem = a % b;
    q = q * AMJ3D_FIXED_ONE;
    i = 0;
    while (i < AMJ3D_FIXED_SHIFT) {
        rem = rem * 2L;
        q = q * 2L;
        if (rem >= b) {
            q += 1L;
            rem -= b;
        }
        i++;
    }
    if (neg) {
        q = -q;
    }
    return q;
}

amj3d_fixed amj3d_fixed_from_text(const char *text, unsigned int len, int *ok)
{
    unsigned int i;
    int sign;
    amj3d_fixed val;
    amj3d_fixed frac;
    amj3d_fixed denom;
    int any;
    int exp_sign;
    long exp_val;
    amj3d_fixed limit;
    i = 0U;
    sign = 1;
    val = 0L;
    frac = 0L;
    denom = 1L;
    any = 0;
    exp_sign = 1;
    exp_val = 0L;
    limit = 2147483647L / 10L;
    if (ok != AMJ3D_NULL) {
        *ok = 0;
    }
    if (len == 0U || text == AMJ3D_NULL) {
        return 0L;
    }
    if (text[i] == '-') {
        sign = -1;
        i++;
    } else if (text[i] == '+') {
        i++;
    }
    while (i < len && amj3d_is_digit((unsigned char)text[i])) {
        any = 1;
        if (val < limit) {
            val = val * 10L + (long)(text[i] - '0');
        }
        i++;
    }
    val = val * AMJ3D_FIXED_ONE;
    if (i < len && text[i] == '.') {
        i++;
        while (i < len && amj3d_is_digit((unsigned char)text[i]) && denom < 1000000L) {
            any = 1;
            denom = denom * 10L;
            frac += ((long)(text[i] - '0') * AMJ3D_FIXED_ONE) / denom;
            i++;
        }
        while (i < len && amj3d_is_digit((unsigned char)text[i])) {
            i++;
        }
    }
    val += frac;
    if (i < len && (text[i] == 'e' || text[i] == 'E')) {
        i++;
        if (i < len && text[i] == '-') {
            exp_sign = -1;
            i++;
        } else if (i < len && text[i] == '+') {
            i++;
        }
        while (i < len && amj3d_is_digit((unsigned char)text[i])) {
            if (exp_val < 8L) {
                exp_val = exp_val * 10L + (long)(text[i] - '0');
            }
            i++;
        }
        while (exp_val > 0L) {
            if (exp_sign > 0) {
                if (val < limit) {
                    val = val * 10L;
                }
            } else {
                val = val / 10L;
            }
            exp_val--;
        }
    }
    if (!any) {
        return 0L;
    }
    if (sign < 0) {
        val = -val;
    }
    if (ok != AMJ3D_NULL) {
        *ok = 1;
    }
    return val;
}

amj3d_fixed amj3d_fixed_from_ieee754_u32(amj3d_u32 bits, int *ok, int *clamped)
{
    amj3d_u32 sign;
    amj3d_u32 exp;
    amj3d_u32 mant;
    amj3d_u32 value;
    int shift;
    amj3d_fixed out;
    if (ok != AMJ3D_NULL) {
        *ok = 0;
    }
    if (clamped != AMJ3D_NULL) {
        *clamped = 0;
    }
    sign = bits & 0x80000000UL;
    exp = (bits >> 23) & 0xFFUL;
    mant = bits & 0x7FFFFFUL;
    if (exp == 255UL) {
        return 0L;
    }
    if (exp == 0UL) {
        if (mant == 0UL) {
            if (ok != AMJ3D_NULL) {
                *ok = 1;
            }
            return 0L;
        }
        return 0L;
    }
    value = mant | 0x800000UL;
    shift = (int)exp - 127 - 7;
    if (shift >= 0) {
        if (shift > 7) {
            if (clamped != AMJ3D_NULL) {
                *clamped = 1;
            }
            out = 2147483647L;
        } else {
            out = (amj3d_fixed)(value << shift);
        }
    } else {
        shift = -shift;
        if (shift >= 31) {
            out = 0L;
        } else {
            out = (amj3d_fixed)(value >> shift);
        }
    }
    if (sign != 0UL) {
        out = -out;
    }
    if (ok != AMJ3D_NULL) {
        *ok = 1;
    }
    return out;
}

const char *amj3d_status_string(int status)
{
    switch (status) {
    case AMJ3D_OK:
        return "ok";
    case AMJ3D_ERR_BAD_ARGUMENT:
        return "bad argument";
    case AMJ3D_ERR_EMPTY:
        return "empty input";
    case AMJ3D_ERR_UNSUPPORTED_BINARY:
        return "unsupported proprietary AMJ binary";
    case AMJ3D_ERR_PARSE:
        return "parse error";
    case AMJ3D_ERR_LIMIT:
        return "limit reached";
    default:
        return "unknown status";
    }
}
