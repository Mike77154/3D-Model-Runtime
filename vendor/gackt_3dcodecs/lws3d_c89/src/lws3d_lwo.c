#include "lws3d_lwo.h"
#include <string.h>

#define LWS3D_LWO_FIXED_ONE ((LWS3D_Fixed)65536L)
#define LWS3D_LWO_FIXED_MAX ((LWS3D_Fixed)2147483647L)
#define LWS3D_LWO_FIXED_MIN ((LWS3D_Fixed)(-2147483647L - 1L))

static int lwo_id_eq(const unsigned char *p, const char *id)
{
    return p[0] == (unsigned char)id[0] && p[1] == (unsigned char)id[1] && p[2] == (unsigned char)id[2] && p[3] == (unsigned char)id[3];
}

static void lwo_copy_id(char out[5], const unsigned char *p)
{
    out[0] = (char)p[0];
    out[1] = (char)p[1];
    out[2] = (char)p[2];
    out[3] = (char)p[3];
    out[4] = '\0';
}

static unsigned int lwo_u2(const unsigned char *p)
{
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

static int lwo_i2(const unsigned char *p)
{
    unsigned int u;
    u = lwo_u2(p);
    if (u & 0x8000U) {
        return (int)(u - 65536U);
    }
    return (int)u;
}

static unsigned long lwo_u4(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) |
           ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] << 8) |
           (unsigned long)p[3];
}

static void lwo_zero_stats(LWS3D_LWO_Stats *stats)
{
    if (stats) {
        memset(stats, 0, sizeof(*stats));
    }
}

static void lwo_stat_overflow(LWS3D_LWO_Object *obj, LWS3D_LWO_Stats *stats)
{
    if (obj) {
        obj->overflowed = 1;
    }
    if (stats) {
        ++stats->overflow_events;
    }
}

static void lwo_stat_truncated(LWS3D_LWO_Object *obj, LWS3D_LWO_Stats *stats)
{
    if (obj) {
        obj->truncated = 1;
    }
    if (stats) {
        ++stats->truncated_chunks;
    }
}

static void lwo_stat_bad_index(LWS3D_LWO_Object *obj, LWS3D_LWO_Stats *stats)
{
    if (obj) {
        obj->corrupt = 1;
    }
    if (stats) {
        ++stats->bad_indices;
    }
}

static int lwo_copy_string(char *dst, int dst_size, const char *src)
{
    int i;
    int truncated;
    truncated = 0;
    if (dst_size <= 0) {
        return 1;
    }
    i = 0;
    while (src[i] && i < dst_size - 1) {
        dst[i] = src[i];
        ++i;
    }
    if (src[i]) {
        truncated = 1;
    }
    dst[i] = '\0';
    return truncated;
}

static unsigned long lwo_read_s0(const unsigned char *data, unsigned long start, unsigned long end, char *out, int out_size)
{
    unsigned long p;
    int i;
    p = start;
    i = 0;
    if (out_size > 0) {
        out[0] = '\0';
    }
    while (p < end && data[p] != 0U) {
        if (i < out_size - 1) {
            out[i] = (char)data[p];
            ++i;
        }
        ++p;
    }
    if (out_size > 0) {
        out[i] = '\0';
    }
    if (p < end) {
        ++p;
    }
    if (((p - start) & 1UL) != 0UL && p < end) {
        ++p;
    }
    return p;
}

LWS3D_Fixed lws3d_lwo_fixed_from_f4be(const unsigned char b[4])
{
    unsigned long raw;
    unsigned long mant;
    unsigned long out;
    int sign;
    int exp;
    int shift;
    raw = lwo_u4(b);
    sign = (raw & 0x80000000UL) ? -1 : 1;
    exp = (int)((raw >> 23) & 0xffUL);
    mant = raw & 0x7fffffUL;
    if (exp == 255) {
        if (sign < 0) {
            return LWS3D_LWO_FIXED_MIN;
        }
        return LWS3D_LWO_FIXED_MAX;
    }
    if (exp == 0) {
        return 0;
    }
    mant |= 0x800000UL;
    shift = exp - 134;
    if (shift >= 8) {
        if (sign < 0) {
            return LWS3D_LWO_FIXED_MIN;
        }
        return LWS3D_LWO_FIXED_MAX;
    }
    if (shift >= 0) {
        out = mant << shift;
        if (out > 2147483647UL) {
            if (sign < 0) {
                return LWS3D_LWO_FIXED_MIN;
            }
            return LWS3D_LWO_FIXED_MAX;
        }
    } else {
        int rshift;
        unsigned long round_bit;
        rshift = -shift;
        if (rshift >= 32) {
            out = 0UL;
        } else {
            out = mant >> rshift;
            if (rshift > 0) {
                round_bit = 1UL << (rshift - 1);
                if ((mant & round_bit) != 0UL && out < 2147483647UL) {
                    ++out;
                }
            }
        }
    }
    if (sign < 0) {
        if (out >= 2147483648UL) {
            return LWS3D_LWO_FIXED_MIN;
        }
        return (LWS3D_Fixed)(-((long)out));
    }
    return (LWS3D_Fixed)out;
}

static LWS3D_Fixed lwo_read_f4(const unsigned char *data, unsigned long pos)
{
    return lws3d_lwo_fixed_from_f4be(data + pos);
}

static void lwo_update_bbox(LWS3D_LWO_Object *obj, LWS3D_Fixed x, LWS3D_Fixed y, LWS3D_Fixed z)
{
    if (!obj->has_bbox) {
        obj->bbox_min.x = x;
        obj->bbox_min.y = y;
        obj->bbox_min.z = z;
        obj->bbox_max.x = x;
        obj->bbox_max.y = y;
        obj->bbox_max.z = z;
        obj->has_bbox = 1;
        return;
    }
    if (x < obj->bbox_min.x) obj->bbox_min.x = x;
    if (y < obj->bbox_min.y) obj->bbox_min.y = y;
    if (z < obj->bbox_min.z) obj->bbox_min.z = z;
    if (x > obj->bbox_max.x) obj->bbox_max.x = x;
    if (y > obj->bbox_max.y) obj->bbox_max.y = y;
    if (z > obj->bbox_max.z) obj->bbox_max.z = z;
}

void lws3d_lwo_init(LWS3D_LWO_Object *obj)
{
    long i;
    if (!obj) {
        return;
    }
    memset(obj, 0, sizeof(*obj));
    obj->format = LWS3D_LWO_FORMAT_UNKNOWN;
    obj->last_pols_first = -1;
    obj->last_pols_count = 0;
    for (i = 0; i < LWS3D_LWO_MAX_POLYGONS; ++i) {
        obj->polygons[i].surface_tag = LWS3D_LWO_SURFACE_NONE;
        obj->polygons[i].surface_index = LWS3D_LWO_SURFACE_NONE;
        obj->polygons[i].part_tag = LWS3D_LWO_SURFACE_NONE;
        obj->polygons[i].smoothing_tag = LWS3D_LWO_SURFACE_NONE;
    }
}

static int lwo_add_unknown(LWS3D_LWO_Object *obj, const unsigned char *id, unsigned long size, LWS3D_LWO_Stats *stats)
{
    if (obj->unknown_count < LWS3D_LWO_MAX_UNKNOWN) {
        lwo_copy_id(obj->unknowns[obj->unknown_count].id, id);
        obj->unknowns[obj->unknown_count].size = size;
        ++obj->unknown_count;
    } else {
        lwo_stat_overflow(obj, stats);
    }
    if (stats) {
        ++stats->unknown_chunks;
    }
    return LWS3D_LWO_OK;
}

