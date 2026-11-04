#include "hmb3d.h"
#include <string.h>

static void hmb3d_set_message(hmb3d_diag *diag, const char *msg)
{
    unsigned int i;
    if (diag == 0 || msg == 0) {
        return;
    }
    i = 0U;
    while (msg[i] != '\0' && i + 1U < sizeof(diag->message)) {
        diag->message[i] = msg[i];
        i++;
    }
    diag->message[i] = '\0';
}

void hmb3d_diag_init(hmb3d_diag *diag)
{
    if (diag != 0) {
        memset(diag, 0, sizeof(*diag));
        diag->code = HMB3D_OK;
        hmb3d_set_message(diag, "ok");
    }
}

void hmb3d_model_init(hmb3d_model *model)
{
    hmb3d_u32 i;
    if (model == 0) {
        return;
    }
    memset(model, 0, sizeof(*model));
    model->bbox_min[0] = HMB3D_FIXED_MAX;
    model->bbox_min[1] = HMB3D_FIXED_MAX;
    model->bbox_min[2] = HMB3D_FIXED_MAX;
    model->bbox_max[0] = HMB3D_FIXED_MIN;
    model->bbox_max[1] = HMB3D_FIXED_MIN;
    model->bbox_max[2] = HMB3D_FIXED_MIN;
    for (i = 0U; i < HMB3D_MAX_OBJECTS; ++i) {
        model->objects[i].bbox_min[0] = HMB3D_FIXED_MAX;
        model->objects[i].bbox_min[1] = HMB3D_FIXED_MAX;
        model->objects[i].bbox_min[2] = HMB3D_FIXED_MAX;
        model->objects[i].bbox_max[0] = HMB3D_FIXED_MIN;
        model->objects[i].bbox_max[1] = HMB3D_FIXED_MIN;
        model->objects[i].bbox_max[2] = HMB3D_FIXED_MIN;
    }
}

const char *hmb3d_error_string(int code)
{
    switch (code) {
    case HMB3D_OK: return "ok";
    case HMB3D_ERR_ARGUMENT: return "bad argument";
    case HMB3D_ERR_TOO_SMALL: return "file too small";
    case HMB3D_ERR_BAD_HEADER: return "bad or unsupported header";
    case HMB3D_ERR_TRUNCATED: return "truncated data";
    case HMB3D_ERR_CAPACITY: return "fixed output capacity exceeded";
    case HMB3D_ERR_RANGE: return "range error";
    case HMB3D_ERR_UNSUPPORTED: return "unsupported data";
    case HMB3D_ERR_NO_GEOMETRY: return "no supported geometry found";
    default: return "unknown error";
    }
}

hmb3d_u16 hmb3d_read_u16_le(const hmb3d_u8 *p)
{
    return (hmb3d_u16)((hmb3d_u16)p[0] | (hmb3d_u16)((hmb3d_u16)p[1] << 8));
}

hmb3d_u16 hmb3d_read_u16_be(const hmb3d_u8 *p)
{
    return (hmb3d_u16)((hmb3d_u16)((hmb3d_u16)p[0] << 8) | (hmb3d_u16)p[1]);
}

hmb3d_u32 hmb3d_read_u32_le(const hmb3d_u8 *p)
{
    return ((hmb3d_u32)p[0]) |
           ((hmb3d_u32)p[1] << 8) |
           ((hmb3d_u32)p[2] << 16) |
           ((hmb3d_u32)p[3] << 24);
}

hmb3d_u32 hmb3d_read_u32_be(const hmb3d_u8 *p)
{
    return ((hmb3d_u32)p[0] << 24) |
           ((hmb3d_u32)p[1] << 16) |
           ((hmb3d_u32)p[2] << 8) |
           ((hmb3d_u32)p[3]);
}

static hmb3d_i16 hmb3d_read_i16(const hmb3d_u8 *p, int be)
{
    hmb3d_u16 u;
    if (be) {
        u = hmb3d_read_u16_be(p);
    } else {
        u = hmb3d_read_u16_le(p);
    }
    return (hmb3d_i16)u;
}

static hmb3d_i32 hmb3d_read_i32(const hmb3d_u8 *p, int be)
{
    hmb3d_u32 u;
    if (be) {
        u = hmb3d_read_u32_be(p);
    } else {
        u = hmb3d_read_u32_le(p);
    }
    if (u >= 2147483648UL) {
        return (hmb3d_i32)(-((hmb3d_i32)(4294967295UL - u)) - 1L);
    }
    return (hmb3d_i32)u;
}

hmb3d_fixed hmb3d_real32_bits_to_fixed(hmb3d_u32 bits, hmb3d_diag *diag)
{
    hmb3d_u32 sign;
    hmb3d_u32 exp;
    hmb3d_u32 mant;
    hmb3d_u32 value;
    int shift;
    hmb3d_u32 out;

    sign = bits >> 31;
    exp = (bits >> 23) & 255UL;
    mant = bits & 8388607UL;

    if (exp == 0UL) {
        return 0L;
    }
    if (exp == 255UL) {
        if (diag != 0) {
            diag->warning_flags |= HMB3D_WARN_REAL_SATURATED;
            diag->values_saturated++;
        }
        return sign ? HMB3D_FIXED_MIN : HMB3D_FIXED_MAX;
    }

    value = mant | 8388608UL;
    shift = (int)exp - 134;

    if (shift >= 8) {
        if (diag != 0) {
            diag->warning_flags |= HMB3D_WARN_REAL_SATURATED;
            diag->values_saturated++;
        }
        return sign ? HMB3D_FIXED_MIN : HMB3D_FIXED_MAX;
    }

    if (shift >= 0) {
        out = value << shift;
    } else {
        int rshift;
        hmb3d_u32 add;
        rshift = -shift;
        if (rshift >= 31) {
            out = 0UL;
        } else {
            add = (rshift > 0) ? (1UL << (rshift - 1)) : 0UL;
            out = (value + add) >> rshift;
        }
    }

    if (out > 2147483647UL) {
        if (diag != 0) {
            diag->warning_flags |= HMB3D_WARN_REAL_SATURATED;
            diag->values_saturated++;
        }
        return sign ? HMB3D_FIXED_MIN : HMB3D_FIXED_MAX;
    }

    if (sign) {
        return (hmb3d_fixed)(-(hmb3d_i32)out);
    }
    return (hmb3d_fixed)out;
}

