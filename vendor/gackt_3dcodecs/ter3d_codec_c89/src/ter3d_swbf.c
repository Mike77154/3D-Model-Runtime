#include "ter3d.h"

#define TER3D_SWBF_WARN_SHORT_OPTIONAL 0x00000001UL
#define TER3D_SWBF_WARN_UNKNOWN_VER    0x00000002UL

static void ter3d_copy_fixed_name(char dst[32], const ter3d_u8 *src)
{
    int i;
    for (i = 0; i < 31; ++i) {
        dst[i] = (char)src[i];
        if (src[i] == 0U) {
            ++i;
            while (i < 32) { dst[i] = 0; ++i; }
            return;
        }
    }
    dst[31] = 0;
}

static void ter3d_zero_swbf(ter3d_swbf_info *info)
{
    int i;
    int j;
    info->valid = 0;
    info->version = 0UL;
    info->extent_min_x = 0;
    info->extent_min_y = 0;
    info->extent_max_x = 0;
    info->extent_max_y = 0;
    info->map_height_multiplier_q16 = 0;
    info->grid_scale_q16 = 0;
    info->full_map_size = 0UL;
    info->height_offset = 0UL;
    info->color0_offset = 0UL;
    info->color1_offset = 0UL;
    info->texture_alpha_offset = 0UL;
    info->water_offset = 0UL;
    info->foliage_offset = 0UL;
    info->header_size = 0UL;
    info->warning_flags = 0UL;
    for (i = 0; i < 16; ++i) {
        for (j = 0; j < 32; ++j) {
            info->layers[i].diffuse[j] = 0;
            info->layers[i].detail[j] = 0;
            info->water[i].texture[j] = 0;
        }
        info->water[i].height0_q16 = 0;
        info->water[i].height1_q16 = 0;
        info->water[i].uv_vel_u_q16 = 0;
        info->water[i].uv_vel_v_q16 = 0;
        info->water[i].uv_repeat_u_q16 = 0;
        info->water[i].uv_repeat_v_q16 = 0;
        for (j = 0; j < 4; ++j) info->water[i].rgba[j] = 0U;
    }
}

static int ter3d_u32_mul_over(ter3d_u32 a, ter3d_u32 b, ter3d_u32 *out)
{
    if (a != 0UL && b > 0xffffffffUL / a) return 1;
    *out = a * b;
    return 0;
}

ter3d_result ter3d_swbf_read_info(const ter3d_u8 *data, ter3d_u32 size, ter3d_swbf_info *out_info)
{
    ter3d_u32 fs;
    ter3d_u32 block2;
    ter3d_u32 block4;
    ter3d_u32 block16;
    ter3d_u32 off;
    int i;

    if (data == 0 || out_info == 0) return TER3D_E_NULL;
    ter3d_zero_swbf(out_info);
    if (size < 2820UL) return TER3D_E_NEED_MORE;
    if (!(data[0] == 'T' && data[1] == 'E' && data[2] == 'R' && data[3] == 'R')) return TER3D_E_FORMAT;

    out_info->version = ter3d_read_le32u(data + 4UL);
    if (out_info->version != 21UL && out_info->version != 22UL) out_info->warning_flags |= TER3D_SWBF_WARN_UNKNOWN_VER;
    out_info->header_size = (out_info->version == 21UL) ? 2820UL : 2821UL;
    if (size < out_info->header_size) return TER3D_E_NEED_MORE;

    out_info->extent_min_x = ter3d_read_le16s(data + 8UL);
    out_info->extent_min_y = ter3d_read_le16s(data + 10UL);
    out_info->extent_max_x = ter3d_read_le16s(data + 12UL);
    out_info->extent_max_y = ter3d_read_le16s(data + 14UL);
    out_info->map_height_multiplier_q16 = ter3d_ieee754_le_to_q16(data + 164UL);
    out_info->grid_scale_q16 = ter3d_ieee754_le_to_q16(data + 168UL);
    out_info->full_map_size = ter3d_read_le32u(data + 176UL);
    fs = out_info->full_map_size;
    if (fs == 0UL) return TER3D_E_FORMAT;
    if (ter3d_u32_mul_over(fs, fs, &block2)) return TER3D_E_OVERFLOW;
    if (ter3d_u32_mul_over(block2, 2UL, &block2)) return TER3D_E_OVERFLOW;
    if (ter3d_u32_mul_over(fs, fs, &block4)) return TER3D_E_OVERFLOW;
    if (ter3d_u32_mul_over(block4, 4UL, &block4)) return TER3D_E_OVERFLOW;
    if (ter3d_u32_mul_over(fs, fs, &block16)) return TER3D_E_OVERFLOW;
    if (ter3d_u32_mul_over(block16, 16UL, &block16)) return TER3D_E_OVERFLOW;

    off = (out_info->version == 21UL) ? 184UL : 185UL;
    for (i = 0; i < 16; ++i) {
        ter3d_copy_fixed_name(out_info->layers[i].diffuse, data + off + (ter3d_u32)i * 64UL);
        ter3d_copy_fixed_name(out_info->layers[i].detail, data + off + (ter3d_u32)i * 64UL + 32UL);
    }
    off += 1024UL;
    for (i = 0; i < 16; ++i) {
        ter3d_u32 woff;
        woff = off + (ter3d_u32)i * 68UL;
        if (woff + 68UL <= size) {
            out_info->water[i].height0_q16 = ter3d_ieee754_le_to_q16(data + woff);
            out_info->water[i].height1_q16 = ter3d_ieee754_le_to_q16(data + woff + 4UL);
            out_info->water[i].uv_vel_u_q16 = ter3d_ieee754_le_to_q16(data + woff + 16UL);
            out_info->water[i].uv_vel_v_q16 = ter3d_ieee754_le_to_q16(data + woff + 20UL);
            out_info->water[i].uv_repeat_u_q16 = ter3d_ieee754_le_to_q16(data + woff + 24UL);
            out_info->water[i].uv_repeat_v_q16 = ter3d_ieee754_le_to_q16(data + woff + 28UL);
            out_info->water[i].rgba[0] = data[woff + 32UL];
            out_info->water[i].rgba[1] = data[woff + 33UL];
            out_info->water[i].rgba[2] = data[woff + 34UL];
            out_info->water[i].rgba[3] = data[woff + 35UL];
            ter3d_copy_fixed_name(out_info->water[i].texture, data + woff + 36UL);
        }
    }

    out_info->height_offset = out_info->header_size;
    out_info->color0_offset = out_info->height_offset + block2;
    out_info->color1_offset = out_info->color0_offset + block4;
    out_info->texture_alpha_offset = out_info->color1_offset + block4;
    out_info->water_offset = out_info->texture_alpha_offset + block16;
    out_info->foliage_offset = out_info->water_offset + ((fs * fs) / 2UL);
    if (out_info->height_offset + block2 > size) return TER3D_E_NEED_MORE;
    if (out_info->foliage_offset > size) out_info->warning_flags |= TER3D_SWBF_WARN_SHORT_OPTIONAL;
    out_info->valid = 1;
    return TER3D_OK;
}