static long lwo_add_layer(LWS3D_LWO_Object *obj, int number, int flags, LWS3D_Vec3 pivot, const char *name, int parent, LWS3D_LWO_Stats *stats)
{
    long i;
    long idx;
    for (i = 0; i < obj->layer_count; ++i) {
        if (obj->layers[i].used && obj->layers[i].number == number) {
            obj->layers[i].flags = flags;
            obj->layers[i].hidden = flags & 1;
            obj->layers[i].parent = parent;
            obj->layers[i].pivot = pivot;
            lwo_copy_string(obj->layers[i].name, LWS3D_LWO_MAX_NAME, name);
            return i;
        }
    }
    if (obj->layer_count >= LWS3D_LWO_MAX_LAYERS) {
        lwo_stat_overflow(obj, stats);
        return -1;
    }
    idx = obj->layer_count;
    ++obj->layer_count;
    obj->layers[idx].used = 1;
    obj->layers[idx].number = number;
    obj->layers[idx].flags = flags;
    obj->layers[idx].hidden = flags & 1;
    obj->layers[idx].parent = parent;
    obj->layers[idx].pivot = pivot;
    lwo_copy_string(obj->layers[idx].name, LWS3D_LWO_MAX_NAME, name);
    obj->layers[idx].first_point = obj->point_count;
    obj->layers[idx].point_count = 0;
    obj->layers[idx].first_polygon = obj->polygon_count;
    obj->layers[idx].polygon_count = 0;
    return idx;
}

static long lwo_default_layer(LWS3D_LWO_Object *obj, LWS3D_LWO_Stats *stats)
{
    LWS3D_Vec3 pivot;
    pivot.x = 0;
    pivot.y = 0;
    pivot.z = 0;
    return lwo_add_layer(obj, 0, 0, pivot, "Layer 0", -1, stats);
}

static int lwo_poly_type_from_id(const char *id)
{
    if (strcmp(id, "FACE") == 0) return LWS3D_LWO_POLY_FACE;
    if (strcmp(id, "CURV") == 0) return LWS3D_LWO_POLY_CURV;
    if (strcmp(id, "PTCH") == 0) return LWS3D_LWO_POLY_PTCH;
    if (strcmp(id, "MBAL") == 0) return LWS3D_LWO_POLY_MBAL;
    if (strcmp(id, "BONE") == 0) return LWS3D_LWO_POLY_BONE;
    return LWS3D_LWO_POLY_OTHER;
}

static int lwo_map_type_from_id(const char *id)
{
    if (strcmp(id, "TXUV") == 0) return LWS3D_LWO_MAP_TXUV;
    if (strcmp(id, "WGHT") == 0) return LWS3D_LWO_MAP_WGHT;
    if (strcmp(id, "RGB ") == 0) return LWS3D_LWO_MAP_RGB;
    if (strcmp(id, "RGBA") == 0) return LWS3D_LWO_MAP_RGBA;
    if (strcmp(id, "MORF") == 0) return LWS3D_LWO_MAP_MORF;
    if (strcmp(id, "SPOT") == 0) return LWS3D_LWO_MAP_SPOT;
    if (strcmp(id, "NORM") == 0) return LWS3D_LWO_MAP_NORM;
    if (strcmp(id, "MNVW") == 0) return LWS3D_LWO_MAP_MNVW;
    if (strcmp(id, "MBAL") == 0) return LWS3D_LWO_MAP_MBAL;
    return LWS3D_LWO_MAP_UNKNOWN;
}

static unsigned long lwo_read_vx(const unsigned char *data, unsigned long *pos, unsigned long end, int *ok)
{
    unsigned long v;
    if (*pos >= end) {
        *ok = 0;
        return 0UL;
    }
    if (data[*pos] == 0xffU) {
        if (*pos + 4UL > end) {
            *ok = 0;
            return 0UL;
        }
        v = lwo_u4(data + *pos) & 0x00ffffffUL;
        *pos += 4UL;
        return v;
    }
    if (*pos + 2UL > end) {
        *ok = 0;
        return 0UL;
    }
    v = (unsigned long)lwo_u2(data + *pos);
    *pos += 2UL;
    return v;
}

static int lwo_parse_layr(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, long *current_layer, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    LWS3D_Vec3 pivot;
    char name[LWS3D_LWO_MAX_NAME];
    int number;
    int flags;
    int parent;
    p = start;
    if (p + 16UL > end) {
        lwo_stat_truncated(obj, stats);
        return LWS3D_LWO_ERR_TRUNCATED;
    }
    number = (int)lwo_u2(data + p);
    p += 2UL;
    flags = (int)lwo_u2(data + p);
    p += 2UL;
    pivot.x = lwo_read_f4(data, p);
    p += 4UL;
    pivot.y = lwo_read_f4(data, p);
    p += 4UL;
    pivot.z = lwo_read_f4(data, p);
    p += 4UL;
    p = lwo_read_s0(data, p, end, name, LWS3D_LWO_MAX_NAME);
    parent = -1;
    if (p + 2UL <= end) {
        parent = lwo_i2(data + p);
    }
    *current_layer = lwo_add_layer(obj, number, flags, pivot, name, parent, stats);
    return LWS3D_LWO_OK;
}

static int lwo_parse_pnts(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, long *current_layer, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    long layer;
    long count;
    LWS3D_LWO_Point *pt;
    layer = *current_layer;
    if (layer < 0) {
        layer = lwo_default_layer(obj, stats);
        *current_layer = layer;
    }
    if (layer < 0) {
        return LWS3D_LWO_ERR_OVERFLOW;
    }
    p = start;
    count = 0;
    while (p + 12UL <= end) {
        if (obj->point_count >= LWS3D_LWO_MAX_POINTS) {
            lwo_stat_overflow(obj, stats);
            return LWS3D_LWO_ERR_OVERFLOW;
        }
        pt = &obj->points[obj->point_count];
        pt->x = lwo_read_f4(data, p);
        p += 4UL;
        pt->y = lwo_read_f4(data, p);
        p += 4UL;
        pt->z = lwo_read_f4(data, p);
        p += 4UL;
        pt->layer = (int)layer;
        lwo_update_bbox(obj, pt->x, pt->y, pt->z);
        ++obj->point_count;
        ++count;
    }
    if (p != end) {
        lwo_stat_truncated(obj, stats);
    }
    if (obj->layers[layer].point_count == 0) {
        obj->layers[layer].first_point = obj->point_count - count;
    }
    obj->layers[layer].point_count += count;
    return LWS3D_LWO_OK;
}

