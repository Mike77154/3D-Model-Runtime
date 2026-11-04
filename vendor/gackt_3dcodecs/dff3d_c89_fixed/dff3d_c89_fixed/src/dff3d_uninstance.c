#include <string.h>
#include "dff3d_native.h"

#define DFF3D_NATIVE_STATUS_NONE 0UL
#define DFF3D_NATIVE_STATUS_D3D_VERTEX 1UL
#define DFF3D_NATIVE_STATUS_D3D_TRIANGLES 2UL
#define DFF3D_NATIVE_STATUS_BINMESH_TRIANGLES 4UL
#define DFF3D_NATIVE_STATUS_FAILED 0x80000000UL

static dff_u32 dff3d_raw_u16(const dff_u8 *p)
{
    dff_u32 v;
    v = (dff_u32)p[0];
    v |= (dff_u32)p[1] << 8;
    return v;
}

static dff_u32 dff3d_raw_u32(const dff_u8 *p)
{
    dff_u32 v;
    v = (dff_u32)p[0];
    v |= (dff_u32)p[1] << 8;
    v |= (dff_u32)p[2] << 16;
    v |= (dff_u32)p[3] << 24;
    return v;
}

static dff_u32 dff3d_abs_fixed_mag(dff_fixed x)
{
    if (x < 0) {
        return (dff_u32)(-(x + 1)) + 1U;
    }
    return (dff_u32)x;
}

static int dff3d_f32_bits_finite(dff_u32 bits)
{
    dff_u32 exp;
    exp = (bits >> 23) & 255U;
    return exp != 255U;
}

static int dff3d_f32_bits_sane_coord(dff_u32 bits)
{
    dff_fixed x;
    dff_u32 mag;
    if (!dff3d_f32_bits_finite(bits)) {
        return 0;
    }
    x = dff3d_fixed_from_f32_bits(bits);
    if (x == DFF3D_FIXED_MAX || x == DFF3D_FIXED_MIN) {
        return 0;
    }
    mag = dff3d_abs_fixed_mag(x);
    return mag <= (dff_u32)(32767UL << DFF3D_FIXED_SHIFT);
}

static int dff3d_f32_bits_sane_unit(dff_u32 bits)
{
    dff_fixed x;
    dff_u32 mag;
    if (!dff3d_f32_bits_finite(bits)) {
        return 0;
    }
    x = dff3d_fixed_from_f32_bits(bits);
    if (x == DFF3D_FIXED_MAX || x == DFF3D_FIXED_MIN) {
        return 0;
    }
    mag = dff3d_abs_fixed_mag(x);
    return mag <= (dff_u32)(3UL << DFF3D_FIXED_SHIFT);
}

static int dff3d_f32_bits_sane_uv(dff_u32 bits)
{
    dff_fixed x;
    dff_u32 mag;
    if (!dff3d_f32_bits_finite(bits)) {
        return 0;
    }
    x = dff3d_fixed_from_f32_bits(bits);
    if (x == DFF3D_FIXED_MAX || x == DFF3D_FIXED_MIN) {
        return 0;
    }
    mag = dff3d_abs_fixed_mag(x);
    return mag <= (dff_u32)(64UL << DFF3D_FIXED_SHIFT);
}

static int dff3d_candidate_positions(const dff_u8 *raw, dff_u32 raw_size, dff_u32 off, dff_u32 stride, dff_u32 vcount, dff_u32 *score)
{
    dff_u32 sample;
    dff_u32 i;
    dff_u32 good;
    dff_u32 nonzero;
    dff_u32 base;
    dff_u32 b0;
    dff_u32 b1;
    dff_u32 b2;
    dff_fixed x;
    dff_fixed y;
    dff_fixed z;

    sample = vcount < 12U ? vcount : 12U;
    good = 0U;
    nonzero = 0U;
    if (sample == 0U) {
        return 0;
    }
    if (off > raw_size || stride == 0U || vcount > ((raw_size - off) / stride)) {
        return 0;
    }
    for (i = 0U; i < sample; ++i) {
        base = off + stride * i;
        if (base + 12U > raw_size) {
            return 0;
        }
        b0 = dff3d_raw_u32(raw + base);
        b1 = dff3d_raw_u32(raw + base + 4U);
        b2 = dff3d_raw_u32(raw + base + 8U);
        if (dff3d_f32_bits_sane_coord(b0) && dff3d_f32_bits_sane_coord(b1) && dff3d_f32_bits_sane_coord(b2)) {
            good++;
            x = dff3d_fixed_from_f32_bits(b0);
            y = dff3d_fixed_from_f32_bits(b1);
            z = dff3d_fixed_from_f32_bits(b2);
            if (x != 0 || y != 0 || z != 0) {
                nonzero++;
            }
        }
    }
    *score = good * 4U + nonzero;
    return good == sample && nonzero != 0U;
}

