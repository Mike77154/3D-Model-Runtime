#include "re4c3d/re4_model.h"
#include "re4c3d/re4_base.h"
#include <string.h>
#include "re4_model_probe_common.inc"

#define RE4_SMX_RECORD_BYTES 0x88UL

re4_err re4_smx_record_read(re4_span src, re4_endian endian, re4_smx_record *out)
{
    re4_u32 i;
    if (out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0UL, RE4_SMX_RECORD_BYTES)) return RE4_ERR_RANGE;
    out->use_smx_id = src.data[0];
    out->mode = src.data[1];
    out->opacity_hierarchy = src.data[2];
    out->face_culling = src.data[3];
    out->light_switch = re4_read_u32_e(src.data + 4UL, endian);
    out->alpha_hierarchy = src.data[8];
    out->unknown_x09 = src.data[9];
    out->unknown_x0a = src.data[10];
    out->unknown_x0b = src.data[11];
    out->color_rgb[0] = src.data[12];
    out->color_rgb[1] = src.data[13];
    out->color_rgb[2] = src.data[14];
    out->color_alpha = src.data[15];
    out->specular_or_unknown = re4_read_u32_e(src.data + 16UL, endian);
    out->texture_movement[0] = re4_f32bits_to_q16(re4_read_u32_e(src.data + 20UL, endian));
    out->texture_movement[1] = re4_f32bits_to_q16(re4_read_u32_e(src.data + 24UL, endian));
    for (i = 0; i < 27U; ++i) {
        out->raw_words[i] = re4_read_u32_e(src.data + 28UL + ((re4_size)i * 4UL), endian);
    }
    return RE4_OK;
}

re4_err re4_smx_table_read(re4_span src, re4_endian endian, re4_arena *arena, re4_smx_table *out)
{
    re4_u32 i;
    re4_u32 n;
    re4_smx_record *records;
    re4_span rec;
    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if ((src.size % RE4_SMX_RECORD_BYTES) != 0UL) return RE4_ERR_BAD_FORMAT;
    n = (re4_u32)(src.size / RE4_SMX_RECORD_BYTES);
    if (n > 65535U) return RE4_ERR_BAD_FORMAT;
    records = 0;
    if (n != 0U) {
        records = (re4_smx_record *)re4_arena_push(arena, (re4_size)n * sizeof(re4_smx_record), sizeof(re4_u32));
        if (records == 0) return RE4_ERR_NO_SPACE;
    }
    for (i = 0; i < n; ++i) {
        re4_err er;
        er = re4_span_sub(src, (re4_size)i * RE4_SMX_RECORD_BYTES, RE4_SMX_RECORD_BYTES, &rec);
        if (er != RE4_OK) return er;
        er = re4_smx_record_read(rec, endian, &records[i]);
        if (er != RE4_OK) return er;
    }
    out->record_count = n;
    out->records = records;
    out->endian = endian;
    return RE4_OK;
}

re4_err re4_smx_probe(re4_span src, re4_model_probe *out)
{
    if (out == 0) return RE4_ERR_NULL;
    if (src.size < RE4_SMX_RECORD_BYTES) return RE4_ERR_RANGE;
    re4_model_probe_fill(src, RE4_MODEL_SMX, out);
    return RE4_OK;
}
