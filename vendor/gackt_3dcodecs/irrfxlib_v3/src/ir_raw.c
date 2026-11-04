#include "ir_raw.h"

#include <string.h>

void ir_raw_range_clear(ir_raw_range* range)
{
    if (!range) {
        return;
    }
    range->begin = 0U;
    range->end = 0U;
}

int ir_raw_range_is_valid(const ir_raw_range* range)
{
    if (!range) {
        return 0;
    }
    return range->end > range->begin;
}

size_t ir_raw_range_size(const ir_raw_range* range)
{
    if (!ir_raw_range_is_valid(range)) {
        return 0U;
    }
    return range->end - range->begin;
}

void ir_raw_source_clear(ir_raw_source* src)
{
    if (!src) {
        return;
    }
    src->data = IR_NULL;
    src->size = 0U;
    src->encoding = 0;
    src->has_bom = 0;
}

static int ir_raw_detect_bom(const unsigned char* data, size_t size, int encoding)
{
    if (!data) {
        return 0;
    }
    if (encoding == 0 && size >= 3U && data[0] == 0xEFU && data[1] == 0xBBU && data[2] == 0xBFU) {
        return 1;
    }
    if ((encoding == 1 || encoding == 2) && size >= 2U) {
        if ((data[0] == 0xFFU && data[1] == 0xFEU) || (data[0] == 0xFEU && data[1] == 0xFFU)) {
            return 1;
        }
    }
    return 0;
}

int ir_raw_source_bind(ir_raw_source* src, const void* data, size_t size, int encoding)
{
    if (!src || !data || size == 0U) {
        return IR_ERR_ARGUMENT;
    }
    src->data = (const unsigned char*)data;
    src->size = size;
    src->encoding = encoding;
    src->has_bom = ir_raw_detect_bom(src->data, size, encoding);
    return IR_OK;
}

int ir_raw_source_view(const ir_raw_source* src, const ir_raw_range* range, const void** out_data, size_t* out_size)
{
    if (!src || !range || !out_data || !out_size) {
        return IR_ERR_ARGUMENT;
    }
    if (!src->data || !ir_raw_range_is_valid(range) || range->end > src->size) {
        return IR_ERR_RANGE;
    }
    *out_data = (const void*)(src->data + range->begin);
    *out_size = range->end - range->begin;
    return IR_OK;
}

int ir_raw_source_copy(const ir_raw_source* src, const ir_raw_range* range, void* out_mem, size_t out_cap, size_t* out_len)
{
    const void* view;
    size_t size;
    int rc;
    if (!out_mem || out_cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_raw_source_view(src, range, &view, &size);
    if (rc != IR_OK) {
        return rc;
    }
    if (size > out_cap) {
        return IR_ERR_CAPACITY;
    }
    memcpy(out_mem, view, size);
    if (out_len) {
        *out_len = size;
    }
    return IR_OK;
}

int ir_raw_source_copy_out(const ir_raw_source* src, const ir_raw_range* range, ir_outbuf* out)
{
    const void* view;
    size_t size;
    int rc;
    rc = ir_raw_source_view(src, range, &view, &size);
    if (rc != IR_OK) {
        return rc;
    }
    return ir_outbuf_putsn(out, (const char*)view, size);
}