static int hmb3d_has_bytes(hmb3d_u32 pos, hmb3d_u32 need, hmb3d_u32 size)
{
    if (pos > size) {
        return 0;
    }
    if (need > size - pos) {
        return 0;
    }
    return 1;
}

static int hmb3d_read_chunk(const hmb3d_u8 *data,
                            hmb3d_u32 size,
                            hmb3d_u32 pos,
                            int be,
                            hmb3d_chunk *chunk)
{
    if (!hmb3d_has_bytes(pos, 20U, size)) {
        return 0;
    }
    chunk->type[0] = (char)data[pos + 0U];
    chunk->type[1] = (char)data[pos + 1U];
    chunk->type[2] = (char)data[pos + 2U];
    chunk->type[3] = (char)data[pos + 3U];
    chunk->type[4] = '\0';
    chunk->major_version = hmb3d_read_i16(data + pos + 4U, be);
    chunk->minor_version = hmb3d_read_i16(data + pos + 6U, be);
    chunk->chunk_id = hmb3d_read_i32(data + pos + 8U, be);
    chunk->parent_id = hmb3d_read_i32(data + pos + 12U, be);
    chunk->data_offset = pos + 20U;
    chunk->data_size = (hmb3d_u32)hmb3d_read_i32(data + pos + 16U, be);
    if (!hmb3d_has_bytes(chunk->data_offset, chunk->data_size, size)) {
        return 0;
    }
    return 1;
}

static int hmb3d_chunk_type_is(const hmb3d_chunk *chunk, const char *name)
{
    return chunk->type[0] == name[0] &&
           chunk->type[1] == name[1] &&
           chunk->type[2] == name[2] &&
           chunk->type[3] == name[3];
}

static void hmb3d_store_chunk(hmb3d_model *model, const hmb3d_chunk *chunk, hmb3d_diag *diag)
{
    if (diag != 0) {
        diag->chunks_seen++;
    }
    if (model->chunk_count < HMB3D_MAX_CHUNKS) {
        model->chunks[model->chunk_count] = *chunk;
        model->chunk_count++;
    } else if (diag != 0) {
        diag->warning_flags |= HMB3D_WARN_TRUNCATED_OUTPUT;
    }
}

static hmb3d_fixed hmb3d_read_real32_fixed(const hmb3d_u8 *p, int be, hmb3d_diag *diag)
{
    hmb3d_u32 bits;
    if (be) {
        bits = hmb3d_read_u32_be(p);
    } else {
        bits = hmb3d_read_u32_le(p);
    }
    return hmb3d_real32_bits_to_fixed(bits, diag);
}

static void hmb3d_add_bbox(hmb3d_fixed *mn, hmb3d_fixed *mx, hmb3d_fixed x, hmb3d_fixed y, hmb3d_fixed z)
{
    if (x < mn[0]) { mn[0] = x; }
    if (y < mn[1]) { mn[1] = y; }
    if (z < mn[2]) { mn[2] = z; }
    if (x > mx[0]) { mx[0] = x; }
    if (y > mx[1]) { mx[1] = y; }
    if (z > mx[2]) { mx[2] = z; }
}

static int hmb3d_read_bytes(const hmb3d_u8 *data,
                            hmb3d_u32 start,
                            hmb3d_u32 end,
                            hmb3d_u32 *pos,
                            hmb3d_u32 need)
{
    (void)data;
    if (*pos < start) {
        return 0;
    }
    if (*pos > end) {
        return 0;
    }
    if (need > end - *pos) {
        return 0;
    }
    return 1;
}

static int hmb3d_skip(hmb3d_u32 end, hmb3d_u32 *pos, hmb3d_u32 amount)
{
    if (*pos > end) {
        return 0;
    }
    if (amount > end - *pos) {
        return 0;
    }
    *pos += amount;
    return 1;
}

static void hmb3d_copy_name(char *dst, const hmb3d_u8 *src, hmb3d_u32 len)
{
    hmb3d_u32 i;
    hmb3d_u32 max;
    max = HMB3D_NAME_BYTES - 1U;
    i = 0U;
    while (i < len && i < max) {
        hmb3d_u8 c;
        c = src[i];
        if (c < 32U || c > 126U) {
            dst[i] = '_';
        } else {
            dst[i] = (char)c;
        }
        i++;
    }
    dst[i] = '\0';
    if (i == 0U) {
        dst[0] = 'o';
        dst[1] = 'b';
        dst[2] = 'j';
        dst[3] = '\0';
    }
}

