#include "gfdxf.h"

#include <stdio.h>
#include <string.h>

#define GFDXF_ENTITY_NONE 0
#define GFDXF_ENTITY_OTHER 1
#define GFDXF_ENTITY_LINE 2
#define GFDXF_ENTITY_3DFACE 3
#define GFDXF_ENTITY_LWPOLYLINE 4
#define GFDXF_ENTITY_POLYLINE 5
#define GFDXF_ENTITY_VERTEX 6
#define GFDXF_ENTITY_INSERT 7
#define GFDXF_ENTITY_CIRCLE 8
#define GFDXF_ENTITY_ARC 9
#define GFDXF_ENTITY_SPLINE 10
#define GFDXF_ENTITY_ELLIPSE 11
#define GFDXF_ENTITY_HATCH 12
#define GFDXF_ENTITY_DIMENSION 13
#define GFDXF_ENTITY_TEXT 14
#define GFDXF_ENTITY_MTEXT 15
#define GFDXF_ENTITY_POINT 16
#define GFDXF_ENTITY_SOLID 17
#define GFDXF_ENTITY_PROXY 18
#define GFDXF_ENTITY_ACIS 19
#define GFDXF_ENTITY_MESH 20
#define GFDXF_ENTITY_IMAGE 21
#define GFDXF_ENTITY_LEADER 22
#define GFDXF_ENTITY_MLEADER 23
#define GFDXF_ENTITY_RAY 24
#define GFDXF_ENTITY_XLINE 25
#define GFDXF_ENTITY_ATTDEF 26
#define GFDXF_ENTITY_ATTRIB 27
#define GFDXF_ENTITY_VIEWPORT 28
#define GFDXF_ENTITY_SHAPE 29
#define GFDXF_ENTITY_TOLERANCE 30
#define GFDXF_ENTITY_MLINE 31
#define GFDXF_ENTITY_HELIX 32
#define GFDXF_ENTITY_LIGHT 33
#define GFDXF_ENTITY_SECTION 34
#define GFDXF_ENTITY_PROXY_OBJECT 35
#define GFDXF_ENTITY_DICTIONARY 36
#define GFDXF_ENTITY_XRECORD 37
#define GFDXF_ENTITY_DICTIONARYVAR 38
#define GFDXF_ENTITY_OBJECT_OTHER 39
#define GFDXF_ENTITY_TABLE 40
#define GFDXF_ENTITY_COORDINATION_MODEL 41
#define GFDXF_ENTITY_OLE 42
#define GFDXF_ENTITY_GEODATA 43
#define GFDXF_ENTITY_POINTCLOUD 44
#define GFDXF_ENTITY_CONSTRAINT 45

#define GFDXF_SECTION_NONE 0
#define GFDXF_SECTION_BLOCKS 1
#define GFDXF_SECTION_ENTITIES 2
#define GFDXF_SECTION_TABLES 3
#define GFDXF_SECTION_CLASSES 4
#define GFDXF_SECTION_OBJECTS 5
#define GFDXF_SECTION_OTHER 6
#define GFDXF_SECTION_HEADER 7

#define GFDXF_TARGET_SCENE 0
#define GFDXF_TARGET_BLOCK 1

static const long gfdxf_sin90_q16_table[91] = {
    0L, 1144L, 2287L, 3430L, 4572L, 5712L, 6850L, 7987L, 9121L, 10252L, 11380L, 12505L, 13626L,
    14742L, 15855L, 16962L, 18064L, 19161L, 20252L, 21336L, 22415L, 23486L, 24550L, 25607L, 26656L, 27697L,
    28729L, 29753L, 30767L, 31772L, 32768L, 33754L, 34729L, 35693L, 36647L, 37590L, 38521L, 39441L, 40348L,
    41243L, 42126L, 42995L, 43852L, 44695L, 45525L, 46341L, 47143L, 47930L, 48703L, 49461L, 50203L, 50931L,
    51643L, 52339L, 53020L, 53684L, 54332L, 54963L, 55578L, 56175L, 56756L, 57319L, 57865L, 58393L, 58903L,
    59396L, 59870L, 60326L, 60764L, 61183L, 61584L, 61966L, 62328L, 62672L, 62997L, 63303L, 63589L, 63856L,
    64104L, 64332L, 64540L, 64729L, 64898L, 65048L, 65177L, 65287L, 65376L, 65446L, 65496L, 65526L, 65536L
};

typedef struct gfdxf_xform_s {
    gfdxf_fx m[3][3];
    gfdxf_vec3 t;
} gfdxf_xform;

typedef struct gfdxf_emit_ctx_s {
    int target;
    gfdxf_scene *scene;
    gfdxf_parser *parser;
    int block_index;
    const gfdxf_callbacks *callbacks;
    void *user;
    gfdxf_xform xf;
} gfdxf_emit_ctx;

typedef struct gfdxf_entity_s {
    int type;
    gfdxf_fx p[4][3];
    int have[4][3];
    int invisible_edges;
    int poly_flags;
    int vertex_flags;
    int face_indices[4];
    gfdxf_fx vertex_bulge;
    int lw_flags;
    int lw_count;
    int lw_have_x;
    int lw_have_y;
    int lw_last_index;
    gfdxf_fx lw_cur_x;
    gfdxf_fx lw_cur_y;
    gfdxf_fx lw_elevation;
    char name[GFDXF_MAX_NAME_CHARS];
    gfdxf_fx scale[3];
    gfdxf_fx rotation;
    int columns;
    int rows;
    gfdxf_fx column_spacing;
    gfdxf_fx row_spacing;
    gfdxf_vec3 extrusion;
    int have_extrusion;
    gfdxf_fx radius;
    gfdxf_fx start_angle;
    gfdxf_fx end_angle;
    int spline_flags;
    int spline_degree;
    int spline_ctrl_count;
    int spline_fit_count;
    int spline_knot_count;
    int spline_weight_count;
    int spline_have_ctrl_x;
    int spline_have_fit_x;
    int spline_ctrl_last;
    int spline_fit_last;
    gfdxf_fx spline_tmp_x;
    gfdxf_fx spline_tmp_y;
    char layer[GFDXF_MAX_NAME_CHARS];
    char text[GFDXF_MAX_TEXT_CHARS];
    char text_style[GFDXF_MAX_NAME_CHARS];
    int text_len;
    int is_mtext;
    int dim_type;
    gfdxf_fx text_height;
    gfdxf_fx text_width;
    gfdxf_fx text_rotation;
    gfdxf_fx ellipse_ratio;
    gfdxf_fx ellipse_start;
    gfdxf_fx ellipse_end;
    int hatch_flags;
    int hatch_solid;
    int hatch_loops;
    int hatch_closed;
    int hatch_count;
    int hatch_have_x;
    int hatch_last_index;
    gfdxf_fx hatch_tmp_x;
    gfdxf_fx hatch_elevation;
    gfdxf_fx hatch_bulge_value;
    int hatch_path_is_polyline;
    int hatch_polyline_has_bulge;
    int hatch_edge_type;
    int hatch_edge_emitted;
    int hatch_edge_have_cx;
    int hatch_edge_have_cy;
    int hatch_edge_have_sx;
    int hatch_edge_have_ex;
    int hatch_edge_have_major_x;
    int hatch_edge_have_major_y;
    gfdxf_fx hatch_edge_cx;
    gfdxf_fx hatch_edge_cy;
    gfdxf_fx hatch_edge_sx;
    gfdxf_fx hatch_edge_ex;
    gfdxf_fx hatch_edge_radius;
    gfdxf_fx hatch_edge_start_angle;
    gfdxf_fx hatch_edge_end_angle;
    gfdxf_fx hatch_edge_major_x;
    gfdxf_fx hatch_edge_major_y;
    gfdxf_fx hatch_edge_ratio;
    int hatch_edge_ccw;
    int payload_class_id;
    int payload_graphics_bytes;
    int payload_data_bits;
    int payload_drawing_format;
    int payload_data_format;
    unsigned long payload_checksum;
    int payload_hex_len;
    int payload_hex_total;
    char payload_hex[GFDXF_MAX_PAYLOAD_HEX_CHARS];
    int payload_handle_count;
    char payload_handles[GFDXF_MAX_HANDLES][GFDXF_MAX_NAME_CHARS];
    int acis_line_count;
    int acis_text_chars;
    char acis_head[GFDXF_MAX_ACIS_HEAD_CHARS];
    int acis_tok_body;
    int acis_tok_lump;
    int acis_tok_shell;
    int acis_tok_face;
    int acis_tok_loop;
    int acis_tok_coedge;
    int acis_tok_edge;
    int acis_tok_vertex;
    int acis_tok_surface;
    int acis_tok_curve;
    int mesh_vertex_count;
    int mesh_face_int_count;
    int mesh_read_faces;
    int mesh_subdiv_level;
    int mesh_have_x;
    int mesh_have_y;
    gfdxf_fx mesh_tmp_x;
    gfdxf_fx mesh_tmp_y;
    char object_type[GFDXF_MAX_NAME_CHARS];
    char object_handle[GFDXF_MAX_NAME_CHARS];
    char object_owner[GFDXF_MAX_NAME_CHARS];
    char object_key[GFDXF_MAX_NAME_CHARS];
    char object_first_text[GFDXF_MAX_OBJECT_TEXT_CHARS];
    int object_text_len;
    int object_text_chunks;
    int object_int_values;
    int object_binary_chunks;
} gfdxf_entity;

typedef struct gfdxf_table_state_s {
    int in_table;
    int table_is_layer;
    int in_layer;
    gfdxf_layer cur_layer;
} gfdxf_table_state;

typedef struct gfdxf_class_state_s {
    int in_class;
    gfdxf_class cur_class;
} gfdxf_class_state;

