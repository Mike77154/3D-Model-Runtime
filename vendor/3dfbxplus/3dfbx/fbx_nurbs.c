#include "fbx_nurbs.h"
#include "fbx_alloc.h"

#include <string.h>

static fbx_u32 fbx_nurbs_point_stride_curve(const FbxCurveGeometry *curve)
{
    fbx_u32 n;
    if (!curve || curve->point_count == 0u) return 3u;
    n = curve->point_count;
    if (curve->dimension >= 4) return 4u;
    if ((n % 4u) == 0u && (n / 4u) >= 2u) return 4u;
    return 3u;
}

static fbx_u32 fbx_nurbs_point_stride_surface(const FbxSurfaceGeometry *surface)
{
    if (!surface || surface->point_count == 0u) return 4u;
    return 4u;
}

static fbx_u32 fbx_nurbs_curve_cp_count(const FbxCurveGeometry *curve, fbx_u32 stride)
{
    if (!curve || stride == 0u) return 0u;
    return curve->point_count / stride;
}

static fbx_real fbx_nurbs_lerp(fbx_real a, fbx_real b, fbx_real t)
{
    return a + fbx_real_mul(b - a, t);
}

static void fbx_nurbs_read_curve_cp(const FbxCurveGeometry *curve, fbx_u32 cp, fbx_u32 stride, fbx_real out[4])
{
    fbx_u32 base;
    base = cp * stride;
    out[0] = curve->points[base + 0u];
    out[1] = curve->points[base + 1u];
    out[2] = curve->points[base + 2u];
    out[3] = (stride >= 4u) ? curve->points[base + 3u] : FBX_REAL_ONE;
    if (out[3] == FBX_REAL_ZERO) out[3] = FBX_REAL_ONE;
}

static fbx_real fbx_nurbs_basis_rec(fbx_u32 i, fbx_u32 p, fbx_real t, const fbx_real *knots, fbx_u32 knot_count)
{
    fbx_real a;
    fbx_real b;
    fbx_real den;
    if (!knots || i + p + 1u >= knot_count) return FBX_REAL_ZERO;
    if (p == 0u) {
        if ((t >= knots[i] && t < knots[i + 1u]) ||
            (i + 1u == knot_count - 1u && t == knots[i + 1u])) {
            return FBX_REAL_ONE;
        }
        return FBX_REAL_ZERO;
    }
    a = FBX_REAL_ZERO;
    den = knots[i + p] - knots[i];
    if (den != FBX_REAL_ZERO) {
        a = fbx_real_mul(fbx_real_div(t - knots[i], den), fbx_nurbs_basis_rec(i, p - 1u, t, knots, knot_count));
    }
    b = FBX_REAL_ZERO;
    den = knots[i + p + 1u] - knots[i + 1u];
    if (den != FBX_REAL_ZERO) {
        b = fbx_real_mul(fbx_real_div(knots[i + p + 1u] - t, den), fbx_nurbs_basis_rec(i + 1u, p - 1u, t, knots, knot_count));
    }
    return a + b;
}

static int fbx_nurbs_eval_curve(const FbxCurveGeometry *curve, fbx_real t, fbx_real out[3])
{
    fbx_u32 stride;
    fbx_u32 cp_count;
    fbx_u32 degree;
    fbx_u32 i;
    fbx_real sum[3];
    fbx_real sum_w;
    if (!curve || !curve->points || !curve->knots) return 0;
    stride = fbx_nurbs_point_stride_curve(curve);
    cp_count = fbx_nurbs_curve_cp_count(curve, stride);
    if (cp_count == 0u || curve->order <= 0) return 0;
    degree = (fbx_u32)(curve->order - 1);
    if (curve->knot_count < cp_count + degree + 1u) return 0;
    sum[0] = FBX_REAL_ZERO;
    sum[1] = FBX_REAL_ZERO;
    sum[2] = FBX_REAL_ZERO;
    sum_w = FBX_REAL_ZERO;
    for (i = 0u; i < cp_count; ++i) {
        fbx_real cp[4];
        fbx_real basis;
        fbx_real wb;
        fbx_nurbs_read_curve_cp(curve, i, stride, cp);
        basis = fbx_nurbs_basis_rec(i, degree, t, curve->knots, curve->knot_count);
        if (basis == FBX_REAL_ZERO) continue;
        wb = fbx_real_mul(basis, cp[3]);
        sum[0] += fbx_real_mul(cp[0], wb);
        sum[1] += fbx_real_mul(cp[1], wb);
        sum[2] += fbx_real_mul(cp[2], wb);
        sum_w += wb;
    }
    if (sum_w == FBX_REAL_ZERO) return 0;
    out[0] = fbx_real_div(sum[0], sum_w);
    out[1] = fbx_real_div(sum[1], sum_w);
    out[2] = fbx_real_div(sum[2], sum_w);
    return 1;
}

