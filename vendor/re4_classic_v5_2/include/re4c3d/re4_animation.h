#ifndef RE4_ANIMATION_H
#define RE4_ANIMATION_H

#include "re4_types.h"
#include "re4_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct re4_fcv_block_s {
    re4_u8 bone_type;
    re4_u8 data_type;
    re4_u8 bone_no;
    re4_u8 value_bytes_per_key;
    re4_u8 axis_value_size[3];
    re4_u32 data_offset;
} re4_fcv_block;

typedef struct re4_fcv_s {
    re4_u16 max_time;
    re4_u8 frame_flags; /* original u16 high two flag bits */
    re4_u8 block_count;
    re4_u32 declared_file_size;
    re4_fcv_block *blocks;
    re4_endian endian;
} re4_fcv;

typedef struct re4_fcv_axis_view_s {
    re4_u16 tag_count;
    re4_u8 value_size;
    re4_u8 tangent_in_size;
    re4_u8 tangent_out_size;
    re4_u8 record_size;
    re4_span time_shifts;
    re4_span values;
} re4_fcv_axis_view;

typedef struct re4_seq_key_s {
    re4_u16 frame_q6; /* SEQ bytes 2..3: 10.6 fixed, 64 = 1 frame */
    re4_u8 se;       /* byte 0: event code (native semantic unverified) */
    re4_u8 flags;    /* byte 1: auxiliary event data, not time */
} re4_seq_key;
typedef struct re4_seq_timeline_s {
    re4_u16 count;
    re4_u8 flags;
    re4_endian endian;
    re4_span data;
} re4_seq_timeline;
re4_err re4_seq_timeline_read(re4_span src,re4_endian endian,re4_seq_timeline *out);
re4_err re4_seq_timeline_key_at(const re4_seq_timeline *seq,re4_u32 index,re4_seq_key *out);
typedef struct re4_seq_observation_s {
    re4_u32 count;
    re4_u16 first_q6, last_q6;
    re4_u32 nonzero_events, nonzero_auxiliary, same_time_pairs;
} re4_seq_observation;
re4_err re4_seq_timeline_inspect(const re4_seq_timeline *seq,
                                re4_seq_observation *out);
re4_err re4_seq_timeline_window(const re4_seq_timeline *seq,
                                re4_u16 start_q6,re4_u16 end_q6,
                                re4_u32 *out_first,re4_u32 *out_count);
/* v5.0: raw event scheduler for a verified, explicitly chosen SEQ.
   The caller must select its FCV/SEQ relationship; file adjacency is NOT proof.
   Time is 10.6 fixed, unsigned unwrapped for deterministic looping.
   Dispatches keys in stable order within [from_q6,to_q6), including across
   loop boundaries; never interprets native event codes. No allocations. */
typedef struct re4_seq_fire_s {
    re4_u32 absolute_q6;  /* unwrapped global time */
    re4_u32 loop_index;   /* zero-based repeat number */
    re4_u32 key_index;    /* original SEQ order, stable across ties */
    re4_seq_key key;
} re4_seq_fire;
typedef re4_err (*re4_seq_fire_fn)(void *user,const re4_seq_fire *event);
re4_err re4_seq_dispatch_range(const re4_seq_timeline *seq,
                               re4_u32 clip_duration_q6,
                               re4_u32 from_q6,re4_u32 to_q6,
                               re4_u32 max_loops,re4_bool include_zero,
                               re4_seq_fire_fn fire,void *user,
                               re4_u32 *out_fired);

typedef struct re4_seq_s {
    re4_u32 file_size;
    re4_u32 possible_command_count;
    re4_u32 first_word;
} re4_seq;

typedef struct re4_mot_ref_s {
    re4_strref model_name;
    re4_strref motion_name;
} re4_mot_ref;

re4_u8 re4_fcv_value_tuple_size(re4_u8 data_type);
re4_u8 re4_fcv_axis_value_size(re4_u8 data_type, re4_u32 axis);
const char *re4_fcv_bone_type_name(re4_u8 bone_type);
const char *re4_fcv_scalar_name(re4_u8 value_size);
re4_err re4_fcv_read(re4_span src, re4_endian endian, re4_arena *arena, re4_fcv *out);
/* Endian-aware variants for original GameCube and later big-endian FCV. */
re4_err re4_fcv_block_axis_views_e(re4_span src, const re4_fcv_block *block,
                                   re4_endian endian, re4_fcv_axis_view *x,
                                   re4_fcv_axis_view *y, re4_fcv_axis_view *z);
re4_err re4_fcv_axis_time_at_e(const re4_fcv_axis_view *axis, re4_u32 index,
                               re4_endian endian, re4_u16 *out_time);
re4_err re4_fcv_block_axis_views(re4_span src, const re4_fcv_block *block,
                                 re4_fcv_axis_view *x, re4_fcv_axis_view *y, re4_fcv_axis_view *z);
re4_err re4_fcv_axis_value_q16(const re4_fcv_axis_view *axis, re4_u32 index,
                               re4_endian endian, re4_q16 *out_value);
re4_err re4_fcv_axis_time_at(const re4_fcv_axis_view *axis, re4_u32 index, re4_u16 *out_time);
/* Inspect every original frame/value/tangent, without resampling. */
re4_err re4_fcv_axis_key_q16(const re4_fcv_axis_view *axis, re4_u32 index,
                             re4_endian endian, re4_q16 out_value_in_out[3]);
/* Bit set of union of original keyframe times across all channels/axes.
   Caller supplies 8192 bytes (65536 possible u16 frames), no heap. */
re4_err re4_fcv_keyframe_union(re4_span src, const re4_fcv *fcv,
                               re4_u8 *time_bits, re4_size bits_capacity,
                               re4_u32 *out_distinct_times);
/* Role-preserving sampler for special channels and rotation channels alike.
   Interpretation of role 4/16/32/160 must be supplied by game IK runtime. */
re4_err re4_fcv_block_sample_hermite_q16(re4_span src, const re4_fcv_block *block,
                                         re4_u16 time, re4_endian endian,
                                         re4_q16 xyz[3]);
re4_err re4_fcv_axis_sample_nearest_q16(const re4_fcv_axis_view *axis, re4_u16 time,
                                        re4_endian endian, re4_q16 *out_value, re4_u32 *out_index);
re4_err re4_fcv_block_sample_nearest_q16(re4_span src, const re4_fcv_block *block, re4_u16 time,
                                         re4_endian endian, re4_q16 out_xyz[3]);
re4_err re4_fcv_validate(re4_span src, const re4_fcv *fcv);
re4_err re4_fcv_axis_sample_hermite_q16(const re4_fcv_axis_view *axis, re4_u16 time, re4_endian endian, re4_q16 *out);
re4_err re4_seq_probe_read(re4_span src, re4_endian endian, re4_seq *out);
re4_err re4_mot_ref_from_payload(re4_span payload, re4_mot_ref *out);

#ifdef __cplusplus
}
#endif

#endif
