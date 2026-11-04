#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>

static re4_u8 arena_mem[65536];

typedef struct test_text_sink_s {
    char buf[4096];
    re4_size len;
} test_text_sink;

static re4_err test_emit(void *user, const char *text, re4_size len)
{
    test_text_sink *sink;
    re4_size i;
    if (user == 0 || text == 0) return RE4_ERR_NULL;
    sink = (test_text_sink *)user;
    if (sink->len + len + 1UL > sizeof(sink->buf)) return RE4_ERR_NO_SPACE;
    for (i = 0UL; i < len; ++i) sink->buf[sink->len + i] = text[i];
    sink->len += len;
    sink->buf[sink->len] = 0;
    return RE4_OK;
}

static void put_le16(re4_u8 *p, re4_u16 v)
{
    p[0] = (re4_u8)(v & 255U);
    p[1] = (re4_u8)((v >> 8) & 255U);
}

static void put_le32(re4_u8 *p, re4_u32 v)
{
    p[0] = (re4_u8)(v & 255U);
    p[1] = (re4_u8)((v >> 8) & 255U);
    p[2] = (re4_u8)((v >> 16) & 255U);
    p[3] = (re4_u8)((v >> 24) & 255U);
}

static void put_be16(re4_u8 *p, re4_u16 v)
{
    p[0] = (re4_u8)((v >> 8) & 255U);
    p[1] = (re4_u8)(v & 255U);
}

static void put_be32(re4_u8 *p, re4_u32 v)
{
    p[0] = (re4_u8)((v >> 24) & 255U);
    p[1] = (re4_u8)((v >> 16) & 255U);
    p[2] = (re4_u8)((v >> 8) & 255U);
    p[3] = (re4_u8)(v & 255U);
}

static int test_tpl_decode_i4(void)
{
    re4_u8 tpl[88];
    re4_u8 rgba[64];
    re4_arena arena;
    re4_span src;
    re4_span img_span;
    re4_tpl out;
    re4_err er;
    re4_size i;
    for (i = 0; i < sizeof(tpl); ++i) tpl[i] = 0;
    for (i = 0; i < sizeof(rgba); ++i) rgba[i] = 0;
    put_be32(tpl + 0, 0x0020AF30U);
    put_be32(tpl + 4, 1U);
    put_be32(tpl + 8, 12U);
    put_be32(tpl + 12, 20U);
    put_be32(tpl + 16, 0U);
    put_be16(tpl + 20, 4U);
    put_be16(tpl + 22, 4U);
    put_be32(tpl + 24, RE4_TPL_I4);
    put_be32(tpl + 28, 56U);
    tpl[56] = 0xF0U;
    src.data = tpl;
    src.size = sizeof(tpl);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_tpl_read_gc(src, &arena, &out);
    if (er != RE4_OK) return 101;
    er = re4_tpl_image_span(src, &out.images[0], &img_span);
    if (er != RE4_OK) return 102;
    if (img_span.size != 32UL) return 103;
    {
        re4_u32 bw;
        re4_u32 bh;
        re4_u32 bb;
        er = re4_tpl_format_block_info(out.images[0].format, &bw, &bh, &bb);
        if (er != RE4_OK) return 106;
        if (bw != 8U || bh != 8U || bb != 32U) return 107;
        if (re4_tpl_format_is_indexed(out.images[0].format)) return 108;
    }
    er = re4_tpl_decode_image_rgba32(src, &out, 0U, rgba, sizeof(rgba), 16U);
    if (er != RE4_OK) return 104;
    if (rgba[0] != 255U || rgba[4] != 0U) return 105;
    return 0;
}


static int test_uhd_tpl_metadata(void)
{
    re4_u8 tpl[0x80];
    re4_arena arena;
    re4_uhd_tpl out;
    re4_span src;
    re4_err er;
    re4_size i;
    for (i = 0; i < sizeof(tpl); ++i) tpl[i] = 0;
    put_le32(tpl + 0, 0x78563412U);
    put_le32(tpl + 4, 1U);
    put_le32(tpl + 8, 0x10U);
    put_le32(tpl + 0x10, 0x20U);
    put_le32(tpl + 0x14, 0x50U);
    put_le16(tpl + 0x20, 16U);
    put_le16(tpl + 0x22, 32U);
    put_le32(tpl + 0x24, RE4_TPL_RGBA8);
    put_le32(tpl + 0x28, 0x60U);
    put_le32(tpl + 0x2C, 1U);
    put_le32(tpl + 0x30, 2U);
    put_le32(tpl + 0x34, 3U);
    put_le32(tpl + 0x38, 4U);
    put_le32(tpl + 0x3C, 0x3F800000U);
    tpl[0x40] = 1U;
    tpl[0x42] = 8U;
    put_le16(tpl + 0x50, 16U);
    tpl[0x52] = 1U;
    put_le32(tpl + 0x54, RE4_TPL_PAL_RGB5A3);
    put_le32(tpl + 0x60, 0x11223344U);
    put_le32(tpl + 0x64, 77U);
    src.data = tpl;
    src.size = sizeof(tpl);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_uhd_tpl_read(src, RE4_ENDIAN_LITTLE, RE4_FALSE, &arena, &out);
    if (er != RE4_OK) return 111;
    if (out.image_count != 1U || out.images == 0) return 112;
    if (out.images[0].height != 16U || out.images[0].width != 32U) return 113;
    if (out.images[0].pixel_format != RE4_TPL_RGBA8) return 114;
    if (out.images[0].pack_id != 0x11223344U || out.images[0].texture_id != 77U) return 115;
    if (out.images[0].has_palette != 1U || out.images[0].colors_count != 16U) return 116;
    return 0;
}

static int test_pmd_survey_and_smx(void)
{
    re4_u8 pmd[192];
    re4_u8 smx[0x88];
    re4_arena arena;
    re4_pmd_model model;
    re4_smx_table table;
    re4_span src;
    re4_err er;
    re4_size i;
    re4_size pos;
    for (i = 0; i < sizeof(pmd); ++i) pmd[i] = 0;
    pos = 58UL;
    put_le32(pmd + pos, 0U); pos += 4UL;
    put_le32(pmd + pos, 0U); pos += 4UL;
    put_le32(pmd + pos, 0U); pos += 4UL;
    put_le32(pmd + pos, 0U); pos += 4UL;
    put_le32(pmd + pos, 0U); pos += 4UL;
    put_le32(pmd + pos, 1U); pos += 4UL;
    for (i = 0; i < 17UL; ++i) { put_le32(pmd + pos, 0U); pos += 4UL; }
    put_le32(pmd + pos, 1U); pos += 4UL;
    put_le32(pmd + pos, 4U); pos += 4UL;
    memcpy(pmd + pos, "tex0", 4); pos += 4UL;
    src.data = pmd;
    src.size = pos;
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_pmd_read_survey(src, &arena, &model);
    if (er != RE4_OK) return 121;
    if (model.material_count != 1U || model.materials == 0) return 122;
    if (!re4_strref_eq_cstr(model.materials[0].texture_name, "tex0")) return 123;

    for (i = 0; i < sizeof(smx); ++i) smx[i] = 0;
    smx[0] = 5U;
    smx[1] = 1U;
    smx[2] = 2U;
    smx[3] = 3U;
    put_le32(smx + 4, 0xABCDEF01U);
    smx[12] = 10U;
    smx[13] = 20U;
    smx[14] = 30U;
    smx[15] = 40U;
    put_le32(smx + 20, 0x3F800000U);
    put_le32(smx + 24, 0x40000000U);
    src.data = smx;
    src.size = sizeof(smx);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_smx_table_read(src, RE4_ENDIAN_LITTLE, &arena, &table);
    if (er != RE4_OK) return 131;
    if (table.record_count != 1U || table.records == 0) return 132;
    if (table.records[0].use_smx_id != 5U || table.records[0].mode != 1U) return 133;
    if (table.records[0].color_rgb[2] != 30U || table.records[0].texture_movement[1] != 131072) return 134;
    return 0;
}

