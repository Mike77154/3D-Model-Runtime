#include "re4c3d/re4_model.h"
#include "re4c3d/re4_base.h"
#include <string.h>
#include "re4_model_probe_common.inc"

#define RE4_BIN_MIN_HEADER_BYTES 0x40UL
#define RE4_BIN_EXT_HEADER_BYTES 0x50UL
#define RE4_BIN_MATERIAL_FIXED_BYTES 24UL
#define RE4_BIN_WEIGHTMAP_BYTES 0x08UL
#define RE4_BIN_BONE_BYTES 0x10UL
#define RE4_BIN_BONEPAIR_BYTES 0x08UL

static re4_bool re4_opt_span_ok(re4_span src, re4_u32 off, re4_size len)
{
    if (off == 0U) return RE4_TRUE;
    return re4_span_ok(src, (re4_size)off, len);
}

static re4_bool re4_opt_table_ok(re4_span src, re4_u32 off, re4_u32 count, re4_size stride)
{
    if (count == 0U) return RE4_TRUE;
    if (off == 0U) return RE4_FALSE;
    if (count > 65535U) return RE4_FALSE;
    return re4_span_ok(src, (re4_size)off, (re4_size)count * stride);
}

re4_bool re4_bin_header_has_flag(const re4_bin_header *h, re4_u32 flag)
{
    if (h == 0) return RE4_FALSE;
    return (h->flags & flag) == flag ? RE4_TRUE : RE4_FALSE;
}

static void re4_bin_read_header(re4_span src, re4_endian endian, re4_bin_header *h)
{
    re4_u32 first;
    memset(h, 0, sizeof(*h));
    h->endian = endian;
    first = re4_read_u32_e(src.data + 0x00UL, endian);
    h->header_size = first;
    h->bone_offset = first;
    h->unknown_x04 = re4_read_u32_e(src.data + 0x04UL, endian);
    h->unknown_x08 = re4_read_u32_e(src.data + 0x08UL, endian);
    h->vertex_colour_offset = re4_read_u32_e(src.data + 0x0CUL, endian);
    h->vertex_texcoord_offset = re4_read_u32_e(src.data + 0x10UL, endian);
    h->weightmap_offset = re4_read_u32_e(src.data + 0x14UL, endian);
    h->weightmap_count = src.data[0x18UL];
    h->bone_count = src.data[0x19UL];
    h->material_count = re4_read_u16_e(src.data + 0x1AUL, endian);
    h->material_offset = re4_read_u32_e(src.data + 0x1CUL, endian);
    h->flags = re4_read_u32_e(src.data + 0x20UL, endian);
    h->texture_count = re4_read_u32_e(src.data + 0x24UL, endian);
    h->vertex_scale = src.data[0x28UL];
    h->unknown_x29 = src.data[0x29UL];
    h->weightmap2_count = re4_read_u16_e(src.data + 0x2AUL, endian);
    h->morph_offset = re4_read_u32_e(src.data + 0x2CUL, endian);
    h->vertex_position_offset = re4_read_u32_e(src.data + 0x30UL, endian);
    h->vertex_normal_offset = re4_read_u32_e(src.data + 0x34UL, endian);
    h->vertex_position_count = re4_read_u16_e(src.data + 0x38UL, endian);
    h->vertex_normal_count = re4_read_u16_e(src.data + 0x3AUL, endian);
    h->version_flags = re4_read_u32_e(src.data + 0x3CUL, endian);
    if (first >= 0x50U && src.size >= RE4_BIN_EXT_HEADER_BYTES) {
        h->bonepair_offset = re4_read_u32_e(src.data + 0x40UL, endian);
        h->adjacent_offset = re4_read_u32_e(src.data + 0x44UL, endian);
        h->vertex_weight_index_offset = re4_read_u32_e(src.data + 0x48UL, endian);
        h->vertex_weight2_index_offset = re4_read_u32_e(src.data + 0x4CUL, endian);
    }
}

static re4_u32 re4_bin_triangles_for_primitive(re4_u16 primitive_type, re4_u16 vertex_count)
{
    if (primitive_type == 5U) {
        return (re4_u32)(vertex_count / 3U);
    }
    if (primitive_type == 6U) {
        return vertex_count >= 3U ? (re4_u32)vertex_count - 2U : 0U;
    }
    if (primitive_type == 7U) {
        return vertex_count >= 3U ? (re4_u32)vertex_count - 2U : 0U;
    }
    if (primitive_type == 8U) {
        return (re4_u32)(vertex_count / 4U) * 2U;
    }
    return 0U;
}

