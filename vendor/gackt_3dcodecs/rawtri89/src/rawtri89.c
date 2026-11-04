#include "rawtri89.h"

#define RW3D_ULONG_MAX_LOCAL (~0UL)
#define RW3D_MAX_U32 4294967295UL
#define RW3D_SAFE_MUL_LIMIT 46340UL

static size_t rw3d_align_default(void)
{
    size_t a;
    size_t b;
    a = sizeof(void *);
    b = sizeof(long);
    return a > b ? a : b;
}

static int rw3d_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int rw3d_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int rw3d_is_line_break(char c)
{
    return c == '\n' || c == '\r';
}

static int rw3d_is_comment(const char *p, const char *end)
{
    if (p >= end) {
        return 0;
    }
    if (*p == '#' || *p == ';') {
        return 1;
    }
    if (*p == '/' && p + 1 < end && p[1] == '/') {
        return 1;
    }
    return 0;
}

static int rw3d_is_sep(char c, unsigned long flags)
{
    if (rw3d_is_space(c)) {
        return 1;
    }
    if ((flags & RW3D_RAW_F_ALLOW_COMMAS) && c == ',') {
        return 1;
    }
    return 0;
}

static rw3d_fix rw3d_sat_pos(unsigned long magnitude, int negative, int *overflow)
{
    if (negative) {
        if (magnitude > 2147483648UL) {
            if (overflow != 0) {
                *overflow = 1;
            }
            return RW3D_FIX_MIN;
        }
        if (magnitude == 2147483648UL) {
            return RW3D_FIX_MIN;
        }
        return (rw3d_fix)(-((rw3d_i32)magnitude));
    }
    if (magnitude > 2147483647UL) {
        if (overflow != 0) {
            *overflow = 1;
        }
        return RW3D_FIX_MAX;
    }
    return (rw3d_fix)magnitude;
}

static unsigned long rw3d_abs_fix_to_ulong(rw3d_fix v)
{
    if (v < 0) {
        if (v == RW3D_FIX_MIN) {
            return 2147483648UL;
        }
        return (unsigned long)(-v);
    }
    return (unsigned long)v;
}

static rw3d_fix rw3d_fix_mul_int_sat(rw3d_fix value, long mul, int *overflow)
{
    int neg;
    unsigned long a;
    unsigned long b;
    unsigned long maxv;
    unsigned long r;

    if (mul == 0 || value == 0) {
        return 0;
    }

    neg = 0;
    if (value < 0) {
        neg = !neg;
        a = rw3d_abs_fix_to_ulong(value);
    } else {
        a = (unsigned long)value;
    }
    if (mul < 0) {
        neg = !neg;
        b = (unsigned long)(-mul);
    } else {
        b = (unsigned long)mul;
    }

    maxv = neg ? 2147483648UL : 2147483647UL;
    if (b != 0UL && a > maxv / b) {
        if (overflow != 0) {
            *overflow = 1;
        }
        return neg ? RW3D_FIX_MIN : RW3D_FIX_MAX;
    }
    r = a * b;
    return rw3d_sat_pos(r, neg, overflow);
}

static rw3d_fix rw3d_fix_div_int_sat(rw3d_fix value, long divv)
{
    if (divv == 0) {
        return value < 0 ? RW3D_FIX_MIN : RW3D_FIX_MAX;
    }
    return (rw3d_fix)(value / divv);
}

void rw3d_arena_init(rw3d_arena *arena, void *memory, size_t size)
{
    if (arena == 0) {
        return;
    }
    arena->base = (unsigned char *)memory;
    arena->size = size;
    arena->used = 0;
    arena->failed = 0;
}

void rw3d_arena_reset(rw3d_arena *arena)
{
    if (arena == 0) {
        return;
    }
    arena->used = 0;
    arena->failed = 0;
}

void *rw3d_arena_alloc(rw3d_arena *arena, size_t size, size_t align)
{
    unsigned long addr;
    size_t mis;
    size_t pad;
    unsigned char *p;

    if (arena == 0 || arena->base == 0 || size == 0) {
        if (arena != 0) {
            arena->failed = 1;
        }
        return 0;
    }
    if (align == 0) {
        align = 1;
    }
    addr = (unsigned long)(arena->base + arena->used);
    mis = (size_t)(addr % (unsigned long)align);
    pad = mis == 0 ? 0 : align - mis;
    if (arena->used > arena->size) {
        arena->failed = 1;
        return 0;
    }
    if (pad > arena->size - arena->used) {
        arena->failed = 1;
        return 0;
    }
    if (size > arena->size - arena->used - pad) {
        arena->failed = 1;
        return 0;
    }
    p = arena->base + arena->used + pad;
    arena->used += pad + size;
    return (void *)p;
}

void rw3d_mesh_zero(rw3d_mesh *mesh)
{
    if (mesh == 0) {
        return;
    }
    mesh->vertices = 0;
    mesh->triangles = 0;
    mesh->vertex_count = 0;
    mesh->triangle_count = 0;
    mesh->vertex_capacity = 0;
    mesh->triangle_capacity = 0;
    mesh->bounds_min.x = 0;
    mesh->bounds_min.y = 0;
    mesh->bounds_min.z = 0;
    mesh->bounds_max.x = 0;
    mesh->bounds_max.y = 0;
    mesh->bounds_max.z = 0;
    mesh->has_bounds = 0;
}

void rw3d_report_zero(rw3d_report *report)
{
    if (report == 0) {
        return;
    }
    report->status = RW3D_OK;
    report->message = "ok";
    report->line_count = 0;
    report->valid_lines = 0;
    report->blank_lines = 0;
    report->invalid_lines = 0;
    report->tri_lines = 0;
    report->quad_lines = 0;
    report->duplicate_vertices = 0;
    report->dropped_degenerate = 0;
    report->dropped_duplicate_triangles = 0;
    report->first_error_line = 0;
    report->first_error_code = RW3D_OK;
    report->arena_used = 0;
    report->bytes_written = 0;
    report->bytes_needed = 0;
}

void rw3d_mesh_stats_zero(rw3d_mesh_stats *stats)
{
    if (stats == 0) {
        return;
    }
    stats->vertices = 0;
    stats->triangles = 0;
    stats->edges_unique = 0;
    stats->boundary_edges = 0;
    stats->nonmanifold_edges = 0;
    stats->duplicate_triangles = 0;
    stats->degenerate_triangles = 0;
    stats->zero_area_triangles = 0;
    stats->isolated_vertices = 0;
}

static void rw3d_report_error(rw3d_report *report, int status, unsigned long line, const char *message)
{
    if (report == 0) {
        return;
    }
    if (report->status == RW3D_OK) {
        report->status = status;
        report->first_error_code = status;
        report->first_error_line = line;
        report->message = message;
    }
}

rw3d_fix rw3d_fix_from_int(long value)
{
    int overflow;
    overflow = 0;
    return rw3d_fix_mul_int_sat((rw3d_fix)value, RW3D_FIX_ONE, &overflow);
}

long rw3d_fix_to_int_floor(rw3d_fix value)
{
    return value / RW3D_FIX_ONE;
}

rw3d_fix rw3d_fix_div_int(rw3d_fix a, long b)
{
    return rw3d_fix_div_int_sat(a, b);
}