static int test_dat_offset_table(void)
{
    re4_u8 dat[20];
    re4_arena arena;
    re4_archive out;
    re4_span src;
    re4_err er;
    re4_size i;
    for (i = 0; i < sizeof(dat); ++i) dat[i] = 0;
    put_le32(dat + 0, 2U);
    put_le32(dat + 4, 12U);
    put_le32(dat + 8, 16U);
    dat[12] = 'A'; dat[13] = 'B'; dat[14] = 'C'; dat[15] = 'D';
    dat[16] = 'E'; dat[17] = 'F'; dat[18] = 'G'; dat[19] = 'H';
    src.data = dat;
    src.size = sizeof(dat);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_dat_read_guess(src, RE4_ENDIAN_LITTLE, &arena, &out);
    if (er != RE4_OK) return 201;
    if (out.entry_count != 2U) return 202;
    if (out.entries[0].offset != 12U || out.entries[0].packed_size != 4U) return 203;
    if (out.flags != 2U) return 204;
    return 0;
}

static int test_gca_metadata(void)
{
    re4_u8 gca[96];
    re4_arena arena;
    re4_archive out;
    re4_span src;
    re4_span file;
    re4_err er;
    re4_size i;
    for (i = 0; i < sizeof(gca); ++i) gca[i] = 0;
    gca[0] = 'G'; gca[1] = 'C'; gca[2] = 'A'; gca[3] = 'X';
    put_le32(gca + 8, 19U);
    gca[16] = 'X'; gca[17] = 'Y'; gca[18] = 'Z';
    gca[19] = 'G'; gca[20] = 'C'; gca[21] = 'A'; gca[22] = '3';
    put_le32(gca + 19 + 24, 1U);
    put_le32(gca + 19 + 32 + 4, 33U);
    put_le32(gca + 19 + 32 + 16, 3U);
    put_le32(gca + 19 + 32 + 24, 3U);
    put_le16(gca + 19 + 64, 5U);
    gca[19 + 66] = 'a'; gca[19 + 67] = '.';
    gca[19 + 68] = 'b'; gca[19 + 69] = 'i'; gca[19 + 70] = 'n';
    src.data = gca;
    src.size = 90UL;
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_gca_read(src, &arena, &out);
    if (er != RE4_OK) return 301;
    if (out.entry_count != 1U) return 302;
    if (out.entries[0].name.len != 5UL) return 303;
    er = re4_archive_entry_span(src, &out.entries[0], &file);
    if (er != RE4_OK) return 304;
    if (file.size != 3UL || file.data[0] != 'X') return 305;
    return 0;
}

static int test_lfs_uncompressed_extract(void)
{
    re4_u8 lfs[32];
    re4_u8 dst[8];
    re4_arena arena;
    re4_lfs out;
    re4_span src;
    re4_err er;
    re4_size written;
    re4_size i;
    for (i = 0; i < sizeof(lfs); ++i) lfs[i] = 0;
    for (i = 0; i < sizeof(dst); ++i) dst[i] = 0;
    put_le32(lfs + 0, 0x584C4452U);
    put_le32(lfs + 4, 0xFEEEBAAAU);
    put_le32(lfs + 8, 4U);
    put_le32(lfs + 12, 4U);
    put_le32(lfs + 16, 1U);
    put_le16(lfs + 20, 4U);
    put_le16(lfs + 22, 4U);
    put_le32(lfs + 24, 8U);
    lfs[28] = 'A'; lfs[29] = 'B'; lfs[30] = 'C'; lfs[31] = 'D';
    src.data = lfs;
    src.size = sizeof(lfs);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_lfs_read(src, &arena, &out);
    if (er != RE4_OK) return 401;
    er = re4_lfs_extract(src, &out, dst, sizeof(dst), &written, 0, 0);
    if (er != RE4_OK) return 402;
    if (written != 4UL || dst[0] != 'A' || dst[3] != 'D') return 403;
    return 0;
}


static int test_lfs_write_store_roundtrip(void)
{
    static re4_u8 raw[70000];
    static re4_u8 lfs[70256];
    static re4_u8 dst[70000];
    re4_arena arena;
    re4_lfs out;
    re4_span raw_span;
    re4_span lfs_span;
    re4_err er;
    re4_size need;
    re4_size written;
    re4_u32 chunks;
    re4_size i;
    for (i = 0; i < sizeof(raw); ++i) raw[i] = (re4_u8)(i * 13UL + 7UL);
    for (i = 0; i < sizeof(lfs); ++i) lfs[i] = 0;
    for (i = 0; i < sizeof(dst); ++i) dst[i] = 0;
    er = re4_lfs_store_size(sizeof(raw), &need, &chunks);
    if (er != RE4_OK) return 421;
    if (chunks != 2U) return 422;
    if (need > sizeof(lfs)) return 423;
    raw_span.data = raw;
    raw_span.size = sizeof(raw);
    er = re4_lfs_write_store(raw_span, RE4_ENDIAN_LITTLE, lfs, sizeof(lfs), &written);
    if (er != RE4_OK) return 424;
    if (written != need) return 425;
    lfs_span.data = lfs;
    lfs_span.size = written;
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_lfs_read(lfs_span, &arena, &out);
    if (er != RE4_OK) return 426;
    if (out.chunk_count != 2U || out.chunks[0].compressed != 0U) return 427;
    er = re4_lfs_extract(lfs_span, &out, dst, sizeof(dst), &written, 0, 0);
    if (er != RE4_OK) return 428;
    if (written != sizeof(raw)) return 429;
    if (memcmp(raw, dst, sizeof(raw)) != 0) return 430;
    return 0;
}

static int test_evd_manifest_file_kinds(void)
{
    re4_u8 evd[0xE8];
    re4_arena arena;
    re4_evd out;
    re4_evd_manifest manifest;
    re4_span src;
    re4_err er;
    re4_size i;
    for (i = 0; i < sizeof(evd); ++i) evd[i] = 0;
    memcpy(evd + 0, "ev001.evd", 9);
    put_le32(evd + 0x40, 0x50U);
    put_le32(evd + 0x44, 16U);
    put_le32(evd + 0x48, 2U);
    put_le32(evd + 0x4C, 0x60U);
    put_le32(evd + 0x50, RE4_EVP_BEGIN_EVT);
    put_le16(evd + 0x50 + 12, 16U);
    memcpy(evd + 0x60, "model.bin", 9);
    put_le32(evd + 0x60 + 0x30, 0xE0U);
    put_le32(evd + 0x60 + 0x34, 4U);
    memcpy(evd + 0xA0, "camera.fcv", 10);
    put_le32(evd + 0xA0 + 0x30, 0xE4U);
    put_le32(evd + 0xA0 + 0x34, 4U);
    evd[0xE0] = 'B'; evd[0xE4] = 'F';
    src.data = evd;
    src.size = sizeof(evd);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_evd_read(src, RE4_ENDIAN_LITTLE, &arena, &out);
    if (er != RE4_OK) return 531;
    er = re4_evd_build_manifest(&out, &manifest);
    if (er != RE4_OK) return 532;
    if (manifest.file_counts[RE4_EVD_FILE_BIN] != 1U) return 533;
    if (manifest.file_counts[RE4_EVD_FILE_FCV] != 1U) return 534;
    if (manifest.packet_counts[RE4_EVP_BEGIN_EVT] != 1U) return 535;
    if (re4_evd_count_files_of_kind(&out, RE4_EVD_FILE_BIN) != 1U) return 536;
    if (out.files[0].flags != (re4_u32)RE4_EVD_FILE_BIN) return 537;
    return 0;
}

