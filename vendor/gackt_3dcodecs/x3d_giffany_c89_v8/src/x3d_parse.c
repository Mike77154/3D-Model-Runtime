#include "x3d_codec.h"
#include <string.h>

#define X3D_XML_MAX_ATTRS 56
#define X3D_XML_STACK 128

typedef struct x3d_xml_attr_s {
    char name[X3D_NAME_MAX];
    const char *value;
    int value_len;
} x3d_xml_attr;

typedef struct x3d_xml_tag_s {
    char name[X3D_NAME_MAX];
    int is_end;
    int self_close;
    int attr_count;
    int line;
    int column;
    x3d_xml_attr attrs[X3D_XML_MAX_ATTRS];
} x3d_xml_tag;

extern int x3d__add_node(x3d_scene *scene, int parent, const char *name, int line);
extern int x3d__add_raw_attr(x3d_scene *scene, int node, const char *name, const char *value);
extern int x3d__copy_attr_text(char *dst, int cap, const char *src, int len);

static int xs_eq(const char *a, const char *b)
{
    int i;
    if (!a || !b) {
        return 0;
    }
    i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        ++i;
    }
    return a[i] == b[i];
}

static int xs_starts(const char *p, const char *lit)
{
    int i;
    i = 0;
    while (lit[i]) {
        if (p[i] != lit[i]) return 0;
        ++i;
    }
    return 1;
}

static int xs_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int xs_sep(char c)
{
    return xs_space(c) || c == ',';
}

static int xs_name_break(char c)
{
    return c == 0 || xs_space(c) || c == '/' || c == '>' || c == '=';
}

static void xs_adv(const char **pp, int *line, int *col)
{
    if (**pp == '\n') {
        *line = *line + 1;
        *col = 1;
    } else {
        *col = *col + 1;
    }
    *pp = *pp + 1;
}