rw3d_fix rw3d_fix_mul(rw3d_fix a, rw3d_fix b)
{
    int neg;
    unsigned long ua;
    unsigned long ub;
    unsigned long prod;
    int shift_a;
    int shift_b;
    int shift;
    unsigned long maxv;

    if (a == 0 || b == 0) {
        return 0;
    }

    neg = 0;
    if (a < 0) {
        neg = !neg;
        ua = rw3d_abs_fix_to_ulong(a);
    } else {
        ua = (unsigned long)a;
    }
    if (b < 0) {
        neg = !neg;
        ub = rw3d_abs_fix_to_ulong(b);
    } else {
        ub = (unsigned long)b;
    }

    shift_a = 0;
    shift_b = 0;
    while (ua > RW3D_SAFE_MUL_LIMIT) {
        ua = (ua + 1UL) >> 1;
        shift_a++;
    }
    while (ub > RW3D_SAFE_MUL_LIMIT) {
        ub = (ub + 1UL) >> 1;
        shift_b++;
    }

    prod = ua * ub;
    shift = shift_a + shift_b - RW3D_FIX_SHIFT;
    maxv = neg ? 2147483648UL : 2147483647UL;

    if (shift >= 0) {
        if (shift >= 31) {
            return neg ? RW3D_FIX_MIN : RW3D_FIX_MAX;
        }
        if (prod > (maxv >> shift)) {
            return neg ? RW3D_FIX_MIN : RW3D_FIX_MAX;
        }
        prod = prod << shift;
    } else {
        shift = -shift;
        if (shift >= 32) {
            prod = 0;
        } else {
            prod = prod >> shift;
        }
    }

    return rw3d_sat_pos(prod, neg, 0);
}

static rw3d_fix rw3d_fix_add_sat(rw3d_fix a, rw3d_fix b)
{
    if (b > 0 && a > RW3D_FIX_MAX - b) {
        return RW3D_FIX_MAX;
    }
    if (b < 0 && a < RW3D_FIX_MIN - b) {
        return RW3D_FIX_MIN;
    }
    return (rw3d_fix)(a + b);
}

static rw3d_fix rw3d_fix_sub_sat(rw3d_fix a, rw3d_fix b)
{
    if (b == RW3D_FIX_MIN) {
        return a >= 0 ? RW3D_FIX_MAX : rw3d_fix_add_sat(a, RW3D_FIX_MAX);
    }
    return rw3d_fix_add_sat(a, (rw3d_fix)(-b));
}

static int rw3d_parse_fixed_token(const char **pp, const char *end, rw3d_fix *out, int *overflow)
{
    const char *p;
    int neg;
    int any;
    unsigned long whole;
    unsigned long magnitude;
    rw3d_fix val;
    rw3d_fix frac_val;
    long divv;
    int digit;
    int exp_neg;
    long exp_val;
    int exp_any;
    long i;

    p = *pp;
    neg = 0;
    any = 0;
    whole = 0;
    frac_val = 0;

    if (p < end && (*p == '+' || *p == '-')) {
        neg = *p == '-';
        p++;
    }

    while (p < end && rw3d_is_digit(*p)) {
        any = 1;
        digit = (int)(*p - '0');
        if (whole <= 400000000UL) {
            whole = whole * 10UL + (unsigned long)digit;
        } else if (overflow != 0) {
            *overflow = 1;
        }
        p++;
    }

    if (p < end && *p == '.') {
        p++;
        divv = 10L;
        while (p < end && rw3d_is_digit(*p)) {
            any = 1;
            digit = (int)(*p - '0');
            if (divv <= 1000000L) {
                frac_val = rw3d_fix_add_sat(frac_val, (rw3d_fix)(((long)digit * RW3D_FIX_ONE + divv / 2L) / divv));
                divv = divv * 10L;
            }
            p++;
        }
    }

    if (!any) {
        return 0;
    }

    if (whole > 32767UL) {
        if (overflow != 0) {
            *overflow = 1;
        }
        magnitude = neg ? 2147483648UL : 2147483647UL;
    } else {
        magnitude = (whole << RW3D_FIX_SHIFT) + (unsigned long)frac_val;
        if (!neg && magnitude > 2147483647UL) {
            if (overflow != 0) {
                *overflow = 1;
            }
            magnitude = 2147483647UL;
        }
        if (neg && magnitude > 2147483648UL) {
            if (overflow != 0) {
                *overflow = 1;
            }
            magnitude = 2147483648UL;
        }
    }
    val = rw3d_sat_pos(magnitude, neg, overflow);

    if (p < end && (*p == 'e' || *p == 'E')) {
        p++;
        exp_neg = 0;
        exp_val = 0;
        exp_any = 0;
        if (p < end && (*p == '+' || *p == '-')) {
            exp_neg = *p == '-';
            p++;
        }
        while (p < end && rw3d_is_digit(*p)) {
            exp_any = 1;
            if (exp_val < 32L) {
                exp_val = exp_val * 10L + (long)(*p - '0');
            }
            p++;
        }
        if (!exp_any) {
            return 0;
        }
        if (exp_val > 18L) {
            exp_val = 18L;
            if (overflow != 0 && !exp_neg) {
                *overflow = 1;
            }
        }
        if (exp_neg) {
            for (i = 0; i < exp_val; ++i) {
                val = rw3d_fix_div_int_sat(val, 10L);
            }
        } else {
            for (i = 0; i < exp_val; ++i) {
                val = rw3d_fix_mul_int_sat(val, 10L, overflow);
            }
        }
    }

    *out = val;
    *pp = p;
    return 1;
}

static int rw3d_parse_line_values(const char *line,
                                  const char *line_end,
                                  unsigned long flags,
                                  rw3d_fix values[12],
                                  int *count,
                                  int *overflow)
{
    const char *p;
    rw3d_fix v;

    p = line;
    *count = 0;

    while (p < line_end) {
        while (p < line_end && rw3d_is_sep(*p, flags)) {
            p++;
        }
        if (p >= line_end || rw3d_is_comment(p, line_end)) {
            break;
        }
        if (*count >= 12) {
            return RW3D_ERR_PARSE;
        }
        if (!rw3d_parse_fixed_token(&p, line_end, &v, overflow)) {
            return RW3D_ERR_PARSE;
        }
        values[*count] = v;
        *count = *count + 1;
        if (p < line_end && !rw3d_is_sep(*p, flags) && !rw3d_is_comment(p, line_end)) {
            return RW3D_ERR_PARSE;
        }
    }
    return RW3D_OK;
}

static void rw3d_update_bounds(rw3d_mesh *mesh, const rw3d_vec3q *v)
{
    if (!mesh->has_bounds) {
        mesh->bounds_min = *v;
        mesh->bounds_max = *v;
        mesh->has_bounds = 1;
        return;
    }
    if (v->x < mesh->bounds_min.x) {
        mesh->bounds_min.x = v->x;
    }
    if (v->y < mesh->bounds_min.y) {
        mesh->bounds_min.y = v->y;
    }
    if (v->z < mesh->bounds_min.z) {
        mesh->bounds_min.z = v->z;
    }
    if (v->x > mesh->bounds_max.x) {
        mesh->bounds_max.x = v->x;
    }
    if (v->y > mesh->bounds_max.y) {
        mesh->bounds_max.y = v->y;
    }
    if (v->z > mesh->bounds_max.z) {
        mesh->bounds_max.z = v->z;
    }
}

