#include "ir_xml.h"

#include <ctype.h>
#include <string.h>

static void ir_xml_seterr(char* err, size_t err_cap, const char* msg)
{
    size_t n;
    if (!err || err_cap == 0U) {
        return;
    }
    if (!msg) {
        msg = "error";
    }
    n = strlen(msg);
    if (n >= err_cap) {
        n = err_cap - 1U;
    }
    memcpy(err, msg, n);
    err[n] = '\0';
}


const char* ir_xml_local_name(const char* name)
{
    const char* p;
    const char* last;
    if (!name) {
        return "";
    }
    last = name;
    p = name;
    while (*p) {
        if (*p == ':') {
            last = p + 1;
        }
        ++p;
    }
    return last;
}

int ir_xml_name_eq(const char* actual, const char* expected_local)
{
    if (!actual || !expected_local) {
        return 0;
    }
    return strcmp(ir_xml_local_name(actual), expected_local) == 0;
}

static int ir_xml_is_name_start(unsigned long cp)
{
    return (cp == '_' || cp == ':' || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z'));
}

static int ir_xml_is_name_char(unsigned long cp)
{
    return ir_xml_is_name_start(cp) || (cp >= '0' && cp <= '9') || cp == '-' || cp == '.';
}

static int ir_xml_is_space_cp(unsigned long cp)
{
    return cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n';
}

static int ir_xml_detect_encoding(const unsigned char* p, size_t n)
{
    if (n >= 3U && p[0] == 0xEFU && p[1] == 0xBBU && p[2] == 0xBFU) {
        return IR_XML_UTF8;
    }
    if (n >= 2U && p[0] == 0xFFU && p[1] == 0xFEU) {
        return IR_XML_UTF16LE;
    }
    if (n >= 2U && p[0] == 0xFEU && p[1] == 0xFFU) {
        return IR_XML_UTF16BE;
    }
    if (n >= 4U && p[0] == '<' && p[1] == 0x00U && p[2] == '?' && p[3] == 0x00U) {
        return IR_XML_UTF16LE;
    }
    if (n >= 4U && p[0] == 0x00U && p[1] == '<' && p[2] == 0x00U && p[3] == '?') {
        return IR_XML_UTF16BE;
    }
    return IR_XML_UTF8;
}

void ir_xml_reader_init(ir_xml_reader* xr, const void* data, size_t size)
{
    const unsigned char* p;
    if (!xr) {
        return;
    }
    p = (const unsigned char*)data;
    xr->data = p;
    xr->size = size;
    xr->pos = 0U;
    xr->has_push = 0;
    xr->push_cp = 0UL;
    xr->line = 1UL;
    xr->col = 1UL;
    xr->last_pos_before = 0U;
    xr->last_pos_after = 0U;
    xr->push_pos_before = 0U;
    xr->push_pos_after = 0U;
    xr->in_cdata = 0;
    xr->encoding = ir_xml_detect_encoding(p, size);
    if (xr->encoding == IR_XML_UTF8 && size >= 3U && p[0] == 0xEFU && p[1] == 0xBBU && p[2] == 0xBFU) {
        xr->pos = 3U;
    } else if ((xr->encoding == IR_XML_UTF16LE || xr->encoding == IR_XML_UTF16BE) && size >= 2U) {
        if ((p[0] == 0xFFU && p[1] == 0xFEU) || (p[0] == 0xFEU && p[1] == 0xFFU)) {
            xr->pos = 2U;
        }
    }
}

size_t ir_xml_reader_raw_offset(const ir_xml_reader* xr)
{
    if (!xr) {
        return 0U;
    }
    if (xr->has_push) {
        return xr->push_pos_before;
    }
    return xr->pos;
}

static void ir_xml_push_last_char(ir_xml_reader* xr, unsigned long cp)
{
    if (!xr) {
        return;
    }
    xr->has_push = 1;
    xr->push_cp = cp;
    xr->push_pos_before = xr->last_pos_before;
    xr->push_pos_after = xr->last_pos_after;
}

static int ir_xml_get_utf8(ir_xml_reader* xr, unsigned long* cp)
{
    unsigned char c0;
    unsigned char c1;
    unsigned char c2;
    unsigned char c3;
    if (xr->pos >= xr->size) {
        return IR_EOF;
    }
    c0 = xr->data[xr->pos++];
    if (c0 < 0x80U) {
        *cp = (unsigned long)c0;
        return IR_OK;
    }
    if ((c0 & 0xE0U) == 0xC0U) {
        if (xr->pos >= xr->size) {
            return IR_ERR_ENCODING;
        }
        c1 = xr->data[xr->pos++];
        if ((c1 & 0xC0U) != 0x80U) {
            return IR_ERR_ENCODING;
        }
        *cp = (unsigned long)(((c0 & 0x1FU) << 6) | (c1 & 0x3FU));
        if (*cp < 0x80UL) {
            return IR_ERR_ENCODING;
        }
        return IR_OK;
    }
    if ((c0 & 0xF0U) == 0xE0U) {
        if (xr->pos + 1U >= xr->size) {
            return IR_ERR_ENCODING;
        }
        c1 = xr->data[xr->pos++];
        c2 = xr->data[xr->pos++];
        if ((c1 & 0xC0U) != 0x80U || (c2 & 0xC0U) != 0x80U) {
            return IR_ERR_ENCODING;
        }
        *cp = (unsigned long)(((c0 & 0x0FU) << 12) | ((c1 & 0x3FU) << 6) | (c2 & 0x3FU));
        if (*cp < 0x800UL || (*cp >= 0xD800UL && *cp <= 0xDFFFUL)) {
            return IR_ERR_ENCODING;
        }
        return IR_OK;
    }
    if ((c0 & 0xF8U) == 0xF0U) {
        if (xr->pos + 2U >= xr->size) {
            return IR_ERR_ENCODING;
        }
        c1 = xr->data[xr->pos++];
        c2 = xr->data[xr->pos++];
        c3 = xr->data[xr->pos++];
        if ((c1 & 0xC0U) != 0x80U || (c2 & 0xC0U) != 0x80U || (c3 & 0xC0U) != 0x80U) {
            return IR_ERR_ENCODING;
        }
        *cp = (unsigned long)(((c0 & 0x07U) << 18) | ((c1 & 0x3FU) << 12) | ((c2 & 0x3FU) << 6) | (c3 & 0x3FU));
        if (*cp < 0x10000UL || *cp > 0x10FFFFUL) {
            return IR_ERR_ENCODING;
        }
        return IR_OK;
    }
    return IR_ERR_ENCODING;
}

static unsigned short ir_xml_get_u16(const unsigned char* p, int enc)
{
    if (enc == IR_XML_UTF16LE) {
        return (unsigned short)(p[0] | ((unsigned short)p[1] << 8));
    }
    return (unsigned short)(((unsigned short)p[0] << 8) | p[1]);
}

static int ir_xml_get_utf16(ir_xml_reader* xr, unsigned long* cp)
{
    unsigned short w1;
    unsigned short w2;
    if (xr->pos + 1U >= xr->size) {
        return IR_EOF;
    }
    w1 = ir_xml_get_u16(xr->data + xr->pos, xr->encoding);
    xr->pos += 2U;
    if (w1 >= 0xD800U && w1 <= 0xDBFFU) {
        if (xr->pos + 1U >= xr->size) {
            return IR_ERR_ENCODING;
        }
        w2 = ir_xml_get_u16(xr->data + xr->pos, xr->encoding);
        xr->pos += 2U;
        if (w2 < 0xDC00U || w2 > 0xDFFFU) {
            return IR_ERR_ENCODING;
        }
        *cp = 0x10000UL + ((((unsigned long)w1 - 0xD800UL) << 10) | ((unsigned long)w2 - 0xDC00UL));
        return IR_OK;
    }
    if (w1 >= 0xDC00U && w1 <= 0xDFFFU) {
        return IR_ERR_ENCODING;
    }
    *cp = (unsigned long)w1;
    return IR_OK;
}

int ir_xml_get_char(ir_xml_reader* xr, unsigned long* cp)
{
    int rc;
    unsigned long c;
    size_t before;
    if (!xr || !cp) {
        return IR_ERR_ARGUMENT;
    }
    if (xr->has_push) {
        xr->has_push = 0;
        xr->last_pos_before = xr->push_pos_before;
        xr->last_pos_after = xr->push_pos_after;
        *cp = xr->push_cp;
        return IR_OK;
    }
    before = xr->pos;
    if (xr->encoding == IR_XML_UTF8) {
        rc = ir_xml_get_utf8(xr, &c);
    } else {
        rc = ir_xml_get_utf16(xr, &c);
    }
    if (rc != IR_OK) {
        return rc;
    }
    xr->last_pos_before = before;
    xr->last_pos_after = xr->pos;
    if (c == '\n') {
        xr->line += 1UL;
        xr->col = 1UL;
    } else {
        xr->col += 1UL;
    }
    *cp = c;
    return IR_OK;
}

int ir_xml_peek_char(ir_xml_reader* xr, unsigned long* cp)
{
    int rc;
    if (!xr || !cp) {
        return IR_ERR_ARGUMENT;
    }
    if (xr->has_push) {
        *cp = xr->push_cp;
        return IR_OK;
    }
    rc = ir_xml_get_char(xr, cp);
    if (rc != IR_OK) {
        return rc;
    }
    ir_xml_push_last_char(xr, *cp);
    return IR_OK;
}

static int ir_xml_expect_char(ir_xml_reader* xr, unsigned long expected, char* err, size_t err_cap)
{
    unsigned long cp;
    int rc;
    rc = ir_xml_get_char(xr, &cp);
    if (rc != IR_OK) {
        ir_xml_seterr(err, err_cap, "unexpected end of input");
        return rc == IR_EOF ? IR_ERR_FORMAT : rc;
    }
    if (cp != expected) {
        ir_xml_seterr(err, err_cap, "unexpected character");
        return IR_ERR_FORMAT;
    }
    return IR_OK;
}

int ir_xml_skip_space(ir_xml_reader* xr)
{
    int rc;
    unsigned long cp;
    if (!xr) {
        return IR_ERR_ARGUMENT;
    }
    for (;;) {
        rc = ir_xml_peek_char(xr, &cp);
        if (rc != IR_OK) {
            return rc;
        }
        if (!ir_xml_is_space_cp(cp)) {
            return IR_OK;
        }
        xr->has_push = 0;
    }
}

static int ir_xml_skip_until_seq(ir_xml_reader* xr, const char* seq)
{
    size_t match;
    int rc;
    unsigned long cp;
    size_t n;
    n = strlen(seq);
    match = 0U;
    for (;;) {
        rc = ir_xml_get_char(xr, &cp);
        if (rc != IR_OK) {
            return rc;
        }
        if (cp == (unsigned long)(unsigned char)seq[match]) {
            ++match;
            if (match == n) {
                return IR_OK;
            }
        } else {
            match = (cp == (unsigned long)(unsigned char)seq[0]) ? 1U : 0U;
        }
    }
}

static int ir_xml_skip_declaration(ir_xml_reader* xr)
{
    int rc;
    int quote;
    int bracket_depth;
    unsigned long cp;
    quote = 0;
    bracket_depth = 0;
    for (;;) {
        rc = ir_xml_get_char(xr, &cp);
        if (rc != IR_OK) {
            return rc;
        }
        if (quote != 0) {
            if (cp == (unsigned long)quote) {
                quote = 0;
            }
            continue;
        }
        if (cp == '"' || cp == '\'') {
            quote = (int)cp;
            continue;
        }
        if (cp == '[') {
            ++bracket_depth;
            continue;
        }
        if (cp == ']') {
            if (bracket_depth > 0) {
                --bracket_depth;
            }
            continue;
        }
        if (cp == '>' && bracket_depth == 0) {
            return IR_OK;
        }
    }
}

static int ir_xml_raw_starts_with_ascii(const ir_xml_reader* xr, const char* seq)
{
    size_t off;
    size_t i;
    if (!xr || !seq) {
        return 0;
    }
    off = ir_xml_reader_raw_offset(xr);
    if (xr->encoding == IR_XML_UTF8) {
        for (i = 0U; seq[i] != '\0'; ++i) {
            if (off + i >= xr->size || xr->data[off + i] != (unsigned char)seq[i]) {
                return 0;
            }
        }
        return 1;
    }
    for (i = 0U; seq[i] != '\0'; ++i) {
        unsigned short w;
        if (off + i * 2U + 1U >= xr->size) {
            return 0;
        }
        w = ir_xml_get_u16(xr->data + off + i * 2U, xr->encoding);
        if (w != (unsigned short)(unsigned char)seq[i]) {
            return 0;
        }
    }
    return 1;
}

static int ir_xml_consume_ascii_seq(ir_xml_reader* xr, const char* seq, char* err, size_t err_cap)
{
    size_t i;
    unsigned long cp;
    int rc;
    for (i = 0U; seq[i] != '\0'; ++i) {
        rc = ir_xml_get_char(xr, &cp);
        if (rc != IR_OK) {
            ir_xml_seterr(err, err_cap, "unexpected end inside XML marker");
            return rc == IR_EOF ? IR_ERR_FORMAT : rc;
        }
        if (cp != (unsigned long)(unsigned char)seq[i]) {
            ir_xml_seterr(err, err_cap, "unexpected XML marker");
            return IR_ERR_FORMAT;
        }
    }
    return IR_OK;
}

static int ir_xml_read_name(ir_xml_reader* xr, char* out, size_t out_cap, char* err, size_t err_cap)
{
    unsigned long cp;
    int rc;
    size_t n;
    if (!xr || !out || out_cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_xml_peek_char(xr, &cp);
    if (rc != IR_OK) {
        ir_xml_seterr(err, err_cap, "expected name");
        return rc == IR_EOF ? IR_ERR_FORMAT : rc;
    }
    if (!ir_xml_is_name_start(cp)) {
        ir_xml_seterr(err, err_cap, "invalid name start");
        return IR_ERR_FORMAT;
    }
    n = 0U;
    for (;;) {
        rc = ir_xml_peek_char(xr, &cp);
        if (rc != IR_OK) {
            break;
        }
        if (!ir_xml_is_name_char(cp)) {
            break;
        }
        xr->has_push = 0;
        if (n + 1U >= out_cap) {
            ir_xml_seterr(err, err_cap, "xml name too long");
            return IR_ERR_OVERFLOW;
        }
        out[n++] = (char)cp;
    }
    out[n] = '\0';
    return IR_OK;
}

static int ir_xml_append_utf8(char* out, size_t out_cap, size_t* len, unsigned long cp)
{
    if ((cp >= 0xD800UL && cp <= 0xDFFFUL) || cp > 0x10FFFFUL) {
        return IR_ERR_ENCODING;
    }
    if (cp < 0x80UL) {
        if (*len + 1U >= out_cap) return IR_ERR_OVERFLOW;
        out[(*len)++] = (char)cp;
        out[*len] = '\0';
        return IR_OK;
    }
    if (cp < 0x800UL) {
        if (*len + 2U >= out_cap) return IR_ERR_OVERFLOW;
        out[(*len)++] = (char)(0xC0U | ((cp >> 6) & 0x1FU));
        out[(*len)++] = (char)(0x80U | (cp & 0x3FU));
        out[*len] = '\0';
        return IR_OK;
    }
    if (cp < 0x10000UL) {
        if (*len + 3U >= out_cap) return IR_ERR_OVERFLOW;
        out[(*len)++] = (char)(0xE0U | ((cp >> 12) & 0x0FU));
        out[(*len)++] = (char)(0x80U | ((cp >> 6) & 0x3FU));
        out[(*len)++] = (char)(0x80U | (cp & 0x3FU));
        out[*len] = '\0';
        return IR_OK;
    }
    if (*len + 4U >= out_cap) return IR_ERR_OVERFLOW;
    out[(*len)++] = (char)(0xF0U | ((cp >> 18) & 0x07U));
    out[(*len)++] = (char)(0x80U | ((cp >> 12) & 0x3FU));
    out[(*len)++] = (char)(0x80U | ((cp >> 6) & 0x3FU));
    out[(*len)++] = (char)(0x80U | (cp & 0x3FU));
    out[*len] = '\0';
    return IR_OK;
}

static int ir_xml_read_entity(ir_xml_reader* xr, unsigned long* cp, char* err, size_t err_cap)
{
    char name[16];
    size_t n;
    unsigned long c;
    int rc;
    n = 0U;
    for (;;) {
        rc = ir_xml_get_char(xr, &c);
        if (rc != IR_OK) {
            ir_xml_seterr(err, err_cap, "unterminated entity");
            return IR_ERR_FORMAT;
        }
        if (c == ';') {
            break;
        }
        if (n + 1U >= sizeof(name)) {
            return IR_ERR_OVERFLOW;
        }
        name[n++] = (char)c;
    }
    name[n] = '\0';
    if (strcmp(name, "amp") == 0) { *cp = '&'; return IR_OK; }
    if (strcmp(name, "lt") == 0) { *cp = '<'; return IR_OK; }
    if (strcmp(name, "gt") == 0) { *cp = '>'; return IR_OK; }
    if (strcmp(name, "quot") == 0) { *cp = '"'; return IR_OK; }
    if (strcmp(name, "apos") == 0) { *cp = '\''; return IR_OK; }
    if (name[0] == '#') {
        unsigned long value;
        size_t i;
        value = 0UL;
        if (name[1] == 'x' || name[1] == 'X') {
            for (i = 2U; name[i] != '\0'; ++i) {
                unsigned long v;
                char ch;
                ch = name[i];
                if (ch >= '0' && ch <= '9') v = (unsigned long)(ch - '0');
                else if (ch >= 'a' && ch <= 'f') v = (unsigned long)(10 + ch - 'a');
                else if (ch >= 'A' && ch <= 'F') v = (unsigned long)(10 + ch - 'A');
                else {
                    ir_xml_seterr(err, err_cap, "invalid hex entity");
                    return IR_ERR_FORMAT;
                }
                value = (value << 4) | v;
            }
        } else {
            for (i = 1U; name[i] != '\0'; ++i) {
                char ch;
                ch = name[i];
                if (ch < '0' || ch > '9') {
                    ir_xml_seterr(err, err_cap, "invalid numeric entity");
                    return IR_ERR_FORMAT;
                }
                value = value * 10UL + (unsigned long)(ch - '0');
            }
        }
        *cp = value;
        return IR_OK;
    }
    ir_xml_seterr(err, err_cap, "unsupported entity");
    return IR_ERR_UNSUPPORTED;
}

static int ir_xml_read_attr_value(ir_xml_reader* xr, char* out, size_t out_cap, char* err, size_t err_cap)
{
    unsigned long quote;
    unsigned long cp;
    int rc;
    size_t n;
    rc = ir_xml_get_char(xr, &quote);
    if (rc != IR_OK || (quote != '"' && quote != '\'')) {
        ir_xml_seterr(err, err_cap, "expected quoted attribute value");
        return IR_ERR_FORMAT;
    }
    n = 0U;
    out[0] = '\0';
    for (;;) {
        rc = ir_xml_get_char(xr, &cp);
        if (rc != IR_OK) {
            ir_xml_seterr(err, err_cap, "unterminated attribute value");
            return IR_ERR_FORMAT;
        }
        if (cp == quote) {
            return IR_OK;
        }
        if (cp == '&') {
            rc = ir_xml_read_entity(xr, &cp, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
        rc = ir_xml_append_utf8(out, out_cap, &n, cp);
        if (rc != IR_OK) {
            ir_xml_seterr(err, err_cap, "attribute value too long");
            return rc;
        }
    }
}

const char* ir_xml_tag_attr(const ir_xml_tag* tag, const char* name)
{
    int i;
    if (!tag || !name) {
        return IR_NULL;
    }
    for (i = 0; i < tag->attr_count; ++i) {
        if (strcmp(tag->attrs[i].name, name) == 0) {
            return tag->attrs[i].value;
        }
    }
    return IR_NULL;
}

const char* ir_xml_tag_attr_local(const ir_xml_tag* tag, const char* local_name)
{
    int i;
    const char* exact;
    if (!tag || !local_name) {
        return IR_NULL;
    }
    exact = ir_xml_tag_attr(tag, local_name);
    if (exact) {
        return exact;
    }
    for (i = 0; i < tag->attr_count; ++i) {
        if (ir_xml_name_eq(tag->attrs[i].name, local_name)) {
            return tag->attrs[i].value;
        }
    }
    return IR_NULL;
}

int ir_xml_next_tag(ir_xml_reader* xr, ir_xml_tag* tag, char* err, size_t err_cap)
{
    unsigned long cp;
    int rc;
    int attr_count;
    char aname[IR_XML_NAME_CAP];
    size_t tag_raw_begin;
    if (!xr || !tag) {
        return IR_ERR_ARGUMENT;
    }
    for (;;) {
        rc = ir_xml_skip_space(xr);
        if (rc == IR_EOF) {
            return IR_EOF;
        }
        if (rc != IR_OK) {
            return rc;
        }
        tag_raw_begin = ir_xml_reader_raw_offset(xr);
        rc = ir_xml_get_char(xr, &cp);
        if (rc == IR_EOF) {
            return IR_EOF;
        }
        if (rc != IR_OK) {
            return rc;
        }
        if (cp != '<') {
            while (cp != '<') {
                rc = ir_xml_get_char(xr, &cp);
                if (rc == IR_EOF) {
                    return IR_EOF;
                }
                if (rc != IR_OK) {
                    return rc;
                }
            }
            tag_raw_begin = xr->last_pos_before;
        }
        rc = ir_xml_peek_char(xr, &cp);
        if (rc != IR_OK) {
            ir_xml_seterr(err, err_cap, "unexpected end after <");
            return IR_ERR_FORMAT;
        }
        if (cp == '?') {
            xr->has_push = 0;
            rc = ir_xml_skip_until_seq(xr, "?>");
            if (rc != IR_OK) {
                ir_xml_seterr(err, err_cap, "unterminated processing instruction");
                return IR_ERR_FORMAT;
            }
            continue;
        }
        if (cp == '!') {
            xr->has_push = 0;
            rc = ir_xml_peek_char(xr, &cp);
            if (rc != IR_OK) {
                return IR_ERR_FORMAT;
            }
            if (cp == '-') {
                rc = ir_xml_expect_char(xr, '-', err, err_cap);
                if (rc != IR_OK) return rc;
                rc = ir_xml_expect_char(xr, '-', err, err_cap);
                if (rc != IR_OK) return rc;
                rc = ir_xml_skip_until_seq(xr, "-->");
                if (rc != IR_OK) {
                    ir_xml_seterr(err, err_cap, "unterminated comment");
                    return IR_ERR_FORMAT;
                }
                continue;
            }
            if (cp == '[') {
                rc = ir_xml_consume_ascii_seq(xr, "[CDATA[", err, err_cap);
                if (rc != IR_OK) {
                    return rc;
                }
                rc = ir_xml_skip_until_seq(xr, "]]>");
                if (rc != IR_OK) {
                    ir_xml_seterr(err, err_cap, "unterminated CDATA section");
                    return IR_ERR_FORMAT;
                }
                continue;
            }
            rc = ir_xml_skip_declaration(xr);
            if (rc != IR_OK) {
                ir_xml_seterr(err, err_cap, "unterminated declaration");
                return IR_ERR_FORMAT;
            }
            continue;
        }
        break;
    }
    tag->line = xr->line;
    tag->col = xr->col;
    tag->attr_count = 0;
    tag->is_end = 0;
    tag->is_empty = 0;
    tag->raw_begin = tag_raw_begin;
    tag->raw_end = tag_raw_begin;
    if (cp == '/') {
        xr->has_push = 0;
        tag->is_end = 1;
        rc = ir_xml_read_name(xr, tag->name, sizeof(tag->name), err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        rc = ir_xml_skip_space(xr);
        if (rc != IR_OK && rc != IR_EOF) {
            return rc;
        }
        rc = ir_xml_expect_char(xr, '>', err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        tag->raw_end = xr->pos;
        return IR_OK;
    }
    rc = ir_xml_read_name(xr, tag->name, sizeof(tag->name), err, err_cap);
    if (rc != IR_OK) {
        return rc;
    }
    attr_count = 0;
    for (;;) {
        rc = ir_xml_skip_space(xr);
        if (rc == IR_EOF) {
            ir_xml_seterr(err, err_cap, "unterminated tag");
            return IR_ERR_FORMAT;
        }
        if (rc != IR_OK) {
            return rc;
        }
        rc = ir_xml_get_char(xr, &cp);
        if (rc != IR_OK) {
            return IR_ERR_FORMAT;
        }
        if (cp == '>') {
            tag->attr_count = attr_count;
            tag->raw_end = xr->pos;
            return IR_OK;
        }
        if (cp == '/') {
            rc = ir_xml_expect_char(xr, '>', err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
            tag->is_empty = 1;
            tag->attr_count = attr_count;
            tag->raw_end = xr->pos;
            return IR_OK;
        }
        ir_xml_push_last_char(xr, cp);
        if (attr_count >= IR_XML_MAX_ATTRS) {
            ir_xml_seterr(err, err_cap, "too many attributes in tag");
            return IR_ERR_CAPACITY;
        }
        rc = ir_xml_read_name(xr, aname, sizeof(aname), err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        rc = ir_xml_skip_space(xr);
        if (rc != IR_OK) {
            return IR_ERR_FORMAT;
        }
        rc = ir_xml_expect_char(xr, '=', err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        rc = ir_xml_skip_space(xr);
        if (rc != IR_OK) {
            return rc;
        }
        strcpy(tag->attrs[attr_count].name, aname);
        rc = ir_xml_read_attr_value(xr, tag->attrs[attr_count].value, sizeof(tag->attrs[attr_count].value), err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        ++attr_count;
    }
}

int ir_xml_skip_element(ir_xml_reader* xr, const ir_xml_tag* start_tag, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int depth;
    int rc;
    if (!xr || !start_tag || start_tag->is_end || start_tag->is_empty) {
        return IR_OK;
    }
    depth = 1;
    while (depth > 0) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            --depth;
        } else if (!tag.is_empty) {
            ++depth;
        }
    }
    return IR_OK;
}

int ir_xml_read_token(ir_xml_reader* xr, char* out, size_t out_cap, int stop_on_lt, int* hit_lt, char* err, size_t err_cap)
{
    unsigned long cp;
    int rc;
    size_t n;
    if (!xr || !out || out_cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    if (hit_lt) {
        *hit_lt = 0;
    }
    n = 0U;
    for (;;) {
        if (xr->in_cdata) {
            if (ir_xml_raw_starts_with_ascii(xr, "]]>") != 0) {
                rc = ir_xml_consume_ascii_seq(xr, "]]>", err, err_cap);
                if (rc != IR_OK) {
                    return rc;
                }
                xr->in_cdata = 0;
                if (n > 0U) {
                    break;
                }
                continue;
            }
            rc = ir_xml_get_char(xr, &cp);
            if (rc != IR_OK) {
                ir_xml_seterr(err, err_cap, "unterminated CDATA section");
                return rc == IR_EOF ? IR_ERR_FORMAT : rc;
            }
            if (ir_xml_is_space_cp(cp)) {
                if (n > 0U) {
                    break;
                }
                continue;
            }
            rc = ir_xml_append_utf8(out, out_cap, &n, cp);
            if (rc != IR_OK) {
                ir_xml_seterr(err, err_cap, "token too long");
                return rc;
            }
            continue;
        }

        rc = ir_xml_peek_char(xr, &cp);
        if (rc != IR_OK) {
            if (rc == IR_EOF && n > 0U) {
                out[n] = '\0';
                return IR_OK;
            }
            return rc;
        }
        if (stop_on_lt && cp == '<') {
            if (n == 0U && ir_xml_raw_starts_with_ascii(xr, "<![CDATA[") != 0) {
                rc = ir_xml_consume_ascii_seq(xr, "<![CDATA[", err, err_cap);
                if (rc != IR_OK) {
                    return rc;
                }
                xr->in_cdata = 1;
                continue;
            }
            if (n == 0U && ir_xml_raw_starts_with_ascii(xr, "<!--") != 0) {
                rc = ir_xml_consume_ascii_seq(xr, "<!--", err, err_cap);
                if (rc != IR_OK) {
                    return rc;
                }
                rc = ir_xml_skip_until_seq(xr, "-->");
                if (rc != IR_OK) {
                    ir_xml_seterr(err, err_cap, "unterminated comment");
                    return rc == IR_EOF ? IR_ERR_FORMAT : rc;
                }
                continue;
            }
            if (n == 0U && ir_xml_raw_starts_with_ascii(xr, "<?") != 0) {
                rc = ir_xml_consume_ascii_seq(xr, "<?", err, err_cap);
                if (rc != IR_OK) {
                    return rc;
                }
                rc = ir_xml_skip_until_seq(xr, "?>");
                if (rc != IR_OK) {
                    ir_xml_seterr(err, err_cap, "unterminated processing instruction");
                    return rc == IR_EOF ? IR_ERR_FORMAT : rc;
                }
                continue;
            }
            if (n == 0U && ir_xml_raw_starts_with_ascii(xr, "<!") != 0) {
                rc = ir_xml_consume_ascii_seq(xr, "<!", err, err_cap);
                if (rc != IR_OK) {
                    return rc;
                }
                rc = ir_xml_skip_declaration(xr);
                if (rc != IR_OK) {
                    ir_xml_seterr(err, err_cap, "unterminated declaration");
                    return rc == IR_EOF ? IR_ERR_FORMAT : rc;
                }
                continue;
            }
            if (hit_lt) {
                *hit_lt = 1;
            }
            break;
        }
        if (ir_xml_is_space_cp(cp)) {
            if (n > 0U) {
                break;
            }
            xr->has_push = 0;
            continue;
        }
        xr->has_push = 0;
        if (cp == '&') {
            rc = ir_xml_read_entity(xr, &cp, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
        rc = ir_xml_append_utf8(out, out_cap, &n, cp);
        if (rc != IR_OK) {
            ir_xml_seterr(err, err_cap, "token too long");
            return rc;
        }
    }
    if (n == 0U) {
        out[0] = '\0';
        return IR_EOF;
    }
    out[n] = '\0';
    return IR_OK;
}

int ir_xml_read_expected_end_tag(ir_xml_reader* xr, const char* name, ir_xml_tag* out_tag, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int rc;
    rc = ir_xml_next_tag(xr, &tag, err, err_cap);
    if (rc != IR_OK) {
        return rc;
    }
    if (!tag.is_end || !ir_xml_name_eq(tag.name, name)) {
        ir_xml_seterr(err, err_cap, "unexpected end tag");
        return IR_ERR_FORMAT;
    }
    if (out_tag) {
        *out_tag = tag;
    }
    return IR_OK;
}

int ir_xml_read_expected_end(ir_xml_reader* xr, const char* name, char* err, size_t err_cap)
{
    return ir_xml_read_expected_end_tag(xr, name, IR_NULL, err, err_cap);
}

const char* ir_xml_encoding_name(int encoding)
{
    switch (encoding) {
    case IR_XML_UTF8: return "UTF-8";
    case IR_XML_UTF16LE: return "UTF-16LE";
    case IR_XML_UTF16BE: return "UTF-16BE";
    default: return "unknown";
    }
}
