#include "tmd_mime3d.h"

static tmd_i32 tmd_mime_fx_mul(tmd_i32 a, tmd_i32 b)
{
    tmd_i32 prod;
    prod = a * b;
    if (prod >= 0) return (prod + (TMD_FX_ONE >> 1)) >> TMD_FX_SHIFT;
    return -(((-prod) + (TMD_FX_ONE >> 1)) >> TMD_FX_SHIFT);
}

static tmd_i16 tmd_mime_clamp_i16(tmd_i32 v)
{
    if (v > 32767L) return (tmd_i16)32767;
    if (v < -32768L) return (tmd_i16)-32768;
    return (tmd_i16)v;
}

static tmd_i32 tmd_mime_lerp_i32(tmd_i32 a, tmd_i32 b, tmd_i32 w)
{
    return a + tmd_mime_fx_mul(b - a, w);
}

int tmd_mime_decode_primitive_type(tmd_u16 primitive_code,
                                   TMD_MIMe_PrimitiveInfo *out_info)
{
    if (out_info == TMD_NULL) return TMD_ERR_NULL;
    out_info->is_reset = (tmd_u8)((primitive_code >> 5) & 0x01U);
    out_info->code0 = (tmd_u8)((primitive_code >> 4) & 0x01U);
    out_info->code1 = (tmd_u8)(primitive_code & 0x01U);
    return TMD_ERR_OK;
}

int tmd_mime_apply_vertex_diffs(const TMD_Vertex *base_vertices,
                                tmd_u32 vertex_count,
                                const TMD_MIMe_VertexDiff *diffs,
                                tmd_u32 diff_count,
                                tmd_i32 weight_fx12,
                                TMD_Vertex *out_vertices,
                                tmd_u32 out_vertex_count)
{
    tmd_u32 i;
    tmd_u32 idx;
    tmd_i32 x;
    tmd_i32 y;
    tmd_i32 z;

    if (base_vertices == TMD_NULL || out_vertices == TMD_NULL) return TMD_ERR_NULL;
    if (diff_count > 0UL && diffs == TMD_NULL) return TMD_ERR_NULL;
    if (vertex_count > out_vertex_count) return TMD_ERR_OUTPUT_OVERFLOW;
    if (diff_count > TMD_MIME_MAX_DIFFS_PER_PRIMITIVE) return TMD_ERR_BAD_MIME;

    for (i = 0UL; i < vertex_count; ++i) out_vertices[i] = base_vertices[i];

    for (i = 0UL; i < diff_count; ++i) {
        idx = (tmd_u32)diffs[i].vertex_index;
        if (idx >= vertex_count) return TMD_ERR_BAD_MIME;
        x = (tmd_i32)out_vertices[idx].x + tmd_mime_fx_mul((tmd_i32)diffs[i].dx, weight_fx12);
        y = (tmd_i32)out_vertices[idx].y + tmd_mime_fx_mul((tmd_i32)diffs[i].dy, weight_fx12);
        z = (tmd_i32)out_vertices[idx].z + tmd_mime_fx_mul((tmd_i32)diffs[i].dz, weight_fx12);
        out_vertices[idx].x = tmd_mime_clamp_i16(x);
        out_vertices[idx].y = tmd_mime_clamp_i16(y);
        out_vertices[idx].z = tmd_mime_clamp_i16(z);
    }
    return TMD_ERR_OK;
}

