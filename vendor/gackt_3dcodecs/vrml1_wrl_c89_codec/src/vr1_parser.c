#include <string.h>
#include <ctype.h>
#include "vr1_parser.h"

#define VR1_OK 0
#define VR1_ERR_HEADER -1
#define VR1_ERR_PARSE -2
#define VR1_ERR_LIMIT -3

#define VR1_WRAP_REPEAT 1
#define VR1_WRAP_CLAMP  2

#define VR1_PART_SIDES  1
#define VR1_PART_BOTTOM 2
#define VR1_PART_TOP    4
#define VR1_PART_ALL    7

#define VR1_TEXT_LEFT   0
#define VR1_TEXT_CENTER 1
#define VR1_TEXT_RIGHT  2

static int vr1_parse_any_node(vr1_parser_context *p);
static int vr1_parse_node_named(vr1_parser_context *p, const char *type_name, long start_offset, const char *def_name);
static void vr1_skip_value(vr1_parser_context *p);
static void vr1_skip_block_after_lbrace(vr1_parser_context *p);

static void vr1_copy_text(char *dst, int cap, const char *src)
{
    int i;

    if (dst == 0 || cap <= 0) return;
    dst[0] = '\0';
    if (src == 0) return;
    i = 0;
    while (i < cap - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int vr1_streq(const char *a, const char *b)
{
    if (a == 0 || b == 0) return 0;
    return strcmp(a, b) == 0;
}

static int vr1_is_nodeish_name(const char *s)
{
    if (s == 0 || s[0] == '\0') return 0;
    if (s[0] >= 'A' && s[0] <= 'Z') return 1;
    return 0;
}

static void vr1_mat_identity(vr1_fx m[16])
{
    int i;

    for (i = 0; i < 16; i++) m[i] = 0;
    m[0] = VR1_FX_ONE;
    m[5] = VR1_FX_ONE;
    m[10] = VR1_FX_ONE;
    m[15] = VR1_FX_ONE;
}

static void vr1_mat_copy(vr1_fx dst[16], const vr1_fx src[16])
{
    int i;
    for (i = 0; i < 16; i++) dst[i] = src[i];
}

static void vr1_mat_mul(vr1_fx out[16], const vr1_fx a[16], const vr1_fx b[16])
{
    int r;
    int c;
    int k;
    vr1_fx tmp[16];
    vr1_fx sum;

    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            sum = 0;
            for (k = 0; k < 4; k++) {
                sum += vr1_fx_mul(a[r * 4 + k], b[k * 4 + c]);
            }
            tmp[r * 4 + c] = sum;
        }
    }
    for (r = 0; r < 16; r++) out[r] = tmp[r];
}

static void vr1_state_postmul(vr1_state *s, const vr1_fx m[16])
{
    vr1_fx tmp[16];
    vr1_mat_mul(tmp, s->matrix, m);
    vr1_mat_copy(s->matrix, tmp);
}

static void vr1_texmat_identity(vr1_fx m[9])
{
    int i;

    for (i = 0; i < 9; i++) m[i] = 0;
    m[0] = VR1_FX_ONE;
    m[4] = VR1_FX_ONE;
    m[8] = VR1_FX_ONE;
}

static void vr1_texmat_copy(vr1_fx dst[9], const vr1_fx src[9])
{
    int i;
    for (i = 0; i < 9; i++) dst[i] = src[i];
}

static void vr1_texmat_mul(vr1_fx out[9], const vr1_fx a[9], const vr1_fx b[9])
{
    int r;
    int c;
    int k;
    vr1_fx tmp[9];
    vr1_fx sum;

    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            sum = 0;
            for (k = 0; k < 3; k++) {
                sum += vr1_fx_mul(a[r * 3 + k], b[k * 3 + c]);
            }
            tmp[r * 3 + c] = sum;
        }
    }
    vr1_texmat_copy(out, tmp);
}

static void vr1_state_postmul_tex(vr1_state *s, const vr1_fx m[9])
{
    vr1_fx tmp[9];
    vr1_texmat_mul(tmp, s->tex_matrix, m);
    vr1_texmat_copy(s->tex_matrix, tmp);
}

static void vr1_texmat_texture2(vr1_fx m[9], vr1_vec2 translation, vr1_vec2 scale_factor, vr1_vec2 center, vr1_fx rotation)
{
    vr1_fx c;
    vr1_fx sn;
    vr1_fx sx;
    vr1_fx sy;

    c = vr1_fx_cos(rotation);
    sn = vr1_fx_sin(rotation);
    sx = scale_factor.x;
    sy = scale_factor.y;
    vr1_texmat_identity(m);
    m[0] = vr1_fx_mul(sx, c);
    m[1] = vr1_fx_mul(sx, sn);
    m[3] = -vr1_fx_mul(sy, sn);
    m[4] = vr1_fx_mul(sy, c);
    m[6] = -vr1_fx_mul(center.x, m[0]) - vr1_fx_mul(center.y, m[3]) + center.x + translation.x;
    m[7] = -vr1_fx_mul(center.x, m[1]) - vr1_fx_mul(center.y, m[4]) + center.y + translation.y;
}

static void vr1_mat_translation(vr1_fx m[16], vr1_vec3 t)
{
    vr1_mat_identity(m);
    m[12] = t.x;
    m[13] = t.y;
    m[14] = t.z;
}

static void vr1_mat_scale(vr1_fx m[16], vr1_vec3 s)
{
    vr1_mat_identity(m);
    m[0] = s.x;
    m[5] = s.y;
    m[10] = s.z;
}

static vr1_vec3 vr1_vec3_make(vr1_fx x, vr1_fx y, vr1_fx z)
{
    vr1_vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

static vr1_vec2 vr1_vec2_make(vr1_fx x, vr1_fx y)
{
    vr1_vec2 v;
    v.x = x;
    v.y = y;
    return v;
}

static vr1_vec3 vr1_vec3_sub(vr1_vec3 a, vr1_vec3 b)
{
    vr1_vec3 v;
    v.x = a.x - b.x;
    v.y = a.y - b.y;
    v.z = a.z - b.z;
    return v;
}

static vr1_vec3 vr1_vec3_cross(vr1_vec3 a, vr1_vec3 b)
{
    vr1_vec3 v;
    v.x = vr1_fx_mul(a.y, b.z) - vr1_fx_mul(a.z, b.y);
    v.y = vr1_fx_mul(a.z, b.x) - vr1_fx_mul(a.x, b.z);
    v.z = vr1_fx_mul(a.x, b.y) - vr1_fx_mul(a.y, b.x);
    return v;
}

static vr1_vec3 vr1_vec3_normalize(vr1_vec3 v)
{
    vr1_fx len2;
    vr1_fx len;

    len2 = vr1_fx_mul(v.x, v.x) + vr1_fx_mul(v.y, v.y) + vr1_fx_mul(v.z, v.z);
    len = vr1_fx_sqrt(len2);
    if (len <= 0) return vr1_vec3_make(0, 0, VR1_FX_ONE);
    v.x = vr1_fx_div(v.x, len);
    v.y = vr1_fx_div(v.y, len);
    v.z = vr1_fx_div(v.z, len);
    return v;
}

static void vr1_mat_rotation(vr1_fx m[16], vr1_vec3 axis, vr1_fx angle)
{
    vr1_fx x;
    vr1_fx y;
    vr1_fx z;
    vr1_fx c;
    vr1_fx s;
    vr1_fx t;
    vr1_fx tx;
    vr1_fx ty;
    vr1_fx tz;
    vr1_fx sx;
    vr1_fx sy;
    vr1_fx sz;

    axis = vr1_vec3_normalize(axis);
    x = axis.x;
    y = axis.y;
    z = axis.z;
    c = vr1_fx_cos(angle);
    s = vr1_fx_sin(angle);
    t = VR1_FX_ONE - c;
    tx = vr1_fx_mul(t, x);
    ty = vr1_fx_mul(t, y);
    tz = vr1_fx_mul(t, z);
    sx = vr1_fx_mul(s, x);
    sy = vr1_fx_mul(s, y);
    sz = vr1_fx_mul(s, z);

    vr1_mat_identity(m);
    /* Row-vector form: transpose of the common column-vector axis-angle matrix. */
    m[0] = vr1_fx_mul(tx, x) + c;
    m[1] = vr1_fx_mul(tx, y) + sz;
    m[2] = vr1_fx_mul(tx, z) - sy;
    m[4] = vr1_fx_mul(ty, x) - sz;
    m[5] = vr1_fx_mul(ty, y) + c;
    m[6] = vr1_fx_mul(ty, z) + sx;
    m[8] = vr1_fx_mul(tz, x) + sy;
    m[9] = vr1_fx_mul(tz, y) - sx;
    m[10] = vr1_fx_mul(tz, z) + c;
}

static vr1_vec3 vr1_transform_point(const vr1_fx m[16], vr1_vec3 pnt)
{
    vr1_vec3 out;

    out.x = vr1_fx_mul(pnt.x, m[0]) + vr1_fx_mul(pnt.y, m[4]) + vr1_fx_mul(pnt.z, m[8]) + m[12];
    out.y = vr1_fx_mul(pnt.x, m[1]) + vr1_fx_mul(pnt.y, m[5]) + vr1_fx_mul(pnt.z, m[9]) + m[13];
    out.z = vr1_fx_mul(pnt.x, m[2]) + vr1_fx_mul(pnt.y, m[6]) + vr1_fx_mul(pnt.z, m[10]) + m[14];
    return out;
}

static vr1_vec3 vr1_transform_vector(const vr1_fx m[16], vr1_vec3 v)
{
    vr1_vec3 out;

    out.x = vr1_fx_mul(v.x, m[0]) + vr1_fx_mul(v.y, m[4]) + vr1_fx_mul(v.z, m[8]);
    out.y = vr1_fx_mul(v.x, m[1]) + vr1_fx_mul(v.y, m[5]) + vr1_fx_mul(v.z, m[9]);
    out.z = vr1_fx_mul(v.x, m[2]) + vr1_fx_mul(v.y, m[6]) + vr1_fx_mul(v.z, m[10]);
    return vr1_vec3_normalize(out);
}

static void vr1_state_init(vr1_state *s)
{
    int i;

    if (s == 0) return;
    memset(s, 0, sizeof(*s));
    s->coord_count = 0;
    s->normal_count = 0;
    s->texcoord_count = 0;
    s->mat_count = 1;
    vr1_material_default(&s->mats[0]);
    s->material_binding = VR1_BIND_DEFAULT;
    s->normal_binding = VR1_BIND_DEFAULT;
    s->texcoord_binding = VR1_BIND_DEFAULT;
    s->vertex_ordering = VR1_ORDER_UNKNOWN;
    s->shape_type = VR1_SHAPE_UNKNOWN;
    s->face_type = VR1_FACE_CONVEX;
    s->crease_angle = vr1_fx_from_ascii("0.5", 3, 0);
    s->tex_translation = vr1_vec2_make(0, 0);
    s->tex_scale = vr1_vec2_make(VR1_FX_ONE, VR1_FX_ONE);
    s->tex_center = vr1_vec2_make(0, 0);
    s->tex_rotation = 0;
    vr1_texmat_identity(s->tex_matrix);
    s->font_size = vr1_fx_from_int(10);
    s->font_family = 0;
    s->font_style = 0;
    vr1_mat_identity(s->matrix);
    for (i = 1; i < VR1_MAX_STATE_MATERIALS; i++) {
        vr1_material_default(&s->mats[i]);
    }
}

void vr1_parser_context_init(vr1_parser_context *p)
{
    if (p == 0) return;
    memset(p, 0, sizeof(*p));
    vr1_state_init(&p->state);
    p->emit_enabled = 1;
}

static int vr1_parse_long_text(const char *text, int len, long *out)
{
    int i;
    int sign;
    int base;
    long v;
    int digit;
    int any;

    if (out == 0 || text == 0 || len <= 0) return 0;
    i = 0;
    sign = 1;
    if (text[i] == '-') { sign = -1; i++; }
    else if (text[i] == '+') { i++; }
    base = 10;
    if (i + 1 < len && text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
        base = 16;
        i += 2;
    } else if (i + 1 < len && text[i] == '0') {
        base = 8;
        i++;
    }
    v = 0;
    any = 0;
    while (i < len) {
        if (text[i] >= '0' && text[i] <= '9') digit = text[i] - '0';
        else if (text[i] >= 'a' && text[i] <= 'f') digit = text[i] - 'a' + 10;
        else if (text[i] >= 'A' && text[i] <= 'F') digit = text[i] - 'A' + 10;
        else break;
        if (digit >= base) break;
        any = 1;
        if (v < 0x0fffffffL) v = v * (long)base + (long)digit;
        i++;
    }
    if (!any) return 0;
    *out = (sign < 0) ? -v : v;
    return 1;
}

static int vr1_next_is(vr1_parser_context *p, int type)
{
    vr1_token t;
    vr1_lexer_peek(&p->lx, &t);
    return t.type == type;
}

static int vr1_accept(vr1_parser_context *p, int type)
{
    vr1_token t;
    vr1_lexer_peek(&p->lx, &t);
    if (t.type == type) {
        vr1_lexer_next(&p->lx);
        return 1;
    }
    return 0;
}

static int vr1_expect(vr1_parser_context *p, int type)
{
    if (vr1_accept(p, type)) return 1;
    vr1_scene_error(p->scene);
    return 0;
}

static vr1_fx vr1_parse_fx_token(vr1_parser_context *p)
{
    int ok;
    vr1_fx v;

    vr1_lexer_next(&p->lx);
    if (p->lx.tok.type != VR1_T_NUMBER && p->lx.tok.type != VR1_T_IDENT) {
        vr1_scene_error(p->scene);
        return 0;
    }
    ok = 0;
    v = vr1_fx_from_ascii(p->lx.tok.text, p->lx.tok.len, &ok);
    if (!ok) vr1_scene_warn(p->scene);
    return v;
}

static long vr1_parse_long_token(vr1_parser_context *p)
{
    long v;

    vr1_lexer_next(&p->lx);
    if (p->lx.tok.type != VR1_T_NUMBER && p->lx.tok.type != VR1_T_IDENT) {
        vr1_scene_error(p->scene);
        return 0;
    }
    v = 0;
    if (!vr1_parse_long_text(p->lx.tok.text, p->lx.tok.len, &v)) {
        vr1_scene_warn(p->scene);
        return 0;
    }
    return v;
}

static int vr1_parse_bool_token(vr1_parser_context *p)
{
    vr1_lexer_next(&p->lx);
    if (p->lx.tok.type == VR1_T_IDENT) {
        if (vr1_streq(p->lx.tok.text, "TRUE")) return 1;
        if (vr1_streq(p->lx.tok.text, "FALSE")) return 0;
    }
    if (p->lx.tok.type == VR1_T_NUMBER || p->lx.tok.type == VR1_T_IDENT) {
        long v;
        v = 0;
        if (vr1_parse_long_text(p->lx.tok.text, p->lx.tok.len, &v)) return v != 0;
    }
    vr1_scene_warn(p->scene);
    return 0;
}

static void vr1_parse_string_token(vr1_parser_context *p, char *out, int cap)
{
    vr1_lexer_next(&p->lx);
    if (p->lx.tok.type == VR1_T_STRING || p->lx.tok.type == VR1_T_IDENT || p->lx.tok.type == VR1_T_NUMBER) {
        vr1_copy_text(out, cap, p->lx.tok.text);
    } else {
        if (out != 0 && cap > 0) out[0] = '\0';
        vr1_scene_warn(p->scene);
    }
}

static vr1_vec3 vr1_parse_vec3(vr1_parser_context *p)
{
    vr1_vec3 v;
    v.x = vr1_parse_fx_token(p);
    v.y = vr1_parse_fx_token(p);
    v.z = vr1_parse_fx_token(p);
    return v;
}

static vr1_vec2 vr1_parse_vec2(vr1_parser_context *p)
{
    vr1_vec2 v;
    v.x = vr1_parse_fx_token(p);
    v.y = vr1_parse_fx_token(p);
    return v;
}

static vr1_vec3 vr1_parse_vec4_as_vec3(vr1_parser_context *p)
{
    vr1_vec3 v;
    vr1_fx w;

    v.x = vr1_parse_fx_token(p);
    v.y = vr1_parse_fx_token(p);
    v.z = vr1_parse_fx_token(p);
    w = vr1_parse_fx_token(p);
    if (w != 0 && w != VR1_FX_ONE) {
        v.x = vr1_fx_div(v.x, w);
        v.y = vr1_fx_div(v.y, w);
        v.z = vr1_fx_div(v.z, w);
    }
    return v;
}

static int vr1_parse_binding_enum(const char *s)
{
    if (vr1_streq(s, "DEFAULT")) return VR1_BIND_DEFAULT;
    if (vr1_streq(s, "OVERALL")) return VR1_BIND_OVERALL;
    if (vr1_streq(s, "PER_PART")) return VR1_BIND_PER_PART;
    if (vr1_streq(s, "PER_PART_INDEXED")) return VR1_BIND_PER_PART_INDEXED;
    if (vr1_streq(s, "PER_FACE")) return VR1_BIND_PER_FACE;
    if (vr1_streq(s, "PER_FACE_INDEXED")) return VR1_BIND_PER_FACE_INDEXED;
    if (vr1_streq(s, "PER_VERTEX")) return VR1_BIND_PER_VERTEX;
    if (vr1_streq(s, "PER_VERTEX_INDEXED")) return VR1_BIND_PER_VERTEX_INDEXED;
    return VR1_BIND_DEFAULT;
}

static void vr1_parse_mflong(vr1_parser_context *p, long *arr, long *count, long max_count)
{
    int bracket;
    long n;

    if (count == 0 || arr == 0) return;
    *count = 0;
    bracket = vr1_accept(p, VR1_T_LBRACKET);
    while (1) {
        if (bracket && vr1_accept(p, VR1_T_RBRACKET)) break;
        if (!bracket && *count > 0) break;
        if (vr1_accept(p, VR1_T_COMMA)) continue;
        if (vr1_next_is(p, VR1_T_EOF) || vr1_next_is(p, VR1_T_RBRACE)) break;
        if (vr1_next_is(p, VR1_T_NUMBER) || vr1_next_is(p, VR1_T_IDENT)) {
            n = vr1_parse_long_token(p);
            if (*count < max_count) {
                arr[*count] = n;
                *count = *count + 1;
            } else {
                vr1_scene_error(p->scene);
            }
        } else {
            vr1_scene_warn(p->scene);
            vr1_lexer_next(&p->lx);
        }
    }
}

static int vr1_parse_mfvec3(vr1_parser_context *p, vr1_vec3 *arr, int max_count)
{
    int bracket;
    int count;

    count = 0;
    bracket = vr1_accept(p, VR1_T_LBRACKET);
    while (1) {
        if (bracket && vr1_accept(p, VR1_T_RBRACKET)) break;
        if (!bracket && count > 0) break;
        if (vr1_accept(p, VR1_T_COMMA)) continue;
        if (vr1_next_is(p, VR1_T_EOF) || vr1_next_is(p, VR1_T_RBRACE)) break;
        if (count < max_count) {
            arr[count] = vr1_parse_vec3(p);
            count++;
        } else {
            vr1_scene_error(p->scene);
            (void)vr1_parse_vec3(p);
        }
        (void)vr1_accept(p, VR1_T_COMMA);
    }
    return count;
}

static int vr1_parse_mfvec4_as_vec3(vr1_parser_context *p, vr1_vec3 *arr, int max_count)
{
    int bracket;
    int count;

    count = 0;
    bracket = vr1_accept(p, VR1_T_LBRACKET);
    while (1) {
        if (bracket && vr1_accept(p, VR1_T_RBRACKET)) break;
        if (!bracket && count > 0) break;
        if (vr1_accept(p, VR1_T_COMMA)) continue;
        if (vr1_next_is(p, VR1_T_EOF) || vr1_next_is(p, VR1_T_RBRACE)) break;
        if (count < max_count) {
            arr[count] = vr1_parse_vec4_as_vec3(p);
            count++;
        } else {
            vr1_scene_error(p->scene);
            (void)vr1_parse_vec4_as_vec3(p);
        }
        (void)vr1_accept(p, VR1_T_COMMA);
    }
    return count;
}

static int vr1_parse_mfvec2(vr1_parser_context *p, vr1_vec2 *arr, int max_count)
{
    int bracket;
    int count;

    count = 0;
    bracket = vr1_accept(p, VR1_T_LBRACKET);
    while (1) {
        if (bracket && vr1_accept(p, VR1_T_RBRACKET)) break;
        if (!bracket && count > 0) break;
        if (vr1_accept(p, VR1_T_COMMA)) continue;
        if (vr1_next_is(p, VR1_T_EOF) || vr1_next_is(p, VR1_T_RBRACE)) break;
        if (count < max_count) {
            arr[count] = vr1_parse_vec2(p);
            count++;
        } else {
            vr1_scene_error(p->scene);
            (void)vr1_parse_vec2(p);
        }
        (void)vr1_accept(p, VR1_T_COMMA);
    }
    return count;
}

static int vr1_parse_mfcolor(vr1_parser_context *p, vr1_color *arr, int max_count)
{
    int bracket;
    int count;
    vr1_vec3 v;

    count = 0;
    bracket = vr1_accept(p, VR1_T_LBRACKET);
    while (1) {
        if (bracket && vr1_accept(p, VR1_T_RBRACKET)) break;
        if (!bracket && count > 0) break;
        if (vr1_accept(p, VR1_T_COMMA)) continue;
        if (vr1_next_is(p, VR1_T_EOF) || vr1_next_is(p, VR1_T_RBRACE)) break;
        v = vr1_parse_vec3(p);
        if (count < max_count) {
            arr[count].r = v.x;
            arr[count].g = v.y;
            arr[count].b = v.z;
            count++;
        } else {
            vr1_scene_error(p->scene);
        }
        (void)vr1_accept(p, VR1_T_COMMA);
    }
    return count;
}

static int vr1_hex_digit_value(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int vr1_parse_rgba_token_text(const char *text, int len, vr1_color *color, vr1_fx *alpha)
{
    int i;
    unsigned long v;
    int digit;
    unsigned long r;
    unsigned long g;
    unsigned long b;
    unsigned long a;

    if (text == 0 || color == 0 || alpha == 0) return 0;
    i = 0;
    if (len >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) i = 2;
    v = 0UL;
    while (i < len) {
        digit = vr1_hex_digit_value((unsigned char)text[i]);
        if (digit < 0) return 0;
        if (v <= 0x0fffffffUL) v = (v << 4) | (unsigned long)digit;
        i++;
    }
    r = (v >> 24) & 255UL;
    g = (v >> 16) & 255UL;
    b = (v >> 8) & 255UL;
    a = v & 255UL;
    color->r = (vr1_fx)((r * (unsigned long)VR1_FX_ONE + 127UL) / 255UL);
    color->g = (vr1_fx)((g * (unsigned long)VR1_FX_ONE + 127UL) / 255UL);
    color->b = (vr1_fx)((b * (unsigned long)VR1_FX_ONE + 127UL) / 255UL);
    *alpha = (vr1_fx)((a * (unsigned long)VR1_FX_ONE + 127UL) / 255UL);
    return 1;
}

static int vr1_parse_mfrgba(vr1_parser_context *p, vr1_color *colors, vr1_fx *transparency, int max_count)
{
    int bracket;
    int count;
    vr1_color c;
    vr1_fx alpha;

    count = 0;
    bracket = vr1_accept(p, VR1_T_LBRACKET);
    while (1) {
        if (bracket && vr1_accept(p, VR1_T_RBRACKET)) break;
        if (!bracket && count > 0) break;
        if (vr1_accept(p, VR1_T_COMMA)) continue;
        if (vr1_next_is(p, VR1_T_EOF) || vr1_next_is(p, VR1_T_RBRACE)) break;
        if (vr1_next_is(p, VR1_T_NUMBER) || vr1_next_is(p, VR1_T_IDENT)) {
            vr1_lexer_next(&p->lx);
            c.r = c.g = c.b = 0;
            alpha = VR1_FX_ONE;
            if (!vr1_parse_rgba_token_text(p->lx.tok.text, p->lx.tok.len, &c, &alpha)) {
                vr1_scene_warn(p->scene);
            } else if (count < max_count) {
                colors[count] = c;
                transparency[count] = VR1_FX_ONE - alpha;
                count++;
            } else {
                vr1_scene_error(p->scene);
            }
        } else {
            vr1_scene_warn(p->scene);
            vr1_lexer_next(&p->lx);
        }
        (void)vr1_accept(p, VR1_T_COMMA);
    }
    return count;
}

static int vr1_parse_mffx(vr1_parser_context *p, vr1_fx *arr, int max_count)
{
    int bracket;
    int count;

    count = 0;
    bracket = vr1_accept(p, VR1_T_LBRACKET);
    while (1) {
        if (bracket && vr1_accept(p, VR1_T_RBRACKET)) break;
        if (!bracket && count > 0) break;
        if (vr1_accept(p, VR1_T_COMMA)) continue;
        if (vr1_next_is(p, VR1_T_EOF) || vr1_next_is(p, VR1_T_RBRACE)) break;
        if (count < max_count) {
            arr[count] = vr1_parse_fx_token(p);
            count++;
        } else {
            vr1_scene_error(p->scene);
            (void)vr1_parse_fx_token(p);
        }
        (void)vr1_accept(p, VR1_T_COMMA);
    }
    return count;
}

static int vr1_parse_mfstring(vr1_parser_context *p, char arr[VR1_MAX_TEXT_STRINGS][VR1_STRING_MAX], int max_count)
{
    int bracket;
    int count;

    count = 0;
    bracket = vr1_accept(p, VR1_T_LBRACKET);
    while (1) {
        if (bracket && vr1_accept(p, VR1_T_RBRACKET)) break;
        if (!bracket && count > 0) break;
        if (vr1_accept(p, VR1_T_COMMA)) continue;
        if (vr1_next_is(p, VR1_T_EOF) || vr1_next_is(p, VR1_T_RBRACE)) break;
        if (count < max_count) {
            vr1_parse_string_token(p, arr[count], VR1_STRING_MAX);
            count++;
        } else {
            vr1_scene_error(p->scene);
            vr1_skip_value(p);
        }
        (void)vr1_accept(p, VR1_T_COMMA);
    }
    return count;
}

static void vr1_skip_value(vr1_parser_context *p)
{
    int depth_brace;
    int depth_bracket;
    int depth_paren;
    vr1_token t;

    depth_brace = 0;
    depth_bracket = 0;
    depth_paren = 0;
    while (1) {
        vr1_lexer_peek(&p->lx, &t);
        if (t.type == VR1_T_EOF) break;
        if (depth_brace == 0 && depth_bracket == 0 && depth_paren == 0) {
            if (t.type == VR1_T_RBRACE || t.type == VR1_T_RBRACKET || t.type == VR1_T_RPAREN) break;
            if (t.type == VR1_T_IDENT && vr1_is_nodeish_name(t.text)) break;
            if (t.type == VR1_T_IDENT && t.text[0] >= 'a' && t.text[0] <= 'z') {
                /* Most field names are lowercase; leave it to the owning parser. */
                break;
            }
        }
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_LBRACE) depth_brace++;
        else if (p->lx.tok.type == VR1_T_RBRACE) { if (depth_brace > 0) depth_brace--; else break; }
        else if (p->lx.tok.type == VR1_T_LBRACKET) depth_bracket++;
        else if (p->lx.tok.type == VR1_T_RBRACKET) { if (depth_bracket > 0) depth_bracket--; else break; }
        else if (p->lx.tok.type == VR1_T_LPAREN) depth_paren++;
        else if (p->lx.tok.type == VR1_T_RPAREN) { if (depth_paren > 0) depth_paren--; else break; }
        if (depth_brace == 0 && depth_bracket == 0 && depth_paren == 0) {
            if (p->lx.tok.type != VR1_T_LBRACE && p->lx.tok.type != VR1_T_LBRACKET && p->lx.tok.type != VR1_T_LPAREN) break;
        }
    }
}

static void vr1_skip_block_after_lbrace(vr1_parser_context *p)
{
    int depth;

    depth = 1;
    while (depth > 0) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_EOF) break;
        if (p->lx.tok.type == VR1_T_LBRACE) depth++;
        else if (p->lx.tok.type == VR1_T_RBRACE) depth--;
    }
}

