#include "usd3d_usdc_abstract.h"
#include <string.h>

static void ua_copy(char *dst, unsigned long cap, const char *src)
{
    unsigned long i;
    if (!dst || cap == 0UL) return;
    if (!src) src = "";
    i = 0UL;
    while (src[i] && i + 1UL < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int ua_streq(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b) == 0;
}

static int ua_append(char *out, unsigned long cap, unsigned long *pos, const char *text)
{
    unsigned long i;
    if (!out || !pos || cap == 0UL) return USD3D_ERR_BAD_ARG;
    if (!text) text = "";
    i = 0UL;
    while (text[i]) {
        if (*pos + 1UL >= cap) return USD3D_ERR_LIMIT;
        out[*pos] = text[i];
        (*pos)++;
        i++;
    }
    out[*pos] = '\0';
    return USD3D_OK;
}

void usd3d_usdc_abstract_init(usd3d_usdc_abstract_data *a)
{
    if (!a) return;
    memset(a, 0, sizeof(*a));
    a->status = USD3D_USDC_ABS_STATUS_EMPTY;
}

const char *usd3d_usdc_abs_spec_kind_name(int kind)
{
    switch (kind) {
    case USD3D_USDC_SPEC_DEF: return "def";
    case USD3D_USDC_SPEC_OVER: return "over";
    case USD3D_USDC_SPEC_CLASS: return "class";
    case USD3D_USDC_SPEC_ATTR: return "attr";
    case USD3D_USDC_SPEC_REL: return "rel";
    default: return "unknown";
    }
}

int usd3d_usdc_abstract_add_spec(usd3d_usdc_abstract_data *a, const char *path, const char *specifier, const char *type_name, int spec_kind)
{
    usd3d_usdc_abs_spec *s;
    if (!a || !path) return USD3D_ERR_BAD_ARG;
    if (a->spec_count >= USD3D_USDC_ABS_MAX_SPECS) return USD3D_ERR_LIMIT;
    s = &a->specs[a->spec_count];
    memset(s, 0, sizeof(*s));
    ua_copy(s->path, USD3D_PATH_MAX, path);
    ua_copy(s->specifier, USD3D_MAX_NAME, specifier ? specifier : usd3d_usdc_abs_spec_kind_name(spec_kind));
    ua_copy(s->type_name, USD3D_TOKEN_MAX, type_name ? type_name : "");
    s->spec_kind = spec_kind;
    s->field_begin = a->field_count;
    s->field_count = 0UL;
    s->active = 1;
    a->spec_count++;
    if (a->status < USD3D_USDC_ABS_STATUS_PARTIAL) a->status = USD3D_USDC_ABS_STATUS_PARTIAL;
    return USD3D_OK;
}

int usd3d_usdc_abstract_add_field(usd3d_usdc_abstract_data *a, const char *path, const char *name, const char *value, const char *source_table, int kind)
{
    usd3d_usdc_abs_field *f;
    unsigned long i;
    if (!a || !path || !name) return USD3D_ERR_BAD_ARG;
    if (a->field_count >= USD3D_USDC_ABS_MAX_FIELDS) return USD3D_ERR_LIMIT;
    f = &a->fields[a->field_count];
    memset(f, 0, sizeof(*f));
    ua_copy(f->path, USD3D_PATH_MAX, path);
    ua_copy(f->name, USD3D_TOKEN_MAX, name);
    ua_copy(f->value, USD3D_USDC_DESER_TEXT_MAX, value ? value : "");
    ua_copy(f->source_table, USD3D_MAX_NAME, source_table ? source_table : "FIELDS");
    f->kind = kind;
    f->active = 1;
    if (kind == USD3D_USDC_FIELD_ATTR) a->attr_count++;
    else if (kind == USD3D_USDC_FIELD_REL) a->rel_count++;
    else if (kind == USD3D_USDC_FIELD_METADATA) a->metadata_count++;
    for (i = 0UL; i < a->spec_count; i++) {
        if (a->specs[i].active && ua_streq(a->specs[i].path, path)) {
            if (a->specs[i].field_count == 0UL) a->specs[i].field_begin = a->field_count;
            a->specs[i].field_count++;
            break;
        }
    }
    a->field_count++;
    return USD3D_OK;
}

int usd3d_usdc_abstract_import_deser(usd3d_usdc_abstract_data *a, const usd3d_usdc_deser *d)
{
    unsigned long i;
    int found;
    const usd3d_usdc_field *f;
    if (!a || !d) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < d->field_count; i++) {
        f = &d->fields[i];
        found = 0;
        if (f->path[0]) {
            unsigned long j;
            for (j = 0UL; j < a->spec_count; j++) {
                if (a->specs[j].active && ua_streq(a->specs[j].path, f->path)) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                (void)usd3d_usdc_abstract_add_spec(a, f->path, "def", "", USD3D_USDC_SPEC_DEF);
            }
            (void)usd3d_usdc_abstract_add_field(a, f->path, f->name, f->value_text, f->source_table, f->kind);
        }
    }
    if (a->field_count || a->spec_count) a->status = USD3D_USDC_ABS_STATUS_SCHEMA_LITE;
    return USD3D_OK;
}

