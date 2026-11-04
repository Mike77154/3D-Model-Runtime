#include "gxsi_decoder3d.h"

/* No stdio, no stdlib, no dynamic allocation in this core. */
#include <string.h>
#include <ctype.h>

#define GXSI_TOK_TEXT_MAX 96

#define GXSI_TOK_EOF     0
#define GXSI_TOK_IDENT   1
#define GXSI_TOK_STRING  2
#define GXSI_TOK_NUMBER  3
#define GXSI_TOK_LBRACE  4
#define GXSI_TOK_RBRACE  5
#define GXSI_TOK_SEP     6

typedef struct GXSI_Token {
    int type;
    char text[GXSI_TOK_TEXT_MAX];
} GXSI_Token;

typedef struct GXSI_Parser {
    const char *data;
    size_t size;
    size_t pos;
    int line;
    GXSI_Token pushed;
    int has_pushed;
    GXSI_Scene *scene;
    GXSI_Options opt;
    int err;
    char current_model[GXSI_NAME_MAX];
    char current_material[GXSI_NAME_MAX];
    GXSI_TransformSRT current_transform;
    int current_parent_bone;
    unsigned int current_mesh_index;
} GXSI_Parser;

static int gxsi_str_eq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
        if (ca != cb) return 0;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static int gxsi_str_has(const char *a, const char *b) {
    const char *p;
    if (b[0] == '\0') return 1;
    for (p = a; *p != '\0'; ++p) {
        const char *x = p;
        const char *y = b;
        while (*x != '\0' && *y != '\0') {
            char cx = *x;
            char cy = *y;
            if (cx >= 'a' && cx <= 'z') cx = (char)(cx - 'a' + 'A');
            if (cy >= 'a' && cy <= 'z') cy = (char)(cy - 'a' + 'A');
            if (cx != cy) break;
            ++x;
            ++y;
        }
        if (*y == '\0') return 1;
    }
    return 0;
}

