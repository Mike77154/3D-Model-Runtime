#include "tmd_codec3d.h"

static int tmd_range_ok(tmd_u32 size, tmd_u32 off, tmd_u32 len)
{
    if (off > size) return 0;
    if (len > size) return 0;
    if (off > (size - len)) return 0;
    return 1;
}

static tmd_u32 tmd_read_u32le_raw(const tmd_u8 *p)
{
    tmd_u32 v;
    v = ((tmd_u32)p[0]);
    v |= ((tmd_u32)p[1]) << 8;
    v |= ((tmd_u32)p[2]) << 16;
    v |= ((tmd_u32)p[3]) << 24;
    return v;
}

static tmd_i16 tmd_i16_from_u16(tmd_u16 v)
{
    return (tmd_i16)v;
}

static tmd_i32 tmd_i32_from_u32(tmd_u32 v)
{
    return (tmd_i32)v;
}

static int tmd_read_u32(const TMD_Context *ctx, tmd_u32 off, tmd_u32 *out)
{
    if (ctx == TMD_NULL || out == TMD_NULL) return TMD_ERR_NULL;
    if (!tmd_range_ok(ctx->size, off, 4UL)) return TMD_ERR_RANGE;
    *out = tmd_read_u32le_raw(ctx->data + off);
    return TMD_ERR_OK;
}

static int tmd_read_vertex_at(const TMD_Context *ctx, tmd_u32 off, TMD_Vertex *out_vertex)
{
    tmd_u32 w0;
    tmd_u32 w1;
    if (ctx == TMD_NULL || out_vertex == TMD_NULL) return TMD_ERR_NULL;
    if (!tmd_range_ok(ctx->size, off, TMD_VERTEX_SIZE)) return TMD_ERR_RANGE;
    w0 = tmd_read_u32le_raw(ctx->data + off);
    w1 = tmd_read_u32le_raw(ctx->data + off + 4UL);
    out_vertex->x = tmd_i16_from_u16((tmd_u16)(w0 & 0xffffUL));
    out_vertex->y = tmd_i16_from_u16((tmd_u16)((w0 >> 16) & 0xffffUL));
    out_vertex->z = tmd_i16_from_u16((tmd_u16)(w1 & 0xffffUL));
    return TMD_ERR_OK;
}

static int tmd_read_normal_at(const TMD_Context *ctx, tmd_u32 off, TMD_Normal *out_normal)
{
    tmd_u32 w0;
    tmd_u32 w1;
    if (ctx == TMD_NULL || out_normal == TMD_NULL) return TMD_ERR_NULL;
    if (!tmd_range_ok(ctx->size, off, TMD_NORMAL_SIZE)) return TMD_ERR_RANGE;
    w0 = tmd_read_u32le_raw(ctx->data + off);
    w1 = tmd_read_u32le_raw(ctx->data + off + 4UL);
    out_normal->x_fx12 = tmd_i16_from_u16((tmd_u16)(w0 & 0xffffUL));
    out_normal->y_fx12 = tmd_i16_from_u16((tmd_u16)((w0 >> 16) & 0xffffUL));
    out_normal->z_fx12 = tmd_i16_from_u16((tmd_u16)(w1 & 0xffffUL));
    return TMD_ERR_OK;
}

static int tmd_section_fits(tmd_u32 size, tmd_u32 off, tmd_u32 count, tmd_u32 elem_size)
{
    if (count == 0UL) return 1;
    if (elem_size != 0UL && count > (0xffffffffUL / elem_size)) return 0;
    return tmd_range_ok(size, off, count * elem_size);
}

