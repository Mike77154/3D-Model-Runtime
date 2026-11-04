#ifndef IR_ATTR_H
#define IR_ATTR_H

#include "ir_common.h"
#include "ir_raw.h"
#include "ir_xml.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ir_attr_kind {
    IR_ATTR_UNKNOWN = 0,
    IR_ATTR_STRING = 1,
    IR_ATTR_INT = 2,
    IR_ATTR_FLOAT = 3,
    IR_ATTR_BOOL = 4,
    IR_ATTR_ENUM = 5,
    IR_ATTR_VECTOR3D = 6,
    IR_ATTR_COLOR = 7,
    IR_ATTR_COLORF = 8,
    IR_ATTR_TEXTURE = 9,
    IR_ATTR_BINARY = 10,
    IR_ATTR_VECTOR2D = 11,
    IR_ATTR_POSITION2D = 12,
    IR_ATTR_RECT = 13,
    IR_ATTR_DIMENSION2D = 14,
    IR_ATTR_MATRIX = 15,
    IR_ATTR_QUATERNION = 16,
    IR_ATTR_BOX3D = 17,
    IR_ATTR_PLANE = 18,
    IR_ATTR_TRIANGLE3D = 19,
    IR_ATTR_LINE2D = 20,
    IR_ATTR_LINE3D = 21,
    IR_ATTR_STRINGWARRAY = 22,
    IR_ATTR_USER_POINTER = 23,
    IR_ATTR_FLOATARRAY = 24,
    IR_ATTR_INTARRAY = 25
};

enum ir_irrlicht_attribute_type {
    IR_IRR_EAT_INT = 0,
    IR_IRR_EAT_FLOAT = 1,
    IR_IRR_EAT_STRING = 2,
    IR_IRR_EAT_BOOL = 3,
    IR_IRR_EAT_ENUM = 4,
    IR_IRR_EAT_COLOR = 5,
    IR_IRR_EAT_COLORF = 6,
    IR_IRR_EAT_VECTOR3D = 7,
    IR_IRR_EAT_POSITION2D = 8,
    IR_IRR_EAT_VECTOR2D = 9,
    IR_IRR_EAT_RECT = 10,
    IR_IRR_EAT_MATRIX = 11,
    IR_IRR_EAT_QUATERNION = 12,
    IR_IRR_EAT_BBOX = 13,
    IR_IRR_EAT_PLANE = 14,
    IR_IRR_EAT_TRIANGLE3D = 15,
    IR_IRR_EAT_LINE2D = 16,
    IR_IRR_EAT_LINE3D = 17,
    IR_IRR_EAT_STRINGWARRAY = 18,
    IR_IRR_EAT_FLOATARRAY = 19,
    IR_IRR_EAT_INTARRAY = 20,
    IR_IRR_EAT_BINARY = 21,
    IR_IRR_EAT_TEXTURE = 22,
    IR_IRR_EAT_USER_POINTER = 23,
    IR_IRR_EAT_DIMENSION2D = 24,
    IR_IRR_EAT_COUNT = 25,
    IR_IRR_EAT_UNKNOWN = 26
};

typedef struct ir_attr_tag {
    ir_u8 kind;
    ir_u8 has_raw_value;
    ir_u8 has_extra_attrs;
    ir_u8 reserved;
    unsigned long line;
    unsigned long col;
    ir_raw_range raw_tag;
    ir_u32 tag_off;
    ir_u16 tag_len;
    ir_u32 name_off;
    ir_u16 name_len;
    ir_u32 value_off;
    ir_u16 value_len;
    ir_u32 extra_attrs_off;
    ir_u16 extra_attrs_len;
    union {
        ir_i32 i;
        ir_fx16 fx;
        ir_u8 boolean;
        ir_u32 argb;
        ir_vec2fx vec2;
        ir_vec3fx vec3;
        ir_vec4fx vec4;
    } v;
} ir_attr;