static int lwo_add_polygon(LWS3D_LWO_Object *obj, long layer, const char *type_id, int type, int flags, int vert_count, unsigned long *local_vertices, LWS3D_LWO_Stats *stats)
{
    long i;
    long first;
    LWS3D_LWO_Polygon *poly;
    if (obj->polygon_count >= LWS3D_LWO_MAX_POLYGONS) {
        lwo_stat_overflow(obj, stats);
        return LWS3D_LWO_ERR_OVERFLOW;
    }
    poly = &obj->polygons[obj->polygon_count];
    memset(poly, 0, sizeof(*poly));
    poly->type = type;
    lwo_copy_string(poly->type_id, 5, type_id);
    poly->layer = (int)layer;
    poly->vertex_count = vert_count;
    poly->stored_vertex_count = vert_count;
    poly->flags = flags;
    poly->surface_tag = LWS3D_LWO_SURFACE_NONE;
    poly->surface_index = LWS3D_LWO_SURFACE_NONE;
    poly->part_tag = LWS3D_LWO_SURFACE_NONE;
    poly->smoothing_tag = LWS3D_LWO_SURFACE_NONE;
    if (poly->stored_vertex_count > LWS3D_LWO_MAX_POLY_VERTS) {
        poly->stored_vertex_count = LWS3D_LWO_MAX_POLY_VERTS;
        poly->vertex_truncated = 1;
        lwo_stat_overflow(obj, stats);
    }
    first = 0;
    if (layer >= 0 && layer < obj->layer_count) {
        first = obj->layers[layer].first_point;
    }
    for (i = 0; i < poly->stored_vertex_count; ++i) {
        if (layer >= 0 && layer < obj->layer_count) {
            if ((long)local_vertices[i] >= obj->layers[layer].point_count) {
                lwo_stat_bad_index(obj, stats);
            }
            poly->vertices[i] = (unsigned long)(first + (long)local_vertices[i]);
        } else {
            poly->vertices[i] = local_vertices[i];
        }
    }
    if (layer >= 0 && layer < obj->layer_count) {
        if (obj->layers[layer].polygon_count == 0) {
            obj->layers[layer].first_polygon = obj->polygon_count;
        }
        ++obj->layers[layer].polygon_count;
    }
    ++obj->polygon_count;
    return LWS3D_LWO_OK;
}

static int lwo_parse_pols_lwo2(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, long *current_layer, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    unsigned long local_vertices[LWS3D_LWO_MAX_POLY_VERTS];
    char type_id[5];
    int type;
    int ok;
    int vert_count;
    int flags;
    int i;
    unsigned int count_flags;
    long layer;
    long before;
    p = start;
    if (p + 4UL > end) {
        lwo_stat_truncated(obj, stats);
        return LWS3D_LWO_ERR_TRUNCATED;
    }
    lwo_copy_id(type_id, data + p);
    p += 4UL;
    type = lwo_poly_type_from_id(type_id);
    layer = *current_layer;
    if (layer < 0) {
        layer = lwo_default_layer(obj, stats);
        *current_layer = layer;
    }
    before = obj->polygon_count;
    while (p + 2UL <= end) {
        count_flags = lwo_u2(data + p);
        p += 2UL;
        vert_count = (int)(count_flags & 0x03ffU);
        flags = (int)((count_flags >> 10) & 0x003fU);
        if (vert_count < 0) {
            lwo_stat_bad_index(obj, stats);
            return LWS3D_LWO_ERR_CORRUPT;
        }
        ok = 1;
        for (i = 0; i < vert_count; ++i) {
            unsigned long vx;
            vx = lwo_read_vx(data, &p, end, &ok);
            if (!ok) {
                lwo_stat_truncated(obj, stats);
                return LWS3D_LWO_ERR_TRUNCATED;
            }
            if (i < LWS3D_LWO_MAX_POLY_VERTS) {
                local_vertices[i] = vx;
            }
        }
        if (lwo_add_polygon(obj, layer, type_id, type, flags, vert_count, local_vertices, stats) != LWS3D_LWO_OK) {
            return LWS3D_LWO_ERR_OVERFLOW;
        }
    }
    if (p != end) {
        lwo_stat_truncated(obj, stats);
    }
    obj->last_pols_first = before;
    obj->last_pols_count = obj->polygon_count - before;
    return LWS3D_LWO_OK;
}

static int lwo_parse_tags(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    char text[LWS3D_LWO_MAX_NAME];
    p = start;
    while (p < end) {
        unsigned long next;
        next = lwo_read_s0(data, p, end, text, LWS3D_LWO_MAX_NAME);
        if (next <= p) {
            break;
        }
        if (obj->tag_count < LWS3D_LWO_MAX_TAGS) {
            lwo_copy_string(obj->tags[obj->tag_count].text, LWS3D_LWO_MAX_NAME, text);
            ++obj->tag_count;
        } else {
            lwo_stat_overflow(obj, stats);
            return LWS3D_LWO_ERR_OVERFLOW;
        }
        p = next;
    }
    return LWS3D_LWO_OK;
}

static int lwo_parse_ptag(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    char type_id[5];
    int ok;
    p = start;
    if (p + 4UL > end) {
        lwo_stat_truncated(obj, stats);
        return LWS3D_LWO_ERR_TRUNCATED;
    }
    lwo_copy_id(type_id, data + p);
    p += 4UL;
    ok = 1;
    while (p < end) {
        unsigned long poly_local;
        unsigned int tag;
        long poly_global;
        poly_local = lwo_read_vx(data, &p, end, &ok);
        if (!ok || p + 2UL > end) {
            lwo_stat_truncated(obj, stats);
            return LWS3D_LWO_ERR_TRUNCATED;
        }
        tag = lwo_u2(data + p);
        p += 2UL;
        poly_global = obj->last_pols_first + (long)poly_local;
        if (obj->last_pols_first < 0 || poly_local >= (unsigned long)obj->last_pols_count || poly_global < 0 || poly_global >= obj->polygon_count) {
            lwo_stat_bad_index(obj, stats);
        } else {
            if (strcmp(type_id, "SURF") == 0) {
                obj->polygons[poly_global].surface_tag = (int)tag;
            } else if (strcmp(type_id, "PART") == 0) {
                obj->polygons[poly_global].part_tag = (int)tag;
            } else if (strcmp(type_id, "SMGP") == 0) {
                obj->polygons[poly_global].smoothing_tag = (int)tag;
            }
        }
    }
    return LWS3D_LWO_OK;
}

static long lwo_add_vmap(LWS3D_LWO_Object *obj, const char *type_id, int type, int dim, int discontinuous, const char *name, LWS3D_LWO_Stats *stats)
{
    long idx;
    if (obj->vmap_count >= LWS3D_LWO_MAX_VMAPS) {
        lwo_stat_overflow(obj, stats);
        return -1;
    }
    idx = obj->vmap_count;
    ++obj->vmap_count;
    lwo_copy_string(obj->vmaps[idx].type_id, 5, type_id);
    obj->vmaps[idx].type = type;
    obj->vmaps[idx].dimension = dim;
    obj->vmaps[idx].discontinuous = discontinuous;
    lwo_copy_string(obj->vmaps[idx].name, LWS3D_LWO_MAX_NAME, name);
    obj->vmaps[idx].first_value = obj->vmap_value_count;
    obj->vmaps[idx].value_count = 0;
    return idx;
}