static dff_u32 dff3d_score_normals(const dff_u8 *raw, dff_u32 raw_size, dff_u32 off, dff_u32 stride, dff_u32 n_off, dff_u32 vcount)
{
    dff_u32 sample;
    dff_u32 i;
    dff_u32 good;
    dff_u32 base;
    dff_u32 b0;
    dff_u32 b1;
    dff_u32 b2;
    sample = vcount < 12U ? vcount : 12U;
    good = 0U;
    if (n_off + 12U > stride) {
        return 0U;
    }
    for (i = 0U; i < sample; ++i) {
        base = off + stride * i + n_off;
        if (base + 12U > raw_size) {
            return 0U;
        }
        b0 = dff3d_raw_u32(raw + base);
        b1 = dff3d_raw_u32(raw + base + 4U);
        b2 = dff3d_raw_u32(raw + base + 8U);
        if (dff3d_f32_bits_sane_unit(b0) && dff3d_f32_bits_sane_unit(b1) && dff3d_f32_bits_sane_unit(b2)) {
            good++;
        }
    }
    return good;
}

static dff_u32 dff3d_score_uvs(const dff_u8 *raw, dff_u32 raw_size, dff_u32 off, dff_u32 stride, dff_u32 uv_off, dff_u32 vcount)
{
    dff_u32 sample;
    dff_u32 i;
    dff_u32 good;
    dff_u32 base;
    dff_u32 b0;
    dff_u32 b1;
    sample = vcount < 12U ? vcount : 12U;
    good = 0U;
    if (uv_off + 8U > stride) {
        return 0U;
    }
    for (i = 0U; i < sample; ++i) {
        base = off + stride * i + uv_off;
        if (base + 8U > raw_size) {
            return 0U;
        }
        b0 = dff3d_raw_u32(raw + base);
        b1 = dff3d_raw_u32(raw + base + 4U);
        if (dff3d_f32_bits_sane_uv(b0) && dff3d_f32_bits_sane_uv(b1)) {
            good++;
        }
    }
    return good;
}

static void dff3d_find_stream_members(const dff_u8 *raw,
                                      dff_u32 raw_size,
                                      dff_u32 off,
                                      dff_u32 stride,
                                      dff_u32 vcount,
                                      dff_u32 need_normals,
                                      dff_u32 need_uv,
                                      dff_u32 *normal_off,
                                      dff_u32 *uv_off,
                                      dff_u32 *score)
{
    dff_u32 best_norm;
    dff_u32 best_uv;
    dff_u32 best_norm_score;
    dff_u32 best_uv_score;
    dff_u32 o;
    dff_u32 s;

    best_norm = 0xffffffffUL;
    best_uv = 0xffffffffUL;
    best_norm_score = 0U;
    best_uv_score = 0U;
    if (need_normals != 0U) {
        for (o = 12U; o + 12U <= stride; o += 4U) {
            s = dff3d_score_normals(raw, raw_size, off, stride, o, vcount);
            if (s > best_norm_score) {
                best_norm_score = s;
                best_norm = o;
            }
        }
        if (best_norm_score < (vcount < 12U ? vcount : 12U)) {
            best_norm = 0xffffffffUL;
        }
    }
    if (need_uv != 0U) {
        for (o = 12U; o + 8U <= stride; o += 4U) {
            if (best_norm != 0xffffffffUL && o >= best_norm && o < best_norm + 12U) {
                continue;
            }
            s = dff3d_score_uvs(raw, raw_size, off, stride, o, vcount);
            if (s > best_uv_score) {
                best_uv_score = s;
                best_uv = o;
            }
        }
        if (best_uv_score < (vcount < 12U ? vcount : 12U)) {
            best_uv = 0xffffffffUL;
        }
    }
    *normal_off = best_norm;
    *uv_off = best_uv;
    *score += best_norm_score * 2U + best_uv_score * 2U;
}