static int test_evd_resource_refs(void)
{
    re4_u8 evd[0x50 + 144];
    re4_arena arena;
    re4_evd out;
    re4_evd_resource_ref ref;
    re4_span src;
    re4_err er;
    re4_size i;
    re4_u8 *p;
    for (i = 0; i < sizeof(evd); ++i) evd[i] = 0;
    put_le32(evd + 0x40, 0x50U);
    put_le32(evd + 0x44, 144U);
    put_le32(evd + 0x48, 0U);
    put_le32(evd + 0x4C, 0x50U + 144U);
    p = evd + 0x50;
    put_le32(p + 0, RE4_EVP_SET_PARTS);
    put_le16(p + 12, 144U);
    p += 16;
    memcpy(p + 0, "mod", 3);
    memcpy(p + 12, "par", 3);
    memcpy(p + 24, "m.bin", 5);
    memcpy(p + 72, "m.tpl", 5);
    src.data = evd;
    src.size = sizeof(evd);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_evd_read(src, RE4_ENDIAN_LITTLE, &arena, &out);
    if (er != RE4_OK) return 501;
    if (re4_evd_count_packets_of_type(&out, RE4_EVP_SET_PARTS) != 1U) return 502;
    er = re4_evd_packet_resource_ref(&out.packets[0], &ref);
    if (er != RE4_OK) return 503;
    if (!re4_strref_eq_cstr(ref.model_name, "mod")) return 504;
    if (!re4_strref_eq_cstr(ref.parent_name, "par")) return 505;
    if (!re4_strref_eq_cstr(ref.bin_name, "m.bin")) return 506;
    if (!re4_strref_eq_cstr(ref.tpl_name, "m.tpl")) return 507;
    return 0;
}


static int test_bin_and_smd_readers(void)
{
    re4_u8 bin[0xC4];
    re4_u8 smd[96];
    re4_arena arena;
    re4_bin_model bm;
    re4_smd_room room;
    re4_span src;
    re4_err er;
    re4_size i;

    for (i = 0; i < sizeof(bin); ++i) bin[i] = 0;
    put_le32(bin + 0x00, 0x50U);
    put_le32(bin + 0x14, 0x98U);
    bin[0x18] = 1U;
    bin[0x19] = 2U;
    put_le16(bin + 0x1A, 1U);
    put_le32(bin + 0x1C, 0x70U);
    put_le32(bin + 0x20, RE4_BIN_FLAG_ALWAYS_ACTIVATED);
    put_le32(bin + 0x24, 3U);
    put_le32(bin + 0x30, 0xA0U);
    put_le16(bin + 0x38, 3U);
    bin[0x70 + 12] = 4U;
    put_le32(bin + 0x70 + 24, 8U);
    put_le32(bin + 0x70 + 32, 1U);
    put_le16(bin + 0x70 + 36, 5U);
    put_le16(bin + 0x70 + 38, 3U);
    bin[0x98] = 1U;
    bin[0x99] = 2U;
    bin[0x9A] = 3U;
    bin[0x9B] = 3U;
    bin[0x9C] = 64U;
    bin[0x9D] = 64U;
    bin[0x9E] = 127U;
    put_le32(bin + 0xA0, 0x3F800000U);
    put_le32(bin + 0xA4, 0x40000000U);
    put_le32(bin + 0xA8, 0xBF800000U);
    src.data = bin;
    src.size = sizeof(bin);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_bin_model_read(src, RE4_ENDIAN_LITTLE, &arena, &bm);
    if (er != RE4_OK) return 701;
    if (bm.material_count != 1U || bm.weightmap_count != 1U) return 702;
    if (bm.materials[0].diffuse_map != 4U || bm.triangle_count_hint != 1U) return 703;
    if (bm.weightmaps[0].bone_id[2] != 3U || bm.bone_count != 2U) return 704;
    {
        re4_q16 xyz[3];
        er = re4_bin_position_q16(src, &bm, 0U, xyz);
        if (er != RE4_OK) return 705;
        if (xyz[0] != 65536 || xyz[1] != 131072 || xyz[2] != (re4_q16)-65536) return 706;
    }

    for (i = 0; i < sizeof(smd); ++i) smd[i] = 0;
    put_le16(smd + 0, 0x1234U);
    put_le16(smd + 2, 1U);
    put_le32(smd + 4, 96U);
    put_le32(smd + 8, 96U);
    put_le32(smd + 12, 96U);
    put_le32(smd + 16, 1U);
    put_le32(smd + 20, 0xABCDEF01U);
    put_le32(smd + 24, 0x3F800000U);
    put_le32(smd + 24 + 24, 0x3F800000U);
    smd[24 + 36] = 2U;
    smd[24 + 37] = 3U;
    smd[24 + 38] = 0xFFU;
    smd[24 + 39] = 4U;
    put_le32(smd + 24 + 68, 0x55AAU);
    src.data = smd;
    src.size = sizeof(smd);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_smd_room_read(src, RE4_ENDIAN_LITTLE, &arena, &room);
    if (er != RE4_OK) return 801;
    if (room.line_count != 1U || room.extra_count != 1U) return 802;
    if (room.extra_parameters[0] != 0xABCDEF01U) return 803;
    if (room.lines[0].position[0] != 65536 || room.lines[0].scale[0] != 65536) return 804;
    if (room.lines[0].bin_file_id != 2U || room.lines[0].tpl_file_id != 3U) return 805;
    if (room.lines[0].object_status != 0x55AAU) return 806;
    return 0;
}


static int test_fcv_axis_sizes_q16(void)
{
    re4_u8 fcv[64];
    re4_arena arena;
    re4_fcv out;
    re4_fcv_axis_view x;
    re4_fcv_axis_view y;
    re4_fcv_axis_view z;
    re4_span src;
    re4_q16 vx;
    re4_q16 vy;
    re4_q16 vz;
    re4_err er;
    re4_size i;
    for (i = 0; i < sizeof(fcv); ++i) fcv[i] = 0;
    put_le16(fcv + 0, 30U);
    fcv[2] = 1U;
    fcv[3] = 0x04U; /* translation */
    fcv[4] = 0x10U; /* float value, signed 16-bit tangents */
    fcv[5] = 2U;    /* joint ID */
    put_le32(fcv + 8, 64U);  /* aligned file size */
    put_le32(fcv + 12, 16U); /* axis block start */
    put_le16(fcv + 16, 1U); put_le16(fcv + 18, 0U);
    put_le32(fcv + 20, 0x3F800000U); /* x value=1.0 */
    put_le16(fcv + 28, 1U); put_le16(fcv + 30, 0U);
    put_le32(fcv + 32, 0x40000000U); /* y=2.0 float */
    put_le16(fcv + 40, 1U); put_le16(fcv + 42, 0U);
    put_le32(fcv + 44, 0xC0000000U); /* z=-2.0 float */
    src.data = fcv;
    src.size = sizeof(fcv);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_fcv_read(src, RE4_ENDIAN_LITTLE, &arena, &out);
    if (er != RE4_OK) return 901;
    if (out.blocks[0].axis_value_size[0] != 4U || out.blocks[0].axis_value_size[1] != 2U || out.blocks[0].axis_value_size[2] != 2U) return 902;
    er = re4_fcv_block_axis_views(src, &out.blocks[0], &x, &y, &z);
    if (er != RE4_OK) return 903;
    if (x.value_size != 4U || y.value_size != 4U || z.value_size != 4U) return 904;
    er = re4_fcv_axis_value_q16(&x, 0U, RE4_ENDIAN_LITTLE, &vx);
    if (er != RE4_OK) return 905;
    er = re4_fcv_axis_value_q16(&y, 0U, RE4_ENDIAN_LITTLE, &vy);
    if (er != RE4_OK) return 906;
    er = re4_fcv_axis_value_q16(&z, 0U, RE4_ENDIAN_LITTLE, &vz);
    if (er != RE4_OK) return 907;
    if (vx != 65536) return 908;
    if (vy != 131072) return 909;
    if (vz != (re4_q16)-131072) return 910;
    {
        re4_q16 sample[3];
        re4_u16 t;
        er = re4_fcv_axis_time_at(&x, 0U, &t);
        if (er != RE4_OK) return 911;
        if (t != 0U) return 912;
        er = re4_fcv_block_sample_nearest_q16(src, &out.blocks[0], 0U, RE4_ENDIAN_LITTLE, sample);
        if (er != RE4_OK) return 913;
        if (sample[0] != vx || sample[1] != vy || sample[2] != vz) return 914;
    }
    return 0;
}

