#include "step89.h"

#define STEP89_UNKNOWN_LOGICAL -1

typedef struct stp_cur_s {
    const char *s;
    int n;
    int pos;
    int end;
} stp_cur;

static int c_is_ws(char c);
static int c_is_digit(char c);
static int c_is_num_start(char c);
static int c_is_ident(char c);
static char c_upper(char c);
static int c_name_eq(const char *a, const char *b);
static int c_skip_ws_comments(stp_cur *c);
static int c_scan_string(stp_cur *c);
static int c_match(stp_cur *c, char ch);
static int c_read_ref(stp_cur *c, int *ref);
static int c_read_fixed(stp_cur *c, step89_fixed *out);
static int c_read_logical(stp_cur *c, int *out);
static int c_skip_value(stp_cur *c);
static int c_skip_arg(stp_cur *c);
static int c_count_refs_in_list(stp_cur *c, int *first_ref, int *count);
static void c_make(step89_model *m, const step89_entity *e, stp_cur *c);
static int c_parse_point(step89_model *m, const step89_entity *e);
static int c_parse_direction(step89_model *m, const step89_entity *e);
static int c_parse_vector(step89_model *m, const step89_entity *e);
static int c_parse_axis2(step89_model *m, const step89_entity *e);
static int c_parse_vertex(step89_model *m, const step89_entity *e);
static int c_parse_line(step89_model *m, const step89_entity *e);
static int c_parse_circle_or_ellipse(step89_model *m, const step89_entity *e, step89_curve_kind kind);
static int c_parse_bspline_curve(step89_model *m, const step89_entity *e);
static int c_parse_surface_axis_radius(step89_model *m, const step89_entity *e, step89_surface_kind kind, int radii);
static int c_parse_bspline_surface(step89_model *m, const step89_entity *e);
static int c_parse_face_bound(step89_model *m, const step89_entity *e, int is_outer);
static int c_parse_edge_loop(step89_model *m, const step89_entity *e);
static int c_parse_oriented_edge(step89_model *m, const step89_entity *e);
static int c_parse_edge_curve(step89_model *m, const step89_entity *e);
static int c_parse_advanced_face(step89_model *m, const step89_entity *e);

static int c_is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static int c_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int c_is_num_start(char c)
{
    return c == '-' || c == '+' || c == '.' || c_is_digit(c);
}

static int c_is_ident(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static char c_upper(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - ('a' - 'A'));
    return c;
}

static int c_name_eq(const char *a, const char *b)
{
    return step89_name_eq(a, b);
}

static int c_skip_ws_comments(stp_cur *c)
{
    int again;
    again = 1;
    while (again && c->pos < c->end) {
        again = 0;
        while (c->pos < c->end && c_is_ws(c->s[c->pos])) ++c->pos;
        if (c->pos + 1 < c->end && c->s[c->pos] == '/' && c->s[c->pos + 1] == '*') {
            c->pos += 2;
            while (c->pos + 1 < c->end) {
                if (c->s[c->pos] == '*' && c->s[c->pos + 1] == '/') {
                    c->pos += 2;
                    break;
                }
                ++c->pos;
            }
            again = 1;
        }
    }
    return c->pos < c->end;
}

static int c_scan_string(stp_cur *c)
{
    if (c->pos >= c->end || c->s[c->pos] != '\'') return 0;
    ++c->pos;
    while (c->pos < c->end) {
        if (c->s[c->pos] == '\'') {
            if (c->pos + 1 < c->end && c->s[c->pos + 1] == '\'') c->pos += 2;
            else {
                ++c->pos;
                return 1;
            }
        } else ++c->pos;
    }
    return 0;
}

static int c_match(stp_cur *c, char ch)
{
    c_skip_ws_comments(c);
    if (c->pos < c->end && c->s[c->pos] == ch) {
        ++c->pos;
        return 1;
    }
    return 0;
}

static int c_read_ref(stp_cur *c, int *ref)
{
    long v;
    int any;
    c_skip_ws_comments(c);
    if (c->pos >= c->end || c->s[c->pos] != '#') return 0;
    ++c->pos;
    v = 0L;
    any = 0;
    while (c->pos < c->end && c_is_digit(c->s[c->pos])) {
        any = 1;
        if (v < 200000000L) v = v * 10L + (long)(c->s[c->pos] - '0');
        ++c->pos;
    }
    if (!any) return 0;
    *ref = (int)v;
    return 1;
}

