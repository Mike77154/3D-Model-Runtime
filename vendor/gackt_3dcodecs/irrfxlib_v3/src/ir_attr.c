#include "ir_attr.h"
#include "ir_fx.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>

#define IR_ATTR_EXTRA_XML_CAP (IR_XML_MAX_ATTRS * (IR_XML_NAME_CAP + IR_XML_VALUE_CAP + 16))

static int ir_attr_kind_from_tag_name(const char* tag)
{
    tag = ir_xml_local_name(tag);
    if (strcmp(tag, "string") == 0) return IR_ATTR_STRING;
    if (strcmp(tag, "int") == 0) return IR_ATTR_INT;
    if (strcmp(tag, "float") == 0) return IR_ATTR_FLOAT;
    if (strcmp(tag, "bool") == 0) return IR_ATTR_BOOL;
    if (strcmp(tag, "enum") == 0) return IR_ATTR_ENUM;
    if (strcmp(tag, "vector3d") == 0) return IR_ATTR_VECTOR3D;
    if (strcmp(tag, "color") == 0) return IR_ATTR_COLOR;
    if (strcmp(tag, "colorf") == 0) return IR_ATTR_COLORF;
    if (strcmp(tag, "texture") == 0) return IR_ATTR_TEXTURE;
    if (strcmp(tag, "binary") == 0) return IR_ATTR_BINARY;
    if (strcmp(tag, "vector2d") == 0) return IR_ATTR_VECTOR2D;
    if (strcmp(tag, "position") == 0) return IR_ATTR_POSITION2D;
    if (strcmp(tag, "rect") == 0) return IR_ATTR_RECT;
    if (strcmp(tag, "dimension2d") == 0) return IR_ATTR_DIMENSION2D;
    if (strcmp(tag, "matrix") == 0) return IR_ATTR_MATRIX;
    if (strcmp(tag, "quaternion") == 0) return IR_ATTR_QUATERNION;
    if (strcmp(tag, "box3d") == 0) return IR_ATTR_BOX3D;
    if (strcmp(tag, "plane") == 0) return IR_ATTR_PLANE;
    if (strcmp(tag, "triangle") == 0) return IR_ATTR_TRIANGLE3D;
    if (strcmp(tag, "line2d") == 0) return IR_ATTR_LINE2D;
    if (strcmp(tag, "line3d") == 0) return IR_ATTR_LINE3D;
    if (strcmp(tag, "stringwarray") == 0) return IR_ATTR_STRINGWARRAY;
    if (strcmp(tag, "userPointer") == 0) return IR_ATTR_USER_POINTER;
    if (strcmp(tag, "floatlist") == 0) return IR_ATTR_FLOATARRAY;
    if (strcmp(tag, "intlist") == 0) return IR_ATTR_INTARRAY;
    if (strcmp(tag, "floatarray") == 0) return IR_ATTR_FLOATARRAY;
    if (strcmp(tag, "intarray") == 0) return IR_ATTR_INTARRAY;
    return IR_ATTR_UNKNOWN;
}

const char* ir_attr_kind_name(int kind)
{
    switch (kind) {
    case IR_ATTR_STRING: return "string";
    case IR_ATTR_INT: return "int";
    case IR_ATTR_FLOAT: return "float";
    case IR_ATTR_BOOL: return "bool";
    case IR_ATTR_ENUM: return "enum";
    case IR_ATTR_VECTOR3D: return "vector3d";
    case IR_ATTR_COLOR: return "color";
    case IR_ATTR_COLORF: return "colorf";
    case IR_ATTR_TEXTURE: return "texture";
    case IR_ATTR_BINARY: return "binary";
    case IR_ATTR_VECTOR2D: return "vector2d";
    case IR_ATTR_POSITION2D: return "position";
    case IR_ATTR_RECT: return "rect";
    case IR_ATTR_DIMENSION2D: return "dimension2d";
    case IR_ATTR_MATRIX: return "matrix";
    case IR_ATTR_QUATERNION: return "quaternion";
    case IR_ATTR_BOX3D: return "box3d";
    case IR_ATTR_PLANE: return "plane";
    case IR_ATTR_TRIANGLE3D: return "triangle";
    case IR_ATTR_LINE2D: return "line2d";
    case IR_ATTR_LINE3D: return "line3d";
    case IR_ATTR_STRINGWARRAY: return "stringwarray";
    case IR_ATTR_USER_POINTER: return "userPointer";
    case IR_ATTR_FLOATARRAY: return "floatlist";
    case IR_ATTR_INTARRAY: return "intlist";
    default: return "unknown";
    }
}

const char* ir_attr_tag_cstr(const ir_attr* attr, const ir_strpool* pool)
{
    const char* s;
    if (!attr || !pool) {
        return IR_NULL;
    }
    s = ir_strpool_get(pool, attr->tag_off);
    if (!s || !*s) {
        s = ir_attr_kind_name((int)attr->kind);
    }
    return s;
}

const char* ir_attr_name_cstr(const ir_attr* attr, const ir_strpool* pool)
{
    if (!attr || !pool) {
        return IR_NULL;
    }
    return ir_strpool_get(pool, attr->name_off);
}

const char* ir_attr_value_cstr(const ir_attr* attr, const ir_strpool* pool)
{
    if (!attr || !pool || !attr->has_raw_value) {
        return IR_NULL;
    }
    return ir_strpool_get(pool, attr->value_off);
}

const char* ir_attr_extra_attrs_cstr(const ir_attr* attr, const ir_strpool* pool)
{
    if (!attr || !pool || !attr->has_extra_attrs) {
        return IR_NULL;
    }
    return ir_strpool_get(pool, attr->extra_attrs_off);
}

int ir_attr_kind_to_irrlicht_eat(int kind)
{
    switch (kind) {
    case IR_ATTR_INT: return IR_IRR_EAT_INT;
    case IR_ATTR_FLOAT: return IR_IRR_EAT_FLOAT;
    case IR_ATTR_STRING: return IR_IRR_EAT_STRING;
    case IR_ATTR_BOOL: return IR_IRR_EAT_BOOL;
    case IR_ATTR_ENUM: return IR_IRR_EAT_ENUM;
    case IR_ATTR_COLOR: return IR_IRR_EAT_COLOR;
    case IR_ATTR_COLORF: return IR_IRR_EAT_COLORF;
    case IR_ATTR_VECTOR3D: return IR_IRR_EAT_VECTOR3D;
    case IR_ATTR_POSITION2D: return IR_IRR_EAT_POSITION2D;
    case IR_ATTR_VECTOR2D: return IR_IRR_EAT_VECTOR2D;
    case IR_ATTR_RECT: return IR_IRR_EAT_RECT;
    case IR_ATTR_MATRIX: return IR_IRR_EAT_MATRIX;
    case IR_ATTR_QUATERNION: return IR_IRR_EAT_QUATERNION;
    case IR_ATTR_BOX3D: return IR_IRR_EAT_BBOX;
    case IR_ATTR_PLANE: return IR_IRR_EAT_PLANE;
    case IR_ATTR_TRIANGLE3D: return IR_IRR_EAT_TRIANGLE3D;
    case IR_ATTR_LINE2D: return IR_IRR_EAT_LINE2D;
    case IR_ATTR_LINE3D: return IR_IRR_EAT_LINE3D;
    case IR_ATTR_STRINGWARRAY: return IR_IRR_EAT_STRINGWARRAY;
    case IR_ATTR_FLOATARRAY: return IR_IRR_EAT_FLOATARRAY;
    case IR_ATTR_INTARRAY: return IR_IRR_EAT_INTARRAY;
    case IR_ATTR_BINARY: return IR_IRR_EAT_BINARY;
    case IR_ATTR_TEXTURE: return IR_IRR_EAT_TEXTURE;
    case IR_ATTR_USER_POINTER: return IR_IRR_EAT_USER_POINTER;
    case IR_ATTR_DIMENSION2D: return IR_IRR_EAT_DIMENSION2D;
    default: return IR_IRR_EAT_UNKNOWN;
    }
}