static void vr1_def_store(vr1_parser_context *p, const char *name, long start, long end)
{
    int i;
    int slot;

    if (p == 0 || name == 0 || name[0] == '\0') return;
    slot = -1;
    for (i = 0; i < p->def_count; i++) {
        if (p->defs[i].valid && vr1_streq(p->defs[i].name, name)) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (p->def_count >= VR1_MAX_DEFS) {
            vr1_scene_warn(p->scene);
            return;
        }
        slot = p->def_count;
        p->def_count++;
    }
    vr1_copy_text(p->defs[slot].name, VR1_NAME_MAX, name);
    p->defs[slot].start = start;
    p->defs[slot].end = end;
    p->defs[slot].valid = 1;
}

static vr1_def *vr1_def_find(vr1_parser_context *p, const char *name)
{
    int i;

    if (p == 0 || name == 0) return 0;
    for (i = p->def_count - 1; i >= 0; i--) {
        if (p->defs[i].valid && vr1_streq(p->defs[i].name, name)) return &p->defs[i];
    }
    return 0;
}

static int vr1_replay_def(vr1_parser_context *p, const char *name)
{
    vr1_def *d;
    vr1_lexer saved_lx;
    int saved_replay;
    long len;
    int result;

    d = vr1_def_find(p, name);
    if (d == 0 || d->end <= d->start) {
        vr1_scene_warn(p->scene);
        return 0;
    }
    if (p->replay_depth >= VR1_MAX_REPLAY_DEPTH) {
        vr1_scene_warn(p->scene);
        return 0;
    }
    saved_lx = p->lx;
    saved_replay = p->replay_depth;
    p->replay_depth++;
    len = d->end - d->start;
    vr1_lexer_init(&p->lx, p->source + d->start, len, 0);
    result = vr1_parse_any_node(p);
    p->lx = saved_lx;
    p->replay_depth = saved_replay;
    return result;
}

static void vr1_push_state(vr1_parser_context *p)
{
    if (p->stack_top >= VR1_MAX_STACK_DEPTH) {
        vr1_scene_error(p->scene);
        return;
    }
    p->stack[p->stack_top] = p->state;
    p->stack_top++;
}

static void vr1_pop_state(vr1_parser_context *p)
{
    if (p->stack_top <= 0) {
        vr1_scene_error(p->scene);
        return;
    }
    p->stack_top--;
    p->state = p->stack[p->stack_top];
}

static void vr1_push_matrix(vr1_parser_context *p)
{
    if (p->matrix_stack_top >= VR1_MAX_STACK_DEPTH) {
        vr1_scene_error(p->scene);
        return;
    }
    vr1_mat_copy(p->matrix_stack[p->matrix_stack_top], p->state.matrix);
    p->matrix_stack_top++;
}

static void vr1_pop_matrix(vr1_parser_context *p)
{
    if (p->matrix_stack_top <= 0) {
        vr1_scene_error(p->scene);
        return;
    }
    p->matrix_stack_top--;
    vr1_mat_copy(p->state.matrix, p->matrix_stack[p->matrix_stack_top]);
}

static int vr1_material_local_index(vr1_parser_context *p, long local)
{
    if (p->state.mat_count <= 0) {
        p->state.mat_count = 1;
        vr1_material_default(&p->state.mats[0]);
    }
    if (local < 0) local = 0;
    if (p->state.mat_count > 0) local = local % (long)p->state.mat_count;
    if (local < 0 || local >= p->state.mat_count) local = 0;
    return vr1_scene_add_material(p->scene, &p->state.mats[(int)local]);
}

static int vr1_current_material_index(vr1_parser_context *p, long face_index, long vertex_index, long explicit_index)
{
    long local;
    int bind;

    bind = p->state.material_binding;
    local = 0;
    if (bind == VR1_BIND_PER_PART || bind == VR1_BIND_PER_FACE) local = face_index;
    else if (bind == VR1_BIND_PER_PART_INDEXED || bind == VR1_BIND_PER_FACE_INDEXED) {
        if (explicit_index >= 0) local = explicit_index;
        else local = face_index;
    } else if (bind == VR1_BIND_PER_VERTEX) local = vertex_index;
    else if (bind == VR1_BIND_PER_VERTEX_INDEXED) {
        if (explicit_index >= 0) local = explicit_index;
        else local = vertex_index;
    } else {
        local = 0;
    }
    return vr1_material_local_index(p, local);
}

static int vr1_current_part_material_index(vr1_parser_context *p, long part_index)
{
    return vr1_current_material_index(p, part_index, 0, -1);
}

static vr1_vec2 vr1_apply_texture_transform(vr1_parser_context *p, vr1_vec2 uv)
{
    vr1_vec2 out;

    out.x = vr1_fx_mul(uv.x, p->state.tex_matrix[0]) + vr1_fx_mul(uv.y, p->state.tex_matrix[3]) + p->state.tex_matrix[6];
    out.y = vr1_fx_mul(uv.x, p->state.tex_matrix[1]) + vr1_fx_mul(uv.y, p->state.tex_matrix[4]) + p->state.tex_matrix[7];
    return out;
}

static vr1_vec3 vr1_face_normal_from_points(vr1_vec3 a, vr1_vec3 b, vr1_vec3 c)
{
    vr1_vec3 u;
    vr1_vec3 v;
    u = vr1_vec3_sub(b, a);
    v = vr1_vec3_sub(c, a);
    return vr1_vec3_normalize(vr1_vec3_cross(u, v));
}

static vr1_vec2 vr1_default_uv_from_bbox(vr1_parser_context *p, vr1_vec3 v, int axis_s, int axis_t, vr1_vec3 minp, vr1_vec3 maxp)
{
    vr1_fx sv;
    vr1_fx tv;
    vr1_fx smin;
    vr1_fx tmin;
    vr1_fx ssize;
    vr1_fx tsize;
    vr1_vec2 uv;

    if (axis_s == 0) { sv = v.x; smin = minp.x; ssize = maxp.x - minp.x; }
    else if (axis_s == 1) { sv = v.y; smin = minp.y; ssize = maxp.y - minp.y; }
    else { sv = v.z; smin = minp.z; ssize = maxp.z - minp.z; }
    if (axis_t == 0) { tv = v.x; tmin = minp.x; tsize = maxp.x - minp.x; }
    else if (axis_t == 1) { tv = v.y; tmin = minp.y; tsize = maxp.y - minp.y; }
    else { tv = v.z; tmin = minp.z; tsize = maxp.z - minp.z; }
    if (ssize == 0) ssize = VR1_FX_ONE;
    if (tsize == 0) tsize = VR1_FX_ONE;
    (void)p;
    uv.x = vr1_fx_div(sv - smin, ssize);
    uv.y = vr1_fx_div(tv - tmin, tsize);
    return uv;
}