static int lwo_parse_vmap_common(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, long *current_layer, int discontinuous, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    char type_id[5];
    char name[LWS3D_LWO_MAX_NAME];
    int dim;
    int type;
    int ok;
    long map_idx;
    long layer;
    p = start;
    if (p + 6UL > end) {
        lwo_stat_truncated(obj, stats);
        return LWS3D_LWO_ERR_TRUNCATED;
    }
    lwo_copy_id(type_id, data + p);
    p += 4UL;
    dim = (int)lwo_u2(data + p);
    p += 2UL;
    if (dim < 0 || dim > 64) {
        lwo_stat_bad_index(obj, stats);
        return LWS3D_LWO_ERR_CORRUPT;
    }
    p = lwo_read_s0(data, p, end, name, LWS3D_LWO_MAX_NAME);
    type = lwo_map_type_from_id(type_id);
    layer = *current_layer;
    if (layer < 0) {
        layer = lwo_default_layer(obj, stats);
        *current_layer = layer;
    }
    map_idx = lwo_add_vmap(obj, type_id, type, dim, discontinuous, name, stats);
    if (map_idx < 0) {
        return LWS3D_LWO_ERR_OVERFLOW;
    }
    ok = 1;
    while (p < end) {
        unsigned long vx;
        long poly;
        int i;
        LWS3D_LWO_VMapValue *val;
        vx = lwo_read_vx(data, &p, end, &ok);
        if (!ok) {
            lwo_stat_truncated(obj, stats);
            return LWS3D_LWO_ERR_TRUNCATED;
        }
        poly = -1;
        if (discontinuous) {
            unsigned long pvx;
            pvx = lwo_read_vx(data, &p, end, &ok);
            if (!ok) {
                lwo_stat_truncated(obj, stats);
                return LWS3D_LWO_ERR_TRUNCATED;
            }
            poly = (long)pvx;
        }
        if (p + (unsigned long)(dim * 4) > end) {
            lwo_stat_truncated(obj, stats);
            return LWS3D_LWO_ERR_TRUNCATED;
        }
        if (obj->vmap_value_count >= LWS3D_LWO_MAX_VMAP_VALUES) {
            lwo_stat_overflow(obj, stats);
            return LWS3D_LWO_ERR_OVERFLOW;
        }
        val = &obj->vmap_values[obj->vmap_value_count];
        memset(val, 0, sizeof(*val));
        val->map_index = map_idx;
        if (layer >= 0 && layer < obj->layer_count) {
            val->point = (unsigned long)(obj->layers[layer].first_point + (long)vx);
        } else {
            val->point = vx;
        }
        val->polygon = poly;
        for (i = 0; i < dim; ++i) {
            LWS3D_Fixed f;
            f = lwo_read_f4(data, p);
            p += 4UL;
            if (i < 4) {
                val->value[i] = f;
            }
        }
        ++obj->vmap_value_count;
        ++obj->vmaps[map_idx].value_count;
    }
    return LWS3D_LWO_OK;
}


static int lwo_block_type_from_id(const char *id)
{
    if (strcmp(id, "IMAP") == 0) return LWS3D_LWO_BLOCK_IMAP;
    if (strcmp(id, "PROC") == 0) return LWS3D_LWO_BLOCK_PROC;
    if (strcmp(id, "GRAD") == 0) return LWS3D_LWO_BLOCK_GRAD;
    if (strcmp(id, "SHDR") == 0) return LWS3D_LWO_BLOCK_SHDR;
    return LWS3D_LWO_BLOCK_UNKNOWN;
}

static long lwo_add_surface_block(LWS3D_LWO_Object *obj, long surface_index, const char *block_id, LWS3D_LWO_Stats *stats)
{
    long idx;
    LWS3D_LWO_SurfaceBlock *b;
    if (obj->block_count >= LWS3D_LWO_MAX_BLOCKS) {
        lwo_stat_overflow(obj, stats);
        return -1;
    }
    idx = obj->block_count;
    ++obj->block_count;
    b = &obj->blocks[idx];
    memset(b, 0, sizeof(*b));
    b->surface_index = (int)surface_index;
    lwo_copy_string(b->block_id, 5, block_id);
    b->block_type = lwo_block_type_from_id(block_id);
    b->enabled = 1;
    b->opacity = LWS3D_LWO_FIXED_ONE;
    b->image_clip = 0UL;
    if (stats) {
        ++stats->surface_blocks;
        if (b->block_type == LWS3D_LWO_BLOCK_PROC) ++stats->procedural_blocks;
        if (b->block_type == LWS3D_LWO_BLOCK_SHDR) ++stats->shader_blocks;
    }
    return idx;
}

static int lwo_add_nodal_blob(LWS3D_LWO_Object *obj, long surface_index, const char *id, unsigned long offset, unsigned long size, int depth, LWS3D_LWO_Stats *stats)
{
    long idx;
    if (obj->nodal_blob_count >= LWS3D_LWO_MAX_NODAL_BLOBS) {
        lwo_stat_overflow(obj, stats);
        return LWS3D_LWO_ERR_OVERFLOW;
    }
    idx = obj->nodal_blob_count;
    ++obj->nodal_blob_count;
    if (id) {
        obj->nodal_blobs[idx].id[0] = id[0];
        obj->nodal_blobs[idx].id[1] = id[1];
        obj->nodal_blobs[idx].id[2] = id[2];
        obj->nodal_blobs[idx].id[3] = id[3];
        obj->nodal_blobs[idx].id[4] = '\0';
    }
    obj->nodal_blobs[idx].surface_index = (int)surface_index;
    obj->nodal_blobs[idx].offset = offset;
    obj->nodal_blobs[idx].size = size;
    obj->nodal_blobs[idx].depth = depth;
    if (stats) ++stats->nodal_blobs;
    return LWS3D_LWO_OK;
}

static int lwo_parse_blok_header(LWS3D_LWO_SurfaceBlock *blk, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    p = lwo_read_s0(data, start, end, blk->ordinal, LWS3D_LWO_MAX_NAME);
    while (p + 6UL <= end) {
        char sid[5];
        unsigned long ssize;
        unsigned long send;
        lwo_copy_id(sid, data + p);
        p += 4UL;
        ssize = (unsigned long)lwo_u2(data + p);
        p += 2UL;
        send = p + ssize;
        if (send > end) return LWS3D_LWO_ERR_TRUNCATED;
        if (stats) ++stats->subchunks_seen;
        if (strcmp(sid, "CHAN") == 0 && p + 4UL <= send) {
            lwo_copy_id(blk->channel, data + p);
        } else if (strcmp(sid, "ENAB") == 0 && p + 2UL <= send) {
            blk->enabled = (int)lwo_u2(data + p);
        } else if (strcmp(sid, "OPAC") == 0 && p + 6UL <= send) {
            blk->opacity_type = (int)lwo_u2(data + p);
            blk->opacity = lwo_read_f4(data, p + 2UL);
        }
        p = send;
        if ((ssize & 1UL) != 0UL && p < end) ++p;
    }
    return LWS3D_LWO_OK;
}