static int gfdxf_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int gfdxf_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static void gfdxf_trim_right(char *s)
{
    int n;
    n = (int)strlen(s);
    while (n > 0 && gfdxf_is_space(s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
}

static const char *gfdxf_skip_space(const char *s)
{
    while (*s != '\0' && gfdxf_is_space(*s)) {
        s++;
    }
    return s;
}

static char gfdxf_up(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

static int gfdxf_str_eq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static int gfdxf_str_eq_i(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (gfdxf_up(*a) != gfdxf_up(*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int gfdxf_contains_i(const char *text, const char *needle)
{
    int i;
    int j;
    if (text == 0 || needle == 0 || needle[0] == '\0') { return 0; }
    for (i = 0; text[i] != '\0'; i++) {
        j = 0;
        while (needle[j] != '\0' && text[i + j] != '\0' && gfdxf_up(text[i + j]) == gfdxf_up(needle[j])) {
            j++;
        }
        if (needle[j] == '\0') { return 1; }
    }
    return 0;
}

static int gfdxf_is_standard_decoded_type(int type)
{
    if (type == GFDXF_ENTITY_LINE || type == GFDXF_ENTITY_3DFACE || type == GFDXF_ENTITY_LWPOLYLINE || type == GFDXF_ENTITY_POLYLINE || type == GFDXF_ENTITY_VERTEX || type == GFDXF_ENTITY_INSERT || type == GFDXF_ENTITY_CIRCLE || type == GFDXF_ENTITY_ARC || type == GFDXF_ENTITY_SPLINE || type == GFDXF_ENTITY_ELLIPSE || type == GFDXF_ENTITY_HATCH || type == GFDXF_ENTITY_DIMENSION || type == GFDXF_ENTITY_TEXT || type == GFDXF_ENTITY_MTEXT || type == GFDXF_ENTITY_POINT || type == GFDXF_ENTITY_SOLID || type == GFDXF_ENTITY_MESH || type == GFDXF_ENTITY_ATTDEF || type == GFDXF_ENTITY_ATTRIB || type == GFDXF_ENTITY_VIEWPORT || type == GFDXF_ENTITY_RAY || type == GFDXF_ENTITY_XLINE || type == GFDXF_ENTITY_MLINE) {
        return 1;
    }
    return 0;
}

static void gfdxf_count_xdata_tag(gfdxf_scene *scene, int code)
{
    if (scene == 0 || code < 1000 || code > 1071) { return; }
    scene->xdata_tags++;
    if (code == 1001) { scene->xdata_app_tags++; }
    else if (code == 1002) { scene->xdata_control_tags++; }
    else if (code == 1003) { scene->xdata_layer_tags++; }
    else if (code == 1004) { scene->xdata_binary_tags++; }
    else if (code == 1005) { scene->xdata_handle_tags++; }
    else if ((code >= 1000 && code <= 1009)) { scene->xdata_string_tags++; }
    else if (code >= 1010 && code <= 1039) { scene->xdata_point_tags++; }
    else if (code >= 1040 && code <= 1059) { scene->xdata_real_tags++; }
    else if (code >= 1060 && code <= 1070) { scene->xdata_int16_tags++; }
    else if (code == 1071) { scene->xdata_int32_tags++; }
}

static void gfdxf_count_acis_tokens(gfdxf_entity *e, const char *value)
{
    if (gfdxf_contains_i(value, "body")) { e->acis_tok_body++; }
    if (gfdxf_contains_i(value, "lump")) { e->acis_tok_lump++; }
    if (gfdxf_contains_i(value, "shell")) { e->acis_tok_shell++; }
    if (gfdxf_contains_i(value, "face")) { e->acis_tok_face++; }
    if (gfdxf_contains_i(value, "loop")) { e->acis_tok_loop++; }
    if (gfdxf_contains_i(value, "coedge")) { e->acis_tok_coedge++; }
    if (gfdxf_contains_i(value, "edge")) { e->acis_tok_edge++; }
    if (gfdxf_contains_i(value, "vertex")) { e->acis_tok_vertex++; }
    if (gfdxf_contains_i(value, "surface") || gfdxf_contains_i(value, "plane") || gfdxf_contains_i(value, "sphere") || gfdxf_contains_i(value, "cone") || gfdxf_contains_i(value, "torus")) { e->acis_tok_surface++; }
    if (gfdxf_contains_i(value, "curve") || gfdxf_contains_i(value, "intcurve") || gfdxf_contains_i(value, "straight")) { e->acis_tok_curve++; }
}

static void gfdxf_copy_name(char *dst, const char *src)
{
    int i;
    i = 0;
    if (dst == 0) {
        return;
    }
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1 < GFDXF_MAX_NAME_CHARS) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void gfdxf_copy_text_cap(char *dst, int cap, const char *src)
{
    int i;
    i = 0;
    if (dst == 0 || cap <= 0) { return; }
    if (src == 0) { dst[0] = '\0'; return; }
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void gfdxf_append_text_cap(char *dst, int cap, int *len, const char *src)
{
    int i;
    if (dst == 0 || cap <= 0 || len == 0 || src == 0) { return; }
    i = 0;
    while (src[i] != '\0' && *len + 1 < cap) {
        dst[*len] = src[i];
        (*len)++;
        i++;
    }
    dst[*len] = '\0';
}

static int gfdxf_is_hex_char(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static unsigned long gfdxf_payload_checksum_step(unsigned long sum, const char *text)
{
    int i;
    unsigned long c;
    i = 0;
    while (text != 0 && text[i] != '\0') {
        c = (unsigned long)(unsigned char)text[i];
        sum = (sum * 131UL) + c;
        i++;
    }
    return sum;
}

static void gfdxf_entity_append_hex(gfdxf_entity *e, const char *value)
{
    int i;
    if (value == 0) { return; }
    i = 0;
    while (value[i] != '\0') {
        if (gfdxf_is_hex_char(value[i])) {
            if (e->payload_hex_len + 1 < GFDXF_MAX_PAYLOAD_HEX_CHARS) {
                e->payload_hex[e->payload_hex_len] = value[i];
                e->payload_hex_len++;
                e->payload_hex[e->payload_hex_len] = '\0';
            }
            e->payload_hex_total++;
        }
        i++;
    }
    e->payload_checksum = gfdxf_payload_checksum_step(e->payload_checksum, value);
}

static void gfdxf_entity_append_handle(gfdxf_entity *e, const char *value)
{
    if (e->payload_handle_count >= GFDXF_MAX_HANDLES) { return; }
    gfdxf_copy_name(e->payload_handles[e->payload_handle_count], value);
    e->payload_handle_count++;
}

static void gfdxf_entity_append_acis_text(gfdxf_entity *e, const char *value)
{
    int len;
    if (value == 0) { return; }
    len = (int)strlen(value);
    if (e->acis_line_count == 0 && e->acis_head[0] == '\0') {
        int dummy;
        dummy = 0;
        gfdxf_append_text_cap(e->acis_head, GFDXF_MAX_ACIS_HEAD_CHARS, &dummy, value);
    }
    e->acis_line_count++;
    e->acis_text_chars += len;
    gfdxf_count_acis_tokens(e, value);
    e->payload_checksum = gfdxf_payload_checksum_step(e->payload_checksum, value);
}

static int gfdxf_parse_int_text(const char *text, int *out_value)
{
    const char *s;
    int sign;
    long value;
    int any;
    s = gfdxf_skip_space(text);
    sign = 1;
    value = 0;
    any = 0;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (gfdxf_is_digit(*s)) {
        value = value * 10L + (long)(*s - '0');
        any = 1;
        s++;
    }
    if (!any) {
        return 0;
    }
    *out_value = (int)(value * (long)sign);
    return 1;
}

static gfdxf_fx gfdxf_fx_abs(gfdxf_fx a)
{
    if (a < 0) {
        return -a;
    }
    return a;
}

static gfdxf_fx gfdxf_fx_mul(gfdxf_fx a, gfdxf_fx b)
{
    return (gfdxf_fx)((a * b) >> GFDXF_FX_SHIFT);
}

static gfdxf_fx gfdxf_fx_div(gfdxf_fx a, gfdxf_fx b)
{
    if (b == 0) {
        return 0;
    }
    return (gfdxf_fx)((a << GFDXF_FX_SHIFT) / b);
}

static gfdxf_fx gfdxf_fx_sqrt(gfdxf_fx x)
{
    gfdxf_fx lo;
    gfdxf_fx hi;
    gfdxf_fx mid;
    gfdxf_fx sq;
    int i;
    if (x <= 0) {
        return 0;
    }
    lo = 0;
    hi = x;
    if (hi < GFDXF_FX_ONE) {
        hi = GFDXF_FX_ONE;
    }
    for (i = 0; i < 28; i++) {
        mid = (lo + hi + 1) / 2;
        sq = gfdxf_fx_mul(mid, mid);
        if (sq <= x) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo;
}

static int gfdxf_fx_to_int_nearest(gfdxf_fx a)
{
    if (a >= 0) {
        return (int)((a + (GFDXF_FX_ONE / 2)) >> GFDXF_FX_SHIFT);
    }
    return -(int)(((-a) + (GFDXF_FX_ONE / 2)) >> GFDXF_FX_SHIFT);
}

static gfdxf_fx gfdxf_q16_to_fx(long value)
{
#if GFDXF_FX_SHIFT > 16
    return (gfdxf_fx)(value << (GFDXF_FX_SHIFT - 16));
#elif GFDXF_FX_SHIFT < 16
    return (gfdxf_fx)(value >> (16 - GFDXF_FX_SHIFT));
#else
    return (gfdxf_fx)value;
#endif
}

static int gfdxf_mod360(int d)
{
    while (d < 0) {
        d += 360;
    }
    while (d >= 360) {
        d -= 360;
    }
    return d;
}

static gfdxf_fx gfdxf_sin_deg_int(int deg)
{
    int d;
    int sign;
    d = gfdxf_mod360(deg);
    sign = 1;
    if (d >= 180) {
        d -= 180;
        sign = -1;
    }
    if (d > 90) {
        d = 180 - d;
    }
    if (sign < 0) {
        return -gfdxf_q16_to_fx(gfdxf_sin90_q16_table[d]);
    }
    return gfdxf_q16_to_fx(gfdxf_sin90_q16_table[d]);
}

static gfdxf_fx gfdxf_cos_deg_int(int deg)
{
    return gfdxf_sin_deg_int(90 - deg);
}

int gfdxf_fx_from_ascii(const char *text, gfdxf_fx *out_value)
{
    const char *s;
    int sign;
    long whole;
    long frac;
    long scale;
    int any;
    int exp_sign;
    int exp_value;
    gfdxf_fx result;
    gfdxf_fx frac_fx;
    int i;
    if (text == 0 || out_value == 0) {
        return 0;
    }
    s = gfdxf_skip_space(text);
    sign = 1;
    whole = 0L;
    frac = 0L;
    scale = 1L;
    any = 0;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (gfdxf_is_digit(*s)) {
        whole = whole * 10L + (long)(*s - '0');
        any = 1;
        s++;
    }
    if (*s == '.') {
        s++;
        while (gfdxf_is_digit(*s)) {
            if (scale < 1000000L) {
                frac = frac * 10L + (long)(*s - '0');
                scale = scale * 10L;
            }
            any = 1;
            s++;
        }
    }
    if (!any) {
        return 0;
    }
    result = (gfdxf_fx)(whole << GFDXF_FX_SHIFT);
    frac_fx = (gfdxf_fx)((frac << GFDXF_FX_SHIFT) / scale);
    result = result + frac_fx;
    if (*s == 'e' || *s == 'E') {
        s++;
        exp_sign = 1;
        exp_value = 0;
        if (*s == '-') {
            exp_sign = -1;
            s++;
        } else if (*s == '+') {
            s++;
        }
        while (gfdxf_is_digit(*s)) {
            exp_value = exp_value * 10 + (int)(*s - '0');
            s++;
        }
        if (exp_sign > 0) {
            for (i = 0; i < exp_value; i++) {
                result = result * 10L;
            }
        } else {
            for (i = 0; i < exp_value; i++) {
                result = result / 10L;
            }
        }
    }
    if (sign < 0) {
        result = -result;
    }
    *out_value = result;
    return 1;
}

void gfdxf_fx_to_ascii(gfdxf_fx value, char *out_text)
{
    long whole;
    long frac;
    int neg;
    gfdxf_fx v;
    if (out_text == 0) {
        return;
    }
    neg = 0;
    v = value;
    if (v < 0) {
        neg = 1;
        v = -v;
    }
    whole = (long)(v >> GFDXF_FX_SHIFT);
    frac = (long)((v & (GFDXF_FX_ONE - 1L)) * 10000L / GFDXF_FX_ONE);
    if (neg) {
        sprintf(out_text, "-%ld.%04ld", whole, frac);
    } else {
        sprintf(out_text, "%ld.%04ld", whole, frac);
    }
}

void gfdxf_scene_init(gfdxf_scene *scene)
{
    if (scene != 0) {
        memset(scene, 0, sizeof(gfdxf_scene));
    }
}

void gfdxf_parser_init(gfdxf_parser *parser)
{
    if (parser != 0) {
        memset(parser, 0, sizeof(gfdxf_parser));
    }
}

int gfdxf_is_binary_dxf_memory(const unsigned char *data, unsigned long length)
{
    static const unsigned char sig[] = {
        'A','u','t','o','C','A','D',' ','B','i','n','a','r','y',' ','D','X','F',13,10,26,0
    };
    unsigned long i;
    if (data == 0) { return GFDXF_BINARY_DXF_NO; }
    if (length < (unsigned long)sizeof(sig)) { return GFDXF_BINARY_DXF_NO; }
    for (i = 0; i < (unsigned long)sizeof(sig); i++) {
        if (data[i] != sig[i]) { return GFDXF_BINARY_DXF_NO; }
    }
    return GFDXF_BINARY_DXF_YES;
}

static void gfdxf_vec_set(gfdxf_vec3 *v, gfdxf_fx x, gfdxf_fx y, gfdxf_fx z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static gfdxf_vec3 gfdxf_vec_cross(gfdxf_vec3 a, gfdxf_vec3 b)
{
    gfdxf_vec3 r;
    r.x = gfdxf_fx_mul(a.y, b.z) - gfdxf_fx_mul(a.z, b.y);
    r.y = gfdxf_fx_mul(a.z, b.x) - gfdxf_fx_mul(a.x, b.z);
    r.z = gfdxf_fx_mul(a.x, b.y) - gfdxf_fx_mul(a.y, b.x);
    return r;
}

static gfdxf_fx gfdxf_vec_dot(gfdxf_vec3 a, gfdxf_vec3 b)
{
    return gfdxf_fx_mul(a.x, b.x) + gfdxf_fx_mul(a.y, b.y) + gfdxf_fx_mul(a.z, b.z);
}

static void gfdxf_vec_normalize(gfdxf_vec3 *v)
{
    gfdxf_fx len2;
    gfdxf_fx len;
    len2 = gfdxf_vec_dot(*v, *v);
    len = gfdxf_fx_sqrt(len2);
    if (len <= 0) {
        v->x = 0;
        v->y = 0;
        v->z = GFDXF_FX_ONE;
    } else {
        v->x = gfdxf_fx_div(v->x, len);
        v->y = gfdxf_fx_div(v->y, len);
        v->z = gfdxf_fx_div(v->z, len);
    }
}

static void gfdxf_xform_identity(gfdxf_xform *xf)
{
    int r;
    int c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            xf->m[r][c] = (r == c) ? GFDXF_FX_ONE : 0;
        }
    }
    xf->t.x = 0;
    xf->t.y = 0;
    xf->t.z = 0;
}

static void gfdxf_xform_apply(const gfdxf_xform *xf, const gfdxf_vec3 *src, gfdxf_vec3 *dst)
{
    dst->x = gfdxf_fx_mul(xf->m[0][0], src->x) + gfdxf_fx_mul(xf->m[0][1], src->y) + gfdxf_fx_mul(xf->m[0][2], src->z) + xf->t.x;
    dst->y = gfdxf_fx_mul(xf->m[1][0], src->x) + gfdxf_fx_mul(xf->m[1][1], src->y) + gfdxf_fx_mul(xf->m[1][2], src->z) + xf->t.y;
    dst->z = gfdxf_fx_mul(xf->m[2][0], src->x) + gfdxf_fx_mul(xf->m[2][1], src->y) + gfdxf_fx_mul(xf->m[2][2], src->z) + xf->t.z;
}

static void gfdxf_xform_compose(const gfdxf_xform *a, const gfdxf_xform *b, gfdxf_xform *out)
{
    gfdxf_xform r;
    int i;
    int j;
    int k;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            r.m[i][j] = 0;
            for (k = 0; k < 3; k++) {
                r.m[i][j] += gfdxf_fx_mul(a->m[i][k], b->m[k][j]);
            }
        }
    }
    gfdxf_xform_apply(a, &b->t, &r.t);
    *out = r;
}

static void gfdxf_ocs_basis(const gfdxf_vec3 *extrusion, gfdxf_vec3 *ax, gfdxf_vec3 *ay, gfdxf_vec3 *az)
{
    gfdxf_vec3 n;
    gfdxf_vec3 ref;
    n = *extrusion;
    gfdxf_vec_normalize(&n);
    *az = n;
    if (gfdxf_fx_abs(n.x) < (GFDXF_FX_ONE / 64) && gfdxf_fx_abs(n.y) < (GFDXF_FX_ONE / 64)) {
        gfdxf_vec_set(&ref, 0, GFDXF_FX_ONE, 0);
    } else {
        gfdxf_vec_set(&ref, 0, 0, GFDXF_FX_ONE);
    }
    *ax = gfdxf_vec_cross(ref, n);
    gfdxf_vec_normalize(ax);
    *ay = gfdxf_vec_cross(n, *ax);
    gfdxf_vec_normalize(ay);
}

static void gfdxf_ocs_to_parent(const gfdxf_vec3 *p, const gfdxf_vec3 *ext, gfdxf_vec3 *dst)
{
    gfdxf_vec3 ax;
    gfdxf_vec3 ay;
    gfdxf_vec3 az;
    gfdxf_ocs_basis(ext, &ax, &ay, &az);
    dst->x = gfdxf_fx_mul(p->x, ax.x) + gfdxf_fx_mul(p->y, ay.x) + gfdxf_fx_mul(p->z, az.x);
    dst->y = gfdxf_fx_mul(p->x, ax.y) + gfdxf_fx_mul(p->y, ay.y) + gfdxf_fx_mul(p->z, az.y);
    dst->z = gfdxf_fx_mul(p->x, ax.z) + gfdxf_fx_mul(p->y, ay.z) + gfdxf_fx_mul(p->z, az.z);
}

static void gfdxf_entity_point_raw(const gfdxf_entity *e, int n, gfdxf_vec3 *v)
{
    v->x = e->p[n][0];
    v->y = e->p[n][1];
    v->z = e->p[n][2];
}

static void gfdxf_entity_point_ocs(const gfdxf_entity *e, int n, gfdxf_vec3 *v)
{
    gfdxf_vec3 p;
    gfdxf_entity_point_raw(e, n, &p);
    gfdxf_ocs_to_parent(&p, &e->extrusion, v);
}

void gfdxf_scene_compute_aabb(gfdxf_scene *scene, gfdxf_aabb *out_box)
{
    int i;
    gfdxf_aabb box;
    memset(&box, 0, sizeof(box));
    if (scene != 0 && scene->vertex_count > 0) {
        box.valid = 1;
        box.min_v = scene->vertices[0];
        box.max_v = scene->vertices[0];
        for (i = 1; i < scene->vertex_count; i++) {
            if (scene->vertices[i].x < box.min_v.x) { box.min_v.x = scene->vertices[i].x; }
            if (scene->vertices[i].y < box.min_v.y) { box.min_v.y = scene->vertices[i].y; }
            if (scene->vertices[i].z < box.min_v.z) { box.min_v.z = scene->vertices[i].z; }
            if (scene->vertices[i].x > box.max_v.x) { box.max_v.x = scene->vertices[i].x; }
            if (scene->vertices[i].y > box.max_v.y) { box.max_v.y = scene->vertices[i].y; }
            if (scene->vertices[i].z > box.max_v.z) { box.max_v.z = scene->vertices[i].z; }
        }
    }
    if (out_box != 0) { *out_box = box; }
    if (scene != 0) { scene->kernel.bounds = box; }
}

static gfdxf_fx gfdxf_dist3_approx(gfdxf_vec3 a, gfdxf_vec3 b)
{
    gfdxf_fx dx;
    gfdxf_fx dy;
    gfdxf_fx dz;
    dx = a.x - b.x;
    dy = a.y - b.y;
    dz = a.z - b.z;
    return gfdxf_fx_sqrt(gfdxf_fx_mul(dx, dx) + gfdxf_fx_mul(dy, dy) + gfdxf_fx_mul(dz, dz));
}

static gfdxf_fx gfdxf_face_area_xy(const gfdxf_scene *scene, const gfdxf_face *fc)
{
    int i;
    int j;
    int a;
    int b;
    gfdxf_fx sum;
    if (fc->count < 3) { return 0; }
    sum = 0;
    for (i = 0; i < fc->count; i++) {
        j = (i + 1) % fc->count;
        a = fc->v[i];
        b = fc->v[j];
        if (a < 0 || b < 0 || a >= scene->vertex_count || b >= scene->vertex_count) { return 0; }
        sum += gfdxf_fx_mul(scene->vertices[a].x, scene->vertices[b].y) - gfdxf_fx_mul(scene->vertices[b].x, scene->vertices[a].y);
    }
    if (sum < 0) { sum = -sum; }
    return sum / 2;
}


static void gfdxf_edge_norm(int a, int b, int *oa, int *ob)
{
    if (a <= b) { *oa = a; *ob = b; }
    else { *oa = b; *ob = a; }
}

static int gfdxf_face_edge_count(const gfdxf_face *fc)
{
    if (fc == 0 || fc->count < 3) { return 0; }
    if (fc->count > 4) { return 4; }
    return fc->count;
}

static int gfdxf_count_matching_face_edges(const gfdxf_scene *scene, int face_i, int edge_i, int ea, int eb)
{
    int i;
    int j;
    int cnt;
    int n;
    int a;
    int b;
    int na;
    int nb;
    const gfdxf_face *fc;
    (void)edge_i;
    cnt = 0;
    for (i = 0; i < scene->face_count; i++) {
        fc = &scene->faces[i];
        n = gfdxf_face_edge_count(fc);
        for (j = 0; j < n; j++) {
            a = fc->v[j];
            b = fc->v[(j + 1) % n];
            gfdxf_edge_norm(a, b, &na, &nb);
            if (na == ea && nb == eb) {
                cnt++;
            }
        }
    }
    (void)face_i;
    return cnt;
}

void gfdxf_kernel_analyze_scene(gfdxf_scene *scene)
{
    int i;
    int j;
    int n;
    int a;
    int b;
    int ea;
    int eb;
    int matches;
    gfdxf_fx len;
    if (scene == 0) { return; }
    scene->kernel.wire_length_approx = 0;
    scene->kernel.face_area_xy_approx = 0;
    scene->kernel.degenerate_lines = 0;
    scene->kernel.degenerate_faces = 0;
    scene->kernel.open_edge_estimate = 0;
    scene->kernel.boundary_edges = 0;
    scene->kernel.nonmanifold_edges = 0;
    gfdxf_scene_compute_aabb(scene, &scene->kernel.bounds);
    for (i = 0; i < scene->line_count; i++) {
        a = scene->lines[i].a;
        b = scene->lines[i].b;
        if (a < 0 || b < 0 || a >= scene->vertex_count || b >= scene->vertex_count || a == b) {
            scene->kernel.degenerate_lines++;
        } else {
            len = gfdxf_dist3_approx(scene->vertices[a], scene->vertices[b]);
            if (len == 0) { scene->kernel.degenerate_lines++; }
            scene->kernel.wire_length_approx += len;
        }
    }
    for (i = 0; i < scene->face_count; i++) {
        len = gfdxf_face_area_xy(scene, &scene->faces[i]);
        if (len == 0) { scene->kernel.degenerate_faces++; }
        scene->kernel.face_area_xy_approx += len;
        n = gfdxf_face_edge_count(&scene->faces[i]);
        for (j = 0; j < n; j++) {
            a = scene->faces[i].v[j];
            b = scene->faces[i].v[(j + 1) % n];
            if (a < 0 || b < 0 || a >= scene->vertex_count || b >= scene->vertex_count || a == b) {
                continue;
            }
            gfdxf_edge_norm(a, b, &ea, &eb);
            matches = gfdxf_count_matching_face_edges(scene, i, j, ea, eb);
            if (matches == 1) { scene->kernel.boundary_edges++; }
            else if (matches > 2) { scene->kernel.nonmanifold_edges++; }
        }
    }
    scene->kernel.open_edge_estimate = scene->line_count + scene->kernel.boundary_edges;
    scene->kernel_analyzed = 1;
}

int gfdxf_scene_validate_indices(const gfdxf_scene *scene, int *bad_lines, int *bad_faces)
{
    int i;
    int bad_l;
    int bad_f;
    bad_l = 0;
    bad_f = 0;
    if (scene == 0) {
        if (bad_lines != 0) { *bad_lines = 0; }
        if (bad_faces != 0) { *bad_faces = 0; }
        return 0;
    }
    for (i = 0; i < scene->line_count; i++) {
        if (scene->lines[i].a < 0 || scene->lines[i].b < 0 || scene->lines[i].a >= scene->vertex_count || scene->lines[i].b >= scene->vertex_count) {
            bad_l++;
        }
    }
    for (i = 0; i < scene->face_count; i++) {
        int j;
        int n;
        n = scene->faces[i].count;
        if (n < 3 || n > 4) { bad_f++; continue; }
        for (j = 0; j < n; j++) {
            if (scene->faces[i].v[j] < 0 || scene->faces[i].v[j] >= scene->vertex_count) {
                bad_f++;
                break;
            }
        }
    }
    if (bad_lines != 0) { *bad_lines = bad_l; }
    if (bad_faces != 0) { *bad_faces = bad_f; }
    return bad_l + bad_f;
}

static int gfdxf_add_vertex_scene(gfdxf_emit_ctx *ctx, gfdxf_fx x, gfdxf_fx y, gfdxf_fx z, int *out_index)
{
    gfdxf_scene *scene;
    int i;
    int idx;
    scene = ctx->scene;
#if GFDXF_MERGE_VERTICES
    for (i = 0; i < scene->vertex_count; i++) {
        if (scene->vertices[i].x == x && scene->vertices[i].y == y && scene->vertices[i].z == z) {
            scene->duplicate_vertices++;
            if (out_index != 0) {
                *out_index = i;
            }
            return 1;
        }
    }
#endif
    if (scene->vertex_count >= GFDXF_MAX_VERTICES) {
        scene->capacity_errors++;
        return 0;
    }
    idx = scene->vertex_count;
    scene->vertices[idx].x = x;
    scene->vertices[idx].y = y;
    scene->vertices[idx].z = z;
    scene->vertex_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    if (ctx->callbacks != 0 && ctx->callbacks->on_vertex != 0) {
        ctx->callbacks->on_vertex(ctx->user, idx, &scene->vertices[idx]);
    }
    return 1;
}

static int gfdxf_add_vertex_block(gfdxf_emit_ctx *ctx, gfdxf_fx x, gfdxf_fx y, gfdxf_fx z, int *out_index)
{
    gfdxf_parser *parser;
    int idx;
    parser = ctx->parser;
    if (ctx->block_index < 0 || ctx->block_index >= parser->block_count) {
        ctx->scene->capacity_errors++;
        return 0;
    }
    if (parser->block_vertex_count >= GFDXF_MAX_BLOCK_VERTICES) {
        ctx->scene->capacity_errors++;
        return 0;
    }
    idx = parser->block_vertex_count;
    parser->block_vertices[idx].x = x;
    parser->block_vertices[idx].y = y;
    parser->block_vertices[idx].z = z;
    parser->block_vertex_count++;
    parser->blocks[ctx->block_index].vertex_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    return 1;
}

static int gfdxf_add_vertex_target(gfdxf_emit_ctx *ctx, const gfdxf_vec3 *src, int *out_index)
{
    gfdxf_vec3 v;
    gfdxf_xform_apply(&ctx->xf, src, &v);
    if (ctx->target == GFDXF_TARGET_BLOCK) {
        return gfdxf_add_vertex_block(ctx, v.x, v.y, v.z, out_index);
    }
    return gfdxf_add_vertex_scene(ctx, v.x, v.y, v.z, out_index);
}

static int gfdxf_add_line_scene(gfdxf_emit_ctx *ctx, int a, int b)
{
    gfdxf_scene *scene;
    int idx;
    scene = ctx->scene;
    if (a < 0 || b < 0 || a == b) {
        return 1;
    }
    if (scene->line_count >= GFDXF_MAX_LINES) {
        scene->capacity_errors++;
        return 0;
    }
    idx = scene->line_count;
    scene->lines[idx].a = a;
    scene->lines[idx].b = b;
    scene->line_count++;
    if (ctx->callbacks != 0 && ctx->callbacks->on_line != 0) {
        ctx->callbacks->on_line(ctx->user, idx, a, b);
    }
    return 1;
}

static int gfdxf_add_line_block(gfdxf_emit_ctx *ctx, int a, int b)
{
    gfdxf_parser *parser;
    int idx;
    parser = ctx->parser;
    if (a < 0 || b < 0 || a == b) {
        return 1;
    }
    if (ctx->block_index < 0 || ctx->block_index >= parser->block_count) {
        ctx->scene->capacity_errors++;
        return 0;
    }
    if (parser->block_line_count >= GFDXF_MAX_BLOCK_LINES) {
        ctx->scene->capacity_errors++;
        return 0;
    }
    idx = parser->block_line_count;
    parser->block_lines[idx].a = a;
    parser->block_lines[idx].b = b;
    parser->block_line_count++;
    parser->blocks[ctx->block_index].line_count++;
    return 1;
}

static int gfdxf_add_line_target(gfdxf_emit_ctx *ctx, int a, int b)
{
    if (ctx->target == GFDXF_TARGET_BLOCK) {
        return gfdxf_add_line_block(ctx, a, b);
    }
    return gfdxf_add_line_scene(ctx, a, b);
}

static int gfdxf_add_face_scene(gfdxf_emit_ctx *ctx, int a, int b, int c, int d, int count, int invisible_edges)
{
    gfdxf_scene *scene;
    int idx;
    scene = ctx->scene;
    if (scene->face_count >= GFDXF_MAX_FACES) {
        scene->capacity_errors++;
        return 0;
    }
    idx = scene->face_count;
    scene->faces[idx].v[0] = a;
    scene->faces[idx].v[1] = b;
    scene->faces[idx].v[2] = c;
    scene->faces[idx].v[3] = d;
    scene->faces[idx].count = count;
    scene->faces[idx].invisible_edges = invisible_edges;
    scene->face_count++;
    if (ctx->callbacks != 0 && ctx->callbacks->on_face != 0) {
        ctx->callbacks->on_face(ctx->user, idx, scene->faces[idx].v, count, invisible_edges);
    }
    return 1;
}

static int gfdxf_add_face_block(gfdxf_emit_ctx *ctx, int a, int b, int c, int d, int count, int invisible_edges)
{
    gfdxf_parser *parser;
    int idx;
    parser = ctx->parser;
    if (ctx->block_index < 0 || ctx->block_index >= parser->block_count) {
        ctx->scene->capacity_errors++;
        return 0;
    }
    if (parser->block_face_count >= GFDXF_MAX_BLOCK_FACES) {
        ctx->scene->capacity_errors++;
        return 0;
    }
    idx = parser->block_face_count;
    parser->block_faces[idx].v[0] = a;
    parser->block_faces[idx].v[1] = b;
    parser->block_faces[idx].v[2] = c;
    parser->block_faces[idx].v[3] = d;
    parser->block_faces[idx].count = count;
    parser->block_faces[idx].invisible_edges = invisible_edges;
    parser->block_face_count++;
    parser->blocks[ctx->block_index].face_count++;
    return 1;
}

static int gfdxf_add_face_target(gfdxf_emit_ctx *ctx, int a, int b, int c, int d, int count, int invisible_edges)
{
    if (ctx->target == GFDXF_TARGET_BLOCK) {
        return gfdxf_add_face_block(ctx, a, b, c, d, count, invisible_edges);
    }
    return gfdxf_add_face_scene(ctx, a, b, c, d, count, invisible_edges);
}

static int gfdxf_read_line(const char *text, unsigned long length, unsigned long *pos, char *out, unsigned long *line_no)
{
    int i;
    if (*pos >= length) {
        return 0;
    }
    i = 0;
    while (*pos < length && text[*pos] != '\n') {
        if (i + 1 < GFDXF_MAX_LINE_CHARS) {
            out[i] = text[*pos];
            i++;
        }
        (*pos)++;
    }
    if (*pos < length && text[*pos] == '\n') {
        (*pos)++;
    }
    out[i] = '\0';
    gfdxf_trim_right(out);
    (*line_no)++;
    return 1;
}

static unsigned int gfdxf_u16_le(const unsigned char *p)
{
    return ((unsigned int)p[0]) | (((unsigned int)p[1]) << 8);
}

static unsigned long gfdxf_u32_le(const unsigned char *p)
{
    return ((unsigned long)p[0]) | (((unsigned long)p[1]) << 8) | (((unsigned long)p[2]) << 16) | (((unsigned long)p[3]) << 24);
}

static long gfdxf_s16_le(const unsigned char *p)
{
    unsigned int u;
    u = gfdxf_u16_le(p);
    if (u >= 32768U) {
        return -((long)(65536U - u));
    }
    return (long)u;
}

static long gfdxf_s32_le(const unsigned char *p)
{
    unsigned long u;
    u = gfdxf_u32_le(p);
    if (u >= 2147483648UL) {
        return -((long)(4294967295UL - u) + 1L);
    }
    return (long)u;
}

static void gfdxf_hex_from_bytes(const unsigned char *src, unsigned long n, char *dst, int cap)
{
    static const char hex[] = "0123456789ABCDEF";
    unsigned long i;
    int j;
    if (cap <= 0) { return; }
    j = 0;
    for (i = 0; i < n && j + 2 < cap; i++) {
        dst[j++] = hex[(src[i] >> 4) & 15U];
        dst[j++] = hex[src[i] & 15U];
    }
    dst[j] = '\0';
}

static int gfdxf_code_is_string(int code)
{
    if (code >= 0 && code <= 9) { return 1; }
    if (code == 100 || code == 102 || code == 105) { return 1; }
    if (code >= 300 && code <= 309) { return 1; }
    if (code >= 320 && code <= 369) { return 1; }
    if (code >= 390 && code <= 399) { return 1; }
    if (code >= 410 && code <= 419) { return 1; }
    if (code >= 430 && code <= 439) { return 1; }
    if (code >= 470 && code <= 481) { return 1; }
    if (code == 999) { return 1; }
    if (code >= 1000 && code <= 1003) { return 1; }
    if (code == 1005) { return 1; }
    return 0;
}

static int gfdxf_code_is_real64(int code)
{
    if (code >= 10 && code <= 59) { return 1; }
    if (code >= 110 && code <= 149) { return 1; }
    if (code >= 210 && code <= 239) { return 1; }
    if (code >= 460 && code <= 469) { return 1; }
    if (code >= 1010 && code <= 1059) { return 1; }
    return 0;
}

static int gfdxf_code_is_i16(int code)
{
    if (code >= 60 && code <= 79) { return 1; }
    if (code >= 170 && code <= 179) { return 1; }
    if (code >= 270 && code <= 299) { return 1; }
    if (code >= 370 && code <= 389) { return 1; }
    if (code >= 400 && code <= 409) { return 1; }
    if (code == 1070) { return 1; }
    return 0;
}

static int gfdxf_code_is_i32(int code)
{
    if (code >= 90 && code <= 99) { return 1; }
    if (code >= 420 && code <= 429) { return 1; }
    if (code >= 440 && code <= 459) { return 1; }
    if (code == 1071) { return 1; }
    return 0;
}

static int gfdxf_code_is_i64(int code)
{
    return code >= 160 && code <= 169;
}

static int gfdxf_code_is_binary_chunk(int code)
{
    if (code >= 310 && code <= 319) { return 1; }
    if (code == 1004) { return 1; }
    return 0;
}

static void gfdxf_i64ish_to_text(const unsigned char *p, char *out, int cap)
{
    long lo;
    long hi;
    if (cap <= 0) { return; }
    lo = gfdxf_s32_le(p);
    hi = gfdxf_s32_le(p + 4);
    if (hi == 0L) {
        sprintf(out, "%ld", lo);
    } else if (hi == -1L && lo < 0L) {
        sprintf(out, "%ld", lo);
    } else {
        /* Portable C89 fallback: preserve high/low halves instead of using 64-bit arithmetic. */
        sprintf(out, "%ld", lo);
    }
}

static gfdxf_fx gfdxf_fx_from_real64_le(const unsigned char *p)
{
    int sign;
    int exp;
    unsigned long frac_top;
    unsigned long q30;
    int shift;
    gfdxf_fx value;
    sign = (p[7] & 0x80U) ? -1 : 1;
    exp = (((int)(p[7] & 0x7FU)) << 4) | ((int)(p[6] >> 4));
    if (exp == 0) { return 0; }
    if (exp >= 2047) {
        return sign < 0 ? (gfdxf_fx)(-2147483647L) : (gfdxf_fx)2147483647L;
    }
    frac_top = (((unsigned long)(p[6] & 0x0FU)) << 26) | (((unsigned long)p[5]) << 18) | (((unsigned long)p[4]) << 10) | (((unsigned long)p[3]) << 2) | (((unsigned long)p[2]) >> 6);
    q30 = (1UL << 30) | frac_top;
    shift = exp - 1023 - 30 + GFDXF_FX_SHIFT;
    if (shift >= 0) {
        if (shift > 20) {
            value = (gfdxf_fx)2147483647L;
        } else {
            value = (gfdxf_fx)(q30 << shift);
        }
    } else {
        if (shift < -31) {
            value = 0;
        } else {
            value = (gfdxf_fx)(q30 >> (-shift));
        }
    }
    return sign < 0 ? -value : value;
}

static int gfdxf_read_binary_string(const unsigned char *data, unsigned long length, gfdxf_parser *parser, char *out, int cap)
{
    int i;
    i = 0;
    while (parser->pos < length && data[parser->pos] != 0) {
        if (i + 1 < cap) {
            out[i++] = (char)data[parser->pos];
        }
        parser->pos++;
    }
    if (parser->pos >= length) {
        parser->last_error = GFDXF_ERR_TRUNCATED_TAG;
        return 0;
    }
    parser->pos++;
    out[i] = '\0';
    return 1;
}

static int gfdxf_read_binary_tag(const char *text, unsigned long length, gfdxf_parser *parser, int *code, const char **value)
{
    const unsigned char *data;
    unsigned long n;
    gfdxf_fx fx;
    int c;
    data = (const unsigned char *)text;
    if (parser->pos >= length) { return 0; }
    if (parser->binary_group_code_width <= 0) { parser->binary_group_code_width = 2; }
    if (parser->pos + (unsigned long)parser->binary_group_code_width > length) {
        parser->last_error = GFDXF_ERR_TRUNCATED_TAG;
        return 0;
    }
    if (parser->binary_group_code_width == 1) {
        c = (int)data[parser->pos];
        parser->pos++;
    } else {
        c = (int)gfdxf_u16_le(data + parser->pos);
        parser->pos += 2UL;
    }
    if (c == 255) {
        if (parser->pos + 2UL > length) { parser->last_error = GFDXF_ERR_TRUNCATED_TAG; return 0; }
        c = (int)gfdxf_u16_le(data + parser->pos);
        parser->pos += 2UL;
    }
    *code = c;
    sprintf(parser->line, "%d", c);
    if (gfdxf_code_is_string(c)) {
        if (!gfdxf_read_binary_string(data, length, parser, parser->value, GFDXF_MAX_LINE_CHARS)) { return 0; }
    } else if (gfdxf_code_is_real64(c)) {
        if (parser->pos + 8UL > length) { parser->last_error = GFDXF_ERR_TRUNCATED_TAG; return 0; }
        fx = gfdxf_fx_from_real64_le(data + parser->pos);
        parser->pos += 8UL;
        gfdxf_fx_to_ascii(fx, parser->value);
    } else if (gfdxf_code_is_i16(c)) {
        if (parser->pos + 2UL > length) { parser->last_error = GFDXF_ERR_TRUNCATED_TAG; return 0; }
        sprintf(parser->value, "%ld", gfdxf_s16_le(data + parser->pos));
        parser->pos += 2UL;
    } else if (gfdxf_code_is_i32(c)) {
        if (parser->pos + 4UL > length) { parser->last_error = GFDXF_ERR_TRUNCATED_TAG; return 0; }
        sprintf(parser->value, "%ld", gfdxf_s32_le(data + parser->pos));
        parser->pos += 4UL;
    } else if (gfdxf_code_is_i64(c)) {
        if (parser->pos + 8UL > length) { parser->last_error = GFDXF_ERR_TRUNCATED_TAG; return 0; }
        gfdxf_i64ish_to_text(data + parser->pos, parser->value, GFDXF_MAX_LINE_CHARS);
        parser->pos += 8UL;
    } else if (gfdxf_code_is_binary_chunk(c)) {
        if (parser->pos + 1UL > length) { parser->last_error = GFDXF_ERR_TRUNCATED_TAG; return 0; }
        n = (unsigned long)data[parser->pos];
        parser->pos++;
        if (parser->pos + n > length) { parser->last_error = GFDXF_ERR_TRUNCATED_TAG; return 0; }
        gfdxf_hex_from_bytes(data + parser->pos, n, parser->value, GFDXF_MAX_LINE_CHARS);
        parser->pos += n;
    } else {
        if (!gfdxf_read_binary_string(data, length, parser, parser->value, GFDXF_MAX_LINE_CHARS)) { return 0; }
    }
    parser->line_no++;
    *value = parser->value;
    return 1;
}

static int gfdxf_read_tag(const char *text, unsigned long length, gfdxf_parser *parser, int *code, const char **value)
{
    if (parser->binary_mode) {
        return gfdxf_read_binary_tag(text, length, parser, code, value);
    }
    if (!gfdxf_read_line(text, length, &parser->pos, parser->line, &parser->line_no)) {
        return 0;
    }
    if (!gfdxf_read_line(text, length, &parser->pos, parser->value, &parser->line_no)) {
        parser->last_error = GFDXF_ERR_TRUNCATED_TAG;
        return 0;
    }
    if (!gfdxf_parse_int_text(parser->line, code)) {
        parser->last_error = GFDXF_ERR_BAD_NUMBER;
        return 0;
    }
    *value = parser->value;
    return 1;
}

static void gfdxf_entity_clear(gfdxf_entity *e)
{
    memset(e, 0, sizeof(gfdxf_entity));
    e->scale[0] = GFDXF_FX_ONE;
    e->scale[1] = GFDXF_FX_ONE;
    e->scale[2] = GFDXF_FX_ONE;
    e->columns = 1;
    e->rows = 1;
    e->extrusion.x = 0;
    e->extrusion.y = 0;
    e->extrusion.z = GFDXF_FX_ONE;
    e->lw_last_index = -1;
    e->spline_degree = 3;
    e->spline_ctrl_last = -1;
    e->spline_fit_last = -1;
    e->text_height = GFDXF_FX_ONE;
    e->text_width = 0;
    e->ellipse_ratio = GFDXF_FX_ONE;
    e->ellipse_start = 0;
    e->ellipse_end = (gfdxf_fx)360L << GFDXF_FX_SHIFT;
    e->hatch_last_index = -1;
    e->hatch_closed = 1;
    e->hatch_edge_ccw = 1;
    e->hatch_edge_ratio = GFDXF_FX_ONE;
}

static int gfdxf_entity_type_from_name(const char *name)
{
    if (gfdxf_str_eq_i(name, "LINE")) {
        return GFDXF_ENTITY_LINE;
    }
    if (gfdxf_str_eq_i(name, "3DFACE")) {
        return GFDXF_ENTITY_3DFACE;
    }
    if (gfdxf_str_eq_i(name, "LWPOLYLINE")) {
        return GFDXF_ENTITY_LWPOLYLINE;
    }
    if (gfdxf_str_eq_i(name, "POLYLINE")) {
        return GFDXF_ENTITY_POLYLINE;
    }
    if (gfdxf_str_eq_i(name, "VERTEX")) {
        return GFDXF_ENTITY_VERTEX;
    }
    if (gfdxf_str_eq_i(name, "INSERT") || gfdxf_str_eq_i(name, "MINSERT")) {
        return GFDXF_ENTITY_INSERT;
    }
    if (gfdxf_str_eq_i(name, "CIRCLE")) {
        return GFDXF_ENTITY_CIRCLE;
    }
    if (gfdxf_str_eq_i(name, "ARC")) {
        return GFDXF_ENTITY_ARC;
    }
    if (gfdxf_str_eq_i(name, "SPLINE")) {
        return GFDXF_ENTITY_SPLINE;
    }
    if (gfdxf_str_eq_i(name, "ELLIPSE")) {
        return GFDXF_ENTITY_ELLIPSE;
    }
    if (gfdxf_str_eq_i(name, "HATCH")) {
        return GFDXF_ENTITY_HATCH;
    }
    if (gfdxf_str_eq_i(name, "DIMENSION")) {
        return GFDXF_ENTITY_DIMENSION;
    }
    if (gfdxf_str_eq_i(name, "TEXT")) {
        return GFDXF_ENTITY_TEXT;
    }
    if (gfdxf_str_eq_i(name, "MTEXT")) {
        return GFDXF_ENTITY_MTEXT;
    }
    if (gfdxf_str_eq_i(name, "POINT")) {
        return GFDXF_ENTITY_POINT;
    }
    if (gfdxf_str_eq_i(name, "SOLID") || gfdxf_str_eq_i(name, "TRACE")) {
        return GFDXF_ENTITY_SOLID;
    }
    if (gfdxf_str_eq_i(name, "ACAD_PROXY_ENTITY")) {
        return GFDXF_ENTITY_PROXY;
    }
    if (gfdxf_str_eq_i(name, "ACAD_PROXY_OBJECT")) {
        return GFDXF_ENTITY_PROXY_OBJECT;
    }
    if (gfdxf_str_eq_i(name, "3DSOLID") || gfdxf_str_eq_i(name, "BODY") || gfdxf_str_eq_i(name, "REGION") || gfdxf_str_eq_i(name, "SURFACE")) {
        return GFDXF_ENTITY_ACIS;
    }
    if (gfdxf_str_eq_i(name, "MESH") || gfdxf_str_eq_i(name, "POLYFACE")) {
        return GFDXF_ENTITY_MESH;
    }
    if (gfdxf_str_eq_i(name, "IMAGE") || gfdxf_str_eq_i(name, "UNDERLAY") || gfdxf_str_eq_i(name, "WIPEOUT") || gfdxf_str_eq_i(name, "PDFUNDERLAY") || gfdxf_str_eq_i(name, "DGNUNDERLAY") || gfdxf_str_eq_i(name, "DWFUNDERLAY")) {
        return GFDXF_ENTITY_IMAGE;
    }
    if (gfdxf_str_eq_i(name, "COORDINATIONMODEL")) {
        return GFDXF_ENTITY_COORDINATION_MODEL;
    }
    if (gfdxf_str_eq_i(name, "OLE2FRAME") || gfdxf_str_eq_i(name, "OLEFRAME")) {
        return GFDXF_ENTITY_OLE;
    }
    if (gfdxf_str_eq_i(name, "GEODATA")) {
        return GFDXF_ENTITY_GEODATA;
    }
    if (gfdxf_str_eq_i(name, "POINTCLOUD") || gfdxf_str_eq_i(name, "POINTCLOUD2") || gfdxf_str_eq_i(name, "POINTCLOUDEX")) {
        return GFDXF_ENTITY_POINTCLOUD;
    }
    if (gfdxf_str_eq_i(name, "ASSOC2DCONSTRAINTGROUP") || gfdxf_str_eq_i(name, "ASSOCDEPENDENCY") || gfdxf_str_eq_i(name, "ASSOCACTION") || gfdxf_str_eq_i(name, "ASSOCGEOMDEPENDENCY")) {
        return GFDXF_ENTITY_CONSTRAINT;
    }
    if (gfdxf_str_eq_i(name, "PLANESURFACE") || gfdxf_str_eq_i(name, "EXTRUDEDSURFACE") || gfdxf_str_eq_i(name, "LOFTEDSURFACE") || gfdxf_str_eq_i(name, "REVOLVEDSURFACE") || gfdxf_str_eq_i(name, "SWEPTSURFACE") || gfdxf_str_eq_i(name, "NURBSURFACE")) {
        return GFDXF_ENTITY_ACIS;
    }
    if (gfdxf_str_eq_i(name, "LEADER")) {
        return GFDXF_ENTITY_LEADER;
    }
    if (gfdxf_str_eq_i(name, "MLEADER")) {
        return GFDXF_ENTITY_MLEADER;
    }
    if (gfdxf_str_eq_i(name, "RAY")) {
        return GFDXF_ENTITY_RAY;
    }
    if (gfdxf_str_eq_i(name, "XLINE")) {
        return GFDXF_ENTITY_XLINE;
    }
    if (gfdxf_str_eq_i(name, "ATTDEF")) {
        return GFDXF_ENTITY_ATTDEF;
    }
    if (gfdxf_str_eq_i(name, "ATTRIB")) {
        return GFDXF_ENTITY_ATTRIB;
    }
    if (gfdxf_str_eq_i(name, "VIEWPORT")) {
        return GFDXF_ENTITY_VIEWPORT;
    }
    if (gfdxf_str_eq_i(name, "SHAPE")) {
        return GFDXF_ENTITY_SHAPE;
    }
    if (gfdxf_str_eq_i(name, "TOLERANCE")) {
        return GFDXF_ENTITY_TOLERANCE;
    }
    if (gfdxf_str_eq_i(name, "MLINE")) {
        return GFDXF_ENTITY_MLINE;
    }
    if (gfdxf_str_eq_i(name, "HELIX")) {
        return GFDXF_ENTITY_HELIX;
    }
    if (gfdxf_str_eq_i(name, "LIGHT")) {
        return GFDXF_ENTITY_LIGHT;
    }
    if (gfdxf_str_eq_i(name, "SECTION")) {
        return GFDXF_ENTITY_SECTION;
    }
    if (gfdxf_str_eq_i(name, "DICTIONARY") || gfdxf_str_eq_i(name, "ACDBDICTIONARYWDFLT")) {
        return GFDXF_ENTITY_DICTIONARY;
    }
    if (gfdxf_str_eq_i(name, "XRECORD")) {
        return GFDXF_ENTITY_XRECORD;
    }
    if (gfdxf_str_eq_i(name, "DICTIONARYVAR")) {
        return GFDXF_ENTITY_DICTIONARYVAR;
    }
    if (gfdxf_str_eq_i(name, "LAYOUT") || gfdxf_str_eq_i(name, "PLOTSETTINGS") || gfdxf_str_eq_i(name, "MATERIAL") || gfdxf_str_eq_i(name, "VISUALSTYLE") || gfdxf_str_eq_i(name, "MLEADERSTYLE") || gfdxf_str_eq_i(name, "FIELD") || gfdxf_str_eq_i(name, "GROUP") || gfdxf_str_eq_i(name, "IDBUFFER") || gfdxf_str_eq_i(name, "RASTERVARIABLES") || gfdxf_str_eq_i(name, "IMAGEDEF") || gfdxf_str_eq_i(name, "IMAGEDEF_REACTOR") || gfdxf_str_eq_i(name, "SPATIAL_FILTER") || gfdxf_str_eq_i(name, "ACDBPLACEHOLDER")) {
        return GFDXF_ENTITY_OBJECT_OTHER;
    }
    if (gfdxf_str_eq_i(name, "TABLE")) {
        return GFDXF_ENTITY_TABLE;
    }
    return GFDXF_ENTITY_OTHER;
}

static void gfdxf_store_point_value(gfdxf_scene *scene, gfdxf_entity *e, int code, const char *value)
{
    int point;
    int axis;
    gfdxf_fx fx;
    if (code >= 10 && code <= 13) {
        point = code - 10;
        axis = 0;
    } else if (code >= 20 && code <= 23) {
        point = code - 20;
        axis = 1;
    } else if (code >= 30 && code <= 33) {
        point = code - 30;
        axis = 2;
    } else {
        return;
    }
    if (point < 0 || point >= 4) {
        return;
    }
    if (gfdxf_fx_from_ascii(value, &fx)) {
        e->p[point][axis] = fx;
        e->have[point][axis] = 1;
    } else {
        scene->bad_numbers++;
    }
}

static void gfdxf_store_extrusion_value(gfdxf_scene *scene, gfdxf_entity *e, int code, const char *value)
{
    gfdxf_fx fx;
    if (code != 210 && code != 220 && code != 230) {
        return;
    }
    if (!gfdxf_fx_from_ascii(value, &fx)) {
        scene->bad_numbers++;
        return;
    }
    if (code == 210) {
        e->extrusion.x = fx;
    } else if (code == 220) {
        e->extrusion.y = fx;
    } else {
        e->extrusion.z = fx;
    }
    e->have_extrusion = 1;
}

static int gfdxf_have_xyz(gfdxf_entity *e, int point)
{
    return e->have[point][0] && e->have[point][1] && e->have[point][2];
}

static int gfdxf_have_xy(gfdxf_entity *e, int point)
{
    return e->have[point][0] && e->have[point][1];
}

static int gfdxf_emit_segment_to_point(gfdxf_emit_ctx *ctx, int *prev_idx, const gfdxf_vec3 *a, const gfdxf_vec3 *b, gfdxf_fx bulge, int depth)
{
    gfdxf_vec3 mid;
    gfdxf_fx nx;
    gfdxf_fx ny;
    gfdxf_fx sub;
    int idx;
    if (bulge == 0 || depth <= 0) {
        if (!gfdxf_add_vertex_target(ctx, b, &idx)) {
            return 0;
        }
        if (!gfdxf_add_line_target(ctx, *prev_idx, idx)) {
            return 0;
        }
        *prev_idx = idx;
        return 1;
    }
    mid.x = (a->x + b->x) / 2;
    mid.y = (a->y + b->y) / 2;
    mid.z = (a->z + b->z) / 2;
    nx = -(b->y - a->y);
    ny = b->x - a->x;
    mid.x += gfdxf_fx_mul(nx, bulge) / 2;
    mid.y += gfdxf_fx_mul(ny, bulge) / 2;
    sub = bulge / 2;
    ctx->scene->tessellated_bulges++;
    if (!gfdxf_emit_segment_to_point(ctx, prev_idx, a, &mid, sub, depth - 1)) {
        return 0;
    }
    if (!gfdxf_emit_segment_to_point(ctx, prev_idx, &mid, b, sub, depth - 1)) {
        return 0;
    }
    return 1;
}

static int gfdxf_emit_poly_points(gfdxf_emit_ctx *ctx, const gfdxf_fx *x, const gfdxf_fx *y, const gfdxf_fx *z, const gfdxf_fx *bulge, int count, int flags, int use_ocs, const gfdxf_vec3 *ext)
{
    int i;
    int first_idx;
    int prev_idx;
    gfdxf_vec3 a;
    gfdxf_vec3 b;
    gfdxf_vec3 tmp;
    if (count < 2) {
        return 1;
    }
    gfdxf_vec_set(&a, x[0], y[0], z[0]);
    if (use_ocs) {
        gfdxf_ocs_to_parent(&a, ext, &tmp);
        a = tmp;
    }
    if (!gfdxf_add_vertex_target(ctx, &a, &first_idx)) {
        return 0;
    }
    prev_idx = first_idx;
    for (i = 1; i < count; i++) {
        gfdxf_vec_set(&b, x[i], y[i], z[i]);
        if (use_ocs) {
            gfdxf_ocs_to_parent(&b, ext, &tmp);
            b = tmp;
        }
        if (!gfdxf_emit_segment_to_point(ctx, &prev_idx, &a, &b, bulge[i - 1], GFDXF_BULGE_SUBDIV)) {
            return 0;
        }
        a = b;
    }
    if ((flags & GFDXF_FLAG_CLOSED) != 0) {
        gfdxf_vec_set(&b, x[0], y[0], z[0]);
        if (use_ocs) {
            gfdxf_ocs_to_parent(&b, ext, &tmp);
            b = tmp;
        }
        if (!gfdxf_emit_segment_to_point(ctx, &prev_idx, &a, &b, bulge[count - 1], GFDXF_BULGE_SUBDIV)) {
            return 0;
        }
    }
    return 1;
}

static int gfdxf_finalize_line(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    int a;
    int b;
    gfdxf_vec3 va;
    gfdxf_vec3 vb;
    if (!gfdxf_have_xyz(e, 0) || !gfdxf_have_xyz(e, 1)) {
        return 1;
    }
    gfdxf_entity_point_raw(e, 0, &va);
    gfdxf_entity_point_raw(e, 1, &vb);
    if (!gfdxf_add_vertex_target(ctx, &va, &a)) {
        return 0;
    }
    if (!gfdxf_add_vertex_target(ctx, &vb, &b)) {
        return 0;
    }
    return gfdxf_add_line_target(ctx, a, b);
}

static int gfdxf_finalize_3dface(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    int ids[4];
    int count;
    int i;
    gfdxf_vec3 v;
    count = 0;
    for (i = 0; i < 4; i++) {
        if (gfdxf_have_xyz(e, i)) {
            count++;
        }
    }
    if (count < 3) {
        return 1;
    }
    for (i = 0; i < count; i++) {
        gfdxf_entity_point_raw(e, i, &v);
        if (!gfdxf_add_vertex_target(ctx, &v, &ids[i])) {
            return 0;
        }
    }
    if (count == 3) {
        ids[3] = ids[2];
    }
    return gfdxf_add_face_target(ctx, ids[0], ids[1], ids[2], ids[3], count, e->invisible_edges);
}

static int gfdxf_finalize_lwpolyline(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    return gfdxf_emit_poly_points(ctx, ctx->parser->lw_x, ctx->parser->lw_y, ctx->parser->lw_z, ctx->parser->lw_bulge, e->lw_count, e->lw_flags, 1, &e->extrusion);
}

static int gfdxf_finalize_poly_vertex(gfdxf_emit_ctx *ctx, gfdxf_entity *e, int *poly_count, int poly_flags)
{
    int i;
    int idx;
    int abs_idx;
    gfdxf_vec3 v;
    if (poly_flags & GFDXF_POLYFACE) {
        if (e->face_indices[0] != 0 && e->face_indices[1] != 0 && e->face_indices[2] != 0) {
            int a;
            int b;
            int c;
            int d;
            a = e->face_indices[0];
            b = e->face_indices[1];
            c = e->face_indices[2];
            d = e->face_indices[3];
            if (a < 0) { a = -a; }
            if (b < 0) { b = -b; }
            if (c < 0) { c = -c; }
            if (d < 0) { d = -d; }
            a--;
            b--;
            c--;
            d--;
            if (a >= 0 && a < *poly_count && b >= 0 && b < *poly_count && c >= 0 && c < *poly_count) {
                int ia;
                int ib;
                int ic;
                int id;
                ia = ctx->parser->poly_scene_index[a];
                ib = ctx->parser->poly_scene_index[b];
                ic = ctx->parser->poly_scene_index[c];
                id = (d >= 0 && d < *poly_count) ? ctx->parser->poly_scene_index[d] : ic;
                if (!gfdxf_add_face_target(ctx, ia, ib, ic, id, d >= 0 ? 4 : 3, 0)) {
                    return 0;
                }
            }
            return 1;
        }
    }
    if (!gfdxf_have_xyz(e, 0)) {
        return 1;
    }
    if (*poly_count >= GFDXF_MAX_POLY_TMP) {
        ctx->scene->capacity_errors++;
        return 0;
    }
    gfdxf_entity_point_raw(e, 0, &v);
    if (!gfdxf_add_vertex_target(ctx, &v, &idx)) {
        return 0;
    }
    abs_idx = *poly_count;
    ctx->parser->poly_x[abs_idx] = e->p[0][0];
    ctx->parser->poly_y[abs_idx] = e->p[0][1];
    ctx->parser->poly_z[abs_idx] = e->p[0][2];
    ctx->parser->poly_bulge[abs_idx] = e->vertex_bulge;
    ctx->parser->poly_scene_index[abs_idx] = idx;
    (*poly_count)++;
    for (i = 0; i < 1; i++) {
        ;
    }
    return 1;
}

static int gfdxf_emit_arc_points(gfdxf_emit_ctx *ctx, const gfdxf_entity *e, int closed)
{
    int i;
    int segs;
    int idx;
    int prev;
    int first;
    int start_deg;
    int end_deg;
    int sweep;
    gfdxf_vec3 center;
    gfdxf_vec3 p;
    gfdxf_vec3 q;
    gfdxf_fx radius;
    if (!gfdxf_have_xy((gfdxf_entity *)e, 0) || e->radius <= 0) {
        return 1;
    }
    gfdxf_entity_point_ocs(e, 0, &center);
    radius = e->radius;
    if (closed) {
        start_deg = 0;
        sweep = 360;
        segs = GFDXF_CURVE_SEGMENTS;
    } else {
        start_deg = gfdxf_fx_to_int_nearest(e->start_angle);
        end_deg = gfdxf_fx_to_int_nearest(e->end_angle);
        sweep = end_deg - start_deg;
        while (sweep <= 0) {
            sweep += 360;
        }
        segs = (sweep * GFDXF_ARC_SEGMENTS + 179) / 180;
        if (segs < 1) {
            segs = 1;
        }
        if (segs > GFDXF_CURVE_SEGMENTS) {
            segs = GFDXF_CURVE_SEGMENTS;
        }
    }
    first = -1;
    prev = -1;
    for (i = 0; i <= segs; i++) {
        int deg;
        gfdxf_vec3 loc;
        deg = start_deg + (sweep * i) / segs;
        loc.x = e->p[0][0] + gfdxf_fx_mul(radius, gfdxf_cos_deg_int(deg));
        loc.y = e->p[0][1] + gfdxf_fx_mul(radius, gfdxf_sin_deg_int(deg));
        loc.z = e->p[0][2];
        gfdxf_ocs_to_parent(&loc, &e->extrusion, &p);
        q = p;
        if (!gfdxf_add_vertex_target(ctx, &q, &idx)) {
            return 0;
        }
        if (first < 0) {
            first = idx;
        }
        if (prev >= 0) {
            if (!gfdxf_add_line_target(ctx, prev, idx)) {
                return 0;
            }
        }
        prev = idx;
    }
    if (closed && first >= 0 && prev >= 0) {
        if (!gfdxf_add_line_target(ctx, prev, first)) {
            return 0;
        }
        ctx->scene->tessellated_circles++;
    } else {
        ctx->scene->tessellated_arcs++;
    }
    return 1;
}

static int gfdxf_finalize_circle(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    return gfdxf_emit_arc_points(ctx, e, 1);
}

static int gfdxf_finalize_arc(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    return gfdxf_emit_arc_points(ctx, e, 0);
}

static int gfdxf_spline_eval(const gfdxf_parser *parser, const gfdxf_entity *e, gfdxf_fx u, gfdxf_vec3 *out)
{
    int n;
    int p;
    int m;
    int span;
    int i;
    int r;
    int j;
    gfdxf_vec3 d[GFDXF_MAX_SPLINE_DEGREE + 1];
    gfdxf_fx hw[GFDXF_MAX_SPLINE_DEGREE + 1];
    n = e->spline_ctrl_count - 1;
    p = e->spline_degree;
    if (p < 1) {
        p = 1;
    }
    if (p > GFDXF_MAX_SPLINE_DEGREE) {
        p = GFDXF_MAX_SPLINE_DEGREE;
    }
    if (n < p) {
        return 0;
    }
    m = e->spline_knot_count - 1;
    if (m < n + p + 1) {
        return 0;
    }
    span = p;
    if (u >= parser->spline_knots[n + 1]) {
        span = n;
    } else {
        for (i = p; i <= n; i++) {
            if (u >= parser->spline_knots[i] && u < parser->spline_knots[i + 1]) {
                span = i;
                break;
            }
        }
    }
    for (j = 0; j <= p; j++) {
        int cp;
        gfdxf_fx w;
        cp = span - p + j;
        w = parser->spline_weights[cp];
        if (w == 0) {
            w = GFDXF_FX_ONE;
        }
        d[j].x = gfdxf_fx_mul(parser->spline_ctrl[cp].x, w);
        d[j].y = gfdxf_fx_mul(parser->spline_ctrl[cp].y, w);
        d[j].z = gfdxf_fx_mul(parser->spline_ctrl[cp].z, w);
        hw[j] = w;
    }
    for (r = 1; r <= p; r++) {
        for (j = p; j >= r; j--) {
            int left;
            int right;
            gfdxf_fx den;
            gfdxf_fx alpha;
            left = span - p + j;
            right = span + 1 + j - r;
            den = parser->spline_knots[right] - parser->spline_knots[left];
            if (den == 0) {
                alpha = 0;
            } else {
                alpha = gfdxf_fx_div(u - parser->spline_knots[left], den);
            }
            d[j].x = gfdxf_fx_mul(GFDXF_FX_ONE - alpha, d[j - 1].x) + gfdxf_fx_mul(alpha, d[j].x);
            d[j].y = gfdxf_fx_mul(GFDXF_FX_ONE - alpha, d[j - 1].y) + gfdxf_fx_mul(alpha, d[j].y);
            d[j].z = gfdxf_fx_mul(GFDXF_FX_ONE - alpha, d[j - 1].z) + gfdxf_fx_mul(alpha, d[j].z);
            hw[j] = gfdxf_fx_mul(GFDXF_FX_ONE - alpha, hw[j - 1]) + gfdxf_fx_mul(alpha, hw[j]);
        }
    }
    if (hw[p] != 0) {
        out->x = gfdxf_fx_div(d[p].x, hw[p]);
        out->y = gfdxf_fx_div(d[p].y, hw[p]);
        out->z = gfdxf_fx_div(d[p].z, hw[p]);
    } else {
        *out = d[p];
    }
    return 1;
}

static int gfdxf_finalize_spline(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    int i;
    int idx;
    int prev;
    gfdxf_vec3 p;
    gfdxf_fx u0;
    gfdxf_fx u1;
    if (e->spline_ctrl_count >= 2 && e->spline_knot_count > e->spline_degree + e->spline_ctrl_count) {
        u0 = ctx->parser->spline_knots[e->spline_degree];
        u1 = ctx->parser->spline_knots[e->spline_ctrl_count];
        prev = -1;
        for (i = 0; i <= GFDXF_SPLINE_SEGMENTS; i++) {
            gfdxf_fx u;
            u = u0 + gfdxf_fx_mul(u1 - u0, gfdxf_fx_div((gfdxf_fx)i << GFDXF_FX_SHIFT, (gfdxf_fx)GFDXF_SPLINE_SEGMENTS << GFDXF_FX_SHIFT));
            if (!gfdxf_spline_eval(ctx->parser, e, u, &p)) {
                return 1;
            }
            if (!gfdxf_add_vertex_target(ctx, &p, &idx)) {
                return 0;
            }
            if (prev >= 0) {
                if (!gfdxf_add_line_target(ctx, prev, idx)) {
                    return 0;
                }
            }
            prev = idx;
        }
        ctx->scene->tessellated_splines++;
        return 1;
    }
    if (e->spline_fit_count >= 2) {
        prev = -1;
        for (i = 0; i < e->spline_fit_count; i++) {
            p = ctx->parser->spline_fit[i];
            if (!gfdxf_add_vertex_target(ctx, &p, &idx)) {
                return 0;
            }
            if (prev >= 0) {
                if (!gfdxf_add_line_target(ctx, prev, idx)) {
                    return 0;
                }
            }
            prev = idx;
        }
        ctx->scene->tessellated_splines++;
        return 1;
    }
    if (e->spline_ctrl_count >= 2) {
        prev = -1;
        for (i = 0; i < e->spline_ctrl_count; i++) {
            p = ctx->parser->spline_ctrl[i];
            if (!gfdxf_add_vertex_target(ctx, &p, &idx)) {
                return 0;
            }
            if (prev >= 0) {
                if (!gfdxf_add_line_target(ctx, prev, idx)) {
                    return 0;
                }
            }
            prev = idx;
        }
        ctx->scene->tessellated_splines++;
    }
    return 1;
}


static void gfdxf_text_append(gfdxf_entity *e, const char *value)
{
    int i;
    if (value == 0) {
        return;
    }
    i = 0;
    while (value[i] != '\0' && e->text_len + 1 < GFDXF_MAX_TEXT_CHARS) {
        e->text[e->text_len] = value[i];
        e->text_len++;
        i++;
    }
    e->text[e->text_len] = '\0';
}

static int gfdxf_add_text_scene(gfdxf_emit_ctx *ctx, const gfdxf_text *src)
{
    int idx;
    if (ctx->target == GFDXF_TARGET_BLOCK) {
        return 1;
    }
    if (ctx->scene->text_count >= GFDXF_MAX_TEXTS) {
        ctx->scene->capacity_errors++;
        return 0;
    }
    idx = ctx->scene->text_count;
    ctx->scene->texts[idx] = *src;
    ctx->scene->text_count++;
    if (ctx->callbacks != 0 && ctx->callbacks->on_text != 0) {
        ctx->callbacks->on_text(ctx->user, idx, &ctx->scene->texts[idx]);
    }
    return 1;
}

static int gfdxf_emit_box_at(gfdxf_emit_ctx *ctx, const gfdxf_vec3 *p, gfdxf_fx w, gfdxf_fx h, gfdxf_fx rot)
{
    int deg;
    gfdxf_fx c;
    gfdxf_fx s;
    gfdxf_vec3 pts[4];
    int ids[4];
    int i;
    if (w <= 0) { w = h * 4; }
    if (h <= 0) { h = GFDXF_FX_ONE; }
    deg = gfdxf_fx_to_int_nearest(rot);
    c = gfdxf_cos_deg_int(deg);
    s = gfdxf_sin_deg_int(deg);
    pts[0] = *p;
    pts[1].x = p->x + gfdxf_fx_mul(w, c);
    pts[1].y = p->y + gfdxf_fx_mul(w, s);
    pts[1].z = p->z;
    pts[2].x = pts[1].x + gfdxf_fx_mul(-h, s);
    pts[2].y = pts[1].y + gfdxf_fx_mul(h, c);
    pts[2].z = p->z;
    pts[3].x = p->x + gfdxf_fx_mul(-h, s);
    pts[3].y = p->y + gfdxf_fx_mul(h, c);
    pts[3].z = p->z;
    for (i = 0; i < 4; i++) {
        if (!gfdxf_add_vertex_target(ctx, &pts[i], &ids[i])) { return 0; }
    }
    for (i = 0; i < 4; i++) {
        if (!gfdxf_add_line_target(ctx, ids[i], ids[(i + 1) & 3])) { return 0; }
    }
    ctx->scene->emitted_text_boxes++;
    return 1;
}

static int gfdxf_finalize_text_common(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    gfdxf_text t;
    gfdxf_vec3 p;
    int i;
    gfdxf_fx w;
    if (!gfdxf_have_xyz(e, 0)) {
        return 1;
    }
    gfdxf_entity_point_raw(e, 0, &p);
    memset(&t, 0, sizeof(t));
    gfdxf_copy_name(t.layer, e->layer);
    for (i = 0; i + 1 < GFDXF_MAX_TEXT_CHARS && e->text[i] != '\0'; i++) {
        t.text[i] = e->text[i];
    }
    t.text[i] = '\0';
    t.insert = p;
    t.height = e->text_height;
    t.width = e->text_width;
    t.rotation = e->text_rotation;
    t.is_mtext = e->is_mtext;
    if (!gfdxf_add_text_scene(ctx, &t)) { return 0; }
    w = e->text_width;
    if (w <= 0) {
        w = (gfdxf_fx)e->text_len << (GFDXF_FX_SHIFT - 1);
        w = gfdxf_fx_mul(w, e->text_height);
    }
    return gfdxf_emit_box_at(ctx, &p, w, e->text_height, e->text_rotation);
}

static int gfdxf_finalize_point(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    gfdxf_vec3 p;
    int idx;
    if (!gfdxf_have_xyz(e, 0)) { return 1; }
    gfdxf_entity_point_raw(e, 0, &p);
    return gfdxf_add_vertex_target(ctx, &p, &idx);
}

static int gfdxf_finalize_solid(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    int ids[4];
    int count;
    int i;
    gfdxf_vec3 v;
    count = 0;
    for (i = 0; i < 4; i++) {
        if (gfdxf_have_xyz(e, i)) { count++; }
    }
    if (count < 3) { return 1; }
    for (i = 0; i < count; i++) {
        gfdxf_entity_point_raw(e, i, &v);
        if (!gfdxf_add_vertex_target(ctx, &v, &ids[i])) { return 0; }
    }
    if (count == 3) { ids[3] = ids[2]; }
    return gfdxf_add_face_target(ctx, ids[0], ids[1], ids[2], ids[3], count, 0);
}

static int gfdxf_emit_ellipse_points(gfdxf_emit_ctx *ctx, const gfdxf_entity *e)
{
    int i;
    int segs;
    int idx;
    int prev;
    int first;
    int start_deg;
    int end_deg;
    int sweep;
    gfdxf_vec3 c;
    gfdxf_vec3 major;
    gfdxf_vec3 minor;
    gfdxf_vec3 p;
    gfdxf_vec3 normal;
    gfdxf_fx ratio;
    if (!gfdxf_have_xyz((gfdxf_entity *)e, 0) || !gfdxf_have_xyz((gfdxf_entity *)e, 1)) {
        return 1;
    }
    gfdxf_entity_point_raw(e, 0, &c);
    gfdxf_entity_point_raw(e, 1, &major);
    ratio = e->ellipse_ratio;
    if (ratio <= 0) { ratio = GFDXF_FX_ONE; }
    normal = e->extrusion;
    gfdxf_vec_normalize(&normal);
    minor = gfdxf_vec_cross(normal, major);
    minor.x = gfdxf_fx_mul(minor.x, ratio);
    minor.y = gfdxf_fx_mul(minor.y, ratio);
    minor.z = gfdxf_fx_mul(minor.z, ratio);
    start_deg = gfdxf_fx_to_int_nearest(e->ellipse_start);
    end_deg = gfdxf_fx_to_int_nearest(e->ellipse_end);
    sweep = end_deg - start_deg;
    while (sweep <= 0) { sweep += 360; }
    if (sweep > 360) { sweep = 360; }
    segs = (sweep * GFDXF_ELLIPSE_SEGMENTS + 359) / 360;
    if (segs < 1) { segs = 1; }
    if (segs > GFDXF_ELLIPSE_SEGMENTS) { segs = GFDXF_ELLIPSE_SEGMENTS; }
    prev = -1;
    first = -1;
    for (i = 0; i <= segs; i++) {
        int deg;
        gfdxf_fx co;
        gfdxf_fx si;
        deg = start_deg + (sweep * i) / segs;
        co = gfdxf_cos_deg_int(deg);
        si = gfdxf_sin_deg_int(deg);
        p.x = c.x + gfdxf_fx_mul(major.x, co) + gfdxf_fx_mul(minor.x, si);
        p.y = c.y + gfdxf_fx_mul(major.y, co) + gfdxf_fx_mul(minor.y, si);
        p.z = c.z + gfdxf_fx_mul(major.z, co) + gfdxf_fx_mul(minor.z, si);
        if (!gfdxf_add_vertex_target(ctx, &p, &idx)) { return 0; }
        if (first < 0) { first = idx; }
        if (prev >= 0) {
            if (!gfdxf_add_line_target(ctx, prev, idx)) { return 0; }
        }
        prev = idx;
    }
    if (sweep == 360 && first >= 0 && prev >= 0) {
        if (!gfdxf_add_line_target(ctx, prev, first)) { return 0; }
    }
    ctx->scene->tessellated_ellipses++;
    return 1;
}


static int gfdxf_finalize_polyseq(gfdxf_emit_ctx *ctx, gfdxf_entity *e, int closed)
{
    int flags;
    flags = closed ? GFDXF_FLAG_CLOSED : 0;
    return gfdxf_emit_poly_points(ctx, ctx->parser->lw_x, ctx->parser->lw_y, ctx->parser->lw_z, ctx->parser->lw_bulge, e->lw_count, flags, 0, 0);
}

static int gfdxf_finalize_ray_common(gfdxf_emit_ctx *ctx, gfdxf_entity *e, int both)
{
    gfdxf_vec3 p;
    gfdxf_vec3 d;
    gfdxf_vec3 a;
    gfdxf_vec3 b;
    gfdxf_fx len;
    int ia;
    int ib;
    if (!gfdxf_have_xyz(e, 0) || !gfdxf_have_xyz(e, 1)) { return 1; }
    gfdxf_entity_point_raw(e, 0, &p);
    gfdxf_entity_point_raw(e, 1, &d);
    gfdxf_vec_normalize(&d);
    len = ((gfdxf_fx)GFDXF_RAY_LENGTH_UNITS) << GFDXF_FX_SHIFT;
    if (both) {
        a.x = p.x - gfdxf_fx_mul(d.x, len);
        a.y = p.y - gfdxf_fx_mul(d.y, len);
        a.z = p.z - gfdxf_fx_mul(d.z, len);
    } else {
        a = p;
    }
    b.x = p.x + gfdxf_fx_mul(d.x, len);
    b.y = p.y + gfdxf_fx_mul(d.y, len);
    b.z = p.z + gfdxf_fx_mul(d.z, len);
    if (!gfdxf_add_vertex_target(ctx, &a, &ia)) { return 0; }
    if (!gfdxf_add_vertex_target(ctx, &b, &ib)) { return 0; }
    return gfdxf_add_line_target(ctx, ia, ib);
}

static int gfdxf_finalize_viewport(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    gfdxf_vec3 p;
    if (!gfdxf_have_xy(e, 0)) { return 1; }
    p.x = e->p[0][0];
    p.y = e->p[0][1];
    p.z = e->have[0][2] ? e->p[0][2] : 0;
    return gfdxf_emit_box_at(ctx, &p, e->text_width, e->text_height, 0);
}

static int gfdxf_finalize_dimension(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    int a;
    int b;
    int c;
    gfdxf_vec3 p0;
    gfdxf_vec3 p1;
    gfdxf_vec3 pt;
    if (gfdxf_have_xyz(e, 1) && gfdxf_have_xyz(e, 2)) {
        gfdxf_entity_point_raw(e, 1, &p0);
        gfdxf_entity_point_raw(e, 2, &p1);
    } else if (gfdxf_have_xyz(e, 0) && gfdxf_have_xyz(e, 1)) {
        gfdxf_entity_point_raw(e, 0, &p0);
        gfdxf_entity_point_raw(e, 1, &p1);
    } else {
        return 1;
    }
    if (!gfdxf_add_vertex_target(ctx, &p0, &a)) { return 0; }
    if (!gfdxf_add_vertex_target(ctx, &p1, &b)) { return 0; }
    if (!gfdxf_add_line_target(ctx, a, b)) { return 0; }
    if (gfdxf_have_xyz(e, 0)) {
        gfdxf_entity_point_raw(e, 0, &pt);
        if (!gfdxf_add_vertex_target(ctx, &pt, &c)) { return 0; }
        if (!gfdxf_add_line_target(ctx, c, a)) { return 0; }
        if (!gfdxf_add_line_target(ctx, c, b)) { return 0; }
    }
    if (gfdxf_have_xyz(e, 1)) {
        gfdxf_entity_point_raw(e, 1, &pt);
        if (!gfdxf_emit_box_at(ctx, &pt, e->text_width, e->text_height, e->text_rotation)) { return 0; }
    }
    ctx->scene->emitted_dimensions++;
    switch (e->dim_type & 7) {
    case 0: ctx->scene->dimension_linear++; break;
    case 1: ctx->scene->dimension_linear++; break;
    case 2: case 5: ctx->scene->dimension_angular++; break;
    case 3: ctx->scene->dimension_diameter++; break;
    case 4: ctx->scene->dimension_radial++; break;
    case 6: ctx->scene->dimension_ordinate++; break;
    default: break;
    }
    return 1;
}

static int gfdxf_emit_hatch_points(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    int i;
    int flags;
    int a;
    int b;
    int c;
    gfdxf_vec3 p;
    gfdxf_vec3 q;
    if (e->hatch_count < 2) { return 1; }
    flags = e->hatch_closed ? GFDXF_FLAG_CLOSED : 0;
    if (!gfdxf_emit_poly_points(ctx, ctx->parser->hatch_x, ctx->parser->hatch_y, ctx->parser->hatch_z, ctx->parser->hatch_bulge, e->hatch_count, flags, 1, &e->extrusion)) {
        return 0;
    }
#if GFDXF_HATCH_TRIANGULATE
    if (e->hatch_solid && e->hatch_count >= 3) {
        for (i = 1; i + 1 < e->hatch_count; i++) {
            gfdxf_vec_set(&p, ctx->parser->hatch_x[0], ctx->parser->hatch_y[0], ctx->parser->hatch_z[0]);
            gfdxf_ocs_to_parent(&p, &e->extrusion, &q);
            if (!gfdxf_add_vertex_target(ctx, &q, &a)) { return 0; }
            gfdxf_vec_set(&p, ctx->parser->hatch_x[i], ctx->parser->hatch_y[i], ctx->parser->hatch_z[i]);
            gfdxf_ocs_to_parent(&p, &e->extrusion, &q);
            if (!gfdxf_add_vertex_target(ctx, &q, &b)) { return 0; }
            gfdxf_vec_set(&p, ctx->parser->hatch_x[i + 1], ctx->parser->hatch_y[i + 1], ctx->parser->hatch_z[i + 1]);
            gfdxf_ocs_to_parent(&p, &e->extrusion, &q);
            if (!gfdxf_add_vertex_target(ctx, &q, &c)) { return 0; }
            if (!gfdxf_add_face_target(ctx, a, b, c, c, 3, 0)) { return 0; }
        }
    }
#endif
    ctx->scene->tessellated_hatches++;
    return 1;
}

static void gfdxf_payload_from_entity(gfdxf_payload *pl, const gfdxf_entity *e, int kind, const char *dxf_name)
{
    int i;
    memset(pl, 0, sizeof(gfdxf_payload));
    pl->kind = kind;
    gfdxf_copy_name(pl->dxf_name, dxf_name);
    pl->class_id = e->payload_class_id;
    pl->graphics_size_bytes = e->payload_graphics_bytes;
    pl->data_size_bits = e->payload_data_bits;
    pl->drawing_format = e->payload_drawing_format;
    pl->original_data_format = e->payload_data_format;
    pl->checksum = e->payload_checksum;
    pl->stored_hex_chars = e->payload_hex_len;
    pl->total_hex_chars = e->payload_hex_total;
    gfdxf_copy_text_cap(pl->first_hex, GFDXF_MAX_PAYLOAD_HEX_CHARS, e->payload_hex);
    pl->handle_count = e->payload_handle_count;
    for (i = 0; i < e->payload_handle_count && i < GFDXF_MAX_HANDLES; i++) {
        gfdxf_copy_name(pl->handles[i], e->payload_handles[i]);
    }
    pl->acis_line_count = e->acis_line_count;
    pl->acis_text_chars = e->acis_text_chars;
    gfdxf_copy_text_cap(pl->acis_head, GFDXF_MAX_ACIS_HEAD_CHARS, e->acis_head);
    pl->acis_tok_body = e->acis_tok_body;
    pl->acis_tok_lump = e->acis_tok_lump;
    pl->acis_tok_shell = e->acis_tok_shell;
    pl->acis_tok_face = e->acis_tok_face;
    pl->acis_tok_loop = e->acis_tok_loop;
    pl->acis_tok_coedge = e->acis_tok_coedge;
    pl->acis_tok_edge = e->acis_tok_edge;
    pl->acis_tok_vertex = e->acis_tok_vertex;
    pl->acis_tok_surface = e->acis_tok_surface;
    pl->acis_tok_curve = e->acis_tok_curve;
    pl->acis_topology_score = e->acis_tok_body + e->acis_tok_lump + e->acis_tok_shell + e->acis_tok_face + e->acis_tok_loop + e->acis_tok_coedge + e->acis_tok_edge + e->acis_tok_vertex;
}


static int gfdxf_add_payload_scene(gfdxf_scene *scene, const gfdxf_entity *e, int kind, const char *dxf_name)
{
    int idx;
    if (scene->payload_count >= GFDXF_MAX_PAYLOADS) {
        scene->capacity_errors++;
        return 0;
    }
    idx = scene->payload_count;
    gfdxf_payload_from_entity(&scene->payloads[idx], e, kind, dxf_name);
    scene->payload_count++;
    return 1;
}

static int gfdxf_finalize_proxy(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    ctx->scene->proxy_entities++;
    ctx->scene->proxy_graphic_bytes += e->payload_graphics_bytes;
    ctx->scene->proxy_data_bits += e->payload_data_bits;
    ctx->scene->proxy_handle_refs += e->payload_handle_count;
    if (e->payload_hex_total > 0) { ctx->scene->custom_binary_payloads++; }
    return gfdxf_add_payload_scene(ctx->scene, e, GFDXF_PAYLOAD_PROXY_ENTITY, "ACAD_PROXY_ENTITY");
}

static int gfdxf_finalize_proxy_object(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    ctx->scene->proxy_object_count++;
    ctx->scene->proxy_object_bits += e->payload_data_bits;
    ctx->scene->proxy_handle_refs += e->payload_handle_count;
    if (e->payload_hex_total > 0) { ctx->scene->custom_binary_payloads++; }
    return gfdxf_add_payload_scene(ctx->scene, e, GFDXF_PAYLOAD_PROXY_OBJECT, "ACAD_PROXY_OBJECT");
}

static int gfdxf_finalize_acis(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    int kind;
    ctx->scene->acis_entities++;
    ctx->scene->acis_text_lines += e->acis_line_count;
    ctx->scene->acis_tok_body += e->acis_tok_body;
    ctx->scene->acis_tok_lump += e->acis_tok_lump;
    ctx->scene->acis_tok_shell += e->acis_tok_shell;
    ctx->scene->acis_tok_face += e->acis_tok_face;
    ctx->scene->acis_tok_loop += e->acis_tok_loop;
    ctx->scene->acis_tok_coedge += e->acis_tok_coedge;
    ctx->scene->acis_tok_edge += e->acis_tok_edge;
    ctx->scene->acis_tok_vertex += e->acis_tok_vertex;
    ctx->scene->acis_tok_surface += e->acis_tok_surface;
    ctx->scene->acis_tok_curve += e->acis_tok_curve;
    if (e->acis_tok_body || e->acis_tok_lump || e->acis_tok_shell || e->acis_tok_face || e->acis_tok_edge || e->acis_tok_vertex) {
        ctx->scene->acis_semantic_payloads++;
    }
    if (e->acis_line_count > 0) {
        ctx->scene->acis_sat_payloads++;
        kind = GFDXF_PAYLOAD_ACIS_TEXT;
    } else if (e->payload_hex_total > 0) {
        ctx->scene->acis_binary_payloads++;
        kind = GFDXF_PAYLOAD_ACIS_BINARY;
    } else {
        kind = GFDXF_PAYLOAD_CUSTOM_BINARY;
    }
    return gfdxf_add_payload_scene(ctx->scene, e, kind, "ACIS_BODY");
}


static int gfdxf_finalize_mesh_marker(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    int i;
    int pos;
    int n;
    int a;
    int b;
    int c;
    int d;
    gfdxf_vec3 v;
    ctx->scene->mesh_entities++;
    if (e->mesh_subdiv_level > ctx->scene->mesh_subdivision_level_max) {
        ctx->scene->mesh_subdivision_level_max = e->mesh_subdiv_level;
    }
    if (e->mesh_vertex_count <= 0) {
        return 1;
    }
    for (i = 0; i < e->mesh_vertex_count && i < GFDXF_MAX_POLY_TMP; i++) {
        gfdxf_vec_set(&v, ctx->parser->poly_x[i], ctx->parser->poly_y[i], ctx->parser->poly_z[i]);
        if (!gfdxf_add_vertex_target(ctx, &v, &ctx->parser->mesh_scene_index[i])) { return 0; }
        ctx->scene->mesh_vertices_emitted++;
    }
    pos = 0;
    while (pos < e->mesh_face_int_count) {
        n = ctx->parser->poly_scene_index[pos];
        pos++;
        if (n < 0) { n = -n; }
        if (n < 3 || pos + n > e->mesh_face_int_count) {
            break;
        }
        a = ctx->parser->poly_scene_index[pos];
        b = ctx->parser->poly_scene_index[pos + 1];
        c = ctx->parser->poly_scene_index[pos + 2];
        d = c;
        if (n > 3) { d = ctx->parser->poly_scene_index[pos + 3]; }
        if (a >= 0 && b >= 0 && c >= 0 && a < e->mesh_vertex_count && b < e->mesh_vertex_count && c < e->mesh_vertex_count) {
            if (d < 0 || d >= e->mesh_vertex_count) { d = c; }
            if (!gfdxf_add_face_target(ctx, ctx->parser->mesh_scene_index[a], ctx->parser->mesh_scene_index[b], ctx->parser->mesh_scene_index[c], ctx->parser->mesh_scene_index[d], n > 3 ? 4 : 3, 0)) { return 0; }
            ctx->scene->mesh_faces_emitted++;
        }
        pos += n;
    }
    return 1;
}

static int gfdxf_finalize_object_record(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    int i;
    int idx;
    gfdxf_object_record *obj;
    if (ctx->scene->object_count >= GFDXF_MAX_OBJECT_RECORDS) {
        ctx->scene->capacity_errors++;
        return 0;
    }
    idx = ctx->scene->object_count;
    obj = &ctx->scene->object_records[idx];
    memset(obj, 0, sizeof(gfdxf_object_record));
    gfdxf_copy_name(obj->type, e->object_type[0] != '\0' ? e->object_type : "OBJECT");
    gfdxf_copy_name(obj->name, e->object_key[0] != '\0' ? e->object_key : e->name);
    gfdxf_copy_name(obj->handle, e->object_handle);
    gfdxf_copy_name(obj->owner, e->object_owner);
    gfdxf_copy_text_cap(obj->first_text, GFDXF_MAX_OBJECT_TEXT_CHARS, e->object_first_text);
    obj->handle_count = e->payload_handle_count;
    for (i = 0; i < e->payload_handle_count && i < GFDXF_MAX_HANDLES; i++) {
        gfdxf_copy_name(obj->handles[i], e->payload_handles[i]);
    }
    obj->text_chunks = e->object_text_chunks;
    obj->int_values = e->object_int_values;
    obj->binary_chunks = e->object_binary_chunks;
    obj->total_hex_chars = e->payload_hex_total;
    obj->stored_hex_chars = e->payload_hex_len;
    obj->checksum = e->payload_checksum;
    ctx->scene->object_count++;
    ctx->scene->object_binary_chunks += obj->binary_chunks;
    ctx->scene->object_text_chunks += obj->text_chunks;
    ctx->scene->object_handle_refs += obj->handle_count;
    if (e->type == GFDXF_ENTITY_DICTIONARY) { ctx->scene->dictionary_objects++; }
    else if (e->type == GFDXF_ENTITY_XRECORD) { ctx->scene->xrecord_objects++; }
    else if (e->type == GFDXF_ENTITY_DICTIONARYVAR) { ctx->scene->dictionaryvar_objects++; }
    else if (e->type == GFDXF_ENTITY_TABLE) { ctx->scene->table_entities++; }
    else { ctx->scene->generic_object_records++; }
    return 1;
}

static int gfdxf_finalize_image_marker(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    gfdxf_vec3 p0;
    gfdxf_vec3 u;
    gfdxf_vec3 v;
    gfdxf_vec3 pts[4];
    int ids[4];
    int i;
    ctx->scene->image_underlay_entities++;
    if (gfdxf_str_eq_i(e->object_type, "WIPEOUT")) { ctx->scene->wipeout_entities++; }
    else if (gfdxf_str_eq_i(e->object_type, "PDFUNDERLAY") || gfdxf_str_eq_i(e->object_type, "DGNUNDERLAY") || gfdxf_str_eq_i(e->object_type, "DWFUNDERLAY") || gfdxf_str_eq_i(e->object_type, "UNDERLAY")) { ctx->scene->underlay_entities++; }
    else { ctx->scene->image_entities++; }
    if (!gfdxf_have_xy(e, 0)) { return 1; }
    p0.x = e->p[0][0];
    p0.y = e->p[0][1];
    p0.z = e->have[0][2] ? e->p[0][2] : 0;
    if (!gfdxf_have_xy(e, 1) || !gfdxf_have_xy(e, 2)) {
        return gfdxf_add_vertex_target(ctx, &p0, &ids[0]);
    }
    u.x = e->p[1][0];
    u.y = e->p[1][1];
    u.z = e->have[1][2] ? e->p[1][2] : 0;
    v.x = e->p[2][0];
    v.y = e->p[2][1];
    v.z = e->have[2][2] ? e->p[2][2] : 0;
    pts[0] = p0;
    pts[1].x = p0.x + u.x; pts[1].y = p0.y + u.y; pts[1].z = p0.z + u.z;
    pts[2].x = p0.x + u.x + v.x; pts[2].y = p0.y + u.y + v.y; pts[2].z = p0.z + u.z + v.z;
    pts[3].x = p0.x + v.x; pts[3].y = p0.y + v.y; pts[3].z = p0.z + v.z;
    for (i = 0; i < 4; i++) {
        if (!gfdxf_add_vertex_target(ctx, &pts[i], &ids[i])) { return 0; }
    }
    for (i = 0; i < 4; i++) {
        if (!gfdxf_add_line_target(ctx, ids[i], ids[(i + 1) & 3])) { return 0; }
    }
    ctx->scene->image_frames_emitted++;
    return 1;
}

int gfdxf_find_block(const gfdxf_parser *parser, const char *name)
{
    int i;
    if (parser == 0 || name == 0) {
        return -1;
    }
    for (i = 0; i < parser->block_count; i++) {
        if (gfdxf_str_eq(parser->blocks[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static void gfdxf_insert_ref_from_entity(gfdxf_insert_ref *dst, const gfdxf_entity *e)
{
    gfdxf_copy_name(dst->name, e->name);
    dst->insert.x = e->p[0][0];
    dst->insert.y = e->p[0][1];
    dst->insert.z = e->p[0][2];
    dst->scale.x = e->scale[0];
    dst->scale.y = e->scale[1];
    dst->scale.z = e->scale[2];
    dst->rotation = e->rotation;
    dst->columns = e->columns;
    dst->rows = e->rows;
    dst->column_spacing = e->column_spacing;
    dst->row_spacing = e->row_spacing;
    dst->extrusion = e->extrusion;
}

static int gfdxf_store_block_insert(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    gfdxf_parser *parser;
    gfdxf_block *block;
    int idx;
    parser = ctx->parser;
    if (ctx->block_index < 0 || ctx->block_index >= parser->block_count) {
        ctx->scene->capacity_errors++;
        return 0;
    }
    if (parser->block_insert_count >= GFDXF_MAX_BLOCK_INSERTS) {
        ctx->scene->capacity_errors++;
        return 0;
    }
    idx = parser->block_insert_count;
    gfdxf_insert_ref_from_entity(&parser->block_inserts[idx], e);
    parser->block_insert_count++;
    block = &parser->blocks[ctx->block_index];
    if (block->insert_count == 0) {
        block->insert_start = idx;
    }
    block->insert_count++;
    return 1;
}

static void gfdxf_insert_build_xform(const gfdxf_block *block, const gfdxf_insert_ref *ins, int col, int row, gfdxf_xform *out)
{
    gfdxf_vec3 ax;
    gfdxf_vec3 ay;
    gfdxf_vec3 az;
    gfdxf_vec3 ip;
    gfdxf_vec3 grid;
    gfdxf_vec3 grid_world;
    gfdxf_vec3 base_world;
    gfdxf_vec3 brx;
    gfdxf_vec3 bry;
    gfdxf_fx c;
    gfdxf_fx s;
    int deg;
    gfdxf_xform_identity(out);
    gfdxf_ocs_basis(&ins->extrusion, &ax, &ay, &az);
    gfdxf_ocs_to_parent(&ins->insert, &ins->extrusion, &ip);
    deg = gfdxf_fx_to_int_nearest(ins->rotation);
    c = gfdxf_cos_deg_int(deg);
    s = gfdxf_sin_deg_int(deg);
    out->m[0][0] = gfdxf_fx_mul(ax.x, gfdxf_fx_mul(c, ins->scale.x)) + gfdxf_fx_mul(ay.x, gfdxf_fx_mul(s, ins->scale.x));
    out->m[1][0] = gfdxf_fx_mul(ax.y, gfdxf_fx_mul(c, ins->scale.x)) + gfdxf_fx_mul(ay.y, gfdxf_fx_mul(s, ins->scale.x));
    out->m[2][0] = gfdxf_fx_mul(ax.z, gfdxf_fx_mul(c, ins->scale.x)) + gfdxf_fx_mul(ay.z, gfdxf_fx_mul(s, ins->scale.x));
    out->m[0][1] = gfdxf_fx_mul(ax.x, gfdxf_fx_mul(-s, ins->scale.y)) + gfdxf_fx_mul(ay.x, gfdxf_fx_mul(c, ins->scale.y));
    out->m[1][1] = gfdxf_fx_mul(ax.y, gfdxf_fx_mul(-s, ins->scale.y)) + gfdxf_fx_mul(ay.y, gfdxf_fx_mul(c, ins->scale.y));
    out->m[2][1] = gfdxf_fx_mul(ax.z, gfdxf_fx_mul(-s, ins->scale.y)) + gfdxf_fx_mul(ay.z, gfdxf_fx_mul(c, ins->scale.y));
    out->m[0][2] = gfdxf_fx_mul(az.x, ins->scale.z);
    out->m[1][2] = gfdxf_fx_mul(az.y, ins->scale.z);
    out->m[2][2] = gfdxf_fx_mul(az.z, ins->scale.z);
    grid.x = gfdxf_fx_mul((gfdxf_fx)col << GFDXF_FX_SHIFT, ins->column_spacing);
    grid.y = gfdxf_fx_mul((gfdxf_fx)row << GFDXF_FX_SHIFT, ins->row_spacing);
    grid.z = 0;
    brx.x = gfdxf_fx_mul(ax.x, c) + gfdxf_fx_mul(ay.x, s);
    brx.y = gfdxf_fx_mul(ax.y, c) + gfdxf_fx_mul(ay.y, s);
    brx.z = gfdxf_fx_mul(ax.z, c) + gfdxf_fx_mul(ay.z, s);
    bry.x = gfdxf_fx_mul(ax.x, -s) + gfdxf_fx_mul(ay.x, c);
    bry.y = gfdxf_fx_mul(ax.y, -s) + gfdxf_fx_mul(ay.y, c);
    bry.z = gfdxf_fx_mul(ax.z, -s) + gfdxf_fx_mul(ay.z, c);
    grid_world.x = gfdxf_fx_mul(brx.x, grid.x) + gfdxf_fx_mul(bry.x, grid.y);
    grid_world.y = gfdxf_fx_mul(brx.y, grid.x) + gfdxf_fx_mul(bry.y, grid.y);
    grid_world.z = gfdxf_fx_mul(brx.z, grid.x) + gfdxf_fx_mul(bry.z, grid.y);
    base_world.x = gfdxf_fx_mul(out->m[0][0], block->base.x) + gfdxf_fx_mul(out->m[0][1], block->base.y) + gfdxf_fx_mul(out->m[0][2], block->base.z);
    base_world.y = gfdxf_fx_mul(out->m[1][0], block->base.x) + gfdxf_fx_mul(out->m[1][1], block->base.y) + gfdxf_fx_mul(out->m[1][2], block->base.z);
    base_world.z = gfdxf_fx_mul(out->m[2][0], block->base.x) + gfdxf_fx_mul(out->m[2][1], block->base.y) + gfdxf_fx_mul(out->m[2][2], block->base.z);
    out->t.x = ip.x + grid_world.x - base_world.x;
    out->t.y = ip.y + grid_world.y - base_world.y;
    out->t.z = ip.z + grid_world.z - base_world.z;
}

static int gfdxf_expand_block(gfdxf_emit_ctx *ctx, int block_index, const gfdxf_xform *xf, int depth);

static int gfdxf_emit_insert_once(gfdxf_emit_ctx *ctx, const gfdxf_insert_ref *ins, int block_index, int col, int row, int depth)
{
    gfdxf_xform child;
    gfdxf_xform composed;
    gfdxf_block *block;
    block = &ctx->parser->blocks[block_index];
    gfdxf_insert_build_xform(block, ins, col, row, &child);
    gfdxf_xform_compose(&ctx->xf, &child, &composed);
    return gfdxf_expand_block(ctx, block_index, &composed, depth + 1);
}

static int gfdxf_expand_block(gfdxf_emit_ctx *ctx, int block_index, const gfdxf_xform *xf, int depth)
{
    gfdxf_parser *parser;
    gfdxf_block *block;
    gfdxf_xform save;
    int i;
    int a;
    int b;
    int c;
    int d;
    int ia;
    int ib;
    int ic;
    int id;
    gfdxf_vec3 va;
    gfdxf_vec3 vb;
    gfdxf_vec3 vc;
    gfdxf_vec3 vd;
    gfdxf_line *ln;
    gfdxf_face *fc;
    parser = ctx->parser;
    if (depth > GFDXF_MAX_INSERT_RECURSION) {
        ctx->scene->max_recursion_hits++;
        return 1;
    }
    block = &parser->blocks[block_index];
    save = ctx->xf;
    ctx->xf = *xf;
    for (i = 0; i < block->line_count; i++) {
        ln = &parser->block_lines[block->line_start + i];
        a = ln->a;
        b = ln->b;
        if (a < block->vertex_start || a >= block->vertex_start + block->vertex_count) {
            continue;
        }
        if (b < block->vertex_start || b >= block->vertex_start + block->vertex_count) {
            continue;
        }
        gfdxf_xform_apply(&ctx->xf, &parser->block_vertices[a], &va);
        gfdxf_xform_apply(&ctx->xf, &parser->block_vertices[b], &vb);
        if (!gfdxf_add_vertex_scene(ctx, va.x, va.y, va.z, &ia)) { ctx->xf = save; return 0; }
        if (!gfdxf_add_vertex_scene(ctx, vb.x, vb.y, vb.z, &ib)) { ctx->xf = save; return 0; }
        if (!gfdxf_add_line_scene(ctx, ia, ib)) { ctx->xf = save; return 0; }
    }
    for (i = 0; i < block->face_count; i++) {
        fc = &parser->block_faces[block->face_start + i];
        a = fc->v[0];
        b = fc->v[1];
        c = fc->v[2];
        d = fc->v[3];
        if (a < block->vertex_start || a >= block->vertex_start + block->vertex_count) { continue; }
        if (b < block->vertex_start || b >= block->vertex_start + block->vertex_count) { continue; }
        if (c < block->vertex_start || c >= block->vertex_start + block->vertex_count) { continue; }
        if (d < block->vertex_start || d >= block->vertex_start + block->vertex_count) { d = c; }
        gfdxf_xform_apply(&ctx->xf, &parser->block_vertices[a], &va);
        gfdxf_xform_apply(&ctx->xf, &parser->block_vertices[b], &vb);
        gfdxf_xform_apply(&ctx->xf, &parser->block_vertices[c], &vc);
        gfdxf_xform_apply(&ctx->xf, &parser->block_vertices[d], &vd);
        if (!gfdxf_add_vertex_scene(ctx, va.x, va.y, va.z, &ia)) { ctx->xf = save; return 0; }
        if (!gfdxf_add_vertex_scene(ctx, vb.x, vb.y, vb.z, &ib)) { ctx->xf = save; return 0; }
        if (!gfdxf_add_vertex_scene(ctx, vc.x, vc.y, vc.z, &ic)) { ctx->xf = save; return 0; }
        if (!gfdxf_add_vertex_scene(ctx, vd.x, vd.y, vd.z, &id)) { ctx->xf = save; return 0; }
        if (!gfdxf_add_face_scene(ctx, ia, ib, ic, id, fc->count, fc->invisible_edges)) { ctx->xf = save; return 0; }
    }
    for (i = 0; i < block->insert_count; i++) {
        gfdxf_insert_ref *ins;
        int bi;
        int r;
        int col;
        ins = &parser->block_inserts[block->insert_start + i];
        bi = gfdxf_find_block(parser, ins->name);
        if (bi < 0) {
            ctx->scene->missing_blocks++;
            continue;
        }
        for (r = 0; r < ins->rows; r++) {
            for (col = 0; col < ins->columns; col++) {
                if (!gfdxf_emit_insert_once(ctx, ins, bi, col, r, depth)) { ctx->xf = save; return 0; }
                ctx->scene->nested_inserts++;
            }
        }
    }
    ctx->xf = save;
    return 1;
}

static int gfdxf_finalize_insert(gfdxf_emit_ctx *ctx, gfdxf_entity *e)
{
    gfdxf_insert_ref ref;
    int block_index;
    int r;
    int c;
    if (!gfdxf_have_xyz(e, 0)) {
        return 1;
    }
    if (ctx->target == GFDXF_TARGET_BLOCK) {
        return gfdxf_store_block_insert(ctx, e);
    }
    block_index = gfdxf_find_block(ctx->parser, e->name);
    if (block_index < 0) {
        ctx->scene->missing_blocks++;
        return 1;
    }
    if (e->columns < 1) { e->columns = 1; }
    if (e->rows < 1) { e->rows = 1; }
    gfdxf_insert_ref_from_entity(&ref, e);
    for (r = 0; r < e->rows; r++) {
        for (c = 0; c < e->columns; c++) {
            if (!gfdxf_emit_insert_once(ctx, &ref, block_index, c, r, 0)) {
                return 0;
            }
            ctx->scene->inserted_blocks++;
        }
    }
    return 1;
}

static int gfdxf_finalize_entity(gfdxf_emit_ctx *ctx, gfdxf_entity *e, int *poly_count, int poly_flags)
{
    int ok;
    ok = 1;
    if (e->type == GFDXF_ENTITY_LINE) {
        ok = gfdxf_finalize_line(ctx, e);
    } else if (e->type == GFDXF_ENTITY_3DFACE) {
        ok = gfdxf_finalize_3dface(ctx, e);
    } else if (e->type == GFDXF_ENTITY_LWPOLYLINE) {
        ok = gfdxf_finalize_lwpolyline(ctx, e);
    } else if (e->type == GFDXF_ENTITY_VERTEX) {
        ok = gfdxf_finalize_poly_vertex(ctx, e, poly_count, poly_flags);
    } else if (e->type == GFDXF_ENTITY_INSERT) {
        ok = gfdxf_finalize_insert(ctx, e);
    } else if (e->type == GFDXF_ENTITY_CIRCLE) {
        ok = gfdxf_finalize_circle(ctx, e);
    } else if (e->type == GFDXF_ENTITY_ARC) {
        ok = gfdxf_finalize_arc(ctx, e);
    } else if (e->type == GFDXF_ENTITY_SPLINE) {
        ok = gfdxf_finalize_spline(ctx, e);
    } else if (e->type == GFDXF_ENTITY_ELLIPSE) {
        ok = gfdxf_emit_ellipse_points(ctx, e);
    } else if (e->type == GFDXF_ENTITY_HATCH) {
        ok = gfdxf_emit_hatch_points(ctx, e);
    } else if (e->type == GFDXF_ENTITY_DIMENSION) {
        ok = gfdxf_finalize_dimension(ctx, e);
    } else if (e->type == GFDXF_ENTITY_TEXT || e->type == GFDXF_ENTITY_MTEXT) {
        ok = gfdxf_finalize_text_common(ctx, e);
    } else if (e->type == GFDXF_ENTITY_ATTDEF || e->type == GFDXF_ENTITY_ATTRIB) {
        ctx->scene->attribute_entities++;
        ok = gfdxf_finalize_text_common(ctx, e);
    } else if (e->type == GFDXF_ENTITY_TOLERANCE) {
        ctx->scene->tolerance_entities++;
        ok = gfdxf_finalize_text_common(ctx, e);
    } else if (e->type == GFDXF_ENTITY_LEADER) {
        ctx->scene->leader_entities++;
        ok = gfdxf_finalize_polyseq(ctx, e, 0);
    } else if (e->type == GFDXF_ENTITY_MLEADER) {
        ctx->scene->mleader_entities++;
        ok = gfdxf_finalize_polyseq(ctx, e, 0);
    } else if (e->type == GFDXF_ENTITY_MLINE) {
        ctx->scene->mline_entities++;
        ok = gfdxf_finalize_polyseq(ctx, e, 0);
    } else if (e->type == GFDXF_ENTITY_RAY) {
        ctx->scene->ray_entities++;
        ok = gfdxf_finalize_ray_common(ctx, e, 0);
    } else if (e->type == GFDXF_ENTITY_XLINE) {
        ctx->scene->xline_entities++;
        ok = gfdxf_finalize_ray_common(ctx, e, 1);
    } else if (e->type == GFDXF_ENTITY_VIEWPORT) {
        ctx->scene->viewport_entities++;
        ok = gfdxf_finalize_viewport(ctx, e);
    } else if (e->type == GFDXF_ENTITY_POINT) {
        ok = gfdxf_finalize_point(ctx, e);
    } else if (e->type == GFDXF_ENTITY_SOLID) {
        ok = gfdxf_finalize_solid(ctx, e);
    } else if (e->type == GFDXF_ENTITY_SHAPE) {
        ctx->scene->shape_entities++;
        ok = gfdxf_finalize_point(ctx, e);
    } else if (e->type == GFDXF_ENTITY_PROXY) {
        ok = gfdxf_finalize_proxy(ctx, e);
    } else if (e->type == GFDXF_ENTITY_PROXY_OBJECT) {
        ok = gfdxf_finalize_proxy_object(ctx, e);
    } else if (e->type == GFDXF_ENTITY_DICTIONARY || e->type == GFDXF_ENTITY_XRECORD || e->type == GFDXF_ENTITY_DICTIONARYVAR || e->type == GFDXF_ENTITY_OBJECT_OTHER || e->type == GFDXF_ENTITY_TABLE) {
        ok = gfdxf_finalize_object_record(ctx, e);
    } else if (e->type == GFDXF_ENTITY_ACIS) {
        if (gfdxf_str_eq_i(e->object_type, "PLANESURFACE") || gfdxf_str_eq_i(e->object_type, "EXTRUDEDSURFACE") || gfdxf_str_eq_i(e->object_type, "LOFTEDSURFACE") || gfdxf_str_eq_i(e->object_type, "REVOLVEDSURFACE") || gfdxf_str_eq_i(e->object_type, "SWEPTSURFACE") || gfdxf_str_eq_i(e->object_type, "NURBSURFACE")) {
            ctx->scene->surface_entities++;
        }
        ok = gfdxf_finalize_acis(ctx, e);
    } else if (e->type == GFDXF_ENTITY_MESH) {
        ok = gfdxf_finalize_mesh_marker(ctx, e);
    } else if (e->type == GFDXF_ENTITY_IMAGE) {
        ok = gfdxf_finalize_image_marker(ctx, e);
    } else if (e->type == GFDXF_ENTITY_COORDINATION_MODEL) {
        ctx->scene->coordination_model_entities++;
        ok = gfdxf_finalize_image_marker(ctx, e);
    } else if (e->type == GFDXF_ENTITY_OLE) {
        ctx->scene->ole_entities++;
        ctx->scene->unhandled_zoo_entities++;
        ok = gfdxf_finalize_image_marker(ctx, e);
    } else if (e->type == GFDXF_ENTITY_GEODATA) {
        ctx->scene->geodata_entities++;
        ctx->scene->generic_object_records++;
        ok = 1;
    } else if (e->type == GFDXF_ENTITY_POINTCLOUD) {
        ctx->scene->pointcloud_entities++;
        ctx->scene->unhandled_zoo_entities++;
        ok = gfdxf_finalize_image_marker(ctx, e);
    } else if (e->type == GFDXF_ENTITY_CONSTRAINT) {
        ctx->scene->constraint_entities++;
        ctx->scene->generic_object_records++;
        ok = 1;
    } else if (e->type == GFDXF_ENTITY_HELIX) {
        ctx->scene->helix_entities++;
        ctx->scene->unhandled_zoo_entities++;
    } else if (e->type == GFDXF_ENTITY_LIGHT) {
        ctx->scene->light_entities++;
        ctx->scene->unhandled_zoo_entities++;
    } else if (e->type == GFDXF_ENTITY_SECTION) {
        ctx->scene->section_entities++;
        ctx->scene->unhandled_zoo_entities++;
    } else if (e->type == GFDXF_ENTITY_OTHER) {
        ctx->scene->ignored_entities++;
        ctx->scene->unhandled_zoo_entities++;
    }
    if (ok && gfdxf_is_standard_decoded_type(e->type)) {
        ctx->scene->standard_decoded_entities++;
    }
    return ok;
}

static void gfdxf_begin_entity(gfdxf_entity *e, int type, gfdxf_parser *parser)
{
    int i;
    gfdxf_entity_clear(e);
    e->type = type;
    if (type == GFDXF_ENTITY_MTEXT) { e->is_mtext = 1; }
    if (type == GFDXF_ENTITY_SPLINE) {
        for (i = 0; i < GFDXF_MAX_SPLINE_POINTS; i++) {
            parser->spline_weights[i] = GFDXF_FX_ONE;
            parser->spline_ctrl[i].x = 0;
            parser->spline_ctrl[i].y = 0;
            parser->spline_ctrl[i].z = 0;
            parser->spline_fit[i].x = 0;
            parser->spline_fit[i].y = 0;
            parser->spline_fit[i].z = 0;
        }
        for (i = 0; i < GFDXF_MAX_SPLINE_KNOTS; i++) {
            parser->spline_knots[i] = 0;
        }
    }
}

static void gfdxf_read_lw_value(gfdxf_scene *scene, gfdxf_parser *parser, gfdxf_entity *e, int code, const char *value)
{
    gfdxf_fx fx;
    int iv;
    if (code == 70) {
        if (gfdxf_parse_int_text(value, &iv)) { e->lw_flags = iv; }
    } else if (code == 38) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->lw_elevation = fx; } else { scene->bad_numbers++; }
    } else if (code == 10) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->lw_cur_x = fx; e->lw_have_x = 1; } else { scene->bad_numbers++; }
    } else if (code == 20) {
        if (e->lw_have_x) {
            if (gfdxf_fx_from_ascii(value, &fx)) {
                if (e->lw_count < GFDXF_MAX_POLY_TMP) {
                    parser->lw_x[e->lw_count] = e->lw_cur_x;
                    parser->lw_y[e->lw_count] = fx;
                    parser->lw_z[e->lw_count] = e->lw_elevation;
                    parser->lw_bulge[e->lw_count] = 0;
                    e->lw_last_index = e->lw_count;
                    e->lw_count++;
                    e->lw_have_x = 0;
                } else { scene->capacity_errors++; }
            } else { scene->bad_numbers++; }
        }
    } else if (code == 42) {
        if (gfdxf_fx_from_ascii(value, &fx)) {
            if (e->lw_last_index >= 0 && e->lw_last_index < GFDXF_MAX_POLY_TMP) { parser->lw_bulge[e->lw_last_index] = fx; }
        } else { scene->bad_numbers++; }
    }
}


static void gfdxf_read_poly3_value(gfdxf_scene *scene, gfdxf_parser *parser, gfdxf_entity *e, int code, const char *value, int xcode)
{
    gfdxf_fx fx;
    if (code == xcode) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->lw_cur_x = fx; e->lw_have_x = 1; e->lw_have_y = 0; } else { scene->bad_numbers++; }
    } else if (code == xcode + 10) {
        if (e->lw_have_x) {
            if (gfdxf_fx_from_ascii(value, &fx)) { e->lw_cur_y = fx; e->lw_have_y = 1; } else { scene->bad_numbers++; }
        }
    } else if (code == xcode + 20) {
        if (e->lw_have_x && e->lw_have_y) {
            if (gfdxf_fx_from_ascii(value, &fx)) {
                if (e->lw_count < GFDXF_MAX_POLY_TMP) {
                    parser->lw_x[e->lw_count] = e->lw_cur_x;
                    parser->lw_y[e->lw_count] = e->lw_cur_y;
                    parser->lw_z[e->lw_count] = fx;
                    parser->lw_bulge[e->lw_count] = 0;
                    e->lw_last_index = e->lw_count;
                    e->lw_count++;
                    e->lw_have_x = 0;
                    e->lw_have_y = 0;
                } else { scene->capacity_errors++; }
            } else { scene->bad_numbers++; }
        }
    }
}

static void gfdxf_read_spline_value(gfdxf_scene *scene, gfdxf_parser *parser, gfdxf_entity *e, int code, const char *value)
{
    int iv;
    gfdxf_fx fx;
    if (code == 70) {
        if (gfdxf_parse_int_text(value, &iv)) { e->spline_flags = iv; }
    } else if (code == 71) {
        if (gfdxf_parse_int_text(value, &iv)) { e->spline_degree = iv; }
    } else if (code == 40) {
        if (gfdxf_fx_from_ascii(value, &fx)) {
            if (e->spline_knot_count < GFDXF_MAX_SPLINE_KNOTS) { parser->spline_knots[e->spline_knot_count++] = fx; } else { scene->capacity_errors++; }
        } else { scene->bad_numbers++; }
    } else if (code == 41) {
        if (gfdxf_fx_from_ascii(value, &fx)) {
            if (e->spline_weight_count < GFDXF_MAX_SPLINE_POINTS) { parser->spline_weights[e->spline_weight_count++] = fx; } else { scene->capacity_errors++; }
        } else { scene->bad_numbers++; }
    } else if (code == 10) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->spline_tmp_x = fx; e->spline_have_ctrl_x = 1; } else { scene->bad_numbers++; }
    } else if (code == 20) {
        if (e->spline_have_ctrl_x) {
            if (gfdxf_fx_from_ascii(value, &fx)) { e->spline_tmp_y = fx; } else { scene->bad_numbers++; }
        }
    } else if (code == 30) {
        if (e->spline_have_ctrl_x) {
            if (gfdxf_fx_from_ascii(value, &fx)) {
                if (e->spline_ctrl_count < GFDXF_MAX_SPLINE_POINTS) {
                    parser->spline_ctrl[e->spline_ctrl_count].x = e->spline_tmp_x;
                    parser->spline_ctrl[e->spline_ctrl_count].y = e->spline_tmp_y;
                    parser->spline_ctrl[e->spline_ctrl_count].z = fx;
                    e->spline_ctrl_last = e->spline_ctrl_count;
                    e->spline_ctrl_count++;
                    e->spline_have_ctrl_x = 0;
                } else { scene->capacity_errors++; }
            } else { scene->bad_numbers++; }
        }
    } else if (code == 11) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->spline_tmp_x = fx; e->spline_have_fit_x = 1; } else { scene->bad_numbers++; }
    } else if (code == 21) {
        if (e->spline_have_fit_x) {
            if (gfdxf_fx_from_ascii(value, &fx)) { e->spline_tmp_y = fx; } else { scene->bad_numbers++; }
        }
    } else if (code == 31) {
        if (e->spline_have_fit_x) {
            if (gfdxf_fx_from_ascii(value, &fx)) {
                if (e->spline_fit_count < GFDXF_MAX_SPLINE_POINTS) {
                    parser->spline_fit[e->spline_fit_count].x = e->spline_tmp_x;
                    parser->spline_fit[e->spline_fit_count].y = e->spline_tmp_y;
                    parser->spline_fit[e->spline_fit_count].z = fx;
                    e->spline_fit_last = e->spline_fit_count;
                    e->spline_fit_count++;
                    e->spline_have_fit_x = 0;
                } else { scene->capacity_errors++; }
            } else { scene->bad_numbers++; }
        }
    }
}


static int gfdxf_hatch_append_point(gfdxf_scene *scene, gfdxf_parser *parser, gfdxf_entity *e, gfdxf_fx x, gfdxf_fx y, gfdxf_fx z)
{
    if (e->hatch_count >= GFDXF_MAX_POLY_TMP) {
        scene->capacity_errors++;
        return 0;
    }
    parser->hatch_x[e->hatch_count] = x;
    parser->hatch_y[e->hatch_count] = y;
    parser->hatch_z[e->hatch_count] = z;
    parser->hatch_bulge[e->hatch_count] = 0;
    e->hatch_last_index = e->hatch_count;
    e->hatch_count++;
    return 1;
}

static void gfdxf_hatch_edge_reset(gfdxf_entity *e, int edge_type)
{
    e->hatch_edge_type = edge_type;
    e->hatch_edge_emitted = 0;
    e->hatch_edge_have_cx = 0;
    e->hatch_edge_have_cy = 0;
    e->hatch_edge_have_sx = 0;
    e->hatch_edge_have_ex = 0;
    e->hatch_edge_have_major_x = 0;
    e->hatch_edge_have_major_y = 0;
    e->hatch_edge_cx = 0;
    e->hatch_edge_cy = 0;
    e->hatch_edge_sx = 0;
    e->hatch_edge_ex = 0;
    e->hatch_edge_radius = 0;
    e->hatch_edge_start_angle = 0;
    e->hatch_edge_end_angle = 0;
    e->hatch_edge_major_x = 0;
    e->hatch_edge_major_y = 0;
    e->hatch_edge_ratio = GFDXF_FX_ONE;
    e->hatch_edge_ccw = 1;
}

static void gfdxf_hatch_append_arc_edge(gfdxf_scene *scene, gfdxf_parser *parser, gfdxf_entity *e)
{
    int i;
    int segs;
    int start_deg;
    int end_deg;
    int sweep;
    if (e->hatch_edge_emitted) { return; }
    if (!e->hatch_edge_have_cx || !e->hatch_edge_have_cy || e->hatch_edge_radius <= 0) { return; }
    start_deg = gfdxf_fx_to_int_nearest(e->hatch_edge_start_angle);
    end_deg = gfdxf_fx_to_int_nearest(e->hatch_edge_end_angle);
    sweep = end_deg - start_deg;
    if (e->hatch_edge_ccw) {
        while (sweep <= 0) { sweep += 360; }
    } else {
        while (sweep >= 0) { sweep -= 360; }
    }
    if (sweep > 360) { sweep = 360; }
    if (sweep < -360) { sweep = -360; }
    segs = sweep >= 0 ? sweep : -sweep;
    segs = (segs * GFDXF_ARC_SEGMENTS + 359) / 360;
    if (segs < 1) { segs = 1; }
    if (segs > GFDXF_ARC_SEGMENTS) { segs = GFDXF_ARC_SEGMENTS; }
    for (i = 0; i <= segs; i++) {
        int deg;
        gfdxf_fx co;
        gfdxf_fx si;
        gfdxf_fx x;
        gfdxf_fx y;
        deg = start_deg + (sweep * i) / segs;
        co = gfdxf_cos_deg_int(deg);
        si = gfdxf_sin_deg_int(deg);
        x = e->hatch_edge_cx + gfdxf_fx_mul(e->hatch_edge_radius, co);
        y = e->hatch_edge_cy + gfdxf_fx_mul(e->hatch_edge_radius, si);
        if (!gfdxf_hatch_append_point(scene, parser, e, x, y, e->hatch_elevation)) { return; }
    }
    e->hatch_edge_emitted = 1;
    scene->tessellated_arcs++;
}

static void gfdxf_hatch_append_ellipse_edge(gfdxf_scene *scene, gfdxf_parser *parser, gfdxf_entity *e)
{
    int i;
    int segs;
    int start_deg;
    int end_deg;
    int sweep;
    gfdxf_fx minor_x;
    gfdxf_fx minor_y;
    if (e->hatch_edge_emitted) { return; }
    if (!e->hatch_edge_have_cx || !e->hatch_edge_have_cy || !e->hatch_edge_have_major_x || !e->hatch_edge_have_major_y) { return; }
    start_deg = gfdxf_fx_to_int_nearest(e->hatch_edge_start_angle);
    end_deg = gfdxf_fx_to_int_nearest(e->hatch_edge_end_angle);
    sweep = end_deg - start_deg;
    if (e->hatch_edge_ccw) {
        while (sweep <= 0) { sweep += 360; }
    } else {
        while (sweep >= 0) { sweep -= 360; }
    }
    if (sweep > 360) { sweep = 360; }
    if (sweep < -360) { sweep = -360; }
    segs = sweep >= 0 ? sweep : -sweep;
    segs = (segs * GFDXF_ELLIPSE_SEGMENTS + 359) / 360;
    if (segs < 1) { segs = 1; }
    if (segs > GFDXF_ELLIPSE_SEGMENTS) { segs = GFDXF_ELLIPSE_SEGMENTS; }
    minor_x = -gfdxf_fx_mul(e->hatch_edge_major_y, e->hatch_edge_ratio);
    minor_y = gfdxf_fx_mul(e->hatch_edge_major_x, e->hatch_edge_ratio);
    for (i = 0; i <= segs; i++) {
        int deg;
        gfdxf_fx co;
        gfdxf_fx si;
        gfdxf_fx x;
        gfdxf_fx y;
        deg = start_deg + (sweep * i) / segs;
        co = gfdxf_cos_deg_int(deg);
        si = gfdxf_sin_deg_int(deg);
        x = e->hatch_edge_cx + gfdxf_fx_mul(e->hatch_edge_major_x, co) + gfdxf_fx_mul(minor_x, si);
        y = e->hatch_edge_cy + gfdxf_fx_mul(e->hatch_edge_major_y, co) + gfdxf_fx_mul(minor_y, si);
        if (!gfdxf_hatch_append_point(scene, parser, e, x, y, e->hatch_elevation)) { return; }
    }
    e->hatch_edge_emitted = 1;
    scene->tessellated_ellipses++;
}

static void gfdxf_read_hatch_value(gfdxf_scene *scene, gfdxf_parser *parser, gfdxf_entity *e, int code, const char *value)
{
    int iv;
    gfdxf_fx fx;
    if (code == 92) {
        scene->hatch_edge_paths++;
        if (gfdxf_parse_int_text(value, &iv)) {
            e->hatch_path_is_polyline = (iv & 2) ? 1 : 0;
            if (e->hatch_path_is_polyline) { scene->hatch_polyline_paths++; gfdxf_hatch_edge_reset(e, 0); }
        } else { scene->bad_numbers++; }
    } else if (code == 72) {
        if (gfdxf_parse_int_text(value, &iv)) {
            if (e->hatch_path_is_polyline) {
                e->hatch_polyline_has_bulge = iv ? 1 : 0;
            } else {
                gfdxf_hatch_edge_reset(e, iv);
                if (iv == 1) { scene->hatch_line_edges++; }
                else if (iv == 2) { scene->hatch_arc_edges++; }
                else if (iv == 3) { scene->hatch_ellipse_edges++; }
                else if (iv == 4) { scene->hatch_spline_edges++; }
            }
        } else { scene->bad_numbers++; }
    } else if (code == 70) {
        if (gfdxf_parse_int_text(value, &iv)) { e->hatch_solid = iv; } else { scene->bad_numbers++; }
    } else if (code == 91) {
        if (gfdxf_parse_int_text(value, &iv)) { e->hatch_loops = iv; } else { scene->bad_numbers++; }
    } else if (code == 73) {
        if (gfdxf_parse_int_text(value, &iv)) {
            if (e->hatch_edge_type == 2 || e->hatch_edge_type == 3) { e->hatch_edge_ccw = iv ? 1 : 0; }
            else { e->hatch_closed = iv ? 1 : 0; }
        } else { scene->bad_numbers++; }
    } else if (code == 75 || code == 76 || code == 52 || code == 53 || code == 43 || code == 44 || code == 45 || code == 46 || code == 49 || code == 79) {
        scene->hatch_pattern_tags++;
    } else if (code >= 450 && code <= 470) {
        scene->hatch_gradient_tags++;
    } else if (code == 97 || code == 330) {
        scene->hatch_source_boundary_refs++;
    } else if (code == 38 || code == 30) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->hatch_elevation = fx; } else { scene->bad_numbers++; }
    } else if (code == 10) {
        if (gfdxf_fx_from_ascii(value, &fx)) {
            if (e->hatch_edge_type == 2 || e->hatch_edge_type == 3) { e->hatch_edge_cx = fx; e->hatch_edge_have_cx = 1; }
            else { e->hatch_tmp_x = fx; e->hatch_have_x = 1; if (e->hatch_edge_type == 1 || e->hatch_edge_type == 4) { e->hatch_edge_sx = fx; e->hatch_edge_have_sx = 1; } }
        } else { scene->bad_numbers++; }
    } else if (code == 20) {
        if (gfdxf_fx_from_ascii(value, &fx)) {
            if (e->hatch_edge_type == 2 || e->hatch_edge_type == 3) { e->hatch_edge_cy = fx; e->hatch_edge_have_cy = 1; }
            else if (e->hatch_have_x) {
                gfdxf_hatch_append_point(scene, parser, e, e->hatch_tmp_x, fx, e->hatch_elevation);
                if (e->hatch_edge_type == 4) { scene->hatch_spline_vertices++; }
                e->hatch_have_x = 0;
            }
        } else { scene->bad_numbers++; }
    } else if (code == 11) {
        if (gfdxf_fx_from_ascii(value, &fx)) {
            if (e->hatch_edge_type == 3) { e->hatch_edge_major_x = fx; e->hatch_edge_have_major_x = 1; }
            else { e->hatch_tmp_x = fx; e->hatch_have_x = 1; e->hatch_edge_ex = fx; e->hatch_edge_have_ex = 1; }
        } else { scene->bad_numbers++; }
    } else if (code == 21) {
        if (gfdxf_fx_from_ascii(value, &fx)) {
            if (e->hatch_edge_type == 3) { e->hatch_edge_major_y = fx; e->hatch_edge_have_major_y = 1; }
            else if (e->hatch_have_x) {
                gfdxf_hatch_append_point(scene, parser, e, e->hatch_tmp_x, fx, e->hatch_elevation);
                if (e->hatch_edge_type == 4) { scene->hatch_spline_vertices++; }
                e->hatch_have_x = 0;
            }
        } else { scene->bad_numbers++; }
    } else if (code == 40) {
        if (gfdxf_fx_from_ascii(value, &fx)) {
            if (e->hatch_edge_type == 2) { e->hatch_edge_radius = fx; }
            else if (e->hatch_edge_type == 3) { e->hatch_edge_ratio = fx; }
        } else { scene->bad_numbers++; }
    } else if (code == 50) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->hatch_edge_start_angle = fx; } else { scene->bad_numbers++; }
    } else if (code == 51) {
        if (gfdxf_fx_from_ascii(value, &fx)) {
            e->hatch_edge_end_angle = fx;
            if (e->hatch_edge_type == 2) { gfdxf_hatch_append_arc_edge(scene, parser, e); }
            else if (e->hatch_edge_type == 3) { gfdxf_hatch_append_ellipse_edge(scene, parser, e); }
        } else { scene->bad_numbers++; }
    } else if (code == 42) {
        if (gfdxf_fx_from_ascii(value, &fx)) {
            if (e->hatch_last_index >= 0 && e->hatch_last_index < GFDXF_MAX_POLY_TMP) { parser->hatch_bulge[e->hatch_last_index] = fx; }
        } else { scene->bad_numbers++; }
    }
}