static int hmb3d_append_vertex(hmb3d_model *model,
                               hmb3d_object *obj,
                               hmb3d_fixed x,
                               hmb3d_fixed y,
                               hmb3d_fixed z,
                               hmb3d_diag *diag)
{
    if (model->vertex_count >= HMB3D_MAX_VERTICES) {
        if (diag != 0) {
            diag->warning_flags |= HMB3D_WARN_TRUNCATED_OUTPUT;
            diag->clipped_vertices++;
        }
        return 0;
    }
    model->vertices[model->vertex_count].x = x;
    model->vertices[model->vertex_count].y = y;
    model->vertices[model->vertex_count].z = z;
    hmb3d_add_bbox(model->bbox_min, model->bbox_max, x, y, z);
    hmb3d_add_bbox(obj->bbox_min, obj->bbox_max, x, y, z);
    model->vertex_count++;
    obj->vertex_count++;
    return 1;
}

static int hmb3d_append_uv(hmb3d_model *model,
                           hmb3d_object *obj,
                           hmb3d_fixed u,
                           hmb3d_fixed v,
                           hmb3d_diag *diag)
{
    if (model->uv_count >= HMB3D_MAX_UVS) {
        if (diag != 0) {
            diag->warning_flags |= HMB3D_WARN_TRUNCATED_OUTPUT;
            diag->clipped_uvs++;
        }
        return 0;
    }
    model->uvs[model->uv_count].u = u;
    model->uvs[model->uv_count].v = v;
    model->uv_count++;
    obj->uv_count++;
    return 1;
}

static int hmb3d_append_corner(hmb3d_model *model,
                               hmb3d_u32 vertex,
                               hmb3d_u32 uv,
                               hmb3d_diag *diag)
{
    if (model->corner_count >= HMB3D_MAX_CORNERS) {
        if (diag != 0) {
            diag->warning_flags |= HMB3D_WARN_TRUNCATED_OUTPUT;
            diag->clipped_corners++;
        }
        return 0;
    }
    model->corners[model->corner_count].vertex = vertex;
    model->corners[model->corner_count].uv = uv;
    model->corner_count++;
    return 1;
}

static int hmb3d_append_face(hmb3d_model *model,
                             hmb3d_object *obj,
                             hmb3d_u16 object_index,
                             hmb3d_u32 first_corner,
                             hmb3d_u16 corner_count,
                             hmb3d_i16 material_index,
                             hmb3d_u8 flags,
                             hmb3d_diag *diag)
{
    if (model->face_count >= HMB3D_MAX_FACES) {
        if (diag != 0) {
            diag->warning_flags |= HMB3D_WARN_TRUNCATED_OUTPUT;
            diag->clipped_faces++;
        }
        return 0;
    }
    model->faces[model->face_count].first_corner = first_corner;
    model->faces[model->face_count].corner_count = corner_count;
    model->faces[model->face_count].material_index = material_index;
    model->faces[model->face_count].flags = flags;
    model->faces[model->face_count].object_index = object_index;
    model->face_count++;
    obj->face_count++;
    return 1;
}

