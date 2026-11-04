#include "re4c3d/re4_event.h"
#include "re4c3d/re4_base.h"
#include <string.h>

const char *re4_evp_type_name(re4_u32 type)
{
    switch (type) {
    case RE4_EVP_BEGIN_EVT: return "BeginEvt";
    case RE4_EVP_SET_PL: return "SetPl";
    case RE4_EVP_SET_EM: return "SetEm";
    case RE4_EVP_SET_OM: return "SetOm";
    case RE4_EVP_SET_PARTS: return "SetParts";
    case RE4_EVP_SET_LIST: return "SetList";
    case RE4_EVP_CAM: return "Cam";
    case RE4_EVP_CAM_POS: return "CamPos";
    case RE4_EVP_CAM_DUMMY: return "CamDummy";
    case RE4_EVP_POS: return "Pos";
    case RE4_EVP_POS_PL: return "PosPl";
    case RE4_EVP_MOT: return "Mot";
    case RE4_EVP_SHP: return "Shp";
    case RE4_EVP_ESP: return "Esp";
    case RE4_EVP_LIT: return "Lit";
    case RE4_EVP_STR: return "Str";
    case RE4_EVP_SE: return "Se";
    case RE4_EVP_MES: return "Mes";
    case RE4_EVP_FUNC: return "Func";
    case RE4_EVP_PARENT_ON: return "ParentOn";
    case RE4_EVP_PARENT_OFF: return "ParentOff";
    case RE4_EVP_END_PL: return "EndPl";
    case RE4_EVP_END_EM: return "EndEm";
    case RE4_EVP_END_OM: return "EndOm";
    case RE4_EVP_END_PARTS: return "EndParts";
    case RE4_EVP_END_LIST: return "EndList";
    case RE4_EVP_END_EVT: return "EndEvt";
    case RE4_EVP_END_PAC: return "EndPac";
    case RE4_EVP_SET_EFF: return "SetEff";
    case RE4_EVP_FADE: return "Fade";
    case RE4_EVP_FOG: return "Fog";
    case RE4_EVP_FOCUS: return "Focus";
    case RE4_EVP_SET_MDT: return "SetMdt";
    default: return "Unknown";
    }
}


const char *re4_evd_file_kind_name(re4_evd_file_kind kind)
{
    switch (kind) {
    case RE4_EVD_FILE_BIN: return "BIN";
    case RE4_EVD_FILE_TPL: return "TPL";
    case RE4_EVD_FILE_FCV: return "FCV";
    case RE4_EVD_FILE_SEQ: return "SEQ";
    case RE4_EVD_FILE_LIT: return "LIT";
    case RE4_EVD_FILE_EFF: return "EFF";
    case RE4_EVD_FILE_SMD: return "SMD";
    case RE4_EVD_FILE_MDT: return "MDT";
    case RE4_EVD_FILE_ESL: return "ESL";
    case RE4_EVD_FILE_PMD: return "PMD";
    default: return "unknown";
    }
}

re4_evd_file_kind re4_evd_file_kind_from_name(re4_strref name)
{
    if (re4_strref_ends_with_cstr_nocase(name, ".bin")) return RE4_EVD_FILE_BIN;
    if (re4_strref_ends_with_cstr_nocase(name, ".tpl")) return RE4_EVD_FILE_TPL;
    if (re4_strref_ends_with_cstr_nocase(name, ".fcv")) return RE4_EVD_FILE_FCV;
    if (re4_strref_ends_with_cstr_nocase(name, ".seq")) return RE4_EVD_FILE_SEQ;
    if (re4_strref_ends_with_cstr_nocase(name, ".lit")) return RE4_EVD_FILE_LIT;
    if (re4_strref_ends_with_cstr_nocase(name, ".eff")) return RE4_EVD_FILE_EFF;
    if (re4_strref_ends_with_cstr_nocase(name, ".smd")) return RE4_EVD_FILE_SMD;
    if (re4_strref_ends_with_cstr_nocase(name, ".mdt")) return RE4_EVD_FILE_MDT;
    if (re4_strref_ends_with_cstr_nocase(name, ".esl")) return RE4_EVD_FILE_ESL;
    if (re4_strref_ends_with_cstr_nocase(name, ".pmd")) return RE4_EVD_FILE_PMD;
    return RE4_EVD_FILE_UNKNOWN;
}

