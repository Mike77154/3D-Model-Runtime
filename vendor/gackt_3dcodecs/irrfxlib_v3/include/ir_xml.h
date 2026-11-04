#ifndef IR_XML_H
#define IR_XML_H

#include "ir_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IR_XML_NAME_CAP
#define IR_XML_NAME_CAP 128
#endif

#ifndef IR_XML_VALUE_CAP
#define IR_XML_VALUE_CAP 4096
#endif

#ifndef IR_XML_MAX_ATTRS
#define IR_XML_MAX_ATTRS 64
#endif

enum ir_xml_encoding {
    IR_XML_UTF8 = 0,
    IR_XML_UTF16LE = 1,
    IR_XML_UTF16BE = 2
};

typedef struct ir_xml_attr_tag {
    char name[IR_XML_NAME_CAP];
    char value[IR_XML_VALUE_CAP];
} ir_xml_attr;

typedef struct ir_xml_tag_tag {
    int is_end;
    int is_empty;
    char name[IR_XML_NAME_CAP];
    int attr_count;
    ir_xml_attr attrs[IR_XML_MAX_ATTRS];
    unsigned long line;
    unsigned long col;
    size_t raw_begin;
    size_t raw_end;
} ir_xml_tag;

typedef struct ir_xml_reader_tag {
    const unsigned char* data;
    size_t size;
    size_t pos;
    int encoding;
    int has_push;
    unsigned long push_cp;
    unsigned long line;
    unsigned long col;
    size_t last_pos_before;
    size_t last_pos_after;
    size_t push_pos_before;
    size_t push_pos_after;
    int in_cdata;
} ir_xml_reader;

void ir_xml_reader_init(ir_xml_reader* xr, const void* data, size_t size);
size_t ir_xml_reader_raw_offset(const ir_xml_reader* xr);
int ir_xml_next_tag(ir_xml_reader* xr, ir_xml_tag* tag, char* err, size_t err_cap);
int ir_xml_skip_element(ir_xml_reader* xr, const ir_xml_tag* start_tag, char* err, size_t err_cap);
const char* ir_xml_tag_attr(const ir_xml_tag* tag, const char* name);
const char* ir_xml_tag_attr_local(const ir_xml_tag* tag, const char* local_name);
const char* ir_xml_local_name(const char* name);
int ir_xml_name_eq(const char* actual, const char* expected_local);
int ir_xml_peek_char(ir_xml_reader* xr, unsigned long* cp);
int ir_xml_get_char(ir_xml_reader* xr, unsigned long* cp);
int ir_xml_skip_space(ir_xml_reader* xr);
int ir_xml_read_token(ir_xml_reader* xr, char* out, size_t out_cap, int stop_on_lt, int* hit_lt, char* err, size_t err_cap);
int ir_xml_read_expected_end(ir_xml_reader* xr, const char* name, char* err, size_t err_cap);
int ir_xml_read_expected_end_tag(ir_xml_reader* xr, const char* name, ir_xml_tag* out_tag, char* err, size_t err_cap);
const char* ir_xml_encoding_name(int encoding);

#ifdef __cplusplus
}
#endif

#endif
