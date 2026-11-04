#include "dff3d_native_common.h"

int dff3d_probe_native_xbox(const dff_u8 *raw,
                            dff_u32 raw_size,
                            Dff3dModel *model,
                            Dff3dGeometry *geom,
                            Dff3dNativeInfo *info,
                            Dff3dDiag *diag)
{
    dff_u32 platform;

    if (raw == DFF3D_NULL || raw_size < 16U || model == DFF3D_NULL || geom == DFF3D_NULL) {
        return 0;
    }
    platform = dff3d_native_raw_u32(raw);
    if (platform != DFF3D_NATIVE_XBOX) {
        return 0;
    }
    if (diag != DFF3D_NULL) {
        diag->native_xbox_probe++;
    }
    if (dff3d_probe_native_d3d_decl(raw, raw_size, model, geom, info, diag)) {
        if (info != DFF3D_NULL) {
            info->uninstance_status |= DFF3D_NATIVE_STATUS_XBOX_VERTEX;
        }
        return 1;
    }
    if (dff3d_native_scan_f32_stream(raw, raw_size, 4U, DFF3D_NATIVE_SCAN_MAX, model, geom, info, diag, DFF3D_NATIVE_STATUS_XBOX_VERTEX)) {
        return 1;
    }
    return 0;
}