static int test_esl(void)
{
    re4_u8 esl[32];
    re4_arena arena;
    re4_esl out;
    re4_span src;
    re4_err er;
    re4_size i;
    for (i = 0; i < sizeof(esl); ++i) esl[i] = 0;
    esl[0] = 3U;
    esl[1] = 7U;
    esl[2] = 9U;
    put_le16(esl + 8, 120U);
    put_le16(esl + 12, 100U);
    put_le16(esl + 24, 2U);
    src.data = esl;
    src.size = sizeof(esl);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_esl_read(src, &arena, &out);
    if (er != RE4_OK) return 601;
    if (out.entry_count != 1U) return 602;
    if (out.entries[0].id != 7U || out.entries[0].hp != 120 || out.entries[0].pos[0] != 100U) return 603;
    return 0;
}

static int test_tplcue_material_smd_text(void)
{
    re4_u8 cue_bytes[16];
    re4_u8 mat_bytes[24];
    re4_arena arena;
    re4_tplcue cue;
    re4_span src;
    re4_err er;
    re4_u32 idx;
    re4_bin_material_core mat;
    re4_bin_material_core ps2;
    re4_u32 bone_id;
    re4_smd_bone_rule rule;
    test_text_sink sink;
    re4_strref text;
    re4_size i;

    for (i = 0UL; i < sizeof(cue_bytes); ++i) cue_bytes[i] = 0;
    put_le32(cue_bytes + 0, 77U);
    put_le32(cue_bytes + 4, 0x11223344U);
    put_le32(cue_bytes + 8, 0x55667788U);
    put_le32(cue_bytes + 12, 88U);
    src.data = cue_bytes;
    src.size = sizeof(cue_bytes);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_tplcue_read_le(src, &arena, &cue);
    if (er != RE4_OK) return 1001;
    if (cue.record_count != 2U) return 1002;
    if (re4_tplcue_record_pack_id(&cue.records[0]) != 0x11223344U) return 1003;
    if (re4_tplcue_record_texture_id(&cue.records[1]) != 88U) return 1004;
    er = re4_tplcue_find(&cue, 0x55667788U, 88U, &idx);
    if (er != RE4_OK || idx != 1U) return 1005;

    sink.len = 0UL;
    sink.buf[0] = 0;
    er = re4_tplcue_emit_idxuhdtpl(&cue, 0x0EU, 64U, 32U, test_emit, &sink);
    if (er != RE4_OK) return 1006;
    if (strstr(sink.buf, "TPL_001") == 0) return 1007;
    if (strstr(sink.buf, "PackID:55667788") == 0) return 1008;
    if (strstr(sink.buf, "PixelFormatType:0E") == 0) return 1009;

    for (i = 0UL; i < sizeof(mat_bytes); ++i) mat_bytes[i] = 0;
    mat_bytes[11] = 0x87U;
    mat_bytes[12] = 2U;
    mat_bytes[13] = 3U;
    mat_bytes[14] = 4U;
    mat_bytes[15] = 1U;
    mat_bytes[16] = 10U;
    mat_bytes[17] = 20U;
    mat_bytes[18] = 30U;
    mat_bytes[21] = 0x44U;
    mat_bytes[23] = 9U;
    src.data = mat_bytes;
    src.size = sizeof(mat_bytes);
    er = re4_bin_material_core_read_uhd24(src, &mat);
    if (er != RE4_OK) return 1010;
    if (mat.diffuse_map != 2U || mat.bump_map != 3U || mat.opacity_map != 4U) return 1011;
    if (!re4_bin_material_core_has(&mat, RE4_BIN_MATERIAL_F_BUMP_MAP)) return 1012;
    if (!re4_bin_material_core_has(&mat, RE4_BIN_MATERIAL_F_OPACITY_MAP)) return 1013;
    if (!re4_bin_material_core_has(&mat, RE4_BIN_MATERIAL_F_NORMALS_EFFECT)) return 1014;
    if (strcmp(re4_bin_material_flag_label(RE4_BIN_MATERIAL_F_CUSTOM_SPECULAR), "custom_specular_map") != 0) return 1015;

    sink.len = 0UL;
    sink.buf[0] = 0;
    er = re4_bin_material_core_emit_idxmaterial(&mat, 5U, test_emit, &sink);
    if (er != RE4_OK) return 1016;
    if (strstr(sink.buf, "UseMaterial:UHD_MATERIAL_005") == 0) return 1017;
    if (strstr(sink.buf, "material_flag:87") == 0) return 1018;

    mat_bytes[0] = 0x87U;
    mat_bytes[1] = 2U;
    mat_bytes[11] = 9U;
    src.data = mat_bytes;
    src.size = 12UL;
    er = re4_bin_material_core_read_ps2_12(src, &ps2);
    if (er != RE4_OK) return 1019;
    if (ps2.material_flag != 0x87U || ps2.diffuse_map != 2U || ps2.custom_specular_map != 9U) return 1020;

    text.ptr = "file_b0ne_002";
    text.len = 13UL;
    er = re4_smd_bone_id_from_name(text, &bone_id);
    if (er != RE4_OK || bone_id != 2U) return 1021;
    text.ptr = "born__01|1|BONE_001|0";
    text.len = 21UL;
    er = re4_smd_replacebone_line_parse(text, &rule);
    if (er != RE4_OK) return 1022;
    if (rule.new_id != 1 || rule.parent_id != 0) return 1023;
    if (!re4_strref_eq_cstr(rule.old_name, "born__01")) return 1024;
    if (!re4_strref_eq_cstr(rule.new_name, "BONE_001")) return 1025;
    return 0;
}


