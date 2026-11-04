#include "ter3d.h"

static ter3d_u32 ter3d_align4_w(ter3d_u32 v)
{
    return (v + 3UL) & ~3UL;
}

static ter3d_result ter3d_put_bytes(ter3d_u8 *out, ter3d_u32 cap, ter3d_u32 *pos, const char *s, ter3d_u32 n)
{
    ter3d_u32 i;
    if (*pos + n > cap) return TER3D_E_OUTPUT_SMALL;
    for (i = 0UL; i < n; ++i) out[*pos + i] = (ter3d_u8)s[i];
    *pos += n;
    return TER3D_OK;
}

static ter3d_result ter3d_put_u16(ter3d_u8 *out, ter3d_u32 cap, ter3d_u32 *pos, ter3d_u16 v)
{
    if (*pos + 2UL > cap) return TER3D_E_OUTPUT_SMALL;
    ter3d_write_le16u(out + *pos, v);
    *pos += 2UL;
    return TER3D_OK;
}

static ter3d_result ter3d_put_u32(ter3d_u8 *out, ter3d_u32 cap, ter3d_u32 *pos, ter3d_u32 v)
{
    if (*pos + 4UL > cap) return TER3D_E_OUTPUT_SMALL;
    ter3d_write_le32u(out + *pos, v);
    *pos += 4UL;
    return TER3D_OK;
}

static ter3d_result ter3d_put_pad_to4(ter3d_u8 *out, ter3d_u32 cap, ter3d_u32 *pos)
{
    ter3d_u32 next;
    next = ter3d_align4_w(*pos);
    while (*pos < next) {
        if (*pos >= cap) return TER3D_E_OUTPUT_SMALL;
        out[*pos] = 0U;
        *pos += 1UL;
    }
    return TER3D_OK;
}

ter3d_result ter3d_terragen_write(const ter3d_write_desc *desc, ter3d_u8 *out, ter3d_u32 out_cap, ter3d_u32 *out_size)
{
    ter3d_u32 pos;
    ter3d_u32 i;
    ter3d_u32 count;
    ter3d_u32 bits;
    ter3d_result r;
    ter3d_u16 size_value;

    if (desc == 0 || out == 0 || out_size == 0) return TER3D_E_NULL;
    *out_size = 0UL;
    if (desc->xpts == 0U || desc->ypts == 0U || desc->elevations == 0) return TER3D_E_FORMAT;
    if ((ter3d_u32)desc->xpts * (ter3d_u32)desc->ypts > 0xffffffffUL / 2UL) return TER3D_E_OVERFLOW;

    size_value = (desc->xpts < desc->ypts) ? (ter3d_u16)(desc->xpts - 1U) : (ter3d_u16)(desc->ypts - 1U);
    pos = 0UL;
    r = ter3d_put_bytes(out, out_cap, &pos, "TERRAGENTERRAIN ", 16UL); if (r != TER3D_OK) return r;

    r = ter3d_put_bytes(out, out_cap, &pos, "SIZE", 4UL); if (r != TER3D_OK) return r;
    r = ter3d_put_u16(out, out_cap, &pos, size_value); if (r != TER3D_OK) return r;
    r = ter3d_put_u16(out, out_cap, &pos, 0U); if (r != TER3D_OK) return r;

    if (desc->xpts != desc->ypts) {
        r = ter3d_put_bytes(out, out_cap, &pos, "XPTS", 4UL); if (r != TER3D_OK) return r;
        r = ter3d_put_u16(out, out_cap, &pos, desc->xpts); if (r != TER3D_OK) return r;
        r = ter3d_put_u16(out, out_cap, &pos, 0U); if (r != TER3D_OK) return r;
        r = ter3d_put_bytes(out, out_cap, &pos, "YPTS", 4UL); if (r != TER3D_OK) return r;
        r = ter3d_put_u16(out, out_cap, &pos, desc->ypts); if (r != TER3D_OK) return r;
        r = ter3d_put_u16(out, out_cap, &pos, 0U); if (r != TER3D_OK) return r;
    }

    if (desc->write_scal) {
        r = ter3d_put_bytes(out, out_cap, &pos, "SCAL", 4UL); if (r != TER3D_OK) return r;
        bits = ter3d_q16_to_ieee754_bits(desc->scale_x_q16); r = ter3d_put_u32(out, out_cap, &pos, bits); if (r != TER3D_OK) return r;
        bits = ter3d_q16_to_ieee754_bits(desc->scale_y_q16); r = ter3d_put_u32(out, out_cap, &pos, bits); if (r != TER3D_OK) return r;
        bits = ter3d_q16_to_ieee754_bits(desc->scale_z_q16); r = ter3d_put_u32(out, out_cap, &pos, bits); if (r != TER3D_OK) return r;
    }

    r = ter3d_put_bytes(out, out_cap, &pos, "ALTW", 4UL); if (r != TER3D_OK) return r;
    r = ter3d_put_u16(out, out_cap, &pos, (ter3d_u16)desc->height_scale); if (r != TER3D_OK) return r;
    r = ter3d_put_u16(out, out_cap, &pos, (ter3d_u16)desc->base_height); if (r != TER3D_OK) return r;

    count = (ter3d_u32)desc->xpts * (ter3d_u32)desc->ypts;
    for (i = 0UL; i < count; ++i) {
        r = ter3d_put_u16(out, out_cap, &pos, (ter3d_u16)desc->elevations[i]);
        if (r != TER3D_OK) return r;
    }
    r = ter3d_put_pad_to4(out, out_cap, &pos); if (r != TER3D_OK) return r;
    r = ter3d_put_bytes(out, out_cap, &pos, "EOF ", 4UL); if (r != TER3D_OK) return r;
    *out_size = pos;
    return TER3D_OK;
}
