#ifndef IR_COMMON_H
#define IR_COMMON_H

#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#if ULONG_MAX < 4294967295UL
#error "irrfxlib requires unsigned long to be at least 32 bits"
#endif

#ifndef IR_NULL
#define IR_NULL ((void*)0)
#endif

typedef signed long ir_i32;
typedef unsigned long ir_u32;
typedef unsigned short ir_u16;
typedef unsigned char ir_u8;
typedef long ir_fx16;

enum ir_result {
    IR_OK = 0,
    IR_EOF = 1,
    IR_ERR_GENERIC = -1,
    IR_ERR_ARGUMENT = -2,
    IR_ERR_FORMAT = -3,
    IR_ERR_RANGE = -4,
    IR_ERR_OVERFLOW = -5,
    IR_ERR_CAPACITY = -6,
    IR_ERR_ENCODING = -7,
    IR_ERR_UNSUPPORTED = -8
};

typedef struct ir_vec2fx_tag {
    ir_fx16 x;
    ir_fx16 y;
} ir_vec2fx;

typedef struct ir_vec3fx_tag {
    ir_fx16 x;
    ir_fx16 y;
    ir_fx16 z;
} ir_vec3fx;

typedef struct ir_vec4fx_tag {
    ir_fx16 x;
    ir_fx16 y;
    ir_fx16 z;
    ir_fx16 w;
} ir_vec4fx;

typedef struct ir_recti_tag {
    ir_i32 x0;
    ir_i32 y0;
    ir_i32 x1;
    ir_i32 y1;
} ir_recti;

typedef struct ir_dim2u_tag {
    ir_u32 width;
    ir_u32 height;
} ir_dim2u;

typedef struct ir_line2fx_tag {
    ir_vec2fx start;
    ir_vec2fx end;
} ir_line2fx;

typedef struct ir_line3fx_tag {
    ir_vec3fx start;
    ir_vec3fx end;
} ir_line3fx;

typedef struct ir_plane3fx_tag {
    ir_vec3fx normal;
    ir_fx16 d;
} ir_plane3fx;

typedef struct ir_triangle3fx_tag {
    ir_vec3fx a;
    ir_vec3fx b;
    ir_vec3fx c;
} ir_triangle3fx;

typedef struct ir_box3fx_tag {
    ir_vec3fx min_edge;
    ir_vec3fx max_edge;
} ir_box3fx;

typedef struct ir_strpool_tag {
    char* data;
    size_t cap;
    size_t len;
} ir_strpool;

typedef struct ir_outbuf_tag {
    char* data;
    size_t cap;
    size_t len;
} ir_outbuf;

void ir_strpool_init(ir_strpool* pool, char* data, size_t cap);
int ir_strpool_put(ir_strpool* pool, const char* s, size_t n, ir_u32* off, ir_u16* len);
const char* ir_strpool_get(const ir_strpool* pool, ir_u32 off);

void ir_outbuf_init(ir_outbuf* out, char* data, size_t cap);
int ir_outbuf_putc(ir_outbuf* out, int ch);
int ir_outbuf_puts(ir_outbuf* out, const char* s);
int ir_outbuf_putsn(ir_outbuf* out, const char* s, size_t n);
int ir_outbuf_pad(ir_outbuf* out, char ch, size_t count);
int ir_outbuf_finalize(ir_outbuf* out, size_t* out_len);

int ir_xml_escape_to_out(ir_outbuf* out, const char* s);
int ir_xml_escape_n_to_out(ir_outbuf* out, const char* s, size_t n);

void ir_box3fx_make_empty(ir_box3fx* box);
int ir_box3fx_is_empty(const ir_box3fx* box);
void ir_box3fx_expand(ir_box3fx* box, ir_vec3fx p);

const char* ir_result_name(int rc);

#ifdef __cplusplus
}
#endif

#endif