static int tmd_resolve_object_ptr(TMD_Context *ctx,
                                  tmd_u32 raw,
                                  tmd_u32 count,
                                  tmd_u32 elem_size,
                                  tmd_u32 *out_offset)
{
    tmd_u32 off_objbase;
    tmd_u32 off_filebase;
    int fixp;

    if (ctx == TMD_NULL || out_offset == TMD_NULL) return TMD_ERR_NULL;
    if (count == 0UL) {
        *out_offset = 0UL;
        return TMD_ERR_OK;
    }

    fixp = (int)(ctx->header.flags & 1UL);

    if (fixp) {
        /* On a dumped file there is no process address space to dereference.
           If the value already looks like a valid file offset, accept it with a warning. */
        if (tmd_section_fits(ctx->size, raw, count, elem_size)) {
            ctx->warnings |= TMD_WARN_FIXP_AS_FILE_OFFSET;
            *out_offset = raw;
            return TMD_ERR_OK;
        }
        return TMD_ERR_UNSUPPORTED_FIXP;
    }

    off_objbase = TMD_HEADER_SIZE + raw;
    if (tmd_section_fits(ctx->size, off_objbase, count, elem_size)) {
        *out_offset = off_objbase;
        return TMD_ERR_OK;
    }

    /* Some loose/repacked assets in the wild use file-start-relative offsets.
       Keep the official object-table base first, then fall back safely. */
    off_filebase = raw;
    if (tmd_section_fits(ctx->size, off_filebase, count, elem_size)) {
        ctx->warnings |= TMD_WARN_OFFSET_BASE_FALLBACK;
        *out_offset = off_filebase;
        return TMD_ERR_OK;
    }

    return TMD_ERR_RANGE;
}

int tmd_parse(TMD_Context *ctx,
              const void *bytes,
              tmd_u32 byte_count,
              TMD_Object *object_slots,
              tmd_u32 object_slot_count)
{
    tmd_u32 i;
    tmd_u32 table_size;
    int result;

    if (ctx == TMD_NULL || bytes == TMD_NULL) return TMD_ERR_NULL;

    ctx->data = (const tmd_u8 *)bytes;
    ctx->size = byte_count;
    ctx->objects = object_slots;
    ctx->object_capacity = object_slot_count;
    ctx->parsed_object_count = 0UL;
    ctx->warnings = 0UL;
    ctx->header.id = 0UL;
    ctx->header.flags = 0UL;
    ctx->header.object_count = 0UL;

    if (byte_count < TMD_HEADER_SIZE) return TMD_ERR_TOO_SMALL;

    ctx->header.id = tmd_read_u32le_raw(ctx->data + 0UL);
    ctx->header.flags = tmd_read_u32le_raw(ctx->data + 4UL);
    ctx->header.object_count = tmd_read_u32le_raw(ctx->data + 8UL);

    if (ctx->header.id != TMD_VERSION_ID) return TMD_ERR_BAD_ID;

    if (ctx->header.object_count > (0xffffffffUL / TMD_OBJECT_ENTRY_SIZE)) return TMD_ERR_RANGE;
    table_size = ctx->header.object_count * TMD_OBJECT_ENTRY_SIZE;
    if (!tmd_range_ok(ctx->size, TMD_HEADER_SIZE, table_size)) return TMD_ERR_RANGE;

    if (ctx->header.object_count > object_slot_count) {
        ctx->warnings |= TMD_WARN_OBJECT_OVERFLOW;
        return TMD_ERR_OBJECT_OVERFLOW;
    }
    if (ctx->header.object_count != 0UL && object_slots == TMD_NULL) return TMD_ERR_NULL;

    for (i = 0UL; i < ctx->header.object_count; ++i) {
        tmd_u32 off;
        TMD_Object *obj;
        off = TMD_HEADER_SIZE + (i * TMD_OBJECT_ENTRY_SIZE);
        obj = object_slots + i;

        obj->raw_vert_top = tmd_read_u32le_raw(ctx->data + off + 0UL);
        obj->n_vert = tmd_read_u32le_raw(ctx->data + off + 4UL);
        obj->raw_normal_top = tmd_read_u32le_raw(ctx->data + off + 8UL);
        obj->n_normal = tmd_read_u32le_raw(ctx->data + off + 12UL);
        obj->raw_primitive_top = tmd_read_u32le_raw(ctx->data + off + 16UL);
        obj->n_primitive = tmd_read_u32le_raw(ctx->data + off + 20UL);
        obj->scale_power = tmd_i32_from_u32(tmd_read_u32le_raw(ctx->data + off + 24UL));
        obj->vert_offset = 0UL;
        obj->normal_offset = 0UL;
        obj->primitive_offset = 0UL;

        if (obj->n_vert == 0UL && obj->n_normal == 0UL && obj->n_primitive == 0UL) {
            ctx->warnings |= TMD_WARN_EMPTY_OBJECT;
        }

        result = tmd_resolve_object_ptr(ctx, obj->raw_vert_top,
                                        obj->n_vert, TMD_VERTEX_SIZE,
                                        &obj->vert_offset);
        if (result != TMD_ERR_OK) return result;

        result = tmd_resolve_object_ptr(ctx, obj->raw_normal_top,
                                        obj->n_normal, TMD_NORMAL_SIZE,
                                        &obj->normal_offset);
        if (result != TMD_ERR_OK) return result;

        /* Primitives are variable length, so only the starting offset can be range checked here. */
        result = tmd_resolve_object_ptr(ctx, obj->raw_primitive_top,
                                        (obj->n_primitive == 0UL) ? 0UL : 1UL,
                                        4UL,
                                        &obj->primitive_offset);
        if (result != TMD_ERR_OK) return result;

        ctx->parsed_object_count++;
    }

    return TMD_ERR_OK;
}