int ir_attr_kind_from_irrlicht_eat(int eat)
{
    switch (eat) {
    case IR_IRR_EAT_INT: return IR_ATTR_INT;
    case IR_IRR_EAT_FLOAT: return IR_ATTR_FLOAT;
    case IR_IRR_EAT_STRING: return IR_ATTR_STRING;
    case IR_IRR_EAT_BOOL: return IR_ATTR_BOOL;
    case IR_IRR_EAT_ENUM: return IR_ATTR_ENUM;
    case IR_IRR_EAT_COLOR: return IR_ATTR_COLOR;
    case IR_IRR_EAT_COLORF: return IR_ATTR_COLORF;
    case IR_IRR_EAT_VECTOR3D: return IR_ATTR_VECTOR3D;
    case IR_IRR_EAT_POSITION2D: return IR_ATTR_POSITION2D;
    case IR_IRR_EAT_VECTOR2D: return IR_ATTR_VECTOR2D;
    case IR_IRR_EAT_RECT: return IR_ATTR_RECT;
    case IR_IRR_EAT_MATRIX: return IR_ATTR_MATRIX;
    case IR_IRR_EAT_QUATERNION: return IR_ATTR_QUATERNION;
    case IR_IRR_EAT_BBOX: return IR_ATTR_BOX3D;
    case IR_IRR_EAT_PLANE: return IR_ATTR_PLANE;
    case IR_IRR_EAT_TRIANGLE3D: return IR_ATTR_TRIANGLE3D;
    case IR_IRR_EAT_LINE2D: return IR_ATTR_LINE2D;
    case IR_IRR_EAT_LINE3D: return IR_ATTR_LINE3D;
    case IR_IRR_EAT_STRINGWARRAY: return IR_ATTR_STRINGWARRAY;
    case IR_IRR_EAT_FLOATARRAY: return IR_ATTR_FLOATARRAY;
    case IR_IRR_EAT_INTARRAY: return IR_ATTR_INTARRAY;
    case IR_IRR_EAT_BINARY: return IR_ATTR_BINARY;
    case IR_IRR_EAT_TEXTURE: return IR_ATTR_TEXTURE;
    case IR_IRR_EAT_USER_POINTER: return IR_ATTR_USER_POINTER;
    case IR_IRR_EAT_DIMENSION2D: return IR_ATTR_DIMENSION2D;
    default: return IR_ATTR_UNKNOWN;
    }
}