static int test_idxuhdtpl_idxmaterial_mtl_scenario_text(void)
{
    static const char tpltxt[] =
        "# comment\n"
        "TPL_000\n"
        "PackID:41000001\n"
        "TextureID:0015\n"
        "PixelFormatType:0E\n"
        "width:128\n"
        "height:64\n"
        "wrap_s:1\n"
        "wrap_t:1\n"
        "TPL_001\n"
        "PackID:07000000\n"
        "TextureID:0004\n"
        "PixelFormatType:0E\n"
        "width:16\n"
        "height:16\n";
    static const char mattxt[] =
        "UseMaterial:UHD_MATERIAL_007\n"
        "material_flag:87\n"
        "diffuse_map:0\n"
        "bump_map:1\n"
        "opacity_map:2\n"
        "generic_specular_map:3\n"
        "intensity_specular_r:10\n"
        "intensity_specular_g:20\n"
        "intensity_specular_b:30\n"
        "specular_scale:32\n"
        "custom_specular_map:4\n";
    static const char scenariotxt[] =
        "Magic:0040\n"
        "SmdFileName:r204_004.SMD\n"
        "TplFileName:r204_004.TPL\n"
        "BinFolder:r204_004_BIN\n"
        "UseIdxUhdTpl:true\n"
        "UseIdxMaterial:false\n"
        "EnableVertexColor:false\n"
        "EnableDinamicVertexColor:true\n";
    re4_arena arena;
    re4_span src;
    re4_idxuhdtpl tplidx;
    re4_idxmaterial midx;
    re4_mtl_texture_ref tref;
    re4_scenario_group_ref group;
    re4_scenario_idx_options opt;
    re4_err er;
    re4_u32 idx;
    re4_u32 ux;
    re4_u32 uy;
    test_text_sink sink;
    re4_strref line;
    re4_u8 packed;

    src.data = (const re4_u8 *)tpltxt;
    src.size = (re4_size)(sizeof(tpltxt) - 1UL);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_idxuhdtpl_read_text(src, &arena, &tplidx);
    if (er != RE4_OK) return 1101;
    if (tplidx.entry_count != 2U) return 1102;
    if (tplidx.entries[0].pack_id != 0x41000001U || tplidx.entries[0].texture_id != 15U) return 1103;
    er = re4_idxuhdtpl_find(&tplidx, 0x07000000U, 4U, &idx);
    if (er != RE4_OK || idx != 1U) return 1104;
    sink.len = 0UL;
    sink.buf[0] = 0;
    er = re4_idxuhdtpl_emit_text(&tplidx, test_emit, &sink);
    if (er != RE4_OK) return 1105;
    if (strstr(sink.buf, "TPL_001") == 0) return 1106;

    src.data = (const re4_u8 *)mattxt;
    src.size = (re4_size)(sizeof(mattxt) - 1UL);
    re4_arena_reset(&arena);
    er = re4_idxmaterial_read_text(src, &arena, &midx);
    if (er != RE4_OK) return 1110;
    if (midx.entry_count != 1U) return 1111;
    if (!re4_strref_eq_cstr(midx.entries[0].name, "UHD_MATERIAL_007")) return 1112;
    if (midx.entries[0].core.material_flag != 0x87U || midx.entries[0].core.custom_specular_map != 4U) return 1113;
    sink.len = 0UL;
    sink.buf[0] = 0;
    er = re4_idxmaterial_emit_text(&midx, test_emit, &sink);
    if (er != RE4_OK) return 1114;
    if (strstr(sink.buf, "custom_specular_map:4") == 0) return 1115;

    line.ptr = "map_Kd qualquer/41000001/0015.dds";
    line.len = 33UL;
    er = re4_mtl_texture_ref_parse(line, &tref);
    if (er != RE4_OK) return 1120;
    if (tref.role != RE4_MTL_TEXROLE_DIFFUSE || tref.pack_id != 0x41000001U || tref.texture_id != 15U) return 1121;
    if (re4_mtl_pixel_format_type_for_ref(&tref) != 0x0EU) return 1122;
    line.ptr = "map_Ns -s 4.000000 3.000000 1 07000000/0000.dds";
    line.len = 47UL;
    er = re4_mtl_texture_ref_parse(line, &tref);
    if (er != RE4_OK) return 1123;
    if (!tref.has_scale || tref.scale_x_units != 4U || tref.scale_y_units != 3U) return 1124;
    packed = re4_mtl_specular_scale_pack(tref.scale_x_units, tref.scale_y_units);
    if (packed != 0x32U) return 1125;
    re4_mtl_specular_scale_unpack(packed, &ux, &uy);
    if (ux != 4U || uy != 3U) return 1126;

    line.ptr = "UHDSCENARIO#SMD_000#SMX_001#TYPE_18#BIN_002#";
    line.len = 44UL;
    er = re4_scenario_group_name_parse(line, &group);
    if (er != RE4_OK) return 1130;
    if (group.prefix != RE4_SCENARIO_PREFIX_UHD || group.smd_id != 0U || group.smx_id != 1U || group.bin_id != 2U) return 1131;
    if (group.object_status != 0x18U) return 1132;
    if (strcmp(re4_scenario_status_flag_label(RE4_SCENARIO_STATUS_ENDS_SMX_GROUP), "ends_smx_group") != 0) return 1133;
    line.ptr = "FILE_00_SMD_003";
    line.len = 15UL;
    er = re4_scenario_group_name_parse(line, &group);
    if (er != RE4_OK) return 1134;
    if (group.prefix != RE4_SCENARIO_PREFIX_FILE || group.file_id != 0U || group.smd_id != 3U) return 1135;

    src.data = (const re4_u8 *)scenariotxt;
    src.size = (re4_size)(sizeof(scenariotxt) - 1UL);
    er = re4_scenario_idx_options_read_text(src, &opt);
    if (er != RE4_OK) return 1140;
    if (opt.magic != 0x40U || !opt.use_idxuhdtpl || opt.use_idxmaterial) return 1141;
    if (!opt.enable_dynamic_vertex_color || !re4_strref_eq_cstr(opt.bin_folder, "r204_004_BIN")) return 1142;
    return 0;
}


static int test_mtl_full_bridge(void)
{
    static const char mtltxt[] =
        "# full MTL bridge\n"
        "newmtl UHD_MATERIAL_000\n"
        "Ka 1.000 1.000 1.000\n"
        "Kd 1.000 1.000 1.000\n"
        "Ks 1.000 0.000 0.000\n"
        "map_Kd qualquer/41000001/0015.dds\n"
        "map_Bump qualquer/41000001/0016.tga\n"
        "Bump qualquer/41000001/0017.tga\n"
        "map_d qualquer/41000001/0018.tga\n"
        "map_Ns -s 4.000000 3.000000 1 41000001/0019.tga\n"
        "newmtl UHD_MATERIAL_001\n"
        "Ks 0.000 1.000 0.000\n"
        "map_Kd qualquer/41000001/0015.dds\n"
        "map_Ns -s 2.000000 2.000000 1 07000000/0004.dds\n";
    re4_arena arena;
    re4_span src;
    re4_mtl mtl;
    re4_idxuhdtpl idx;
    re4_idxmaterial mats;
    re4_u32 found;
    re4_err er;

    src.data = (const re4_u8 *)mtltxt;
    src.size = (re4_size)(sizeof(mtltxt) - 1UL);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_mtl_read_text(src, &arena, &mtl);
    if (er != RE4_OK) return 1201;
    if (mtl.material_count != 2U) return 1202;
    if (!re4_strref_eq_cstr(mtl.materials[0].name, "UHD_MATERIAL_000")) return 1203;
    if ((mtl.materials[0].seen_mask & RE4_MTL_MATERIAL_BUMP_IS_BUMP) == 0U) return 1204;
    if (mtl.materials[0].bump.texture_id != 17U) return 1205;
    if (mtl.materials[0].specular_rgb[0] != 255U || mtl.materials[0].specular_rgb[1] != 0U) return 1206;

    er = re4_mtl_collect_idxuhdtpl(&mtl, 64U, 32U, &arena, &idx);
    if (er != RE4_OK) return 1210;
    if (idx.entry_count != 5U) return 1211;
    er = re4_idxuhdtpl_find(&idx, 0x41000001U, 16U, &found);
    if (er == RE4_OK) return 1212;
    er = re4_idxuhdtpl_find(&idx, 0x41000001U, 17U, &found);
    if (er != RE4_OK || idx.entries[found].id != 1U) return 1213;
    if (idx.entries[found].pixel_format_type != 0x03U) return 1214;
    er = re4_idxuhdtpl_find(&idx, 0x41000001U, 19U, &found);
    if (er != RE4_OK || idx.entries[found].pixel_format_type != 0x06U) return 1215;

    er = re4_idxmaterial_from_mtl(&mtl, &idx, &arena, &mats);
    if (er != RE4_OK) return 1220;
    if (mats.entry_count != 2U) return 1221;
    if (mats.entries[0].core.material_flag != (RE4_BIN_MATERIAL_F_BUMP_MAP | RE4_BIN_MATERIAL_F_OPACITY_MAP | RE4_BIN_MATERIAL_F_CUSTOM_SPECULAR)) return 1222;
    if (mats.entries[0].core.diffuse_map != 0U || mats.entries[0].core.bump_map != 1U) return 1223;
    if (mats.entries[0].core.opacity_map != 2U || mats.entries[0].core.custom_specular_map != 3U) return 1224;
    if (mats.entries[0].core.specular_scale != 0x32U) return 1225;
    if (mats.entries[1].core.material_flag != RE4_BIN_MATERIAL_F_GENERIC_SPECULAR) return 1226;
    if (mats.entries[1].core.generic_specular_map != 4U || mats.entries[1].core.specular_scale != 0x11U) return 1227;
    return 0;
}