static void xs_copy_frag(char *dst, int cap, const char *src, int len)
{
    int i;
    if (!dst || cap <= 0) return;
    if (!src || len <= 0) {
        dst[0] = 0;
        return;
    }
    i = 0;
    while (i + 1 < cap && i < len) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static void xs_diag(x3d_diag *d, int code, int line, int col, const char *msg)
{
    int i;
    if (!d) return;
    d->code = code;
    d->line = line;
    d->column = col;
    d->node_index = X3D_NONE;
    i = 0;
    if (msg) {
        while (i + 1 < (int)sizeof(d->message) && msg[i]) {
            d->message[i] = msg[i];
            ++i;
        }
    }
    d->message[i] = 0;
}

static void xs_init_tag(x3d_xml_tag *tag)
{
    int i;
    tag->name[0] = 0;
    tag->is_end = 0;
    tag->self_close = 0;
    tag->attr_count = 0;
    tag->line = 1;
    tag->column = 1;
    for (i = 0; i < X3D_XML_MAX_ATTRS; ++i) {
        tag->attrs[i].name[0] = 0;
        tag->attrs[i].value = 0;
        tag->attrs[i].value_len = 0;
    }
}

static int xs_skip_until(const char **pp, int *line, int *col, const char *end)
{
    const char *p;
    p = *pp;
    while (*p) {
        if (xs_starts(p, end)) {
            while (*end) {
                xs_adv(&p, line, col);
                ++end;
            }
            *pp = p;
            return X3D_STATUS_OK;
        }
        xs_adv(&p, line, col);
    }
    *pp = p;
    return X3D_STATUS_XML;
}

static int xs_read_tag(const char **pp, int *line, int *col, x3d_xml_tag *tag, x3d_diag *diag)
{
    const char *p;
    const char *start;
    const char *vstart;
    char quote;
    int len;
    int done;
    int ac;

    xs_init_tag(tag);
    p = *pp;
    while (*p && *p != '<') {
        xs_adv(&p, line, col);
    }
    if (!*p) {
        *pp = p;
        return 0;
    }
    tag->line = *line;
    tag->column = *col;
    xs_adv(&p, line, col);
    if (xs_starts(p, "!--")) {
        xs_adv(&p, line, col);
        xs_adv(&p, line, col);
        xs_adv(&p, line, col);
        if (xs_skip_until(&p, line, col, "-->") != X3D_STATUS_OK) {
            xs_diag(diag, X3D_STATUS_XML, tag->line, tag->column, "unterminated XML comment");
            *pp = p;
            return X3D_STATUS_XML;
        }
        *pp = p;
        return xs_read_tag(pp, line, col, tag, diag);
    }
    if (*p == '?') {
        xs_adv(&p, line, col);
        if (xs_skip_until(&p, line, col, "?>") != X3D_STATUS_OK) {
            xs_diag(diag, X3D_STATUS_XML, tag->line, tag->column, "unterminated XML processing instruction");
            *pp = p;
            return X3D_STATUS_XML;
        }
        *pp = p;
        return xs_read_tag(pp, line, col, tag, diag);
    }
    if (xs_starts(p, "![CDATA[")) {
        while (*p && *p != '[') xs_adv(&p, line, col);
        if (*p == '[') xs_adv(&p, line, col);
        if (xs_skip_until(&p, line, col, "]]>") != X3D_STATUS_OK) {
            xs_diag(diag, X3D_STATUS_XML, tag->line, tag->column, "unterminated CDATA section");
            *pp = p;
            return X3D_STATUS_XML;
        }
        *pp = p;
        return xs_read_tag(pp, line, col, tag, diag);
    }
    if (*p == '!') {
        while (*p && *p != '>') {
            xs_adv(&p, line, col);
        }
        if (*p == '>') xs_adv(&p, line, col);
        *pp = p;
        return xs_read_tag(pp, line, col, tag, diag);
    }
    if (*p == '/') {
        tag->is_end = 1;
        xs_adv(&p, line, col);
    }
    while (*p && xs_space(*p)) xs_adv(&p, line, col);
    start = p;
    while (*p && !xs_name_break(*p)) xs_adv(&p, line, col);
    len = (int)(p - start);
    xs_copy_frag(tag->name, X3D_NAME_MAX, start, len);
    if (tag->is_end) {
        while (*p && *p != '>') xs_adv(&p, line, col);
        if (*p == '>') xs_adv(&p, line, col);
        *pp = p;
        return 1;
    }
    done = 0;
    while (*p && !done) {
        while (*p && xs_space(*p)) xs_adv(&p, line, col);
        if (*p == '/') {
            tag->self_close = 1;
            xs_adv(&p, line, col);
            while (*p && xs_space(*p)) xs_adv(&p, line, col);
            if (*p == '>') {
                xs_adv(&p, line, col);
                done = 1;
            }
        } else if (*p == '>') {
            xs_adv(&p, line, col);
            done = 1;
        } else if (*p) {
            ac = tag->attr_count;
            if (ac >= X3D_XML_MAX_ATTRS) {
                xs_diag(diag, X3D_STATUS_CAPACITY, tag->line, tag->column, "too many attributes on XML tag");
                return X3D_STATUS_CAPACITY;
            }
            start = p;
            while (*p && !xs_name_break(*p)) xs_adv(&p, line, col);
            len = (int)(p - start);
            xs_copy_frag(tag->attrs[ac].name, X3D_NAME_MAX, start, len);
            while (*p && xs_space(*p)) xs_adv(&p, line, col);
            if (*p != '=') {
                xs_diag(diag, X3D_STATUS_XML, tag->line, tag->column, "attribute without equals sign");
                return X3D_STATUS_XML;
            }
            xs_adv(&p, line, col);
            while (*p && xs_space(*p)) xs_adv(&p, line, col);
            quote = *p;
            if (quote != '\'' && quote != '"') {
                xs_diag(diag, X3D_STATUS_XML, tag->line, tag->column, "attribute without quote delimiter");
                return X3D_STATUS_XML;
            }
            xs_adv(&p, line, col);
            vstart = p;
            while (*p && *p != quote) xs_adv(&p, line, col);
            if (*p != quote) {
                xs_diag(diag, X3D_STATUS_XML, tag->line, tag->column, "unterminated attribute value");
                return X3D_STATUS_XML;
            }
            tag->attrs[ac].value = vstart;
            tag->attrs[ac].value_len = (int)(p - vstart);
            tag->attr_count = tag->attr_count + 1;
            xs_adv(&p, line, col);
        }
    }
    *pp = p;
    return 1;
}

static const x3d_xml_attr *xs_attr(const x3d_xml_tag *tag, const char *name)
{
    int i;
    for (i = 0; i < tag->attr_count; ++i) {
        if (xs_eq(tag->attrs[i].name, name)) {
            return &tag->attrs[i];
        }
    }
    return 0;
}

static int xs_next_token(const char *v, int len, int *pos, const char **tok, int *tok_len)
{
    int p;
    int start;
    p = *pos;
    while (p < len && xs_sep(v[p])) ++p;
    if (p >= len) {
        *pos = p;
        return 0;
    }
    start = p;
    if (v[p] == '"') {
        ++p;
        start = p;
        while (p < len && v[p] != '"') ++p;
        *tok = v + start;
        *tok_len = p - start;
        if (p < len && v[p] == '"') ++p;
        *pos = p;
        return 1;
    }
    while (p < len && !xs_sep(v[p])) ++p;
    *tok = v + start;
    *tok_len = p - start;
    *pos = p;
    return 1;
}

static int xs_parse_int_token(const char *s, int len, int *out)
{
    int i;
    int sign;
    long v;
    if (!s || len <= 0 || !out) return X3D_STATUS_BAD_ARG;
    i = 0;
    sign = 1;
    if (s[i] == '-') {
        sign = -1;
        ++i;
    } else if (s[i] == '+') {
        ++i;
    }
    v = 0;
    while (i < len && s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (long)(s[i] - '0');
        ++i;
    }
    *out = (int)(v * (long)sign);
    return X3D_STATUS_OK;
}

static int xs_parse_bool_attr(const x3d_xml_attr *a, int *out)
{
    if (!a || !out) return X3D_STATUS_BAD_ARG;
    if (a->value_len == 4 && a->value[0] == 't' && a->value[1] == 'r' && a->value[2] == 'u' && a->value[3] == 'e') {
        *out = X3D_TRUE;
        return X3D_STATUS_OK;
    }
    if (a->value_len == 5 && a->value[0] == 'f' && a->value[1] == 'a' && a->value[2] == 'l' && a->value[3] == 's' && a->value[4] == 'e') {
        *out = X3D_FALSE;
        return X3D_STATUS_OK;
    }
    if (a->value_len == 4 && a->value[0] == 'T' && a->value[1] == 'R' && a->value[2] == 'U' && a->value[3] == 'E') {
        *out = X3D_TRUE;
        return X3D_STATUS_OK;
    }
    if (a->value_len == 5 && a->value[0] == 'F' && a->value[1] == 'A' && a->value[2] == 'L' && a->value[3] == 'S' && a->value[4] == 'E') {
        *out = X3D_FALSE;
        return X3D_STATUS_OK;
    }
    return X3D_STATUS_NUMBER;
}

static int xs_parse_fx_attr(const x3d_xml_attr *a, x3d_fx *out)
{
    if (!a || !out) return X3D_STATUS_BAD_ARG;
    return x3d_fx_parse(a->value, a->value_len, out);
}

static int xs_parse_vec3_attr(const x3d_xml_attr *a, x3d_vec3 *out)
{
    const char *tok;
    int tok_len;
    int pos;
    x3d_fx vals[3];
    int i;
    if (!a || !out) return X3D_STATUS_BAD_ARG;
    pos = 0;
    i = 0;
    while (i < 3 && xs_next_token(a->value, a->value_len, &pos, &tok, &tok_len)) {
        if (x3d_fx_parse(tok, tok_len, &vals[i]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        ++i;
    }
    if (i != 3) return X3D_STATUS_NUMBER;
    out->x = vals[0];
    out->y = vals[1];
    out->z = vals[2];
    return X3D_STATUS_OK;
}

static int xs_parse_vec2_attr(const x3d_xml_attr *a, x3d_vec2 *out)
{
    const char *tok;
    int tok_len;
    int pos;
    x3d_fx vals[2];
    int i;
    if (!a || !out) return X3D_STATUS_BAD_ARG;
    pos = 0;
    i = 0;
    while (i < 2 && xs_next_token(a->value, a->value_len, &pos, &tok, &tok_len)) {
        if (x3d_fx_parse(tok, tok_len, &vals[i]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        ++i;
    }
    if (i != 2) return X3D_STATUS_NUMBER;
    out->x = vals[0];
    out->y = vals[1];
    return X3D_STATUS_OK;
}

static int xs_parse_fx_pair_attr(const x3d_xml_attr *a, x3d_fx *out0, x3d_fx *out1)
{
    const char *tok;
    int tok_len;
    int pos;
    x3d_fx vals[2];
    int i;
    if (!a || !out0 || !out1) return X3D_STATUS_BAD_ARG;
    pos = 0;
    i = 0;
    while (i < 2 && xs_next_token(a->value, a->value_len, &pos, &tok, &tok_len)) {
        if (x3d_fx_parse(tok, tok_len, &vals[i]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        ++i;
    }
    if (i != 2) return X3D_STATUS_NUMBER;
    *out0 = vals[0];
    *out1 = vals[1];
    return X3D_STATUS_OK;
}

static int xs_parse_rot_attr(const x3d_xml_attr *a, x3d_rot *out)
{
    const char *tok;
    int tok_len;
    int pos;
    x3d_fx vals[4];
    int i;
    if (!a || !out) return X3D_STATUS_BAD_ARG;
    pos = 0;
    i = 0;
    while (i < 4 && xs_next_token(a->value, a->value_len, &pos, &tok, &tok_len)) {
        if (x3d_fx_parse(tok, tok_len, &vals[i]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        ++i;
    }
    if (i != 4) return X3D_STATUS_NUMBER;
    out->x = vals[0];
    out->y = vals[1];
    out->z = vals[2];
    out->angle = vals[3];
    return X3D_STATUS_OK;
}

static int xs_parse_color_attr(const x3d_xml_attr *a, x3d_color *out, int rgba)
{
    const char *tok;
    int tok_len;
    int pos;
    x3d_fx vals[4];
    int need;
    int i;
    if (!a || !out) return X3D_STATUS_BAD_ARG;
    need = rgba ? 4 : 3;
    pos = 0;
    i = 0;
    vals[3] = X3D_ONE;
    while (i < need && xs_next_token(a->value, a->value_len, &pos, &tok, &tok_len)) {
        if (x3d_fx_parse(tok, tok_len, &vals[i]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        ++i;
    }
    if (i != need) return X3D_STATUS_NUMBER;
    out->r = vals[0];
    out->g = vals[1];
    out->b = vals[2];
    out->a = vals[3];
    return X3D_STATUS_OK;
}

static int xs_parse_mfvec3(x3d_scene *s, const x3d_xml_attr *a, int *start_out, int *count_out)
{
    const char *tok;
    int tok_len;
    int pos;
    x3d_fx vals[3];
    int component;
    int count;
    int start;
    if (!s || !a || !start_out || !count_out) return X3D_STATUS_BAD_ARG;
    start = s->vec3_count;
    count = 0;
    component = 0;
    pos = 0;
    vals[0] = 0;
    vals[1] = 0;
    vals[2] = 0;
    while (xs_next_token(a->value, a->value_len, &pos, &tok, &tok_len)) {
        if (x3d_fx_parse(tok, tok_len, &vals[component]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        component = component + 1;
        if (component == 3) {
            if (s->vec3_count >= X3D_MAX_VEC3) return X3D_STATUS_CAPACITY;
            s->vec3_pool[s->vec3_count].x = vals[0];
            s->vec3_pool[s->vec3_count].y = vals[1];
            s->vec3_pool[s->vec3_count].z = vals[2];
            s->vec3_count = s->vec3_count + 1;
            count = count + 1;
            component = 0;
        }
    }
    *start_out = start;
    *count_out = count;
    if (component != 0) return X3D_STATUS_NUMBER;
    return X3D_STATUS_OK;
}

static int xs_parse_mfvec2(x3d_scene *s, const x3d_xml_attr *a, int *start_out, int *count_out)
{
    const char *tok;
    int tok_len;
    int pos;
    x3d_fx vals[2];
    int component;
    int count;
    int start;
    if (!s || !a || !start_out || !count_out) return X3D_STATUS_BAD_ARG;
    start = s->vec2_count;
    count = 0;
    component = 0;
    pos = 0;
    while (xs_next_token(a->value, a->value_len, &pos, &tok, &tok_len)) {
        if (x3d_fx_parse(tok, tok_len, &vals[component]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        component = component + 1;
        if (component == 2) {
            if (s->vec2_count >= X3D_MAX_VEC2) return X3D_STATUS_CAPACITY;
            s->vec2_pool[s->vec2_count].x = vals[0];
            s->vec2_pool[s->vec2_count].y = vals[1];
            s->vec2_count = s->vec2_count + 1;
            count = count + 1;
            component = 0;
        }
    }
    *start_out = start;
    *count_out = count;
    if (component != 0) return X3D_STATUS_NUMBER;
    return X3D_STATUS_OK;
}

static int xs_parse_mfcolor(x3d_scene *s, const x3d_xml_attr *a, int *start_out, int *count_out, int rgba)
{
    const char *tok;
    int tok_len;
    int pos;
    x3d_fx vals[4];
    int component;
    int need;
    int count;
    int start;
    if (!s || !a || !start_out || !count_out) return X3D_STATUS_BAD_ARG;
    start = s->color_count;
    count = 0;
    component = 0;
    need = rgba ? 4 : 3;
    vals[3] = X3D_ONE;
    pos = 0;
    while (xs_next_token(a->value, a->value_len, &pos, &tok, &tok_len)) {
        if (x3d_fx_parse(tok, tok_len, &vals[component]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        component = component + 1;
        if (component == need) {
            if (s->color_count >= X3D_MAX_COLOR) return X3D_STATUS_CAPACITY;
            s->color_pool[s->color_count].r = vals[0];
            s->color_pool[s->color_count].g = vals[1];
            s->color_pool[s->color_count].b = vals[2];
            s->color_pool[s->color_count].a = vals[3];
            s->color_count = s->color_count + 1;
            count = count + 1;
            component = 0;
        }
    }
    *start_out = start;
    *count_out = count;
    if (component != 0) return X3D_STATUS_NUMBER;
    return X3D_STATUS_OK;
}

static int xs_parse_int_list(x3d_scene *s, const x3d_xml_attr *a, int *start_out, int *count_out)
{
    const char *tok;
    int tok_len;
    int pos;
    int val;
    int start;
    int count;
    if (!s || !a || !start_out || !count_out) return X3D_STATUS_BAD_ARG;
    start = s->int_count;
    count = 0;
    pos = 0;
    while (xs_next_token(a->value, a->value_len, &pos, &tok, &tok_len)) {
        if (xs_parse_int_token(tok, tok_len, &val) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        if (s->int_count >= X3D_MAX_INTS) return X3D_STATUS_CAPACITY;
        s->int_pool[s->int_count] = val;
        s->int_count = s->int_count + 1;
        count = count + 1;
    }
    *start_out = start;
    *count_out = count;
    return X3D_STATUS_OK;
}

static int xs_parse_fx_list(x3d_scene *s, const x3d_xml_attr *a, int *start_out, int *count_out)
{
    const char *tok;
    int tok_len;
    int pos;
    x3d_fx val;
    int start;
    int count;
    if (!s || !a || !start_out || !count_out) return X3D_STATUS_BAD_ARG;
    start = s->fx_count;
    count = 0;
    pos = 0;
    while (xs_next_token(a->value, a->value_len, &pos, &tok, &tok_len)) {
        if (x3d_fx_parse(tok, tok_len, &val) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        if (s->fx_count >= X3D_MAX_FX) return X3D_STATUS_CAPACITY;
        s->fx_pool[s->fx_count] = val;
        s->fx_count = s->fx_count + 1;
        count = count + 1;
    }
    *start_out = start;
    *count_out = count;
    return X3D_STATUS_OK;
}

static int xs_parse_mfrot_to_fx(x3d_scene *s, const x3d_xml_attr *a, int *start_out, int *count_out)
{
    int st;
    int ct;
    int rc;
    rc = xs_parse_fx_list(s, a, &st, &ct);
    *start_out = st;
    *count_out = ct / 4;
    if (rc != X3D_STATUS_OK) return rc;
    if ((ct % 4) != 0) return X3D_STATUS_NUMBER;
    return X3D_STATUS_OK;
}

static void xs_copy_attr_to(char *dst, int cap, const x3d_xml_attr *a, x3d_scene *s)
{
    int rc;
    if (!dst || cap <= 0) return;
    if (!a) {
        dst[0] = 0;
        return;
    }
    rc = x3d__copy_attr_text(dst, cap, a->value, a->value_len);
    if (rc != X3D_STATUS_OK && s) {
        s->clipped_value_count = s->clipped_value_count + 1;
    }
}

static void xs_copy_first_mfstring(char *dst, int cap, const x3d_xml_attr *a, x3d_scene *s)
{
    const char *tok;
    int tok_len;
    int pos;
    if (!dst || cap <= 0) return;
    dst[0] = 0;
    if (!a) return;
    pos = 0;
    if (xs_next_token(a->value, a->value_len, &pos, &tok, &tok_len)) {
        x3d__copy_attr_text(dst, cap, tok, tok_len);
    } else {
        xs_copy_attr_to(dst, cap, a, s);
    }
}

static int xs_common_attr(const char *n)
{
    if (xs_eq(n, "DEF")) return 1;
    if (xs_eq(n, "USE")) return 1;
    if (xs_eq(n, "containerField")) return 1;
    if (xs_eq(n, "class")) return 1;
    return 0;
}

static int xs_known_attr(int type, const char *n)
{
    if (xs_common_attr(n)) return 1;
    if (type == X3D_NODE_TRANSFORM) {
        if (xs_eq(n, "bboxCenter") || xs_eq(n, "bboxSize") || xs_eq(n, "center") || xs_eq(n, "rotation") || xs_eq(n, "scale") || xs_eq(n, "scaleOrientation") || xs_eq(n, "translation")) return 1;
    } else if (type == X3D_NODE_MATERIAL) {
        if (xs_eq(n, "ambientIntensity") || xs_eq(n, "diffuseColor") || xs_eq(n, "emissiveColor") || xs_eq(n, "shininess") || xs_eq(n, "specularColor") || xs_eq(n, "transparency")) return 1;
    } else if (type == X3D_NODE_PHYSICAL_MATERIAL) {
        if (xs_eq(n, "baseColor") || xs_eq(n, "emissiveColor") || xs_eq(n, "metallic") || xs_eq(n, "roughness") || xs_eq(n, "transparency") || xs_eq(n, "normalScale") || xs_eq(n, "occlusionStrength") || xs_eq(n, "baseTextureMapping") || xs_eq(n, "emissiveTextureMapping") || xs_eq(n, "metallicRoughnessTextureMapping") || xs_eq(n, "normalTextureMapping") || xs_eq(n, "occlusionTextureMapping")) return 1;
    } else if (type == X3D_NODE_UNLIT_MATERIAL) {
        if (xs_eq(n, "emissiveColor") || xs_eq(n, "transparency") || xs_eq(n, "normalScale") || xs_eq(n, "emissiveTextureMapping") || xs_eq(n, "normalTextureMapping")) return 1;
    } else if (type == X3D_NODE_IMAGE_TEXTURE) {
        if (xs_eq(n, "repeatS") || xs_eq(n, "repeatT") || xs_eq(n, "url")) return 1;
    } else if (type == X3D_NODE_TEXTURE_TRANSFORM) {
        if (xs_eq(n, "center") || xs_eq(n, "rotation") || xs_eq(n, "scale") || xs_eq(n, "translation") || xs_eq(n, "mapping")) return 1;
    } else if (type == X3D_NODE_PIXEL_TEXTURE || type == X3D_NODE_MOVIE_TEXTURE || type == X3D_NODE_MULTI_TEXTURE || type == X3D_NODE_MULTI_TEXTURE_COORDINATE || type == X3D_NODE_MULTI_TEXTURE_TRANSFORM || type == X3D_NODE_ELEVATION_GRID) {
        return 0;
    } else if (type == X3D_NODE_INDEXED_FACE_SET) {
        if (xs_eq(n, "ccw") || xs_eq(n, "colorIndex") || xs_eq(n, "colorPerVertex") || xs_eq(n, "convex") || xs_eq(n, "coordIndex") || xs_eq(n, "creaseAngle") || xs_eq(n, "normalIndex") || xs_eq(n, "normalPerVertex") || xs_eq(n, "solid") || xs_eq(n, "texCoordIndex")) return 1;
    } else if (type == X3D_NODE_INDEXED_TRIANGLE_SET || type == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET || type == X3D_NODE_INDEXED_TRIANGLE_FAN_SET) {
        if (xs_eq(n, "ccw") || xs_eq(n, "colorPerVertex") || xs_eq(n, "normalPerVertex") || xs_eq(n, "solid") || xs_eq(n, "index")) return 1;
    } else if (type == X3D_NODE_TRIANGLE_SET || type == X3D_NODE_TRIANGLE_STRIP_SET || type == X3D_NODE_TRIANGLE_FAN_SET) {
        if (xs_eq(n, "ccw") || xs_eq(n, "colorPerVertex") || xs_eq(n, "normalPerVertex") || xs_eq(n, "solid") || xs_eq(n, "stripCount") || xs_eq(n, "fanCount")) return 1;
    } else if (type == X3D_NODE_INDEXED_LINE_SET) {
        if (xs_eq(n, "colorIndex") || xs_eq(n, "colorPerVertex") || xs_eq(n, "coordIndex")) return 1;
    } else if (type == X3D_NODE_LINE_SET) {
        if (xs_eq(n, "vertexCount")) return 1;
    } else if (type == X3D_NODE_POINT_SET) {
        return 1;
    } else if (type == X3D_NODE_COORDINATE || type == X3D_NODE_NORMAL) {
        if (xs_eq(n, "point") || xs_eq(n, "vector")) return 1;
    } else if (type == X3D_NODE_TEXTURE_COORDINATE) {
        if (xs_eq(n, "point")) return 1;
    } else if (type == X3D_NODE_COLOR) {
        if (xs_eq(n, "color")) return 1;
    } else if (type == X3D_NODE_BOX) {
        if (xs_eq(n, "size") || xs_eq(n, "solid")) return 1;
    } else if (type == X3D_NODE_SPHERE) {
        if (xs_eq(n, "radius") || xs_eq(n, "solid")) return 1;
    } else if (type == X3D_NODE_CONE) {
        if (xs_eq(n, "bottom") || xs_eq(n, "bottomRadius") || xs_eq(n, "height") || xs_eq(n, "side") || xs_eq(n, "solid")) return 1;
    } else if (type == X3D_NODE_CYLINDER) {
        if (xs_eq(n, "bottom") || xs_eq(n, "height") || xs_eq(n, "radius") || xs_eq(n, "side") || xs_eq(n, "solid") || xs_eq(n, "top")) return 1;
    } else if (type == X3D_NODE_VIEWPOINT) {
        if (xs_eq(n, "centerOfRotation") || xs_eq(n, "description") || xs_eq(n, "fieldOfView") || xs_eq(n, "jump") || xs_eq(n, "orientation") || xs_eq(n, "position") || xs_eq(n, "retainUserOffsets")) return 1;
    } else if (type == X3D_NODE_DIRECTIONAL_LIGHT || type == X3D_NODE_POINT_LIGHT || type == X3D_NODE_SPOT_LIGHT) {
        if (xs_eq(n, "ambientIntensity") || xs_eq(n, "color") || xs_eq(n, "direction") || xs_eq(n, "global") || xs_eq(n, "intensity") || xs_eq(n, "location") || xs_eq(n, "on") || xs_eq(n, "radius") || xs_eq(n, "beamWidth") || xs_eq(n, "cutOffAngle")) return 1;
    } else if (type == X3D_NODE_INLINE) {
        if (xs_eq(n, "url") || xs_eq(n, "load")) return 1;
    } else if (type == X3D_NODE_TIME_SENSOR) {
        if (xs_eq(n, "cycleInterval") || xs_eq(n, "enabled") || xs_eq(n, "loop") || xs_eq(n, "startTime") || xs_eq(n, "stopTime") || xs_eq(n, "pauseTime") || xs_eq(n, "resumeTime")) return 1;
    } else if (type == X3D_NODE_POSITION_INTERPOLATOR || type == X3D_NODE_ORIENTATION_INTERPOLATOR || type == X3D_NODE_SCALAR_INTERPOLATOR) {
        if (xs_eq(n, "key") || xs_eq(n, "keyValue")) return 1;
    } else if (type == X3D_NODE_ROUTE) {
        if (xs_eq(n, "fromNode") || xs_eq(n, "fromField") || xs_eq(n, "toNode") || xs_eq(n, "toField")) return 1;
    } else if (type == X3D_NODE_WORLD_INFO) {
        if (xs_eq(n, "title") || xs_eq(n, "info")) return 1;
    } else if (type == X3D_NODE_SWITCH) {
        if (xs_eq(n, "whichChoice") || xs_eq(n, "bboxCenter") || xs_eq(n, "bboxSize")) return 1;
    } else if (type == X3D_NODE_LOD) {
        if (xs_eq(n, "center") || xs_eq(n, "range") || xs_eq(n, "forceTransitions") || xs_eq(n, "bboxCenter") || xs_eq(n, "bboxSize")) return 1;
    } else if (type == X3D_NODE_METADATA_STRING) {
        if (xs_eq(n, "name") || xs_eq(n, "reference") || xs_eq(n, "value")) return 1;
    }
    return 0;
}

static int xs_parse_common(x3d_scene *s, int node, const x3d_xml_tag *tag)
{
    const x3d_xml_attr *a;
    a = xs_attr(tag, "DEF");
    if (a) xs_copy_attr_to(s->nodes[node].def_name, X3D_NAME_MAX, a, s);
    a = xs_attr(tag, "USE");
    if (a) xs_copy_attr_to(s->nodes[node].use_name, X3D_NAME_MAX, a, s);
    a = xs_attr(tag, "containerField");
    if (a) xs_copy_attr_to(s->nodes[node].container_field, X3D_NAME_MAX, a, s);
    a = xs_attr(tag, "class");
    if (a) xs_copy_attr_to(s->nodes[node].class_name, X3D_NAME_MAX, a, s);
    return X3D_STATUS_OK;
}

static int xs_is_rendering_geom(int type)
{
    return type == X3D_NODE_INDEXED_TRIANGLE_SET ||
        type == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET ||
        type == X3D_NODE_INDEXED_TRIANGLE_FAN_SET ||
        type == X3D_NODE_TRIANGLE_SET ||
        type == X3D_NODE_TRIANGLE_STRIP_SET ||
        type == X3D_NODE_TRIANGLE_FAN_SET ||
        type == X3D_NODE_INDEXED_LINE_SET ||
        type == X3D_NODE_LINE_SET ||
        type == X3D_NODE_POINT_SET;
}


static int xs_type_is_sensor(int t)
{
    return t == X3D_NODE_TOUCH_SENSOR || t == X3D_NODE_PLANE_SENSOR ||
        t == X3D_NODE_CYLINDER_SENSOR || t == X3D_NODE_SPHERE_SENSOR ||
        t == X3D_NODE_PROXIMITY_SENSOR || t == X3D_NODE_VISIBILITY_SENSOR ||
        t == X3D_NODE_LOAD_SENSOR || t == X3D_NODE_KEY_SENSOR ||
        t == X3D_NODE_STRING_SENSOR || t == X3D_NODE_COLLISION_SENSOR ||
        t == X3D_NODE_LINE_PICK_SENSOR || t == X3D_NODE_POINT_PICK_SENSOR ||
        t == X3D_NODE_PRIMITIVE_PICK_SENSOR || t == X3D_NODE_VOLUME_PICK_SENSOR ||
        t == X3D_NODE_GEO_PROXIMITY_SENSOR || t == X3D_NODE_GEO_TOUCH_SENSOR ||
        t == X3D_NODE_TRANSFORM_SENSOR;
}

static int xs_type_is_cad(int t)
{
    return t == X3D_NODE_CAD_ASSEMBLY || t == X3D_NODE_CAD_PART ||
        t == X3D_NODE_CAD_LAYER || t == X3D_NODE_CAD_FACE || t == X3D_NODE_QUAD_SET;
}

static int xs_type_is_nurbs(int t)
{
    return t == X3D_NODE_NURBS_CURVE || t == X3D_NODE_NURBS_CURVE_2D ||
        t == X3D_NODE_NURBS_PATCH_SURFACE || t == X3D_NODE_NURBS_SET ||
        t == X3D_NODE_NURBS_TRIMMED_SURFACE || t == X3D_NODE_NURBS_TEXTURE_COORDINATE ||
        t == X3D_NODE_NURBS_ORIENTATION_INTERPOLATOR || t == X3D_NODE_NURBS_POSITION_INTERPOLATOR ||
        t == X3D_NODE_NURBS_SURFACE_INTERPOLATOR || t == X3D_NODE_NURBS_SWEPT_SURFACE ||
        t == X3D_NODE_NURBS_SWUNG_SURFACE || t == X3D_NODE_CONTOUR_2D ||
        t == X3D_NODE_CONTOUR_POLYLINE_2D;
}

static int xs_type_is_hanim(int t)
{
    return t == X3D_NODE_HANIM_HUMANOID || t == X3D_NODE_HANIM_JOINT ||
        t == X3D_NODE_HANIM_SEGMENT || t == X3D_NODE_HANIM_SITE ||
        t == X3D_NODE_HANIM_DISPLACER || t == X3D_NODE_HANIM_MOTION;
}

static int xs_type_is_geo(int t)
{
    return t == X3D_NODE_GEO_COORDINATE || t == X3D_NODE_GEO_ELEVATION_GRID ||
        t == X3D_NODE_GEO_LOCATION || t == X3D_NODE_GEO_LOD ||
        t == X3D_NODE_GEO_METADATA || t == X3D_NODE_GEO_ORIGIN ||
        t == X3D_NODE_GEO_POSITION_INTERPOLATOR || t == X3D_NODE_GEO_PROXIMITY_SENSOR ||
        t == X3D_NODE_GEO_TOUCH_SENSOR || t == X3D_NODE_GEO_TRANSFORM ||
        t == X3D_NODE_GEO_VIEWPOINT;
}

static int xs_type_is_proto(int t)
{
    return t == X3D_NODE_PROTO_DECLARE || t == X3D_NODE_PROTO_INTERFACE ||
        t == X3D_NODE_PROTO_BODY || t == X3D_NODE_PROTO_INSTANCE ||
        t == X3D_NODE_EXTERN_PROTO_DECLARE || t == X3D_NODE_IS ||
        t == X3D_NODE_CONNECT || t == X3D_NODE_FIELD_VALUE;
}

static int xs_type_is_shader(int t)
{
    return t == X3D_NODE_COMPOSED_SHADER || t == X3D_NODE_SHADER_PART ||
        t == X3D_NODE_SHADER_PROGRAM || t == X3D_NODE_PACKAGED_SHADER ||
        t == X3D_NODE_PROGRAM_SHADER || t == X3D_NODE_FLOAT_VERTEX_ATTRIBUTE ||
        t == X3D_NODE_MATRIX3_VERTEX_ATTRIBUTE || t == X3D_NODE_MATRIX4_VERTEX_ATTRIBUTE;
}

static int xs_type_is_environment(int t)
{
    return t == X3D_NODE_BACKGROUND || t == X3D_NODE_TEXTURE_BACKGROUND ||
        t == X3D_NODE_FOG || t == X3D_NODE_LOCAL_FOG ||
        t == X3D_NODE_FOG_COORDINATE;
}

static int xs_type_is_navigation(int t)
{
    return t == X3D_NODE_NAVIGATION_INFO || t == X3D_NODE_BILLBOARD ||
        t == X3D_NODE_COLLISION || t == X3D_NODE_LOD ||
        t == X3D_NODE_VIEWPOINT_GROUP || t == X3D_NODE_ANCHOR;
}

static int xs_type_is_sound(int t)
{
    return t == X3D_NODE_SOUND || t == X3D_NODE_SPATIAL_SOUND ||
        t == X3D_NODE_AUDIO_CLIP || t == X3D_NODE_GAIN ||
        t == X3D_NODE_OSCILLATOR_SOURCE || t == X3D_NODE_AUDIO_DESTINATION ||
        t == X3D_NODE_LISTENER_POINT_SOURCE;
}

static int xs_type_is_physics(int t)
{
    return t == X3D_NODE_RIGID_BODY_COLLECTION || t == X3D_NODE_RIGID_BODY ||
        t == X3D_NODE_COLLIDABLE_SHAPE || t == X3D_NODE_COLLIDABLE_OFFSET ||
        t == X3D_NODE_COLLISION_COLLECTION || t == X3D_NODE_COLLISION_SPACE ||
        t == X3D_NODE_CONTACT || t == X3D_NODE_BALL_JOINT ||
        t == X3D_NODE_SINGLE_AXIS_HINGE_JOINT || t == X3D_NODE_DOUBLE_AXIS_HINGE_JOINT ||
        t == X3D_NODE_SLIDER_JOINT || t == X3D_NODE_UNIVERSAL_JOINT ||
        t == X3D_NODE_MOTOR_JOINT;
}

static int xs_type_is_event_utility(int t)
{
    return t == X3D_NODE_BOOLEAN_FILTER || t == X3D_NODE_BOOLEAN_SEQUENCER ||
        t == X3D_NODE_BOOLEAN_TOGGLE || t == X3D_NODE_BOOLEAN_TRIGGER ||
        t == X3D_NODE_INTEGER_SEQUENCER || t == X3D_NODE_INTEGER_TRIGGER ||
        t == X3D_NODE_TIME_TRIGGER;
}

static int xs_text_capture_type(int t)
{
    return t == X3D_NODE_SCRIPT || t == X3D_NODE_SHADER_PART ||
        t == X3D_NODE_SHADER_PROGRAM || t == X3D_NODE_PACKAGED_SHADER ||
        t == X3D_NODE_PROGRAM_SHADER;
}

static int xs_parse_int_attr(const x3d_xml_attr *a, int *out)
{
    if (!a || !out) return X3D_STATUS_BAD_ARG;
    return xs_parse_int_token(a->value, a->value_len, out);
}

static void xs_link_geo_child(x3d_scene *s, int node)
{
    int p;
    p = s->nodes[node].parent;
    if (p == X3D_NONE) return;
    if (s->nodes[p].type == X3D_NODE_INDEXED_FACE_SET) {
        if (s->nodes[node].type == X3D_NODE_COORDINATE) s->ifs_nodes[p].coord_node = node;
        if (s->nodes[node].type == X3D_NODE_NORMAL) s->ifs_nodes[p].normal_node = node;
        if (s->nodes[node].type == X3D_NODE_TEXTURE_COORDINATE) s->ifs_nodes[p].texcoord_node = node;
        if (s->nodes[node].type == X3D_NODE_COLOR) s->ifs_nodes[p].color_node = node;
    }
    if (xs_is_rendering_geom(s->nodes[p].type)) {
        if (s->nodes[node].type == X3D_NODE_COORDINATE) s->indexed_geoms[p].coord_node = node;
        if (s->nodes[node].type == X3D_NODE_NORMAL) s->indexed_geoms[p].normal_node = node;
        if (s->nodes[node].type == X3D_NODE_TEXTURE_COORDINATE) s->indexed_geoms[p].texcoord_node = node;
        if (s->nodes[node].type == X3D_NODE_COLOR) s->indexed_geoms[p].color_node = node;
    }
    if (xs_type_is_nurbs(s->nodes[p].type)) {
        if (s->nodes[node].type == X3D_NODE_COORDINATE || s->nodes[node].type == X3D_NODE_COORDINATE_DOUBLE) s->nurbs_nodes[p].control_point_node = node;
    }
}

static int xs_parse_node_data(x3d_scene *s, int node, const x3d_xml_tag *tag, x3d_diag *diag)
{
    const x3d_xml_attr *a;
    int rc;
    int st;
    int ct;
    int type;
    int route_idx;
    int rgba;
    char raw[X3D_VALUE_MAX];
    int i;

    type = s->nodes[node].type;
    xs_parse_common(s, node, tag);

    if (type == X3D_NODE_TRANSFORM) {
        a = xs_attr(tag, "bboxCenter"); if (a) xs_parse_vec3_attr(a, &s->transforms[node].bbox_center);
        a = xs_attr(tag, "bboxSize"); if (a) xs_parse_vec3_attr(a, &s->transforms[node].bbox_size);
        a = xs_attr(tag, "center"); if (a) xs_parse_vec3_attr(a, &s->transforms[node].center);
        a = xs_attr(tag, "rotation"); if (a) xs_parse_rot_attr(a, &s->transforms[node].rotation);
        a = xs_attr(tag, "scale"); if (a) xs_parse_vec3_attr(a, &s->transforms[node].scale);
        a = xs_attr(tag, "scaleOrientation"); if (a) xs_parse_rot_attr(a, &s->transforms[node].scale_orientation);
        a = xs_attr(tag, "translation"); if (a) xs_parse_vec3_attr(a, &s->transforms[node].translation);
    } else if (type == X3D_NODE_MATERIAL) {
        a = xs_attr(tag, "ambientIntensity"); if (a) xs_parse_fx_attr(a, &s->materials[node].ambient_intensity);
        a = xs_attr(tag, "diffuseColor"); if (a) xs_parse_color_attr(a, &s->materials[node].diffuse_color, 0);
        a = xs_attr(tag, "emissiveColor"); if (a) xs_parse_color_attr(a, &s->materials[node].emissive_color, 0);
        a = xs_attr(tag, "shininess"); if (a) xs_parse_fx_attr(a, &s->materials[node].shininess);
        a = xs_attr(tag, "specularColor"); if (a) xs_parse_color_attr(a, &s->materials[node].specular_color, 0);
        a = xs_attr(tag, "transparency"); if (a) xs_parse_fx_attr(a, &s->materials[node].transparency);
    } else if (type == X3D_NODE_PHYSICAL_MATERIAL) {
        a = xs_attr(tag, "baseColor"); if (a) xs_parse_color_attr(a, &s->pbr_materials[node].base_color, 0);
        a = xs_attr(tag, "emissiveColor"); if (a) xs_parse_color_attr(a, &s->pbr_materials[node].emissive_color, 0);
        a = xs_attr(tag, "metallic"); if (a) xs_parse_fx_attr(a, &s->pbr_materials[node].metallic);
        a = xs_attr(tag, "roughness"); if (a) xs_parse_fx_attr(a, &s->pbr_materials[node].roughness);
        a = xs_attr(tag, "transparency"); if (a) xs_parse_fx_attr(a, &s->pbr_materials[node].transparency);
        a = xs_attr(tag, "normalScale"); if (a) xs_parse_fx_attr(a, &s->pbr_materials[node].normal_scale);
        a = xs_attr(tag, "occlusionStrength"); if (a) xs_parse_fx_attr(a, &s->pbr_materials[node].occlusion_strength);
        a = xs_attr(tag, "baseTextureMapping"); if (a) xs_copy_attr_to(s->pbr_materials[node].base_texture_mapping, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "emissiveTextureMapping"); if (a) xs_copy_attr_to(s->pbr_materials[node].emissive_texture_mapping, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "metallicRoughnessTextureMapping"); if (a) xs_copy_attr_to(s->pbr_materials[node].metallic_roughness_texture_mapping, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "normalTextureMapping"); if (a) xs_copy_attr_to(s->pbr_materials[node].normal_texture_mapping, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "occlusionTextureMapping"); if (a) xs_copy_attr_to(s->pbr_materials[node].occlusion_texture_mapping, X3D_NAME_MAX, a, s);
    } else if (type == X3D_NODE_UNLIT_MATERIAL) {
        s->pbr_materials[node].base_color.r = X3D_ONE;
        s->pbr_materials[node].base_color.g = X3D_ONE;
        s->pbr_materials[node].base_color.b = X3D_ONE;
        s->pbr_materials[node].base_color.a = X3D_ONE;
        a = xs_attr(tag, "emissiveColor"); if (a) xs_parse_color_attr(a, &s->pbr_materials[node].emissive_color, 0);
        a = xs_attr(tag, "transparency"); if (a) xs_parse_fx_attr(a, &s->pbr_materials[node].transparency);
        a = xs_attr(tag, "normalScale"); if (a) xs_parse_fx_attr(a, &s->pbr_materials[node].normal_scale);
        a = xs_attr(tag, "emissiveTextureMapping"); if (a) xs_copy_attr_to(s->pbr_materials[node].emissive_texture_mapping, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "normalTextureMapping"); if (a) xs_copy_attr_to(s->pbr_materials[node].normal_texture_mapping, X3D_NAME_MAX, a, s);
    } else if (type == X3D_NODE_IMAGE_TEXTURE) {
        a = xs_attr(tag, "repeatS"); if (a) xs_parse_bool_attr(a, &s->image_textures[node].repeat_s);
        a = xs_attr(tag, "repeatT"); if (a) xs_parse_bool_attr(a, &s->image_textures[node].repeat_t);
        a = xs_attr(tag, "url"); if (a) xs_copy_first_mfstring(s->image_textures[node].url, X3D_VALUE_MAX, a, s);
    } else if (type == X3D_NODE_TEXTURE_TRANSFORM) {
        a = xs_attr(tag, "center"); if (a) xs_parse_vec2_attr(a, &s->texture_transforms[node].center);
        a = xs_attr(tag, "rotation"); if (a) xs_parse_fx_attr(a, &s->texture_transforms[node].rotation);
        a = xs_attr(tag, "scale"); if (a) xs_parse_vec2_attr(a, &s->texture_transforms[node].scale);
        a = xs_attr(tag, "translation"); if (a) xs_parse_vec2_attr(a, &s->texture_transforms[node].translation);
        a = xs_attr(tag, "mapping"); if (a) xs_copy_attr_to(s->texture_transforms[node].mapping, X3D_NAME_MAX, a, s);
    } else if (type == X3D_NODE_INDEXED_FACE_SET) {
        a = xs_attr(tag, "ccw"); if (a) xs_parse_bool_attr(a, &s->ifs_nodes[node].ccw);
        a = xs_attr(tag, "colorPerVertex"); if (a) xs_parse_bool_attr(a, &s->ifs_nodes[node].color_per_vertex);
        a = xs_attr(tag, "convex"); if (a) xs_parse_bool_attr(a, &s->ifs_nodes[node].convex);
        a = xs_attr(tag, "normalPerVertex"); if (a) xs_parse_bool_attr(a, &s->ifs_nodes[node].normal_per_vertex);
        a = xs_attr(tag, "solid"); if (a) xs_parse_bool_attr(a, &s->ifs_nodes[node].solid);
        a = xs_attr(tag, "creaseAngle"); if (a) xs_parse_fx_attr(a, &s->ifs_nodes[node].crease_angle);
        a = xs_attr(tag, "coordIndex"); if (a) { rc = xs_parse_int_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->ifs_nodes[node].coord_index_start = st; s->ifs_nodes[node].coord_index_count = ct; }
        a = xs_attr(tag, "normalIndex"); if (a) { rc = xs_parse_int_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->ifs_nodes[node].normal_index_start = st; s->ifs_nodes[node].normal_index_count = ct; }
        a = xs_attr(tag, "texCoordIndex"); if (a) { rc = xs_parse_int_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->ifs_nodes[node].texcoord_index_start = st; s->ifs_nodes[node].texcoord_index_count = ct; }
        a = xs_attr(tag, "colorIndex"); if (a) { rc = xs_parse_int_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->ifs_nodes[node].color_index_start = st; s->ifs_nodes[node].color_index_count = ct; }
    } else if (type == X3D_NODE_INDEXED_TRIANGLE_SET || type == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET || type == X3D_NODE_INDEXED_TRIANGLE_FAN_SET) {
        a = xs_attr(tag, "ccw"); if (a) xs_parse_bool_attr(a, &s->indexed_geoms[node].ccw);
        a = xs_attr(tag, "colorPerVertex"); if (a) xs_parse_bool_attr(a, &s->indexed_geoms[node].color_per_vertex);
        a = xs_attr(tag, "normalPerVertex"); if (a) xs_parse_bool_attr(a, &s->indexed_geoms[node].normal_per_vertex);
        a = xs_attr(tag, "solid"); if (a) xs_parse_bool_attr(a, &s->indexed_geoms[node].solid);
        a = xs_attr(tag, "index"); if (a) { rc = xs_parse_int_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->indexed_geoms[node].index_start = st; s->indexed_geoms[node].index_count = ct; }
    } else if (type == X3D_NODE_TRIANGLE_SET || type == X3D_NODE_TRIANGLE_STRIP_SET || type == X3D_NODE_TRIANGLE_FAN_SET) {
        a = xs_attr(tag, "ccw"); if (a) xs_parse_bool_attr(a, &s->indexed_geoms[node].ccw);
        a = xs_attr(tag, "colorPerVertex"); if (a) xs_parse_bool_attr(a, &s->indexed_geoms[node].color_per_vertex);
        a = xs_attr(tag, "normalPerVertex"); if (a) xs_parse_bool_attr(a, &s->indexed_geoms[node].normal_per_vertex);
        a = xs_attr(tag, "solid"); if (a) xs_parse_bool_attr(a, &s->indexed_geoms[node].solid);
        a = xs_attr(tag, "stripCount"); if (a) { rc = xs_parse_int_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->indexed_geoms[node].strip_count_start = st; s->indexed_geoms[node].strip_count_count = ct; }
        a = xs_attr(tag, "fanCount"); if (a) { rc = xs_parse_int_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->indexed_geoms[node].fan_count_start = st; s->indexed_geoms[node].fan_count_count = ct; }
    } else if (type == X3D_NODE_INDEXED_LINE_SET) {
        a = xs_attr(tag, "colorPerVertex"); if (a) xs_parse_bool_attr(a, &s->indexed_geoms[node].color_per_vertex);
        a = xs_attr(tag, "coordIndex"); if (a) { rc = xs_parse_int_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->indexed_geoms[node].index_start = st; s->indexed_geoms[node].index_count = ct; }
    } else if (type == X3D_NODE_LINE_SET) {
        a = xs_attr(tag, "vertexCount"); if (a) { rc = xs_parse_int_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->indexed_geoms[node].vertex_count_start = st; s->indexed_geoms[node].vertex_count_count = ct; }
    } else if (type == X3D_NODE_COORDINATE) {
        a = xs_attr(tag, "point");
        if (a) { rc = xs_parse_mfvec3(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->coord_refs[node].start = st; s->coord_refs[node].count = ct; }
        xs_link_geo_child(s, node);
    } else if (type == X3D_NODE_NORMAL) {
        a = xs_attr(tag, "vector");
        if (!a) a = xs_attr(tag, "point");
        if (a) { rc = xs_parse_mfvec3(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->normal_refs[node].start = st; s->normal_refs[node].count = ct; }
        xs_link_geo_child(s, node);
    } else if (type == X3D_NODE_TEXTURE_COORDINATE) {
        a = xs_attr(tag, "point");
        if (a) { rc = xs_parse_mfvec2(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->texcoord_refs[node].start = st; s->texcoord_refs[node].count = ct; }
        xs_link_geo_child(s, node);
    } else if (type == X3D_NODE_COLOR) {
        rgba = xs_eq(tag->name, "ColorRGBA");
        a = xs_attr(tag, "color");
        if (a) { rc = xs_parse_mfcolor(s, a, &st, &ct, rgba); if (rc != X3D_STATUS_OK) return rc; s->color_refs[node].start = st; s->color_refs[node].count = ct; }
        xs_link_geo_child(s, node);
    } else if (type == X3D_NODE_BOX) {
        a = xs_attr(tag, "size"); if (a) xs_parse_vec3_attr(a, &s->boxes[node].size);
        a = xs_attr(tag, "solid"); if (a) xs_parse_bool_attr(a, &s->boxes[node].solid);
    } else if (type == X3D_NODE_SPHERE) {
        a = xs_attr(tag, "radius"); if (a) xs_parse_fx_attr(a, &s->spheres[node].radius);
        a = xs_attr(tag, "solid"); if (a) xs_parse_bool_attr(a, &s->spheres[node].solid);
    } else if (type == X3D_NODE_CONE) {
        a = xs_attr(tag, "bottom"); if (a) xs_parse_bool_attr(a, &s->cones[node].bottom);
        a = xs_attr(tag, "bottomRadius"); if (a) xs_parse_fx_attr(a, &s->cones[node].bottom_radius);
        a = xs_attr(tag, "height"); if (a) xs_parse_fx_attr(a, &s->cones[node].height);
        a = xs_attr(tag, "side"); if (a) xs_parse_bool_attr(a, &s->cones[node].side);
        a = xs_attr(tag, "solid"); if (a) xs_parse_bool_attr(a, &s->cones[node].solid);
    } else if (type == X3D_NODE_CYLINDER) {
        a = xs_attr(tag, "bottom"); if (a) xs_parse_bool_attr(a, &s->cylinders[node].bottom);
        a = xs_attr(tag, "height"); if (a) xs_parse_fx_attr(a, &s->cylinders[node].height);
        a = xs_attr(tag, "radius"); if (a) xs_parse_fx_attr(a, &s->cylinders[node].radius);
        a = xs_attr(tag, "side"); if (a) xs_parse_bool_attr(a, &s->cylinders[node].side);
        a = xs_attr(tag, "solid"); if (a) xs_parse_bool_attr(a, &s->cylinders[node].solid);
        a = xs_attr(tag, "top"); if (a) xs_parse_bool_attr(a, &s->cylinders[node].top);
    } else if (type == X3D_NODE_VIEWPOINT) {
        a = xs_attr(tag, "centerOfRotation"); if (a) xs_parse_vec3_attr(a, &s->viewpoints[node].center_of_rotation);
        a = xs_attr(tag, "description"); if (a) xs_copy_attr_to(s->viewpoints[node].description, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "fieldOfView"); if (a) xs_parse_fx_attr(a, &s->viewpoints[node].field_of_view);
        a = xs_attr(tag, "jump"); if (a) xs_parse_bool_attr(a, &s->viewpoints[node].jump);
        a = xs_attr(tag, "orientation"); if (a) xs_parse_rot_attr(a, &s->viewpoints[node].orientation);
        a = xs_attr(tag, "position"); if (a) xs_parse_vec3_attr(a, &s->viewpoints[node].position);
        a = xs_attr(tag, "retainUserOffsets"); if (a) xs_parse_bool_attr(a, &s->viewpoints[node].retain_user_offsets);
    } else if (type == X3D_NODE_DIRECTIONAL_LIGHT || type == X3D_NODE_POINT_LIGHT || type == X3D_NODE_SPOT_LIGHT) {
        a = xs_attr(tag, "ambientIntensity"); if (a) xs_parse_fx_attr(a, &s->lights[node].ambient_intensity);
        a = xs_attr(tag, "color"); if (a) xs_parse_color_attr(a, &s->lights[node].color, 0);
        a = xs_attr(tag, "direction"); if (a) xs_parse_vec3_attr(a, &s->lights[node].direction);
        a = xs_attr(tag, "global"); if (a) xs_parse_bool_attr(a, &s->lights[node].global);
        a = xs_attr(tag, "intensity"); if (a) xs_parse_fx_attr(a, &s->lights[node].intensity);
        a = xs_attr(tag, "location"); if (a) xs_parse_vec3_attr(a, &s->lights[node].location);
        a = xs_attr(tag, "on"); if (a) xs_parse_bool_attr(a, &s->lights[node].on);
        a = xs_attr(tag, "radius"); if (a) xs_parse_fx_attr(a, &s->lights[node].radius);
        a = xs_attr(tag, "beamWidth"); if (a) xs_parse_fx_attr(a, &s->lights[node].beam_width);
        a = xs_attr(tag, "cutOffAngle"); if (a) xs_parse_fx_attr(a, &s->lights[node].cut_off_angle);
    } else if (type == X3D_NODE_INLINE) {
        a = xs_attr(tag, "url"); if (a) xs_copy_first_mfstring(s->inlines[node].url, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "load"); if (a) xs_parse_bool_attr(a, &s->inlines[node].load);
    } else if (type == X3D_NODE_TIME_SENSOR) {
        a = xs_attr(tag, "cycleInterval"); if (a) xs_parse_fx_attr(a, &s->time_sensors[node].cycle_interval);
        a = xs_attr(tag, "enabled"); if (a) xs_parse_bool_attr(a, &s->time_sensors[node].enabled);
        a = xs_attr(tag, "loop"); if (a) xs_parse_bool_attr(a, &s->time_sensors[node].loop);
        a = xs_attr(tag, "startTime"); if (a) xs_parse_fx_attr(a, &s->time_sensors[node].start_time);
        a = xs_attr(tag, "stopTime"); if (a) xs_parse_fx_attr(a, &s->time_sensors[node].stop_time);
        a = xs_attr(tag, "pauseTime"); if (a) xs_parse_fx_attr(a, &s->time_sensors[node].pause_time);
        a = xs_attr(tag, "resumeTime"); if (a) xs_parse_fx_attr(a, &s->time_sensors[node].resume_time);
    } else if (type == X3D_NODE_SCRIPT) {
        a = xs_attr(tag, "directOutput"); if (a) xs_parse_bool_attr(a, &s->scripts[node].direct_output);
        a = xs_attr(tag, "mustEvaluate"); if (a) xs_parse_bool_attr(a, &s->scripts[node].must_evaluate);
        a = xs_attr(tag, "url"); if (a) xs_copy_first_mfstring(s->scripts[node].url, X3D_VALUE_MAX, a, s);
    } else if (type == X3D_NODE_FIELD) {
        int pnode;
        int owner;
        pnode = s->nodes[node].parent;
        owner = pnode;
        if (pnode >= 0 && pnode < s->node_count && s->nodes[pnode].type == X3D_NODE_PROTO_INTERFACE) {
            owner = s->nodes[pnode].parent;
        }
        if (owner >= 0 && owner < s->node_count &&
            (s->nodes[owner].type == X3D_NODE_SCRIPT || xs_type_is_proto(s->nodes[owner].type) || xs_type_is_shader(s->nodes[owner].type))) {
            if (s->script_field_count >= X3D_MAX_SCRIPT_FIELDS) return X3D_STATUS_CAPACITY;
            route_idx = s->script_field_count;
            s->script_field_count = s->script_field_count + 1;
            if (s->nodes[owner].type == X3D_NODE_SCRIPT) {
                if (s->scripts[owner].field_start == X3D_NONE) s->scripts[owner].field_start = route_idx;
                s->scripts[owner].field_count = s->scripts[owner].field_count + 1;
            } else if (xs_type_is_shader(s->nodes[owner].type)) {
                if (s->shader_nodes[owner].field_start == X3D_NONE) s->shader_nodes[owner].field_start = route_idx;
                s->shader_nodes[owner].field_count = s->shader_nodes[owner].field_count + 1;
            } else {
                if (s->proto_nodes[owner].field_start == X3D_NONE) s->proto_nodes[owner].field_start = route_idx;
                s->proto_nodes[owner].field_count = s->proto_nodes[owner].field_count + 1;
            }
            s->script_fields[route_idx].script_node = owner;
            s->script_fields[route_idx].field_node = node;
            a = xs_attr(tag, "accessType"); if (a) xs_copy_attr_to(s->script_fields[route_idx].access_type, X3D_NAME_MAX, a, s);
            a = xs_attr(tag, "type"); if (a) xs_copy_attr_to(s->script_fields[route_idx].field_type, X3D_NAME_MAX, a, s);
            a = xs_attr(tag, "name"); if (a) xs_copy_attr_to(s->script_fields[route_idx].name, X3D_NAME_MAX, a, s);
            a = xs_attr(tag, "value"); if (a) xs_copy_attr_to(s->script_fields[route_idx].value, X3D_VALUE_MAX, a, s);
        }
    } else if (xs_type_is_sensor(type)) {
        a = xs_attr(tag, "enabled"); if (a) xs_parse_bool_attr(a, &s->sensors[node].enabled);
        a = xs_attr(tag, "description"); if (a) xs_copy_attr_to(s->sensors[node].description, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "objectType"); if (a) xs_copy_attr_to(s->sensors[node].object_type, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "center"); if (a) xs_parse_vec3_attr(a, &s->sensors[node].center);
        a = xs_attr(tag, "size"); if (a) xs_parse_vec3_attr(a, &s->sensors[node].size);
        a = xs_attr(tag, "minPosition"); if (a) xs_parse_vec3_attr(a, &s->sensors[node].min_position);
        a = xs_attr(tag, "maxPosition"); if (a) xs_parse_vec3_attr(a, &s->sensors[node].max_position);
        a = xs_attr(tag, "offset"); if (a) xs_parse_vec3_attr(a, &s->sensors[node].offset);
    } else if (xs_type_is_cad(type)) {
        a = xs_attr(tag, "name"); if (a) xs_copy_attr_to(s->cad_nodes[node].name, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "partName"); if (a) xs_copy_attr_to(s->cad_nodes[node].name, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "faceType"); if (a) xs_copy_attr_to(s->cad_nodes[node].face_type, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "visible"); if (a) xs_parse_bool_attr(a, &s->cad_nodes[node].visible);
        a = xs_attr(tag, "bboxCenter"); if (a) xs_parse_vec3_attr(a, &s->cad_nodes[node].bbox_center);
        a = xs_attr(tag, "bboxSize"); if (a) xs_parse_vec3_attr(a, &s->cad_nodes[node].bbox_size);
        a = xs_attr(tag, "center"); if (a) xs_parse_vec3_attr(a, &s->cad_nodes[node].center);
        a = xs_attr(tag, "rotation"); if (a) xs_parse_rot_attr(a, &s->cad_nodes[node].rotation);
        a = xs_attr(tag, "scale"); if (a) xs_parse_vec3_attr(a, &s->cad_nodes[node].scale);
        a = xs_attr(tag, "scaleOrientation"); if (a) xs_parse_rot_attr(a, &s->cad_nodes[node].scale_orientation);
        a = xs_attr(tag, "translation"); if (a) xs_parse_vec3_attr(a, &s->cad_nodes[node].translation);
    } else if (xs_type_is_nurbs(type) || type == X3D_NODE_COORDINATE_DOUBLE) {
        if (type == X3D_NODE_COORDINATE_DOUBLE) {
            a = xs_attr(tag, "point");
            if (a) { rc = xs_parse_mfvec3(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->coord_refs[node].start = st; s->coord_refs[node].count = ct; }
            xs_link_geo_child(s, node);
        } else {
            a = xs_attr(tag, "order"); if (a) xs_parse_int_attr(a, &s->nurbs_nodes[node].order);
            a = xs_attr(tag, "uOrder"); if (a) xs_parse_int_attr(a, &s->nurbs_nodes[node].u_order);
            a = xs_attr(tag, "vOrder"); if (a) xs_parse_int_attr(a, &s->nurbs_nodes[node].v_order);
            a = xs_attr(tag, "uDimension"); if (a) xs_parse_int_attr(a, &s->nurbs_nodes[node].u_dimension);
            a = xs_attr(tag, "vDimension"); if (a) xs_parse_int_attr(a, &s->nurbs_nodes[node].v_dimension);
            a = xs_attr(tag, "tessellation"); if (a) xs_parse_int_attr(a, &s->nurbs_nodes[node].tessellation);
            a = xs_attr(tag, "uTessellation"); if (a) xs_parse_int_attr(a, &s->nurbs_nodes[node].u_tessellation);
            a = xs_attr(tag, "vTessellation"); if (a) xs_parse_int_attr(a, &s->nurbs_nodes[node].v_tessellation);
            a = xs_attr(tag, "closed"); if (a) xs_parse_bool_attr(a, &s->nurbs_nodes[node].closed);
            a = xs_attr(tag, "uClosed"); if (a) xs_parse_bool_attr(a, &s->nurbs_nodes[node].u_closed);
            a = xs_attr(tag, "vClosed"); if (a) xs_parse_bool_attr(a, &s->nurbs_nodes[node].v_closed);
            a = xs_attr(tag, "solid"); if (a) xs_parse_bool_attr(a, &s->nurbs_nodes[node].solid);
            a = xs_attr(tag, "knot"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->nurbs_nodes[node].knot_start = st; s->nurbs_nodes[node].knot_count = ct; }
            a = xs_attr(tag, "uKnot"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->nurbs_nodes[node].u_knot_start = st; s->nurbs_nodes[node].u_knot_count = ct; }
            a = xs_attr(tag, "vKnot"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->nurbs_nodes[node].v_knot_start = st; s->nurbs_nodes[node].v_knot_count = ct; }
            a = xs_attr(tag, "weight"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->nurbs_nodes[node].weight_start = st; s->nurbs_nodes[node].weight_count = ct; }
        }
    } else if (xs_type_is_hanim(type)) {
        a = xs_attr(tag, "name"); if (a) xs_copy_attr_to(s->hanim_nodes[node].name, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "version"); if (a) xs_copy_attr_to(s->hanim_nodes[node].version, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "info"); if (a) xs_copy_attr_to(s->hanim_nodes[node].info, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "center"); if (a) xs_parse_vec3_attr(a, &s->hanim_nodes[node].center);
        a = xs_attr(tag, "rotation"); if (a) xs_parse_rot_attr(a, &s->hanim_nodes[node].rotation);
        a = xs_attr(tag, "scale"); if (a) xs_parse_vec3_attr(a, &s->hanim_nodes[node].scale);
        a = xs_attr(tag, "scaleOrientation"); if (a) xs_parse_rot_attr(a, &s->hanim_nodes[node].scale_orientation);
        a = xs_attr(tag, "translation"); if (a) xs_parse_vec3_attr(a, &s->hanim_nodes[node].translation);
        a = xs_attr(tag, "skinCoordIndex"); if (a) { rc = xs_parse_int_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->hanim_nodes[node].skin_coord_index_start = st; s->hanim_nodes[node].skin_coord_index_count = ct; }
        a = xs_attr(tag, "skinCoordWeight"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->hanim_nodes[node].skin_coord_weight_start = st; s->hanim_nodes[node].skin_coord_weight_count = ct; }
        a = xs_attr(tag, "displacements"); if (a) { rc = xs_parse_mfvec3(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->hanim_nodes[node].displacements_start = st; s->hanim_nodes[node].displacements_count = ct; }
        a = xs_attr(tag, "frameValues"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->hanim_nodes[node].frame_values_start = st; s->hanim_nodes[node].frame_values_count = ct; }
    } else if (xs_type_is_geo(type)) {
        a = xs_attr(tag, "geoSystem"); if (a) xs_copy_attr_to(s->geo_nodes[node].geo_system, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "geoGridOrigin"); if (a) xs_copy_attr_to(s->geo_nodes[node].geo_grid_origin, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "point"); if (a) { rc = xs_parse_mfvec3(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->geo_nodes[node].point_start = st; s->geo_nodes[node].point_count = ct; s->coord_refs[node].start = st; s->coord_refs[node].count = ct; }
        a = xs_attr(tag, "geoCoords"); if (a) xs_parse_vec3_attr(a, &s->geo_nodes[node].geo_coords);
        a = xs_attr(tag, "geoCenter"); if (a) xs_parse_vec3_attr(a, &s->geo_nodes[node].geo_center);
        a = xs_attr(tag, "center"); if (a) { xs_parse_vec3_attr(a, &s->geo_nodes[node].center); xs_parse_vec3_attr(a, &s->sensors[node].center); }
        a = xs_attr(tag, "size"); if (a) { xs_parse_vec3_attr(a, &s->geo_nodes[node].size); xs_parse_vec3_attr(a, &s->sensors[node].size); }
        a = xs_attr(tag, "position"); if (a) xs_parse_vec3_attr(a, &s->geo_nodes[node].position);
        a = xs_attr(tag, "orientation"); if (a) xs_parse_rot_attr(a, &s->geo_nodes[node].orientation);
    } else if (xs_type_is_proto(type)) {
        a = xs_attr(tag, "name"); if (a) xs_copy_attr_to(s->proto_nodes[node].name, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "url"); if (a) xs_copy_first_mfstring(s->proto_nodes[node].url, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "appinfo"); if (a) xs_copy_attr_to(s->proto_nodes[node].appinfo, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "documentation"); if (a) xs_copy_attr_to(s->proto_nodes[node].documentation, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "protoField"); if (a) xs_copy_attr_to(s->proto_nodes[node].proto_field, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "nodeField"); if (a) xs_copy_attr_to(s->proto_nodes[node].node_field, X3D_NAME_MAX, a, s);
    } else if (xs_type_is_shader(type)) {
        a = xs_attr(tag, "language"); if (a) xs_copy_attr_to(s->shader_nodes[node].language, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "type"); if (a) xs_copy_attr_to(s->shader_nodes[node].type, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "url"); if (a) xs_copy_first_mfstring(s->shader_nodes[node].url, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "activate"); if (a) xs_parse_bool_attr(a, &s->shader_nodes[node].activate);
        a = xs_attr(tag, "isSelected"); if (a) xs_parse_bool_attr(a, &s->shader_nodes[node].is_selected);
        a = xs_attr(tag, "isValid"); if (a) xs_parse_bool_attr(a, &s->shader_nodes[node].is_valid);
        a = xs_attr(tag, "name"); if (a) xs_copy_attr_to(s->proto_nodes[node].name, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "value"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc == X3D_STATUS_OK) { s->interpolators[node].value_start = st; s->interpolators[node].value_count = ct; } }
    } else if (xs_type_is_environment(type)) {
        a = xs_attr(tag, "set_bind"); if (a) xs_parse_bool_attr(a, &s->environment_nodes[node].bind);
        a = xs_attr(tag, "bind"); if (a) xs_parse_bool_attr(a, &s->environment_nodes[node].bind);
        a = xs_attr(tag, "isBound"); if (a) xs_parse_bool_attr(a, &s->environment_nodes[node].is_bound);
        a = xs_attr(tag, "bindTime"); if (a) xs_parse_fx_attr(a, &s->environment_nodes[node].bind_time);
        a = xs_attr(tag, "skyColor"); if (a) xs_parse_color_attr(a, &s->environment_nodes[node].sky_color, 0);
        a = xs_attr(tag, "groundColor"); if (a) xs_parse_color_attr(a, &s->environment_nodes[node].ground_color, 0);
        a = xs_attr(tag, "transparency"); if (a) xs_parse_fx_attr(a, &s->environment_nodes[node].transparency);
        a = xs_attr(tag, "backUrl"); if (a) xs_copy_first_mfstring(s->environment_nodes[node].back_url, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "frontUrl"); if (a) xs_copy_first_mfstring(s->environment_nodes[node].front_url, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "leftUrl"); if (a) xs_copy_first_mfstring(s->environment_nodes[node].left_url, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "rightUrl"); if (a) xs_copy_first_mfstring(s->environment_nodes[node].right_url, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "topUrl"); if (a) xs_copy_first_mfstring(s->environment_nodes[node].top_url, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "bottomUrl"); if (a) xs_copy_first_mfstring(s->environment_nodes[node].bottom_url, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "fogType"); if (a) xs_copy_attr_to(s->environment_nodes[node].fog_type, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "color"); if (a) xs_parse_color_attr(a, &s->environment_nodes[node].fog_color, 0);
        a = xs_attr(tag, "visibilityRange"); if (a) xs_parse_fx_attr(a, &s->environment_nodes[node].visibility_range);
        a = xs_attr(tag, "enabled"); if (a) xs_parse_bool_attr(a, &s->environment_nodes[node].enabled);
        if (type == X3D_NODE_FOG_COORDINATE) { a = xs_attr(tag, "depth"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc == X3D_STATUS_OK) { s->interpolators[node].value_start = st; s->interpolators[node].value_count = ct; } } }
    } else if (xs_type_is_navigation(type)) {
        a = xs_attr(tag, "type"); if (a) xs_copy_attr_to(s->navigation_nodes[node].type, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "description"); if (a) xs_copy_attr_to(s->navigation_nodes[node].description, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "headlight"); if (a) xs_parse_bool_attr(a, &s->navigation_nodes[node].headlight);
        a = xs_attr(tag, "speed"); if (a) xs_parse_fx_attr(a, &s->navigation_nodes[node].speed);
        a = xs_attr(tag, "transitionTime"); if (a) xs_parse_fx_attr(a, &s->navigation_nodes[node].transition_time);
        a = xs_attr(tag, "visibilityLimit"); if (a) xs_parse_fx_attr(a, &s->navigation_nodes[node].visibility_limit);
        a = xs_attr(tag, "avatarSize"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc == X3D_STATUS_OK) { s->navigation_nodes[node].avatar_size_start = st; s->navigation_nodes[node].avatar_size_count = ct; } }
        a = xs_attr(tag, "axisOfRotation"); if (a) xs_parse_vec3_attr(a, &s->navigation_nodes[node].axis_of_rotation);
        a = xs_attr(tag, "bboxCenter"); if (a) xs_parse_vec3_attr(a, &s->navigation_nodes[node].bbox_center);
        a = xs_attr(tag, "bboxSize"); if (a) xs_parse_vec3_attr(a, &s->navigation_nodes[node].bbox_size);
    } else if (xs_type_is_sound(type)) {
        a = xs_attr(tag, "url"); if (a) xs_copy_first_mfstring(s->audio_nodes[node].url, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "description"); if (a) xs_copy_attr_to(s->audio_nodes[node].description, X3D_VALUE_MAX, a, s);
        a = xs_attr(tag, "enabled"); if (a) xs_parse_bool_attr(a, &s->audio_nodes[node].enabled);
        a = xs_attr(tag, "loop"); if (a) xs_parse_bool_attr(a, &s->audio_nodes[node].loop);
        a = xs_attr(tag, "startTime"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].start_time);
        a = xs_attr(tag, "stopTime"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].stop_time);
        a = xs_attr(tag, "pauseTime"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].pause_time);
        a = xs_attr(tag, "resumeTime"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].resume_time);
        a = xs_attr(tag, "pitch"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].pitch);
        a = xs_attr(tag, "gain"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].gain);
        a = xs_attr(tag, "intensity"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].intensity);
        a = xs_attr(tag, "priority"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].priority);
        a = xs_attr(tag, "minFront"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].min_front);
        a = xs_attr(tag, "maxFront"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].max_front);
        a = xs_attr(tag, "minBack"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].min_back);
        a = xs_attr(tag, "maxBack"); if (a) xs_parse_fx_attr(a, &s->audio_nodes[node].max_back);
        a = xs_attr(tag, "location"); if (a) xs_parse_vec3_attr(a, &s->audio_nodes[node].location);
        a = xs_attr(tag, "direction"); if (a) xs_parse_vec3_attr(a, &s->audio_nodes[node].direction);
    } else if (xs_type_is_physics(type)) {
        a = xs_attr(tag, "enabled"); if (a) xs_parse_bool_attr(a, &s->physics_nodes[node].enabled);
        a = xs_attr(tag, "autoDisable"); if (a) xs_parse_bool_attr(a, &s->physics_nodes[node].auto_disable);
        a = xs_attr(tag, "fixed"); if (a) xs_parse_bool_attr(a, &s->physics_nodes[node].fixed);
        a = xs_attr(tag, "useGlobalGravity"); if (a) xs_parse_bool_attr(a, &s->physics_nodes[node].use_global_gravity);
        a = xs_attr(tag, "mass"); if (a) xs_parse_fx_attr(a, &s->physics_nodes[node].mass);
        a = xs_attr(tag, "bounce"); if (a) xs_parse_fx_attr(a, &s->physics_nodes[node].bounce);
        a = xs_attr(tag, "frictionCoefficients"); if (a) xs_parse_fx_pair_attr(a, &s->physics_nodes[node].friction_coefficients[0], &s->physics_nodes[node].friction_coefficients[1]);
        a = xs_attr(tag, "minBounceSpeed"); if (a) xs_parse_fx_attr(a, &s->physics_nodes[node].min_bounce_speed);
        a = xs_attr(tag, "slipFactors"); if (a) xs_parse_fx_pair_attr(a, &s->physics_nodes[node].slip_factors[0], &s->physics_nodes[node].slip_factors[1]);
        a = xs_attr(tag, "gravity"); if (a) xs_parse_vec3_attr(a, &s->physics_nodes[node].gravity);
        a = xs_attr(tag, "position"); if (a) xs_parse_vec3_attr(a, &s->physics_nodes[node].position);
        a = xs_attr(tag, "orientation"); if (a) xs_parse_rot_attr(a, &s->physics_nodes[node].orientation);
        a = xs_attr(tag, "linearVelocity"); if (a) xs_parse_vec3_attr(a, &s->physics_nodes[node].linear_velocity);
        a = xs_attr(tag, "angularVelocity"); if (a) xs_parse_vec3_attr(a, &s->physics_nodes[node].angular_velocity);
        a = xs_attr(tag, "centerOfMass"); if (a) xs_parse_vec3_attr(a, &s->physics_nodes[node].center_of_mass);
        a = xs_attr(tag, "bboxCenter"); if (a) xs_parse_vec3_attr(a, &s->physics_nodes[node].bbox_center);
        a = xs_attr(tag, "bboxSize"); if (a) xs_parse_vec3_attr(a, &s->physics_nodes[node].bbox_size);
        a = xs_attr(tag, "body1"); if (a) xs_copy_attr_to(s->physics_nodes[node].body1, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "body2"); if (a) xs_copy_attr_to(s->physics_nodes[node].body2, X3D_NAME_MAX, a, s);
    } else if (xs_type_is_event_utility(type)) {
        a = xs_attr(tag, "key"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc == X3D_STATUS_OK) { s->interpolators[node].key_start = st; s->interpolators[node].key_count = ct; } }
        a = xs_attr(tag, "keyValue"); if (a) { rc = xs_parse_int_list(s, a, &st, &ct); if (rc == X3D_STATUS_OK) { s->indexed_geoms[node].index_start = st; s->indexed_geoms[node].index_count = ct; } }
    } else if (type == X3D_NODE_POSITION_INTERPOLATOR) {
        a = xs_attr(tag, "key"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->interpolators[node].key_start = st; s->interpolators[node].key_count = ct; }
        a = xs_attr(tag, "keyValue"); if (a) { rc = xs_parse_mfvec3(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->interpolators[node].value_start = st; s->interpolators[node].value_count = ct; }
    } else if (type == X3D_NODE_ORIENTATION_INTERPOLATOR) {
        a = xs_attr(tag, "key"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->interpolators[node].key_start = st; s->interpolators[node].key_count = ct; }
        a = xs_attr(tag, "keyValue"); if (a) { rc = xs_parse_mfrot_to_fx(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->interpolators[node].value_start = st; s->interpolators[node].value_count = ct; }
    } else if (type == X3D_NODE_SCALAR_INTERPOLATOR) {
        a = xs_attr(tag, "key"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->interpolators[node].key_start = st; s->interpolators[node].key_count = ct; }
        a = xs_attr(tag, "keyValue"); if (a) { rc = xs_parse_fx_list(s, a, &st, &ct); if (rc != X3D_STATUS_OK) return rc; s->interpolators[node].value_start = st; s->interpolators[node].value_count = ct; }
    } else if (type == X3D_NODE_ROUTE) {
        if (s->route_count >= X3D_MAX_ROUTES) return X3D_STATUS_CAPACITY;
        route_idx = s->route_count;
        s->route_count = s->route_count + 1;
        s->routes[route_idx].node_index = node;
        a = xs_attr(tag, "fromNode"); if (a) xs_copy_attr_to(s->routes[route_idx].from_node, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "fromField"); if (a) xs_copy_attr_to(s->routes[route_idx].from_field, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "toNode"); if (a) xs_copy_attr_to(s->routes[route_idx].to_node, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "toField"); if (a) xs_copy_attr_to(s->routes[route_idx].to_field, X3D_NAME_MAX, a, s);
    } else if (type == X3D_NODE_WORLD_INFO) {
        a = xs_attr(tag, "title"); if (a) xs_copy_attr_to(s->viewpoints[node].description, X3D_VALUE_MAX, a, s);
    }

    for (i = 0; i < tag->attr_count; ++i) {
        if (!xs_known_attr(type, tag->attrs[i].name) || type == X3D_NODE_UNKNOWN) {
            raw[0] = 0;
            xs_copy_attr_to(raw, X3D_VALUE_MAX, &tag->attrs[i], s);
            x3d__add_raw_attr(s, node, tag->attrs[i].name, raw);
        }
    }
    if (diag && s->clipped_value_count > 0) {
        diag->warnings = diag->warnings + 1;
    }
    return X3D_STATUS_OK;
}

static int xs_parse_head_stmt(x3d_scene *s, const x3d_xml_tag *tag)
{
    const x3d_xml_attr *a;
    int idx;
    int level;
    if (!s || !tag) return X3D_STATUS_BAD_ARG;
    if (xs_eq(tag->name, "meta")) {
        if (s->meta_count >= X3D_MAX_META) return X3D_STATUS_CAPACITY;
        idx = s->meta_count;
        s->meta_count = s->meta_count + 1;
        s->metas[idx].name[0] = 0;
        s->metas[idx].content[0] = 0;
        a = xs_attr(tag, "name"); if (a) xs_copy_attr_to(s->metas[idx].name, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "content"); if (a) xs_copy_attr_to(s->metas[idx].content, X3D_VALUE_MAX, a, s);
        return X3D_STATUS_OK;
    }
    if (xs_eq(tag->name, "component")) {
        if (s->component_count >= X3D_MAX_COMPONENTS) return X3D_STATUS_CAPACITY;
        idx = s->component_count;
        s->component_count = s->component_count + 1;
        s->components[idx].name[0] = 0;
        s->components[idx].level = 1;
        a = xs_attr(tag, "name"); if (a) xs_copy_attr_to(s->components[idx].name, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "level");
        if (a && xs_parse_int_token(a->value, a->value_len, &level) == X3D_STATUS_OK) s->components[idx].level = level;
        return X3D_STATUS_OK;
    }
    if (xs_eq(tag->name, "unit")) {
        if (s->unit_count >= X3D_MAX_UNITS) return X3D_STATUS_CAPACITY;
        idx = s->unit_count;
        s->unit_count = s->unit_count + 1;
        s->units[idx].category[0] = 0;
        s->units[idx].name[0] = 0;
        s->units[idx].conversion_factor = X3D_ONE;
        a = xs_attr(tag, "category"); if (a) xs_copy_attr_to(s->units[idx].category, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "name"); if (a) xs_copy_attr_to(s->units[idx].name, X3D_NAME_MAX, a, s);
        a = xs_attr(tag, "conversionFactor"); if (a) xs_parse_fx_attr(a, &s->units[idx].conversion_factor);
        return X3D_STATUS_OK;
    }
    return X3D_STATUS_OK;
}

static int xs_parse_x3d_root(x3d_scene *s, const x3d_xml_tag *tag)
{
    const x3d_xml_attr *a;
    a = xs_attr(tag, "version");
    if (a) xs_copy_attr_to(s->version, (int)sizeof(s->version), a, s);
    a = xs_attr(tag, "profile");
    if (a) xs_copy_attr_to(s->profile, X3D_NAME_MAX, a, s);
    return X3D_STATUS_OK;
}

static int xs_nearest_parent(const int *stack, int sp)
{
    int i;
    for (i = sp - 1; i >= 0; --i) {
        if (stack[i] >= 0) return stack[i];
    }
    return X3D_NONE;
}


static int xs_current_text_node(const x3d_scene *s, const int *stack, int sp)
{
    int i;
    int n;
    if (!s || !stack) return X3D_NONE;
    for (i = sp - 1; i >= 0; --i) {
        n = stack[i];
        if (n >= 0 && n < s->node_count && xs_text_capture_type(s->nodes[n].type)) return n;
    }
    return X3D_NONE;
}

static int xs_text_has_nonspace(const char *p, int len)
{
    int i;
    for (i = 0; i < len; ++i) {
        if (!xs_space(p[i])) return 1;
    }
    return 0;
}

static void xs_append_node_source(x3d_scene *s, int node, const char *p, int len)
{
    int d;
    int i;
    char *dst;
    int cap;
    if (!s || node < 0 || node >= s->node_count || !p || len <= 0) return;
    if (!xs_text_has_nonspace(p, len)) return;
    if (s->nodes[node].type == X3D_NODE_SCRIPT) {
        dst = s->scripts[node].source;
        cap = X3D_SCRIPT_TEXT_MAX;
    } else {
        dst = s->shader_nodes[node].source;
        cap = X3D_SHADER_TEXT_MAX;
    }
    d = 0;
    while (d < cap - 1 && dst[d]) ++d;
    i = 0;
    while (d < cap - 1 && i < len) {
        dst[d] = p[i];
        d = d + 1;
        i = i + 1;
    }
    dst[d] = 0;
    if (i < len) s->clipped_value_count = s->clipped_value_count + 1;
}

static void xs_capture_node_text(x3d_scene *s, const int *stack, int sp, const char **pp, const char *end, int *line, int *col)
{
    const char *p;
    const char *start;
    int script;
    script = xs_current_text_node(s, stack, sp);
    if (script == X3D_NONE) return;
    p = *pp;
    while (p < end && *p) {
        if (xs_starts(p, "<![CDATA[")) {
            int k;
            k = 0;
            while (k < 9) { xs_adv(&p, line, col); ++k; }
            start = p;
            while (p < end && *p && !xs_starts(p, "]]>") ) xs_adv(&p, line, col);
            xs_append_node_source(s, script, start, (int)(p - start));
            if (xs_starts(p, "]]>")) {
                xs_adv(&p, line, col);
                xs_adv(&p, line, col);
                xs_adv(&p, line, col);
            }
        } else if (*p == '<') {
            break;
        } else {
            start = p;
            while (p < end && *p && *p != '<') xs_adv(&p, line, col);
            xs_append_node_source(s, script, start, (int)(p - start));
        }
    }
    *pp = p;
}

int x3d_parse_xml(x3d_scene *scene, const char *text, int text_len, x3d_diag *diag)
{
    const char *p;
    const char *end;
    int line;
    int col;
    int rc;
    x3d_xml_tag tag;
    int stack[X3D_XML_STACK];
    int sp;
    int parent;
    int node;
    int inside_head;
    int scene_depth;
    int marker;

    if (!scene || !text || text_len < 0) {
        xs_diag(diag, X3D_STATUS_BAD_ARG, 1, 1, "bad parse argument");
        return X3D_STATUS_BAD_ARG;
    }
    x3d_scene_init(scene);
    if (diag) {
        diag->code = X3D_STATUS_OK;
        diag->line = 1;
        diag->column = 1;
        diag->node_index = X3D_NONE;
        diag->warnings = 0;
        diag->message[0] = 0;
    }
    p = text;
    end = text + text_len;
    line = 1;
    col = 1;
    sp = 0;
    inside_head = 0;
    scene_depth = 0;
    while (p < end && *p) {
        xs_capture_node_text(scene, stack, sp, &p, end, &line, &col);
        rc = xs_read_tag(&p, &line, &col, &tag, diag);
        if (rc == 0) break;
        if (rc < 0) return rc;
        if (tag.name[0] == 0) continue;
        if (tag.is_end) {
            if (xs_eq(tag.name, "head")) {
                if (inside_head > 0) inside_head = inside_head - 1;
            }
            if (xs_eq(tag.name, "Scene")) {
                if (scene_depth > 0) scene_depth = scene_depth - 1;
            }
            if (sp > 0) sp = sp - 1;
        } else {
            marker = X3D_NONE;
            if (xs_eq(tag.name, "X3D")) {
                xs_parse_x3d_root(scene, &tag);
                marker = -10;
            } else if (xs_eq(tag.name, "head")) {
                inside_head = inside_head + 1;
                marker = -20;
            } else if (inside_head > 0) {
                rc = xs_parse_head_stmt(scene, &tag);
                if (rc != X3D_STATUS_OK) {
                    xs_diag(diag, rc, tag.line, tag.column, "head statement capacity exceeded");
                    return rc;
                }
                marker = -21;
            } else if (xs_eq(tag.name, "Scene") || scene_depth > 0) {
                parent = xs_nearest_parent(stack, sp);
                if (xs_eq(tag.name, "Scene")) {
                    parent = X3D_NONE;
                    scene_depth = scene_depth + 1;
                }
                node = x3d__add_node(scene, parent, tag.name, tag.line);
                if (node == X3D_NONE) {
                    xs_diag(diag, X3D_STATUS_CAPACITY, tag.line, tag.column, "node capacity exceeded");
                    return X3D_STATUS_CAPACITY;
                }
                marker = node;
                rc = xs_parse_node_data(scene, node, &tag, diag);
                if (rc != X3D_STATUS_OK) {
                    xs_diag(diag, rc, tag.line, tag.column, "field parse failed");
                    if (diag) diag->node_index = node;
                    return rc;
                }
            } else {
                marker = -30;
            }
            if (!tag.self_close) {
                if (sp >= X3D_XML_STACK) {
                    xs_diag(diag, X3D_STATUS_CAPACITY, tag.line, tag.column, "XML stack capacity exceeded");
                    return X3D_STATUS_CAPACITY;
                }
                stack[sp] = marker;
                sp = sp + 1;
            } else {
                if (xs_eq(tag.name, "head") && inside_head > 0) inside_head = inside_head - 1;
                if (xs_eq(tag.name, "Scene") && scene_depth > 0) scene_depth = scene_depth - 1;
            }
        }
    }
    if (scene->scene_node == X3D_NONE) {
        node = x3d__add_node(scene, X3D_NONE, "Scene", 1);
        if (node == X3D_NONE) {
            xs_diag(diag, X3D_STATUS_CAPACITY, 1, 1, "could not create fallback Scene node");
            return X3D_STATUS_CAPACITY;
        }
    }
    if (diag) {
        diag->code = scene->unsupported_node_count > 0 || scene->clipped_value_count > 0 ? X3D_STATUS_WARNINGS : X3D_STATUS_OK;
        diag->warnings = diag->warnings + scene->unsupported_node_count + scene->clipped_value_count;
        if (diag->message[0] == 0 && diag->warnings > 0) {
            xs_diag(diag, X3D_STATUS_WARNINGS, 1, 1, "scene parsed with preserved unsupported data");
        }
    }
    return X3D_STATUS_OK;
}