static int c_read_fixed(stp_cur *c, step89_fixed *out)
{
    int neg;
    int any;
    long ip;
    long fp;
    long frac_scale;
    long val;
    int exp_neg;
    int exp_any;
    int exp_val;
    int i;
    c_skip_ws_comments(c);
    if (c->pos >= c->end || !c_is_num_start(c->s[c->pos])) return 0;
    neg = 0;
    if (c->s[c->pos] == '-' || c->s[c->pos] == '+') {
        if (c->s[c->pos] == '-') neg = 1;
        ++c->pos;
    }
    ip = 0L;
    fp = 0L;
    frac_scale = STEP89_FIXED_SCALE / 10L;
    any = 0;
    while (c->pos < c->end && c_is_digit(c->s[c->pos])) {
        any = 1;
        if (ip < 200000000L) ip = ip * 10L + (long)(c->s[c->pos] - '0');
        ++c->pos;
    }
    if (c->pos < c->end && c->s[c->pos] == '.') {
        ++c->pos;
        while (c->pos < c->end && c_is_digit(c->s[c->pos])) {
            any = 1;
            if (frac_scale > 0L) {
                fp += (long)(c->s[c->pos] - '0') * frac_scale;
                frac_scale /= 10L;
            }
            ++c->pos;
        }
    }
    if (!any) return 0;
    if (ip > LONG_MAX / STEP89_FIXED_SCALE) val = LONG_MAX;
    else val = ip * STEP89_FIXED_SCALE + fp;
    if (c->pos < c->end && (c->s[c->pos] == 'E' || c->s[c->pos] == 'e')) {
        ++c->pos;
        exp_neg = 0;
        if (c->pos < c->end && (c->s[c->pos] == '-' || c->s[c->pos] == '+')) {
            if (c->s[c->pos] == '-') exp_neg = 1;
            ++c->pos;
        }
        exp_val = 0;
        exp_any = 0;
        while (c->pos < c->end && c_is_digit(c->s[c->pos])) {
            exp_any = 1;
            if (exp_val < 30) exp_val = exp_val * 10 + (int)(c->s[c->pos] - '0');
            ++c->pos;
        }
        if (exp_any) {
            for (i = 0; i < exp_val; ++i) {
                if (exp_neg) val /= 10L;
                else {
                    if (val > LONG_MAX / 10L) {
                        val = LONG_MAX;
                        break;
                    }
                    val *= 10L;
                }
            }
        }
    }
    if (neg) val = -val;
    *out = (step89_fixed)val;
    return 1;
}

static int c_read_logical(stp_cur *c, int *out)
{
    char a;
    c_skip_ws_comments(c);
    if (c->pos + 2 >= c->end || c->s[c->pos] != '.') return 0;
    a = c_upper(c->s[c->pos + 1]);
    if (c->s[c->pos + 2] == '.') {
        if (a == 'T') *out = 1;
        else if (a == 'F') *out = 0;
        else *out = STEP89_UNKNOWN_LOGICAL;
        c->pos += 3;
        return 1;
    }
    return 0;
}

static int c_skip_value(stp_cur *c)
{
    int depth;
    c_skip_ws_comments(c);
    if (c->pos >= c->end) return 0;
    if (c->s[c->pos] == '\'') return c_scan_string(c);
    if (c->s[c->pos] == '(') {
        depth = 1;
        ++c->pos;
        while (c->pos < c->end && depth > 0) {
            if (c->s[c->pos] == '\'') {
                if (!c_scan_string(c)) return 0;
                continue;
            }
            if (c->pos + 1 < c->end && c->s[c->pos] == '/' && c->s[c->pos + 1] == '*') {
                c_skip_ws_comments(c);
                continue;
            }
            if (c->s[c->pos] == '(') ++depth;
            else if (c->s[c->pos] == ')') --depth;
            ++c->pos;
        }
        return depth == 0;
    }
    if (c->s[c->pos] == '#') {
        int ref;
        return c_read_ref(c, &ref);
    }
    if (c->s[c->pos] == '$' || c->s[c->pos] == '*') {
        ++c->pos;
        return 1;
    }
    if (c->s[c->pos] == '.') {
        ++c->pos;
        while (c->pos < c->end && c->s[c->pos] != '.') ++c->pos;
        if (c->pos < c->end) ++c->pos;
        return 1;
    }
    if (c_is_num_start(c->s[c->pos])) {
        step89_fixed tmp;
        return c_read_fixed(c, &tmp);
    }
    if (c_is_ident(c->s[c->pos])) {
        while (c->pos < c->end && c_is_ident(c->s[c->pos])) ++c->pos;
        c_skip_ws_comments(c);
        if (c->pos < c->end && c->s[c->pos] == '(') return c_skip_value(c);
        return 1;
    }
    ++c->pos;
    return 1;
}

