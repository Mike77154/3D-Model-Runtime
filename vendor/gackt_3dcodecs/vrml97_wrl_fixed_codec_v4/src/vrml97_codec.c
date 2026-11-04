#include "vrml97_codec.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#define VRML97_TOK_EOF 0
#define VRML97_TOK_ID 1
#define VRML97_TOK_NUMBER 2
#define VRML97_TOK_STRING 3
#define VRML97_TOK_LBRACE 4
#define VRML97_TOK_RBRACE 5
#define VRML97_TOK_LBRACKET 6
#define VRML97_TOK_RBRACKET 7
#define VRML97_TOK_DOT 8
#define VRML97_TOK_OTHER 9

#define VRML97_DEF_COORD 1
#define VRML97_DEF_NORMAL 2
#define VRML97_DEF_COLOR 3
#define VRML97_DEF_TEXCOORD 4
#define VRML97_DEF_MATERIAL 5
#define VRML97_DEF_TEXTURE 6
#define VRML97_DEF_APPEARANCE 7
#define VRML97_DEF_GEOMETRY 8
#define VRML97_DEF_PROTO 9

#define VRML97_PI 205887L
#define VRML97_TWO_PI 411775L
#define VRML97_HALF_PI 102944L
#define VRML97_PRIM_SEGMENTS 16
#define VRML97_SPHERE_RINGS 8
#define VRML97_TEXT_MAX_LINES 16
#define VRML97_TEXT_LINE_MAX 160

#define VRML97_FACE_OK 0
#define VRML97_FACE_END 1

#define VRML97_GROUP_NORMAL 0
#define VRML97_GROUP_TRANSFORM 1
#define VRML97_GROUP_LOD 2
#define VRML97_GROUP_SWITCH 3

#define VRML97_TEXTURE_IMAGE 1
#define VRML97_TEXTURE_PIXEL 2
#define VRML97_TEXTURE_MOVIE 3

typedef struct vrml97_tok_s {
    int type;
    char text[VRML97_NAME_MAX];
    vrml97_fx fx;
    long ivalue;
} vrml97_tok;

typedef struct vrml97_reader_s {
    const char *src;
    unsigned long len;
    unsigned long pos;
    unsigned long end;
    unsigned long tok_start;
    unsigned long tok_end;
    int line;
    int col;
    vrml97_tok tok;
} vrml97_reader;

typedef struct vrml97_app_s {
    int material;
    int texture;
    vrml97_tex_transform tex_transform;
} vrml97_app;

typedef struct vrml97_transform_s {
    vrml97_vec3 center;
    vrml97_vec3 translation;
    vrml97_vec3 scale;
    vrml97_vec3 rotation_axis;
    vrml97_fx rotation_angle;
    vrml97_vec3 scale_axis;
    vrml97_fx scale_angle;
} vrml97_transform;

static int vrml97_parse_nodes(vrml97_scene *scene, vrml97_reader *r);
static int vrml97_parse_node(vrml97_scene *scene, vrml97_reader *r);
static int vrml97_parse_node_by_type(vrml97_scene *scene, vrml97_reader *r, const char *type_name, const char *def_name);
static int vrml97_parse_group(vrml97_scene *scene, vrml97_reader *r, const char *def_name, int group_kind);
static int vrml97_parse_shape(vrml97_scene *scene, vrml97_reader *r, const char *def_name);
static int vrml97_parse_geometry_span(vrml97_scene *scene, const char *src, unsigned long start, unsigned long end, int material, int texture, const char *name);
static int vrml97_parse_indexed_face_set(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name);
static int vrml97_parse_box(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name);
static int vrml97_parse_sphere(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name);
static int vrml97_parse_cylinder(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name);
static int vrml97_parse_cone(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name);
static int vrml97_parse_elevation_grid(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name);
static int vrml97_parse_extrusion(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name);
static int vrml97_parse_indexed_line_set(vrml97_scene *scene, vrml97_reader *r, int material, const char *name);
static int vrml97_parse_point_set(vrml97_scene *scene, vrml97_reader *r, int material, const char *name);
static int vrml97_parse_text(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name);
static int vrml97_parse_texture_transform_node(vrml97_scene *scene, vrml97_reader *r, vrml97_tex_transform *tt);
static int vrml97_parse_appearance_node(vrml97_scene *scene, vrml97_reader *r, vrml97_app *app);
static int vrml97_parse_material_node(vrml97_scene *scene, vrml97_reader *r, int *mat_index);
static int vrml97_parse_texture_node(vrml97_scene *scene, vrml97_reader *r, int *tex_index);
static int vrml97_parse_coordinate_node(vrml97_scene *scene, vrml97_reader *r);
static int vrml97_parse_normal_node(vrml97_scene *scene, vrml97_reader *r);
static int vrml97_parse_color_node(vrml97_scene *scene, vrml97_reader *r);
static int vrml97_parse_texcoord_node(vrml97_scene *scene, vrml97_reader *r);
static int vrml97_parse_viewpoint_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name);
static int vrml97_parse_light_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name, int kind);
static int vrml97_parse_background_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name);
static int vrml97_parse_fog_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name);
static int vrml97_parse_navigation_info_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name);
static int vrml97_parse_world_info_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name);
static int vrml97_parse_inline_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name);

static void vrml97_set_error(vrml97_scene *scene, vrml97_reader *r, const char *msg)
{
    if (scene != 0) {
        scene->error_line = r != 0 ? r->line : 0;
        scene->error_column = r != 0 ? r->col : 0;
        if (msg != 0) {
            strncpy(scene->last_error, msg, sizeof(scene->last_error) - 1u);
            scene->last_error[sizeof(scene->last_error) - 1u] = '\0';
        }
    }
}

static void vrml97_warn(vrml97_scene *scene, unsigned long bit)
{
    if (scene != 0) {
        scene->warnings |= bit;
        scene->warning_count++;
    }
}

static int vrml97_streq(const char *a, const char *b)
{
    if (a == 0 || b == 0) {
        return VRML97_FALSE;
    }
    return strcmp(a, b) == 0 ? VRML97_TRUE : VRML97_FALSE;
}

static void vrml97_copy_name(char *dst, const char *src, int cap)
{
    int i;
    if (cap <= 0) {
        return;
    }
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    i = 0;
    while (src[i] != '\0' && i < cap - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static long vrml97_abs_long(long v)
{
    if (v < 0) {
        if (v == LONG_MIN) {
            return LONG_MAX;
        }
        return -v;
    }
    return v;
}

static vrml97_fx vrml97_fx_clamp_unsigned(unsigned long v, int neg)
{
    unsigned long maxp;
    maxp = neg ? (unsigned long)LONG_MAX : (unsigned long)LONG_MAX;
    if (v > maxp) {
        return neg ? -LONG_MAX : LONG_MAX;
    }
    if (neg) {
        return -(vrml97_fx)v;
    }
    return (vrml97_fx)v;
}

vrml97_fx vrml97_fx_from_int(long v)
{
    if (v > (LONG_MAX >> VRML97_FX_SHIFT)) {
        return LONG_MAX;
    }
    if (v < -(LONG_MAX >> VRML97_FX_SHIFT)) {
        return -LONG_MAX;
    }
    return (vrml97_fx)(v << VRML97_FX_SHIFT);
}

long vrml97_fx_to_int_floor(vrml97_fx v)
{
    if (v >= 0) {
        return v >> VRML97_FX_SHIFT;
    }
    return -(((-v) + VRML97_FX_ONE - 1L) >> VRML97_FX_SHIFT);
}

static vrml97_fx vrml97_fx_mul(vrml97_fx a, vrml97_fx b)
{
    int neg;
    unsigned long ua;
    unsigned long ub;
    unsigned long ah;
    unsigned long al;
    unsigned long bh;
    unsigned long bl;
    unsigned long part;
    unsigned long res;

    if (a == 0 || b == 0) {
        return 0;
    }
    neg = 0;
    if (a < 0) {
        neg = !neg;
        ua = (unsigned long)vrml97_abs_long(a);
    } else {
        ua = (unsigned long)a;
    }
    if (b < 0) {
        neg = !neg;
        ub = (unsigned long)vrml97_abs_long(b);
    } else {
        ub = (unsigned long)b;
    }

    ah = ua >> 16;
    al = ua & 0xffffUL;
    bh = ub >> 16;
    bl = ub & 0xffffUL;

    res = 0;

    if (ah != 0 && bh != 0) {
        part = ah * bh;
        if (part > ((unsigned long)LONG_MAX >> 16)) {
            return neg ? -LONG_MAX : LONG_MAX;
        }
        res += part << 16;
    }

    part = ah * bl;
    if (part > (unsigned long)LONG_MAX - res) {
        return neg ? -LONG_MAX : LONG_MAX;
    }
    res += part;

    part = bh * al;
    if (part > (unsigned long)LONG_MAX - res) {
        return neg ? -LONG_MAX : LONG_MAX;
    }
    res += part;

    part = (al * bl) >> 16;
    if (part > (unsigned long)LONG_MAX - res) {
        return neg ? -LONG_MAX : LONG_MAX;
    }
    res += part;

    return vrml97_fx_clamp_unsigned(res, neg);
}

static vrml97_fx vrml97_fx_div(vrml97_fx a, vrml97_fx b)
{
    int neg;
    unsigned long ua;
    unsigned long ub;
    unsigned long qint;
    unsigned long rem;
    unsigned long q;
    int i;

    if (b == 0) {
        return a < 0 ? -LONG_MAX : LONG_MAX;
    }
    if (a == 0) {
        return 0;
    }
    neg = 0;
    if (a < 0) {
        neg = !neg;
        ua = (unsigned long)vrml97_abs_long(a);
    } else {
        ua = (unsigned long)a;
    }
    if (b < 0) {
        neg = !neg;
        ub = (unsigned long)vrml97_abs_long(b);
    } else {
        ub = (unsigned long)b;
    }
    if (ub == 0) {
        return neg ? -LONG_MAX : LONG_MAX;
    }

    qint = ua / ub;
    rem = ua % ub;
    if (qint > ((unsigned long)LONG_MAX >> 16)) {
        return neg ? -LONG_MAX : LONG_MAX;
    }
    q = qint << 16;
    for (i = 0; i < 16; ++i) {
        if (rem > ((unsigned long)LONG_MAX >> 1)) {
            rem = ub;
        } else {
            rem <<= 1;
        }
        if (rem >= ub) {
            rem -= ub;
            q |= (1UL << (15 - i));
        }
    }
    return vrml97_fx_clamp_unsigned(q, neg);
}

static vrml97_fx vrml97_fx_sqrt(vrml97_fx v)
{
    vrml97_fx y;
    vrml97_fx ny;
    int i;

    if (v <= 0) {
        return 0;
    }
    y = v > VRML97_FX_ONE ? v : VRML97_FX_ONE;
    for (i = 0; i < 18; ++i) {
        ny = (y + vrml97_fx_div(v, y)) >> 1;
        if (ny == y) {
            break;
        }
        y = ny;
    }
    return y;
}

static vrml97_fx vrml97_fx_mod_angle(vrml97_fx a)
{
    while (a > VRML97_PI) {
        a -= VRML97_TWO_PI;
    }
    while (a < -VRML97_PI) {
        a += VRML97_TWO_PI;
    }
    return a;
}

static vrml97_fx vrml97_fx_sin(vrml97_fx x)
{
    vrml97_fx y;
    vrml97_fx q;
    vrml97_fx num;
    vrml97_fx den;
    int neg;

    x = vrml97_fx_mod_angle(x);
    neg = VRML97_FALSE;
    if (x < 0) {
        neg = VRML97_TRUE;
        x = -x;
    }
    if (x > VRML97_PI) {
        x = VRML97_TWO_PI - x;
        neg = !neg;
    }
    y = VRML97_PI - x;
    q = vrml97_fx_mul(x, y);
    num = q << 4;
    den = vrml97_fx_mul(VRML97_PI, VRML97_PI);
    den = den * 5L - q * 4L;
    if (den == 0) {
        return 0;
    }
    y = vrml97_fx_div(num, den);
    if (neg) {
        y = -y;
    }
    return y;
}

static vrml97_fx vrml97_fx_cos(vrml97_fx x)
{
    return vrml97_fx_sin(x + VRML97_HALF_PI);
}

static vrml97_vec3 vrml97_vec3_make(vrml97_fx x, vrml97_fx y, vrml97_fx z)
{
    vrml97_vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

static vrml97_vec2 vrml97_vec2_make(vrml97_fx x, vrml97_fx y)
{
    vrml97_vec2 v;
    v.x = x;
    v.y = y;
    return v;
}

static int vrml97_vec2_equal(vrml97_vec2 a, vrml97_vec2 b)
{
    return (a.x == b.x && a.y == b.y) ? VRML97_TRUE : VRML97_FALSE;
}

static vrml97_vec3 vrml97_vec3_sub(vrml97_vec3 a, vrml97_vec3 b)
{
    vrml97_vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

static vrml97_vec3 vrml97_vec3_cross(vrml97_vec3 a, vrml97_vec3 b)
{
    vrml97_vec3 r;
    r.x = vrml97_fx_mul(a.y, b.z) - vrml97_fx_mul(a.z, b.y);
    r.y = vrml97_fx_mul(a.z, b.x) - vrml97_fx_mul(a.x, b.z);
    r.z = vrml97_fx_mul(a.x, b.y) - vrml97_fx_mul(a.y, b.x);
    return r;
}

static vrml97_vec3 vrml97_vec3_normalize(vrml97_vec3 v)
{
    vrml97_fx lsq;
    vrml97_fx len;
    vrml97_vec3 r;
    lsq = vrml97_fx_mul(v.x, v.x) + vrml97_fx_mul(v.y, v.y) + vrml97_fx_mul(v.z, v.z);
    len = vrml97_fx_sqrt(lsq);
    if (len <= 0) {
        return vrml97_vec3_make(0, 0, VRML97_FX_ONE);
    }
    r.x = vrml97_fx_div(v.x, len);
    r.y = vrml97_fx_div(v.y, len);
    r.z = vrml97_fx_div(v.z, len);
    return r;
}

static void vrml97_mat_identity(vrml97_mat4 *m)
{
    int r;
    int c;
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            m->m[r][c] = r == c ? VRML97_FX_ONE : 0;
        }
    }
}

static vrml97_mat4 vrml97_mat_mul(const vrml97_mat4 *a, const vrml97_mat4 *b)
{
    vrml97_mat4 r;
    int i;
    int j;
    int k;
    vrml97_fx sum;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            sum = 0;
            for (k = 0; k < 4; ++k) {
                sum += vrml97_fx_mul(a->m[i][k], b->m[k][j]);
            }
            r.m[i][j] = sum;
        }
    }
    return r;
}

static vrml97_mat4 vrml97_mat_translate(vrml97_vec3 t)
{
    vrml97_mat4 m;
    vrml97_mat_identity(&m);
    m.m[0][3] = t.x;
    m.m[1][3] = t.y;
    m.m[2][3] = t.z;
    return m;
}

static vrml97_mat4 vrml97_mat_scale(vrml97_vec3 s)
{
    vrml97_mat4 m;
    vrml97_mat_identity(&m);
    m.m[0][0] = s.x;
    m.m[1][1] = s.y;
    m.m[2][2] = s.z;
    return m;
}

static vrml97_mat4 vrml97_mat_rotate(vrml97_vec3 axis, vrml97_fx angle)
{
    vrml97_mat4 m;
    vrml97_fx x;
    vrml97_fx y;
    vrml97_fx z;
    vrml97_fx s;
    vrml97_fx c;
    vrml97_fx oc;
    vrml97_fx xx;
    vrml97_fx yy;
    vrml97_fx zz;
    vrml97_fx xy;
    vrml97_fx xz;
    vrml97_fx yz;

    axis = vrml97_vec3_normalize(axis);
    x = axis.x;
    y = axis.y;
    z = axis.z;
    s = vrml97_fx_sin(angle);
    c = vrml97_fx_cos(angle);
    oc = VRML97_FX_ONE - c;
    xx = vrml97_fx_mul(x, x);
    yy = vrml97_fx_mul(y, y);
    zz = vrml97_fx_mul(z, z);
    xy = vrml97_fx_mul(x, y);
    xz = vrml97_fx_mul(x, z);
    yz = vrml97_fx_mul(y, z);

    vrml97_mat_identity(&m);
    m.m[0][0] = c + vrml97_fx_mul(xx, oc);
    m.m[0][1] = vrml97_fx_mul(xy, oc) - vrml97_fx_mul(z, s);
    m.m[0][2] = vrml97_fx_mul(xz, oc) + vrml97_fx_mul(y, s);
    m.m[1][0] = vrml97_fx_mul(xy, oc) + vrml97_fx_mul(z, s);
    m.m[1][1] = c + vrml97_fx_mul(yy, oc);
    m.m[1][2] = vrml97_fx_mul(yz, oc) - vrml97_fx_mul(x, s);
    m.m[2][0] = vrml97_fx_mul(xz, oc) - vrml97_fx_mul(y, s);
    m.m[2][1] = vrml97_fx_mul(yz, oc) + vrml97_fx_mul(x, s);
    m.m[2][2] = c + vrml97_fx_mul(zz, oc);
    return m;
}

static vrml97_vec3 vrml97_mat_point(const vrml97_mat4 *m, vrml97_vec3 p)
{
    vrml97_vec3 r;
    r.x = vrml97_fx_mul(m->m[0][0], p.x) + vrml97_fx_mul(m->m[0][1], p.y) + vrml97_fx_mul(m->m[0][2], p.z) + m->m[0][3];
    r.y = vrml97_fx_mul(m->m[1][0], p.x) + vrml97_fx_mul(m->m[1][1], p.y) + vrml97_fx_mul(m->m[1][2], p.z) + m->m[1][3];
    r.z = vrml97_fx_mul(m->m[2][0], p.x) + vrml97_fx_mul(m->m[2][1], p.y) + vrml97_fx_mul(m->m[2][2], p.z) + m->m[2][3];
    return r;
}

static vrml97_vec3 vrml97_mat_vector(const vrml97_mat4 *m, vrml97_vec3 p)
{
    vrml97_vec3 r;
    r.x = vrml97_fx_mul(m->m[0][0], p.x) + vrml97_fx_mul(m->m[0][1], p.y) + vrml97_fx_mul(m->m[0][2], p.z);
    r.y = vrml97_fx_mul(m->m[1][0], p.x) + vrml97_fx_mul(m->m[1][1], p.y) + vrml97_fx_mul(m->m[1][2], p.z);
    r.z = vrml97_fx_mul(m->m[2][0], p.x) + vrml97_fx_mul(m->m[2][1], p.y) + vrml97_fx_mul(m->m[2][2], p.z);
    return vrml97_vec3_normalize(r);
}

static vrml97_mat4 vrml97_make_transform(vrml97_transform *t)
{
    vrml97_mat4 m;
    vrml97_mat4 a;
    vrml97_mat4 b;
    vrml97_vec3 neg_center;
    vrml97_vec3 neg_axis;

    vrml97_mat_identity(&m);
    a = vrml97_mat_translate(t->translation);
    m = vrml97_mat_mul(&m, &a);
    a = vrml97_mat_translate(t->center);
    m = vrml97_mat_mul(&m, &a);
    a = vrml97_mat_rotate(t->rotation_axis, t->rotation_angle);
    m = vrml97_mat_mul(&m, &a);
    a = vrml97_mat_rotate(t->scale_axis, t->scale_angle);
    m = vrml97_mat_mul(&m, &a);
    a = vrml97_mat_scale(t->scale);
    m = vrml97_mat_mul(&m, &a);
    neg_axis = t->scale_axis;
    b = vrml97_mat_rotate(neg_axis, -t->scale_angle);
    m = vrml97_mat_mul(&m, &b);
    neg_center.x = -t->center.x;
    neg_center.y = -t->center.y;
    neg_center.z = -t->center.z;
    a = vrml97_mat_translate(neg_center);
    m = vrml97_mat_mul(&m, &a);
    return m;
}

static void vrml97_default_material(vrml97_material *m)
{
    vrml97_copy_name(m->name, "vrml_default", VRML97_NAME_MAX);
    m->diffuse_color = vrml97_vec3_make((vrml97_fx)(VRML97_FX_ONE * 4L / 5L), (vrml97_fx)(VRML97_FX_ONE * 4L / 5L), (vrml97_fx)(VRML97_FX_ONE * 4L / 5L));
    m->emissive_color = vrml97_vec3_make(0, 0, 0);
    m->specular_color = vrml97_vec3_make(0, 0, 0);
    m->ambient_intensity = (vrml97_fx)(VRML97_FX_ONE / 5L);
    m->shininess = (vrml97_fx)(VRML97_FX_ONE / 5L);
    m->transparency = 0;
}

static void vrml97_default_tex_transform(vrml97_tex_transform *tt)
{
    tt->center = vrml97_vec2_make(0, 0);
    tt->scale = vrml97_vec2_make(VRML97_FX_ONE, VRML97_FX_ONE);
    tt->translation = vrml97_vec2_make(0, 0);
    tt->rotation = 0;
    tt->active = VRML97_FALSE;
}

void vrml97_scene_init(vrml97_scene *scene)
{
    if (scene == 0) {
        return;
    }
    memset(scene, 0, sizeof(*scene));
    scene->material_count = 1;
    vrml97_default_material(&scene->materials[0]);
    scene->matrix_top = 0;
    vrml97_mat_identity(&scene->matrix_stack[0]);
    vrml97_default_tex_transform(&scene->current_tex_transform);
}