static int hmb3d_parse_polh(const hmb3d_u8 *data,
                            hmb3d_u32 size,
                            const hmb3d_chunk *chunk,
                            int be,
                            hmb3d_model *model,
                            hmb3d_diag *diag)
{
    hmb3d_u32 pos;
    hmb3d_u32 end;
    hmb3d_i16 dupe;
    hmb3d_i16 slen_signed;
    hmb3d_u32 slen;
    hmb3d_u32 i;
    hmb3d_i32 nverts;
    hmb3d_i32 nuvs;
    hmb3d_i32 nfaces;
    hmb3d_object *obj;
    hmb3d_u16 obj_index;

    (void)size;
    if (model->object_count >= HMB3D_MAX_OBJECTS) {
        if (diag != 0) {
            diag->warning_flags |= HMB3D_WARN_TRUNCATED_OUTPUT;
        }
        return HMB3D_ERR_CAPACITY;
    }

    obj_index = (hmb3d_u16)model->object_count;
    obj = &model->objects[model->object_count];
    memset(obj, 0, sizeof(*obj));
    obj->bbox_min[0] = HMB3D_FIXED_MAX;
    obj->bbox_min[1] = HMB3D_FIXED_MAX;
    obj->bbox_min[2] = HMB3D_FIXED_MAX;
    obj->bbox_max[0] = HMB3D_FIXED_MIN;
    obj->bbox_max[1] = HMB3D_FIXED_MIN;
    obj->bbox_max[2] = HMB3D_FIXED_MIN;
    obj->source_chunk_offset = chunk->data_offset - 20U;
    obj->source_chunk_id = (hmb3d_u32)chunk->chunk_id;
    obj->parent_chunk_id = chunk->parent_id;
    obj->first_vertex = model->vertex_count;
    obj->first_uv = model->uv_count;
    obj->first_face = model->face_count;
    obj->name[0] = 'o';
    obj->name[1] = 'b';
    obj->name[2] = 'j';
    obj->name[3] = '\0';

    pos = chunk->data_offset;
    end = chunk->data_offset + chunk->data_size;

    if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, 4U)) {
        return HMB3D_ERR_TRUNCATED;
    }
    dupe = hmb3d_read_i16(data + pos, be);
    (void)dupe;
    slen_signed = hmb3d_read_i16(data + pos + 2U, be);
    pos += 4U;
    if (slen_signed < 0) {
        return HMB3D_ERR_RANGE;
    }
    slen = (hmb3d_u32)slen_signed;
    if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, slen)) {
        return HMB3D_ERR_TRUNCATED;
    }
    hmb3d_copy_name(obj->name, data + pos, slen);
    pos += slen;

    if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, 48U)) {
        return HMB3D_ERR_TRUNCATED;
    }
    for (i = 0U; i < 12U; ++i) {
        obj->axes[i] = hmb3d_read_real32_fixed(data + pos + i * 4U, be, diag);
    }
    pos += 48U;

    if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, 48U)) {
        return HMB3D_ERR_TRUNCATED;
    }
    for (i = 0U; i < 12U; ++i) {
        obj->transform[i] = hmb3d_read_real32_fixed(data + pos + i * 4U, be, diag);
    }
    pos += 48U;

    if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, 4U)) {
        return HMB3D_ERR_TRUNCATED;
    }
    nverts = hmb3d_read_i32(data + pos, be);
    pos += 4U;
    if (nverts < 0 || nverts > 100000000L) {
        return HMB3D_ERR_RANGE;
    }
    for (i = 0U; i < (hmb3d_u32)nverts; ++i) {
        hmb3d_fixed x;
        hmb3d_fixed y;
        hmb3d_fixed z;
        if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, 12U)) {
            return HMB3D_ERR_TRUNCATED;
        }
        x = hmb3d_read_real32_fixed(data + pos, be, diag);
        y = hmb3d_read_real32_fixed(data + pos + 4U, be, diag);
        z = hmb3d_read_real32_fixed(data + pos + 8U, be, diag);
        hmb3d_append_vertex(model, obj, x, y, z, diag);
        pos += 12U;
    }

    if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, 4U)) {
        return HMB3D_ERR_TRUNCATED;
    }
    nuvs = hmb3d_read_i32(data + pos, be);
    pos += 4U;
    if (nuvs < 0 || nuvs > 100000000L) {
        return HMB3D_ERR_RANGE;
    }
    for (i = 0U; i < (hmb3d_u32)nuvs; ++i) {
        hmb3d_fixed u;
        hmb3d_fixed v;
        if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, 8U)) {
            return HMB3D_ERR_TRUNCATED;
        }
        u = hmb3d_read_real32_fixed(data + pos, be, diag);
        v = hmb3d_read_real32_fixed(data + pos + 4U, be, diag);
        hmb3d_append_uv(model, obj, u, v, diag);
        pos += 8U;
    }

    if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, 4U)) {
        return HMB3D_ERR_TRUNCATED;
    }
    nfaces = hmb3d_read_i32(data + pos, be);
    pos += 4U;
    if (nfaces < 0 || nfaces > 100000000L) {
        return HMB3D_ERR_RANGE;
    }

    for (i = 0U; i < (hmb3d_u32)nfaces; ++i) {
        hmb3d_u8 flags;
        hmb3d_i16 count_signed;
        hmb3d_i16 material_index;
        hmb3d_u32 j;
        hmb3d_u32 first_corner;
        hmb3d_u16 count;
        hmb3d_u32 corners_written;

        if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, 3U)) {
            return HMB3D_ERR_TRUNCATED;
        }
        flags = data[pos];
        count_signed = hmb3d_read_i16(data + pos + 1U, be);
        pos += 3U;
        if (count_signed < 0) {
            return HMB3D_ERR_RANGE;
        }
        count = (hmb3d_u16)count_signed;
        material_index = -1;
        if ((flags & HMB3D_FACE_HOLE) == 0U) {
            if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, 2U)) {
                return HMB3D_ERR_TRUNCATED;
            }
            material_index = hmb3d_read_i16(data + pos, be);
            pos += 2U;
        } else if (diag != 0) {
            diag->warning_flags |= HMB3D_WARN_HOLE_FACE_SKIPPED;
            diag->faces_hole_skipped++;
        }
        if (!hmb3d_read_bytes(data, chunk->data_offset, end, &pos, ((hmb3d_u32)count) * 8U)) {
            return HMB3D_ERR_TRUNCATED;
        }

        first_corner = model->corner_count;
        corners_written = 0U;
        for (j = 0U; j < (hmb3d_u32)count; ++j) {
            hmb3d_i32 vi_signed;
            hmb3d_i32 ui_signed;
            hmb3d_u32 vi;
            hmb3d_u32 ui;
            vi_signed = hmb3d_read_i32(data + pos, be);
            ui_signed = hmb3d_read_i32(data + pos + 4U, be);
            pos += 8U;
            if (vi_signed < 0 || (hmb3d_u32)vi_signed >= obj->vertex_count) {
                vi = HMB3D_INVALID_INDEX;
                if (diag != 0) {
                    diag->warning_flags |= HMB3D_WARN_INDEX_RANGE;
                    diag->bad_indices++;
                }
            } else {
                vi = obj->first_vertex + (hmb3d_u32)vi_signed;
            }
            if (ui_signed < 0 || (hmb3d_u32)ui_signed >= obj->uv_count) {
                ui = HMB3D_INVALID_INDEX;
                if (ui_signed >= 0 && diag != 0) {
                    diag->warning_flags |= HMB3D_WARN_INDEX_RANGE;
                    diag->bad_indices++;
                }
            } else {
                ui = obj->first_uv + (hmb3d_u32)ui_signed;
            }
            if (hmb3d_append_corner(model, vi, ui, diag)) {
                corners_written++;
            }
        }
        if ((flags & HMB3D_FACE_HOLE) == 0U && corners_written >= 2U) {
            hmb3d_append_face(model, obj, obj_index, first_corner,
                              (hmb3d_u16)corners_written, material_index, flags, diag);
        }
    }

    if (chunk->minor_version > 4) {
        if (!hmb3d_skip(end, &pos, 4U)) {
            return HMB3D_ERR_TRUNCATED;
        }
        if (chunk->minor_version > 5 && chunk->minor_version < 8) {
            if (!hmb3d_skip(end, &pos, 2U)) {
                return HMB3D_ERR_TRUNCATED;
            }
        }
    }

    model->object_count++;
    if (diag != 0) {
        diag->polh_seen++;
    }
    return HMB3D_OK;
}

