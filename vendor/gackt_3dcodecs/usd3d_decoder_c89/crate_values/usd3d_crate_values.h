#ifndef USD3D_CRATE_VALUES_H
#define USD3D_CRATE_VALUES_H

#include "usd3d.h"

#define USD3D_CRATE_VALUE_TEXT_MAX 128

#define USD3D_CRATE_TYPE_UNKNOWN 0
#define USD3D_CRATE_TYPE_TOKEN 1
#define USD3D_CRATE_TYPE_STRING 2
#define USD3D_CRATE_TYPE_INT 3
#define USD3D_CRATE_TYPE_UINT 4
#define USD3D_CRATE_TYPE_FIXED 5
#define USD3D_CRATE_TYPE_VEC3F 6
#define USD3D_CRATE_TYPE_MATRIX4D 7
#define USD3D_CRATE_TYPE_PATH 8
#define USD3D_CRATE_TYPE_ASSET_PATH 9

const char *usd3d_crate_value_type_name(int type_code);
int usd3d_crate_guess_value_type(const char *field_name);
int usd3d_crate_format_le_i32(const unsigned char *src, unsigned long size, char *out, unsigned int out_cap);
int usd3d_crate_format_le_u32(const unsigned char *src, unsigned long size, char *out, unsigned int out_cap);
int usd3d_crate_format_q16(const unsigned char *src, unsigned long size, char *out, unsigned int out_cap);
int usd3d_crate_emit_printable_values(const unsigned char *data,
                                      unsigned long size,
                                      int (*value_cb)(void *user, const char *table_name, unsigned long index, const char *type_name, const char *value_text),
                                      void *user);

#endif