static void gxsi_copy_name(char *dst, const char *src) {
    unsigned int i;
    if (dst == 0) return;
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    for (i = 0; i + 1U < (unsigned int)GXSI_NAME_MAX && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static void gxsi_copy_small(char *dst, unsigned int cap, const char *src) {
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    for (i = 0; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static void gxsi_warn(GXSI_Scene *scene, const char *msg) {
    unsigned int n;
    if (scene == 0 || msg == 0) return;
    n = (unsigned int)scene->warning_count;
    if (n < (unsigned int)GXSI_MAX_WARNINGS) {
        gxsi_copy_small(scene->warnings[n], 96U, msg);
        scene->warning_count++;
    }
}

void gxsi_scene_init(GXSI_Scene *scene) {
    unsigned int i;
    if (scene == 0) return;
    memset(scene, 0, sizeof(GXSI_Scene));
    for (i = 0; i < (unsigned int)GXSI_MAX_MESHES; ++i) {
        scene->meshes[i].transform.scale[0] = GXSI_FX_ONE;
        scene->meshes[i].transform.scale[1] = GXSI_FX_ONE;
        scene->meshes[i].transform.scale[2] = GXSI_FX_ONE;
    }
}

void gxsi_default_options(GXSI_Options *opt) {
    if (opt != 0) {
        opt->flags = GXSI_OPT_ACCEPT_LOOSE_HEADER | GXSI_OPT_PARSE_TRISTRIPS | GXSI_OPT_KEEP_GOING | GXSI_OPT_PARSE_ADVANCED | GXSI_OPT_BINARY_PROBE;
    }
}

static void gxsi_transform_identity(GXSI_TransformSRT *t) {
    if (t == 0) return;
    memset(t, 0, sizeof(GXSI_TransformSRT));
    t->scale[0] = GXSI_FX_ONE;
    t->scale[1] = GXSI_FX_ONE;
    t->scale[2] = GXSI_FX_ONE;
}

static int gxsi_is_ident_start(int c) {
    return (c == '_' || c == '$' || c == '-' || c == '.' || isalpha((unsigned char)c));
}

static int gxsi_is_ident_char(int c) {
    return (c == '_' || c == '$' || c == '-' || c == '.' || c == ':' || isalnum((unsigned char)c));
}

static int gxsi_is_num_start(const GXSI_Parser *p) {
    int c;
    if (p->pos >= p->size) return 0;
    c = (unsigned char)p->data[p->pos];
    if (c >= '0' && c <= '9') return 1;
    if ((c == '-' || c == '+') && p->pos + 1U < p->size) {
        int n = (unsigned char)p->data[p->pos + 1U];
        return ((n >= '0' && n <= '9') || n == '.');
    }
    if (c == '.' && p->pos + 1U < p->size) {
        int n2 = (unsigned char)p->data[p->pos + 1U];
        return (n2 >= '0' && n2 <= '9');
    }
    return 0;
}

static void gxsi_skip_ws(GXSI_Parser *p) {
    while (p->pos < p->size) {
        char c = p->data[p->pos];
        if (c == '\n') {
            p->line++;
            p->pos++;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\f') {
            p->pos++;
        } else if (c == '/' && p->pos + 1U < p->size && p->data[p->pos + 1U] == '/') {
            p->pos += 2U;
            while (p->pos < p->size && p->data[p->pos] != '\n') p->pos++;
        } else if (c == '/' && p->pos + 1U < p->size && p->data[p->pos + 1U] == '*') {
            p->pos += 2U;
            while (p->pos + 1U < p->size) {
                if (p->data[p->pos] == '\n') p->line++;
                if (p->data[p->pos] == '*' && p->data[p->pos + 1U] == '/') {
                    p->pos += 2U;
                    break;
                }
                p->pos++;
            }
        } else {
            break;
        }
    }
}

static GXSI_Token gxsi_next_token_raw(GXSI_Parser *p) {
    GXSI_Token t;
    unsigned int k;
    int c;
    t.type = GXSI_TOK_EOF;
    t.text[0] = '\0';

    gxsi_skip_ws(p);
    if (p->pos >= p->size) return t;
    c = (unsigned char)p->data[p->pos];

    if (c == '{') {
        p->pos++;
        t.type = GXSI_TOK_LBRACE;
        gxsi_copy_small(t.text, GXSI_TOK_TEXT_MAX, "{");
        return t;
    }
    if (c == '}') {
        p->pos++;
        t.type = GXSI_TOK_RBRACE;
        gxsi_copy_small(t.text, GXSI_TOK_TEXT_MAX, "}");
        return t;
    }
    if (c == ',' || c == ';') {
        p->pos++;
        t.type = GXSI_TOK_SEP;
        t.text[0] = (char)c;
        t.text[1] = '\0';
        return t;
    }
    if (c == '"') {
        p->pos++;
        k = 0U;
        while (p->pos < p->size) {
            c = (unsigned char)p->data[p->pos++];
            if (c == '"') break;
            if (c == '\\' && p->pos < p->size) {
                c = (unsigned char)p->data[p->pos++];
            }
            if (k + 1U < GXSI_TOK_TEXT_MAX) t.text[k++] = (char)c;
        }
        t.text[k] = '\0';
        t.type = GXSI_TOK_STRING;
        return t;
    }
    if (gxsi_is_num_start(p)) {
        k = 0U;
        while (p->pos < p->size) {
            c = (unsigned char)p->data[p->pos];
            if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
                if (k + 1U < GXSI_TOK_TEXT_MAX) t.text[k++] = (char)c;
                p->pos++;
            } else {
                break;
            }
        }
        t.text[k] = '\0';
        t.type = GXSI_TOK_NUMBER;
        return t;
    }
    if (gxsi_is_ident_start(c)) {
        k = 0U;
        while (p->pos < p->size) {
            c = (unsigned char)p->data[p->pos];
            if (!gxsi_is_ident_char(c)) break;
            if (k + 1U < GXSI_TOK_TEXT_MAX) t.text[k++] = (char)c;
            p->pos++;
        }
        t.text[k] = '\0';
        t.type = GXSI_TOK_IDENT;
        return t;
    }

    p->pos++;
    t.type = GXSI_TOK_SEP;
    t.text[0] = (char)c;
    t.text[1] = '\0';
    return t;
}

static GXSI_Token gxsi_next_token(GXSI_Parser *p) {
    GXSI_Token t;
    if (p->has_pushed) {
        p->has_pushed = 0;
        return p->pushed;
    }
    do {
        t = gxsi_next_token_raw(p);
    } while (t.type == GXSI_TOK_SEP);
    return t;
}

static void gxsi_push_token(GXSI_Parser *p, GXSI_Token t) {
    p->pushed = t;
    p->has_pushed = 1;
}

static int gxsi_token_is_name(GXSI_Token *t, const char *name) {
    return (t->type == GXSI_TOK_IDENT || t->type == GXSI_TOK_STRING) && gxsi_str_eq(t->text, name);
}

static int gxsi_token_has_name(GXSI_Token *t, const char *name) {
    return (t->type == GXSI_TOK_IDENT || t->type == GXSI_TOK_STRING) && gxsi_str_has(t->text, name);
}

static GXSI_Token gxsi_expect_block_start(GXSI_Parser *p, char *out_name) {
    GXSI_Token t;
    if (out_name != 0) out_name[0] = '\0';
    t = gxsi_next_token(p);
    if (t.type == GXSI_TOK_LBRACE) return t;
    if ((t.type == GXSI_TOK_IDENT || t.type == GXSI_TOK_STRING) && out_name != 0) gxsi_copy_name(out_name, t.text);
    t = gxsi_next_token(p);
    if (t.type != GXSI_TOK_LBRACE) {
        p->err = GXSI_ERR_PARSE;
    }
    return t;
}

static int gxsi_parse_int_text(const char *s) {
    int sign = 1;
    int v = 0;
    if (*s == '-') { sign = -1; ++s; }
    else if (*s == '+') { ++s; }
    while (*s >= '0' && *s <= '9') {
        if (v < 100000000) v = v * 10 + (*s - '0');
        ++s;
    }
    return sign * v;
}

static int gxsi_read_int(GXSI_Parser *p, int *out) {
    GXSI_Token t = gxsi_next_token(p);
    if (t.type != GXSI_TOK_NUMBER && t.type != GXSI_TOK_IDENT && t.type != GXSI_TOK_STRING) {
        return 0;
    }
    *out = gxsi_parse_int_text(t.text);
    return 1;
}

static int gxsi_read_fx(GXSI_Parser *p, gxsi_fx *out) {
    GXSI_Token t = gxsi_next_token(p);
    if (t.type != GXSI_TOK_NUMBER && t.type != GXSI_TOK_IDENT && t.type != GXSI_TOK_STRING) return 0;
    *out = gxsi_fx_from_decimal(t.text);
    return 1;
}

/* Q16.16 decimal conversion. Handles common scientific notation with integer powers of 10. */
gxsi_fx gxsi_fx_from_decimal(const char *text) {
    int sign = 1;
    long whole = 0L;
    long frac = 0L;
    long div = 1L;
    int frac_digits = 0;
    int exp_sign = 1;
    int exp = 0;
    gxsi_fx value;
    const long max_whole = 32767L;
    const char *p = text;

    if (p == 0) return 0L;
    if (*p == '-') { sign = -1; ++p; }
    else if (*p == '+') { ++p; }

    while (*p >= '0' && *p <= '9') {
        if (whole < max_whole) whole = whole * 10L + (long)(*p - '0');
        if (whole > max_whole) whole = max_whole;
        ++p;
    }
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9' && frac_digits < 6) {
            frac = frac * 10L + (long)(*p - '0');
            div *= 10L;
            frac_digits++;
            ++p;
        }
        while (*p >= '0' && *p <= '9') ++p;
    }
    if (*p == 'e' || *p == 'E') {
        ++p;
        if (*p == '-') { exp_sign = -1; ++p; }
        else if (*p == '+') { ++p; }
        while (*p >= '0' && *p <= '9') {
            if (exp < 9) exp = exp * 10 + (*p - '0');
            ++p;
        }
    }

    value = (gxsi_fx)(whole << 16);
    if (div > 1L) {
        value += (gxsi_fx)((frac * 65536L + (div / 2L)) / div);
    }

    while (exp > 0) {
        if (exp_sign > 0) {
            if (value > (gxsi_fx)214748364L) { value = (gxsi_fx)2147483647L; break; }
            value *= 10L;
        } else {
            value /= 10L;
        }
        --exp;
    }
    if (sign < 0) value = -value;
    return value;
}

static unsigned short gxsi_clamp_u16(unsigned int v) {
    if (v > 65535U) return (unsigned short)65535U;
    return (unsigned short)v;
}


static gxsi_fx gxsi_fx_mul(gxsi_fx a, gxsi_fx b) {
    int neg = 0;
    unsigned long ua;
    unsigned long ub;
    unsigned long hi;
    if (a < 0) { neg = !neg; a = -a; }
    if (b < 0) { neg = !neg; b = -b; }
    ua = (unsigned long)a;
    ub = (unsigned long)b;
    /* Q16.16 multiply using an 8-bit pre-shift, portable and safe for game-scale values. */
    hi = (ua >> 8) * (ub >> 8);
    if (hi > 2147483647UL) hi = 2147483647UL;
    return neg ? -(gxsi_fx)hi : (gxsi_fx)hi;
}

static int gxsi_fx_nonzero(gxsi_fx v) {
    return v > (gxsi_fx)64L || v < (gxsi_fx)-64L;
}

static void gxsi_compose_srt(const GXSI_TransformSRT *parent, const GXSI_TransformSRT *local, GXSI_TransformSRT *out) {
    int i;
    if (out == 0) return;
    if (parent == 0 || parent->present == 0) {
        *out = *local;
        return;
    }
    if (local == 0 || local->present == 0) {
        *out = *parent;
        return;
    }
    for (i = 0; i < 3; ++i) {
        out->scale[i] = gxsi_fx_mul(parent->scale[i], local->scale[i]);
        out->rot_deg[i] = parent->rot_deg[i] + local->rot_deg[i];
        out->trans[i] = parent->trans[i] + local->trans[i];
    }
    out->present = 1;
}

static int gxsi_add_bone(GXSI_Parser *p, const char *name) {
    GXSI_Bone *b;
    if (p->scene->bone_count >= (unsigned int)GXSI_MAX_BONES) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "bone capacity exceeded");
        return -1;
    }
    b = &p->scene->bones[p->scene->bone_count];
    memset(b, 0, sizeof(GXSI_Bone));
    gxsi_copy_name(b->name, name);
    b->parent = p->current_parent_bone;
    b->transform = p->current_transform;
    p->scene->bone_count++;
    return (int)(p->scene->bone_count - 1U);
}

static int gxsi_add_weight(GXSI_Parser *p, GXSI_Envelope *e, int vertex, int bone, gxsi_fx weight) {
    GXSI_Weight *w;
    if (p->scene->weight_count >= (unsigned int)GXSI_MAX_WEIGHTS) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "weight capacity exceeded");
        return 0;
    }
    if (vertex < 0 || bone < 0) return 0;
    w = &p->scene->weights[p->scene->weight_count++];
    w->vertex = gxsi_clamp_u16((unsigned int)vertex);
    w->bone = gxsi_clamp_u16((unsigned int)bone);
    w->weight = weight;
    e->weight_count++;
    return 1;
}

static int gxsi_add_fcurve_key(GXSI_Parser *p, GXSI_FCurve *fc, gxsi_fx time, gxsi_fx value) {
    GXSI_FCurveKey *k;
    if (p->scene->fcurve_key_count >= (unsigned int)GXSI_MAX_FCURVE_KEYS) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "fcurve key capacity exceeded");
        return 0;
    }
    k = &p->scene->fcurve_keys[p->scene->fcurve_key_count++];
    memset(k, 0, sizeof(GXSI_FCurveKey));
    k->time = time;
    k->value = value;
    fc->key_count++;
    return 1;
}