static int rw3d_vec_equal(const rw3d_vec3q *a, const rw3d_vec3q *b)
{
    return a->x == b->x && a->y == b->y && a->z == b->z;
}

static int rw3d_mesh_add_vertex(rw3d_mesh *mesh,
                                const rw3d_vec3q *v,
                                unsigned long flags,
                                rw3d_report *report,
                                rw3d_u32 *index_out)
{
    rw3d_u32 i;

    if (flags & RW3D_RAW_F_DEDUP_EXACT) {
        for (i = 0; i < mesh->vertex_count; ++i) {
            if (rw3d_vec_equal(&mesh->vertices[i], v)) {
                if (report != 0) {
                    report->duplicate_vertices++;
                }
                *index_out = i;
                return RW3D_OK;
            }
        }
    }

    if (mesh->vertex_count >= mesh->vertex_capacity) {
        return RW3D_ERR_CAPACITY;
    }
    mesh->vertices[mesh->vertex_count] = *v;
    *index_out = mesh->vertex_count;
    mesh->vertex_count++;
    rw3d_update_bounds(mesh, v);
    return RW3D_OK;
}

static rw3d_vec3q rw3d_vec_sub(const rw3d_vec3q *a, const rw3d_vec3q *b)
{
    rw3d_vec3q r;
    r.x = rw3d_fix_sub_sat(a->x, b->x);
    r.y = rw3d_fix_sub_sat(a->y, b->y);
    r.z = rw3d_fix_sub_sat(a->z, b->z);
    return r;
}

static rw3d_vec3q rw3d_cross(const rw3d_vec3q *a, const rw3d_vec3q *b)
{
    rw3d_vec3q r;
    rw3d_fix aybz;
    rw3d_fix azby;
    rw3d_fix azbx;
    rw3d_fix axbz;
    rw3d_fix axby;
    rw3d_fix aybx;

    aybz = rw3d_fix_mul(a->y, b->z);
    azby = rw3d_fix_mul(a->z, b->y);
    azbx = rw3d_fix_mul(a->z, b->x);
    axbz = rw3d_fix_mul(a->x, b->z);
    axby = rw3d_fix_mul(a->x, b->y);
    aybx = rw3d_fix_mul(a->y, b->x);

    r.x = rw3d_fix_sub_sat(aybz, azby);
    r.y = rw3d_fix_sub_sat(azbx, axbz);
    r.z = rw3d_fix_sub_sat(axby, aybx);
    return r;
}

static rw3d_vec3q rw3d_compute_tri_normal(const rw3d_mesh *mesh, rw3d_u32 a, rw3d_u32 b, rw3d_u32 c)
{
    rw3d_vec3q e1;
    rw3d_vec3q e2;
    rw3d_vec3q n;

    e1 = rw3d_vec_sub(&mesh->vertices[b], &mesh->vertices[a]);
    e2 = rw3d_vec_sub(&mesh->vertices[c], &mesh->vertices[a]);
    n = rw3d_cross(&e1, &e2);
    return n;
}

static int rw3d_normal_is_zero(const rw3d_vec3q *n)
{
    return n->x == 0 && n->y == 0 && n->z == 0;
}

static int rw3d_mesh_add_tri(rw3d_mesh *mesh,
                             rw3d_u32 a,
                             rw3d_u32 b,
                             rw3d_u32 c,
                             unsigned long flags,
                             rw3d_report *report)
{
    rw3d_vec3q n;

    if (a == b || b == c || a == c) {
        if (flags & RW3D_RAW_F_DROP_DEGENERATE) {
            if (report != 0) {
                report->dropped_degenerate++;
            }
            return RW3D_OK;
        }
    }

    n.x = 0;
    n.y = 0;
    n.z = 0;
    if (flags & RW3D_RAW_F_COMPUTE_NORMALS) {
        n = rw3d_compute_tri_normal(mesh, a, b, c);
        if ((flags & RW3D_RAW_F_DROP_DEGENERATE) && rw3d_normal_is_zero(&n)) {
            if (report != 0) {
                report->dropped_degenerate++;
            }
            return RW3D_OK;
        }
    }

    if (mesh->triangle_count >= mesh->triangle_capacity) {
        return RW3D_ERR_CAPACITY;
    }
    mesh->triangles[mesh->triangle_count].a = a;
    mesh->triangles[mesh->triangle_count].b = b;
    mesh->triangles[mesh->triangle_count].c = c;
    mesh->triangles[mesh->triangle_count].normal = n;
    mesh->triangle_count++;
    return RW3D_OK;
}

static int rw3d_count_pass(const char *data,
                           size_t length,
                           unsigned long flags,
                           rw3d_report *report,
                           rw3d_u32 *triangles_needed)
{
    const char *p;
    const char *end;
    const char *line;
    const char *line_end;
    rw3d_fix values[12];
    int count;
    int err;
    int overflow;
    unsigned long need;

    p = data;
    end = data + length;
    need = 0UL;

    while (p < end) {
        line = p;
        while (p < end && !rw3d_is_line_break(*p)) {
            p++;
        }
        line_end = p;
        if (p < end && *p == '\r') {
            p++;
            if (p < end && *p == '\n') {
                p++;
            }
        } else if (p < end && *p == '\n') {
            p++;
        }

        if (report != 0) {
            report->line_count++;
        }
        overflow = 0;
        err = rw3d_parse_line_values(line, line_end, flags, values, &count, &overflow);
        if (err != RW3D_OK || overflow) {
            if (report != 0) {
                report->invalid_lines++;
            }
            rw3d_report_error(report, overflow ? RW3D_ERR_OVERFLOW : RW3D_ERR_PARSE,
                              report != 0 ? report->line_count : 0UL,
                              overflow ? "fixed-point numeric overflow" : "invalid RAW numeric line");
            if (flags & RW3D_RAW_F_STRICT) {
                return overflow ? RW3D_ERR_OVERFLOW : RW3D_ERR_PARSE;
            }
            continue;
        }

        if (count == 0) {
            if (report != 0) {
                report->blank_lines++;
            }
        } else if (count == 9) {
            if (need > RW3D_MAX_U32 - 1UL) {
                rw3d_report_error(report, RW3D_ERR_OVERFLOW, report != 0 ? report->line_count : 0UL, "too many triangles");
                return RW3D_ERR_OVERFLOW;
            }
            need += 1UL;
            if (report != 0) {
                report->tri_lines++;
                report->valid_lines++;
            }
        } else if (count == 12 && (flags & RW3D_RAW_F_ALLOW_QUADS)) {
            if (!(flags & RW3D_RAW_F_TRIANGULATE_QUADS)) {
                rw3d_report_error(report, RW3D_ERR_UNSUPPORTED, report != 0 ? report->line_count : 0UL, "quad line requires triangulation flag");
                return RW3D_ERR_UNSUPPORTED;
            }
            if (need > RW3D_MAX_U32 - 2UL) {
                rw3d_report_error(report, RW3D_ERR_OVERFLOW, report != 0 ? report->line_count : 0UL, "too many triangles");
                return RW3D_ERR_OVERFLOW;
            }
            need += 2UL;
            if (report != 0) {
                report->quad_lines++;
                report->valid_lines++;
            }
        } else {
            if (report != 0) {
                report->invalid_lines++;
            }
            rw3d_report_error(report, RW3D_ERR_PARSE, report != 0 ? report->line_count : 0UL, "RAW line must contain 9 triangle values or 12 quad values");
            if (flags & RW3D_RAW_F_STRICT) {
                return RW3D_ERR_PARSE;
            }
        }
    }

    *triangles_needed = (rw3d_u32)need;
    return RW3D_OK;
}