static int test_scenario_idx_idxuubin_order(void)
{
    static const char scenariotxt[] =
        "Magic:0040\n"
        "SmdFileName:r204_004.SMD\n"
        "TplFileName:r204_004.TPL\n"
        "BinFolder:r204_004_BIN\n"
        "UseIdxUhdTpl:true\n"
        "UseIdxMaterial:false\n"
        "EnableVertexColor:false\n"
        "EnableDinamicVertexColor:true\n"
        "ExtraParameter_0:1\n"
        "ExtraFileName_0:r100_blk.SMD\n"
        "ExtraTplFileName_0:r100_blk.TPL\n"
        "SMD_000\n"
        "PositionX:1.5000\n"
        "PositionY:-2.2500\n"
        "PositionZ:0.0\n"
        "AngleX:90.0\n"
        "AngleY:0.0\n"
        "AngleZ:180.0\n"
        "ScaleX:1.0\n"
        "ScaleY:2.0\n"
        "ScaleZ:3.0\n"
        "TplFileID:1\n"
        "BinFileID:2\n"
        "SmxID:3\n"
        "ObjectStatus:18\n"
        "FixedFF:FF\n"
        "Unused1:0A\n"
        "Unused7:07\n";
    static const char ordertxt[] =
        "UseMaterial:C_ALPHA\n"
        "material_flag:04\n"
        "opacity_map:3\n"
        "UseMaterial:B_OPAQUE\n"
        "material_flag:00\n"
        "UseMaterial:A_ALPHA\n"
        "material_flag:04\n"
        "opacity_map:4\n";
    static const char ubintxt[] =
        "Header\n"
        "UseVertexColor:1\n"
        "MaterialCount:2\n"
        "BoneCount:3\n"
        "WeightMapCount:4\n"
        "TexCount:5\n"
        "MorphCount:1\n"
        "Scale:1.2500\n";
    re4_arena arena;
    re4_span src;
    re4_scenario_idx_doc scen;
    re4_idxmaterial mats;
    re4_idxmaterial_order_report report;
    re4_u32 *plan;
    re4_u32 plan_count;
    re4_idxuubin uubin;
    re4_idxuubin_kv kv;
    re4_err er;

    src.data = (const re4_u8 *)scenariotxt;
    src.size = (re4_size)(sizeof(scenariotxt) - 1UL);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_scenario_idx_read_text(src, &arena, &scen);
    if (er != RE4_OK) return 1301;
    if (scen.entry_count != 1U) return 1302;
    if (scen.extra_parameter_count != 1U || scen.extra_file_name_count != 1U || scen.extra_tpl_file_name_count != 1U) return 1303;
    if (scen.entries[0].position[0] != 98304 || scen.entries[0].position[1] != (re4_q16)-147456) return 1304;
    if (scen.entries[0].scale[1] != 131072 || scen.entries[0].scale[2] != 196608) return 1305;
    if (scen.entries[0].tpl_file_id != 1U || scen.entries[0].bin_file_id != 2U || scen.entries[0].smx_id != 3U) return 1306;
    if (scen.entries[0].object_status != 0x18U || scen.entries[0].fixed_ff != 0xFFU) return 1307;
    if (!re4_scenario_idx_entry_has_status(&scen.entries[0], RE4_SCENARIO_STATUS_USE_SHARED_BIN)) return 1308;
    if (!re4_scenario_idx_entry_has_status(&scen.entries[0], RE4_SCENARIO_STATUS_ENDS_SMX_GROUP)) return 1309;
    if (scen.entries[0].unused[0] != 0x0AU || scen.entries[0].unused[6] != 0x07U) return 1310;

    src.data = (const re4_u8 *)ordertxt;
    src.size = (re4_size)(sizeof(ordertxt) - 1UL);
    re4_arena_reset(&arena);
    er = re4_idxmaterial_read_text(src, &arena, &mats);
    if (er != RE4_OK) return 1320;
    er = re4_idxmaterial_validate_order(&mats, &report);
    if (er != RE4_OK) return 1321;
    if ((report.flags & RE4_IDXMATERIAL_ORDER_OPAQUE_AFTER_A) == 0U) return 1322;
    if ((report.flags & RE4_IDXMATERIAL_ORDER_ALPHA_BREAK) == 0U) return 1323;
    if (report.transparent_count != 2U) return 1324;
    er = re4_idxmaterial_make_order_plan(&mats, &arena, &plan, &plan_count);
    if (er != RE4_OK) return 1325;
    if (plan_count != 3U) return 1326;
    if (plan[0] != 1U || plan[1] != 2U || plan[2] != 0U) return 1327;

    src.data = (const re4_u8 *)ubintxt;
    src.size = (re4_size)(sizeof(ubintxt) - 1UL);
    re4_arena_reset(&arena);
    er = re4_idxuubin_read_text(src, &arena, &uubin);
    if (er != RE4_OK) return 1330;
    if (uubin.kv_count != 7U) return 1331;
    if (!uubin.use_vertex_color || uubin.material_count != 2U || uubin.bone_count != 3U) return 1332;
    if (uubin.weightmap_count != 4U || uubin.texture_count != 5U || uubin.morph_count != 1U) return 1333;
    er = re4_idxuubin_find_first(&uubin, "Scale", &kv);
    if (er != RE4_OK) return 1334;
    if ((kv.flags & RE4_IDXUUBIN_KV_HAS_Q16) == 0U || kv.value_q16 != 81920) return 1335;
    er = re4_idxuubin_find_first(&uubin, "UseVertexColor", &kv);
    if (er != RE4_OK) return 1336;
    if ((kv.flags & RE4_IDXUUBIN_KV_BOOL_TRUE) == 0U) return 1337;
    return 0;
}


