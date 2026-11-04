#include "cob_private.h"

void cob_scene_clear(cob_scene *scene)
{
    if (scene != 0) {
        memset(scene, 0, sizeof(*scene));
        scene->little_endian = 1;
        scene->header_version[0] = 'V';
        scene->header_version[1] = '0';
        scene->header_version[2] = '0';
        scene->header_version[3] = '.';
        scene->header_version[4] = '0';
        scene->header_version[5] = '1';
        scene->header_version[6] = '\0';
    }
}

const char *cob_status_text(int status)
{
    switch (status) {
    case COB_OK: return "ok";
    case COB_ERR_ARGUMENT: return "bad argument";
    case COB_ERR_BAD_MAGIC: return "bad Caligari/COB header";
    case COB_ERR_UNSUPPORTED_ENDIAN: return "unsupported endian mode";
    case COB_ERR_UNSUPPORTED_FORMAT: return "unsupported COB flavor";
    case COB_ERR_TRUNCATED: return "truncated file/chunk";
    case COB_ERR_CAPACITY: return "static scene capacity exceeded";
    case COB_ERR_PARSE: return "parse error";
    case COB_ERR_WRITE: return "write error";
    default: return "unknown status";
    }
}

void cob_diag_add(cob_scene *scene, int code, cob_i32 chunk_id, const char *type, const char *text)
{
    cob_diag *d;
    unsigned long i;

    if (scene == 0) {
        return;
    }
    if (scene->diagnostic_count >= COB_DIAG_MAX) {
        return;
    }
    d = &scene->diagnostics[scene->diagnostic_count++];
    memset(d, 0, sizeof(*d));
    d->code = code;
    d->chunk_id = chunk_id;
    if (type != 0) {
        for (i = 0; i < 4 && type[i] != '\0'; ++i) {
            d->chunk_type[i] = type[i];
        }
        d->chunk_type[4] = '\0';
    }
    if (text != 0) {
        cob_copy_name(d->text, COB_ARRAY_COUNT(d->text), text, cob_strlen_ul(text));
    }
}

unsigned long cob_strlen_ul(const char *s)
{
    unsigned long n;
    n = 0;
    if (s == 0) {
        return 0;
    }
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

void cob_copy_name(char *dst, unsigned long dst_size, const char *src, unsigned long src_len)
{
    unsigned long i;
    unsigned long maxn;

    if (dst == 0 || dst_size == 0) {
        return;
    }
    maxn = dst_size - 1;
    if (src_len < maxn) {
        maxn = src_len;
    }
    for (i = 0; i < maxn; ++i) {
        dst[i] = src[i];
    }
    dst[maxn] = '\0';
}

void cob_set_identity(cob_fixed m[16])
{
    int i;
    for (i = 0; i < 16; ++i) {
        m[i] = 0;
    }
    m[0] = COB_FIXED_ONE;
    m[5] = COB_FIXED_ONE;
    m[10] = COB_FIXED_ONE;
    m[15] = COB_FIXED_ONE;
}

void cob_zero_axes(cob_fixed a[12])
{
    int i;
    for (i = 0; i < 12; ++i) {
        a[i] = 0;
    }
    a[3] = COB_FIXED_ONE;
    a[7] = COB_FIXED_ONE;
    a[11] = COB_FIXED_ONE;
}

int cob_starts_with(const char *s, const char *prefix)
{
    unsigned long i;
    if (s == 0 || prefix == 0) {
        return 0;
    }
    for (i = 0; prefix[i] != '\0'; ++i) {
        if (s[i] != prefix[i]) {
            return 0;
        }
    }
    return 1;
}

const char *cob_skip_spaces(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        ++p;
    }
    return p;
}

const char *cob_next_token(const char *p, const char *end, const char **tok, unsigned long *len)
{
    const char *s;
    const char *e;

    s = cob_skip_spaces(p, end);
    e = s;
    while (e < end && *e != ' ' && *e != '\t' && *e != '\r' && *e != '\n') {
        ++e;
    }
    if (tok != 0) {
        *tok = s;
    }
    if (len != 0) {
        *len = (unsigned long)(e - s);
    }
    return e;
}

long cob_parse_i32_token(const char *p, const char *end, int *ok)
{
    long sign;
    long val;
    int any;

    sign = 1;
    val = 0;
    any = 0;
    p = cob_skip_spaces(p, end);
    if (p < end && *p == '-') {
        sign = -1;
        ++p;
    } else if (p < end && *p == '+') {
        ++p;
    }
    while (p < end && *p >= '0' && *p <= '9') {
        any = 1;
        if (val <= 214748364L) {
            val = val * 10L + (long)(*p - '0');
        }
        ++p;
    }
    if (ok != 0) {
        *ok = any;
    }
    return sign * val;
}

unsigned long cob_parse_u32_token(const char *p, const char *end, int *ok)
{
    unsigned long val;
    int any;

    val = 0;
    any = 0;
    p = cob_skip_spaces(p, end);
    if (p < end && *p == '+') {
        ++p;
    }
    while (p < end && *p >= '0' && *p <= '9') {
        any = 1;
        if (val <= 429496729UL / 10UL) {
            val = val * 10UL + (unsigned long)(*p - '0');
        }
        ++p;
    }
    if (ok != 0) {
        *ok = any;
    }
    return val;
}

