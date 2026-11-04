#include "x3d_codec.h"
#include <string.h>
#include <stdio.h>

#ifndef X3D_JSON_XML_CAP
#define X3D_JSON_XML_CAP 131072
#endif

#define J8_KEY_MAX 96
#define J8_VAL_MAX 512
#define J8_OK 0
#define J8_FULL 1

typedef struct j8p_s {
    const char *p;
    const char *end;
    int line;
} j8p;

typedef struct j8b_s {
    char *out;
    int cap;
    int len;
    int err;
} j8b;

extern int x3d_parse_xml(x3d_scene *scene, const char *text, int text_len, x3d_diag *diag);
extern int x3d_triangulate_node(const x3d_scene *scene, int geom_node, x3d_tri_mesh *mesh, x3d_diag *diag);
extern int x3d_compute_coordinate_bounds(const x3d_scene *scene, x3d_bounds *bounds);
extern int x3d_build_draw_list(const x3d_scene *scene, x3d_draw_list *out_list);
extern int x3d_validate_proto_routes(const x3d_scene *scene, x3d_diag *diag);

static char j8_xml[X3D_JSON_XML_CAP];

static int j8_eq(const char *a, const char *b)
{
    int i;
    if (!a || !b) return 0;
    i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i = i + 1;
    }
    return a[i] == b[i];
}

static void j8_copy(char *dst, int cap, const char *src)
{
    int i;
    if (!dst || cap <= 0) return;
    i = 0;
    if (src) {
        while (i + 1 < cap && src[i]) {
            dst[i] = src[i];
            i = i + 1;
        }
    }
    dst[i] = 0;
}

static int j8_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void j8_ws(j8p *p)
{
    while (p && p->p < p->end && *p->p && j8_space(*p->p)) {
        if (*p->p == '\n') p->line = p->line + 1;
        p->p = p->p + 1;
    }
}

static void j8_diag(x3d_diag *d, int code, int line, const char *msg)
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
            i = i + 1;
        }
    }
    d->message[i] = 0;
}

static void jb_putc(j8b *b, char c)
{
    if (!b || b->err) return;
    if (b->len + 1 >= b->cap) {
        b->err = J8_FULL;
        return;
    }
    b->out[b->len] = c;
    b->len = b->len + 1;
    b->out[b->len] = 0;
}

static void jb_puts(j8b *b, const char *s)
{
    int i;
    if (!s) return;
    i = 0;
    while (s[i]) {
        jb_putc(b, s[i]);
        i = i + 1;
    }
}

static void jb_xml_escape(j8b *b, const char *s)
{
    int i;
    char c;
    if (!s) return;
    i = 0;
    while (s[i]) {
        c = s[i];
        if (c == '&') jb_puts(b, "&amp;");
        else if (c == '<') jb_puts(b, "&lt;");
        else if (c == '>') jb_puts(b, "&gt;");
        else if (c == '\'') jb_puts(b, "&apos;");
        else jb_putc(b, c);
        i = i + 1;
    }
}

static int j8_take(j8p *p, char c)
{
    j8_ws(p);
    if (!p || p->p >= p->end || *p->p != c) return 0;
    p->p = p->p + 1;
    return 1;
}

static int j8_is_term(char c)
{
    return c == 0 || c == ',' || c == '}' || c == ']' || j8_space(c);
}

static int j8_string(j8p *p, char *out, int cap)
{
    int i;
    char c;
    char esc;
    j8_ws(p);
    if (!p || p->p >= p->end || *p->p != '"') return 0;
    p->p = p->p + 1;
    i = 0;
    while (p->p < p->end && *p->p) {
        c = *p->p;
        p->p = p->p + 1;
        if (c == '"') {
            if (out && cap > 0) out[i < cap ? i : cap - 1] = 0;
            return 1;
        }
        if (c == '\\' && p->p < p->end) {
            esc = *p->p;
            p->p = p->p + 1;
            if (esc == 'n') c = '\n';
            else if (esc == 'r') c = '\r';
            else if (esc == 't') c = '\t';
            else if (esc == 'b') c = ' ';
            else if (esc == 'f') c = ' ';
            else if (esc == 'u') {
                if (p->end - p->p >= 4) p->p = p->p + 4;
                c = '?';
            } else c = esc;
        }
        if (c == '\n') p->line = p->line + 1;
        if (out && cap > 0 && i + 1 < cap) {
            out[i] = c;
            i = i + 1;
        }
    }
    if (out && cap > 0) out[i < cap ? i : cap - 1] = 0;
    return 0;
}