static int dff3d_store_native_vertices(const dff_u8 *raw,
                                       dff_u32 raw_size,
                                       dff_u32 off,
                                       dff_u32 stride,
                                       dff_u32 normal_off,
                                       dff_u32 uv_off,
                                       Dff3dModel *model,
                                       Dff3dGeometry *geom,
                                       Dff3dDiag *diag)
{
    dff_u32 vcount;
    dff_u32 store;
    dff_u32 base;
    dff_u32 i;
    dff_u32 p;
    Dff3dVertex *v;

    vcount = geom->declared_vertex_count;
    store = 0U;
    if (model->vertex_count < DFF3D_MAX_VERTICES) {
        store = DFF3D_MAX_VERTICES - model->vertex_count;
        if (store > vcount) {
            store = vcount;
        }
    }
    if (store < vcount && diag != DFF3D_NULL) {
        diag->overflow_vertices += vcount - store;
    }
    if (store == 0U) {
        return 0;
    }
    geom->vertex_offset = model->vertex_count;
    geom->vertex_count = store;
    model->vertex_count += store;
    for (i = 0U; i < store; ++i) {
        v = &model->vertices[geom->vertex_offset + i];
        memset(v, 0, sizeof(*v));
        v->color.r = 255U;
        v->color.g = 255U;
        v->color.b = 255U;
        v->color.a = 255U;
        base = off + stride * i;
        v->pos.x = dff3d_fixed_from_f32_bits(dff3d_raw_u32(raw + base));
        v->pos.y = dff3d_fixed_from_f32_bits(dff3d_raw_u32(raw + base + 4U));
        v->pos.z = dff3d_fixed_from_f32_bits(dff3d_raw_u32(raw + base + 8U));
        v->has_pos = 1U;
        if (normal_off != 0xffffffffUL && base + normal_off + 12U <= raw_size) {
            p = base + normal_off;
            v->normal.x = dff3d_fixed_from_f32_bits(dff3d_raw_u32(raw + p));
            v->normal.y = dff3d_fixed_from_f32_bits(dff3d_raw_u32(raw + p + 4U));
            v->normal.z = dff3d_fixed_from_f32_bits(dff3d_raw_u32(raw + p + 8U));
            v->has_normal = 1U;
        }
        if (uv_off != 0xffffffffUL && base + uv_off + 8U <= raw_size) {
            p = base + uv_off;
            v->uv0.u = dff3d_fixed_from_f32_bits(dff3d_raw_u32(raw + p));
            v->uv0.v = dff3d_fixed_from_f32_bits(dff3d_raw_u32(raw + p + 4U));
            v->has_uv0 = 1U;
        }
    }
    geom->has_positions = 1U;
    if (normal_off != 0xffffffffUL) {
        geom->has_normals = 1U;
    }
    return 1;
}

static int dff3d_indices_u16_good(const dff_u8 *raw, dff_u32 raw_size, dff_u32 off, dff_u32 need, dff_u32 vcount, dff_u32 *score)
{
    dff_u32 i;
    dff_u32 good;
    dff_u32 nonzero;
    dff_u32 idx;
    good = 0U;
    nonzero = 0U;
    if (off > raw_size || need > ((raw_size - off) / 2U)) {
        return 0;
    }
    for (i = 0U; i < need; ++i) {
        idx = dff3d_raw_u16(raw + off + i * 2U);
        if (idx < vcount) {
            good++;
            if (idx != 0U) {
                nonzero++;
            }
        }
    }
    *score = good + nonzero;
    return good == need && nonzero != 0U;
}