const char *tmd_result_string(int code)
{
    switch (code) {
    case TMD_ERR_OK: return "ok";
    case TMD_ERR_NULL: return "null argument";
    case TMD_ERR_TOO_SMALL: return "file too small";
    case TMD_ERR_BAD_ID: return "bad TMD id";
    case TMD_ERR_UNSUPPORTED_FIXP: return "FIXP real-address TMD cannot be mapped from file bytes";
    case TMD_ERR_RANGE: return "out of range";
    case TMD_ERR_OBJECT_OVERFLOW: return "object output array too small";
    case TMD_ERR_BAD_PRIMITIVE: return "bad primitive packet";
    case TMD_ERR_UNSUPPORTED_PRIM: return "unsupported primitive kind";
    case TMD_ERR_OUTPUT_OVERFLOW: return "output array too small";
    case TMD_ERR_BAD_TOD: return "bad TOD animation data";
    case TMD_ERR_UNSUPPORTED_TOD_PACKET: return "unsupported TOD packet";
    case TMD_ERR_BAD_SKELETON: return "bad skeleton";
    case TMD_ERR_BAD_ANIMATION: return "bad animation";
    case TMD_ERR_BAD_HMD: return "bad HMD data";
    case TMD_ERR_BAD_MIME: return "bad MIMe data";
    default: return "unknown error";
    }
}

int tmd_get_object(const TMD_Context *ctx,
                   tmd_u32 object_index,
                   TMD_Object *out_object)
{
    if (ctx == TMD_NULL || out_object == TMD_NULL) return TMD_ERR_NULL;
    if (object_index >= ctx->parsed_object_count) return TMD_ERR_RANGE;
    *out_object = ctx->objects[object_index];
    return TMD_ERR_OK;
}

int tmd_get_vertex(const TMD_Context *ctx,
                   tmd_u32 object_index,
                   tmd_u32 vertex_index,
                   TMD_Vertex *out_vertex)
{
    const TMD_Object *obj;
    tmd_u32 off;
    if (ctx == TMD_NULL || out_vertex == TMD_NULL) return TMD_ERR_NULL;
    if (object_index >= ctx->parsed_object_count) return TMD_ERR_RANGE;
    obj = ctx->objects + object_index;
    if (vertex_index >= obj->n_vert) return TMD_ERR_RANGE;
    if (vertex_index > (0xffffffffUL / TMD_VERTEX_SIZE)) return TMD_ERR_RANGE;
    off = obj->vert_offset + (vertex_index * TMD_VERTEX_SIZE);
    return tmd_read_vertex_at(ctx, off, out_vertex);
}