static int hmb3d_parse_chunk_stream(const hmb3d_u8 *data,
                                    hmb3d_u32 size,
                                    hmb3d_u32 start,
                                    int be,
                                    hmb3d_model *model,
                                    hmb3d_diag *diag)
{
    hmb3d_u32 pos;
    int code;
    pos = start;
    code = HMB3D_OK;

    while (pos + 20U <= size) {
        hmb3d_chunk chunk;
        if (!hmb3d_read_chunk(data, size, pos, be, &chunk)) {
            if (diag != 0) {
                diag->warning_flags |= HMB3D_WARN_UNKNOWN_CHUNKS;
            }
            break;
        }
        hmb3d_store_chunk(model, &chunk, diag);
        if (hmb3d_chunk_type_is(&chunk, "END ") || hmb3d_chunk_type_is(&chunk, "End ")) {
            pos = chunk.data_offset + chunk.data_size;
            break;
        }
        if (hmb3d_chunk_type_is(&chunk, "PolH") || hmb3d_chunk_type_is(&chunk, "POLH")) {
            code = hmb3d_parse_polh(data, size, &chunk, be, model, diag);
            if (code != HMB3D_OK) {
                if (diag != 0) {
                    diag->code = code;
                    hmb3d_set_message(diag, hmb3d_error_string(code));
                }
                return code;
            }
        } else if (diag != 0) {
            diag->chunks_unknown++;
            diag->warning_flags |= HMB3D_WARN_UNKNOWN_CHUNKS;
        }
        pos = chunk.data_offset + chunk.data_size;
    }
    if (diag != 0) {
        diag->bytes_read = pos;
    }
    return HMB3D_OK;
}

static int hmb3d_header_is_caligari(const hmb3d_u8 *data, hmb3d_u32 size)
{
    if (size < 32U) {
        return 0;
    }
    if (memcmp(data, "Caligari ", 9U) == 0) {
        return 1;
    }
    return 0;
}

static int hmb3d_guess_be(const hmb3d_u8 *data, hmb3d_u32 size)
{
    if (hmb3d_header_is_caligari(data, size)) {
        if (data[16] == (hmb3d_u8)'H' && data[17] == (hmb3d_u8)'L') {
            return 1;
        }
    }
    return 0;
}

static int hmb3d_scan_for_polh(const hmb3d_u8 *data,
                               hmb3d_u32 size,
                               hmb3d_model *model,
                               hmb3d_diag *diag)
{
    hmb3d_u32 pos;
    int tried;
    tried = 0;
    pos = 0U;
    while (pos + 20U <= size) {
        if ((data[pos] == (hmb3d_u8)'P' || data[pos] == (hmb3d_u8)'p') &&
            (data[pos + 1U] == (hmb3d_u8)'o' || data[pos + 1U] == (hmb3d_u8)'O') &&
            (data[pos + 2U] == (hmb3d_u8)'l' || data[pos + 2U] == (hmb3d_u8)'L') &&
            (data[pos + 3U] == (hmb3d_u8)'H' || data[pos + 3U] == (hmb3d_u8)'h')) {
            hmb3d_chunk chunk;
            if (hmb3d_read_chunk(data, size, pos, 0, &chunk)) {
                tried = 1;
                if (diag != 0) {
                    diag->warning_flags |= HMB3D_WARN_HEADERLESS_SCAN;
                }
                hmb3d_store_chunk(model, &chunk, diag);
                if (hmb3d_parse_polh(data, size, &chunk, 0, model, diag) == HMB3D_OK) {
                    pos = chunk.data_offset + chunk.data_size;
                    continue;
                }
            }
            if (hmb3d_read_chunk(data, size, pos, 1, &chunk)) {
                tried = 1;
                if (diag != 0) {
                    diag->warning_flags |= HMB3D_WARN_HEADERLESS_SCAN | HMB3D_WARN_BIG_ENDIAN;
                }
                hmb3d_store_chunk(model, &chunk, diag);
                if (hmb3d_parse_polh(data, size, &chunk, 1, model, diag) == HMB3D_OK) {
                    pos = chunk.data_offset + chunk.data_size;
                    continue;
                }
            }
        }
        pos++;
    }
    if (model->object_count > 0U) {
        return HMB3D_OK;
    }
    return tried ? HMB3D_ERR_NO_GEOMETRY : HMB3D_ERR_BAD_HEADER;
}


static int hmb3d_is_hmp_magic(const hmb3d_u8 *data, int *version, int *reversed)
{
    if (data == 0 || version == 0 || reversed == 0) {
        return 0;
    }
    *reversed = 0;
    if (data[0] == (hmb3d_u8)'H' && data[1] == (hmb3d_u8)'M' && data[2] == (hmb3d_u8)'P') {
        if (data[3] == (hmb3d_u8)'4' || data[3] == (hmb3d_u8)'5' || data[3] == (hmb3d_u8)'7') {
            *version = (int)(data[3] - (hmb3d_u8)'0');
            return 1;
        }
    }
    if (data[1] == (hmb3d_u8)'P' && data[2] == (hmb3d_u8)'M' && data[3] == (hmb3d_u8)'H') {
        if (data[0] == (hmb3d_u8)'4' || data[0] == (hmb3d_u8)'5' || data[0] == (hmb3d_u8)'7') {
            *version = (int)(data[0] - (hmb3d_u8)'0');
            *reversed = 1;
            return 1;
        }
    }
    return 0;
}

static hmb3d_u32 hmb3d_fixed_to_u32_round(hmb3d_fixed v)
{
    hmb3d_u32 mag;
    if (v <= 0L) {
        return 0U;
    }
    mag = (hmb3d_u32)v;
    return (mag + 32768UL) >> HMB3D_FIXED_SHIFT;
}