static void gfdxf_read_text_value(gfdxf_scene *scene, gfdxf_entity *e, int code, const char *value)
{
    gfdxf_fx fx;
    if (code == 1 || code == 3) {
        gfdxf_text_append(e, value);
    } else if (code == 7) {
        gfdxf_copy_name(e->text_style, value);
    } else if (code == 40) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->text_height = fx; } else { scene->bad_numbers++; }
    } else if (code == 41) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->text_width = fx; } else { scene->bad_numbers++; }
    } else if (code == 42 && e->type == GFDXF_ENTITY_MTEXT) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->text_width = fx; } else { scene->bad_numbers++; }
    } else if (code == 50) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->text_rotation = fx; } else { scene->bad_numbers++; }
    }
}

static int gfdxf_layer_find_parser(const gfdxf_parser *parser, const char *name)
{
    int i;
    if (parser == 0 || name == 0) { return -1; }
    for (i = 0; i < parser->layer_count; i++) {
        if (gfdxf_str_eq_i(parser->layers[i].name, name)) { return i; }
    }
    return -1;
}

static void gfdxf_class_state_clear(gfdxf_class_state *cs)
{
    memset(cs, 0, sizeof(gfdxf_class_state));
}

static void gfdxf_finish_class(gfdxf_scene *scene, gfdxf_parser *parser, gfdxf_class_state *cs)
{
    int idx;
    if (!cs->in_class) { return; }
    if (cs->cur_class.dxf_record[0] != '\0' || cs->cur_class.cpp_name[0] != '\0') {
        if (parser->class_count < GFDXF_MAX_CLASSES) {
            idx = parser->class_count;
            cs->cur_class.class_id = 500 + idx;
            parser->classes[idx] = cs->cur_class;
            parser->class_count++;
            scene->class_records++;
        } else {
            scene->capacity_errors++;
        }
    }
    gfdxf_class_state_clear(cs);
}