int cob_find_token_i32(const char *line, const char *token, long *out)
{
    const char *p;
    const char *q;
    int ok;

    if (line == 0 || token == 0 || out == 0) {
        return 0;
    }
    p = line;
    while (*p != '\0') {
        q = token;
        while (*q != '\0' && p[q - token] == *q) {
            ++q;
        }
        if (*q == '\0') {
            p += cob_strlen_ul(token);
            *out = cob_parse_i32_token(p, p + cob_strlen_ul(p), &ok);
            return ok;
        }
        ++p;
    }
    return 0;
}

int cob_match_chunk_header(const char *line, char type_out[5])
{
    if (line == 0) {
        return 0;
    }
    if (line[0] == '\0' || line[1] == '\0' || line[2] == '\0' || line[3] == '\0') {
        return 0;
    }
    if (line[4] != ' ' && line[4] != '\t') {
        return 0;
    }
    if (line[5] != 'V') {
        return 0;
    }
    if (type_out != 0) {
        type_out[0] = line[0];
        type_out[1] = line[1];
        type_out[2] = line[2];
        type_out[3] = line[3];
        type_out[4] = '\0';
    }
    return 1;
}

int cob_parse_chunk_info_ascii(cob_chunk_info *ci, const char *line)
{
    const char *p;
    const char *end;
    long tmp;
    int ok;

    if (ci == 0 || line == 0) {
        return 0;
    }
    memset(ci, 0, sizeof(*ci));
    if (!cob_match_chunk_header(line, ci->type)) {
        return 0;
    }
    end = line + cob_strlen_ul(line);
    p = line + 6;
    ci->major = 0;
    ci->minor = 0;
    if (p < end && *p >= '0' && *p <= '9') {
        ci->major = (cob_u16)(*p - '0');
    }
    while (p < end && *p != '.') {
        ++p;
    }
    if (p < end && *p == '.') {
        ++p;
        tmp = cob_parse_i32_token(p, end, &ok);
        if (ok) {
            ci->minor = (cob_u16)tmp;
        }
    }
    if (cob_find_token_i32(line, "Id", &tmp)) {
        ci->id = (cob_i32)tmp;
    }
    if (cob_find_token_i32(line, "Parent", &tmp)) {
        ci->parent_id = (cob_i32)tmp;
    }
    if (cob_find_token_i32(line, "Size", &tmp)) {
        ci->size = (cob_i32)tmp;
    } else {
        ci->size = -1;
    }
    return 1;
}

int cob_emit_n(cob_write_fn fn, void *user, const void *bytes, unsigned long count)
{
    if (fn == 0) {
        return COB_ERR_ARGUMENT;
    }
    if (count == 0) {
        return COB_OK;
    }
    if (fn(user, bytes, count) != 0) {
        return COB_ERR_WRITE;
    }
    return COB_OK;
}

int cob_emit(cob_write_fn fn, void *user, const char *s)
{
    return cob_emit_n(fn, user, s, cob_strlen_ul(s));
}

int cob_emit_i(cob_write_fn fn, void *user, long v)
{
    char buf[48];
    sprintf(buf, "%ld", v);
    return cob_emit(fn, user, buf);
}

int cob_emit_u(cob_write_fn fn, void *user, unsigned long v)
{
    char buf[48];
    sprintf(buf, "%lu", v);
    return cob_emit(fn, user, buf);
}

int cob_emit_fixed(cob_write_fn fn, void *user, cob_fixed v, int decimals)
{
    char buf[64];
    if (!cob_fixed_to_cstr(v, buf, sizeof(buf), decimals)) {
        return COB_ERR_PARSE;
    }
    return cob_emit(fn, user, buf);
}

cob_u16 cob_get_u16_le(const unsigned char *p)
{
    return (cob_u16)(((cob_u16)p[0]) | ((cob_u16)p[1] << 8));
}

cob_i16 cob_get_i16_le(const unsigned char *p)
{
    return (cob_i16)cob_get_u16_le(p);
}

cob_u32 cob_get_u32_le(const unsigned char *p)
{
    cob_u32 v;
    v = (cob_u32)p[0];
    v |= ((cob_u32)p[1]) << 8;
    v |= ((cob_u32)p[2]) << 16;
    v |= ((cob_u32)p[3]) << 24;
    return v;
}

cob_i32 cob_get_i32_le(const unsigned char *p)
{
    return (cob_i32)cob_get_u32_le(p);
}

void cob_put_u16_le(unsigned char *p, cob_u16 v)
{
    p[0] = (unsigned char)(v & 255U);
    p[1] = (unsigned char)((v >> 8) & 255U);
}

void cob_put_i16_le(unsigned char *p, cob_i16 v)
{
    cob_put_u16_le(p, (cob_u16)v);
}

void cob_put_u32_le(unsigned char *p, cob_u32 v)
{
    p[0] = (unsigned char)(v & 255UL);
    p[1] = (unsigned char)((v >> 8) & 255UL);
    p[2] = (unsigned char)((v >> 16) & 255UL);
    p[3] = (unsigned char)((v >> 24) & 255UL);
}

void cob_put_i32_le(unsigned char *p, cob_i32 v)
{
    cob_put_u32_le(p, (cob_u32)v);
}