static void j8_skip_value(j8p *p);

static void j8_skip_array(j8p *p)
{
    int first;
    if (!j8_take(p, '[')) return;
    first = 1;
    while (p->p < p->end) {
        j8_ws(p);
        if (j8_take(p, ']')) return;
        if (!first) j8_take(p, ',');
        j8_skip_value(p);
        first = 0;
    }
}

static void j8_skip_object(j8p *p)
{
    char key[J8_KEY_MAX];
    if (!j8_take(p, '{')) return;
    while (p->p < p->end) {
        j8_ws(p);
        if (j8_take(p, '}')) return;
        if (!j8_string(p, key, (int)sizeof(key))) return;
        j8_take(p, ':');
        j8_skip_value(p);
        j8_ws(p);
        if (j8_take(p, ',')) { }
    }
}

static void j8_skip_scalar(j8p *p)
{
    j8_ws(p);
    while (p->p < p->end && !j8_is_term(*p->p)) p->p = p->p + 1;
}

static void j8_skip_value(j8p *p)
{
    j8_ws(p);
    if (!p || p->p >= p->end) return;
    if (*p->p == '"') { char tmp[8]; j8_string(p, tmp, (int)sizeof(tmp)); return; }
    if (*p->p == '{') { j8_skip_object(p); return; }
    if (*p->p == '[') { j8_skip_array(p); return; }
    j8_skip_scalar(p);
}

static int j8_value_is_object(j8p *p)
{
    j8_ws(p);
    return p && p->p < p->end && *p->p == '{';
}

static int j8_value_is_array(j8p *p)
{
    j8_ws(p);
    return p && p->p < p->end && *p->p == '[';
}

static void j8_attr_scalar(j8p *p, j8b *b)
{
    char tmp[J8_VAL_MAX];
    int i;
    j8_ws(p);
    i = 0;
    while (p->p < p->end && !j8_is_term(*p->p)) {
        if (i + 1 < (int)sizeof(tmp)) {
            tmp[i] = *p->p;
            i = i + 1;
        }
        p->p = p->p + 1;
    }
    tmp[i] = 0;
    jb_xml_escape(b, tmp);
}

static void j8_attr_value(j8p *p, j8b *b, int in_array)
{
    char tmp[J8_VAL_MAX];
    int first;
    j8_ws(p);
    if (!p || p->p >= p->end) return;
    if (*p->p == '"') {
        if (j8_string(p, tmp, (int)sizeof(tmp))) {
            if (in_array) jb_putc(b, '"');
            jb_xml_escape(b, tmp);
            if (in_array) jb_putc(b, '"');
        }
        return;
    }
    if (*p->p == '[') {
        p->p = p->p + 1;
        first = 1;
        while (p->p < p->end) {
            j8_ws(p);
            if (j8_take(p, ']')) break;
            if (!first) jb_putc(b, ' ');
            j8_attr_value(p, b, 1);
            j8_ws(p);
            if (j8_take(p, ',')) { }
            first = 0;
        }
        return;
    }
    if (*p->p == '{') {
        j8_skip_object(p);
        return;
    }
    j8_attr_scalar(p, b);
}

static void j8_attr_pair(j8p *p, j8b *b, const char *name)
{
    jb_putc(b, ' ');
    jb_puts(b, name);
    jb_puts(b, "='");
    j8_attr_value(p, b, 0);
    jb_putc(b, '\'');
}

static int j8_is_stmt_name(const char *k)
{
    return j8_eq(k, "meta") || j8_eq(k, "component") || j8_eq(k, "unit");
}