int rw3d_estimate_raw_arena(const char *data,
                            size_t length,
                            unsigned long flags,
                            size_t *bytes_needed,
                            rw3d_report *report)
{
    rw3d_u32 triangles_needed;
    rw3d_u32 vertices_needed;
    size_t vec_bytes;
    size_t tri_bytes;
    size_t align_pad;
    int st;

    if (report != 0) {
        rw3d_report_zero(report);
    }
    if (bytes_needed != 0) {
        *bytes_needed = 0UL;
    }
    if (flags == 0UL) {
        flags = RW3D_RAW_DEFAULT_FLAGS;
    }
    if (data == 0 || bytes_needed == 0) {
        rw3d_report_error(report, RW3D_ERR_ARG, 0UL, "invalid argument");
        return RW3D_ERR_ARG;
    }

    triangles_needed = 0UL;
    st = rw3d_count_pass(data, length, flags, report, &triangles_needed);
    if (st != RW3D_OK) {
        return st;
    }
    vertices_needed = triangles_needed * 3UL;
    if (triangles_needed != 0UL && vertices_needed / 3UL != triangles_needed) {
        rw3d_report_error(report, RW3D_ERR_OVERFLOW, 0UL, "vertex count overflow");
        return RW3D_ERR_OVERFLOW;
    }

    vec_bytes = (size_t)vertices_needed * sizeof(rw3d_vec3q);
    tri_bytes = (size_t)triangles_needed * sizeof(rw3d_tri);
    if ((rw3d_u32)(vec_bytes / sizeof(rw3d_vec3q)) != vertices_needed ||
        (rw3d_u32)(tri_bytes / sizeof(rw3d_tri)) != triangles_needed) {
        rw3d_report_error(report, RW3D_ERR_OVERFLOW, 0UL, "arena byte count overflow");
        return RW3D_ERR_OVERFLOW;
    }
    align_pad = rw3d_align_default() * 2UL;
    if (vec_bytes > ((size_t)(~0UL)) - tri_bytes ||
        vec_bytes + tri_bytes > ((size_t)(~0UL)) - align_pad) {
        rw3d_report_error(report, RW3D_ERR_OVERFLOW, 0UL, "arena estimate overflow");
        return RW3D_ERR_OVERFLOW;
    }
    *bytes_needed = vec_bytes + tri_bytes + align_pad;
    if (report != 0) {
        report->bytes_needed = *bytes_needed;
        report->status = RW3D_OK;
        report->message = "ok";
    }
    return RW3D_OK;
}

static int rw3d_emit_face(rw3d_mesh *mesh,
                          rw3d_vec3q v[4],
                          int count,
                          unsigned long flags,
                          rw3d_report *report)
{
    rw3d_u32 i0;
    rw3d_u32 i1;
    rw3d_u32 i2;
    rw3d_u32 i3;
    int st;

    st = rw3d_mesh_add_vertex(mesh, &v[0], flags, report, &i0);
    if (st != RW3D_OK) {
        return st;
    }
    st = rw3d_mesh_add_vertex(mesh, &v[1], flags, report, &i1);
    if (st != RW3D_OK) {
        return st;
    }
    st = rw3d_mesh_add_vertex(mesh, &v[2], flags, report, &i2);
    if (st != RW3D_OK) {
        return st;
    }
    st = rw3d_mesh_add_tri(mesh, i0, i1, i2, flags, report);
    if (st != RW3D_OK) {
        return st;
    }

    if (count == 4) {
        st = rw3d_mesh_add_vertex(mesh, &v[3], flags, report, &i3);
        if (st != RW3D_OK) {
            return st;
        }
        st = rw3d_mesh_add_tri(mesh, i0, i2, i3, flags, report);
        if (st != RW3D_OK) {
            return st;
        }
    }
    return RW3D_OK;
}

static int rw3d_parse_pass(const char *data,
                           size_t length,
                           unsigned long flags,
                           rw3d_mesh *mesh,
                           rw3d_report *report)
{
    const char *p;
    const char *end;
    const char *line;
    const char *line_end;
    rw3d_fix values[12];
    rw3d_vec3q verts[4];
    int count;
    int err;
    int overflow;
    int st;
    unsigned long line_number;

    p = data;
    end = data + length;
    line_number = 0UL;

    while (p < end) {
        line = p;
        while (p < end && !rw3d_is_line_break(*p)) {
            p++;
        }
        line_end = p;
        if (p < end && *p == '\r') {
            p++;
            if (p < end && *p == '\n') {
                p++;
            }
        } else if (p < end && *p == '\n') {
            p++;
        }
        line_number++;

        overflow = 0;
        err = rw3d_parse_line_values(line, line_end, flags, values, &count, &overflow);
        if (err != RW3D_OK || overflow) {
            if (flags & RW3D_RAW_F_STRICT) {
                return overflow ? RW3D_ERR_OVERFLOW : RW3D_ERR_PARSE;
            }
            continue;
        }
        if (count == 9 || (count == 12 && (flags & RW3D_RAW_F_ALLOW_QUADS))) {
            verts[0].x = values[0];
            verts[0].y = values[1];
            verts[0].z = values[2];
            verts[1].x = values[3];
            verts[1].y = values[4];
            verts[1].z = values[5];
            verts[2].x = values[6];
            verts[2].y = values[7];
            verts[2].z = values[8];
            if (count == 12) {
                verts[3].x = values[9];
                verts[3].y = values[10];
                verts[3].z = values[11];
            }
            st = rw3d_emit_face(mesh, verts, count == 12 ? 4 : 3, flags, report);
            if (st != RW3D_OK) {
                rw3d_report_error(report, st, line_number, st == RW3D_ERR_CAPACITY ? "mesh capacity exceeded" : "mesh emit failed");
                return st;
            }
        }
    }

    return RW3D_OK;
}

