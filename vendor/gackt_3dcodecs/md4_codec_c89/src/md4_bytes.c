#include "md4_internal.h"

md4_u16 md4_read_u16_le(const md4_u8 *p)
{
    return (md4_u16)(((md4_u16)p[0]) | ((md4_u16)p[1] << 8));
}

md4_u32 md4_read_u32_le(const md4_u8 *p)
{
    return ((md4_u32)p[0]) |
           ((md4_u32)p[1] << 8) |
           ((md4_u32)p[2] << 16) |
           ((md4_u32)p[3] << 24);
}

md4_i32 md4_read_i32_le(const md4_u8 *p)
{
    return (md4_i32)md4_read_u32_le(p);
}

md4_fx_t md4_read_fx32_from_f32_le(const md4_u8 *p)
{
    return md4_fx_from_ieee754_bits(md4_read_u32_le(p));
}

void md4_write_u16_le(md4_u8 *p, md4_u16 v)
{
    p[0] = (md4_u8)(v & 0xffU);
    p[1] = (md4_u8)((v >> 8) & 0xffU);
}

void md4_write_u32_le(md4_u8 *p, md4_u32 v)
{
    p[0] = (md4_u8)(v & 0xffU);
    p[1] = (md4_u8)((v >> 8) & 0xffU);
    p[2] = (md4_u8)((v >> 16) & 0xffU);
    p[3] = (md4_u8)((v >> 24) & 0xffU);
}

void md4_write_i32_le(md4_u8 *p, md4_i32 v)
{
    md4_write_u32_le(p, (md4_u32)v);
}

void md4_write_f32_le_from_fx32(md4_u8 *p, md4_fx_t v)
{
    md4_write_u32_le(p, md4_fx_to_ieee754_bits(v));
}
