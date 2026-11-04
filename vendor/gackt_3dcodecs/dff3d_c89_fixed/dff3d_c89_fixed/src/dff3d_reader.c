#include "dff3d_reader.h"
#include "dff3d_fixed.h"

void dff3d_reader_init(Dff3dReader *r, const void *data, dff_u32 size)
{
    if (r != DFF3D_NULL) {
        r->data = (const dff_u8*)data;
        r->size = size;
        r->pos = 0U;
        r->error = 0;
    }
}

int dff3d_reader_can_read(const Dff3dReader *r, dff_u32 n)
{
    if (r == DFF3D_NULL) {
        return 0;
    }
    if (r->pos > r->size) {
        return 0;
    }
    if (n > (r->size - r->pos)) {
        return 0;
    }
    return 1;
}

dff_u8 dff3d_read_u8(Dff3dReader *r)
{
    dff_u8 v;
    if (!dff3d_reader_can_read(r, 1U)) {
        if (r != DFF3D_NULL) {
            r->error = 1;
        }
        return 0U;
    }
    v = r->data[r->pos];
    r->pos += 1U;
    return v;
}

dff_u16 dff3d_read_u16(Dff3dReader *r)
{
    dff_u16 v;
    if (!dff3d_reader_can_read(r, 2U)) {
        if (r != DFF3D_NULL) {
            r->error = 1;
        }
        return 0U;
    }
    v = (dff_u16)r->data[r->pos];
    v = (dff_u16)(v | ((dff_u16)r->data[r->pos + 1U] << 8));
    r->pos += 2U;
    return v;
}

dff_u32 dff3d_read_u32(Dff3dReader *r)
{
    dff_u32 v;
    if (!dff3d_reader_can_read(r, 4U)) {
        if (r != DFF3D_NULL) {
            r->error = 1;
        }
        return 0UL;
    }
    v = (dff_u32)r->data[r->pos];
    v |= ((dff_u32)r->data[r->pos + 1U] << 8);
    v |= ((dff_u32)r->data[r->pos + 2U] << 16);
    v |= ((dff_u32)r->data[r->pos + 3U] << 24);
    r->pos += 4U;
    return v;
}

dff_i32 dff3d_read_i32(Dff3dReader *r)
{
    dff_u32 u;
    u = dff3d_read_u32(r);
    return (dff_i32)u;
}

dff_fixed dff3d_read_f32_fixed(Dff3dReader *r)
{
    dff_u32 bits;
    bits = dff3d_read_u32(r);
    return dff3d_fixed_from_f32_bits(bits);
}

int dff3d_read_chunk_header(Dff3dReader *r, Dff3dChunk *c)
{
    dff_u32 start;
    dff_u32 body;
    dff_u32 end;

    if (r == DFF3D_NULL || c == DFF3D_NULL) {
        return 0;
    }
    start = r->pos;
    if (!dff3d_reader_can_read(r, 12U)) {
        r->error = 1;
        return 0;
    }
    c->type = dff3d_read_u32(r);
    c->size = dff3d_read_u32(r);
    c->version_stamp = dff3d_read_u32(r);
    body = r->pos;
    if (c->size > (r->size - body)) {
        r->error = 1;
        r->pos = start;
        return 0;
    }
    end = body + c->size;
    c->body = body;
    c->end = end;
    return 1;
}

void dff3d_reader_skip_to(Dff3dReader *r, dff_u32 pos)
{
    if (r == DFF3D_NULL) {
        return;
    }
    if (pos > r->size) {
        r->pos = r->size;
        r->error = 1;
    } else {
        r->pos = pos;
    }
}

void dff3d_reader_sub(Dff3dReader *dst, const Dff3dReader *src, const Dff3dChunk *c)
{
    if (dst == DFF3D_NULL || src == DFF3D_NULL || c == DFF3D_NULL) {
        return;
    }
    dst->data = src->data;
    dst->size = c->end;
    dst->pos = c->body;
    dst->error = 0;
}

dff_u32 dff3d_decode_rw_version(dff_u32 library_id_stamp)
{
    dff_u32 version;
    if ((library_id_stamp & 0xffff0000UL) != 0UL) {
        version = ((library_id_stamp >> 14) & 0x0003ff00UL) + 0x00030000UL;
        version |= ((library_id_stamp >> 16) & 0x0000003fUL);
        return version;
    }
    return library_id_stamp << 8;
}