static int c_skip_arg(stp_cur *c)
{
    if (!c_skip_value(c)) return 0;
    c_skip_ws_comments(c);
    if (c->pos < c->end && c->s[c->pos] == ',') ++c->pos;
    return 1;
}

static int c_count_refs_in_list(stp_cur *c, int *first_ref, int *count)
{
    int ref;
    int first;
    int cnt;
    if (!c_match(c, '(')) return 0;
    first = 0;
    cnt = 0;
    while (c->pos < c->end) {
        c_skip_ws_comments(c);
        if (c->pos < c->end && c->s[c->pos] == ')') {
            ++c->pos;
            *first_ref = first;
            *count = cnt;
            return 1;
        }
        if (c->pos < c->end && c->s[c->pos] == '#') {
            if (!c_read_ref(c, &ref)) return 0;
            if (cnt == 0) first = ref;
            ++cnt;
        } else {
            if (!c_skip_value(c)) return 0;
        }
        c_skip_ws_comments(c);
        if (c->pos < c->end && c->s[c->pos] == ',') ++c->pos;
    }
    return 0;
}

static void c_make(step89_model *m, const step89_entity *e, stp_cur *c)
{
    c->s = m->text;
    c->n = m->text_len;
    c->pos = e->args_begin;
    c->end = e->args_begin + e->args_len;
}

static int c_parse_point(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    step89_fixed x;
    step89_fixed y;
    step89_fixed z;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_match(&c, '(')) return 0;
    if (!c_read_fixed(&c, &x)) return 0;
    if (!c_match(&c, ',')) return 0;
    if (!c_read_fixed(&c, &y)) return 0;
    z = 0L;
    c_skip_ws_comments(&c);
    if (c.pos < c.end && c.s[c.pos] == ',') {
        ++c.pos;
        if (!c_read_fixed(&c, &z)) return 0;
    }
    if (m->point_count >= STEP89_MAX_POINTS) return -1;
    m->points[m->point_count].ref = e->id;
    m->points[m->point_count].x = x;
    m->points[m->point_count].y = y;
    m->points[m->point_count].z = z;
    ++m->point_count;
    return 1;
}

static int c_parse_direction(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    step89_fixed x;
    step89_fixed y;
    step89_fixed z;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_match(&c, '(')) return 0;
    if (!c_read_fixed(&c, &x)) return 0;
    if (!c_match(&c, ',')) return 0;
    if (!c_read_fixed(&c, &y)) return 0;
    z = 0L;
    c_skip_ws_comments(&c);
    if (c.pos < c.end && c.s[c.pos] == ',') {
        ++c.pos;
        if (!c_read_fixed(&c, &z)) return 0;
    }
    if (m->direction_count >= STEP89_MAX_DIRECTIONS) return -1;
    m->directions[m->direction_count].ref = e->id;
    m->directions[m->direction_count].x = x;
    m->directions[m->direction_count].y = y;
    m->directions[m->direction_count].z = z;
    ++m->direction_count;
    return 1;
}

static int c_parse_vector(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    int dir;
    step89_fixed mag;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_read_ref(&c, &dir)) return 0;
    if (!c_match(&c, ',')) return 0;
    if (!c_read_fixed(&c, &mag)) return 0;
    if (m->vector_count >= STEP89_MAX_VECTORS) return -1;
    m->vectors[m->vector_count].ref = e->id;
    m->vectors[m->vector_count].direction_ref = dir;
    m->vectors[m->vector_count].magnitude = mag;
    ++m->vector_count;
    return 1;
}

