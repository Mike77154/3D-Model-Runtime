#include "ir_common.h"

#include <string.h>

void ir_strpool_init(ir_strpool* pool, char* data, size_t cap)
{
    if (!pool) {
        return;
    }
    pool->data = data;
    pool->cap = cap;
    pool->len = 0;
    if (data && cap) {
        data[0] = '\0';
    }
}

int ir_strpool_put(ir_strpool* pool, const char* s, size_t n, ir_u32* off, ir_u16* len)
{
    size_t need;
    size_t start;
    if (!pool || !pool->data || !off || !len) {
        return IR_ERR_ARGUMENT;
    }
    if (!s) {
        s = "";
        n = 0;
    }
    need = n + 1;
    if (pool->len + need > pool->cap) {
        return IR_ERR_CAPACITY;
    }
    if (n > 65535UL) {
        return IR_ERR_RANGE;
    }
    start = pool->len;
    if (n) {
        memcpy(pool->data + start, s, n);
    }
    pool->data[start + n] = '\0';
    pool->len += need;
    *off = (ir_u32)start;
    *len = (ir_u16)n;
    return IR_OK;
}

const char* ir_strpool_get(const ir_strpool* pool, ir_u32 off)
{
    if (!pool || !pool->data || off >= pool->len) {
        return "";
    }
    return pool->data + off;
}

void ir_outbuf_init(ir_outbuf* out, char* data, size_t cap)
{
    if (!out) {
        return;
    }
    out->data = data;
    out->cap = cap;
    out->len = 0;
    if (data && cap) {
        data[0] = '\0';
    }
}

int ir_outbuf_putc(ir_outbuf* out, int ch)
{
    if (!out || !out->data) {
        return IR_ERR_ARGUMENT;
    }
    if (out->len + 1 >= out->cap) {
        return IR_ERR_CAPACITY;
    }
    out->data[out->len++] = (char)ch;
    out->data[out->len] = '\0';
    return IR_OK;
}

int ir_outbuf_puts(ir_outbuf* out, const char* s)
{
    size_t n;
    if (!s) {
        s = "";
    }
    n = strlen(s);
    return ir_outbuf_putsn(out, s, n);
}

int ir_outbuf_putsn(ir_outbuf* out, const char* s, size_t n)
{
    if (!out || !out->data || !s) {
        return IR_ERR_ARGUMENT;
    }
    if (out->len + n >= out->cap) {
        return IR_ERR_CAPACITY;
    }
    memcpy(out->data + out->len, s, n);
    out->len += n;
    out->data[out->len] = '\0';
    return IR_OK;
}

int ir_outbuf_pad(ir_outbuf* out, char ch, size_t count)
{
    size_t i;
    int rc;
    for (i = 0; i < count; ++i) {
        rc = ir_outbuf_putc(out, (int)ch);
        if (rc != IR_OK) {
            return rc;
        }
    }
    return IR_OK;
}

int ir_outbuf_finalize(ir_outbuf* out, size_t* out_len)
{
    if (!out || !out->data) {
        return IR_ERR_ARGUMENT;
    }
    if (out->len >= out->cap) {
        return IR_ERR_CAPACITY;
    }
    out->data[out->len] = '\0';
    if (out_len) {
        *out_len = out->len;
    }
    return IR_OK;
}

int ir_xml_escape_n_to_out(ir_outbuf* out, const char* s, size_t n)
{
    size_t i;
    int rc;
    char c;
    if (!out || !s) {
        return IR_ERR_ARGUMENT;
    }
    for (i = 0; i < n; ++i) {
        c = s[i];
        switch (c) {
        case '&':
            rc = ir_outbuf_puts(out, "&amp;");
            break;
        case '<':
            rc = ir_outbuf_puts(out, "&lt;");
            break;
        case '>':
            rc = ir_outbuf_puts(out, "&gt;");
            break;
        case '"':
            rc = ir_outbuf_puts(out, "&quot;");
            break;
        case '\'':
            rc = ir_outbuf_puts(out, "&apos;");
            break;
        default:
            rc = ir_outbuf_putc(out, (int)c);
            break;
        }
        if (rc != IR_OK) {
            return rc;
        }
    }
    return IR_OK;
}

int ir_xml_escape_to_out(ir_outbuf* out, const char* s)
{
    size_t n;
    if (!s) {
        s = "";
    }
    n = strlen(s);
    return ir_xml_escape_n_to_out(out, s, n);
}

void ir_box3fx_make_empty(ir_box3fx* box)
{
    if (!box) {
        return;
    }
    box->min_edge.x = 2147483647L;
    box->min_edge.y = 2147483647L;
    box->min_edge.z = 2147483647L;
    box->max_edge.x = -2147483647L - 1L;
    box->max_edge.y = -2147483647L - 1L;
    box->max_edge.z = -2147483647L - 1L;
}

int ir_box3fx_is_empty(const ir_box3fx* box)
{
    if (!box) {
        return 1;
    }
    return box->min_edge.x > box->max_edge.x;
}

void ir_box3fx_expand(ir_box3fx* box, ir_vec3fx p)
{
    if (!box) {
        return;
    }
    if (ir_box3fx_is_empty(box)) {
        box->min_edge = p;
        box->max_edge = p;
        return;
    }
    if (p.x < box->min_edge.x) box->min_edge.x = p.x;
    if (p.y < box->min_edge.y) box->min_edge.y = p.y;
    if (p.z < box->min_edge.z) box->min_edge.z = p.z;
    if (p.x > box->max_edge.x) box->max_edge.x = p.x;
    if (p.y > box->max_edge.y) box->max_edge.y = p.y;
    if (p.z > box->max_edge.z) box->max_edge.z = p.z;
}

const char* ir_result_name(int rc)
{
    switch (rc) {
    case IR_OK: return "IR_OK";
    case IR_EOF: return "IR_EOF";
    case IR_ERR_GENERIC: return "IR_ERR_GENERIC";
    case IR_ERR_ARGUMENT: return "IR_ERR_ARGUMENT";
    case IR_ERR_FORMAT: return "IR_ERR_FORMAT";
    case IR_ERR_RANGE: return "IR_ERR_RANGE";
    case IR_ERR_OVERFLOW: return "IR_ERR_OVERFLOW";
    case IR_ERR_CAPACITY: return "IR_ERR_CAPACITY";
    case IR_ERR_ENCODING: return "IR_ERR_ENCODING";
    case IR_ERR_UNSUPPORTED: return "IR_ERR_UNSUPPORTED";
    default: return "IR_ERR_UNKNOWN";
    }
}
