#include "ir_writer.h"

#include <string.h>

void ir_writer_options_init(ir_writer_options* opt)
{
    if (!opt) {
        return;
    }
    opt->encoding = IR_WRITER_UTF8;
    opt->emit_bom = 0;
    opt->preserve_source_if_possible = 0;
    opt->scratch_utf8 = IR_NULL;
    opt->scratch_utf8_cap = 0U;
}

static int ir_writer_utf8_next(const unsigned char* s, size_t n, size_t* pos, unsigned long* cp)
{
    unsigned char c0;
    unsigned char c1;
    unsigned char c2;
    unsigned char c3;
    if (!s || !pos || !cp) {
        return IR_ERR_ARGUMENT;
    }
    if (*pos >= n) {
        return IR_EOF;
    }
    c0 = s[(*pos)++];
    if (c0 < 0x80U) {
        *cp = (unsigned long)c0;
        return IR_OK;
    }
    if ((c0 & 0xE0U) == 0xC0U) {
        if (*pos >= n) return IR_ERR_ENCODING;
        c1 = s[(*pos)++];
        if ((c1 & 0xC0U) != 0x80U) return IR_ERR_ENCODING;
        *cp = (unsigned long)(((c0 & 0x1FU) << 6) | (c1 & 0x3FU));
        return IR_OK;
    }
    if ((c0 & 0xF0U) == 0xE0U) {
        if (*pos + 1U >= n) return IR_ERR_ENCODING;
        c1 = s[(*pos)++];
        c2 = s[(*pos)++];
        if ((c1 & 0xC0U) != 0x80U || (c2 & 0xC0U) != 0x80U) return IR_ERR_ENCODING;
        *cp = (unsigned long)(((c0 & 0x0FU) << 12) | ((c1 & 0x3FU) << 6) | (c2 & 0x3FU));
        return IR_OK;
    }
    if ((c0 & 0xF8U) == 0xF0U) {
        if (*pos + 2U >= n) return IR_ERR_ENCODING;
        c1 = s[(*pos)++];
        c2 = s[(*pos)++];
        c3 = s[(*pos)++];
        if ((c1 & 0xC0U) != 0x80U || (c2 & 0xC0U) != 0x80U || (c3 & 0xC0U) != 0x80U) return IR_ERR_ENCODING;
        *cp = (unsigned long)(((c0 & 0x07U) << 18) | ((c1 & 0x3FU) << 12) | ((c2 & 0x3FU) << 6) | (c3 & 0x3FU));
        return IR_OK;
    }
    return IR_ERR_ENCODING;
}

int ir_utf8_to_utf16le(const char* utf8, size_t utf8_len, void* out_mem, size_t out_cap, size_t* out_len, int emit_bom)
{
    const unsigned char* src;
    unsigned char* dst;
    size_t src_pos;
    size_t dst_pos;
    unsigned long cp;
    int rc;
    if (!utf8 || !out_mem) {
        return IR_ERR_ARGUMENT;
    }
    src = (const unsigned char*)utf8;
    dst = (unsigned char*)out_mem;
    src_pos = 0U;
    dst_pos = 0U;
    if (emit_bom) {
        if (out_cap < 2U) {
            return IR_ERR_CAPACITY;
        }
        dst[dst_pos++] = 0xFFU;
        dst[dst_pos++] = 0xFEU;
    }
    for (;;) {
        rc = ir_writer_utf8_next(src, utf8_len, &src_pos, &cp);
        if (rc == IR_EOF) {
            break;
        }
        if (rc != IR_OK) {
            return rc;
        }
        if (cp <= 0xFFFFUL) {
            if (cp >= 0xD800UL && cp <= 0xDFFFUL) {
                return IR_ERR_ENCODING;
            }
            if (dst_pos + 2U > out_cap) {
                return IR_ERR_CAPACITY;
            }
            dst[dst_pos++] = (unsigned char)(cp & 0xFFU);
            dst[dst_pos++] = (unsigned char)((cp >> 8) & 0xFFU);
        } else if (cp <= 0x10FFFFUL) {
            unsigned long v;
            unsigned long w1;
            unsigned long w2;
            if (dst_pos + 4U > out_cap) {
                return IR_ERR_CAPACITY;
            }
            v = cp - 0x10000UL;
            w1 = 0xD800UL + ((v >> 10) & 0x3FFUL);
            w2 = 0xDC00UL + (v & 0x3FFUL);
            dst[dst_pos++] = (unsigned char)(w1 & 0xFFU);
            dst[dst_pos++] = (unsigned char)((w1 >> 8) & 0xFFU);
            dst[dst_pos++] = (unsigned char)(w2 & 0xFFU);
            dst[dst_pos++] = (unsigned char)((w2 >> 8) & 0xFFU);
        } else {
            return IR_ERR_ENCODING;
        }
    }
    if (out_len) {
        *out_len = dst_pos;
    }
    return IR_OK;
}

