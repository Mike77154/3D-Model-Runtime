#ifndef MODELRT_RE4SEQ89_H
#define MODELRT_RE4SEQ89_H
#include "model_runtime89.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Native RE4 SEQ raw keys: Q6 = 64 units per authored FCV frame. */
typedef struct modelrt_re4_seq_key_s {
    modelrt_u8 code,auxiliary;
    modelrt_u16 time_q6;
} modelrt_re4_seq_key;
#define MODELRT_RE4_SEQ_SHOW 1u
#define MODELRT_RE4_SEQ_HIDE 2u
#define MODELRT_RE4_SEQ_POSE 3u
#define MODELRT_RE4_SEQ_PARENT 4u
#define MODELRT_RE4_SEQ_UNPARENT 5u
typedef struct modelrt_re4_seq_rule_s {
    modelrt_u8 code,auxiliary,match_auxiliary,action,target,value;
} modelrt_re4_seq_rule;
typedef struct modelrt_re4_seq_visual_state_s {
    modelrt_u32 visible_mask,cursor_q6;
    modelrt_u8 pose_id,parent[32];
    modelrt_u32 applied_events,unmapped_events;
} modelrt_re4_seq_visual_state;
/* Does not apply effects to meshes automatically. No assumptions about
   original game event codes; only caller-specified rules are applied.
   duration_q6 is explicitly supplied by the caller and must be verified
   against the intended FCV/SEQ pair. */
int modelrt_re4_seq_key_count(const modelrt_handle *handle,modelrt_u32 *out_count);
int modelrt_re4_seq_key_at(const modelrt_handle *handle,modelrt_u32 index,
                          modelrt_re4_seq_key *out);
int modelrt_re4_seq_seek(const modelrt_handle *handle,
    modelrt_u32 duration_q6,modelrt_u32 to_q6,modelrt_u32 max_loops,
    const modelrt_re4_seq_rule *rules,modelrt_u32 rule_count,int strict_unknown,
    modelrt_u32 initial_visible,modelrt_u8 initial_pose,
    modelrt_re4_seq_visual_state *out);
#ifdef __cplusplus
}
#endif
#endif