static int test_ps2_tpl_idxps2bin_r100(void)
{
    static const char ps2tpltxt[] =
        "ImageFlipY:true\n"
        "ImageFolder:idm000\n"
        "ImageFormat:TGA\n";
    static const char headertxt[] =
        "Bitdepth:9\n"
        "Interlace:0x03\n"
        "Width:128\n"
        "Height:64\n"
        "Next:0x20\n"
        "MipmapStatus:0x02\n"
        "Qwc:8\n"
        "Ref:0x77\n"
        "MipmapHeader1Offset:0x40\n"
        "MipmapHeader2Offset:0x70\n"
        "GsMip:0x11112222\n"
        "GsMipHi:0x33334444\n"
        "IndexesOffset:0x100\n"
        "PaletteOffset:0x180\n"
        "GsTex:0x1234ABCD\n"
        "GsTexHi:0x56789ABC\n";
    static const char ps2bintxt[] =
        "IDXPS2BIN\n"
        "UseIdxMaterial:true\n"
        "AutoCalcBoundingBox:true\n"
        "MaterialCount:3\n"
        "BoneCount:4\n"
        "WeightMapCount:5\n"
        "Scale:1.5000\n";
    static const char r100txt[] =
        "Magic:0140\n"
        "ExtraParameter_0:7\n"
        "ExtraParameter_1:9\n"
        "ExtraSmdFileName_0:r100_00_000.SMD\n"
        "ExtraTplFileName_0:R100.FILE_0.TPL\n";
    re4_u8 ps2tplbin[0x600];
    re4_u8 ps2small[0x600];
    re4_u8 rgba_out[64];
    re4_ps2_tpl_image_header small_h;
    re4_u8 pal_rgba[4];
    re4_span src;
    re4_span idxspan;
    re4_span palspan;
    re4_idxps2tpl ps2tpl;
    re4_idxtpl_header header;
    re4_idxtpl_header header_from_bin;
    re4_ps2_tpl ps2tpl_binary;
    re4_idxps2bin ps2bin;
    re4_idxuubin_kv kv;
    re4_scenario_r100 r100;
    re4_arena arena;
    re4_u32 tex_id;
    re4_strref ps2path;
    re4_err er;
    re4_size i;
    re4_u32 warnings;

    for (i = 0UL; i < sizeof(ps2tplbin); ++i) ps2tplbin[i] = 0U;
    put_le32(ps2tplbin + 0x00, 0x3054504CU);
    put_le32(ps2tplbin + 0x04, 1U);
    put_le32(ps2tplbin + 0x08, 0x10U);
    put_le32(ps2tplbin + 0x0C, 0U);
    put_le16(ps2tplbin + 0x10, 128U);
    put_le16(ps2tplbin + 0x12, 64U);
    put_le16(ps2tplbin + 0x14, 9U);
    put_le16(ps2tplbin + 0x16, 3U);
    put_le16(ps2tplbin + 0x18, 0x20U);
    put_le16(ps2tplbin + 0x1A, RE4_PS2TPL_MIPMAP_TWO);
    put_le16(ps2tplbin + 0x1C, 8U);
    put_le16(ps2tplbin + 0x1E, 0x77U);
    put_le32(ps2tplbin + 0x20, 0x40U);
    put_le32(ps2tplbin + 0x24, 0x70U);
    put_le32(ps2tplbin + 0x28, 0x11112222U);
    put_le32(ps2tplbin + 0x2C, 0x33334444U);
    put_le32(ps2tplbin + 0x30, 0x100U);
    put_le32(ps2tplbin + 0x34, 0x180U);
    put_le32(ps2tplbin + 0x38, 0x1234ABCDU);
    put_le32(ps2tplbin + 0x3C, 0x56789ABCU);
    put_le16(ps2tplbin + 0x40, 64U);
    put_le16(ps2tplbin + 0x42, 32U);
    put_le16(ps2tplbin + 0x44, 9U);
    put_le16(ps2tplbin + 0x46, 0U);
    put_le16(ps2tplbin + 0x4C, 2U);
    put_le32(ps2tplbin + 0x60, 0x120U);
    put_le32(ps2tplbin + 0x64, 0x180U);
    put_le16(ps2tplbin + 0x70, 32U);
    put_le16(ps2tplbin + 0x72, 16U);
    put_le16(ps2tplbin + 0x74, 9U);
    put_le16(ps2tplbin + 0x7C, 1U);
    put_le32(ps2tplbin + 0x90, 0x140U);
    put_le32(ps2tplbin + 0x94, 0x180U);

    src.data = (const re4_u8 *)ps2tpltxt;
    src.size = (re4_size)(sizeof(ps2tpltxt) - 1UL);
    er = re4_idxps2tpl_read_text(src, &ps2tpl);
    if (er != RE4_OK) return 1401;
    if (!ps2tpl.image_flip_y || ps2tpl.seen_mask != (RE4_IDXPS2TPL_SEEN_IMAGE_FLIP_Y | RE4_IDXPS2TPL_SEEN_IMAGE_FOLDER | RE4_IDXPS2TPL_SEEN_IMAGE_FORMAT)) return 1402;

    src.data = (const re4_u8 *)headertxt;
    src.size = (re4_size)(sizeof(headertxt) - 1UL);
    er = re4_idxtpl_header_read_text(src, &header);
    if (er != RE4_OK) return 1410;
    if (header.bitdepth != 9U || re4_ps2tpl_bitdepth_palette_limit(header.bitdepth) != 256U) return 1411;
    if (!re4_idxtpl_header_has_interlace(&header, RE4_PS2TPL_INTERLACE_ROTATE_IMAGE)) return 1412;
    if (!re4_idxtpl_header_has_interlace(&header, RE4_PS2TPL_INTERLACE_SWIZZLE)) return 1413;
    if (header.width != 128U || header.height != 64U || header.gs_tex != 0x1234ABCDU || header.gs_tex_hi != 0x56789ABCU) return 1414;
    if (!re4_idxtpl_header_has_mipmaps(&header) || header.indexes_offset != 0x100U || header.palette_offset != 0x180U) return 1415;
    warnings = 0U;
    er = re4_idxtpl_header_validate_basic(&header, &warnings);
    if (er != RE4_OK || warnings != 0U) return 1416;

    src.data = ps2tplbin;
    src.size = sizeof(ps2tplbin);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_ps2_tpl_read(src, RE4_ENDIAN_LITTLE, &arena, &ps2tpl_binary);
    if (er != RE4_OK) return 1417;
    if (ps2tpl_binary.image_count != 1U || ps2tpl_binary.start_offset != 0x10U) return 1418;
    if (!re4_ps2_tpl_image_has_mipmaps(&ps2tpl_binary.images[0])) return 1419;
    er = re4_ps2_tpl_indexes_span(src, &ps2tpl_binary.images[0].main_header, &idxspan);
    if (er != RE4_OK || idxspan.size != 128UL) return 1450;
    er = re4_ps2_tpl_palette_span(src, &ps2tpl_binary.images[0].main_header, &palspan);
    if (er != RE4_OK || palspan.size != 1024UL) return 1451;
    er = re4_ps2_tpl_main_header_as_idxtpl(&ps2tpl_binary.images[0], &header_from_bin);
    if (er != RE4_OK) return 1452;
    if (header_from_bin.ref != 0x77U || header_from_bin.mipmap_header1_offset != 0x40U || header_from_bin.gs_mip_hi != 0x33334444U) return 1453;

    for (i = 0UL; i < sizeof(ps2small); ++i) ps2small[i] = 0U;
    for (i = 0UL; i < sizeof(rgba_out); ++i) rgba_out[i] = 0U;
    memset(&small_h, 0, sizeof(small_h));
    small_h.width = 4U;
    small_h.height = 2U;
    small_h.bitdepth = 0x0008U;
    small_h.interlace = 0U;
    small_h.qwc = 1U;
    small_h.indexes_offset = 0x40U;
    small_h.palette_offset = 0x80U;
    ps2small[0x40] = 0x10U;
    ps2small[0x41] = 0x98U;
    ps2small[0x42] = 0x32U;
    ps2small[0x43] = 0xBAU;
    ps2small[0x80 + 0] = 10U; ps2small[0x80 + 3] = 0x80U;
    ps2small[0x84 + 0] = 20U; ps2small[0x84 + 3] = 0x80U;
    ps2small[0x88 + 0] = 30U; ps2small[0x88 + 3] = 0x80U;
    ps2small[0x8C + 0] = 40U; ps2small[0x8C + 3] = 0x80U;
    ps2small[0x80 + (16UL * 4UL) + 0] = 80U; ps2small[0x80 + (16UL * 4UL) + 3] = 0x80U;
    ps2small[0x80 + (17UL * 4UL) + 0] = 90U; ps2small[0x80 + (17UL * 4UL) + 3] = 0x80U;
    ps2small[0x80 + (18UL * 4UL) + 0] = 100U; ps2small[0x80 + (18UL * 4UL) + 3] = 0x80U;
    ps2small[0x80 + (19UL * 4UL) + 0] = 110U; ps2small[0x80 + (19UL * 4UL) + 3] = 0x80U;
    src.data = ps2small;
    src.size = sizeof(ps2small);
    er = re4_ps2_tpl_palette_span(src, &small_h, &palspan);
    if (er != RE4_OK || palspan.size != 0x80UL) return 1454;
    if (re4_ps2_tpl_palette_source_index(0x0008U, 8U) != 16U) return 1455;
    er = re4_ps2_tpl_palette_entry_rgba32(palspan, 0x0008U, 8U, pal_rgba);
    if (er != RE4_OK || pal_rgba[0] != 80U || pal_rgba[3] != 255U) return 1456;
    er = re4_ps2_tpl_decode_header_rgba32(src, &small_h, rgba_out, sizeof(rgba_out), 16U);
    if (er != RE4_OK) return 1457;
    if (rgba_out[0] != 10U || rgba_out[4] != 20U || rgba_out[8] != 80U || rgba_out[12] != 90U) return 1458;
    if (rgba_out[16] != 30U || rgba_out[20] != 40U || rgba_out[24] != 100U || rgba_out[28] != 110U) return 1459;

    for (i = 0UL; i < sizeof(rgba_out); ++i) rgba_out[i] = 0U;
    memset(&small_h, 0, sizeof(small_h));
    small_h.width = 2U;
    small_h.height = 1U;
    small_h.bitdepth = 0x0009U;
    small_h.interlace = 0U;
    small_h.qwc = 1U;
    small_h.indexes_offset = 0x40U;
    small_h.palette_offset = 0x80U;
    ps2small[0x40] = 8U;
    ps2small[0x41] = 16U;
    ps2small[0x80 + (16UL * 4UL) + 0] = 160U; ps2small[0x80 + (16UL * 4UL) + 3] = 0x80U;
    ps2small[0x80 + (8UL * 4UL) + 0] = 88U; ps2small[0x80 + (8UL * 4UL) + 3] = 0x80U;
    if (re4_ps2_tpl_palette_source_index(0x0009U, 8U) != 16U) return 1460;
    if (re4_ps2_tpl_palette_source_index(0x0009U, 16U) != 8U) return 1461;
    er = re4_ps2_tpl_decode_header_rgba32(src, &small_h, rgba_out, sizeof(rgba_out), 8U);
    if (er != RE4_OK || rgba_out[0] != 160U || rgba_out[4] != 88U) return 1462;

    for (i = 0UL; i < sizeof(rgba_out); ++i) rgba_out[i] = 0U;
    memset(&small_h, 0, sizeof(small_h));
    small_h.width = 1U;
    small_h.height = 2U;
    small_h.bitdepth = 0x0006U;
    small_h.interlace = 0U;
    small_h.qwc = 1U;
    small_h.indexes_offset = 0x40U;
    ps2small[0x40] = 1U; ps2small[0x41] = 2U; ps2small[0x42] = 3U; ps2small[0x43] = 4U;
    ps2small[0x44] = 5U; ps2small[0x45] = 6U; ps2small[0x46] = 7U; ps2small[0x47] = 8U;
    er = re4_ps2_tpl_decode_header_rgba32(src, &small_h, rgba_out, sizeof(rgba_out), 4U);
    if (er != RE4_OK || rgba_out[0] != 1U || rgba_out[3] != 4U || rgba_out[4] != 5U || rgba_out[7] != 8U) return 1463;
    small_h.interlace = RE4_PS2TPL_INTERLACE_SWIZZLE;
    er = re4_ps2_tpl_decode_header_rgba32(src, &small_h, rgba_out, sizeof(rgba_out), 4U);
    if (er != RE4_ERR_UNSUPPORTED) return 1464;

    src.data = (const re4_u8 *)ps2bintxt;
    src.size = (re4_size)(sizeof(ps2bintxt) - 1UL);
    re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
    er = re4_idxps2bin_read_text(src, &arena, &ps2bin);
    if (er != RE4_OK) return 1420;
    if (ps2bin.kv_count != 6U || !ps2bin.use_idxmaterial || !ps2bin.auto_calc_bounding_box) return 1421;
    if (ps2bin.material_count != 3U || ps2bin.bone_count != 4U || ps2bin.weightmap_count != 5U) return 1422;
    er = re4_idxps2bin_find_first(&ps2bin, "Scale", &kv);
    if (er != RE4_OK) return 1423;
    if ((kv.flags & RE4_IDXUUBIN_KV_HAS_Q16) == 0U || kv.value_q16 != 98304) return 1424;

    ps2path.ptr = "folder/0015.tga";
    ps2path.len = 15UL;
    er = re4_mtl_ps2_texture_id_from_path(ps2path, &tex_id);
    if (er != RE4_OK || tex_id != 15U) return 1430;
    if (re4_scenario_alignment_for_prefix(RE4_SCENARIO_PREFIX_GG) != 32U) return 1431;
    if (re4_scenario_alignment_for_prefix(RE4_SCENARIO_PREFIX_UU) != 16U) return 1432;

    src.data = (const re4_u8 *)r100txt;
    src.size = (re4_size)(sizeof(r100txt) - 1UL);
    re4_arena_reset(&arena);
    er = re4_scenario_r100_read_text(src, &arena, &r100);
    if (er != RE4_OK) return 1440;
    if (r100.extra_parameter_count != 2U || r100.extra_smd_count != 1U || r100.extra_tpl_count != 1U) return 1441;
    if (r100.extra_parameters[1].id != 1U || r100.extra_parameters[1].value != 9U) return 1442;
    return 0;
}

int main(void)
{
    int r;
    r = test_tpl_decode_i4(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_uhd_tpl_metadata(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_pmd_survey_and_smx(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_dat_offset_table(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_gca_metadata(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_lfs_uncompressed_extract(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_lfs_write_store_roundtrip(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_evd_manifest_file_kinds(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_evd_resource_refs(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_bin_and_smd_readers(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_esl(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_fcv_axis_sizes_q16(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_tplcue_material_smd_text(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_idxuhdtpl_idxmaterial_mtl_scenario_text(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_mtl_full_bridge(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_scenario_idx_idxuubin_order(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    r = test_ps2_tpl_idxps2bin_r100(); if (r != 0) { printf("selftest failed %d\n", r); return 1; }
    printf("selftest ok\n");
    return 0;
}
