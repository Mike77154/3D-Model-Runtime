#include "usd3d_crate_values.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>

static unsigned long cv_read_le32(const unsigned char *src)
{
    return ((unsigned long)src[0]) | (((unsigned long)src[1]) << 8) | (((unsigned long)src[2]) << 16) | (((unsigned long)src[3]) << 24);
}

const char *usd3d_crate_value_type_name(int type_code)
{
    switch (type_code) {
    case USD3D_CRATE_TYPE_TOKEN: return "token";
    case USD3D_CRATE_TYPE_STRING: return "string";
    case USD3D_CRATE_TYPE_INT: return "int";
    case USD3D_CRATE_TYPE_UINT: return "uint";
    case USD3D_CRATE_TYPE_FIXED: return "fixed_q16_16";
    case USD3D_CRATE_TYPE_VEC3F: return "vec3_fixed_q16_16";
    case USD3D_CRATE_TYPE_MATRIX4D: return "matrix4_fixed_q16_16";
    case USD3D_CRATE_TYPE_PATH: return "path";
    case USD3D_CRATE_TYPE_ASSET_PATH: return "asset";
    default: return "unknown";
    }
}

int usd3d_crate_guess_value_type(const char *field_name)
{
    if (field_name == 0) return USD3D_CRATE_TYPE_UNKNOWN;
    if (strstr(field_name, "asset") != 0 || strstr(field_name, "file") != 0 || strstr(field_name, "sourceAsset") != 0) return USD3D_CRATE_TYPE_ASSET_PATH;
    if (strstr(field_name, "path") != 0 || strstr(field_name, "target") != 0 || strstr(field_name, "rel") != 0) return USD3D_CRATE_TYPE_PATH;
    if (strstr(field_name, "matrix") != 0 || strstr(field_name, "transform") != 0) return USD3D_CRATE_TYPE_MATRIX4D;
    if (strstr(field_name, "point") != 0 || strstr(field_name, "normal") != 0 || strstr(field_name, "color") != 0 || strstr(field_name, "scale") != 0) return USD3D_CRATE_TYPE_VEC3F;
    if (strstr(field_name, "index") != 0 || strstr(field_name, "count") != 0) return USD3D_CRATE_TYPE_INT;
    if (strstr(field_name, "token") != 0 || strstr(field_name, "id") != 0 || strstr(field_name, "type") != 0) return USD3D_CRATE_TYPE_TOKEN;
    return USD3D_CRATE_TYPE_STRING;
}

int usd3d_crate_format_le_i32(const unsigned char *src, unsigned long size, char *out, unsigned int out_cap)
{
    long v;
    if (src == 0 || out == 0 || size < 4UL || out_cap == 0U) return USD3D_ERR_BAD_ARG;
    v = (long)cv_read_le32(src);
    if ((v & 0x80000000L) != 0L) v = -((long)((~((unsigned long)v) + 1UL) & 0xffffffffUL));
    sprintf(out, "%ld", v);
    return USD3D_OK;
}

int usd3d_crate_format_le_u32(const unsigned char *src, unsigned long size, char *out, unsigned int out_cap)
{
    unsigned long v;
    if (src == 0 || out == 0 || size < 4UL || out_cap == 0U) return USD3D_ERR_BAD_ARG;
    v = cv_read_le32(src);
    sprintf(out, "%lu", v);
    return USD3D_OK;
}

int usd3d_crate_format_q16(const unsigned char *src, unsigned long size, char *out, unsigned int out_cap)
{
    usd3d_fixed v;
    if (src == 0 || out == 0 || size < 4UL || out_cap == 0U) return USD3D_ERR_BAD_ARG;
    v = (usd3d_fixed)cv_read_le32(src);
    usd3d_fixed_to_string(v, out, out_cap, 4U);
    return USD3D_OK;
}

int usd3d_crate_emit_printable_values(const unsigned char *data,
                                      unsigned long size,
                                      int (*value_cb)(void *user, const char *table_name, unsigned long index, const char *type_name, const char *value_text),
                                      void *user)
{
    unsigned long i;
    unsigned long start;
    unsigned long index;
    char text[USD3D_CRATE_VALUE_TEXT_MAX];
    unsigned int n;
    int c;

    if (data == 0 || value_cb == 0) return USD3D_ERR_BAD_ARG;
    index = 0UL;
    i = 0UL;
    while (i < size) {
        c = (int)data[i];
        if (c >= 32 && c <= 126) {
            start = i;
            n = 0U;
            while (i < size) {
                c = (int)data[i];
                if (!(c >= 32 && c <= 126)) break;
                if (n + 1U < (unsigned int)sizeof(text)) text[n++] = (char)c;
                i++;
            }
            text[n] = '\0';
            if (i - start >= 3UL) {
                if (value_cb(user, "PRINTABLE", index, "string/token", text) != 0) return USD3D_ERR_PARSE;
                index++;
            }
        } else {
            i++;
        }
    }
    return USD3D_OK;
}