const char *vrml97_status_string(int code)
{
    switch (code) {
    case VRML97_OK: return "ok";
    case VRML97_ERR_NULL: return "null argument";
    case VRML97_ERR_SYNTAX: return "syntax error";
    case VRML97_ERR_OVERFLOW: return "fixed buffer limit reached";
    case VRML97_ERR_STACK: return "transform stack limit reached";
    case VRML97_ERR_IO: return "I/O error";
    default: return "unknown";
    }
}

static int vrml97_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',';
}

static int vrml97_is_id_start(char c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
        return VRML97_TRUE;
    }
    return VRML97_FALSE;
}

static int vrml97_is_id_char(char c)
{
    if (vrml97_is_id_start(c)) {
        return VRML97_TRUE;
    }
    if (c >= '0' && c <= '9') {
        return VRML97_TRUE;
    }
    if (c == '-' || c == '+') {
        return VRML97_TRUE;
    }
    return VRML97_FALSE;
}

static int vrml97_is_num_start(char c, char n)
{
    if (c >= '0' && c <= '9') {
        return VRML97_TRUE;
    }
    if (c == '.' && n >= '0' && n <= '9') {
        return VRML97_TRUE;
    }
    if ((c == '-' || c == '+') && ((n >= '0' && n <= '9') || n == '.')) {
        return VRML97_TRUE;
    }
    return VRML97_FALSE;
}

static void vrml97_reader_init(vrml97_reader *r, const char *data, unsigned long start, unsigned long end)
{
    memset(r, 0, sizeof(*r));
    r->src = data;
    r->pos = start;
    r->len = end;
    r->end = end;
    r->line = 1;
    r->col = 1;
}

static char vrml97_reader_ch(vrml97_reader *r)
{
    if (r->pos >= r->end) {
        return '\0';
    }
    return r->src[r->pos];
}

static char vrml97_reader_next_ch(vrml97_reader *r)
{
    if (r->pos + 1u >= r->end) {
        return '\0';
    }
    return r->src[r->pos + 1u];
}

static void vrml97_reader_advance(vrml97_reader *r)
{
    char c;
    if (r->pos >= r->end) {
        return;
    }
    c = r->src[r->pos];
    r->pos++;
    if (c == '\n') {
        r->line++;
        r->col = 1;
    } else {
        r->col++;
    }
}

static void vrml97_skip_ws(vrml97_reader *r)
{
    char c;
    for (;;) {
        c = vrml97_reader_ch(r);
        if (c == '\0') {
            return;
        }
        if (vrml97_is_space(c)) {
            vrml97_reader_advance(r);
            continue;
        }
        if (c == '#') {
            while (vrml97_reader_ch(r) != '\0' && vrml97_reader_ch(r) != '\n') {
                vrml97_reader_advance(r);
            }
            continue;
        }
        break;
    }
}

static vrml97_fx vrml97_parse_num_text(const char *s)
{
    int neg;
    long ip;
    vrml97_fx v;
    vrml97_fx f;
    vrml97_fx step;
    int exp_neg;
    int exp_val;
    int i;
    char c;

    neg = VRML97_FALSE;
    ip = 0;
    v = 0;
    f = 0;
    step = VRML97_FX_ONE / 10L;
    exp_neg = VRML97_FALSE;
    exp_val = 0;
    i = 0;

    if (s[i] == '-') {
        neg = VRML97_TRUE;
        i++;
    } else if (s[i] == '+') {
        i++;
    }

    while (s[i] >= '0' && s[i] <= '9') {
        if (ip < (LONG_MAX / 10L)) {
            ip = ip * 10L + (long)(s[i] - '0');
        }
        i++;
    }
    v = vrml97_fx_from_int(ip);

    if (s[i] == '.') {
        i++;
        while (s[i] >= '0' && s[i] <= '9') {
            f += (vrml97_fx)((long)(s[i] - '0') * step);
            if (step > 0) {
                step /= 10L;
            }
            i++;
        }
    }
    v += f;

    if (s[i] == 'e' || s[i] == 'E') {
        i++;
        if (s[i] == '-') {
            exp_neg = VRML97_TRUE;
            i++;
        } else if (s[i] == '+') {
            i++;
        }
        while (s[i] >= '0' && s[i] <= '9') {
            if (exp_val < 12) {
                exp_val = exp_val * 10 + (int)(s[i] - '0');
            }
            i++;
        }
        if (exp_val > 8) {
            exp_val = 8;
        }
        for (i = 0; i < exp_val; ++i) {
            if (exp_neg) {
                v /= 10L;
            } else {
                if (v > LONG_MAX / 10L) {
                    v = LONG_MAX;
                    break;
                }
                v *= 10L;
            }
        }
    }

    c = s[0];
    (void)c;
    if (neg) {
        v = -v;
    }
    return v;
}

static long vrml97_parse_hex_text(const char *s)
{
    unsigned long v;
    int i;
    char c;
    v = 0;
    i = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        i = 2;
    }
    for (;;) {
        c = s[i];
        if (c >= '0' && c <= '9') {
            v = (v << 4) + (unsigned long)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            v = (v << 4) + (unsigned long)(10 + c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            v = (v << 4) + (unsigned long)(10 + c - 'A');
        } else {
            break;
        }
        if (v > (unsigned long)LONG_MAX) {
            v = (unsigned long)LONG_MAX;
            break;
        }
        i++;
    }
    return (long)v;
}

static int vrml97_next(vrml97_reader *r)
{
    char c;
    char n;
    int len;

    vrml97_skip_ws(r);
    r->tok_start = r->pos;
    memset(&r->tok, 0, sizeof(r->tok));

    c = vrml97_reader_ch(r);
    if (c == '\0') {
        r->tok.type = VRML97_TOK_EOF;
        r->tok_end = r->pos;
        return r->tok.type;
    }

    if (c == '{') {
        r->tok.type = VRML97_TOK_LBRACE;
        r->tok.text[0] = c;
        r->tok.text[1] = '\0';
        vrml97_reader_advance(r);
        r->tok_end = r->pos;
        return r->tok.type;
    }
    if (c == '}') {
        r->tok.type = VRML97_TOK_RBRACE;
        r->tok.text[0] = c;
        r->tok.text[1] = '\0';
        vrml97_reader_advance(r);
        r->tok_end = r->pos;
        return r->tok.type;
    }
    if (c == '[') {
        r->tok.type = VRML97_TOK_LBRACKET;
        r->tok.text[0] = c;
        r->tok.text[1] = '\0';
        vrml97_reader_advance(r);
        r->tok_end = r->pos;
        return r->tok.type;
    }
    if (c == ']') {
        r->tok.type = VRML97_TOK_RBRACKET;
        r->tok.text[0] = c;
        r->tok.text[1] = '\0';
        vrml97_reader_advance(r);
        r->tok_end = r->pos;
        return r->tok.type;
    }
    if (c == '.') {
        n = vrml97_reader_next_ch(r);
        if (!vrml97_is_num_start(c, n)) {
            r->tok.type = VRML97_TOK_DOT;
            r->tok.text[0] = c;
            r->tok.text[1] = '\0';
            vrml97_reader_advance(r);
            r->tok_end = r->pos;
            return r->tok.type;
        }
    }
    if (c == '0' && (vrml97_reader_next_ch(r) == 'x' || vrml97_reader_next_ch(r) == 'X')) {
        r->tok.type = VRML97_TOK_NUMBER;
        len = 0;
        while (vrml97_reader_ch(r) != '\0') {
            c = vrml97_reader_ch(r);
            if (!((c >= '0' && c <= '9') || c == 'x' || c == 'X' || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                break;
            }
            if (len < VRML97_NAME_MAX - 1) {
                r->tok.text[len++] = c;
            }
            vrml97_reader_advance(r);
        }
        r->tok.text[len] = '\0';
        r->tok.ivalue = vrml97_parse_hex_text(r->tok.text);
        r->tok.fx = vrml97_fx_from_int(r->tok.ivalue);
        r->tok_end = r->pos;
        return r->tok.type;
    }

    if (c == '"') {
        r->tok.type = VRML97_TOK_STRING;
        vrml97_reader_advance(r);
        len = 0;
        while (vrml97_reader_ch(r) != '\0' && vrml97_reader_ch(r) != '"') {
            c = vrml97_reader_ch(r);
            if (c == '\\') {
                vrml97_reader_advance(r);
                c = vrml97_reader_ch(r);
                if (c == '\0') {
                    break;
                }
            }
            if (len < VRML97_URL_MAX - 1) {
                r->tok.text[len++] = c;
            }
            vrml97_reader_advance(r);
        }
        if (vrml97_reader_ch(r) == '"') {
            vrml97_reader_advance(r);
        }
        r->tok.text[len] = '\0';
        r->tok_end = r->pos;
        return r->tok.type;
    }

    n = vrml97_reader_next_ch(r);
    if (vrml97_is_num_start(c, n)) {
        r->tok.type = VRML97_TOK_NUMBER;
        len = 0;
        if (c == '-' || c == '+') {
            if (len < VRML97_NAME_MAX - 1) {
                r->tok.text[len++] = c;
            }
            vrml97_reader_advance(r);
            c = vrml97_reader_ch(r);
        }
        while ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '-' || c == '+') {
            if ((c == '-' || c == '+') && len > 0) {
                char pc;
                pc = r->tok.text[len - 1];
                if (!(pc == 'e' || pc == 'E')) {
                    break;
                }
            }
            if (len < VRML97_NAME_MAX - 1) {
                r->tok.text[len++] = c;
            }
            vrml97_reader_advance(r);
            c = vrml97_reader_ch(r);
        }
        r->tok.text[len] = '\0';
        r->tok.fx = vrml97_parse_num_text(r->tok.text);
        r->tok.ivalue = vrml97_fx_to_int_floor(r->tok.fx);
        r->tok_end = r->pos;
        return r->tok.type;
    }

    if (vrml97_is_id_start(c)) {
        r->tok.type = VRML97_TOK_ID;
        len = 0;
        while (vrml97_reader_ch(r) != '\0' && vrml97_is_id_char(vrml97_reader_ch(r))) {
            if (len < VRML97_NAME_MAX - 1) {
                r->tok.text[len++] = vrml97_reader_ch(r);
            }
            vrml97_reader_advance(r);
        }
        r->tok.text[len] = '\0';
        r->tok_end = r->pos;
        return r->tok.type;
    }

    r->tok.type = VRML97_TOK_OTHER;
    r->tok.text[0] = c;
    r->tok.text[1] = '\0';
    vrml97_reader_advance(r);
    r->tok_end = r->pos;
    return r->tok.type;
}

static int vrml97_expect(vrml97_scene *scene, vrml97_reader *r, int type, const char *msg)
{
    if (vrml97_next(r) != type) {
        vrml97_set_error(scene, r, msg);
        return VRML97_ERR_SYNTAX;
    }
    return VRML97_OK;
}

static int vrml97_skip_compound(vrml97_reader *r, int open_type, int close_type)
{
    int depth;
    int t;
    depth = 1;
    while (depth > 0) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t == open_type) {
            depth++;
        } else if (t == close_type) {
            depth--;
        }
    }
    return VRML97_OK;
}

static int vrml97_capture_node_span(vrml97_reader *r, unsigned long *start, unsigned long *end)
{
    int t;
    unsigned long s;

    vrml97_skip_ws(r);
    s = r->pos;
    t = vrml97_next(r);
    if (t == VRML97_TOK_EOF) {
        *start = s;
        *end = s;
        return VRML97_ERR_SYNTAX;
    }
    if (t == VRML97_TOK_ID && vrml97_streq(r->tok.text, "NULL")) {
        *start = s;
        *end = r->tok_end;
        return VRML97_OK;
    }
    if (t == VRML97_TOK_ID && vrml97_streq(r->tok.text, "USE")) {
        t = vrml97_next(r);
        *start = s;
        *end = r->tok_end;
        return t == VRML97_TOK_ID ? VRML97_OK : VRML97_ERR_SYNTAX;
    }
    if (t == VRML97_TOK_ID && vrml97_streq(r->tok.text, "DEF")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            *start = s;
            *end = r->tok_end;
            return VRML97_ERR_SYNTAX;
        }
        if (vrml97_next(r) != VRML97_TOK_ID) {
            *start = s;
            *end = r->tok_end;
            return VRML97_ERR_SYNTAX;
        }
    } else if (t != VRML97_TOK_ID) {
        *start = s;
        *end = r->tok_end;
        return VRML97_OK;
    }

    vrml97_skip_ws(r);
    if (vrml97_reader_ch(r) == '{') {
        vrml97_next(r);
        if (vrml97_skip_compound(r, VRML97_TOK_LBRACE, VRML97_TOK_RBRACE) != VRML97_OK) {
            *start = s;
            *end = r->tok_end;
            return VRML97_ERR_SYNTAX;
        }
    }
    *start = s;
    *end = r->tok_end;
    return VRML97_OK;
}

static int vrml97_skip_value(vrml97_reader *r)
{
    int t;
    unsigned long s;
    unsigned long e;
    vrml97_skip_ws(r);
    if (vrml97_reader_ch(r) == '[') {
        vrml97_next(r);
        return vrml97_skip_compound(r, VRML97_TOK_LBRACKET, VRML97_TOK_RBRACKET);
    }
    if (vrml97_reader_ch(r) == '{') {
        vrml97_next(r);
        return vrml97_skip_compound(r, VRML97_TOK_LBRACE, VRML97_TOK_RBRACE);
    }
    t = vrml97_next(r);
    if (t == VRML97_TOK_ID) {
        vrml97_skip_ws(r);
        if (vrml97_reader_ch(r) == '{' || vrml97_streq(r->tok.text, "DEF") || vrml97_streq(r->tok.text, "USE")) {
            r->pos = r->tok_start;
            return vrml97_capture_node_span(r, &s, &e);
        }
    }
    return VRML97_OK;
}

static int vrml97_read_bool(vrml97_reader *r, int *out)
{
    int t;
    t = vrml97_next(r);
    if (t == VRML97_TOK_ID) {
        if (vrml97_streq(r->tok.text, "TRUE")) {
            *out = VRML97_TRUE;
            return VRML97_OK;
        }
        if (vrml97_streq(r->tok.text, "FALSE")) {
            *out = VRML97_FALSE;
            return VRML97_OK;
        }
    }
    if (t == VRML97_TOK_NUMBER) {
        *out = r->tok.ivalue != 0 ? VRML97_TRUE : VRML97_FALSE;
        return VRML97_OK;
    }
    return VRML97_ERR_SYNTAX;
}

static int vrml97_read_vec3(vrml97_reader *r, vrml97_vec3 *v)
{
    if (vrml97_next(r) != VRML97_TOK_NUMBER) {
        return VRML97_ERR_SYNTAX;
    }
    v->x = r->tok.fx;
    if (vrml97_next(r) != VRML97_TOK_NUMBER) {
        return VRML97_ERR_SYNTAX;
    }
    v->y = r->tok.fx;
    if (vrml97_next(r) != VRML97_TOK_NUMBER) {
        return VRML97_ERR_SYNTAX;
    }
    v->z = r->tok.fx;
    return VRML97_OK;
}

static int vrml97_read_vec2(vrml97_reader *r, vrml97_vec2 *v)
{
    if (vrml97_next(r) != VRML97_TOK_NUMBER) {
        return VRML97_ERR_SYNTAX;
    }
    v->x = r->tok.fx;
    if (vrml97_next(r) != VRML97_TOK_NUMBER) {
        return VRML97_ERR_SYNTAX;
    }
    v->y = r->tok.fx;
    return VRML97_OK;
}

