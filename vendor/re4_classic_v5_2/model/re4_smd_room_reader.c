#include "re4c3d/re4_model.h"
#include "re4c3d/re4_base.h"
#include <string.h>
#include "re4_model_probe_common.inc"

#define RE4_SMD_HEADER_BYTES 0x14UL
#define RE4_SMD_LINE_BYTES   0x48UL

static void re4_smd_read_vec3_q16(const re4_u8 *p, re4_endian endian, re4_q16 out[3])
{
    out[0] = re4_f32bits_to_q16(re4_read_u32_e(p + 0UL, endian));
    out[1] = re4_f32bits_to_q16(re4_read_u32_e(p + 4UL, endian));
    out[2] = re4_f32bits_to_q16(re4_read_u32_e(p + 8UL, endian));
}

static void re4_smd_read_line(re4_span src, re4_size off, re4_endian endian, re4_smd_line *line)
{
    re4_u32 i;
    const re4_u8 *p;
    p = src.data + off;
    memset(line, 0, sizeof(*line));
    re4_smd_read_vec3_q16(p + 0UL, endian, line->position);
    re4_smd_read_vec3_q16(p + 12UL, endian, line->angle);
    re4_smd_read_vec3_q16(p + 24UL, endian, line->scale);
    line->bin_file_id = p[36];
    line->tpl_file_id = p[37];
    line->fixed_ff = p[38];
    line->smx_id = p[39];
    for (i = 0; i < 7U; ++i) {
        line->unused[i] = re4_read_u32_e(p + 40UL + ((re4_size)i * 4UL), endian);
    }
    line->object_status = re4_read_u32_e(p + 68UL, endian);
}

re4_err re4_smd_room_read(re4_span src, re4_endian endian, re4_arena *arena, re4_smd_room *out)
{
    re4_u32 i;
    re4_size line_off;
    re4_smd_line *lines;
    re4_u32 *extra;

    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0UL, RE4_SMD_HEADER_BYTES)) return RE4_ERR_RANGE;

    out->magic = re4_read_u16_e(src.data + 0UL, endian);
    out->line_count = re4_read_u16_e(src.data + 2UL, endian);
    out->offset_bin_array = re4_read_u32_e(src.data + 4UL, endian);
    out->offset_tpl_array = re4_read_u32_e(src.data + 8UL, endian);
    out->first_bin_or_file_end = re4_read_u32_e(src.data + 12UL, endian);
    out->extra_count = re4_read_u32_e(src.data + 16UL, endian);
    out->endian = endian;

    if (out->line_count > 4096U || out->extra_count > 65535U) return RE4_ERR_BAD_FORMAT;
    if (!re4_span_ok(src, RE4_SMD_HEADER_BYTES, (re4_size)out->extra_count * 4UL)) return RE4_ERR_RANGE;

    if (out->extra_count != 0U) {
        extra = (re4_u32 *)re4_arena_push(arena, (re4_size)out->extra_count * sizeof(re4_u32), sizeof(re4_u32));
        if (extra == 0) return RE4_ERR_NO_SPACE;
        for (i = 0; i < out->extra_count; ++i) {
            extra[i] = re4_read_u32_e(src.data + RE4_SMD_HEADER_BYTES + ((re4_size)i * 4UL), endian);
        }
        out->extra_parameters = extra;
    }

    line_off = RE4_SMD_HEADER_BYTES + ((re4_size)out->extra_count * 4UL);
    if (!re4_span_ok(src, line_off, (re4_size)out->line_count * RE4_SMD_LINE_BYTES)) return RE4_ERR_RANGE;
    if (out->line_count != 0U) {
        lines = (re4_smd_line *)re4_arena_push(arena, (re4_size)out->line_count * sizeof(re4_smd_line), sizeof(re4_u32));
        if (lines == 0) return RE4_ERR_NO_SPACE;
        for (i = 0; i < (re4_u32)out->line_count; ++i) {
            re4_smd_read_line(src, line_off + ((re4_size)i * RE4_SMD_LINE_BYTES), endian, &lines[i]);
        }
        out->lines = lines;
    }

    if (out->offset_bin_array != 0U && (re4_size)out->offset_bin_array > src.size) return RE4_ERR_RANGE;
    if (out->offset_tpl_array != 0U && (re4_size)out->offset_tpl_array > src.size) return RE4_ERR_RANGE;
    if (out->first_bin_or_file_end != 0U && (re4_size)out->first_bin_or_file_end > src.size) return RE4_ERR_RANGE;
    return RE4_OK;
}

re4_err re4_smd_room_probe(re4_span src, re4_model_probe *out)
{
    if (out == 0) return RE4_ERR_NULL;
    if (src.size < 16UL) return RE4_ERR_RANGE;
    re4_model_probe_fill(src, RE4_MODEL_SMD_ROOM, out);
    return RE4_OK;
}

static re4_err re4_smd_span_between(re4_span src, re4_u32 start, re4_u32 end, re4_span *out)
{
    if (out == 0) return RE4_ERR_NULL;
    if (start == 0U) {
        out->data = 0;
        out->size = 0;
        return RE4_OK;
    }
    if (end == 0U) end = (re4_u32)src.size;
    if (end < start) return RE4_ERR_BAD_FORMAT;
    return re4_span_sub(src, (re4_size)start, (re4_size)(end - start), out);
}

re4_err re4_smd_room_bin_table_span(re4_span src, const re4_smd_room *room, re4_span *out)
{
    re4_u32 end;
    if (room == 0) return RE4_ERR_NULL;
    end = room->offset_tpl_array != 0U ? room->offset_tpl_array : room->first_bin_or_file_end;
    return re4_smd_span_between(src, room->offset_bin_array, end, out);
}

re4_err re4_smd_room_tpl_table_span(re4_span src, const re4_smd_room *room, re4_span *out)
{
    if (room == 0) return RE4_ERR_NULL;
    return re4_smd_span_between(src, room->offset_tpl_array, room->first_bin_or_file_end, out);
}

re4_err re4_smd_room_payload_tail_span(re4_span src, const re4_smd_room *room, re4_span *out)
{
    if (room == 0) return RE4_ERR_NULL;
    return re4_smd_span_between(src, room->first_bin_or_file_end, (re4_u32)src.size, out);
}

re4_bool re4_smd_line_references_tpl(const re4_smd_line *line)
{
    if (line == 0) return RE4_FALSE;
    if (line->tpl_file_id != 0xFFU) return RE4_TRUE;
    return RE4_FALSE;
}

re4_bool re4_smd_line_references_smx(const re4_smd_line *line)
{
    if (line == 0) return RE4_FALSE;
    if (line->smx_id != 0xFFU) return RE4_TRUE;
    return RE4_FALSE;
}
