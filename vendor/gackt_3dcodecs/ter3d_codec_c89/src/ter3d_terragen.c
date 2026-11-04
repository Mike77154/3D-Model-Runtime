#include "ter3d.h"

#define TER3D_WARN_UNKNOWN_CHUNK 0x00000001UL
#define TER3D_WARN_MISSING_EOF   0x00000002UL
#define TER3D_WARN_BAD_CURVE     0x00000004UL
#define TER3D_WARN_SATURATED     0x00000008UL
#define TER3D_S32_MAX ((ter3d_s32)2147483647L)
#define TER3D_S32_MIN ((ter3d_s32)(-2147483647L - 1L))

static int ter3d_marker_eq(const ter3d_u8 *p, const char *s)
{
    return p[0] == (ter3d_u8)s[0] && p[1] == (ter3d_u8)s[1] && p[2] == (ter3d_u8)s[2] && p[3] == (ter3d_u8)s[3];
}

static ter3d_chunk_id ter3d_chunk_id_from_marker(const ter3d_u8 *p)
{
    if (ter3d_marker_eq(p, "SIZE")) return TER3D_CHUNK_SIZE;
    if (ter3d_marker_eq(p, "XPTS")) return TER3D_CHUNK_XPTS;
    if (ter3d_marker_eq(p, "YPTS")) return TER3D_CHUNK_YPTS;
    if (ter3d_marker_eq(p, "SCAL")) return TER3D_CHUNK_SCAL;
    if (ter3d_marker_eq(p, "CRAD")) return TER3D_CHUNK_CRAD;
    if (ter3d_marker_eq(p, "CRVM")) return TER3D_CHUNK_CRVM;
    if (ter3d_marker_eq(p, "ALTW")) return TER3D_CHUNK_ALTW;
    if (ter3d_marker_eq(p, "EOF ")) return TER3D_CHUNK_EOF;
    return TER3D_CHUNK_UNKNOWN;
}

static ter3d_u32 ter3d_align4(ter3d_u32 v)
{
    return (v + 3UL) & ~3UL;
}

static int ter3d_mul_u32_over(ter3d_u32 a, ter3d_u32 b, ter3d_u32 *out)
{
    if (a != 0UL && b > (0xffffffffUL / a)) return 1;
    *out = a * b;
    return 0;
}

static void ter3d_zero_terragen_info(ter3d_terragen_info *info)
{
    ter3d_u16 i;
    info->size_value = 0U;
    info->xpts = 0U;
    info->ypts = 0U;
    info->has_size = 0;
    info->has_xpts = 0;
    info->has_ypts = 0;
    info->has_scal = 0;
    info->has_crad = 0;
    info->has_crvm = 0;
    info->has_altw = 0;
    info->has_eof = 0;
    info->scale_x_q16 = TER3D_DEFAULT_TERRAGEN_SCALE_Q16;
    info->scale_y_q16 = TER3D_DEFAULT_TERRAGEN_SCALE_Q16;
    info->scale_z_q16 = TER3D_DEFAULT_TERRAGEN_SCALE_Q16;
    info->crad_km_q16 = ter3d_fixed_from_int(6370L);
    info->curve_mode = 0UL;
    info->height_scale = 0;
    info->base_height = 0;
    info->altw_offset = 0UL;
    info->elevations_offset = 0UL;
    info->elevation_count = 0UL;
    info->eof_offset = 0UL;
    info->chunk_count = 0U;
    info->warning_flags = 0UL;
    for (i = 0U; i < TER3D_MAX_CHUNKS; ++i) {
        info->chunks[i].marker[0] = 0;
        info->chunks[i].marker[1] = 0;
        info->chunks[i].marker[2] = 0;
        info->chunks[i].marker[3] = 0;
        info->chunks[i].id = TER3D_CHUNK_UNKNOWN;
        info->chunks[i].offset = 0UL;
        info->chunks[i].data_offset = 0UL;
        info->chunks[i].data_size = 0UL;
    }
}

static ter3d_result ter3d_add_chunk(ter3d_terragen_info *info, const ter3d_u8 *data, ter3d_u32 off, ter3d_chunk_id id, ter3d_u32 data_size)
{
    ter3d_chunk_rec *rec;
    if (info->chunk_count >= TER3D_MAX_CHUNKS) return TER3D_E_OVERFLOW;
    rec = &info->chunks[info->chunk_count];
    rec->marker[0] = (char)data[off + 0UL];
    rec->marker[1] = (char)data[off + 1UL];
    rec->marker[2] = (char)data[off + 2UL];
    rec->marker[3] = (char)data[off + 3UL];
    rec->id = id;
    rec->offset = off;
    rec->data_offset = off + 4UL;
    rec->data_size = data_size;
    info->chunk_count = (ter3d_u16)(info->chunk_count + 1U);
    return TER3D_OK;
}

