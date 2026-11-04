#include "x3d_codec.h"
#include <string.h>
#include <stdio.h>

#define CV_TOKEN_MAX 192
#define CV_DEF_MAX 256
#define CV_STACK_MAX 128

#define CV_T_END 0
#define CV_T_WORD 1
#define CV_T_STRING 2
#define CV_T_SYM 3

typedef struct cv_tok_s {
    int kind;
    char text[CV_TOKEN_MAX];
    int line;
} cv_tok;

typedef struct cv_parse_s {
    const char *p;
    const char *end;
    int line;
} cv_parse;

typedef struct cv_def_s {
    char name[X3D_NAME_MAX];
    char type[X3D_NAME_MAX];
} cv_def;

typedef struct cv_defs_s {
    cv_def items[CV_DEF_MAX];
    int count;
} cv_defs;

typedef struct cv_buf_s {
    char *out;
    int cap;
    int len;
} cv_buf;

extern int x3d__add_node(x3d_scene *scene, int parent, const char *name, int line);
extern int x3d__add_raw_attr(x3d_scene *scene, int node, const char *name, const char *value);
extern int x3d__copy_attr_text(char *dst, int cap, const char *src, int len);

static int cv_eq(const char *a, const char *b)
{
    int i;
    if (!a || !b) return 0;
    i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        ++i;
    }
    return a[i] == b[i];
}

static int cv_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int cv_is_sym(char c)
{
    return c == '{' || c == '}' || c == '[' || c == ']' || c == ',';
}

static void cv_diag(x3d_diag *d, int code, int line, const char *msg)
{
    int i;
    if (!d) return;
    d->code = code;
    d->line = line;
    d->column = 1;
    d->node_index = X3D_NONE;
    d->warnings = 0;
    i = 0;
    if (msg) {
        while (i + 1 < (int)sizeof(d->message) && msg[i]) {
            d->message[i] = msg[i];
            ++i;
        }
    }
    d->message[i] = 0;
}

static void cv_skip_ws(cv_parse *p)
{
    while (p->p < p->end && *p->p) {
        if (*p->p == '#') {
            while (p->p < p->end && *p->p && *p->p != '\n') ++p->p;
        } else if (cv_is_space(*p->p)) {
            if (*p->p == '\n') p->line = p->line + 1;
            ++p->p;
        } else {
            break;
        }
    }
}

static void cv_copy_token(char *dst, int cap, const char *src, int len)
{
    int i;
    if (!dst || cap <= 0) return;
    i = 0;
    while (i + 1 < cap && i < len) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static int cv_next(cv_parse *p, cv_tok *t)
{
    const char *s;
    int len;
    char q;
    if (!p || !t) return 0;
    cv_skip_ws(p);
    t->text[0] = 0;
    t->line = p->line;
    if (p->p >= p->end || !*p->p) {
        t->kind = CV_T_END;
        return 0;
    }
    if (cv_is_sym(*p->p)) {
        t->kind = CV_T_SYM;
        t->text[0] = *p->p;
        t->text[1] = 0;
        ++p->p;
        return 1;
    }
    if (*p->p == '"' || *p->p == '\'') {
        q = *p->p;
        ++p->p;
        s = p->p;
        while (p->p < p->end && *p->p && *p->p != q) {
            if (*p->p == '\n') p->line = p->line + 1;
            ++p->p;
        }
        len = (int)(p->p - s);
        cv_copy_token(t->text, CV_TOKEN_MAX, s, len);
        if (p->p < p->end && *p->p == q) ++p->p;
        t->kind = CV_T_STRING;
        return 1;
    }
    s = p->p;
    while (p->p < p->end && *p->p && !cv_is_space(*p->p) && !cv_is_sym(*p->p) && *p->p != '#') {
        ++p->p;
    }
    len = (int)(p->p - s);
    cv_copy_token(t->text, CV_TOKEN_MAX, s, len);
    t->kind = CV_T_WORD;
    return 1;
}

static int cv_peek(cv_parse *p, cv_tok *t)
{
    cv_parse save;
    int rc;
    save = *p;
    rc = cv_next(p, t);
    *p = save;
    return rc;
}

static int cv_take_sym(cv_parse *p, char c)
{
    cv_tok t;
    cv_parse save;
    save = *p;
    if (cv_next(p, &t) && t.kind == CV_T_SYM && t.text[0] == c) return 1;
    *p = save;
    return 0;
}

static void cv_def_add(cv_defs *defs, const char *name, const char *type)
{
    int i;
    if (!defs || !name || !type || !name[0] || !type[0]) return;
    for (i = 0; i < defs->count; ++i) {
        if (cv_eq(defs->items[i].name, name)) {
            x3d__copy_attr_text(defs->items[i].type, X3D_NAME_MAX, type, (int)strlen(type));
            return;
        }
    }
    if (defs->count >= CV_DEF_MAX) return;
    x3d__copy_attr_text(defs->items[defs->count].name, X3D_NAME_MAX, name, (int)strlen(name));
    x3d__copy_attr_text(defs->items[defs->count].type, X3D_NAME_MAX, type, (int)strlen(type));
    defs->count = defs->count + 1;
}

static const char *cv_def_type(const cv_defs *defs, const char *name)
{
    int i;
    if (!defs || !name) return 0;
    for (i = 0; i < defs->count; ++i) {
        if (cv_eq(defs->items[i].name, name)) return defs->items[i].type;
    }
    return 0;
}

static int cv_parse_int_text(const char *s, int *out)
{
    int i;
    int sign;
    long v;
    if (!s || !out) return X3D_STATUS_BAD_ARG;
    i = 0;
    sign = 1;
    if (s[i] == '-') { sign = -1; ++i; }
    else if (s[i] == '+') { ++i; }
    v = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (long)(s[i] - '0');
        ++i;
    }
    *out = (int)(v * (long)sign);
    return X3D_STATUS_OK;
}

static int cv_parse_bool_text(const char *s, int *out)
{
    if (!s || !out) return X3D_STATUS_BAD_ARG;
    if (cv_eq(s, "TRUE") || cv_eq(s, "true") || cv_eq(s, "1")) { *out = X3D_TRUE; return X3D_STATUS_OK; }
    if (cv_eq(s, "FALSE") || cv_eq(s, "false") || cv_eq(s, "0")) { *out = X3D_FALSE; return X3D_STATUS_OK; }
    return X3D_STATUS_NUMBER;
}

static int cv_next_value_token(const char *v, int len, int *pos, const char **tok, int *tok_len)
{
    int p;
    int st;
    p = *pos;
    while (p < len && (v[p] == ' ' || v[p] == '\t' || v[p] == '\n' || v[p] == '\r' || v[p] == ',')) ++p;
    if (p >= len) { *pos = p; return 0; }
    st = p;
    while (p < len && v[p] != ' ' && v[p] != '\t' && v[p] != '\n' && v[p] != '\r' && v[p] != ',') ++p;
    *tok = v + st;
    *tok_len = p - st;
    *pos = p;
    return 1;
}

static int cv_parse_fx_text(const char *s, x3d_fx *out)
{
    if (!s || !out) return X3D_STATUS_BAD_ARG;
    return x3d_fx_parse(s, (int)strlen(s), out);
}