static void vr1_choose_bbox_axes(vr1_vec3 minp, vr1_vec3 maxp, int *axis_s, int *axis_t)
{
    vr1_fx dx;
    vr1_fx dy;
    vr1_fx dz;

    dx = vr1_fx_abs(maxp.x - minp.x);
    dy = vr1_fx_abs(maxp.y - minp.y);
    dz = vr1_fx_abs(maxp.z - minp.z);
    if (dx >= dy && dx >= dz) {
        *axis_s = 0;
        *axis_t = (dy >= dz) ? 1 : 2;
    } else if (dy >= dx && dy >= dz) {
        *axis_s = 1;
        *axis_t = (dx >= dz) ? 0 : 2;
    } else {
        *axis_s = 2;
        *axis_t = (dx >= dy) ? 0 : 1;
    }
}

static void vr1_emit_triangle_vertices_mats(vr1_parser_context *p, vr1_vec3 pa, vr1_vec3 pb, vr1_vec3 pc, vr1_vec3 na, vr1_vec3 nb, vr1_vec3 nc, vr1_vec2 uva, vr1_vec2 uvb, vr1_vec2 uvc, int material_a, int material_b, int material_c, int triangle_material)
{
    vr1_vertex va;
    vr1_vertex vb;
    vr1_vertex vc;
    long ia;
    long ib;
    long ic;

    if (!p->emit_enabled) return;
    uva = vr1_apply_texture_transform(p, uva);
    uvb = vr1_apply_texture_transform(p, uvb);
    uvc = vr1_apply_texture_transform(p, uvc);
    va.pos = pa;
    va.normal = na;
    va.uv = uva;
    va.material = material_a;
    vb.pos = pb;
    vb.normal = nb;
    vb.uv = uvb;
    vb.material = material_b;
    vc.pos = pc;
    vc.normal = nc;
    vc.uv = uvc;
    vc.material = material_c;
    ia = vr1_scene_add_vertex(p->scene, &va);
    ib = vr1_scene_add_vertex(p->scene, &vb);
    ic = vr1_scene_add_vertex(p->scene, &vc);
    if (ia >= 0 && ib >= 0 && ic >= 0) {
        (void)vr1_scene_add_triangle(p->scene, ia, ib, ic, triangle_material);
    }
}

static void vr1_emit_triangle_vertices(vr1_parser_context *p, vr1_vec3 pa, vr1_vec3 pb, vr1_vec3 pc, vr1_vec3 na, vr1_vec3 nb, vr1_vec3 nc, vr1_vec2 uva, vr1_vec2 uvb, vr1_vec2 uvc, int material)
{
    vr1_emit_triangle_vertices_mats(p, pa, pb, pc, na, nb, nc, uva, uvb, uvc, material, material, material, material);
}

typedef struct vr1_face_vert_s {
    int coord;
    int normal;
    int tex;
    long mat_index;
    long vertex_ordinal;
} vr1_face_vert;

static int vr1_index_valid(int idx, int count)
{
    return idx >= 0 && idx < count;
}

static vr1_vec3 vr1_get_face_vertex_pos(vr1_parser_context *p, int idx)
{
    vr1_vec3 z;
    z = vr1_vec3_make(0, 0, 0);
    if (!vr1_index_valid(idx, p->state.coord_count)) return z;
    return vr1_transform_point(p->state.matrix, p->state.coords[idx]);
}

static vr1_vec3 vr1_get_face_vertex_normal(vr1_parser_context *p, int idx, int coord_idx, vr1_vec3 fallback)
{
    if (vr1_index_valid(idx, p->state.normal_count)) {
        return vr1_transform_vector(p->state.matrix, p->state.normals[idx]);
    }
    if (p->gen_normals_ready && vr1_index_valid(coord_idx, p->state.coord_count) && p->gen_normal_valid[coord_idx]) {
        return p->gen_normals[coord_idx];
    }
    return fallback;
}

static vr1_vec2 vr1_get_face_vertex_uv(vr1_parser_context *p, int tex_idx, int coord_idx, vr1_vec3 local, int axis_s, int axis_t, vr1_vec3 minp, vr1_vec3 maxp)
{
    if (vr1_index_valid(tex_idx, p->state.texcoord_count)) return p->state.texcoords[tex_idx];
    if (tex_idx < 0 && vr1_index_valid(coord_idx, p->state.texcoord_count)) return p->state.texcoords[coord_idx];
    return vr1_default_uv_from_bbox(p, local, axis_s, axis_t, minp, maxp);
}

static long vr1_material_index_for_face(vr1_parser_context *p, long face_no, const vr1_face_vert *fv, int fv_count)
{
    long explicit_index;
    int bind;

    bind = p->state.material_binding;
    explicit_index = -1;
    if (bind == VR1_BIND_PER_FACE_INDEXED || bind == VR1_BIND_PER_PART_INDEXED) {
        if (face_no >= 0 && face_no < p->material_index_count) explicit_index = p->material_index[face_no];
    } else if (bind == VR1_BIND_PER_VERTEX_INDEXED && fv_count > 0) {
        explicit_index = fv[0].mat_index;
    }
    return (long)vr1_current_material_index(p, face_no, (fv_count > 0) ? fv[0].vertex_ordinal : 0, explicit_index);
}

static int vr1_normal_index_for_vertex(vr1_parser_context *p, long face_no, long vertex_no, long parallel_index, int coord_idx)
{
    int bind;
    long n;

    bind = p->state.normal_binding;
    if (bind == VR1_BIND_DEFAULT) bind = VR1_BIND_PER_VERTEX_INDEXED;
    n = -1;
    if (bind == VR1_BIND_OVERALL) n = 0;
    else if (bind == VR1_BIND_PER_PART || bind == VR1_BIND_PER_FACE) n = face_no;
    else if (bind == VR1_BIND_PER_PART_INDEXED || bind == VR1_BIND_PER_FACE_INDEXED) {
        if (face_no >= 0 && face_no < p->normal_index_count) n = p->normal_index[face_no];
    } else if (bind == VR1_BIND_PER_VERTEX) n = vertex_no;
    else if (bind == VR1_BIND_PER_VERTEX_INDEXED) {
        if (parallel_index >= 0 && parallel_index < p->normal_index_count) n = p->normal_index[parallel_index];
        else n = coord_idx;
    }
    return (int)n;
}

static int vr1_tex_index_for_vertex(vr1_parser_context *p, long face_no, long vertex_no, long parallel_index, int coord_idx)
{
    int bind;
    long n;

    bind = p->state.texcoord_binding;
    if (bind == VR1_BIND_DEFAULT) bind = VR1_BIND_PER_VERTEX_INDEXED;
    n = -1;
    if (bind == VR1_BIND_OVERALL) n = 0;
    else if (bind == VR1_BIND_PER_PART || bind == VR1_BIND_PER_FACE) n = face_no;
    else if (bind == VR1_BIND_PER_PART_INDEXED || bind == VR1_BIND_PER_FACE_INDEXED) {
        if (face_no >= 0 && face_no < p->texcoord_index_count) n = p->texcoord_index[face_no];
    } else if (bind == VR1_BIND_PER_VERTEX) {
        n = vertex_no;
    } else if (bind == VR1_BIND_PER_VERTEX_INDEXED) {
        if (parallel_index >= 0 && parallel_index < p->texcoord_index_count) n = p->texcoord_index[parallel_index];
        else n = coord_idx;
    }
    return (int)n;
}

static void vr1_emit_ifs_tri_from_face(vr1_parser_context *p, const vr1_face_vert *fv, int iaf, int ibf, int icf, long face_no, vr1_vec3 bbox_min, vr1_vec3 bbox_max, int axis_s, int axis_t, int mat)
{
    vr1_vec3 p0;
    vr1_vec3 p1;
    vr1_vec3 p2;
    vr1_vec3 fn;
    vr1_vec3 n0;
    vr1_vec3 n1;
    vr1_vec3 n2;
    vr1_vec2 uv0;
    vr1_vec2 uv1;
    vr1_vec2 uv2;
    int ni0;
    int ni1;
    int ni2;
    int ti0;
    int ti1;
    int ti2;

    p0 = vr1_get_face_vertex_pos(p, fv[iaf].coord);
    p1 = vr1_get_face_vertex_pos(p, fv[ibf].coord);
    p2 = vr1_get_face_vertex_pos(p, fv[icf].coord);
    fn = vr1_face_normal_from_points(p0, p1, p2);
    if (p->state.vertex_ordering == VR1_ORDER_CLOCKWISE) {
        fn.x = -fn.x;
        fn.y = -fn.y;
        fn.z = -fn.z;
    }
    ni0 = vr1_normal_index_for_vertex(p, face_no, fv[iaf].vertex_ordinal, fv[iaf].vertex_ordinal, fv[iaf].coord);
    ni1 = vr1_normal_index_for_vertex(p, face_no, fv[ibf].vertex_ordinal, fv[ibf].vertex_ordinal, fv[ibf].coord);
    ni2 = vr1_normal_index_for_vertex(p, face_no, fv[icf].vertex_ordinal, fv[icf].vertex_ordinal, fv[icf].coord);
    n0 = vr1_get_face_vertex_normal(p, ni0, fv[iaf].coord, fn);
    n1 = vr1_get_face_vertex_normal(p, ni1, fv[ibf].coord, fn);
    n2 = vr1_get_face_vertex_normal(p, ni2, fv[icf].coord, fn);
    ti0 = vr1_tex_index_for_vertex(p, face_no, fv[iaf].vertex_ordinal, fv[iaf].vertex_ordinal, fv[iaf].coord);
    ti1 = vr1_tex_index_for_vertex(p, face_no, fv[ibf].vertex_ordinal, fv[ibf].vertex_ordinal, fv[ibf].coord);
    ti2 = vr1_tex_index_for_vertex(p, face_no, fv[icf].vertex_ordinal, fv[icf].vertex_ordinal, fv[icf].coord);
    uv0 = vr1_get_face_vertex_uv(p, ti0, fv[iaf].coord, p->state.coords[fv[iaf].coord], axis_s, axis_t, bbox_min, bbox_max);
    uv1 = vr1_get_face_vertex_uv(p, ti1, fv[ibf].coord, p->state.coords[fv[ibf].coord], axis_s, axis_t, bbox_min, bbox_max);
    uv2 = vr1_get_face_vertex_uv(p, ti2, fv[icf].coord, p->state.coords[fv[icf].coord], axis_s, axis_t, bbox_min, bbox_max);
    if (p->state.material_binding == VR1_BIND_PER_VERTEX || p->state.material_binding == VR1_BIND_PER_VERTEX_INDEXED) {
        int m0;
        int m1;
        int m2;
        m0 = vr1_current_material_index(p, face_no, fv[iaf].vertex_ordinal, fv[iaf].mat_index);
        m1 = vr1_current_material_index(p, face_no, fv[ibf].vertex_ordinal, fv[ibf].mat_index);
        m2 = vr1_current_material_index(p, face_no, fv[icf].vertex_ordinal, fv[icf].mat_index);
        vr1_emit_triangle_vertices_mats(p, p0, p1, p2, n0, n1, n2, uv0, uv1, uv2, m0, m1, m2, mat);
    } else {
        vr1_emit_triangle_vertices(p, p0, p1, p2, n0, n1, n2, uv0, uv1, uv2, mat);
    }
}

static vr1_vec2 vr1_project_to_face_2d(vr1_vec3 n, vr1_vec3 pnt)
{
    vr1_fx ax;
    vr1_fx ay;
    vr1_fx az;
    vr1_vec2 out;

    ax = vr1_fx_abs(n.x);
    ay = vr1_fx_abs(n.y);
    az = vr1_fx_abs(n.z);
    if (ax >= ay && ax >= az) {
        out.x = pnt.y;
        out.y = pnt.z;
    } else if (ay >= ax && ay >= az) {
        out.x = pnt.x;
        out.y = pnt.z;
    } else {
        out.x = pnt.x;
        out.y = pnt.y;
    }
    return out;
}

static vr1_fx vr1_cross2(vr1_vec2 a, vr1_vec2 b, vr1_vec2 c)
{
    return vr1_fx_mul(b.x - a.x, c.y - a.y) - vr1_fx_mul(b.y - a.y, c.x - a.x);
}

static int vr1_point_in_tri2(vr1_vec2 pnt, vr1_vec2 a, vr1_vec2 b, vr1_vec2 c, int ccw)
{
    vr1_fx c0;
    vr1_fx c1;
    vr1_fx c2;

    c0 = vr1_cross2(a, b, pnt);
    c1 = vr1_cross2(b, c, pnt);
    c2 = vr1_cross2(c, a, pnt);
    if (ccw) return c0 >= 0 && c1 >= 0 && c2 >= 0;
    return c0 <= 0 && c1 <= 0 && c2 <= 0;
}

static int vr1_try_emit_ifs_face_ear(vr1_parser_context *p, const vr1_face_vert *fv, int fv_count, long face_no, vr1_vec3 bbox_min, vr1_vec3 bbox_max, int axis_s, int axis_t, int mat)
{
    int active[VR1_MAX_FACE_VERTS];
    vr1_vec2 proj[VR1_MAX_FACE_VERTS];
    int remain;
    int i;
    int guard;
    vr1_fx signed_area;
    int ccw;
    vr1_vec3 p0;
    vr1_vec3 p1;
    vr1_vec3 p2;
    vr1_vec3 normal;

    if (fv_count < 4 || fv_count > VR1_MAX_FACE_VERTS) return 0;
    p0 = vr1_get_face_vertex_pos(p, fv[0].coord);
    p1 = vr1_get_face_vertex_pos(p, fv[1].coord);
    p2 = vr1_get_face_vertex_pos(p, fv[2].coord);
    normal = vr1_face_normal_from_points(p0, p1, p2);
    if (normal.x == 0 && normal.y == 0 && normal.z == 0) return 0;
    for (i = 0; i < fv_count; i++) {
        active[i] = i;
        proj[i] = vr1_project_to_face_2d(normal, vr1_get_face_vertex_pos(p, fv[i].coord));
    }
    signed_area = 0;
    for (i = 0; i < fv_count; i++) {
        int j;
        j = (i + 1) % fv_count;
        signed_area += vr1_fx_mul(proj[i].x, proj[j].y) - vr1_fx_mul(proj[j].x, proj[i].y);
    }
    if (signed_area == 0) return 0;
    ccw = signed_area > 0;
    remain = fv_count;
    guard = 0;
    while (remain > 3 && guard < VR1_MAX_FACE_VERTS * VR1_MAX_FACE_VERTS) {
        int found;
        found = 0;
        for (i = 0; i < remain; i++) {
            int ip;
            int ic;
            int in;
            int j;
            int inside;
            vr1_fx turn;

            ip = active[(i + remain - 1) % remain];
            ic = active[i];
            in = active[(i + 1) % remain];
            turn = vr1_cross2(proj[ip], proj[ic], proj[in]);
            if ((ccw && turn <= 0) || (!ccw && turn >= 0)) continue;
            inside = 0;
            for (j = 0; j < remain; j++) {
                int q;
                q = active[j];
                if (q == ip || q == ic || q == in) continue;
                if (vr1_point_in_tri2(proj[q], proj[ip], proj[ic], proj[in], ccw)) {
                    inside = 1;
                    break;
                }
            }
            if (!inside) {
                vr1_emit_ifs_tri_from_face(p, fv, ip, ic, in, face_no, bbox_min, bbox_max, axis_s, axis_t, mat);
                for (j = i; j + 1 < remain; j++) active[j] = active[j + 1];
                remain--;
                found = 1;
                break;
            }
        }
        if (!found) return 0;
        guard++;
    }
    if (remain == 3) {
        vr1_emit_ifs_tri_from_face(p, fv, active[0], active[1], active[2], face_no, bbox_min, bbox_max, axis_s, axis_t, mat);
        return 1;
    }
    return 0;
}

static void vr1_emit_ifs_face(vr1_parser_context *p, const vr1_face_vert *fv, int fv_count, long face_no, vr1_vec3 bbox_min, vr1_vec3 bbox_max, int axis_s, int axis_t)
{
    int i;
    int mat;

    if (fv_count < 3) return;
    mat = (int)vr1_material_index_for_face(p, face_no, fv, fv_count);
    if (fv_count > 3 && p->state.face_type != VR1_FACE_CONVEX) {
        if (vr1_try_emit_ifs_face_ear(p, fv, fv_count, face_no, bbox_min, bbox_max, axis_s, axis_t, mat)) return;
        vr1_scene_warn(p->scene);
    }
    for (i = 1; i + 1 < fv_count; i++) {
        vr1_emit_ifs_tri_from_face(p, fv, 0, i, i + 1, face_no, bbox_min, bbox_max, axis_s, axis_t, mat);
    }
}

static void vr1_clear_generated_normals(vr1_parser_context *p)
{
    int i;

    p->gen_normals_ready = 0;
    for (i = 0; i < VR1_MAX_STATE_POINTS; i++) {
        p->gen_normals[i] = vr1_vec3_make(0, 0, 0);
        p->gen_normal_valid[i] = 0;
    }
}

