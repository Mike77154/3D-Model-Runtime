#ifndef RE4_EVENT_H
#define RE4_EVENT_H

#include "re4_types.h"
#include "re4_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum re4_evp_type_e {
    RE4_EVP_BEGIN_EVT = 0,
    RE4_EVP_SET_PL = 1,
    RE4_EVP_SET_EM = 2,
    RE4_EVP_SET_OM = 3,
    RE4_EVP_SET_PARTS = 4,
    RE4_EVP_SET_LIST = 5,
    RE4_EVP_CAM = 6,
    RE4_EVP_CAM_POS = 7,
    RE4_EVP_CAM_DUMMY = 8,
    RE4_EVP_POS = 9,
    RE4_EVP_POS_PL = 10,
    RE4_EVP_MOT = 11,
    RE4_EVP_SHP = 12,
    RE4_EVP_ESP = 13,
    RE4_EVP_LIT = 14,
    RE4_EVP_STR = 15,
    RE4_EVP_SE = 16,
    RE4_EVP_MES = 17,
    RE4_EVP_FUNC = 18,
    RE4_EVP_PARENT_ON = 19,
    RE4_EVP_PARENT_OFF = 20,
    RE4_EVP_END_PL = 21,
    RE4_EVP_END_EM = 22,
    RE4_EVP_END_OM = 23,
    RE4_EVP_END_PARTS = 24,
    RE4_EVP_END_LIST = 25,
    RE4_EVP_END_EVT = 26,
    RE4_EVP_END_PAC = 27,
    RE4_EVP_SET_EFF = 28,
    RE4_EVP_FADE = 29,
    RE4_EVP_FOG = 30,
    RE4_EVP_FOCUS = 31,
    RE4_EVP_SET_MDT = 32
} re4_evp_type;

typedef struct re4_evd_packet_s {
    re4_u32 type;
    re4_u32 flags_common;
    re4_i16 cut_no;
    re4_i16 frame;
    re4_i16 size;
    re4_u16 pac_no;
    re4_u32 file_offset;
    re4_span payload;
} re4_evd_packet;

typedef struct re4_evd_file_s {
    re4_strref name;
    re4_u32 offset;
    re4_u32 size;
    re4_u32 flags;
} re4_evd_file;

typedef struct re4_evd_s {
    re4_strref name_file;
    re4_strref room_no;
    re4_strref event_no;
    re4_i32 serial_no;
    re4_i32 event_flag;
    re4_u32 offset_pac;
    re4_u32 size_pac;
    re4_u32 num_bin_tbl;
    re4_u32 offset_bin_tbl;
    re4_u32 packet_count;
    re4_evd_packet *packets;
    re4_evd_file *files;
    re4_endian endian;
} re4_evd;

#define RE4_EVD_PACKET_TYPE_LIMIT 33U

typedef enum re4_evd_file_kind_e {
    RE4_EVD_FILE_UNKNOWN = 0,
    RE4_EVD_FILE_BIN = 1,
    RE4_EVD_FILE_TPL = 2,
    RE4_EVD_FILE_FCV = 3,
    RE4_EVD_FILE_SEQ = 4,
    RE4_EVD_FILE_LIT = 5,
    RE4_EVD_FILE_EFF = 6,
    RE4_EVD_FILE_SMD = 7,
    RE4_EVD_FILE_MDT = 8,
    RE4_EVD_FILE_ESL = 9,
    RE4_EVD_FILE_PMD = 10,
    RE4_EVD_FILE_COUNT = 11
} re4_evd_file_kind;

typedef struct re4_evd_manifest_s {
    re4_u32 file_counts[RE4_EVD_FILE_COUNT];
    re4_u32 packet_counts[RE4_EVD_PACKET_TYPE_LIMIT];
    re4_u32 unknown_packet_count;
    re4_u32 model_ref_count;
    re4_u32 parent_ref_count;
    re4_u32 bin_ref_count;
    re4_u32 tpl_ref_count;
} re4_evd_manifest;

#define RE4_EVD_REF_MODEL 1U
#define RE4_EVD_REF_PARENT 2U
#define RE4_EVD_REF_BIN 4U
#define RE4_EVD_REF_TPL 8U

typedef struct re4_evd_resource_ref_s {
    re4_strref model_name;
    re4_strref parent_name;
    re4_strref bin_name;
    re4_strref tpl_name;
    re4_u32 flags;
} re4_evd_resource_ref;


typedef struct re4_evd_resource_index_s {
    re4_u32 count;
    re4_u32 skipped_packets;
    re4_evd_resource_ref *refs;
} re4_evd_resource_index;


typedef struct re4_evd_packet_info_s {
    re4_strref name_mod;
    re4_strref name_parent;
    re4_strref name_bin;
    re4_strref name_tpl;
    re4_i32 ivalue0;
    re4_i32 ivalue1;
    re4_i32 ivalue2;
    re4_q16 pos[3];
    re4_q16 ang[3];
    re4_u32 has_pos;
    re4_u32 has_ang;
} re4_evd_packet_info;

typedef struct re4_lit_probe_s {
    re4_u32 file_size;
    re4_u32 possible_record_count;
} re4_lit_probe;

typedef struct re4_eff_probe_s {
    re4_u32 file_size;
    re4_u32 possible_record_count;
} re4_eff_probe;

re4_err re4_evd_read(re4_span src, re4_endian endian, re4_arena *arena, re4_evd *out);
const char *re4_evp_type_name(re4_u32 type);
const char *re4_evd_file_kind_name(re4_evd_file_kind kind);
re4_evd_file_kind re4_evd_file_kind_from_name(re4_strref name);
re4_err re4_evd_build_manifest(const re4_evd *evd, re4_evd_manifest *out);
re4_u32 re4_evd_count_files_of_kind(const re4_evd *evd, re4_evd_file_kind kind);
re4_err re4_evd_packet_info_read(const re4_evd_packet *p, re4_endian endian, re4_evd_packet_info *out);
re4_err re4_evd_packet_resource_ref(const re4_evd_packet *p, re4_evd_resource_ref *out);
re4_err re4_evd_packet_resource_ref_e(const re4_evd_packet *p, re4_endian endian, re4_evd_resource_ref *out);
re4_err re4_evd_resource_index_build(const re4_evd *evd, re4_arena *arena, re4_evd_resource_index *out);
re4_err re4_evd_packet_resource_ref_e(const re4_evd_packet *p, re4_endian endian, re4_evd_resource_ref *out);
re4_err re4_evd_file_span(re4_span src, const re4_evd *evd, re4_u32 file_index, re4_span *out);
re4_err re4_evd_packet_ref_name(const re4_evd_packet *p, re4_strref *a, re4_strref *b);
re4_u32 re4_evd_count_packets_of_type(const re4_evd *evd, re4_u32 type);

re4_err re4_lit_probe_read(re4_span src, re4_lit_probe *out);
re4_err re4_eff_probe_read(re4_span src, re4_eff_probe *out);

#ifdef __cplusplus
}
#endif

#endif
