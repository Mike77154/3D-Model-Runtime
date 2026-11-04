#include "re1codec.h"
#include <string.h>

#define RE1C_CANON_MAGIC 0x31433152u
#define RE1C_CANON_VERSION 1u

static re1c_u16 re1c_rd16(const re1c_u8 *p)
{
    return (re1c_u16)((re1c_u16)p[0] | (re1c_u16)((re1c_u16)p[1] << 8));
}

static re1c_u32 re1c_rd32(const re1c_u8 *p)
{
    return (re1c_u32)p[0] |
           ((re1c_u32)p[1] << 8) |
           ((re1c_u32)p[2] << 16) |
           ((re1c_u32)p[3] << 24);
}

static int re1c_range_ok(re1c_u32 pos, re1c_u32 len, re1c_u32 size)
{
    if (pos > size) return 0;
    if (len > size - pos) return 0;
    return 1;
}

static int re1c_get_bank_sections(const re1c_view *view, re1c_u32 bank,
                                  const re1c_section **out_emr,
                                  const re1c_section **out_edd)
{
    if (view == NULL || out_emr == NULL || out_edd == NULL) return RE1C_ERR_ARGUMENT;
    if (view->section_count != RE1C_SECTION_COUNT) return RE1C_ERR_FORMAT;
    if (bank == RE1C_BANK_A) {
        *out_emr = &view->section[RE1C_SEC_EMR_A];
        *out_edd = &view->section[RE1C_SEC_EDD_A];
        return RE1C_OK;
    }
    if (bank == RE1C_BANK_B) {
        *out_emr = &view->section[RE1C_SEC_EMR_B];
        *out_edd = &view->section[RE1C_SEC_EDD_B];
        return RE1C_OK;
    }
    return RE1C_ERR_RANGE;
}

int re1c_check_host(void)
{
    if (sizeof(re1c_u8) != 1u) return RE1C_ERR_HOST;
    if (sizeof(re1c_u16) != 2u) return RE1C_ERR_HOST;
    if (sizeof(re1c_u32) != 4u) return RE1C_ERR_HOST;
    if (sizeof(re1c_i16) != 2u) return RE1C_ERR_HOST;
    if (sizeof(re1c_i32) != 4u) return RE1C_ERR_HOST;
    return RE1C_OK;
}

const char *re1c_result_string(int code)
{
    switch (code) {
    case RE1C_OK: return "ok";
    case RE1C_ERR_ARGUMENT: return "bad argument";
    case RE1C_ERR_HOST: return "unsupported host integer widths";
    case RE1C_ERR_FORMAT: return "bad RE1 format";
    case RE1C_ERR_RANGE: return "out of range";
    case RE1C_ERR_CAPACITY: return "output capacity too small";
    case RE1C_ERR_MISMATCH: return "canonical byte mismatch";
    case RE1C_ERR_UNSUPPORTED: return "unsupported RE1 packet";
    case RE1C_ERR_NOT_PRESENT: return "RE1 section or animation bank not present";
    default: return "unknown error";
    }
}

const char *re1c_texture_locator_name(void)
{
    return "embedded:TIM";
}

static int re1c_directory_valid(const re1c_u32 off[5], re1c_u32 dir_start)
{
    if (off[0] == 0u) {
        if (off[1] != 0u) return 0;
    } else {
        if (off[0] >= dir_start) return 0;
        if (off[1] <= off[0]) return 0;
    }
    if (off[2] <= off[1] || off[2] >= dir_start) return 0;
    if (off[3] <= off[2] || off[3] >= dir_start) return 0;
    if (off[4] <= off[3] || off[4] >= dir_start) return 0;
    return 1;
}

static void re1c_build_sections(const re1c_u8 *data, re1c_u32 size,
                                re1c_u32 dir_start, const re1c_u32 off[5],
                                re1c_view *out_view)
{
    re1c_u32 begin[RE1C_SECTION_COUNT];
    re1c_u32 end[RE1C_SECTION_COUNT];
    re1c_u32 i;
    begin[0] = 0u;
    end[0] = off[0];
    begin[1] = off[0];
    end[1] = off[1];
    begin[2] = off[1];
    end[2] = off[2];
    begin[3] = off[2];
    end[3] = off[3];
    begin[4] = off[3];
    end[4] = off[4];
    begin[5] = off[4];
    end[5] = dir_start;
    memset(out_view, 0, sizeof(*out_view));
    out_view->file_data = data;
    out_view->file_size = size;
    out_view->section_count = RE1C_SECTION_COUNT;
    for (i = 0u; i < RE1C_SECTION_COUNT; ++i) {
        out_view->section[i].data = data + begin[i];
        out_view->section[i].size = end[i] - begin[i];
        out_view->section[i].source_offset = begin[i];
    }
}

static void re1c_trim_tim_to_declared_size(re1c_view *view)
{
    re1c_section *tim;
    re1c_u32 flags;
    re1c_u32 pos;
    re1c_u32 sec_size;
    if (view == NULL) return;
    tim = &view->section[RE1C_SEC_TIM];
    if (tim->data == NULL || tim->size < 20u) return;
    if (re1c_rd32(tim->data) != 0x10u) return;
    flags = re1c_rd32(tim->data + 4u);
    pos = 8u;
    if ((flags & 8u) != 0u) {
        if (!re1c_range_ok(pos, 12u, tim->size)) return;
        sec_size = re1c_rd32(tim->data + pos);
        if (sec_size < 12u || !re1c_range_ok(pos, sec_size, tim->size)) return;
        pos += sec_size;
    }
    if (!re1c_range_ok(pos, 12u, tim->size)) return;
    sec_size = re1c_rd32(tim->data + pos);
    if (sec_size < 12u || !re1c_range_ok(pos, sec_size, tim->size)) return;
    pos += sec_size;
    tim->size = pos;
}

int re1c_open_emd(const re1c_u8 *data, re1c_u32 size, re1c_view *out_view)
{
    re1c_u32 dir_start;
    re1c_u32 off[5];
    re1c_u32 compact[4];
    re1c_u32 i;
    if (data == NULL || out_view == NULL) return RE1C_ERR_ARGUMENT;
    if (re1c_check_host() != RE1C_OK) return RE1C_ERR_HOST;
    if (size < 16u) return RE1C_ERR_FORMAT;

    if (size >= 20u) {
        dir_start = size - 20u;
        for (i = 0u; i < 5u; ++i) off[i] = re1c_rd32(data + dir_start + i * 4u);
        if (re1c_directory_valid(off, dir_start)) {
            re1c_build_sections(data, size, dir_start, off, out_view);
            re1c_trim_tim_to_declared_size(out_view);
            return RE1C_OK;
        }
    }

    dir_start = size - 16u;
    for (i = 0u; i < 4u; ++i) compact[i] = re1c_rd32(data + dir_start + i * 4u);
    off[0] = 0u;
    off[1] = compact[0];
    off[2] = compact[1];
    off[3] = compact[2];
    off[4] = compact[3];
    if (!re1c_directory_valid(off, dir_start)) return RE1C_ERR_FORMAT;
    re1c_build_sections(data, size, dir_start, off, out_view);
    re1c_trim_tim_to_declared_size(out_view);
    return RE1C_OK;
}