static int cv_parse_vec3_text(const char *v, x3d_vec3 *out)
{
    const char *tok;
    int len;
    int pos;
    x3d_fx vals[3];
    int i;
    if (!v || !out) return X3D_STATUS_BAD_ARG;
    pos = 0;
    i = 0;
    while (i < 3 && cv_next_value_token(v, (int)strlen(v), &pos, &tok, &len)) {
        if (x3d_fx_parse(tok, len, &vals[i]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        ++i;
    }
    if (i != 3) return X3D_STATUS_NUMBER;
    out->x = vals[0]; out->y = vals[1]; out->z = vals[2];
    return X3D_STATUS_OK;
}

static int cv_parse_rot_text(const char *v, x3d_rot *out)
{
    const char *tok;
    int len;
    int pos;
    x3d_fx vals[4];
    int i;
    if (!v || !out) return X3D_STATUS_BAD_ARG;
    pos = 0;
    i = 0;
    while (i < 4 && cv_next_value_token(v, (int)strlen(v), &pos, &tok, &len)) {
        if (x3d_fx_parse(tok, len, &vals[i]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        ++i;
    }
    if (i != 4) return X3D_STATUS_NUMBER;
    out->x = vals[0]; out->y = vals[1]; out->z = vals[2]; out->angle = vals[3];
    return X3D_STATUS_OK;
}

static int cv_parse_color_text(const char *v, x3d_color *out)
{
    const char *tok;
    int len;
    int pos;
    x3d_fx vals[3];
    int i;
    if (!v || !out) return X3D_STATUS_BAD_ARG;
    pos = 0;
    i = 0;
    while (i < 3 && cv_next_value_token(v, (int)strlen(v), &pos, &tok, &len)) {
        if (x3d_fx_parse(tok, len, &vals[i]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        ++i;
    }
    if (i != 3) return X3D_STATUS_NUMBER;
    out->r = vals[0]; out->g = vals[1]; out->b = vals[2]; out->a = X3D_ONE;
    return X3D_STATUS_OK;
}

static int cv_pool_fx_list(x3d_scene *s, const char *v, int *start_out, int *count_out)
{
    const char *tok;
    int len;
    int pos;
    x3d_fx fx;
    int start;
    int count;
    if (!s || !v || !start_out || !count_out) return X3D_STATUS_BAD_ARG;
    start = s->fx_count;
    count = 0;
    pos = 0;
    while (cv_next_value_token(v, (int)strlen(v), &pos, &tok, &len)) {
        if (x3d_fx_parse(tok, len, &fx) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        if (s->fx_count >= X3D_MAX_FX) return X3D_STATUS_CAPACITY;
        s->fx_pool[s->fx_count] = fx;
        s->fx_count = s->fx_count + 1;
        count = count + 1;
    }
    *start_out = start; *count_out = count;
    return X3D_STATUS_OK;
}

static int cv_pool_int_list(x3d_scene *s, const char *v, int *start_out, int *count_out)
{
    const char *tok;
    int len;
    int pos;
    int iv;
    int start;
    int count;
    char tmp[CV_TOKEN_MAX];
    if (!s || !v || !start_out || !count_out) return X3D_STATUS_BAD_ARG;
    start = s->int_count;
    count = 0;
    pos = 0;
    while (cv_next_value_token(v, (int)strlen(v), &pos, &tok, &len)) {
        cv_copy_token(tmp, CV_TOKEN_MAX, tok, len);
        if (cv_eq(tmp, "TRUE") || cv_eq(tmp, "true")) iv = 1;
        else if (cv_eq(tmp, "FALSE") || cv_eq(tmp, "false")) iv = 0;
        else if (cv_parse_int_text(tmp, &iv) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        if (s->int_count >= X3D_MAX_INTS) return X3D_STATUS_CAPACITY;
        s->int_pool[s->int_count] = iv;
        s->int_count = s->int_count + 1;
        count = count + 1;
    }
    *start_out = start; *count_out = count;
    return X3D_STATUS_OK;
}

static int cv_pool_vec3_list(x3d_scene *s, const char *v, int *start_out, int *count_out)
{
    const char *tok;
    int len;
    int pos;
    x3d_fx vals[3];
    int k;
    int start;
    int count;
    if (!s || !v || !start_out || !count_out) return X3D_STATUS_BAD_ARG;
    start = s->vec3_count;
    count = 0;
    k = 0;
    pos = 0;
    while (cv_next_value_token(v, (int)strlen(v), &pos, &tok, &len)) {
        if (x3d_fx_parse(tok, len, &vals[k]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        k = k + 1;
        if (k == 3) {
            if (s->vec3_count >= X3D_MAX_VEC3) return X3D_STATUS_CAPACITY;
            s->vec3_pool[s->vec3_count].x = vals[0];
            s->vec3_pool[s->vec3_count].y = vals[1];
            s->vec3_pool[s->vec3_count].z = vals[2];
            s->vec3_count = s->vec3_count + 1;
            count = count + 1;
            k = 0;
        }
    }
    if (k != 0) return X3D_STATUS_NUMBER;
    *start_out = start; *count_out = count;
    return X3D_STATUS_OK;
}

static int cv_pool_vec2_list(x3d_scene *s, const char *v, int *start_out, int *count_out)
{
    const char *tok;
    int len;
    int pos;
    x3d_fx vals[2];
    int k;
    int start;
    int count;
    if (!s || !v || !start_out || !count_out) return X3D_STATUS_BAD_ARG;
    start = s->vec2_count;
    count = 0;
    k = 0;
    pos = 0;
    while (cv_next_value_token(v, (int)strlen(v), &pos, &tok, &len)) {
        if (x3d_fx_parse(tok, len, &vals[k]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        k = k + 1;
        if (k == 2) {
            if (s->vec2_count >= X3D_MAX_VEC2) return X3D_STATUS_CAPACITY;
            s->vec2_pool[s->vec2_count].x = vals[0];
            s->vec2_pool[s->vec2_count].y = vals[1];
            s->vec2_count = s->vec2_count + 1;
            count = count + 1;
            k = 0;
        }
    }
    if (k != 0) return X3D_STATUS_NUMBER;
    *start_out = start; *count_out = count;
    return X3D_STATUS_OK;
}

static int cv_pool_color_list(x3d_scene *s, const char *v, int *start_out, int *count_out)
{
    const char *tok;
    int len;
    int pos;
    x3d_fx vals[3];
    int k;
    int start;
    int count;
    if (!s || !v || !start_out || !count_out) return X3D_STATUS_BAD_ARG;
    start = s->color_count;
    count = 0;
    k = 0;
    pos = 0;
    while (cv_next_value_token(v, (int)strlen(v), &pos, &tok, &len)) {
        if (x3d_fx_parse(tok, len, &vals[k]) != X3D_STATUS_OK) return X3D_STATUS_NUMBER;
        k = k + 1;
        if (k == 3) {
            if (s->color_count >= X3D_MAX_COLOR) return X3D_STATUS_CAPACITY;
            s->color_pool[s->color_count].r = vals[0];
            s->color_pool[s->color_count].g = vals[1];
            s->color_pool[s->color_count].b = vals[2];
            s->color_pool[s->color_count].a = X3D_ONE;
            s->color_count = s->color_count + 1;
            count = count + 1;
            k = 0;
        }
    }
    if (k != 0) return X3D_STATUS_NUMBER;
    *start_out = start; *count_out = count;
    return X3D_STATUS_OK;
}

static void cv_link_child(x3d_scene *s, int child)
{
    int p;
    int pt;
    int ct;
    if (!s || child < 0 || child >= s->node_count) return;
    p = s->nodes[child].parent;
    if (p < 0 || p >= s->node_count) return;
    pt = s->nodes[p].type;
    ct = s->nodes[child].type;
    if (pt == X3D_NODE_INDEXED_FACE_SET) {
        if (ct == X3D_NODE_COORDINATE || ct == X3D_NODE_GEO_COORDINATE) s->ifs_nodes[p].coord_node = child;
        if (ct == X3D_NODE_NORMAL) s->ifs_nodes[p].normal_node = child;
        if (ct == X3D_NODE_TEXTURE_COORDINATE) s->ifs_nodes[p].texcoord_node = child;
        if (ct == X3D_NODE_COLOR) s->ifs_nodes[p].color_node = child;
    }
    if (pt == X3D_NODE_INDEXED_TRIANGLE_SET || pt == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET ||
        pt == X3D_NODE_INDEXED_TRIANGLE_FAN_SET || pt == X3D_NODE_TRIANGLE_SET ||
        pt == X3D_NODE_TRIANGLE_STRIP_SET || pt == X3D_NODE_TRIANGLE_FAN_SET ||
        pt == X3D_NODE_INDEXED_LINE_SET || pt == X3D_NODE_LINE_SET || pt == X3D_NODE_POINT_SET) {
        if (ct == X3D_NODE_COORDINATE) s->indexed_geoms[p].coord_node = child;
        if (ct == X3D_NODE_NORMAL) s->indexed_geoms[p].normal_node = child;
        if (ct == X3D_NODE_TEXTURE_COORDINATE) s->indexed_geoms[p].texcoord_node = child;
        if (ct == X3D_NODE_COLOR) s->indexed_geoms[p].color_node = child;
    }
    if (pt == X3D_NODE_NURBS_CURVE || pt == X3D_NODE_NURBS_PATCH_SURFACE || pt == X3D_NODE_NURBS_SET || pt == X3D_NODE_NURBS_TRIMMED_SURFACE) {
        if (ct == X3D_NODE_COORDINATE || ct == X3D_NODE_COORDINATE_DOUBLE) s->nurbs_nodes[p].control_point_node = child;
    }
}


static int cv_known_data_field(int type, const char *field)
{
    if (!field) return 0;
    if (type == X3D_NODE_TRANSFORM) {
        return cv_eq(field, "translation") || cv_eq(field, "scale") || cv_eq(field, "center") || cv_eq(field, "rotation") || cv_eq(field, "scaleOrientation") || cv_eq(field, "bboxCenter") || cv_eq(field, "bboxSize");
    }
    if (type == X3D_NODE_MATERIAL) {
        return cv_eq(field, "diffuseColor") || cv_eq(field, "emissiveColor") || cv_eq(field, "specularColor") || cv_eq(field, "ambientIntensity") || cv_eq(field, "shininess") || cv_eq(field, "transparency");
    }
    if (type == X3D_NODE_PHYSICAL_MATERIAL || type == X3D_NODE_UNLIT_MATERIAL) {
        return cv_eq(field, "baseColor") || cv_eq(field, "emissiveColor") || cv_eq(field, "metallic") || cv_eq(field, "roughness") || cv_eq(field, "transparency") || cv_eq(field, "normalScale");
    }
    if (type == X3D_NODE_IMAGE_TEXTURE || type == X3D_NODE_INLINE) return cv_eq(field, "url");
    if (type == X3D_NODE_INDEXED_FACE_SET) {
        return cv_eq(field, "coordIndex") || cv_eq(field, "normalIndex") || cv_eq(field, "texCoordIndex") || cv_eq(field, "colorIndex") || cv_eq(field, "ccw") || cv_eq(field, "solid") || cv_eq(field, "convex") || cv_eq(field, "creaseAngle");
    }
    if (type == X3D_NODE_INDEXED_TRIANGLE_SET || type == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET || type == X3D_NODE_INDEXED_TRIANGLE_FAN_SET) return cv_eq(field, "index");
    if (type == X3D_NODE_COORDINATE || type == X3D_NODE_NORMAL || type == X3D_NODE_GEO_COORDINATE) return cv_eq(field, "point") || cv_eq(field, "vector");
    if (type == X3D_NODE_TEXTURE_COORDINATE) return cv_eq(field, "point");
    if (type == X3D_NODE_COLOR) return cv_eq(field, "color");
    if (type == X3D_NODE_BOX) return cv_eq(field, "size") || cv_eq(field, "solid");
    if (type == X3D_NODE_SPHERE) return cv_eq(field, "radius") || cv_eq(field, "solid");
    if (type == X3D_NODE_CONE) return cv_eq(field, "bottomRadius") || cv_eq(field, "height") || cv_eq(field, "bottom") || cv_eq(field, "side");
    if (type == X3D_NODE_CYLINDER) return cv_eq(field, "radius") || cv_eq(field, "height");
    if (type == X3D_NODE_TIME_SENSOR) return cv_eq(field, "cycleInterval") || cv_eq(field, "startTime") || cv_eq(field, "stopTime") || cv_eq(field, "loop") || cv_eq(field, "enabled");
    if (type == X3D_NODE_TOUCH_SENSOR || type == X3D_NODE_PROXIMITY_SENSOR || type == X3D_NODE_VISIBILITY_SENSOR) return cv_eq(field, "enabled") || cv_eq(field, "center") || cv_eq(field, "size") || cv_eq(field, "description");
    if (type == X3D_NODE_POSITION_INTERPOLATOR || type == X3D_NODE_SCALAR_INTERPOLATOR) return cv_eq(field, "key") || cv_eq(field, "keyValue");
    return 0;
}

static void cv_apply_field(x3d_scene *s, int node, const char *field, const char *value)
{
    int type;
    int st;
    int ct;
    if (!s || node < 0 || node >= s->node_count || !field || !value) return;
    type = s->nodes[node].type;
    if (!cv_known_data_field(type, field)) x3d__add_raw_attr(s, node, field, value);
    if (type == X3D_NODE_TRANSFORM) {
        if (cv_eq(field, "translation")) cv_parse_vec3_text(value, &s->transforms[node].translation);
        else if (cv_eq(field, "scale")) cv_parse_vec3_text(value, &s->transforms[node].scale);
        else if (cv_eq(field, "center")) cv_parse_vec3_text(value, &s->transforms[node].center);
        else if (cv_eq(field, "rotation")) cv_parse_rot_text(value, &s->transforms[node].rotation);
        else if (cv_eq(field, "scaleOrientation")) cv_parse_rot_text(value, &s->transforms[node].scale_orientation);
        else if (cv_eq(field, "bboxCenter")) cv_parse_vec3_text(value, &s->transforms[node].bbox_center);
        else if (cv_eq(field, "bboxSize")) cv_parse_vec3_text(value, &s->transforms[node].bbox_size);
    } else if (type == X3D_NODE_MATERIAL) {
        if (cv_eq(field, "diffuseColor")) cv_parse_color_text(value, &s->materials[node].diffuse_color);
        else if (cv_eq(field, "emissiveColor")) cv_parse_color_text(value, &s->materials[node].emissive_color);
        else if (cv_eq(field, "specularColor")) cv_parse_color_text(value, &s->materials[node].specular_color);
        else if (cv_eq(field, "ambientIntensity")) cv_parse_fx_text(value, &s->materials[node].ambient_intensity);
        else if (cv_eq(field, "shininess")) cv_parse_fx_text(value, &s->materials[node].shininess);
        else if (cv_eq(field, "transparency")) cv_parse_fx_text(value, &s->materials[node].transparency);
    } else if (type == X3D_NODE_PHYSICAL_MATERIAL || type == X3D_NODE_UNLIT_MATERIAL) {
        if (cv_eq(field, "baseColor")) cv_parse_color_text(value, &s->pbr_materials[node].base_color);
        else if (cv_eq(field, "emissiveColor")) cv_parse_color_text(value, &s->pbr_materials[node].emissive_color);
        else if (cv_eq(field, "metallic")) cv_parse_fx_text(value, &s->pbr_materials[node].metallic);
        else if (cv_eq(field, "roughness")) cv_parse_fx_text(value, &s->pbr_materials[node].roughness);
        else if (cv_eq(field, "transparency")) cv_parse_fx_text(value, &s->pbr_materials[node].transparency);
        else if (cv_eq(field, "normalScale")) cv_parse_fx_text(value, &s->pbr_materials[node].normal_scale);
    } else if (type == X3D_NODE_IMAGE_TEXTURE || type == X3D_NODE_INLINE) {
        if (cv_eq(field, "url")) x3d__copy_attr_text(s->image_textures[node].url, X3D_VALUE_MAX, value, (int)strlen(value));
    } else if (type == X3D_NODE_INDEXED_FACE_SET) {
        if (cv_eq(field, "coordIndex")) { if (cv_pool_int_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->ifs_nodes[node].coord_index_start = st; s->ifs_nodes[node].coord_index_count = ct; } }
        else if (cv_eq(field, "normalIndex")) { if (cv_pool_int_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->ifs_nodes[node].normal_index_start = st; s->ifs_nodes[node].normal_index_count = ct; } }
        else if (cv_eq(field, "texCoordIndex")) { if (cv_pool_int_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->ifs_nodes[node].texcoord_index_start = st; s->ifs_nodes[node].texcoord_index_count = ct; } }
        else if (cv_eq(field, "colorIndex")) { if (cv_pool_int_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->ifs_nodes[node].color_index_start = st; s->ifs_nodes[node].color_index_count = ct; } }
        else if (cv_eq(field, "ccw")) cv_parse_bool_text(value, &s->ifs_nodes[node].ccw);
        else if (cv_eq(field, "solid")) cv_parse_bool_text(value, &s->ifs_nodes[node].solid);
        else if (cv_eq(field, "convex")) cv_parse_bool_text(value, &s->ifs_nodes[node].convex);
        else if (cv_eq(field, "creaseAngle")) cv_parse_fx_text(value, &s->ifs_nodes[node].crease_angle);
    } else if (type == X3D_NODE_INDEXED_TRIANGLE_SET || type == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET || type == X3D_NODE_INDEXED_TRIANGLE_FAN_SET) {
        if (cv_eq(field, "index")) { if (cv_pool_int_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->indexed_geoms[node].index_start = st; s->indexed_geoms[node].index_count = ct; } }
    } else if (type == X3D_NODE_COORDINATE || type == X3D_NODE_NORMAL || type == X3D_NODE_GEO_COORDINATE) {
        if (cv_eq(field, "point") || cv_eq(field, "vector")) { if (cv_pool_vec3_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->coord_refs[node].start = st; s->coord_refs[node].count = ct; } }
    } else if (type == X3D_NODE_TEXTURE_COORDINATE) {
        if (cv_eq(field, "point")) { if (cv_pool_vec2_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->texcoord_refs[node].start = st; s->texcoord_refs[node].count = ct; } }
    } else if (type == X3D_NODE_COLOR) {
        if (cv_eq(field, "color")) { if (cv_pool_color_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->color_refs[node].start = st; s->color_refs[node].count = ct; } }
    } else if (type == X3D_NODE_BOX) {
        if (cv_eq(field, "size")) cv_parse_vec3_text(value, &s->boxes[node].size);
        else if (cv_eq(field, "solid")) cv_parse_bool_text(value, &s->boxes[node].solid);
    } else if (type == X3D_NODE_SPHERE) {
        if (cv_eq(field, "radius")) cv_parse_fx_text(value, &s->spheres[node].radius);
        else if (cv_eq(field, "solid")) cv_parse_bool_text(value, &s->spheres[node].solid);
    } else if (type == X3D_NODE_CONE) {
        if (cv_eq(field, "bottomRadius")) cv_parse_fx_text(value, &s->cones[node].bottom_radius);
        else if (cv_eq(field, "height")) cv_parse_fx_text(value, &s->cones[node].height);
        else if (cv_eq(field, "bottom")) cv_parse_bool_text(value, &s->cones[node].bottom);
        else if (cv_eq(field, "side")) cv_parse_bool_text(value, &s->cones[node].side);
    } else if (type == X3D_NODE_CYLINDER) {
        if (cv_eq(field, "radius")) cv_parse_fx_text(value, &s->cylinders[node].radius);
        else if (cv_eq(field, "height")) cv_parse_fx_text(value, &s->cylinders[node].height);
    } else if (type == X3D_NODE_TIME_SENSOR) {
        if (cv_eq(field, "cycleInterval")) cv_parse_fx_text(value, &s->time_sensors[node].cycle_interval);
        else if (cv_eq(field, "startTime")) cv_parse_fx_text(value, &s->time_sensors[node].start_time);
        else if (cv_eq(field, "stopTime")) cv_parse_fx_text(value, &s->time_sensors[node].stop_time);
        else if (cv_eq(field, "loop")) cv_parse_bool_text(value, &s->time_sensors[node].loop);
        else if (cv_eq(field, "enabled")) cv_parse_bool_text(value, &s->time_sensors[node].enabled);
    } else if (type == X3D_NODE_TOUCH_SENSOR || type == X3D_NODE_PROXIMITY_SENSOR || type == X3D_NODE_VISIBILITY_SENSOR) {
        if (cv_eq(field, "enabled")) cv_parse_bool_text(value, &s->sensors[node].enabled);
        else if (cv_eq(field, "center")) cv_parse_vec3_text(value, &s->sensors[node].center);
        else if (cv_eq(field, "size")) cv_parse_vec3_text(value, &s->sensors[node].size);
        else if (cv_eq(field, "description")) x3d__copy_attr_text(s->sensors[node].description, X3D_VALUE_MAX, value, (int)strlen(value));
    } else if (type == X3D_NODE_POSITION_INTERPOLATOR) {
        if (cv_eq(field, "key")) { if (cv_pool_fx_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->interpolators[node].key_start = st; s->interpolators[node].key_count = ct; } }
        else if (cv_eq(field, "keyValue")) { if (cv_pool_vec3_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->interpolators[node].value_start = st; s->interpolators[node].value_count = ct; } }
    } else if (type == X3D_NODE_SCALAR_INTERPOLATOR) {
        if (cv_eq(field, "key")) { if (cv_pool_fx_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->interpolators[node].key_start = st; s->interpolators[node].key_count = ct; } }
        else if (cv_eq(field, "keyValue")) { if (cv_pool_fx_list(s, value, &st, &ct) == X3D_STATUS_OK) { s->interpolators[node].value_start = st; s->interpolators[node].value_count = ct; } }
    }
}

static int cv_field_arity(const char *field)
{
    if (!field) return 1;
    if (cv_eq(field, "translation") || cv_eq(field, "scale") || cv_eq(field, "center") ||
        cv_eq(field, "bboxCenter") || cv_eq(field, "bboxSize") || cv_eq(field, "size") ||
        cv_eq(field, "diffuseColor") || cv_eq(field, "emissiveColor") || cv_eq(field, "specularColor") ||
        cv_eq(field, "baseColor") || cv_eq(field, "direction") || cv_eq(field, "location") ||
        cv_eq(field, "position") || cv_eq(field, "linearVelocity") || cv_eq(field, "gravity")) return 3;
    if (cv_eq(field, "rotation") || cv_eq(field, "orientation") || cv_eq(field, "scaleOrientation") || cv_eq(field, "plane")) return 4;
    if (cv_eq(field, "frictionCoefficients") || cv_eq(field, "slipFactors")) return 2;
    return 1;
}

static int cv_token_is_child_start(cv_parse *p, const cv_tok *t)
{
    cv_tok a;
    cv_parse save;
    if (!t || t->kind != CV_T_WORD) return 0;
    if (cv_eq(t->text, "DEF") || cv_eq(t->text, "USE") || cv_eq(t->text, "ROUTE")) return 1;
    if (x3d_node_type_from_name(t->text) != X3D_NODE_UNKNOWN) return 1;
    save = *p;
    if (cv_next(p, &a) && a.kind == CV_T_SYM && a.text[0] == '{') {
        *p = save;
        return 1;
    }
    *p = save;
    return 0;
}

static void cv_append_value(char *dst, int cap, const char *src)
{
    int d;
    int i;
    if (!dst || cap <= 0 || !src) return;
    d = (int)strlen(dst);
    if (d > 0 && d + 1 < cap) {
        dst[d] = ' ';
        d = d + 1;
        dst[d] = 0;
    }
    i = 0;
    while (d + 1 < cap && src[i]) {
        dst[d] = src[i];
        ++d;
        ++i;
    }
    dst[d] = 0;
}

static int cv_parse_node(cv_parse *p, x3d_scene *s, cv_defs *defs, int parent, const char *container, x3d_diag *diag);

static int cv_parse_field_or_child(cv_parse *p, x3d_scene *s, cv_defs *defs, int node, x3d_diag *diag)
{
    cv_tok field;
    cv_tok t;
    cv_parse save;
    char value[X3D_VALUE_MAX];
    int n;
    int ar;
    int got;
    int depth;
    if (!cv_next(p, &field)) return X3D_STATUS_XML;
    if (field.kind == CV_T_SYM && field.text[0] == '}') return 1;
    if (cv_token_is_child_start(p, &field)) {
        save = *p;
        *p = save;
        p->p = p->p - (int)strlen(field.text);
        return X3D_STATUS_XML;
    }
    if (!cv_next(p, &t)) return X3D_STATUS_XML;
    if (t.kind == CV_T_SYM && t.text[0] == '[') {
        value[0] = 0;
        if (cv_peek(p, &t) && cv_token_is_child_start(p, &t)) {
            while (cv_peek(p, &t) && !(t.kind == CV_T_SYM && t.text[0] == ']')) {
                n = cv_parse_node(p, s, defs, node, field.text, diag);
                if (n < 0) return n;
                cv_link_child(s, n);
            }
            cv_take_sym(p, ']');
        } else {
            depth = 1;
            while (depth > 0 && cv_next(p, &t)) {
                if (t.kind == CV_T_SYM && t.text[0] == '[') { depth = depth + 1; cv_append_value(value, X3D_VALUE_MAX, t.text); }
                else if (t.kind == CV_T_SYM && t.text[0] == ']') { depth = depth - 1; if (depth > 0) cv_append_value(value, X3D_VALUE_MAX, t.text); }
                else if (!(t.kind == CV_T_SYM && t.text[0] == ',')) cv_append_value(value, X3D_VALUE_MAX, t.text);
            }
            cv_apply_field(s, node, field.text, value);
        }
        return X3D_STATUS_OK;
    }
    if (cv_token_is_child_start(p, &t)) {
        p->p = p->p - (int)strlen(t.text);
        n = cv_parse_node(p, s, defs, node, field.text, diag);
        if (n < 0) return n;
        cv_link_child(s, n);
        return X3D_STATUS_OK;
    }
    value[0] = 0;
    cv_append_value(value, X3D_VALUE_MAX, t.text);
    ar = cv_field_arity(field.text);
    got = 1;
    while (got < ar && cv_next(p, &t)) {
        if (t.kind == CV_T_SYM) return X3D_STATUS_XML;
        cv_append_value(value, X3D_VALUE_MAX, t.text);
        got = got + 1;
    }
    cv_apply_field(s, node, field.text, value);
    return X3D_STATUS_OK;
}

static int cv_parse_route(cv_parse *p, x3d_scene *s, int parent)
{
    cv_tok a;
    cv_tok to;
    cv_tok b;
    int idx;
    int dot;
    int node;
    char from_node[X3D_NAME_MAX];
    char from_field[X3D_NAME_MAX];
    char to_node[X3D_NAME_MAX];
    char to_field[X3D_NAME_MAX];
    if (!cv_next(p, &a)) return X3D_STATUS_XML;
    if (!cv_next(p, &to)) return X3D_STATUS_XML;
    if (!cv_next(p, &b)) return X3D_STATUS_XML;
    if (!cv_eq(to.text, "TO")) return X3D_STATUS_XML;
    from_node[0] = 0; from_field[0] = 0; to_node[0] = 0; to_field[0] = 0;
    dot = 0;
    while (a.text[dot] && a.text[dot] != '.') ++dot;
    cv_copy_token(from_node, X3D_NAME_MAX, a.text, dot);
    if (a.text[dot] == '.') x3d__copy_attr_text(from_field, X3D_NAME_MAX, a.text + dot + 1, (int)strlen(a.text + dot + 1));
    dot = 0;
    while (b.text[dot] && b.text[dot] != '.') ++dot;
    cv_copy_token(to_node, X3D_NAME_MAX, b.text, dot);
    if (b.text[dot] == '.') x3d__copy_attr_text(to_field, X3D_NAME_MAX, b.text + dot + 1, (int)strlen(b.text + dot + 1));
    node = x3d__add_node(s, parent, "ROUTE", a.line);
    if (node == X3D_NONE || s->route_count >= X3D_MAX_ROUTES) return X3D_STATUS_CAPACITY;
    idx = s->route_count;
    s->route_count = s->route_count + 1;
    s->routes[idx].node_index = node;
    x3d__copy_attr_text(s->routes[idx].from_node, X3D_NAME_MAX, from_node, (int)strlen(from_node));
    x3d__copy_attr_text(s->routes[idx].from_field, X3D_NAME_MAX, from_field, (int)strlen(from_field));
    x3d__copy_attr_text(s->routes[idx].to_node, X3D_NAME_MAX, to_node, (int)strlen(to_node));
    x3d__copy_attr_text(s->routes[idx].to_field, X3D_NAME_MAX, to_field, (int)strlen(to_field));
    return node;
}

static int cv_parse_node(cv_parse *p, x3d_scene *s, cv_defs *defs, int parent, const char *container, x3d_diag *diag)
{
    cv_tok t;
    cv_tok name;
    cv_tok dname;
    const char *utype;
    int node;
    int rc;
    char def_name[X3D_NAME_MAX];
    def_name[0] = 0;
    if (!cv_next(p, &t)) return X3D_STATUS_XML;
    if (cv_eq(t.text, "ROUTE")) return cv_parse_route(p, s, parent);
    if (cv_eq(t.text, "DEF")) {
        if (!cv_next(p, &dname)) return X3D_STATUS_XML;
        x3d__copy_attr_text(def_name, X3D_NAME_MAX, dname.text, (int)strlen(dname.text));
        if (!cv_next(p, &name)) return X3D_STATUS_XML;
    } else if (cv_eq(t.text, "USE")) {
        if (!cv_next(p, &dname)) return X3D_STATUS_XML;
        utype = cv_def_type(defs, dname.text);
        if (!utype) utype = "Group";
        node = x3d__add_node(s, parent, utype, dname.line);
        if (node == X3D_NONE) return X3D_STATUS_CAPACITY;
        x3d__copy_attr_text(s->nodes[node].use_name, X3D_NAME_MAX, dname.text, (int)strlen(dname.text));
        if (container && container[0]) x3d__copy_attr_text(s->nodes[node].container_field, X3D_NAME_MAX, container, (int)strlen(container));
        return node;
    } else {
        name = t;
    }
    node = x3d__add_node(s, parent, name.text, name.line);
    if (node == X3D_NONE) return X3D_STATUS_CAPACITY;
    if (def_name[0]) {
        x3d__copy_attr_text(s->nodes[node].def_name, X3D_NAME_MAX, def_name, (int)strlen(def_name));
        cv_def_add(defs, def_name, name.text);
    }
    if (container && container[0]) x3d__copy_attr_text(s->nodes[node].container_field, X3D_NAME_MAX, container, (int)strlen(container));
    if (!cv_take_sym(p, '{')) return node;
    while (cv_peek(p, &t)) {
        if (t.kind == CV_T_SYM && t.text[0] == '}') { cv_next(p, &t); break; }
        if (cv_token_is_child_start(p, &t)) {
            rc = cv_parse_node(p, s, defs, node, "", diag);
            if (rc < 0) return rc;
            cv_link_child(s, rc);
        } else {
            rc = cv_parse_field_or_child(p, s, defs, node, diag);
            if (rc == 1) break;
            if (rc != X3D_STATUS_OK) return rc;
        }
    }
    cv_link_child(s, node);
    return node;
}

static void cv_read_header_version(const char *text, int len, char *version, int cap)
{
    int i;
    int j;
    version[0] = 0;
    if (!text || len <= 0) return;
    i = 0;
    while (i < len && text[i] != 'V') ++i;
    if (i >= len) return;
    ++i;
    j = 0;
    while (i < len && j + 1 < cap && text[i] && !cv_is_space(text[i])) {
        version[j] = text[i];
        ++i;
        ++j;
    }
    version[j] = 0;
}

int x3d_parse_classic_vrml(x3d_scene *scene, const char *text, int text_len, x3d_diag *diag)
{
    cv_parse p;
    cv_tok t;
    cv_tok v;
    cv_defs defs;
    int rc;
    int scene_node;
    int level;
    char version[16];
    if (!scene || !text || text_len < 0) {
        cv_diag(diag, X3D_STATUS_BAD_ARG, 1, "bad ClassicVRML parse argument");
        return X3D_STATUS_BAD_ARG;
    }
    x3d_scene_init(scene);
    if (diag) { diag->code = X3D_STATUS_OK; diag->line = 1; diag->column = 1; diag->node_index = X3D_NONE; diag->warnings = 0; diag->message[0] = 0; }
    cv_read_header_version(text, text_len, version, (int)sizeof(version));
    if (version[0]) x3d__copy_attr_text(scene->version, (int)sizeof(scene->version), version, (int)strlen(version));
    defs.count = 0;
    p.p = text;
    p.end = text + text_len;
    p.line = 1;
    scene_node = x3d__add_node(scene, X3D_NONE, "Scene", 1);
    if (scene_node == X3D_NONE) return X3D_STATUS_CAPACITY;
    while (cv_peek(&p, &t)) {
        if (cv_eq(t.text, "PROFILE")) {
            cv_next(&p, &t); if (cv_next(&p, &v)) x3d__copy_attr_text(scene->profile, X3D_NAME_MAX, v.text, (int)strlen(v.text));
        } else if (cv_eq(t.text, "COMPONENT")) {
            cv_next(&p, &t); if (cv_next(&p, &v) && scene->component_count < X3D_MAX_COMPONENTS) {
                int k; int n;
                k = 0; while (v.text[k] && v.text[k] != ':') ++k;
                n = scene->component_count; scene->component_count = scene->component_count + 1;
                cv_copy_token(scene->components[n].name, X3D_NAME_MAX, v.text, k);
                scene->components[n].level = 1;
                if (v.text[k] == ':') { cv_parse_int_text(v.text + k + 1, &level); scene->components[n].level = level; }
            }
        } else if (cv_eq(t.text, "META")) {
            cv_next(&p, &t);
            if (scene->meta_count < X3D_MAX_META && cv_next(&p, &t) && cv_next(&p, &v)) {
                int n;
                n = scene->meta_count; scene->meta_count = scene->meta_count + 1;
                x3d__copy_attr_text(scene->metas[n].name, X3D_NAME_MAX, t.text, (int)strlen(t.text));
                x3d__copy_attr_text(scene->metas[n].content, X3D_VALUE_MAX, v.text, (int)strlen(v.text));
            }
        } else if (cv_eq(t.text, "UNIT")) {
            cv_next(&p, &t);
            if (scene->unit_count < X3D_MAX_UNITS && cv_next(&p, &t) && cv_next(&p, &v)) {
                int n;
                n = scene->unit_count; scene->unit_count = scene->unit_count + 1;
                x3d__copy_attr_text(scene->units[n].category, X3D_NAME_MAX, t.text, (int)strlen(t.text));
                x3d__copy_attr_text(scene->units[n].name, X3D_NAME_MAX, v.text, (int)strlen(v.text));
                scene->units[n].conversion_factor = X3D_ONE;
                if (cv_next(&p, &v)) cv_parse_fx_text(v.text, &scene->units[n].conversion_factor);
            }
        } else if (cv_eq(t.text, "Scene")) {
            cv_next(&p, &t);
            cv_take_sym(&p, '{');
            while (cv_peek(&p, &t)) {
                if (t.kind == CV_T_SYM && t.text[0] == '}') { cv_next(&p, &t); break; }
                rc = cv_parse_node(&p, scene, &defs, scene_node, "", diag);
                if (rc < 0) { cv_diag(diag, rc, p.line, "ClassicVRML scene parse failed"); return rc; }
            }
        } else {
            rc = cv_parse_node(&p, scene, &defs, scene_node, "", diag);
            if (rc < 0) { cv_diag(diag, rc, p.line, "ClassicVRML node parse failed"); return rc; }
        }
    }
    if (diag) {
        diag->code = scene->unsupported_node_count > 0 ? X3D_STATUS_WARNINGS : X3D_STATUS_OK;
        diag->warnings = scene->unsupported_node_count;
        if (diag->warnings > 0) x3d__copy_attr_text(diag->message, (int)sizeof(diag->message), "ClassicVRML parsed with preserved nodes", 40);
    }
    return X3D_STATUS_OK;
}

int x3d_parse_any(x3d_scene *scene, const unsigned char *data, int len, x3d_diag *diag)
{
    int enc;
    if (!scene || !data || len < 0) return X3D_STATUS_BAD_ARG;
    enc = x3d_guess_encoding(data, len);
    if (enc == X3D_ENCODING_XML) return x3d_parse_xml(scene, (const char *)data, len, diag);
    if (enc == X3D_ENCODING_CLASSIC_VRML) return x3d_parse_classic_vrml(scene, (const char *)data, len, diag);
    if (enc == X3D_ENCODING_JSON) return x3d_parse_json(scene, (const char *)data, len, diag);
    cv_diag(diag, X3D_STATUS_XML, 1, "encoding detected but parser is not linked for it");
    return X3D_STATUS_XML;
}

static int cb_put(cv_buf *b, const char *s)
{
    int i;
    if (!b || !s) return X3D_STATUS_BAD_ARG;
    i = 0;
    while (s[i]) {
        if (b->len + 1 >= b->cap) return X3D_STATUS_OUTPUT_FULL;
        b->out[b->len] = s[i];
        b->len = b->len + 1;
        ++i;
    }
    b->out[b->len] = 0;
    return X3D_STATUS_OK;
}

static int cb_put_fx(cv_buf *b, x3d_fx v)
{
    char tmp[48];
    x3d_fx_to_text(v, tmp, (int)sizeof(tmp));
    return cb_put(b, tmp);
}

static int cb_indent(cv_buf *b, int depth)
{
    int i;
    for (i = 0; i < depth; ++i) {
        if (cb_put(b, "  ") != X3D_STATUS_OK) return X3D_STATUS_OUTPUT_FULL;
    }
    return X3D_STATUS_OK;
}

static int cb_put_vec3_field(cv_buf *b, const char *name, x3d_vec3 v, int depth)
{
    if (cb_indent(b, depth) != X3D_STATUS_OK) return X3D_STATUS_OUTPUT_FULL;
    cb_put(b, name); cb_put(b, " "); cb_put_fx(b, v.x); cb_put(b, " "); cb_put_fx(b, v.y); cb_put(b, " "); cb_put_fx(b, v.z); cb_put(b, "\n");
    return X3D_STATUS_OK;
}

static int cb_put_color_field(cv_buf *b, const char *name, x3d_color v, int depth)
{
    if (cb_indent(b, depth) != X3D_STATUS_OK) return X3D_STATUS_OUTPUT_FULL;
    cb_put(b, name); cb_put(b, " "); cb_put_fx(b, v.r); cb_put(b, " "); cb_put_fx(b, v.g); cb_put(b, " "); cb_put_fx(b, v.b); cb_put(b, "\n");
    return X3D_STATUS_OK;
}

static int cb_put_int_list(cv_buf *b, const int *vals, int count)
{
    int i;
    char tmp[32];
    for (i = 0; i < count; ++i) {
        if (i > 0) cb_put(b, " ");
        sprintf(tmp, "%d", vals[i]);
        cb_put(b, tmp);
    }
    return X3D_STATUS_OK;
}


static int cb_put_fx_list(cv_buf *b, const x3d_fx *vals, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (i > 0) cb_put(b, " ");
        cb_put_fx(b, vals[i]);
    }
    return X3D_STATUS_OK;
}

static int cb_write_node(const x3d_scene *s, int node, cv_buf *b, int depth)
{
    int c;
    int t;
    const char *name;
    if (!s || node < 0 || node >= s->node_count || !b) return X3D_STATUS_BAD_ARG;
    if (s->nodes[node].type == X3D_NODE_ROUTE) return X3D_STATUS_OK;
    if (s->nodes[node].use_name[0]) {
        cb_indent(b, depth); cb_put(b, "USE "); cb_put(b, s->nodes[node].use_name); cb_put(b, "\n");
        return X3D_STATUS_OK;
    }
    name = s->nodes[node].name[0] ? s->nodes[node].name : x3d_node_type_name(s->nodes[node].type);
    cb_indent(b, depth);
    if (s->nodes[node].def_name[0]) { cb_put(b, "DEF "); cb_put(b, s->nodes[node].def_name); cb_put(b, " "); }
    cb_put(b, name); cb_put(b, " {\n");
    t = s->nodes[node].type;
    if (t == X3D_NODE_TRANSFORM) {
        if (s->transforms[node].translation.x || s->transforms[node].translation.y || s->transforms[node].translation.z) cb_put_vec3_field(b, "translation", s->transforms[node].translation, depth + 1);
    } else if (t == X3D_NODE_MATERIAL) {
        cb_put_color_field(b, "diffuseColor", s->materials[node].diffuse_color, depth + 1);
    } else if (t == X3D_NODE_BOX) {
        cb_put_vec3_field(b, "size", s->boxes[node].size, depth + 1);
    } else if (t == X3D_NODE_SPHERE) {
        cb_indent(b, depth + 1); cb_put(b, "radius "); cb_put_fx(b, s->spheres[node].radius); cb_put(b, "\n");
    } else if (t == X3D_NODE_INDEXED_FACE_SET) {
        if (s->ifs_nodes[node].coord_index_start >= 0 && s->ifs_nodes[node].coord_index_count > 0) {
            cb_indent(b, depth + 1); cb_put(b, "coordIndex [ "); cb_put_int_list(b, s->int_pool + s->ifs_nodes[node].coord_index_start, s->ifs_nodes[node].coord_index_count); cb_put(b, " ]\n");
        }
    } else if (t == X3D_NODE_COORDINATE) {
        if (s->coord_refs[node].start >= 0 && s->coord_refs[node].count > 0) {
            int i;
            cb_indent(b, depth + 1); cb_put(b, "point [ ");
            for (i = 0; i < s->coord_refs[node].count; ++i) {
                x3d_vec3 v;
                if (i > 0) cb_put(b, ", ");
                v = s->vec3_pool[s->coord_refs[node].start + i];
                cb_put_fx(b, v.x); cb_put(b, " "); cb_put_fx(b, v.y); cb_put(b, " "); cb_put_fx(b, v.z);
            }
            cb_put(b, " ]\n");
        }
    } else if (t == X3D_NODE_TIME_SENSOR) {
        cb_indent(b, depth + 1); cb_put(b, "cycleInterval "); cb_put_fx(b, s->time_sensors[node].cycle_interval); cb_put(b, "\n");
        cb_indent(b, depth + 1); cb_put(b, "loop "); cb_put(b, s->time_sensors[node].loop ? "TRUE" : "FALSE"); cb_put(b, "\n");
        cb_indent(b, depth + 1); cb_put(b, "startTime "); cb_put_fx(b, s->time_sensors[node].start_time); cb_put(b, "\n");
    } else if (t == X3D_NODE_SCALAR_INTERPOLATOR) {
        if (s->interpolators[node].key_start >= 0 && s->interpolators[node].key_count > 0) {
            cb_indent(b, depth + 1); cb_put(b, "key [ "); cb_put_fx_list(b, s->fx_pool + s->interpolators[node].key_start, s->interpolators[node].key_count); cb_put(b, " ]\n");
        }
        if (s->interpolators[node].value_start >= 0 && s->interpolators[node].value_count > 0) {
            cb_indent(b, depth + 1); cb_put(b, "keyValue [ "); cb_put_fx_list(b, s->fx_pool + s->interpolators[node].value_start, s->interpolators[node].value_count); cb_put(b, " ]\n");
        }
    }
    c = s->nodes[node].first_child;
    while (c != X3D_NONE) {
        cb_write_node(s, c, b, depth + 1);
        c = s->nodes[c].next_sibling;
    }
    cb_indent(b, depth); cb_put(b, "}\n");
    return X3D_STATUS_OK;
}

int x3d_write_classic_vrml(const x3d_scene *scene, char *out, int out_cap, int *out_len)
{
    cv_buf b;
    int i;
    int c;
    char tmp[32];
    if (!scene || !out || out_cap <= 0 || !out_len) return X3D_STATUS_BAD_ARG;
    b.out = out; b.cap = out_cap; b.len = 0; out[0] = 0;
    cb_put(&b, "#X3D V"); cb_put(&b, scene->version[0] ? scene->version : "4.0"); cb_put(&b, " utf8\n");
    cb_put(&b, "PROFILE "); cb_put(&b, scene->profile[0] ? scene->profile : "Interchange"); cb_put(&b, "\n");
    for (i = 0; i < scene->component_count; ++i) {
        cb_put(&b, "COMPONENT "); cb_put(&b, scene->components[i].name); cb_put(&b, ":"); sprintf(tmp, "%d", scene->components[i].level); cb_put(&b, tmp); cb_put(&b, "\n");
    }
    if (scene->scene_node >= 0) c = scene->nodes[scene->scene_node].first_child; else c = 0;
    while (c != X3D_NONE && c < scene->node_count) {
        cb_write_node(scene, c, &b, 0);
        c = scene->nodes[c].next_sibling;
    }
    for (i = 0; i < scene->route_count; ++i) {
        cb_put(&b, "ROUTE "); cb_put(&b, scene->routes[i].from_node); cb_put(&b, "."); cb_put(&b, scene->routes[i].from_field); cb_put(&b, " TO "); cb_put(&b, scene->routes[i].to_node); cb_put(&b, "."); cb_put(&b, scene->routes[i].to_field); cb_put(&b, "\n");
    }
    *out_len = b.len;
    return X3D_STATUS_OK;
}

static int v7_is_geom_type(int t)
{
    return t == X3D_NODE_INDEXED_FACE_SET || t == X3D_NODE_INDEXED_TRIANGLE_SET ||
        t == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET || t == X3D_NODE_INDEXED_TRIANGLE_FAN_SET ||
        t == X3D_NODE_TRIANGLE_SET || t == X3D_NODE_TRIANGLE_STRIP_SET || t == X3D_NODE_TRIANGLE_FAN_SET ||
        t == X3D_NODE_BOX || t == X3D_NODE_SPHERE || t == X3D_NODE_CONE || t == X3D_NODE_CYLINDER;
}

static int v7_find_child_geom(const x3d_scene *s, int node)
{
    int c;
    if (!s || node < 0) return X3D_NONE;
    c = s->nodes[node].first_child;
    while (c != X3D_NONE) {
        if (v7_is_geom_type(s->nodes[c].type)) return c;
        c = s->nodes[c].next_sibling;
    }
    return X3D_NONE;
}

static int v7_find_first_in_subtree(const x3d_scene *s, int node, int t0, int t1, int t2)
{
    int c;
    int r;
    if (!s || node < 0) return X3D_NONE;
    if (s->nodes[node].type == t0 || s->nodes[node].type == t1 || s->nodes[node].type == t2) return node;
    c = s->nodes[node].first_child;
    while (c != X3D_NONE) {
        r = v7_find_first_in_subtree(s, c, t0, t1, t2);
        if (r != X3D_NONE) return r;
        c = s->nodes[c].next_sibling;
    }
    return X3D_NONE;
}

int x3d_build_draw_list(const x3d_scene *scene, x3d_draw_list *out_list)
{
    int i;
    int shape;
    int geom;
    int app;
    int mat;
    x3d_diag diag;
    x3d_tri_mesh mesh;
    if (!scene || !out_list) return X3D_STATUS_BAD_ARG;
    out_list->count = 0;
    for (i = 0; i < X3D_MAX_DRAW_ITEMS; ++i) {
        out_list->items[i].shape_node = X3D_NONE;
        out_list->items[i].geometry_node = X3D_NONE;
        out_list->items[i].appearance_node = X3D_NONE;
        out_list->items[i].material_node = X3D_NONE;
        out_list->items[i].coord_node = X3D_NONE;
        out_list->items[i].tri_count = 0;
        out_list->items[i].bounds.valid = 0;
    }
    for (shape = 0; shape < scene->node_count; ++shape) {
        if (scene->nodes[shape].type == X3D_NODE_SHAPE) {
            geom = v7_find_child_geom(scene, shape);
            if (geom != X3D_NONE && out_list->count < X3D_MAX_DRAW_ITEMS) {
                x3d_draw_item *it;
                it = &out_list->items[out_list->count];
                out_list->count = out_list->count + 1;
                it->shape_node = shape;
                it->geometry_node = geom;
                app = v7_find_first_in_subtree(scene, shape, X3D_NODE_APPEARANCE, X3D_NODE_UNKNOWN, X3D_NODE_UNKNOWN);
                mat = v7_find_first_in_subtree(scene, shape, X3D_NODE_MATERIAL, X3D_NODE_PHYSICAL_MATERIAL, X3D_NODE_UNLIT_MATERIAL);
                it->appearance_node = app;
                it->material_node = mat;
                if (scene->nodes[geom].type == X3D_NODE_INDEXED_FACE_SET) it->coord_node = scene->ifs_nodes[geom].coord_node;
                else it->coord_node = scene->indexed_geoms[geom].coord_node;
                if (x3d_triangulate_node(scene, geom, &mesh, &diag) == X3D_STATUS_OK) it->tri_count = mesh.tri_count;
                if (it->coord_node >= 0 && scene->coord_refs[it->coord_node].count > 0) {
                    int k;
                    int st;
                    x3d_vec3 v;
                    st = scene->coord_refs[it->coord_node].start;
                    v = scene->vec3_pool[st];
                    it->bounds.valid = 1;
                    it->bounds.min = v;
                    it->bounds.max = v;
                    for (k = 1; k < scene->coord_refs[it->coord_node].count; ++k) {
                        v = scene->vec3_pool[st + k];
                        if (v.x < it->bounds.min.x) it->bounds.min.x = v.x;
                        if (v.y < it->bounds.min.y) it->bounds.min.y = v.y;
                        if (v.z < it->bounds.min.z) it->bounds.min.z = v.z;
                        if (v.x > it->bounds.max.x) it->bounds.max.x = v.x;
                        if (v.y > it->bounds.max.y) it->bounds.max.y = v.y;
                        if (v.z > it->bounds.max.z) it->bounds.max.z = v.z;
                    }
                }
            }
        }
    }
    return X3D_STATUS_OK;
}

int x3d_validate_proto_routes(const x3d_scene *scene, x3d_diag *diag)
{
    int i;
    int a;
    int b;
    if (!scene) return X3D_STATUS_BAD_ARG;
    for (i = 0; i < scene->route_count; ++i) {
        if (x3d_resolve_route(scene, i, &a, &b) != X3D_STATUS_OK) {
            cv_diag(diag, X3D_STATUS_WARNINGS, 1, "unresolved route endpoint");
            if (diag) { diag->warnings = 1; diag->node_index = scene->routes[i].node_index; }
            return X3D_STATUS_WARNINGS;
        }
    }
    if (diag) { diag->code = X3D_STATUS_OK; diag->warnings = 0; diag->message[0] = 0; }
    return X3D_STATUS_OK;
}

void x3d_v7_collect_summary(const x3d_scene *scene, x3d_v7_summary *summary)
{
    int i;
    int defs;
    int uses;
    if (!summary) return;
    memset(summary, 0, sizeof(*summary));
    if (!scene) return;
    x3d_count_defs_uses(scene, &defs, &uses);
    summary->def_nodes = defs;
    summary->use_nodes = uses;
    summary->raw_attrs = scene->raw_attr_count;
    summary->components = scene->component_count;
    summary->metas = scene->meta_count;
    summary->units = scene->unit_count;
    summary->routes = scene->route_count;
    for (i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].type == X3D_NODE_PROTO_DECLARE || scene->nodes[i].type == X3D_NODE_PROTO_INSTANCE || scene->nodes[i].type == X3D_NODE_EXTERN_PROTO_DECLARE) summary->proto_related = summary->proto_related + 1;
        if (v7_is_geom_type(scene->nodes[i].type)) summary->drawable_geometries = summary->drawable_geometries + 1;
        if (scene->nodes[i].use_name[0]) {
            int def_node;
            if (x3d_resolve_use(scene, i, &def_node) != X3D_STATUS_OK) summary->unresolved_uses = summary->unresolved_uses + 1;
        }
    }
    summary->max_depth = 0;
    for (i = 0; i < scene->node_count; ++i) {
        int d;
        d = x3d_node_depth(scene, i);
        if (d > summary->max_depth) summary->max_depth = d;
    }
}