static void vr1_prepare_generated_normals(vr1_parser_context *p)
{
    long i;
    int j;
    int face_count;
    int face[VR1_MAX_FACE_VERTS];

    vr1_clear_generated_normals(p);
    if (p->state.normal_count > 0) return;
    if (p->state.coord_count <= 0) return;
    if (p->state.crease_angle <= 0) return;

    face_count = 0;
    for (i = 0; i <= p->coord_index_count; i++) {
        long ci;
        ci = (i < p->coord_index_count) ? p->coord_index[i] : -1;
        if (ci == -1) {
            if (face_count >= 3) {
                vr1_vec3 p0;
                vr1_vec3 p1;
                vr1_vec3 p2;
                vr1_vec3 fn;

                p0 = vr1_get_face_vertex_pos(p, face[0]);
                p1 = vr1_get_face_vertex_pos(p, face[1]);
                p2 = vr1_get_face_vertex_pos(p, face[2]);
                fn = vr1_face_normal_from_points(p0, p1, p2);
                if (p->state.vertex_ordering == VR1_ORDER_CLOCKWISE) {
                    fn.x = -fn.x;
                    fn.y = -fn.y;
                    fn.z = -fn.z;
                }
                for (j = 0; j < face_count; j++) {
                    int idx;
                    idx = face[j];
                    if (vr1_index_valid(idx, p->state.coord_count)) {
                        p->gen_normals[idx].x += fn.x;
                        p->gen_normals[idx].y += fn.y;
                        p->gen_normals[idx].z += fn.z;
                        p->gen_normal_valid[idx] = 1;
                    }
                }
            }
            face_count = 0;
        } else if (vr1_index_valid((int)ci, p->state.coord_count)) {
            if (face_count < VR1_MAX_FACE_VERTS) {
                face[face_count] = (int)ci;
                face_count++;
            }
        }
    }

    for (j = 0; j < p->state.coord_count; j++) {
        if (p->gen_normal_valid[j]) p->gen_normals[j] = vr1_vec3_normalize(p->gen_normals[j]);
    }
    p->gen_normals_ready = 1;
}

static void vr1_compute_index_bbox(vr1_parser_context *p, vr1_vec3 *minp, vr1_vec3 *maxp)
{
    long i;
    int idx;
    int have;
    vr1_vec3 v;

    have = 0;
    minp->x = minp->y = minp->z = 0;
    maxp->x = maxp->y = maxp->z = 0;
    for (i = 0; i < p->coord_index_count; i++) {
        idx = (int)p->coord_index[i];
        if (!vr1_index_valid(idx, p->state.coord_count)) continue;
        v = p->state.coords[idx];
        if (!have) {
            *minp = v;
            *maxp = v;
            have = 1;
        } else {
            if (v.x < minp->x) minp->x = v.x;
            if (v.y < minp->y) minp->y = v.y;
            if (v.z < minp->z) minp->z = v.z;
            if (v.x > maxp->x) maxp->x = v.x;
            if (v.y > maxp->y) maxp->y = v.y;
            if (v.z > maxp->z) maxp->z = v.z;
        }
    }
    if (!have && p->state.coord_count > 0) {
        *minp = p->state.coords[0];
        *maxp = p->state.coords[0];
    }
}

static void vr1_emit_indexed_faces(vr1_parser_context *p)
{
    long i;
    long face_no;
    int face_count;
    vr1_face_vert face[VR1_MAX_FACE_VERTS];
    vr1_vec3 minp;
    vr1_vec3 maxp;
    int axis_s;
    int axis_t;

    vr1_compute_index_bbox(p, &minp, &maxp);
    vr1_choose_bbox_axes(minp, maxp, &axis_s, &axis_t);
    vr1_prepare_generated_normals(p);
    face_no = 0;
    face_count = 0;
    for (i = 0; i <= p->coord_index_count; i++) {
        long ci;
        ci = (i < p->coord_index_count) ? p->coord_index[i] : -1;
        if (ci == -1) {
            if (face_count >= 3) {
                vr1_emit_ifs_face(p, face, face_count, face_no, minp, maxp, axis_s, axis_t);
                face_no++;
            }
            face_count = 0;
        } else {
            if (vr1_index_valid((int)ci, p->state.coord_count)) {
                if (face_count < VR1_MAX_FACE_VERTS) {
                    face[face_count].coord = (int)ci;
                    face[face_count].normal = -1;
                    face[face_count].tex = -1;
                    face[face_count].mat_index = (i < p->material_index_count) ? p->material_index[i] : -1;
                    face[face_count].vertex_ordinal = i;
                    face_count++;
                } else {
                    vr1_scene_warn(p->scene);
                }
            } else {
                vr1_scene_warn(p->scene);
            }
        }
    }
}

static void vr1_emit_indexed_lines(vr1_parser_context *p)
{
    long i;
    int last;
    int mat;
    int bind;
    long segment_no;
    long polyline_no;
    vr1_vertex va;
    vr1_vertex vb;
    long ia;
    long ib;

    last = -1;
    segment_no = 0;
    polyline_no = 0;
    bind = p->state.material_binding;
    for (i = 0; i < p->coord_index_count; i++) {
        int idx;
        idx = (int)p->coord_index[i];
        if (idx == -1) {
            last = -1;
            polyline_no++;
        } else if (vr1_index_valid(idx, p->state.coord_count)) {
            if (last >= 0) {
                long face_key;
                long explicit_index;

                face_key = polyline_no;
                if (bind == VR1_BIND_PER_PART || bind == VR1_BIND_PER_PART_INDEXED) face_key = segment_no;
                explicit_index = -1;
                if (bind == VR1_BIND_PER_PART_INDEXED && segment_no < p->material_index_count) explicit_index = p->material_index[segment_no];
                else if (bind == VR1_BIND_PER_FACE_INDEXED && polyline_no < p->material_index_count) explicit_index = p->material_index[polyline_no];
                else if (bind == VR1_BIND_PER_VERTEX_INDEXED && i < p->material_index_count) explicit_index = p->material_index[i];
                mat = vr1_current_material_index(p, face_key, i, explicit_index);
                memset(&va, 0, sizeof(va));
                memset(&vb, 0, sizeof(vb));
                va.pos = vr1_get_face_vertex_pos(p, last);
                vb.pos = vr1_get_face_vertex_pos(p, idx);
                va.normal = vr1_vec3_make(0, 0, VR1_FX_ONE);
                vb.normal = va.normal;
                va.material = mat;
                vb.material = mat;
                ia = vr1_scene_add_vertex(p->scene, &va);
                ib = vr1_scene_add_vertex(p->scene, &vb);
                if (ia >= 0 && ib >= 0) (void)vr1_scene_add_line(p->scene, ia, ib, mat);
                segment_no++;
            }
            last = idx;
        }
    }
}

static void vr1_emit_pointset(vr1_parser_context *p, long start_index, long num_points)
{
    long i;
    long end;
    vr1_point pt;
    int mat;

    if (start_index < 0) start_index = 0;
    if (start_index >= p->state.coord_count) return;
    if (num_points < 0) end = p->state.coord_count;
    else end = start_index + num_points;
    if (end > p->state.coord_count) end = p->state.coord_count;
    for (i = start_index; i < end; i++) {
        mat = vr1_current_material_index(p, i - start_index, i, -1);
        pt.pos = vr1_transform_point(p->state.matrix, p->state.coords[i]);
        pt.material = mat;
        (void)vr1_scene_add_point(p->scene, &pt);
    }
}

static void vr1_emit_primitive_tri_with_mat(vr1_parser_context *p, vr1_vec3 a, vr1_vec3 b, vr1_vec3 c, vr1_vec2 uva, vr1_vec2 uvb, vr1_vec2 uvc, int material)
{
    vr1_vec3 ta;
    vr1_vec3 tb;
    vr1_vec3 tc;
    vr1_vec3 n;

    ta = vr1_transform_point(p->state.matrix, a);
    tb = vr1_transform_point(p->state.matrix, b);
    tc = vr1_transform_point(p->state.matrix, c);
    n = vr1_face_normal_from_points(ta, tb, tc);
    vr1_emit_triangle_vertices(p, ta, tb, tc, n, n, n, uva, uvb, uvc, material);
}

static void vr1_emit_primitive_tri(vr1_parser_context *p, vr1_vec3 a, vr1_vec3 b, vr1_vec3 c, vr1_vec2 uva, vr1_vec2 uvb, vr1_vec2 uvc)
{
    int mat;

    mat = vr1_current_material_index(p, 0, 0, -1);
    vr1_emit_primitive_tri_with_mat(p, a, b, c, uva, uvb, uvc, mat);
}

static void vr1_emit_cube(vr1_parser_context *p, vr1_fx width, vr1_fx height, vr1_fx depth)
{
    vr1_fx hx;
    vr1_fx hy;
    vr1_fx hz;
    vr1_vec3 v[8];
    vr1_vec2 uv00;
    vr1_vec2 uv10;
    vr1_vec2 uv11;
    vr1_vec2 uv01;
    int m_front;
    int m_back;
    int m_left;
    int m_right;
    int m_top;
    int m_bottom;

    hx = width / 2;
    hy = height / 2;
    hz = depth / 2;
    v[0] = vr1_vec3_make(-hx, -hy, -hz);
    v[1] = vr1_vec3_make( hx, -hy, -hz);
    v[2] = vr1_vec3_make( hx,  hy, -hz);
    v[3] = vr1_vec3_make(-hx,  hy, -hz);
    v[4] = vr1_vec3_make(-hx, -hy,  hz);
    v[5] = vr1_vec3_make( hx, -hy,  hz);
    v[6] = vr1_vec3_make( hx,  hy,  hz);
    v[7] = vr1_vec3_make(-hx,  hy,  hz);
    uv00 = vr1_vec2_make(0, 0);
    uv10 = vr1_vec2_make(VR1_FX_ONE, 0);
    uv11 = vr1_vec2_make(VR1_FX_ONE, VR1_FX_ONE);
    uv01 = vr1_vec2_make(0, VR1_FX_ONE);

    m_front = vr1_current_part_material_index(p, 0);
    m_back = vr1_current_part_material_index(p, 1);
    m_left = vr1_current_part_material_index(p, 2);
    m_right = vr1_current_part_material_index(p, 3);
    m_top = vr1_current_part_material_index(p, 4);
    m_bottom = vr1_current_part_material_index(p, 5);

    vr1_emit_primitive_tri_with_mat(p, v[4], v[5], v[6], uv00, uv10, uv11, m_front);
    vr1_emit_primitive_tri_with_mat(p, v[4], v[6], v[7], uv00, uv11, uv01, m_front);
    vr1_emit_primitive_tri_with_mat(p, v[1], v[0], v[3], uv00, uv10, uv11, m_back);
    vr1_emit_primitive_tri_with_mat(p, v[1], v[3], v[2], uv00, uv11, uv01, m_back);
    vr1_emit_primitive_tri_with_mat(p, v[0], v[4], v[7], uv00, uv10, uv11, m_left);
    vr1_emit_primitive_tri_with_mat(p, v[0], v[7], v[3], uv00, uv11, uv01, m_left);
    vr1_emit_primitive_tri_with_mat(p, v[5], v[1], v[2], uv00, uv10, uv11, m_right);
    vr1_emit_primitive_tri_with_mat(p, v[5], v[2], v[6], uv00, uv11, uv01, m_right);
    vr1_emit_primitive_tri_with_mat(p, v[3], v[7], v[6], uv00, uv10, uv11, m_top);
    vr1_emit_primitive_tri_with_mat(p, v[3], v[6], v[2], uv00, uv11, uv01, m_top);
    vr1_emit_primitive_tri_with_mat(p, v[0], v[1], v[5], uv00, uv10, uv11, m_bottom);
    vr1_emit_primitive_tri_with_mat(p, v[0], v[5], v[4], uv00, uv11, uv01, m_bottom);
}

static void vr1_circle_point_xz(vr1_fx radius, int deg, vr1_fx y, vr1_vec3 *out)
{
    vr1_fx ang;
    ang = (vr1_fx)((long)deg * VR1_FX_PI / 180L);
    out->x = vr1_fx_mul(radius, vr1_fx_sin(ang));
    out->y = y;
    out->z = vr1_fx_mul(radius, vr1_fx_cos(ang));
}

static void vr1_emit_cylinder(vr1_parser_context *p, int parts, vr1_fx radius, vr1_fx height)
{
    int i;
    int seg;
    int d0;
    int d1;
    vr1_fx hy;
    vr1_vec3 top0;
    vr1_vec3 top1;
    vr1_vec3 bot0;
    vr1_vec3 bot1;
    vr1_vec3 ct;
    vr1_vec3 cb;
    vr1_vec2 uv0;
    vr1_vec2 uv1;
    vr1_vec2 uv2;
    vr1_vec2 uv3;
    int mat_sides;
    int mat_top;
    int mat_bottom;

    mat_sides = vr1_current_part_material_index(p, 0);
    mat_top = vr1_current_part_material_index(p, 1);
    mat_bottom = vr1_current_part_material_index(p, 2);
    seg = VR1_CYLINDER_SEGMENTS;
    hy = height / 2;
    ct = vr1_vec3_make(0, hy, 0);
    cb = vr1_vec3_make(0, -hy, 0);
    for (i = 0; i < seg; i++) {
        d0 = (360 * i) / seg;
        d1 = (360 * (i + 1)) / seg;
        vr1_circle_point_xz(radius, d0, hy, &top0);
        vr1_circle_point_xz(radius, d1, hy, &top1);
        vr1_circle_point_xz(radius, d0, -hy, &bot0);
        vr1_circle_point_xz(radius, d1, -hy, &bot1);
        uv0 = vr1_vec2_make((VR1_FX_ONE * i) / seg, 0);
        uv1 = vr1_vec2_make((VR1_FX_ONE * (i + 1)) / seg, 0);
        uv2 = vr1_vec2_make((VR1_FX_ONE * (i + 1)) / seg, VR1_FX_ONE);
        uv3 = vr1_vec2_make((VR1_FX_ONE * i) / seg, VR1_FX_ONE);
        if (parts & VR1_PART_SIDES) {
            vr1_emit_primitive_tri_with_mat(p, bot0, bot1, top1, uv0, uv1, uv2, mat_sides);
            vr1_emit_primitive_tri_with_mat(p, bot0, top1, top0, uv0, uv2, uv3, mat_sides);
        }
        if (parts & VR1_PART_TOP) {
            vr1_emit_primitive_tri_with_mat(p, ct, top0, top1, vr1_vec2_make(VR1_FX_HALF, VR1_FX_HALF), uv0, uv1, mat_top);
        }
        if (parts & VR1_PART_BOTTOM) {
            vr1_emit_primitive_tri_with_mat(p, cb, bot1, bot0, vr1_vec2_make(VR1_FX_HALF, VR1_FX_HALF), uv1, uv0, mat_bottom);
        }
    }
}

static void vr1_emit_cone(vr1_parser_context *p, int parts, vr1_fx radius, vr1_fx height)
{
    int i;
    int seg;
    int d0;
    int d1;
    vr1_fx hy;
    vr1_vec3 apex;
    vr1_vec3 bot0;
    vr1_vec3 bot1;
    vr1_vec3 cb;
    vr1_vec2 uv0;
    vr1_vec2 uv1;
    int mat_sides;
    int mat_bottom;

    mat_sides = vr1_current_part_material_index(p, 0);
    mat_bottom = vr1_current_part_material_index(p, 1);
    seg = VR1_CYLINDER_SEGMENTS;
    hy = height / 2;
    apex = vr1_vec3_make(0, hy, 0);
    cb = vr1_vec3_make(0, -hy, 0);
    for (i = 0; i < seg; i++) {
        d0 = (360 * i) / seg;
        d1 = (360 * (i + 1)) / seg;
        vr1_circle_point_xz(radius, d0, -hy, &bot0);
        vr1_circle_point_xz(radius, d1, -hy, &bot1);
        uv0 = vr1_vec2_make((VR1_FX_ONE * i) / seg, 0);
        uv1 = vr1_vec2_make((VR1_FX_ONE * (i + 1)) / seg, 0);
        if (parts & VR1_PART_SIDES) {
            vr1_emit_primitive_tri_with_mat(p, bot0, bot1, apex, uv0, uv1, vr1_vec2_make((uv0.x + uv1.x) / 2, VR1_FX_ONE), mat_sides);
        }
        if (parts & VR1_PART_BOTTOM) {
            vr1_emit_primitive_tri_with_mat(p, cb, bot1, bot0, vr1_vec2_make(VR1_FX_HALF, VR1_FX_HALF), uv1, uv0, mat_bottom);
        }
    }
}

static void vr1_emit_sphere(vr1_parser_context *p, vr1_fx radius)
{
    int stack;
    int slice;
    int stacks;
    int slices;
    int lat0;
    int lat1;
    int lon0;
    int lon1;
    vr1_vec3 p00;
    vr1_vec3 p01;
    vr1_vec3 p10;
    vr1_vec3 p11;
    vr1_vec2 uv00;
    vr1_vec2 uv01;
    vr1_vec2 uv10;
    vr1_vec2 uv11;

    stacks = VR1_SPHERE_STACKS;
    slices = VR1_SPHERE_SLICES;
    for (stack = 0; stack < stacks; stack++) {
        lat0 = -90 + (180 * stack) / stacks;
        lat1 = -90 + (180 * (stack + 1)) / stacks;
        for (slice = 0; slice < slices; slice++) {
            lon0 = (360 * slice) / slices;
            lon1 = (360 * (slice + 1)) / slices;
            {
                vr1_fx a0;
                vr1_fx a1;
                vr1_fx b0;
                vr1_fx b1;
                vr1_fx r0;
                vr1_fx r1;
                a0 = (vr1_fx)((long)lat0 * VR1_FX_PI / 180L);
                a1 = (vr1_fx)((long)lat1 * VR1_FX_PI / 180L);
                b0 = (vr1_fx)((long)lon0 * VR1_FX_PI / 180L);
                b1 = (vr1_fx)((long)lon1 * VR1_FX_PI / 180L);
                r0 = vr1_fx_mul(radius, vr1_fx_cos(a0));
                r1 = vr1_fx_mul(radius, vr1_fx_cos(a1));
                p00.x = vr1_fx_mul(r0, vr1_fx_sin(b0));
                p00.y = vr1_fx_mul(radius, vr1_fx_sin(a0));
                p00.z = vr1_fx_mul(r0, vr1_fx_cos(b0));
                p01.x = vr1_fx_mul(r0, vr1_fx_sin(b1));
                p01.y = p00.y;
                p01.z = vr1_fx_mul(r0, vr1_fx_cos(b1));
                p10.x = vr1_fx_mul(r1, vr1_fx_sin(b0));
                p10.y = vr1_fx_mul(radius, vr1_fx_sin(a1));
                p10.z = vr1_fx_mul(r1, vr1_fx_cos(b0));
                p11.x = vr1_fx_mul(r1, vr1_fx_sin(b1));
                p11.y = p10.y;
                p11.z = vr1_fx_mul(r1, vr1_fx_cos(b1));
            }
            uv00 = vr1_vec2_make((VR1_FX_ONE * slice) / slices, (VR1_FX_ONE * stack) / stacks);
            uv01 = vr1_vec2_make((VR1_FX_ONE * (slice + 1)) / slices, (VR1_FX_ONE * stack) / stacks);
            uv10 = vr1_vec2_make((VR1_FX_ONE * slice) / slices, (VR1_FX_ONE * (stack + 1)) / stacks);
            uv11 = vr1_vec2_make((VR1_FX_ONE * (slice + 1)) / slices, (VR1_FX_ONE * (stack + 1)) / stacks);
            if (stack != 0) vr1_emit_primitive_tri(p, p00, p01, p11, uv00, uv01, uv11);
            if (stack + 1 != stacks) vr1_emit_primitive_tri(p, p00, p11, p10, uv00, uv11, uv10);
        }
    }
}

