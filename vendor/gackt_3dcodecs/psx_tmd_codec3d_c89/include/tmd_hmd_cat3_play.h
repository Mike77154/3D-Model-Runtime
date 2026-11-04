#ifndef TMD_HMD_CAT3_PLAY_H
#define TMD_HMD_CAT3_PLAY_H

/*
   tmd_hmd_cat3_play.h
   Higher-level fixed-point helpers for HMD Category 3 animation playback.

   This layer stays intentionally portable: it does not call Psy-Q drivers or
   patch pointers. It decodes sequence starts, reads common COORDINATE and
   GENERAL key parameter packets, interpolates LINEAR/BEZIER/BSPLINE scalars,
   and advances a no-heap sequence cursor that can feed the user's own engine.

   C89, no malloc/free/realloc, no float/double.
*/

#include "tmd_hmd_cat3.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TMD_HMD_CAT3_INTR_NONE        0U
#define TMD_HMD_CAT3_INTR_LINEAR      1U
#define TMD_HMD_CAT3_INTR_BEZIER      2U
#define TMD_HMD_CAT3_INTR_BSPLINE     3U
#define TMD_HMD_CAT3_INTR_LINEAR_ONE  9U
#define TMD_HMD_CAT3_INTR_BEZIER_ONE  10U
#define TMD_HMD_CAT3_INTR_BSPLINE_ONE 11U

#define TMD_HMD_CAT3_PLAY_WARN_UNSUPPORTED_INTR ((tmd_u32)0x02000000UL)
#define TMD_HMD_CAT3_PLAY_WARN_TFRAME_ZERO      ((tmd_u32)0x04000000UL)
#define TMD_HMD_CAT3_PLAY_WARN_PARAM_RANGE      ((tmd_u32)0x08000000UL)

#define TMD_HMD_CAT3_COMPONENTS 3U
#define TMD_HMD_CAT3_CTRLPTS    3U

typedef struct TMD_HMD_Cat3_SequenceStart TMD_HMD_Cat3_SequenceStart;
typedef struct TMD_HMD_Cat3_CoordKey TMD_HMD_Cat3_CoordKey;
typedef struct TMD_HMD_Cat3_GeneralKey TMD_HMD_Cat3_GeneralKey;
typedef struct TMD_HMD_Cat3_PlayState TMD_HMD_Cat3_PlayState;
typedef struct TMD_HMD_Cat3_PlayEvent TMD_HMD_Cat3_PlayEvent;

struct TMD_HMD_Cat3_SequenceStart {
    tmd_u32 raw_update_index;
    tmd_u8 update_slot;
    tmd_u32 start_control_index;
    tmd_u16 stream_id;
};

struct TMD_HMD_Cat3_CoordKey {
    tmd_u8 has_trans;
    tmd_u8 has_rot;
    tmd_u8 has_scale;
    tmd_u8 trans_intr;
    tmd_u8 rot_intr;
    tmd_u8 scale_intr;
    tmd_u8 rot_order;
    tmd_i32 trans[TMD_HMD_CAT3_CTRLPTS][TMD_HMD_CAT3_COMPONENTS];
    tmd_i32 rot_deg_fx12[TMD_HMD_CAT3_CTRLPTS][TMD_HMD_CAT3_COMPONENTS];
    tmd_i32 scale_fx12[TMD_HMD_CAT3_CTRLPTS][TMD_HMD_CAT3_COMPONENTS];
    tmd_u32 words_used;
};

struct TMD_HMD_Cat3_GeneralKey {
    tmd_u8 length_code;
    tmd_u8 write_mask;
    tmd_u8 intr_alg;
    tmd_u8 component_count;
    tmd_i32 value[TMD_HMD_CAT3_CTRLPTS][4];
    tmd_u32 words_used;
};