static void gfdxf_read_class_value(gfdxf_scene *scene, gfdxf_class_state *cs, int code, const char *value)
{
    int iv;
    if (!cs->in_class) { return; }
    if (code == 1) {
        gfdxf_copy_name(cs->cur_class.dxf_record, value);
    } else if (code == 2) {
        gfdxf_copy_name(cs->cur_class.cpp_name, value);
    } else if (code == 3) {
        gfdxf_copy_name(cs->cur_class.app_name, value);
    } else if (code == 90) {
        if (gfdxf_parse_int_text(value, &iv)) { cs->cur_class.flags = iv; } else { scene->bad_numbers++; }
    } else if (code == 280) {
        if (gfdxf_parse_int_text(value, &iv)) { cs->cur_class.proxy_flags_280 = iv; } else { scene->bad_numbers++; }
    } else if (code == 281) {
        if (gfdxf_parse_int_text(value, &iv)) { cs->cur_class.proxy_flags_281 = iv; } else { scene->bad_numbers++; }
    }
}

static void gfdxf_copy_classes_to_scene(gfdxf_scene *scene, const gfdxf_parser *parser)
{
    int i;
    scene->class_count = 0;
    scene->class_acdb_records = 0;
    scene->class_custom_records = 0;
    scene->class_objectdbx_records = 0;
    for (i = 0; i < parser->class_count && i < GFDXF_MAX_CLASSES; i++) {
        scene->classes[i] = parser->classes[i];
        scene->class_count++;
        if (gfdxf_contains_i(scene->classes[i].cpp_name, "ACDB") || gfdxf_contains_i(scene->classes[i].dxf_record, "ACDB")) {
            scene->class_acdb_records++;
        } else if (gfdxf_contains_i(scene->classes[i].app_name, "OBJECTDBX") || gfdxf_contains_i(scene->classes[i].cpp_name, "OBJECTDBX")) {
            scene->class_objectdbx_records++;
        } else {
            scene->class_custom_records++;
        }
    }
}