static int vr1_ascii_visible_len(const char *s)
{
    int n;

    n = 0;
    if (s == 0) return 0;
    while (s[n] != '\0' && n < VR1_STRING_MAX - 1) n++;
    return n;
}

static vr1_fx vr1_text_natural_width(vr1_parser_context *p, const char *text)
{
    vr1_fx per_char;
    int chars;

    chars = vr1_ascii_visible_len(text);
    if (chars <= 0) return 0;
    per_char = vr1_fx_mul(p->state.font_size, (VR1_FX_ONE * 3L) / 5L);
    return vr1_fx_mul(per_char, vr1_fx_from_int(chars));
}

static void vr1_emit_ascii_text(vr1_parser_context *p, char strings[VR1_MAX_TEXT_STRINGS][VR1_STRING_MAX], int string_count, vr1_fx spacing, int justification, vr1_fx *widths, int width_count)
{
    int i;
    int mat;
    vr1_fx line_step;
    vr1_vec2 uv00;
    vr1_vec2 uv10;
    vr1_vec2 uv11;
    vr1_vec2 uv01;

    if (string_count <= 0) return;
    mat = vr1_current_material_index(p, 0, 0, -1);
    line_step = vr1_fx_mul(p->state.font_size, spacing);
    uv00 = vr1_vec2_make(0, 0);
    uv10 = vr1_vec2_make(VR1_FX_ONE, 0);
    uv11 = vr1_vec2_make(VR1_FX_ONE, VR1_FX_ONE);
    uv01 = vr1_vec2_make(0, VR1_FX_ONE);

    for (i = 0; i < string_count; i++) {
        vr1_fx w;
        vr1_fx h;
        vr1_fx x0;
        vr1_fx x1;
        vr1_fx y0;
        vr1_fx y1;
        vr1_vec3 a;
        vr1_vec3 b;
        vr1_vec3 c;
        vr1_vec3 d;

        w = 0;
        if (width_count > 0) w = widths[i % width_count];
        if (w <= 0) w = vr1_text_natural_width(p, strings[i]);
        if (w <= 0) continue;
        h = p->state.font_size;
        x0 = 0;
        if (justification == VR1_TEXT_CENTER) x0 = -w / 2;
        else if (justification == VR1_TEXT_RIGHT) x0 = -w;
        x1 = x0 + w;
        y0 = -vr1_fx_mul(line_step, vr1_fx_from_int(i));
        y1 = y0 + h;
        a = vr1_vec3_make(x0, y0, 0);
        b = vr1_vec3_make(x1, y0, 0);
        c = vr1_vec3_make(x1, y1, 0);
        d = vr1_vec3_make(x0, y1, 0);
        vr1_emit_primitive_tri_with_mat(p, a, b, c, uv00, uv10, uv11, mat);
        vr1_emit_primitive_tri_with_mat(p, a, c, d, uv00, uv11, uv01, mat);
    }
}

static int vr1_parse_parts_mask(vr1_parser_context *p, int is_cylinder)
{
    int parts;
    int paren;

    parts = 0;
    paren = vr1_accept(p, VR1_T_LPAREN);
    while (1) {
        if (paren && vr1_accept(p, VR1_T_RPAREN)) break;
        if (vr1_accept(p, VR1_T_PIPE) || vr1_accept(p, VR1_T_COMMA)) continue;
        if (vr1_next_is(p, VR1_T_IDENT)) {
            vr1_lexer_next(&p->lx);
            if (vr1_streq(p->lx.tok.text, "ALL")) parts |= VR1_PART_ALL;
            else if (vr1_streq(p->lx.tok.text, "SIDES")) parts |= VR1_PART_SIDES;
            else if (vr1_streq(p->lx.tok.text, "BOTTOM")) parts |= VR1_PART_BOTTOM;
            else if (vr1_streq(p->lx.tok.text, "TOP")) parts |= VR1_PART_TOP;
            else vr1_scene_warn(p->scene);
        } else {
            break;
        }
        if (!paren) break;
    }
    if (parts == 0) parts = is_cylinder ? VR1_PART_ALL : (VR1_PART_SIDES | VR1_PART_BOTTOM);
    return parts;
}

static void vr1_apply_texture_to_state(vr1_parser_context *p, const char *filename, int wrap_s, int wrap_t)
{
    int i;

    for (i = 0; i < p->state.mat_count; i++) {
        vr1_copy_text(p->state.mats[i].texture, VR1_STRING_MAX, filename);
        p->state.mats[i].wrap_s = wrap_s;
        p->state.mats[i].wrap_t = wrap_t;
    }
}

static int vr1_max_int(int a, int b)
{
    return (a > b) ? a : b;
}

static void vr1_parse_material_node(vr1_parser_context *p)
{
    vr1_color ambient[VR1_MAX_STATE_MATERIALS];
    vr1_color diffuse[VR1_MAX_STATE_MATERIALS];
    vr1_color specular[VR1_MAX_STATE_MATERIALS];
    vr1_color emissive[VR1_MAX_STATE_MATERIALS];
    vr1_fx shininess[VR1_MAX_STATE_MATERIALS];
    vr1_fx transparency[VR1_MAX_STATE_MATERIALS];
    int ac;
    int dc;
    int sc;
    int ec;
    int shc;
    int tc;
    int i;
    int total;
    vr1_material def;

    vr1_material_default(&def);
    for (i = 0; i < VR1_MAX_STATE_MATERIALS; i++) {
        ambient[i] = def.ambient;
        diffuse[i] = def.diffuse;
        specular[i] = def.specular;
        emissive[i] = def.emissive;
        shininess[i] = def.shininess;
        transparency[i] = def.transparency;
    }
    ac = dc = sc = ec = shc = tc = 1;

    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_scene_warn(p->scene); continue; }
        if (vr1_streq(p->lx.tok.text, "ambientColor")) ac = vr1_parse_mfcolor(p, ambient, VR1_MAX_STATE_MATERIALS);
        else if (vr1_streq(p->lx.tok.text, "diffuseColor")) dc = vr1_parse_mfcolor(p, diffuse, VR1_MAX_STATE_MATERIALS);
        else if (vr1_streq(p->lx.tok.text, "specularColor")) sc = vr1_parse_mfcolor(p, specular, VR1_MAX_STATE_MATERIALS);
        else if (vr1_streq(p->lx.tok.text, "emissiveColor")) ec = vr1_parse_mfcolor(p, emissive, VR1_MAX_STATE_MATERIALS);
        else if (vr1_streq(p->lx.tok.text, "shininess")) shc = vr1_parse_mffx(p, shininess, VR1_MAX_STATE_MATERIALS);
        else if (vr1_streq(p->lx.tok.text, "transparency")) tc = vr1_parse_mffx(p, transparency, VR1_MAX_STATE_MATERIALS);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);

    total = ac;
    total = vr1_max_int(total, dc);
    total = vr1_max_int(total, sc);
    total = vr1_max_int(total, ec);
    total = vr1_max_int(total, shc);
    total = vr1_max_int(total, tc);
    if (total <= 0) total = 1;
    if (total > VR1_MAX_STATE_MATERIALS) total = VR1_MAX_STATE_MATERIALS;
    p->state.mat_count = total;
    for (i = 0; i < total; i++) {
        p->state.mats[i].ambient = ambient[(ac > 0) ? (i % ac) : 0];
        p->state.mats[i].diffuse = diffuse[(dc > 0) ? (i % dc) : 0];
        p->state.mats[i].specular = specular[(sc > 0) ? (i % sc) : 0];
        p->state.mats[i].emissive = emissive[(ec > 0) ? (i % ec) : 0];
        p->state.mats[i].shininess = shininess[(shc > 0) ? (i % shc) : 0];
        p->state.mats[i].transparency = transparency[(tc > 0) ? (i % tc) : 0];
        p->state.mats[i].texture[0] = '\0';
        p->state.mats[i].wrap_s = VR1_WRAP_REPEAT;
        p->state.mats[i].wrap_t = VR1_WRAP_REPEAT;
    }
}

static void vr1_apply_packed_colors_to_state(vr1_parser_context *p, const vr1_color *colors, const vr1_fx *transparency, int color_count)
{
    int i;

    if (color_count <= 0) return;
    if (color_count > VR1_MAX_STATE_MATERIALS) color_count = VR1_MAX_STATE_MATERIALS;
    p->state.mat_count = color_count;
    for (i = 0; i < color_count; i++) {
        vr1_material m;
        vr1_material_default(&m);
        m.diffuse = colors[i];
        m.ambient.r = colors[i].r / 5;
        m.ambient.g = colors[i].g / 5;
        m.ambient.b = colors[i].b / 5;
        m.transparency = transparency[i];
        p->state.mats[i] = m;
    }
    if (p->state.material_binding == VR1_BIND_DEFAULT && color_count > 1) {
        p->state.material_binding = VR1_BIND_PER_VERTEX_INDEXED;
    }
}

static void vr1_parse_packed_color_node(vr1_parser_context *p)
{
    vr1_color colors[VR1_MAX_STATE_MATERIALS];
    vr1_fx transparency[VR1_MAX_STATE_MATERIALS];
    int color_count;
    int i;
    vr1_material def;

    vr1_material_default(&def);
    for (i = 0; i < VR1_MAX_STATE_MATERIALS; i++) {
        colors[i] = def.diffuse;
        transparency[i] = 0;
    }
    color_count = 0;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "rgba")) color_count = vr1_parse_mfrgba(p, colors, transparency, VR1_MAX_STATE_MATERIALS);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_apply_packed_colors_to_state(p, colors, transparency, color_count);
}

static void vr1_parse_coordinate3_node(vr1_parser_context *p)
{
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "point")) {
            p->state.coord_count = vr1_parse_mfvec3(p, p->state.coords, VR1_MAX_STATE_POINTS);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
}

static void vr1_parse_coordinate4_node(vr1_parser_context *p)
{
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "point")) {
            p->state.coord_count = vr1_parse_mfvec4_as_vec3(p, p->state.coords, VR1_MAX_STATE_POINTS);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
}

static void vr1_parse_normal_node(vr1_parser_context *p)
{
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "vector")) {
            p->state.normal_count = vr1_parse_mfvec3(p, p->state.normals, VR1_MAX_STATE_NORMALS);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
}

static void vr1_parse_texcoord2_node(vr1_parser_context *p)
{
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "point")) {
            p->state.texcoord_count = vr1_parse_mfvec2(p, p->state.texcoords, VR1_MAX_STATE_TEXCOORDS);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
}

static void vr1_parse_material_binding_node(vr1_parser_context *p)
{
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "value")) {
            vr1_lexer_next(&p->lx);
            p->state.material_binding = vr1_parse_binding_enum(p->lx.tok.text);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
}

static void vr1_parse_normal_binding_node(vr1_parser_context *p)
{
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "value")) {
            vr1_lexer_next(&p->lx);
            p->state.normal_binding = vr1_parse_binding_enum(p->lx.tok.text);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
}

static void vr1_parse_texture_coordinate_binding_node(vr1_parser_context *p)
{
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "value")) {
            vr1_lexer_next(&p->lx);
            p->state.texcoord_binding = vr1_parse_binding_enum(p->lx.tok.text);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
}

static void vr1_parse_vertex_property_node(vr1_parser_context *p)
{
    vr1_color colors[VR1_MAX_STATE_MATERIALS];
    vr1_fx transparency[VR1_MAX_STATE_MATERIALS];
    int color_count;
    int i;
    vr1_material def;

    vr1_material_default(&def);
    for (i = 0; i < VR1_MAX_STATE_MATERIALS; i++) {
        colors[i] = def.diffuse;
        transparency[i] = 0;
    }
    color_count = 0;

    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "vertex")) {
            p->state.coord_count = vr1_parse_mfvec3(p, p->state.coords, VR1_MAX_STATE_POINTS);
        } else if (vr1_streq(p->lx.tok.text, "normal")) {
            p->state.normal_count = vr1_parse_mfvec3(p, p->state.normals, VR1_MAX_STATE_NORMALS);
        } else if (vr1_streq(p->lx.tok.text, "texCoord") || vr1_streq(p->lx.tok.text, "textureCoordinate")) {
            p->state.texcoord_count = vr1_parse_mfvec2(p, p->state.texcoords, VR1_MAX_STATE_TEXCOORDS);
        } else if (vr1_streq(p->lx.tok.text, "orderedRGBA")) {
            color_count = vr1_parse_mfrgba(p, colors, transparency, VR1_MAX_STATE_MATERIALS);
        } else if (vr1_streq(p->lx.tok.text, "materialBinding")) {
            vr1_lexer_next(&p->lx);
            p->state.material_binding = vr1_parse_binding_enum(p->lx.tok.text);
        } else if (vr1_streq(p->lx.tok.text, "normalBinding")) {
            vr1_lexer_next(&p->lx);
            p->state.normal_binding = vr1_parse_binding_enum(p->lx.tok.text);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);

    if (color_count > 0) {
        vr1_apply_packed_colors_to_state(p, colors, transparency, color_count);
        if (p->state.material_binding == VR1_BIND_DEFAULT) p->state.material_binding = VR1_BIND_PER_VERTEX_INDEXED;
    }
}

static void vr1_parse_shape_hints_node(vr1_parser_context *p)
{
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "vertexOrdering")) {
            vr1_lexer_next(&p->lx);
            if (vr1_streq(p->lx.tok.text, "CLOCKWISE")) p->state.vertex_ordering = VR1_ORDER_CLOCKWISE;
            else if (vr1_streq(p->lx.tok.text, "COUNTERCLOCKWISE")) p->state.vertex_ordering = VR1_ORDER_COUNTERCLOCKWISE;
            else p->state.vertex_ordering = VR1_ORDER_UNKNOWN;
        } else if (vr1_streq(p->lx.tok.text, "shapeType")) {
            vr1_lexer_next(&p->lx);
            if (vr1_streq(p->lx.tok.text, "SOLID")) p->state.shape_type = VR1_SHAPE_SOLID;
            else p->state.shape_type = VR1_SHAPE_UNKNOWN;
        } else if (vr1_streq(p->lx.tok.text, "faceType")) {
            vr1_lexer_next(&p->lx);
            if (vr1_streq(p->lx.tok.text, "CONVEX")) p->state.face_type = VR1_FACE_CONVEX;
            else p->state.face_type = VR1_FACE_UNKNOWN;
        } else if (vr1_streq(p->lx.tok.text, "creaseAngle")) {
            p->state.crease_angle = vr1_parse_fx_token(p);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
}

static int vr1_parse_font_style_mask(vr1_parser_context *p)
{
    int mask;
    int paren;

    mask = 0;
    paren = vr1_accept(p, VR1_T_LPAREN);
    while (1) {
        if (paren && vr1_accept(p, VR1_T_RPAREN)) break;
        if (vr1_accept(p, VR1_T_PIPE) || vr1_accept(p, VR1_T_COMMA)) continue;
        if (vr1_next_is(p, VR1_T_IDENT)) {
            vr1_lexer_next(&p->lx);
            if (vr1_streq(p->lx.tok.text, "BOLD")) mask |= 1;
            else if (vr1_streq(p->lx.tok.text, "ITALIC")) mask |= 2;
            else if (vr1_streq(p->lx.tok.text, "NONE")) mask = 0;
            else vr1_scene_warn(p->scene);
        } else {
            break;
        }
        if (!paren) break;
    }
    return mask;
}

static void vr1_parse_font_style_node(vr1_parser_context *p)
{
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "size")) {
            p->state.font_size = vr1_parse_fx_token(p);
            if (p->state.font_size <= 0) p->state.font_size = vr1_fx_from_int(10);
        } else if (vr1_streq(p->lx.tok.text, "family")) {
            vr1_lexer_next(&p->lx);
            if (vr1_streq(p->lx.tok.text, "SANS")) p->state.font_family = 1;
            else if (vr1_streq(p->lx.tok.text, "TYPEWRITER")) p->state.font_family = 2;
            else p->state.font_family = 0;
        } else if (vr1_streq(p->lx.tok.text, "style")) {
            p->state.font_style = vr1_parse_font_style_mask(p);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
}