static int ir_attr_extra_name_char(int ch)
{
    return ch == '_' || ch == ':' || ch == '-' || ch == '.' ||
        (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9');
}

static int ir_attr_append_utf8_local(char* out, size_t cap, size_t* len, unsigned long cp)
{
    if (!out || !len || cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    if ((cp >= 0xD800UL && cp <= 0xDFFFUL) || cp > 0x10FFFFUL) {
        return IR_ERR_ENCODING;
    }
    if (cp < 0x80UL) {
        if (*len + 1U >= cap) return IR_ERR_CAPACITY;
        out[(*len)++] = (char)cp;
    } else if (cp < 0x800UL) {
        if (*len + 2U >= cap) return IR_ERR_CAPACITY;
        out[(*len)++] = (char)(0xC0U | ((cp >> 6) & 0x1FU));
        out[(*len)++] = (char)(0x80U | (cp & 0x3FU));
    } else if (cp < 0x10000UL) {
        if (*len + 3U >= cap) return IR_ERR_CAPACITY;
        out[(*len)++] = (char)(0xE0U | ((cp >> 12) & 0x0FU));
        out[(*len)++] = (char)(0x80U | ((cp >> 6) & 0x3FU));
        out[(*len)++] = (char)(0x80U | (cp & 0x3FU));
    } else {
        if (*len + 4U >= cap) return IR_ERR_CAPACITY;
        out[(*len)++] = (char)(0xF0U | ((cp >> 18) & 0x07U));
        out[(*len)++] = (char)(0x80U | ((cp >> 12) & 0x3FU));
        out[(*len)++] = (char)(0x80U | ((cp >> 6) & 0x3FU));
        out[(*len)++] = (char)(0x80U | (cp & 0x3FU));
    }
    out[*len] = '\0';
    return IR_OK;
}

static int ir_attr_decode_entity_local(const char** cursor, unsigned long* out_cp)
{
    char name[32];
    const char* p;
    size_t n;
    unsigned long value;
    size_t i;
    if (!cursor || !*cursor || !out_cp || **cursor != '&') {
        return IR_ERR_ARGUMENT;
    }
    p = *cursor + 1;
    n = 0U;
    while (*p && *p != ';') {
        if (n + 1U >= sizeof(name)) {
            return IR_ERR_OVERFLOW;
        }
        name[n++] = *p++;
    }
    if (*p != ';') {
        return IR_ERR_FORMAT;
    }
    name[n] = '\0';
    *cursor = p + 1;
    if (strcmp(name, "amp") == 0) { *out_cp = '&'; return IR_OK; }
    if (strcmp(name, "lt") == 0) { *out_cp = '<'; return IR_OK; }
    if (strcmp(name, "gt") == 0) { *out_cp = '>'; return IR_OK; }
    if (strcmp(name, "quot") == 0) { *out_cp = '"'; return IR_OK; }
    if (strcmp(name, "apos") == 0) { *out_cp = '\''; return IR_OK; }
    if (name[0] != '#') {
        return IR_ERR_UNSUPPORTED;
    }
    value = 0UL;
    if (name[1] == 'x' || name[1] == 'X') {
        for (i = 2U; name[i] != '\0'; ++i) {
            int ch;
            unsigned long v;
            ch = (unsigned char)name[i];
            if (ch >= '0' && ch <= '9') v = (unsigned long)(ch - '0');
            else if (ch >= 'a' && ch <= 'f') v = (unsigned long)(10 + ch - 'a');
            else if (ch >= 'A' && ch <= 'F') v = (unsigned long)(10 + ch - 'A');
            else return IR_ERR_FORMAT;
            value = (value << 4) | v;
        }
    } else {
        for (i = 1U; name[i] != '\0'; ++i) {
            int ch;
            ch = (unsigned char)name[i];
            if (ch < '0' || ch > '9') return IR_ERR_FORMAT;
            value = value * 10UL + (unsigned long)(ch - '0');
        }
    }
    *out_cp = value;
    return IR_OK;
}

static int ir_attr_copy_extra_value(const char* begin, const char* end, char* out, size_t out_cap)
{
    const char* p;
    size_t len;
    int rc;
    if (!begin || !end || !out || out_cap == 0U || end < begin) {
        return IR_ERR_ARGUMENT;
    }
    len = 0U;
    out[0] = '\0';
    p = begin;
    while (p < end) {
        unsigned long cp;
        if (*p == '&') {
            const char* save;
            save = p;
            rc = ir_attr_decode_entity_local(&p, &cp);
            if (rc != IR_OK) {
                p = save;
                cp = (unsigned long)(unsigned char)*p++;
            }
        } else {
            cp = (unsigned long)(unsigned char)*p++;
        }
        rc = ir_attr_append_utf8_local(out, out_cap, &len, cp);
        if (rc != IR_OK) {
            return rc;
        }
    }
    return IR_OK;
}

int ir_attr_extra_value(const ir_attr* attr, const ir_strpool* pool, const char* attr_name, char* out, size_t out_cap)
{
    const char* extra;
    const char* p;
    size_t want_len;
    if (!attr || !pool || !attr_name || !out || out_cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    out[0] = '\0';
    extra = ir_attr_extra_attrs_cstr(attr, pool);
    if (!extra) {
        return IR_EOF;
    }
    attr_name = ir_xml_local_name(attr_name);
    want_len = strlen(attr_name);
    p = extra;
    while (*p) {
        const char* name_begin;
        const char* name_end;
        const char* value_begin;
        const char* value_end;
        char quote;
        while (*p && isspace((unsigned char)*p)) {
            ++p;
        }
        if (!*p) {
            break;
        }
        name_begin = p;
        while (*p && ir_attr_extra_name_char((unsigned char)*p)) {
            ++p;
        }
        name_end = p;
        while (*p && isspace((unsigned char)*p)) {
            ++p;
        }
        if (*p != '=') {
            return IR_ERR_FORMAT;
        }
        ++p;
        while (*p && isspace((unsigned char)*p)) {
            ++p;
        }
        if (*p != '"' && *p != '\'') {
            return IR_ERR_FORMAT;
        }
        quote = *p++;
        value_begin = p;
        while (*p && *p != quote) {
            ++p;
        }
        if (*p != quote) {
            return IR_ERR_FORMAT;
        }
        value_end = p;
        ++p;
        if ((size_t)(name_end - name_begin) == want_len && memcmp(name_begin, attr_name, want_len) == 0) {
            return ir_attr_copy_extra_value(value_begin, value_end, out, out_cap);
        }
    }
    return IR_EOF;
}

int ir_attr_stringwarray_count(const ir_attr* attr, const ir_strpool* pool, int* out_count)
{
    char value[IR_XML_VALUE_CAP];
    int rc;
    ir_i32 count;
    int inferred;
    if (!attr || !pool || !out_count) {
        return IR_ERR_ARGUMENT;
    }
    if (attr->kind != IR_ATTR_STRINGWARRAY) {
        return IR_ERR_FORMAT;
    }
    rc = ir_attr_extra_value(attr, pool, "count", value, sizeof(value));
    if (rc == IR_OK) {
        rc = ir_i32_from_str(value, &count);
        if (rc != IR_OK) return rc;
        if (count < 0) return IR_ERR_RANGE;
        *out_count = (int)count;
        return IR_OK;
    }
    if (rc != IR_EOF) {
        return rc;
    }
    inferred = 0;
    for (;;) {
        char key[32];
        char tmp[8];
        if (inferred >= 1000000) {
            return IR_ERR_RANGE;
        }
        strcpy(key, "value");
        rc = ir_i32_to_str((ir_i32)inferred, tmp, sizeof(tmp));
        if (rc != IR_OK) return rc;
        strcat(key, tmp);
        rc = ir_attr_extra_value(attr, pool, key, value, sizeof(value));
        if (rc == IR_EOF) {
            break;
        }
        if (rc != IR_OK) {
            return rc;
        }
        ++inferred;
    }
    *out_count = inferred;
    return IR_OK;
}

int ir_attr_stringwarray_value(const ir_attr* attr, const ir_strpool* pool, int index, char* out, size_t out_cap)
{
    char key[32];
    char tmp[16];
    int rc;
    if (!attr || !pool || !out || out_cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    if (attr->kind != IR_ATTR_STRINGWARRAY || index < 0) {
        return IR_ERR_FORMAT;
    }
    strcpy(key, "value");
    rc = ir_i32_to_str((ir_i32)index, tmp, sizeof(tmp));
    if (rc != IR_OK) return rc;
    if (strlen(key) + strlen(tmp) + 1U > sizeof(key)) {
        return IR_ERR_CAPACITY;
    }
    strcat(key, tmp);
    return ir_attr_extra_value(attr, pool, key, out, out_cap);
}

int ir_attr_numeric_count(int kind)
{
    switch (kind) {
    case IR_ATTR_INT:
    case IR_ATTR_FLOAT:
    case IR_ATTR_BOOL:
        return 1;
    case IR_ATTR_VECTOR2D:
    case IR_ATTR_POSITION2D:
    case IR_ATTR_DIMENSION2D:
        return 2;
    case IR_ATTR_VECTOR3D:
        return 3;
    case IR_ATTR_COLOR:
    case IR_ATTR_COLORF:
    case IR_ATTR_QUATERNION:
    case IR_ATTR_RECT:
    case IR_ATTR_PLANE:
    case IR_ATTR_LINE2D:
        return 4;
    case IR_ATTR_BOX3D:
    case IR_ATTR_LINE3D:
        return 6;
    case IR_ATTR_TRIANGLE3D:
        return 9;
    case IR_ATTR_MATRIX:
        return 16;
    case IR_ATTR_FLOATARRAY:
    case IR_ATTR_INTARRAY:
        return -1;
    default:
        return 0;
    }
}

static int ir_attr_is_number_start(int ch)
{
    return ch == '+' || ch == '-' || ch == '.' || isdigit((unsigned char)ch);
}

static int ir_attr_next_number_token(const char** cursor, char* token, size_t cap)
{
    const char* p;
    const char* start;
    const char* base;
    size_t n;
    int digits;
    int exp_digits;
    if (!cursor || !*cursor || !token || cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    p = *cursor;
    base = p;
    for (;;) {
        while (*p && !ir_attr_is_number_start((unsigned char)*p)) {
            ++p;
        }
        if (!*p) {
            *cursor = p;
            token[0] = '\0';
            return IR_EOF;
        }
        if (p > base && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) {
            ++p;
            continue;
        }
        start = p;
        if (*p == '+' || *p == '-') {
            ++p;
        }
        digits = 0;
        while (isdigit((unsigned char)*p)) {
            ++p;
            ++digits;
        }
        if (*p == '.') {
            ++p;
            while (isdigit((unsigned char)*p)) {
                ++p;
                ++digits;
            }
        }
        if (digits == 0) {
            p = start + 1;
            continue;
        }
        if (*p == 'e' || *p == 'E') {
            const char* exp_start;
            exp_start = p;
            ++p;
            if (*p == '+' || *p == '-') {
                ++p;
            }
            exp_digits = 0;
            while (isdigit((unsigned char)*p)) {
                ++p;
                ++exp_digits;
            }
            if (exp_digits == 0) {
                p = exp_start;
            }
        }
        n = (size_t)(p - start);
        if (n + 1U > cap) {
            return IR_ERR_OVERFLOW;
        }
        memcpy(token, start, n);
        token[n] = '\0';
        *cursor = p;
        return IR_OK;
    }
}

static int ir_attr_parse_fx_list_value(const char* value, ir_fx16* out, int max_count, int* out_count)
{
    const char* p;
    char token[96];
    int count;
    int rc;
    if (!value || max_count < 0 || (max_count > 0 && !out)) {
        return IR_ERR_ARGUMENT;
    }
    p = value;
    count = 0;
    while (count < max_count) {
        rc = ir_attr_next_number_token(&p, token, sizeof(token));
        if (rc == IR_EOF) {
            break;
        }
        if (rc != IR_OK) {
            return rc;
        }
        rc = ir_fx_from_str(token, &out[count]);
        if (rc != IR_OK) {
            return rc;
        }
        ++count;
    }
    if (out_count) {
        *out_count = count;
    }
    return IR_OK;
}

static int ir_attr_parse_i32_list_value(const char* value, ir_i32* out, int max_count, int* out_count)
{
    const char* p;
    char token[96];
    int count;
    int rc;
    if (!value || max_count < 0 || (max_count > 0 && !out)) {
        return IR_ERR_ARGUMENT;
    }
    p = value;
    count = 0;
    while (count < max_count) {
        ir_fx16 fx;
        rc = ir_attr_next_number_token(&p, token, sizeof(token));
        if (rc == IR_EOF) {
            break;
        }
        if (rc != IR_OK) {
            return rc;
        }
        rc = ir_i32_from_str(token, &out[count]);
        if (rc != IR_OK) {
            rc = ir_fx_from_str(token, &fx);
            if (rc != IR_OK) {
                return rc;
            }
            out[count] = (ir_i32)(fx / IR_FX_ONE);
        }
        ++count;
    }
    if (out_count) {
        *out_count = count;
    }
    return IR_OK;
}

int ir_attr_parse_fx_list(const ir_attr* attr, const ir_strpool* pool, ir_fx16* out, int max_count, int* out_count)
{
    const char* value;
    value = ir_attr_value_cstr(attr, pool);
    if (!value) {
        return IR_ERR_FORMAT;
    }
    return ir_attr_parse_fx_list_value(value, out, max_count, out_count);
}

int ir_attr_parse_i32_list(const ir_attr* attr, const ir_strpool* pool, ir_i32* out, int max_count, int* out_count)
{
    const char* value;
    value = ir_attr_value_cstr(attr, pool);
    if (!value) {
        return IR_ERR_FORMAT;
    }
    return ir_attr_parse_i32_list_value(value, out, max_count, out_count);
}


static int ir_attr_i32_from_irr_string(const char* value, ir_i32* out);
static int ir_attr_color_from_value(const char* value, ir_u32* out);

static int ir_attr_parse_fx_exact_value(const char* value, ir_fx16* out, int count)
{
    int got;
    int rc;
    if (!value || !out || count <= 0) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_attr_parse_fx_list_value(value, out, count, &got);
    if (rc != IR_OK) {
        return rc;
    }
    if (got != count) {
        return IR_ERR_FORMAT;
    }
    return IR_OK;
}

static int ir_attr_vec2_from_value(const char* value, ir_vec2fx* out)
{
    ir_fx16 tmp[2];
    int rc;
    if (!out) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_attr_parse_fx_exact_value(value, tmp, 2);
    if (rc != IR_OK) {
        return rc;
    }
    out->x = tmp[0];
    out->y = tmp[1];
    return IR_OK;
}

static int ir_attr_vec3_from_value(const char* value, ir_vec3fx* out)
{
    ir_fx16 tmp[3];
    int rc;
    if (!out) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_attr_parse_fx_exact_value(value, tmp, 3);
    if (rc != IR_OK) {
        return rc;
    }
    out->x = tmp[0];
    out->y = tmp[1];
    out->z = tmp[2];
    return IR_OK;
}

static int ir_attr_vec4_from_value(const char* value, ir_vec4fx* out)
{
    ir_fx16 tmp[4];
    int rc;
    if (!out) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_attr_parse_fx_exact_value(value, tmp, 4);
    if (rc != IR_OK) {
        return rc;
    }
    out->x = tmp[0];
    out->y = tmp[1];
    out->z = tmp[2];
    out->w = tmp[3];
    return IR_OK;
}

static int ir_attr_bool_ascii_ci_eq(const char* s, size_t n, const char* lit)
{
    size_t i;
    if (!s || !lit || strlen(lit) != n) {
        return 0;
    }
    for (i = 0U; i < n; ++i) {
        int a;
        int b;
        a = (unsigned char)s[i];
        b = (unsigned char)lit[i];
        if (a >= 'A' && a <= 'Z') a = a - 'A' + 'a';
        if (b >= 'A' && b <= 'Z') b = b - 'A' + 'a';
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static int ir_attr_bool_from_value(const char* value, int* out)
{
    const char* p;
    size_t n;
    ir_fx16 nums[16];
    int count;
    int rc;
    if (!value || !out) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_bool_from_str(value, out);
    if (rc == IR_OK) {
        return IR_OK;
    }
    p = value;
    while (*p && isspace((unsigned char)*p)) {
        ++p;
    }
    n = strlen(p);
    while (n > 0U && isspace((unsigned char)p[n - 1U])) {
        --n;
    }
    if (ir_attr_bool_ascii_ci_eq(p, n, "yes") || ir_attr_bool_ascii_ci_eq(p, n, "on")) {
        *out = 1;
        return IR_OK;
    }
    if (ir_attr_bool_ascii_ci_eq(p, n, "no") || ir_attr_bool_ascii_ci_eq(p, n, "off") || ir_attr_bool_ascii_ci_eq(p, n, "none")) {
        *out = 0;
        return IR_OK;
    }
    count = 0;
    rc = ir_attr_parse_fx_list_value(value, nums, (int)(sizeof(nums) / sizeof(nums[0])), &count);
    if (rc == IR_OK && count > 0) {
        int i;
        *out = 0;
        for (i = 0; i < count; ++i) {
            if (nums[i] != 0) {
                *out = 1;
                break;
            }
        }
        return IR_OK;
    }
    return IR_ERR_FORMAT;
}

static int ir_attr_fx01_to_u8(ir_fx16 value)
{
    long v;
    if (value <= 0) {
        return 0;
    }
    if (value >= IR_FX_ONE) {
        return 255;
    }
    v = (long)(((value * 255L) + (IR_FX_ONE / 2L)) / IR_FX_ONE);
    if (v < 0L) return 0;
    if (v > 255L) return 255;
    return (int)v;
}

static ir_u32 ir_attr_colorf_to_argb(const ir_vec4fx* c)
{
    unsigned long r;
    unsigned long g;
    unsigned long b;
    unsigned long a;
    r = (unsigned long)ir_attr_fx01_to_u8(c->x);
    g = (unsigned long)ir_attr_fx01_to_u8(c->y);
    b = (unsigned long)ir_attr_fx01_to_u8(c->z);
    a = (unsigned long)ir_attr_fx01_to_u8(c->w);
    return (ir_u32)((a << 24) | (r << 16) | (g << 8) | b);
}

int ir_attr_as_i32(const ir_attr* attr, const ir_strpool* pool, ir_i32* out)
{
    const char* value;
    ir_fx16 fx;
    int rc;
    if (!attr || !out) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (value) {
        return ir_attr_i32_from_irr_string(value, out);
    }
    switch (attr->kind) {
    case IR_ATTR_INT:
        *out = attr->v.i;
        return IR_OK;
    case IR_ATTR_FLOAT:
        *out = (ir_i32)(attr->v.fx / IR_FX_ONE);
        return IR_OK;
    case IR_ATTR_BOOL:
        *out = attr->v.boolean ? 1L : 0L;
        return IR_OK;
    default:
        rc = ir_attr_as_fx(attr, pool, &fx);
        if (rc == IR_OK) {
            *out = (ir_i32)(fx / IR_FX_ONE);
        }
        return rc;
    }
}

int ir_attr_as_u32(const ir_attr* attr, const ir_strpool* pool, ir_u32* out)
{
    const char* value;
    ir_i32 tmp;
    int rc;
    if (!attr || !out) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (value) {
        return ir_u32_from_irr_index_str(value, out);
    }
    rc = ir_attr_as_i32(attr, pool, &tmp);
    if (rc != IR_OK) {
        return rc;
    }
    if (tmp < 0) {
        return IR_ERR_RANGE;
    }
    *out = (ir_u32)tmp;
    return IR_OK;
}

int ir_attr_as_fx(const ir_attr* attr, const ir_strpool* pool, ir_fx16* out)
{
    const char* value;
    int count;
    int rc;
    if (!attr || !out) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (value) {
        rc = ir_fx_from_str(value, out);
        if (rc == IR_OK) {
            return IR_OK;
        }
        rc = ir_attr_parse_fx_list_value(value, out, 1, &count);
        if (rc == IR_OK && count == 1) {
            return IR_OK;
        }
        return rc;
    }
    switch (attr->kind) {
    case IR_ATTR_FLOAT:
        *out = attr->v.fx;
        return IR_OK;
    case IR_ATTR_INT:
        *out = (ir_fx16)(attr->v.i * IR_FX_ONE);
        return IR_OK;
    case IR_ATTR_BOOL:
        *out = attr->v.boolean ? IR_FX_ONE : 0;
        return IR_OK;
    default:
        return IR_ERR_FORMAT;
    }
}

int ir_attr_as_bool(const ir_attr* attr, const ir_strpool* pool, int* out)
{
    const char* value;
    if (!attr || !out) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (value) {
        return ir_attr_bool_from_value(value, out);
    }
    switch (attr->kind) {
    case IR_ATTR_BOOL:
        *out = attr->v.boolean ? 1 : 0;
        return IR_OK;
    case IR_ATTR_INT:
        *out = attr->v.i != 0;
        return IR_OK;
    case IR_ATTR_FLOAT:
        *out = attr->v.fx != 0;
        return IR_OK;
    default:
        return IR_ERR_FORMAT;
    }
}

int ir_attr_as_argb(const ir_attr* attr, const ir_strpool* pool, ir_u32* out)
{
    const char* value;
    int rc;
    if (!attr || !out) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (value) {
        if (attr->kind == IR_ATTR_COLORF) {
            ir_vec4fx c;
            rc = ir_attr_vec4_from_value(value, &c);
            if (rc == IR_OK) {
                *out = ir_attr_colorf_to_argb(&c);
                return IR_OK;
            }
        }
        return ir_attr_color_from_value(value, out);
    }
    if (attr->kind == IR_ATTR_COLOR) {
        *out = attr->v.argb;
        return IR_OK;
    }
    if (attr->kind == IR_ATTR_COLORF) {
        *out = ir_attr_colorf_to_argb(&attr->v.vec4);
        return IR_OK;
    }
    return IR_ERR_FORMAT;
}

int ir_attr_as_vec2(const ir_attr* attr, const ir_strpool* pool, ir_vec2fx* out)
{
    const char* value;
    if (!attr || !out) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (value) {
        return ir_attr_vec2_from_value(value, out);
    }
    if (attr->kind == IR_ATTR_VECTOR2D || attr->kind == IR_ATTR_POSITION2D || attr->kind == IR_ATTR_DIMENSION2D) {
        *out = attr->v.vec2;
        return IR_OK;
    }
    return IR_ERR_FORMAT;
}

int ir_attr_as_vec3(const ir_attr* attr, const ir_strpool* pool, ir_vec3fx* out)
{
    const char* value;
    if (!attr || !out) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (value) {
        return ir_attr_vec3_from_value(value, out);
    }
    if (attr->kind == IR_ATTR_VECTOR3D) {
        *out = attr->v.vec3;
        return IR_OK;
    }
    return IR_ERR_FORMAT;
}

int ir_attr_as_vec4(const ir_attr* attr, const ir_strpool* pool, ir_vec4fx* out)
{
    const char* value;
    if (!attr || !out) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (value) {
        return ir_attr_vec4_from_value(value, out);
    }
    if (attr->kind == IR_ATTR_COLORF || attr->kind == IR_ATTR_QUATERNION) {
        *out = attr->v.vec4;
        return IR_OK;
    }
    return IR_ERR_FORMAT;
}

int ir_attr_as_box3(const ir_attr* attr, const ir_strpool* pool, ir_box3fx* out)
{
    const char* value;
    ir_fx16 tmp[6];
    int rc;
    if (!attr || !out) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (!value) {
        return IR_ERR_FORMAT;
    }
    rc = ir_attr_parse_fx_exact_value(value, tmp, 6);
    if (rc != IR_OK) {
        return rc;
    }
    out->min_edge.x = tmp[0];
    out->min_edge.y = tmp[1];
    out->min_edge.z = tmp[2];
    out->max_edge.x = tmp[3];
    out->max_edge.y = tmp[4];
    out->max_edge.z = tmp[5];
    return IR_OK;
}

int ir_attr_as_matrix16(const ir_attr* attr, const ir_strpool* pool, ir_fx16 out_matrix16[16])
{
    const char* value;
    if (!attr || !out_matrix16) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (!value) {
        return IR_ERR_FORMAT;
    }
    return ir_attr_parse_fx_exact_value(value, out_matrix16, 16);
}

static ir_fx16 ir_attr_u8_to_fx01(unsigned long v)
{
    if (v > 255UL) {
        v = 255UL;
    }
    return (ir_fx16)((v * (unsigned long)IR_FX_ONE + 127UL) / 255UL);
}

int ir_attr_as_colorf(const ir_attr* attr, const ir_strpool* pool, ir_vec4fx* out_rgba)
{
    ir_u32 argb;
    int rc;
    if (!attr || !out_rgba) {
        return IR_ERR_ARGUMENT;
    }
    if (attr->kind == IR_ATTR_COLORF) {
        return ir_attr_as_vec4(attr, pool, out_rgba);
    }
    rc = ir_attr_as_argb(attr, pool, &argb);
    if (rc != IR_OK) {
        return rc;
    }
    out_rgba->x = ir_attr_u8_to_fx01((unsigned long)((argb >> 16) & 0xffUL));
    out_rgba->y = ir_attr_u8_to_fx01((unsigned long)((argb >> 8) & 0xffUL));
    out_rgba->z = ir_attr_u8_to_fx01((unsigned long)(argb & 0xffUL));
    out_rgba->w = ir_attr_u8_to_fx01((unsigned long)((argb >> 24) & 0xffUL));
    return IR_OK;
}

int ir_attr_as_quaternion(const ir_attr* attr, const ir_strpool* pool, ir_vec4fx* out_xyzw)
{
    if (!attr || !out_xyzw) {
        return IR_ERR_ARGUMENT;
    }
    return ir_attr_as_vec4(attr, pool, out_xyzw);
}

int ir_attr_as_rect_i32(const ir_attr* attr, const ir_strpool* pool, ir_recti* out_rect)
{
    const char* value;
    ir_i32 tmp[4];
    int got;
    int rc;
    if (!attr || !out_rect) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (!value) {
        return IR_ERR_FORMAT;
    }
    tmp[0] = 0; tmp[1] = 0; tmp[2] = 0; tmp[3] = 0;
    rc = ir_attr_parse_i32_list_value(value, tmp, 4, &got);
    if (rc != IR_OK) {
        return rc;
    }
    if (got < 2) {
        return IR_ERR_FORMAT;
    }
    if (got < 3) {
        tmp[2] = tmp[0];
    }
    if (got < 4) {
        tmp[3] = tmp[1];
    }
    out_rect->x0 = tmp[0];
    out_rect->y0 = tmp[1];
    out_rect->x1 = tmp[2];
    out_rect->y1 = tmp[3];
    return IR_OK;
}

static ir_u32 ir_attr_i32_to_u32_nonnegative(ir_i32 v)
{
    if (v < 0) {
        return 0UL;
    }
    return (ir_u32)v;
}

int ir_attr_as_dimension2d_u32(const ir_attr* attr, const ir_strpool* pool, ir_dim2u* out_dim)
{
    const char* value;
    ir_i32 tmp[2];
    int got;
    int rc;
    if (!attr || !out_dim) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (!value) {
        return IR_ERR_FORMAT;
    }
    tmp[0] = 0; tmp[1] = 0;
    rc = ir_attr_parse_i32_list_value(value, tmp, 2, &got);
    if (rc != IR_OK) {
        return rc;
    }
    if (got < 2) {
        return IR_ERR_FORMAT;
    }
    out_dim->width = ir_attr_i32_to_u32_nonnegative(tmp[0]);
    out_dim->height = ir_attr_i32_to_u32_nonnegative(tmp[1]);
    return IR_OK;
}

int ir_attr_as_line2(const ir_attr* attr, const ir_strpool* pool, ir_line2fx* out_line)
{
    const char* value;
    ir_fx16 tmp[4];
    int rc;
    if (!attr || !out_line) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (!value) {
        return IR_ERR_FORMAT;
    }
    rc = ir_attr_parse_fx_exact_value(value, tmp, 4);
    if (rc != IR_OK) {
        return rc;
    }
    out_line->start.x = tmp[0];
    out_line->start.y = tmp[1];
    out_line->end.x = tmp[2];
    out_line->end.y = tmp[3];
    return IR_OK;
}

int ir_attr_as_line3(const ir_attr* attr, const ir_strpool* pool, ir_line3fx* out_line)
{
    const char* value;
    ir_fx16 tmp[6];
    int rc;
    if (!attr || !out_line) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (!value) {
        return IR_ERR_FORMAT;
    }
    rc = ir_attr_parse_fx_exact_value(value, tmp, 6);
    if (rc != IR_OK) {
        return rc;
    }
    out_line->start.x = tmp[0];
    out_line->start.y = tmp[1];
    out_line->start.z = tmp[2];
    out_line->end.x = tmp[3];
    out_line->end.y = tmp[4];
    out_line->end.z = tmp[5];
    return IR_OK;
}

int ir_attr_as_plane(const ir_attr* attr, const ir_strpool* pool, ir_plane3fx* out_plane)
{
    const char* value;
    ir_fx16 tmp[4];
    int rc;
    if (!attr || !out_plane) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (!value) {
        return IR_ERR_FORMAT;
    }
    rc = ir_attr_parse_fx_exact_value(value, tmp, 4);
    if (rc != IR_OK) {
        return rc;
    }
    out_plane->normal.x = tmp[0];
    out_plane->normal.y = tmp[1];
    out_plane->normal.z = tmp[2];
    out_plane->d = tmp[3];
    return IR_OK;
}

int ir_attr_as_triangle3(const ir_attr* attr, const ir_strpool* pool, ir_triangle3fx* out_triangle)
{
    const char* value;
    ir_fx16 tmp[9];
    int rc;
    if (!attr || !out_triangle) {
        return IR_ERR_ARGUMENT;
    }
    value = ir_attr_value_cstr(attr, pool);
    if (!value) {
        return IR_ERR_FORMAT;
    }
    rc = ir_attr_parse_fx_exact_value(value, tmp, 9);
    if (rc != IR_OK) {
        return rc;
    }
    out_triangle->a.x = tmp[0];
    out_triangle->a.y = tmp[1];
    out_triangle->a.z = tmp[2];
    out_triangle->b.x = tmp[3];
    out_triangle->b.y = tmp[4];
    out_triangle->b.z = tmp[5];
    out_triangle->c.x = tmp[6];
    out_triangle->c.y = tmp[7];
    out_triangle->c.z = tmp[8];
    return IR_OK;
}

static int ir_attr_i32_from_irr_string(const char* value, ir_i32* out)
{
    ir_i32 tmp[1];
    int count;
    int rc;
    rc = ir_i32_from_str(value, out);
    if (rc == IR_OK) {
        return IR_OK;
    }
    rc = ir_attr_parse_i32_list_value(value, tmp, 1, &count);
    if (rc == IR_OK && count == 1) {
        *out = tmp[0];
        return IR_OK;
    }
    return rc;
}

static int ir_attr_u8_from_i32(ir_i32 value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (int)value;
}

static int ir_attr_color_from_value(const char* value, ir_u32* out)
{
    int rc;
    ir_i32 rgba[4];
    int count;
    rc = ir_color_argb_from_hex(value, out);
    if (rc == IR_OK) {
        return IR_OK;
    }
    rc = ir_attr_parse_i32_list_value(value, rgba, 4, &count);
    if (rc == IR_OK && count == 4) {
        unsigned long r;
        unsigned long g;
        unsigned long b;
        unsigned long a;
        r = (unsigned long)ir_attr_u8_from_i32(rgba[0]);
        g = (unsigned long)ir_attr_u8_from_i32(rgba[1]);
        b = (unsigned long)ir_attr_u8_from_i32(rgba[2]);
        a = (unsigned long)ir_attr_u8_from_i32(rgba[3]);
        *out = (ir_u32)((a << 24) | (r << 16) | (g << 8) | b);
        return IR_OK;
    }
    return IR_ERR_FORMAT;
}

static int ir_attr_append_char(char* buf, size_t cap, size_t* len, int ch)
{
    if (!buf || !len || *len + 1U >= cap) {
        return IR_ERR_CAPACITY;
    }
    buf[*len] = (char)ch;
    *len += 1U;
    buf[*len] = '\0';
    return IR_OK;
}

static int ir_attr_append_str(char* buf, size_t cap, size_t* len, const char* s)
{
    size_t n;
    if (!s) {
        s = "";
    }
    n = strlen(s);
    if (!buf || !len || *len + n >= cap) {
        return IR_ERR_CAPACITY;
    }
    memcpy(buf + *len, s, n);
    *len += n;
    buf[*len] = '\0';
    return IR_OK;
}

static int ir_attr_append_xml_escaped(char* buf, size_t cap, size_t* len, const char* s)
{
    int rc;
    const unsigned char* p;
    if (!s) {
        s = "";
    }
    p = (const unsigned char*)s;
    while (*p) {
        switch (*p) {
        case '&': rc = ir_attr_append_str(buf, cap, len, "&amp;"); break;
        case '<': rc = ir_attr_append_str(buf, cap, len, "&lt;"); break;
        case '>': rc = ir_attr_append_str(buf, cap, len, "&gt;"); break;
        case '"': rc = ir_attr_append_str(buf, cap, len, "&quot;"); break;
        case '\'': rc = ir_attr_append_str(buf, cap, len, "&apos;"); break;
        default: rc = ir_attr_append_char(buf, cap, len, (int)*p); break;
        }
        if (rc != IR_OK) {
            return rc;
        }
        ++p;
    }
    return IR_OK;
}

static int ir_attr_build_extra_attrs(const ir_xml_tag* tag, char* extra, size_t cap, size_t* out_len)
{
    int i;
    int rc;
    size_t len;
    if (!tag || !extra || !out_len || cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    len = 0U;
    extra[0] = '\0';
    for (i = 0; i < tag->attr_count; ++i) {
        const char* aname;
        const char* avalue;
        aname = ir_xml_local_name(tag->attrs[i].name);
        avalue = tag->attrs[i].value;
        if (ir_xml_name_eq(aname, "name") || ir_xml_name_eq(aname, "value")) {
            continue;
        }
        rc = ir_attr_append_char(extra, cap, &len, ' '); if (rc != IR_OK) return rc;
        rc = ir_attr_append_str(extra, cap, &len, aname); if (rc != IR_OK) return rc;
        rc = ir_attr_append_str(extra, cap, &len, "=\""); if (rc != IR_OK) return rc;
        rc = ir_attr_append_xml_escaped(extra, cap, &len, avalue); if (rc != IR_OK) return rc;
        rc = ir_attr_append_char(extra, cap, &len, '"'); if (rc != IR_OK) return rc;
    }
    *out_len = len;
    return IR_OK;
}

static int ir_attr_value_required(int kind)
{
    if (kind == IR_ATTR_UNKNOWN) return 0;
    if (kind == IR_ATTR_STRINGWARRAY) return 0;
    if (kind == IR_ATTR_USER_POINTER) return 0;
    return 1;
}

int ir_attr_from_xml_tag(const ir_xml_tag* tag, ir_attr* out, ir_strpool* pool, char* err, size_t err_cap)
{
    const char* name;
    const char* value;
    int rc;
    int b;
    char extra[IR_ATTR_EXTRA_XML_CAP];
    size_t extra_len;
    if (!tag || !out || !pool) {
        return IR_ERR_ARGUMENT;
    }
    name = ir_xml_tag_attr_local(tag, "name");
    value = ir_xml_tag_attr_local(tag, "value");
    if (!name) {
        name = "";
    }
    memset(out, 0, sizeof(*out));
    out->kind = (ir_u8)ir_attr_kind_from_tag_name(tag->name);
    if (!value && ir_attr_value_required((int)out->kind)) {
        if (err && err_cap) {
            strcpy(err, "attribute tag requires value");
        }
        return IR_ERR_FORMAT;
    }
    out->line = tag->line;
    out->col = tag->col;
    out->raw_tag.begin = tag->raw_begin;
    out->raw_tag.end = tag->raw_end;
    rc = ir_attr_build_extra_attrs(tag, extra, sizeof(extra), &extra_len);
    if (rc != IR_OK) {
        if (err && err_cap) {
            strcpy(err, "attribute tag has too many auxiliary XML attributes");
        }
        return rc;
    }
    rc = ir_strpool_put(pool, tag->name, strlen(tag->name), &out->tag_off, &out->tag_len);
    if (rc != IR_OK) return rc;
    rc = ir_strpool_put(pool, name, strlen(name), &out->name_off, &out->name_len);
    if (rc != IR_OK) return rc;
    if (value) {
        rc = ir_strpool_put(pool, value, strlen(value), &out->value_off, &out->value_len);
        if (rc != IR_OK) return rc;
        out->has_raw_value = 1U;
    }
    if (extra_len > 0U) {
        rc = ir_strpool_put(pool, extra, extra_len, &out->extra_attrs_off, &out->extra_attrs_len);
        if (rc != IR_OK) return rc;
        out->has_extra_attrs = 1U;
    }
    rc = IR_OK;
    if (value) {
        switch (out->kind) {
        case IR_ATTR_INT:
            rc = ir_attr_i32_from_irr_string(value, &out->v.i);
            break;
        case IR_ATTR_FLOAT:
            rc = ir_fx_from_str(value, &out->v.fx);
            if (rc != IR_OK) {
                int parsed_count;
                rc = ir_attr_parse_fx_list_value(value, &out->v.fx, 1, &parsed_count);
                if (rc == IR_OK && parsed_count != 1) {
                    rc = IR_ERR_FORMAT;
                }
            }
            break;
        case IR_ATTR_BOOL:
            rc = ir_attr_bool_from_value(value, &b);
            if (rc != IR_OK) {
                b = 0;
                rc = IR_OK;
            }
            out->v.boolean = (ir_u8)(b ? 1 : 0);
            break;
        case IR_ATTR_VECTOR2D:
        case IR_ATTR_POSITION2D:
        case IR_ATTR_DIMENSION2D:
            rc = ir_attr_vec2_from_value(value, &out->v.vec2);
            break;
        case IR_ATTR_VECTOR3D:
            rc = ir_attr_vec3_from_value(value, &out->v.vec3);
            break;
        case IR_ATTR_COLOR:
            rc = ir_attr_color_from_value(value, &out->v.argb);
            break;
        case IR_ATTR_COLORF:
        case IR_ATTR_QUATERNION:
            rc = ir_attr_vec4_from_value(value, &out->v.vec4);
            break;
        case IR_ATTR_STRING:
        case IR_ATTR_ENUM:
        case IR_ATTR_TEXTURE:
        case IR_ATTR_BINARY:
        case IR_ATTR_RECT:
        case IR_ATTR_MATRIX:
        case IR_ATTR_BOX3D:
        case IR_ATTR_PLANE:
        case IR_ATTR_TRIANGLE3D:
        case IR_ATTR_LINE2D:
        case IR_ATTR_LINE3D:
        case IR_ATTR_STRINGWARRAY:
        case IR_ATTR_USER_POINTER:
        case IR_ATTR_FLOATARRAY:
        case IR_ATTR_INTARRAY:
        case IR_ATTR_UNKNOWN:
        default:
            rc = IR_OK;
            break;
        }
    }
    if (rc != IR_OK) {
        rc = IR_OK;
    }
    return rc;
}

static int ir_attr_format_vec2(const ir_vec2fx* vec, char* buf, size_t cap)
{
    char xbuf[32];
    char ybuf[32];
    int rc;
    rc = ir_fx_to_str(vec->x, xbuf, sizeof(xbuf)); if (rc != IR_OK) return rc;
    rc = ir_fx_to_str(vec->y, ybuf, sizeof(ybuf)); if (rc != IR_OK) return rc;
    if (strlen(xbuf) + strlen(ybuf) + 3U > cap) return IR_ERR_CAPACITY;
    strcpy(buf, xbuf); strcat(buf, ", "); strcat(buf, ybuf);
    return IR_OK;
}

static int ir_attr_format_value(const ir_attr* attr, char* buf, size_t cap)
{
    int rc;
    char xbuf[32];
    char ybuf[32];
    char zbuf[32];
    char wbuf[32];
    const char* v;
    if (!attr || !buf || cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    if (attr->has_raw_value) {
        return IR_ERR_UNSUPPORTED;
    }
    switch (attr->kind) {
    case IR_ATTR_INT:
        return ir_i32_to_str(attr->v.i, buf, cap);
    case IR_ATTR_FLOAT:
        return ir_fx_to_str(attr->v.fx, buf, cap);
    case IR_ATTR_BOOL:
        v = attr->v.boolean ? "true" : "false";
        if (strlen(v) + 1U > cap) return IR_ERR_CAPACITY;
        strcpy(buf, v);
        return IR_OK;
    case IR_ATTR_VECTOR2D:
    case IR_ATTR_POSITION2D:
    case IR_ATTR_DIMENSION2D:
        return ir_attr_format_vec2(&attr->v.vec2, buf, cap);
    case IR_ATTR_VECTOR3D:
        rc = ir_fx_to_str(attr->v.vec3.x, xbuf, sizeof(xbuf)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(attr->v.vec3.y, ybuf, sizeof(ybuf)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(attr->v.vec3.z, zbuf, sizeof(zbuf)); if (rc != IR_OK) return rc;
        if (strlen(xbuf) + strlen(ybuf) + strlen(zbuf) + 5U > cap) return IR_ERR_CAPACITY;
        strcpy(buf, xbuf); strcat(buf, ", "); strcat(buf, ybuf); strcat(buf, ", "); strcat(buf, zbuf);
        return IR_OK;
    case IR_ATTR_COLOR:
        if (cap < 9U) return IR_ERR_CAPACITY;
        sprintf(buf, "%08lx", (unsigned long)attr->v.argb);
        return IR_OK;
    case IR_ATTR_COLORF:
    case IR_ATTR_QUATERNION:
        rc = ir_fx_to_str(attr->v.vec4.x, xbuf, sizeof(xbuf)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(attr->v.vec4.y, ybuf, sizeof(ybuf)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(attr->v.vec4.z, zbuf, sizeof(zbuf)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(attr->v.vec4.w, wbuf, sizeof(wbuf)); if (rc != IR_OK) return rc;
        if (strlen(xbuf) + strlen(ybuf) + strlen(zbuf) + strlen(wbuf) + 7U > cap) return IR_ERR_CAPACITY;
        strcpy(buf, xbuf); strcat(buf, ", "); strcat(buf, ybuf); strcat(buf, ", "); strcat(buf, zbuf); strcat(buf, ", "); strcat(buf, wbuf);
        return IR_OK;
    default:
        return IR_ERR_UNSUPPORTED;
    }
}

int ir_attr_write_xml(const ir_attr* attr, const ir_strpool* pool, ir_outbuf* out, size_t indent, char* err, size_t err_cap)
{
    const char* tag;
    const char* name;
    const char* value;
    const char* extra;
    char fmtbuf[256];
    int rc;
    if (!attr || !pool || !out) {
        return IR_ERR_ARGUMENT;
    }
    tag = ir_xml_local_name(ir_strpool_get(pool, attr->tag_off));
    if (!tag || !*tag) {
        tag = ir_attr_kind_name((int)attr->kind);
    }
    name = ir_strpool_get(pool, attr->name_off);
    value = IR_NULL;
    if (attr->has_raw_value) {
        value = ir_strpool_get(pool, attr->value_off);
    }
    if (!value && ir_attr_value_required((int)attr->kind)) {
        rc = ir_attr_format_value(attr, fmtbuf, sizeof(fmtbuf));
        if (rc != IR_OK) {
            if (err && err_cap) {
                strcpy(err, "unable to serialize attribute value");
            }
            return rc;
        }
        value = fmtbuf;
    }
    extra = "";
    if (attr->has_extra_attrs) {
        extra = ir_strpool_get(pool, attr->extra_attrs_off);
    }
    rc = ir_outbuf_pad(out, ' ', indent);
    if (rc != IR_OK) return rc;
    rc = ir_outbuf_putc(out, '<'); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, tag); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, " name=\""); if (rc != IR_OK) return rc;
    rc = ir_xml_escape_to_out(out, name); if (rc != IR_OK) return rc;
    rc = ir_outbuf_putc(out, '"'); if (rc != IR_OK) return rc;
    if (value) {
        rc = ir_outbuf_puts(out, " value=\""); if (rc != IR_OK) return rc;
        rc = ir_xml_escape_to_out(out, value); if (rc != IR_OK) return rc;
        rc = ir_outbuf_putc(out, '"'); if (rc != IR_OK) return rc;
    }
    if (extra && *extra) {
        rc = ir_outbuf_puts(out, extra); if (rc != IR_OK) return rc;
    }
    rc = ir_outbuf_puts(out, " />\n"); if (rc != IR_OK) return rc;
    (void)err;
    (void)err_cap;
    return IR_OK;
}