static hmb3d_fixed hmb3d_fixed_add_sat(hmb3d_fixed a, hmb3d_fixed b, hmb3d_diag *diag)
{
    if (b > 0L && a > HMB3D_FIXED_MAX - b) {
        if (diag != 0) { diag->warning_flags |= HMB3D_WARN_REAL_SATURATED; diag->values_saturated++; }
        return HMB3D_FIXED_MAX;
    }
    if (b < 0L && a < HMB3D_FIXED_MIN - b) {
        if (diag != 0) { diag->warning_flags |= HMB3D_WARN_REAL_SATURATED; diag->values_saturated++; }
        return HMB3D_FIXED_MIN;
    }
    return a + b;
}

static hmb3d_fixed hmb3d_fixed_mul_u32_sat(hmb3d_fixed a, hmb3d_u32 b, hmb3d_diag *diag)
{
    hmb3d_fixed result;
    hmb3d_fixed add;
    result = 0L;
    add = a;
    while (b != 0U) {
        if ((b & 1UL) != 0U) {
            result = hmb3d_fixed_add_sat(result, add, diag);
        }
        b >>= 1;
        if (b != 0U) {
            add = hmb3d_fixed_add_sat(add, add, diag);
        }
    }
    return result;
}

static hmb3d_fixed hmb3d_fixed_mul_i32_sat(hmb3d_fixed a, hmb3d_i32 n, hmb3d_diag *diag)
{
    hmb3d_u32 mag;
    hmb3d_fixed r;
    if (n < 0L) {
        mag = (hmb3d_u32)(-n);
        r = hmb3d_fixed_mul_u32_sat(a, mag, diag);
        if (r == HMB3D_FIXED_MIN) { return HMB3D_FIXED_MAX; }
        return -r;
    }
    return hmb3d_fixed_mul_u32_sat(a, (hmb3d_u32)n, diag);
}

static hmb3d_fixed hmb3d_hmp_height_from_z(hmb3d_u16 z, hmb3d_fixed tri_x, hmb3d_diag *diag)
{
    hmb3d_i32 diff;
    hmb3d_fixed q;
    hmb3d_fixed r;
    hmb3d_i32 rem;
    hmb3d_fixed out;
    diff = (hmb3d_i32)z - 32768L;
    q = tri_x / 8192L;
    rem = tri_x % 8192L;
    out = hmb3d_fixed_mul_i32_sat(q, diff, diag);
    r = (hmb3d_fixed)((rem * diff) / 8192L);
    out = hmb3d_fixed_add_sat(out, r, diag);
    return out;
}

static hmb3d_fixed hmb3d_fixed_ratio_u32(hmb3d_u32 num, hmb3d_u32 den)
{
    if (den == 0U) {
        return 0L;
    }
    if (num > 65535UL) {
        return HMB3D_FIXED_MAX;
    }
    return (hmb3d_fixed)((num << HMB3D_FIXED_SHIFT) / den);
}

static int hmb3d_hmp_read_i32_le(const hmb3d_u8 *data, hmb3d_u32 off, hmb3d_i32 *out)
{
    if (out == 0) { return 0; }
    *out = hmb3d_read_i32(data + off, 0);
    return 1;
}

static int hmb3d_hmp_skin_payload_size(hmb3d_u32 typ, hmb3d_u32 w, hmb3d_u32 h, hmb3d_u32 *out)
{
    hmb3d_u32 px;
    if (out == 0) { return 0; }
    if (w == 0U || h == 0U || w > 16384UL || h > 16384UL) {
        return 0;
    }
    if (w > 4294967295UL / h) {
        return 0;
    }
    px = w * h;
    if (typ == 1U || typ == 2U || typ == 3U) {
        *out = px;
        return 1;
    }
    if (typ == 4U) {
        if (px > 1073741823UL) { return 0; }
        *out = px * 4U;
        return 1;
    }
    if (typ == 5U || typ == 6U) {
        if (px > 1431655765UL) { return 0; }
        *out = px * 3U;
        return 1;
    }
    return 0;
}

static int hmb3d_hmp_try_skip_skins(const hmb3d_u8 *data,
                                    hmb3d_u32 size,
                                    hmb3d_u32 numskins,
                                    hmb3d_u32 *frame_off,
                                    hmb3d_diag *diag)
{
    hmb3d_u32 i;
    hmb3d_u32 pos;
    pos = 84U;
    for (i = 0U; i < numskins; ++i) {
        hmb3d_u32 typ;
        hmb3d_u32 w;
        hmb3d_u32 h;
        hmb3d_u32 payload;
        if (!hmb3d_has_bytes(pos, 12U, size)) {
            return 0;
        }
        typ = hmb3d_read_u32_le(data + pos);
        pos += 4U;
        if (typ == 0U) {
            if (!hmb3d_has_bytes(pos, 12U, size)) {
                return 0;
            }
            pos += 8U;
            typ = hmb3d_read_u32_le(data + pos);
            pos += 4U;
        }
        if (!hmb3d_has_bytes(pos, 8U, size)) {
            return 0;
        }
        w = hmb3d_read_u32_le(data + pos);
        pos += 4U;
        h = hmb3d_read_u32_le(data + pos);
        pos += 4U;
        if (!hmb3d_hmp_skin_payload_size(typ, w, h, &payload)) {
            return 0;
        }
        if (!hmb3d_has_bytes(pos, payload, size)) {
            return 0;
        }
        pos += payload;
    }
    if (!hmb3d_has_bytes(pos, 36U, size)) {
        return 0;
    }
    *frame_off = pos;
    if (diag != 0 && numskins != 0U) {
        diag->warning_flags |= HMB3D_WARN_HMP_SKINS_SKIPPED;
    }
    return 1;
}

