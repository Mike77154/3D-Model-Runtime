#include <string.h>
#include "usd3d_usdc_schema.h"

static void u3s_copy(char *dst, unsigned long cap, const char *src)
{
    unsigned long i;
    if (!dst || cap == 0UL) return;
    if (!src) src = "";
    i = 0UL;
    while (i + 1UL < cap && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int u3s_contains(const char *s, const char *pat)
{
    if (!s || !pat) return 0;
    return strstr(s, pat) != 0;
}

static int u3s_starts(const char *s, const char *pre)
{
    unsigned long i;
    if (!s || !pre) return 0;
    i = 0UL;
    while (pre[i] != '\0') {
        if (s[i] != pre[i]) return 0;
        i++;
    }
    return 1;
}

static unsigned long u3s_append(char *out, unsigned long cap, unsigned long pos, const char *text)
{
    unsigned long i;
    if (!out || cap == 0UL) return 0UL;
    if (!text) text = "";
    i = 0UL;
    while (pos + 1UL < cap && text[i] != '\0') {
        out[pos] = text[i];
        pos++;
        i++;
    }
    out[pos] = '\0';
    return pos;
}

void usd3d_usdc_schema_init(usd3d_usdc_schema *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

const char *usd3d_usdc_schema_role_name(int role)
{
    switch (role) {
    case USD3D_USDC_SCHEMA_ROLE_PRIM: return "prim";
    case USD3D_USDC_SCHEMA_ROLE_ATTRIBUTE: return "attribute";
    case USD3D_USDC_SCHEMA_ROLE_RELATIONSHIP: return "relationship";
    case USD3D_USDC_SCHEMA_ROLE_METADATA: return "metadata";
    case USD3D_USDC_SCHEMA_ROLE_COMPOSITION: return "composition";
    default: return "unknown";
    }
}

const char *usd3d_usdc_schema_hint_name(int hint)
{
    switch (hint) {
    case USD3D_USDC_SCHEMA_HINT_TOKEN: return "token";
    case USD3D_USDC_SCHEMA_HINT_STRING: return "string";
    case USD3D_USDC_SCHEMA_HINT_ASSET: return "asset";
    case USD3D_USDC_SCHEMA_HINT_PATH: return "path";
    case USD3D_USDC_SCHEMA_HINT_FIXED: return "fixed";
    case USD3D_USDC_SCHEMA_HINT_VEC3: return "vec3";
    case USD3D_USDC_SCHEMA_HINT_MATRIX4: return "matrix4";
    case USD3D_USDC_SCHEMA_HINT_INT_ARRAY: return "int_array";
    case USD3D_USDC_SCHEMA_HINT_FLOAT_ARRAY: return "fixed_array";
    default: return "unknown";
    }
}

const char *usd3d_usdc_schema_severity_name(int sev)
{
    switch (sev) {
    case USD3D_USDC_SCHEMA_SEV_INFO: return "info";
    case USD3D_USDC_SCHEMA_SEV_WARN: return "warn";
    case USD3D_USDC_SCHEMA_SEV_ERROR: return "error";
    default: return "unknown";
    }
}

int usd3d_usdc_schema_add_type(usd3d_usdc_schema *s, const char *path, const char *specifier, const char *type_name, int role)
{
    usd3d_usdc_schema_type *t;
    if (!s || !path) return USD3D_ERR_BAD_ARG;
    if (s->type_count >= USD3D_USDC_SCHEMA_MAX_TYPES) return USD3D_ERR_LIMIT;
    t = &s->types[s->type_count];
    memset(t, 0, sizeof(*t));
    u3s_copy(t->path, USD3D_PATH_MAX, path);
    u3s_copy(t->specifier, USD3D_MAX_NAME, specifier ? specifier : "def");
    u3s_copy(t->type_name, USD3D_TOKEN_MAX, type_name ? type_name : "Scope");
    t->role = role;
    t->active = 1;
    s->type_count++;
    return USD3D_OK;
}

int usd3d_usdc_schema_add_diag(usd3d_usdc_schema *s, const char *path, const char *field, const char *message, int severity)
{
    usd3d_usdc_schema_diag *d;
    if (!s) return USD3D_ERR_BAD_ARG;
    if (s->diag_count >= USD3D_USDC_SCHEMA_MAX_DIAGS) return USD3D_ERR_LIMIT;
    d = &s->diags[s->diag_count];
    memset(d, 0, sizeof(*d));
    u3s_copy(d->path, USD3D_PATH_MAX, path ? path : "");
    u3s_copy(d->field, USD3D_TOKEN_MAX, field ? field : "");
    u3s_copy(d->message, USD3D_TOKEN_MAX, message ? message : "");
    d->severity = severity;
    d->active = 1;
    s->diag_count++;
    return USD3D_OK;
}

int usd3d_usdc_schema_classify_field(const usd3d_usdc_abs_field *f, int *out_role, int *out_hint)
{
    int role;
    int hint;
    if (!f) return USD3D_ERR_BAD_ARG;
    role = USD3D_USDC_SCHEMA_ROLE_ATTRIBUTE;
    hint = USD3D_USDC_SCHEMA_HINT_UNKNOWN;

    if (u3s_starts(f->name, "rel ") || u3s_contains(f->name, ":binding") || u3s_contains(f->name, ".connect")) role = USD3D_USDC_SCHEMA_ROLE_RELATIONSHIP;
    if (u3s_contains(f->name, "references") || u3s_contains(f->name, "payload") || u3s_contains(f->name, "inherits") || u3s_contains(f->name, "specializes") || u3s_contains(f->name, "variantSet")) role = USD3D_USDC_SCHEMA_ROLE_COMPOSITION;
    if (u3s_contains(f->name, "kind") || u3s_contains(f->name, "apiSchemas") || u3s_contains(f->name, "specifier")) role = USD3D_USDC_SCHEMA_ROLE_METADATA;

    if (u3s_contains(f->value, "@")) hint = USD3D_USDC_SCHEMA_HINT_ASSET;
    else if (u3s_contains(f->value, "</")) hint = USD3D_USDC_SCHEMA_HINT_PATH;
    else if (u3s_contains(f->value, "matrix4")) hint = USD3D_USDC_SCHEMA_HINT_MATRIX4;
    else if (u3s_contains(f->name, "points") || u3s_contains(f->name, "color") || u3s_contains(f->name, "translate")) hint = USD3D_USDC_SCHEMA_HINT_VEC3;
    else if (u3s_contains(f->name, "indices") || u3s_contains(f->name, "counts")) hint = USD3D_USDC_SCHEMA_HINT_INT_ARRAY;
    else if (u3s_contains(f->value, "\"")) hint = USD3D_USDC_SCHEMA_HINT_STRING;
    else hint = USD3D_USDC_SCHEMA_HINT_TOKEN;

    if (out_role) *out_role = role;
    if (out_hint) *out_hint = hint;
    return USD3D_OK;
}

int usd3d_usdc_schema_import_abstract(usd3d_usdc_schema *s, const usd3d_usdc_abstract_data *a)
{
    unsigned long i;
    int r;
    int role;
    int hint;
    if (!s || !a) return USD3D_ERR_BAD_ARG;
    usd3d_usdc_schema_init(s);
    for (i = 0UL; i < a->spec_count; ++i) {
        if (a->specs[i].active) {
            r = usd3d_usdc_schema_add_type(s, a->specs[i].path, a->specs[i].specifier, a->specs[i].type_name, USD3D_USDC_SCHEMA_ROLE_PRIM);
            if (r != USD3D_OK) return r;
        }
    }
    for (i = 0UL; i < a->field_count; ++i) {
        if (a->fields[i].active) {
            (void)usd3d_usdc_schema_classify_field(&a->fields[i], &role, &hint);
            if (role == USD3D_USDC_SCHEMA_ROLE_ATTRIBUTE) s->attr_count++;
            else if (role == USD3D_USDC_SCHEMA_ROLE_RELATIONSHIP) s->rel_count++;
            else if (role == USD3D_USDC_SCHEMA_ROLE_METADATA) s->metadata_count++;
            else if (role == USD3D_USDC_SCHEMA_ROLE_COMPOSITION) s->composition_count++;
            if (hint == USD3D_USDC_SCHEMA_HINT_UNKNOWN) {
                (void)usd3d_usdc_schema_add_diag(s, a->fields[i].path, a->fields[i].name, "unknown field value hint", USD3D_USDC_SCHEMA_SEV_INFO);
            }
        }
    }
    return USD3D_OK;
}

int usd3d_usdc_schema_validate(const usd3d_usdc_schema *s, const usd3d_usdc_abstract_data *a, usd3d_usdc_schema *out_diags)
{
    unsigned long i;
    int role;
    int hint;
    (void)s;
    if (!a || !out_diags) return USD3D_ERR_BAD_ARG;
    usd3d_usdc_schema_init(out_diags);
    for (i = 0UL; i < a->field_count; ++i) {
        if (a->fields[i].active) {
            (void)usd3d_usdc_schema_classify_field(&a->fields[i], &role, &hint);
            if (a->fields[i].path[0] != '/') {
                (void)usd3d_usdc_schema_add_diag(out_diags, a->fields[i].path, a->fields[i].name, "field path is not absolute", USD3D_USDC_SCHEMA_SEV_WARN);
            }
            if (role == USD3D_USDC_SCHEMA_ROLE_RELATIONSHIP && hint != USD3D_USDC_SCHEMA_HINT_PATH && hint != USD3D_USDC_SCHEMA_HINT_ASSET) {
                (void)usd3d_usdc_schema_add_diag(out_diags, a->fields[i].path, a->fields[i].name, "relationship lacks path-like target", USD3D_USDC_SCHEMA_SEV_WARN);
            }
        }
    }
    return USD3D_OK;
}

int usd3d_usdc_schema_emit_usda(const usd3d_usdc_schema *s, const usd3d_usdc_abstract_data *a, char *out_text, unsigned long out_cap)
{
    unsigned long pos;
    unsigned long i;
    unsigned long j;
    int role;
    int hint;
    char line[512];
    if (!s || !a || !out_text || out_cap == 0UL) return USD3D_ERR_BAD_ARG;
    pos = 0UL;
    out_text[0] = '\0';
    pos = u3s_append(out_text, out_cap, pos, "#usda 1.0\n# emitted by usd3d_usdc_schema (lossy schema-lite)\n\n");
    for (i = 0UL; i < s->type_count; ++i) {
        if (s->types[i].active) {
            line[0] = '\0';
            (void)strcat(line, s->types[i].specifier[0] ? s->types[i].specifier : "def");
            (void)strcat(line, " ");
            (void)strcat(line, s->types[i].type_name[0] ? s->types[i].type_name : "Scope");
            (void)strcat(line, " \"");
            (void)strcat(line, s->types[i].path);
            (void)strcat(line, "\"\n{\n");
            pos = u3s_append(out_text, out_cap, pos, line);
            for (j = 0UL; j < a->field_count; ++j) {
                if (a->fields[j].active && strcmp(a->fields[j].path, s->types[i].path) == 0) {
                    (void)usd3d_usdc_schema_classify_field(&a->fields[j], &role, &hint);
                    pos = u3s_append(out_text, out_cap, pos, "    custom ");
                    pos = u3s_append(out_text, out_cap, pos, usd3d_usdc_schema_hint_name(hint));
                    pos = u3s_append(out_text, out_cap, pos, " ");
                    pos = u3s_append(out_text, out_cap, pos, a->fields[j].name);
                    pos = u3s_append(out_text, out_cap, pos, " = ");
                    pos = u3s_append(out_text, out_cap, pos, a->fields[j].value);
                    pos = u3s_append(out_text, out_cap, pos, "\n");
                }
            }
            pos = u3s_append(out_text, out_cap, pos, "}\n\n");
        }
    }
    if (pos + 1UL >= out_cap) return USD3D_ERR_LIMIT;
    return USD3D_OK;
}

int usd3d_usdc_schema_emit(const usd3d_usdc_schema *s,
                           int (*type_cb)(void *user, const usd3d_usdc_schema_type *t),
                           int (*diag_cb)(void *user, const usd3d_usdc_schema_diag *d),
                           void *user)
{
    unsigned long i;
    int r;
    if (!s) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < s->type_count; ++i) {
        if (s->types[i].active && type_cb) {
            r = type_cb(user, &s->types[i]);
            if (r != 0) return r;
        }
    }
    for (i = 0UL; i < s->diag_count; ++i) {
        if (s->diags[i].active && diag_cb) {
            r = diag_cb(user, &s->diags[i]);
            if (r != 0) return r;
        }
    }
    return USD3D_OK;
}