int gfdxf_find_class_by_id(const gfdxf_scene *scene, int class_id)
{
    int i;
    if (scene == 0) { return -1; }
    for (i = 0; i < scene->class_count; i++) {
        if (scene->classes[i].class_id == class_id) { return i; }
    }
    return -1;
}

static void gfdxf_post_analyze_payloads(gfdxf_scene *scene)
{
    int i;
    int ci;
    gfdxf_payload *pl;
    if (scene == 0) { return; }
    for (i = 0; i < scene->payload_count; i++) {
        pl = &scene->payloads[i];
        ci = gfdxf_find_class_by_id(scene, pl->class_id);
        pl->proxy_class_index = ci;
        pl->diagnostic_flags = 0;
        if (ci >= 0) {
            gfdxf_copy_name(pl->resolved_dxf_record, scene->classes[ci].dxf_record);
            gfdxf_copy_name(pl->resolved_cpp_name, scene->classes[ci].cpp_name);
            gfdxf_copy_name(pl->resolved_app_name, scene->classes[ci].app_name);
            pl->diagnostic_flags |= GFDXF_PAYLOAD_DIAG_HAS_CLASS;
            scene->payloads_with_resolved_classes++;
        } else if (pl->class_id != 0) {
            pl->diagnostic_flags |= GFDXF_PAYLOAD_DIAG_CLASS_UNRESOLVED;
            scene->payloads_with_unresolved_classes++;
        }
        if (pl->stored_hex_chars < pl->total_hex_chars) {
            pl->diagnostic_flags |= GFDXF_PAYLOAD_DIAG_HEX_TRUNCATED;
            scene->payloads_with_truncated_previews++;
        }
        if (pl->handle_count > 0) {
            pl->diagnostic_flags |= GFDXF_PAYLOAD_DIAG_HAS_HANDLES;
            scene->payloads_with_handles++;
        }
        if (pl->acis_topology_score > 0) {
            pl->diagnostic_flags |= GFDXF_PAYLOAD_DIAG_HAS_ACIS_TOPO;
            scene->acis_topology_score += pl->acis_topology_score;
        }
        if (pl->total_hex_chars > 0 && pl->acis_line_count == 0) {
            pl->diagnostic_flags |= GFDXF_PAYLOAD_DIAG_BINARY_ONLY;
            scene->payloads_binary_only++;
        }
        if (pl->kind == GFDXF_PAYLOAD_PROXY_ENTITY || pl->kind == GFDXF_PAYLOAD_PROXY_OBJECT) {
            if (ci >= 0) { scene->objectarx_proxy_resolved++; }
            else { scene->objectarx_proxy_unresolved++; }
            pl->proxy_has_graphics = pl->graphics_size_bytes > 0 || pl->total_hex_chars > 0;
            pl->proxy_has_entity_data = pl->data_size_bits > 0;
            pl->proxy_requires_enabler = 1;
            pl->diagnostic_flags |= GFDXF_PAYLOAD_DIAG_REQUIRES_ENABLER;
            scene->objectarx_enabler_required++;
            if (pl->graphics_size_bytes > 0) { scene->proxy_graphics_sections++; }
            if (pl->data_size_bits > 0) { scene->proxy_entity_data_sections++; }
        }
    }
}