static int fbx_nurbs_curve_polyline_fallback(const FbxCurveGeometry *curve, fbx_real **out_positions, fbx_u32 *out_vertex_count)
{
    fbx_u32 stride;
    fbx_u32 cp_count;
    fbx_u32 i;
    fbx_real *dst;
    if (!curve || !curve->points || !out_positions || !out_vertex_count) return 0;
    stride = fbx_nurbs_point_stride_curve(curve);
    cp_count = fbx_nurbs_curve_cp_count(curve, stride);
    if (cp_count == 0u) return 0;
    dst = (fbx_real*)fbx_alloc_zero_n((size_t)cp_count * 3u, sizeof(fbx_real));
    if (!dst) return 0;
    for (i = 0u; i < cp_count; ++i) {
        fbx_real cp[4];
        fbx_nurbs_read_curve_cp(curve, i, stride, cp);
        dst[i * 3u + 0u] = cp[0];
        dst[i * 3u + 1u] = cp[1];
        dst[i * 3u + 2u] = cp[2];
    }
    *out_positions = dst;
    *out_vertex_count = cp_count;
    return 1;
}

int fbx_nurbs_curve_tessellate(const FbxCurveGeometry *curve,
                               int segments,
                               fbx_real **out_positions,
                               fbx_u32 *out_vertex_count)
{
    fbx_u32 sample_count;
    fbx_u32 i;
    fbx_u32 degree;
    fbx_real t0;
    fbx_real t1;
    fbx_real *dst;
    if (out_positions) *out_positions = NULL;
    if (out_vertex_count) *out_vertex_count = 0u;
    if (!curve || !out_positions || !out_vertex_count || !curve->points) return 0;
    if (segments < 1) segments = 16;
    if (!curve->knots || curve->order <= 1) return fbx_nurbs_curve_polyline_fallback(curve, out_positions, out_vertex_count);
    degree = (fbx_u32)(curve->order - 1);
    if (curve->knot_count <= degree + 1u) return fbx_nurbs_curve_polyline_fallback(curve, out_positions, out_vertex_count);
    t0 = curve->knots[degree];
    t1 = curve->knots[curve->knot_count - degree - 1u];
    if (t1 <= t0) return fbx_nurbs_curve_polyline_fallback(curve, out_positions, out_vertex_count);
    sample_count = (fbx_u32)segments + 1u;
    dst = (fbx_real*)fbx_alloc_zero_n((size_t)sample_count * 3u, sizeof(fbx_real));
    if (!dst) return 0;
    for (i = 0u; i < sample_count; ++i) {
        fbx_real u;
        fbx_real p[3];
        u = fbx_real_div(fbx_real_from_int((int)i), fbx_real_from_int(segments));
        u = fbx_nurbs_lerp(t0, t1, u);
        if (i + 1u == sample_count) {
            fbx_u32 stride;
            fbx_u32 cp_count;
            fbx_real cp[4];
            stride = fbx_nurbs_point_stride_curve(curve);
            cp_count = fbx_nurbs_curve_cp_count(curve, stride);
            if (cp_count == 0u) {
                fbx_release(dst);
                return 0;
            }
            fbx_nurbs_read_curve_cp(curve, cp_count - 1u, stride, cp);
            p[0] = cp[0];
            p[1] = cp[1];
            p[2] = cp[2];
        } else if (!fbx_nurbs_eval_curve(curve, u, p)) {
            fbx_release(dst);
            return fbx_nurbs_curve_polyline_fallback(curve, out_positions, out_vertex_count);
        }
        dst[i * 3u + 0u] = p[0];
        dst[i * 3u + 1u] = p[1];
        dst[i * 3u + 2u] = p[2];
    }
    *out_positions = dst;
    *out_vertex_count = sample_count;
    return 1;
}

static void fbx_nurbs_surface_read_cp(const FbxSurfaceGeometry *surface, fbx_u32 cp, fbx_real out[4])
{
    fbx_u32 base;
    fbx_u32 stride;
    stride = fbx_nurbs_point_stride_surface(surface);
    base = cp * stride;
    out[0] = surface->points[base + 0u];
    out[1] = surface->points[base + 1u];
    out[2] = surface->points[base + 2u];
    out[3] = surface->points[base + 3u];
    if (out[3] == FBX_REAL_ZERO) out[3] = FBX_REAL_ONE;
}