int rw3d_parse_raw_memory(const char *data,
                          size_t length,
                          rw3d_arena *arena,
                          unsigned long flags,
                          rw3d_mesh *mesh,
                          rw3d_report *report)
{
    rw3d_u32 triangles_needed;
    rw3d_u32 vertices_needed;
    size_t vec_bytes;
    size_t tri_bytes;
    int st;

    if (report != 0) {
        rw3d_report_zero(report);
    }
    if (mesh != 0) {
        rw3d_mesh_zero(mesh);
    }
    if (flags == 0UL) {
        flags = RW3D_RAW_DEFAULT_FLAGS;
    }

    if (data == 0 || arena == 0 || mesh == 0) {
        rw3d_report_error(report, RW3D_ERR_ARG, 0UL, "invalid argument");
        return RW3D_ERR_ARG;
    }

    triangles_needed = 0;
    st = rw3d_count_pass(data, length, flags, report, &triangles_needed);
    if (st != RW3D_OK) {
        if (report != 0) {
            report->arena_used = arena->used;
        }
        return st;
    }

    vertices_needed = triangles_needed * 3UL;
    if (triangles_needed != 0UL && vertices_needed / 3UL != triangles_needed) {
        rw3d_report_error(report, RW3D_ERR_OVERFLOW, 0UL, "vertex count overflow");
        return RW3D_ERR_OVERFLOW;
    }

    mesh->vertex_capacity = vertices_needed;
    mesh->triangle_capacity = triangles_needed;

    if (vertices_needed > 0UL) {
        vec_bytes = (size_t)vertices_needed * sizeof(rw3d_vec3q);
        tri_bytes = (size_t)triangles_needed * sizeof(rw3d_tri);
        if ((rw3d_u32)(vec_bytes / sizeof(rw3d_vec3q)) != vertices_needed ||
            (rw3d_u32)(tri_bytes / sizeof(rw3d_tri)) != triangles_needed) {
            rw3d_report_error(report, RW3D_ERR_OVERFLOW, 0UL, "arena byte count overflow");
            return RW3D_ERR_OVERFLOW;
        }
        mesh->vertices = (rw3d_vec3q *)rw3d_arena_alloc(arena, vec_bytes, rw3d_align_default());
        mesh->triangles = (rw3d_tri *)rw3d_arena_alloc(arena, tri_bytes, rw3d_align_default());
        if (mesh->vertices == 0 || mesh->triangles == 0) {
            rw3d_mesh_zero(mesh);
            rw3d_report_error(report, RW3D_ERR_ARENA, 0UL, "arena too small");
            if (report != 0) {
                report->arena_used = arena->used;
            }
            return RW3D_ERR_ARENA;
        }
    }

    st = rw3d_parse_pass(data, length, flags, mesh, report);
    if (st != RW3D_OK) {
        if (report != 0) {
            report->arena_used = arena->used;
        }
        return st;
    }

    if (report != 0) {
        report->status = RW3D_OK;
        report->message = "ok";
        report->arena_used = arena->used;
    }
    return RW3D_OK;
}

static int rw3d_emit_char(char *out, size_t out_cap, size_t *needed, char c)
{
    if (*needed + 1UL < out_cap && out != 0) {
        out[*needed] = c;
    }
    *needed = *needed + 1UL;
    return RW3D_OK;
}

static int rw3d_emit_text(char *out, size_t out_cap, size_t *needed, const char *s)
{
    while (*s != '\0') {
        rw3d_emit_char(out, out_cap, needed, *s);
        s++;
    }
    return RW3D_OK;
}

static int rw3d_u32_to_decimal(unsigned long v, char *out, size_t out_cap)
{
    char tmp[16];
    int n;
    int i;

    if (out_cap == 0) {
        return 0;
    }
    n = 0;
    if (v == 0UL) {
        if (out_cap > 1UL) {
            out[0] = '0';
            out[1] = '\0';
        } else {
            out[0] = '\0';
        }
        return out_cap > 1UL ? 1 : 0;
    }
    while (v != 0UL && n < (int)sizeof(tmp)) {
        tmp[n] = (char)('0' + (v % 10UL));
        v /= 10UL;
        n++;
    }
    i = 0;
    while (n > 0 && (size_t)i + 1UL < out_cap) {
        n--;
        out[i] = tmp[n];
        i++;
    }
    out[i] = '\0';
    return i;
}

int rw3d_fix_to_decimal(rw3d_fix value, char *out, size_t out_cap, int frac_digits)
{
    unsigned long mag;
    unsigned long whole;
    unsigned long frac;
    char tmp[32];
    int pos;
    int n;
    int i;
    unsigned long digit;

    if (out == 0 || out_cap == 0) {
        return 0;
    }
    if (frac_digits < 0) {
        frac_digits = 0;
    }
    if (frac_digits > 6) {
        frac_digits = 6;
    }
    pos = 0;
    if (value < 0) {
        if ((size_t)pos + 1UL < out_cap) {
            out[pos] = '-';
            pos++;
        }
        mag = rw3d_abs_fix_to_ulong(value);
    } else {
        mag = (unsigned long)value;
    }

    whole = mag >> RW3D_FIX_SHIFT;
    frac = mag & 65535UL;
    n = rw3d_u32_to_decimal(whole, tmp, sizeof(tmp));
    for (i = 0; i < n; ++i) {
        if ((size_t)pos + 1UL < out_cap) {
            out[pos] = tmp[i];
            pos++;
        }
    }

    if (frac_digits > 0) {
        if ((size_t)pos + 1UL < out_cap) {
            out[pos] = '.';
            pos++;
        }
        for (i = 0; i < frac_digits; ++i) {
            frac *= 10UL;
            digit = frac >> RW3D_FIX_SHIFT;
            frac = frac & 65535UL;
            if ((size_t)pos + 1UL < out_cap) {
                out[pos] = (char)('0' + digit);
                pos++;
            }
        }
    }

    if ((size_t)pos < out_cap) {
        out[pos] = '\0';
    } else {
        out[out_cap - 1UL] = '\0';
    }
    return pos;
}

static void rw3d_write_vec(char *out, size_t out_cap, size_t *needed, const rw3d_vec3q *v, int frac_digits)
{
    char num[40];
    rw3d_fix_to_decimal(v->x, num, sizeof(num), frac_digits);
    rw3d_emit_text(out, out_cap, needed, num);
    rw3d_emit_char(out, out_cap, needed, ' ');
    rw3d_fix_to_decimal(v->y, num, sizeof(num), frac_digits);
    rw3d_emit_text(out, out_cap, needed, num);
    rw3d_emit_char(out, out_cap, needed, ' ');
    rw3d_fix_to_decimal(v->z, num, sizeof(num), frac_digits);
    rw3d_emit_text(out, out_cap, needed, num);
}

int rw3d_write_raw_memory(const rw3d_mesh *mesh,
                          char *out,
                          size_t out_cap,
                          unsigned long flags,
                          rw3d_report *report)
{
    size_t needed;
    rw3d_u32 i;
    const rw3d_tri *t;
    int frac_digits;

    if (report != 0) {
        rw3d_report_zero(report);
    }
    if (mesh == 0 || (out == 0 && out_cap != 0)) {
        rw3d_report_error(report, RW3D_ERR_ARG, 0UL, "invalid argument");
        return RW3D_ERR_ARG;
    }

    frac_digits = (flags & RW3D_WRITE_F_SIX_DECIMALS) ? 6 : 4;
    needed = 0UL;

    for (i = 0; i < mesh->triangle_count; ++i) {
        t = &mesh->triangles[i];
        if (t->a >= mesh->vertex_count || t->b >= mesh->vertex_count || t->c >= mesh->vertex_count) {
            rw3d_report_error(report, RW3D_ERR_PARSE, 0UL, "triangle index out of range");
            return RW3D_ERR_PARSE;
        }
        rw3d_write_vec(out, out_cap, &needed, &mesh->vertices[t->a], frac_digits);
        rw3d_emit_char(out, out_cap, &needed, ' ');
        rw3d_write_vec(out, out_cap, &needed, &mesh->vertices[t->b], frac_digits);
        rw3d_emit_char(out, out_cap, &needed, ' ');
        rw3d_write_vec(out, out_cap, &needed, &mesh->vertices[t->c], frac_digits);
        if (flags & RW3D_WRITE_F_CRLF) {
            rw3d_emit_char(out, out_cap, &needed, '\r');
            rw3d_emit_char(out, out_cap, &needed, '\n');
        } else {
            rw3d_emit_char(out, out_cap, &needed, '\n');
        }
    }

    if (out_cap > 0UL && out != 0) {
        if (needed < out_cap) {
            out[needed] = '\0';
        } else {
            out[out_cap - 1UL] = '\0';
        }
    }
    if (report != 0) {
        report->bytes_needed = needed + 1UL;
        report->bytes_written = needed < out_cap ? needed : (out_cap == 0UL ? 0UL : out_cap - 1UL);
        if (needed + 1UL > out_cap) {
            report->status = RW3D_ERR_CAPACITY;
            report->message = "output buffer too small";
            return RW3D_ERR_CAPACITY;
        }
        report->status = RW3D_OK;
        report->message = "ok";
    }
    if (needed + 1UL > out_cap) {
        return RW3D_ERR_CAPACITY;
    }
    return RW3D_OK;
}