static void gfdxf_table_state_clear(gfdxf_table_state *ts)
{
    memset(ts, 0, sizeof(gfdxf_table_state));
    ts->cur_layer.color = 7;
    ts->cur_layer.lineweight = -3;
}

static void gfdxf_finish_layer(gfdxf_scene *scene, gfdxf_parser *parser, gfdxf_table_state *ts)
{
    int idx;
    if (!ts->in_layer) { return; }
    if (ts->cur_layer.name[0] != '\0' && gfdxf_layer_find_parser(parser, ts->cur_layer.name) < 0) {
        if (parser->layer_count < GFDXF_MAX_LAYERS) {
            idx = parser->layer_count;
            parser->layers[idx] = ts->cur_layer;
            parser->layer_count++;
            scene->table_records++;
        } else {
            scene->capacity_errors++;
        }
    }
    ts->in_layer = 0;
    memset(&ts->cur_layer, 0, sizeof(ts->cur_layer));
    ts->cur_layer.color = 7;
    ts->cur_layer.lineweight = -3;
}

static void gfdxf_read_layer_value(gfdxf_scene *scene, gfdxf_table_state *ts, int code, const char *value)
{
    int iv;
    if (!ts->in_layer) { return; }
    if (code == 2) {
        gfdxf_copy_name(ts->cur_layer.name, value);
    } else if (code == 62) {
        if (gfdxf_parse_int_text(value, &iv)) { ts->cur_layer.color = iv; }
    } else if (code == 70) {
        if (gfdxf_parse_int_text(value, &iv)) { ts->cur_layer.flags = iv; }
    } else if (code == 370) {
        if (gfdxf_parse_int_text(value, &iv)) { ts->cur_layer.lineweight = iv; }
    } else {
        (void)scene;
    }
}