static void fbx_nurbs_surface_sample_grid(const FbxSurfaceGeometry *surface, fbx_real fu, fbx_real fv, fbx_real out[3])
{
    fbx_u32 du;
    fbx_u32 dv;
    fbx_u32 iu;
    fbx_u32 iv;
    fbx_u32 iu1;
    fbx_u32 iv1;
    fbx_real lu;
    fbx_real lv;
    fbx_real p00[4];
    fbx_real p10[4];
    fbx_real p01[4];
    fbx_real p11[4];
    fbx_real a[3];
    fbx_real b[3];
    du = surface->dimension_u > 0 ? (fbx_u32)surface->dimension_u : 1u;
    dv = surface->dimension_v > 0 ? (fbx_u32)surface->dimension_v : 1u;
    if (du < 2u || dv < 2u || du * dv > surface->point_count / 4u) {
        out[0] = surface->points[0];
        out[1] = surface->points[1];
        out[2] = surface->points[2];
        return;
    }
    lu = fbx_real_mul(fu, fbx_real_from_int((int)(du - 1u)));
    lv = fbx_real_mul(fv, fbx_real_from_int((int)(dv - 1u)));
    iu = (fbx_u32)(lu >> FBX_REAL_SHIFT);
    iv = (fbx_u32)(lv >> FBX_REAL_SHIFT);
    if (iu >= du - 1u) iu = du - 2u;
    if (iv >= dv - 1u) iv = dv - 2u;
    iu1 = iu + 1u;
    iv1 = iv + 1u;
    lu -= fbx_real_from_int((int)iu);
    lv -= fbx_real_from_int((int)iv);
    fbx_nurbs_surface_read_cp(surface, iv * du + iu, p00);
    fbx_nurbs_surface_read_cp(surface, iv * du + iu1, p10);
    fbx_nurbs_surface_read_cp(surface, iv1 * du + iu, p01);
    fbx_nurbs_surface_read_cp(surface, iv1 * du + iu1, p11);
    a[0] = fbx_nurbs_lerp(p00[0], p10[0], lu);
    a[1] = fbx_nurbs_lerp(p00[1], p10[1], lu);
    a[2] = fbx_nurbs_lerp(p00[2], p10[2], lu);
    b[0] = fbx_nurbs_lerp(p01[0], p11[0], lu);
    b[1] = fbx_nurbs_lerp(p01[1], p11[1], lu);
    b[2] = fbx_nurbs_lerp(p01[2], p11[2], lu);
    out[0] = fbx_nurbs_lerp(a[0], b[0], lv);
    out[1] = fbx_nurbs_lerp(a[1], b[1], lv);
    out[2] = fbx_nurbs_lerp(a[2], b[2], lv);
}

int fbx_nurbs_surface_tessellate(const FbxSurfaceGeometry *surface,
                                 int segments_u,
                                 int segments_v,
                                 fbx_real **out_positions,
                                 fbx_u32 **out_indices,
                                 fbx_u32 *out_vertex_count,
                                 fbx_u32 *out_index_count)
{
    fbx_u32 su;
    fbx_u32 sv;
    fbx_u32 vc;
    fbx_u32 ic;
    fbx_u32 u;
    fbx_u32 v;
    fbx_real *pos;
    fbx_u32 *idx;
    if (out_positions) *out_positions = NULL;
    if (out_indices) *out_indices = NULL;
    if (out_vertex_count) *out_vertex_count = 0u;
    if (out_index_count) *out_index_count = 0u;
    if (!surface || !surface->points || !out_positions || !out_indices || !out_vertex_count || !out_index_count) return 0;
    if (segments_u < 1) segments_u = 8;
    if (segments_v < 1) segments_v = 8;
    su = (fbx_u32)segments_u;
    sv = (fbx_u32)segments_v;
    vc = (su + 1u) * (sv + 1u);
    ic = su * sv * 6u;
    pos = (fbx_real*)fbx_alloc_zero_n((size_t)vc * 3u, sizeof(fbx_real));
    if (!pos) return 0;
    idx = (fbx_u32*)fbx_alloc_zero_n((size_t)ic, sizeof(fbx_u32));
    if (!idx) { fbx_release(pos); return 0; }
    for (v = 0u; v <= sv; ++v) {
        for (u = 0u; u <= su; ++u) {
            fbx_real fu;
            fbx_real fv;
            fbx_real p[3];
            fbx_u32 vi;
            fu = fbx_real_div(fbx_real_from_int((int)u), fbx_real_from_int((int)su));
            fv = fbx_real_div(fbx_real_from_int((int)v), fbx_real_from_int((int)sv));
            fbx_nurbs_surface_sample_grid(surface, fu, fv, p);
            vi = v * (su + 1u) + u;
            pos[vi * 3u + 0u] = p[0];
            pos[vi * 3u + 1u] = p[1];
            pos[vi * 3u + 2u] = p[2];
        }
    }
    {
        fbx_u32 k;
        k = 0u;
        for (v = 0u; v < sv; ++v) {
            for (u = 0u; u < su; ++u) {
                fbx_u32 a;
                fbx_u32 b;
                fbx_u32 c;
                fbx_u32 d;
                a = v * (su + 1u) + u;
                b = a + 1u;
                c = a + (su + 1u);
                d = c + 1u;
                idx[k++] = a; idx[k++] = b; idx[k++] = d;
                idx[k++] = a; idx[k++] = d; idx[k++] = c;
            }
        }
    }
    *out_positions = pos;
    *out_indices = idx;
    *out_vertex_count = vc;
    *out_index_count = ic;
    return 1;
}