static int gxsi_add_nurbs_cp(GXSI_Parser *p, GXSI_Nurbs *n, GXSI_Vec4 cp) {
    if (p->scene->nurbs_cp_count >= (unsigned int)GXSI_MAX_NURBS_CP) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "nurbs control point capacity exceeded");
        return 0;
    }
    p->scene->nurbs_cp[p->scene->nurbs_cp_count++] = cp;
    n->cp_count++;
    return 1;
}

static int gxsi_add_mesh(GXSI_Parser *p, const char *name, GXSI_Mesh **out) {
    GXSI_Scene *s = p->scene;
    GXSI_Mesh *m;
    if (s->mesh_count >= (unsigned int)GXSI_MAX_MESHES) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(s, "mesh capacity exceeded");
        return 0;
    }
    m = &s->meshes[s->mesh_count];
    memset(m, 0, sizeof(GXSI_Mesh));
    gxsi_copy_name(m->name, name);
    gxsi_copy_name(m->model_name, p->current_model);
    gxsi_copy_name(m->material_name, p->current_material);
    m->transform = p->current_transform;
    m->first_vertex = s->vertex_count;
    m->first_normal = s->normal_count;
    m->first_texcoord = s->texcoord_count;
    m->first_triangle = s->triangle_count;
    s->mesh_count++;
    p->current_mesh_index = s->mesh_count - 1U;
    *out = m;
    return 1;
}

static int gxsi_add_vertex(GXSI_Parser *p, GXSI_Mesh *m, GXSI_Vec3 v) {
    if (p->scene->vertex_count >= (unsigned int)GXSI_MAX_VERTICES) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "vertex capacity exceeded");
        return 0;
    }
    p->scene->vertices[p->scene->vertex_count++] = v;
    m->vertex_count++;
    return 1;
}

static int gxsi_add_normal(GXSI_Parser *p, GXSI_Mesh *m, GXSI_Vec3 v) {
    if (p->scene->normal_count >= (unsigned int)GXSI_MAX_NORMALS) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "normal capacity exceeded");
        return 0;
    }
    p->scene->normals[p->scene->normal_count++] = v;
    m->normal_count++;
    return 1;
}

static int gxsi_add_uv(GXSI_Parser *p, GXSI_Mesh *m, GXSI_Vec2 v) {
    if (p->scene->texcoord_count >= (unsigned int)GXSI_MAX_TEXCOORDS) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "texcoord capacity exceeded");
        return 0;
    }
    p->scene->texcoords[p->scene->texcoord_count++] = v;
    m->texcoord_count++;
    return 1;
}

static int gxsi_add_tri(GXSI_Parser *p, GXSI_Mesh *m, int a, int b, int c) {
    GXSI_Triangle tri;
    unsigned int va;
    unsigned int vb;
    unsigned int vc;
    int mi;
    if (p->scene->triangle_count >= (unsigned int)GXSI_MAX_TRIANGLES) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "triangle capacity exceeded");
        return 0;
    }
    if (a < 0 || b < 0 || c < 0) return 0;
    va = m->first_vertex + (unsigned int)a;
    vb = m->first_vertex + (unsigned int)b;
    vc = m->first_vertex + (unsigned int)c;
    if (va >= p->scene->vertex_count || vb >= p->scene->vertex_count || vc >= p->scene->vertex_count) {
        gxsi_warn(p->scene, "triangle index outside current mesh vertex range");
        return 0;
    }
    memset(&tri, 0, sizeof(tri));
    tri.v[0] = gxsi_clamp_u16(va);
    tri.v[1] = gxsi_clamp_u16(vb);
    tri.v[2] = gxsi_clamp_u16(vc);
    tri.mesh = gxsi_clamp_u16(p->scene->mesh_count - 1U);
    mi = gxsi_find_material(p->scene, m->material_name);
    tri.material = (mi >= 0) ? (unsigned short)mi : (unsigned short)65535U;
    p->scene->triangles[p->scene->triangle_count++] = tri;
    m->triangle_count++;
    return 1;
}

static void gxsi_skip_block_after_open(GXSI_Parser *p) {
    int depth = 1;
    GXSI_Token t;
    while (depth > 0 && p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) {
            p->err = GXSI_ERR_PARSE;
            return;
        }
        if (t.type == GXSI_TOK_LBRACE) ++depth;
        else if (t.type == GXSI_TOK_RBRACE) --depth;
    }
}

static int gxsi_parse_vec3(GXSI_Parser *p, GXSI_Vec3 *out) {
    return gxsi_read_fx(p, &out->x) && gxsi_read_fx(p, &out->y) && gxsi_read_fx(p, &out->z);
}

static int gxsi_parse_vec2(GXSI_Parser *p, GXSI_Vec2 *out) {
    return gxsi_read_fx(p, &out->u) && gxsi_read_fx(p, &out->v);
}

static void gxsi_parse_shape(GXSI_Parser *p, GXSI_Mesh *m) {
    char dummy[GXSI_NAME_MAX];
    GXSI_Token t;
    int count;
    gxsi_expect_block_start(p, dummy);
    if (p->err != GXSI_OK) return;

    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_RBRACE) return;
        if (t.type == GXSI_TOK_LBRACE) { gxsi_skip_block_after_open(p); continue; }

        if (t.type == GXSI_TOK_NUMBER) {
            GXSI_Token label;
            count = gxsi_parse_int_text(t.text);
            label = gxsi_next_token(p);
            if (label.type == GXSI_TOK_RBRACE) return;
            if (label.type == GXSI_TOK_LBRACE) { gxsi_skip_block_after_open(p); continue; }
            if (gxsi_token_is_name(&label, "ORDERED")) {
                continue;
            } else if (gxsi_token_is_name(&label, "POSITION")) {
                int i;
                for (i = 0; i < count && p->err == GXSI_OK; ++i) {
                    GXSI_Vec3 v;
                    if (!gxsi_parse_vec3(p, &v)) { p->err = GXSI_ERR_PARSE; return; }
                    gxsi_add_vertex(p, m, v);
                }
            } else if (gxsi_token_is_name(&label, "NORMAL")) {
                int j;
                for (j = 0; j < count && p->err == GXSI_OK; ++j) {
                    GXSI_Vec3 n;
                    if (!gxsi_parse_vec3(p, &n)) { p->err = GXSI_ERR_PARSE; return; }
                    gxsi_add_normal(p, m, n);
                }
            } else if (gxsi_token_has_name(&label, "TEX") || gxsi_token_has_name(&label, "UV")) {
                int k;
                for (k = 0; k < count && p->err == GXSI_OK; ++k) {
                    GXSI_Vec2 uv;
                    if (!gxsi_parse_vec2(p, &uv)) { p->err = GXSI_ERR_PARSE; return; }
                    gxsi_add_uv(p, m, uv);
                }
            } else {
                /* Unknown counted stream inside shape; leave it alone by scanning until next block close. */
            }
        }
    }
}

static void gxsi_parse_triangle_list(GXSI_Parser *p, GXSI_Mesh *m) {
    char name[GXSI_NAME_MAX];
    GXSI_Token t;
    int count;
    int i;
    int got;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    if (!gxsi_read_int(p, &count)) { p->err = GXSI_ERR_PARSE; return; }

    /* Skip metadata strings like "NORMAL" and material names before tuple data. */
    got = 0;
    while (!got && p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_NUMBER) {
            gxsi_push_token(p, t);
            got = 1;
        } else if (t.type == GXSI_TOK_RBRACE) {
            return;
        } else if (t.type == GXSI_TOK_LBRACE) {
            gxsi_skip_block_after_open(p);
        }
    }

    for (i = 0; i < count && p->err == GXSI_OK; ++i) {
        int a, b, c;
        if (!gxsi_read_int(p, &a) || !gxsi_read_int(p, &b) || !gxsi_read_int(p, &c)) { p->err = GXSI_ERR_PARSE; return; }
        gxsi_add_tri(p, m, a, b, c);
    }

    /* Ignore optional normal/material index tail. */
    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_RBRACE) return;
        if (t.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
    }
}

