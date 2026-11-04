#include "ter3d.h"

#define TER3D_S32_MAX_LOCAL ((ter3d_s32)2147483647L)
#define TER3D_S32_MIN_LOCAL ((ter3d_s32)(-2147483647L - 1L))

static ter3d_fixed ter3d_index_to_q16(ter3d_u32 v)
{
    if (v > 32767UL) return TER3D_S32_MAX_LOCAL;
    return (ter3d_fixed)((ter3d_s32)v << TER3D_Q);
}

ter3d_result ter3d_terragen_emit_mesh(const ter3d_u8 *data, ter3d_u32 size, const ter3d_terragen_info *info, const ter3d_mesh_callbacks *cb)
{
    ter3d_u32 x;
    ter3d_u32 y;
    ter3d_u32 idx;
    ter3d_mesh_vertex v;
    ter3d_mesh_tri t;
    ter3d_fixed h;
    ter3d_fixed xq;
    ter3d_fixed yq;
    ter3d_fixed zq;
    ter3d_result r;

    if (data == 0 || info == 0 || cb == 0) return TER3D_E_NULL;
    if (cb->vertex == 0 && cb->triangle == 0) return TER3D_E_NULL;
    if (!info->has_altw) return TER3D_E_FORMAT;
    if (info->xpts == 0U || info->ypts == 0U) return TER3D_E_FORMAT;

    idx = 0UL;
    if (cb->vertex != 0) {
        for (y = 0UL; y < (ter3d_u32)info->ypts; ++y) {
            for (x = 0UL; x < (ter3d_u32)info->xpts; ++x) {
                r = ter3d_terragen_get_height_q16(data, size, info, (ter3d_u16)x, (ter3d_u16)y, &h);
                if (r != TER3D_OK) return r;
                xq = ter3d_index_to_q16(x);
                yq = ter3d_index_to_q16(cb->flip_y ? ((ter3d_u32)info->ypts - 1UL - y) : y);
                zq = h;
                if (cb->emit_world_meters) {
                    xq = ter3d_fixed_mul_sat(xq, info->scale_x_q16);
                    yq = ter3d_fixed_mul_sat(yq, info->scale_y_q16);
                    zq = ter3d_fixed_mul_sat(zq, info->scale_z_q16);
                }
                v.index = idx;
                v.ix = (ter3d_u16)x;
                v.iy = (ter3d_u16)y;
                v.x_q16 = xq;
                v.y_q16 = yq;
                v.z_q16 = zq;
                if (cb->vertex(cb->user, &v) == 0) return TER3D_E_CALLBACK;
                ++idx;
            }
        }
    }

    if (cb->triangle != 0) {
        ter3d_u32 tri_idx;
        ter3d_u32 row;
        ter3d_u32 a;
        ter3d_u32 b;
        ter3d_u32 c;
        ter3d_u32 d;
        tri_idx = 0UL;
        if (info->xpts < 2U || info->ypts < 2U) return TER3D_OK;
        for (y = 0UL; y < (ter3d_u32)(info->ypts - 1U); ++y) {
            row = y * (ter3d_u32)info->xpts;
            for (x = 0UL; x < (ter3d_u32)(info->xpts - 1U); ++x) {
                a = row + x;
                b = row + x + 1UL;
                c = row + (ter3d_u32)info->xpts + x;
                d = c + 1UL;
                t.index = tri_idx++;
                if (cb->flip_y) { t.a = a; t.b = d; t.c = b; }
                else { t.a = a; t.b = c; t.c = b; }
                if (cb->triangle(cb->user, &t) == 0) return TER3D_E_CALLBACK;
                t.index = tri_idx++;
                if (cb->flip_y) { t.a = a; t.b = c; t.c = d; }
                else { t.a = b; t.b = c; t.c = d; }
                if (cb->triangle(cb->user, &t) == 0) return TER3D_E_CALLBACK;
            }
        }
    }
    return TER3D_OK;
}
