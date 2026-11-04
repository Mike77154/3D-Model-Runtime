#include "dff3d_native_common.h"

#define DFF3D_D3DDECLTYPE_FLOAT1 0U
#define DFF3D_D3DDECLTYPE_FLOAT2 1U
#define DFF3D_D3DDECLTYPE_FLOAT3 2U
#define DFF3D_D3DDECLTYPE_FLOAT4 3U
#define DFF3D_D3DDECLTYPE_D3DCOLOR 4U
#define DFF3D_D3DDECLTYPE_UNUSED 17U
#define DFF3D_D3DDECLUSAGE_POSITION 0U
#define DFF3D_D3DDECLUSAGE_NORMAL 3U
#define DFF3D_D3DDECLUSAGE_TEXCOORD 5U
#define DFF3D_D3DDECLUSAGE_COLOR 10U
#define DFF3D_D3DFVF_XYZ 0x00000002UL
#define DFF3D_D3DFVF_NORMAL 0x00000010UL
#define DFF3D_D3DFVF_DIFFUSE 0x00000040UL
#define DFF3D_D3DFVF_SPECULAR 0x00000080UL
#define DFF3D_D3DFVF_TEXCOUNT_MASK 0x00000f00UL
#define DFF3D_D3DFVF_TEXCOUNT_SHIFT 8U

typedef struct Dff3dD3dDeclGuessTag {
    dff_u32 decl_offset;
    dff_u32 decl_end;
    dff_u32 pos_off;
    dff_u32 normal_off;
    dff_u32 uv_off;
    dff_u32 color_off;
    dff_u32 stride;
    dff_u32 score;
} Dff3dD3dDeclGuess;

static dff_u32 dff3d_decl_type_size(dff_u32 type)
{
    switch (type) {
    case DFF3D_D3DDECLTYPE_FLOAT1: return 4U;
    case DFF3D_D3DDECLTYPE_FLOAT2: return 8U;
    case DFF3D_D3DDECLTYPE_FLOAT3: return 12U;
    case DFF3D_D3DDECLTYPE_FLOAT4: return 16U;
    case DFF3D_D3DDECLTYPE_D3DCOLOR: return 4U;
    default: break;
    }
    return 0U;
}

static int dff3d_decl_good_platform(dff_u32 platform)
{
    return platform == DFF3D_NATIVE_D3D8 || platform == DFF3D_NATIVE_D3D9 || platform == DFF3D_NATIVE_XBOX;
}

static dff_u32 dff3d_round4(dff_u32 x)
{
    return (x + 3U) & ~3U;
}