static int vrml97_read_rotation(vrml97_reader *r, vrml97_vec3 *axis, vrml97_fx *angle)
{
    if (vrml97_read_vec3(r, axis) != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    if (vrml97_next(r) != VRML97_TOK_NUMBER) {
        return VRML97_ERR_SYNTAX;
    }
    *angle = r->tok.fx;
    return VRML97_OK;
}

static int vrml97_find_def(vrml97_scene *scene, const char *name, int kind)
{
    int i;
    for (i = scene->def_count - 1; i >= 0; --i) {
        if (scene->defs[i].kind == kind && vrml97_streq(scene->defs[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static int vrml97_add_def_base(vrml97_scene *scene, const char *name, int kind)
{
    int i;
    if (name == 0 || name[0] == '\0') {
        return -1;
    }
    if (scene->def_count >= VRML97_MAX_DEFS) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return -1;
    }
    i = scene->def_count++;
    memset(&scene->defs[i], 0, sizeof(scene->defs[i]));
    vrml97_copy_name(scene->defs[i].name, name, VRML97_NAME_MAX);
    scene->defs[i].kind = kind;
    scene->defs[i].material = -1;
    scene->defs[i].texture = -1;
    return i;
}

static int vrml97_register_def_vec3(vrml97_scene *scene, const char *name, int kind, const vrml97_vec3 *data, int count)
{
    int i;
    int n;
    i = vrml97_add_def_base(scene, name, kind);
    if (i < 0) {
        return VRML97_ERR_OVERFLOW;
    }
    if (scene->def_vec3_count + count > VRML97_DEF_VEC3_POOL) {
        scene->def_count--;
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_ERR_OVERFLOW;
    }
    scene->defs[i].start = scene->def_vec3_count;
    scene->defs[i].count = count;
    for (n = 0; n < count; ++n) {
        scene->def_vec3_pool[scene->def_vec3_count++] = data[n];
    }
    return VRML97_OK;
}

static int vrml97_register_def_vec2(vrml97_scene *scene, const char *name, int kind, const vrml97_vec2 *data, int count)
{
    int i;
    int n;
    i = vrml97_add_def_base(scene, name, kind);
    if (i < 0) {
        return VRML97_ERR_OVERFLOW;
    }
    if (scene->def_vec2_count + count > VRML97_DEF_VEC2_POOL) {
        scene->def_count--;
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_ERR_OVERFLOW;
    }
    scene->defs[i].start = scene->def_vec2_count;
    scene->defs[i].count = count;
    for (n = 0; n < count; ++n) {
        scene->def_vec2_pool[scene->def_vec2_count++] = data[n];
    }
    return VRML97_OK;
}

static int vrml97_copy_def_vec3(vrml97_scene *scene, int def_index, vrml97_vec3 *dst, int max_count, int *out_count)
{
    int i;
    int count;
    if (def_index < 0) {
        return VRML97_ERR_SYNTAX;
    }
    count = scene->defs[def_index].count;
    if (count > max_count) {
        count = max_count;
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
    }
    for (i = 0; i < count; ++i) {
        dst[i] = scene->def_vec3_pool[scene->defs[def_index].start + i];
    }
    *out_count = count;
    return VRML97_OK;
}

static int vrml97_copy_def_vec2(vrml97_scene *scene, int def_index, vrml97_vec2 *dst, int max_count, int *out_count)
{
    int i;
    int count;
    if (def_index < 0) {
        return VRML97_ERR_SYNTAX;
    }
    count = scene->defs[def_index].count;
    if (count > max_count) {
        count = max_count;
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
    }
    for (i = 0; i < count; ++i) {
        dst[i] = scene->def_vec2_pool[scene->defs[def_index].start + i];
    }
    *out_count = count;
    return VRML97_OK;
}

static int vrml97_register_def_material(vrml97_scene *scene, const char *name, int mat_index)
{
    int i;
    i = vrml97_add_def_base(scene, name, VRML97_DEF_MATERIAL);
    if (i < 0) {
        return VRML97_ERR_OVERFLOW;
    }
    scene->defs[i].material = mat_index;
    return VRML97_OK;
}

static int vrml97_register_def_texture(vrml97_scene *scene, const char *name, int tex_index)
{
    int i;
    i = vrml97_add_def_base(scene, name, VRML97_DEF_TEXTURE);
    if (i < 0) {
        return VRML97_ERR_OVERFLOW;
    }
    scene->defs[i].texture = tex_index;
    return VRML97_OK;
}

static int vrml97_register_def_appearance(vrml97_scene *scene, const char *name, int mat_index, int tex_index)
{
    int i;
    i = vrml97_add_def_base(scene, name, VRML97_DEF_APPEARANCE);
    if (i < 0) {
        return VRML97_ERR_OVERFLOW;
    }
    scene->defs[i].material = mat_index;
    scene->defs[i].texture = tex_index;
    return VRML97_OK;
}

static int vrml97_register_def_span(vrml97_scene *scene, const char *name, int kind, unsigned long start, unsigned long end)
{
    int i;
    if (end < start) {
        return VRML97_ERR_SYNTAX;
    }
    if (start > (unsigned long)INT_MAX || (end - start) > (unsigned long)INT_MAX) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_ERR_OVERFLOW;
    }
    i = vrml97_add_def_base(scene, name, kind);
    if (i < 0) {
        return VRML97_ERR_OVERFLOW;
    }
    scene->defs[i].start = (int)start;
    scene->defs[i].count = (int)(end - start);
    return VRML97_OK;
}

static int vrml97_parse_vec3_array(vrml97_scene *scene, vrml97_reader *r, vrml97_vec3 *dst, int max_count, int *out_count)
{
    int t;
    int count;
    vrml97_vec3 v;

    count = 0;
    t = vrml97_next(r);
    if (t == VRML97_TOK_LBRACKET) {
        for (;;) {
            vrml97_skip_ws(r);
            if (vrml97_reader_ch(r) == ']') {
                vrml97_next(r);
                break;
            }
            if (vrml97_read_vec3(r, &v) != VRML97_OK) {
                vrml97_set_error(scene, r, "expected vec3 array value");
                return VRML97_ERR_SYNTAX;
            }
            if (count < max_count) {
                dst[count++] = v;
            } else {
                vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
            }
        }
    } else if (t == VRML97_TOK_NUMBER) {
        v.x = r->tok.fx;
        if (vrml97_next(r) != VRML97_TOK_NUMBER) {
            return VRML97_ERR_SYNTAX;
        }
        v.y = r->tok.fx;
        if (vrml97_next(r) != VRML97_TOK_NUMBER) {
            return VRML97_ERR_SYNTAX;
        }
        v.z = r->tok.fx;
        if (count < max_count) {
            dst[count++] = v;
        }
    } else {
        return VRML97_ERR_SYNTAX;
    }
    *out_count = count;
    return VRML97_OK;
}

static int vrml97_parse_vec2_array(vrml97_scene *scene, vrml97_reader *r, vrml97_vec2 *dst, int max_count, int *out_count)
{
    int t;
    int count;
    vrml97_vec2 v;

    count = 0;
    t = vrml97_next(r);
    if (t == VRML97_TOK_LBRACKET) {
        for (;;) {
            vrml97_skip_ws(r);
            if (vrml97_reader_ch(r) == ']') {
                vrml97_next(r);
                break;
            }
            if (vrml97_read_vec2(r, &v) != VRML97_OK) {
                vrml97_set_error(scene, r, "expected vec2 array value");
                return VRML97_ERR_SYNTAX;
            }
            if (count < max_count) {
                dst[count++] = v;
            } else {
                vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
            }
        }
    } else if (t == VRML97_TOK_NUMBER) {
        v.x = r->tok.fx;
        if (vrml97_next(r) != VRML97_TOK_NUMBER) {
            return VRML97_ERR_SYNTAX;
        }
        v.y = r->tok.fx;
        if (count < max_count) {
            dst[count++] = v;
        }
    } else {
        return VRML97_ERR_SYNTAX;
    }
    *out_count = count;
    return VRML97_OK;
}

static int vrml97_parse_int_array(vrml97_scene *scene, vrml97_reader *r, int *dst, int max_count, int *out_count)
{
    int t;
    int count;

    count = 0;
    t = vrml97_next(r);
    if (t == VRML97_TOK_LBRACKET) {
        for (;;) {
            vrml97_skip_ws(r);
            if (vrml97_reader_ch(r) == ']') {
                vrml97_next(r);
                break;
            }
            t = vrml97_next(r);
            if (t != VRML97_TOK_NUMBER) {
                vrml97_set_error(scene, r, "expected integer array value");
                return VRML97_ERR_SYNTAX;
            }
            if (count < max_count) {
                dst[count++] = (int)r->tok.ivalue;
            } else {
                vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
            }
        }
    } else if (t == VRML97_TOK_NUMBER) {
        if (count < max_count) {
            dst[count++] = (int)r->tok.ivalue;
        }
    } else {
        return VRML97_ERR_SYNTAX;
    }
    *out_count = count;
    return VRML97_OK;
}

static int vrml97_parse_string_array(vrml97_reader *r, char *out, int out_cap)
{
    int t;
    out[0] = '\0';
    t = vrml97_next(r);
    if (t == VRML97_TOK_LBRACKET) {
        for (;;) {
            vrml97_skip_ws(r);
            if (vrml97_reader_ch(r) == ']') {
                vrml97_next(r);
                break;
            }
            t = vrml97_next(r);
            if (t == VRML97_TOK_STRING && out[0] == '\0') {
                vrml97_copy_name(out, r->tok.text, out_cap);
            }
        }
        return VRML97_OK;
    }
    if (t == VRML97_TOK_STRING) {
        vrml97_copy_name(out, r->tok.text, out_cap);
        return VRML97_OK;
    }
    return VRML97_ERR_SYNTAX;
}

static int vrml97_parse_fx_array(vrml97_scene *scene, vrml97_reader *r, vrml97_fx *dst, int max_count, int *out_count)
{
    int t;
    int count;
    count = 0;
    t = vrml97_next(r);
    if (t == VRML97_TOK_LBRACKET) {
        for (;;) {
            vrml97_skip_ws(r);
            if (vrml97_reader_ch(r) == ']') {
                vrml97_next(r);
                break;
            }
            t = vrml97_next(r);
            if (t != VRML97_TOK_NUMBER) {
                vrml97_set_error(scene, r, "expected number array value");
                return VRML97_ERR_SYNTAX;
            }
            if (count < max_count) {
                dst[count++] = r->tok.fx;
            } else {
                vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
            }
        }
    } else if (t == VRML97_TOK_NUMBER) {
        if (count < max_count) dst[count++] = r->tok.fx;
    } else {
        return VRML97_ERR_SYNTAX;
    }
    *out_count = count;
    return VRML97_OK;
}

static int vrml97_parse_string_list(vrml97_reader *r, char *out, int stride, int max_items, int *out_count)
{
    int t;
    int count;
    int i;
    count = 0;
    t = vrml97_next(r);
    if (t == VRML97_TOK_LBRACKET) {
        for (;;) {
            vrml97_skip_ws(r);
            if (vrml97_reader_ch(r) == ']') {
                vrml97_next(r);
                break;
            }
            t = vrml97_next(r);
            if (t != VRML97_TOK_STRING) return VRML97_ERR_SYNTAX;
            if (count < max_items) {
                for (i = 0; i < stride - 1 && r->tok.text[i] != '\0'; ++i) out[count * stride + i] = r->tok.text[i];
                out[count * stride + i] = '\0';
                count++;
            }
        }
    } else if (t == VRML97_TOK_STRING) {
        if (count < max_items) {
            for (i = 0; i < stride - 1 && r->tok.text[i] != '\0'; ++i) out[count * stride + i] = r->tok.text[i];
            out[count * stride + i] = '\0';
            count++;
        }
    } else {
        return VRML97_ERR_SYNTAX;
    }
    *out_count = count;
    return VRML97_OK;
}

static int vrml97_parse_rotation_array(vrml97_scene *scene, vrml97_reader *r, vrml97_vec3 *axis, vrml97_fx *angle, int max_count, int *out_count)
{
    int t;
    int count;
    vrml97_vec3 a;
    vrml97_fx ang;
    count = 0;
    t = vrml97_next(r);
    if (t == VRML97_TOK_LBRACKET) {
        for (;;) {
            vrml97_skip_ws(r);
            if (vrml97_reader_ch(r) == ']') {
                vrml97_next(r);
                break;
            }
            if (vrml97_read_rotation(r, &a, &ang) != VRML97_OK) {
                vrml97_set_error(scene, r, "expected rotation array value");
                return VRML97_ERR_SYNTAX;
            }
            if (count < max_count) {
                axis[count] = a;
                angle[count] = ang;
                count++;
            } else {
                vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
            }
        }
    } else if (t == VRML97_TOK_NUMBER) {
        a.x = r->tok.fx;
        if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
        a.y = r->tok.fx;
        if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
        a.z = r->tok.fx;
        if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
        ang = r->tok.fx;
        if (count < max_count) {
            axis[count] = a;
            angle[count] = ang;
            count++;
        }
    } else {
        return VRML97_ERR_SYNTAX;
    }
    *out_count = count;
    return VRML97_OK;
}

static void vrml97_default_viewpoint(vrml97_viewpoint *v)
{
    vrml97_copy_name(v->name, "", VRML97_NAME_MAX);
    vrml97_copy_name(v->description, "", VRML97_URL_MAX);
    v->position = vrml97_vec3_make(0, 0, vrml97_fx_from_int(10));
    v->orientation_axis = vrml97_vec3_make(0, 0, VRML97_FX_ONE);
    v->orientation_angle = 0;
    v->field_of_view = VRML97_HALF_PI / 2L;
    v->jump = VRML97_TRUE;
}

static void vrml97_default_light(vrml97_light *l, int kind)
{
    vrml97_copy_name(l->name, "", VRML97_NAME_MAX);
    l->kind = kind;
    l->on = VRML97_TRUE;
    l->color = vrml97_vec3_make(VRML97_FX_ONE, VRML97_FX_ONE, VRML97_FX_ONE);
    l->intensity = VRML97_FX_ONE;
    l->ambient_intensity = 0;
    l->location = vrml97_vec3_make(0, 0, 0);
    l->direction = vrml97_vec3_make(0, 0, -VRML97_FX_ONE);
    l->attenuation = vrml97_vec3_make(VRML97_FX_ONE, 0, 0);
    l->radius = vrml97_fx_from_int(100);
    l->beam_width = VRML97_HALF_PI;
    l->cut_off_angle = VRML97_HALF_PI / 2L;
}

static void vrml97_default_background(vrml97_background *b)
{
    int i;
    memset(b, 0, sizeof(*b));
    b->sky_color[0] = vrml97_vec3_make(0, 0, 0);
    b->sky_color_count = 1;
    for (i = 0; i < VRML97_MAX_BG_URLS; ++i) {
        b->urls[i][0] = '\0';
    }
}

static void vrml97_default_fog(vrml97_fog *f)
{
    vrml97_copy_name(f->name, "", VRML97_NAME_MAX);
    vrml97_copy_name(f->fog_type, "LINEAR", VRML97_NAME_MAX);
    f->color = vrml97_vec3_make(VRML97_FX_ONE, VRML97_FX_ONE, VRML97_FX_ONE);
    f->visibility_range = 0;
}

static void vrml97_default_navigation_info(vrml97_navigation_info *n)
{
    vrml97_copy_name(n->name, "", VRML97_NAME_MAX);
    vrml97_copy_name(n->type, "WALK", VRML97_URL_MAX);
    n->avatar_size[0] = VRML97_FX_ONE / 4L;
    n->avatar_size[1] = (VRML97_FX_ONE * 16L) / 10L;
    n->avatar_size[2] = (VRML97_FX_ONE * 3L) / 4L;
    n->avatar_size_count = 3;
    n->speed = VRML97_FX_ONE;
    n->visibility_limit = 0;
    n->headlight = VRML97_TRUE;
}

static void vrml97_default_world_info(vrml97_world_info *w)
{
    vrml97_copy_name(w->name, "", VRML97_NAME_MAX);
    vrml97_copy_name(w->title, "", VRML97_URL_MAX);
    vrml97_copy_name(w->info, "", VRML97_URL_MAX);
}

static void vrml97_default_inline(vrml97_inline *inl)
{
    vrml97_copy_name(inl->name, "", VRML97_NAME_MAX);
    vrml97_copy_name(inl->url, "", VRML97_URL_MAX);
    inl->bbox_center = vrml97_vec3_make(0, 0, 0);
    inl->bbox_size = vrml97_vec3_make(-VRML97_FX_ONE, -VRML97_FX_ONE, -VRML97_FX_ONE);
}

static int vrml97_parse_fx3_list(vrml97_scene *scene, vrml97_reader *r, vrml97_fx *dst, int max_count, int *out_count)
{
    return vrml97_parse_fx_array(scene, r, dst, max_count, out_count);
}

static int vrml97_parse_url_slot(vrml97_reader *r, char urls[VRML97_MAX_BG_URLS][VRML97_URL_MAX], int slot)
{
    if (slot < 0 || slot >= VRML97_MAX_BG_URLS) return VRML97_ERR_SYNTAX;
    return vrml97_parse_string_array(r, urls[slot], VRML97_URL_MAX);
}

static int vrml97_parse_viewpoint_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_viewpoint v;
    vrml97_default_viewpoint(&v);
    if (def_name != 0) vrml97_copy_name(v.name, def_name, VRML97_NAME_MAX);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Viewpoint body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "position")) {
            if (vrml97_read_vec3(r, &v.position) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "orientation")) {
            if (vrml97_read_rotation(r, &v.orientation_axis, &v.orientation_angle) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "fieldOfView")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            v.field_of_view = r->tok.fx;
        } else if (vrml97_streq(field, "description")) {
            if (vrml97_parse_string_array(r, v.description, VRML97_URL_MAX) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "jump")) {
            if (vrml97_read_bool(r, &v.jump) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    v.position = vrml97_mat_point(&scene->matrix_stack[scene->matrix_top], v.position);
    v.orientation_axis = vrml97_mat_vector(&scene->matrix_stack[scene->matrix_top], v.orientation_axis);
    if (scene->viewpoint_count >= VRML97_MAX_VIEWPOINTS) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_OK;
    }
    scene->viewpoints[scene->viewpoint_count++] = v;
    return VRML97_OK;
}

static int vrml97_parse_light_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name, int kind)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_light l;
    vrml97_default_light(&l, kind);
    if (def_name != 0) vrml97_copy_name(l.name, def_name, VRML97_NAME_MAX);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected light body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "on")) {
            if (vrml97_read_bool(r, &l.on) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "color")) {
            if (vrml97_read_vec3(r, &l.color) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "intensity")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            l.intensity = r->tok.fx;
        } else if (vrml97_streq(field, "ambientIntensity")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            l.ambient_intensity = r->tok.fx;
        } else if (vrml97_streq(field, "location")) {
            if (vrml97_read_vec3(r, &l.location) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "direction")) {
            if (vrml97_read_vec3(r, &l.direction) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "attenuation")) {
            if (vrml97_read_vec3(r, &l.attenuation) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "radius")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            l.radius = r->tok.fx;
        } else if (vrml97_streq(field, "beamWidth")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            l.beam_width = r->tok.fx;
        } else if (vrml97_streq(field, "cutOffAngle")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            l.cut_off_angle = r->tok.fx;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    l.location = vrml97_mat_point(&scene->matrix_stack[scene->matrix_top], l.location);
    l.direction = vrml97_mat_vector(&scene->matrix_stack[scene->matrix_top], l.direction);
    if (scene->light_count >= VRML97_MAX_LIGHTS) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_OK;
    }
    scene->lights[scene->light_count++] = l;
    return VRML97_OK;
}

static int vrml97_parse_background_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_background b;
    vrml97_default_background(&b);
    if (def_name != 0) vrml97_copy_name(b.name, def_name, VRML97_NAME_MAX);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Background body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "skyColor")) {
            if (vrml97_parse_vec3_array(scene, r, b.sky_color, VRML97_MAX_BG_COLORS, &b.sky_color_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "skyAngle")) {
            if (vrml97_parse_fx_array(scene, r, b.sky_angle, VRML97_MAX_BG_COLORS, &b.sky_angle_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "groundColor")) {
            if (vrml97_parse_vec3_array(scene, r, b.ground_color, VRML97_MAX_BG_COLORS, &b.ground_color_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "groundAngle")) {
            if (vrml97_parse_fx_array(scene, r, b.ground_angle, VRML97_MAX_BG_COLORS, &b.ground_angle_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "frontUrl")) {
            if (vrml97_parse_url_slot(r, b.urls, 0) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "backUrl")) {
            if (vrml97_parse_url_slot(r, b.urls, 1) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "leftUrl")) {
            if (vrml97_parse_url_slot(r, b.urls, 2) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "rightUrl")) {
            if (vrml97_parse_url_slot(r, b.urls, 3) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "topUrl")) {
            if (vrml97_parse_url_slot(r, b.urls, 4) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "bottomUrl")) {
            if (vrml97_parse_url_slot(r, b.urls, 5) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    if (scene->background_count >= VRML97_MAX_BACKGROUNDS) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_OK;
    }
    scene->backgrounds[scene->background_count++] = b;
    return VRML97_OK;
}

static int vrml97_parse_fog_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_fog f;
    vrml97_default_fog(&f);
    if (def_name != 0) vrml97_copy_name(f.name, def_name, VRML97_NAME_MAX);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Fog body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "color")) {
            if (vrml97_read_vec3(r, &f.color) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "fogType")) {
            if (vrml97_parse_string_array(r, f.fog_type, VRML97_NAME_MAX) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "visibilityRange")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            f.visibility_range = r->tok.fx;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    if (scene->fog_count >= VRML97_MAX_FOGS) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_OK;
    }
    scene->fogs[scene->fog_count++] = f;
    return VRML97_OK;
}

static int vrml97_parse_navigation_info_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_navigation_info n;
    vrml97_default_navigation_info(&n);
    if (def_name != 0) vrml97_copy_name(n.name, def_name, VRML97_NAME_MAX);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected NavigationInfo body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "type")) {
            if (vrml97_parse_string_array(r, n.type, VRML97_URL_MAX) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "avatarSize")) {
            if (vrml97_parse_fx3_list(scene, r, n.avatar_size, 3, &n.avatar_size_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "speed")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            n.speed = r->tok.fx;
        } else if (vrml97_streq(field, "visibilityLimit")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            n.visibility_limit = r->tok.fx;
        } else if (vrml97_streq(field, "headlight")) {
            if (vrml97_read_bool(r, &n.headlight) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    if (scene->navigation_info_count >= VRML97_MAX_NAVINFO) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_OK;
    }
    scene->navigation_infos[scene->navigation_info_count++] = n;
    return VRML97_OK;
}

static int vrml97_parse_world_info_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_world_info w;
    vrml97_default_world_info(&w);
    if (def_name != 0) vrml97_copy_name(w.name, def_name, VRML97_NAME_MAX);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected WorldInfo body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "title")) {
            if (vrml97_parse_string_array(r, w.title, VRML97_URL_MAX) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "info")) {
            if (vrml97_parse_string_array(r, w.info, VRML97_URL_MAX) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    if (scene->world_info_count >= VRML97_MAX_WORLDINFO) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_OK;
    }
    scene->world_infos[scene->world_info_count++] = w;
    return VRML97_OK;
}

static int vrml97_parse_inline_node(vrml97_scene *scene, vrml97_reader *r, const char *def_name)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_inline inl;
    vrml97_default_inline(&inl);
    if (def_name != 0) vrml97_copy_name(inl.name, def_name, VRML97_NAME_MAX);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Inline body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "url")) {
            if (vrml97_parse_string_array(r, inl.url, VRML97_URL_MAX) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "bboxCenter")) {
            if (vrml97_read_vec3(r, &inl.bbox_center) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "bboxSize")) {
            if (vrml97_read_vec3(r, &inl.bbox_size) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    inl.bbox_center = vrml97_mat_point(&scene->matrix_stack[scene->matrix_top], inl.bbox_center);
    if (scene->inline_count >= VRML97_MAX_INLINES) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_OK;
    }
    scene->inlines[scene->inline_count++] = inl;
    return VRML97_OK;
}

static int vrml97_add_material(vrml97_scene *scene, vrml97_material *mat)
{
    int id;
    if (scene->material_count >= VRML97_MAX_MATERIALS) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return 0;
    }
    id = scene->material_count++;
    scene->materials[id] = *mat;
    return id;
}

static int vrml97_add_texture(vrml97_scene *scene, vrml97_texture *tex)
{
    int id;
    if (scene->texture_count >= VRML97_MAX_TEXTURES) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return -1;
    }
    id = scene->texture_count++;
    scene->textures[id] = *tex;
    return id;
}

static void vrml97_default_transform(vrml97_transform *t)
{
    t->center = vrml97_vec3_make(0, 0, 0);
    t->translation = vrml97_vec3_make(0, 0, 0);
    t->scale = vrml97_vec3_make(VRML97_FX_ONE, VRML97_FX_ONE, VRML97_FX_ONE);
    t->rotation_axis = vrml97_vec3_make(0, 0, VRML97_FX_ONE);
    t->rotation_angle = 0;
    t->scale_axis = vrml97_vec3_make(0, 0, VRML97_FX_ONE);
    t->scale_angle = 0;
}

static int vrml97_push_matrix(vrml97_scene *scene, const vrml97_mat4 *local)
{
    vrml97_mat4 world;
    if (scene->matrix_top + 1 >= VRML97_STACK_MAX) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_ERR_STACK;
    }
    world = vrml97_mat_mul(&scene->matrix_stack[scene->matrix_top], local);
    scene->matrix_top++;
    scene->matrix_stack[scene->matrix_top] = world;
    return VRML97_OK;
}

static void vrml97_pop_matrix(vrml97_scene *scene)
{
    if (scene->matrix_top > 0) {
        scene->matrix_top--;
    }
}

static int vrml97_parse_material_body(vrml97_scene *scene, vrml97_reader *r, vrml97_material *mat)
{
    int t;
    char field[VRML97_NAME_MAX];

    vrml97_default_material(mat);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Material body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "diffuseColor")) {
            if (vrml97_read_vec3(r, &mat->diffuse_color) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "emissiveColor")) {
            if (vrml97_read_vec3(r, &mat->emissive_color) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "specularColor")) {
            if (vrml97_read_vec3(r, &mat->specular_color) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "ambientIntensity")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) {
                return VRML97_ERR_SYNTAX;
            }
            mat->ambient_intensity = r->tok.fx;
        } else if (vrml97_streq(field, "shininess")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) {
                return VRML97_ERR_SYNTAX;
            }
            mat->shininess = r->tok.fx;
        } else if (vrml97_streq(field, "transparency")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) {
                return VRML97_ERR_SYNTAX;
            }
            mat->transparency = r->tok.fx;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    return VRML97_OK;
}