static void vr1_parse_ascii_text_node(vr1_parser_context *p)
{
    char strings[VR1_MAX_TEXT_STRINGS][VR1_STRING_MAX];
    vr1_fx widths[VR1_MAX_TEXT_STRINGS];
    int string_count;
    int width_count;
    int i;
    vr1_fx spacing;
    int justification;

    for (i = 0; i < VR1_MAX_TEXT_STRINGS; i++) {
        strings[i][0] = '\0';
        widths[i] = 0;
    }
    string_count = 0;
    width_count = 0;
    spacing = VR1_FX_ONE;
    justification = VR1_TEXT_LEFT;

    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "string")) {
            string_count = vr1_parse_mfstring(p, strings, VR1_MAX_TEXT_STRINGS);
        } else if (vr1_streq(p->lx.tok.text, "spacing")) {
            spacing = vr1_parse_fx_token(p);
            if (spacing <= 0) spacing = VR1_FX_ONE;
        } else if (vr1_streq(p->lx.tok.text, "justification")) {
            vr1_lexer_next(&p->lx);
            if (vr1_streq(p->lx.tok.text, "CENTER")) justification = VR1_TEXT_CENTER;
            else if (vr1_streq(p->lx.tok.text, "RIGHT")) justification = VR1_TEXT_RIGHT;
            else justification = VR1_TEXT_LEFT;
        } else if (vr1_streq(p->lx.tok.text, "width")) {
            width_count = vr1_parse_mffx(p, widths, VR1_MAX_TEXT_STRINGS);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_emit_ascii_text(p, strings, string_count, spacing, justification, widths, width_count);
}


static void vr1_parse_info_node(vr1_parser_context *p)
{
    char text[VR1_STRING_MAX];

    text[0] = '\0';
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "string")) {
            vr1_parse_string_token(p, text, VR1_STRING_MAX);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    (void)vr1_scene_add_info(p->scene, text);
}

static void vr1_parse_label_node(vr1_parser_context *p)
{
    char text[VR1_STRING_MAX];

    text[0] = '\0';
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "label")) vr1_parse_string_token(p, text, VR1_STRING_MAX);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    if (text[0] != '\0') (void)vr1_scene_add_info(p->scene, text);
}

static void vr1_parse_wwwinline_node(vr1_parser_context *p)
{
    vr1_inline inl;

    memset(&inl, 0, sizeof(inl));
    inl.bbox_center = vr1_vec3_make(0, 0, 0);
    inl.bbox_size = vr1_vec3_make(0, 0, 0);
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "name") || vr1_streq(p->lx.tok.text, "filename")) {
            vr1_parse_string_token(p, inl.name, VR1_STRING_MAX);
        } else if (vr1_streq(p->lx.tok.text, "bboxCenter")) {
            inl.bbox_center = vr1_parse_vec3(p);
        } else if (vr1_streq(p->lx.tok.text, "bboxSize")) {
            inl.bbox_size = vr1_parse_vec3(p);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    (void)vr1_scene_add_inline(p->scene, &inl);
}

static void vr1_parse_texture2_node(vr1_parser_context *p)
{
    char filename[VR1_STRING_MAX];
    int wrap_s;
    int wrap_t;

    filename[0] = '\0';
    wrap_s = VR1_WRAP_REPEAT;
    wrap_t = VR1_WRAP_REPEAT;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "filename")) {
            vr1_parse_string_token(p, filename, VR1_STRING_MAX);
        } else if (vr1_streq(p->lx.tok.text, "wrapS")) {
            vr1_lexer_next(&p->lx);
            wrap_s = vr1_streq(p->lx.tok.text, "CLAMP") ? VR1_WRAP_CLAMP : VR1_WRAP_REPEAT;
        } else if (vr1_streq(p->lx.tok.text, "wrapT")) {
            vr1_lexer_next(&p->lx);
            wrap_t = vr1_streq(p->lx.tok.text, "CLAMP") ? VR1_WRAP_CLAMP : VR1_WRAP_REPEAT;
        } else if (vr1_streq(p->lx.tok.text, "image")) {
            vr1_skip_value(p);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_apply_texture_to_state(p, filename, wrap_s, wrap_t);
}

static void vr1_parse_translation_node(vr1_parser_context *p)
{
    vr1_vec3 t;
    vr1_fx m[16];

    t = vr1_vec3_make(0, 0, 0);
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "translation")) t = vr1_parse_vec3(p);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_mat_translation(m, t);
    vr1_state_postmul(&p->state, m);
}

static void vr1_parse_scale_node(vr1_parser_context *p)
{
    vr1_vec3 s;
    vr1_fx m[16];

    s = vr1_vec3_make(VR1_FX_ONE, VR1_FX_ONE, VR1_FX_ONE);
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "scaleFactor")) s = vr1_parse_vec3(p);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_mat_scale(m, s);
    vr1_state_postmul(&p->state, m);
}

static void vr1_parse_rotation_node(vr1_parser_context *p)
{
    vr1_vec3 axis;
    vr1_fx angle;
    vr1_fx m[16];

    axis = vr1_vec3_make(0, 0, VR1_FX_ONE);
    angle = 0;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "rotation")) {
            axis.x = vr1_parse_fx_token(p);
            axis.y = vr1_parse_fx_token(p);
            axis.z = vr1_parse_fx_token(p);
            angle = vr1_parse_fx_token(p);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_mat_rotation(m, axis, angle);
    vr1_state_postmul(&p->state, m);
}


static void vr1_parse_rotation_xyz_node(vr1_parser_context *p)
{
    vr1_vec3 axis;
    vr1_fx angle;
    vr1_fx m[16];

    axis = vr1_vec3_make(0, 0, VR1_FX_ONE);
    angle = 0;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "axis")) {
            vr1_lexer_next(&p->lx);
            if (vr1_streq(p->lx.tok.text, "X")) axis = vr1_vec3_make(VR1_FX_ONE, 0, 0);
            else if (vr1_streq(p->lx.tok.text, "Y")) axis = vr1_vec3_make(0, VR1_FX_ONE, 0);
            else axis = vr1_vec3_make(0, 0, VR1_FX_ONE);
        } else if (vr1_streq(p->lx.tok.text, "angle")) {
            angle = vr1_parse_fx_token(p);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_mat_rotation(m, axis, angle);
    vr1_state_postmul(&p->state, m);
}

static void vr1_parse_matrix_transform_node(vr1_parser_context *p)
{
    vr1_fx m[16];
    int i;

    vr1_mat_identity(m);
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "matrix")) {
            for (i = 0; i < 16; i++) m[i] = vr1_parse_fx_token(p);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_state_postmul(&p->state, m);
}

static void vr1_parse_transform_node(vr1_parser_context *p)
{
    vr1_vec3 translation;
    vr1_vec3 center;
    vr1_vec3 scale_factor;
    vr1_vec3 rot_axis;
    vr1_vec3 so_axis;
    vr1_fx rot_angle;
    vr1_fx so_angle;
    vr1_fx m[16];
    vr1_fx tmp[16];
    vr1_vec3 neg_center;
    vr1_vec3 neg_axis;

    translation = vr1_vec3_make(0, 0, 0);
    center = vr1_vec3_make(0, 0, 0);
    scale_factor = vr1_vec3_make(VR1_FX_ONE, VR1_FX_ONE, VR1_FX_ONE);
    rot_axis = vr1_vec3_make(0, 0, VR1_FX_ONE);
    so_axis = vr1_vec3_make(0, 0, VR1_FX_ONE);
    rot_angle = 0;
    so_angle = 0;

    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "translation")) translation = vr1_parse_vec3(p);
        else if (vr1_streq(p->lx.tok.text, "center")) center = vr1_parse_vec3(p);
        else if (vr1_streq(p->lx.tok.text, "scaleFactor")) scale_factor = vr1_parse_vec3(p);
        else if (vr1_streq(p->lx.tok.text, "rotation")) {
            rot_axis.x = vr1_parse_fx_token(p);
            rot_axis.y = vr1_parse_fx_token(p);
            rot_axis.z = vr1_parse_fx_token(p);
            rot_angle = vr1_parse_fx_token(p);
        } else if (vr1_streq(p->lx.tok.text, "scaleOrientation")) {
            so_axis.x = vr1_parse_fx_token(p);
            so_axis.y = vr1_parse_fx_token(p);
            so_axis.z = vr1_parse_fx_token(p);
            so_angle = vr1_parse_fx_token(p);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);

    vr1_mat_identity(m);
    vr1_mat_translation(tmp, translation); vr1_mat_mul(m, m, tmp);
    vr1_mat_translation(tmp, center); vr1_mat_mul(m, m, tmp);
    vr1_mat_rotation(tmp, rot_axis, rot_angle); vr1_mat_mul(m, m, tmp);
    vr1_mat_rotation(tmp, so_axis, so_angle); vr1_mat_mul(m, m, tmp);
    vr1_mat_scale(tmp, scale_factor); vr1_mat_mul(m, m, tmp);
    neg_axis = so_axis;
    vr1_mat_rotation(tmp, neg_axis, -so_angle); vr1_mat_mul(m, m, tmp);
    neg_center = vr1_vec3_make(-center.x, -center.y, -center.z);
    vr1_mat_translation(tmp, neg_center); vr1_mat_mul(m, m, tmp);
    vr1_state_postmul(&p->state, m);
}


static vr1_fx vr1_units_scale_from_name(const char *name)
{
    if (vr1_streq(name, "METERS")) return VR1_FX_ONE;
    if (vr1_streq(name, "CENTIMETERS")) return vr1_fx_div(VR1_FX_ONE, vr1_fx_from_int(100));
    if (vr1_streq(name, "MILLIMETERS")) return vr1_fx_div(VR1_FX_ONE, vr1_fx_from_int(1000));
    if (vr1_streq(name, "KILOMETERS")) return vr1_fx_from_int(1000);
    if (vr1_streq(name, "INCHES")) return vr1_fx_from_ascii("0.0254", 6, 0);
    if (vr1_streq(name, "FEET")) return vr1_fx_from_ascii("0.3048", 6, 0);
    if (vr1_streq(name, "YARDS")) return vr1_fx_from_ascii("0.9144", 6, 0);
    return VR1_FX_ONE;
}

static void vr1_parse_units_node(vr1_parser_context *p)
{
    vr1_fx scale;
    vr1_fx m[16];
    vr1_vec3 sv;

    scale = VR1_FX_ONE;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "units")) {
            vr1_lexer_next(&p->lx);
            if (p->lx.tok.type == VR1_T_IDENT) scale = vr1_units_scale_from_name(p->lx.tok.text);
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    sv = vr1_vec3_make(scale, scale, scale);
    vr1_mat_scale(m, sv);
    vr1_state_postmul(&p->state, m);
}


static void vr1_compute_state_bbox(vr1_parser_context *p, vr1_vec3 *minp, vr1_vec3 *maxp)
{
    int i;

    minp->x = minp->y = minp->z = 0;
    maxp->x = maxp->y = maxp->z = 0;
    if (p->state.coord_count <= 0) return;
    *minp = p->state.coords[0];
    *maxp = p->state.coords[0];
    for (i = 1; i < p->state.coord_count; i++) {
        vr1_vec3 v;
        v = p->state.coords[i];
        if (v.x < minp->x) minp->x = v.x;
        if (v.y < minp->y) minp->y = v.y;
        if (v.z < minp->z) minp->z = v.z;
        if (v.x > maxp->x) maxp->x = v.x;
        if (v.y > maxp->y) maxp->y = v.y;
        if (v.z > maxp->z) maxp->z = v.z;
    }
}

static void vr1_emit_faceset(vr1_parser_context *p, long start_index, const long *num_vertices, long num_count)
{
    vr1_vec3 minp;
    vr1_vec3 maxp;
    int axis_s;
    int axis_t;
    long cursor;
    long face_no;
    long f;

    if (start_index < 0) start_index = 0;
    if (start_index >= p->state.coord_count) return;
    vr1_compute_state_bbox(p, &minp, &maxp);
    vr1_choose_bbox_axes(minp, maxp, &axis_s, &axis_t);
    vr1_clear_generated_normals(p);
    cursor = start_index;
    face_no = 0;
    if (num_count <= 0) num_count = 1;
    for (f = 0; f < num_count && cursor < p->state.coord_count; f++) {
        long nv;
        long j;
        vr1_face_vert face[VR1_MAX_FACE_VERTS];
        int fc;

        nv = (num_vertices != 0) ? num_vertices[f] : -1;
        if (nv < 0) nv = (long)p->state.coord_count - cursor;
        if (nv <= 0) continue;
        fc = 0;
        for (j = 0; j < nv && cursor + j < p->state.coord_count; j++) {
            if (fc < VR1_MAX_FACE_VERTS) {
                face[fc].coord = (int)(cursor + j);
                face[fc].normal = -1;
                face[fc].tex = -1;
                face[fc].mat_index = -1;
                face[fc].vertex_ordinal = cursor + j;
                fc++;
            } else {
                vr1_scene_warn(p->scene);
            }
        }
        if (fc >= 3) vr1_emit_ifs_face(p, face, fc, face_no, minp, maxp, axis_s, axis_t);
        cursor += nv;
        face_no++;
    }
}

static void vr1_emit_lineset(vr1_parser_context *p, long start_index, const long *num_vertices, long num_count)
{
    long cursor;
    long strip;
    long segment_no;

    if (start_index < 0) start_index = 0;
    if (start_index >= p->state.coord_count) return;
    cursor = start_index;
    strip = 0;
    segment_no = 0;
    if (num_count <= 0) num_count = 1;
    while (strip < num_count && cursor < p->state.coord_count) {
        long nv;
        long j;
        nv = (num_vertices != 0) ? num_vertices[strip] : -1;
        if (nv < 0) nv = (long)p->state.coord_count - cursor;
        for (j = 0; j + 1 < nv && cursor + j + 1 < p->state.coord_count; j++) {
            vr1_vertex va;
            vr1_vertex vb;
            long ia;
            long ib;
            int mat;
            memset(&va, 0, sizeof(va));
            memset(&vb, 0, sizeof(vb));
            mat = vr1_current_material_index(p, strip, cursor + j, -1);
            va.pos = vr1_transform_point(p->state.matrix, p->state.coords[cursor + j]);
            vb.pos = vr1_transform_point(p->state.matrix, p->state.coords[cursor + j + 1]);
            va.normal = vr1_vec3_make(0, 0, VR1_FX_ONE);
            vb.normal = va.normal;
            va.material = mat;
            vb.material = mat;
            ia = vr1_scene_add_vertex(p->scene, &va);
            ib = vr1_scene_add_vertex(p->scene, &vb);
            if (ia >= 0 && ib >= 0) (void)vr1_scene_add_line(p->scene, ia, ib, mat);
            segment_no++;
        }
        cursor += nv;
        strip++;
    }
    (void)segment_no;
}

static void vr1_emit_quadmesh(vr1_parser_context *p, long start_index, long vertices_per_row, long vertices_per_column)
{
    vr1_vec3 minp;
    vr1_vec3 maxp;
    int axis_s;
    int axis_t;
    long row;
    long col;
    long face_no;

    if (start_index < 0) start_index = 0;
    if (vertices_per_row < 2 || vertices_per_column < 2) return;
    if (start_index >= p->state.coord_count) return;
    if (start_index + vertices_per_row * vertices_per_column > p->state.coord_count) {
        vr1_scene_warn(p->scene);
    }
    vr1_compute_state_bbox(p, &minp, &maxp);
    vr1_choose_bbox_axes(minp, maxp, &axis_s, &axis_t);
    vr1_clear_generated_normals(p);
    face_no = 0;
    for (row = 0; row + 1 < vertices_per_column; row++) {
        for (col = 0; col + 1 < vertices_per_row; col++) {
            long base;
            vr1_face_vert face[4];
            int k;
            base = start_index + row * vertices_per_row + col;
            if (base + vertices_per_row + 1 >= p->state.coord_count) continue;
            face[0].coord = (int)base;
            face[1].coord = (int)(base + 1);
            face[2].coord = (int)(base + vertices_per_row + 1);
            face[3].coord = (int)(base + vertices_per_row);
            for (k = 0; k < 4; k++) {
                face[k].normal = -1;
                face[k].tex = -1;
                face[k].mat_index = -1;
                face[k].vertex_ordinal = face[k].coord;
            }
            vr1_emit_ifs_face(p, face, 4, face_no, minp, maxp, axis_s, axis_t);
            face_no++;
        }
    }
}

static void vr1_parse_quadmesh_node(vr1_parser_context *p)
{
    long start_index;
    long vertices_per_row;
    long vertices_per_column;

    start_index = 0;
    vertices_per_row = 0;
    vertices_per_column = 0;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "startIndex")) start_index = vr1_parse_long_token(p);
        else if (vr1_streq(p->lx.tok.text, "verticesPerRow")) vertices_per_row = vr1_parse_long_token(p);
        else if (vr1_streq(p->lx.tok.text, "verticesPerColumn")) vertices_per_column = vr1_parse_long_token(p);
        else if (vr1_streq(p->lx.tok.text, "vertexProperty")) {
            if (vr1_next_is(p, VR1_T_IDENT)) {
                vr1_lexer_next(&p->lx);
                if (vr1_streq(p->lx.tok.text, "VertexProperty")) vr1_parse_vertex_property_node(p);
                else vr1_skip_value(p);
            } else {
                vr1_skip_value(p);
            }
        } else {
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_emit_quadmesh(p, start_index, vertices_per_row, vertices_per_column);
}

static void vr1_parse_faceset_node(vr1_parser_context *p)
{
    long start_index;
    long nums[VR1_MAX_FACE_VERTS];
    long num_count;

    start_index = 0;
    num_count = 0;
    nums[0] = -1;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "startIndex")) start_index = vr1_parse_long_token(p);
        else if (vr1_streq(p->lx.tok.text, "numVertices")) vr1_parse_mflong(p, nums, &num_count, VR1_MAX_FACE_VERTS);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    if (num_count <= 0) { nums[0] = -1; num_count = 1; }
    vr1_emit_faceset(p, start_index, nums, num_count);
}