int tmd_get_vertex_fx12(const TMD_Context *ctx,
                        tmd_u32 object_index,
                        tmd_u32 vertex_index,
                        tmd_i32 *out_x_fx12,
                        tmd_i32 *out_y_fx12,
                        tmd_i32 *out_z_fx12)
{
    TMD_Vertex v;
    int r;
    if (out_x_fx12 == TMD_NULL || out_y_fx12 == TMD_NULL || out_z_fx12 == TMD_NULL) return TMD_ERR_NULL;
    r = tmd_get_vertex(ctx, object_index, vertex_index, &v);
    if (r != TMD_ERR_OK) return r;
    *out_x_fx12 = ((tmd_i32)v.x) << TMD_FX_SHIFT;
    *out_y_fx12 = ((tmd_i32)v.y) << TMD_FX_SHIFT;
    *out_z_fx12 = ((tmd_i32)v.z) << TMD_FX_SHIFT;
    return TMD_ERR_OK;
}

int tmd_get_normal(const TMD_Context *ctx,
                   tmd_u32 object_index,
                   tmd_u32 normal_index,
                   TMD_Normal *out_normal)
{
    const TMD_Object *obj;
    tmd_u32 off;
    if (ctx == TMD_NULL || out_normal == TMD_NULL) return TMD_ERR_NULL;
    if (object_index >= ctx->parsed_object_count) return TMD_ERR_RANGE;
    obj = ctx->objects + object_index;
    if (normal_index >= obj->n_normal) return TMD_ERR_RANGE;
    if (normal_index > (0xffffffffUL / TMD_NORMAL_SIZE)) return TMD_ERR_RANGE;
    off = obj->normal_offset + (normal_index * TMD_NORMAL_SIZE);
    return tmd_read_normal_at(ctx, off, out_normal);
}

int tmd_get_primitive(const TMD_Context *ctx,
                      tmd_u32 object_index,
                      tmd_u32 primitive_index,
                      TMD_Primitive *out_prim)
{
    const TMD_Object *obj;
    tmd_u32 off;
    tmd_u32 i;
    tmd_u32 header;
    tmd_u32 packet_size;

    if (ctx == TMD_NULL || out_prim == TMD_NULL) return TMD_ERR_NULL;
    if (object_index >= ctx->parsed_object_count) return TMD_ERR_RANGE;
    obj = ctx->objects + object_index;
    if (primitive_index >= obj->n_primitive) return TMD_ERR_RANGE;

    off = obj->primitive_offset;
    for (i = 0UL; i <= primitive_index; ++i) {
        if (!tmd_range_ok(ctx->size, off, 4UL)) return TMD_ERR_BAD_PRIMITIVE;
        header = tmd_read_u32le_raw(ctx->data + off);
        out_prim->olen = (tmd_u8)(header & 0xffUL);
        out_prim->ilen = (tmd_u8)((header >> 8) & 0xffUL);
        out_prim->flag = (tmd_u8)((header >> 16) & 0xffUL);
        out_prim->mode = (tmd_u8)((header >> 24) & 0xffUL);
        packet_size = 4UL + ((tmd_u32)out_prim->ilen * 4UL);
        if (!tmd_range_ok(ctx->size, off, packet_size)) return TMD_ERR_BAD_PRIMITIVE;
        if (i == primitive_index) {
            out_prim->packet_offset = off;
            out_prim->packet_size = packet_size;
            return TMD_ERR_OK;
        }
        off += packet_size;
    }

    return TMD_ERR_RANGE;
}

int tmd_primitive_is_polygon(const TMD_Primitive *prim)
{
    if (prim == TMD_NULL) return 0;
    return (((prim->mode >> 5) & 7U) == 1U) ? 1 : 0;
}

int tmd_primitive_is_line(const TMD_Primitive *prim)
{
    if (prim == TMD_NULL) return 0;
    return (((prim->mode >> 5) & 7U) == 2U) ? 1 : 0;
}

int tmd_primitive_is_sprite(const TMD_Primitive *prim)
{
    if (prim == TMD_NULL) return 0;
    return (((prim->mode >> 5) & 7U) == 3U) ? 1 : 0;
}