static void j8_empty_stmt(j8p *p, j8b *b, const char *tag)
{
    char key[J8_KEY_MAX];
    char attr[J8_KEY_MAX];
    if (!j8_take(p, '{')) { j8_skip_value(p); return; }
    jb_putc(b, '<');
    jb_puts(b, tag);
    while (p->p < p->end) {
        j8_ws(p);
        if (j8_take(p, '}')) break;
        if (!j8_string(p, key, (int)sizeof(key))) break;
        j8_take(p, ':');
        if (key[0] == '@') j8_copy(attr, (int)sizeof(attr), key + 1);
        else j8_copy(attr, (int)sizeof(attr), key);
        if (j8_value_is_object(p) || j8_value_is_array(p)) {
            j8_attr_pair(p, b, attr);
        } else {
            j8_attr_pair(p, b, attr);
        }
        j8_ws(p);
        j8_take(p, ',');
    }
    jb_puts(b, "/>");
}

static void j8_head_value(j8p *p, j8b *b, const char *tag)
{
    int first;
    if (j8_value_is_array(p)) {
        j8_take(p, '[');
        first = 1;
        while (p->p < p->end) {
            j8_ws(p);
            if (j8_take(p, ']')) break;
            if (!first) j8_take(p, ',');
            j8_empty_stmt(p, b, tag);
            first = 0;
            j8_ws(p);
            j8_take(p, ',');
        }
    } else if (j8_value_is_object(p)) {
        j8_empty_stmt(p, b, tag);
    } else {
        j8_skip_value(p);
    }
}

static void j8_node_wrapped(j8p *p, j8b *b);
static void j8_node_body(j8p *p, j8b *b, const char *name);

static int j8_key_is_container(const char *k)
{
    return k && k[0] == '-';
}

static int j8_key_is_nodeish(const char *k)
{
    if (!k || !k[0]) return 0;
    if (j8_eq(k, "ROUTE") || j8_eq(k, "IS") || j8_eq(k, "connect") ||
        j8_eq(k, "field") || j8_eq(k, "fieldValue")) return 1;
    return x3d_node_type_from_name(k) != X3D_NODE_UNKNOWN;
}

static void j8_children_value(j8p *p, j8b *b)
{
    int first;
    char key[J8_KEY_MAX];
    if (j8_value_is_array(p)) {
        j8_take(p, '[');
        first = 1;
        while (p->p < p->end) {
            j8_ws(p);
            if (j8_take(p, ']')) break;
            if (!first) j8_take(p, ',');
            j8_node_wrapped(p, b);
            first = 0;
            j8_ws(p);
            j8_take(p, ',');
        }
    } else if (j8_value_is_object(p)) {
        j8_take(p, '{');
        while (p->p < p->end) {
            j8_ws(p);
            if (j8_take(p, '}')) break;
            if (!j8_string(p, key, (int)sizeof(key))) break;
            j8_take(p, ':');
            if (j8_key_is_nodeish(key)) j8_node_body(p, b, key);
            else j8_skip_value(p);
            j8_ws(p);
            j8_take(p, ',');
        }
    } else {
        j8_skip_value(p);
    }
}

static void j8_node_body(j8p *p, j8b *b, const char *name)
{
    char key[J8_KEY_MAX];
    char attr[J8_KEY_MAX];
    int open;
    int has_child;
    if (!j8_take(p, '{')) { j8_skip_value(p); return; }
    jb_putc(b, '<');
    jb_puts(b, name);
    open = 1;
    has_child = 0;
    while (p->p < p->end) {
        j8_ws(p);
        if (j8_take(p, '}')) break;
        if (!j8_string(p, key, (int)sizeof(key))) break;
        j8_take(p, ':');
        if (key[0] == '@') {
            if (open) {
                j8_copy(attr, (int)sizeof(attr), key + 1);
                j8_attr_pair(p, b, attr);
            } else {
                j8_skip_value(p);
            }
        } else if (j8_key_is_container(key)) {
            if (open) { jb_putc(b, '>'); open = 0; }
            has_child = 1;
            j8_children_value(p, b);
        } else if (j8_key_is_nodeish(key)) {
            if (open) { jb_putc(b, '>'); open = 0; }
            has_child = 1;
            j8_node_body(p, b, key);
        } else {
            if (j8_value_is_object(p)) {
                if (open) { jb_putc(b, '>'); open = 0; }
                has_child = 1;
                j8_children_value(p, b);
            } else {
                if (open) j8_attr_pair(p, b, key);
                else j8_skip_value(p);
            }
        }
        j8_ws(p);
        j8_take(p, ',');
    }
    if (open && !has_child) {
        jb_puts(b, "/>");
    } else {
        if (open) jb_putc(b, '>');
        jb_puts(b, "</");
        jb_puts(b, name);
        jb_putc(b, '>');
    }
}

