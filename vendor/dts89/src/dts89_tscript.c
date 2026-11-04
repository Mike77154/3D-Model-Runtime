#include "dts89.h"
#include <string.h>


static int ts_is_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static int ts_is_alpha(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int ts_is_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static int ts_is_alnum(char ch)
{
    return ts_is_alpha(ch) || ts_is_digit(ch);
}

typedef struct dts89_ts_parser {
    const char *text;
    dts89_u32 size;
    dts89_u32 pos;
} dts89_ts_parser;

typedef struct dts89_span {
    const char *ptr;
    dts89_u32 len;
} dts89_span;

static void ts_skip_ws(dts89_ts_parser *p)
{
    int again;
    do {
        again = 0;
        while (p->pos < p->size && ts_is_space(p->text[p->pos])) p->pos++;
        if (p->pos + 1u < p->size && p->text[p->pos] == '/' && p->text[p->pos + 1u] == '/') {
            p->pos += 2u;
            while (p->pos < p->size && p->text[p->pos] != '\n') p->pos++;
            again = 1;
        } else if (p->pos + 1u < p->size && p->text[p->pos] == '/' && p->text[p->pos + 1u] == '*') {
            p->pos += 2u;
            while (p->pos + 1u < p->size) {
                if (p->text[p->pos] == '*' && p->text[p->pos + 1u] == '/') {
                    p->pos += 2u;
                    break;
                }
                p->pos++;
            }
            again = 1;
        }
    } while (again);
}

static int ts_parse_ident(dts89_ts_parser *p, dts89_span *out)
{
    dts89_u32 start;
    if (!p || !out) return DTS89_ERR_BAD_ARG;
    ts_skip_ws(p);
    if (p->pos >= p->size) return DTS89_ERR_TRUNCATED;
    if (!(ts_is_alpha(p->text[p->pos]) || p->text[p->pos] == '_')) return DTS89_ERR_FORMAT;
    start = p->pos++;
    while (p->pos < p->size) {
        char ch = p->text[p->pos];
        if (!(ts_is_alnum(ch) || ch == '_' || ch == ':')) break;
        p->pos++;
    }
    out->ptr = p->text + start;
    out->len = p->pos - start;
    return DTS89_OK;
}

static int ts_ident_eq(const dts89_span *s, const char *kw)
{
    size_t n;
    if (!s || !kw) return DTS89_FALSE;
    n = strlen(kw);
    if (s->len != (dts89_u32)n) return DTS89_FALSE;
    return memcmp(s->ptr, kw, n) == 0 ? DTS89_TRUE : DTS89_FALSE;
}

static int ts_expect_char(dts89_ts_parser *p, char ch)
{
    ts_skip_ws(p);
    if (p->pos >= p->size) return DTS89_ERR_TRUNCATED;
    if (p->text[p->pos] != ch) return DTS89_ERR_FORMAT;
    p->pos++;
    return DTS89_OK;
}

static dts89_span ts_trim_span(dts89_span s)
{
    while (s.len && ts_is_space(s.ptr[0])) {
        s.ptr++;
        s.len--;
    }
    while (s.len && ts_is_space(s.ptr[s.len - 1u])) {
        s.len--;
    }
    return s;
}

static int ts_parse_paren_name(dts89_ts_parser *p, dts89_span *out)
{
    dts89_u32 start;
    int rc;
    rc = ts_expect_char(p, '('); if (rc) return rc;
    start = p->pos;
    while (p->pos < p->size && p->text[p->pos] != ')') p->pos++;
    if (p->pos >= p->size) return DTS89_ERR_TRUNCATED;
    out->ptr = p->text + start;
    out->len = p->pos - start;
    *out = ts_trim_span(*out);
    p->pos++;
    return DTS89_OK;
}

static int ts_parse_index(dts89_ts_parser *p, dts89_s32 *out_index)
{
    dts89_u32 value;
    ts_skip_ws(p);
    if (p->pos >= p->size || p->text[p->pos] != '[') {
        *out_index = -1;
        return DTS89_OK;
    }
    p->pos++;
    ts_skip_ws(p);
    value = 0u;
    if (p->pos >= p->size || !ts_is_digit(p->text[p->pos])) return DTS89_ERR_FORMAT;
    while (p->pos < p->size && ts_is_digit(p->text[p->pos])) {
        value = value * 10u + (dts89_u32)(p->text[p->pos] - '0');
        p->pos++;
    }
    ts_skip_ws(p);
    if (p->pos >= p->size || p->text[p->pos] != ']') return DTS89_ERR_FORMAT;
    p->pos++;
    *out_index = (dts89_s32)value;
    return DTS89_OK;
}

static int ts_parse_value(dts89_ts_parser *p, dts89_span *out, dts89_u32 *quoted)
{
    dts89_u32 start;
    if (!p || !out || !quoted) return DTS89_ERR_BAD_ARG;
    ts_skip_ws(p);
    if (p->pos >= p->size) return DTS89_ERR_TRUNCATED;
    if (p->text[p->pos] == '"') {
        *quoted = 1u;
        p->pos++;
        start = p->pos;
        while (p->pos < p->size) {
            char ch = p->text[p->pos];
            if (ch == '\\' && p->pos + 1u < p->size) {
                p->pos += 2u;
                continue;
            }
            if (ch == '"') break;
            p->pos++;
        }
        if (p->pos >= p->size) return DTS89_ERR_TRUNCATED;
        out->ptr = p->text + start;
        out->len = p->pos - start;
        p->pos++;
        return DTS89_OK;
    }
    *quoted = 0u;
    start = p->pos;
    while (p->pos < p->size && p->text[p->pos] != ';') p->pos++;
    if (p->pos >= p->size) return DTS89_ERR_TRUNCATED;
    out->ptr = p->text + start;
    out->len = p->pos - start;
    *out = ts_trim_span(*out);
    return DTS89_OK;
}

static char *ts_copy_span(dts89_arena *arena, dts89_span s, dts89_u32 *out_len)
{
    char *dst;
    if (!arena) return 0;
    dst = (char*)dts89_arena_alloc(arena, s.len + 1u, 1u);
    if (!dst) return 0;
    if (s.len) memcpy(dst, s.ptr, (size_t)s.len);
    dst[s.len] = '\0';
    if (out_len) *out_len = s.len;
    return dst;
}

static char *ts_copy_quoted(dts89_arena *arena, dts89_span s, dts89_u32 *out_len)
{
    char *dst;
    dts89_u32 i;
    dts89_u32 used;
    if (!arena) return 0;
    dst = (char*)dts89_arena_alloc(arena, s.len + 1u, 1u);
    if (!dst) return 0;
    used = 0u;
    i = 0u;
    while (i < s.len) {
        char ch = s.ptr[i++];
        if (ch == '\\' && i < s.len) {
            ch = s.ptr[i++];
            if (ch == 'n') ch = '\n';
            else if (ch == 'r') ch = '\r';
            else if (ch == 't') ch = '\t';
        }
        dst[used++] = ch;
    }
    dst[used] = '\0';
    if (out_len) *out_len = used;
    return dst;
}

static int ts_count_objects(const char *text, dts89_u32 size, dts89_s32 *out_count)
{
    dts89_ts_parser p;
    dts89_span kw, cls, name;
    int rc;
    dts89_s32 count;
    p.text = text;
    p.size = size;
    p.pos = 0u;
    count = 0;
    for (;;) {
        ts_skip_ws(&p);
        if (p.pos >= p.size) break;
        rc = ts_parse_ident(&p, &kw); if (rc) return rc;
        if (!ts_ident_eq(&kw, "singleton")) return DTS89_ERR_FORMAT;
        rc = ts_parse_ident(&p, &cls); if (rc) return rc;
        rc = ts_parse_paren_name(&p, &name); if (rc) return rc;
        while (p.pos < p.size && p.text[p.pos] != '{') p.pos++;
        if (p.pos >= p.size) return DTS89_ERR_TRUNCATED;
        p.pos++;
        while (p.pos < p.size) {
            ts_skip_ws(&p);
            if (p.pos >= p.size) return DTS89_ERR_TRUNCATED;
            if (p.text[p.pos] == '}') {
                p.pos++;
                break;
            }
            while (p.pos < p.size && p.text[p.pos] != ';') p.pos++;
            if (p.pos >= p.size) return DTS89_ERR_TRUNCATED;
            p.pos++;
        }
        ts_skip_ws(&p);
        if (p.pos < p.size && p.text[p.pos] == ';') p.pos++;
        count++;
    }
    *out_count = count;
    return DTS89_OK;
}

static int ts_count_assignments(const char *text, dts89_u32 size, dts89_s32 num_objects, dts89_s32 *counts)
{
    dts89_ts_parser p;
    dts89_span kw, cls, name, key, value;
    dts89_u32 quoted;
    dts89_s32 idx;
    dts89_s32 obj;
    int rc;
    p.text = text;
    p.size = size;
    p.pos = 0u;
    memset(counts, 0, (size_t)num_objects * sizeof(dts89_s32));
    obj = 0;
    while (obj < num_objects) {
        ts_skip_ws(&p);
        rc = ts_parse_ident(&p, &kw); if (rc) return rc;
        rc = ts_parse_ident(&p, &cls); if (rc) return rc;
        rc = ts_parse_paren_name(&p, &name); if (rc) return rc;
        while (p.pos < p.size && p.text[p.pos] != '{') p.pos++;
        if (p.pos >= p.size) return DTS89_ERR_TRUNCATED;
        p.pos++;
        for (;;) {
            ts_skip_ws(&p);
            if (p.pos >= p.size) return DTS89_ERR_TRUNCATED;
            if (p.text[p.pos] == '}') {
                p.pos++;
                break;
            }
            rc = ts_parse_ident(&p, &key); if (rc) return rc;
            rc = ts_parse_index(&p, &idx); if (rc) return rc;
            rc = ts_expect_char(&p, '='); if (rc) return rc;
            rc = ts_parse_value(&p, &value, &quoted); if (rc) return rc;
            rc = ts_expect_char(&p, ';'); if (rc) return rc;
            counts[obj]++;
        }
        ts_skip_ws(&p);
        if (p.pos < p.size && p.text[p.pos] == ';') p.pos++;
        obj++;
    }
    return DTS89_OK;
}

int dts89_tscript_parse_singletons(const void *text, dts89_u32 size, dts89_arena *arena, dts89_ts_file *out_file)
{
    dts89_s32 num_objects;
    dts89_s32 *counts;
    dts89_ts_parser p;
    dts89_s32 obj;
    int rc;
    if (!text || !arena || !out_file) return DTS89_ERR_BAD_ARG;
    memset(out_file, 0, sizeof(*out_file));
    rc = ts_count_objects((const char*)text, size, &num_objects);
    if (rc) return rc;
    out_file->num_objects = num_objects;
    out_file->objects = (dts89_ts_object*)dts89_arena_alloc(arena, (dts89_u32)num_objects * sizeof(dts89_ts_object), 4u);
    if (!out_file->objects && num_objects) return DTS89_ERR_NO_SPACE;
    memset(out_file->objects, 0, (size_t)num_objects * sizeof(dts89_ts_object));
    counts = (dts89_s32*)dts89_arena_alloc(arena, (dts89_u32)num_objects * sizeof(dts89_s32), 4u);
    if (!counts && num_objects) return DTS89_ERR_NO_SPACE;
    rc = ts_count_assignments((const char*)text, size, num_objects, counts);
    if (rc) return rc;
    for (obj = 0; obj < num_objects; ++obj) {
        out_file->objects[obj].num_assignments = counts[obj];
        out_file->objects[obj].assignments = (dts89_ts_assignment*)dts89_arena_alloc(arena, (dts89_u32)counts[obj] * sizeof(dts89_ts_assignment), 4u);
        if (!out_file->objects[obj].assignments && counts[obj]) return DTS89_ERR_NO_SPACE;
        memset(out_file->objects[obj].assignments, 0, (size_t)counts[obj] * sizeof(dts89_ts_assignment));
    }

    p.text = (const char*)text;
    p.size = size;
    p.pos = 0u;
    for (obj = 0; obj < num_objects; ++obj) {
        dts89_ts_object *o = &out_file->objects[obj];
        dts89_span kw, cls, name;
        dts89_s32 aidx;
        rc = ts_parse_ident(&p, &kw); if (rc) return rc;
        rc = ts_parse_ident(&p, &cls); if (rc) return rc;
        rc = ts_parse_paren_name(&p, &name); if (rc) return rc;
        o->class_name.ptr = ts_copy_span(arena, cls, &o->class_name.len);
        if (!o->class_name.ptr) return DTS89_ERR_NO_SPACE;
        o->object_name.ptr = ts_copy_span(arena, name, &o->object_name.len);
        if (!o->object_name.ptr) return DTS89_ERR_NO_SPACE;
        while (p.pos < p.size && p.text[p.pos] != '{') p.pos++;
        if (p.pos >= p.size) return DTS89_ERR_TRUNCATED;
        p.pos++;
        aidx = 0;
        for (;;) {
            dts89_span key, value;
            dts89_u32 quoted;
            dts89_s32 array_index;
            ts_skip_ws(&p);
            if (p.pos >= p.size) return DTS89_ERR_TRUNCATED;
            if (p.text[p.pos] == '}') {
                p.pos++;
                break;
            }
            rc = ts_parse_ident(&p, &key); if (rc) return rc;
            rc = ts_parse_index(&p, &array_index); if (rc) return rc;
            rc = ts_expect_char(&p, '='); if (rc) return rc;
            rc = ts_parse_value(&p, &value, &quoted); if (rc) return rc;
            rc = ts_expect_char(&p, ';'); if (rc) return rc;
            o->assignments[aidx].key.ptr = ts_copy_span(arena, key, &o->assignments[aidx].key.len);
            if (!o->assignments[aidx].key.ptr) return DTS89_ERR_NO_SPACE;
            o->assignments[aidx].array_index = array_index;
            o->assignments[aidx].value_was_quoted = quoted;
            if (quoted)
                o->assignments[aidx].value.ptr = ts_copy_quoted(arena, value, &o->assignments[aidx].value.len);
            else
                o->assignments[aidx].value.ptr = ts_copy_span(arena, value, &o->assignments[aidx].value.len);
            if (!o->assignments[aidx].value.ptr) return DTS89_ERR_NO_SPACE;
            aidx++;
        }
        ts_skip_ws(&p);
        if (p.pos < p.size && p.text[p.pos] == ';') p.pos++;
    }
    return DTS89_OK;
}

static int ts_write_escaped_string(dts89_writer *w, const char *s)
{
    int rc;
    while (*s) {
        unsigned char ch = (unsigned char)*s++;
        if (ch == '\\' || ch == '"') {
            rc = dts89_write_u8(w, '\\'); if (rc) return rc;
            rc = dts89_write_u8(w, ch); if (rc) return rc;
        } else if (ch == '\n') {
            rc = dts89_write_bytes(w, "\\n", 2u); if (rc) return rc;
        } else if (ch == '\r') {
            rc = dts89_write_bytes(w, "\\r", 2u); if (rc) return rc;
        } else if (ch == '\t') {
            rc = dts89_write_bytes(w, "\\t", 2u); if (rc) return rc;
        } else {
            rc = dts89_write_u8(w, ch); if (rc) return rc;
        }
    }
    return DTS89_OK;
}

int dts89_tscript_write_singletons(const dts89_ts_file *file, dts89_writer *w)
{
    dts89_s32 i;
    int rc;
    if (!file || !w) return DTS89_ERR_BAD_ARG;
    for (i = 0; i < file->num_objects; ++i) {
        const dts89_ts_object *o = &file->objects[i];
        dts89_s32 j;
        rc = dts89_write_bytes(w, "singleton ", 10u); if (rc) return rc;
        rc = dts89_write_bytes(w, o->class_name.ptr, o->class_name.len); if (rc) return rc;
        rc = dts89_write_u8(w, '('); if (rc) return rc;
        rc = dts89_write_bytes(w, o->object_name.ptr, o->object_name.len); if (rc) return rc;
        rc = dts89_write_bytes(w, ") {\n", 4u); if (rc) return rc;
        for (j = 0; j < o->num_assignments; ++j) {
            const dts89_ts_assignment *a = &o->assignments[j];
            rc = dts89_write_bytes(w, "  ", 2u); if (rc) return rc;
            rc = dts89_write_bytes(w, a->key.ptr, a->key.len); if (rc) return rc;
            if (a->array_index >= 0) {
                char buf[16];
                unsigned int v;
                int len;
                rc = dts89_write_u8(w, '['); if (rc) return rc;
                v = (unsigned int)a->array_index;
                len = 0;
                do {
                    buf[len++] = (char)('0' + (v % 10u));
                    v /= 10u;
                } while (v && len < (int)sizeof(buf));
                while (len > 0) {
                    rc = dts89_write_u8(w, (dts89_u8)buf[--len]); if (rc) return rc;
                }
                rc = dts89_write_u8(w, ']'); if (rc) return rc;
            }
            rc = dts89_write_bytes(w, " = ", 3u); if (rc) return rc;
            if (a->value_was_quoted) {
                rc = dts89_write_u8(w, '"'); if (rc) return rc;
                rc = ts_write_escaped_string(w, a->value.ptr); if (rc) return rc;
                rc = dts89_write_u8(w, '"'); if (rc) return rc;
            } else {
                rc = dts89_write_bytes(w, a->value.ptr, a->value.len); if (rc) return rc;
            }
            rc = dts89_write_bytes(w, ";\n", 2u); if (rc) return rc;
        }
        rc = dts89_write_bytes(w, "};\n", 3u); if (rc) return rc;
    }
    return DTS89_OK;
}
