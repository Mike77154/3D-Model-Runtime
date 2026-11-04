#include "usd3d_usdc_deser.h"

#include <string.h>
#include <ctype.h>

static void ud_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static int ud_is_print(int c)
{
    return c >= 32 && c <= 126;
}

static unsigned long ud_read_le32_raw(const unsigned char *data, unsigned long size, unsigned long offset, int *ok)
{
    unsigned long v;
    if (ok != 0) *ok = 0;
    if (data == 0 || offset + 4UL > size) return 0UL;
    v = ((unsigned long)data[offset]) | (((unsigned long)data[offset + 1UL]) << 8) | (((unsigned long)data[offset + 2UL]) << 16) | (((unsigned long)data[offset + 3UL]) << 24);
    if (ok != 0) *ok = 1;
    return v;
}

static int ud_parse_long(const char *s, long *out)
{
    long sign;
    long v;
    if (s == 0 || out == 0) return 0;
    sign = 1L;
    if (*s == '-') { sign = -1L; s++; }
    if (!isdigit((unsigned char)*s)) return 0;
    v = 0L;
    while (isdigit((unsigned char)*s)) {
        v = (v * 10L) + (long)(*s - '0');
        s++;
    }
    *out = v * sign;
    return 1;
}

static int ud_looks_path(const char *s)
{
    if (s == 0 || s[0] == '\0') return 0;
    if (s[0] == '/') return 1;
    if (strstr(s, "/") != 0 && (strstr(s, ".usd") != 0 || strstr(s, ".png") != 0 || strstr(s, ".vdb") != 0)) return 1;
    return 0;
}

static int ud_looks_asset(const char *s)
{
    if (s == 0) return 0;
    if (s[0] == '@') return 1;
    if (strstr(s, ".usd") != 0 || strstr(s, ".usda") != 0 || strstr(s, ".usdc") != 0 || strstr(s, ".usdz") != 0) return 1;
    if (strstr(s, ".png") != 0 || strstr(s, ".jpg") != 0 || strstr(s, ".jpeg") != 0 || strstr(s, ".exr") != 0) return 1;
    if (strstr(s, ".mtlx") != 0 || strstr(s, ".mdl") != 0 || strstr(s, ".vdb") != 0 || strstr(s, ".f3d") != 0) return 1;
    return 0;
}

static int ud_looks_typename(const char *s)
{
    if (s == 0) return 0;
    if (strcmp(s, "Mesh") == 0 || strcmp(s, "Xform") == 0 || strcmp(s, "Scope") == 0) return 1;
    if (strcmp(s, "Material") == 0 || strcmp(s, "Shader") == 0 || strcmp(s, "Skeleton") == 0) return 1;
    if (strcmp(s, "SkelRoot") == 0 || strcmp(s, "SkelAnimation") == 0 || strcmp(s, "Camera") == 0) return 1;
    if (strstr(s, "Light") != 0 || strstr(s, "Volume") != 0) return 1;
    return 0;
}

static int ud_classify_text(const char *s)
{
    long dummy;
    if (s == 0 || s[0] == '\0') return USD3D_USDC_VALUE_UNKNOWN;
    if (strcmp(s, "true") == 0 || strcmp(s, "false") == 0) return USD3D_USDC_VALUE_BOOL;
    if (strcmp(s, "def") == 0 || strcmp(s, "over") == 0 || strcmp(s, "class") == 0) return USD3D_USDC_VALUE_SPECIFIER;
    if (ud_looks_asset(s)) return USD3D_USDC_VALUE_ASSET;
    if (ud_looks_path(s)) return USD3D_USDC_VALUE_PATH;
    if (ud_looks_typename(s)) return USD3D_USDC_VALUE_TYPENAME;
    if (ud_parse_long(s, &dummy)) return USD3D_USDC_VALUE_INT;
    return USD3D_USDC_VALUE_TOKEN;
}

const char *usd3d_usdc_value_type_name(int type)
{
    switch (type) {
    case USD3D_USDC_VALUE_TOKEN: return "token";
    case USD3D_USDC_VALUE_STRING: return "string";
    case USD3D_USDC_VALUE_PATH: return "path";
    case USD3D_USDC_VALUE_ASSET: return "asset";
    case USD3D_USDC_VALUE_INT: return "int";
    case USD3D_USDC_VALUE_UINT: return "uint";
    case USD3D_USDC_VALUE_FIXED: return "fixed";
    case USD3D_USDC_VALUE_BOOL: return "bool";
    case USD3D_USDC_VALUE_SPECIFIER: return "specifier";
    case USD3D_USDC_VALUE_TYPENAME: return "typename";
    default: return "unknown";
    }
}