static int dff3d_try_decl_at(const dff_u8 *raw, dff_u32 raw_size, dff_u32 off, Dff3dD3dDeclGuess *g)
{
    dff_u32 n;
    dff_u32 p;
    dff_u32 stream;
    dff_u32 offset;
    dff_u32 type;
    dff_u32 method;
    dff_u32 usage;
    dff_u32 usage_index;
    dff_u32 sz;
    dff_u32 max_end;
    dff_u32 elems;

    if (g == DFF3D_NULL || off + 16U > raw_size) {
        return 0;
    }
    g->decl_offset = off;
    g->decl_end = 0U;
    g->pos_off = 0xffffffffUL;
    g->normal_off = 0xffffffffUL;
    g->uv_off = 0xffffffffUL;
    g->color_off = 0xffffffffUL;
    g->stride = 0U;
    g->score = 0U;
    max_end = 0U;
    elems = 0U;
    p = off;
    for (n = 0U; n < 16U; ++n) {
        if (p + 8U > raw_size) {
            return 0;
        }
        stream = dff3d_native_raw_u16(raw + p);
        offset = dff3d_native_raw_u16(raw + p + 2U);
        type = raw[p + 4U];
        method = raw[p + 5U];
        usage = raw[p + 6U];
        usage_index = raw[p + 7U];
        if (stream == 0xffU && offset == 0U && type == DFF3D_D3DDECLTYPE_UNUSED) {
            g->decl_end = p + 8U;
            break;
        }
        if (stream != 0U || method != 0U || usage_index > 4U) {
            return 0;
        }
        sz = dff3d_decl_type_size(type);
        if (sz == 0U || offset > 128U || offset + sz > 160U) {
            return 0;
        }
        if (offset + sz > max_end) {
            max_end = offset + sz;
        }
        if (usage == DFF3D_D3DDECLUSAGE_POSITION && type == DFF3D_D3DDECLTYPE_FLOAT3 && usage_index == 0U) {
            g->pos_off = offset;
            g->score += 20U;
        } else if (usage == DFF3D_D3DDECLUSAGE_NORMAL && type == DFF3D_D3DDECLTYPE_FLOAT3 && usage_index == 0U) {
            g->normal_off = offset;
            g->score += 8U;
        } else if (usage == DFF3D_D3DDECLUSAGE_TEXCOORD && type == DFF3D_D3DDECLTYPE_FLOAT2 && usage_index == 0U) {
            g->uv_off = offset;
            g->score += 8U;
        } else if (usage == DFF3D_D3DDECLUSAGE_COLOR && type == DFF3D_D3DDECLTYPE_D3DCOLOR) {
            g->color_off = offset;
            g->score += 2U;
        }
        elems++;
        p += 8U;
    }
    if (g->decl_end == 0U || elems == 0U || g->pos_off == 0xffffffffUL) {
        return 0;
    }
    g->stride = dff3d_round4(max_end);
    if (g->stride < 12U || g->stride > 160U) {
        return 0;
    }
    if (g->normal_off != 0xffffffffUL && g->normal_off + 12U > g->stride) {
        return 0;
    }
    if (g->uv_off != 0xffffffffUL && g->uv_off + 8U > g->stride) {
        return 0;
    }
    return 1;
}

static int dff3d_store_decl_vbo(const dff_u8 *raw,
                                dff_u32 raw_size,
                                const Dff3dD3dDeclGuess *g,
                                Dff3dModel *model,
                                Dff3dGeometry *geom,
                                Dff3dNativeInfo *info,
                                Dff3dDiag *diag)
{
    dff_u32 off;
    dff_u32 limit;
    dff_u32 score;
    dff_u32 best_off;
    dff_u32 best_score;
    dff_u32 vcount;

    if (g == DFF3D_NULL || geom == DFF3D_NULL) {
        return 0;
    }
    vcount = geom->declared_vertex_count;
    if (vcount == 0U || g->stride == 0U || g->pos_off + 12U > g->stride) {
        return 0;
    }
    best_off = 0U;
    best_score = 0U;
    off = g->decl_end;
    limit = raw_size;
    if (off + 65536U < limit && off + 65536U > off) {
        limit = off + 65536U;
    }
    while (off + g->stride * vcount <= limit) {
        score = 0U;
        if (dff3d_native_candidate_positions(raw, raw_size, off, g->stride, g->pos_off, vcount, &score)) {
            if (score + g->score > best_score) {
                best_score = score + g->score;
                best_off = off;
            }
        }
        off += 4U;
    }
    if (best_score == 0U) {
        return 0;
    }
    if (!dff3d_native_store_vertices_f32_ex(raw, raw_size, best_off, g->stride, g->pos_off, g->normal_off, g->uv_off, g->color_off, model, geom, diag)) {
        return 0;
    }
    if (info != DFF3D_NULL) {
        info->vertex_offset_bytes = best_off;
        info->vertex_stride = g->stride;
        info->vertex_count = geom->vertex_count;
        info->candidate_score = best_score;
        info->uninstance_status |= DFF3D_NATIVE_STATUS_D3D_DECL | DFF3D_NATIVE_STATUS_D3D_VERTEX;
    }
    if (diag != DFF3D_NULL) {
        diag->native_uninstanced++;
    }
    (void)dff3d_native_find_and_store_indices(raw, raw_size, best_off + g->stride * vcount, model, geom, diag, info, DFF3D_NATIVE_STATUS_D3D_DECL);
    return 1;
}