static ter3d_u32 ter3d_find_next_known_marker(const ter3d_u8 *data, ter3d_u32 size, ter3d_u32 start)
{
    ter3d_u32 off;
    off = ter3d_align4(start);
    while (off + 4UL <= size) {
        if (ter3d_chunk_id_from_marker(data + off) != TER3D_CHUNK_UNKNOWN) return off;
        off += 4UL;
    }
    return size;
}

ter3d_result ter3d_terragen_read_info(const ter3d_u8 *data, ter3d_u32 size, ter3d_terragen_info *out_info)
{
    ter3d_u32 off;
    ter3d_u32 points;
    ter3d_u32 bytes;
    ter3d_result ar;

    if (data == 0 || out_info == 0) return TER3D_E_NULL;
    points = 0UL;
    bytes = 0UL;
    ter3d_zero_terragen_info(out_info);

    if (size < 20UL) return TER3D_E_NEED_MORE;
    if (!(data[0] == 'T' && data[1] == 'E' && data[2] == 'R' && data[3] == 'R' &&
          data[4] == 'A' && data[5] == 'G' && data[6] == 'E' && data[7] == 'N' &&
          data[8] == 'T' && data[9] == 'E' && data[10] == 'R' && data[11] == 'R' &&
          data[12] == 'A' && data[13] == 'I' && data[14] == 'N' && data[15] == ' ')) {
        return TER3D_E_FORMAT;
    }

    off = 16UL;
    while (off + 4UL <= size) {
        ter3d_chunk_id id;
        ter3d_u32 need;
        ter3d_u32 data_size;
        id = ter3d_chunk_id_from_marker(data + off);
        if (id == TER3D_CHUNK_UNKNOWN) {
            ter3d_u32 next;
            out_info->warning_flags |= TER3D_WARN_UNKNOWN_CHUNK;
            next = ter3d_find_next_known_marker(data, size, off + 4UL);
            data_size = (next >= off + 4UL) ? (next - off - 4UL) : 0UL;
            ar = ter3d_add_chunk(out_info, data, off, id, data_size);
            if (ar != TER3D_OK) return ar;
            off = next;
            continue;
        }

        if (id == TER3D_CHUNK_EOF) {
            out_info->has_eof = 1;
            out_info->eof_offset = off;
            ar = ter3d_add_chunk(out_info, data, off, id, 0UL);
            if (ar != TER3D_OK) return ar;
            return out_info->has_altw ? TER3D_OK : TER3D_E_NOT_FOUND;
        }

        need = 0UL;
        if (id == TER3D_CHUNK_SIZE || id == TER3D_CHUNK_XPTS || id == TER3D_CHUNK_YPTS) need = 4UL;
        else if (id == TER3D_CHUNK_SCAL) need = 12UL;
        else if (id == TER3D_CHUNK_CRAD || id == TER3D_CHUNK_CRVM) need = 4UL;
        else if (id == TER3D_CHUNK_ALTW) {
            if (!out_info->has_size) return TER3D_E_BAD_ORDER;
            if (!out_info->has_xpts) out_info->xpts = (ter3d_u16)(out_info->size_value + 1U);
            if (!out_info->has_ypts) out_info->ypts = (ter3d_u16)(out_info->size_value + 1U);
            if (out_info->xpts == 0U || out_info->ypts == 0U) return TER3D_E_FORMAT;
            if (ter3d_mul_u32_over((ter3d_u32)out_info->xpts, (ter3d_u32)out_info->ypts, &points)) return TER3D_E_OVERFLOW;
            if (ter3d_mul_u32_over(points, 2UL, &bytes)) return TER3D_E_OVERFLOW;
            need = 4UL + bytes;
        }

        if (off + 4UL + need > size) return TER3D_E_NEED_MORE;
        ar = ter3d_add_chunk(out_info, data, off, id, need);
        if (ar != TER3D_OK) return ar;

        if (id == TER3D_CHUNK_SIZE) {
            out_info->size_value = ter3d_read_le16u(data + off + 4UL);
            out_info->has_size = 1;
            if (!out_info->has_xpts) out_info->xpts = (ter3d_u16)(out_info->size_value + 1U);
            if (!out_info->has_ypts) out_info->ypts = (ter3d_u16)(out_info->size_value + 1U);
        } else if (id == TER3D_CHUNK_XPTS) {
            if (!out_info->has_size) return TER3D_E_BAD_ORDER;
            out_info->xpts = ter3d_read_le16u(data + off + 4UL);
            out_info->has_xpts = 1;
        } else if (id == TER3D_CHUNK_YPTS) {
            if (!out_info->has_size) return TER3D_E_BAD_ORDER;
            out_info->ypts = ter3d_read_le16u(data + off + 4UL);
            out_info->has_ypts = 1;
        } else if (id == TER3D_CHUNK_SCAL) {
            out_info->scale_x_q16 = ter3d_ieee754_le_to_q16(data + off + 4UL);
            out_info->scale_y_q16 = ter3d_ieee754_le_to_q16(data + off + 8UL);
            out_info->scale_z_q16 = ter3d_ieee754_le_to_q16(data + off + 12UL);
            out_info->has_scal = 1;
        } else if (id == TER3D_CHUNK_CRAD) {
            out_info->crad_km_q16 = ter3d_ieee754_le_to_q16(data + off + 4UL);
            out_info->has_crad = 1;
        } else if (id == TER3D_CHUNK_CRVM) {
            out_info->curve_mode = ter3d_read_le32u(data + off + 4UL);
            out_info->has_crvm = 1;
            if (out_info->curve_mode > 1UL) out_info->warning_flags |= TER3D_WARN_BAD_CURVE;
        } else if (id == TER3D_CHUNK_ALTW) {
            out_info->height_scale = ter3d_read_le16s(data + off + 4UL);
            out_info->base_height = ter3d_read_le16s(data + off + 6UL);
            out_info->altw_offset = off;
            out_info->elevations_offset = off + 8UL;
            out_info->elevation_count = points;
            out_info->has_altw = 1;
        }
        off = ter3d_align4(off + 4UL + need);
    }

    if (!out_info->has_altw) return TER3D_E_NOT_FOUND;
    out_info->warning_flags |= TER3D_WARN_MISSING_EOF;
    return TER3D_OK;
}