static re4_err re4_bin_read_material(re4_span src, re4_size off, re4_endian endian,
                                     re4_u32 material_index, re4_u32 *vertex_cursor,
                                     re4_bin_material *m, re4_bin_strip *strips,
                                     re4_u32 strip_cap, re4_u32 *strip_cursor)
{
    re4_size i;
    re4_size strip_pos;
    re4_size end_pos;
    const re4_u8 *p;
    re4_u32 tri_total;
    re4_u32 v_start;

    if (!re4_span_ok(src, off, 36UL)) return RE4_ERR_RANGE;
    if (vertex_cursor == 0 || strip_cursor == 0) return RE4_ERR_NULL;

    memset(m, 0, sizeof(*m));
    p = src.data + off;
    for (i = 0; i < RE4_BIN_MATERIAL_FIXED_BYTES; ++i) m->raw[i] = p[i];
    m->byte_offset = (re4_u32)off;
    m->material_flag = p[11];
    m->diffuse_map = p[12];
    m->bump_map = p[13];
    m->opacity_map = p[14];
    m->generic_specular_map = p[15];
    m->specular_rgb[0] = p[16];
    m->specular_rgb[1] = p[17];
    m->specular_rgb[2] = p[18];
    m->specular_scale = p[21];
    m->custom_specular_map = p[23];
    m->face_buffer_size = re4_read_u32_e(p + 24UL, endian);
    m->face_count_word = re4_read_u32_e(p + 28UL, endian);
    m->first_strip = (re4_u32)(off + 36UL);
    m->vertex_base = *vertex_cursor;

    if (m->face_buffer_size < 4U) return RE4_ERR_BAD_FORMAT;
    if (m->face_buffer_size > src.size) return RE4_ERR_BAD_FORMAT;
    if (!re4_span_ok(src, off + 32UL, (re4_size)m->face_buffer_size)) return RE4_ERR_RANGE;
    m->strip_count = re4_read_u32_e(p + 32UL, endian);
    if (m->strip_count > 65535U) return RE4_ERR_BAD_FORMAT;
    if (m->face_buffer_size < 4U + (m->strip_count * 4U)) return RE4_ERR_BAD_FORMAT;

    strip_pos = off + 36UL;
    end_pos = off + 32UL + (re4_size)m->face_buffer_size;
    tri_total = 0U;
    v_start = *vertex_cursor;

    for (i = 0; i < (re4_size)m->strip_count; ++i) {
        re4_u16 typ;
        re4_u16 cnt;
        re4_u32 tris;
        if (!re4_span_ok(src, strip_pos, 4UL)) return RE4_ERR_RANGE;
        typ = re4_read_u16_e(src.data + strip_pos, endian);
        cnt = re4_read_u16_e(src.data + strip_pos + 2UL, endian);
        tris = re4_bin_triangles_for_primitive(typ, cnt);
        if (*strip_cursor < strip_cap && strips != 0) {
            strips[*strip_cursor].material_index = material_index;
            strips[*strip_cursor].primitive_type = typ;
            strips[*strip_cursor].vertex_count = cnt;
            strips[*strip_cursor].vertex_base = *vertex_cursor;
            strips[*strip_cursor].triangle_count_hint = tris;
        }
        ++(*strip_cursor);
        *vertex_cursor += (re4_u32)cnt;
        tri_total += tris;
        strip_pos += 4UL;
    }

    if (strip_pos > end_pos) return RE4_ERR_RANGE;
    m->byte_size = (re4_u32)(32UL + (re4_size)m->face_buffer_size);
    m->vertex_count_hint = *vertex_cursor - v_start;
    m->triangle_count_hint = tri_total;
    return RE4_OK;
}

static void re4_bin_read_weightmap(re4_span src, re4_size off, re4_bin_weight_map *w)
{
    const re4_u8 *p;
    p = src.data + off;
    w->bone_id[0] = p[0];
    w->bone_id[1] = p[1];
    w->bone_id[2] = p[2];
    w->count = p[3];
    w->weight[0] = p[4];
    w->weight[1] = p[5];
    w->weight[2] = p[6];
    w->unused = p[7];
}

static void re4_bin_read_bone(re4_span src, re4_size off, re4_endian endian, re4_bin_bone *b)
{
    const re4_u8 *p;
    p = src.data + off;
    b->id = p[0];
    b->parent = p[1];
    b->position[0] = re4_f32bits_to_q16(re4_read_u32_e(p + 4UL, endian));
    b->position[1] = re4_f32bits_to_q16(re4_read_u32_e(p + 8UL, endian));
    b->position[2] = re4_f32bits_to_q16(re4_read_u32_e(p + 12UL, endian));
}