static int dff3d_indices_u32_good(const dff_u8 *raw, dff_u32 raw_size, dff_u32 off, dff_u32 need, dff_u32 vcount, dff_u32 *score)
{
    dff_u32 i;
    dff_u32 good;
    dff_u32 nonzero;
    dff_u32 idx;
    good = 0U;
    nonzero = 0U;
    if (off > raw_size || need > ((raw_size - off) / 4U)) {
        return 0;
    }
    for (i = 0U; i < need; ++i) {
        idx = dff3d_raw_u32(raw + off + i * 4U);
        if (idx < vcount) {
            good++;
            if (idx != 0U) {
                nonzero++;
            }
        }
    }
    *score = good + nonzero;
    return good == need && nonzero != 0U;
}

static dff_u32 dff3d_make_triangle(Dff3dModel *model, Dff3dGeometry *geom, dff_u32 a, dff_u32 b, dff_u32 c, dff_u32 mat, Dff3dDiag *diag)
{
    Dff3dTriangle *t;
    if (a == b || b == c || a == c) {
        return 0U;
    }
    if (a >= geom->vertex_count || b >= geom->vertex_count || c >= geom->vertex_count) {
        return 0U;
    }
    if (model->triangle_count >= DFF3D_MAX_TRIANGLES) {
        if (diag != DFF3D_NULL) {
            diag->overflow_triangles++;
        }
        return 0U;
    }
    if (geom->triangle_count == 0U) {
        geom->triangle_offset = model->triangle_count;
    }
    t = &model->triangles[model->triangle_count];
    model->triangle_count++;
    t->a = (dff_u16)a;
    t->b = (dff_u16)b;
    t->c = (dff_u16)c;
    t->material = (dff_u16)(geom->material_offset + mat);
    t->geometry = (dff_u32)(geom - model->geometries);
    geom->triangle_count++;
    return 1U;
}

static int dff3d_store_triangles_from_indices(const dff_u8 *raw,
                                              dff_u32 raw_size,
                                              dff_u32 off,
                                              dff_u32 index_size,
                                              Dff3dModel *model,
                                              Dff3dGeometry *geom,
                                              Dff3dDiag *diag,
                                              Dff3dNativeInfo *info)
{
    dff_u32 need;
    dff_u32 i;
    dff_u32 a;
    dff_u32 b;
    dff_u32 c;
    dff_u32 made;

    if (geom->declared_triangle_count == 0U) {
        return 0;
    }
    need = geom->declared_triangle_count * 3U;
    if (index_size == 2U && need > ((raw_size - off) / 2U)) {
        return 0;
    }
    if (index_size == 4U && need > ((raw_size - off) / 4U)) {
        return 0;
    }
    made = 0U;
    for (i = 0U; i < geom->declared_triangle_count; ++i) {
        if (index_size == 2U) {
            a = dff3d_raw_u16(raw + off + (i * 3U + 0U) * 2U);
            b = dff3d_raw_u16(raw + off + (i * 3U + 1U) * 2U);
            c = dff3d_raw_u16(raw + off + (i * 3U + 2U) * 2U);
        } else {
            a = dff3d_raw_u32(raw + off + (i * 3U + 0U) * 4U);
            b = dff3d_raw_u32(raw + off + (i * 3U + 1U) * 4U);
            c = dff3d_raw_u32(raw + off + (i * 3U + 2U) * 4U);
        }
        made += dff3d_make_triangle(model, geom, a, b, c, 0U, diag);
    }
    if (made != 0U && info != DFF3D_NULL) {
        info->index_offset_bytes = off;
        info->index_size_bytes = index_size;
        info->index_count = need;
        info->triangles_rebuilt += made;
        info->uninstance_status |= DFF3D_NATIVE_STATUS_D3D_TRIANGLES;
    }
    return made != 0U;
}