static int hmb3d_hmp_find_frame_offset(const hmb3d_u8 *data,
                                       hmb3d_u32 size,
                                       hmb3d_u32 numskins,
                                       hmb3d_u32 vert_bytes,
                                       hmb3d_u32 *frame_off,
                                       hmb3d_diag *diag)
{
    hmb3d_u32 q;
    if (numskins == 0U) {
        if (hmb3d_has_bytes(84U, 36U + vert_bytes, size)) {
            *frame_off = 84U;
            return 1;
        }
        return 0;
    }
    if (hmb3d_hmp_try_skip_skins(data, size, numskins, frame_off, diag)) {
        if (hmb3d_has_bytes(*frame_off, 36U + vert_bytes, size)) {
            return 1;
        }
    }
    q = 84U;
    while (hmb3d_has_bytes(q, 36U + vert_bytes, size)) {
        hmb3d_u32 tag;
        tag = hmb3d_read_u32_le(data + q);
        if (tag == 2UL || tag == 1UL || tag == 0UL) {
            *frame_off = q;
            if (diag != 0) {
                diag->warning_flags |= HMB3D_WARN_HMP_SKINS_SKIPPED;
            }
            return 1;
        }
        q++;
    }
    return 0;
}

static int hmb3d_parse_hmp_terrain(const hmb3d_u8 *data,
                                   hmb3d_u32 size,
                                   hmb3d_model *model,
                                   hmb3d_diag *diag)
{
    int version;
    int reversed;
    hmb3d_i32 numskins_i;
    hmb3d_i32 numverts_i;
    hmb3d_i32 numframes_i;
    hmb3d_fixed tri_x;
    hmb3d_fixed tri_y;
    hmb3d_fixed width_fixed;
    hmb3d_u32 width;
    hmb3d_u32 height;
    hmb3d_u32 numverts;
    hmb3d_u32 numskins;
    hmb3d_u32 vert_stride;
    hmb3d_u32 vert_bytes;
    hmb3d_u32 frame_off;
    hmb3d_u32 vert_off;
    hmb3d_u32 x;
    hmb3d_u32 y;
    hmb3d_object *obj;
    hmb3d_u16 obj_index;

    if (!hmb3d_is_hmp_magic(data, &version, &reversed)) {
        return HMB3D_ERR_BAD_HEADER;
    }
    if (reversed) {
        return HMB3D_ERR_UNSUPPORTED;
    }
    if (size < 120U) {
        return HMB3D_ERR_TOO_SMALL;
    }
    hmb3d_hmp_read_i32_le(data, 48U, &numskins_i); /* overwritten below by fixed width? guard not used */
    tri_x = hmb3d_read_real32_fixed(data + 36U, 0, diag);
    tri_y = hmb3d_read_real32_fixed(data + 40U, 0, diag);
    width_fixed = hmb3d_read_real32_fixed(data + 44U, 0, diag);
    numskins_i = hmb3d_read_i32(data + 48U, 0);
    numverts_i = hmb3d_read_i32(data + 60U, 0);
    numframes_i = hmb3d_read_i32(data + 68U, 0);

    if (tri_x == 0L || tri_y == 0L || width_fixed <= 0L || numverts_i <= 0L || numframes_i <= 0L) {
        return HMB3D_ERR_BAD_HEADER;
    }
    if (numskins_i < 0L || numskins_i > 64L) {
        return HMB3D_ERR_RANGE;
    }
    numskins = (hmb3d_u32)numskins_i;
    numverts = (hmb3d_u32)numverts_i;
    width = hmb3d_fixed_to_u32_round(width_fixed);
    if (width == 0U || width > numverts || width > HMB3D_MAX_VERTICES) {
        return HMB3D_ERR_RANGE;
    }
    if ((numverts / width) == 0U || (numverts % width) != 0U) {
        return HMB3D_ERR_RANGE;
    }
    height = numverts / width;
    if (height == 0U || height > HMB3D_MAX_VERTICES) {
        return HMB3D_ERR_RANGE;
    }
    if (numverts > HMB3D_MAX_VERTICES) {
        if (diag != 0) { diag->warning_flags |= HMB3D_WARN_TRUNCATED_OUTPUT; }
    }
    vert_stride = (version == 4) ? 8U : 4U;
    if (numverts > 4294967295UL / vert_stride) {
        return HMB3D_ERR_RANGE;
    }
    vert_bytes = numverts * vert_stride;
    if (!hmb3d_hmp_find_frame_offset(data, size, numskins, vert_bytes, &frame_off, diag)) {
        return HMB3D_ERR_TRUNCATED;
    }
    vert_off = frame_off + 36U;
    if (!hmb3d_has_bytes(vert_off, vert_bytes, size)) {
        return HMB3D_ERR_TRUNCATED;
    }
    if (model->object_count >= HMB3D_MAX_OBJECTS) {
        return HMB3D_ERR_CAPACITY;
    }
    obj_index = (hmb3d_u16)model->object_count;
    obj = &model->objects[model->object_count];
    memset(obj, 0, sizeof(*obj));
    obj->bbox_min[0] = HMB3D_FIXED_MAX;
    obj->bbox_min[1] = HMB3D_FIXED_MAX;
    obj->bbox_min[2] = HMB3D_FIXED_MAX;
    obj->bbox_max[0] = HMB3D_FIXED_MIN;
    obj->bbox_max[1] = HMB3D_FIXED_MIN;
    obj->bbox_max[2] = HMB3D_FIXED_MIN;
    obj->source_chunk_offset = 0U;
    obj->source_chunk_id = (hmb3d_u32)version;
    obj->parent_chunk_id = -1L;
    obj->first_vertex = model->vertex_count;
    obj->first_uv = model->uv_count;
    obj->first_face = model->face_count;
    obj->name[0] = 'h'; obj->name[1] = 'm'; obj->name[2] = 'p'; obj->name[3] = '_';
    obj->name[4] = (char)('0' + version); obj->name[5] = '_'; obj->name[6] = 't'; obj->name[7] = 'e';
    obj->name[8] = 'r'; obj->name[9] = 'r'; obj->name[10] = 'a'; obj->name[11] = 'i'; obj->name[12] = 'n'; obj->name[13] = '\0';

    for (y = 0U; y < height; ++y) {
        for (x = 0U; x < width; ++x) {
            hmb3d_u32 idx;
            hmb3d_u32 p;
            hmb3d_u16 zraw;
            hmb3d_fixed vx;
            hmb3d_fixed vy;
            hmb3d_fixed vz;
            idx = y * width + x;
            p = vert_off + idx * vert_stride;
            if (version == 4) {
                zraw = hmb3d_read_u16_le(data + p + 4U);
            } else {
                zraw = hmb3d_read_u16_le(data + p);
            }
            vx = hmb3d_fixed_mul_u32_sat(tri_x, x, diag);
            vy = hmb3d_fixed_mul_u32_sat(tri_y, y, diag);
            vz = hmb3d_hmp_height_from_z(zraw, tri_x, diag);
            hmb3d_append_vertex(model, obj, vx, vy, vz, diag);
            hmb3d_append_uv(model, obj,
                            hmb3d_fixed_ratio_u32(x, (width > 1U) ? (width - 1U) : 1U),
                            hmb3d_fixed_ratio_u32(y, (height > 1U) ? (height - 1U) : 1U),
                            diag);
        }
    }

    if (width > 1U && height > 1U) {
        for (y = 0U; y + 1U < height; ++y) {
            for (x = 0U; x + 1U < width; ++x) {
                hmb3d_u32 first_corner;
                hmb3d_u32 v0;
                hmb3d_u32 v1;
                hmb3d_u32 v2;
                hmb3d_u32 v3;
                first_corner = model->corner_count;
                v0 = obj->first_vertex + y * width + x;
                v1 = obj->first_vertex + (y + 1U) * width + x;
                v2 = obj->first_vertex + (y + 1U) * width + x + 1U;
                v3 = obj->first_vertex + y * width + x + 1U;
                hmb3d_append_corner(model, v0, obj->first_uv + y * width + x, diag);
                hmb3d_append_corner(model, v1, obj->first_uv + (y + 1U) * width + x, diag);
                hmb3d_append_corner(model, v2, obj->first_uv + (y + 1U) * width + x + 1U, diag);
                hmb3d_append_corner(model, v3, obj->first_uv + y * width + x + 1U, diag);
                hmb3d_append_face(model, obj, obj_index, first_corner, 4U, 0, 0U, diag);
            }
        }
    }

    model->object_count++;
    if (diag != 0) {
        diag->hmp_seen++;
        diag->warning_flags |= HMB3D_WARN_ASSIMP_HMP;
        if (version == 4) {
            diag->warning_flags |= HMB3D_WARN_HMP4_PARTIAL;
        }
    }
    return HMB3D_OK;
}

