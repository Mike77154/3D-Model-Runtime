#ifndef RE4_SEQ_STATE_H
#define RE4_SEQ_STATE_H
/* Scene-state adapter for RE4 SEQ. Actions must be EXPLICITLY supplied by
   caller (or mod author); NO native meaning is inferred from event bytes.
   C89, bounded 32-part mask, no heap, fixed caller-owned structures. */
#include "re4_animation.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RE4_SEQ_STATE_PARTS 32U
#define RE4_SEQ_PARENT_NONE 255U
#define RE4_SEQ_ACTION_SHOW 1U
#define RE4_SEQ_ACTION_HIDE 2U
#define RE4_SEQ_ACTION_SET_POSE 3U
#define RE4_SEQ_ACTION_SET_PARENT 4U
#define RE4_SEQ_ACTION_CLEAR_PARENT 5U

typedef struct re4_seq_state_rule_s {
    re4_u8 event_code;  /* Nonzero raw SEQ event byte */
    re4_u8 auxiliary;   /* Match exactly if match_auxiliary is true */
    re4_u8 match_auxiliary;
    re4_u8 action;
    re4_u8 target;      /* 0..31 for SHOW/HIDE/SET_PARENT/CLEAR_PARENT */
    re4_u8 value;       /* Pose ID or parent ID (0..31) */
} re4_seq_state_rule;

typedef struct re4_seq_state_s {
    re4_u32 visible_mask;
    re4_u32 cursor_q6;
    re4_u8 pose_id;
    re4_u8 parent[RE4_SEQ_STATE_PARTS];
    re4_u8 initialized;
} re4_seq_state;

typedef struct re4_seq_state_stats_s {
    re4_u32 raw_nonzero;
    re4_u32 applied;
    re4_u32 unmapped;
    re4_u32 last_absolute_q6;
} re4_seq_state_stats;

void re4_seq_state_init(re4_seq_state *state, re4_u32 visible_mask,
                        re4_u8 pose_id);
re4_err re4_seq_state_rules_validate(const re4_seq_state_rule *rules,
                                     re4_u32 rule_count);
/* Atomic state transition: malformed / unknown (strict) events never change
   caller's current state or clock. Requires from_q6 == state.cursor_q6.
   Codes == 0 are never interpreted as actions. */
re4_err re4_seq_state_advance(const re4_seq_timeline *seq,
                              re4_u32 clip_period_q6,
                              re4_u32 from_q6,re4_u32 to_q6,
                              re4_u32 max_loops,
                              const re4_seq_state_rule *rules,
                              re4_u32 rule_count,re4_bool strict_unknown,
                              re4_seq_state *state,
                              re4_seq_state_stats *stats);
/* Reconstruct state reproducibly by replaying from initial pose & visibility;
   permits seeking backwards without persisting any implicit engine state. */
re4_err re4_seq_state_seek(const re4_seq_timeline *seq,
                           re4_u32 clip_period_q6,re4_u32 to_q6,
                           re4_u32 max_loops,
                           const re4_seq_state_rule *rules,
                           re4_u32 rule_count,re4_bool strict_unknown,
                           re4_u32 initial_visible,re4_u8 initial_pose,
                           re4_seq_state *out,re4_seq_state_stats *stats);
#ifdef __cplusplus
}
#endif
#endif
