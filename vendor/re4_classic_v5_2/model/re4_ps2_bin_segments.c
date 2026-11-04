#include "re4c3d/re4_gcps2.h"
#include "re4c3d/re4_base.h"
#include <string.h>

static re4_bool ps2_table(re4_span file, re4_u32 off, re4_u32 count, re4_u32 stride)
{
    if (count == 0U) return RE4_TRUE;
    if (off == 0U || count > RE4_U32_MAX / stride) return RE4_FALSE;
    return re4_span_ok(file, (re4_size)off, (re4_size)count * stride);
}

re4_err re4_ps2_bin_read(re4_span file, re4_ps2_bin *out)
{
    re4_u32 i;
    if (out == 0 || file.data == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(file, 0U, 0x50U)) return RE4_ERR_RANGE;
    if (re4_read_u16_le(file.data) != 0x0030U) return RE4_ERR_BAD_MAGIC;
    out->file = file;
    out->texture_count = re4_read_u16_le(file.data + 2U);
    out->bones_offset = re4_read_u32_le(file.data + 4U);
    out->vertex_shift = file.data[8U];
    out->bone_count = file.data[9U];
    out->material_count = re4_read_u16_le(file.data + 10U);
    out->materials_offset = re4_read_u32_le(file.data + 12U);
    out->version = re4_read_u32_le(file.data + 0x18U);
    out->bonepairs_offset = re4_read_u32_le(file.data + 0x1CU);
    out->flags = re4_read_u32_le(file.data + 0x24U);
    if (!ps2_table(file, out->bones_offset, out->bone_count, 16U) ||
        !ps2_table(file, out->materials_offset, out->material_count, 16U)) return RE4_ERR_RANGE;
    if (out->bonepairs_offset) {
        re4_u32 count;
        if (!re4_span_ok(file, out->bonepairs_offset, 4U)) return RE4_ERR_RANGE;
        count = re4_read_u32_le(file.data + out->bonepairs_offset);
        if (count > (RE4_U32_MAX - out->bonepairs_offset - 4U)/8U ||
            !re4_span_ok(file, (re4_size)out->bonepairs_offset + 4U, (re4_size)count * 8U))
            return RE4_ERR_RANGE;
    }
    for (i = 0U; i < 7U; ++i) out->bounds[i] = re4_f32bits_to_q16(re4_read_u32_le(file.data + 0x30U + 4U*i));
    return RE4_OK;
}

re4_err re4_ps2_bin_bone(const re4_ps2_bin *bin, re4_u32 index, re4_ps2_bone *out)
{
    re4_size at;
    re4_u32 i;
    const re4_u8 *p;
    if (!bin || !out || !bin->file.data) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (index >= bin->bone_count) return RE4_ERR_RANGE;
    at = (re4_size)bin->bones_offset + (re4_size)index * 16U;
    if (!re4_span_ok(bin->file, at, 16U)) return RE4_ERR_RANGE;
    p = bin->file.data + at;
    out->id = p[0];
    out->parent = p[1];
    for (i = 0U; i < 3U; ++i)
        out->xyz[i] = re4_f32bits_to_q16(re4_read_u32_le(p + 4U + i*4U));
    return RE4_OK;
}

