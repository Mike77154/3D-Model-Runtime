#ifndef TMD_HMD_CAT3_H
#define TMD_HMD_CAT3_H

/*
   tmd_hmd_cat3.h
   Defensive HMD Category 3 animation program scanner/interpreter.

   This is not a dynamic Psy-Q/libhmd replacement. It is a small, fixed-point,
   no-heap VM layer for reading and stepping the Category 3 sequence-control
   bytecode/program data described by Sony's HMD format.

   C89, caller-owned slots, no malloc/free/realloc, no float/double.
*/

#include "tmd_hmd3d.h"
#include "tmd_anim3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TMD_HMD_CAT3_WARN_NAKED_TYPE        ((tmd_u32)0x00100000UL)
#define TMD_HMD_CAT3_WARN_CHAIN_PTR_WORDS   ((tmd_u32)0x00200000UL)
#define TMD_HMD_CAT3_WARN_CHAIN_PTR_BYTES   ((tmd_u32)0x00400000UL)
#define TMD_HMD_CAT3_WARN_TRUNCATED_SECTION ((tmd_u32)0x00800000UL)
#define TMD_HMD_CAT3_WARN_UNKNOWN_DESC      ((tmd_u32)0x01000000UL)

#define TMD_HMD_CAT3_MAX_UPDATE_PTRS        8U
#define TMD_HMD_CAT3_MAX_VM_OPS             64U

#define TMD_HMD_CAT3_DESC_NORMAL            0U
#define TMD_HMD_CAT3_DESC_JUMP              1U
#define TMD_HMD_CAT3_DESC_CONTROL           2U
#define TMD_HMD_CAT3_DESC_UNKNOWN           255U

#define TMD_HMD_CAT3_EVENT_NONE             0U
#define TMD_HMD_CAT3_EVENT_KEY              1U
#define TMD_HMD_CAT3_EVENT_JUMP             2U
#define TMD_HMD_CAT3_EVENT_END              3U
#define TMD_HMD_CAT3_EVENT_WORK             4U
#define TMD_HMD_CAT3_EVENT_UNKNOWN_CONTROL  5U

#define TMD_HMD_CAT3_CONTROL_END            1U
#define TMD_HMD_CAT3_CONTROL_WORK           2U

#define TMD_HMD_CAT3_SPEED_ONE              16
#define TMD_HMD_CAT3_NO_INTR_IDX            0xffffU

typedef struct TMD_HMD_Cat3_TypeInfo TMD_HMD_Cat3_TypeInfo;
typedef struct TMD_HMD_Cat3_Primitive TMD_HMD_Cat3_Primitive;
typedef struct TMD_HMD_Cat3_DataSection TMD_HMD_Cat3_DataSection;
typedef struct TMD_HMD_Cat3_AnimHeader TMD_HMD_Cat3_AnimHeader;
typedef struct TMD_HMD_Cat3_Descriptor TMD_HMD_Cat3_Descriptor;
typedef struct TMD_HMD_Cat3_SequenceState TMD_HMD_Cat3_SequenceState;
typedef struct TMD_HMD_Cat3_Event TMD_HMD_Cat3_Event;
typedef struct TMD_HMD_Cat3_RstKey TMD_HMD_Cat3_RstKey;

struct TMD_HMD_Cat3_TypeInfo {
    tmd_u32 raw_type;
    tmd_u8 developer_id;
    tmd_u8 category;
    tmd_u8 driver;
    tmd_u16 primitive_code;

    /* Animation type field helpers. These are conservative bit extractions,
       not libhmd function pointers. raw_type is always preserved. */
    tmd_u8 ini;
    tmd_u8 frame_driver_category;
    tmd_u8 target;

    /* TGT=0 coordinate update helpers. */
    tmd_u8 rot_order;
    tmd_u8 scale_intr;
    tmd_u8 rot_intr;
    tmd_u8 trans_intr;

    /* TGT=1 general-purpose update helpers. */
    tmd_u8 general_length;
    tmd_u8 general_write_mask;
    tmd_u8 general_intr_alg;
};

struct TMD_HMD_Cat3_Primitive {
    tmd_u32 primitive_offset;
    tmd_u32 next_raw;
    tmd_u32 next_offset;
    tmd_u32 primitive_header_raw;
    tmd_u32 primitive_header_offset;
    tmd_u32 type_count_raw;
    tmd_u32 type_count;
    tmd_u8 mapped_flag;
};

struct TMD_HMD_Cat3_DataSection {
    tmd_u32 primitive_offset;
    tmd_u32 section_offset;
    tmd_u32 data_offset;
    tmd_u32 data_bytes;
    tmd_u32 data_count;
    tmd_u32 word_size;
    TMD_HMD_Cat3_TypeInfo type_info;
};