static int vrml97_parse_material_node(vrml97_scene *scene, vrml97_reader *r, int *mat_index)
{
    int t;
    char def_name[VRML97_NAME_MAX];
    vrml97_material mat;
    int idx;

    *mat_index = 0;
    def_name[0] = '\0';
    t = vrml97_next(r);
    if (t != VRML97_TOK_ID) {
        return VRML97_ERR_SYNTAX;
    }
    if (vrml97_streq(r->tok.text, "NULL")) {
        *mat_index = 0;
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "USE")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        idx = vrml97_find_def(scene, r->tok.text, VRML97_DEF_MATERIAL);
        if (idx >= 0) {
            *mat_index = scene->defs[idx].material;
            return VRML97_OK;
        }
        vrml97_warn(scene, VRML97_WARN_BAD_USE);
        *mat_index = 0;
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "DEF")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        vrml97_copy_name(def_name, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
    }
    if (!vrml97_streq(r->tok.text, "Material")) {
        vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_NODE);
        vrml97_skip_value(r);
        return VRML97_OK;
    }
    if (vrml97_parse_material_body(scene, r, &mat) != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    idx = vrml97_add_material(scene, &mat);
    *mat_index = idx;
    if (def_name[0] != '\0') {
        vrml97_copy_name(scene->materials[idx].name, def_name, VRML97_NAME_MAX);
        vrml97_register_def_material(scene, def_name, idx);
    }
    return VRML97_OK;
}

static int vrml97_parse_sfimage(vrml97_scene *scene, vrml97_reader *r, vrml97_texture *tex)
{
    long w;
    long h;
    long c;
    long total;
    long i;
    int t;
    if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
    w = r->tok.ivalue;
    if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
    h = r->tok.ivalue;
    if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
    c = r->tok.ivalue;
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    if (c < 0) c = 0;
    tex->pixel_width = (int)w;
    tex->pixel_height = (int)h;
    tex->pixel_components = (int)c;
    total = w * h;
    if (total < 0 || total > 2147483647L) total = 2147483647L;
    tex->pixel_count = (int)total;
    for (i = 0; i < total; ++i) {
        t = vrml97_next(r);
        if (t != VRML97_TOK_NUMBER) {
            vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
            return VRML97_OK;
        }
    }
    return VRML97_OK;
}

static int vrml97_parse_texture_body(vrml97_scene *scene, vrml97_reader *r, vrml97_texture *tex, int kind)
{
    int t;
    char field[VRML97_NAME_MAX];

    memset(tex, 0, sizeof(*tex));
    tex->repeat_s = VRML97_TRUE;
    tex->repeat_t = VRML97_TRUE;
    tex->kind = kind;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected texture body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "url")) {
            if (vrml97_parse_string_array(r, tex->url, VRML97_URL_MAX) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "repeatS")) {
            if (vrml97_read_bool(r, &tex->repeat_s) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "repeatT")) {
            if (vrml97_read_bool(r, &tex->repeat_t) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "image") && kind == VRML97_TEXTURE_PIXEL) {
            if (vrml97_parse_sfimage(scene, r, tex) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    return VRML97_OK;
}

static int vrml97_parse_texture_node(vrml97_scene *scene, vrml97_reader *r, int *tex_index)
{
    int t;
    char def_name[VRML97_NAME_MAX];
    vrml97_texture tex;
    int idx;
    int kind;

    *tex_index = -1;
    def_name[0] = '\0';
    t = vrml97_next(r);
    if (t != VRML97_TOK_ID) {
        return VRML97_ERR_SYNTAX;
    }
    if (vrml97_streq(r->tok.text, "NULL")) {
        *tex_index = -1;
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "USE")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        idx = vrml97_find_def(scene, r->tok.text, VRML97_DEF_TEXTURE);
        if (idx >= 0) {
            *tex_index = scene->defs[idx].texture;
            return VRML97_OK;
        }
        vrml97_warn(scene, VRML97_WARN_BAD_USE);
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "DEF")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        vrml97_copy_name(def_name, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
    }
    kind = VRML97_TEXTURE_IMAGE;
    if (vrml97_streq(r->tok.text, "PixelTexture")) {
        kind = VRML97_TEXTURE_PIXEL;
    } else if (vrml97_streq(r->tok.text, "MovieTexture")) {
        kind = VRML97_TEXTURE_MOVIE;
    } else if (!vrml97_streq(r->tok.text, "ImageTexture")) {
        vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_NODE);
        vrml97_skip_value(r);
        return VRML97_OK;
    }
    if (vrml97_parse_texture_body(scene, r, &tex, kind) != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    idx = vrml97_add_texture(scene, &tex);
    *tex_index = idx;
    if (idx >= 0 && def_name[0] != '\0') {
        vrml97_copy_name(scene->textures[idx].name, def_name, VRML97_NAME_MAX);
        vrml97_register_def_texture(scene, def_name, idx);
    }
    return VRML97_OK;
}

static vrml97_vec2 vrml97_apply_tex_transform_one(const vrml97_tex_transform *tt, vrml97_vec2 v)
{
    vrml97_fx x;
    vrml97_fx y;
    vrml97_fx c;
    vrml97_fx si;
    vrml97_fx rx;
    vrml97_fx ry;
    x = v.x - tt->center.x;
    y = v.y - tt->center.y;
    x = vrml97_fx_mul(x, tt->scale.x);
    y = vrml97_fx_mul(y, tt->scale.y);
    c = vrml97_fx_cos(tt->rotation);
    si = vrml97_fx_sin(tt->rotation);
    rx = vrml97_fx_mul(x, c) - vrml97_fx_mul(y, si);
    ry = vrml97_fx_mul(x, si) + vrml97_fx_mul(y, c);
    v.x = rx + tt->center.x + tt->translation.x;
    v.y = ry + tt->center.y + tt->translation.y;
    return v;
}

static int vrml97_parse_texture_transform_body(vrml97_scene *scene, vrml97_reader *r, vrml97_tex_transform *tt)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_default_tex_transform(tt);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected TextureTransform body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "center")) {
            if (vrml97_read_vec2(r, &tt->center) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "scale")) {
            if (vrml97_read_vec2(r, &tt->scale) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "translation")) {
            if (vrml97_read_vec2(r, &tt->translation) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "rotation")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            tt->rotation = r->tok.fx;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    tt->active = VRML97_TRUE;
    return VRML97_OK;
}

static int vrml97_parse_texture_transform_node(vrml97_scene *scene, vrml97_reader *r, vrml97_tex_transform *tt)
{
    int t;
    char def_name[VRML97_NAME_MAX];
    (void)def_name;
    t = vrml97_next(r);
    if (t != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
    if (vrml97_streq(r->tok.text, "NULL")) {
        vrml97_default_tex_transform(tt);
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "USE")) {
        if (vrml97_next(r) != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
        vrml97_warn(scene, VRML97_WARN_BAD_USE);
        vrml97_default_tex_transform(tt);
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "DEF")) {
        if (vrml97_next(r) != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
        vrml97_copy_name(def_name, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_next(r) != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
    }
    if (!vrml97_streq(r->tok.text, "TextureTransform")) {
        vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_NODE);
        vrml97_skip_value(r);
        vrml97_default_tex_transform(tt);
        return VRML97_OK;
    }
    return vrml97_parse_texture_transform_body(scene, r, tt);
}

static int vrml97_parse_appearance_body(vrml97_scene *scene, vrml97_reader *r, vrml97_app *app)
{
    int t;
    char field[VRML97_NAME_MAX];

    app->material = 0;
    app->texture = -1;
    vrml97_default_tex_transform(&app->tex_transform);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Appearance body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "material")) {
            if (vrml97_parse_material_node(scene, r, &app->material) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "texture")) {
            if (vrml97_parse_texture_node(scene, r, &app->texture) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "textureTransform")) {
            if (vrml97_parse_texture_transform_node(scene, r, &app->tex_transform) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    return VRML97_OK;
}

static int vrml97_parse_appearance_node(vrml97_scene *scene, vrml97_reader *r, vrml97_app *app)
{
    int t;
    char def_name[VRML97_NAME_MAX];
    int idx;

    app->material = 0;
    app->texture = -1;
    def_name[0] = '\0';
    t = vrml97_next(r);
    if (t != VRML97_TOK_ID) {
        return VRML97_ERR_SYNTAX;
    }
    if (vrml97_streq(r->tok.text, "NULL")) {
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "USE")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        idx = vrml97_find_def(scene, r->tok.text, VRML97_DEF_APPEARANCE);
        if (idx >= 0) {
            app->material = scene->defs[idx].material;
            app->texture = scene->defs[idx].texture;
            return VRML97_OK;
        }
        vrml97_warn(scene, VRML97_WARN_BAD_USE);
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "DEF")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        vrml97_copy_name(def_name, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
    }
    if (!vrml97_streq(r->tok.text, "Appearance")) {
        vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_NODE);
        vrml97_skip_value(r);
        return VRML97_OK;
    }
    if (vrml97_parse_appearance_body(scene, r, app) != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    if (def_name[0] != '\0') {
        vrml97_register_def_appearance(scene, def_name, app->material, app->texture);
    }
    return VRML97_OK;
}

static int vrml97_parse_coordinate_body(vrml97_scene *scene, vrml97_reader *r)
{
    int t;
    char field[VRML97_NAME_MAX];
    scene->work_coord_count = 0;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Coordinate body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "point")) {
            if (vrml97_parse_vec3_array(scene, r, scene->work_coord, VRML97_MAX_COORDS, &scene->work_coord_count) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    return VRML97_OK;
}

static int vrml97_parse_coordinate_node(vrml97_scene *scene, vrml97_reader *r)
{
    int t;
    char def_name[VRML97_NAME_MAX];
    int idx;

    scene->work_coord_count = 0;
    def_name[0] = '\0';
    t = vrml97_next(r);
    if (t != VRML97_TOK_ID) {
        return VRML97_ERR_SYNTAX;
    }
    if (vrml97_streq(r->tok.text, "NULL")) {
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "USE")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        idx = vrml97_find_def(scene, r->tok.text, VRML97_DEF_COORD);
        if (idx >= 0) {
            return vrml97_copy_def_vec3(scene, idx, scene->work_coord, VRML97_MAX_COORDS, &scene->work_coord_count);
        }
        vrml97_warn(scene, VRML97_WARN_BAD_USE);
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "DEF")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        vrml97_copy_name(def_name, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
    }
    if (!vrml97_streq(r->tok.text, "Coordinate")) {
        vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_NODE);
        vrml97_skip_value(r);
        return VRML97_OK;
    }
    if (vrml97_parse_coordinate_body(scene, r) != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    if (def_name[0] != '\0') {
        vrml97_register_def_vec3(scene, def_name, VRML97_DEF_COORD, scene->work_coord, scene->work_coord_count);
    }
    return VRML97_OK;
}

static int vrml97_parse_normal_body(vrml97_scene *scene, vrml97_reader *r)
{
    int t;
    char field[VRML97_NAME_MAX];
    scene->work_normal_count = 0;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Normal body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "vector")) {
            if (vrml97_parse_vec3_array(scene, r, scene->work_normal, VRML97_MAX_NORMALS, &scene->work_normal_count) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    return VRML97_OK;
}

static int vrml97_parse_normal_node(vrml97_scene *scene, vrml97_reader *r)
{
    int t;
    char def_name[VRML97_NAME_MAX];
    int idx;

    scene->work_normal_count = 0;
    def_name[0] = '\0';
    t = vrml97_next(r);
    if (t != VRML97_TOK_ID) {
        return VRML97_ERR_SYNTAX;
    }
    if (vrml97_streq(r->tok.text, "NULL")) {
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "USE")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        idx = vrml97_find_def(scene, r->tok.text, VRML97_DEF_NORMAL);
        if (idx >= 0) {
            return vrml97_copy_def_vec3(scene, idx, scene->work_normal, VRML97_MAX_NORMALS, &scene->work_normal_count);
        }
        vrml97_warn(scene, VRML97_WARN_BAD_USE);
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "DEF")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        vrml97_copy_name(def_name, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
    }
    if (!vrml97_streq(r->tok.text, "Normal")) {
        vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_NODE);
        vrml97_skip_value(r);
        return VRML97_OK;
    }
    if (vrml97_parse_normal_body(scene, r) != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    if (def_name[0] != '\0') {
        vrml97_register_def_vec3(scene, def_name, VRML97_DEF_NORMAL, scene->work_normal, scene->work_normal_count);
    }
    return VRML97_OK;
}

static int vrml97_parse_color_body(vrml97_scene *scene, vrml97_reader *r)
{
    int t;
    char field[VRML97_NAME_MAX];
    scene->work_color_count = 0;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Color body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "color")) {
            if (vrml97_parse_vec3_array(scene, r, scene->work_color, VRML97_MAX_COLORS, &scene->work_color_count) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    return VRML97_OK;
}

static int vrml97_parse_color_node(vrml97_scene *scene, vrml97_reader *r)
{
    int t;
    char def_name[VRML97_NAME_MAX];
    int idx;

    scene->work_color_count = 0;
    def_name[0] = '\0';
    t = vrml97_next(r);
    if (t != VRML97_TOK_ID) {
        return VRML97_ERR_SYNTAX;
    }
    if (vrml97_streq(r->tok.text, "NULL")) {
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "USE")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        idx = vrml97_find_def(scene, r->tok.text, VRML97_DEF_COLOR);
        if (idx >= 0) {
            return vrml97_copy_def_vec3(scene, idx, scene->work_color, VRML97_MAX_COLORS, &scene->work_color_count);
        }
        vrml97_warn(scene, VRML97_WARN_BAD_USE);
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "DEF")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        vrml97_copy_name(def_name, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
    }
    if (!vrml97_streq(r->tok.text, "Color")) {
        vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_NODE);
        vrml97_skip_value(r);
        return VRML97_OK;
    }
    if (vrml97_parse_color_body(scene, r) != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    if (def_name[0] != '\0') {
        vrml97_register_def_vec3(scene, def_name, VRML97_DEF_COLOR, scene->work_color, scene->work_color_count);
    }
    return VRML97_OK;
}

static int vrml97_parse_texcoord_body(vrml97_scene *scene, vrml97_reader *r)
{
    int t;
    char field[VRML97_NAME_MAX];
    scene->work_texcoord_count = 0;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected TextureCoordinate body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "point")) {
            if (vrml97_parse_vec2_array(scene, r, scene->work_texcoord, VRML97_MAX_TEXCOORDS, &scene->work_texcoord_count) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    return VRML97_OK;
}

static int vrml97_parse_texcoord_node(vrml97_scene *scene, vrml97_reader *r)
{
    int t;
    char def_name[VRML97_NAME_MAX];
    int idx;

    scene->work_texcoord_count = 0;
    def_name[0] = '\0';
    t = vrml97_next(r);
    if (t != VRML97_TOK_ID) {
        return VRML97_ERR_SYNTAX;
    }
    if (vrml97_streq(r->tok.text, "NULL")) {
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "USE")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        idx = vrml97_find_def(scene, r->tok.text, VRML97_DEF_TEXCOORD);
        if (idx >= 0) {
            return vrml97_copy_def_vec2(scene, idx, scene->work_texcoord, VRML97_MAX_TEXCOORDS, &scene->work_texcoord_count);
        }
        vrml97_warn(scene, VRML97_WARN_BAD_USE);
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "DEF")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        vrml97_copy_name(def_name, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
    }
    if (!vrml97_streq(r->tok.text, "TextureCoordinate")) {
        vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_NODE);
        vrml97_skip_value(r);
        return VRML97_OK;
    }
    if (vrml97_parse_texcoord_body(scene, r) != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    if (def_name[0] != '\0') {
        vrml97_register_def_vec2(scene, def_name, VRML97_DEF_TEXCOORD, scene->work_texcoord, scene->work_texcoord_count);
    }
    return VRML97_OK;
}

static int vrml97_begin_mesh(vrml97_scene *scene, const char *name, int material, int texture)
{
    int id;
    if (scene->mesh_count >= VRML97_MAX_MESHES) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return -1;
    }
    id = scene->mesh_count++;
    memset(&scene->meshes[id], 0, sizeof(scene->meshes[id]));
    if (name != 0 && name[0] != '\0') {
        vrml97_copy_name(scene->meshes[id].name, name, VRML97_NAME_MAX);
    } else {
        vrml97_copy_name(scene->meshes[id].name, "mesh", VRML97_NAME_MAX);
    }
    scene->meshes[id].first_triangle = scene->triangles_count;
    scene->meshes[id].triangle_count = 0;
    scene->meshes[id].material = material;
    scene->meshes[id].texture = texture;
    return id;
}

static void vrml97_end_mesh(vrml97_scene *scene, int mesh_id)
{
    if (mesh_id >= 0 && mesh_id < scene->mesh_count) {
        scene->meshes[mesh_id].triangle_count = scene->triangles_count - scene->meshes[mesh_id].first_triangle;
    }
}

static int vrml97_emit_triangle(vrml97_scene *scene, int mesh_id, vrml97_triangle *tri)
{
    vrml97_mat4 *m;
    int i;
    if (scene->triangles_count >= VRML97_MAX_TRIANGLES) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_ERR_OVERFLOW;
    }
    m = &scene->matrix_stack[scene->matrix_top];
    for (i = 0; i < 3; ++i) {
        tri->p[i] = vrml97_mat_point(m, tri->p[i]);
        if (tri->has_n) {
            tri->n[i] = vrml97_mat_vector(m, tri->n[i]);
        }
        if (tri->has_tc && scene->current_tex_transform.active) {
            tri->tc[i] = vrml97_apply_tex_transform_one(&scene->current_tex_transform, tri->tc[i]);
        }
    }
    scene->triangles[scene->triangles_count++] = *tri;
    if (mesh_id >= 0 && mesh_id < scene->mesh_count) {
        scene->meshes[mesh_id].triangle_count++;
    }
    return VRML97_OK;
}

static int vrml97_emit_line(vrml97_scene *scene, vrml97_line *line)
{
    vrml97_mat4 *m;
    int i;
    if (scene->lines_count >= VRML97_MAX_LINES) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_ERR_OVERFLOW;
    }
    m = &scene->matrix_stack[scene->matrix_top];
    for (i = 0; i < 2; ++i) {
        line->p[i] = vrml97_mat_point(m, line->p[i]);
    }
    scene->lines[scene->lines_count++] = *line;
    return VRML97_OK;
}

static int vrml97_emit_point(vrml97_scene *scene, vrml97_point *point)
{
    vrml97_mat4 *m;
    if (scene->points_count >= VRML97_MAX_POINTS) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_ERR_OVERFLOW;
    }
    m = &scene->matrix_stack[scene->matrix_top];
    point->p = vrml97_mat_point(m, point->p);
    scene->points[scene->points_count++] = *point;
    return VRML97_OK;
}

static vrml97_vec3 vrml97_flat_normal(vrml97_vec3 a, vrml97_vec3 b, vrml97_vec3 c)
{
    vrml97_vec3 u;
    vrml97_vec3 v;
    u = vrml97_vec3_sub(b, a);
    v = vrml97_vec3_sub(c, a);
    return vrml97_vec3_normalize(vrml97_vec3_cross(u, v));
}

static int vrml97_safe_index(int idx, int count)
{
    if (idx < 0 || idx >= count) {
        return -1;
    }
    return idx;
}

static void vrml97_pick_tex(vrml97_scene *scene, int list_pos, int coord_idx, int has_tex_index, vrml97_vec2 *tc, int *has_tc)
{
    int idx;
    idx = has_tex_index ? scene->work_texcoord_index[list_pos] : coord_idx;
    idx = vrml97_safe_index(idx, scene->work_texcoord_count);
    if (idx >= 0) {
        *tc = scene->work_texcoord[idx];
        *has_tc = VRML97_TRUE;
    }
}

static void vrml97_pick_normal_vertex(vrml97_scene *scene, int list_pos, int coord_idx, int has_normal_index, vrml97_vec3 *n, int *has_n)
{
    int idx;
    idx = has_normal_index ? scene->work_normal_index[list_pos] : coord_idx;
    idx = vrml97_safe_index(idx, scene->work_normal_count);
    if (idx >= 0) {
        *n = scene->work_normal[idx];
        *has_n = VRML97_TRUE;
    }
}

static void vrml97_pick_color_vertex(vrml97_scene *scene, int list_pos, int coord_idx, int has_color_index, vrml97_vec3 *c, int *has_c)
{
    int idx;
    idx = has_color_index ? scene->work_color_index[list_pos] : coord_idx;
    idx = vrml97_safe_index(idx, scene->work_color_count);
    if (idx >= 0) {
        *c = scene->work_color[idx];
        *has_c = VRML97_TRUE;
    }
}

static void vrml97_pick_normal_face(vrml97_scene *scene, int face_idx, int has_normal_index, vrml97_vec3 *n, int *has_n)
{
    int idx;
    idx = has_normal_index ? scene->work_normal_index[face_idx] : face_idx;
    idx = vrml97_safe_index(idx, scene->work_normal_count);
    if (idx >= 0) {
        *n = scene->work_normal[idx];
        *has_n = VRML97_TRUE;
    }
}

static void vrml97_pick_color_face(vrml97_scene *scene, int face_idx, int has_color_index, vrml97_vec3 *c, int *has_c)
{
    int idx;
    idx = has_color_index ? scene->work_color_index[face_idx] : face_idx;
    idx = vrml97_safe_index(idx, scene->work_color_count);
    if (idx >= 0) {
        *c = scene->work_color[idx];
        *has_c = VRML97_TRUE;
    }
}

static vrml97_fx vrml97_face_cross2(vrml97_vec2 a, vrml97_vec2 b, vrml97_vec2 c)
{
    vrml97_fx abx;
    vrml97_fx aby;
    vrml97_fx acx;
    vrml97_fx acy;
    abx = b.x - a.x;
    aby = b.y - a.y;
    acx = c.x - a.x;
    acy = c.y - a.y;
    return vrml97_fx_mul(abx, acy) - vrml97_fx_mul(aby, acx);
}