int tmd_mime_apply_normal_diffs(const TMD_Normal *base_normals,
                                tmd_u32 normal_count,
                                const TMD_MIMe_NormalDiff *diffs,
                                tmd_u32 diff_count,
                                tmd_i32 weight_fx12,
                                TMD_Normal *out_normals,
                                tmd_u32 out_normal_count)
{
    tmd_u32 i;
    tmd_u32 idx;
    tmd_i32 x;
    tmd_i32 y;
    tmd_i32 z;

    if (base_normals == TMD_NULL || out_normals == TMD_NULL) return TMD_ERR_NULL;
    if (diff_count > 0UL && diffs == TMD_NULL) return TMD_ERR_NULL;
    if (normal_count > out_normal_count) return TMD_ERR_OUTPUT_OVERFLOW;
    if (diff_count > TMD_MIME_MAX_DIFFS_PER_PRIMITIVE) return TMD_ERR_BAD_MIME;

    for (i = 0UL; i < normal_count; ++i) out_normals[i] = base_normals[i];

    for (i = 0UL; i < diff_count; ++i) {
        idx = (tmd_u32)diffs[i].normal_index;
        if (idx >= normal_count) return TMD_ERR_BAD_MIME;
        x = (tmd_i32)out_normals[idx].x_fx12 + tmd_mime_fx_mul((tmd_i32)diffs[i].dx_fx12, weight_fx12);
        y = (tmd_i32)out_normals[idx].y_fx12 + tmd_mime_fx_mul((tmd_i32)diffs[i].dy_fx12, weight_fx12);
        z = (tmd_i32)out_normals[idx].z_fx12 + tmd_mime_fx_mul((tmd_i32)diffs[i].dz_fx12, weight_fx12);
        out_normals[idx].x_fx12 = tmd_mime_clamp_i16(x);
        out_normals[idx].y_fx12 = tmd_mime_clamp_i16(y);
        out_normals[idx].z_fx12 = tmd_mime_clamp_i16(z);
    }
    return TMD_ERR_OK;
}

int tmd_mime_blend_vertices(const TMD_Vertex *a_vertices,
                            const TMD_Vertex *b_vertices,
                            tmd_u32 vertex_count,
                            tmd_i32 weight_fx12,
                            TMD_Vertex *out_vertices,
                            tmd_u32 out_vertex_count)
{
    tmd_u32 i;
    if (a_vertices == TMD_NULL || b_vertices == TMD_NULL || out_vertices == TMD_NULL) return TMD_ERR_NULL;
    if (vertex_count > out_vertex_count) return TMD_ERR_OUTPUT_OVERFLOW;
    for (i = 0UL; i < vertex_count; ++i) {
        out_vertices[i].x = tmd_mime_clamp_i16(tmd_mime_lerp_i32((tmd_i32)a_vertices[i].x, (tmd_i32)b_vertices[i].x, weight_fx12));
        out_vertices[i].y = tmd_mime_clamp_i16(tmd_mime_lerp_i32((tmd_i32)a_vertices[i].y, (tmd_i32)b_vertices[i].y, weight_fx12));
        out_vertices[i].z = tmd_mime_clamp_i16(tmd_mime_lerp_i32((tmd_i32)a_vertices[i].z, (tmd_i32)b_vertices[i].z, weight_fx12));
    }
    return TMD_ERR_OK;
}

int tmd_mime_blend_normals(const TMD_Normal *a_normals,
                           const TMD_Normal *b_normals,
                           tmd_u32 normal_count,
                           tmd_i32 weight_fx12,
                           TMD_Normal *out_normals,
                           tmd_u32 out_normal_count)
{
    tmd_u32 i;
    if (a_normals == TMD_NULL || b_normals == TMD_NULL || out_normals == TMD_NULL) return TMD_ERR_NULL;
    if (normal_count > out_normal_count) return TMD_ERR_OUTPUT_OVERFLOW;
    for (i = 0UL; i < normal_count; ++i) {
        out_normals[i].x_fx12 = tmd_mime_clamp_i16(tmd_mime_lerp_i32((tmd_i32)a_normals[i].x_fx12, (tmd_i32)b_normals[i].x_fx12, weight_fx12));
        out_normals[i].y_fx12 = tmd_mime_clamp_i16(tmd_mime_lerp_i32((tmd_i32)a_normals[i].y_fx12, (tmd_i32)b_normals[i].y_fx12, weight_fx12));
        out_normals[i].z_fx12 = tmd_mime_clamp_i16(tmd_mime_lerp_i32((tmd_i32)a_normals[i].z_fx12, (tmd_i32)b_normals[i].z_fx12, weight_fx12));
    }
    return TMD_ERR_OK;
}
