#include "nff89_cad.h"

#define NFF89_CAD_TRUE 1U
#define NFF89_CAD_FALSE 0U
#define NFF89_CAD_EPS 1L

static nff89_fix cad_abs(nff89_fix a)
{
    return a < 0L ? -a : a;
}

static nff89_fix cad_clamp(nff89_fix x, nff89_fix a, nff89_fix b)
{
    if (x < a) return a;
    if (x > b) return b;
    return x;
}


static nff89_fix cad_min2(nff89_fix a, nff89_fix b)
{
    return a < b ? a : b;
}

static nff89_fix cad_max2(nff89_fix a, nff89_fix b)
{
    return a > b ? a : b;
}

static void cad_trim_loop_reset(nff89_cad_trim_poly *loop)
{
    loop->first = 0U;
    loop->count = 0U;
    loop->u_min = 0L;
    loop->u_max = 0L;
    loop->v_min = 0L;
    loop->v_max = 0L;
    loop->signed_area = 0L;
    loop->closed = NFF89_CAD_FALSE;
}

static void cad_trim_loop_include(nff89_cad_trim_poly *loop, const nff89_cad_uv *p)
{
    if (loop->count == 0U) {
        loop->u_min = p->u;
        loop->u_max = p->u;
        loop->v_min = p->v;
        loop->v_max = p->v;
    } else {
        loop->u_min = cad_min2(loop->u_min, p->u);
        loop->u_max = cad_max2(loop->u_max, p->u);
        loop->v_min = cad_min2(loop->v_min, p->v);
        loop->v_max = cad_max2(loop->v_max, p->v);
    }
    loop->count++;
}

static nff89_fix cad_uv_cross(const nff89_cad_uv *a, const nff89_cad_uv *b, const nff89_cad_uv *c)
{
    nff89_fix ab_u;
    nff89_fix ab_v;
    nff89_fix ac_u;
    nff89_fix ac_v;
    ab_u = b->u - a->u;
    ab_v = b->v - a->v;
    ac_u = c->u - a->u;
    ac_v = c->v - a->v;
    return nff89_fix_mul(ab_u, ac_v) - nff89_fix_mul(ab_v, ac_u);
}

static unsigned cad_point_on_segment(nff89_fix u, nff89_fix v,
                                     const nff89_cad_uv *a,
                                     const nff89_cad_uv *b)
{
    nff89_cad_uv p;
    nff89_fix cr;
    nff89_fix tol;
    p.u = u;
    p.v = v;
    tol = 16L;
    if (u < cad_min2(a->u, b->u) - tol || u > cad_max2(a->u, b->u) + tol) return NFF89_CAD_FALSE;
    if (v < cad_min2(a->v, b->v) - tol || v > cad_max2(a->v, b->v) + tol) return NFF89_CAD_FALSE;
    cr = cad_uv_cross(a, b, &p);
    if (cad_abs(cr) <= tol) return NFF89_CAD_TRUE;
    return NFF89_CAD_FALSE;
}

static nff89_fix cad_triangle_uv_area2(const nff89_cad_vertex *a,
                                       const nff89_cad_vertex *b,
                                       const nff89_cad_vertex *c)
{
    nff89_fix cr;
    cr = cad_uv_cross(&a->uv, &b->uv, &c->uv);
    return cad_abs(cr);
}

static nff89_fix cad_lerp(nff89_fix a, nff89_fix b, nff89_fix t)
{
    return a + nff89_fix_mul(b - a, t);
}

static nff89_fix cad_sqrt(nff89_fix x)
{
    nff89_fix lo;
    nff89_fix hi;
    nff89_fix mid;
    nff89_fix sq;
    int i;
    if (x <= 0L) return 0L;
    lo = 0L;
    hi = x > NFF89_FIX_ONE ? x : NFF89_FIX_ONE;
    for (i = 0; i < 32; i++) {
        mid = lo + ((hi - lo) / 2L);
        sq = nff89_fix_mul(mid, mid);
        if (sq > x) hi = mid;
        else lo = mid;
    }
    return lo;
}