static void j8_node_wrapped(j8p *p, j8b *b)
{
    char key[J8_KEY_MAX];
    if (!j8_take(p, '{')) { j8_skip_value(p); return; }
    while (p->p < p->end) {
        j8_ws(p);
        if (j8_take(p, '}')) break;
        if (!j8_string(p, key, (int)sizeof(key))) break;
        j8_take(p, ':');
        if (j8_key_is_nodeish(key)) j8_node_body(p, b, key);
        else j8_skip_value(p);
        j8_ws(p);
        j8_take(p, ',');
    }
}

static void j8_head(j8p *p, j8b *b)
{
    char key[J8_KEY_MAX];
    if (!j8_take(p, '{')) { j8_skip_value(p); return; }
    jb_puts(b, "<head>");
    while (p->p < p->end) {
        j8_ws(p);
        if (j8_take(p, '}')) break;
        if (!j8_string(p, key, (int)sizeof(key))) break;
        j8_take(p, ':');
        if (j8_is_stmt_name(key)) j8_head_value(p, b, key);
        else j8_skip_value(p);
        j8_ws(p);
        j8_take(p, ',');
    }
    jb_puts(b, "</head>");
}

static void j8_x3d(j8p *p, j8b *b)
{
    char key[J8_KEY_MAX];
    char attr[J8_KEY_MAX];
    int open;
    if (!j8_take(p, '{')) { j8_skip_value(p); return; }
    jb_puts(b, "<X3D");
    open = 1;
    while (p->p < p->end) {
        j8_ws(p);
        if (j8_take(p, '}')) break;
        if (!j8_string(p, key, (int)sizeof(key))) break;
        j8_take(p, ':');
        if (key[0] == '@') {
            if (open) {
                j8_copy(attr, (int)sizeof(attr), key + 1);
                j8_attr_pair(p, b, attr);
            } else {
                j8_skip_value(p);
            }
        } else if (j8_eq(key, "head")) {
            if (open) { jb_putc(b, '>'); open = 0; }
            j8_head(p, b);
        } else if (j8_eq(key, "Scene")) {
            if (open) { jb_putc(b, '>'); open = 0; }
            j8_node_body(p, b, "Scene");
        } else {
            j8_skip_value(p);
        }
        j8_ws(p);
        j8_take(p, ',');
    }
    if (open) jb_putc(b, '>');
    jb_puts(b, "</X3D>");
}

int x3d_json_to_xml(const char *json, int json_len, char *out, int out_cap, int *out_len)
{
    j8p p;
    j8b b;
    char key[J8_KEY_MAX];
    if (!json || json_len < 0 || !out || out_cap <= 0 || !out_len) return X3D_STATUS_BAD_ARG;
    p.p = json;
    p.end = json + json_len;
    p.line = 1;
    b.out = out;
    b.cap = out_cap;
    b.len = 0;
    b.err = 0;
    out[0] = 0;
    j8_ws(&p);
    if (!j8_take(&p, '{')) return X3D_STATUS_XML;
    while (p.p < p.end) {
        j8_ws(&p);
        if (j8_take(&p, '}')) break;
        if (!j8_string(&p, key, (int)sizeof(key))) return X3D_STATUS_XML;
        j8_take(&p, ':');
        if (j8_eq(key, "X3D")) j8_x3d(&p, &b);
        else j8_skip_value(&p);
        j8_ws(&p);
        j8_take(&p, ',');
    }
    if (b.err) return X3D_STATUS_OUTPUT_FULL;
    *out_len = b.len;
    return X3D_STATUS_OK;
}

int x3d_parse_json(x3d_scene *scene, const char *json, int json_len, x3d_diag *diag)
{
    int rc;
    int xml_len;
    if (!scene || !json || json_len < 0) return X3D_STATUS_BAD_ARG;
    rc = x3d_json_to_xml(json, json_len, j8_xml, (int)sizeof(j8_xml), &xml_len);
    if (rc != X3D_STATUS_OK) {
        j8_diag(diag, rc, 1, "JSON to XML bridge failed");
        return rc;
    }
    rc = x3d_parse_xml(scene, j8_xml, xml_len, diag);
    if (diag && rc == X3D_STATUS_OK) {
        diag->code = X3D_STATUS_OK;
        diag->message[0] = 0;
    }
    return rc;
}