static void gxsi_parse_polygon_list(GXSI_Parser *p, GXSI_Mesh *m) {
    char name[GXSI_NAME_MAX];
    GXSI_Token t;
    int count;
    int i;
    int got;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    if (!gxsi_read_int(p, &count)) { p->err = GXSI_ERR_PARSE; return; }

    got = 0;
    while (!got && p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_NUMBER) { gxsi_push_token(p, t); got = 1; }
        else if (t.type == GXSI_TOK_RBRACE) return;
        else if (t.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
    }

    for (i = 0; i < count && p->err == GXSI_OK; ++i) {
        int n;
        int first;
        int prev;
        int cur;
        int k;
        if (!gxsi_read_int(p, &n)) { p->err = GXSI_ERR_PARSE; return; }
        if (n < 3) {
            for (k = 0; k < n; ++k) {
                int junk;
                if (!gxsi_read_int(p, &junk)) { p->err = GXSI_ERR_PARSE; return; }
            }
        } else {
            if (!gxsi_read_int(p, &first) || !gxsi_read_int(p, &prev)) { p->err = GXSI_ERR_PARSE; return; }
            for (k = 2; k < n; ++k) {
                if (!gxsi_read_int(p, &cur)) { p->err = GXSI_ERR_PARSE; return; }
                gxsi_add_tri(p, m, first, prev, cur);
                prev = cur;
            }
        }
    }

    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_RBRACE) return;
        if (t.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
    }
}

static void gxsi_parse_tristrip(GXSI_Parser *p, GXSI_Mesh *m) {
    char name[GXSI_NAME_MAX];
    int count;
    int i;
    int prev2 = 0;
    int prev1 = 0;
    int cur = 0;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    if (!gxsi_read_int(p, &count)) { p->err = GXSI_ERR_PARSE; return; }
    for (i = 0; i < count && p->err == GXSI_OK; ++i) {
        if (!gxsi_read_int(p, &cur)) { p->err = GXSI_ERR_PARSE; return; }
        if (i == 0) prev2 = cur;
        else if (i == 1) prev1 = cur;
        else {
            if (cur != prev1 && cur != prev2 && prev1 != prev2) {
                if ((i & 1) == 0) gxsi_add_tri(p, m, prev2, prev1, cur);
                else gxsi_add_tri(p, m, prev1, prev2, cur);
            }
            prev2 = prev1;
            prev1 = cur;
        }
    }
    /* consume to end of SI_TriStrip block */
    while (p->err == GXSI_OK) {
        GXSI_Token t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_RBRACE) return;
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
    }
}

static void gxsi_parse_tristrip_list(GXSI_Parser *p, GXSI_Mesh *m) {
    char name[GXSI_NAME_MAX];
    GXSI_Token t;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_RBRACE) return;
        if (gxsi_token_is_name(&t, "SI_TriStrip") || gxsi_token_is_name(&t, "XSI_TriStrip")) {
            gxsi_parse_tristrip(p, m);
        } else if (t.type == GXSI_TOK_LBRACE) {
            gxsi_skip_block_after_open(p);
        }
    }
}

static void gxsi_parse_texture2d(GXSI_Parser *p, GXSI_Mesh *m) {
    char name[GXSI_NAME_MAX];
    GXSI_Token t;
    (void)m;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_RBRACE) return;
        if (t.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
    }
}


static void gxsi_parse_transform(GXSI_Parser *p);

static void gxsi_parse_fcurve(GXSI_Parser *p) {
    char name[GXSI_NAME_MAX];
    GXSI_FCurve *fc;
    GXSI_Token t;
    int count = -1;
    int got_count = 0;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    if (p->scene->fcurve_count >= (unsigned int)GXSI_MAX_FCURVES) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "fcurve capacity exceeded");
        gxsi_skip_block_after_open(p);
        return;
    }
    fc = &p->scene->fcurves[p->scene->fcurve_count++];
    memset(fc, 0, sizeof(GXSI_FCurve));
    gxsi_copy_name(fc->name, name);
    fc->first_key = p->scene->fcurve_key_count;

    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_RBRACE) return;
        if (t.type == GXSI_TOK_LBRACE) { gxsi_skip_block_after_open(p); continue; }
        if (t.type == GXSI_TOK_STRING || t.type == GXSI_TOK_IDENT) {
            if (fc->target[0] == '\0') gxsi_copy_name(fc->target, t.text);
            else if (fc->parameter[0] == '\0') gxsi_copy_name(fc->parameter, t.text);
        } else if (t.type == GXSI_TOK_NUMBER) {
            if (!got_count) {
                count = gxsi_parse_int_text(t.text);
                got_count = 1;
            } else {
                GXSI_Token v;
                gxsi_fx time = gxsi_fx_from_decimal(t.text);
                gxsi_fx value;
                v = gxsi_next_token(p);
                if (v.type == GXSI_TOK_RBRACE) return;
                if (v.type != GXSI_TOK_NUMBER && v.type != GXSI_TOK_IDENT && v.type != GXSI_TOK_STRING) { p->err = GXSI_ERR_PARSE; return; }
                value = gxsi_fx_from_decimal(v.text);
                gxsi_add_fcurve_key(p, fc, time, value);
                if (count >= 0 && fc->key_count >= (unsigned int)count) {
                    /* Leave the tail to the closing-brace scanner; exporters often store tangents/interp after pairs. */
                    while (p->err == GXSI_OK) {
                        GXSI_Token tail = gxsi_next_token(p);
                        if (tail.type == GXSI_TOK_RBRACE) return;
                        if (tail.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
                        if (tail.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
                    }
                }
            }
        }
    }
}

static void gxsi_parse_envelope(GXSI_Parser *p) {
    char name[GXSI_NAME_MAX];
    GXSI_Envelope *e;
    GXSI_Token t;
    int got_count = 0;
    int count = -1;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    if (p->scene->envelope_count >= (unsigned int)GXSI_MAX_ENVELOPES) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "envelope capacity exceeded");
        gxsi_skip_block_after_open(p);
        return;
    }
    e = &p->scene->envelopes[p->scene->envelope_count++];
    memset(e, 0, sizeof(GXSI_Envelope));
    gxsi_copy_name(e->name, name);
    e->mesh = gxsi_clamp_u16(p->current_mesh_index);
    e->first_weight = p->scene->weight_count;
    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_RBRACE) return;
        if (t.type == GXSI_TOK_LBRACE) { gxsi_skip_block_after_open(p); continue; }
        if (t.type == GXSI_TOK_NUMBER) {
            int vi;
            int bi;
            gxsi_fx wt;
            if (!got_count) {
                count = gxsi_parse_int_text(t.text);
                got_count = 1;
                continue;
            }
            vi = gxsi_parse_int_text(t.text);
            if (!gxsi_read_int(p, &bi)) { p->err = GXSI_ERR_PARSE; return; }
            if (!gxsi_read_fx(p, &wt)) { p->err = GXSI_ERR_PARSE; return; }
            gxsi_add_weight(p, e, vi, bi, wt);
            if (count >= 0 && e->weight_count >= (unsigned int)count) {
                while (p->err == GXSI_OK) {
                    GXSI_Token tail = gxsi_next_token(p);
                    if (tail.type == GXSI_TOK_RBRACE) return;
                    if (tail.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
                    if (tail.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
                }
            }
        }
    }
}