const char *usd3d_usdc_field_kind_name(int kind)
{
    switch (kind) {
    case USD3D_USDC_FIELD_ATTR: return "attr";
    case USD3D_USDC_FIELD_REL: return "rel";
    case USD3D_USDC_FIELD_METADATA: return "metadata";
    case USD3D_USDC_FIELD_ARC: return "arc";
    default: return "unknown";
    }
}

void usd3d_usdc_deser_init(usd3d_usdc_deser *d)
{
    if (d == 0) return;
    memset(d, 0, sizeof(*d));
}

int usd3d_usdc_deser_add_value(usd3d_usdc_deser *d, int type, const char *text, unsigned long offset, unsigned long size)
{
    usd3d_usdc_value *v;
    long lv;
    usd3d_fixed fx;
    if (d == 0) return USD3D_ERR_BAD_ARG;
    if (d->value_count >= (unsigned long)USD3D_USDC_DESER_MAX_VALUES) return USD3D_ERR_LIMIT;
    v = &d->values[d->value_count];
    memset(v, 0, sizeof(*v));
    v->index = d->value_count;
    v->type = type;
    v->offset = offset;
    v->size = size;
    ud_copy(v->text, sizeof(v->text), text);
    if (type == USD3D_USDC_VALUE_INT && ud_parse_long(v->text, &lv)) v->i32_value = lv;
    if (type == USD3D_USDC_VALUE_UINT && ud_parse_long(v->text, &lv)) v->u32_value = (unsigned long)lv;
    if (type == USD3D_USDC_VALUE_FIXED && usd3d_parse_fixed_text(v->text, &fx) == USD3D_OK) v->fixed_value = fx;
    if (type == USD3D_USDC_VALUE_PATH) d->path_count++;
    if (type == USD3D_USDC_VALUE_ASSET) d->asset_count++;
    if (type == USD3D_USDC_VALUE_INT || type == USD3D_USDC_VALUE_UINT || type == USD3D_USDC_VALUE_FIXED || type == USD3D_USDC_VALUE_BOOL) d->numeric_count++;
    d->value_count++;
    return USD3D_OK;
}

int usd3d_usdc_deser_add_field(usd3d_usdc_deser *d, int kind, const char *path, const char *name, const char *value_text, const char *source_table)
{
    usd3d_usdc_field *f;
    if (d == 0) return USD3D_ERR_BAD_ARG;
    if (d->field_count >= (unsigned long)USD3D_USDC_DESER_MAX_FIELDS) return USD3D_ERR_LIMIT;
    f = &d->fields[d->field_count];
    memset(f, 0, sizeof(*f));
    f->index = d->field_count;
    f->kind = kind;
    ud_copy(f->path, sizeof(f->path), path);
    ud_copy(f->name, sizeof(f->name), name);
    ud_copy(f->value_text, sizeof(f->value_text), value_text);
    ud_copy(f->source_table, sizeof(f->source_table), source_table);
    f->path_value = 0UL;
    f->name_value = 0UL;
    f->value_index = 0UL;
    d->field_count++;
    return USD3D_OK;
}

static void ud_maybe_field(usd3d_usdc_deser *d, const char *text)
{
    char left[USD3D_TOKEN_MAX];
    char right[USD3D_USDC_DESER_TEXT_MAX];
    char path[USD3D_PATH_MAX];
    char name[USD3D_TOKEN_MAX];
    const char *eq;
    const char *dot;
    unsigned int i;
    int kind;
    if (d == 0 || text == 0) return;
    eq = strchr(text, '=');
    if (eq == 0) eq = strchr(text, ':');
    if (eq == 0) return;
    i = 0U;
    while (text + i < eq && i + 1U < sizeof(left)) {
        left[i] = text[i];
        i++;
    }
    left[i] = '\0';
    ud_copy(right, sizeof(right), eq + 1);
    dot = strrchr(left, '.');
    if (dot != 0) {
        i = 0U;
        while (left + i < dot && i + 1U < sizeof(path)) { path[i] = left[i]; i++; }
        path[i] = '\0';
        ud_copy(name, sizeof(name), dot + 1);
    } else {
        ud_copy(path, sizeof(path), "");
        ud_copy(name, sizeof(name), left);
    }
    kind = USD3D_USDC_FIELD_ATTR;
    if (strstr(name, "rel") != 0 || strstr(name, "relationship") != 0) kind = USD3D_USDC_FIELD_REL;
    if (strstr(name, "variant") != 0 || strstr(name, "metadata") != 0 || strstr(name, "specifier") != 0) kind = USD3D_USDC_FIELD_METADATA;
    if (strstr(name, "reference") != 0 || strstr(name, "payload") != 0 || strstr(name, "inherit") != 0 || strstr(name, "specialize") != 0) kind = USD3D_USDC_FIELD_ARC;
    usd3d_usdc_deser_add_field(d, kind, path, name, right, "printable-field");
    d->field_like_count++;
}