static nff89_vec3 cad_sub3(nff89_vec3 a, nff89_vec3 b)
{
    nff89_vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

static nff89_vec3 cad_cross3(nff89_vec3 a, nff89_vec3 b)
{
    nff89_vec3 r;
    r.x = nff89_fix_mul(a.y, b.z) - nff89_fix_mul(a.z, b.y);
    r.y = nff89_fix_mul(a.z, b.x) - nff89_fix_mul(a.x, b.z);
    r.z = nff89_fix_mul(a.x, b.y) - nff89_fix_mul(a.y, b.x);
    return r;
}

static void cad_normalize3(nff89_vec3 *v)
{
    nff89_fix len2;
    nff89_fix len;
    if (v == (nff89_vec3 *)0) return;
    len2 = nff89_fix_mul(v->x, v->x) + nff89_fix_mul(v->y, v->y) + nff89_fix_mul(v->z, v->z);
    len = cad_sqrt(len2);
    if (len <= NFF89_CAD_EPS) {
        v->x = 0L;
        v->y = 0L;
        v->z = NFF89_FIX_ONE;
        return;
    }
    v->x = nff89_fix_div(v->x, len);
    v->y = nff89_fix_div(v->y, len);
    v->z = nff89_fix_div(v->z, len);
}

static unsigned cad_nurbs_cp_index(const nff89_nurbs *s, unsigned u_i, unsigned v_i)
{
    return v_i * s->u_point_count + u_i;
}

static int cad_check_knots(const nff89_fix *k, unsigned count)
{
    unsigned i;
    if (k == (const nff89_fix *)0 || count < 2U) return NFF89_CAD_ERR_KNOTS;
    for (i = 1U; i < count; i++) {
        if (k[i] < k[i - 1U]) return NFF89_CAD_ERR_KNOTS;
    }
    return NFF89_CAD_OK;
}

int nff89_cad_validate_nurbs(const nff89_nurbs *s)
{
    unsigned cp_need;
    if (s == (const nff89_nurbs *)0) return NFF89_CAD_ERR_BAD_ARG;
    if (s->u_degree < 0 || s->v_degree < 0) return NFF89_CAD_ERR_DEGREE;
    if (s->u_degree > NFF89_CAD_MAX_DEGREE || s->v_degree > NFF89_CAD_MAX_DEGREE) return NFF89_CAD_ERR_DEGREE;
    if (s->u_point_count == 0U || s->v_point_count == 0U) return NFF89_CAD_ERR_CONTROL;
    if ((unsigned)s->u_degree >= s->u_point_count || (unsigned)s->v_degree >= s->v_point_count) return NFF89_CAD_ERR_DEGREE;
    if (s->u_knot_count < s->u_point_count + (unsigned)s->u_degree + 1U) return NFF89_CAD_ERR_KNOTS;
    if (s->v_knot_count < s->v_point_count + (unsigned)s->v_degree + 1U) return NFF89_CAD_ERR_KNOTS;
    if (s->u_knot_count > (unsigned)NFF89_MAX_NURBS_KNOTS || s->v_knot_count > (unsigned)NFF89_MAX_NURBS_KNOTS) return NFF89_CAD_ERR_KNOTS;
    cp_need = s->u_point_count * s->v_point_count;
    if (cp_need == 0U || cp_need > s->cp_count || cp_need > (unsigned)NFF89_MAX_NURBS_CP) return NFF89_CAD_ERR_CONTROL;
    if (cad_check_knots(s->u_knots, s->u_point_count + (unsigned)s->u_degree + 1U) != NFF89_CAD_OK) return NFF89_CAD_ERR_KNOTS;
    if (cad_check_knots(s->v_knots, s->v_point_count + (unsigned)s->v_degree + 1U) != NFF89_CAD_OK) return NFF89_CAD_ERR_KNOTS;
    if (s->u_knots[(unsigned)s->u_degree] >= s->u_knots[s->u_point_count]) return NFF89_CAD_ERR_KNOTS;
    if (s->v_knots[(unsigned)s->v_degree] >= s->v_knots[s->v_point_count]) return NFF89_CAD_ERR_KNOTS;
    return NFF89_CAD_OK;
}

int nff89_cad_nurbs_domain(const nff89_nurbs *s,
                           nff89_fix *u_min, nff89_fix *u_max,
                           nff89_fix *v_min, nff89_fix *v_max)
{
    int r;
    r = nff89_cad_validate_nurbs(s);
    if (r != NFF89_CAD_OK) return r;
    if (u_min != (nff89_fix *)0) *u_min = s->u_knots[(unsigned)s->u_degree];
    if (u_max != (nff89_fix *)0) *u_max = s->u_knots[s->u_point_count];
    if (v_min != (nff89_fix *)0) *v_min = s->v_knots[(unsigned)s->v_degree];
    if (v_max != (nff89_fix *)0) *v_max = s->v_knots[s->v_point_count];
    return NFF89_CAD_OK;
}

static int cad_find_span(unsigned n, int degree, nff89_fix u, const nff89_fix *knots)
{
    unsigned low;
    unsigned high;
    unsigned mid;
    if (u >= knots[n + 1U]) return (int)n;
    if (u <= knots[(unsigned)degree]) return degree;
    low = (unsigned)degree;
    high = n + 1U;
    mid = (low + high) / 2U;
    while (u < knots[mid] || u >= knots[mid + 1U]) {
        if (u < knots[mid]) high = mid;
        else low = mid;
        mid = (low + high) / 2U;
    }
    return (int)mid;
}

static int cad_basis_funs(int span, nff89_fix u, int degree, const nff89_fix *knots, nff89_fix *basis)
{
    nff89_fix left[NFF89_CAD_MAX_DEGREE + 1];
    nff89_fix right[NFF89_CAD_MAX_DEGREE + 1];
    nff89_fix saved;
    nff89_fix temp;
    nff89_fix denom;
    int j;
    int r;
    if (degree > NFF89_CAD_MAX_DEGREE) return NFF89_CAD_ERR_DEGREE;
    basis[0] = NFF89_FIX_ONE;
    for (j = 1; j <= degree; j++) {
        left[j] = u - knots[(unsigned)(span + 1 - j)];
        right[j] = knots[(unsigned)(span + j)] - u;
        saved = 0L;
        for (r = 0; r < j; r++) {
            denom = right[r + 1] + left[j - r];
            if (cad_abs(denom) <= NFF89_CAD_EPS) temp = 0L;
            else temp = nff89_fix_div(basis[r], denom);
            basis[r] = saved + nff89_fix_mul(right[r + 1], temp);
            saved = nff89_fix_mul(left[j - r], temp);
        }
        basis[j] = saved;
    }
    return NFF89_CAD_OK;
}

int nff89_cad_eval_nurbs_surface(const nff89_nurbs *s,
                                 nff89_fix u, nff89_fix v,
                                 nff89_vec3 *out_pos)
{
    nff89_fix u_min;
    nff89_fix u_max;
    nff89_fix v_min;
    nff89_fix v_max;
    nff89_fix Nu[NFF89_CAD_MAX_DEGREE + 1];
    nff89_fix Nv[NFF89_CAD_MAX_DEGREE + 1];
    nff89_fix wsum;
    nff89_fix sx;
    nff89_fix sy;
    nff89_fix sz;
    int uspan;
    int vspan;
    int l;
    int k;
    int r;
    unsigned ui;
    unsigned vi;
    unsigned idx;
    nff89_fix b;
    nff89_fix wb;
    const nff89_vec4 *cp;

    if (out_pos == (nff89_vec3 *)0) return NFF89_CAD_ERR_BAD_ARG;
    r = nff89_cad_nurbs_domain(s, &u_min, &u_max, &v_min, &v_max);
    if (r != NFF89_CAD_OK) return r;
    u = cad_clamp(u, u_min, u_max);
    v = cad_clamp(v, v_min, v_max);
    if (u == u_max) u = u_max - NFF89_CAD_EPS;
    if (v == v_max) v = v_max - NFF89_CAD_EPS;

    uspan = cad_find_span(s->u_point_count - 1U, s->u_degree, u, s->u_knots);
    vspan = cad_find_span(s->v_point_count - 1U, s->v_degree, v, s->v_knots);
    r = cad_basis_funs(uspan, u, s->u_degree, s->u_knots, Nu); if (r != NFF89_CAD_OK) return r;
    r = cad_basis_funs(vspan, v, s->v_degree, s->v_knots, Nv); if (r != NFF89_CAD_OK) return r;

    sx = 0L; sy = 0L; sz = 0L; wsum = 0L;
    for (l = 0; l <= s->v_degree; l++) {
        vi = (unsigned)(vspan - s->v_degree + l);
        for (k = 0; k <= s->u_degree; k++) {
            ui = (unsigned)(uspan - s->u_degree + k);
            idx = cad_nurbs_cp_index(s, ui, vi);
            if (idx >= s->cp_count) return NFF89_CAD_ERR_CONTROL;
            cp = &s->cp[idx];
            b = nff89_fix_mul(Nu[k], Nv[l]);
            wb = nff89_fix_mul(b, cp->w);
            sx += nff89_fix_mul(cp->x, wb);
            sy += nff89_fix_mul(cp->y, wb);
            sz += nff89_fix_mul(cp->z, wb);
            wsum += wb;
        }
    }
    if (cad_abs(wsum) <= NFF89_CAD_EPS) return NFF89_CAD_ERR_WEIGHT;
    out_pos->x = nff89_fix_div(sx, wsum);
    out_pos->y = nff89_fix_div(sy, wsum);
    out_pos->z = nff89_fix_div(sz, wsum);
    return NFF89_CAD_OK;
}

int nff89_cad_eval_nurbs_surface_vertex(const nff89_nurbs *s,
                                        nff89_fix u, nff89_fix v,
                                        nff89_cad_vertex *out_vertex)
{
    nff89_fix u_min;
    nff89_fix u_max;
    nff89_fix v_min;
    nff89_fix v_max;
    nff89_fix du;
    nff89_fix dv;
    nff89_vec3 pu0;
    nff89_vec3 pu1;
    nff89_vec3 pv0;
    nff89_vec3 pv1;
    nff89_vec3 tu;
    nff89_vec3 tv;
    int r;
    if (out_vertex == (nff89_cad_vertex *)0) return NFF89_CAD_ERR_BAD_ARG;
    r = nff89_cad_nurbs_domain(s, &u_min, &u_max, &v_min, &v_max);
    if (r != NFF89_CAD_OK) return r;
    r = nff89_cad_eval_nurbs_surface(s, u, v, &out_vertex->pos);
    if (r != NFF89_CAD_OK) return r;
    out_vertex->uv.u = u;
    out_vertex->uv.v = v;
    du = (u_max - u_min) / 256L;
    dv = (v_max - v_min) / 256L;
    if (du < NFF89_CAD_EPS) du = NFF89_CAD_EPS;
    if (dv < NFF89_CAD_EPS) dv = NFF89_CAD_EPS;
    nff89_cad_eval_nurbs_surface(s, cad_clamp(u - du, u_min, u_max), v, &pu0);
    nff89_cad_eval_nurbs_surface(s, cad_clamp(u + du, u_min, u_max), v, &pu1);
    nff89_cad_eval_nurbs_surface(s, u, cad_clamp(v - dv, v_min, v_max), &pv0);
    nff89_cad_eval_nurbs_surface(s, u, cad_clamp(v + dv, v_min, v_max), &pv1);
    tu = cad_sub3(pu1, pu0);
    tv = cad_sub3(pv1, pv0);
    out_vertex->normal = cad_cross3(tu, tv);
    cad_normalize3(&out_vertex->normal);
    out_vertex->has_normal = NFF89_CAD_TRUE;
    return NFF89_CAD_OK;
}

int nff89_cad_eval_trim_curve(const nff89_trim *trim,
                              nff89_fix u,
                              nff89_cad_uv *out_uv)
{
    nff89_fix basis[NFF89_CAD_MAX_DEGREE + 1];
    nff89_fix u_min;
    nff89_fix u_max;
    nff89_fix sx;
    nff89_fix sy;
    nff89_fix sw;
    int degree;
    int span;
    int k;
    int r;
    unsigned n;
    unsigned idx;
    nff89_fix wb;
    const nff89_vec4 *cp;
    if (trim == (const nff89_trim *)0 || out_uv == (nff89_cad_uv *)0) return NFF89_CAD_ERR_BAD_ARG;
    if (trim->order <= 0) return NFF89_CAD_ERR_DEGREE;
    degree = trim->order - 1;
    if (degree > NFF89_CAD_MAX_DEGREE) return NFF89_CAD_ERR_DEGREE;
    if (trim->cp_count == 0U || (unsigned)degree >= trim->cp_count) return NFF89_CAD_ERR_CONTROL;
    if (trim->knot_count < trim->cp_count + (unsigned)degree + 1U) return NFF89_CAD_ERR_KNOTS;
    if (cad_check_knots(trim->knots, trim->cp_count + (unsigned)degree + 1U) != NFF89_CAD_OK) return NFF89_CAD_ERR_KNOTS;
    n = trim->cp_count - 1U;
    u_min = trim->knots[(unsigned)degree];
    u_max = trim->knots[trim->cp_count];
    if (u_min >= u_max) return NFF89_CAD_ERR_KNOTS;
    u = cad_clamp(u, u_min, u_max);
    if (u == u_max) u = u_max - NFF89_CAD_EPS;
    span = cad_find_span(n, degree, u, trim->knots);
    r = cad_basis_funs(span, u, degree, trim->knots, basis); if (r != NFF89_CAD_OK) return r;
    sx = 0L; sy = 0L; sw = 0L;
    for (k = 0; k <= degree; k++) {
        idx = (unsigned)(span - degree + k);
        if (idx >= trim->cp_count) return NFF89_CAD_ERR_CONTROL;
        cp = &trim->cp[idx];
        wb = nff89_fix_mul(basis[k], cp->w);
        sx += nff89_fix_mul(cp->x, wb);
        sy += nff89_fix_mul(cp->y, wb);
        sw += wb;
    }
    if (cad_abs(sw) <= NFF89_CAD_EPS) return NFF89_CAD_ERR_WEIGHT;
    out_uv->u = nff89_fix_div(sx, sw);
    out_uv->v = nff89_fix_div(sy, sw);
    return NFF89_CAD_OK;
}


int nff89_cad_flatten_trim_curve(const nff89_trim *trim,
                                 unsigned samples,
                                 nff89_cad_uv *scratch_points,
                                 unsigned scratch_points_max,
                                 unsigned *out_count,
                                 nff89_cad_trim_poly *out_loop)
{
    nff89_fix u_min;
    nff89_fix u_max;
    nff89_fix t;
    nff89_fix step_num;
    nff89_fix step_den;
    nff89_cad_uv prev;
    nff89_cad_uv cur;
    nff89_fix area;
    unsigned i;
    unsigned count;
    int degree;
    int r;

    if (trim == (const nff89_trim *)0 || scratch_points == (nff89_cad_uv *)0 ||
        out_count == (unsigned *)0 || out_loop == (nff89_cad_trim_poly *)0) return NFF89_CAD_ERR_BAD_ARG;
    if (trim->order <= 0) return NFF89_CAD_ERR_DEGREE;
    degree = trim->order - 1;
    if (degree > NFF89_CAD_MAX_DEGREE) return NFF89_CAD_ERR_DEGREE;
    if (trim->cp_count == 0U || (unsigned)degree >= trim->cp_count) return NFF89_CAD_ERR_CONTROL;
    if (trim->knot_count < trim->cp_count + (unsigned)degree + 1U) return NFF89_CAD_ERR_KNOTS;
    if (samples < 4U) samples = 4U;
    count = samples + 1U;
    if (count > scratch_points_max || count > NFF89_CAD_MAX_TRIM_POLY) return NFF89_CAD_ERR_CAPACITY;

    u_min = trim->knots[(unsigned)degree];
    u_max = trim->knots[trim->cp_count];
    if (u_min >= u_max) return NFF89_CAD_ERR_KNOTS;

    cad_trim_loop_reset(out_loop);
    step_num = u_max - u_min;
    step_den = nff89_int_to_fix((long)samples);
    area = 0L;
    for (i = 0U; i < count; i++) {
        t = u_min + nff89_fix_mul(step_num, nff89_fix_div(nff89_int_to_fix((long)i), step_den));
        if (i == samples) t = u_max;
        r = nff89_cad_eval_trim_curve(trim, t, &cur);
        if (r != NFF89_CAD_OK) return r;
        scratch_points[i] = cur;
        cad_trim_loop_include(out_loop, &cur);
        if (i > 0U) {
            area += nff89_fix_mul(prev.u, cur.v) - nff89_fix_mul(cur.u, prev.v);
        }
        prev = cur;
    }
    if (count >= 2U) {
        if (cad_abs(scratch_points[0U].u - scratch_points[count - 1U].u) <= 16L &&
            cad_abs(scratch_points[0U].v - scratch_points[count - 1U].v) <= 16L) {
            out_loop->closed = NFF89_CAD_TRUE;
        } else {
            area += nff89_fix_mul(scratch_points[count - 1U].u, scratch_points[0U].v) -
                    nff89_fix_mul(scratch_points[0U].u, scratch_points[count - 1U].v);
            out_loop->closed = NFF89_CAD_TRUE;
        }
    }
    out_loop->signed_area = area;
    *out_count = count;
    return NFF89_CAD_OK;
}

int nff89_cad_build_trim_polylines(const nff89_nurbs *surface,
                                   unsigned samples_per_trim,
                                   nff89_cad_uv *scratch_points,
                                   unsigned scratch_points_max,
                                   unsigned *out_point_count,
                                   nff89_cad_trim_poly *scratch_loops,
                                   unsigned scratch_loops_max,
                                   unsigned *out_loop_count)
{
    unsigned i;
    unsigned total;
    unsigned got;
    nff89_cad_trim_poly loop;
    int r;

    if (surface == (const nff89_nurbs *)0 || scratch_points == (nff89_cad_uv *)0 ||
        out_point_count == (unsigned *)0 || scratch_loops == (nff89_cad_trim_poly *)0 ||
        out_loop_count == (unsigned *)0) return NFF89_CAD_ERR_BAD_ARG;
    if (samples_per_trim < 4U) samples_per_trim = 4U;
    total = 0U;
    *out_loop_count = 0U;
    for (i = 0U; i < surface->trim_count; i++) {
        if (*out_loop_count >= scratch_loops_max || *out_loop_count >= NFF89_CAD_MAX_TRIM_LOOPS) return NFF89_CAD_ERR_CAPACITY;
        if (total >= scratch_points_max) return NFF89_CAD_ERR_CAPACITY;
        r = nff89_cad_flatten_trim_curve(&surface->trims[i], samples_per_trim,
                                         scratch_points + total,
                                         scratch_points_max - total,
                                         &got,
                                         &loop);
        if (r != NFF89_CAD_OK) return r;
        loop.first = total;
        scratch_loops[*out_loop_count] = loop;
        (*out_loop_count)++;
        total += got;
    }
    *out_point_count = total;
    return NFF89_CAD_OK;
}

int nff89_cad_point_in_trim_polylines(nff89_fix u,
                                      nff89_fix v,
                                      const nff89_cad_uv *points,
                                      const nff89_cad_trim_poly *loops,
                                      unsigned loop_count,
                                      unsigned *out_inside)
{
    unsigned li;
    unsigned i;
    unsigned j;
    unsigned inside;
    const nff89_cad_trim_poly *loop;
    const nff89_cad_uv *a;
    const nff89_cad_uv *b;
    nff89_fix den;
    nff89_fix ratio;
    nff89_fix xint;

    if (out_inside == (unsigned *)0) return NFF89_CAD_ERR_BAD_ARG;
    if (loop_count == 0U) {
        *out_inside = NFF89_CAD_TRUE;
        return NFF89_CAD_OK;
    }
    if (points == (const nff89_cad_uv *)0 || loops == (const nff89_cad_trim_poly *)0) return NFF89_CAD_ERR_BAD_ARG;

    inside = NFF89_CAD_FALSE;
    for (li = 0U; li < loop_count; li++) {
        loop = &loops[li];
        if (loop->count < 3U) continue;
        if (u < loop->u_min || u > loop->u_max || v < loop->v_min || v > loop->v_max) {
            continue;
        }
        j = loop->count - 1U;
        for (i = 0U; i < loop->count; i++) {
            a = &points[loop->first + i];
            b = &points[loop->first + j];
            if (cad_point_on_segment(u, v, a, b)) {
                *out_inside = NFF89_CAD_TRUE;
                return NFF89_CAD_OK;
            }
            if (((a->v > v) && (b->v <= v)) || ((b->v > v) && (a->v <= v))) {
                den = b->v - a->v;
                if (cad_abs(den) > NFF89_CAD_EPS) {
                    ratio = nff89_fix_div(v - a->v, den);
                    xint = a->u + nff89_fix_mul(b->u - a->u, ratio);
                    if (u < xint) inside = inside ? NFF89_CAD_FALSE : NFF89_CAD_TRUE;
                }
            }
            j = i;
        }
    }
    *out_inside = inside;
    return NFF89_CAD_OK;
}

int nff89_cad_eval_bezier_patch(const nff89_bezier *patch,
                                nff89_fix u, nff89_fix v,
                                nff89_vec3 *out_pos)
{
    nff89_vec3 rows[NFF89_CAD_MAX_DEGREE + 1];
    nff89_vec3 work[NFF89_CAD_MAX_DEGREE + 1];
    int i;
    int j;
    int r;
    int ud;
    int vd;
    unsigned idx;
    if (patch == (const nff89_bezier *)0 || out_pos == (nff89_vec3 *)0) return NFF89_CAD_ERR_BAD_ARG;
    ud = patch->u_degree;
    vd = patch->v_degree;
    if (ud < 0 || vd < 0 || ud > NFF89_CAD_MAX_DEGREE || vd > NFF89_CAD_MAX_DEGREE) return NFF89_CAD_ERR_DEGREE;
    if (patch->cp_count < (unsigned)((ud + 1) * (vd + 1))) return NFF89_CAD_ERR_CONTROL;
    u = cad_clamp(u, 0L, NFF89_FIX_ONE);
    v = cad_clamp(v, 0L, NFF89_FIX_ONE);
    for (j = 0; j <= vd; j++) {
        for (i = 0; i <= ud; i++) {
            idx = (unsigned)(j * (ud + 1) + i);
            work[i] = patch->cp[idx];
        }
        for (r = 1; r <= ud; r++) {
            for (i = 0; i <= ud - r; i++) {
                work[i].x = cad_lerp(work[i].x, work[i + 1].x, u);
                work[i].y = cad_lerp(work[i].y, work[i + 1].y, u);
                work[i].z = cad_lerp(work[i].z, work[i + 1].z, u);
            }
        }
        rows[j] = work[0];
    }
    for (r = 1; r <= vd; r++) {
        for (j = 0; j <= vd - r; j++) {
            rows[j].x = cad_lerp(rows[j].x, rows[j + 1].x, v);
            rows[j].y = cad_lerp(rows[j].y, rows[j + 1].y, v);
            rows[j].z = cad_lerp(rows[j].z, rows[j + 1].z, v);
        }
    }
    *out_pos = rows[0];
    return NFF89_CAD_OK;
}

int nff89_cad_eval_bezier_vertex(const nff89_bezier *patch,
                                 nff89_fix u, nff89_fix v,
                                 nff89_cad_vertex *out_vertex)
{
    nff89_vec3 pu0;
    nff89_vec3 pu1;
    nff89_vec3 pv0;
    nff89_vec3 pv1;
    nff89_vec3 tu;
    nff89_vec3 tv;
    nff89_fix du;
    nff89_fix dv;
    int r;
    if (out_vertex == (nff89_cad_vertex *)0) return NFF89_CAD_ERR_BAD_ARG;
    r = nff89_cad_eval_bezier_patch(patch, u, v, &out_vertex->pos);
    if (r != NFF89_CAD_OK) return r;
    out_vertex->uv.u = u;
    out_vertex->uv.v = v;
    du = NFF89_FIX_ONE / 256L;
    dv = NFF89_FIX_ONE / 256L;
    nff89_cad_eval_bezier_patch(patch, cad_clamp(u - du, 0L, NFF89_FIX_ONE), v, &pu0);
    nff89_cad_eval_bezier_patch(patch, cad_clamp(u + du, 0L, NFF89_FIX_ONE), v, &pu1);
    nff89_cad_eval_bezier_patch(patch, u, cad_clamp(v - dv, 0L, NFF89_FIX_ONE), &pv0);
    nff89_cad_eval_bezier_patch(patch, u, cad_clamp(v + dv, 0L, NFF89_FIX_ONE), &pv1);
    tu = cad_sub3(pu1, pu0);
    tv = cad_sub3(pv1, pv0);
    out_vertex->normal = cad_cross3(tu, tv);
    cad_normalize3(&out_vertex->normal);
    out_vertex->has_normal = NFF89_CAD_TRUE;
    return NFF89_CAD_OK;
}

static int cad_grid_index(unsigned x, unsigned y, unsigned width, unsigned *out)
{
    if (out == (unsigned *)0) return NFF89_CAD_ERR_BAD_ARG;
    *out = y * width + x;
    return NFF89_CAD_OK;
}

int nff89_cad_tessellate_nurbs(const nff89_nurbs *s,
                               unsigned u_steps,
                               unsigned v_steps,
                               nff89_cad_vertex *scratch_grid,
                               unsigned scratch_grid_max,
                               nff89_cad_tri_cb cb,
                               void *user)
{
    nff89_fix u_min;
    nff89_fix u_max;
    nff89_fix v_min;
    nff89_fix v_max;
    unsigned width;
    unsigned height;
    unsigned need;
    unsigned i;
    unsigned j;
    unsigned ia;
    unsigned ib;
    unsigned ic;
    unsigned id;
    nff89_fix u;
    nff89_fix v;
    int r;
    if (scratch_grid == (nff89_cad_vertex *)0 || cb == (nff89_cad_tri_cb)0) return NFF89_CAD_ERR_BAD_ARG;
    if (u_steps == 0U || v_steps == 0U) return NFF89_CAD_ERR_RANGE;
    width = u_steps + 1U;
    height = v_steps + 1U;
    need = width * height;
    if (need > scratch_grid_max || need > NFF89_CAD_MAX_GRID) return NFF89_CAD_ERR_CAPACITY;
    r = nff89_cad_nurbs_domain(s, &u_min, &u_max, &v_min, &v_max);
    if (r != NFF89_CAD_OK) return r;
    for (j = 0U; j < height; j++) {
        v = v_min + nff89_fix_mul(v_max - v_min, nff89_fix_div(nff89_int_to_fix((long)j), nff89_int_to_fix((long)v_steps)));
        for (i = 0U; i < width; i++) {
            cad_grid_index(i, j, width, &ia);
            u = u_min + nff89_fix_mul(u_max - u_min, nff89_fix_div(nff89_int_to_fix((long)i), nff89_int_to_fix((long)u_steps)));
            r = nff89_cad_eval_nurbs_surface_vertex(s, u, v, &scratch_grid[ia]);
            if (r != NFF89_CAD_OK) return r;
        }
    }
    for (j = 0U; j < v_steps; j++) {
        for (i = 0U; i < u_steps; i++) {
            cad_grid_index(i, j, width, &ia);
            cad_grid_index(i + 1U, j, width, &ib);
            cad_grid_index(i + 1U, j + 1U, width, &ic);
            cad_grid_index(i, j + 1U, width, &id);
            r = cb(user, &scratch_grid[ia], &scratch_grid[ib], &scratch_grid[ic], s);
            if (r != 0) return NFF89_CAD_ERR_CALLBACK;
            r = cb(user, &scratch_grid[ia], &scratch_grid[ic], &scratch_grid[id], s);
            if (r != 0) return NFF89_CAD_ERR_CALLBACK;
        }
    }
    return NFF89_CAD_OK;
}


int nff89_cad_tessellate_nurbs_trimmed(const nff89_nurbs *s,
                                       unsigned u_steps,
                                       unsigned v_steps,
                                       unsigned cell_subdiv,
                                       unsigned trim_samples_per_curve,
                                       nff89_cad_vertex *scratch_grid,
                                       unsigned scratch_grid_max,
                                       nff89_cad_uv *scratch_trim_points,
                                       unsigned scratch_trim_points_max,
                                       nff89_cad_trim_poly *scratch_trim_loops,
                                       unsigned scratch_trim_loops_max,
                                       nff89_cad_tri_cb cb,
                                       void *user)
{
    nff89_fix u_min;
    nff89_fix u_max;
    nff89_fix v_min;
    nff89_fix v_max;
    unsigned trim_point_count;
    unsigned trim_loop_count;
    unsigned ru_steps;
    unsigned rv_steps;
    unsigned width;
    unsigned height;
    unsigned need;
    unsigned i;
    unsigned j;
    unsigned ia;
    unsigned ib;
    unsigned ic;
    unsigned id;
    unsigned inside;
    nff89_fix u;
    nff89_fix v;
    nff89_fix cu;
    nff89_fix cv;
    int r;

    if (scratch_grid == (nff89_cad_vertex *)0 || cb == (nff89_cad_tri_cb)0) return NFF89_CAD_ERR_BAD_ARG;
    if (u_steps == 0U || v_steps == 0U) return NFF89_CAD_ERR_RANGE;
    if (s == (const nff89_nurbs *)0) return NFF89_CAD_ERR_BAD_ARG;
    if (s->trim_count == 0U) return nff89_cad_tessellate_nurbs(s, u_steps, v_steps, scratch_grid, scratch_grid_max, cb, user);
    if (scratch_trim_points == (nff89_cad_uv *)0 || scratch_trim_loops == (nff89_cad_trim_poly *)0) return NFF89_CAD_ERR_BAD_ARG;
    if (cell_subdiv == 0U) cell_subdiv = 1U;
    if (trim_samples_per_curve == 0U) trim_samples_per_curve = 16U;
    if (u_steps > 65535U / cell_subdiv || v_steps > 65535U / cell_subdiv) return NFF89_CAD_ERR_RANGE;
    ru_steps = u_steps * cell_subdiv;
    rv_steps = v_steps * cell_subdiv;
    width = ru_steps + 1U;
    height = rv_steps + 1U;
    if (height != 0U && width > 65535U / height) return NFF89_CAD_ERR_CAPACITY;
    need = width * height;
    if (need > scratch_grid_max || need > NFF89_CAD_MAX_GRID) return NFF89_CAD_ERR_CAPACITY;

    r = nff89_cad_build_trim_polylines(s, trim_samples_per_curve,
                                       scratch_trim_points, scratch_trim_points_max,
                                       &trim_point_count,
                                       scratch_trim_loops, scratch_trim_loops_max,
                                       &trim_loop_count);
    if (r != NFF89_CAD_OK) return r;
    (void)trim_point_count;

    r = nff89_cad_nurbs_domain(s, &u_min, &u_max, &v_min, &v_max);
    if (r != NFF89_CAD_OK) return r;

    for (j = 0U; j < height; j++) {
        v = v_min + nff89_fix_mul(v_max - v_min, nff89_fix_div(nff89_int_to_fix((long)j), nff89_int_to_fix((long)rv_steps)));
        for (i = 0U; i < width; i++) {
            cad_grid_index(i, j, width, &ia);
            u = u_min + nff89_fix_mul(u_max - u_min, nff89_fix_div(nff89_int_to_fix((long)i), nff89_int_to_fix((long)ru_steps)));
            r = nff89_cad_eval_nurbs_surface_vertex(s, u, v, &scratch_grid[ia]);
            if (r != NFF89_CAD_OK) return r;
        }
    }

    for (j = 0U; j < rv_steps; j++) {
        for (i = 0U; i < ru_steps; i++) {
            cad_grid_index(i, j, width, &ia);
            cad_grid_index(i + 1U, j, width, &ib);
            cad_grid_index(i + 1U, j + 1U, width, &ic);
            cad_grid_index(i, j + 1U, width, &id);

            cu = (scratch_grid[ia].uv.u + scratch_grid[ib].uv.u + scratch_grid[ic].uv.u) / 3L;
            cv = (scratch_grid[ia].uv.v + scratch_grid[ib].uv.v + scratch_grid[ic].uv.v) / 3L;
            r = nff89_cad_point_in_trim_polylines(cu, cv, scratch_trim_points, scratch_trim_loops, trim_loop_count, &inside);
            if (r != NFF89_CAD_OK) return r;
            if (inside && cad_triangle_uv_area2(&scratch_grid[ia], &scratch_grid[ib], &scratch_grid[ic]) > NFF89_CAD_EPS) {
                r = cb(user, &scratch_grid[ia], &scratch_grid[ib], &scratch_grid[ic], s);
                if (r != 0) return NFF89_CAD_ERR_CALLBACK;
            }

            cu = (scratch_grid[ia].uv.u + scratch_grid[ic].uv.u + scratch_grid[id].uv.u) / 3L;
            cv = (scratch_grid[ia].uv.v + scratch_grid[ic].uv.v + scratch_grid[id].uv.v) / 3L;
            r = nff89_cad_point_in_trim_polylines(cu, cv, scratch_trim_points, scratch_trim_loops, trim_loop_count, &inside);
            if (r != NFF89_CAD_OK) return r;
            if (inside && cad_triangle_uv_area2(&scratch_grid[ia], &scratch_grid[ic], &scratch_grid[id]) > NFF89_CAD_EPS) {
                r = cb(user, &scratch_grid[ia], &scratch_grid[ic], &scratch_grid[id], s);
                if (r != 0) return NFF89_CAD_ERR_CALLBACK;
            }
        }
    }
    return NFF89_CAD_OK;
}

int nff89_cad_tessellate_bezier(const nff89_bezier *patch,
                                unsigned u_steps,
                                unsigned v_steps,
                                nff89_cad_vertex *scratch_grid,
                                unsigned scratch_grid_max,
                                nff89_cad_bezier_tri_cb cb,
                                void *user)
{
    unsigned width;
    unsigned height;
    unsigned need;
    unsigned i;
    unsigned j;
    unsigned ia;
    unsigned ib;
    unsigned ic;
    unsigned id;
    nff89_fix u;
    nff89_fix v;
    int r;
    if (patch == (const nff89_bezier *)0 || scratch_grid == (nff89_cad_vertex *)0 || cb == (nff89_cad_bezier_tri_cb)0) return NFF89_CAD_ERR_BAD_ARG;
    if (u_steps == 0U || v_steps == 0U) return NFF89_CAD_ERR_RANGE;
    width = u_steps + 1U;
    height = v_steps + 1U;
    need = width * height;
    if (need > scratch_grid_max || need > NFF89_CAD_MAX_GRID) return NFF89_CAD_ERR_CAPACITY;
    for (j = 0U; j < height; j++) {
        v = nff89_fix_div(nff89_int_to_fix((long)j), nff89_int_to_fix((long)v_steps));
        for (i = 0U; i < width; i++) {
            cad_grid_index(i, j, width, &ia);
            u = nff89_fix_div(nff89_int_to_fix((long)i), nff89_int_to_fix((long)u_steps));
            r = nff89_cad_eval_bezier_vertex(patch, u, v, &scratch_grid[ia]);
            if (r != NFF89_CAD_OK) return r;
        }
    }
    for (j = 0U; j < v_steps; j++) {
        for (i = 0U; i < u_steps; i++) {
            cad_grid_index(i, j, width, &ia);
            cad_grid_index(i + 1U, j, width, &ib);
            cad_grid_index(i + 1U, j + 1U, width, &ic);
            cad_grid_index(i, j + 1U, width, &id);
            r = cb(user, &scratch_grid[ia], &scratch_grid[ib], &scratch_grid[ic], patch);
            if (r != 0) return NFF89_CAD_ERR_CALLBACK;
            r = cb(user, &scratch_grid[ia], &scratch_grid[ic], &scratch_grid[id], patch);
            if (r != 0) return NFF89_CAD_ERR_CALLBACK;
        }
    }
    return NFF89_CAD_OK;
}

int nff89_cad_tessellate_scene_nurbs(const nff89_scene *scene,
                                     unsigned u_steps,
                                     unsigned v_steps,
                                     nff89_cad_vertex *scratch_grid,
                                     unsigned scratch_grid_max,
                                     nff89_cad_tri_cb cb,
                                     void *user)
{
    unsigned i;
    int r;
    if (scene == (const nff89_scene *)0) return NFF89_CAD_ERR_BAD_ARG;
    for (i = 0U; i < scene->nurbs_count; i++) {
        r = nff89_cad_tessellate_nurbs(&scene->nurbs[i], u_steps, v_steps, scratch_grid, scratch_grid_max, cb, user);
        if (r != NFF89_CAD_OK) return r;
    }
    return NFF89_CAD_OK;
}


int nff89_cad_tessellate_scene_nurbs_trimmed(const nff89_scene *scene,
                                             unsigned u_steps,
                                             unsigned v_steps,
                                             unsigned cell_subdiv,
                                             unsigned trim_samples_per_curve,
                                             nff89_cad_vertex *scratch_grid,
                                             unsigned scratch_grid_max,
                                             nff89_cad_uv *scratch_trim_points,
                                             unsigned scratch_trim_points_max,
                                             nff89_cad_trim_poly *scratch_trim_loops,
                                             unsigned scratch_trim_loops_max,
                                             nff89_cad_tri_cb cb,
                                             void *user)
{
    unsigned i;
    int r;
    if (scene == (const nff89_scene *)0) return NFF89_CAD_ERR_BAD_ARG;
    for (i = 0U; i < scene->nurbs_count; i++) {
        r = nff89_cad_tessellate_nurbs_trimmed(&scene->nurbs[i], u_steps, v_steps,
                                               cell_subdiv, trim_samples_per_curve,
                                               scratch_grid, scratch_grid_max,
                                               scratch_trim_points, scratch_trim_points_max,
                                               scratch_trim_loops, scratch_trim_loops_max,
                                               cb, user);
        if (r != NFF89_CAD_OK) return r;
    }
    return NFF89_CAD_OK;
}

const char *nff89_cad_error_name(int code)
{
    switch (code) {
    case NFF89_CAD_OK: return "NFF89_CAD_OK";
    case NFF89_CAD_ERR_BAD_ARG: return "NFF89_CAD_ERR_BAD_ARG";
    case NFF89_CAD_ERR_DEGREE: return "NFF89_CAD_ERR_DEGREE";
    case NFF89_CAD_ERR_KNOTS: return "NFF89_CAD_ERR_KNOTS";
    case NFF89_CAD_ERR_CONTROL: return "NFF89_CAD_ERR_CONTROL";
    case NFF89_CAD_ERR_RANGE: return "NFF89_CAD_ERR_RANGE";
    case NFF89_CAD_ERR_WEIGHT: return "NFF89_CAD_ERR_WEIGHT";
    case NFF89_CAD_ERR_CAPACITY: return "NFF89_CAD_ERR_CAPACITY";
    case NFF89_CAD_ERR_CALLBACK: return "NFF89_CAD_ERR_CALLBACK";
    case NFF89_CAD_ERR_TRIM: return "NFF89_CAD_ERR_TRIM";
    default: return "NFF89_CAD_ERR_UNKNOWN";
    }
}