static tmd_u32 tmd_prim_word_offset(const TMD_Primitive *prim, tmd_u32 word_index)
{
    return prim->packet_offset + 4UL + (word_index * 4UL);
}

static tmd_u16 tmd_hi16(tmd_u32 w)
{
    return (tmd_u16)((w >> 16) & 0xffffUL);
}

static tmd_u16 tmd_lo16(tmd_u32 w)
{
    return (tmd_u16)(w & 0xffffUL);
}

static void tmd_rgb_from_word(tmd_u32 w, TMD_RGB *rgb)
{
    rgb->r = (tmd_u8)(w & 0xffUL);
    rgb->g = (tmd_u8)((w >> 8) & 0xffUL);
    rgb->b = (tmd_u8)((w >> 16) & 0xffUL);
}

static void tmd_uv_from_lo16(tmd_u16 lo, TMD_UV *uv)
{
    uv->u = (tmd_u8)(lo & 0xffU);
    uv->v = (tmd_u8)((lo >> 8) & 0xffU);
}

static void tmd_face_clear(TMD_Face *f)
{
    int i;
    f->object_index = 0UL;
    f->packet_offset = 0UL;
    f->vertex_count = 0U;
    f->mode = 0U;
    f->packet_flag = 0U;
    f->ilen = 0U;
    f->olen = 0U;
    f->attrs = 0U;
    f->cba = 0U;
    f->tsb = 0U;
    for (i = 0; i < 4; ++i) {
        f->v[i] = TMD_INVALID_INDEX;
        f->n[i] = TMD_INVALID_INDEX;
        f->uv[i].u = 0U;
        f->uv[i].v = 0U;
        f->rgb[i].r = 255U;
        f->rgb[i].g = 255U;
        f->rgb[i].b = 255U;
    }
}

static int tmd_decode_uvs(const TMD_Context *ctx,
                          const TMD_Primitive *prim,
                          TMD_Face *face,
                          tmd_u32 nverts)
{
    tmd_u32 i;
    tmd_u32 w;
    tmd_u16 lo;
    tmd_u16 hi;
    for (i = 0UL; i < nverts; ++i) {
        if (tmd_read_u32(ctx, tmd_prim_word_offset(prim, i), &w) != TMD_ERR_OK) return TMD_ERR_BAD_PRIMITIVE;
        hi = tmd_hi16(w);
        lo = tmd_lo16(w);
        if (i == 0UL) face->cba = hi;
        if (i == 1UL) face->tsb = hi;
        tmd_uv_from_lo16(lo, &face->uv[i]);
    }
    return TMD_ERR_OK;
}

static int tmd_decode_colors(const TMD_Context *ctx,
                             const TMD_Primitive *prim,
                             TMD_Face *face,
                             tmd_u32 first_word,
                             tmd_u32 color_count,
                             tmd_u32 nverts)
{
    tmd_u32 i;
    tmd_u32 w;
    TMD_RGB rgb;

    if (color_count == 0UL) return TMD_ERR_OK;

    for (i = 0UL; i < color_count; ++i) {
        if (tmd_read_u32(ctx, tmd_prim_word_offset(prim, first_word + i), &w) != TMD_ERR_OK) return TMD_ERR_BAD_PRIMITIVE;
        tmd_rgb_from_word(w, &rgb);
        if (color_count == 1UL) {
            tmd_u32 k;
            for (k = 0UL; k < nverts; ++k) face->rgb[k] = rgb;
        } else {
            if (i < 4UL) face->rgb[i] = rgb;
        }
    }
    return TMD_ERR_OK;
}