static void jw_put_fx(j8b *b, x3d_fx v)
{
    char tmp[48];
    x3d_fx_to_text(v, tmp, (int)sizeof(tmp));
    jb_puts(b, tmp);
}

static void jw_escape(j8b *b, const char *s)
{
    int i;
    char c;
    if (!s) return;
    i = 0;
    while (s[i]) {
        c = s[i];
        if (c == '"') jb_puts(b, "\\\"");
        else if (c == '\\') jb_puts(b, "\\\\");
        else if (c == '\n') jb_puts(b, "\\n");
        else if (c == '\r') jb_puts(b, "\\r");
        else if (c == '\t') jb_puts(b, "\\t");
        else jb_putc(b, c);
        i = i + 1;
    }
}

static void jw_str_pair(j8b *b, const char *name, const char *value, int *need_comma)
{
    if (!value || !value[0]) return;
    if (*need_comma) jb_puts(b, ", ");
    jb_putc(b, '"');
    jb_puts(b, name);
    jb_puts(b, "\": \"");
    jw_escape(b, value);
    jb_putc(b, '"');
    *need_comma = 1;
}

static void jw_fx_pair(j8b *b, const char *name, x3d_fx value, int *need_comma)
{
    if (*need_comma) jb_puts(b, ", ");
    jb_putc(b, '"');
    jb_puts(b, name);
    jb_puts(b, "\": ");
    jw_put_fx(b, value);
    *need_comma = 1;
}

static void jw_vec3_pair(j8b *b, const char *name, x3d_vec3 v, int *need_comma)
{
    if (*need_comma) jb_puts(b, ", ");
    jb_putc(b, '"'); jb_puts(b, name); jb_puts(b, "\": [");
    jw_put_fx(b, v.x); jb_puts(b, ", "); jw_put_fx(b, v.y); jb_puts(b, ", "); jw_put_fx(b, v.z);
    jb_putc(b, ']');
    *need_comma = 1;
}

static void jw_color_pair(j8b *b, const char *name, x3d_color c, int *need_comma)
{
    if (*need_comma) jb_puts(b, ", ");
    jb_putc(b, '"'); jb_puts(b, name); jb_puts(b, "\": [");
    jw_put_fx(b, c.r); jb_puts(b, ", "); jw_put_fx(b, c.g); jb_puts(b, ", "); jw_put_fx(b, c.b);
    jb_putc(b, ']');
    *need_comma = 1;
}

static void jw_int_array_pair(j8b *b, const char *name, const int *vals, int count, int *need_comma)
{
    int i;
    char tmp[32];
    if (!vals || count <= 0) return;
    if (*need_comma) jb_puts(b, ", ");
    jb_putc(b, '"'); jb_puts(b, name); jb_puts(b, "\": [");
    for (i = 0; i < count; ++i) {
        if (i > 0) jb_puts(b, ", ");
        sprintf(tmp, "%d", vals[i]);
        jb_puts(b, tmp);
    }
    jb_putc(b, ']');
    *need_comma = 1;
}

static void jw_vec3_array_pair(j8b *b, const char *name, const x3d_vec3 *vals, int count, int *need_comma)
{
    int i;
    if (!vals || count <= 0) return;
    if (*need_comma) jb_puts(b, ", ");
    jb_putc(b, '"'); jb_puts(b, name); jb_puts(b, "\": [");
    for (i = 0; i < count; ++i) {
        if (i > 0) jb_puts(b, ", ");
        jb_putc(b, '[');
        jw_put_fx(b, vals[i].x); jb_puts(b, ", "); jw_put_fx(b, vals[i].y); jb_puts(b, ", "); jw_put_fx(b, vals[i].z);
        jb_putc(b, ']');
    }
    jb_putc(b, ']');
    *need_comma = 1;
}

static int jw_has_child(const x3d_scene *s, int node)
{
    return s && node >= 0 && node < s->node_count && s->nodes[node].first_child != X3D_NONE;
}

