#include "dff3d_native_common.h"

static int dff3d_mobile_platform(dff_u32 platform)
{
    if (platform == DFF3D_NATIVE_OPENGL || platform == DFF3D_NATIVE_PSP || platform == DFF3D_NATIVE_SOFT || platform == DFF3D_NATIVE_MAC) {
        return 1;
    }
    return 0;
}

int dff3d_probe_native_mobile(const dff_u8 *raw,
                              dff_u32 raw_size,
                              Dff3dModel *model,
                              Dff3dGeometry *geom,
                              Dff3dNativeInfo *info,
                              Dff3dDiag *diag)
{
    dff_u32 platform;

    if (raw == DFF3D_NULL || raw_size < 12U || model == DFF3D_NULL || geom == DFF3D_NULL) {
        return 0;
    }
    platform = dff3d_native_raw_u32(raw);
    if (!dff3d_mobile_platform(platform)) {
        return 0;
    }
    if (diag != DFF3D_NULL) {
        diag->native_mobile_probe++;
    }
    if (dff3d_native_scan_f32_stream(raw, raw_size, 4U, DFF3D_NATIVE_SCAN_MAX, model, geom, info, diag, DFF3D_NATIVE_STATUS_MOBILE_VERTEX)) {
        return 1;
    }
    if (dff3d_native_scan_i16_stream(raw, raw_size, 4U, DFF3D_NATIVE_SCAN_MAX, model, geom, info, diag, DFF3D_NATIVE_STATUS_MOBILE_VERTEX)) {
        return 1;
    }
    return 0;
}