ter3d_result ter3d_terragen_raw_elevation(const ter3d_u8 *data, ter3d_u32 size, const ter3d_terragen_info *info, ter3d_u16 x, ter3d_u16 y, ter3d_s16 *out_elevation)
{
    ter3d_u32 idx;
    ter3d_u32 off;
    if (data == 0 || info == 0 || out_elevation == 0) return TER3D_E_NULL;
    if (!info->has_altw) return TER3D_E_FORMAT;
    if (x >= info->xpts || y >= info->ypts) return TER3D_E_RANGE;
    idx = (ter3d_u32)y * (ter3d_u32)info->xpts + (ter3d_u32)x;
    off = info->elevations_offset + idx * 2UL;
    if (off + 2UL > size) return TER3D_E_NEED_MORE;
    *out_elevation = ter3d_read_le16s(data + off);
    return TER3D_OK;
}

ter3d_result ter3d_terragen_get_height_q16(const ter3d_u8 *data, ter3d_u32 size, const ter3d_terragen_info *info, ter3d_u16 x, ter3d_u16 y, ter3d_fixed *out_height_q16)
{
    ter3d_s16 elev;
    ter3d_result r;
    ter3d_fixed base_q16;
    ter3d_fixed delta_q16;
    if (out_height_q16 == 0) return TER3D_E_NULL;
    r = ter3d_terragen_raw_elevation(data, size, info, x, y, &elev);
    if (r != TER3D_OK) return r;
    base_q16 = (ter3d_fixed)((ter3d_s32)info->base_height << TER3D_Q);
    delta_q16 = (ter3d_fixed)((ter3d_s32)elev * (ter3d_s32)info->height_scale);
    *out_height_q16 = ter3d_fixed_add_sat(base_q16, delta_q16);
    return TER3D_OK;
}

ter3d_result ter3d_terragen_bounds(const ter3d_u8 *data, ter3d_u32 size, const ter3d_terragen_info *info, ter3d_bounds *out_bounds)
{
    ter3d_u32 i;
    ter3d_s16 elev;
    ter3d_fixed h;
    ter3d_fixed minz;
    ter3d_fixed maxz;
    ter3d_fixed base_q16;
    ter3d_fixed delta_q16;
    ter3d_u32 off;

    if (data == 0 || info == 0 || out_bounds == 0) return TER3D_E_NULL;
    if (!info->has_altw || info->elevation_count == 0UL) return TER3D_E_FORMAT;
    if (info->elevations_offset + info->elevation_count * 2UL > size) return TER3D_E_NEED_MORE;

    base_q16 = (ter3d_fixed)((ter3d_s32)info->base_height << TER3D_Q);
    minz = TER3D_S32_MAX;
    maxz = TER3D_S32_MIN;
    for (i = 0UL; i < info->elevation_count; ++i) {
        off = info->elevations_offset + i * 2UL;
        elev = ter3d_read_le16s(data + off);
        delta_q16 = (ter3d_fixed)((ter3d_s32)elev * (ter3d_s32)info->height_scale);
        h = ter3d_fixed_add_sat(base_q16, delta_q16);
        if (h < minz) minz = h;
        if (h > maxz) maxz = h;
    }
    out_bounds->min_z_q16 = minz;
    out_bounds->max_z_q16 = maxz;
    out_bounds->min_x_q16 = 0;
    out_bounds->min_y_q16 = 0;
    out_bounds->max_x_q16 = (ter3d_fixed)((ter3d_s32)(info->xpts > 0U ? info->xpts - 1U : 0U) << TER3D_Q);
    out_bounds->max_y_q16 = (ter3d_fixed)((ter3d_s32)(info->ypts > 0U ? info->ypts - 1U : 0U) << TER3D_Q);
    return TER3D_OK;
}