static void jw_raw_attrs(j8b *b, const x3d_scene *s, int node, int *need_comma)
{
    int i;
    char name[J8_KEY_MAX];
    for (i = 0; i < s->raw_attr_count; ++i) {
        if (s->raw_attrs[i].node_index == node) {
            name[0] = '@';
            j8_copy(name + 1, (int)sizeof(name) - 1, s->raw_attrs[i].name);
            jw_str_pair(b, name, s->raw_attrs[i].value, need_comma);
        }
    }
}

static void jw_node(j8b *b, const x3d_scene *s, int node);

static void jw_child_list(j8b *b, const x3d_scene *s, int node, int *need_comma)
{
    int c;
    int first;
    if (!jw_has_child(s, node)) return;
    if (*need_comma) jb_puts(b, ", ");
    jb_puts(b, "\"-children\": [");
    first = 1;
    c = s->nodes[node].first_child;
    while (c != X3D_NONE) {
        if (!first) jb_puts(b, ", ");
        jw_node(b, s, c);
        first = 0;
        c = s->nodes[c].next_sibling;
    }
    jb_putc(b, ']');
    *need_comma = 1;
}

static void jw_node_attrs(j8b *b, const x3d_scene *s, int node, int *need_comma)
{
    int t;
    x3d_node n;
    if (!s || node < 0 || node >= s->node_count) return;
    n = s->nodes[node];
    jw_str_pair(b, "@DEF", n.def_name, need_comma);
    jw_str_pair(b, "@USE", n.use_name, need_comma);
    jw_str_pair(b, "@containerField", n.container_field, need_comma);
    t = n.type;
    if (t == X3D_NODE_TRANSFORM) {
        if (s->transforms[node].translation.x || s->transforms[node].translation.y || s->transforms[node].translation.z) jw_vec3_pair(b, "@translation", s->transforms[node].translation, need_comma);
    } else if (t == X3D_NODE_MATERIAL) {
        jw_color_pair(b, "@diffuseColor", s->materials[node].diffuse_color, need_comma);
    } else if (t == X3D_NODE_PHYSICAL_MATERIAL) {
        jw_color_pair(b, "@baseColor", s->pbr_materials[node].base_color, need_comma);
        jw_fx_pair(b, "@metallic", s->pbr_materials[node].metallic, need_comma);
        jw_fx_pair(b, "@roughness", s->pbr_materials[node].roughness, need_comma);
    } else if (t == X3D_NODE_INDEXED_FACE_SET) {
        if (s->ifs_nodes[node].coord_index_start >= 0) jw_int_array_pair(b, "@coordIndex", s->int_pool + s->ifs_nodes[node].coord_index_start, s->ifs_nodes[node].coord_index_count, need_comma);
    } else if (t == X3D_NODE_INDEXED_TRIANGLE_SET) {
        if (s->indexed_geoms[node].index_start >= 0) jw_int_array_pair(b, "@index", s->int_pool + s->indexed_geoms[node].index_start, s->indexed_geoms[node].index_count, need_comma);
    } else if (t == X3D_NODE_COORDINATE) {
        if (s->coord_refs[node].start >= 0) jw_vec3_array_pair(b, "@point", s->vec3_pool + s->coord_refs[node].start, s->coord_refs[node].count, need_comma);
    } else if (t == X3D_NODE_BOX) {
        jw_vec3_pair(b, "@size", s->boxes[node].size, need_comma);
    } else if (t == X3D_NODE_SPHERE) {
        jw_fx_pair(b, "@radius", s->spheres[node].radius, need_comma);
    } else if (t == X3D_NODE_TIME_SENSOR) {
        jw_fx_pair(b, "@cycleInterval", s->time_sensors[node].cycle_interval, need_comma);
        jw_fx_pair(b, "@startTime", s->time_sensors[node].start_time, need_comma);
        jw_str_pair(b, "@loop", s->time_sensors[node].loop ? "true" : "false", need_comma);
    } else if (t == X3D_NODE_SCALAR_INTERPOLATOR) {
        if (s->interpolators[node].key_start >= 0) {
            int i;
            if (*need_comma) jb_puts(b, ", ");
            jb_puts(b, "\"@key\": [");
            for (i = 0; i < s->interpolators[node].key_count; ++i) {
                if (i > 0) jb_puts(b, ", ");
                jw_put_fx(b, s->fx_pool[s->interpolators[node].key_start + i]);
            }
            jb_putc(b, ']');
            *need_comma = 1;
        }
        if (s->interpolators[node].value_start >= 0) {
            int i;
            if (*need_comma) jb_puts(b, ", ");
            jb_puts(b, "\"@keyValue\": [");
            for (i = 0; i < s->interpolators[node].value_count; ++i) {
                if (i > 0) jb_puts(b, ", ");
                jw_put_fx(b, s->fx_pool[s->interpolators[node].value_start + i]);
            }
            jb_putc(b, ']');
            *need_comma = 1;
        }
    } else if (t == X3D_NODE_ROUTE) {
        int r;
        for (r = 0; r < s->route_count; ++r) {
            if (s->routes[r].node_index == node) {
                jw_str_pair(b, "@fromNode", s->routes[r].from_node, need_comma);
                jw_str_pair(b, "@fromField", s->routes[r].from_field, need_comma);
                jw_str_pair(b, "@toNode", s->routes[r].to_node, need_comma);
                jw_str_pair(b, "@toField", s->routes[r].to_field, need_comma);
            }
        }
    }
    jw_raw_attrs(b, s, node, need_comma);
}