static int dff3d_find_and_store_indices(const dff_u8 *raw,
                                        dff_u32 raw_size,
                                        dff_u32 start,
                                        Dff3dModel *model,
                                        Dff3dGeometry *geom,
                                        Dff3dDiag *diag,
                                        Dff3dNativeInfo *info)
{
    dff_u32 need;
    dff_u32 off;
    dff_u32 score;
    dff_u32 best_off;
    dff_u32 best_size;
    dff_u32 best_score;
    dff_u32 limit;

    need = geom->declared_triangle_count * 3U;
    if (need == 0U || geom->vertex_count == 0U) {
        return 0;
    }
    best_off = 0U;
    best_size = 0U;
    best_score = 0U;
    limit = raw_size;
    if (start + 65536U < limit) {
        limit = start + 65536U;
    }
    off = start;
    while (off + need * 2U <= limit) {
        score = 0U;
        if (dff3d_indices_u16_good(raw, raw_size, off, need, geom->vertex_count, &score)) {
            if (score > best_score) {
                best_score = score;
                best_off = off;
                best_size = 2U;
            }
        }
        off += 2U;
    }
    off = start;
    while (off + need * 4U <= limit) {
        score = 0U;
        if (dff3d_indices_u32_good(raw, raw_size, off, need, geom->vertex_count, &score)) {
            if (score > best_score) {
                best_score = score;
                best_off = off;
                best_size = 4U;
            }
        }
        off += 4U;
    }
    if (best_size == 0U) {
        return 0;
    }
    return dff3d_store_triangles_from_indices(raw, raw_size, best_off, best_size, model, geom, diag, info);
}

int dff3d_probe_native_d3d(const dff_u8 *raw,
                           dff_u32 raw_size,
                           Dff3dModel *model,
                           Dff3dGeometry *geom,
                           Dff3dNativeInfo *info,
                           Dff3dDiag *diag)
{
    dff_u32 platform;
    dff_u32 vcount;
    dff_u32 min_stride;
    dff_u32 off;
    dff_u32 stride;
    dff_u32 score;
    dff_u32 best_off;
    dff_u32 best_stride;
    dff_u32 best_score;
    dff_u32 best_norm;
    dff_u32 best_uv;
    dff_u32 norm;
    dff_u32 uv;
    dff_u32 need_normals;
    dff_u32 need_uv;
    dff_u32 scan_limit;

    if (raw == DFF3D_NULL || model == DFF3D_NULL || geom == DFF3D_NULL || raw_size < 16U) {
        return 0;
    }
    platform = dff3d_raw_u32(raw);
    if (platform != DFF3D_NATIVE_D3D8 && platform != DFF3D_NATIVE_D3D9) {
        return 0;
    }
    if (diag != DFF3D_NULL) {
        diag->native_d3d_probe++;
    }
    vcount = geom->declared_vertex_count;
    if (vcount == 0U || vcount > DFF3D_MAX_VERTICES) {
        if (diag != DFF3D_NULL) {
            diag->native_probe_fail++;
        }
        return 0;
    }
    need_normals = (geom->flags & DFF3D_GEOM_NORMALS) != 0UL ? 1U : 0U;
    need_uv = geom->uv_layers != 0U ? 1U : 0U;
    min_stride = 12U;
    if (need_normals != 0U) {
        min_stride += 12U;
    }
    if (need_uv != 0U) {
        min_stride += 8U;
    }
    if ((geom->flags & DFF3D_GEOM_PRELIT) != 0UL) {
        min_stride += 4U;
    }
    if (min_stride < 12U) {
        min_stride = 12U;
    }

    best_off = 0U;
    best_stride = 0U;
    best_score = 0U;
    best_norm = 0xffffffffUL;
    best_uv = 0xffffffffUL;
    scan_limit = raw_size;
    if (scan_limit > 262144U) {
        scan_limit = 262144U;
    }
    off = 4U;
    while (off + min_stride * vcount <= scan_limit) {
        stride = min_stride;
        while (stride <= 96U && off + stride * vcount <= raw_size) {
            score = 0U;
            if (dff3d_candidate_positions(raw, raw_size, off, stride, vcount, &score)) {
                norm = 0xffffffffUL;
                uv = 0xffffffffUL;
                dff3d_find_stream_members(raw, raw_size, off, stride, vcount, need_normals, need_uv, &norm, &uv, &score);
                if (score > best_score) {
                    best_score = score;
                    best_off = off;
                    best_stride = stride;
                    best_norm = norm;
                    best_uv = uv;
                }
            }
            stride += 4U;
        }
        off += 4U;
    }
    if (best_stride == 0U) {
        if (diag != DFF3D_NULL) {
            diag->native_probe_fail++;
        }
        if (info != DFF3D_NULL) {
            info->uninstance_status |= DFF3D_NATIVE_STATUS_FAILED;
        }
        return 0;
    }
    if (!dff3d_store_native_vertices(raw, raw_size, best_off, best_stride, best_norm, best_uv, model, geom, diag)) {
        if (diag != DFF3D_NULL) {
            diag->native_probe_fail++;
        }
        return 0;
    }
    if (info != DFF3D_NULL) {
        info->vertex_offset_bytes = best_off;
        info->vertex_stride = best_stride;
        info->vertex_count = geom->vertex_count;
        info->candidate_score = best_score;
        info->uninstance_status |= DFF3D_NATIVE_STATUS_D3D_VERTEX;
    }
    if (diag != DFF3D_NULL) {
        diag->native_uninstanced++;
    }
    (void)dff3d_find_and_store_indices(raw, raw_size, best_off + best_stride * vcount, model, geom, diag, info);
    return 1;
}