static void vr1_parse_lineset_node(vr1_parser_context *p)
{
    long start_index;
    long nums[VR1_MAX_FACE_VERTS];
    long num_count;

    start_index = 0;
    num_count = 0;
    nums[0] = -1;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "startIndex")) start_index = vr1_parse_long_token(p);
        else if (vr1_streq(p->lx.tok.text, "numVertices")) vr1_parse_mflong(p, nums, &num_count, VR1_MAX_FACE_VERTS);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    if (num_count <= 0) { nums[0] = -1; num_count = 1; }
    vr1_emit_lineset(p, start_index, nums, num_count);
}

static void vr1_emit_tri_strip_face(vr1_parser_context *p, const int *idx, int a, int b, int c, long face_no, vr1_vec3 minp, vr1_vec3 maxp, int axis_s, int axis_t)
{
    vr1_face_vert tri[3];
    int k;

    for (k = 0; k < 3; k++) {
        tri[k].coord = (k == 0) ? idx[a] : ((k == 1) ? idx[b] : idx[c]);
        tri[k].normal = -1;
        tri[k].tex = -1;
        tri[k].mat_index = -1;
        tri[k].vertex_ordinal = face_no * 3 + k;
    }
    vr1_emit_ifs_face(p, tri, 3, face_no, minp, maxp, axis_s, axis_t);
}

static void vr1_parse_triangle_stripset_node(vr1_parser_context *p)
{
    long start_index;
    long nums[VR1_MAX_FACE_VERTS];
    long num_count;
    vr1_vec3 minp;
    vr1_vec3 maxp;
    int axis_s;
    int axis_t;
    long cursor;
    long strip;
    long face_no;

    start_index = 0;
    num_count = 0;
    nums[0] = -1;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "startIndex")) start_index = vr1_parse_long_token(p);
        else if (vr1_streq(p->lx.tok.text, "numVertices")) vr1_parse_mflong(p, nums, &num_count, VR1_MAX_FACE_VERTS);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    if (num_count <= 0) { nums[0] = -1; num_count = 1; }
    if (start_index < 0) start_index = 0;
    if (start_index >= p->state.coord_count) return;
    vr1_compute_state_bbox(p, &minp, &maxp);
    vr1_choose_bbox_axes(minp, maxp, &axis_s, &axis_t);
    vr1_clear_generated_normals(p);
    cursor = start_index;
    face_no = 0;
    for (strip = 0; strip < num_count && cursor < p->state.coord_count; strip++) {
        long nv;
        long j;
        int idx[VR1_MAX_FACE_VERTS];
        int ic;
        nv = nums[strip];
        if (nv < 0) nv = (long)p->state.coord_count - cursor;
        ic = 0;
        for (j = 0; j < nv && cursor + j < p->state.coord_count && ic < VR1_MAX_FACE_VERTS; j++) {
            idx[ic] = (int)(cursor + j);
            ic++;
        }
        for (j = 0; j + 2 < ic; j++) {
            if ((j & 1) == 0) vr1_emit_tri_strip_face(p, idx, (int)j, (int)j + 1, (int)j + 2, face_no, minp, maxp, axis_s, axis_t);
            else vr1_emit_tri_strip_face(p, idx, (int)j + 1, (int)j, (int)j + 2, face_no, minp, maxp, axis_s, axis_t);
            face_no++;
        }
        cursor += nv;
    }
}

static void vr1_parse_indexed_triangle_stripset_node(vr1_parser_context *p)
{
    vr1_vec3 minp;
    vr1_vec3 maxp;
    int axis_s;
    int axis_t;
    int idx[VR1_MAX_FACE_VERTS];
    int ic;
    long i;
    long face_no;

    p->coord_index_count = 0;
    p->material_index_count = 0;
    p->normal_index_count = 0;
    p->texcoord_index_count = 0;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "coordIndex")) vr1_parse_mflong(p, p->coord_index, &p->coord_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "materialIndex")) vr1_parse_mflong(p, p->material_index, &p->material_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "normalIndex")) vr1_parse_mflong(p, p->normal_index, &p->normal_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "textureCoordIndex")) vr1_parse_mflong(p, p->texcoord_index, &p->texcoord_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "vertexProperty")) {
            if (vr1_next_is(p, VR1_T_IDENT)) {
                vr1_lexer_next(&p->lx);
                if (vr1_streq(p->lx.tok.text, "VertexProperty")) vr1_parse_vertex_property_node(p);
                else vr1_skip_value(p);
            } else {
                vr1_skip_value(p);
            }
        }
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_compute_index_bbox(p, &minp, &maxp);
    vr1_choose_bbox_axes(minp, maxp, &axis_s, &axis_t);
    vr1_clear_generated_normals(p);
    face_no = 0;
    ic = 0;
    for (i = 0; i <= p->coord_index_count; i++) {
        long ci;
        ci = (i < p->coord_index_count) ? p->coord_index[i] : -1;
        if (ci == -1) {
            long j;
            for (j = 0; j + 2 < ic; j++) {
                if ((j & 1) == 0) vr1_emit_tri_strip_face(p, idx, (int)j, (int)j + 1, (int)j + 2, face_no, minp, maxp, axis_s, axis_t);
                else vr1_emit_tri_strip_face(p, idx, (int)j + 1, (int)j, (int)j + 2, face_no, minp, maxp, axis_s, axis_t);
                face_no++;
            }
            ic = 0;
        } else if (vr1_index_valid((int)ci, p->state.coord_count)) {
            if (ic < VR1_MAX_FACE_VERTS) idx[ic++] = (int)ci;
            else vr1_scene_warn(p->scene);
        }
    }
}

static void vr1_parse_indexed_triangle_mesh_node(vr1_parser_context *p)
{
    vr1_vec3 minp;
    vr1_vec3 maxp;
    int axis_s;
    int axis_t;
    long i;
    long face_no;

    p->coord_index_count = 0;
    p->material_index_count = 0;
    p->normal_index_count = 0;
    p->texcoord_index_count = 0;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "coordIndex")) vr1_parse_mflong(p, p->coord_index, &p->coord_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "materialIndex")) vr1_parse_mflong(p, p->material_index, &p->material_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "normalIndex")) vr1_parse_mflong(p, p->normal_index, &p->normal_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "textureCoordIndex")) vr1_parse_mflong(p, p->texcoord_index, &p->texcoord_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "vertexProperty")) {
            if (vr1_next_is(p, VR1_T_IDENT)) {
                vr1_lexer_next(&p->lx);
                if (vr1_streq(p->lx.tok.text, "VertexProperty")) vr1_parse_vertex_property_node(p);
                else vr1_skip_value(p);
            } else {
                vr1_skip_value(p);
            }
        }
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_compute_index_bbox(p, &minp, &maxp);
    vr1_choose_bbox_axes(minp, maxp, &axis_s, &axis_t);
    vr1_clear_generated_normals(p);
    face_no = 0;
    for (i = 0; i + 2 < p->coord_index_count; i += 3) {
        vr1_face_vert tri[3];
        int k;
        if (!vr1_index_valid((int)p->coord_index[i], p->state.coord_count) || !vr1_index_valid((int)p->coord_index[i + 1], p->state.coord_count) || !vr1_index_valid((int)p->coord_index[i + 2], p->state.coord_count)) {
            vr1_scene_warn(p->scene);
            continue;
        }
        for (k = 0; k < 3; k++) {
            tri[k].coord = (int)p->coord_index[i + k];
            tri[k].normal = -1;
            tri[k].tex = -1;
            tri[k].mat_index = (i + k < p->material_index_count) ? p->material_index[i + k] : -1;
            tri[k].vertex_ordinal = i + k;
        }
        vr1_emit_ifs_face(p, tri, 3, face_no, minp, maxp, axis_s, axis_t);
        face_no++;
    }
}

static void vr1_parse_indexed_faceset_node(vr1_parser_context *p)
{
    p->coord_index_count = 0;
    p->material_index_count = 0;
    p->normal_index_count = 0;
    p->texcoord_index_count = 0;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "coordIndex")) vr1_parse_mflong(p, p->coord_index, &p->coord_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "materialIndex")) vr1_parse_mflong(p, p->material_index, &p->material_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "normalIndex")) vr1_parse_mflong(p, p->normal_index, &p->normal_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "textureCoordIndex")) vr1_parse_mflong(p, p->texcoord_index, &p->texcoord_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "vertexProperty")) {
            if (vr1_next_is(p, VR1_T_IDENT)) {
                vr1_lexer_next(&p->lx);
                if (vr1_streq(p->lx.tok.text, "VertexProperty")) vr1_parse_vertex_property_node(p);
                else vr1_skip_value(p);
            } else {
                vr1_skip_value(p);
            }
        }
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_emit_indexed_faces(p);
}

static void vr1_parse_indexed_lineset_node(vr1_parser_context *p)
{
    p->coord_index_count = 0;
    p->material_index_count = 0;
    p->normal_index_count = 0;
    p->texcoord_index_count = 0;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "coordIndex")) vr1_parse_mflong(p, p->coord_index, &p->coord_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "materialIndex")) vr1_parse_mflong(p, p->material_index, &p->material_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "normalIndex")) vr1_parse_mflong(p, p->normal_index, &p->normal_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "textureCoordIndex")) vr1_parse_mflong(p, p->texcoord_index, &p->texcoord_index_count, VR1_MAX_FIELD_INDICES);
        else if (vr1_streq(p->lx.tok.text, "vertexProperty")) {
            if (vr1_next_is(p, VR1_T_IDENT)) {
                vr1_lexer_next(&p->lx);
                if (vr1_streq(p->lx.tok.text, "VertexProperty")) vr1_parse_vertex_property_node(p);
                else vr1_skip_value(p);
            } else {
                vr1_skip_value(p);
            }
        }
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_emit_indexed_lines(p);
}

static void vr1_parse_pointset_node(vr1_parser_context *p)
{
    long start_index;
    long num_points;

    start_index = 0;
    num_points = -1;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "startIndex")) start_index = vr1_parse_long_token(p);
        else if (vr1_streq(p->lx.tok.text, "numPoints")) num_points = vr1_parse_long_token(p);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_emit_pointset(p, start_index, num_points);
}

static void vr1_parse_cube_node(vr1_parser_context *p)
{
    vr1_fx width;
    vr1_fx height;
    vr1_fx depth;

    width = vr1_fx_from_int(2);
    height = vr1_fx_from_int(2);
    depth = vr1_fx_from_int(2);
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "width")) width = vr1_parse_fx_token(p);
        else if (vr1_streq(p->lx.tok.text, "height")) height = vr1_parse_fx_token(p);
        else if (vr1_streq(p->lx.tok.text, "depth")) depth = vr1_parse_fx_token(p);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_emit_cube(p, width, height, depth);
}

static void vr1_parse_sphere_node(vr1_parser_context *p)
{
    vr1_fx radius;

    radius = VR1_FX_ONE;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type == VR1_T_IDENT && vr1_streq(p->lx.tok.text, "radius")) radius = vr1_parse_fx_token(p);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_emit_sphere(p, radius);
}

static void vr1_parse_cylinder_node(vr1_parser_context *p)
{
    int parts;
    vr1_fx radius;
    vr1_fx height;

    parts = VR1_PART_ALL;
    radius = VR1_FX_ONE;
    height = vr1_fx_from_int(2);
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "parts")) parts = vr1_parse_parts_mask(p, 1);
        else if (vr1_streq(p->lx.tok.text, "radius")) radius = vr1_parse_fx_token(p);
        else if (vr1_streq(p->lx.tok.text, "height")) height = vr1_parse_fx_token(p);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_emit_cylinder(p, parts, radius, height);
}

static void vr1_parse_cone_node(vr1_parser_context *p)
{
    int parts;
    vr1_fx radius;
    vr1_fx height;

    parts = VR1_PART_SIDES | VR1_PART_BOTTOM;
    radius = VR1_FX_ONE;
    height = vr1_fx_from_int(2);
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "parts")) parts = vr1_parse_parts_mask(p, 0);
        else if (vr1_streq(p->lx.tok.text, "bottomRadius")) radius = vr1_parse_fx_token(p);
        else if (vr1_streq(p->lx.tok.text, "height")) height = vr1_parse_fx_token(p);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    vr1_emit_cone(p, parts, radius, height);
}

static void vr1_parse_camera_node(vr1_parser_context *p, int type)
{
    vr1_camera c;

    memset(&c, 0, sizeof(c));
    c.type = type;
    c.position = vr1_vec3_make(0, 0, VR1_FX_ONE);
    c.axis = vr1_vec3_make(0, 0, VR1_FX_ONE);
    c.angle = 0;
    c.focal_distance = vr1_fx_from_int(5);
    c.height = vr1_fx_from_int(2);
    c.height_angle = vr1_fx_from_ascii("0.785398", 8, 0);
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "position")) c.position = vr1_parse_vec3(p);
        else if (vr1_streq(p->lx.tok.text, "orientation")) {
            c.axis.x = vr1_parse_fx_token(p);
            c.axis.y = vr1_parse_fx_token(p);
            c.axis.z = vr1_parse_fx_token(p);
            c.angle = vr1_parse_fx_token(p);
        } else if (vr1_streq(p->lx.tok.text, "focalDistance")) c.focal_distance = vr1_parse_fx_token(p);
        else if (vr1_streq(p->lx.tok.text, "height")) c.height = vr1_parse_fx_token(p);
        else if (vr1_streq(p->lx.tok.text, "heightAngle")) c.height_angle = vr1_parse_fx_token(p);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    c.position = vr1_transform_point(p->state.matrix, c.position);
    c.axis = vr1_transform_vector(p->state.matrix, c.axis);
    (void)vr1_scene_add_camera(p->scene, &c);
}

static void vr1_parse_light_node(vr1_parser_context *p, int type)
{
    vr1_light l;

    memset(&l, 0, sizeof(l));
    l.type = type;
    l.on = 1;
    l.intensity = VR1_FX_ONE;
    l.color.r = VR1_FX_ONE;
    l.color.g = VR1_FX_ONE;
    l.color.b = VR1_FX_ONE;
    l.location = vr1_vec3_make(0, 0, VR1_FX_ONE);
    l.direction = vr1_vec3_make(0, 0, -VR1_FX_ONE);
    l.drop_off_rate = 0;
    l.cut_off_angle = vr1_fx_from_ascii("0.785398", 8, 0);
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "on")) l.on = vr1_parse_bool_token(p);
        else if (vr1_streq(p->lx.tok.text, "intensity")) l.intensity = vr1_parse_fx_token(p);
        else if (vr1_streq(p->lx.tok.text, "color")) {
            l.color.r = vr1_parse_fx_token(p);
            l.color.g = vr1_parse_fx_token(p);
            l.color.b = vr1_parse_fx_token(p);
        } else if (vr1_streq(p->lx.tok.text, "location")) l.location = vr1_parse_vec3(p);
        else if (vr1_streq(p->lx.tok.text, "direction")) l.direction = vr1_parse_vec3(p);
        else if (vr1_streq(p->lx.tok.text, "dropOffRate")) l.drop_off_rate = vr1_parse_fx_token(p);
        else if (vr1_streq(p->lx.tok.text, "cutOffAngle")) l.cut_off_angle = vr1_parse_fx_token(p);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    l.location = vr1_transform_point(p->state.matrix, l.location);
    l.direction = vr1_transform_vector(p->state.matrix, l.direction);
    (void)vr1_scene_add_light(p->scene, &l);
}

static void vr1_parse_texture2_transform_node(vr1_parser_context *p)
{
    vr1_vec2 translation;
    vr1_vec2 scale_factor;
    vr1_vec2 center;
    vr1_fx rotation;

    translation = vr1_vec2_make(0, 0);
    scale_factor = vr1_vec2_make(VR1_FX_ONE, VR1_FX_ONE);
    center = vr1_vec2_make(0, 0);
    rotation = 0;
    if (!vr1_expect(p, VR1_T_LBRACE)) return;
    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) { vr1_skip_value(p); continue; }
        if (vr1_streq(p->lx.tok.text, "translation")) translation = vr1_parse_vec2(p);
        else if (vr1_streq(p->lx.tok.text, "rotation")) rotation = vr1_parse_fx_token(p);
        else if (vr1_streq(p->lx.tok.text, "scaleFactor")) scale_factor = vr1_parse_vec2(p);
        else if (vr1_streq(p->lx.tok.text, "center")) center = vr1_parse_vec2(p);
        else vr1_skip_value(p);
    }
    (void)vr1_expect(p, VR1_T_RBRACE);
    p->state.tex_translation = translation;
    p->state.tex_scale = scale_factor;
    p->state.tex_center = center;
    p->state.tex_rotation = rotation;
    {
        vr1_fx tm[9];
        vr1_texmat_texture2(tm, translation, scale_factor, center, rotation);
        vr1_state_postmul_tex(&p->state, tm);
    }
}