int rw3d_recompute_normals(rw3d_mesh *mesh)
{
    rw3d_u32 i;
    rw3d_tri *t;

    if (mesh == 0) {
        return RW3D_ERR_ARG;
    }
    for (i = 0; i < mesh->triangle_count; ++i) {
        t = &mesh->triangles[i];
        if (t->a >= mesh->vertex_count || t->b >= mesh->vertex_count || t->c >= mesh->vertex_count) {
            return RW3D_ERR_PARSE;
        }
        t->normal = rw3d_compute_tri_normal(mesh, t->a, t->b, t->c);
    }
    return RW3D_OK;
}

int rw3d_validate_mesh(const rw3d_mesh *mesh, rw3d_report *report)
{
    rw3d_u32 i;
    const rw3d_tri *t;

    if (report != 0) {
        rw3d_report_zero(report);
    }
    if (mesh == 0) {
        rw3d_report_error(report, RW3D_ERR_ARG, 0UL, "invalid mesh");
        return RW3D_ERR_ARG;
    }
    for (i = 0; i < mesh->triangle_count; ++i) {
        t = &mesh->triangles[i];
        if (t->a >= mesh->vertex_count || t->b >= mesh->vertex_count || t->c >= mesh->vertex_count) {
            rw3d_report_error(report, RW3D_ERR_PARSE, (unsigned long)i + 1UL, "triangle index out of range");
            return RW3D_ERR_PARSE;
        }
    }
    return RW3D_OK;
}


static void rw3d_set_report_status(rw3d_report *report, int status, const char *message, size_t needed, size_t written)
{
    if (report == 0) {
        return;
    }
    report->status = status;
    report->message = message;
    report->bytes_needed = needed;
    report->bytes_written = written;
}

static int rw3d_finish_writer(rw3d_report *report, size_t out_cap, size_t needed)
{
    size_t written;
    written = needed < out_cap ? needed : (out_cap == 0UL ? 0UL : out_cap - 1UL);
    if (needed + 1UL > out_cap) {
        rw3d_set_report_status(report, RW3D_ERR_CAPACITY, "output buffer too small", needed + 1UL, written);
        return RW3D_ERR_CAPACITY;
    }
    rw3d_set_report_status(report, RW3D_OK, "ok", needed + 1UL, needed);
    return RW3D_OK;
}

int rw3d_write_obj_memory(const rw3d_mesh *mesh,
                          char *out,
                          size_t out_cap,
                          unsigned long flags,
                          rw3d_report *report)
{
    size_t needed;
    rw3d_u32 i;
    char num[40];
    int frac_digits;
    const rw3d_vec3q *v;
    const rw3d_tri *t;

    if (report != 0) {
        rw3d_report_zero(report);
    }
    if (mesh == 0 || (out == 0 && out_cap != 0)) {
        rw3d_report_error(report, RW3D_ERR_ARG, 0UL, "invalid argument");
        return RW3D_ERR_ARG;
    }

    frac_digits = (flags & RW3D_WRITE_F_SIX_DECIMALS) ? 6 : 4;
    needed = 0UL;
    rw3d_emit_text(out, out_cap, &needed, "# rawtri89 OBJ export\n");
    rw3d_emit_text(out, out_cap, &needed, "o rawtri89_mesh\n");

    for (i = 0; i < mesh->vertex_count; ++i) {
        v = &mesh->vertices[i];
        rw3d_emit_text(out, out_cap, &needed, "v ");
        rw3d_fix_to_decimal(v->x, num, sizeof(num), frac_digits);
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, ' ');
        rw3d_fix_to_decimal(v->y, num, sizeof(num), frac_digits);
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, ' ');
        rw3d_fix_to_decimal(v->z, num, sizeof(num), frac_digits);
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, '\n');
    }

    for (i = 0; i < mesh->triangle_count; ++i) {
        t = &mesh->triangles[i];
        if (t->a >= mesh->vertex_count || t->b >= mesh->vertex_count || t->c >= mesh->vertex_count) {
            rw3d_report_error(report, RW3D_ERR_PARSE, 0UL, "triangle index out of range");
            return RW3D_ERR_PARSE;
        }
        rw3d_emit_text(out, out_cap, &needed, "f ");
        rw3d_u32_to_decimal((unsigned long)t->a + 1UL, num, sizeof(num));
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, ' ');
        rw3d_u32_to_decimal((unsigned long)t->b + 1UL, num, sizeof(num));
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, ' ');
        rw3d_u32_to_decimal((unsigned long)t->c + 1UL, num, sizeof(num));
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, '\n');
    }

    if (out_cap > 0UL && out != 0) {
        if (needed < out_cap) {
            out[needed] = '\0';
        } else {
            out[out_cap - 1UL] = '\0';
        }
    }
    return rw3d_finish_writer(report, out_cap, needed);
}