static re4_u32 re4_bin_effective_weight_count(const re4_bin_header *h)
{
    if (h->weightmap2_count > 255U) return (re4_u32)h->weightmap2_count;
    return (re4_u32)h->weightmap_count;
}

static re4_err re4_bin_count_materials(re4_span src, const re4_bin_header *h, re4_endian endian,
                                       re4_u32 *strip_count, re4_u32 *vertex_count,
                                       re4_u32 *tri_count)
{
    re4_u32 i;
    re4_size pos;
    re4_u32 strips;
    re4_u32 verts;
    re4_u32 tris;

    if (strip_count == 0 || vertex_count == 0 || tri_count == 0) return RE4_ERR_NULL;
    strips = 0U;
    verts = 0U;
    tris = 0U;
    pos = (re4_size)h->material_offset;
    for (i = 0; i < (re4_u32)h->material_count; ++i) {
        re4_bin_material tmp;
        re4_u32 local_strip_cursor;
        local_strip_cursor = strips;
        if (pos > src.size) return RE4_ERR_RANGE;
        {
            re4_err er;
            er = re4_bin_read_material(src, pos, endian, i, &verts, &tmp, 0, 0U, &local_strip_cursor);
            if (er != RE4_OK) return er;
        }
        strips = local_strip_cursor;
        tris += tmp.triangle_count_hint;
        pos += (re4_size)tmp.byte_size;
    }
    *strip_count = strips;
    *vertex_count = verts;
    *tri_count = tris;
    return RE4_OK;
}