static dff_u32 dff3d_fvf_tex_count(dff_u32 fvf)
{
    return (fvf & DFF3D_D3DFVF_TEXCOUNT_MASK) >> DFF3D_D3DFVF_TEXCOUNT_SHIFT;
}

static int dff3d_fvf_guess(dff_u32 fvf, Dff3dD3dDeclGuess *g)
{
    dff_u32 pos;
    dff_u32 tex;

    if (g == DFF3D_NULL) {
        return 0;
    }
    if ((fvf & DFF3D_D3DFVF_XYZ) == 0U) {
        return 0;
    }
    tex = dff3d_fvf_tex_count(fvf);
    if (tex > 2U) {
        return 0;
    }
    g->pos_off = 0U;
    g->normal_off = 0xffffffffUL;
    g->uv_off = 0xffffffffUL;
    g->color_off = 0xffffffffUL;
    g->stride = 12U;
    g->score = 10U;
    pos = 12U;
    if ((fvf & DFF3D_D3DFVF_NORMAL) != 0U) {
        g->normal_off = pos;
        pos += 12U;
        g->score += 4U;
    }
    if ((fvf & DFF3D_D3DFVF_DIFFUSE) != 0U) {
        g->color_off = pos;
        pos += 4U;
    }
    if ((fvf & DFF3D_D3DFVF_SPECULAR) != 0U) {
        pos += 4U;
    }
    if (tex > 0U) {
        g->uv_off = pos;
        pos += 8U * tex;
        g->score += 4U;
    }
    g->stride = dff3d_round4(pos);
    return g->stride >= 12U && g->stride <= 128U;
}

static int dff3d_try_fvf_scan(const dff_u8 *raw,
                              dff_u32 raw_size,
                              Dff3dModel *model,
                              Dff3dGeometry *geom,
                              Dff3dNativeInfo *info,
                              Dff3dDiag *diag)
{
    dff_u32 off;
    dff_u32 fvf;
    Dff3dD3dDeclGuess g;

    off = 4U;
    while (off + 4U <= raw_size && off < 512U) {
        fvf = dff3d_native_raw_u32(raw + off);
        if (dff3d_fvf_guess(fvf, &g)) {
            g.decl_offset = off;
            g.decl_end = off + 4U;
            if (dff3d_store_decl_vbo(raw, raw_size, &g, model, geom, info, diag)) {
                return 1;
            }
        }
        off += 4U;
    }
    return 0;
}

int dff3d_probe_native_d3d_decl(const dff_u8 *raw,
                                dff_u32 raw_size,
                                Dff3dModel *model,
                                Dff3dGeometry *geom,
                                Dff3dNativeInfo *info,
                                Dff3dDiag *diag)
{
    dff_u32 platform;
    dff_u32 off;
    Dff3dD3dDeclGuess g;

    if (raw == DFF3D_NULL || raw_size < 12U || geom == DFF3D_NULL || model == DFF3D_NULL) {
        return 0;
    }
    platform = dff3d_native_raw_u32(raw);
    if (!dff3d_decl_good_platform(platform)) {
        return 0;
    }
    if (diag != DFF3D_NULL) {
        diag->native_d3d_decl_probe++;
    }
    off = 4U;
    while (off + 16U <= raw_size && off < 2048U) {
        if (dff3d_try_decl_at(raw, raw_size, off, &g)) {
            if (dff3d_store_decl_vbo(raw, raw_size, &g, model, geom, info, diag)) {
                return 1;
            }
        }
        off += 4U;
    }
    return dff3d_try_fvf_scan(raw, raw_size, model, geom, info, diag);
}