static void gfdxf_read_payload_value(gfdxf_scene *scene, gfdxf_entity *e, int code, const char *value)
{
    int iv;
    if (code == 91) {
        if (gfdxf_parse_int_text(value, &iv)) { e->payload_class_id = iv; } else { scene->bad_numbers++; }
    } else if (code == 90 && e->payload_class_id == 0) {
        if (gfdxf_parse_int_text(value, &iv)) { e->payload_class_id = iv; } else { scene->bad_numbers++; }
    } else if (code == 92) {
        if (gfdxf_parse_int_text(value, &iv)) { e->payload_graphics_bytes = iv; } else { scene->bad_numbers++; }
    } else if (code == 93) {
        if (gfdxf_parse_int_text(value, &iv)) { e->payload_data_bits = iv; } else { scene->bad_numbers++; }
    } else if (code == 95) {
        if (gfdxf_parse_int_text(value, &iv)) { e->payload_drawing_format = iv; } else { scene->bad_numbers++; }
    } else if (code == 70) {
        if (gfdxf_parse_int_text(value, &iv)) { e->payload_data_format = iv; } else { scene->bad_numbers++; }
    } else if (code == 310) {
        gfdxf_entity_append_hex(e, value);
    } else if (code == 330 || code == 340 || code == 350 || code == 360) {
        gfdxf_entity_append_handle(e, value);
    }
}

static void gfdxf_read_acis_value(gfdxf_scene *scene, gfdxf_entity *e, int code, const char *value)
{
    (void)scene;
    if (code == 1 || code == 3) {
        gfdxf_entity_append_acis_text(e, value);
    } else if (code == 310) {
        gfdxf_entity_append_hex(e, value);
    } else if (code == 330 || code == 340 || code == 350 || code == 360) {
        gfdxf_entity_append_handle(e, value);
    }
}


static void gfdxf_read_header_value(gfdxf_scene *scene, gfdxf_parser *parser, int code, const char *value)
{
    int iv;
    gfdxf_fx fx;
    if (code == 9) {
        gfdxf_copy_name(parser->header_var, value);
        scene->header_vars++;
        return;
    }
    if (parser->header_var[0] == '\0') { return; }
    if (gfdxf_str_eq_i(parser->header_var, "$ACADVER") && code == 1) {
        gfdxf_copy_text_cap(scene->acadver, GFDXF_MAX_HEADER_CHARS, value);
    } else if (gfdxf_str_eq_i(parser->header_var, "$INSUNITS") && code == 70) {
        if (gfdxf_parse_int_text(value, &iv)) { scene->header_insunits = iv; } else { scene->bad_numbers++; }
    } else if (gfdxf_str_eq_i(parser->header_var, "$MEASUREMENT") && code == 70) {
        if (gfdxf_parse_int_text(value, &iv)) { scene->header_measurement = iv; } else { scene->bad_numbers++; }
    } else if (gfdxf_str_eq_i(parser->header_var, "$LTSCALE") && code == 40) {
        if (gfdxf_fx_from_ascii(value, &fx)) { scene->header_ltscale = fx; } else { scene->bad_numbers++; }
    } else if (gfdxf_str_eq_i(parser->header_var, "$TEXTSIZE") && code == 40) {
        if (gfdxf_fx_from_ascii(value, &fx)) { scene->header_textsize = fx; } else { scene->bad_numbers++; }
    } else if (gfdxf_str_eq_i(parser->header_var, "$DIMSCALE") && code == 40) {
        if (gfdxf_fx_from_ascii(value, &fx)) { scene->header_dimscale = fx; } else { scene->bad_numbers++; }
    } else if (gfdxf_str_eq_i(parser->header_var, "$DIMTXT") && code == 40) {
        if (gfdxf_fx_from_ascii(value, &fx)) { scene->header_dimtxt = fx; } else { scene->bad_numbers++; }
    } else if (gfdxf_str_eq_i(parser->header_var, "$EXTMIN")) {
        if (code == 10) { if (gfdxf_fx_from_ascii(value, &fx)) { scene->header_extmin.x = fx; scene->header_has_extmin = 1; } else { scene->bad_numbers++; } }
        else if (code == 20) { if (gfdxf_fx_from_ascii(value, &fx)) { scene->header_extmin.y = fx; } else { scene->bad_numbers++; } }
        else if (code == 30) { if (gfdxf_fx_from_ascii(value, &fx)) { scene->header_extmin.z = fx; } else { scene->bad_numbers++; } }
    } else if (gfdxf_str_eq_i(parser->header_var, "$EXTMAX")) {
        if (code == 10) { if (gfdxf_fx_from_ascii(value, &fx)) { scene->header_extmax.x = fx; scene->header_has_extmax = 1; } else { scene->bad_numbers++; } }
        else if (code == 20) { if (gfdxf_fx_from_ascii(value, &fx)) { scene->header_extmax.y = fx; } else { scene->bad_numbers++; } }
        else if (code == 30) { if (gfdxf_fx_from_ascii(value, &fx)) { scene->header_extmax.z = fx; } else { scene->bad_numbers++; } }
    }
}

static void gfdxf_copy_layers_to_scene(gfdxf_scene *scene, const gfdxf_parser *parser)
{
    int i;
    scene->layer_count = 0;
    for (i = 0; i < parser->layer_count && i < GFDXF_MAX_LAYERS; i++) {
        scene->layers[i] = parser->layers[i];
        scene->layer_count++;
    }
}


static void gfdxf_read_mesh_value(gfdxf_scene *scene, gfdxf_parser *parser, gfdxf_entity *e, int code, const char *value)
{
    gfdxf_fx fx;
    int iv;
    if (code == 91) {
        if (gfdxf_parse_int_text(value, &iv)) { e->mesh_subdiv_level = iv; } else { scene->bad_numbers++; }
    } else if (code == 94 || code == 95 || code == 140) {
        e->mesh_read_faces = 0;
        if (code == 94 || code == 95) { scene->mesh_edge_items++; }
        if (code == 140) { scene->mesh_crease_edges++; }
    } else if (code == 93) {
        e->mesh_read_faces = 1;
    } else if (code == 10) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->mesh_tmp_x = fx; e->mesh_have_x = 1; e->mesh_have_y = 0; } else { scene->bad_numbers++; }
    } else if (code == 20) {
        if (e->mesh_have_x) { if (gfdxf_fx_from_ascii(value, &fx)) { e->mesh_tmp_y = fx; e->mesh_have_y = 1; } else { scene->bad_numbers++; } }
    } else if (code == 30) {
        if (e->mesh_have_x && e->mesh_have_y) {
            if (gfdxf_fx_from_ascii(value, &fx)) {
                if (e->mesh_vertex_count < GFDXF_MAX_POLY_TMP) {
                    parser->poly_x[e->mesh_vertex_count] = e->mesh_tmp_x;
                    parser->poly_y[e->mesh_vertex_count] = e->mesh_tmp_y;
                    parser->poly_z[e->mesh_vertex_count] = fx;
                    e->mesh_vertex_count++;
                } else { scene->capacity_errors++; }
                e->mesh_have_x = 0;
                e->mesh_have_y = 0;
            } else { scene->bad_numbers++; }
        }
    } else if (code == 90 && e->mesh_read_faces) {
        if (gfdxf_parse_int_text(value, &iv)) {
            if (e->mesh_face_int_count < GFDXF_MAX_POLY_TMP) {
                parser->poly_scene_index[e->mesh_face_int_count] = iv;
                e->mesh_face_int_count++;
                scene->mesh_face_list_items++;
            } else { scene->capacity_errors++; }
        } else { scene->bad_numbers++; }
    }
}

static void gfdxf_read_object_value(gfdxf_scene *scene, gfdxf_entity *e, int code, const char *value)
{
    int iv;
    if (code == 5 || code == 105) {
        gfdxf_copy_name(e->object_handle, value);
    } else if (code == 330) {
        if (e->object_owner[0] == '\0') { gfdxf_copy_name(e->object_owner, value); }
        gfdxf_entity_append_handle(e, value);
    } else if (code == 340 || code == 350 || code == 360) {
        gfdxf_entity_append_handle(e, value);
    } else if (code == 3) {
        if (e->object_key[0] == '\0') { gfdxf_copy_name(e->object_key, value); }
        e->object_text_chunks++;
        gfdxf_append_text_cap(e->object_first_text, GFDXF_MAX_OBJECT_TEXT_CHARS, &e->object_text_len, value);
    } else if (code == 1 || code == 2 || (code >= 300 && code <= 309)) {
        e->object_text_chunks++;
        gfdxf_append_text_cap(e->object_first_text, GFDXF_MAX_OBJECT_TEXT_CHARS, &e->object_text_len, value);
    } else if (code == 90 || code == 91 || code == 92 || code == 93 || code == 94 || code == 95 || code == 70 || code == 71 || code == 280 || code == 281 || code == 290) {
        if (gfdxf_parse_int_text(value, &iv)) { e->object_int_values++; } else { scene->bad_numbers++; }
    } else if (code == 310) {
        e->object_binary_chunks++;
        gfdxf_entity_append_hex(e, value);
    }
}