static int tmd_decode_no_light_indices(const TMD_Context *ctx,
                                       const TMD_Primitive *prim,
                                       TMD_Face *face,
                                       tmd_u32 index_word,
                                       tmd_u32 nverts)
{
    tmd_u32 w0;
    tmd_u32 w1;
    if (nverts != 3UL && nverts != 4UL) return TMD_ERR_BAD_PRIMITIVE;
    if (tmd_read_u32(ctx, tmd_prim_word_offset(prim, index_word), &w0) != TMD_ERR_OK) return TMD_ERR_BAD_PRIMITIVE;
    if (tmd_read_u32(ctx, tmd_prim_word_offset(prim, index_word + 1UL), &w1) != TMD_ERR_OK) return TMD_ERR_BAD_PRIMITIVE;

    face->v[0] = tmd_lo16(w0);
    face->v[1] = tmd_hi16(w0);
    face->v[2] = tmd_lo16(w1);
    if (nverts == 4UL) face->v[3] = tmd_hi16(w1);
    return TMD_ERR_OK;
}

static int tmd_decode_light_indices(const TMD_Context *ctx,
                                    const TMD_Primitive *prim,
                                    TMD_Face *face,
                                    tmd_u32 index_word,
                                    tmd_u32 nverts,
                                    int gouraud)
{
    tmd_u32 w;
    if (nverts != 3UL && nverts != 4UL) return TMD_ERR_BAD_PRIMITIVE;

    if (gouraud) {
        tmd_u32 i;
        for (i = 0UL; i < nverts; ++i) {
            if (tmd_read_u32(ctx, tmd_prim_word_offset(prim, index_word + i), &w) != TMD_ERR_OK) return TMD_ERR_BAD_PRIMITIVE;
            face->v[i] = tmd_hi16(w);
            face->n[i] = tmd_lo16(w);
        }
    } else {
        tmd_u16 n0;
        tmd_u32 w1;
        tmd_u32 w2;
        if (tmd_read_u32(ctx, tmd_prim_word_offset(prim, index_word), &w) != TMD_ERR_OK) return TMD_ERR_BAD_PRIMITIVE;
        if (tmd_read_u32(ctx, tmd_prim_word_offset(prim, index_word + 1UL), &w1) != TMD_ERR_OK) return TMD_ERR_BAD_PRIMITIVE;
        face->v[0] = tmd_hi16(w);
        n0 = tmd_lo16(w);
        face->n[0] = n0;
        face->n[1] = n0;
        face->n[2] = n0;
        face->v[1] = tmd_lo16(w1);
        face->v[2] = tmd_hi16(w1);
        if (nverts == 4UL) {
            if (tmd_read_u32(ctx, tmd_prim_word_offset(prim, index_word + 2UL), &w2) != TMD_ERR_OK) return TMD_ERR_BAD_PRIMITIVE;
            face->v[3] = tmd_lo16(w2);
            face->n[3] = n0;
        }
    }
    return TMD_ERR_OK;
}

