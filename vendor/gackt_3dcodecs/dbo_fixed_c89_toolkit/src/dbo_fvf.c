#include "dbo_int.h"

static dbo_u32 dbo__read_u32_le_ptr(const dbo_u8* p)
{
    return (dbo_u32)p[0]
         | ((dbo_u32)p[1] << 8)
         | ((dbo_u32)p[2] << 16)
         | ((dbo_u32)p[3] << 24);
}

static void dbo__write_u32_le_ptr(dbo_u8* p, dbo_u32 v)
{
    p[0] = (dbo_u8)(v & 0xFFUL);
    p[1] = (dbo_u8)((v >> 8) & 0xFFUL);
    p[2] = (dbo_u8)((v >> 16) & 0xFFUL);
    p[3] = (dbo_u8)((v >> 24) & 0xFFUL);
}

static dbo_u32 dbo__fvf_position_bytes(dbo_u32 fvf)
{
    dbo_u32 pos = fvf & DBO_D3DFVF_POSITION_MASK;
    switch (pos)
    {
    case DBO_D3DFVF_XYZ:
        return 12UL;
    case DBO_D3DFVF_XYZRHW:
        return 16UL;
    case DBO_D3DFVF_XYZB1:
        return 16UL;
    case DBO_D3DFVF_XYZB2:
        return 20UL;
    case DBO_D3DFVF_XYZB3:
        return 24UL;
    case DBO_D3DFVF_XYZB4:
        return 28UL;
    case DBO_D3DFVF_XYZB5:
        return 32UL;
    case DBO_D3DFVF_XYZW:
        return 16UL;
    default:
        return 0UL;
    }
}

dbo_u32 dbo_fvf_texcoord_count(dbo_u32 fvf)
{
    return (fvf & DBO_D3DFVF_TEXCOUNT_MASK) >> DBO_D3DFVF_TEXCOUNT_SHIFT;
}

dbo_u32 dbo_fvf_texcoord_dims(dbo_u32 fvf, dbo_u32 set_index)
{
    dbo_u32 bits;
    if (set_index >= 8UL)
    {
        return 0UL;
    }
    bits = (fvf >> (16UL + set_index * 2UL)) & 0x3UL;
    switch (bits)
    {
    case 0UL:
        return 2UL;
    case 1UL:
        return 3UL;
    case 2UL:
        return 4UL;
    default:
        return 1UL;
    }
}

dbo_u32 dbo_fvf_vertex_stride(dbo_u32 fvf)
{
    dbo_u32 stride;
    dbo_u32 i;
    dbo_u32 tcount;

    if (fvf == 0UL)
    {
        return 0UL;
    }

    stride = dbo__fvf_position_bytes(fvf);
    if ((fvf & DBO_D3DFVF_PSIZE) != 0UL)
    {
        stride += 4UL;
    }
    if ((fvf & DBO_D3DFVF_NORMAL) != 0UL)
    {
        stride += 12UL;
    }
    if ((fvf & DBO_D3DFVF_DIFFUSE) != 0UL)
    {
        stride += 4UL;
    }
    if ((fvf & DBO_D3DFVF_SPECULAR) != 0UL)
    {
        stride += 4UL;
    }

    tcount = dbo_fvf_texcoord_count(fvf);
    for (i = 0UL; i < tcount; ++i)
    {
        stride += dbo_fvf_texcoord_dims(fvf, i) * 4UL;
    }

    return stride;
}