static int ir_writer_get_options(const ir_writer_options* in, ir_writer_options* out)
{
    if (!out) {
        return IR_ERR_ARGUMENT;
    }
    if (in) {
        *out = *in;
    } else {
        ir_writer_options_init(out);
    }
    return IR_OK;
}

static size_t ir_writer_bom_size_for_source(const ir_raw_source* src)
{
    if (!src || !src->has_bom) {
        return 0U;
    }
    if (src->encoding == IR_XML_UTF8) {
        return 3U;
    }
    if (src->encoding == IR_XML_UTF16LE || src->encoding == IR_XML_UTF16BE) {
        return 2U;
    }
    return 0U;
}

static int ir_writer_copy_source_document(const ir_raw_source* src, const ir_raw_range* doc_range, int target_encoding, int emit_bom, void* out_mem, size_t out_cap, size_t* out_len)
{
    size_t bom_size;
    ir_raw_range range;
    unsigned char* dst;
    size_t raw_len;
    int rc;
    if (!src || !doc_range || !out_mem) {
        return IR_ERR_ARGUMENT;
    }
    if (!src->data || !ir_raw_range_is_valid(doc_range)) {
        return IR_ERR_RANGE;
    }
    if (target_encoding == IR_WRITER_UTF8 && src->encoding != IR_XML_UTF8) {
        return IR_ERR_UNSUPPORTED;
    }
    if (target_encoding == IR_WRITER_UTF16LE && src->encoding != IR_XML_UTF16LE) {
        return IR_ERR_UNSUPPORTED;
    }
    bom_size = ir_writer_bom_size_for_source(src);
    range = *doc_range;
    if (!emit_bom && bom_size > 0U && src->has_bom && range.end >= range.begin + bom_size) {
        range.begin += bom_size;
    }
    raw_len = ir_raw_range_size(&range);
    if (emit_bom && (!src->has_bom || bom_size == 0U)) {
        size_t need;
        need = raw_len + ((target_encoding == IR_WRITER_UTF8) ? 3U : 2U);
        if (need > out_cap) {
            return IR_ERR_CAPACITY;
        }
        dst = (unsigned char*)out_mem;
        if (target_encoding == IR_WRITER_UTF8) {
            dst[0] = 0xEFU; dst[1] = 0xBBU; dst[2] = 0xBFU;
            rc = ir_raw_source_copy(src, &range, dst + 3U, out_cap - 3U, &raw_len);
            if (rc != IR_OK) return rc;
            if (out_len) *out_len = raw_len + 3U;
            return IR_OK;
        }
        dst[0] = 0xFFU; dst[1] = 0xFEU;
        rc = ir_raw_source_copy(src, &range, dst + 2U, out_cap - 2U, &raw_len);
        if (rc != IR_OK) return rc;
        if (out_len) *out_len = raw_len + 2U;
        return IR_OK;
    }
    return ir_raw_source_copy(src, &range, out_mem, out_cap, out_len);
}