static re4_err re4_evd_walk_packets(re4_span src, re4_endian e, re4_u32 start, re4_u32 bytes, re4_u32 *count)
{
    re4_size pos;
    re4_size end;
    re4_u32 n;
    if (count == 0) return RE4_ERR_NULL;
    if (!re4_span_ok(src, start, bytes)) return RE4_ERR_RANGE;
    pos = start;
    end = (re4_size)start + (re4_size)bytes;
    n = 0;
    while (pos < end) {
        re4_i16 sz;
        if (end - pos < 16UL) return RE4_ERR_BAD_FORMAT;
        sz = re4_read_i16_e(src.data + pos + 12UL, e);
        if (sz < 16) return RE4_ERR_BAD_FORMAT;
        if ((re4_size)sz > end - pos) return RE4_ERR_RANGE;
        pos += (re4_size)sz;
        ++n;
    }
    *count = n;
    return RE4_OK;
}

re4_err re4_evd_read(re4_span src, re4_endian endian, re4_arena *arena, re4_evd *out)
{
    re4_u32 packet_count;
    re4_u32 i;
    re4_size pos;
    re4_err er;
    re4_evd_packet *packets;
    re4_evd_file *files;

    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0, 0x50UL)) return RE4_ERR_RANGE;

    out->name_file = re4_strref_fixed(src.data + 0x00UL, 32);
    out->room_no = re4_strref_fixed(src.data + 0x20UL, 8);
    out->event_no = re4_strref_fixed(src.data + 0x28UL, 8);
    out->serial_no = re4_read_i32_e(src.data + 0x30UL, endian);
    out->event_flag = re4_read_i32_e(src.data + 0x34UL, endian);
    out->offset_pac = re4_read_u32_e(src.data + 0x40UL, endian);
    out->size_pac = re4_read_u32_e(src.data + 0x44UL, endian);
    out->num_bin_tbl = re4_read_u32_e(src.data + 0x48UL, endian);
    out->offset_bin_tbl = re4_read_u32_e(src.data + 0x4CUL, endian);
    out->endian = endian;

    er = re4_evd_walk_packets(src, endian, out->offset_pac, out->size_pac, &packet_count);
    if (er != RE4_OK) return er;
    packets = (re4_evd_packet *)re4_arena_push(arena, (re4_size)packet_count * sizeof(re4_evd_packet), sizeof(re4_u32));
    if (packet_count != 0 && packets == 0) return RE4_ERR_NO_SPACE;

    if (!re4_span_ok(src, out->offset_bin_tbl, (re4_size)out->num_bin_tbl * 0x40UL)) return RE4_ERR_RANGE;
    files = (re4_evd_file *)re4_arena_push(arena, (re4_size)out->num_bin_tbl * sizeof(re4_evd_file), sizeof(re4_u32));
    if (out->num_bin_tbl != 0 && files == 0) return RE4_ERR_NO_SPACE;

    pos = out->offset_pac;
    for (i = 0; i < packet_count; ++i) {
        re4_i16 sz;
        packets[i].type = re4_read_u32_e(src.data + pos + 0UL, endian);
        packets[i].flags_common = re4_read_u32_e(src.data + pos + 4UL, endian);
        packets[i].cut_no = re4_read_i16_e(src.data + pos + 8UL, endian);
        packets[i].frame = re4_read_i16_e(src.data + pos + 10UL, endian);
        sz = re4_read_i16_e(src.data + pos + 12UL, endian);
        packets[i].size = sz;
        packets[i].pac_no = re4_read_u16_e(src.data + pos + 14UL, endian);
        packets[i].file_offset = (re4_u32)pos;
        packets[i].payload.data = src.data + pos + 16UL;
        packets[i].payload.size = (re4_size)sz - 16UL;
        pos += (re4_size)sz;
    }

    for (i = 0; i < out->num_bin_tbl; ++i) {
        re4_size off;
        off = (re4_size)out->offset_bin_tbl + ((re4_size)i * 0x40UL);
        files[i].name = re4_strref_fixed(src.data + off, 0x30UL);
        files[i].offset = re4_read_u32_e(src.data + off + 0x30UL, endian);
        files[i].size = re4_read_u32_e(src.data + off + 0x34UL, endian);
        files[i].flags = (re4_u32)re4_evd_file_kind_from_name(files[i].name);
        if (!re4_span_ok(src, files[i].offset, files[i].size)) return RE4_ERR_RANGE;
    }

    out->packet_count = packet_count;
    out->packets = packets;
    out->files = files;
    return RE4_OK;
}