int re1c_open_emw(const re1c_u8 *data, re1c_u32 size, re1c_view *out_view)
{
    re1c_u32 dir_start;
    re1c_u32 emr_end;
    re1c_u32 edd_end;
    if (data == NULL || out_view == NULL) return RE1C_ERR_ARGUMENT;
    if (re1c_check_host() != RE1C_OK) return RE1C_ERR_HOST;
    if (size < 20u) return RE1C_ERR_FORMAT;
    dir_start = size - 8u;
    emr_end = re1c_rd32(data + dir_start + 0u);
    edd_end = re1c_rd32(data + dir_start + 4u);
    if (emr_end == 0u || emr_end >= edd_end || edd_end >= dir_start) return RE1C_ERR_FORMAT;
    if (re1c_rd32(data + edd_end) != 0x41u) return RE1C_ERR_FORMAT;
    memset(out_view, 0, sizeof(*out_view));
    out_view->file_data = data;
    out_view->file_size = size;
    out_view->section_count = RE1C_SECTION_COUNT;
    out_view->section[RE1C_SEC_EMR_B].data = data;
    out_view->section[RE1C_SEC_EMR_B].size = emr_end;
    out_view->section[RE1C_SEC_EMR_B].source_offset = 0u;
    out_view->section[RE1C_SEC_EDD_B].data = data + emr_end;
    out_view->section[RE1C_SEC_EDD_B].size = edd_end - emr_end;
    out_view->section[RE1C_SEC_EDD_B].source_offset = emr_end;
    out_view->section[RE1C_SEC_TMD].data = data + edd_end;
    out_view->section[RE1C_SEC_TMD].size = dir_start - edd_end;
    out_view->section[RE1C_SEC_TMD].source_offset = edd_end;
    out_view->section[RE1C_SEC_EMR_A].data = data;
    out_view->section[RE1C_SEC_EDD_A].data = data;
    out_view->section[RE1C_SEC_TIM].data = data + dir_start;
    out_view->section[RE1C_SEC_TIM].source_offset = dir_start;
    return RE1C_OK;
}

int re1c_open_tmd(const re1c_u8 *data, re1c_u32 size, re1c_view *out_view)
{
    if (data == NULL || out_view == NULL) return RE1C_ERR_ARGUMENT;
    if (re1c_check_host() != RE1C_OK) return RE1C_ERR_HOST;
    if (size < 40u) return RE1C_ERR_FORMAT;
    if (re1c_rd32(data + 0u) != 0x41u) return RE1C_ERR_FORMAT;
    memset(out_view, 0, sizeof(*out_view));
    out_view->file_data = data;
    out_view->file_size = size;
    out_view->section_count = RE1C_SECTION_COUNT;
    out_view->section[RE1C_SEC_EMR_A].data = data;
    out_view->section[RE1C_SEC_EDD_A].data = data;
    out_view->section[RE1C_SEC_EMR_B].data = data;
    out_view->section[RE1C_SEC_EDD_B].data = data;
    out_view->section[RE1C_SEC_TMD].data = data;
    out_view->section[RE1C_SEC_TMD].size = size;
    out_view->section[RE1C_SEC_TMD].source_offset = 0u;
    out_view->section[RE1C_SEC_TIM].data = data + size;
    out_view->section[RE1C_SEC_TIM].source_offset = size;
    return RE1C_OK;
}