ter3d_result ter3d_swbf_raw_height(const ter3d_u8 *data, ter3d_u32 size, const ter3d_swbf_info *info, ter3d_u32 x, ter3d_u32 y, ter3d_s16 *out_height)
{
    ter3d_u32 off;
    ter3d_u32 idx;
    if (data == 0 || info == 0 || out_height == 0) return TER3D_E_NULL;
    if (!info->valid || info->full_map_size == 0UL) return TER3D_E_FORMAT;
    if (x >= info->full_map_size || y >= info->full_map_size) return TER3D_E_RANGE;
    idx = y * info->full_map_size + x;
    off = info->height_offset + idx * 2UL;
    if (off + 2UL > size) return TER3D_E_NEED_MORE;
    *out_height = ter3d_read_le16s(data + off);
    return TER3D_OK;
}

ter3d_result ter3d_swbf_height_q16(const ter3d_u8 *data, ter3d_u32 size, const ter3d_swbf_info *info, ter3d_u32 x, ter3d_u32 y, ter3d_fixed *out_height_q16)
{
    ter3d_s16 h;
    ter3d_result r;
    if (out_height_q16 == 0) return TER3D_E_NULL;
    r = ter3d_swbf_raw_height(data, size, info, x, y, &h);
    if (r != TER3D_OK) return r;
    *out_height_q16 = ter3d_fixed_mul_sat(ter3d_fixed_from_int((ter3d_s32)h), info->map_height_multiplier_q16);
    return TER3D_OK;
}

ter3d_result ter3d_swbf_color0_rgba(const ter3d_u8 *data, ter3d_u32 size, const ter3d_swbf_info *info, ter3d_u32 x, ter3d_u32 y, ter3d_u8 rgba[4])
{
    ter3d_u32 off;
    ter3d_u32 idx;
    if (data == 0 || info == 0 || rgba == 0) return TER3D_E_NULL;
    if (!info->valid || info->full_map_size == 0UL) return TER3D_E_FORMAT;
    if (x >= info->full_map_size || y >= info->full_map_size) return TER3D_E_RANGE;
    idx = y * info->full_map_size + x;
    off = info->color0_offset + idx * 4UL;
    if (off + 4UL > size) return TER3D_E_NEED_MORE;
    rgba[0] = data[off + 0UL];
    rgba[1] = data[off + 1UL];
    rgba[2] = data[off + 2UL];
    rgba[3] = data[off + 3UL];
    return TER3D_OK;
}

ter3d_result ter3d_swbf_texture_alpha16(const ter3d_u8 *data, ter3d_u32 size, const ter3d_swbf_info *info, ter3d_u32 x, ter3d_u32 y, ter3d_u8 alpha16[16])
{
    ter3d_u32 off;
    ter3d_u32 idx;
    int i;
    if (data == 0 || info == 0 || alpha16 == 0) return TER3D_E_NULL;
    if (!info->valid || info->full_map_size == 0UL) return TER3D_E_FORMAT;
    if (x >= info->full_map_size || y >= info->full_map_size) return TER3D_E_RANGE;
    idx = y * info->full_map_size + x;
    off = info->texture_alpha_offset + idx * 16UL;
    if (off + 16UL > size) return TER3D_E_NEED_MORE;
    for (i = 0; i < 16; ++i) alpha16[i] = data[off + (ter3d_u32)i];
    return TER3D_OK;
}