int hmb3d_load_memory(const hmb3d_u8 *data,
                      hmb3d_u32 size,
                      hmb3d_model *model,
                      hmb3d_diag *diag)
{
    int code;
    int be;

    if (diag != 0) {
        hmb3d_diag_init(diag);
    }
    if (model == 0 || data == 0) {
        if (diag != 0) {
            diag->code = HMB3D_ERR_ARGUMENT;
            hmb3d_set_message(diag, hmb3d_error_string(HMB3D_ERR_ARGUMENT));
        }
        return HMB3D_ERR_ARGUMENT;
    }
    hmb3d_model_init(model);
    if (size < 20U) {
        if (diag != 0) {
            diag->code = HMB3D_ERR_TOO_SMALL;
            hmb3d_set_message(diag, hmb3d_error_string(HMB3D_ERR_TOO_SMALL));
        }
        return HMB3D_ERR_TOO_SMALL;
    }

    {
        int hmp_version;
        int hmp_reversed;
        if (hmb3d_is_hmp_magic(data, &hmp_version, &hmp_reversed)) {
            (void)hmp_version;
            (void)hmp_reversed;
            code = hmb3d_parse_hmp_terrain(data, size, model, diag);
            if (diag != 0) {
                diag->code = code;
                hmb3d_set_message(diag, hmb3d_error_string(code));
            }
            return code;
        }
    }

    if (hmb3d_header_is_caligari(data, size)) {
        be = hmb3d_guess_be(data, size);
        if (be && diag != 0) {
            diag->warning_flags |= HMB3D_WARN_BIG_ENDIAN;
        }
        code = hmb3d_parse_chunk_stream(data, size, 32U, be, model, diag);
    } else {
        code = hmb3d_parse_chunk_stream(data, size, 0U, 0, model, diag);
        if (model->object_count == 0U) {
            hmb3d_model_init(model);
            if (diag != 0) {
                hmb3d_u32 saved_warn;
                saved_warn = diag->warning_flags;
                hmb3d_diag_init(diag);
                diag->warning_flags = saved_warn;
            }
            code = hmb3d_scan_for_polh(data, size, model, diag);
        }
    }

    if (code == HMB3D_OK && model->object_count == 0U) {
        code = HMB3D_ERR_NO_GEOMETRY;
    }
    if (diag != 0) {
        diag->code = code;
        hmb3d_set_message(diag, hmb3d_error_string(code));
    }
    return code;
}