static dbo_bool dbo__vertex_ptr(const dbo_scene* scene, const dbo_mesh* mesh, dbo_u32 vertex_index, const dbo_u8** out_ptr, dbo_u32* out_stride)
{
    const dbo_u8* base;
    dbo_u32 stride;

    if (scene == 0 || mesh == 0 || out_ptr == 0 || out_stride == 0)
    {
        return DBO_FALSE;
    }

    base = (const dbo_u8*)dbo_blob_data(scene, mesh->vertex_data);
    if (base == 0)
    {
        return DBO_FALSE;
    }

    stride = mesh->fvf_size;
    if (stride == 0UL)
    {
        stride = dbo_fvf_vertex_stride(mesh->fvf);
    }
    if (stride == 0UL)
    {
        return DBO_FALSE;
    }
    if (vertex_index >= mesh->vertex_count)
    {
        return DBO_FALSE;
    }
    if (mesh->vertex_data.size < (vertex_index + 1UL) * stride)
    {
        return DBO_FALSE;
    }

    *out_ptr = base + vertex_index * stride;
    *out_stride = stride;
    return DBO_TRUE;
}

static dbo_bool dbo__vertex_ptr_w(const dbo_mesh* mesh, void* vertex_ptr, dbo_u8** out_ptr)
{
    if (mesh == 0 || vertex_ptr == 0 || out_ptr == 0)
    {
        return DBO_FALSE;
    }
    *out_ptr = (dbo_u8*)vertex_ptr;
    return DBO_TRUE;
}

static dbo_u32 dbo__normal_offset(dbo_u32 fvf, dbo_bool* has_normal)
{
    dbo_u32 off = dbo__fvf_position_bytes(fvf);
    if ((fvf & DBO_D3DFVF_PSIZE) != 0UL)
    {
        off += 4UL;
    }
    if ((fvf & DBO_D3DFVF_NORMAL) != 0UL)
    {
        *has_normal = DBO_TRUE;
        return off;
    }
    *has_normal = DBO_FALSE;
    return 0UL;
}

static dbo_u32 dbo__texcoord_offset(dbo_u32 fvf, dbo_u32 set_index, dbo_u32* out_dims)
{
    dbo_u32 off;
    dbo_u32 i;
    off = dbo__fvf_position_bytes(fvf);
    if ((fvf & DBO_D3DFVF_PSIZE) != 0UL) off += 4UL;
    if ((fvf & DBO_D3DFVF_NORMAL) != 0UL) off += 12UL;
    if ((fvf & DBO_D3DFVF_DIFFUSE) != 0UL) off += 4UL;
    if ((fvf & DBO_D3DFVF_SPECULAR) != 0UL) off += 4UL;
    if (set_index >= dbo_fvf_texcoord_count(fvf))
    {
        *out_dims = 0UL;
        return 0UL;
    }
    for (i = 0UL; i < set_index; ++i)
    {
        off += dbo_fvf_texcoord_dims(fvf, i) * 4UL;
    }
    *out_dims = dbo_fvf_texcoord_dims(fvf, set_index);
    return off;
}

dbo_bool dbo_fvf_decode_position(const dbo_scene* scene, const dbo_mesh* mesh, dbo_u32 vertex_index, dbo_fx32 out_xyz[3])
{
    const dbo_u8* p;
    dbo_u32 stride;
    if (!dbo__vertex_ptr(scene, mesh, vertex_index, &p, &stride))
    {
        (void)stride;
        return DBO_FALSE;
    }
    out_xyz[0] = dbo_fx_from_float_bits(dbo__read_u32_le_ptr(p + 0UL));
    out_xyz[1] = dbo_fx_from_float_bits(dbo__read_u32_le_ptr(p + 4UL));
    out_xyz[2] = dbo_fx_from_float_bits(dbo__read_u32_le_ptr(p + 8UL));
    return DBO_TRUE;
}

dbo_bool dbo_fvf_decode_normal(const dbo_scene* scene, const dbo_mesh* mesh, dbo_u32 vertex_index, dbo_fx32 out_nrm[3])
{
    const dbo_u8* p;
    dbo_u32 stride;
    dbo_bool has_normal;
    dbo_u32 off;
    if (!dbo__vertex_ptr(scene, mesh, vertex_index, &p, &stride))
    {
        (void)stride;
        return DBO_FALSE;
    }
    off = dbo__normal_offset(mesh->fvf, &has_normal);
    if (!has_normal)
    {
        return DBO_FALSE;
    }
    out_nrm[0] = dbo_fx_from_float_bits(dbo__read_u32_le_ptr(p + off + 0UL));
    out_nrm[1] = dbo_fx_from_float_bits(dbo__read_u32_le_ptr(p + off + 4UL));
    out_nrm[2] = dbo_fx_from_float_bits(dbo__read_u32_le_ptr(p + off + 8UL));
    return DBO_TRUE;
}