int rw3d_write_ply_ascii_memory(const rw3d_mesh *mesh,
                                char *out,
                                size_t out_cap,
                                unsigned long flags,
                                rw3d_report *report)
{
    size_t needed;
    rw3d_u32 i;
    char num[40];
    int frac_digits;
    const rw3d_vec3q *v;
    const rw3d_tri *t;

    if (report != 0) {
        rw3d_report_zero(report);
    }
    if (mesh == 0 || (out == 0 && out_cap != 0)) {
        rw3d_report_error(report, RW3D_ERR_ARG, 0UL, "invalid argument");
        return RW3D_ERR_ARG;
    }

    frac_digits = (flags & RW3D_WRITE_F_SIX_DECIMALS) ? 6 : 4;
    needed = 0UL;
    rw3d_emit_text(out, out_cap, &needed, "ply\nformat ascii 1.0\ncomment rawtri89 PLY export\nelement vertex ");
    rw3d_u32_to_decimal((unsigned long)mesh->vertex_count, num, sizeof(num));
    rw3d_emit_text(out, out_cap, &needed, num);
    rw3d_emit_text(out, out_cap, &needed, "\nproperty float x\nproperty float y\nproperty float z\nelement face ");
    rw3d_u32_to_decimal((unsigned long)mesh->triangle_count, num, sizeof(num));
    rw3d_emit_text(out, out_cap, &needed, num);
    rw3d_emit_text(out, out_cap, &needed, "\nproperty list uchar int vertex_indices\nend_header\n");

    for (i = 0; i < mesh->vertex_count; ++i) {
        v = &mesh->vertices[i];
        rw3d_fix_to_decimal(v->x, num, sizeof(num), frac_digits);
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, ' ');
        rw3d_fix_to_decimal(v->y, num, sizeof(num), frac_digits);
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, ' ');
        rw3d_fix_to_decimal(v->z, num, sizeof(num), frac_digits);
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, '\n');
    }

    for (i = 0; i < mesh->triangle_count; ++i) {
        t = &mesh->triangles[i];
        if (t->a >= mesh->vertex_count || t->b >= mesh->vertex_count || t->c >= mesh->vertex_count) {
            rw3d_report_error(report, RW3D_ERR_PARSE, 0UL, "triangle index out of range");
            return RW3D_ERR_PARSE;
        }
        rw3d_emit_text(out, out_cap, &needed, "3 ");
        rw3d_u32_to_decimal((unsigned long)t->a, num, sizeof(num));
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, ' ');
        rw3d_u32_to_decimal((unsigned long)t->b, num, sizeof(num));
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, ' ');
        rw3d_u32_to_decimal((unsigned long)t->c, num, sizeof(num));
        rw3d_emit_text(out, out_cap, &needed, num);
        rw3d_emit_char(out, out_cap, &needed, '\n');
    }

    if (out_cap > 0UL && out != 0) {
        if (needed < out_cap) {
            out[needed] = '\0';
        } else {
            out[out_cap - 1UL] = '\0';
        }
    }
    return rw3d_finish_writer(report, out_cap, needed);
}

int rw3d_recompute_bounds(rw3d_mesh *mesh)
{
    rw3d_u32 i;

    if (mesh == 0) {
        return RW3D_ERR_ARG;
    }
    mesh->has_bounds = 0;
    for (i = 0; i < mesh->vertex_count; ++i) {
        rw3d_update_bounds(mesh, &mesh->vertices[i]);
    }
    return RW3D_OK;
}

static void rw3d_u32_sort3(rw3d_u32 *a, rw3d_u32 *b, rw3d_u32 *c)
{
    rw3d_u32 t;
    if (*a > *b) { t = *a; *a = *b; *b = t; }
    if (*b > *c) { t = *b; *b = *c; *c = t; }
    if (*a > *b) { t = *a; *a = *b; *b = t; }
}

static void rw3d_edge_canon(const rw3d_tri *t, int edge, rw3d_u32 *a, rw3d_u32 *b)
{
    if (edge == 0) {
        *a = t->a;
        *b = t->b;
    } else if (edge == 1) {
        *a = t->b;
        *b = t->c;
    } else {
        *a = t->c;
        *b = t->a;
    }
    if (*a > *b) {
        rw3d_u32 tmp;
        tmp = *a;
        *a = *b;
        *b = tmp;
    }
}

static int rw3d_edges_same(const rw3d_mesh *mesh, rw3d_u32 a, rw3d_u32 b, rw3d_u32 ti, int ei)
{
    rw3d_u32 c;
    rw3d_u32 d;
    rw3d_edge_canon(&mesh->triangles[ti], ei, &c, &d);
    return a == c && b == d;
}

static int rw3d_edge_first_occurrence(const rw3d_mesh *mesh, rw3d_u32 ti, int ei)
{
    rw3d_u32 a;
    rw3d_u32 b;
    rw3d_u32 j;
    int ej;

    rw3d_edge_canon(&mesh->triangles[ti], ei, &a, &b);
    for (j = 0; j <= ti; ++j) {
        for (ej = 0; ej < 3; ++ej) {
            if (j == ti && ej == ei) {
                return 1;
            }
            if (rw3d_edges_same(mesh, a, b, j, ej)) {
                return 0;
            }
        }
    }
    return 1;
}

static unsigned long rw3d_edge_count_occurrences(const rw3d_mesh *mesh, rw3d_u32 ti, int ei)
{
    rw3d_u32 a;
    rw3d_u32 b;
    rw3d_u32 j;
    int ej;
    unsigned long count;

    rw3d_edge_canon(&mesh->triangles[ti], ei, &a, &b);
    count = 0UL;
    for (j = 0; j < mesh->triangle_count; ++j) {
        for (ej = 0; ej < 3; ++ej) {
            if (rw3d_edges_same(mesh, a, b, j, ej)) {
                count++;
            }
        }
    }
    return count;
}

static int rw3d_vertex_used(const rw3d_mesh *mesh, rw3d_u32 v)
{
    rw3d_u32 i;
    const rw3d_tri *t;
    for (i = 0; i < mesh->triangle_count; ++i) {
        t = &mesh->triangles[i];
        if (t->a == v || t->b == v || t->c == v) {
            return 1;
        }
    }
    return 0;
}

int rw3d_analyze_mesh(const rw3d_mesh *mesh, rw3d_mesh_stats *stats)
{
    rw3d_u32 i;
    rw3d_u32 j;
    rw3d_u32 a0;
    rw3d_u32 b0;
    rw3d_u32 c0;
    rw3d_u32 a1;
    rw3d_u32 b1;
    rw3d_u32 c1;
    int e;
    unsigned long occ;
    const rw3d_tri *t;
    rw3d_vec3q n;

    if (stats != 0) {
        rw3d_mesh_stats_zero(stats);
    }
    if (mesh == 0 || stats == 0) {
        return RW3D_ERR_ARG;
    }
    stats->vertices = (unsigned long)mesh->vertex_count;
    stats->triangles = (unsigned long)mesh->triangle_count;

    for (i = 0; i < mesh->triangle_count; ++i) {
        t = &mesh->triangles[i];
        if (t->a >= mesh->vertex_count || t->b >= mesh->vertex_count || t->c >= mesh->vertex_count) {
            return RW3D_ERR_PARSE;
        }
        if (t->a == t->b || t->b == t->c || t->a == t->c) {
            stats->degenerate_triangles++;
        }
        n = rw3d_compute_tri_normal(mesh, t->a, t->b, t->c);
        if (rw3d_normal_is_zero(&n)) {
            stats->zero_area_triangles++;
        }
        a0 = t->a;
        b0 = t->b;
        c0 = t->c;
        rw3d_u32_sort3(&a0, &b0, &c0);
        for (j = i + 1UL; j < mesh->triangle_count; ++j) {
            a1 = mesh->triangles[j].a;
            b1 = mesh->triangles[j].b;
            c1 = mesh->triangles[j].c;
            rw3d_u32_sort3(&a1, &b1, &c1);
            if (a0 == a1 && b0 == b1 && c0 == c1) {
                stats->duplicate_triangles++;
            }
        }
    }

    for (i = 0; i < mesh->triangle_count; ++i) {
        for (e = 0; e < 3; ++e) {
            if (rw3d_edge_first_occurrence(mesh, i, e)) {
                stats->edges_unique++;
                occ = rw3d_edge_count_occurrences(mesh, i, e);
                if (occ == 1UL) {
                    stats->boundary_edges++;
                } else if (occ > 2UL) {
                    stats->nonmanifold_edges++;
                }
            }
        }
    }

    for (i = 0; i < mesh->vertex_count; ++i) {
        if (!rw3d_vertex_used(mesh, i)) {
            stats->isolated_vertices++;
        }
    }
    return RW3D_OK;
}