static void gfdxf_read_entity_value(gfdxf_scene *scene, gfdxf_parser *parser, gfdxf_entity *e, int code, const char *value)
{
    int iv;
    gfdxf_fx fx;
    if (code == 8) { gfdxf_copy_name(e->layer, value); }
    if (code == 67) { if (gfdxf_parse_int_text(value, &iv)) { if (iv) { scene->paperspace_entities++; } else { scene->modelspace_entities++; } } else { scene->bad_numbers++; } }
    if (code == 330) { scene->named_owner_refs++; }
    if (code == 360) { scene->extension_dictionary_refs++; }
    if (code == 390) { scene->plotstyle_handles++; }
    if (code == 410) { scene->layout_handles++; }
    if (code == 430) { scene->color_book_tags++; }
    if (code == 3 && e->type == GFDXF_ENTITY_DIMENSION) { scene->dimension_style_refs++; }
    if (code == 5 || code == 105 || code == 330 || code == 340 || code == 350 || code == 360) { scene->common_handle_tags++; }
    if (code == 100) { scene->common_subclass_tags++; }
    if (code == 102) { scene->common_appdata_tags++; }
    if (code == 330 || code == 331) { scene->reactor_handle_tags++; }
    if (code == 6) { scene->entity_linetype_tags++; scene->standard_metadata_tags++; }
    if (code == 370) { scene->entity_lineweight_tags++; scene->standard_metadata_tags++; }
    if (code == 420) { scene->entity_true_color_tags++; scene->standard_metadata_tags++; }
    if (code == 440) { scene->entity_transparency_tags++; scene->standard_metadata_tags++; }
    if (code == 347) { scene->entity_material_handles++; scene->standard_metadata_tags++; }
    if (code >= 1000 && code <= 1071) { gfdxf_count_xdata_tag(scene, code); }
    if (e->type == GFDXF_ENTITY_LINE || e->type == GFDXF_ENTITY_3DFACE || e->type == GFDXF_ENTITY_VERTEX || e->type == GFDXF_ENTITY_INSERT || e->type == GFDXF_ENTITY_CIRCLE || e->type == GFDXF_ENTITY_ARC || e->type == GFDXF_ENTITY_ELLIPSE || e->type == GFDXF_ENTITY_DIMENSION || e->type == GFDXF_ENTITY_TEXT || e->type == GFDXF_ENTITY_MTEXT || e->type == GFDXF_ENTITY_ATTDEF || e->type == GFDXF_ENTITY_ATTRIB || e->type == GFDXF_ENTITY_TOLERANCE || e->type == GFDXF_ENTITY_VIEWPORT || e->type == GFDXF_ENTITY_RAY || e->type == GFDXF_ENTITY_XLINE || e->type == GFDXF_ENTITY_POINT || e->type == GFDXF_ENTITY_SOLID || e->type == GFDXF_ENTITY_SHAPE || e->type == GFDXF_ENTITY_IMAGE || e->type == GFDXF_ENTITY_COORDINATION_MODEL || e->type == GFDXF_ENTITY_OLE || e->type == GFDXF_ENTITY_POINTCLOUD) {
        gfdxf_store_point_value(scene, e, code, value);
    }
    if (code == 210 || code == 220 || code == 230) {
        if (code == 210) { scene->extrusion_vectors++; }
        gfdxf_store_extrusion_value(scene, e, code, value);
    }
    if (e->type == GFDXF_ENTITY_3DFACE && code == 70) {
        if (gfdxf_parse_int_text(value, &iv)) { e->invisible_edges = iv; }
    } else if (e->type == GFDXF_ENTITY_POLYLINE && code == 70) {
        if (gfdxf_parse_int_text(value, &iv)) { e->poly_flags = iv; }
    } else if (e->type == GFDXF_ENTITY_VERTEX && code == 70) {
        if (gfdxf_parse_int_text(value, &iv)) { e->vertex_flags = iv; }
    } else if (e->type == GFDXF_ENTITY_VERTEX && code >= 71 && code <= 74) {
        if (gfdxf_parse_int_text(value, &iv)) { e->face_indices[code - 71] = iv; }
    } else if (e->type == GFDXF_ENTITY_VERTEX && code == 42) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->vertex_bulge = fx; } else { scene->bad_numbers++; }
    } else if (e->type == GFDXF_ENTITY_LWPOLYLINE) {
        gfdxf_read_lw_value(scene, parser, e, code, value);
    } else if (e->type == GFDXF_ENTITY_INSERT) {
        if (code == 2) { gfdxf_copy_name(e->name, value); }
        else if (code == 41) { if (gfdxf_fx_from_ascii(value, &fx)) { e->scale[0] = fx; } else { scene->bad_numbers++; } }
        else if (code == 42) { if (gfdxf_fx_from_ascii(value, &fx)) { e->scale[1] = fx; } else { scene->bad_numbers++; } }
        else if (code == 43) { if (gfdxf_fx_from_ascii(value, &fx)) { e->scale[2] = fx; } else { scene->bad_numbers++; } }
        else if (code == 50) { if (gfdxf_fx_from_ascii(value, &fx)) { e->rotation = fx; } else { scene->bad_numbers++; } }
        else if (code == 70) { if (gfdxf_parse_int_text(value, &iv)) { e->columns = iv; } }
        else if (code == 71) { if (gfdxf_parse_int_text(value, &iv)) { e->rows = iv; } }
        else if (code == 44) { if (gfdxf_fx_from_ascii(value, &fx)) { e->column_spacing = fx; } else { scene->bad_numbers++; } }
        else if (code == 45) { if (gfdxf_fx_from_ascii(value, &fx)) { e->row_spacing = fx; } else { scene->bad_numbers++; } }
    } else if ((e->type == GFDXF_ENTITY_CIRCLE || e->type == GFDXF_ENTITY_ARC) && code == 40) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->radius = fx; } else { scene->bad_numbers++; }
    } else if (e->type == GFDXF_ENTITY_ARC && code == 50) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->start_angle = fx; } else { scene->bad_numbers++; }
    } else if (e->type == GFDXF_ENTITY_ARC && code == 51) {
        if (gfdxf_fx_from_ascii(value, &fx)) { e->end_angle = fx; } else { scene->bad_numbers++; }
    } else if (e->type == GFDXF_ENTITY_SPLINE) {
        gfdxf_read_spline_value(scene, parser, e, code, value);
    } else if (e->type == GFDXF_ENTITY_ELLIPSE) {
        if (code == 40) { if (gfdxf_fx_from_ascii(value, &fx)) { e->ellipse_ratio = fx; } else { scene->bad_numbers++; } }
        else if (code == 41) { if (gfdxf_fx_from_ascii(value, &fx)) { e->ellipse_start = fx; } else { scene->bad_numbers++; } }
        else if (code == 42) { if (gfdxf_fx_from_ascii(value, &fx)) { e->ellipse_end = fx; } else { scene->bad_numbers++; } }
    } else if (e->type == GFDXF_ENTITY_HATCH) {
        gfdxf_read_hatch_value(scene, parser, e, code, value);
    } else if (e->type == GFDXF_ENTITY_DIMENSION) {
        if (code == 1 || code == 3) { gfdxf_text_append(e, value); }
        else if (code == 70) { if (gfdxf_parse_int_text(value, &iv)) { e->dim_type = iv; } }
        else if (code == 40) { if (gfdxf_fx_from_ascii(value, &fx)) { e->text_height = fx; } else { scene->bad_numbers++; } }
        else if (code == 41) { if (gfdxf_fx_from_ascii(value, &fx)) { e->text_width = fx; } else { scene->bad_numbers++; } }
        else if (code == 50) { if (gfdxf_fx_from_ascii(value, &fx)) { e->text_rotation = fx; } else { scene->bad_numbers++; } }
    } else if (e->type == GFDXF_ENTITY_TEXT || e->type == GFDXF_ENTITY_MTEXT || e->type == GFDXF_ENTITY_ATTDEF || e->type == GFDXF_ENTITY_ATTRIB || e->type == GFDXF_ENTITY_TOLERANCE) {
        gfdxf_read_text_value(scene, e, code, value);
    } else if (e->type == GFDXF_ENTITY_LEADER || e->type == GFDXF_ENTITY_MLEADER) {
        if (code == 10 || code == 20 || code == 30) { gfdxf_read_poly3_value(scene, parser, e, code, value, 10); }
        else if (code == 72) { if (gfdxf_parse_int_text(value, &iv) && iv == 1) { scene->tessellated_splines++; } }
    } else if (e->type == GFDXF_ENTITY_MLINE) {
        if (code == 11 || code == 21 || code == 31) { gfdxf_read_poly3_value(scene, parser, e, code, value, 11); }
    } else if (e->type == GFDXF_ENTITY_VIEWPORT) {
        if (code == 40) { if (gfdxf_fx_from_ascii(value, &fx)) { e->text_width = fx; } else { scene->bad_numbers++; } }
        else if (code == 41) { if (gfdxf_fx_from_ascii(value, &fx)) { e->text_height = fx; } else { scene->bad_numbers++; } }
    } else if (e->type == GFDXF_ENTITY_MESH) {
        gfdxf_read_mesh_value(scene, parser, e, code, value);
    } else if (e->type == GFDXF_ENTITY_DICTIONARY || e->type == GFDXF_ENTITY_XRECORD || e->type == GFDXF_ENTITY_DICTIONARYVAR || e->type == GFDXF_ENTITY_OBJECT_OTHER || e->type == GFDXF_ENTITY_TABLE) {
        gfdxf_read_object_value(scene, e, code, value);
    } else if (e->type == GFDXF_ENTITY_PROXY || e->type == GFDXF_ENTITY_PROXY_OBJECT) {
        gfdxf_read_payload_value(scene, e, code, value);
    } else if (e->type == GFDXF_ENTITY_ACIS) {
        gfdxf_read_acis_value(scene, e, code, value);
    }
}

static void gfdxf_ctx_setup(gfdxf_emit_ctx *ctx, int target, gfdxf_scene *scene, gfdxf_parser *parser, int block_index, const gfdxf_callbacks *callbacks, void *user)
{
    ctx->target = target;
    ctx->scene = scene;
    ctx->parser = parser;
    ctx->block_index = block_index;
    ctx->callbacks = callbacks;
    ctx->user = user;
    gfdxf_xform_identity(&ctx->xf);
}

static int gfdxf_start_block(gfdxf_scene *scene, gfdxf_parser *parser)
{
    int idx;
    if (parser->block_count >= GFDXF_MAX_BLOCKS) {
        scene->capacity_errors++;
        return -1;
    }
    idx = parser->block_count;
    memset(&parser->blocks[idx], 0, sizeof(gfdxf_block));
    parser->blocks[idx].vertex_start = parser->block_vertex_count;
    parser->blocks[idx].line_start = parser->block_line_count;
    parser->blocks[idx].face_start = parser->block_face_count;
    parser->blocks[idx].insert_start = parser->block_insert_count;
    parser->block_count++;
    return idx;
}

static void gfdxf_read_block_header(gfdxf_scene *scene, gfdxf_parser *parser, int block_index, int code, const char *value)
{
    gfdxf_fx fx;
    if (block_index < 0 || block_index >= parser->block_count) {
        return;
    }
    if (code == 2) {
        gfdxf_copy_name(parser->blocks[block_index].name, value);
    } else if (code == 3 && parser->blocks[block_index].name[0] == '\0') {
        gfdxf_copy_name(parser->blocks[block_index].name, value);
    } else if (code == 10) {
        if (gfdxf_fx_from_ascii(value, &fx)) { parser->blocks[block_index].base.x = fx; } else { scene->bad_numbers++; }
    } else if (code == 20) {
        if (gfdxf_fx_from_ascii(value, &fx)) { parser->blocks[block_index].base.y = fx; } else { scene->bad_numbers++; }
    } else if (code == 30) {
        if (gfdxf_fx_from_ascii(value, &fx)) { parser->blocks[block_index].base.z = fx; } else { scene->bad_numbers++; }
    }
}

static int gfdxf_finish_pending_poly(gfdxf_emit_ctx *ctx, int *poly_active, int *poly_count, int *poly_flags)
{
    if (*poly_active) {
        if (!gfdxf_emit_poly_points(ctx, ctx->parser->poly_x, ctx->parser->poly_y, ctx->parser->poly_z, ctx->parser->poly_bulge, *poly_count, *poly_flags, 0, 0)) {
            return 0;
        }
        *poly_active = 0;
        *poly_count = 0;
        *poly_flags = 0;
    }
    return 1;
}

int gfdxf_parse_memory_ex(const char *text, unsigned long length, gfdxf_scene *scene, gfdxf_parser *parser, const gfdxf_callbacks *callbacks, void *user)
{
    int code;
    const char *value;
    int section;
    int in_block;
    int in_block_header;
    int current_block;
    int target;
    int next_type;
    int poly_active;
    int poly_count;
    int poly_flags;
    gfdxf_table_state ts;
    gfdxf_class_state cs;
    gfdxf_entity cur;
    gfdxf_emit_ctx ctx;
    if (text == 0 || scene == 0 || parser == 0) {
        return GFDXF_ERR_NULL;
    }
    memset(scene, 0, sizeof(gfdxf_scene));
    parser->binary_mode = 0;
    parser->binary_group_code_width = 0;
    if (gfdxf_is_binary_dxf_memory((const unsigned char *)text, length) == GFDXF_BINARY_DXF_YES) {
        scene->binary_dxf_detected = 1;
        parser->binary_mode = 1;
        parser->pos = 22UL;
        if (length > 24UL && ((const unsigned char *)text)[22] == 0U && ((const unsigned char *)text)[23] == 0U) {
            parser->binary_group_code_width = 2;
        } else {
            parser->binary_group_code_width = 1;
        }
        scene->bin_group_code_width = parser->binary_group_code_width;
    } else {
        parser->pos = 0;
    }
    parser->line_no = 0;
    parser->last_error = GFDXF_OK;
    section = GFDXF_SECTION_NONE;
    in_block = 0;
    in_block_header = 0;
    current_block = -1;
    poly_active = 0;
    poly_count = 0;
    poly_flags = 0;
    parser->block_count = 0;
    parser->block_vertex_count = 0;
    parser->block_line_count = 0;
    parser->block_face_count = 0;
    parser->block_insert_count = 0;
    parser->layer_count = 0;
    parser->class_count = 0;
    parser->header_var[0] = '\0';
    gfdxf_table_state_clear(&ts);
    gfdxf_class_state_clear(&cs);
    gfdxf_begin_entity(&cur, GFDXF_ENTITY_NONE, parser);
    while (gfdxf_read_tag(text, length, parser, &code, &value)) {
        if (parser->binary_mode) {
            scene->bin_tags_read++;
            if (code > 254) { scene->binary_extended_group_codes++; }
            if (code == 255) { scene->bin_xdata_escape_tags++; }
            if (gfdxf_code_is_string(code)) { scene->bin_string_values++; }
            else if (gfdxf_code_is_real64(code)) { scene->bin_real64_values++; }
            else if (gfdxf_code_is_binary_chunk(code)) { scene->bin_binary_chunks++; }
            else if (gfdxf_code_is_i16(code) || gfdxf_code_is_i32(code) || gfdxf_code_is_i64(code)) { scene->bin_int_values++; }
        }
        if (code == 0) {
            target = in_block ? GFDXF_TARGET_BLOCK : GFDXF_TARGET_SCENE;
            gfdxf_ctx_setup(&ctx, target, scene, parser, current_block, callbacks, user);
            if (!gfdxf_finalize_entity(&ctx, &cur, &poly_count, poly_flags)) {
                return GFDXF_ERR_CAPACITY;
            }
            gfdxf_begin_entity(&cur, GFDXF_ENTITY_NONE, parser);
            if (gfdxf_str_eq_i(value, "SEQEND")) {
                if (poly_active) {
                    if (!gfdxf_finish_pending_poly(&ctx, &poly_active, &poly_count, &poly_flags)) {
                        return GFDXF_ERR_CAPACITY;
                    }
                }
            } else if (gfdxf_str_eq_i(value, "ENDSEC")) {
                gfdxf_finish_layer(scene, parser, &ts);
                gfdxf_finish_class(scene, parser, &cs);
                gfdxf_table_state_clear(&ts);
                gfdxf_class_state_clear(&cs);
                if (poly_active) {
                    if (!gfdxf_finish_pending_poly(&ctx, &poly_active, &poly_count, &poly_flags)) {
                        return GFDXF_ERR_CAPACITY;
                    }
                }
                section = GFDXF_SECTION_NONE;
                in_block = 0;
                in_block_header = 0;
                current_block = -1;
            } else if (section == GFDXF_SECTION_TABLES && gfdxf_str_eq_i(value, "TABLE")) {
                gfdxf_table_state_clear(&ts);
                ts.in_table = 1;
            } else if (section == GFDXF_SECTION_CLASSES && gfdxf_str_eq_i(value, "CLASS")) {
                gfdxf_finish_class(scene, parser, &cs);
                gfdxf_class_state_clear(&cs);
                cs.in_class = 1;
            } else if (gfdxf_str_eq_i(value, "ENDTAB")) {
                if (section == GFDXF_SECTION_TABLES) { gfdxf_finish_layer(scene, parser, &ts); gfdxf_table_state_clear(&ts); }
            } else if (section == GFDXF_SECTION_TABLES && ts.in_table && ts.table_is_layer && gfdxf_str_eq_i(value, "LAYER")) {
                gfdxf_finish_layer(scene, parser, &ts);
                ts.in_layer = 1;
                memset(&ts.cur_layer, 0, sizeof(ts.cur_layer));
                ts.cur_layer.color = 7;
                ts.cur_layer.lineweight = -3;
            } else if (section == GFDXF_SECTION_TABLES && ts.in_table && gfdxf_str_eq_i(value, "APPID")) {
                scene->table_appid_records++;
                scene->table_records++;
            } else if (section == GFDXF_SECTION_TABLES && ts.in_table && gfdxf_str_eq_i(value, "LTYPE")) {
                scene->table_ltype_records++;
                scene->table_records++;
            } else if (section == GFDXF_SECTION_TABLES && ts.in_table && gfdxf_str_eq_i(value, "STYLE")) {
                scene->table_style_records++;
                scene->table_records++;
            } else if (section == GFDXF_SECTION_TABLES && ts.in_table && gfdxf_str_eq_i(value, "DIMSTYLE")) {
                scene->table_dimstyle_records++;
                scene->table_records++;
            } else if (section == GFDXF_SECTION_TABLES && ts.in_table && gfdxf_str_eq_i(value, "BLOCK_RECORD")) {
                scene->table_block_record_records++;
                scene->table_records++;
            } else if (section == GFDXF_SECTION_TABLES && ts.in_table && gfdxf_str_eq_i(value, "UCS")) {
                scene->table_ucs_records++;
                scene->table_records++;
            } else if (section == GFDXF_SECTION_TABLES && ts.in_table && gfdxf_str_eq_i(value, "VPORT")) {
                scene->table_vport_records++;
                scene->table_records++;
            } else if (section == GFDXF_SECTION_TABLES && ts.in_table && gfdxf_str_eq_i(value, "VIEW")) {
                scene->table_view_records++;
                scene->table_records++;
            } else if (gfdxf_str_eq_i(value, "BLOCK")) {
                if (section == GFDXF_SECTION_BLOCKS) {
                    current_block = gfdxf_start_block(scene, parser);
                    in_block = current_block >= 0;
                    in_block_header = in_block;
                }
            } else if (gfdxf_str_eq_i(value, "ENDBLK")) {
                in_block = 0;
                in_block_header = 0;
                current_block = -1;
            } else if (gfdxf_str_eq_i(value, "SECTION")) {
                section = GFDXF_SECTION_OTHER;
            } else if (section == GFDXF_SECTION_BLOCKS && in_block) {
                in_block_header = 0;
                next_type = gfdxf_entity_type_from_name(value);
                if (next_type == GFDXF_ENTITY_POLYLINE) {
                    poly_active = 1;
                    poly_count = 0;
                    poly_flags = 0;
                }
                if (next_type != GFDXF_ENTITY_POLYLINE) {
                    gfdxf_begin_entity(&cur, next_type, parser);
                    gfdxf_copy_name(cur.object_type, value);
                } else {
                    gfdxf_begin_entity(&cur, next_type, parser);
                    gfdxf_copy_name(cur.object_type, value);
                }
            } else if (section == GFDXF_SECTION_ENTITIES) {
                next_type = gfdxf_entity_type_from_name(value);
                if (next_type == GFDXF_ENTITY_POLYLINE) {
                    poly_active = 1;
                    poly_count = 0;
                    poly_flags = 0;
                }
                gfdxf_begin_entity(&cur, next_type, parser);
                gfdxf_copy_name(cur.object_type, value);
            } else if (section == GFDXF_SECTION_OBJECTS) {
                next_type = gfdxf_entity_type_from_name(value);
                gfdxf_begin_entity(&cur, next_type, parser);
                gfdxf_copy_name(cur.object_type, value);
            }
        } else if (code == 2 && section == GFDXF_SECTION_OTHER) {
            if (gfdxf_str_eq_i(value, "HEADER")) {
                section = GFDXF_SECTION_HEADER;
            } else if (gfdxf_str_eq_i(value, "BLOCKS")) {
                section = GFDXF_SECTION_BLOCKS;
            } else if (gfdxf_str_eq_i(value, "ENTITIES")) {
                section = GFDXF_SECTION_ENTITIES;
            } else if (gfdxf_str_eq_i(value, "TABLES")) {
                section = GFDXF_SECTION_TABLES;
            } else if (gfdxf_str_eq_i(value, "CLASSES")) {
                section = GFDXF_SECTION_CLASSES;
            } else if (gfdxf_str_eq_i(value, "OBJECTS")) {
                section = GFDXF_SECTION_OBJECTS;
            }
        } else {
            if (section == GFDXF_SECTION_HEADER) {
                gfdxf_read_header_value(scene, parser, code, value);
            } else if (section == GFDXF_SECTION_TABLES) {
                if (ts.in_table && !ts.in_layer && code == 2) {
                    if (gfdxf_str_eq_i(value, "LAYER")) { ts.table_is_layer = 1; }
                } else if (ts.table_is_layer && ts.in_layer) {
                    gfdxf_read_layer_value(scene, &ts, code, value);
                }
            } else if (section == GFDXF_SECTION_CLASSES) {
                gfdxf_read_class_value(scene, &cs, code, value);
            } else if (in_block_header) {
                gfdxf_read_block_header(scene, parser, current_block, code, value);
            } else {
                gfdxf_read_entity_value(scene, parser, &cur, code, value);
                if (cur.type == GFDXF_ENTITY_POLYLINE && code == 70) {
                    poly_flags = cur.poly_flags;
                }
            }
        }
    }
    target = in_block ? GFDXF_TARGET_BLOCK : GFDXF_TARGET_SCENE;
    gfdxf_ctx_setup(&ctx, target, scene, parser, current_block, callbacks, user);
    if (!gfdxf_finalize_entity(&ctx, &cur, &poly_count, poly_flags)) {
        return GFDXF_ERR_CAPACITY;
    }
    if (poly_active) {
        if (!gfdxf_finish_pending_poly(&ctx, &poly_active, &poly_count, &poly_flags)) {
            return GFDXF_ERR_CAPACITY;
        }
    }
    gfdxf_finish_layer(scene, parser, &ts);
    gfdxf_finish_class(scene, parser, &cs);
    gfdxf_copy_layers_to_scene(scene, parser);
    gfdxf_copy_classes_to_scene(scene, parser);
    gfdxf_post_analyze_payloads(scene);
    gfdxf_kernel_analyze_scene(scene);
    if (parser->last_error != GFDXF_OK) {
        if (parser->binary_mode && parser->last_error == GFDXF_ERR_TRUNCATED_TAG) { scene->bin_truncated_tags++; }
        return parser->last_error;
    }
    if (scene->capacity_errors > 0) {
        return GFDXF_ERR_CAPACITY;
    }
    if (scene->bad_numbers > 0) {
        return GFDXF_ERR_BAD_NUMBER;
    }
    return GFDXF_OK;
}

int gfdxf_parse_memory(const char *text, unsigned long length, gfdxf_scene *scene, gfdxf_parser *parser)
{
    return gfdxf_parse_memory_ex(text, length, scene, parser, 0, 0);
}

int gfdxf_parse_binary_memory_ex(const unsigned char *data, unsigned long length, gfdxf_scene *scene, gfdxf_parser *parser, const gfdxf_callbacks *callbacks, void *user)
{
    return gfdxf_parse_memory_ex((const char *)data, length, scene, parser, callbacks, user);
}

int gfdxf_parse_binary_memory(const unsigned char *data, unsigned long length, gfdxf_scene *scene, gfdxf_parser *parser)
{
    return gfdxf_parse_binary_memory_ex(data, length, scene, parser, 0, 0);
}

int gfdxf_payload_class_index(const gfdxf_scene *scene, const gfdxf_payload *payload)
{
    int i;
    if (scene == 0 || payload == 0) { return -1; }
    for (i = 0; i < scene->class_count; i++) {
        if (scene->classes[i].class_id == payload->class_id) { return i; }
    }
    return -1;
}

int gfdxf_payload_diagnostic_flags(const gfdxf_payload *payload)
{
    if (payload == 0) { return 0; }
    return payload->diagnostic_flags;
}