int re1c_open_model(const re1c_u8 *data, re1c_u32 size, re1c_view *out_view, re1c_u32 *out_model_kind)
{
    re1c_u32 count;
    int rc;
    if (data == NULL || out_view == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_open_emd(data, size, out_view);
    if (rc == RE1C_OK) {
        rc = re1c_get_tmd_object_count(out_view, &count);
        if (rc == RE1C_OK) {
            if (out_model_kind != NULL) *out_model_kind = RE1C_MODEL_EMD;
            return RE1C_OK;
        }
    }
    rc = re1c_open_emw(data, size, out_view);
    if (rc == RE1C_OK) {
        rc = re1c_get_tmd_object_count(out_view, &count);
        if (rc == RE1C_OK) {
            if (out_model_kind != NULL) *out_model_kind = RE1C_MODEL_EMW;
            return RE1C_OK;
        }
    }
    rc = re1c_open_tmd(data, size, out_view);
    if (rc == RE1C_OK) {
        rc = re1c_get_tmd_object_count(out_view, &count);
        if (rc == RE1C_OK) {
            if (out_model_kind != NULL) *out_model_kind = RE1C_MODEL_TMD;
            return RE1C_OK;
        }
    }
    return RE1C_ERR_FORMAT;
}

int re1c_open_sections(const re1c_section sections[RE1C_SECTION_COUNT], re1c_view *out_view)
{
    re1c_u32 i;
    if (sections == NULL || out_view == NULL) return RE1C_ERR_ARGUMENT;
    if (re1c_check_host() != RE1C_OK) return RE1C_ERR_HOST;
    memset(out_view, 0, sizeof(*out_view));
    out_view->section_count = RE1C_SECTION_COUNT;
    for (i = 0u; i < RE1C_SECTION_COUNT; ++i) {
        if (sections[i].size != 0u && sections[i].data == NULL) return RE1C_ERR_ARGUMENT;
        if (i == RE1C_SEC_TMD && sections[i].size == 0u) return RE1C_ERR_ARGUMENT;
        out_view->section[i] = sections[i];
    }
    return RE1C_OK;
}

int re1c_has_section(const re1c_view *view, re1c_u32 section_index, re1c_u32 *out_present)
{
    if (view == NULL || out_present == NULL) return RE1C_ERR_ARGUMENT;
    if (view->section_count != RE1C_SECTION_COUNT) return RE1C_ERR_FORMAT;
    if (section_index >= RE1C_SECTION_COUNT) return RE1C_ERR_RANGE;
    *out_present = view->section[section_index].size != 0u ? 1u : 0u;
    return RE1C_OK;
}

int re1c_bank_available(const re1c_view *view, re1c_u32 bank, re1c_u32 *out_present)
{
    const re1c_section *emr;
    const re1c_section *edd;
    int rc;
    if (out_present == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_bank_sections(view, bank, &emr, &edd);
    if (rc != RE1C_OK) return rc;
    if (emr->size == 0u && edd->size == 0u) {
        *out_present = 0u;
        return RE1C_OK;
    }
    if (emr->size == 0u || edd->size == 0u) return RE1C_ERR_FORMAT;
    if (emr->data == NULL || edd->data == NULL) return RE1C_ERR_FORMAT;
    *out_present = 1u;
    return RE1C_OK;
}

int re1c_get_preferred_bank(const re1c_view *view, re1c_u32 *out_bank)
{
    re1c_u32 present;
    int rc;
    if (out_bank == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_bank_available(view, RE1C_BANK_B, &present);
    if (rc != RE1C_OK) return rc;
    if (present != 0u) {
        *out_bank = RE1C_BANK_B;
        return RE1C_OK;
    }
    rc = re1c_bank_available(view, RE1C_BANK_A, &present);
    if (rc != RE1C_OK) return rc;
    if (present != 0u) {
        *out_bank = RE1C_BANK_A;
        return RE1C_OK;
    }
    return RE1C_ERR_NOT_PRESENT;
}

re1c_i32 re1c_position_word_to_centi(re1c_u16 word)
{
    if (word > 32767u) return (re1c_i32)(65535u - (re1c_u32)word);
    return -(re1c_i32)word;
}

re1c_i32 re1c_centi_to_q16(re1c_i32 centi)
{
    return (re1c_i32)((centi * (re1c_i32)RE1C_Q16_ONE) / 100);
}

re1c_i32 re1c_angle12_to_turn_q16(re1c_i16 angle12)
{
    return (re1c_i32)(((re1c_i32)angle12 * (re1c_i32)RE1C_Q16_ONE) / 4095);
}

int re1c_get_bank_info(const re1c_view *view, re1c_u32 bank, re1c_bank_info *out_info)
{
    const re1c_section *emr;
    const re1c_section *edd;
    re1c_u32 first_stream;
    re1c_u32 i;
    re1c_u32 type_value;
    int rc;
    if (out_info == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_bank_sections(view, bank, &emr, &edd);
    if (rc != RE1C_OK) return rc;
    if (emr->size == 0u && edd->size == 0u) return RE1C_ERR_NOT_PRESENT;
    if (emr->size == 0u || edd->size == 0u) return RE1C_ERR_FORMAT;
    if (emr->data == NULL || edd->data == NULL) return RE1C_ERR_FORMAT;
    if (emr->size < 8u || edd->size < 4u) return RE1C_ERR_FORMAT;
    out_info->part_list_offset = re1c_rd16(emr->data + 0u);
    out_info->animation_offset = re1c_rd16(emr->data + 2u);
    out_info->object_count = re1c_rd16(emr->data + 4u);
    out_info->frame_size = re1c_rd16(emr->data + 6u);
    if (out_info->object_count == 0u || out_info->object_count > 128u) return RE1C_ERR_FORMAT;
    if (out_info->frame_size < 12u + (re1c_u32)out_info->object_count * 6u) return RE1C_ERR_FORMAT;
    if (!re1c_range_ok(out_info->part_list_offset, (re1c_u32)out_info->object_count * 4u, emr->size)) return RE1C_ERR_FORMAT;
    if (out_info->animation_offset >= emr->size) return RE1C_ERR_FORMAT;
    if (emr->size < (re1c_u32)out_info->animation_offset + 4u) return RE1C_ERR_FORMAT;
    out_info->pose_count = (emr->size - 4u - (re1c_u32)out_info->animation_offset) / (re1c_u32)out_info->frame_size;
    first_stream = re1c_rd16(edd->data + 2u);
    if (first_stream < 4u || first_stream > edd->size || (first_stream & 3u) != 0u) return RE1C_ERR_FORMAT;
    out_info->clip_count = first_stream / 4u;
    type_value = 0u;
    for (i = 0u; i < (re1c_u32)out_info->object_count; ++i) {
        type_value ^= re1c_rd32(emr->data + out_info->part_list_offset + i * 4u);
    }
    out_info->emd_type = type_value;
    return RE1C_OK;
}

static int re1c_find_parent(const re1c_view *view, re1c_u32 bank, re1c_u32 object_index, re1c_i16 *out_parent)
{
    re1c_bank_info bi;
    const re1c_section *emr;
    const re1c_section *edd;
    re1c_u32 i;
    int rc;
    rc = re1c_get_bank_info(view, bank, &bi);
    if (rc != RE1C_OK) return rc;
    if (object_index >= (re1c_u32)bi.object_count) return RE1C_ERR_RANGE;
    rc = re1c_get_bank_sections(view, bank, &emr, &edd);
    if (rc != RE1C_OK) return rc;
    (void)edd;
    *out_parent = (re1c_i16)-1;
    for (i = 0u; i < (re1c_u32)bi.object_count; ++i) {
        re1c_u16 count;
        re1c_u16 rel;
        re1c_u32 j;
        count = re1c_rd16(emr->data + bi.part_list_offset + i * 4u + 0u);
        rel = re1c_rd16(emr->data + bi.part_list_offset + i * 4u + 2u);
        if (!re1c_range_ok((re1c_u32)bi.part_list_offset + (re1c_u32)rel, (re1c_u32)count, emr->size)) return RE1C_ERR_FORMAT;
        for (j = 0u; j < (re1c_u32)count; ++j) {
            if ((re1c_u32)emr->data[bi.part_list_offset + rel + j] == object_index) {
                *out_parent = (re1c_i16)i;
                return RE1C_OK;
            }
        }
    }
    return RE1C_OK;
}

int re1c_get_pivot(const re1c_view *view, re1c_u32 bank, re1c_u32 object_index, re1c_pivot *out_pivot)
{
    re1c_bank_info bi;
    const re1c_section *emr;
    const re1c_section *edd;
    const re1c_u8 *p;
    int rc;
    if (out_pivot == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_bank_info(view, bank, &bi);
    if (rc != RE1C_OK) return rc;
    if (object_index >= (re1c_u32)bi.object_count) return RE1C_ERR_RANGE;
    rc = re1c_get_bank_sections(view, bank, &emr, &edd);
    if (rc != RE1C_OK) return rc;
    (void)edd;
    if (!re1c_range_ok(8u + object_index * 6u, 6u, emr->size)) return RE1C_ERR_FORMAT;
    p = emr->data + 8u + object_index * 6u;
    out_pivot->z_centi = re1c_position_word_to_centi(re1c_rd16(p + 0u));
    out_pivot->y_centi = re1c_position_word_to_centi(re1c_rd16(p + 2u));
    out_pivot->x_centi = re1c_position_word_to_centi(re1c_rd16(p + 4u));
    rc = re1c_find_parent(view, bank, object_index, &out_pivot->parent);
    return rc;
}

int re1c_get_bind_global_centi(const re1c_view *view, re1c_u32 bank, re1c_u32 object_index, re1c_vec3_centi *out_position)
{
    re1c_bank_info bi;
    re1c_pivot p;
    re1c_i16 current;
    re1c_u32 guard;
    int rc;
    if (out_position == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_bank_info(view, bank, &bi);
    if (rc != RE1C_OK) return rc;
    if (object_index >= (re1c_u32)bi.object_count) return RE1C_ERR_RANGE;
    out_position->x = 0;
    out_position->y = 0;
    out_position->z = 0;
    current = (re1c_i16)object_index;
    guard = 0u;
    while (current >= 0) {
        if (guard++ > (re1c_u32)bi.object_count) return RE1C_ERR_FORMAT;
        rc = re1c_get_pivot(view, bank, (re1c_u32)current, &p);
        if (rc != RE1C_OK) return rc;
        out_position->x += p.x_centi;
        out_position->y += p.y_centi;
        out_position->z += p.z_centi;
        current = p.parent;
    }
    return RE1C_OK;
}

int re1c_get_child_count(const re1c_view *view, re1c_u32 bank, re1c_u32 object_index, re1c_u16 *out_count)
{
    re1c_bank_info bi;
    const re1c_section *emr;
    const re1c_section *edd;
    int rc;
    if (out_count == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_bank_info(view, bank, &bi);
    if (rc != RE1C_OK) return rc;
    if (object_index >= (re1c_u32)bi.object_count) return RE1C_ERR_RANGE;
    rc = re1c_get_bank_sections(view, bank, &emr, &edd);
    if (rc != RE1C_OK) return rc;
    (void)edd;
    *out_count = re1c_rd16(emr->data + bi.part_list_offset + object_index * 4u);
    return RE1C_OK;
}

int re1c_get_child(const re1c_view *view, re1c_u32 bank, re1c_u32 object_index, re1c_u32 child_index, re1c_u8 *out_child)
{
    re1c_bank_info bi;
    const re1c_section *emr;
    const re1c_section *edd;
    re1c_u16 count;
    re1c_u16 rel;
    int rc;
    if (out_child == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_bank_info(view, bank, &bi);
    if (rc != RE1C_OK) return rc;
    if (object_index >= (re1c_u32)bi.object_count) return RE1C_ERR_RANGE;
    rc = re1c_get_bank_sections(view, bank, &emr, &edd);
    if (rc != RE1C_OK) return rc;
    (void)edd;
    count = re1c_rd16(emr->data + bi.part_list_offset + object_index * 4u + 0u);
    rel = re1c_rd16(emr->data + bi.part_list_offset + object_index * 4u + 2u);
    if (child_index >= (re1c_u32)count) return RE1C_ERR_RANGE;
    if (!re1c_range_ok((re1c_u32)bi.part_list_offset + (re1c_u32)rel, (re1c_u32)count, emr->size)) return RE1C_ERR_FORMAT;
    *out_child = emr->data[bi.part_list_offset + rel + child_index];
    return RE1C_OK;
}

int re1c_get_clip(const re1c_view *view, re1c_u32 bank, re1c_u32 clip_index, re1c_clip *out_clip)
{
    re1c_bank_info bi;
    const re1c_section *emr;
    const re1c_section *edd;
    int rc;
    if (out_clip == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_bank_info(view, bank, &bi);
    if (rc != RE1C_OK) return rc;
    if (clip_index >= bi.clip_count) return RE1C_ERR_RANGE;
    rc = re1c_get_bank_sections(view, bank, &emr, &edd);
    if (rc != RE1C_OK) return rc;
    (void)emr;
    out_clip->frame_count = re1c_rd16(edd->data + clip_index * 4u + 0u);
    out_clip->stream_offset = re1c_rd16(edd->data + clip_index * 4u + 2u);
    if (out_clip->frame_count == 0u) return RE1C_ERR_FORMAT;
    if (!re1c_range_ok(out_clip->stream_offset, (re1c_u32)out_clip->frame_count * 4u, edd->size)) return RE1C_ERR_FORMAT;
    return RE1C_OK;
}

int re1c_get_frame_ref(const re1c_view *view, re1c_u32 bank, re1c_u32 clip_index, re1c_u32 frame_index, re1c_frame_ref *out_ref)
{
    re1c_clip clip;
    const re1c_section *emr;
    const re1c_section *edd;
    re1c_u32 pos;
    int rc;
    if (out_ref == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_clip(view, bank, clip_index, &clip);
    if (rc != RE1C_OK) return rc;
    if (frame_index >= (re1c_u32)clip.frame_count) return RE1C_ERR_RANGE;
    rc = re1c_get_bank_sections(view, bank, &emr, &edd);
    if (rc != RE1C_OK) return rc;
    (void)emr;
    pos = (re1c_u32)clip.stream_offset + frame_index * 4u;
    out_ref->pose_index = re1c_rd16(edd->data + pos + 0u);
    out_ref->unknown_word = re1c_rd16(edd->data + pos + 2u);
    out_ref->step_units = out_ref->unknown_word;
    return RE1C_OK;
}

int re1c_get_mv_frame_duration_ms(re1c_u32 *out_ms)
{
    if (out_ms == NULL) return RE1C_ERR_ARGUMENT;
    *out_ms = RE1C_MV_FRAME_MS;
    return RE1C_OK;
}

int re1c_get_clip_duration_ms(const re1c_view *view, re1c_u32 bank, re1c_u32 clip_index, re1c_u32 *out_ms)
{
    re1c_clip clip;
    int rc;
    if (out_ms == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_clip(view, bank, clip_index, &clip);
    if (rc != RE1C_OK) return rc;
    *out_ms = (re1c_u32)clip.frame_count * RE1C_MV_FRAME_MS;
    return RE1C_OK;
}

static int re1c_get_pose_ptr(const re1c_view *view, re1c_u32 bank, re1c_u32 pose_index,
                             const re1c_u8 **out_pose, re1c_bank_info *out_bi)
{
    re1c_bank_info bi;
    const re1c_section *emr;
    const re1c_section *edd;
    re1c_u32 pos;
    int rc;
    if (out_pose == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_bank_info(view, bank, &bi);
    if (rc != RE1C_OK) return rc;
    if (pose_index >= bi.pose_count) return RE1C_ERR_RANGE;
    rc = re1c_get_bank_sections(view, bank, &emr, &edd);
    if (rc != RE1C_OK) return rc;
    (void)edd;
    pos = (re1c_u32)bi.animation_offset + pose_index * (re1c_u32)bi.frame_size;
    if (!re1c_range_ok(pos, bi.frame_size, emr->size)) return RE1C_ERR_FORMAT;
    *out_pose = emr->data + pos;
    if (out_bi != NULL) *out_bi = bi;
    return RE1C_OK;
}

int re1c_get_pose_root(const re1c_view *view, re1c_u32 bank, re1c_u32 pose_index, re1c_pose_root *out_root)
{
    const re1c_u8 *p;
    int rc;
    if (out_root == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_pose_ptr(view, bank, pose_index, &p, NULL);
    if (rc != RE1C_OK) return rc;
    out_root->z_centi = re1c_position_word_to_centi(re1c_rd16(p + 0u));
    out_root->y_centi = re1c_position_word_to_centi(re1c_rd16(p + 2u));
    out_root->x_centi = re1c_position_word_to_centi(re1c_rd16(p + 4u));
    return RE1C_OK;
}

int re1c_get_pose_angle(const re1c_view *view, re1c_u32 bank, re1c_u32 pose_index, re1c_u32 object_index, re1c_pose_angle *out_angle)
{
    const re1c_u8 *p;
    re1c_bank_info bi;
    re1c_u32 off;
    int rc;
    if (out_angle == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_pose_ptr(view, bank, pose_index, &p, &bi);
    if (rc != RE1C_OK) return rc;
    if (object_index >= (re1c_u32)bi.object_count) return RE1C_ERR_RANGE;
    off = 12u + object_index * 6u;
    out_angle->file_ax12 = (re1c_u16)(re1c_rd16(p + off + 0u) & 0x0FFFu);
    out_angle->file_ay12 = (re1c_u16)(re1c_rd16(p + off + 2u) & 0x0FFFu);
    out_angle->file_az12 = (re1c_u16)(re1c_rd16(p + off + 4u) & 0x0FFFu);
    out_angle->render_x12 = (re1c_i16)(-(re1c_i16)out_angle->file_az12);
    out_angle->render_y12 = (re1c_i16)(-(re1c_i16)out_angle->file_ay12);
    out_angle->render_z12 = (re1c_i16)(-(re1c_i16)out_angle->file_ax12);
    return RE1C_OK;
}

int re1c_get_tmd_object_count(const re1c_view *view, re1c_u32 *out_count)
{
    const re1c_section *tmd;
    if (view == NULL || out_count == NULL) return RE1C_ERR_ARGUMENT;
    if (view->section_count != RE1C_SECTION_COUNT) return RE1C_ERR_FORMAT;
    tmd = &view->section[RE1C_SEC_TMD];
    if (tmd->size == 0u) return RE1C_ERR_NOT_PRESENT;
    if (tmd->data == NULL || tmd->size < 12u) return RE1C_ERR_FORMAT;
    *out_count = re1c_rd32(tmd->data + 8u);
    if (*out_count == 0u || *out_count > 2048u) return RE1C_ERR_FORMAT;
    if (!re1c_range_ok(12u, *out_count * 28u, tmd->size)) return RE1C_ERR_FORMAT;
    return RE1C_OK;
}

int re1c_get_tmd_object(const re1c_view *view, re1c_u32 object_index, re1c_tmd_object *out_object)
{
    const re1c_section *tmd;
    re1c_u32 count;
    const re1c_u8 *d;
    int rc;
    if (out_object == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_tmd_object_count(view, &count);
    if (rc != RE1C_OK) return rc;
    if (object_index >= count) return RE1C_ERR_RANGE;
    tmd = &view->section[RE1C_SEC_TMD];
    d = tmd->data + 12u + object_index * 28u;
    out_object->vertex_offset = re1c_rd32(d + 0u);
    out_object->vertex_count = re1c_rd32(d + 4u);
    out_object->normal_offset = re1c_rd32(d + 8u);
    out_object->normal_count = re1c_rd32(d + 12u);
    out_object->primitive_offset = re1c_rd32(d + 16u);
    out_object->primitive_count = re1c_rd32(d + 20u);
    out_object->scale_word = re1c_rd32(d + 24u);
    if (!re1c_range_ok(12u + out_object->vertex_offset, out_object->vertex_count * 8u, tmd->size)) return RE1C_ERR_FORMAT;
    if (!re1c_range_ok(12u + out_object->normal_offset, out_object->normal_count * 8u, tmd->size)) return RE1C_ERR_FORMAT;
    if (!re1c_range_ok(12u + out_object->primitive_offset, 0u, tmd->size)) return RE1C_ERR_FORMAT;
    return RE1C_OK;
}

int re1c_get_vertex_raw(const re1c_view *view, re1c_u32 object_index, re1c_u32 vertex_index, re1c_vertex_raw *out_vertex)
{
    re1c_tmd_object obj;
    const re1c_section *tmd;
    const re1c_u8 *p;
    int rc;
    if (out_vertex == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_tmd_object(view, object_index, &obj);
    if (rc != RE1C_OK) return rc;
    if (vertex_index >= obj.vertex_count) return RE1C_ERR_RANGE;
    tmd = &view->section[RE1C_SEC_TMD];
    p = tmd->data + 12u + obj.vertex_offset + vertex_index * 8u;
    out_vertex->file_x = re1c_rd16(p + 0u);
    out_vertex->file_y = re1c_rd16(p + 2u);
    out_vertex->file_z = re1c_rd16(p + 4u);
    out_vertex->pad = re1c_rd16(p + 6u);
    return RE1C_OK;
}

int re1c_get_vertex_centi(const re1c_view *view, re1c_u32 object_index, re1c_u32 vertex_index, re1c_vec3_centi *out_vertex)
{
    re1c_vertex_raw raw;
    int rc;
    if (out_vertex == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_vertex_raw(view, object_index, vertex_index, &raw);
    if (rc != RE1C_OK) return rc;
    out_vertex->x = re1c_position_word_to_centi(raw.file_z);
    out_vertex->y = re1c_position_word_to_centi(raw.file_y);
    out_vertex->z = re1c_position_word_to_centi(raw.file_x);
    return RE1C_OK;
}

int re1c_get_normal_raw(const re1c_view *view, re1c_u32 object_index, re1c_u32 normal_index, re1c_vertex_raw *out_normal)
{
    re1c_tmd_object obj;
    const re1c_section *tmd;
    const re1c_u8 *p;
    int rc;
    if (out_normal == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_tmd_object(view, object_index, &obj);
    if (rc != RE1C_OK) return rc;
    if (normal_index >= obj.normal_count) return RE1C_ERR_RANGE;
    tmd = &view->section[RE1C_SEC_TMD];
    p = tmd->data + 12u + obj.normal_offset + normal_index * 8u;
    out_normal->file_x = re1c_rd16(p + 0u);
    out_normal->file_y = re1c_rd16(p + 2u);
    out_normal->file_z = re1c_rd16(p + 4u);
    out_normal->pad = re1c_rd16(p + 6u);
    return RE1C_OK;
}

static int re1c_locate_primitive(const re1c_view *view, re1c_u32 object_index,
                                 re1c_u32 primitive_index, const re1c_u8 **out_packet,
                                 re1c_u32 *out_packet_size)
{
    re1c_tmd_object obj;
    const re1c_section *tmd;
    re1c_u32 i;
    re1c_u32 pos;
    re1c_u32 packet_size;
    int rc;
    if (out_packet == NULL || out_packet_size == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_get_tmd_object(view, object_index, &obj);
    if (rc != RE1C_OK) return rc;
    if (primitive_index >= obj.primitive_count) return RE1C_ERR_RANGE;
    tmd = &view->section[RE1C_SEC_TMD];
    pos = 12u + obj.primitive_offset;
    for (i = 0u; i <= primitive_index; ++i) {
        if (!re1c_range_ok(pos, 4u, tmd->size)) return RE1C_ERR_FORMAT;
        packet_size = 4u + (re1c_u32)tmd->data[pos + 1u] * 4u;
        if (packet_size < 4u || !re1c_range_ok(pos, packet_size, tmd->size)) return RE1C_ERR_FORMAT;
        if (i == primitive_index) {
            *out_packet = tmd->data + pos;
            *out_packet_size = packet_size;
            return RE1C_OK;
        }
        pos += packet_size;
    }
    return RE1C_ERR_RANGE;
}

int re1c_get_primitive(const re1c_view *view, re1c_u32 object_index, re1c_u32 primitive_index, re1c_primitive *out_primitive)
{
    const re1c_u8 *p;
    re1c_u32 packet_size;
    int rc;
    if (out_primitive == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_locate_primitive(view, object_index, primitive_index, &p, &packet_size);
    if (rc != RE1C_OK) return rc;
    memset(out_primitive, 0, sizeof(*out_primitive));
    out_primitive->raw_packet = p;
    out_primitive->raw_packet_size = packet_size;
    out_primitive->code = re1c_rd32(p + 0u);

    if (out_primitive->code == RE1C_PRIM_FT3_A || out_primitive->code == RE1C_PRIM_FT3_B || out_primitive->code == RE1C_PRIM_FT3_C) {
        if (packet_size < 28u) return RE1C_ERR_FORMAT;
        out_primitive->sides = 3u;
        out_primitive->u[0] = p[4]; out_primitive->v[0] = p[5];
        out_primitive->cba = re1c_rd16(p + 6u);
        out_primitive->u[1] = p[8]; out_primitive->v[1] = p[9];
        out_primitive->tsb = re1c_rd16(p + 10u);
        out_primitive->u[2] = p[12]; out_primitive->v[2] = p[13];
        out_primitive->normal_index[0] = re1c_rd16(p + 16u);
        out_primitive->vertex_index[0] = re1c_rd16(p + 18u);
        out_primitive->normal_index[1] = re1c_rd16(p + 20u);
        out_primitive->vertex_index[1] = re1c_rd16(p + 22u);
        out_primitive->normal_index[2] = re1c_rd16(p + 24u);
        out_primitive->vertex_index[2] = re1c_rd16(p + 26u);
        return RE1C_OK;
    }
    if (out_primitive->code == RE1C_PRIM_FT4_A || out_primitive->code == RE1C_PRIM_FT4_B || out_primitive->code == RE1C_PRIM_FT4_C) {
        if (packet_size < 36u) return RE1C_ERR_FORMAT;
        out_primitive->sides = 4u;
        out_primitive->u[0] = p[4]; out_primitive->v[0] = p[5];
        out_primitive->cba = re1c_rd16(p + 6u);
        out_primitive->u[1] = p[8]; out_primitive->v[1] = p[9];
        out_primitive->tsb = re1c_rd16(p + 10u);
        out_primitive->u[2] = p[12]; out_primitive->v[2] = p[13];
        out_primitive->u[3] = p[16]; out_primitive->v[3] = p[17];
        out_primitive->normal_index[0] = re1c_rd16(p + 20u);
        out_primitive->vertex_index[0] = re1c_rd16(p + 22u);
        out_primitive->normal_index[1] = re1c_rd16(p + 24u);
        out_primitive->vertex_index[1] = re1c_rd16(p + 26u);
        out_primitive->normal_index[2] = re1c_rd16(p + 28u);
        out_primitive->vertex_index[2] = re1c_rd16(p + 30u);
        out_primitive->normal_index[3] = re1c_rd16(p + 32u);
        out_primitive->vertex_index[3] = re1c_rd16(p + 34u);
        return RE1C_OK;
    }
    out_primitive->sides = 0u;
    return RE1C_ERR_UNSUPPORTED;
}

int re1c_get_tim_info(const re1c_view *view, re1c_tim_info *out_info)
{
    const re1c_section *tim;
    re1c_u32 flags;
    re1c_u32 pos;
    re1c_u32 sec_size;
    if (view == NULL || out_info == NULL) return RE1C_ERR_ARGUMENT;
    if (view->section_count != RE1C_SECTION_COUNT) return RE1C_ERR_FORMAT;
    tim = &view->section[RE1C_SEC_TIM];
    if (tim->size == 0u) return RE1C_ERR_NOT_PRESENT;
    if (tim->data == NULL || tim->size < 20u) return RE1C_ERR_FORMAT;
    if (re1c_rd32(tim->data + 0u) != 0x10u) return RE1C_ERR_FORMAT;
    memset(out_info, 0, sizeof(*out_info));
    flags = re1c_rd32(tim->data + 4u);
    out_info->pixel_mode = flags & 7u;
    out_info->has_clut = ((flags & 8u) != 0u) ? 1u : 0u;
    pos = 8u;
    if (out_info->has_clut != 0u) {
        if (!re1c_range_ok(pos, 12u, tim->size)) return RE1C_ERR_FORMAT;
        sec_size = re1c_rd32(tim->data + pos);
        if (sec_size < 12u || !re1c_range_ok(pos, sec_size, tim->size)) return RE1C_ERR_FORMAT;
        out_info->clut_x = re1c_rd16(tim->data + pos + 4u);
        out_info->clut_y = re1c_rd16(tim->data + pos + 6u);
        out_info->clut_w = re1c_rd16(tim->data + pos + 8u);
        out_info->clut_h = re1c_rd16(tim->data + pos + 10u);
        out_info->clut_data = tim->data + pos + 12u;
        out_info->clut_bytes = sec_size - 12u;
        pos += sec_size;
    }
    if (!re1c_range_ok(pos, 12u, tim->size)) return RE1C_ERR_FORMAT;
    sec_size = re1c_rd32(tim->data + pos);
    if (sec_size < 12u || !re1c_range_ok(pos, sec_size, tim->size)) return RE1C_ERR_FORMAT;
    out_info->pixel_x = re1c_rd16(tim->data + pos + 4u);
    out_info->pixel_y = re1c_rd16(tim->data + pos + 6u);
    out_info->pixel_w_words = re1c_rd16(tim->data + pos + 8u);
    out_info->pixel_h = re1c_rd16(tim->data + pos + 10u);
    out_info->pixel_data = tim->data + pos + 12u;
    out_info->pixel_bytes = sec_size - 12u;
    if (out_info->pixel_mode == 0u) out_info->width_pixels = (re1c_u32)out_info->pixel_w_words * 4u;
    else if (out_info->pixel_mode == 1u) out_info->width_pixels = (re1c_u32)out_info->pixel_w_words * 2u;
    else if (out_info->pixel_mode == 2u) out_info->width_pixels = (re1c_u32)out_info->pixel_w_words;
    else if (out_info->pixel_mode == 3u) out_info->width_pixels = ((re1c_u32)out_info->pixel_w_words * 2u) / 3u;
    else return RE1C_ERR_UNSUPPORTED;
    out_info->height_pixels = out_info->pixel_h;
    return RE1C_OK;
}

int re1c_get_primitive_page_xy(const re1c_tim_info *tim, const re1c_primitive *prim, re1c_i32 *out_page_x, re1c_i32 *out_page_y)
{
    re1c_i32 page_no;
    if (tim == NULL || prim == NULL || out_page_x == NULL || out_page_y == NULL) return RE1C_ERR_ARGUMENT;
    page_no = (re1c_i32)(prim->tsb & 0x000Fu);
    if (tim->pixel_mode == 0u) *out_page_x = page_no * 256;
    else if (tim->pixel_mode == 1u) *out_page_x = page_no * 128;
    else *out_page_x = page_no * 64;
    *out_page_y = ((prim->tsb & 0x0010u) != 0u) ? 256 : 0;
    return RE1C_OK;
}

int re1c_get_primitive_clut_xy(const re1c_primitive *prim, re1c_i32 *out_clut_x, re1c_i32 *out_clut_y)
{
    if (prim == NULL || out_clut_x == NULL || out_clut_y == NULL) return RE1C_ERR_ARGUMENT;
    *out_clut_x = (re1c_i32)((prim->cba & 0x003Fu) * 16u);
    *out_clut_y = (re1c_i32)((prim->cba >> 6) & 0x01FFu);
    return RE1C_OK;
}

int re1c_tim_sample_index(const re1c_tim_info *tim, const re1c_primitive *prim, re1c_u8 u, re1c_u8 v, re1c_u16 *out_psx1555)
{
    re1c_i32 page_x;
    re1c_i32 page_y;
    re1c_i32 sx;
    re1c_i32 sy;
    re1c_i32 cx;
    re1c_i32 cy;
    re1c_i32 row;
    re1c_i32 col;
    re1c_u32 idx;
    re1c_u32 colors;
    re1c_u32 row_bytes;
    re1c_u8 packed;
    int rc;
    if (tim == NULL || prim == NULL || out_psx1555 == NULL) return RE1C_ERR_ARGUMENT;
    if (tim->pixel_data == NULL || tim->clut_data == NULL) return RE1C_ERR_FORMAT;
    if (tim->pixel_mode != 0u && tim->pixel_mode != 1u) return RE1C_ERR_UNSUPPORTED;
    rc = re1c_get_primitive_page_xy(tim, prim, &page_x, &page_y);
    if (rc != RE1C_OK) return rc;
    sx = page_x + (re1c_i32)u;
    sy = page_y + (re1c_i32)v;
    if (sx < 0 || sy < 0 || (re1c_u32)sx >= tim->width_pixels || (re1c_u32)sy >= tim->height_pixels) return RE1C_ERR_RANGE;
    row_bytes = (re1c_u32)tim->pixel_w_words * 2u;
    if (tim->pixel_mode == 0u) {
        packed = tim->pixel_data[(re1c_u32)sy * row_bytes + ((re1c_u32)sx >> 1)];
        idx = (((re1c_u32)sx & 1u) != 0u) ? ((re1c_u32)packed >> 4) & 15u : (re1c_u32)packed & 15u;
        colors = 16u;
    } else {
        idx = tim->pixel_data[(re1c_u32)sy * row_bytes + (re1c_u32)sx];
        colors = 256u;
    }
    re1c_get_primitive_clut_xy(prim, &cx, &cy);
    row = cy - (re1c_i32)tim->clut_y;
    col = cx - (re1c_i32)tim->clut_x;
    if (row < 0 || col < 0) return RE1C_ERR_RANGE;
    if ((re1c_u32)row >= (re1c_u32)tim->clut_h) return RE1C_ERR_RANGE;
    if ((re1c_u32)col + colors > (re1c_u32)tim->clut_w) return RE1C_ERR_RANGE;
    *out_psx1555 = re1c_rd16(tim->clut_data + (((re1c_u32)row * (re1c_u32)tim->clut_w) + (re1c_u32)col + idx) * 2u);
    return RE1C_OK;
}

void re1c_psx1555_to_rgba8(re1c_u16 color, re1c_u8 rgba[4])
{
    if (rgba == NULL) return;
    rgba[0] = (re1c_u8)(((re1c_u32)(color & 31u) * 255u) / 31u);
    rgba[1] = (re1c_u8)(((re1c_u32)((color >> 5) & 31u) * 255u) / 31u);
    rgba[2] = (re1c_u8)(((re1c_u32)((color >> 10) & 31u) * 255u) / 31u);
    rgba[3] = (color == 0u) ? 0u : 255u;
}

int re1c_validate(const re1c_view *view, re1c_stats *out_stats)
{
    re1c_stats st;
    re1c_bank_info bi;
    re1c_tim_info tim;
    re1c_u32 objects;
    re1c_u32 oi;
    re1c_u32 present;
    int rc;
    if (view == NULL) return RE1C_ERR_ARGUMENT;
    memset(&st, 0, sizeof(st));

    rc = re1c_bank_available(view, RE1C_BANK_A, &present); if (rc != RE1C_OK) return rc;
    st.bank_a_present = present;
    if (present != 0u) {
        rc = re1c_get_bank_info(view, RE1C_BANK_A, &bi); if (rc != RE1C_OK) return rc;
        st.bank_a_object_count = (re1c_u32)bi.object_count;
        st.bank_a_clips = bi.clip_count;
        st.bank_a_poses = bi.pose_count;
    }
    rc = re1c_bank_available(view, RE1C_BANK_B, &present); if (rc != RE1C_OK) return rc;
    st.bank_b_present = present;
    if (present != 0u) {
        rc = re1c_get_bank_info(view, RE1C_BANK_B, &bi); if (rc != RE1C_OK) return rc;
        st.bank_b_object_count = (re1c_u32)bi.object_count;
        st.bank_b_clips = bi.clip_count;
        st.bank_b_poses = bi.pose_count;
    }

    rc = re1c_get_tmd_object_count(view, &objects); if (rc != RE1C_OK) return rc;
    st.object_count = objects;
    for (oi = 0u; oi < objects; ++oi) {
        re1c_tmd_object obj;
        re1c_u32 pi;
        rc = re1c_get_tmd_object(view, oi, &obj); if (rc != RE1C_OK) return rc;
        st.vertex_count += obj.vertex_count;
        st.normal_count += obj.normal_count;
        st.primitive_count += obj.primitive_count;
        for (pi = 0u; pi < obj.primitive_count; ++pi) {
            re1c_primitive prim;
            rc = re1c_get_primitive(view, oi, pi, &prim);
            if (rc == RE1C_OK) {
                re1c_u32 k;
                st.supported_primitive_count++;
                for (k = 0u; k < (re1c_u32)prim.sides; ++k) {
                    if ((re1c_u32)prim.vertex_index[k] >= obj.vertex_count) return RE1C_ERR_FORMAT;
                    if ((re1c_u32)prim.normal_index[k] >= obj.normal_count) return RE1C_ERR_FORMAT;
                }
            } else if (rc == RE1C_ERR_UNSUPPORTED) st.unsupported_primitive_count++;
            else return rc;
        }
    }
    rc = re1c_get_tim_info(view, &tim);
    if (rc == RE1C_OK) {
        st.texture_present = 1u;
        st.texture_width = tim.width_pixels;
        st.texture_height = tim.height_pixels;
    } else if (rc != RE1C_ERR_NOT_PRESENT) return rc;
    if (out_stats != NULL) *out_stats = st;
    return RE1C_OK;
}

typedef struct re1c_writer_s {
    re1c_u8 *data;
    re1c_u32 cap;
    re1c_u32 pos;
    int error;
} re1c_writer;

static void re1c_w8(re1c_writer *w, re1c_u8 v)
{
    if (w->error != 0) return;
    if (w->pos >= w->cap) { w->error = RE1C_ERR_CAPACITY; return; }
    w->data[w->pos++] = v;
}

static void re1c_w16(re1c_writer *w, re1c_u16 v)
{
    re1c_w8(w, (re1c_u8)(v & 255u));
    re1c_w8(w, (re1c_u8)((v >> 8) & 255u));
}

static void re1c_w32(re1c_writer *w, re1c_u32 v)
{
    re1c_w8(w, (re1c_u8)(v & 255u));
    re1c_w8(w, (re1c_u8)((v >> 8) & 255u));
    re1c_w8(w, (re1c_u8)((v >> 16) & 255u));
    re1c_w8(w, (re1c_u8)((v >> 24) & 255u));
}

static void re1c_wi32(re1c_writer *w, re1c_i32 v)
{
    re1c_w32(w, (re1c_u32)v);
}

static void re1c_wbytes(re1c_writer *w, const re1c_u8 *p, re1c_u32 n)
{
    re1c_u32 i;
    if (p == NULL && n != 0u) { w->error = RE1C_ERR_ARGUMENT; return; }
    for (i = 0u; i < n; ++i) re1c_w8(w, p[i]);
}

static int re1c_canon_bank(const re1c_view *view, re1c_u32 bank, re1c_writer *w)
{
    re1c_bank_info bi;
    re1c_u32 i;
    int rc;
    const re1c_section *emr;
    const re1c_section *edd;
    rc = re1c_get_bank_info(view, bank, &bi);
    if (rc == RE1C_ERR_NOT_PRESENT) {
        re1c_w16(w, 0u);
        re1c_w16(w, 0u);
        re1c_w32(w, 0u);
        re1c_w32(w, 0u);
        re1c_w32(w, 0u);
        return w->error;
    }
    if (rc != RE1C_OK) return rc;
    rc = re1c_get_bank_sections(view, bank, &emr, &edd); if (rc != RE1C_OK) return rc;
    (void)edd;
    re1c_w16(w, bi.object_count);
    re1c_w16(w, bi.frame_size);
    re1c_w32(w, bi.pose_count);
    re1c_w32(w, bi.clip_count);
    re1c_w32(w, bi.emd_type);
    for (i = 0u; i < (re1c_u32)bi.object_count; ++i) {
        re1c_pivot p;
        re1c_u16 cc;
        re1c_u32 j;
        rc = re1c_get_pivot(view, bank, i, &p); if (rc != RE1C_OK) return rc;
        re1c_wi32(w, p.x_centi); re1c_wi32(w, p.y_centi); re1c_wi32(w, p.z_centi);
        re1c_wbytes(w, emr->data + 8u + i * 6u, 6u);
        re1c_w16(w, (re1c_u16)p.parent);
        rc = re1c_get_child_count(view, bank, i, &cc); if (rc != RE1C_OK) return rc;
        re1c_w16(w, cc);
        for (j = 0u; j < (re1c_u32)cc; ++j) {
            re1c_u8 child;
            rc = re1c_get_child(view, bank, i, j, &child); if (rc != RE1C_OK) return rc;
            re1c_w8(w, child);
        }
    }
    for (i = 0u; i < bi.clip_count; ++i) {
        re1c_clip clip;
        re1c_u32 j;
        rc = re1c_get_clip(view, bank, i, &clip); if (rc != RE1C_OK) return rc;
        re1c_w16(w, clip.frame_count);
        for (j = 0u; j < (re1c_u32)clip.frame_count; ++j) {
            re1c_frame_ref fr;
            rc = re1c_get_frame_ref(view, bank, i, j, &fr); if (rc != RE1C_OK) return rc;
            re1c_w16(w, fr.pose_index); re1c_w16(w, fr.step_units);
        }
    }
    for (i = 0u; i < bi.pose_count; ++i) {
        re1c_pose_root root;
        re1c_u32 j;
        rc = re1c_get_pose_root(view, bank, i, &root); if (rc != RE1C_OK) return rc;
        re1c_wi32(w, root.x_centi); re1c_wi32(w, root.y_centi); re1c_wi32(w, root.z_centi);
        for (j = 0u; j < (re1c_u32)bi.object_count; ++j) {
            re1c_pose_angle a;
            rc = re1c_get_pose_angle(view, bank, i, j, &a); if (rc != RE1C_OK) return rc;
            re1c_w16(w, a.file_ax12); re1c_w16(w, a.file_ay12); re1c_w16(w, a.file_az12);
        }
        {
            const re1c_u8 *raw_pose;
            rc = re1c_get_pose_ptr(view, bank, i, &raw_pose, NULL); if (rc != RE1C_OK) return rc;
            re1c_wbytes(w, raw_pose, bi.frame_size);
        }
    }
    return w->error;
}

int re1c_canonical_build(const re1c_view *view, re1c_u8 *out_bytes, re1c_u32 capacity, re1c_u32 *out_size)
{
    re1c_writer w;
    re1c_u32 object_count;
    re1c_u32 oi;
    re1c_tim_info tim;
    int rc;
    if (view == NULL || out_bytes == NULL || out_size == NULL) return RE1C_ERR_ARGUMENT;
    w.data = out_bytes; w.cap = capacity; w.pos = 0u; w.error = 0;
    re1c_w32(&w, RE1C_CANON_MAGIC);
    re1c_w32(&w, RE1C_CANON_VERSION);
    rc = re1c_canon_bank(view, RE1C_BANK_A, &w); if (rc != RE1C_OK) return rc;
    rc = re1c_canon_bank(view, RE1C_BANK_B, &w); if (rc != RE1C_OK) return rc;
    rc = re1c_get_tmd_object_count(view, &object_count); if (rc != RE1C_OK) return rc;
    re1c_w32(&w, object_count);
    for (oi = 0u; oi < object_count; ++oi) {
        re1c_tmd_object obj;
        re1c_u32 i;
        rc = re1c_get_tmd_object(view, oi, &obj); if (rc != RE1C_OK) return rc;
        re1c_w32(&w, obj.vertex_count); re1c_w32(&w, obj.normal_count); re1c_w32(&w, obj.primitive_count);
        re1c_w32(&w, obj.scale_word);
        for (i = 0u; i < obj.vertex_count; ++i) {
            re1c_vertex_raw raw;
            re1c_vec3_centi v;
            rc = re1c_get_vertex_raw(view, oi, i, &raw); if (rc != RE1C_OK) return rc;
            rc = re1c_get_vertex_centi(view, oi, i, &v); if (rc != RE1C_OK) return rc;
            re1c_w16(&w, raw.file_x); re1c_w16(&w, raw.file_y); re1c_w16(&w, raw.file_z); re1c_w16(&w, raw.pad);
            re1c_wi32(&w, v.x); re1c_wi32(&w, v.y); re1c_wi32(&w, v.z);
        }
        for (i = 0u; i < obj.normal_count; ++i) {
            re1c_vertex_raw raw;
            rc = re1c_get_normal_raw(view, oi, i, &raw); if (rc != RE1C_OK) return rc;
            re1c_w16(&w, raw.file_x); re1c_w16(&w, raw.file_y); re1c_w16(&w, raw.file_z); re1c_w16(&w, raw.pad);
        }
        for (i = 0u; i < obj.primitive_count; ++i) {
            re1c_primitive p;
            rc = re1c_get_primitive(view, oi, i, &p);
            if (rc != RE1C_OK && rc != RE1C_ERR_UNSUPPORTED) return rc;
            re1c_w32(&w, p.code);
            re1c_wbytes(&w, p.raw_packet, p.raw_packet_size);
        }
    }
    rc = re1c_get_tim_info(view, &tim);
    if (rc == RE1C_OK) {
        re1c_w32(&w, tim.pixel_mode); re1c_w32(&w, tim.has_clut);
        re1c_w16(&w, tim.clut_x); re1c_w16(&w, tim.clut_y); re1c_w16(&w, tim.clut_w); re1c_w16(&w, tim.clut_h);
        re1c_w16(&w, tim.pixel_x); re1c_w16(&w, tim.pixel_y); re1c_w16(&w, tim.pixel_w_words); re1c_w16(&w, tim.pixel_h);
        re1c_w32(&w, tim.clut_bytes); re1c_wbytes(&w, tim.clut_data, tim.clut_bytes);
        re1c_w32(&w, tim.pixel_bytes); re1c_wbytes(&w, tim.pixel_data, tim.pixel_bytes);
    } else if (rc == RE1C_ERR_NOT_PRESENT) {
        re1c_w32(&w, 0u); re1c_w32(&w, 0u);
        re1c_w16(&w, 0u); re1c_w16(&w, 0u); re1c_w16(&w, 0u); re1c_w16(&w, 0u);
        re1c_w16(&w, 0u); re1c_w16(&w, 0u); re1c_w16(&w, 0u); re1c_w16(&w, 0u);
        re1c_w32(&w, 0u);
        re1c_w32(&w, 0u);
    } else return rc;
    if (w.error != 0) return w.error;
    *out_size = w.pos;
    return RE1C_OK;
}

re1c_u32 re1c_fnv1a32(const re1c_u8 *data, re1c_u32 size)
{
    re1c_u32 h;
    re1c_u32 i;
    h = 2166136261u;
    if (data == NULL) return 0u;
    for (i = 0u; i < size; ++i) {
        h ^= (re1c_u32)data[i];
        h *= 16777619u;
    }
    return h;
}

int re1c_compare_canonical(const re1c_u8 *a, re1c_u32 size_a,
                           const re1c_u8 *b, re1c_u32 size_b,
                           re1c_compare *out_compare)
{
    re1c_u32 i;
    if (a == NULL || b == NULL || out_compare == NULL) return RE1C_ERR_ARGUMENT;
    memset(out_compare, 0, sizeof(*out_compare));
    out_compare->size_a = size_a;
    out_compare->size_b = size_b;
    if (size_a != size_b) {
        out_compare->first_diff = (size_a < size_b) ? size_a : size_b;
        return RE1C_ERR_MISMATCH;
    }
    for (i = 0u; i < size_a; ++i) {
        if (a[i] != b[i]) {
            out_compare->first_diff = i;
            out_compare->byte_a = a[i];
            out_compare->byte_b = b[i];
            return RE1C_ERR_MISMATCH;
        }
    }
    out_compare->first_diff = size_a;
    return RE1C_OK;
}

int re1c_compare_views(const re1c_view *a, const re1c_view *b,
                       re1c_u8 *scratch_a, re1c_u8 *scratch_b,
                       re1c_u32 scratch_capacity, re1c_compare *out_compare)
{
    re1c_u32 sa;
    re1c_u32 sb;
    int rc;
    if (a == NULL || b == NULL || scratch_a == NULL || scratch_b == NULL || out_compare == NULL) return RE1C_ERR_ARGUMENT;
    rc = re1c_canonical_build(a, scratch_a, scratch_capacity, &sa); if (rc != RE1C_OK) return rc;
    rc = re1c_canonical_build(b, scratch_b, scratch_capacity, &sb); if (rc != RE1C_OK) return rc;
    return re1c_compare_canonical(scratch_a, sa, scratch_b, sb, out_compare);
}