static int lwo_parse_blok(LWS3D_LWO_Object *obj, long surface_index, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    long idx;
    LWS3D_LWO_SurfaceBlock *blk;
    char hid[5];
    unsigned long hsize;
    unsigned long hend;
    if (start + 6UL > end) return LWS3D_LWO_ERR_TRUNCATED;
    p = start;
    lwo_copy_id(hid, data + p);
    p += 4UL;
    hsize = (unsigned long)lwo_u2(data + p);
    p += 2UL;
    hend = p + hsize;
    if (hend > end) return LWS3D_LWO_ERR_TRUNCATED;
    idx = lwo_add_surface_block(obj, surface_index, hid, stats);
    if (idx < 0) return LWS3D_LWO_ERR_OVERFLOW;
    blk = &obj->blocks[idx];
    lwo_parse_blok_header(blk, data, p, hend, stats);
    p = hend;
    if ((hsize & 1UL) != 0UL && p < end) ++p;
    while (p + 6UL <= end) {
        char sid[5];
        unsigned long ssize;
        unsigned long send;
        lwo_copy_id(sid, data + p);
        p += 4UL;
        ssize = (unsigned long)lwo_u2(data + p);
        p += 2UL;
        send = p + ssize;
        if (send > end) return LWS3D_LWO_ERR_TRUNCATED;
        if (stats) ++stats->subchunks_seen;
        if (strcmp(sid, "FUNC") == 0) {
            unsigned long next;
            next = lwo_read_s0(data, p, send, blk->algorithm, LWS3D_LWO_MAX_NAME);
            if (next < send) blk->opaque_data_size = send - next;
        } else if (strcmp(sid, "VMAP") == 0) {
            lwo_read_s0(data, p, send, blk->vmap_name, LWS3D_LWO_MAX_NAME);
        } else if (strcmp(sid, "IMAG") == 0 && p + 4UL <= send) {
            blk->image_clip = lwo_u4(data + p);
        } else if (strcmp(sid, "PROJ") == 0 && p + 2UL <= send) {
            blk->projection = (int)lwo_u2(data + p);
        } else if (strcmp(sid, "AXIS") == 0 && p + 2UL <= send) {
            blk->axis = (int)lwo_u2(data + p);
        } else if (strcmp(sid, "CNTR") == 0 && p + 12UL <= send) {
            blk->center.x = lwo_read_f4(data, p);
            blk->center.y = lwo_read_f4(data, p + 4UL);
            blk->center.z = lwo_read_f4(data, p + 8UL);
        } else if (strcmp(sid, "SIZE") == 0 && p + 12UL <= send) {
            blk->size.x = lwo_read_f4(data, p);
            blk->size.y = lwo_read_f4(data, p + 4UL);
            blk->size.z = lwo_read_f4(data, p + 8UL);
        } else if (strcmp(sid, "ROTA") == 0 && p + 12UL <= send) {
            blk->rotation.x = lwo_read_f4(data, p);
            blk->rotation.y = lwo_read_f4(data, p + 4UL);
            blk->rotation.z = lwo_read_f4(data, p + 8UL);
        } else if (strcmp(sid, "NODS") == 0 || strcmp(sid, "NDTA") == 0 || strcmp(sid, "NODE") == 0) {
            lwo_add_nodal_blob(obj, surface_index, sid, p, ssize, 0, stats);
        } else {
            if (stats) ++stats->unknown_subchunks;
        }
        p = send;
        if ((ssize & 1UL) != 0UL && p < end) ++p;
    }
    return LWS3D_LWO_OK;
}

static int lwo_find_surface_by_name(const LWS3D_LWO_Object *obj, const char *name)
{
    long i;
    for (i = 0; i < obj->surface_count; ++i) {
        if (strcmp(obj->surfaces[i].name, name) == 0) {
            return (int)i;
        }
    }
    return LWS3D_LWO_SURFACE_NONE;
}

static long lwo_add_surface(LWS3D_LWO_Object *obj, const char *name, const char *source, LWS3D_LWO_Stats *stats)
{
    int found;
    long idx;
    found = lwo_find_surface_by_name(obj, name);
    if (found >= 0) {
        if (source) {
            lwo_copy_string(obj->surfaces[found].source, LWS3D_LWO_MAX_NAME, source);
        }
        return (long)found;
    }
    if (obj->surface_count >= LWS3D_LWO_MAX_SURFACES) {
        lwo_stat_overflow(obj, stats);
        return -1;
    }
    idx = obj->surface_count;
    ++obj->surface_count;
    memset(&obj->surfaces[idx], 0, sizeof(obj->surfaces[idx]));
    lwo_copy_string(obj->surfaces[idx].name, LWS3D_LWO_MAX_NAME, name);
    if (source) {
        lwo_copy_string(obj->surfaces[idx].source, LWS3D_LWO_MAX_NAME, source);
    }
    obj->surfaces[idx].diffuse = LWS3D_LWO_FIXED_ONE;
    obj->surfaces[idx].sidedness = 1;
    obj->surfaces[idx].color.x = LWS3D_LWO_FIXED_ONE;
    obj->surfaces[idx].color.y = LWS3D_LWO_FIXED_ONE;
    obj->surfaces[idx].color.z = LWS3D_LWO_FIXED_ONE;
    return idx;
}

static void lwo_parse_surface_sub(LWS3D_LWO_Object *obj, long surf_idx, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    LWS3D_LWO_Surface *surf;
    surf = &obj->surfaces[surf_idx];
    p = start;
    while (p + 6UL <= end) {
        unsigned long sstart;
        unsigned long ssize;
        unsigned long send;
        char sid[5];
        lwo_copy_id(sid, data + p);
        p += 4UL;
        ssize = (unsigned long)lwo_u2(data + p);
        p += 2UL;
        sstart = p;
        send = sstart + ssize;
        if (send > end) {
            if (stats) {
                ++stats->truncated_chunks;
            }
            return;
        }
        if (stats) {
            ++stats->subchunks_seen;
        }
        if (strcmp(sid, "COLR") == 0 && sstart + 12UL <= send) {
            surf->color.x = lwo_read_f4(data, sstart);
            surf->color.y = lwo_read_f4(data, sstart + 4UL);
            surf->color.z = lwo_read_f4(data, sstart + 8UL);
            surf->has_color = 1;
        } else if (strcmp(sid, "DIFF") == 0 && sstart + 4UL <= send) {
            surf->diffuse = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "LUMI") == 0 && sstart + 4UL <= send) {
            surf->luminosity = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "SPEC") == 0 && sstart + 4UL <= send) {
            surf->specular = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "GLOS") == 0 && sstart + 4UL <= send) {
            surf->glossiness = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "REFL") == 0 && sstart + 4UL <= send) {
            surf->reflection = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "TRAN") == 0 && sstart + 4UL <= send) {
            surf->transparency = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "SIDE") == 0 && sstart + 2UL <= send) {
            surf->sidedness = (int)lwo_u2(data + sstart);
        } else if (strcmp(sid, "SMAN") == 0 && sstart + 4UL <= send) {
            surf->smoothing_angle = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "BLOK") == 0) {
            lwo_parse_blok(obj, surf_idx, data, sstart, send, stats);
        } else if (strcmp(sid, "NODS") == 0 || strcmp(sid, "NDTA") == 0 || strcmp(sid, "NODE") == 0) {
            lwo_add_nodal_blob(obj, surf_idx, sid, sstart, ssize, 0, stats);
        } else {
            if (stats) {
                ++stats->unknown_subchunks;
            }
        }
        p = send;
        if ((ssize & 1UL) != 0UL && p < end) {
            ++p;
        }
    }
}

static int lwo_parse_surf_lwo2(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    char name[LWS3D_LWO_MAX_NAME];
    char source[LWS3D_LWO_MAX_NAME];
    long surf_idx;
    p = start;
    p = lwo_read_s0(data, p, end, name, LWS3D_LWO_MAX_NAME);
    p = lwo_read_s0(data, p, end, source, LWS3D_LWO_MAX_NAME);
    surf_idx = lwo_add_surface(obj, name, source, stats);
    if (surf_idx < 0) {
        return LWS3D_LWO_ERR_OVERFLOW;
    }
    lwo_parse_surface_sub(obj, surf_idx, data, p, end, stats);
    return LWS3D_LWO_OK;
}

