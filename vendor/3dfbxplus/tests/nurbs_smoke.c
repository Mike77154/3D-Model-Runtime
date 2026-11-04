#include "3dfbx.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    FbxCurveGeometry curve;
    FbxSurfaceGeometry surface;
    fbx_real cpts[16];
    fbx_real knots[8];
    fbx_real spts[16];
    fbx_real *curve_pos;
    fbx_real *surface_pos;
    fbx_u32 *surface_idx;
    fbx_u32 curve_count;
    fbx_u32 surface_vc;
    fbx_u32 surface_ic;
    int ok;
    memset(&curve, 0, sizeof(curve));
    memset(&surface, 0, sizeof(surface));
    cpts[0] = fbx_real_from_int(0); cpts[1] = fbx_real_from_int(0); cpts[2] = fbx_real_from_int(0); cpts[3] = FBX_REAL_ONE;
    cpts[4] = fbx_real_from_int(1); cpts[5] = fbx_real_from_int(2); cpts[6] = fbx_real_from_int(0); cpts[7] = FBX_REAL_ONE;
    cpts[8] = fbx_real_from_int(2); cpts[9] = fbx_real_from_int(2); cpts[10] = fbx_real_from_int(0); cpts[11] = FBX_REAL_ONE;
    cpts[12] = fbx_real_from_int(3); cpts[13] = fbx_real_from_int(0); cpts[14] = fbx_real_from_int(0); cpts[15] = FBX_REAL_ONE;
    knots[0] = FBX_REAL_ZERO; knots[1] = FBX_REAL_ZERO; knots[2] = FBX_REAL_ZERO; knots[3] = FBX_REAL_ZERO;
    knots[4] = FBX_REAL_ONE; knots[5] = FBX_REAL_ONE; knots[6] = FBX_REAL_ONE; knots[7] = FBX_REAL_ONE;
    curve.valid = 1;
    curve.kind = FBX_MODEL_TYPE_NURBS_CURVE;
    curve.order = 4;
    curve.dimension = 4;
    curve.point_count = 16u;
    curve.points = cpts;
    curve.knot_count = 8u;
    curve.knots = knots;
    curve_pos = NULL;
    curve_count = 0u;
    ok = fbx_nurbs_curve_tessellate(&curve, 8, &curve_pos, &curve_count);
    if (!ok || curve_count != 9u || !curve_pos) return 1;

    spts[0] = fbx_real_from_int(0); spts[1] = fbx_real_from_int(0); spts[2] = FBX_REAL_ZERO; spts[3] = FBX_REAL_ONE;
    spts[4] = fbx_real_from_int(1); spts[5] = fbx_real_from_int(0); spts[6] = FBX_REAL_ZERO; spts[7] = FBX_REAL_ONE;
    spts[8] = fbx_real_from_int(0); spts[9] = fbx_real_from_int(1); spts[10] = FBX_REAL_ZERO; spts[11] = FBX_REAL_ONE;
    spts[12] = fbx_real_from_int(1); spts[13] = fbx_real_from_int(1); spts[14] = fbx_real_from_int(1); spts[15] = FBX_REAL_ONE;
    surface.valid = 1;
    surface.dimension_u = 2;
    surface.dimension_v = 2;
    surface.point_count = 16u;
    surface.points = spts;
    surface_pos = NULL;
    surface_idx = NULL;
    surface_vc = 0u;
    surface_ic = 0u;
    ok = fbx_nurbs_surface_tessellate(&surface, 2, 2, &surface_pos, &surface_idx, &surface_vc, &surface_ic);
    if (!ok || surface_vc != 9u || surface_ic != 24u || !surface_pos || !surface_idx) return 2;
    fbx_release(curve_pos);
    fbx_release(surface_pos);
    fbx_release(surface_idx);
    printf("nurbs smoke OK: curve_vertices=%u surface_vertices=%u indices=%u\n", (unsigned)curve_count, (unsigned)surface_vc, (unsigned)surface_ic);
    return 0;
}