struct TMD_HMD_Cat3_PlayState {
    TMD_HMD_Cat3_SequenceState seq;
    tmd_u16 current_type_index;
    tmd_u16 current_param_index;
    tmd_u16 next_type_index;
    tmd_u16 next_param_index;
    tmd_i32 tframe_fx4;
    tmd_i32 rframe_fx4;
    tmd_i8 speed_fx4;
    tmd_u8 primed;
};

struct TMD_HMD_Cat3_PlayEvent {
    tmd_u8 event_kind;
    TMD_HMD_Cat3_Event vm_event;
    tmd_i32 t_fx12;
    tmd_u16 src_param_index;
    tmd_u16 dst_param_index;
    tmd_u16 type_index;
};

int tmd_hmd_cat3_read_sequence_start(const TMD_HMD_Context *ctx,
                                     tmd_u32 sequence_pointer_offset,
                                     tmd_u32 start_index,
                                     TMD_HMD_Cat3_SequenceStart *out_start);

void tmd_hmd_cat3_play_start(TMD_HMD_Cat3_PlayState *state,
                             tmd_u16 stream_id,
                             tmd_u16 start_control_index,
                             tmd_i8 speed_fx4);

int tmd_hmd_cat3_play_step(const tmd_u32 *control_words,
                           tmd_u32 control_word_count,
                           TMD_HMD_Cat3_PlayState *state,
                           TMD_HMD_Cat3_PlayEvent *out_event);

int tmd_hmd_cat3_param_word_offset(tmd_u32 param_top_offset,
                                   tmd_u16 parameter_index,
                                   tmd_u32 *out_offset);

int tmd_hmd_cat3_coord_key_words(const TMD_HMD_Cat3_TypeInfo *type_info,
                                 tmd_u32 *out_words,
                                 tmd_u32 *out_warnings);

int tmd_hmd_cat3_read_coord_key(const TMD_HMD_Context *ctx,
                                tmd_u32 param_top_offset,
                                tmd_u16 parameter_index,
                                const TMD_HMD_Cat3_TypeInfo *type_info,
                                TMD_HMD_Cat3_CoordKey *out_key,
                                tmd_u32 *out_warnings);

int tmd_hmd_cat3_coord_key_to_transform(const TMD_HMD_Cat3_CoordKey *key,
                                        TMD_Transform *out_transform);

int tmd_hmd_cat3_blend_coord_keys(const TMD_HMD_Cat3_CoordKey *src_key,
                                  const TMD_HMD_Cat3_CoordKey *dst_key,
                                  tmd_i32 t_fx12,
                                  TMD_Transform *out_transform,
                                  tmd_u32 *out_warnings);

int tmd_hmd_cat3_general_key_words(const TMD_HMD_Cat3_TypeInfo *type_info,
                                   tmd_u32 *out_words,
                                   tmd_u32 *out_warnings);

int tmd_hmd_cat3_read_general_key(const TMD_HMD_Context *ctx,
                                  tmd_u32 param_top_offset,
                                  tmd_u16 parameter_index,
                                  const TMD_HMD_Cat3_TypeInfo *type_info,
                                  TMD_HMD_Cat3_GeneralKey *out_key,
                                  tmd_u32 *out_warnings);

int tmd_hmd_cat3_curve_linear_i32(tmd_i32 a,
                                  tmd_i32 b,
                                  tmd_i32 t_fx12,
                                  tmd_i32 *out_value);

int tmd_hmd_cat3_curve_bezier_i32(tmd_i32 p0,
                                  tmd_i32 p1,
                                  tmd_i32 p2,
                                  tmd_i32 p3,
                                  tmd_i32 t_fx12,
                                  tmd_i32 *out_value);

int tmd_hmd_cat3_curve_bspline_i32(tmd_i32 p0,
                                   tmd_i32 p1,
                                   tmd_i32 p2,
                                   tmd_i32 p3,
                                   tmd_i32 t_fx12,
                                   tmd_i32 *out_value);

#ifdef __cplusplus
}
#endif

#endif /* TMD_HMD_CAT3_PLAY_H */