static vrml97_vec2 vrml97_project_face_point(vrml97_vec3 p, int axis)
{
    vrml97_vec2 r;
    if (axis == 0) {
        r.x = p.y;
        r.y = p.z;
    } else if (axis == 1) {
        r.x = p.x;
        r.y = p.z;
    } else {
        r.x = p.x;
        r.y = p.y;
    }
    return r;
}

static int vrml97_pick_projection_axis(vrml97_scene *scene, int *face_indices, int face_count)
{
    int i;
    int ai;
    int bi;
    int ci;
    vrml97_vec3 n;
    long ax;
    long ay;
    long az;

    for (i = 1; i < face_count - 1; ++i) {
        ai = face_indices[0];
        bi = face_indices[i];
        ci = face_indices[i + 1];
        if (vrml97_safe_index(ai, scene->work_coord_count) < 0 || vrml97_safe_index(bi, scene->work_coord_count) < 0 || vrml97_safe_index(ci, scene->work_coord_count) < 0) {
            continue;
        }
        n = vrml97_flat_normal(scene->work_coord[ai], scene->work_coord[bi], scene->work_coord[ci]);
        ax = vrml97_abs_long(n.x);
        ay = vrml97_abs_long(n.y);
        az = vrml97_abs_long(n.z);
        if (ax >= ay && ax >= az) return 0;
        if (ay >= ax && ay >= az) return 1;
        return 2;
    }
    return 2;
}

static vrml97_fx vrml97_polygon_area2(vrml97_vec2 *pts, int *order, int count)
{
    vrml97_fx a;
    int i;
    int j;
    a = 0;
    for (i = 0; i < count; ++i) {
        j = (i + 1) % count;
        a += vrml97_fx_mul(pts[order[i]].x, pts[order[j]].y) - vrml97_fx_mul(pts[order[j]].x, pts[order[i]].y);
    }
    return a;
}

static int vrml97_point_inside_tri2(vrml97_vec2 a, vrml97_vec2 b, vrml97_vec2 c, vrml97_vec2 p, int positive)
{
    vrml97_fx c1;
    vrml97_fx c2;
    vrml97_fx c3;
    c1 = vrml97_face_cross2(a, b, p);
    c2 = vrml97_face_cross2(b, c, p);
    c3 = vrml97_face_cross2(c, a, p);
    if (positive) {
        return (c1 >= 0 && c2 >= 0 && c3 >= 0) ? VRML97_TRUE : VRML97_FALSE;
    }
    return (c1 <= 0 && c2 <= 0 && c3 <= 0) ? VRML97_TRUE : VRML97_FALSE;
}

static int vrml97_is_ear2(vrml97_vec2 *pts, int *order, int count, int pos, int positive)
{
    int prev;
    int curr;
    int next;
    int i;
    int oi;
    vrml97_fx cross;

    prev = order[(pos + count - 1) % count];
    curr = order[pos];
    next = order[(pos + 1) % count];
    cross = vrml97_face_cross2(pts[prev], pts[curr], pts[next]);
    if (positive) {
        if (cross <= 0) return VRML97_FALSE;
    } else {
        if (cross >= 0) return VRML97_FALSE;
    }
    for (i = 0; i < count; ++i) {
        oi = order[i];
        if (oi == prev || oi == curr || oi == next) {
            continue;
        }
        if (vrml97_point_inside_tri2(pts[prev], pts[curr], pts[next], pts[oi], positive)) {
            return VRML97_FALSE;
        }
    }
    return VRML97_TRUE;
}

static int vrml97_emit_ifs_tri(vrml97_scene *scene, int mesh_id, int *face_indices, int *face_pos, int face_number, int material, int texture, int ccw, int solid, int color_per_vertex, int normal_per_vertex, int a, int b, int c)
{
    int ai;
    int bi;
    int ci;
    int ap;
    int bp;
    int cp;
    int tmp;
    vrml97_triangle tri;
    vrml97_vec3 flat;
    vrml97_vec3 face_n;
    vrml97_vec3 face_c;
    int has_face_n;
    int has_face_c;
    int has_tex_index;
    int has_norm_index;
    int has_color_index;

    if (!ccw) {
        tmp = b;
        b = c;
        c = tmp;
    }
    ai = face_indices[a];
    bi = face_indices[b];
    ci = face_indices[c];
    ap = face_pos[a];
    bp = face_pos[b];
    cp = face_pos[c];
    if (vrml97_safe_index(ai, scene->work_coord_count) < 0 || vrml97_safe_index(bi, scene->work_coord_count) < 0 || vrml97_safe_index(ci, scene->work_coord_count) < 0) {
        return VRML97_OK;
    }

    has_tex_index = scene->work_texcoord_index_count > 0 ? VRML97_TRUE : VRML97_FALSE;
    has_norm_index = scene->work_normal_index_count > 0 ? VRML97_TRUE : VRML97_FALSE;
    has_color_index = scene->work_color_index_count > 0 ? VRML97_TRUE : VRML97_FALSE;
    has_face_n = VRML97_FALSE;
    has_face_c = VRML97_FALSE;
    face_n = vrml97_vec3_make(0, 0, VRML97_FX_ONE);
    face_c = vrml97_vec3_make(VRML97_FX_ONE, VRML97_FX_ONE, VRML97_FX_ONE);

    if (!normal_per_vertex && scene->work_normal_count > 0) {
        vrml97_pick_normal_face(scene, face_number, has_norm_index, &face_n, &has_face_n);
    }
    if (!color_per_vertex && scene->work_color_count > 0) {
        vrml97_pick_color_face(scene, face_number, has_color_index, &face_c, &has_face_c);
    }

    memset(&tri, 0, sizeof(tri));
    tri.p[0] = scene->work_coord[ai];
    tri.p[1] = scene->work_coord[bi];
    tri.p[2] = scene->work_coord[ci];
    tri.material = material;
    tri.texture = texture;
    tri.ccw = ccw;
    tri.solid = solid;

    tri.has_tc = VRML97_FALSE;
    if (scene->work_texcoord_count > 0) {
        vrml97_pick_tex(scene, ap, ai, has_tex_index, &tri.tc[0], &tri.has_tc);
        vrml97_pick_tex(scene, bp, bi, has_tex_index, &tri.tc[1], &tri.has_tc);
        vrml97_pick_tex(scene, cp, ci, has_tex_index, &tri.tc[2], &tri.has_tc);
    }

    tri.has_n = VRML97_FALSE;
    if (normal_per_vertex && scene->work_normal_count > 0) {
        vrml97_pick_normal_vertex(scene, ap, ai, has_norm_index, &tri.n[0], &tri.has_n);
        vrml97_pick_normal_vertex(scene, bp, bi, has_norm_index, &tri.n[1], &tri.has_n);
        vrml97_pick_normal_vertex(scene, cp, ci, has_norm_index, &tri.n[2], &tri.has_n);
    } else if (has_face_n) {
        tri.n[0] = face_n;
        tri.n[1] = face_n;
        tri.n[2] = face_n;
        tri.has_n = VRML97_TRUE;
    } else {
        flat = vrml97_flat_normal(tri.p[0], tri.p[1], tri.p[2]);
        tri.n[0] = flat;
        tri.n[1] = flat;
        tri.n[2] = flat;
        tri.has_n = VRML97_TRUE;
        vrml97_warn(scene, VRML97_WARN_MISSING_NORMALS);
    }

    tri.has_c = VRML97_FALSE;
    if (color_per_vertex && scene->work_color_count > 0) {
        vrml97_pick_color_vertex(scene, ap, ai, has_color_index, &tri.c[0], &tri.has_c);
        vrml97_pick_color_vertex(scene, bp, bi, has_color_index, &tri.c[1], &tri.has_c);
        vrml97_pick_color_vertex(scene, cp, ci, has_color_index, &tri.c[2], &tri.has_c);
    } else if (has_face_c) {
        tri.c[0] = face_c;
        tri.c[1] = face_c;
        tri.c[2] = face_c;
        tri.has_c = VRML97_TRUE;
    }

    return vrml97_emit_triangle(scene, mesh_id, &tri);
}

static int vrml97_emit_ifs_face(vrml97_scene *scene, int mesh_id, int *face_indices, int *face_pos, int face_count, int face_number, int material, int texture, int ccw, int solid, int convex, int color_per_vertex, int normal_per_vertex)
{
    int i;
    int axis;
    int order[VRML97_MAX_FACE_VERTS];
    vrml97_vec2 pts[VRML97_MAX_FACE_VERTS];
    vrml97_fx area;
    int positive;
    int remaining;
    int guard;
    int ear_pos;
    int found;
    int j;
    int prev;
    int curr;
    int next;
    int idx;

    if (face_count < 3) {
        return VRML97_OK;
    }
    if (convex || face_count == 3) {
        for (i = 1; i < face_count - 1; ++i) {
            if (vrml97_emit_ifs_tri(scene, mesh_id, face_indices, face_pos, face_number, material, texture, ccw, solid, color_per_vertex, normal_per_vertex, 0, i, i + 1) != VRML97_OK) {
                return VRML97_ERR_OVERFLOW;
            }
        }
        return VRML97_OK;
    }

    scene->concave_face_count++;
    axis = vrml97_pick_projection_axis(scene, face_indices, face_count);
    for (i = 0; i < face_count; ++i) {
        idx = face_indices[i];
        if (vrml97_safe_index(idx, scene->work_coord_count) < 0) {
            goto fan_fallback;
        }
        pts[i] = vrml97_project_face_point(scene->work_coord[idx], axis);
        order[i] = i;
    }
    area = vrml97_polygon_area2(pts, order, face_count);
    if (area == 0) {
        goto fan_fallback;
    }
    positive = area > 0 ? VRML97_TRUE : VRML97_FALSE;
    remaining = face_count;
    guard = 0;
    while (remaining > 3 && guard < VRML97_MAX_FACE_VERTS * VRML97_MAX_FACE_VERTS) {
        found = VRML97_FALSE;
        for (ear_pos = 0; ear_pos < remaining; ++ear_pos) {
            if (vrml97_is_ear2(pts, order, remaining, ear_pos, positive)) {
                prev = order[(ear_pos + remaining - 1) % remaining];
                curr = order[ear_pos];
                next = order[(ear_pos + 1) % remaining];
                if (vrml97_emit_ifs_tri(scene, mesh_id, face_indices, face_pos, face_number, material, texture, ccw, solid, color_per_vertex, normal_per_vertex, prev, curr, next) != VRML97_OK) {
                    return VRML97_ERR_OVERFLOW;
                }
                for (j = ear_pos; j < remaining - 1; ++j) {
                    order[j] = order[j + 1];
                }
                remaining--;
                found = VRML97_TRUE;
                break;
            }
        }
        if (!found) {
            goto fan_fallback;
        }
        guard++;
    }
    if (remaining == 3) {
        if (vrml97_emit_ifs_tri(scene, mesh_id, face_indices, face_pos, face_number, material, texture, ccw, solid, color_per_vertex, normal_per_vertex, order[0], order[1], order[2]) != VRML97_OK) {
            return VRML97_ERR_OVERFLOW;
        }
    }
    return VRML97_OK;

fan_fallback:
    vrml97_warn(scene, VRML97_WARN_NONCONVEX_HINT);
    for (i = 1; i < face_count - 1; ++i) {
        if (vrml97_emit_ifs_tri(scene, mesh_id, face_indices, face_pos, face_number, material, texture, ccw, solid, color_per_vertex, normal_per_vertex, 0, i, i + 1) != VRML97_OK) {
            return VRML97_ERR_OVERFLOW;
        }
    }
    return VRML97_OK;
}

static int vrml97_emit_ifs(vrml97_scene *scene, int material, int texture, const char *name, int ccw, int solid, int convex, int color_per_vertex, int normal_per_vertex)
{
    int mesh_id;
    int face_indices[VRML97_MAX_FACE_VERTS];
    int face_pos[VRML97_MAX_FACE_VERTS];
    int face_count;
    int face_number;
    int i;
    int idx;
    int status;

    (void)convex;
    if (!convex) {
        vrml97_warn(scene, VRML97_WARN_NONCONVEX_HINT);
    }
    mesh_id = vrml97_begin_mesh(scene, name, material, texture);
    if (mesh_id < 0) {
        return VRML97_ERR_OVERFLOW;
    }
    face_count = 0;
    face_number = 0;
    for (i = 0; i < scene->work_coord_index_count; ++i) {
        idx = scene->work_coord_index[i];
        if (idx < 0) {
            status = vrml97_emit_ifs_face(scene, mesh_id, face_indices, face_pos, face_count, face_number, material, texture, ccw, solid, convex, color_per_vertex, normal_per_vertex);
            if (status != VRML97_OK) {
                return status;
            }
            face_count = 0;
            face_number++;
        } else {
            if (face_count < VRML97_MAX_FACE_VERTS) {
                face_indices[face_count] = idx;
                face_pos[face_count] = i;
                face_count++;
            } else {
                vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
            }
        }
    }
    if (face_count > 0) {
        status = vrml97_emit_ifs_face(scene, mesh_id, face_indices, face_pos, face_count, face_number, material, texture, ccw, solid, convex, color_per_vertex, normal_per_vertex);
        if (status != VRML97_OK) {
            return status;
        }
    }
    vrml97_end_mesh(scene, mesh_id);
    return VRML97_OK;
}

static int vrml97_color_at(vrml97_scene *scene, int idx, vrml97_vec3 *out)
{
    idx = vrml97_safe_index(idx, scene->work_color_count);
    if (idx < 0) {
        return VRML97_FALSE;
    }
    *out = scene->work_color[idx];
    return VRML97_TRUE;
}

static int vrml97_parse_indexed_line_set(vrml97_scene *scene, vrml97_reader *r, int material, const char *name)
{
    int t;
    char field[VRML97_NAME_MAX];
    int color_per_vertex;
    int i;
    int idx;
    int prev_idx;
    int prev_pos;
    int polyline;
    int has_color_index;
    vrml97_line line;
    vrml97_vec3 c0;
    vrml97_vec3 c1;
    int ci0;
    int ci1;
    int status;
    (void)name;
    scene->work_coord_count = 0;
    scene->work_color_count = 0;
    scene->work_coord_index_count = 0;
    scene->work_color_index_count = 0;
    color_per_vertex = VRML97_TRUE;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected IndexedLineSet body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "coord")) {
            if (vrml97_parse_coordinate_node(scene, r) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "color")) {
            if (vrml97_parse_color_node(scene, r) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "coordIndex")) {
            if (vrml97_parse_int_array(scene, r, scene->work_coord_index, VRML97_MAX_INDICES, &scene->work_coord_index_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "colorIndex")) {
            if (vrml97_parse_int_array(scene, r, scene->work_color_index, VRML97_MAX_INDICES, &scene->work_color_index_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "colorPerVertex")) {
            if (vrml97_read_bool(r, &color_per_vertex) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    has_color_index = scene->work_color_index_count > 0 ? VRML97_TRUE : VRML97_FALSE;
    prev_idx = -1;
    prev_pos = -1;
    polyline = 0;
    for (i = 0; i < scene->work_coord_index_count; ++i) {
        idx = scene->work_coord_index[i];
        if (idx < 0) {
            prev_idx = -1;
            prev_pos = -1;
            polyline++;
            continue;
        }
        if (vrml97_safe_index(idx, scene->work_coord_count) < 0) {
            prev_idx = -1;
            prev_pos = -1;
            continue;
        }
        if (prev_idx >= 0) {
            memset(&line, 0, sizeof(line));
            line.p[0] = scene->work_coord[prev_idx];
            line.p[1] = scene->work_coord[idx];
            line.material = material;
            if (scene->work_color_count > 0) {
                if (color_per_vertex) {
                    ci0 = has_color_index ? (prev_pos < scene->work_color_index_count ? scene->work_color_index[prev_pos] : -1) : prev_idx;
                    ci1 = has_color_index ? (i < scene->work_color_index_count ? scene->work_color_index[i] : -1) : idx;
                    if (vrml97_color_at(scene, ci0, &c0) && vrml97_color_at(scene, ci1, &c1)) {
                        line.c[0] = c0;
                        line.c[1] = c1;
                        line.has_c = VRML97_TRUE;
                    }
                } else {
                    ci0 = has_color_index ? (polyline < scene->work_color_index_count ? scene->work_color_index[polyline] : -1) : polyline;
                    if (vrml97_color_at(scene, ci0, &c0)) {
                        line.c[0] = c0;
                        line.c[1] = c0;
                        line.has_c = VRML97_TRUE;
                    }
                }
            }
            status = vrml97_emit_line(scene, &line);
            if (status != VRML97_OK) return status;
        }
        prev_idx = idx;
        prev_pos = i;
    }
    return VRML97_OK;
}

static int vrml97_parse_point_set(vrml97_scene *scene, vrml97_reader *r, int material, const char *name)
{
    int t;
    char field[VRML97_NAME_MAX];
    int i;
    vrml97_point point;
    (void)name;
    scene->work_coord_count = 0;
    scene->work_color_count = 0;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected PointSet body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "coord")) {
            if (vrml97_parse_coordinate_node(scene, r) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "color")) {
            if (vrml97_parse_color_node(scene, r) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    for (i = 0; i < scene->work_coord_count; ++i) {
        memset(&point, 0, sizeof(point));
        point.p = scene->work_coord[i];
        point.material = material;
        if (i < scene->work_color_count) {
            point.c = scene->work_color[i];
            point.has_c = VRML97_TRUE;
        }
        if (vrml97_emit_point(scene, &point) != VRML97_OK) return VRML97_ERR_OVERFLOW;
    }
    return VRML97_OK;
}

static int vrml97_parse_indexed_face_set(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name)
{
    int t;
    char field[VRML97_NAME_MAX];
    int ccw;
    int solid;
    int convex;
    int color_per_vertex;
    int normal_per_vertex;
    vrml97_fx crease_angle;

    scene->work_coord_count = 0;
    scene->work_normal_count = 0;
    scene->work_color_count = 0;
    scene->work_texcoord_count = 0;
    scene->work_coord_index_count = 0;
    scene->work_normal_index_count = 0;
    scene->work_color_index_count = 0;
    scene->work_texcoord_index_count = 0;
    ccw = VRML97_TRUE;
    solid = VRML97_TRUE;
    convex = VRML97_TRUE;
    color_per_vertex = VRML97_TRUE;
    normal_per_vertex = VRML97_TRUE;
    crease_angle = 0;

    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected IndexedFaceSet body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "coord")) {
            if (vrml97_parse_coordinate_node(scene, r) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "normal")) {
            if (vrml97_parse_normal_node(scene, r) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "color")) {
            if (vrml97_parse_color_node(scene, r) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "texCoord")) {
            if (vrml97_parse_texcoord_node(scene, r) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "coordIndex")) {
            if (vrml97_parse_int_array(scene, r, scene->work_coord_index, VRML97_MAX_INDICES, &scene->work_coord_index_count) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "normalIndex")) {
            if (vrml97_parse_int_array(scene, r, scene->work_normal_index, VRML97_MAX_INDICES, &scene->work_normal_index_count) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "colorIndex")) {
            if (vrml97_parse_int_array(scene, r, scene->work_color_index, VRML97_MAX_INDICES, &scene->work_color_index_count) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "texCoordIndex")) {
            if (vrml97_parse_int_array(scene, r, scene->work_texcoord_index, VRML97_MAX_INDICES, &scene->work_texcoord_index_count) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "ccw")) {
            if (vrml97_read_bool(r, &ccw) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "solid")) {
            if (vrml97_read_bool(r, &solid) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "convex")) {
            if (vrml97_read_bool(r, &convex) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "colorPerVertex")) {
            if (vrml97_read_bool(r, &color_per_vertex) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "normalPerVertex")) {
            if (vrml97_read_bool(r, &normal_per_vertex) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "creaseAngle")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) {
                return VRML97_ERR_SYNTAX;
            }
            crease_angle = r->tok.fx;
            (void)crease_angle;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    return vrml97_emit_ifs(scene, material, texture, name, ccw, solid, convex, color_per_vertex, normal_per_vertex);
}

static void vrml97_make_tri(vrml97_triangle *tri, vrml97_vec3 a, vrml97_vec3 b, vrml97_vec3 c, int material, int texture, int ccw, int solid)
{
    vrml97_vec3 n;
    memset(tri, 0, sizeof(*tri));
    tri->p[0] = a;
    tri->p[1] = b;
    tri->p[2] = c;
    n = vrml97_flat_normal(a, b, c);
    tri->n[0] = n;
    tri->n[1] = n;
    tri->n[2] = n;
    tri->has_n = VRML97_TRUE;
    tri->material = material;
    tri->texture = texture;
    tri->ccw = ccw;
    tri->solid = solid;
}

static int vrml97_emit_quad(vrml97_scene *scene, int mesh_id, vrml97_vec3 a, vrml97_vec3 b, vrml97_vec3 c, vrml97_vec3 d, int material, int texture)
{
    vrml97_triangle tri;
    vrml97_make_tri(&tri, a, b, c, material, texture, VRML97_TRUE, VRML97_TRUE);
    if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) {
        return VRML97_ERR_OVERFLOW;
    }
    vrml97_make_tri(&tri, a, c, d, material, texture, VRML97_TRUE, VRML97_TRUE);
    if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) {
        return VRML97_ERR_OVERFLOW;
    }
    return VRML97_OK;
}