re4_err re4_ps2_bin_node(const re4_ps2_bin *bin, re4_u32 material_index, re4_ps2_node *out)
{
    re4_u32 at;
    re4_u32 node_off;
    re4_u32 count, bytes;
    if (bin == 0 || out == 0 || bin->file.data == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (material_index >= bin->material_count) return RE4_ERR_RANGE;
    at = bin->materials_offset + material_index * 16U;
    if (!re4_span_ok(bin->file, at, 16U)) return RE4_ERR_RANGE;
    node_off = re4_read_u32_le(bin->file.data + at + 12U);
    if (!re4_span_ok(bin->file, node_off, 4U)) return RE4_ERR_RANGE;
    count = bin->file.data[node_off + 3U];
    bytes = (4U + count + 15U) & ~15U;
    if (!re4_span_ok(bin->file, node_off, bytes)) return RE4_ERR_RANGE;
    out->offset = node_off;
    out->first_span_bytes = re4_read_u16_le(bin->file.data + node_off);
    out->segments = (re4_u16)bin->file.data[node_off + 2U] + 1U;
    out->bone_ids = (re4_u8)count;
    out->bones.data = bin->file.data + node_off + 4U;
    out->bones.size = count;
    out->first_segment = node_off + bytes;
    return RE4_OK;
}

/* The PS2 node contains an optional VIF weight-map block followed by a
   fixed 3x16-byte group and up to 255 lines of 24-byte packed vertices.
   This reader records the exact original spans, without guessing skinning. */
re4_err re4_ps2_node_segment(const re4_ps2_bin *bin, const re4_ps2_node *node,
                             re4_u32 segment_index, re4_ps2_segment *out)
{
    re4_u32 i;
    re4_size at, weight_off, weight_bytes;
    re4_u32 lines, bytes;
    re4_bool scenario;
    const re4_u8 *p;
    if (!bin || !node || !out || !bin->file.data) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (segment_index >= node->segments) return RE4_ERR_RANGE;
    at = (re4_size)node->first_segment;
    for (i = 0U; i <= segment_index; ++i) {
        if (!re4_span_ok(bin->file, at, 16U)) return RE4_ERR_RANGE;
        p = bin->file.data + at;
        scenario = RE4_FALSE;
        weight_bytes = 0U;
        weight_off = 0U;
        if (p[12] == 0U && p[14] > 1U) {
            weight_bytes = (re4_size)p[0] * 16U;
            weight_off = at + 16U;
            if (!re4_span_ok(bin->file, weight_off, weight_bytes)) return RE4_ERR_RANGE;
            at = weight_off + weight_bytes;
        } else {
            scenario = RE4_TRUE;
        }
        if (!re4_span_ok(bin->file, at, 48U)) return RE4_ERR_RANGE;
        p = bin->file.data + at;
        lines = (re4_u32)p[16U];
        bytes = (re4_u32)p[32U] * 16U;
        if (lines > bytes / 24U) return RE4_ERR_BAD_FORMAT;
        if (!re4_span_ok(bin->file, at + 48U, bytes)) return RE4_ERR_RANGE;
        if (i == segment_index) {
            out->vertex_offset = (re4_u32)(at + 48U);
            out->vertex_count = lines;
            out->weightmap_offset = (re4_u32)weight_off;
            out->weightmap_bytes = (re4_u32)weight_bytes;
            out->conversion_factor = re4_f32bits_to_q16(re4_read_u32_le(p + 16U + 12U));
            out->scenario_color = scenario;
        }
        at += 48U + bytes;
        if (i > 0U) {
            if (!re4_span_ok(bin->file, at, 16U)) return RE4_ERR_RANGE;
            at += 16U;
        }
    }
    return RE4_OK;
}

re4_err re4_ps2_segment_vertex(const re4_ps2_bin *bin, const re4_ps2_segment *segment,
                               re4_u32 index, re4_ps2_vertex *out)
{
    re4_size at;
    const re4_u8 *p;
    re4_u32 i;
    if (!bin || !segment || !out || !bin->file.data) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (index >= segment->vertex_count) return RE4_ERR_RANGE;
    at = (re4_size)segment->vertex_offset + (re4_size)index * 24U;
    if (!re4_span_ok(bin->file, at, 24U)) return RE4_ERR_RANGE;
    p = bin->file.data + at;
    for (i = 0; i < 3U; ++i) out->xyz[i] = re4_read_i16_e(p + 2U*i, RE4_ENDIAN_LITTLE);
    if (segment->scenario_color) {
        out->index_mount = re4_read_u16_le(p + 6U);
        out->texcoord[0] = re4_read_i16_e(p + 8U, RE4_ENDIAN_LITTLE);
        out->texcoord[1] = re4_read_i16_e(p + 10U, RE4_ENDIAN_LITTLE);
        out->unknown_a = re4_read_u16_le(p + 12U);
        out->index_complement = re4_read_u16_le(p + 14U);
        for (i = 0U; i < 3U; ++i) out->normal_or_color[i] = re4_read_i16_e(p + 16U + 2U*i, RE4_ENDIAN_LITTLE);
        out->unknown_b = re4_read_u16_le(p + 22U);
        out->weight_or_alpha = out->unknown_b;
    } else {
        out->unknown_b = re4_read_u16_le(p + 6U);
        out->weight_or_alpha = out->unknown_b;
        for (i = 0U; i < 3U; ++i) out->normal_or_color[i] = re4_read_i16_e(p + 8U + 2U*i, RE4_ENDIAN_LITTLE);
        out->index_complement = re4_read_u16_le(p + 14U);
        out->texcoord[0] = re4_read_i16_e(p + 16U, RE4_ENDIAN_LITTLE);
        out->texcoord[1] = re4_read_i16_e(p + 18U, RE4_ENDIAN_LITTLE);
        out->unknown_a = re4_read_u16_le(p + 20U);
        out->index_mount = re4_read_u16_le(p + 22U);
    }
    return RE4_OK;
}

static re4_q16 ps2_signed_mul_div100(re4_q16 factor, re4_i16 coord)
{
    re4_u32 mag_f, mag_c, a, b, whole, frac, mag;
    re4_bool neg;
    if (factor == 0 || coord == 0) return 0;
    neg = ((factor < 0) != (coord < 0)) ? RE4_TRUE : RE4_FALSE;
    mag_f = (factor < 0) ? (re4_u32)(-(factor + 1)) + 1U : (re4_u32)factor;
    mag_c = (coord < 0) ? (re4_u32)(-(re4_i32)coord) : (re4_u32)coord;
    a = mag_f / 100U;
    b = mag_f % 100U;
    /* Check the 32-bit multiplication before performing it. */
    if (a > RE4_U32_MAX / mag_c) return neg ? RE4_I32_MIN : RE4_I32_MAX;
    whole = a * mag_c;
    frac = (b * mag_c) / 100U; /* <= 3,276,700, always fits */
    if (whole > RE4_U32_MAX - frac) return neg ? RE4_I32_MIN : RE4_I32_MAX;
    mag = whole + frac;
    return re4_clamp_i64_like_q16(mag, neg);
}

re4_err re4_ps2_vertex_q16(const re4_ps2_segment *segment,
                           const re4_ps2_vertex *vertex,
                           re4_q16 xyz[3], re4_q16 uv[2])
{
    re4_u32 i;
    if (!segment || !vertex || !xyz || !uv) return RE4_ERR_NULL;
    for (i = 0U; i < 3U; ++i)
        xyz[i] = ps2_signed_mul_div100(segment->conversion_factor, vertex->xyz[i]);
    for (i = 0U; i < 2U; ++i)
        uv[i] = ((re4_i32)vertex->texcoord[i] * 65536) / 255;
    return RE4_OK;
}

re4_err re4_ps2_segment_weight(const re4_ps2_bin *bin,
                              const re4_ps2_segment *segment,
                              re4_u32 entry_index, re4_ps2_weight *out)
{
    re4_size at;
    const re4_u8 *p;
    re4_u32 i;
    if (!bin || !segment || !out || !bin->file.data) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (entry_index >= segment->weightmap_bytes / 32U) return RE4_ERR_RANGE;
    at = (re4_size)segment->weightmap_offset + (re4_size)entry_index * 32U;
    if (!re4_span_ok(bin->file, at, 32U)) return RE4_ERR_RANGE;
    p = bin->file.data + at;
    for (i = 0U; i < 3U; ++i) {
        out->bone_slot[i] = re4_read_u32_le(p + i*4U);
        out->weight[i] = re4_f32bits_to_q16(re4_read_u32_le(p + 16U + i*4U));
    }
    out->amount = re4_read_u32_le(p + 12U);
    return RE4_OK;
}

re4_err re4_ps2_segment_triangles(const re4_ps2_bin *bin, const re4_ps2_segment *segment,
                                  re4_gc_triangle *out_faces, re4_u32 face_capacity,
                                  re4_u32 *face_count)
{
    re4_u32 i, count;
    re4_bool flip;
    re4_ps2_vertex vertex;
    re4_err er;
    if (!bin || !segment || !face_count) return RE4_ERR_NULL;
    *face_count = 0U;
    count = 0U;
    flip = RE4_FALSE;
    for (i = 0U; i < segment->vertex_count; ++i) {
        er = re4_ps2_segment_vertex(bin, segment, i, &vertex);
        if (er != RE4_OK) return er;
        if (i >= 2U && vertex.index_complement == 0U) {
            if (out_faces) {
                if (count >= face_capacity) return RE4_ERR_NO_SPACE;
                if (flip) {
                    out_faces[count].corner[0] = i;
                    out_faces[count].corner[1] = i - 1U;
                    out_faces[count].corner[2] = i - 2U;
                } else {
                    out_faces[count].corner[0] = i - 2U;
                    out_faces[count].corner[1] = i - 1U;
                    out_faces[count].corner[2] = i;
                }
            }
            ++count;
            flip = !flip;
        } else flip = RE4_FALSE;
    }
    *face_count = count;
    return RE4_OK;
}