int usd3d_usdc_abstract_resolve_field(const usd3d_usdc_abstract_data *a, const char *path, const char *name, usd3d_usdc_abs_field *out_field)
{
    long i;
    if (!a || !path || !name || !out_field) return USD3D_ERR_BAD_ARG;
    i = (long)a->field_count - 1L;
    while (i >= 0L) {
        if (a->fields[i].active && ua_streq(a->fields[i].path, path) && ua_streq(a->fields[i].name, name)) {
            *out_field = a->fields[i];
            return USD3D_OK;
        }
        i--;
    }
    return USD3D_ERR_PARSE;
}

int usd3d_usdc_abstract_emit_usda_lite(const usd3d_usdc_abstract_data *a, char *out_text, unsigned long out_cap)
{
    unsigned long pos;
    unsigned long i;
    unsigned long j;
    char line[USD3D_PATH_MAX + USD3D_TOKEN_MAX + USD3D_USDC_DESER_TEXT_MAX + 64];
    if (!a || !out_text || out_cap == 0UL) return USD3D_ERR_BAD_ARG;
    pos = 0UL;
    out_text[0] = '\0';
    if (ua_append(out_text, out_cap, &pos, "#usda 1.0\n(\n    customLayerData = { string usd3d = \"usdc abstract lite\" }\n)\n\n") != USD3D_OK) return USD3D_ERR_LIMIT;
    for (i = 0UL; i < a->spec_count; i++) {
        if (!a->specs[i].active) continue;
        line[0] = '\0';
        strcat(line, usd3d_usdc_abs_spec_kind_name(a->specs[i].spec_kind));
        strcat(line, " \"");
        strcat(line, a->specs[i].path);
        strcat(line, "\"\n{\n");
        if (ua_append(out_text, out_cap, &pos, line) != USD3D_OK) return USD3D_ERR_LIMIT;
        for (j = 0UL; j < a->field_count; j++) {
            if (a->fields[j].active && ua_streq(a->fields[j].path, a->specs[i].path)) {
                line[0] = '\0';
                strcat(line, "    custom string ");
                strcat(line, a->fields[j].name);
                strcat(line, " = \"");
                strcat(line, a->fields[j].value);
                strcat(line, "\"\n");
                if (ua_append(out_text, out_cap, &pos, line) != USD3D_OK) return USD3D_ERR_LIMIT;
            }
        }
        if (ua_append(out_text, out_cap, &pos, "}\n\n") != USD3D_OK) return USD3D_ERR_LIMIT;
    }
    return USD3D_OK;
}

int usd3d_usdc_abstract_emit(const usd3d_usdc_abstract_data *a,
                             int (*spec_cb)(void *user, const usd3d_usdc_abs_spec *s),
                             int (*field_cb)(void *user, const usd3d_usdc_abs_field *f),
                             void *user)
{
    unsigned long i;
    int r;
    if (!a) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < a->spec_count; i++) {
        if (a->specs[i].active && spec_cb) {
            r = spec_cb(user, &a->specs[i]);
            if (r != 0) return r;
        }
    }
    for (i = 0UL; i < a->field_count; i++) {
        if (a->fields[i].active && field_cb) {
            r = field_cb(user, &a->fields[i]);
            if (r != 0) return r;
        }
    }
    return USD3D_OK;
}