static int vrml97_parse_box(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_vec3 size;
    vrml97_fx hx;
    vrml97_fx hy;
    vrml97_fx hz;
    vrml97_vec3 p[8];
    int mesh_id;

    size = vrml97_vec3_make(VRML97_FX_ONE * 2L, VRML97_FX_ONE * 2L, VRML97_FX_ONE * 2L);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Box body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "size")) {
            if (vrml97_read_vec3(r, &size) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }

    hx = size.x / 2L;
    hy = size.y / 2L;
    hz = size.z / 2L;
    p[0] = vrml97_vec3_make(-hx, -hy, -hz);
    p[1] = vrml97_vec3_make(hx, -hy, -hz);
    p[2] = vrml97_vec3_make(hx, hy, -hz);
    p[3] = vrml97_vec3_make(-hx, hy, -hz);
    p[4] = vrml97_vec3_make(-hx, -hy, hz);
    p[5] = vrml97_vec3_make(hx, -hy, hz);
    p[6] = vrml97_vec3_make(hx, hy, hz);
    p[7] = vrml97_vec3_make(-hx, hy, hz);

    mesh_id = vrml97_begin_mesh(scene, name, material, texture);
    if (mesh_id < 0) {
        return VRML97_ERR_OVERFLOW;
    }
    if (vrml97_emit_quad(scene, mesh_id, p[5], p[4], p[7], p[6], material, texture) != VRML97_OK) return VRML97_ERR_OVERFLOW;
    if (vrml97_emit_quad(scene, mesh_id, p[0], p[1], p[2], p[3], material, texture) != VRML97_OK) return VRML97_ERR_OVERFLOW;
    if (vrml97_emit_quad(scene, mesh_id, p[4], p[0], p[3], p[7], material, texture) != VRML97_OK) return VRML97_ERR_OVERFLOW;
    if (vrml97_emit_quad(scene, mesh_id, p[1], p[5], p[6], p[2], material, texture) != VRML97_OK) return VRML97_ERR_OVERFLOW;
    if (vrml97_emit_quad(scene, mesh_id, p[3], p[2], p[6], p[7], material, texture) != VRML97_OK) return VRML97_ERR_OVERFLOW;
    if (vrml97_emit_quad(scene, mesh_id, p[4], p[5], p[1], p[0], material, texture) != VRML97_OK) return VRML97_ERR_OVERFLOW;
    vrml97_end_mesh(scene, mesh_id);
    return VRML97_OK;
}

static int vrml97_parse_sphere(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_fx radius;
    int mesh_id;
    int ring;
    int seg;
    vrml97_fx theta0;
    vrml97_fx theta1;
    vrml97_fx phi0;
    vrml97_fx phi1;
    vrml97_vec3 p00;
    vrml97_vec3 p01;
    vrml97_vec3 p10;
    vrml97_vec3 p11;
    vrml97_triangle tri;

    radius = VRML97_FX_ONE;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Sphere body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "radius")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) {
                return VRML97_ERR_SYNTAX;
            }
            radius = r->tok.fx;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    mesh_id = vrml97_begin_mesh(scene, name, material, texture);
    if (mesh_id < 0) {
        return VRML97_ERR_OVERFLOW;
    }
    for (ring = 0; ring < VRML97_SPHERE_RINGS; ++ring) {
        theta0 = -VRML97_HALF_PI + (VRML97_PI * ring) / VRML97_SPHERE_RINGS;
        theta1 = -VRML97_HALF_PI + (VRML97_PI * (ring + 1)) / VRML97_SPHERE_RINGS;
        for (seg = 0; seg < VRML97_PRIM_SEGMENTS; ++seg) {
            phi0 = (VRML97_TWO_PI * seg) / VRML97_PRIM_SEGMENTS;
            phi1 = (VRML97_TWO_PI * (seg + 1)) / VRML97_PRIM_SEGMENTS;
            p00 = vrml97_vec3_make(vrml97_fx_mul(radius, vrml97_fx_mul(vrml97_fx_cos(theta0), vrml97_fx_sin(phi0))), vrml97_fx_mul(radius, vrml97_fx_sin(theta0)), vrml97_fx_mul(radius, vrml97_fx_mul(vrml97_fx_cos(theta0), vrml97_fx_cos(phi0))));
            p01 = vrml97_vec3_make(vrml97_fx_mul(radius, vrml97_fx_mul(vrml97_fx_cos(theta0), vrml97_fx_sin(phi1))), vrml97_fx_mul(radius, vrml97_fx_sin(theta0)), vrml97_fx_mul(radius, vrml97_fx_mul(vrml97_fx_cos(theta0), vrml97_fx_cos(phi1))));
            p10 = vrml97_vec3_make(vrml97_fx_mul(radius, vrml97_fx_mul(vrml97_fx_cos(theta1), vrml97_fx_sin(phi0))), vrml97_fx_mul(radius, vrml97_fx_sin(theta1)), vrml97_fx_mul(radius, vrml97_fx_mul(vrml97_fx_cos(theta1), vrml97_fx_cos(phi0))));
            p11 = vrml97_vec3_make(vrml97_fx_mul(radius, vrml97_fx_mul(vrml97_fx_cos(theta1), vrml97_fx_sin(phi1))), vrml97_fx_mul(radius, vrml97_fx_sin(theta1)), vrml97_fx_mul(radius, vrml97_fx_mul(vrml97_fx_cos(theta1), vrml97_fx_cos(phi1))));
            if (ring > 0) {
                vrml97_make_tri(&tri, p00, p10, p11, material, texture, VRML97_TRUE, VRML97_TRUE);
                if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) return VRML97_ERR_OVERFLOW;
            }
            if (ring < VRML97_SPHERE_RINGS - 1) {
                vrml97_make_tri(&tri, p00, p11, p01, material, texture, VRML97_TRUE, VRML97_TRUE);
                if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) return VRML97_ERR_OVERFLOW;
            }
        }
    }
    vrml97_end_mesh(scene, mesh_id);
    return VRML97_OK;
}

static int vrml97_parse_cylinder(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_fx radius;
    vrml97_fx height;
    int side;
    int top;
    int bottom;
    int mesh_id;
    int seg;
    vrml97_fx a0;
    vrml97_fx a1;
    vrml97_vec3 p0;
    vrml97_vec3 p1;
    vrml97_vec3 p2;
    vrml97_vec3 p3;
    vrml97_vec3 center_top;
    vrml97_vec3 center_bottom;
    vrml97_triangle tri;
    vrml97_fx hy;

    radius = VRML97_FX_ONE;
    height = VRML97_FX_ONE * 2L;
    side = VRML97_TRUE;
    top = VRML97_TRUE;
    bottom = VRML97_TRUE;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Cylinder body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "radius")) { if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX; radius = r->tok.fx; }
        else if (vrml97_streq(field, "height")) { if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX; height = r->tok.fx; }
        else if (vrml97_streq(field, "side")) { if (vrml97_read_bool(r, &side) != VRML97_OK) return VRML97_ERR_SYNTAX; }
        else if (vrml97_streq(field, "top")) { if (vrml97_read_bool(r, &top) != VRML97_OK) return VRML97_ERR_SYNTAX; }
        else if (vrml97_streq(field, "bottom")) { if (vrml97_read_bool(r, &bottom) != VRML97_OK) return VRML97_ERR_SYNTAX; }
        else { vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD); vrml97_skip_value(r); }
    }
    mesh_id = vrml97_begin_mesh(scene, name, material, texture);
    if (mesh_id < 0) return VRML97_ERR_OVERFLOW;
    hy = height / 2L;
    center_top = vrml97_vec3_make(0, hy, 0);
    center_bottom = vrml97_vec3_make(0, -hy, 0);
    for (seg = 0; seg < VRML97_PRIM_SEGMENTS; ++seg) {
        a0 = (VRML97_TWO_PI * seg) / VRML97_PRIM_SEGMENTS;
        a1 = (VRML97_TWO_PI * (seg + 1)) / VRML97_PRIM_SEGMENTS;
        p0 = vrml97_vec3_make(vrml97_fx_mul(radius, vrml97_fx_sin(a0)), -hy, vrml97_fx_mul(radius, vrml97_fx_cos(a0)));
        p1 = vrml97_vec3_make(vrml97_fx_mul(radius, vrml97_fx_sin(a1)), -hy, vrml97_fx_mul(radius, vrml97_fx_cos(a1)));
        p2 = vrml97_vec3_make(vrml97_fx_mul(radius, vrml97_fx_sin(a1)), hy, vrml97_fx_mul(radius, vrml97_fx_cos(a1)));
        p3 = vrml97_vec3_make(vrml97_fx_mul(radius, vrml97_fx_sin(a0)), hy, vrml97_fx_mul(radius, vrml97_fx_cos(a0)));
        if (side) {
            if (vrml97_emit_quad(scene, mesh_id, p0, p1, p2, p3, material, texture) != VRML97_OK) return VRML97_ERR_OVERFLOW;
        }
        if (top) {
            vrml97_make_tri(&tri, center_top, p3, p2, material, texture, VRML97_TRUE, VRML97_TRUE);
            if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) return VRML97_ERR_OVERFLOW;
        }
        if (bottom) {
            vrml97_make_tri(&tri, center_bottom, p1, p0, material, texture, VRML97_TRUE, VRML97_TRUE);
            if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) return VRML97_ERR_OVERFLOW;
        }
    }
    vrml97_end_mesh(scene, mesh_id);
    return VRML97_OK;
}

static int vrml97_parse_cone(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_fx radius;
    vrml97_fx height;
    int side;
    int bottom;
    int mesh_id;
    int seg;
    vrml97_fx a0;
    vrml97_fx a1;
    vrml97_vec3 p0;
    vrml97_vec3 p1;
    vrml97_vec3 apex;
    vrml97_vec3 center_bottom;
    vrml97_triangle tri;
    vrml97_fx hy;

    radius = VRML97_FX_ONE;
    height = VRML97_FX_ONE * 2L;
    side = VRML97_TRUE;
    bottom = VRML97_TRUE;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Cone body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "bottomRadius")) { if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX; radius = r->tok.fx; }
        else if (vrml97_streq(field, "height")) { if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX; height = r->tok.fx; }
        else if (vrml97_streq(field, "side")) { if (vrml97_read_bool(r, &side) != VRML97_OK) return VRML97_ERR_SYNTAX; }
        else if (vrml97_streq(field, "bottom")) { if (vrml97_read_bool(r, &bottom) != VRML97_OK) return VRML97_ERR_SYNTAX; }
        else { vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD); vrml97_skip_value(r); }
    }
    mesh_id = vrml97_begin_mesh(scene, name, material, texture);
    if (mesh_id < 0) return VRML97_ERR_OVERFLOW;
    hy = height / 2L;
    apex = vrml97_vec3_make(0, hy, 0);
    center_bottom = vrml97_vec3_make(0, -hy, 0);
    for (seg = 0; seg < VRML97_PRIM_SEGMENTS; ++seg) {
        a0 = (VRML97_TWO_PI * seg) / VRML97_PRIM_SEGMENTS;
        a1 = (VRML97_TWO_PI * (seg + 1)) / VRML97_PRIM_SEGMENTS;
        p0 = vrml97_vec3_make(vrml97_fx_mul(radius, vrml97_fx_sin(a0)), -hy, vrml97_fx_mul(radius, vrml97_fx_cos(a0)));
        p1 = vrml97_vec3_make(vrml97_fx_mul(radius, vrml97_fx_sin(a1)), -hy, vrml97_fx_mul(radius, vrml97_fx_cos(a1)));
        if (side) {
            vrml97_make_tri(&tri, p0, p1, apex, material, texture, VRML97_TRUE, VRML97_TRUE);
            if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) return VRML97_ERR_OVERFLOW;
        }
        if (bottom) {
            vrml97_make_tri(&tri, center_bottom, p1, p0, material, texture, VRML97_TRUE, VRML97_TRUE);
            if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) return VRML97_ERR_OVERFLOW;
        }
    }
    vrml97_end_mesh(scene, mesh_id);
    return VRML97_OK;
}


static int vrml97_parse_elevation_grid(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name)
{
    int t;
    char field[VRML97_NAME_MAX];
    int xdim;
    int zdim;
    vrml97_fx xspacing;
    vrml97_fx zspacing;
    int ccw;
    int solid;
    int mesh_id;
    int x;
    int z;
    int i00;
    int i10;
    int i11;
    int i01;
    vrml97_vec3 p00;
    vrml97_vec3 p10;
    vrml97_vec3 p11;
    vrml97_vec3 p01;
    vrml97_triangle tri;

    scene->work_height_count = 0;
    xdim = 0;
    zdim = 0;
    xspacing = VRML97_FX_ONE;
    zspacing = VRML97_FX_ONE;
    ccw = VRML97_TRUE;
    solid = VRML97_TRUE;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected ElevationGrid body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "height")) {
            if (vrml97_parse_fx_array(scene, r, scene->work_height, VRML97_MAX_COORDS, &scene->work_height_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "xDimension")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            xdim = (int)r->tok.ivalue;
        } else if (vrml97_streq(field, "zDimension")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            zdim = (int)r->tok.ivalue;
        } else if (vrml97_streq(field, "xSpacing")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            xspacing = r->tok.fx;
        } else if (vrml97_streq(field, "zSpacing")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            zspacing = r->tok.fx;
        } else if (vrml97_streq(field, "ccw")) {
            if (vrml97_read_bool(r, &ccw) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "solid")) {
            if (vrml97_read_bool(r, &solid) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else {
            vrml97_skip_value(r);
        }
    }
    if (xdim < 2 || zdim < 2) return VRML97_OK;
    if (xdim * zdim > VRML97_MAX_COORDS) {
        vrml97_warn(scene, VRML97_WARN_LIMIT_REACHED);
        return VRML97_ERR_OVERFLOW;
    }
    mesh_id = vrml97_begin_mesh(scene, name, material, texture);
    if (mesh_id < 0) return VRML97_ERR_OVERFLOW;
    for (z = 0; z < zdim - 1; ++z) {
        for (x = 0; x < xdim - 1; ++x) {
            i00 = x + z * xdim;
            i10 = (x + 1) + z * xdim;
            i11 = (x + 1) + (z + 1) * xdim;
            i01 = x + (z + 1) * xdim;
            p00 = vrml97_vec3_make(vrml97_fx_mul(xspacing, vrml97_fx_from_int((long)x)), i00 < scene->work_height_count ? scene->work_height[i00] : 0, vrml97_fx_mul(zspacing, vrml97_fx_from_int((long)z)));
            p10 = vrml97_vec3_make(vrml97_fx_mul(xspacing, vrml97_fx_from_int((long)(x + 1))), i10 < scene->work_height_count ? scene->work_height[i10] : 0, vrml97_fx_mul(zspacing, vrml97_fx_from_int((long)z)));
            p11 = vrml97_vec3_make(vrml97_fx_mul(xspacing, vrml97_fx_from_int((long)(x + 1))), i11 < scene->work_height_count ? scene->work_height[i11] : 0, vrml97_fx_mul(zspacing, vrml97_fx_from_int((long)(z + 1))));
            p01 = vrml97_vec3_make(vrml97_fx_mul(xspacing, vrml97_fx_from_int((long)x)), i01 < scene->work_height_count ? scene->work_height[i01] : 0, vrml97_fx_mul(zspacing, vrml97_fx_from_int((long)(z + 1))));
            if (ccw) {
                vrml97_make_tri(&tri, p00, p01, p11, material, texture, ccw, solid);
                if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) return VRML97_ERR_OVERFLOW;
                vrml97_make_tri(&tri, p00, p11, p10, material, texture, ccw, solid);
                if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) return VRML97_ERR_OVERFLOW;
            } else {
                vrml97_make_tri(&tri, p00, p11, p01, material, texture, ccw, solid);
                if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) return VRML97_ERR_OVERFLOW;
                vrml97_make_tri(&tri, p00, p10, p11, material, texture, ccw, solid);
                if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) return VRML97_ERR_OVERFLOW;
            }
            vrml97_warn(scene, VRML97_WARN_MISSING_NORMALS);
        }
    }
    vrml97_end_mesh(scene, mesh_id);
    return VRML97_OK;
}

static vrml97_vec2 vrml97_extrusion_scale_at(vrml97_scene *scene, int spine_index)
{
    if (scene->work_scale_count <= 0) return vrml97_vec2_make(VRML97_FX_ONE, VRML97_FX_ONE);
    if (spine_index < scene->work_scale_count) return scene->work_scale[spine_index];
    return scene->work_scale[scene->work_scale_count - 1];
}

static vrml97_fx vrml97_extrusion_angle_at(vrml97_scene *scene, int spine_index)
{
    vrml97_vec3 axis;
    vrml97_fx angle;
    if (scene->work_orientation_count <= 0) return 0;
    if (spine_index < scene->work_orientation_count) {
        axis = scene->work_orientation_axis[spine_index];
        angle = scene->work_orientation_angle[spine_index];
    } else {
        axis = scene->work_orientation_axis[scene->work_orientation_count - 1];
        angle = scene->work_orientation_angle[scene->work_orientation_count - 1];
    }
    if (axis.z < 0) angle = -angle;
    if (axis.z == 0 && (axis.x != 0 || axis.y != 0)) return 0;
    return angle;
}

static vrml97_vec3 vrml97_extrusion_vertex(vrml97_scene *scene, int spine_index, int cross_index)
{
    vrml97_vec2 cs;
    vrml97_vec2 sc;
    vrml97_fx x;
    vrml97_fx z;
    vrml97_fx rx;
    vrml97_fx rz;
    vrml97_fx a;
    vrml97_fx c;
    vrml97_fx si;
    vrml97_vec3 base;
    cs = scene->work_cross_section[cross_index];
    sc = vrml97_extrusion_scale_at(scene, spine_index);
    x = vrml97_fx_mul(cs.x, sc.x);
    z = vrml97_fx_mul(cs.y, sc.y);
    a = vrml97_extrusion_angle_at(scene, spine_index);
    if (a != 0) {
        c = vrml97_fx_cos(a);
        si = vrml97_fx_sin(a);
        rx = vrml97_fx_mul(x, c) - vrml97_fx_mul(z, si);
        rz = vrml97_fx_mul(x, si) + vrml97_fx_mul(z, c);
        x = rx;
        z = rz;
    }
    base = scene->work_spine[spine_index];
    return vrml97_vec3_make(base.x + x, base.y, base.z + z);
}

static int vrml97_emit_extrusion_cap(vrml97_scene *scene, int mesh_id, int spine_index, int ring_count, int reverse, int material, int texture)
{
    int c;
    vrml97_vec3 v0;
    vrml97_vec3 v1;
    vrml97_vec3 v2;
    vrml97_triangle tri;
    if (ring_count < 3) return VRML97_OK;
    v0 = vrml97_extrusion_vertex(scene, spine_index, 0);
    for (c = 1; c < ring_count - 1; ++c) {
        if (reverse) {
            v1 = vrml97_extrusion_vertex(scene, spine_index, c + 1);
            v2 = vrml97_extrusion_vertex(scene, spine_index, c);
        } else {
            v1 = vrml97_extrusion_vertex(scene, spine_index, c);
            v2 = vrml97_extrusion_vertex(scene, spine_index, c + 1);
        }
        vrml97_make_tri(&tri, v0, v1, v2, material, texture, VRML97_TRUE, VRML97_TRUE);
        if (vrml97_emit_triangle(scene, mesh_id, &tri) != VRML97_OK) return VRML97_ERR_OVERFLOW;
    }
    return VRML97_OK;
}