re4_err re4_bin_model_read(re4_span src, re4_endian endian, re4_arena *arena, re4_bin_model *out)
{
    re4_u32 i;
    re4_bin_material *materials;
    re4_bin_strip *strips;
    re4_bin_weight_map *weights;
    re4_bin_bone *bones;
    re4_bin_bone_pair *pairs;
    re4_u32 strip_count;
    re4_u32 true_vertices;
    re4_u32 tri_count;
    re4_u32 strip_cursor;
    re4_u32 vertex_cursor;
    re4_u32 effective_weight_count;
    re4_size pos;

    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0UL, RE4_BIN_MIN_HEADER_BYTES)) return RE4_ERR_RANGE;

    re4_bin_read_header(src, endian, &out->header);
    if (!(out->header.bone_offset == 0x40U || out->header.bone_offset == 0x50U || out->header.bone_offset == 0x60U || out->header.bone_offset == 0x98U)) {
        return RE4_ERR_BAD_FORMAT;
    }
    if ((out->header.flags & RE4_BIN_FLAG_ALWAYS_ACTIVATED) != RE4_BIN_FLAG_ALWAYS_ACTIVATED) return RE4_ERR_BAD_FORMAT;
    if (!re4_span_ok(src, 0UL, (re4_size)out->header.bone_offset)) return RE4_ERR_RANGE;

    if (!re4_opt_table_ok(src, out->header.material_offset, out->header.material_count, 36UL)) return RE4_ERR_RANGE;
    effective_weight_count = re4_bin_effective_weight_count(&out->header);
    if (!re4_opt_table_ok(src, out->header.weightmap_offset, effective_weight_count, RE4_BIN_WEIGHTMAP_BYTES)) return RE4_ERR_RANGE;
    if (!re4_opt_table_ok(src, out->header.bone_offset, (re4_u32)out->header.bone_count, RE4_BIN_BONE_BYTES)) return RE4_ERR_RANGE;

    if (out->header.material_count != 0U) {
        re4_err er;
        er = re4_bin_count_materials(src, &out->header, endian, &strip_count, &true_vertices, &tri_count);
        if (er != RE4_OK) return er;
        materials = (re4_bin_material *)re4_arena_push(arena, (re4_size)out->header.material_count * sizeof(re4_bin_material), sizeof(re4_u32));
        if (materials == 0) return RE4_ERR_NO_SPACE;
        strips = 0;
        if (strip_count != 0U) {
            strips = (re4_bin_strip *)re4_arena_push(arena, (re4_size)strip_count * sizeof(re4_bin_strip), sizeof(re4_u32));
            if (strips == 0) return RE4_ERR_NO_SPACE;
        }
        strip_cursor = 0U;
        vertex_cursor = 0U;
        pos = (re4_size)out->header.material_offset;
        for (i = 0; i < (re4_u32)out->header.material_count; ++i) {
            re4_err er2;
            er2 = re4_bin_read_material(src, pos, endian, i, &vertex_cursor, &materials[i], strips, strip_count, &strip_cursor);
            if (er2 != RE4_OK) return er2;
            pos += (re4_size)materials[i].byte_size;
        }
        out->materials = materials;
        out->material_count = out->header.material_count;
        out->strips = strips;
        out->strip_count = strip_count;
        out->true_vertex_count = true_vertices;
        out->triangle_count_hint = tri_count;
    } else {
        out->true_vertex_count = out->header.vertex_position_count;
    }

    if (out->true_vertex_count == 0U) out->true_vertex_count = out->header.vertex_position_count;
    if (!re4_opt_span_ok(src, out->header.vertex_position_offset, (re4_size)out->true_vertex_count * 12UL)) return RE4_ERR_RANGE;
    if (!re4_opt_span_ok(src, out->header.vertex_normal_offset, (re4_size)out->true_vertex_count * 12UL)) return RE4_ERR_RANGE;
    if (!re4_opt_span_ok(src, out->header.vertex_texcoord_offset, (re4_size)out->true_vertex_count * 8UL)) return RE4_ERR_RANGE;
    if (!re4_opt_span_ok(src, out->header.vertex_colour_offset, (re4_size)out->true_vertex_count * 4UL)) return RE4_ERR_RANGE;
    if (!re4_opt_span_ok(src, out->header.vertex_weight_index_offset, (re4_size)out->true_vertex_count * 2UL)) return RE4_ERR_RANGE;

    if (out->header.bone_count != 0U) {
        bones = (re4_bin_bone *)re4_arena_push(arena, (re4_size)out->header.bone_count * sizeof(re4_bin_bone), sizeof(re4_u32));
        if (bones == 0) return RE4_ERR_NO_SPACE;
        for (i = 0; i < (re4_u32)out->header.bone_count; ++i) {
            re4_bin_read_bone(src, (re4_size)out->header.bone_offset + ((re4_size)i * RE4_BIN_BONE_BYTES), endian, &bones[i]);
        }
        out->bones = bones;
        out->bone_count = out->header.bone_count;
    }

    if (effective_weight_count != 0U) {
        weights = (re4_bin_weight_map *)re4_arena_push(arena, (re4_size)effective_weight_count * sizeof(re4_bin_weight_map), sizeof(re4_u32));
        if (weights == 0) return RE4_ERR_NO_SPACE;
        for (i = 0; i < effective_weight_count; ++i) {
            re4_bin_read_weightmap(src, (re4_size)out->header.weightmap_offset + ((re4_size)i * RE4_BIN_WEIGHTMAP_BYTES), &weights[i]);
        }
        out->weightmaps = weights;
        out->weightmap_count = effective_weight_count;
    }

    if (out->header.bonepair_offset != 0U && re4_bin_header_has_flag(&out->header, RE4_BIN_FLAG_ENABLE_BONEPAIR_TAG)) {
        re4_u32 pair_count;
        if (!re4_span_ok(src, (re4_size)out->header.bonepair_offset, 4UL)) return RE4_ERR_RANGE;
        pair_count = re4_read_u32_e(src.data + out->header.bonepair_offset, endian);
        if (pair_count > 65535U) return RE4_ERR_BAD_FORMAT;
        if (!re4_span_ok(src, (re4_size)out->header.bonepair_offset + 4UL, (re4_size)pair_count * RE4_BIN_BONEPAIR_BYTES)) return RE4_ERR_RANGE;
        pairs = (re4_bin_bone_pair *)re4_arena_push(arena, (re4_size)pair_count * sizeof(re4_bin_bone_pair), sizeof(re4_u32));
        if (pair_count != 0U && pairs == 0) return RE4_ERR_NO_SPACE;
        for (i = 0; i < pair_count; ++i) {
            re4_size o;
            o = (re4_size)out->header.bonepair_offset + 4UL + ((re4_size)i * RE4_BIN_BONEPAIR_BYTES);
            pairs[i].bone[0] = re4_read_u16_e(src.data + o + 0UL, endian);
            pairs[i].bone[1] = re4_read_u16_e(src.data + o + 2UL, endian);
            pairs[i].bone[2] = re4_read_u16_e(src.data + o + 4UL, endian);
            pairs[i].bone[3] = re4_read_u16_e(src.data + o + 6UL, endian);
        }
        out->bonepairs = pairs;
        out->bonepair_count = pair_count;
    }

    return RE4_OK;
}