int tmd_decode_primitive_face(const TMD_Context *ctx,
                              tmd_u32 object_index,
                              const TMD_Primitive *prim,
                              TMD_Face *out_face)
{
    tmd_u32 nverts;
    tmd_u32 uv_count;
    tmd_u32 color_start;
    tmd_u32 color_count;
    tmd_u32 index_start;
    int textured;
    int quad;
    int gouraud;
    int no_light;
    int gradation;
    int r;

    if (ctx == TMD_NULL || prim == TMD_NULL || out_face == TMD_NULL) return TMD_ERR_NULL;
    if (!tmd_primitive_is_polygon(prim)) return TMD_ERR_UNSUPPORTED_PRIM;

    tmd_face_clear(out_face);
    out_face->object_index = object_index;
    out_face->packet_offset = prim->packet_offset;
    out_face->mode = prim->mode;
    out_face->packet_flag = prim->flag;
    out_face->ilen = prim->ilen;
    out_face->olen = prim->olen;

    quad = ((prim->mode & 0x08U) != 0U) ? 1 : 0;
    gouraud = ((prim->mode & 0x10U) != 0U) ? 1 : 0;
    textured = ((prim->mode & 0x04U) != 0U) ? 1 : 0;
    no_light = ((prim->flag & 0x01U) != 0U) ? 1 : 0;
    gradation = ((prim->flag & 0x04U) != 0U || gouraud) ? 1 : 0;

    nverts = quad ? 4UL : 3UL;
    out_face->vertex_count = quad ? TMD_FACE_QUAD : TMD_FACE_TRIANGLE;
    if (quad) out_face->attrs |= TMD_FACEF_QUAD;
    if (textured) out_face->attrs |= TMD_FACEF_TEXTURED;
    if (gouraud) out_face->attrs |= TMD_FACEF_GOURAUD;
    if (no_light) out_face->attrs |= TMD_FACEF_NO_LIGHT;
    if ((prim->flag & 0x02U) != 0U) out_face->attrs |= TMD_FACEF_DOUBLE_SIDED;
    if ((prim->mode & 0x02U) != 0U) out_face->attrs |= TMD_FACEF_SEMITRANS;
    if ((prim->mode & 0x01U) != 0U) out_face->attrs |= TMD_FACEF_TGE;
    if (gradation) out_face->attrs |= TMD_FACEF_GRADATION;

    uv_count = textured ? nverts : 0UL;
    if (textured) {
        r = tmd_decode_uvs(ctx, prim, out_face, nverts);
        if (r != TMD_ERR_OK) return r;
    }

    if (no_light) {
        color_start = uv_count;
        color_count = gradation ? nverts : 1UL;
    } else {
        color_start = uv_count;
        if (textured) color_count = 0UL;
        else color_count = ((prim->flag & 0x04U) != 0U) ? nverts : 1UL;
    }

    r = tmd_decode_colors(ctx, prim, out_face, color_start, color_count, nverts);
    if (r != TMD_ERR_OK) return r;

    index_start = uv_count + color_count;
    if (no_light) {
        r = tmd_decode_no_light_indices(ctx, prim, out_face, index_start, nverts);
    } else {
        r = tmd_decode_light_indices(ctx, prim, out_face, index_start, nverts, gouraud);
    }
    if (r != TMD_ERR_OK) return r;

    if (out_face->v[0] == TMD_INVALID_INDEX || out_face->v[1] == TMD_INVALID_INDEX || out_face->v[2] == TMD_INVALID_INDEX) {
        return TMD_ERR_BAD_PRIMITIVE;
    }

    return TMD_ERR_OK;
}

int tmd_decode_object_faces(const TMD_Context *ctx,
                            tmd_u32 object_index,
                            TMD_Face *face_slots,
                            tmd_u32 face_slot_count,
                            tmd_u32 *out_written,
                            tmd_u32 *out_needed,
                            tmd_u32 *out_unsupported)
{
    const TMD_Object *obj;
    tmd_u32 i;
    tmd_u32 written;
    tmd_u32 needed;
    tmd_u32 unsupported;
    int r;
    TMD_Primitive prim;
    TMD_Face face;

    if (ctx == TMD_NULL) return TMD_ERR_NULL;
    if (object_index >= ctx->parsed_object_count) return TMD_ERR_RANGE;
    if (face_slot_count != 0UL && face_slots == TMD_NULL) return TMD_ERR_NULL;

    obj = ctx->objects + object_index;
    written = 0UL;
    needed = 0UL;
    unsupported = 0UL;

    for (i = 0UL; i < obj->n_primitive; ++i) {
        r = tmd_get_primitive(ctx, object_index, i, &prim);
        if (r != TMD_ERR_OK) return r;
        if (!tmd_primitive_is_polygon(&prim)) {
            unsupported++;
            continue;
        }
        r = tmd_decode_primitive_face(ctx, object_index, &prim, &face);
        if (r == TMD_ERR_UNSUPPORTED_PRIM) {
            unsupported++;
            continue;
        }
        if (r != TMD_ERR_OK) return r;
        needed++;
        if (written < face_slot_count) {
            face_slots[written] = face;
            written++;
        }
    }

    if (out_written != TMD_NULL) *out_written = written;
    if (out_needed != TMD_NULL) *out_needed = needed;
    if (out_unsupported != TMD_NULL) *out_unsupported = unsupported;

    if (written < needed) return TMD_ERR_OUTPUT_OVERFLOW;
    return TMD_ERR_OK;
}