static dff_u32 dff3d_bin_index_at(const Dff3dModel *model, const Dff3dBinMesh *bm, dff_u32 n)
{
    if (bm->index_offset + n >= model->bin_index_count) {
        return 0xffffffffUL;
    }
    return model->bin_indices[bm->index_offset + n];
}

static dff_u32 dff3d_rebuild_geom_from_binmesh(Dff3dModel *model, Dff3dGeometry *geom, Dff3dDiag *diag)
{
    dff_u32 i;
    dff_u32 j;
    dff_u32 made;
    Dff3dBinMesh *bm;
    dff_u32 a;
    dff_u32 b;
    dff_u32 c;
    dff_u32 tmp;

    if (model == DFF3D_NULL || geom == DFF3D_NULL) {
        return 0U;
    }
    if (geom->triangle_count != 0U || geom->vertex_count == 0U || geom->bin_mesh_count == 0U) {
        return 0U;
    }
    made = 0U;
    for (i = 0U; i < geom->bin_mesh_count; ++i) {
        if (geom->bin_mesh_offset + i >= model->bin_mesh_count) {
            break;
        }
        bm = &model->bin_meshes[geom->bin_mesh_offset + i];
        if (bm->is_tristrip != 0U) {
            for (j = 2U; j < bm->index_count; ++j) {
                a = dff3d_bin_index_at(model, bm, j - 2U);
                b = dff3d_bin_index_at(model, bm, j - 1U);
                c = dff3d_bin_index_at(model, bm, j);
                if (a == 0xffffffffUL || b == 0xffffffffUL || c == 0xffffffffUL) {
                    break;
                }
                if ((j & 1U) != 0U) {
                    tmp = a;
                    a = b;
                    b = tmp;
                }
                made += dff3d_make_triangle(model, geom, a, b, c, bm->material, diag);
            }
        } else {
            for (j = 0U; j + 2U < bm->index_count; j += 3U) {
                a = dff3d_bin_index_at(model, bm, j);
                b = dff3d_bin_index_at(model, bm, j + 1U);
                c = dff3d_bin_index_at(model, bm, j + 2U);
                if (a == 0xffffffffUL || b == 0xffffffffUL || c == 0xffffffffUL) {
                    break;
                }
                made += dff3d_make_triangle(model, geom, a, b, c, bm->material, diag);
            }
        }
    }
    if (made != 0U && diag != DFF3D_NULL) {
        diag->binmesh_tris_rebuilt += made;
    }
    return made;
}

int dff3d_uninstance_model(Dff3dModel *model, Dff3dDiag *diag)
{
    dff_u32 i;
    dff_u32 made;
    Dff3dGeometry *geom;

    if (model == DFF3D_NULL) {
        return DFF3D_ERR_NULL;
    }
    made = 0U;
    for (i = 0U; i < model->geometry_count; ++i) {
        geom = &model->geometries[i];
        made += dff3d_rebuild_geom_from_binmesh(model, geom, diag);
    }
    return made != 0U ? DFF3D_OK : DFF3D_ERR_NOT_DFF;
}