static void gxsi_parse_nurbs_generic(GXSI_Parser *p, unsigned short type) {
    char name[GXSI_NAME_MAX];
    GXSI_Nurbs *n;
    GXSI_Token t;
    int got_header = 0;
    int expected_cp = -1;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    if (p->scene->nurbs_count >= (unsigned int)GXSI_MAX_NURBS) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "nurbs capacity exceeded");
        gxsi_skip_block_after_open(p);
        return;
    }
    n = &p->scene->nurbs[p->scene->nurbs_count++];
    memset(n, 0, sizeof(GXSI_Nurbs));
    gxsi_copy_name(n->name, name);
    n->type = type;
    n->first_cp = p->scene->nurbs_cp_count;
    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_RBRACE) return;
        if (t.type == GXSI_TOK_LBRACE) { gxsi_skip_block_after_open(p); continue; }
        if (t.type == GXSI_TOK_NUMBER) {
            if (!got_header) {
                n->degree_u = gxsi_clamp_u16((unsigned int)gxsi_parse_int_text(t.text));
                gxsi_read_int(p, &expected_cp);
                n->count_u = gxsi_clamp_u16((unsigned int)expected_cp);
                got_header = 1;
            } else {
                GXSI_Vec4 cp;
                cp.x = gxsi_fx_from_decimal(t.text);
                if (!gxsi_read_fx(p, &cp.y)) { p->err = GXSI_ERR_PARSE; return; }
                if (!gxsi_read_fx(p, &cp.z)) { p->err = GXSI_ERR_PARSE; return; }
                if (!gxsi_read_fx(p, &cp.w)) cp.w = GXSI_FX_ONE;
                gxsi_add_nurbs_cp(p, n, cp);
                if (expected_cp >= 0 && n->cp_count >= (unsigned int)expected_cp) {
                    while (p->err == GXSI_OK) {
                        GXSI_Token tail = gxsi_next_token(p);
                        if (tail.type == GXSI_TOK_RBRACE) return;
                        if (tail.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
                        if (tail.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
                    }
                }
            }
        }
    }
}

static void gxsi_parse_bone_like(GXSI_Parser *p, const char *fallback_name) {
    char old_model[GXSI_NAME_MAX];
    char old_material[GXSI_NAME_MAX];
    GXSI_TransformSRT old_transform;
    char name[GXSI_NAME_MAX];
    GXSI_Token t;
    int old_parent;
    int this_bone;

    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    if (name[0] == '\0') gxsi_copy_name(name, fallback_name);

    gxsi_copy_name(old_model, p->current_model);
    gxsi_copy_name(old_material, p->current_material);
    old_transform = p->current_transform;
    old_parent = p->current_parent_bone;
    this_bone = gxsi_add_bone(p, name);
    p->current_parent_bone = this_bone;

    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; break; }
        if (t.type == GXSI_TOK_RBRACE) break;
        if (gxsi_token_is_name(&t, "SI_Transform") || gxsi_token_is_name(&t, "XSI_Transform")) {
            gxsi_parse_transform(p);
            if (this_bone >= 0) p->scene->bones[(unsigned int)this_bone].transform = p->current_transform;
        } else if (gxsi_token_is_name(&t, "SI_ChainBone") || gxsi_token_is_name(&t, "XSI_ChainBone") || gxsi_token_is_name(&t, "SI_Bone") || gxsi_token_is_name(&t, "XSI_Bone")) {
            gxsi_parse_bone_like(p, t.text);
        } else if (gxsi_token_is_name(&t, "SI_FCurve") || gxsi_token_is_name(&t, "XSI_FCurve") || gxsi_token_is_name(&t, "FCurve")) {
            gxsi_parse_fcurve(p);
        } else if (t.type == GXSI_TOK_LBRACE) {
            gxsi_skip_block_after_open(p);
        } else {
            GXSI_Token look = gxsi_next_token(p);
            if (look.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
            else gxsi_push_token(p, look);
        }
    }
    gxsi_copy_name(p->current_model, old_model);
    gxsi_copy_name(p->current_material, old_material);
    p->current_transform = old_transform;
    p->current_parent_bone = old_parent;
}

static void gxsi_parse_anim_container(GXSI_Parser *p) {
    char name[GXSI_NAME_MAX];
    GXSI_Token t;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_RBRACE) return;
        if (gxsi_token_is_name(&t, "SI_FCurve") || gxsi_token_is_name(&t, "XSI_FCurve") || gxsi_token_is_name(&t, "FCurve")) {
            gxsi_parse_fcurve(p);
        } else if (t.type == GXSI_TOK_LBRACE) {
            gxsi_skip_block_after_open(p);
        } else {
            GXSI_Token look = gxsi_next_token(p);
            if (look.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
            else gxsi_push_token(p, look);
        }
    }
}

static void gxsi_parse_mesh(GXSI_Parser *p) {
    char name[GXSI_NAME_MAX];
    GXSI_Mesh *m;
    GXSI_Token t;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    if (!gxsi_add_mesh(p, name, &m)) {
        gxsi_skip_block_after_open(p);
        return;
    }

    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_RBRACE) return;
        if (gxsi_token_is_name(&t, "SI_Shape") || gxsi_token_is_name(&t, "XSI_Shape")) {
            gxsi_parse_shape(p, m);
        } else if (gxsi_token_is_name(&t, "SI_TriangleList") || gxsi_token_is_name(&t, "XSI_TriangleList")) {
            gxsi_parse_triangle_list(p, m);
        } else if (gxsi_token_is_name(&t, "SI_PolygonList") || gxsi_token_is_name(&t, "XSI_PolygonList")) {
            gxsi_parse_polygon_list(p, m);
        } else if (gxsi_token_is_name(&t, "SI_TriStripList") || gxsi_token_is_name(&t, "XSI_TriangleStripList") || gxsi_token_is_name(&t, "XSI_TriStripList")) {
            if ((p->opt.flags & GXSI_OPT_PARSE_TRISTRIPS) != 0U) gxsi_parse_tristrip_list(p, m);
            else {
                GXSI_Token t2 = gxsi_expect_block_start(p, name);
                (void)t2;
                gxsi_skip_block_after_open(p);
            }
        } else if (gxsi_token_is_name(&t, "SI_Texture2D") || gxsi_token_is_name(&t, "XSI_Texture2D")) {
            gxsi_parse_texture2d(p, m);
        } else if (gxsi_token_is_name(&t, "SI_Envelope") || gxsi_token_is_name(&t, "XSI_Envelope") || gxsi_token_is_name(&t, "Envelope")) {
            gxsi_parse_envelope(p);
        } else if (gxsi_token_is_name(&t, "SI_FCurve") || gxsi_token_is_name(&t, "XSI_FCurve") || gxsi_token_is_name(&t, "FCurve")) {
            gxsi_parse_fcurve(p);
        } else if (t.type == GXSI_TOK_LBRACE) {
            gxsi_skip_block_after_open(p);
        } else {
            /* Unknown data immediately followed by a block? Consume it safely. */
            GXSI_Token look = gxsi_next_token(p);
            if (look.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
            else gxsi_push_token(p, look);
        }
    }
}

static void gxsi_parse_transform(GXSI_Parser *p) {
    char name[GXSI_NAME_MAX];
    int i;
    GXSI_TransformSRT parent;
    GXSI_TransformSRT local;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    parent = p->current_transform;
    gxsi_transform_identity(&local);
    for (i = 0; i < 3; ++i) {
        if (!gxsi_read_fx(p, &local.scale[i])) { p->err = GXSI_ERR_PARSE; return; }
    }
    for (i = 0; i < 3; ++i) {
        if (!gxsi_read_fx(p, &local.rot_deg[i])) { p->err = GXSI_ERR_PARSE; return; }
    }
    for (i = 0; i < 3; ++i) {
        if (!gxsi_read_fx(p, &local.trans[i])) { p->err = GXSI_ERR_PARSE; return; }
    }
    local.present = 1;
    gxsi_compose_srt(&parent, &local, &p->current_transform);
    while (p->err == GXSI_OK) {
        GXSI_Token t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_RBRACE) return;
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
    }
}

static void gxsi_parse_global_material(GXSI_Parser *p) {
    char name[GXSI_NAME_MAX];
    GXSI_Token t;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    t = gxsi_next_token(p);
    if (t.type == GXSI_TOK_STRING || t.type == GXSI_TOK_IDENT) {
        gxsi_copy_name(p->current_material, t.text);
    }
    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_RBRACE) return;
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
    }
}