static int c_parse_axis2(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    int loc;
    int axis;
    int rd;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_read_ref(&c, &loc)) return 0;
    axis = 0;
    rd = 0;
    c_skip_ws_comments(&c);
    if (c.pos < c.end && c.s[c.pos] == ',') {
        ++c.pos;
        c_skip_ws_comments(&c);
        if (c.pos < c.end && c.s[c.pos] == '#') c_read_ref(&c, &axis);
        else c_skip_value(&c);
    }
    c_skip_ws_comments(&c);
    if (c.pos < c.end && c.s[c.pos] == ',') {
        ++c.pos;
        c_skip_ws_comments(&c);
        if (c.pos < c.end && c.s[c.pos] == '#') c_read_ref(&c, &rd);
        else c_skip_value(&c);
    }
    if (m->axis2_count >= STEP89_MAX_AXIS2) return -1;
    m->axis2[m->axis2_count].ref = e->id;
    m->axis2[m->axis2_count].location_ref = loc;
    m->axis2[m->axis2_count].axis_ref = axis;
    m->axis2[m->axis2_count].ref_direction_ref = rd;
    ++m->axis2_count;
    return 1;
}

static int c_parse_vertex(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    int p;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_read_ref(&c, &p)) return 0;
    if (m->vertex_count >= STEP89_MAX_VERTICES) return -1;
    m->vertices[m->vertex_count].ref = e->id;
    m->vertices[m->vertex_count].point_ref = p;
    ++m->vertex_count;
    return 1;
}

static int c_parse_line(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    int p;
    int v;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_read_ref(&c, &p)) return 0;
    if (!c_match(&c, ',')) return 0;
    if (!c_read_ref(&c, &v)) return 0;
    if (m->curve_count >= STEP89_MAX_CURVES) return -1;
    m->curves[m->curve_count].ref = e->id;
    m->curves[m->curve_count].kind = STEP89_CURVE_LINE;
    m->curves[m->curve_count].a_ref = p;
    m->curves[m->curve_count].b_ref = v;
    m->curves[m->curve_count].c_ref = 0;
    m->curves[m->curve_count].r0 = 0L;
    m->curves[m->curve_count].r1 = 0L;
    m->curves[m->curve_count].degree = 0;
    m->curves[m->curve_count].control_count = 0;
    ++m->curve_count;
    return 1;
}

static int c_parse_circle_or_ellipse(step89_model *m, const step89_entity *e, step89_curve_kind kind)
{
    stp_cur c;
    int axis;
    step89_fixed r0;
    step89_fixed r1;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_read_ref(&c, &axis)) return 0;
    if (!c_match(&c, ',')) return 0;
    if (!c_read_fixed(&c, &r0)) return 0;
    r1 = 0L;
    if (kind == STEP89_CURVE_ELLIPSE) {
        if (!c_match(&c, ',')) return 0;
        if (!c_read_fixed(&c, &r1)) return 0;
    }
    if (m->curve_count >= STEP89_MAX_CURVES) return -1;
    m->curves[m->curve_count].ref = e->id;
    m->curves[m->curve_count].kind = kind;
    m->curves[m->curve_count].a_ref = axis;
    m->curves[m->curve_count].b_ref = 0;
    m->curves[m->curve_count].c_ref = 0;
    m->curves[m->curve_count].r0 = r0;
    m->curves[m->curve_count].r1 = r1;
    m->curves[m->curve_count].degree = 0;
    m->curves[m->curve_count].control_count = 0;
    ++m->curve_count;
    return 1;
}

static int c_parse_bspline_curve(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    step89_fixed tmp;
    int degree;
    int first;
    int count;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_read_fixed(&c, &tmp)) return 0;
    degree = (int)(tmp / STEP89_FIXED_SCALE);
    if (!c_match(&c, ',')) return 0;
    if (!c_count_refs_in_list(&c, &first, &count)) return 0;
    if (m->curve_count >= STEP89_MAX_CURVES) return -1;
    m->curves[m->curve_count].ref = e->id;
    m->curves[m->curve_count].kind = STEP89_CURVE_BSPLINE;
    m->curves[m->curve_count].a_ref = first;
    m->curves[m->curve_count].b_ref = 0;
    m->curves[m->curve_count].c_ref = 0;
    m->curves[m->curve_count].r0 = 0L;
    m->curves[m->curve_count].r1 = 0L;
    m->curves[m->curve_count].degree = degree;
    m->curves[m->curve_count].control_count = count;
    ++m->curve_count;
    return 1;
}