re4_err re4_bin_vertex_read(re4_span src, const re4_bin_model *model, re4_u32 index, re4_bin_vertex *out)
{
    const re4_bin_header *h;
    re4_size off;
    if (model == 0 || out == 0) return RE4_ERR_NULL;
    if (index >= model->true_vertex_count) return RE4_ERR_RANGE;
    memset(out, 0, sizeof(*out));
    h = &model->header;
    if (h->vertex_position_offset != 0U) {
        off = (re4_size)h->vertex_position_offset + ((re4_size)index * 12UL);
        if (!re4_span_ok(src, off, 12UL)) return RE4_ERR_RANGE;
        out->position[0] = re4_f32bits_to_q16(re4_read_u32_e(src.data + off + 0UL, h->endian));
        out->position[1] = re4_f32bits_to_q16(re4_read_u32_e(src.data + off + 4UL, h->endian));
        out->position[2] = re4_f32bits_to_q16(re4_read_u32_e(src.data + off + 8UL, h->endian));
        out->flags |= RE4_BIN_VERTEX_HAS_POSITION;
    }
    if (h->vertex_normal_offset != 0U) {
        off = (re4_size)h->vertex_normal_offset + ((re4_size)index * 12UL);
        if (!re4_span_ok(src, off, 12UL)) return RE4_ERR_RANGE;
        out->normal[0] = re4_f32bits_to_q16(re4_read_u32_e(src.data + off + 0UL, h->endian));
        out->normal[1] = re4_f32bits_to_q16(re4_read_u32_e(src.data + off + 4UL, h->endian));
        out->normal[2] = re4_f32bits_to_q16(re4_read_u32_e(src.data + off + 8UL, h->endian));
        out->flags |= RE4_BIN_VERTEX_HAS_NORMAL;
    }
    if (h->vertex_texcoord_offset != 0U) {
        off = (re4_size)h->vertex_texcoord_offset + ((re4_size)index * 8UL);
        if (!re4_span_ok(src, off, 8UL)) return RE4_ERR_RANGE;
        out->uv[0] = re4_f32bits_to_q16(re4_read_u32_e(src.data + off + 0UL, h->endian));
        out->uv[1] = re4_f32bits_to_q16(re4_read_u32_e(src.data + off + 4UL, h->endian));
        out->flags |= RE4_BIN_VERTEX_HAS_UV;
    }
    if (h->vertex_colour_offset != 0U) {
        re4_u32 c;
        off = (re4_size)h->vertex_colour_offset + ((re4_size)index * 4UL);
        if (!re4_span_ok(src, off, 4UL)) return RE4_ERR_RANGE;
        c = re4_read_u32_e(src.data + off, h->endian);
        out->color_argb[0] = (re4_u8)(c & 0xFFU);
        out->color_argb[1] = (re4_u8)((c >> 8) & 0xFFU);
        out->color_argb[2] = (re4_u8)((c >> 16) & 0xFFU);
        out->color_argb[3] = (re4_u8)((c >> 24) & 0xFFU);
        out->flags |= RE4_BIN_VERTEX_HAS_COLOR;
    }
    if (h->vertex_weight_index_offset != 0U) {
        off = (re4_size)h->vertex_weight_index_offset + ((re4_size)index * 2UL);
        if (!re4_span_ok(src, off, 2UL)) return RE4_ERR_RANGE;
        out->weight_index = re4_read_u16_e(src.data + off, h->endian);
        out->flags |= RE4_BIN_VERTEX_HAS_WEIGHT;
    }
    return RE4_OK;
}

re4_err re4_bin_material_strip_span(re4_span src, const re4_bin_material *mat, re4_span *out)
{
    if (mat == 0 || out == 0) return RE4_ERR_NULL;
    return re4_span_sub(src, mat->first_strip, (re4_size)mat->strip_count * 4UL, out);
}

re4_err re4_bin_model_probe(re4_span src, re4_model_probe *out)
{
    if (out == 0) return RE4_ERR_NULL;
    if (src.size < 16UL) return RE4_ERR_RANGE;
    re4_model_probe_fill(src, RE4_MODEL_BIN, out);
    return RE4_OK;
}

re4_err re4_bin_position_q16(re4_span src, const re4_bin_model *model, re4_u32 index, re4_q16 out_xyz[3])
{
    re4_bin_vertex v;
    re4_err er;
    if (out_xyz == 0) return RE4_ERR_NULL;
    er = re4_bin_vertex_read(src, model, index, &v);
    if (er != RE4_OK) return er;
    if ((v.flags & RE4_BIN_VERTEX_HAS_POSITION) == 0U) return RE4_ERR_BAD_FORMAT;
    out_xyz[0] = v.position[0];
    out_xyz[1] = v.position[1];
    out_xyz[2] = v.position[2];
    return RE4_OK;
}