dbo_bool dbo_fvf_decode_texcoord(const dbo_scene* scene, const dbo_mesh* mesh, dbo_u32 vertex_index, dbo_u32 set_index, dbo_fx32 out_uv[4])
{
    const dbo_u8* p;
    dbo_u32 stride;
    dbo_u32 off;
    dbo_u32 dims;
    dbo_u32 i;
    if (!dbo__vertex_ptr(scene, mesh, vertex_index, &p, &stride))
    {
        (void)stride;
        return DBO_FALSE;
    }
    off = dbo__texcoord_offset(mesh->fvf, set_index, &dims);
    if (dims == 0UL)
    {
        return DBO_FALSE;
    }
    for (i = 0UL; i < 4UL; ++i)
    {
        out_uv[i] = 0;
    }
    for (i = 0UL; i < dims; ++i)
    {
        out_uv[i] = dbo_fx_from_float_bits(dbo__read_u32_le_ptr(p + off + i * 4UL));
    }
    return DBO_TRUE;
}

dbo_bool dbo_fvf_encode_position(dbo_u32 fvf, void* vertex, const dbo_fx32 xyz[3])
{
    dbo_u8* p;
    if (!dbo__vertex_ptr_w((const dbo_mesh*)1, vertex, &p))
    {
        return DBO_FALSE;
    }
    if (dbo__fvf_position_bytes(fvf) < 12UL)
    {
        return DBO_FALSE;
    }
    dbo__write_u32_le_ptr(p + 0UL, dbo_fx_to_float_bits(xyz[0]));
    dbo__write_u32_le_ptr(p + 4UL, dbo_fx_to_float_bits(xyz[1]));
    dbo__write_u32_le_ptr(p + 8UL, dbo_fx_to_float_bits(xyz[2]));
    return DBO_TRUE;
}

dbo_bool dbo_fvf_encode_normal(dbo_u32 fvf, void* vertex, const dbo_fx32 nrm[3])
{
    dbo_u8* p;
    dbo_bool has_normal;
    dbo_u32 off;
    if (!dbo__vertex_ptr_w((const dbo_mesh*)1, vertex, &p))
    {
        return DBO_FALSE;
    }
    off = dbo__normal_offset(fvf, &has_normal);
    if (!has_normal)
    {
        return DBO_FALSE;
    }
    dbo__write_u32_le_ptr(p + off + 0UL, dbo_fx_to_float_bits(nrm[0]));
    dbo__write_u32_le_ptr(p + off + 4UL, dbo_fx_to_float_bits(nrm[1]));
    dbo__write_u32_le_ptr(p + off + 8UL, dbo_fx_to_float_bits(nrm[2]));
    return DBO_TRUE;
}

dbo_bool dbo_fvf_encode_texcoord(dbo_u32 fvf, void* vertex, dbo_u32 set_index, const dbo_fx32 uv[4])
{
    dbo_u8* p;
    dbo_u32 off;
    dbo_u32 dims;
    dbo_u32 i;
    if (!dbo__vertex_ptr_w((const dbo_mesh*)1, vertex, &p))
    {
        return DBO_FALSE;
    }
    off = dbo__texcoord_offset(fvf, set_index, &dims);
    if (dims == 0UL)
    {
        return DBO_FALSE;
    }
    for (i = 0UL; i < dims; ++i)
    {
        dbo__write_u32_le_ptr(p + off + i * 4UL, dbo_fx_to_float_bits(uv[i]));
    }
    return DBO_TRUE;
}