struct TMD_HMD_Cat3_AnimHeader {
    tmd_u32 header_offset;
    tmd_u32 header_size_words;
    tmd_u32 interp_table_top_raw;
    tmd_u32 interp_table_offset;
    tmd_u32 control_top_raw;
    tmd_u32 control_offset;
    tmd_u32 param_top_raw;
    tmd_u32 param_offset;
    tmd_u32 coord_top_raw;
    tmd_u32 coord_offset;
    tmd_u32 update_top_raw[TMD_HMD_CAT3_MAX_UPDATE_PTRS];
    tmd_u32 update_offset[TMD_HMD_CAT3_MAX_UPDATE_PTRS];
    tmd_u32 update_count;
    tmd_u32 warnings;
};

struct TMD_HMD_Cat3_Descriptor {
    tmd_u32 raw;
    tmd_u8 kind;

    /* Normal/key descriptor. */
    tmd_u8 type_index;
    tmd_u8 tframe;
    tmd_u16 parameter_index;

    /* Jump descriptor. */
    tmd_u8 stream_dst;
    tmd_u8 stream_condition;
    tmd_u16 sequence_index;

    /* Control descriptor. */
    tmd_u8 control_code;
    tmd_u8 p2;
    tmd_u16 p1;
};

struct TMD_HMD_Cat3_SequenceState {
    tmd_u16 stream_id;
    tmd_u16 start_index;
    tmd_u16 control_index;
    tmd_u16 intr_index;
    tmd_u16 src_intr_index;
    tmd_u16 dst_param_index;
    tmd_i32 rframe_fx4;
    tmd_u16 aframe;
    tmd_i8 speed_fx4;
    tmd_u8 stopped;
};

struct TMD_HMD_Cat3_Event {
    tmd_u8 event_kind;
    TMD_HMD_Cat3_Descriptor descriptor;
    tmd_u16 old_control_index;
    tmd_u16 new_control_index;
    tmd_u16 old_stream_id;
    tmd_u16 new_stream_id;
};

struct TMD_HMD_Cat3_RstKey {
    TMD_Transform transform;
};

void tmd_hmd_cat3_decode_type(tmd_u32 raw_type,
                              TMD_HMD_Cat3_TypeInfo *out_info);

int tmd_hmd_cat3_scan_block(const TMD_HMD_Context *ctx,
                            tmd_u32 block_index,
                            TMD_HMD_Cat3_Primitive *primitive_slots,
                            tmd_u32 primitive_slot_count,
                            TMD_HMD_Cat3_DataSection *cat3_slots,
                            tmd_u32 cat3_slot_count,
                            tmd_u32 *out_primitive_count,
                            tmd_u32 *out_cat3_count,
                            tmd_u32 *out_warnings);

int tmd_hmd_cat3_parse_anim_header(const TMD_HMD_Context *ctx,
                                   tmd_u32 header_offset,
                                   TMD_HMD_Cat3_AnimHeader *out_header);

int tmd_hmd_cat3_scan_interp_table(const TMD_HMD_Context *ctx,
                                   tmd_u32 table_offset,
                                   TMD_HMD_Cat3_TypeInfo *type_slots,
                                   tmd_u32 type_slot_count,
                                   tmd_u32 *out_type_count,
                                   tmd_u32 *out_flags,
                                   tmd_u32 *out_warnings);

void tmd_hmd_cat3_decode_descriptor(tmd_u32 raw,
                                    TMD_HMD_Cat3_Descriptor *out_desc);

int tmd_hmd_cat3_read_descriptor(const TMD_HMD_Context *ctx,
                                 tmd_u32 control_offset,
                                 tmd_u32 descriptor_index,
                                 TMD_HMD_Cat3_Descriptor *out_desc);

void tmd_hmd_cat3_sequence_start(TMD_HMD_Cat3_SequenceState *state,
                                 tmd_u16 stream_id,
                                 tmd_u16 start_index,
                                 tmd_i8 speed_fx4,
                                 tmd_u16 aframe);

int tmd_hmd_cat3_sequence_step(const tmd_u32 *control_words,
                               tmd_u32 control_word_count,
                               TMD_HMD_Cat3_SequenceState *state,
                               TMD_HMD_Cat3_Event *out_event);

int tmd_hmd_cat3_interp_rst_key(const TMD_HMD_Cat3_RstKey *src_key,
                                const TMD_HMD_Cat3_RstKey *dst_key,
                                tmd_i32 t_fx12,
                                TMD_Transform *out_transform);

const char *tmd_hmd_cat3_descriptor_string(tmd_u8 kind);
const char *tmd_hmd_cat3_event_string(tmd_u8 event_kind);

#ifdef __cplusplus
}
#endif

#endif /* TMD_HMD_CAT3_H */