static int c_parse_surface_axis_radius(step89_model *m, const step89_entity *e, step89_surface_kind kind, int radii)
{
    stp_cur c;
    int axis;
    step89_fixed r0;
    step89_fixed r1;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_read_ref(&c, &axis)) return 0;
    r0 = 0L;
    r1 = 0L;
    if (radii >= 1) {
        if (!c_match(&c, ',')) return 0;
        if (!c_read_fixed(&c, &r0)) return 0;
    }
    if (radii >= 2) {
        if (!c_match(&c, ',')) return 0;
        if (!c_read_fixed(&c, &r1)) return 0;
    }
    if (m->surface_count >= STEP89_MAX_SURFACES) return -1;
    m->surfaces[m->surface_count].ref = e->id;
    m->surfaces[m->surface_count].kind = kind;
    m->surfaces[m->surface_count].axis_ref = axis;
    m->surfaces[m->surface_count].r0 = r0;
    m->surfaces[m->surface_count].r1 = r1;
    m->surfaces[m->surface_count].degree_u = 0;
    m->surfaces[m->surface_count].degree_v = 0;
    m->surfaces[m->surface_count].control_count = 0;
    ++m->surface_count;
    return 1;
}

static int c_parse_bspline_surface(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    step89_fixed tmp;
    int degree_u;
    int degree_v;
    int first_row;
    int row_count;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_read_fixed(&c, &tmp)) return 0;
    degree_u = (int)(tmp / STEP89_FIXED_SCALE);
    if (!c_match(&c, ',')) return 0;
    if (!c_read_fixed(&c, &tmp)) return 0;
    degree_v = (int)(tmp / STEP89_FIXED_SCALE);
    if (!c_match(&c, ',')) return 0;
    if (!c_count_refs_in_list(&c, &first_row, &row_count)) return 0;
    if (m->surface_count >= STEP89_MAX_SURFACES) return -1;
    m->surfaces[m->surface_count].ref = e->id;
    m->surfaces[m->surface_count].kind = STEP89_SURFACE_BSPLINE;
    m->surfaces[m->surface_count].axis_ref = first_row;
    m->surfaces[m->surface_count].r0 = 0L;
    m->surfaces[m->surface_count].r1 = 0L;
    m->surfaces[m->surface_count].degree_u = degree_u;
    m->surfaces[m->surface_count].degree_v = degree_v;
    m->surfaces[m->surface_count].control_count = row_count;
    ++m->surface_count;
    return 1;
}

static int c_parse_face_bound(step89_model *m, const step89_entity *e, int is_outer)
{
    stp_cur c;
    int loop;
    int orient;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_read_ref(&c, &loop)) return 0;
    if (!c_match(&c, ',')) return 0;
    orient = STEP89_UNKNOWN_LOGICAL;
    c_read_logical(&c, &orient);
    if (m->face_bound_count >= STEP89_MAX_FACE_BOUNDS) return -1;
    m->face_bounds[m->face_bound_count].ref = e->id;
    m->face_bounds[m->face_bound_count].loop_ref = loop;
    m->face_bounds[m->face_bound_count].orientation = orient;
    m->face_bounds[m->face_bound_count].is_outer = is_outer;
    ++m->face_bound_count;
    return 1;
}

static int c_parse_edge_loop(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    int first;
    int count;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_count_refs_in_list(&c, &first, &count)) return 0;
    if (m->edge_loop_count >= STEP89_MAX_EDGE_LOOPS) return -1;
    m->edge_loops[m->edge_loop_count].ref = e->id;
    m->edge_loops[m->edge_loop_count].first_oriented_edge_ref = first;
    m->edge_loops[m->edge_loop_count].oriented_edge_count = count;
    ++m->edge_loop_count;
    return 1;
}