static void jw_node(j8b *b, const x3d_scene *s, int node)
{
    int need;
    const char *name;
    if (!s || node < 0 || node >= s->node_count) return;
    name = s->nodes[node].name[0] ? s->nodes[node].name : x3d_node_type_name(s->nodes[node].type);
    jb_puts(b, "{\"");
    jw_escape(b, name);
    jb_puts(b, "\": {");
    need = 0;
    jw_node_attrs(b, s, node, &need);
    jw_child_list(b, s, node, &need);
    jb_puts(b, "}}");
}

int x3d_write_json(const x3d_scene *scene, char *out, int out_cap, int *out_len)
{
    j8b b;
    int i;
    int c;
    int first;
    if (!scene || !out || out_cap <= 0 || !out_len) return X3D_STATUS_BAD_ARG;
    b.out = out;
    b.cap = out_cap;
    b.len = 0;
    b.err = 0;
    out[0] = 0;
    jb_puts(&b, "{\"X3D\": {");
    jb_puts(&b, "\"@profile\": \""); jw_escape(&b, scene->profile[0] ? scene->profile : "Interchange"); jb_puts(&b, "\", ");
    jb_puts(&b, "\"@version\": \""); jw_escape(&b, scene->version[0] ? scene->version : "4.0"); jb_puts(&b, "\"");
    if (scene->meta_count > 0 || scene->component_count > 0 || scene->unit_count > 0) {
        jb_puts(&b, ", \"head\": {");
        first = 1;
        if (scene->component_count > 0) {
            if (!first) jb_puts(&b, ", ");
            jb_puts(&b, "\"component\": [");
            for (i = 0; i < scene->component_count; ++i) {
                char tmp[32];
                if (i > 0) jb_puts(&b, ", ");
                jb_puts(&b, "{\"@name\": \""); jw_escape(&b, scene->components[i].name); jb_puts(&b, "\", \"@level\": ");
                sprintf(tmp, "%d", scene->components[i].level); jb_puts(&b, tmp); jb_putc(&b, '}');
            }
            jb_putc(&b, ']');
            first = 0;
        }
        if (scene->meta_count > 0) {
            if (!first) jb_puts(&b, ", ");
            jb_puts(&b, "\"meta\": [");
            for (i = 0; i < scene->meta_count; ++i) {
                if (i > 0) jb_puts(&b, ", ");
                jb_puts(&b, "{\"@name\": \""); jw_escape(&b, scene->metas[i].name); jb_puts(&b, "\", \"@content\": \""); jw_escape(&b, scene->metas[i].content); jb_puts(&b, "\"}");
            }
            jb_putc(&b, ']');
            first = 0;
        }
        jb_putc(&b, '}');
    }
    jb_puts(&b, ", \"Scene\": {\"-children\": [");
    first = 1;
    if (scene->scene_node >= 0) {
        c = scene->nodes[scene->scene_node].first_child;
        while (c != X3D_NONE) {
            if (!first) jb_puts(&b, ", ");
            jw_node(&b, scene, c);
            first = 0;
            c = scene->nodes[c].next_sibling;
        }
    }
    jb_puts(&b, "]}}}");
    if (b.err) return X3D_STATUS_OUTPUT_FULL;
    *out_len = b.len;
    return X3D_STATUS_OK;
}