static int vr1_parse_group_contents(vr1_parser_context *p, const char *group_name)
{
    int pushed_state;
    int pushed_matrix;
    int old_emit;
    int is_anchor;
    long child_index;
    long which_child;
    vr1_anchor anchor;

    pushed_state = 0;
    pushed_matrix = 0;
    old_emit = p->emit_enabled;
    child_index = 0;
    which_child = -1;
    is_anchor = vr1_streq(group_name, "WWWAnchor");
    memset(&anchor, 0, sizeof(anchor));

    if (vr1_streq(group_name, "Separator") || is_anchor) {
        vr1_push_state(p);
        pushed_state = 1;
    } else if (vr1_streq(group_name, "TransformSeparator")) {
        vr1_push_matrix(p);
        pushed_matrix = 1;
    }

    if (!vr1_expect(p, VR1_T_LBRACE)) {
        if (pushed_state) vr1_pop_state(p);
        if (pushed_matrix) vr1_pop_matrix(p);
        return 0;
    }

    if (is_anchor) {
        anchor.first_triangle = p->scene->triangle_count;
        anchor.first_line = p->scene->line_count;
        anchor.first_point = p->scene->point_count;
    }

    while (!vr1_next_is(p, VR1_T_EOF) && !vr1_next_is(p, VR1_T_RBRACE)) {
        vr1_token t;
        vr1_lexer_peek(&p->lx, &t);
        if (t.type != VR1_T_IDENT) {
            vr1_lexer_next(&p->lx);
            continue;
        }
        if (is_anchor && vr1_streq(t.text, "name")) {
            vr1_lexer_next(&p->lx);
            vr1_parse_string_token(p, anchor.name, VR1_STRING_MAX);
        } else if (is_anchor && vr1_streq(t.text, "description")) {
            vr1_lexer_next(&p->lx);
            vr1_parse_string_token(p, anchor.description, VR1_STRING_MAX);
        } else if (is_anchor && vr1_streq(t.text, "map")) {
            vr1_lexer_next(&p->lx);
            vr1_lexer_next(&p->lx);
            anchor.map_point = vr1_streq(p->lx.tok.text, "POINT") ? 1 : 0;
        } else if (vr1_streq(t.text, "renderCulling") || vr1_streq(t.text, "name") || vr1_streq(t.text, "description") || vr1_streq(t.text, "map") || vr1_streq(t.text, "range") || vr1_streq(t.text, "center") || vr1_streq(t.text, "screenArea")) {
            vr1_lexer_next(&p->lx);
            vr1_skip_value(p);
        } else if (vr1_streq(t.text, "whichChild")) {
            vr1_lexer_next(&p->lx);
            which_child = vr1_parse_long_token(p);
        } else if (vr1_is_nodeish_name(t.text) || vr1_streq(t.text, "DEF") || vr1_streq(t.text, "USE")) {
            int should_emit;
            should_emit = 1;
            if (vr1_streq(group_name, "Switch")) {
                if (which_child == -1) should_emit = 0;
                else if (which_child == -3) should_emit = 1;
                else if (which_child >= 0) should_emit = (child_index == which_child);
            } else if (vr1_streq(group_name, "LOD") || vr1_streq(group_name, "LevelOfDetail")) {
                /* Geometry extraction has no live eye point; keep the highest-detail child. */
                should_emit = (child_index == 0);
            }
            p->emit_enabled = old_emit && should_emit;
            (void)vr1_parse_any_node(p);
            p->emit_enabled = old_emit;
            child_index++;
        } else {
            vr1_lexer_next(&p->lx);
            vr1_skip_value(p);
        }
    }
    (void)vr1_expect(p, VR1_T_RBRACE);

    if (is_anchor) {
        anchor.triangle_count = p->scene->triangle_count - anchor.first_triangle;
        anchor.line_count = p->scene->line_count - anchor.first_line;
        anchor.point_count = p->scene->point_count - anchor.first_point;
        if (anchor.name[0] != '\0' || anchor.description[0] != '\0' || anchor.triangle_count > 0 || anchor.line_count > 0 || anchor.point_count > 0) {
            (void)vr1_scene_add_anchor(p->scene, &anchor);
        }
    }
    if (pushed_state) vr1_pop_state(p);
    if (pushed_matrix) vr1_pop_matrix(p);
    p->emit_enabled = old_emit;
    return 1;
}

static int vr1_is_supported_fallback_type(const char *type_name)
{
    if (type_name == 0) return 0;
    if (vr1_streq(type_name, "Group")) return 1;
    if (vr1_streq(type_name, "Separator")) return 1;
    if (vr1_streq(type_name, "TransformSeparator")) return 1;
    if (vr1_streq(type_name, "Coordinate3")) return 1;
    if (vr1_streq(type_name, "Normal")) return 1;
    if (vr1_streq(type_name, "TextureCoordinate2")) return 1;
    if (vr1_streq(type_name, "TextureCoordinateBinding")) return 1;
    if (vr1_streq(type_name, "VertexProperty")) return 1;
    if (vr1_streq(type_name, "Material")) return 1;
    if (vr1_streq(type_name, "MaterialBinding")) return 1;
    if (vr1_streq(type_name, "NormalBinding")) return 1;
    if (vr1_streq(type_name, "ShapeHints")) return 1;
    if (vr1_streq(type_name, "Texture2")) return 1;
    if (vr1_streq(type_name, "Texture2Transform")) return 1;
    if (vr1_streq(type_name, "Translation")) return 1;
    if (vr1_streq(type_name, "Rotation")) return 1;
    if (vr1_streq(type_name, "Scale")) return 1;
    if (vr1_streq(type_name, "Transform")) return 1;
    if (vr1_streq(type_name, "MatrixTransform")) return 1;
    if (vr1_streq(type_name, "RotationXYZ")) return 1;
    if (vr1_streq(type_name, "IndexedFaceSet")) return 1;
    if (vr1_streq(type_name, "FaceSet")) return 1;
    if (vr1_streq(type_name, "LineSet")) return 1;
    if (vr1_streq(type_name, "TriangleStripSet")) return 1;
    if (vr1_streq(type_name, "IndexedTriangleStripSet")) return 1;
    if (vr1_streq(type_name, "IndexedTriangleMesh")) return 1;
    if (vr1_streq(type_name, "IndexedLineSet")) return 1;
    if (vr1_streq(type_name, "PointSet")) return 1;
    if (vr1_streq(type_name, "Cube")) return 1;
    if (vr1_streq(type_name, "Sphere")) return 1;
    if (vr1_streq(type_name, "Cylinder")) return 1;
    if (vr1_streq(type_name, "Cone")) return 1;
    if (vr1_streq(type_name, "PerspectiveCamera")) return 1;
    if (vr1_streq(type_name, "OrthographicCamera")) return 1;
    if (vr1_streq(type_name, "DirectionalLight")) return 1;
    if (vr1_streq(type_name, "PointLight")) return 1;
    if (vr1_streq(type_name, "SpotLight")) return 1;
    if (vr1_streq(type_name, "FontStyle")) return 1;
    if (vr1_streq(type_name, "AsciiText")) return 1;
    if (vr1_streq(type_name, "Info")) return 1;
    if (vr1_streq(type_name, "WWWInline")) return 1;
    if (vr1_streq(type_name, "File")) return 1;
    if (vr1_streq(type_name, "Units")) return 1;
    if (vr1_streq(type_name, "Label")) return 1;
    if (vr1_streq(type_name, "Text3")) return 1;
    if (vr1_streq(type_name, "Text2")) return 1;
    if (vr1_streq(type_name, "QuadMesh")) return 1;
    if (vr1_streq(type_name, "PackedColor")) return 1;
    if (vr1_streq(type_name, "Coordinate4")) return 1;
    return 0;
}

static int vr1_scan_isA_fallback(vr1_parser_context *p, char fallback[VR1_NAME_MAX])
{
    vr1_lexer saved;
    vr1_token t;
    int depth;
    int in_isA;

    if (fallback == 0) return 0;
    fallback[0] = '\0';
    saved = p->lx;
    if (!vr1_accept(p, VR1_T_LBRACE)) {
        p->lx = saved;
        return 0;
    }
    depth = 1;
    in_isA = 0;
    while (depth > 0) {
        vr1_lexer_next(&p->lx);
        t = p->lx.tok;
        if (t.type == VR1_T_EOF) break;
        if (t.type == VR1_T_LBRACE) { depth++; in_isA = 0; }
        else if (t.type == VR1_T_RBRACE) { depth--; in_isA = 0; }
        else if (depth == 1 && t.type == VR1_T_IDENT && vr1_streq(t.text, "isA")) { in_isA = 1; }
        else if (depth == 1 && in_isA && (t.type == VR1_T_STRING || t.type == VR1_T_IDENT)) {
            if (vr1_is_supported_fallback_type(t.text)) {
                vr1_copy_text(fallback, VR1_NAME_MAX, t.text);
                p->lx = saved;
                return 1;
            }
        } else if (depth == 1 && in_isA && t.type == VR1_T_RBRACKET) {
            in_isA = 0;
        }
    }
    p->lx = saved;
    return 0;
}

static void vr1_parse_unknown_node(vr1_parser_context *p, const char *type_name)
{
    char fallback[VR1_NAME_MAX];

    (void)type_name;
    if (vr1_scan_isA_fallback(p, fallback)) {
        (void)vr1_parse_node_named(p, fallback, p->lx.pos, 0);
        return;
    }
    p->scene->skipped_unknown_nodes++;
    if (vr1_accept(p, VR1_T_LBRACE)) {
        vr1_skip_block_after_lbrace(p);
    } else {
        vr1_skip_value(p);
    }
}

static int vr1_parse_node_named(vr1_parser_context *p, const char *type_name, long start_offset, const char *def_name)
{
    long end_offset;

    if (type_name == 0) return 0;
    if (vr1_streq(type_name, "Group") || vr1_streq(type_name, "Separator") || vr1_streq(type_name, "TransformSeparator") || vr1_streq(type_name, "WWWAnchor") || vr1_streq(type_name, "Switch") || vr1_streq(type_name, "LOD") || vr1_streq(type_name, "LevelOfDetail")) {
        (void)vr1_parse_group_contents(p, type_name);
    } else if (vr1_streq(type_name, "Coordinate3")) vr1_parse_coordinate3_node(p);
    else if (vr1_streq(type_name, "Coordinate4")) vr1_parse_coordinate4_node(p);
    else if (vr1_streq(type_name, "Normal")) vr1_parse_normal_node(p);
    else if (vr1_streq(type_name, "TextureCoordinate2")) vr1_parse_texcoord2_node(p);
    else if (vr1_streq(type_name, "TextureCoordinateBinding")) vr1_parse_texture_coordinate_binding_node(p);
    else if (vr1_streq(type_name, "VertexProperty")) vr1_parse_vertex_property_node(p);
    else if (vr1_streq(type_name, "PackedColor")) vr1_parse_packed_color_node(p);
    else if (vr1_streq(type_name, "Material")) vr1_parse_material_node(p);
    else if (vr1_streq(type_name, "MaterialBinding")) vr1_parse_material_binding_node(p);
    else if (vr1_streq(type_name, "NormalBinding")) vr1_parse_normal_binding_node(p);
    else if (vr1_streq(type_name, "ShapeHints")) vr1_parse_shape_hints_node(p);
    else if (vr1_streq(type_name, "Texture2")) vr1_parse_texture2_node(p);
    else if (vr1_streq(type_name, "Texture2Transform")) vr1_parse_texture2_transform_node(p);
    else if (vr1_streq(type_name, "Translation")) vr1_parse_translation_node(p);
    else if (vr1_streq(type_name, "Rotation")) vr1_parse_rotation_node(p);
    else if (vr1_streq(type_name, "Scale")) vr1_parse_scale_node(p);
    else if (vr1_streq(type_name, "Transform")) vr1_parse_transform_node(p);
    else if (vr1_streq(type_name, "MatrixTransform")) vr1_parse_matrix_transform_node(p);
    else if (vr1_streq(type_name, "RotationXYZ")) vr1_parse_rotation_xyz_node(p);
    else if (vr1_streq(type_name, "Units")) vr1_parse_units_node(p);
    else if (vr1_streq(type_name, "IndexedFaceSet")) vr1_parse_indexed_faceset_node(p);
    else if (vr1_streq(type_name, "FaceSet")) vr1_parse_faceset_node(p);
    else if (vr1_streq(type_name, "LineSet")) vr1_parse_lineset_node(p);
    else if (vr1_streq(type_name, "TriangleStripSet")) vr1_parse_triangle_stripset_node(p);
    else if (vr1_streq(type_name, "QuadMesh")) vr1_parse_quadmesh_node(p);
    else if (vr1_streq(type_name, "IndexedTriangleStripSet")) vr1_parse_indexed_triangle_stripset_node(p);
    else if (vr1_streq(type_name, "IndexedTriangleMesh")) vr1_parse_indexed_triangle_mesh_node(p);
    else if (vr1_streq(type_name, "IndexedLineSet")) vr1_parse_indexed_lineset_node(p);
    else if (vr1_streq(type_name, "PointSet")) vr1_parse_pointset_node(p);
    else if (vr1_streq(type_name, "Cube")) vr1_parse_cube_node(p);
    else if (vr1_streq(type_name, "Sphere")) vr1_parse_sphere_node(p);
    else if (vr1_streq(type_name, "Cylinder")) vr1_parse_cylinder_node(p);
    else if (vr1_streq(type_name, "Cone")) vr1_parse_cone_node(p);
    else if (vr1_streq(type_name, "PerspectiveCamera")) vr1_parse_camera_node(p, 1);
    else if (vr1_streq(type_name, "OrthographicCamera")) vr1_parse_camera_node(p, 2);
    else if (vr1_streq(type_name, "DirectionalLight")) vr1_parse_light_node(p, 1);
    else if (vr1_streq(type_name, "PointLight")) vr1_parse_light_node(p, 2);
    else if (vr1_streq(type_name, "SpotLight")) vr1_parse_light_node(p, 3);
    else if (vr1_streq(type_name, "FontStyle")) vr1_parse_font_style_node(p);
    else if (vr1_streq(type_name, "AsciiText") || vr1_streq(type_name, "Text2") || vr1_streq(type_name, "Text3")) vr1_parse_ascii_text_node(p);
    else if (vr1_streq(type_name, "Info")) vr1_parse_info_node(p);
    else if (vr1_streq(type_name, "Label")) vr1_parse_label_node(p);
    else if (vr1_streq(type_name, "WWWInline") || vr1_streq(type_name, "File")) vr1_parse_wwwinline_node(p);
    else vr1_parse_unknown_node(p, type_name);

    end_offset = p->lx.pos;
    if (def_name != 0 && def_name[0] != '\0' && p->replay_depth == 0) {
        vr1_def_store(p, def_name, start_offset, end_offset);
    }
    return 1;
}

static int vr1_parse_any_node(vr1_parser_context *p)
{
    char def_name[VR1_NAME_MAX];
    char type_name[VR1_NAME_MAX];
    long start;

    def_name[0] = '\0';
    vr1_lexer_next(&p->lx);
    if (p->lx.tok.type == VR1_T_EOF) return 0;
    if (p->lx.tok.type != VR1_T_IDENT) {
        vr1_scene_error(p->scene);
        return 0;
    }

    if (vr1_streq(p->lx.tok.text, "DEF")) {
        vr1_parse_string_token(p, def_name, VR1_NAME_MAX);
        vr1_lexer_next(&p->lx);
        if (p->lx.tok.type != VR1_T_IDENT) {
            vr1_scene_error(p->scene);
            return 0;
        }
        start = p->lx.tok.start;
        vr1_copy_text(type_name, VR1_NAME_MAX, p->lx.tok.text);
        return vr1_parse_node_named(p, type_name, start, def_name);
    }

    if (vr1_streq(p->lx.tok.text, "USE")) {
        vr1_parse_string_token(p, def_name, VR1_NAME_MAX);
        return vr1_replay_def(p, def_name);
    }

    start = p->lx.tok.start;
    vr1_copy_text(type_name, VR1_NAME_MAX, p->lx.tok.text);
    return vr1_parse_node_named(p, type_name, start, 0);
}

static long vr1_header_end(const char *buf, long len)
{
    long i;

    if (buf == 0 || len <= 0) return -1;
    if (len >= 16 && strncmp(buf, "#VRML V1.0 ascii", 16) == 0) {
        i = 0;
        while (i < len && buf[i] != '\n' && buf[i] != '\r') i++;
        while (i < len && (buf[i] == '\n' || buf[i] == '\r')) i++;
        return i;
    }
    if ((len >= 20 && strncmp(buf, "#Inventor V1.0 ascii", 20) == 0) ||
        (len >= 20 && strncmp(buf, "#Inventor V2.0 ascii", 20) == 0) ||
        (len >= 20 && strncmp(buf, "#Inventor V2.1 ascii", 20) == 0)) {
        i = 0;
        while (i < len && buf[i] != '\n' && buf[i] != '\r') i++;
        while (i < len && (buf[i] == '\n' || buf[i] == '\r')) i++;
        return i;
    }
    return -1;
}

int vr1_parse_buffer(vr1_parser_context *p, vr1_scene *scene, const char *buf, long len)
{
    long start;

    if (p == 0 || scene == 0 || buf == 0 || len <= 0) return VR1_ERR_PARSE;
    vr1_parser_context_init(p);
    vr1_scene_init(scene);
    p->scene = scene;
    p->source = buf;
    p->source_len = len;
    start = vr1_header_end(buf, len);
    if (start < 0) {
        vr1_scene_error(scene);
        return VR1_ERR_HEADER;
    }
    vr1_lexer_init(&p->lx, buf, len, start);
    while (!vr1_next_is(p, VR1_T_EOF)) {
        if (!vr1_parse_any_node(p)) break;
    }
    if (scene->error_count > 0) return VR1_ERR_PARSE;
    return VR1_OK;
}

const char *vr1_result_name(int code)
{
    if (code == VR1_OK) return "ok";
    if (code == VR1_ERR_HEADER) return "bad header";
    if (code == VR1_ERR_PARSE) return "parse error";
    if (code == VR1_ERR_LIMIT) return "static limit reached";
    return "unknown";
}
