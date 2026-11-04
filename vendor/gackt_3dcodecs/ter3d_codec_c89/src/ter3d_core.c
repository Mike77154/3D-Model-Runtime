#include "ter3d.h"

const char *ter3d_result_name(ter3d_result r)
{
    switch (r) {
    case TER3D_OK: return "TER3D_OK";
    case TER3D_E_NULL: return "TER3D_E_NULL";
    case TER3D_E_RANGE: return "TER3D_E_RANGE";
    case TER3D_E_FORMAT: return "TER3D_E_FORMAT";
    case TER3D_E_NEED_MORE: return "TER3D_E_NEED_MORE";
    case TER3D_E_UNSUPPORTED: return "TER3D_E_UNSUPPORTED";
    case TER3D_E_BAD_ORDER: return "TER3D_E_BAD_ORDER";
    case TER3D_E_OVERFLOW: return "TER3D_E_OVERFLOW";
    case TER3D_E_OUTPUT_SMALL: return "TER3D_E_OUTPUT_SMALL";
    case TER3D_E_CALLBACK: return "TER3D_E_CALLBACK";
    case TER3D_E_NOT_FOUND: return "TER3D_E_NOT_FOUND";
    default: return "TER3D_E_UNKNOWN";
    }
}

ter3d_u16 ter3d_read_le16u(const ter3d_u8 *p)
{
    return (ter3d_u16)(((ter3d_u16)p[0]) | ((ter3d_u16)p[1] << 8));
}

ter3d_s16 ter3d_read_le16s(const ter3d_u8 *p)
{
    ter3d_u16 u;
    u = ter3d_read_le16u(p);
    return (ter3d_s16)u;
}

ter3d_u32 ter3d_read_le32u(const ter3d_u8 *p)
{
    return ((ter3d_u32)p[0]) |
           ((ter3d_u32)p[1] << 8) |
           ((ter3d_u32)p[2] << 16) |
           ((ter3d_u32)p[3] << 24);
}

void ter3d_write_le16u(ter3d_u8 *p, ter3d_u16 v)
{
    p[0] = (ter3d_u8)(v & 255U);
    p[1] = (ter3d_u8)((v >> 8) & 255U);
}

void ter3d_write_le32u(ter3d_u8 *p, ter3d_u32 v)
{
    p[0] = (ter3d_u8)(v & 255UL);
    p[1] = (ter3d_u8)((v >> 8) & 255UL);
    p[2] = (ter3d_u8)((v >> 16) & 255UL);
    p[3] = (ter3d_u8)((v >> 24) & 255UL);
}

static int ter3d_memeq(const ter3d_u8 *a, const char *b, ter3d_u32 n)
{
    ter3d_u32 i;
    if (a == 0 || b == 0) return 0;
    for (i = 0; i < n; ++i) {
        if (a[i] != (ter3d_u8)b[i]) return 0;
    }
    return 1;
}

ter3d_family ter3d_probe_family(const ter3d_u8 *data, ter3d_u32 size)
{
    if (data == 0 || size < 4UL) return TER3D_FAMILY_UNKNOWN;
    if (size >= 16UL && ter3d_memeq(data, "TERRAGEN", 8UL) && ter3d_memeq(data + 8, "TERRAIN ", 8UL)) {
        return TER3D_FAMILY_TERRAGEN;
    }
    if (size >= 2820UL && ter3d_memeq(data, "TERR", 4UL)) {
        return TER3D_FAMILY_SWBF;
    }
    return TER3D_FAMILY_UNKNOWN;
}

ter3d_result ter3d_read_info(const ter3d_u8 *data, ter3d_u32 size, ter3d_info *out_info)
{
    ter3d_family family;
    ter3d_result r;
    if (data == 0 || out_info == 0) return TER3D_E_NULL;
    out_info->family = TER3D_FAMILY_UNKNOWN;
    family = ter3d_probe_family(data, size);
    if (family == TER3D_FAMILY_TERRAGEN) {
        r = ter3d_terragen_read_info(data, size, &out_info->terragen);
        if (r == TER3D_OK) out_info->family = family;
        return r;
    }
    if (family == TER3D_FAMILY_SWBF) {
        r = ter3d_swbf_read_info(data, size, &out_info->swbf);
        if (r == TER3D_OK) out_info->family = family;
        return r;
    }
    return TER3D_E_FORMAT;
}
