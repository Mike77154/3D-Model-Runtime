#include "dff3d_native_common.h"

static int dff3d_ps2_platform(dff_u32 platform)
{
    return platform == DFF3D_NATIVE_PS2 || platform == DFF3D_NATIVE_PS2_FOURCC;
}

static dff_u32 dff3d_ps2_packet_score(const dff_u8 *raw, dff_u32 raw_size)
{
    dff_u32 i;
    dff_u32 score;
    dff_u32 w;

    score = 0U;
    i = 4U;
    while (i + 4U <= raw_size && i < 4096U) {
        w = dff3d_native_raw_u32(raw + i);
        if ((w & 0xff000000UL) == 0x50000000UL || (w & 0xff000000UL) == 0x60000000UL || (w & 0xff000000UL) == 0x70000000UL) {
            score++;
        }
        if ((w & 0x7fffffffUL) == 0x00008000UL || (w & 0xffff0000UL) == 0x10000000UL) {
            score++;
        }
        i += 4U;
    }
    return score;
}

int dff3d_probe_native_ps2(const dff_u8 *raw,
                           dff_u32 raw_size,
                           Dff3dModel *model,
                           Dff3dGeometry *geom,
                           Dff3dNativeInfo *info,
                           Dff3dDiag *diag)
{
    dff_u32 platform;
    dff_u32 score;

    if (raw == DFF3D_NULL || raw_size < 12U || model == DFF3D_NULL || geom == DFF3D_NULL) {
        return 0;
    }
    platform = dff3d_native_raw_u32(raw);
    if (!dff3d_ps2_platform(platform)) {
        return 0;
    }
    if (diag != DFF3D_NULL) {
        diag->native_ps2_probe++;
    }
    score = dff3d_ps2_packet_score(raw, raw_size);
    if (info != DFF3D_NULL) {
        info->candidate_score += score;
    }
    if (dff3d_native_scan_f32_stream(raw, raw_size, 4U, DFF3D_NATIVE_SCAN_MAX, model, geom, info, diag, DFF3D_NATIVE_STATUS_PS2_VERTEX)) {
        return 1;
    }
    if (dff3d_native_scan_i16_stream(raw, raw_size, 4U, DFF3D_NATIVE_SCAN_MAX, model, geom, info, diag, DFF3D_NATIVE_STATUS_PS2_VERTEX)) {
        return 1;
    }
    return 0;
}