static int lwo_parse_bbox(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    if (start + 24UL > end) {
        lwo_stat_truncated(obj, stats);
        return LWS3D_LWO_ERR_TRUNCATED;
    }
    obj->bbox_min.x = lwo_read_f4(data, start);
    obj->bbox_min.y = lwo_read_f4(data, start + 4UL);
    obj->bbox_min.z = lwo_read_f4(data, start + 8UL);
    obj->bbox_max.x = lwo_read_f4(data, start + 12UL);
    obj->bbox_max.y = lwo_read_f4(data, start + 16UL);
    obj->bbox_max.z = lwo_read_f4(data, start + 20UL);
    obj->has_bbox = 1;
    return LWS3D_LWO_OK;
}

static int lwo_parse_clip(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    long idx;
    p = start;
    if (p + 4UL > end) {
        lwo_stat_truncated(obj, stats);
        return LWS3D_LWO_ERR_TRUNCATED;
    }
    if (obj->clip_count >= LWS3D_LWO_MAX_CLIPS) {
        lwo_stat_overflow(obj, stats);
        return LWS3D_LWO_ERR_OVERFLOW;
    }
    idx = obj->clip_count;
    ++obj->clip_count;
    obj->clips[idx].index = lwo_u4(data + p);
    obj->clips[idx].still_path[0] = '\0';
    p += 4UL;
    while (p + 6UL <= end) {
        char sid[5];
        unsigned long ssize;
        unsigned long send;
        lwo_copy_id(sid, data + p);
        p += 4UL;
        ssize = (unsigned long)lwo_u2(data + p);
        p += 2UL;
        send = p + ssize;
        if (send > end) {
            lwo_stat_truncated(obj, stats);
            return LWS3D_LWO_ERR_TRUNCATED;
        }
        if (stats) {
            ++stats->subchunks_seen;
        }
        if (strcmp(sid, "STIL") == 0) {
            lwo_read_s0(data, p, send, obj->clips[idx].still_path, LWS3D_LWO_MAX_PATH);
        } else {
            if (stats) {
                ++stats->unknown_subchunks;
            }
        }
        p = send;
        if ((ssize & 1UL) != 0UL && p < end) {
            ++p;
        }
    }
    return LWS3D_LWO_OK;
}

static int lwo_parse_srfs_lwob(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    char name[LWS3D_LWO_MAX_NAME];
    p = start;
    while (p < end) {
        unsigned long next;
        next = lwo_read_s0(data, p, end, name, LWS3D_LWO_MAX_NAME);
        if (next <= p) {
            break;
        }
        if (name[0] != '\0') {
            if (lwo_add_surface(obj, name, "", stats) < 0) {
                return LWS3D_LWO_ERR_OVERFLOW;
            }
            if (obj->tag_count < LWS3D_LWO_MAX_TAGS) {
                lwo_copy_string(obj->tags[obj->tag_count].text, LWS3D_LWO_MAX_NAME, name);
                ++obj->tag_count;
            }
        }
        p = next;
    }
    return LWS3D_LWO_OK;
}

static int lwo_parse_pols_lwob(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, long *current_layer, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    long layer;
    unsigned long local_vertices[LWS3D_LWO_MAX_POLY_VERTS];
    long before;
    layer = *current_layer;
    if (layer < 0) {
        layer = lwo_default_layer(obj, stats);
        *current_layer = layer;
    }
    p = start;
    before = obj->polygon_count;
    while (p + 2UL <= end) {
        unsigned int count;
        int i;
        int surf;
        count = lwo_u2(data + p);
        p += 2UL;
        count &= 0x03ffU;
        if (p + (unsigned long)(count * 2U) + 2UL > end) {
            lwo_stat_truncated(obj, stats);
            return LWS3D_LWO_ERR_TRUNCATED;
        }
        for (i = 0; i < (int)count; ++i) {
            unsigned int vx;
            vx = lwo_u2(data + p);
            p += 2UL;
            if (i < LWS3D_LWO_MAX_POLY_VERTS) {
                local_vertices[i] = (unsigned long)vx;
            }
        }
        surf = lwo_i2(data + p);
        p += 2UL;
        if (lwo_add_polygon(obj, layer, "FACE", LWS3D_LWO_POLY_FACE, 0, (int)count, local_vertices, stats) != LWS3D_LWO_OK) {
            return LWS3D_LWO_ERR_OVERFLOW;
        }
        if (obj->polygon_count > 0) {
            long pi;
            pi = obj->polygon_count - 1;
            if (surf > 0) {
                obj->polygons[pi].surface_tag = surf - 1;
            } else if (surf < 0) {
                obj->polygons[pi].surface_tag = (-surf) - 1;
            }
        }
    }
    obj->last_pols_first = before;
    obj->last_pols_count = obj->polygon_count - before;
    return LWS3D_LWO_OK;
}

int lws3d_lwo_resolve_surfaces(LWS3D_LWO_Object *obj)
{
    long i;
    if (!obj) {
        return LWS3D_LWO_ERR_BAD_ARG;
    }
    for (i = 0; i < obj->polygon_count; ++i) {
        int tag;
        tag = obj->polygons[i].surface_tag;
        obj->polygons[i].surface_index = LWS3D_LWO_SURFACE_NONE;
        if (tag >= 0 && tag < obj->tag_count) {
            obj->polygons[i].surface_index = lwo_find_surface_by_name(obj, obj->tags[tag].text);
        }
        if (obj->polygons[i].surface_index == LWS3D_LWO_SURFACE_NONE && tag >= 0 && tag < obj->surface_count) {
            obj->polygons[i].surface_index = tag;
        }
    }
    return LWS3D_LWO_OK;
}

int lws3d_lwo_build_triangles(LWS3D_LWO_Object *obj, LWS3D_LWO_Stats *stats)
{
    long i;
    if (!obj) {
        return LWS3D_LWO_ERR_BAD_ARG;
    }
    obj->triangle_count = 0;
    for (i = 0; i < obj->polygon_count; ++i) {
        LWS3D_LWO_Polygon *p;
        int k;
        p = &obj->polygons[i];
        if (p->type != LWS3D_LWO_POLY_FACE && p->type != LWS3D_LWO_POLY_PTCH) {
            continue;
        }
        if (p->stored_vertex_count < 3) {
            continue;
        }
        for (k = 1; k + 1 < p->stored_vertex_count; ++k) {
            LWS3D_LWO_Triangle *t;
            if (obj->triangle_count >= LWS3D_LWO_MAX_TRIANGLES) {
                lwo_stat_overflow(obj, stats);
                return LWS3D_LWO_ERR_OVERFLOW;
            }
            t = &obj->triangles[obj->triangle_count];
            t->polygon = (unsigned long)i;
            t->v0 = p->vertices[0];
            t->v1 = p->vertices[k];
            t->v2 = p->vertices[k + 1];
            t->surface_index = p->surface_index;
            ++obj->triangle_count;
            if (stats) {
                ++stats->polygons_triangulated;
            }
        }
    }
    return LWS3D_LWO_OK;
}