static re4_strref re4_empty_ref(void)
{
    re4_strref s;
    s.ptr = 0;
    s.len = 0;
    return s;
}

static re4_bool re4_payload_ok(const re4_evd_packet *p, re4_size need)
{
    if (p == 0) return RE4_FALSE;
    if (p->payload.data == 0) return RE4_FALSE;
    return p->payload.size >= need ? RE4_TRUE : RE4_FALSE;
}

static void re4_q16_vec3_from_payload(re4_span pl, re4_size off, re4_endian endian, re4_q16 out[3])
{
    out[0] = re4_f32bits_to_q16(re4_read_u32_e(pl.data + off + 0UL, endian));
    out[1] = re4_f32bits_to_q16(re4_read_u32_e(pl.data + off + 4UL, endian));
    out[2] = re4_f32bits_to_q16(re4_read_u32_e(pl.data + off + 8UL, endian));
}

re4_err re4_evd_packet_info_read(const re4_evd_packet *p, re4_endian endian, re4_evd_packet_info *out)
{
    re4_span pl;
    if (p == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    out->name_mod = re4_empty_ref();
    out->name_parent = re4_empty_ref();
    out->name_bin = re4_empty_ref();
    out->name_tpl = re4_empty_ref();
    pl = p->payload;

    switch (p->type) {
    case RE4_EVP_BEGIN_EVT:
    case RE4_EVP_END_EVT:
    case RE4_EVP_END_PAC:
        return RE4_OK;

    case RE4_EVP_SET_PL:
    case RE4_EVP_SET_EM:
    case RE4_EVP_POS_PL:
    case RE4_EVP_PARENT_OFF:
    case RE4_EVP_END_PL:
    case RE4_EVP_END_EM:
    case RE4_EVP_END_OM:
    case RE4_EVP_END_PARTS:
    case RE4_EVP_END_LIST:
        if (!re4_payload_ok(p, 12UL)) return RE4_ERR_RANGE;
        out->name_mod = re4_strref_fixed(pl.data + 0UL, 12);
        return RE4_OK;

    case RE4_EVP_SET_LIST:
        if (!re4_payload_ok(p, 16UL)) return RE4_ERR_RANGE;
        out->name_mod = re4_strref_fixed(pl.data + 0UL, 12);
        out->ivalue0 = re4_read_i32_e(pl.data + 12UL, endian);
        return RE4_OK;

    case RE4_EVP_SET_OM:
        if (!re4_payload_ok(p, 108UL)) return RE4_ERR_RANGE;
        out->name_mod = re4_strref_fixed(pl.data + 0UL, 12);
        out->name_bin = re4_strref_fixed(pl.data + 12UL, 48);
        out->name_tpl = re4_strref_fixed(pl.data + 60UL, 48);
        return RE4_OK;

    case RE4_EVP_SET_PARTS:
        if (!re4_payload_ok(p, 120UL)) return RE4_ERR_RANGE;
        out->name_mod = re4_strref_fixed(pl.data + 0UL, 12);
        out->name_parent = re4_strref_fixed(pl.data + 12UL, 12);
        out->name_bin = re4_strref_fixed(pl.data + 24UL, 48);
        out->name_tpl = re4_strref_fixed(pl.data + 72UL, 48);
        return RE4_OK;

    case RE4_EVP_MOT:
    case RE4_EVP_SHP:
        if (!re4_payload_ok(p, 60UL)) return RE4_ERR_RANGE;
        out->name_mod = re4_strref_fixed(pl.data + 0UL, 12);
        out->name_bin = re4_strref_fixed(pl.data + 12UL, 48);
        return RE4_OK;

    case RE4_EVP_CAM:
    case RE4_EVP_LIT:
    case RE4_EVP_SET_EFF:
    case RE4_EVP_FOG:
    case RE4_EVP_FOCUS:
    case RE4_EVP_SET_MDT:
        if (!re4_payload_ok(p, 48UL)) return RE4_ERR_RANGE;
        out->name_bin = re4_strref_fixed(pl.data + 0UL, 48);
        return RE4_OK;

    case RE4_EVP_CAM_POS:
        if (!re4_payload_ok(p, 24UL)) return RE4_ERR_RANGE;
        re4_q16_vec3_from_payload(pl, 0UL, endian, out->pos);
        re4_q16_vec3_from_payload(pl, 12UL, endian, out->ang);
        out->has_pos = 1U;
        out->has_ang = 1U;
        return RE4_OK;

    case RE4_EVP_CAM_DUMMY:
        if (!re4_payload_ok(p, 4UL)) return RE4_ERR_RANGE;
        out->ivalue0 = re4_read_i32_e(pl.data + 0UL, endian);
        return RE4_OK;

    case RE4_EVP_POS:
        if (!re4_payload_ok(p, 52UL)) return RE4_ERR_RANGE;
        out->name_mod = re4_strref_fixed(pl.data + 0UL, 12);
        out->name_parent = re4_strref_fixed(pl.data + 12UL, 12);
        re4_q16_vec3_from_payload(pl, 24UL, endian, out->pos);
        re4_q16_vec3_from_payload(pl, 36UL, endian, out->ang);
        out->ivalue0 = re4_read_i32_e(pl.data + 48UL, endian);
        out->has_pos = 1U;
        out->has_ang = 1U;
        return RE4_OK;

    case RE4_EVP_ESP:
        if (!re4_payload_ok(p, 20UL)) return RE4_ERR_RANGE;
        out->name_mod = re4_strref_fixed(pl.data + 0UL, 12);
        out->ivalue0 = re4_read_i32_e(pl.data + 12UL, endian);
        out->ivalue1 = re4_read_i32_e(pl.data + 16UL, endian);
        return RE4_OK;

    case RE4_EVP_STR:
    case RE4_EVP_SE:
    case RE4_EVP_MES:
    case RE4_EVP_FUNC:
        if (!re4_payload_ok(p, 8UL)) return RE4_ERR_RANGE;
        out->ivalue0 = re4_read_i32_e(pl.data + 0UL, endian);
        out->ivalue1 = re4_read_i32_e(pl.data + 4UL, endian);
        return RE4_OK;

    case RE4_EVP_FADE:
        if (!re4_payload_ok(p, 12UL)) return RE4_ERR_RANGE;
        out->ivalue0 = re4_read_i32_e(pl.data + 0UL, endian);
        out->ivalue1 = re4_read_i32_e(pl.data + 4UL, endian);
        out->ivalue2 = re4_read_i32_e(pl.data + 8UL, endian);
        return RE4_OK;

    case RE4_EVP_PARENT_ON:
        if (!re4_payload_ok(p, 24UL)) return RE4_ERR_RANGE;
        out->name_mod = re4_strref_fixed(pl.data + 0UL, 12);
        out->name_parent = re4_strref_fixed(pl.data + 12UL, 12);
        return RE4_OK;

    default:
        return RE4_ERR_UNSUPPORTED;
    }
}

re4_err re4_evd_packet_ref_name(const re4_evd_packet *p, re4_strref *a, re4_strref *b)
{
    re4_evd_packet_info info;
    re4_err er;
    if (p == 0 || a == 0 || b == 0) return RE4_ERR_NULL;
    *a = re4_empty_ref();
    *b = re4_empty_ref();
    er = re4_evd_packet_info_read(p, RE4_ENDIAN_LITTLE, &info);
    if (er != RE4_OK) return er;
    if (info.name_mod.ptr != 0) *a = info.name_mod;
    else if (info.name_bin.ptr != 0) *a = info.name_bin;
    if (info.name_bin.ptr != 0 && info.name_mod.ptr != 0) *b = info.name_bin;
    else if (info.name_tpl.ptr != 0) *b = info.name_tpl;
    return RE4_OK;
}

re4_err re4_evd_packet_resource_ref_e(const re4_evd_packet *p, re4_endian endian, re4_evd_resource_ref *out)
{
    re4_evd_packet_info info;
    re4_err er;
    if (p == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    er = re4_evd_packet_info_read(p, endian, &info);
    if (er != RE4_OK) return er;
    out->model_name = info.name_mod;
    out->parent_name = info.name_parent;
    out->bin_name = info.name_bin;
    out->tpl_name = info.name_tpl;
    if (out->model_name.ptr != 0) out->flags |= RE4_EVD_REF_MODEL;
    if (out->parent_name.ptr != 0) out->flags |= RE4_EVD_REF_PARENT;
    if (out->bin_name.ptr != 0) out->flags |= RE4_EVD_REF_BIN;
    if (out->tpl_name.ptr != 0) out->flags |= RE4_EVD_REF_TPL;
    return RE4_OK;
}

re4_err re4_evd_packet_resource_ref(const re4_evd_packet *p, re4_evd_resource_ref *out)
{
    return re4_evd_packet_resource_ref_e(p, RE4_ENDIAN_LITTLE, out);
}

re4_u32 re4_evd_count_packets_of_type(const re4_evd *evd, re4_u32 type)
{
    re4_u32 i;
    re4_u32 n;
    if (evd == 0 || evd->packets == 0) return 0;
    n = 0;
    for (i = 0; i < evd->packet_count; ++i) {
        if (evd->packets[i].type == type) ++n;
    }
    return n;
}


re4_u32 re4_evd_count_files_of_kind(const re4_evd *evd, re4_evd_file_kind kind)
{
    re4_u32 i;
    re4_u32 n;
    if (evd == 0 || evd->files == 0) return 0;
    n = 0;
    for (i = 0; i < evd->num_bin_tbl; ++i) {
        if (re4_evd_file_kind_from_name(evd->files[i].name) == kind) ++n;
    }
    return n;
}

re4_err re4_evd_build_manifest(const re4_evd *evd, re4_evd_manifest *out)
{
    re4_u32 i;
    if (evd == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (evd->files != 0) {
        for (i = 0; i < evd->num_bin_tbl; ++i) {
            re4_evd_file_kind k;
            k = re4_evd_file_kind_from_name(evd->files[i].name);
            if ((re4_u32)k < RE4_EVD_FILE_COUNT) out->file_counts[k] += 1U;
        }
    }
    if (evd->packets != 0) {
        for (i = 0; i < evd->packet_count; ++i) {
            const re4_evd_packet *p;
            re4_evd_resource_ref rr;
            re4_err er;
            p = &evd->packets[i];
            if (p->type < RE4_EVD_PACKET_TYPE_LIMIT) out->packet_counts[p->type] += 1U;
            else out->unknown_packet_count += 1U;
            er = re4_evd_packet_resource_ref_e(p, evd->endian, &rr);
            if (er == RE4_OK) {
                if ((rr.flags & RE4_EVD_REF_MODEL) != 0U) out->model_ref_count += 1U;
                if ((rr.flags & RE4_EVD_REF_PARENT) != 0U) out->parent_ref_count += 1U;
                if ((rr.flags & RE4_EVD_REF_BIN) != 0U) out->bin_ref_count += 1U;
                if ((rr.flags & RE4_EVD_REF_TPL) != 0U) out->tpl_ref_count += 1U;
            }
        }
    }
    return RE4_OK;
}

re4_err re4_evd_file_span(re4_span src, const re4_evd *evd, re4_u32 file_index, re4_span *out)
{
    const re4_evd_file *f;
    if (evd == 0 || out == 0) return RE4_ERR_NULL;
    if (file_index >= evd->num_bin_tbl) return RE4_ERR_RANGE;
    f = &evd->files[file_index];
    return re4_span_sub(src, f->offset, f->size, out);
}

re4_err re4_evd_resource_index_build(const re4_evd *evd, re4_arena *arena, re4_evd_resource_index *out)
{
    re4_u32 i;
    re4_u32 n;
    re4_u32 skipped;
    re4_evd_resource_ref *refs;
    if (evd == 0 || arena == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (evd->packet_count != 0U && evd->packets == 0) return RE4_ERR_NULL;
    n = 0U;
    skipped = 0U;
    for (i = 0; i < evd->packet_count; ++i) {
        re4_evd_resource_ref tmp;
        re4_err er;
        er = re4_evd_packet_resource_ref_e(&evd->packets[i], evd->endian, &tmp);
        if (er == RE4_OK && tmp.flags != 0U) ++n;
        else if (er != RE4_OK && er != RE4_ERR_UNSUPPORTED) ++skipped;
    }
    refs = 0;
    if (n != 0U) {
        refs = (re4_evd_resource_ref *)re4_arena_push(arena, (re4_size)n * sizeof(re4_evd_resource_ref), sizeof(re4_u32));
        if (refs == 0) return RE4_ERR_NO_SPACE;
    }
    n = 0U;
    for (i = 0; i < evd->packet_count; ++i) {
        re4_evd_resource_ref tmp;
        re4_err er;
        er = re4_evd_packet_resource_ref_e(&evd->packets[i], evd->endian, &tmp);
        if (er == RE4_OK && tmp.flags != 0U) {
            refs[n] = tmp;
            ++n;
        }
    }
    out->count = n;
    out->skipped_packets = skipped;
    out->refs = refs;
    return RE4_OK;
}