int ir_attr_from_xml_tag(const ir_xml_tag* tag, ir_attr* out, ir_strpool* pool, char* err, size_t err_cap);
int ir_attr_write_xml(const ir_attr* attr, const ir_strpool* pool, ir_outbuf* out, size_t indent, char* err, size_t err_cap);
const char* ir_attr_kind_name(int kind);
const char* ir_attr_tag_cstr(const ir_attr* attr, const ir_strpool* pool);
const char* ir_attr_name_cstr(const ir_attr* attr, const ir_strpool* pool);
const char* ir_attr_value_cstr(const ir_attr* attr, const ir_strpool* pool);
const char* ir_attr_extra_attrs_cstr(const ir_attr* attr, const ir_strpool* pool);
int ir_attr_numeric_count(int kind);
int ir_attr_parse_fx_list(const ir_attr* attr, const ir_strpool* pool, ir_fx16* out, int max_count, int* out_count);
int ir_attr_parse_i32_list(const ir_attr* attr, const ir_strpool* pool, ir_i32* out, int max_count, int* out_count);
int ir_attr_kind_to_irrlicht_eat(int kind);
int ir_attr_kind_from_irrlicht_eat(int eat);
int ir_attr_extra_value(const ir_attr* attr, const ir_strpool* pool, const char* attr_name, char* out, size_t out_cap);
int ir_attr_stringwarray_count(const ir_attr* attr, const ir_strpool* pool, int* out_count);
int ir_attr_stringwarray_value(const ir_attr* attr, const ir_strpool* pool, int index, char* out, size_t out_cap);

/* Convenience typed accessors. They parse the preserved raw XML value when available,
 * and fall back to the cached union value for attrs built programmatically.
 * Numeric list accessors intentionally follow Irrlicht's permissive CNumbersAttribute
 * style: labels, commas, parentheses and extra text are skipped while numbers are read.
 */
int ir_attr_as_i32(const ir_attr* attr, const ir_strpool* pool, ir_i32* out);
int ir_attr_as_u32(const ir_attr* attr, const ir_strpool* pool, ir_u32* out);
int ir_attr_as_fx(const ir_attr* attr, const ir_strpool* pool, ir_fx16* out);
int ir_attr_as_bool(const ir_attr* attr, const ir_strpool* pool, int* out);
int ir_attr_as_argb(const ir_attr* attr, const ir_strpool* pool, ir_u32* out);
int ir_attr_as_vec2(const ir_attr* attr, const ir_strpool* pool, ir_vec2fx* out);
int ir_attr_as_vec3(const ir_attr* attr, const ir_strpool* pool, ir_vec3fx* out);
int ir_attr_as_vec4(const ir_attr* attr, const ir_strpool* pool, ir_vec4fx* out);
int ir_attr_as_box3(const ir_attr* attr, const ir_strpool* pool, ir_box3fx* out);
int ir_attr_as_matrix16(const ir_attr* attr, const ir_strpool* pool, ir_fx16 out_matrix16[16]);
int ir_attr_as_colorf(const ir_attr* attr, const ir_strpool* pool, ir_vec4fx* out_rgba);
int ir_attr_as_quaternion(const ir_attr* attr, const ir_strpool* pool, ir_vec4fx* out_xyzw);
int ir_attr_as_rect_i32(const ir_attr* attr, const ir_strpool* pool, ir_recti* out_rect);
int ir_attr_as_dimension2d_u32(const ir_attr* attr, const ir_strpool* pool, ir_dim2u* out_dim);
int ir_attr_as_line2(const ir_attr* attr, const ir_strpool* pool, ir_line2fx* out_line);
int ir_attr_as_line3(const ir_attr* attr, const ir_strpool* pool, ir_line3fx* out_line);
int ir_attr_as_plane(const ir_attr* attr, const ir_strpool* pool, ir_plane3fx* out_plane);
int ir_attr_as_triangle3(const ir_attr* attr, const ir_strpool* pool, ir_triangle3fx* out_triangle);

#ifdef __cplusplus
}
#endif

#endif