static int v8_attr_arity(const char *name)
{
    if (!name) return 0;
    if (j8_eq(name, "translation") || j8_eq(name, "scale") || j8_eq(name, "center") ||
        j8_eq(name, "bboxCenter") || j8_eq(name, "bboxSize") || j8_eq(name, "size") ||
        j8_eq(name, "diffuseColor") || j8_eq(name, "emissiveColor") || j8_eq(name, "specularColor") ||
        j8_eq(name, "baseColor") || j8_eq(name, "direction") || j8_eq(name, "location") ||
        j8_eq(name, "position") || j8_eq(name, "linearVelocity") || j8_eq(name, "gravity") ||
        j8_eq(name, "point")) return 3;
    if (j8_eq(name, "rotation") || j8_eq(name, "orientation") || j8_eq(name, "scaleOrientation") ||
        j8_eq(name, "plane")) return 4;
    if (j8_eq(name, "frictionCoefficients") || j8_eq(name, "slipFactors") ||
        j8_eq(name, "texCoord") || j8_eq(name, "offset")) return 2;
    return 0;
}

static int v8_token_count(const char *s)
{
    int i;
    int count;
    int in_tok;
    if (!s) return 0;
    i = 0;
    count = 0;
    in_tok = 0;
    while (s[i]) {
        if (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n' || s[i] == ',') {
            in_tok = 0;
        } else {
            if (!in_tok) count = count + 1;
            in_tok = 1;
        }
        i = i + 1;
    }
    return count;
}

int x3d_validate_field_arity(const x3d_scene *scene, x3d_diag *diag)
{
    int i;
    int ar;
    int ct;
    if (!scene) return X3D_STATUS_BAD_ARG;
    for (i = 0; i < scene->raw_attr_count; ++i) {
        ar = v8_attr_arity(scene->raw_attrs[i].name);
        if (ar > 0) {
            ct = v8_token_count(scene->raw_attrs[i].value);
            if (ct > 0 && (ct % ar) != 0) {
                j8_diag(diag, X3D_STATUS_NUMBER, scene->nodes[scene->raw_attrs[i].node_index].line, "field arity mismatch");
                if (diag) diag->node_index = scene->raw_attrs[i].node_index;
                return X3D_STATUS_NUMBER;
            }
        }
    }
    if (diag) j8_diag(diag, X3D_STATUS_OK, 1, "field arity ok");
    return X3D_STATUS_OK;
}

void x3d_v8_collect_summary(const x3d_scene *scene, x3d_v8_summary *summary)
{
    int i;
    x3d_draw_list dl;
    if (!summary) return;
    memset(summary, 0, sizeof(*summary));
    if (!scene) return;
    summary->nodes = scene->node_count;
    summary->routes = scene->route_count;
    summary->raw_attrs = scene->raw_attr_count;
    summary->components = scene->component_count;
    summary->metas = scene->meta_count;
    for (i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].use_name[0]) summary->use_nodes = summary->use_nodes + 1;
        if (scene->nodes[i].def_name[0]) summary->def_nodes = summary->def_nodes + 1;
        if (scene->nodes[i].type == X3D_NODE_SCALAR_INTERPOLATOR ||
            scene->nodes[i].type == X3D_NODE_POSITION_INTERPOLATOR ||
            scene->nodes[i].type == X3D_NODE_ORIENTATION_INTERPOLATOR) summary->interpolators = summary->interpolators + 1;
        if (scene->nodes[i].type == X3D_NODE_SCRIPT) summary->script_nodes = summary->script_nodes + 1;
        if (x3d_is_sensor_node(scene->nodes[i].type)) summary->sensor_nodes = summary->sensor_nodes + 1;
    }
    if (x3d_build_draw_list(scene, &dl) == X3D_STATUS_OK) summary->draw_items = dl.count;
    if (x3d_validate_proto_routes(scene, 0) == X3D_STATUS_OK) summary->route_proto_validation_ok = 1;
    if (x3d_validate_field_arity(scene, 0) == X3D_STATUS_OK) summary->field_arity_ok = 1;
}