static void ud_scan_printable(usd3d_usdc_deser *d, const unsigned char *data, unsigned long size)
{
    unsigned long i;
    unsigned long start;
    unsigned int n;
    char buf[USD3D_USDC_DESER_TEXT_MAX];
    int type;
    i = 0UL;
    while (i < size) {
        if (!ud_is_print((int)data[i])) { i++; continue; }
        start = i;
        n = 0U;
        while (i < size && ud_is_print((int)data[i])) {
            if (n + 1U < sizeof(buf)) buf[n++] = (char)data[i];
            i++;
        }
        buf[n] = '\0';
        if ((i - start) >= 2UL && n > 0U) {
            type = ud_classify_text(buf);
            usd3d_usdc_deser_add_value(d, type, buf, start, i - start);
            ud_maybe_field(d, buf);
        }
    }
}

static void ud_scan_numeric_atoms(usd3d_usdc_deser *d, const unsigned char *data, unsigned long size)
{
    unsigned long i;
    unsigned long v;
    int ok;
    char buf[64];
    if (size < 16UL) return;
    for (i = 8UL; i + 4UL <= size && d->numeric_count < 64UL; i += 4UL) {
        v = ud_read_le32_raw(data, size, i, &ok);
        if (!ok) break;
        if (v < 1000000UL && v != 0UL) {
            /* Avoid snprintf for strict C89 portability. */
            buf[0] = '\0';
            {
                char tmp[32];
                unsigned int k;
                unsigned int j;
                unsigned long t;
                t = v;
                k = 0U;
                do { tmp[k++] = (char)('0' + (t % 10UL)); t /= 10UL; } while (t != 0UL && k < sizeof(tmp));
                j = 0U;
                while (k > 0U && j + 1U < sizeof(buf)) buf[j++] = tmp[--k];
                buf[j] = '\0';
            }
            usd3d_usdc_deser_add_value(d, USD3D_USDC_VALUE_UINT, buf, i, 4UL);
        }
    }
}

int usd3d_usdc_deser_scan_memory(const unsigned char *data, unsigned long size, usd3d_usdc_deser *d)
{
    if (data == 0 || d == 0) return USD3D_ERR_BAD_ARG;
    usd3d_usdc_deser_init(d);
    if (size >= 8UL && memcmp(data, "PXR-USDC", 8) == 0) {
        d->has_magic = 1;
        d->status = USD3D_USDC_STATUS_PARTIAL;
        if (size >= 16UL) {
            d->version_major = (unsigned long)data[8];
            d->version_minor = (unsigned long)data[9];
        }
    }
    ud_scan_printable(d, data, size);
    ud_scan_numeric_atoms(d, data, size);
    if (d->has_magic && d->value_count > 0UL) d->status = USD3D_USDC_STATUS_COMPLETE_LITE;
    d->unsupported_bytes = size;
    if (d->value_count < size) d->unsupported_bytes = size - d->value_count;
    return USD3D_OK;
}

int usd3d_usdc_deser_read_atom_le(const unsigned char *data, unsigned long size, unsigned long offset, int type, usd3d_usdc_value *out_value)
{
    unsigned long u;
    int ok;
    if (data == 0 || out_value == 0) return USD3D_ERR_BAD_ARG;
    memset(out_value, 0, sizeof(*out_value));
    out_value->type = type;
    out_value->offset = offset;
    if (type == USD3D_USDC_VALUE_UINT || type == USD3D_USDC_VALUE_INT || type == USD3D_USDC_VALUE_FIXED || type == USD3D_USDC_VALUE_BOOL) {
        u = ud_read_le32_raw(data, size, offset, &ok);
        if (!ok) return USD3D_ERR_LIMIT;
        out_value->u32_value = u;
        out_value->i32_value = (long)u;
        out_value->fixed_value = (usd3d_fixed)u;
        out_value->size = 4UL;
        return USD3D_OK;
    }
    return USD3D_ERR_UNSUPPORTED;
}

int usd3d_usdc_deser_emit(const usd3d_usdc_deser *d,
                          int (*value_cb)(void *user, const usd3d_usdc_value *v),
                          int (*field_cb)(void *user, const usd3d_usdc_field *f),
                          void *user)
{
    unsigned long i;
    if (d == 0) return USD3D_ERR_BAD_ARG;
    if (value_cb != 0) {
        for (i = 0UL; i < d->value_count; ++i) if (value_cb(user, &d->values[i]) != 0) return USD3D_ERR_PARSE;
    }
    if (field_cb != 0) {
        for (i = 0UL; i < d->field_count; ++i) if (field_cb(user, &d->fields[i]) != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}