static void lwo_parse_surface_sub32(LWS3D_LWO_Object *obj, long surf_idx, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    LWS3D_LWO_Surface *surf;
    surf = &obj->surfaces[surf_idx];
    p = start;
    while (p + 8UL <= end) {
        unsigned long sstart;
        unsigned long ssize;
        unsigned long send;
        char sid[5];
        lwo_copy_id(sid, data + p);
        p += 4UL;
        ssize = lwo_u4(data + p);
        p += 4UL;
        sstart = p;
        send = sstart + ssize;
        if (send > end || send < sstart) {
            lwo_stat_truncated(obj, stats);
            return;
        }
        if (stats) ++stats->subchunks_seen;
        if (strcmp(sid, "COLR") == 0 && sstart + 12UL <= send) {
            surf->color.x = lwo_read_f4(data, sstart);
            surf->color.y = lwo_read_f4(data, sstart + 4UL);
            surf->color.z = lwo_read_f4(data, sstart + 8UL);
            surf->has_color = 1;
        } else if (strcmp(sid, "DIFF") == 0 && sstart + 4UL <= send) {
            surf->diffuse = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "LUMI") == 0 && sstart + 4UL <= send) {
            surf->luminosity = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "SPEC") == 0 && sstart + 4UL <= send) {
            surf->specular = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "GLOS") == 0 && sstart + 4UL <= send) {
            surf->glossiness = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "REFL") == 0 && sstart + 4UL <= send) {
            surf->reflection = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "TRAN") == 0 && sstart + 4UL <= send) {
            surf->transparency = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "SIDE") == 0 && sstart + 2UL <= send) {
            surf->sidedness = (int)lwo_u2(data + sstart);
        } else if (strcmp(sid, "SMAN") == 0 && sstart + 4UL <= send) {
            surf->smoothing_angle = lwo_read_f4(data, sstart);
        } else if (strcmp(sid, "BLOK") == 0) {
            lwo_parse_blok(obj, surf_idx, data, sstart, send, stats);
        } else if (strcmp(sid, "NODS") == 0 || strcmp(sid, "NDTA") == 0 || strcmp(sid, "NODE") == 0 || strcmp(sid, "NOD3") == 0) {
            lwo_add_nodal_blob(obj, surf_idx, sid, sstart, ssize, 1, stats);
        } else if (strcmp(sid, "FORM") == 0) {
            lwo_add_nodal_blob(obj, surf_idx, sid, sstart, ssize, 1, stats);
        } else {
            if (stats) ++stats->unknown_subchunks;
        }
        p = send;
        if ((ssize & 1UL) != 0UL && p < end) ++p;
    }
}

static int lwo_parse_surf_lwo3(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    char name[LWS3D_LWO_MAX_NAME];
    char source[LWS3D_LWO_MAX_NAME];
    long surf_idx;
    p = start;
    p = lwo_read_s0(data, p, end, name, LWS3D_LWO_MAX_NAME);
    p = lwo_read_s0(data, p, end, source, LWS3D_LWO_MAX_NAME);
    surf_idx = lwo_add_surface(obj, name, source, stats);
    if (surf_idx < 0) return LWS3D_LWO_ERR_OVERFLOW;
    lwo_parse_surface_sub32(obj, surf_idx, data, p, end, stats);
    return LWS3D_LWO_OK;
}

static int lwo_parse_lwo3_chunks(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, long *current_layer, int depth, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    int err;
    if (depth > 16) {
        lwo_stat_bad_index(obj, stats);
        return LWS3D_LWO_ERR_CORRUPT;
    }
    p = start;
    err = LWS3D_LWO_OK;
    while (p + 8UL <= end) {
        const unsigned char *id;
        unsigned long csize;
        unsigned long cstart;
        unsigned long cend;
        id = data + p;
        p += 4UL;
        csize = lwo_u4(data + p);
        p += 4UL;
        cstart = p;
        cend = cstart + csize;
        if (cend > end || cend < cstart) {
            lwo_stat_truncated(obj, stats);
            return LWS3D_LWO_ERR_TRUNCATED;
        }
        if (stats) ++stats->chunks_seen;
        if (lwo_id_eq(id, "FORM")) {
            char form_type[5];
            if (cstart + 4UL <= cend) {
                lwo_copy_id(form_type, data + cstart);
                if (stats) ++stats->lwo3_forms;
                err = lwo_parse_lwo3_chunks(obj, data, cstart + 4UL, cend, current_layer, depth + 1, stats);
            } else {
                err = LWS3D_LWO_ERR_TRUNCATED;
            }
        } else if (lwo_id_eq(id, "LAYR")) {
            err = lwo_parse_layr(obj, data, cstart, cend, current_layer, stats);
        } else if (lwo_id_eq(id, "PNTS")) {
            err = lwo_parse_pnts(obj, data, cstart, cend, current_layer, stats);
        } else if (lwo_id_eq(id, "POLS")) {
            err = lwo_parse_pols_lwo2(obj, data, cstart, cend, current_layer, stats);
        } else if (lwo_id_eq(id, "TAGS")) {
            err = lwo_parse_tags(obj, data, cstart, cend, stats);
        } else if (lwo_id_eq(id, "PTAG")) {
            err = lwo_parse_ptag(obj, data, cstart, cend, stats);
        } else if (lwo_id_eq(id, "VMAP")) {
            err = lwo_parse_vmap_common(obj, data, cstart, cend, current_layer, 0, stats);
        } else if (lwo_id_eq(id, "VMAD")) {
            err = lwo_parse_vmap_common(obj, data, cstart, cend, current_layer, 1, stats);
        } else if (lwo_id_eq(id, "SURF")) {
            err = lwo_parse_surf_lwo3(obj, data, cstart, cend, stats);
        } else if (lwo_id_eq(id, "BBOX")) {
            err = lwo_parse_bbox(obj, data, cstart, cend, stats);
        } else if (lwo_id_eq(id, "CLIP")) {
            err = lwo_parse_clip(obj, data, cstart, cend, stats);
        } else if (lwo_id_eq(id, "NODS") || lwo_id_eq(id, "NDTA") || lwo_id_eq(id, "NODE") || lwo_id_eq(id, "NOD3")) {
            err = lwo_add_nodal_blob(obj, -1, (const char *)id, cstart, csize, depth, stats);
        } else {
            err = lwo_add_unknown(obj, id, csize, stats);
        }
        if (err != LWS3D_LWO_OK && err != LWS3D_LWO_ERR_OVERFLOW) return err;
        p = cend;
        if ((csize & 1UL) != 0UL && p < end) ++p;
    }
    if (p != end) lwo_stat_truncated(obj, stats);
    return obj->overflowed ? LWS3D_LWO_ERR_OVERFLOW : LWS3D_LWO_OK;
}

