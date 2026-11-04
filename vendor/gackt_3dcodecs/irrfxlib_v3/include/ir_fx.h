#ifndef IR_FX_H
#define IR_FX_H

#include "ir_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IR_FX_ONE ((ir_fx16)65536L)

int ir_i32_from_str(const char* s, ir_i32* out);
int ir_u32_from_str(const char* s, ir_u32* out);
int ir_u32_from_irr_index_str(const char* s, ir_u32* out);
int ir_bool_from_str(const char* s, int* out);
int ir_fx_from_str(const char* s, ir_fx16* out);
int ir_fx_parse_list(const char* s, ir_fx16* out, int count);
int ir_vec2fx_from_str(const char* s, ir_vec2fx* out);
int ir_vec3fx_from_str(const char* s, ir_vec3fx* out);
int ir_vec4fx_from_str(const char* s, ir_vec4fx* out);
int ir_box3fx_from_attrs(const char* min_edge, const char* max_edge, ir_box3fx* out);
int ir_color_argb_from_hex(const char* s, ir_u32* out);
int ir_fx_to_str(ir_fx16 value, char* out, size_t cap);
int ir_i32_to_str(ir_i32 value, char* out, size_t cap);
int ir_u32_to_str(ir_u32 value, char* out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
