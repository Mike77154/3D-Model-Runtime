#ifndef IR_RAW_H
#define IR_RAW_H

#include "ir_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ir_raw_range_tag {
    size_t begin;
    size_t end;
} ir_raw_range;

typedef struct ir_raw_source_tag {
    const unsigned char* data;
    size_t size;
    int encoding;
    int has_bom;
} ir_raw_source;

void ir_raw_range_clear(ir_raw_range* range);
int ir_raw_range_is_valid(const ir_raw_range* range);
size_t ir_raw_range_size(const ir_raw_range* range);
void ir_raw_source_clear(ir_raw_source* src);
int ir_raw_source_bind(ir_raw_source* src, const void* data, size_t size, int encoding);
int ir_raw_source_view(const ir_raw_source* src, const ir_raw_range* range, const void** out_data, size_t* out_size);
int ir_raw_source_copy(const ir_raw_source* src, const ir_raw_range* range, void* out_mem, size_t out_cap, size_t* out_len);
int ir_raw_source_copy_out(const ir_raw_source* src, const ir_raw_range* range, ir_outbuf* out);

#ifdef __cplusplus
}
#endif

#endif