int ir_mesh_write_memory_ex(const ir_mesh_doc* doc, void* out_mem, size_t out_cap, size_t* out_len, const ir_writer_options* opt, char* err, size_t err_cap)
{
    ir_writer_options local_opt;
    size_t inner_len;
    int rc;
    unsigned char* dst;
    rc = ir_writer_get_options(opt, &local_opt);
    if (rc != IR_OK) {
        return rc;
    }
    if (!doc || !out_mem) {
        return IR_ERR_ARGUMENT;
    }
    if (local_opt.preserve_source_if_possible) {
        rc = ir_writer_copy_source_document(&doc->raw_source, &doc->raw_document, local_opt.encoding, local_opt.emit_bom, out_mem, out_cap, out_len);
        if (rc == IR_OK) {
            return IR_OK;
        }
        if (rc != IR_ERR_UNSUPPORTED && rc != IR_ERR_RANGE) {
            return rc;
        }
    }
    if (local_opt.encoding == IR_WRITER_UTF8) {
        dst = (unsigned char*)out_mem;
        if (local_opt.emit_bom) {
            if (out_cap < 3U) {
                return IR_ERR_CAPACITY;
            }
            dst[0] = 0xEFU;
            dst[1] = 0xBBU;
            dst[2] = 0xBFU;
            rc = ir_mesh_write_memory(doc, (char*)(dst + 3U), out_cap - 3U, &inner_len, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
            if (out_len) {
                *out_len = inner_len + 3U;
            }
            return IR_OK;
        }
        return ir_mesh_write_memory(doc, (char*)out_mem, out_cap, out_len, err, err_cap);
    }
    if (local_opt.encoding == IR_WRITER_UTF16LE) {
        if (!local_opt.scratch_utf8 || local_opt.scratch_utf8_cap == 0U) {
            return IR_ERR_ARGUMENT;
        }
        rc = ir_mesh_write_memory(doc, local_opt.scratch_utf8, local_opt.scratch_utf8_cap, &inner_len, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        return ir_utf8_to_utf16le(local_opt.scratch_utf8, inner_len, out_mem, out_cap, out_len, local_opt.emit_bom);
    }
    return IR_ERR_UNSUPPORTED;
}

int ir_scene_write_memory_ex(const ir_scene_doc* doc, void* out_mem, size_t out_cap, size_t* out_len, const ir_writer_options* opt, char* err, size_t err_cap)
{
    ir_writer_options local_opt;
    size_t inner_len;
    int rc;
    unsigned char* dst;
    rc = ir_writer_get_options(opt, &local_opt);
    if (rc != IR_OK) {
        return rc;
    }
    if (!doc || !out_mem) {
        return IR_ERR_ARGUMENT;
    }
    if (local_opt.preserve_source_if_possible) {
        rc = ir_writer_copy_source_document(&doc->raw_source, &doc->raw_document, local_opt.encoding, local_opt.emit_bom, out_mem, out_cap, out_len);
        if (rc == IR_OK) {
            return IR_OK;
        }
        if (rc != IR_ERR_UNSUPPORTED && rc != IR_ERR_RANGE) {
            return rc;
        }
    }
    if (local_opt.encoding == IR_WRITER_UTF8) {
        dst = (unsigned char*)out_mem;
        if (local_opt.emit_bom) {
            if (out_cap < 3U) {
                return IR_ERR_CAPACITY;
            }
            dst[0] = 0xEFU;
            dst[1] = 0xBBU;
            dst[2] = 0xBFU;
            rc = ir_scene_write_memory(doc, (char*)(dst + 3U), out_cap - 3U, &inner_len, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
            if (out_len) {
                *out_len = inner_len + 3U;
            }
            return IR_OK;
        }
        return ir_scene_write_memory(doc, (char*)out_mem, out_cap, out_len, err, err_cap);
    }
    if (local_opt.encoding == IR_WRITER_UTF16LE) {
        if (!local_opt.scratch_utf8 || local_opt.scratch_utf8_cap == 0U) {
            return IR_ERR_ARGUMENT;
        }
        rc = ir_scene_write_memory(doc, local_opt.scratch_utf8, local_opt.scratch_utf8_cap, &inner_len, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        return ir_utf8_to_utf16le(local_opt.scratch_utf8, inner_len, out_mem, out_cap, out_len, local_opt.emit_bom);
    }
    return IR_ERR_UNSUPPORTED;
}