static int vrml97_parse_extrusion(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name)
{
    int t;
    char field[VRML97_NAME_MAX];
    int begin_cap;
    int end_cap;
    int ccw;
    int solid;
    int spine_i;
    int cross_i;
    int cross_j;
    int ring_count;
    int mesh_id;
    int closed;
    vrml97_vec3 a;
    vrml97_vec3 b;
    vrml97_vec3 c;
    vrml97_vec3 d;
    int status;
    scene->work_cross_section_count = 5;
    scene->work_cross_section[0] = vrml97_vec2_make(-VRML97_FX_ONE, VRML97_FX_ONE);
    scene->work_cross_section[1] = vrml97_vec2_make(VRML97_FX_ONE, VRML97_FX_ONE);
    scene->work_cross_section[2] = vrml97_vec2_make(VRML97_FX_ONE, -VRML97_FX_ONE);
    scene->work_cross_section[3] = vrml97_vec2_make(-VRML97_FX_ONE, -VRML97_FX_ONE);
    scene->work_cross_section[4] = scene->work_cross_section[0];
    scene->work_spine_count = 2;
    scene->work_spine[0] = vrml97_vec3_make(0, 0, 0);
    scene->work_spine[1] = vrml97_vec3_make(0, VRML97_FX_ONE, 0);
    scene->work_scale_count = 1;
    scene->work_scale[0] = vrml97_vec2_make(VRML97_FX_ONE, VRML97_FX_ONE);
    scene->work_orientation_count = 1;
    scene->work_orientation_axis[0] = vrml97_vec3_make(0, 0, VRML97_FX_ONE);
    scene->work_orientation_angle[0] = 0;
    begin_cap = VRML97_TRUE;
    end_cap = VRML97_TRUE;
    ccw = VRML97_TRUE;
    solid = VRML97_TRUE;
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Extrusion body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "crossSection")) {
            if (vrml97_parse_vec2_array(scene, r, scene->work_cross_section, VRML97_MAX_CROSS_SECTION, &scene->work_cross_section_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "spine")) {
            if (vrml97_parse_vec3_array(scene, r, scene->work_spine, VRML97_MAX_SPINE, &scene->work_spine_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "scale")) {
            if (vrml97_parse_vec2_array(scene, r, scene->work_scale, VRML97_MAX_SPINE, &scene->work_scale_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "orientation")) {
            if (vrml97_parse_rotation_array(scene, r, scene->work_orientation_axis, scene->work_orientation_angle, VRML97_MAX_SPINE, &scene->work_orientation_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "beginCap")) {
            if (vrml97_read_bool(r, &begin_cap) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "endCap")) {
            if (vrml97_read_bool(r, &end_cap) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "ccw")) {
            if (vrml97_read_bool(r, &ccw) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "solid")) {
            if (vrml97_read_bool(r, &solid) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else {
            vrml97_skip_value(r);
        }
    }
    if (scene->work_spine_count < 2 || scene->work_cross_section_count < 2) return VRML97_OK;
    closed = vrml97_vec2_equal(scene->work_cross_section[0], scene->work_cross_section[scene->work_cross_section_count - 1]);
    ring_count = scene->work_cross_section_count;
    if (closed && ring_count > 1) ring_count--;
    if (ring_count < 2) return VRML97_OK;
    mesh_id = vrml97_begin_mesh(scene, name, material, texture);
    if (mesh_id < 0) return VRML97_ERR_OVERFLOW;
    for (spine_i = 0; spine_i < scene->work_spine_count - 1; ++spine_i) {
        for (cross_i = 0; cross_i < ring_count; ++cross_i) {
            cross_j = cross_i + 1;
            if (cross_j >= ring_count) {
                if (!closed) continue;
                cross_j = 0;
            }
            a = vrml97_extrusion_vertex(scene, spine_i, cross_i);
            b = vrml97_extrusion_vertex(scene, spine_i + 1, cross_i);
            c = vrml97_extrusion_vertex(scene, spine_i + 1, cross_j);
            d = vrml97_extrusion_vertex(scene, spine_i, cross_j);
            if (ccw) status = vrml97_emit_quad(scene, mesh_id, a, b, c, d, material, texture);
            else status = vrml97_emit_quad(scene, mesh_id, a, d, c, b, material, texture);
            if (status != VRML97_OK) return status;
        }
    }
    if (begin_cap && ring_count >= 3) {
        status = vrml97_emit_extrusion_cap(scene, mesh_id, 0, ring_count, ccw ? VRML97_TRUE : VRML97_FALSE, material, texture);
        if (status != VRML97_OK) return status;
    }
    if (end_cap && ring_count >= 3) {
        status = vrml97_emit_extrusion_cap(scene, mesh_id, scene->work_spine_count - 1, ring_count, ccw ? VRML97_FALSE : VRML97_TRUE, material, texture);
        if (status != VRML97_OK) return status;
    }
    vrml97_end_mesh(scene, mesh_id);
    vrml97_warn(scene, VRML97_WARN_APPROXIMATED_NODE);
    (void)solid;
    return VRML97_OK;
}

typedef struct vrml97_font_cfg_s {
    vrml97_fx size;
    vrml97_fx spacing;
    int horizontal;
    int left_to_right;
    int top_to_bottom;
} vrml97_font_cfg;

static void vrml97_font_default(vrml97_font_cfg *cfg)
{
    cfg->size = VRML97_FX_ONE;
    cfg->spacing = VRML97_FX_ONE;
    cfg->horizontal = VRML97_TRUE;
    cfg->left_to_right = VRML97_TRUE;
    cfg->top_to_bottom = VRML97_TRUE;
}

static int vrml97_parse_font_style_node(vrml97_scene *scene, vrml97_reader *r, vrml97_font_cfg *cfg)
{
    int t;
    char field[VRML97_NAME_MAX];
    char def_name[VRML97_NAME_MAX];
    (void)def_name;
    t = vrml97_next(r);
    if (t != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
    if (vrml97_streq(r->tok.text, "NULL") || vrml97_streq(r->tok.text, "USE")) {
        if (vrml97_streq(r->tok.text, "USE")) {
            if (vrml97_next(r) != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
            vrml97_warn(scene, VRML97_WARN_BAD_USE);
        }
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "DEF")) {
        if (vrml97_next(r) != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
        vrml97_copy_name(def_name, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_next(r) != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
    }
    if (!vrml97_streq(r->tok.text, "FontStyle")) {
        vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_NODE);
        vrml97_skip_value(r);
        return VRML97_OK;
    }
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected FontStyle body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "size")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            cfg->size = r->tok.fx;
        } else if (vrml97_streq(field, "spacing")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            cfg->spacing = r->tok.fx;
        } else if (vrml97_streq(field, "horizontal")) {
            if (vrml97_read_bool(r, &cfg->horizontal) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "leftToRight")) {
            if (vrml97_read_bool(r, &cfg->left_to_right) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "topToBottom")) {
            if (vrml97_read_bool(r, &cfg->top_to_bottom) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else {
            vrml97_skip_value(r);
        }
    }
    return VRML97_OK;
}

static int vrml97_emit_text_rect(vrml97_scene *scene, int mesh_id, vrml97_fx x0, vrml97_fx y0, vrml97_fx x1, vrml97_fx y1, int material, int texture)
{
    vrml97_vec3 a;
    vrml97_vec3 b;
    vrml97_vec3 c;
    vrml97_vec3 d;
    a = vrml97_vec3_make(x0, y0, 0);
    b = vrml97_vec3_make(x1, y0, 0);
    c = vrml97_vec3_make(x1, y1, 0);
    d = vrml97_vec3_make(x0, y1, 0);
    return vrml97_emit_quad(scene, mesh_id, a, b, c, d, material, texture);
}

static int vrml97_parse_text(vrml97_scene *scene, vrml97_reader *r, int material, int texture, const char *name)
{
    int t;
    char field[VRML97_NAME_MAX];
    char lines[VRML97_TEXT_MAX_LINES][VRML97_TEXT_LINE_MAX];
    int line_count;
    vrml97_fx length_values[VRML97_TEXT_MAX_LINES];
    int length_count;
    vrml97_fx max_extent;
    vrml97_font_cfg cfg;
    int mesh_id;
    int line_i;
    int char_i;
    int len;
    vrml97_fx base_char_w;
    vrml97_fx char_w;
    vrml97_fx line_h;
    vrml97_fx x;
    vrml97_fx y;
    int dir;
    int status;
    line_count = 0;
    length_count = 0;
    max_extent = 0;
    vrml97_font_default(&cfg);
    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Text body") != VRML97_OK) return VRML97_ERR_SYNTAX;
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) break;
        if (t == VRML97_TOK_EOF) return VRML97_ERR_SYNTAX;
        if (t != VRML97_TOK_ID) continue;
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "string")) {
            if (vrml97_parse_string_list(r, &lines[0][0], VRML97_TEXT_LINE_MAX, VRML97_TEXT_MAX_LINES, &line_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "length")) {
            if (vrml97_parse_fx_array(scene, r, length_values, VRML97_TEXT_MAX_LINES, &length_count) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "maxExtent")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            max_extent = r->tok.fx;
        } else if (vrml97_streq(field, "fontStyle")) {
            if (vrml97_parse_font_style_node(scene, r, &cfg) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    if (line_count <= 0) return VRML97_OK;
    mesh_id = vrml97_begin_mesh(scene, name, material, texture);
    if (mesh_id < 0) return VRML97_ERR_OVERFLOW;
    base_char_w = vrml97_fx_mul(cfg.size, (vrml97_fx)(VRML97_FX_ONE * 3L / 5L));
    line_h = cfg.size;
    dir = cfg.top_to_bottom ? -1 : 1;
    y = 0;
    for (line_i = 0; line_i < line_count; ++line_i) {
        char_w = base_char_w;
        len = (int)strlen(lines[line_i]);
        if (len <= 0) {
            y += vrml97_fx_mul(cfg.size, cfg.spacing) * (vrml97_fx)dir;
            continue;
        }
        if (line_i < length_count && length_values[line_i] > 0) {
            char_w = vrml97_fx_div(length_values[line_i], vrml97_fx_from_int((long)len));
        } else if (max_extent > 0 && vrml97_fx_mul(char_w, vrml97_fx_from_int((long)len)) > max_extent) {
            char_w = vrml97_fx_div(max_extent, vrml97_fx_from_int((long)len));
        }
        x = cfg.left_to_right ? 0 : vrml97_fx_mul(char_w, vrml97_fx_from_int((long)(len - 1)));
        for (char_i = 0; char_i < len; ++char_i) {
            if (lines[line_i][char_i] != ' ' && lines[line_i][char_i] != '\t') {
                if (cfg.left_to_right) status = vrml97_emit_text_rect(scene, mesh_id, x, y, x + char_w, y + line_h, material, texture);
                else status = vrml97_emit_text_rect(scene, mesh_id, x - char_w, y, x, y + line_h, material, texture);
                if (status != VRML97_OK) return status;
            }
            if (cfg.left_to_right) x += char_w;
            else x -= char_w;
        }
        y += vrml97_fx_mul(cfg.size, cfg.spacing) * (vrml97_fx)dir;
    }
    vrml97_end_mesh(scene, mesh_id);
    vrml97_warn(scene, VRML97_WARN_APPROXIMATED_NODE);
    (void)cfg.horizontal;
    return VRML97_OK;
}

static int vrml97_parse_geometry_span(vrml97_scene *scene, const char *src, unsigned long start, unsigned long end, int material, int texture, const char *name)
{
    vrml97_reader sub;
    int t;
    char def_name[VRML97_NAME_MAX];
    int idx;
    unsigned long node_start;
    unsigned long node_end;
    vrml97_reader_init(&sub, src, start, end);
    def_name[0] = '\0';
    t = vrml97_next(&sub);
    if (t != VRML97_TOK_ID) return VRML97_OK;
    if (vrml97_streq(sub.tok.text, "NULL")) return VRML97_OK;
    if (vrml97_streq(sub.tok.text, "USE")) {
        if (vrml97_next(&sub) != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
        idx = vrml97_find_def(scene, sub.tok.text, VRML97_DEF_GEOMETRY);
        if (idx >= 0) {
            node_start = (unsigned long)scene->defs[idx].start;
            node_end = node_start + (unsigned long)scene->defs[idx].count;
            return vrml97_parse_geometry_span(scene, src, node_start, node_end, material, texture, name);
        }
        vrml97_warn(scene, VRML97_WARN_BAD_USE);
        return VRML97_OK;
    }
    if (vrml97_streq(sub.tok.text, "DEF")) {
        if (vrml97_next(&sub) != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
        vrml97_copy_name(def_name, sub.tok.text, VRML97_NAME_MAX);
        if (vrml97_next(&sub) != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
        vrml97_register_def_span(scene, def_name, VRML97_DEF_GEOMETRY, start, end);
    }
    if (def_name[0] != '\0') name = def_name;
    if (vrml97_streq(sub.tok.text, "IndexedFaceSet")) return vrml97_parse_indexed_face_set(scene, &sub, material, texture, name);
    if (vrml97_streq(sub.tok.text, "IndexedLineSet")) return vrml97_parse_indexed_line_set(scene, &sub, material, name);
    if (vrml97_streq(sub.tok.text, "PointSet")) return vrml97_parse_point_set(scene, &sub, material, name);
    if (vrml97_streq(sub.tok.text, "Text")) return vrml97_parse_text(scene, &sub, material, texture, name);
    if (vrml97_streq(sub.tok.text, "Extrusion")) return vrml97_parse_extrusion(scene, &sub, material, texture, name);
    if (vrml97_streq(sub.tok.text, "ElevationGrid")) return vrml97_parse_elevation_grid(scene, &sub, material, texture, name);
    if (vrml97_streq(sub.tok.text, "Box")) return vrml97_parse_box(scene, &sub, material, texture, name);
    if (vrml97_streq(sub.tok.text, "Sphere")) return vrml97_parse_sphere(scene, &sub, material, texture, name);
    if (vrml97_streq(sub.tok.text, "Cylinder")) return vrml97_parse_cylinder(scene, &sub, material, texture, name);
    if (vrml97_streq(sub.tok.text, "Cone")) return vrml97_parse_cone(scene, &sub, material, texture, name);
    vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_NODE);
    return VRML97_OK;
}

static int vrml97_parse_shape(vrml97_scene *scene, vrml97_reader *r, const char *def_name)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_app app;
    unsigned long geo_start;
    unsigned long geo_end;
    int have_geo;
    vrml97_tex_transform old_tt;
    int status;

    app.material = 0;
    app.texture = -1;
    vrml97_default_tex_transform(&app.tex_transform);
    geo_start = 0;
    geo_end = 0;
    have_geo = VRML97_FALSE;

    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected Shape body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "appearance")) {
            if (vrml97_parse_appearance_node(scene, r, &app) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
        } else if (vrml97_streq(field, "geometry")) {
            if (vrml97_capture_node_span(r, &geo_start, &geo_end) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
            have_geo = VRML97_TRUE;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    if (have_geo) {
        old_tt = scene->current_tex_transform;
        scene->current_tex_transform = app.tex_transform;
        status = vrml97_parse_geometry_span(scene, r->src, geo_start, geo_end, app.material, app.texture, def_name);
        scene->current_tex_transform = old_tt;
        return status;
    }
    return VRML97_OK;
}

static int vrml97_parse_children_span(vrml97_scene *scene, const char *src, unsigned long start, unsigned long end)
{
    vrml97_reader sub;
    vrml97_reader_init(&sub, src, start, end);
    return vrml97_parse_nodes(scene, &sub);
}

static int vrml97_capture_children(vrml97_scene *scene, vrml97_reader *r, unsigned long *start, unsigned long *end, int *bracketed)
{
    int t;
    unsigned long s;
    unsigned long e;
    vrml97_skip_ws(r);
    if (vrml97_reader_ch(r) == '[') {
        vrml97_next(r);
        s = r->pos;
        if (vrml97_skip_compound(r, VRML97_TOK_LBRACKET, VRML97_TOK_RBRACKET) != VRML97_OK) {
            vrml97_set_error(scene, r, "unterminated children list");
            return VRML97_ERR_SYNTAX;
        }
        e = r->tok_start;
        *start = s;
        *end = e;
        *bracketed = VRML97_TRUE;
        return VRML97_OK;
    }
    t = vrml97_next(r);
    if (t == VRML97_TOK_ID && vrml97_streq(r->tok.text, "NULL")) {
        *start = r->tok_start;
        *end = r->tok_end;
        *bracketed = VRML97_FALSE;
        return VRML97_OK;
    }
    r->pos = r->tok_start;
    if (vrml97_capture_node_span(r, start, end) != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    *bracketed = VRML97_FALSE;
    return VRML97_OK;
}

static int vrml97_select_child_span(vrml97_scene *scene, const char *src, unsigned long start, unsigned long end, int which, unsigned long *out_start, unsigned long *out_end)
{
    vrml97_reader sub;
    unsigned long cs;
    unsigned long ce;
    int index;
    int status;

    if (which < 0) {
        *out_start = start;
        *out_end = start;
        return VRML97_FACE_END;
    }
    vrml97_reader_init(&sub, src, start, end);
    index = 0;
    for (;;) {
        vrml97_skip_ws(&sub);
        if (sub.pos >= sub.end) {
            break;
        }
        status = vrml97_capture_node_span(&sub, &cs, &ce);
        if (status != VRML97_OK) {
            vrml97_set_error(scene, &sub, "bad child span");
            return status;
        }
        if (index == which) {
            *out_start = cs;
            *out_end = ce;
            return VRML97_OK;
        }
        index++;
    }
    *out_start = start;
    *out_end = start;
    return VRML97_FACE_END;
}

static int vrml97_parse_group(vrml97_scene *scene, vrml97_reader *r, const char *def_name, int group_kind)
{
    int t;
    char field[VRML97_NAME_MAX];
    vrml97_transform tr;
    unsigned long children_start;
    unsigned long children_end;
    unsigned long selected_start;
    unsigned long selected_end;
    int have_children;
    int bracketed;
    int which_choice;
    int is_transform;
    vrml97_mat4 local;
    int status;

    (void)def_name;
    is_transform = group_kind == VRML97_GROUP_TRANSFORM ? VRML97_TRUE : VRML97_FALSE;
    vrml97_default_transform(&tr);
    children_start = 0;
    children_end = 0;
    selected_start = 0;
    selected_end = 0;
    have_children = VRML97_FALSE;
    bracketed = VRML97_FALSE;
    which_choice = -1;

    if (vrml97_expect(scene, r, VRML97_TOK_LBRACE, "expected grouping body") != VRML97_OK) {
        return VRML97_ERR_SYNTAX;
    }
    for (;;) {
        t = vrml97_next(r);
        if (t == VRML97_TOK_RBRACE) {
            break;
        }
        if (t == VRML97_TOK_EOF) {
            return VRML97_ERR_SYNTAX;
        }
        if (t != VRML97_TOK_ID) {
            continue;
        }
        vrml97_copy_name(field, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_streq(field, "children") || vrml97_streq(field, "choice") || vrml97_streq(field, "level")) {
            if (vrml97_capture_children(scene, r, &children_start, &children_end, &bracketed) != VRML97_OK) {
                return VRML97_ERR_SYNTAX;
            }
            have_children = VRML97_TRUE;
        } else if (is_transform && vrml97_streq(field, "translation")) {
            if (vrml97_read_vec3(r, &tr.translation) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (is_transform && vrml97_streq(field, "center")) {
            if (vrml97_read_vec3(r, &tr.center) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (is_transform && vrml97_streq(field, "scale")) {
            if (vrml97_read_vec3(r, &tr.scale) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (is_transform && vrml97_streq(field, "rotation")) {
            if (vrml97_read_rotation(r, &tr.rotation_axis, &tr.rotation_angle) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (is_transform && vrml97_streq(field, "scaleOrientation")) {
            if (vrml97_read_rotation(r, &tr.scale_axis, &tr.scale_angle) != VRML97_OK) return VRML97_ERR_SYNTAX;
        } else if (vrml97_streq(field, "bboxCenter") || vrml97_streq(field, "bboxSize") || vrml97_streq(field, "proxy") || vrml97_streq(field, "description") || vrml97_streq(field, "parameter")) {
            vrml97_skip_value(r);
        } else if (vrml97_streq(field, "range") || vrml97_streq(field, "center")) {
            vrml97_skip_value(r);
        } else if (vrml97_streq(field, "whichChoice")) {
            if (vrml97_next(r) != VRML97_TOK_NUMBER) return VRML97_ERR_SYNTAX;
            which_choice = (int)r->tok.ivalue;
        } else {
            vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_FIELD);
            vrml97_skip_value(r);
        }
    }
    if (have_children) {
        if (is_transform) {
            local = vrml97_make_transform(&tr);
        } else {
            vrml97_mat_identity(&local);
        }
        status = vrml97_push_matrix(scene, &local);
        if (status != VRML97_OK) {
            return status;
        }
        selected_start = children_start;
        selected_end = children_end;
        if (group_kind == VRML97_GROUP_SWITCH) {
            scene->static_choice_count++;
            vrml97_warn(scene, VRML97_WARN_STATIC_CHOICE);
            if (which_choice < 0) {
                vrml97_pop_matrix(scene);
                return VRML97_OK;
            }
            if (bracketed) {
                status = vrml97_select_child_span(scene, r->src, children_start, children_end, which_choice, &selected_start, &selected_end);
                if (status == VRML97_FACE_END) {
                    vrml97_pop_matrix(scene);
                    return VRML97_OK;
                }
                if (status != VRML97_OK) {
                    vrml97_pop_matrix(scene);
                    return status;
                }
            }
        } else if (group_kind == VRML97_GROUP_LOD) {
            scene->static_choice_count++;
            vrml97_warn(scene, VRML97_WARN_STATIC_CHOICE);
            if (bracketed) {
                status = vrml97_select_child_span(scene, r->src, children_start, children_end, 0, &selected_start, &selected_end);
                if (status == VRML97_FACE_END) {
                    vrml97_pop_matrix(scene);
                    return VRML97_OK;
                }
                if (status != VRML97_OK) {
                    vrml97_pop_matrix(scene);
                    return status;
                }
            }
        }
        status = vrml97_parse_children_span(scene, r->src, selected_start, selected_end);
        vrml97_pop_matrix(scene);
        return status;
    }
    return VRML97_OK;
}

static int vrml97_skip_proto_or_route(vrml97_scene *scene, vrml97_reader *r, const char *kw)
{
    int t;
    char proto_name[VRML97_NAME_MAX];
    unsigned long body_start;
    unsigned long body_end;

    proto_name[0] = '\0';
    vrml97_warn(scene, VRML97_WARN_ROUTE_PROTO_SKIPPED);
    if (vrml97_streq(kw, "ROUTE")) {
        scene->route_count++;
        t = vrml97_next(r);
        if (t != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
        vrml97_skip_ws(r);
        if (vrml97_reader_ch(r) == '.') {
            vrml97_next(r);
            if (vrml97_next(r) != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
        }
        t = vrml97_next(r);
        if (t != VRML97_TOK_ID || !vrml97_streq(r->tok.text, "TO")) return VRML97_ERR_SYNTAX;
        t = vrml97_next(r);
        if (t != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
        vrml97_skip_ws(r);
        if (vrml97_reader_ch(r) == '.') {
            vrml97_next(r);
            if (vrml97_next(r) != VRML97_TOK_ID) return VRML97_ERR_SYNTAX;
        }
        return VRML97_OK;
    }

    t = vrml97_next(r);
    if (t != VRML97_TOK_ID) {
        vrml97_skip_value(r);
        return VRML97_OK;
    }
    vrml97_copy_name(proto_name, r->tok.text, VRML97_NAME_MAX);

    vrml97_skip_ws(r);
    if (vrml97_reader_ch(r) == '[') {
        vrml97_next(r);
        if (vrml97_skip_compound(r, VRML97_TOK_LBRACKET, VRML97_TOK_RBRACKET) != VRML97_OK) return VRML97_ERR_SYNTAX;
    }

    vrml97_skip_ws(r);
    if (vrml97_streq(kw, "PROTO")) {
        scene->proto_count++;
        if (vrml97_reader_ch(r) == '{') {
            vrml97_next(r);
            body_start = r->pos;
            if (vrml97_skip_compound(r, VRML97_TOK_LBRACE, VRML97_TOK_RBRACE) != VRML97_OK) return VRML97_ERR_SYNTAX;
            body_end = r->tok_start;
            vrml97_register_def_span(scene, proto_name, VRML97_DEF_PROTO, body_start, body_end);
        }
    } else {
        scene->externproto_count++;
        vrml97_skip_value(r);
    }
    return VRML97_OK;
}

static int vrml97_parse_node_by_type(vrml97_scene *scene, vrml97_reader *r, const char *type_name, const char *def_name)
{
    if (vrml97_streq(type_name, "Shape")) {
        return vrml97_parse_shape(scene, r, def_name);
    }
    if (vrml97_streq(type_name, "Transform")) {
        return vrml97_parse_group(scene, r, def_name, VRML97_GROUP_TRANSFORM);
    }
    if (vrml97_streq(type_name, "LOD")) {
        return vrml97_parse_group(scene, r, def_name, VRML97_GROUP_LOD);
    }
    if (vrml97_streq(type_name, "Switch")) {
        return vrml97_parse_group(scene, r, def_name, VRML97_GROUP_SWITCH);
    }
    if (vrml97_streq(type_name, "Group") || vrml97_streq(type_name, "StaticGroup") || vrml97_streq(type_name, "Collision") || vrml97_streq(type_name, "Anchor") || vrml97_streq(type_name, "Billboard")) {
        return vrml97_parse_group(scene, r, def_name, VRML97_GROUP_NORMAL);
    }
    if (vrml97_streq(type_name, "WorldInfo")) {
        return vrml97_parse_world_info_node(scene, r, def_name);
    }
    if (vrml97_streq(type_name, "NavigationInfo")) {
        return vrml97_parse_navigation_info_node(scene, r, def_name);
    }
    if (vrml97_streq(type_name, "Viewpoint")) {
        return vrml97_parse_viewpoint_node(scene, r, def_name);
    }
    if (vrml97_streq(type_name, "DirectionalLight")) {
        return vrml97_parse_light_node(scene, r, def_name, VRML97_LIGHT_DIRECTIONAL);
    }
    if (vrml97_streq(type_name, "PointLight")) {
        return vrml97_parse_light_node(scene, r, def_name, VRML97_LIGHT_POINT);
    }
    if (vrml97_streq(type_name, "SpotLight")) {
        return vrml97_parse_light_node(scene, r, def_name, VRML97_LIGHT_SPOT);
    }
    if (vrml97_streq(type_name, "Background")) {
        return vrml97_parse_background_node(scene, r, def_name);
    }
    if (vrml97_streq(type_name, "Fog")) {
        return vrml97_parse_fog_node(scene, r, def_name);
    }
    if (vrml97_streq(type_name, "Inline")) {
        return vrml97_parse_inline_node(scene, r, def_name);
    }
    if (vrml97_streq(type_name, "Script") || vrml97_streq(type_name, "TimeSensor") || vrml97_streq(type_name, "TouchSensor") || vrml97_streq(type_name, "PlaneSensor") || vrml97_streq(type_name, "CylinderSensor") || vrml97_streq(type_name, "SphereSensor") || vrml97_streq(type_name, "ProximitySensor") || vrml97_streq(type_name, "VisibilitySensor") || vrml97_streq(type_name, "ColorInterpolator") || vrml97_streq(type_name, "CoordinateInterpolator") || vrml97_streq(type_name, "NormalInterpolator") || vrml97_streq(type_name, "OrientationInterpolator") || vrml97_streq(type_name, "PositionInterpolator") || vrml97_streq(type_name, "ScalarInterpolator") || vrml97_streq(type_name, "Sound") || vrml97_streq(type_name, "AudioClip")) {
        scene->runtime_node_count++;
        vrml97_warn(scene, VRML97_WARN_RUNTIME_NODE);
        vrml97_skip_value(r);
        return VRML97_OK;
    }
    if (vrml97_streq(type_name, "PROTO") || vrml97_streq(type_name, "EXTERNPROTO") || vrml97_streq(type_name, "ROUTE")) {
        return vrml97_skip_proto_or_route(scene, r, type_name);
    }
    {
        int proto_def;
        unsigned long proto_start;
        unsigned long proto_end;
        proto_def = vrml97_find_def(scene, type_name, VRML97_DEF_PROTO);
        if (proto_def >= 0) {
            vrml97_warn(scene, VRML97_WARN_PROTO_INSTANCE);
            scene->proto_instance_count++;
            vrml97_skip_ws(r);
            if (vrml97_reader_ch(r) == '{') {
                vrml97_next(r);
                if (vrml97_skip_compound(r, VRML97_TOK_LBRACE, VRML97_TOK_RBRACE) != VRML97_OK) return VRML97_ERR_SYNTAX;
            }
            proto_start = (unsigned long)scene->defs[proto_def].start;
            proto_end = proto_start + (unsigned long)scene->defs[proto_def].count;
            return vrml97_parse_children_span(scene, r->src, proto_start, proto_end);
        }
    }
    scene->unsupported_node_count++;
    vrml97_warn(scene, VRML97_WARN_UNSUPPORTED_NODE);
    vrml97_skip_value(r);
    return VRML97_OK;
}

static int vrml97_parse_node(vrml97_scene *scene, vrml97_reader *r)
{
    int t;
    char def_name[VRML97_NAME_MAX];
    char type_name[VRML97_NAME_MAX];

    def_name[0] = '\0';
    t = vrml97_next(r);
    if (t == VRML97_TOK_EOF) {
        return VRML97_FACE_END;
    }
    if (t != VRML97_TOK_ID) {
        return VRML97_OK;
    }
    if (vrml97_streq(r->tok.text, "DEF")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        vrml97_copy_name(def_name, r->tok.text, VRML97_NAME_MAX);
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        vrml97_copy_name(type_name, r->tok.text, VRML97_NAME_MAX);
        return vrml97_parse_node_by_type(scene, r, type_name, def_name);
    }
    if (vrml97_streq(r->tok.text, "USE")) {
        if (vrml97_next(r) != VRML97_TOK_ID) {
            return VRML97_ERR_SYNTAX;
        }
        vrml97_warn(scene, VRML97_WARN_BAD_USE);
        return VRML97_OK;
    }
    vrml97_copy_name(type_name, r->tok.text, VRML97_NAME_MAX);
    return vrml97_parse_node_by_type(scene, r, type_name, 0);
}

static int vrml97_parse_nodes(vrml97_scene *scene, vrml97_reader *r)
{
    int status;
    for (;;) {
        vrml97_skip_ws(r);
        if (r->pos >= r->end) {
            break;
        }
        status = vrml97_parse_node(scene, r);
        if (status == VRML97_FACE_END) {
            break;
        }
        if (status != VRML97_OK) {
            return status;
        }
    }
    return VRML97_OK;
}

static int vrml97_has_header(const char *data, unsigned long len)
{
    unsigned long i;
    const char *magic;
    magic = "#VRML V2.0 utf8";
    i = 0;
    while (i < len && (data[i] == ' ' || data[i] == '\t' || data[i] == '\r' || data[i] == '\n')) {
        i++;
    }
    if (i + strlen(magic) > len) {
        return VRML97_FALSE;
    }
    return memcmp(data + i, magic, strlen(magic)) == 0 ? VRML97_TRUE : VRML97_FALSE;
}

int vrml97_parse(vrml97_scene *scene, const char *data, unsigned long len)
{
    vrml97_reader r;
    int status;
    if (scene == 0 || data == 0) {
        return VRML97_ERR_NULL;
    }
    vrml97_scene_init(scene);
    if (!vrml97_has_header(data, len)) {
        vrml97_warn(scene, VRML97_WARN_MISSING_HEADER);
    }
    vrml97_reader_init(&r, data, 0, len);
    status = vrml97_parse_nodes(scene, &r);
    if (status != VRML97_OK) {
        if (scene->last_error[0] == '\0') {
            vrml97_set_error(scene, &r, vrml97_status_string(status));
        }
    }
    return status;
}

void vrml97_fx_to_text(vrml97_fx v, char *out, int out_cap, int digits)
{
    int neg;
    long whole;
    long rem;
    int pos;
    int i;
    int d;
    char tmp[32];
    int tpos;

    if (out_cap <= 0) {
        return;
    }
    out[0] = '\0';
    if (digits < 0) digits = 0;
    if (digits > 6) digits = 6;
    neg = VRML97_FALSE;
    if (v < 0) {
        neg = VRML97_TRUE;
        v = -v;
    }
    whole = v >> VRML97_FX_SHIFT;
    rem = v & (VRML97_FX_ONE - 1L);
    pos = 0;
    if (neg && pos < out_cap - 1) {
        out[pos++] = '-';
    }
    tpos = 0;
    if (whole == 0) {
        tmp[tpos++] = '0';
    } else {
        while (whole > 0 && tpos < (int)sizeof(tmp) - 1) {
            tmp[tpos++] = (char)('0' + (whole % 10L));
            whole /= 10L;
        }
    }
    while (tpos > 0 && pos < out_cap - 1) {
        out[pos++] = tmp[--tpos];
    }
    if (digits > 0 && pos < out_cap - 1) {
        out[pos++] = '.';
        for (i = 0; i < digits && pos < out_cap - 1; ++i) {
            rem *= 10L;
            d = (int)(rem >> VRML97_FX_SHIFT);
            rem = rem & (VRML97_FX_ONE - 1L);
            out[pos++] = (char)('0' + d);
        }
    }
    out[pos] = '\0';
}

static void vrml97_fprint_fx(FILE *f, vrml97_fx v)
{
    char b[48];
    vrml97_fx_to_text(v, b, (int)sizeof(b), 5);
    fputs(b, f);
}

static void vrml97_fprint_vec3(FILE *f, vrml97_vec3 v)
{
    vrml97_fprint_fx(f, v.x);
    fputc(' ', f);
    vrml97_fprint_fx(f, v.y);
    fputc(' ', f);
    vrml97_fprint_fx(f, v.z);
}

static void vrml97_fprint_vec2(FILE *f, vrml97_vec2 v)
{
    vrml97_fprint_fx(f, v.x);
    fputc(' ', f);
    vrml97_fprint_fx(f, v.y);
}

static int vrml97_write_mtl(const vrml97_scene *scene, const char *mtl_path)
{
    FILE *f;
    int i;
    if (mtl_path == 0 || mtl_path[0] == '\0') {
        return VRML97_OK;
    }
    f = fopen(mtl_path, "wb");
    if (f == 0) {
        return VRML97_ERR_IO;
    }
    fprintf(f, "# Generated by vrml97_codec C89 fixed-buffer decoder\n");
    for (i = 0; i < scene->material_count; ++i) {
        fprintf(f, "newmtl mat_%d\n", i);
        fputs("Ka ", f);
        vrml97_fprint_fx(f, scene->materials[i].ambient_intensity);
        fputc(' ', f);
        vrml97_fprint_fx(f, scene->materials[i].ambient_intensity);
        fputc(' ', f);
        vrml97_fprint_fx(f, scene->materials[i].ambient_intensity);
        fputc('\n', f);
        fputs("Kd ", f);
        vrml97_fprint_vec3(f, scene->materials[i].diffuse_color);
        fputc('\n', f);
        fputs("Ks ", f);
        vrml97_fprint_vec3(f, scene->materials[i].specular_color);
        fputc('\n', f);
        fputs("Ke ", f);
        vrml97_fprint_vec3(f, scene->materials[i].emissive_color);
        fputc('\n', f);
        fputs("Ns ", f);
        vrml97_fprint_fx(f, scene->materials[i].shininess);
        fputc('\n', f);
        fputs("d ", f);
        vrml97_fprint_fx(f, VRML97_FX_ONE - scene->materials[i].transparency);
        fputc('\n', f);
        fputc('\n', f);
    }
    fclose(f);
    return VRML97_OK;
}

static const char *vrml97_light_kind_name(int kind)
{
    if (kind == VRML97_LIGHT_DIRECTIONAL) return "DirectionalLight";
    if (kind == VRML97_LIGHT_POINT) return "PointLight";
    if (kind == VRML97_LIGHT_SPOT) return "SpotLight";
    return "Light";
}

int vrml97_write_obj_file(const vrml97_scene *scene, const char *obj_path, const char *mtl_path)
{
    FILE *f;
    unsigned long i;
    int j;
    unsigned long vi;
    unsigned long ti;
    unsigned long ni;
    int status;
    int last_mat;
    const vrml97_triangle *tri;
    const vrml97_line *line;
    const vrml97_point *point;
    char mtl_base[VRML97_URL_MAX];
    const char *p;

    if (scene == 0 || obj_path == 0) {
        return VRML97_ERR_NULL;
    }
    status = vrml97_write_mtl(scene, mtl_path);
    if (status != VRML97_OK) {
        return status;
    }
    f = fopen(obj_path, "wb");
    if (f == 0) {
        return VRML97_ERR_IO;
    }
    fprintf(f, "# Generated by vrml97_codec C89 fixed-buffer decoder\n");
    if (mtl_path != 0 && mtl_path[0] != '\0') {
        p = strrchr(mtl_path, '/');
        if (p == 0) {
            p = strrchr(mtl_path, '\\');
        }
        if (p != 0) {
            vrml97_copy_name(mtl_base, p + 1, VRML97_URL_MAX);
        } else {
            vrml97_copy_name(mtl_base, mtl_path, VRML97_URL_MAX);
        }
        fprintf(f, "mtllib %s\n", mtl_base);
    }

    fprintf(f, "# scene viewpoints=%d lights=%d backgrounds=%d fogs=%d navigation_infos=%d world_infos=%d inlines=%d\n",
            scene->viewpoint_count, scene->light_count, scene->background_count, scene->fog_count,
            scene->navigation_info_count, scene->world_info_count, scene->inline_count);
    for (i = 0; i < (unsigned long)scene->world_info_count; ++i) {
        fprintf(f, "# world_info %lu name=%s title=%s info=%s\n", i, scene->world_infos[i].name, scene->world_infos[i].title, scene->world_infos[i].info);
    }
    for (i = 0; i < (unsigned long)scene->navigation_info_count; ++i) {
        fprintf(f, "# navigation_info %lu name=%s type=%s headlight=%d speed=", i, scene->navigation_infos[i].name, scene->navigation_infos[i].type, scene->navigation_infos[i].headlight);
        vrml97_fprint_fx(f, scene->navigation_infos[i].speed);
        fputs(" visibility_limit=", f);
        vrml97_fprint_fx(f, scene->navigation_infos[i].visibility_limit);
        fputc('\n', f);
    }
    for (i = 0; i < (unsigned long)scene->viewpoint_count; ++i) {
        fprintf(f, "# viewpoint %lu name=%s desc=%s pos=", i, scene->viewpoints[i].name, scene->viewpoints[i].description);
        vrml97_fprint_vec3(f, scene->viewpoints[i].position);
        fputs(" axis=", f);
        vrml97_fprint_vec3(f, scene->viewpoints[i].orientation_axis);
        fputs(" angle=", f);
        vrml97_fprint_fx(f, scene->viewpoints[i].orientation_angle);
        fputs(" fov=", f);
        vrml97_fprint_fx(f, scene->viewpoints[i].field_of_view);
        fprintf(f, " jump=%d\n", scene->viewpoints[i].jump);
    }
    for (i = 0; i < (unsigned long)scene->light_count; ++i) {
        fprintf(f, "# light %lu kind=%s name=%s on=%d intensity=", i, vrml97_light_kind_name(scene->lights[i].kind), scene->lights[i].name, scene->lights[i].on);
        vrml97_fprint_fx(f, scene->lights[i].intensity);
        fputs(" color=", f);
        vrml97_fprint_vec3(f, scene->lights[i].color);
        fputs(" location=", f);
        vrml97_fprint_vec3(f, scene->lights[i].location);
        fputs(" direction=", f);
        vrml97_fprint_vec3(f, scene->lights[i].direction);
        fputc('\n', f);
    }
    for (i = 0; i < (unsigned long)scene->background_count; ++i) {
        fprintf(f, "# background %lu name=%s sky_colors=%d ground_colors=%d front=%s back=%s left=%s right=%s top=%s bottom=%s\n",
                i, scene->backgrounds[i].name, scene->backgrounds[i].sky_color_count, scene->backgrounds[i].ground_color_count,
                scene->backgrounds[i].urls[0], scene->backgrounds[i].urls[1], scene->backgrounds[i].urls[2],
                scene->backgrounds[i].urls[3], scene->backgrounds[i].urls[4], scene->backgrounds[i].urls[5]);
    }
    for (i = 0; i < (unsigned long)scene->fog_count; ++i) {
        fprintf(f, "# fog %lu name=%s type=%s color=", i, scene->fogs[i].name, scene->fogs[i].fog_type);
        vrml97_fprint_vec3(f, scene->fogs[i].color);
        fputs(" visibility_range=", f);
        vrml97_fprint_fx(f, scene->fogs[i].visibility_range);
        fputc('\n', f);
    }
    for (i = 0; i < (unsigned long)scene->inline_count; ++i) {
        fprintf(f, "# inline %lu name=%s url=%s bbox_center=", i, scene->inlines[i].name, scene->inlines[i].url);
        vrml97_fprint_vec3(f, scene->inlines[i].bbox_center);
        fputs(" bbox_size=", f);
        vrml97_fprint_vec3(f, scene->inlines[i].bbox_size);
        fputc('\n', f);
    }

    for (i = 0; i < scene->triangles_count; ++i) {
        tri = &scene->triangles[i];
        for (j = 0; j < 3; ++j) {
            fputs("v ", f);
            vrml97_fprint_vec3(f, tri->p[j]);
            if (tri->has_c) {
                fputc(' ', f);
                vrml97_fprint_vec3(f, tri->c[j]);
            }
            fputc('\n', f);
        }
    }
    for (i = 0; i < scene->triangles_count; ++i) {
        tri = &scene->triangles[i];
        if (tri->has_tc) {
            for (j = 0; j < 3; ++j) {
                fputs("vt ", f);
                vrml97_fprint_vec2(f, tri->tc[j]);
                fputc('\n', f);
            }
        }
    }
    for (i = 0; i < scene->triangles_count; ++i) {
        tri = &scene->triangles[i];
        if (tri->has_n) {
            for (j = 0; j < 3; ++j) {
                fputs("vn ", f);
                vrml97_fprint_vec3(f, tri->n[j]);
                fputc('\n', f);
            }
        }
    }

    last_mat = -9999;
    vi = 1;
    ti = 1;
    ni = 1;
    for (i = 0; i < scene->triangles_count; ++i) {
        tri = &scene->triangles[i];
        if (tri->material != last_mat) {
            fprintf(f, "usemtl mat_%d\n", tri->material >= 0 ? tri->material : 0);
            last_mat = tri->material;
        }
        fputs("f", f);
        for (j = 0; j < 3; ++j) {
            fputc(' ', f);
            if (tri->has_tc && tri->has_n) {
                fprintf(f, "%lu/%lu/%lu", vi + (unsigned long)j, ti + (unsigned long)j, ni + (unsigned long)j);
            } else if (tri->has_tc) {
                fprintf(f, "%lu/%lu", vi + (unsigned long)j, ti + (unsigned long)j);
            } else if (tri->has_n) {
                fprintf(f, "%lu//%lu", vi + (unsigned long)j, ni + (unsigned long)j);
            } else {
                fprintf(f, "%lu", vi + (unsigned long)j);
            }
        }
        fputc('\n', f);
        vi += 3u;
        if (tri->has_tc) ti += 3u;
        if (tri->has_n) ni += 3u;
    }
    for (i = 0; i < scene->lines_count; ++i) {
        line = &scene->lines[i];
        if (line->material != last_mat) {
            fprintf(f, "usemtl mat_%d\n", line->material >= 0 ? line->material : 0);
            last_mat = line->material;
        }
        for (j = 0; j < 2; ++j) {
            fputs("v ", f);
            vrml97_fprint_vec3(f, line->p[j]);
            if (line->has_c) {
                fputc(' ', f);
                vrml97_fprint_vec3(f, line->c[j]);
            }
            fputc('\n', f);
        }
        fprintf(f, "l %lu %lu\n", vi, vi + 1u);
        vi += 2u;
    }
    for (i = 0; i < scene->points_count; ++i) {
        point = &scene->points[i];
        if (point->material != last_mat) {
            fprintf(f, "usemtl mat_%d\n", point->material >= 0 ? point->material : 0);
            last_mat = point->material;
        }
        fputs("v ", f);
        vrml97_fprint_vec3(f, point->p);
        if (point->has_c) {
            fputc(' ', f);
            vrml97_fprint_vec3(f, point->c);
        }
        fputc('\n', f);
        fprintf(f, "p %lu\n", vi);
        vi += 1u;
    }
    fclose(f);
    return VRML97_OK;
}