int rw3d_remove_degenerate_tris_in_place(rw3d_mesh *mesh, rw3d_report *report)
{
    rw3d_u32 i;
    rw3d_u32 w;
    rw3d_tri *t;
    rw3d_vec3q n;

    if (report != 0) {
        rw3d_report_zero(report);
    }
    if (mesh == 0) {
        rw3d_report_error(report, RW3D_ERR_ARG, 0UL, "invalid mesh");
        return RW3D_ERR_ARG;
    }
    w = 0UL;
    for (i = 0; i < mesh->triangle_count; ++i) {
        t = &mesh->triangles[i];
        if (t->a >= mesh->vertex_count || t->b >= mesh->vertex_count || t->c >= mesh->vertex_count) {
            rw3d_report_error(report, RW3D_ERR_PARSE, (unsigned long)i + 1UL, "triangle index out of range");
            return RW3D_ERR_PARSE;
        }
        n = rw3d_compute_tri_normal(mesh, t->a, t->b, t->c);
        if (t->a == t->b || t->b == t->c || t->a == t->c || rw3d_normal_is_zero(&n)) {
            if (report != 0) {
                report->dropped_degenerate++;
            }
            continue;
        }
        mesh->triangles[w] = *t;
        mesh->triangles[w].normal = n;
        w++;
    }
    mesh->triangle_count = w;
    rw3d_recompute_bounds(mesh);
    if (report != 0) {
        report->status = RW3D_OK;
        report->message = "ok";
    }
    return RW3D_OK;
}


static int rw3d_tri_same_canon(const rw3d_tri *a, const rw3d_tri *b)
{
    rw3d_u32 a0;
    rw3d_u32 a1;
    rw3d_u32 a2;
    rw3d_u32 b0;
    rw3d_u32 b1;
    rw3d_u32 b2;

    a0 = a->a;
    a1 = a->b;
    a2 = a->c;
    b0 = b->a;
    b1 = b->b;
    b2 = b->c;
    rw3d_u32_sort3(&a0, &a1, &a2);
    rw3d_u32_sort3(&b0, &b1, &b2);
    return a0 == b0 && a1 == b1 && a2 == b2;
}

int rw3d_remove_duplicate_tris_in_place(rw3d_mesh *mesh, rw3d_report *report)
{
    rw3d_u32 i;
    rw3d_u32 j;
    rw3d_u32 w;
    int duplicate;

    if (report != 0) {
        rw3d_report_zero(report);
    }
    if (mesh == 0) {
        rw3d_report_error(report, RW3D_ERR_ARG, 0UL, "invalid mesh");
        return RW3D_ERR_ARG;
    }
    w = 0UL;
    for (i = 0; i < mesh->triangle_count; ++i) {
        if (mesh->triangles[i].a >= mesh->vertex_count ||
            mesh->triangles[i].b >= mesh->vertex_count ||
            mesh->triangles[i].c >= mesh->vertex_count) {
            rw3d_report_error(report, RW3D_ERR_PARSE, (unsigned long)i + 1UL, "triangle index out of range");
            return RW3D_ERR_PARSE;
        }
        duplicate = 0;
        for (j = 0; j < w; ++j) {
            if (rw3d_tri_same_canon(&mesh->triangles[i], &mesh->triangles[j])) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            if (report != 0) {
                report->dropped_duplicate_triangles++;
            }
            continue;
        }
        mesh->triangles[w] = mesh->triangles[i];
        w++;
    }
    mesh->triangle_count = w;
    rw3d_recompute_normals(mesh);
    rw3d_recompute_bounds(mesh);
    if (report != 0) {
        report->status = RW3D_OK;
        report->message = "ok";
    }
    return RW3D_OK;
}

int rw3d_flip_winding(rw3d_mesh *mesh)
{
    rw3d_u32 i;
    rw3d_u32 tmp;

    if (mesh == 0) {
        return RW3D_ERR_ARG;
    }
    for (i = 0; i < mesh->triangle_count; ++i) {
        tmp = mesh->triangles[i].b;
        mesh->triangles[i].b = mesh->triangles[i].c;
        mesh->triangles[i].c = tmp;
        mesh->triangles[i].normal.x = -mesh->triangles[i].normal.x;
        mesh->triangles[i].normal.y = -mesh->triangles[i].normal.y;
        mesh->triangles[i].normal.z = -mesh->triangles[i].normal.z;
    }
    return RW3D_OK;
}

int rw3d_scale_translate(rw3d_mesh *mesh, rw3d_fix scale, rw3d_vec3q translate)
{
    rw3d_u32 i;

    if (mesh == 0) {
        return RW3D_ERR_ARG;
    }
    for (i = 0; i < mesh->vertex_count; ++i) {
        mesh->vertices[i].x = rw3d_fix_add_sat(rw3d_fix_mul(mesh->vertices[i].x, scale), translate.x);
        mesh->vertices[i].y = rw3d_fix_add_sat(rw3d_fix_mul(mesh->vertices[i].y, scale), translate.y);
        mesh->vertices[i].z = rw3d_fix_add_sat(rw3d_fix_mul(mesh->vertices[i].z, scale), translate.z);
    }
    rw3d_recompute_bounds(mesh);
    rw3d_recompute_normals(mesh);
    return RW3D_OK;
}

int rw3d_center_to_origin(rw3d_mesh *mesh)
{
    rw3d_vec3q center;
    rw3d_vec3q neg;

    if (mesh == 0) {
        return RW3D_ERR_ARG;
    }
    if (!mesh->has_bounds) {
        rw3d_recompute_bounds(mesh);
    }
    if (!mesh->has_bounds) {
        return RW3D_OK;
    }
    center.x = rw3d_fix_div_int_sat(rw3d_fix_add_sat(mesh->bounds_min.x, mesh->bounds_max.x), 2L);
    center.y = rw3d_fix_div_int_sat(rw3d_fix_add_sat(mesh->bounds_min.y, mesh->bounds_max.y), 2L);
    center.z = rw3d_fix_div_int_sat(rw3d_fix_add_sat(mesh->bounds_min.z, mesh->bounds_max.z), 2L);
    neg.x = -center.x;
    neg.y = -center.y;
    neg.z = -center.z;
    return rw3d_scale_translate(mesh, RW3D_FIX_ONE, neg);
}

const char *rw3d_status_name(int status)
{
    switch (status) {
    case RW3D_OK:
        return "RW3D_OK";
    case RW3D_ERR_ARG:
        return "RW3D_ERR_ARG";
    case RW3D_ERR_ARENA:
        return "RW3D_ERR_ARENA";
    case RW3D_ERR_PARSE:
        return "RW3D_ERR_PARSE";
    case RW3D_ERR_OVERFLOW:
        return "RW3D_ERR_OVERFLOW";
    case RW3D_ERR_CAPACITY:
        return "RW3D_ERR_CAPACITY";
    case RW3D_ERR_UNSUPPORTED:
        return "RW3D_ERR_UNSUPPORTED";
    default:
        return "RW3D_UNKNOWN";
    }
}