static int c_parse_oriented_edge(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    int edge;
    int orient;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_read_ref(&c, &edge)) return 0;
    if (!c_match(&c, ',')) return 0;
    orient = STEP89_UNKNOWN_LOGICAL;
    c_read_logical(&c, &orient);
    if (m->oriented_edge_count >= STEP89_MAX_ORIENTED_EDGES) return -1;
    m->oriented_edges[m->oriented_edge_count].ref = e->id;
    m->oriented_edges[m->oriented_edge_count].edge_curve_ref = edge;
    m->oriented_edges[m->oriented_edge_count].orientation = orient;
    ++m->oriented_edge_count;
    return 1;
}

static int c_parse_edge_curve(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    int sv;
    int ev;
    int curve;
    int same;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_read_ref(&c, &sv)) return 0;
    if (!c_match(&c, ',')) return 0;
    if (!c_read_ref(&c, &ev)) return 0;
    if (!c_match(&c, ',')) return 0;
    if (!c_read_ref(&c, &curve)) return 0;
    if (!c_match(&c, ',')) return 0;
    same = STEP89_UNKNOWN_LOGICAL;
    c_read_logical(&c, &same);
    if (m->edge_curve_count >= STEP89_MAX_EDGE_CURVES) return -1;
    m->edge_curves[m->edge_curve_count].ref = e->id;
    m->edge_curves[m->edge_curve_count].start_vertex_ref = sv;
    m->edge_curves[m->edge_curve_count].end_vertex_ref = ev;
    m->edge_curves[m->edge_curve_count].curve_ref = curve;
    m->edge_curves[m->edge_curve_count].same_sense = same;
    ++m->edge_curve_count;
    return 1;
}

static int c_parse_advanced_face(step89_model *m, const step89_entity *e)
{
    stp_cur c;
    int first;
    int count;
    int surf;
    int same;
    c_make(m, e, &c);
    if (!c_match(&c, '(')) return 0;
    if (!c_skip_arg(&c)) return 0;
    if (!c_count_refs_in_list(&c, &first, &count)) return 0;
    if (!c_match(&c, ',')) return 0;
    if (!c_read_ref(&c, &surf)) return 0;
    if (!c_match(&c, ',')) return 0;
    same = STEP89_UNKNOWN_LOGICAL;
    c_read_logical(&c, &same);
    if (m->face_count >= STEP89_MAX_FACES) return -1;
    m->faces[m->face_count].ref = e->id;
    m->faces[m->face_count].first_bound_ref = first;
    m->faces[m->face_count].bound_count = count;
    m->faces[m->face_count].surface_ref = surf;
    m->faces[m->face_count].same_sense = same;
    ++m->face_count;
    return 1;
}