static int lwo_parse_form_chunks(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long start, unsigned long end, LWS3D_LWO_Stats *stats)
{
    unsigned long p;
    long current_layer;
    int err;
    p = start;
    current_layer = -1;
    err = LWS3D_LWO_OK;
    if (obj->format == LWS3D_LWO_FORMAT_LWO3) {
        err = lwo_parse_lwo3_chunks(obj, data, start, end, &current_layer, 0, stats);
        lws3d_lwo_resolve_surfaces(obj);
        lws3d_lwo_build_triangles(obj, stats);
        if (obj->overflowed && err == LWS3D_LWO_OK) return LWS3D_LWO_ERR_OVERFLOW;
        return err;
    }
    while (p + 8UL <= end) {
        unsigned long csize;
        unsigned long cstart;
        unsigned long cend;
        const unsigned char *id;
        id = data + p;
        p += 4UL;
        csize = lwo_u4(data + p);
        p += 4UL;
        cstart = p;
        cend = cstart + csize;
        if (cend > end || cend < cstart) {
            lwo_stat_truncated(obj, stats);
            return LWS3D_LWO_ERR_TRUNCATED;
        }
        if (stats) {
            ++stats->chunks_seen;
        }
        if (obj->format == LWS3D_LWO_FORMAT_LWO2) {
            if (lwo_id_eq(id, "LAYR")) {
                err = lwo_parse_layr(obj, data, cstart, cend, &current_layer, stats);
            } else if (lwo_id_eq(id, "PNTS")) {
                err = lwo_parse_pnts(obj, data, cstart, cend, &current_layer, stats);
            } else if (lwo_id_eq(id, "POLS")) {
                err = lwo_parse_pols_lwo2(obj, data, cstart, cend, &current_layer, stats);
            } else if (lwo_id_eq(id, "TAGS")) {
                err = lwo_parse_tags(obj, data, cstart, cend, stats);
            } else if (lwo_id_eq(id, "PTAG")) {
                err = lwo_parse_ptag(obj, data, cstart, cend, stats);
            } else if (lwo_id_eq(id, "VMAP")) {
                err = lwo_parse_vmap_common(obj, data, cstart, cend, &current_layer, 0, stats);
            } else if (lwo_id_eq(id, "VMAD")) {
                err = lwo_parse_vmap_common(obj, data, cstart, cend, &current_layer, 1, stats);
            } else if (lwo_id_eq(id, "SURF")) {
                err = lwo_parse_surf_lwo2(obj, data, cstart, cend, stats);
            } else if (lwo_id_eq(id, "BBOX")) {
                err = lwo_parse_bbox(obj, data, cstart, cend, stats);
            } else if (lwo_id_eq(id, "CLIP")) {
                err = lwo_parse_clip(obj, data, cstart, cend, stats);
            } else {
                err = lwo_add_unknown(obj, id, csize, stats);
            }
        } else if (obj->format == LWS3D_LWO_FORMAT_LWOB) {
            if (lwo_id_eq(id, "PNTS")) {
                err = lwo_parse_pnts(obj, data, cstart, cend, &current_layer, stats);
            } else if (lwo_id_eq(id, "POLS")) {
                err = lwo_parse_pols_lwob(obj, data, cstart, cend, &current_layer, stats);
            } else if (lwo_id_eq(id, "SRFS")) {
                err = lwo_parse_srfs_lwob(obj, data, cstart, cend, stats);
            } else if (lwo_id_eq(id, "SURF")) {
                err = lwo_parse_surf_lwo2(obj, data, cstart, cend, stats);
            } else {
                err = lwo_add_unknown(obj, id, csize, stats);
            }
        } else {
            err = lwo_add_unknown(obj, id, csize, stats);
        }
        if (err != LWS3D_LWO_OK && err != LWS3D_LWO_ERR_OVERFLOW) {
            return err;
        }
        p = cend;
        if ((csize & 1UL) != 0UL && p < end) {
            ++p;
        }
    }
    if (p != end) {
        lwo_stat_truncated(obj, stats);
    }
    lws3d_lwo_resolve_surfaces(obj);
    lws3d_lwo_build_triangles(obj, stats);
    if (obj->overflowed) {
        return LWS3D_LWO_ERR_OVERFLOW;
    }
    return LWS3D_LWO_OK;
}

int lws3d_lwo_parse_buffer(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long size, LWS3D_LWO_Stats *stats)
{
    unsigned long form_size;
    unsigned long end;
    if (!obj || !data) {
        return LWS3D_LWO_ERR_BAD_ARG;
    }
    lws3d_lwo_init(obj);
    lwo_zero_stats(stats);
    if (size < 12UL || !lwo_id_eq(data, "FORM")) {
        return LWS3D_LWO_ERR_NOT_IFF;
    }
    form_size = lwo_u4(data + 4UL);
    obj->form_size = form_size;
    if (lwo_id_eq(data + 8UL, "LWO2")) {
        obj->format = LWS3D_LWO_FORMAT_LWO2;
    } else if (lwo_id_eq(data + 8UL, "LWOB")) {
        obj->format = LWS3D_LWO_FORMAT_LWOB;
    } else if (lwo_id_eq(data + 8UL, "LWO3")) {
        obj->format = LWS3D_LWO_FORMAT_LWO3;
    } else {
        obj->format = LWS3D_LWO_FORMAT_UNKNOWN;
        return LWS3D_LWO_ERR_UNSUPPORTED;
    }
    end = 8UL + form_size;
    if (end > size || end < 12UL) {
        end = size;
        lwo_stat_truncated(obj, stats);
    }
    return lwo_parse_form_chunks(obj, data, 12UL, end, stats);
}

const char *lws3d_lwo_error_name(int err)
{
    switch (err) {
    case LWS3D_LWO_OK: return "OK";
    case LWS3D_LWO_ERR_BAD_ARG: return "BAD_ARG";
    case LWS3D_LWO_ERR_NOT_IFF: return "NOT_IFF";
    case LWS3D_LWO_ERR_UNSUPPORTED: return "UNSUPPORTED";
    case LWS3D_LWO_ERR_TRUNCATED: return "TRUNCATED";
    case LWS3D_LWO_ERR_OVERFLOW: return "OVERFLOW";
    case LWS3D_LWO_ERR_CORRUPT: return "CORRUPT";
    default: return "UNKNOWN_ERROR";
    }
}

const char *lws3d_lwo_format_name(int format)
{
    switch (format) {
    case LWS3D_LWO_FORMAT_LWO2: return "LWO2";
    case LWS3D_LWO_FORMAT_LWOB: return "LWOB";
    case LWS3D_LWO_FORMAT_LWO3: return "LWO3";
    default: return "UNKNOWN";
    }
}

const char *lws3d_lwo_polygon_type_name(int type)
{
    switch (type) {
    case LWS3D_LWO_POLY_FACE: return "FACE";
    case LWS3D_LWO_POLY_CURV: return "CURV";
    case LWS3D_LWO_POLY_PTCH: return "PTCH";
    case LWS3D_LWO_POLY_MBAL: return "MBAL";
    case LWS3D_LWO_POLY_BONE: return "BONE";
    case LWS3D_LWO_POLY_OTHER: return "OTHER";
    default: return "UNKNOWN";
    }
}

const char *lws3d_lwo_block_type_name(int type)
{
    switch (type) {
    case LWS3D_LWO_BLOCK_IMAP: return "IMAP";
    case LWS3D_LWO_BLOCK_PROC: return "PROC";
    case LWS3D_LWO_BLOCK_GRAD: return "GRAD";
    case LWS3D_LWO_BLOCK_SHDR: return "SHDR";
    default: return "UNKNOWN";
    }
}

const LWS3D_LWO_Surface *lws3d_lwo_polygon_surface(const LWS3D_LWO_Object *obj, const LWS3D_LWO_Polygon *poly)
{
    if (!obj || !poly) {
        return (const LWS3D_LWO_Surface *)0;
    }
    if (poly->surface_index < 0 || poly->surface_index >= obj->surface_count) {
        return (const LWS3D_LWO_Surface *)0;
    }
    return &obj->surfaces[poly->surface_index];
}

const LWS3D_LWO_Tag *lws3d_lwo_tag_at(const LWS3D_LWO_Object *obj, int tag_index)
{
    if (!obj || tag_index < 0 || tag_index >= obj->tag_count) {
        return (const LWS3D_LWO_Tag *)0;
    }
    return &obj->tags[tag_index];
}