static void gxsi_parse_material(GXSI_Parser *p) {
    char name[GXSI_NAME_MAX];
    GXSI_Material *mat;
    int i;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    if (p->scene->material_count >= (unsigned int)GXSI_MAX_MATERIALS) {
        p->err = GXSI_ERR_OVERFLOW;
        gxsi_warn(p->scene, "material capacity exceeded");
        gxsi_skip_block_after_open(p);
        return;
    }
    mat = &p->scene->materials[p->scene->material_count++];
    memset(mat, 0, sizeof(GXSI_Material));
    gxsi_copy_name(mat->name, name);
    for (i = 0; i < 4 && p->err == GXSI_OK; ++i) if (!gxsi_read_fx(p, &mat->diffuse[i])) { p->err = GXSI_ERR_PARSE; return; }
    gxsi_read_fx(p, &mat->shininess);
    for (i = 0; i < 3 && p->err == GXSI_OK; ++i) gxsi_read_fx(p, &mat->specular[i]);
    for (i = 0; i < 3 && p->err == GXSI_OK; ++i) gxsi_read_fx(p, &mat->emission[i]);
    /* Some exporters continue with flags and ambient-like triplets. Parse cautiously. */
    for (i = 0; i < 3 && p->err == GXSI_OK; ++i) gxsi_read_fx(p, &mat->ambient[i]);
    while (p->err == GXSI_OK) {
        GXSI_Token t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_RBRACE) return;
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
    }
}


static void gxsi_parse_material_library(GXSI_Parser *p) {
    char name[GXSI_NAME_MAX];
    GXSI_Token t;
    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;
    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; return; }
        if (t.type == GXSI_TOK_RBRACE) return;
        if (gxsi_token_is_name(&t, "SI_Material")) {
            gxsi_parse_material(p);
        } else if (t.type == GXSI_TOK_LBRACE) {
            gxsi_skip_block_after_open(p);
        } else {
            GXSI_Token look = gxsi_next_token(p);
            if (look.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
            else gxsi_push_token(p, look);
        }
    }
}