step89_result step89_extract_geometry(step89_model *m)
{
    int i;
    int r;
    const step89_entity *e;
    if (!m) return STEP89_ERR_NULL;
    m->point_count = 0;
    m->direction_count = 0;
    m->vector_count = 0;
    m->axis2_count = 0;
    m->vertex_count = 0;
    m->curve_count = 0;
    m->surface_count = 0;
    m->face_bound_count = 0;
    m->edge_loop_count = 0;
    m->oriented_edge_count = 0;
    m->edge_curve_count = 0;
    m->face_count = 0;
    m->unsupported_brep_count = 0;
    m->warning_count = 0;
    for (i = 0; i < m->entity_count; ++i) {
        e = &m->entities[i];
        r = 0;
        if (c_name_eq(e->name, "CARTESIAN_POINT")) r = c_parse_point(m, e);
        else if (c_name_eq(e->name, "DIRECTION")) r = c_parse_direction(m, e);
        else if (c_name_eq(e->name, "VECTOR")) r = c_parse_vector(m, e);
        else if (c_name_eq(e->name, "AXIS2_PLACEMENT_3D")) r = c_parse_axis2(m, e);
        else if (c_name_eq(e->name, "AXIS2_PLACEMENT_2D")) r = c_parse_axis2(m, e);
        else if (c_name_eq(e->name, "VERTEX_POINT")) r = c_parse_vertex(m, e);
        else if (c_name_eq(e->name, "LINE")) r = c_parse_line(m, e);
        else if (c_name_eq(e->name, "CIRCLE")) r = c_parse_circle_or_ellipse(m, e, STEP89_CURVE_CIRCLE);
        else if (c_name_eq(e->name, "ELLIPSE")) r = c_parse_circle_or_ellipse(m, e, STEP89_CURVE_ELLIPSE);
        else if (c_name_eq(e->name, "B_SPLINE_CURVE_WITH_KNOTS")) r = c_parse_bspline_curve(m, e);
        else if (c_name_eq(e->name, "B_SPLINE_CURVE")) r = c_parse_bspline_curve(m, e);
        else if (c_name_eq(e->name, "PLANE")) r = c_parse_surface_axis_radius(m, e, STEP89_SURFACE_PLANE, 0);
        else if (c_name_eq(e->name, "CYLINDRICAL_SURFACE")) r = c_parse_surface_axis_radius(m, e, STEP89_SURFACE_CYLINDRICAL, 1);
        else if (c_name_eq(e->name, "CONICAL_SURFACE")) r = c_parse_surface_axis_radius(m, e, STEP89_SURFACE_CONICAL, 2);
        else if (c_name_eq(e->name, "SPHERICAL_SURFACE")) r = c_parse_surface_axis_radius(m, e, STEP89_SURFACE_SPHERICAL, 1);
        else if (c_name_eq(e->name, "TOROIDAL_SURFACE")) r = c_parse_surface_axis_radius(m, e, STEP89_SURFACE_TOROIDAL, 2);
        else if (c_name_eq(e->name, "B_SPLINE_SURFACE_WITH_KNOTS")) r = c_parse_bspline_surface(m, e);
        else if (c_name_eq(e->name, "B_SPLINE_SURFACE")) r = c_parse_bspline_surface(m, e);
        else if (c_name_eq(e->name, "FACE_OUTER_BOUND")) r = c_parse_face_bound(m, e, 1);
        else if (c_name_eq(e->name, "FACE_BOUND")) r = c_parse_face_bound(m, e, 0);
        else if (c_name_eq(e->name, "EDGE_LOOP")) r = c_parse_edge_loop(m, e);
        else if (c_name_eq(e->name, "ORIENTED_EDGE")) r = c_parse_oriented_edge(m, e);
        else if (c_name_eq(e->name, "EDGE_CURVE")) r = c_parse_edge_curve(m, e);
        else if (c_name_eq(e->name, "ADVANCED_FACE")) r = c_parse_advanced_face(m, e);
        else if (c_name_eq(e->name, "MANIFOLD_SOLID_BREP") || c_name_eq(e->name, "CLOSED_SHELL") ||
                 c_name_eq(e->name, "OPEN_SHELL") || c_name_eq(e->name, "SHELL_BASED_SURFACE_MODEL") ||
                 c_name_eq(e->name, "ADVANCED_BREP_SHAPE_REPRESENTATION")) {
            ++m->unsupported_brep_count;
            r = 1;
        }
        if (r < 0) {
            ++m->warning_count;
        }
    }
    if (m->warning_count > 0) return STEP89_ERR_LIMIT;
    return STEP89_OK;
}

const step89_point *step89_find_point(const step89_model *m, int ref)
{
    int i;
    if (!m) return (const step89_point *)0;
    for (i = 0; i < m->point_count; ++i) if (m->points[i].ref == ref) return &m->points[i];
    return (const step89_point *)0;
}

const step89_direction *step89_find_direction(const step89_model *m, int ref)
{
    int i;
    if (!m) return (const step89_direction *)0;
    for (i = 0; i < m->direction_count; ++i) if (m->directions[i].ref == ref) return &m->directions[i];
    return (const step89_direction *)0;
}

const step89_axis2 *step89_find_axis2(const step89_model *m, int ref)
{
    int i;
    if (!m) return (const step89_axis2 *)0;
    for (i = 0; i < m->axis2_count; ++i) if (m->axis2[i].ref == ref) return &m->axis2[i];
    return (const step89_axis2 *)0;
}

const step89_curve *step89_find_curve(const step89_model *m, int ref)
{
    int i;
    if (!m) return (const step89_curve *)0;
    for (i = 0; i < m->curve_count; ++i) if (m->curves[i].ref == ref) return &m->curves[i];
    return (const step89_curve *)0;
}

const step89_surface *step89_find_surface(const step89_model *m, int ref)
{
    int i;
    if (!m) return (const step89_surface *)0;
    for (i = 0; i < m->surface_count; ++i) if (m->surfaces[i].ref == ref) return &m->surfaces[i];
    return (const step89_surface *)0;
}