static void gxsi_parse_model(GXSI_Parser *p) {
    char old_model[GXSI_NAME_MAX];
    char old_material[GXSI_NAME_MAX];
    GXSI_TransformSRT old_transform;
    char name[GXSI_NAME_MAX];
    GXSI_Token t;

    gxsi_expect_block_start(p, name);
    if (p->err != GXSI_OK) return;

    gxsi_copy_name(old_model, p->current_model);
    gxsi_copy_name(old_material, p->current_material);
    old_transform = p->current_transform;

    if (name[0] != '\0') gxsi_copy_name(p->current_model, name);
    p->current_transform = old_transform;

    while (p->err == GXSI_OK) {
        t = gxsi_next_token(p);
        if (t.type == GXSI_TOK_EOF) { p->err = GXSI_ERR_PARSE; break; }
        if (t.type == GXSI_TOK_RBRACE) break;
        if (gxsi_token_is_name(&t, "SI_Transform") || gxsi_token_is_name(&t, "XSI_Transform")) {
            gxsi_parse_transform(p);
        } else if (gxsi_token_is_name(&t, "SI_GlobalMaterial")) {
            gxsi_parse_global_material(p);
        } else if (gxsi_token_is_name(&t, "SI_Mesh") || gxsi_token_is_name(&t, "XSI_Mesh")) {
            gxsi_parse_mesh(p);
        } else if (gxsi_token_is_name(&t, "SI_Model") || gxsi_token_is_name(&t, "XSI_Model")) {
            gxsi_parse_model(p);
        } else if (gxsi_token_is_name(&t, "SI_MaterialLibrary")) {
            gxsi_parse_material_library(p);
        } else if (gxsi_token_is_name(&t, "SI_Envelope") || gxsi_token_is_name(&t, "XSI_Envelope") || gxsi_token_is_name(&t, "Envelope")) {
            gxsi_parse_envelope(p);
        } else if (gxsi_token_is_name(&t, "SI_FCurve") || gxsi_token_is_name(&t, "XSI_FCurve") || gxsi_token_is_name(&t, "FCurve")) {
            gxsi_parse_fcurve(p);
        } else if (gxsi_token_is_name(&t, "SI_Animation") || gxsi_token_is_name(&t, "XSI_Animation") || gxsi_token_is_name(&t, "SI_ActionSource") || gxsi_token_is_name(&t, "XSI_ActionSource")) {
            gxsi_parse_anim_container(p);
        } else if (gxsi_token_is_name(&t, "SI_ChainBone") || gxsi_token_is_name(&t, "XSI_ChainBone") || gxsi_token_is_name(&t, "SI_Bone") || gxsi_token_is_name(&t, "XSI_Bone") || gxsi_token_is_name(&t, "SI_Skeleton")) {
            gxsi_parse_bone_like(p, t.text);
        } else if (gxsi_token_is_name(&t, "XSI_SurfaceMesh") || gxsi_token_is_name(&t, "SI_SurfaceMesh") || gxsi_token_is_name(&t, "XSI_NurbsSurface") || gxsi_token_is_name(&t, "SI_NurbsSurface")) {
            gxsi_parse_nurbs_generic(p, 2U);
        } else if (gxsi_token_is_name(&t, "XSI_CurveList") || gxsi_token_is_name(&t, "SI_CurveList") || gxsi_token_is_name(&t, "XSI_NurbsCurve") || gxsi_token_is_name(&t, "SI_NurbsCurve")) {
            gxsi_parse_nurbs_generic(p, 1U);
        } else if (gxsi_token_is_name(&t, "SI_Material")) {
            gxsi_parse_material(p);
        } else if (t.type == GXSI_TOK_LBRACE) {
            gxsi_skip_block_after_open(p);
        } else {
            GXSI_Token look = gxsi_next_token(p);
            if (look.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(p);
            else gxsi_push_token(p, look);
        }
    }

    gxsi_copy_name(p->current_model, old_model);
    gxsi_copy_name(p->current_material, old_material);
    p->current_transform = old_transform;
}

int gxsi_decode_ascii(const char *data, size_t size, GXSI_Scene *scene, const GXSI_Options *opt) {
    GXSI_Parser p;
    GXSI_Token t;
    GXSI_Options local_opt;

    if (data == 0 || scene == 0) return GXSI_ERR_NULL;
    gxsi_scene_init(scene);
    gxsi_default_options(&local_opt);
    if (opt != 0) local_opt = *opt;

    memset(&p, 0, sizeof(p));
    p.data = data;
    p.size = size;
    p.line = 1;
    p.scene = scene;
    p.opt = local_opt;
    p.err = GXSI_OK;
    p.current_parent_bone = -1;
    p.current_mesh_index = 65535U;
    gxsi_transform_identity(&p.current_transform);

    t = gxsi_next_token(&p);
    if (!gxsi_token_is_name(&t, "xsi")) {
        if ((local_opt.flags & GXSI_OPT_ACCEPT_LOOSE_HEADER) == 0U) return GXSI_ERR_BAD_HEADER;
        gxsi_push_token(&p, t);
    } else {
        GXSI_Token v = gxsi_next_token(&p);
        if (v.type == GXSI_TOK_NUMBER || v.type == GXSI_TOK_IDENT) {
            GXSI_Token fmt;
            /* Common headers look like: xsi 0300txt 0032. The scanner sees that as
               0300 / txt / 0032, so consume the format and numeric-size fields. */
            if (gxsi_str_has(v.text, "bin")) {
                scene->is_binary = 1;
                return GXSI_ERR_UNSUPPORTED_BINARY;
            }
            scene->xsi_major = 3;
            scene->xsi_minor = 0;
            fmt = gxsi_next_token(&p);
            if (fmt.type == GXSI_TOK_IDENT && (gxsi_str_has(fmt.text, "txt") || gxsi_str_has(fmt.text, "ascii"))) {
                GXSI_Token bits = gxsi_next_token(&p);
                if (bits.type != GXSI_TOK_NUMBER) gxsi_push_token(&p, bits);
            } else if (fmt.type == GXSI_TOK_IDENT && gxsi_str_has(fmt.text, "bin")) {
                scene->is_binary = 1;
                return GXSI_ERR_UNSUPPORTED_BINARY;
            } else if (fmt.type == GXSI_TOK_NUMBER && gxsi_str_has(v.text, "txt")) {
                /* version+format came as one token on a looser scanner */
            } else {
                gxsi_push_token(&p, fmt);
            }
        } else {
            gxsi_push_token(&p, v);
        }
    }

    while (p.err == GXSI_OK) {
        t = gxsi_next_token(&p);
        if (t.type == GXSI_TOK_EOF) break;
        if (gxsi_token_is_name(&t, "SI_Model") || gxsi_token_is_name(&t, "XSI_Model")) {
            gxsi_parse_model(&p);
        } else if (gxsi_token_is_name(&t, "SI_Mesh") || gxsi_token_is_name(&t, "XSI_Mesh")) {
            gxsi_parse_mesh(&p);
        } else if (gxsi_token_is_name(&t, "SI_MaterialLibrary")) {
            gxsi_parse_material_library(&p);
        } else if (gxsi_token_is_name(&t, "SI_Material")) {
            gxsi_parse_material(&p);
        } else if (gxsi_token_is_name(&t, "SI_Envelope") || gxsi_token_is_name(&t, "XSI_Envelope") || gxsi_token_is_name(&t, "Envelope")) {
            gxsi_parse_envelope(&p);
        } else if (gxsi_token_is_name(&t, "SI_FCurve") || gxsi_token_is_name(&t, "XSI_FCurve") || gxsi_token_is_name(&t, "FCurve")) {
            gxsi_parse_fcurve(&p);
        } else if (gxsi_token_is_name(&t, "SI_Animation") || gxsi_token_is_name(&t, "XSI_Animation") || gxsi_token_is_name(&t, "SI_ActionSource") || gxsi_token_is_name(&t, "XSI_ActionSource")) {
            gxsi_parse_anim_container(&p);
        } else if (gxsi_token_is_name(&t, "XSI_SurfaceMesh") || gxsi_token_is_name(&t, "SI_SurfaceMesh") || gxsi_token_is_name(&t, "XSI_NurbsSurface") || gxsi_token_is_name(&t, "SI_NurbsSurface")) {
            gxsi_parse_nurbs_generic(&p, 2U);
        } else if (gxsi_token_is_name(&t, "XSI_CurveList") || gxsi_token_is_name(&t, "SI_CurveList") || gxsi_token_is_name(&t, "XSI_NurbsCurve") || gxsi_token_is_name(&t, "SI_NurbsCurve")) {
            gxsi_parse_nurbs_generic(&p, 1U);
        } else if (t.type == GXSI_TOK_LBRACE) {
            gxsi_skip_block_after_open(&p);
        } else {
            GXSI_Token look = gxsi_next_token(&p);
            if (look.type == GXSI_TOK_LBRACE) gxsi_skip_block_after_open(&p);
            else gxsi_push_token(&p, look);
        }
    }

    if (p.err == GXSI_ERR_OVERFLOW && (local_opt.flags & GXSI_OPT_KEEP_GOING) != 0U) {
        return GXSI_ERR_OVERFLOW;
    }
    if (p.err != GXSI_OK) return p.err;
    if ((local_opt.flags & GXSI_OPT_APPLY_TRANSFORMS) != 0U) gxsi_apply_mesh_transforms(scene);
    return GXSI_OK;
}


int gxsi_probe_binary_dotxsi(const unsigned char *data, size_t size, GXSI_Scene *scene) {
    size_t i;
    int has_xsi = 0;
    if (data == 0 || scene == 0) return GXSI_ERR_NULL;
    scene->is_binary = 1;
    scene->binary_probe.detected = 1;
    scene->binary_probe.header_bytes = (int)((size < 64U) ? size : 64U);
    scene->binary_probe.version_hint = 0;
    scene->binary_probe.bits_hint = 0;
    scene->binary_probe.format_hint[0] = '\0';
    for (i = 0U; i + 2U < size && i < 64U; ++i) {
        if ((data[i] == 'x' || data[i] == 'X') && (data[i+1U] == 's' || data[i+1U] == 'S') && (data[i+2U] == 'i' || data[i+2U] == 'I')) has_xsi = 1;
        if ((data[i] == 'b' || data[i] == 'B') && (data[i+1U] == 'i' || data[i+1U] == 'I') && (data[i+2U] == 'n' || data[i+2U] == 'N')) gxsi_copy_small(scene->binary_probe.format_hint, 16U, "bin");
    }
    if (has_xsi) return GXSI_ERR_BINARY_LAYOUT_UNKNOWN;
    return GXSI_ERR_BINARY_LAYOUT_UNKNOWN;
}

int gxsi_decode_auto(const void *data, size_t size, GXSI_Scene *scene, const GXSI_Options *opt) {
    const unsigned char *bytes;
    GXSI_Options local_opt;
    size_t i;
    int textish = 1;
    if (data == 0 || scene == 0) return GXSI_ERR_NULL;
    gxsi_default_options(&local_opt);
    if (opt != 0) local_opt = *opt;
    bytes = (const unsigned char *)data;
    gxsi_scene_init(scene);
    if (size >= 8U) {
        for (i = 0U; i < size && i < 64U; ++i) {
            unsigned char c = bytes[i];
            if (c == 0U || (c < 9U && c != '\n' && c != '\r' && c != '\t')) textish = 0;
        }
        if (!textish) {
            if ((local_opt.flags & GXSI_OPT_BINARY_PROBE) != 0U) return gxsi_probe_binary_dotxsi(bytes, size, scene);
            scene->is_binary = 1;
            return GXSI_ERR_UNSUPPORTED_BINARY;
        }
    }
    return gxsi_decode_ascii((const char *)data, size, scene, &local_opt);
}

const char *gxsi_status_string(int status) {
    switch (status) {
    case GXSI_OK: return "ok";
    case GXSI_ERR_NULL: return "null argument";
    case GXSI_ERR_BAD_HEADER: return "bad or missing XSI header";
    case GXSI_ERR_OVERFLOW: return "static capacity exceeded";
    case GXSI_ERR_PARSE: return "parse error";
    case GXSI_ERR_UNSUPPORTED_BINARY: return "binary dotXSI not enabled";
    case GXSI_ERR_BINARY_LAYOUT_UNKNOWN: return "binary dotXSI detected; layout probe only";
    default: return "unknown status";
    }
}

int gxsi_find_material(const GXSI_Scene *scene, const char *name) {
    unsigned int i;
    if (scene == 0 || name == 0 || name[0] == '\0') return -1;
    for (i = 0; i < scene->material_count; ++i) {
        if (gxsi_str_eq(scene->materials[i].name, name)) return (int)i;
    }
    return -1;
}


int gxsi_find_bone(const GXSI_Scene *scene, const char *name) {
    unsigned int i;
    if (scene == 0 || name == 0 || name[0] == '\0') return -1;
    for (i = 0; i < scene->bone_count; ++i) {
        if (gxsi_str_eq(scene->bones[i].name, name)) return (int)i;
    }
    return -1;
}

static gxsi_fx gxsi_sin_deg_int(int d) {
    static const unsigned long s91[91] = {
        0, 1144, 2287, 3430, 4572, 5712, 6850, 7987, 9121, 10252, 11380, 12505, 13626, 14742, 15855, 16962,
        18064, 19161, 20252, 21336, 22415, 23486, 24550, 25607, 26656, 27697, 28729, 29753, 30767, 31772, 32768, 33754,
        34729, 35693, 36647, 37590, 38521, 39441, 40348, 41243, 42126, 42995, 43852, 44695, 45525, 46341, 47143, 47930,
        48703, 49461, 50203, 50931, 51643, 52339, 53020, 53684, 54332, 54963, 55578, 56175, 56756, 57319, 57865, 58393,
        58903, 59396, 59870, 60326, 60764, 61183, 61584, 61966, 62328, 62672, 62997, 63303, 63589, 63856, 64104, 64332,
        64540, 64729, 64898, 65048, 65177, 65287, 65376, 65446, 65496, 65526, 65536
    };
    int sign = 1;
    while (d < 0) d += 360;
    d %= 360;
    if (d > 180) { d -= 180; sign = -1; }
    if (d > 90) d = 180 - d;
    return sign > 0 ? (gxsi_fx)s91[d] : -(gxsi_fx)s91[d];
}

static gxsi_fx gxsi_cos_deg_int(int d) {
    return gxsi_sin_deg_int(d + 90);
}

static GXSI_Vec3 gxsi_apply_srt_to_vec3(GXSI_Vec3 v, const GXSI_TransformSRT *t) {
    gxsi_fx x;
    gxsi_fx y;
    gxsi_fx z;
    gxsi_fx nx;
    gxsi_fx ny;
    gxsi_fx nz;
    gxsi_fx s;
    gxsi_fx c;
    int rx;
    int ry;
    int rz;
    if (t == 0 || !t->present) return v;
    x = gxsi_fx_mul(v.x, t->scale[0]);
    y = gxsi_fx_mul(v.y, t->scale[1]);
    z = gxsi_fx_mul(v.z, t->scale[2]);
    rx = GXSI_FX_TO_INT(t->rot_deg[0]);
    ry = GXSI_FX_TO_INT(t->rot_deg[1]);
    rz = GXSI_FX_TO_INT(t->rot_deg[2]);
    if (gxsi_fx_nonzero(t->rot_deg[0])) {
        s = gxsi_sin_deg_int(rx);
        c = gxsi_cos_deg_int(rx);
        ny = gxsi_fx_mul(y, c) - gxsi_fx_mul(z, s);
        nz = gxsi_fx_mul(y, s) + gxsi_fx_mul(z, c);
        y = ny;
        z = nz;
    }
    if (gxsi_fx_nonzero(t->rot_deg[1])) {
        s = gxsi_sin_deg_int(ry);
        c = gxsi_cos_deg_int(ry);
        nx = gxsi_fx_mul(x, c) + gxsi_fx_mul(z, s);
        nz = -gxsi_fx_mul(x, s) + gxsi_fx_mul(z, c);
        x = nx;
        z = nz;
    }
    if (gxsi_fx_nonzero(t->rot_deg[2])) {
        s = gxsi_sin_deg_int(rz);
        c = gxsi_cos_deg_int(rz);
        nx = gxsi_fx_mul(x, c) - gxsi_fx_mul(y, s);
        ny = gxsi_fx_mul(x, s) + gxsi_fx_mul(y, c);
        x = nx;
        y = ny;
    }
    v.x = x + t->trans[0];
    v.y = y + t->trans[1];
    v.z = z + t->trans[2];
    return v;
}

void gxsi_apply_mesh_transforms(GXSI_Scene *scene) {
    unsigned int i;
    if (scene == 0) return;
    for (i = 0U; i < scene->mesh_count; ++i) {
        GXSI_Mesh *m = &scene->meshes[i];
        unsigned int j;
        unsigned int end = m->first_vertex + m->vertex_count;
        for (j = m->first_vertex; j < end && j < scene->vertex_count; ++j) {
            scene->vertices[j] = gxsi_apply_srt_to_vec3(scene->vertices[j], &m->transform);
        }
        gxsi_transform_identity(&m->transform);
    }
}

static void gxsi_write_text(GXSI_WriteFn fn, void *user, const char *text) {
    if (fn != 0 && text != 0) fn(user, text);
}

static void gxsi_write_uint(GXSI_WriteFn fn, void *user, unsigned int v) {
    char buf[16];
    char tmp[16];
    unsigned int n = 0U;
    unsigned int i;
    if (v == 0U) {
        gxsi_write_text(fn, user, "0");
        return;
    }
    while (v > 0U && n < 15U) {
        tmp[n++] = (char)('0' + (v % 10U));
        v /= 10U;
    }
    for (i = 0U; i < n; ++i) buf[i] = tmp[n - 1U - i];
    buf[n] = '\0';
    gxsi_write_text(fn, user, buf);
}

void gxsi_fx_to_string(gxsi_fx v, char *out, size_t out_size, int decimals) {
    char tmp[32];
    unsigned long whole;
    unsigned long frac;
    unsigned int pos = 0U;
    unsigned int ti = 0U;
    unsigned int i;
    int neg = 0;
    unsigned long pow10 = 1UL;
    if (out == 0 || out_size == 0U) return;
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;
    if (v < 0) { neg = 1; v = -v; }
    whole = (unsigned long)(v >> 16);
    frac = (unsigned long)(v & 65535L);
    for (i = 0U; i < (unsigned int)decimals; ++i) pow10 *= 10UL;
    frac = (frac * pow10 + 32768UL) / 65536UL;
    if (frac >= pow10 && pow10 > 0UL) { whole++; frac -= pow10; }

    if (neg && pos + 1U < out_size) out[pos++] = '-';
    if (whole == 0UL) {
        tmp[ti++] = '0';
    } else {
        while (whole > 0UL && ti < 31U) {
            tmp[ti++] = (char)('0' + (whole % 10UL));
            whole /= 10UL;
        }
    }
    while (ti > 0U && pos + 1U < out_size) out[pos++] = tmp[--ti];
    if (decimals > 0 && pos + 1U < out_size) {
        unsigned long div = pow10 / 10UL;
        out[pos++] = '.';
        while (div > 0UL && pos + 1U < out_size) {
            out[pos++] = (char)('0' + (frac / div) % 10UL);
            div /= 10UL;
        }
    }
    out[pos] = '\0';
}

static void gxsi_write_fx(GXSI_WriteFn fn, void *user, gxsi_fx v) {
    char buf[32];
    gxsi_fx_to_string(v, buf, sizeof(buf), 6);
    gxsi_write_text(fn, user, buf);
}

void gxsi_write_obj(const GXSI_Scene *scene, GXSI_WriteFn write_fn, void *user) {
    unsigned int i;
    if (scene == 0 || write_fn == 0) return;
    gxsi_write_text(write_fn, user, "# OBJ emitted by gxsi_decoder3d\n");
    for (i = 0; i < scene->vertex_count; ++i) {
        gxsi_write_text(write_fn, user, "v ");
        gxsi_write_fx(write_fn, user, scene->vertices[i].x);
        gxsi_write_text(write_fn, user, " ");
        gxsi_write_fx(write_fn, user, scene->vertices[i].y);
        gxsi_write_text(write_fn, user, " ");
        gxsi_write_fx(write_fn, user, scene->vertices[i].z);
        gxsi_write_text(write_fn, user, "\n");
    }
    for (i = 0; i < scene->normal_count; ++i) {
        gxsi_write_text(write_fn, user, "vn ");
        gxsi_write_fx(write_fn, user, scene->normals[i].x);
        gxsi_write_text(write_fn, user, " ");
        gxsi_write_fx(write_fn, user, scene->normals[i].y);
        gxsi_write_text(write_fn, user, " ");
        gxsi_write_fx(write_fn, user, scene->normals[i].z);
        gxsi_write_text(write_fn, user, "\n");
    }
    for (i = 0; i < scene->texcoord_count; ++i) {
        gxsi_write_text(write_fn, user, "vt ");
        gxsi_write_fx(write_fn, user, scene->texcoords[i].u);
        gxsi_write_text(write_fn, user, " ");
        gxsi_write_fx(write_fn, user, scene->texcoords[i].v);
        gxsi_write_text(write_fn, user, "\n");
    }
    for (i = 0; i < scene->mesh_count; ++i) {
        gxsi_write_text(write_fn, user, "o ");
        gxsi_write_text(write_fn, user, scene->meshes[i].name[0] ? scene->meshes[i].name : "mesh");
        gxsi_write_text(write_fn, user, "\n");
        {
            unsigned int j;
            unsigned int end = scene->meshes[i].first_triangle + scene->meshes[i].triangle_count;
            for (j = scene->meshes[i].first_triangle; j < end; ++j) {
                gxsi_write_text(write_fn, user, "f ");
                gxsi_write_uint(write_fn, user, (unsigned int)scene->triangles[j].v[0] + 1U);
                gxsi_write_text(write_fn, user, " ");
                gxsi_write_uint(write_fn, user, (unsigned int)scene->triangles[j].v[1] + 1U);
                gxsi_write_text(write_fn, user, " ");
                gxsi_write_uint(write_fn, user, (unsigned int)scene->triangles[j].v[2] + 1U);
                gxsi_write_text(write_fn, user, "\n");
            }
        }
    }
}
