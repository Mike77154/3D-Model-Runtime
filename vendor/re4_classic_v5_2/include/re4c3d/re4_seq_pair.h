#ifndef RE4_SEQ_PAIR_H
#define RE4_SEQ_PAIR_H
#include "re4_animation.h"
/* RE4 PC 2007 FCV/SEQ timing evidence, not proof of native pairing.
   Fixed-point only; caller-owned arena; no heap, no file-system dependency. */
#ifdef __cplusplus
extern "C" {
#endif
typedef enum re4_pair_timing_e {
    RE4_PAIR_EXACT_END = 1,        /* SEQ last key == FCV final frame * 64 */
    RE4_PAIR_WITHIN_CLIP = 2,      /* key stays within FCV, ends earlier */
    RE4_PAIR_SEQ_OVERRUN = 3,      /* SEQ key beyond FCV final frame */
    RE4_PAIR_DISPATCH_TOO_LONG = 4 /* period exceeds existing event scheduler */
} re4_pair_timing;
typedef struct re4_seq_pair_report_s {
    re4_u16 fcv_last_frame;
    re4_u16 seq_last_q6;
    re4_u32 fcv_end_q6;
    re4_u32 loop_period_q6;
    re4_u32 seq_keys;
    re4_u32 nonzero_codes;
    re4_u32 fcv_fingerprint;
    re4_u32 seq_fingerprint;
    re4_pair_timing timing;
} re4_seq_pair_report;
/* FNV-1a 32: an accidental-mismatch fingerprint, NOT a secure identity. */
re4_u32 re4_seq_pair_fingerprint(re4_span s);
re4_err re4_seq_pair_probe(re4_span fcv_raw,re4_span seq_raw,
                            re4_arena *arena,re4_seq_pair_report *out);
/* Event dispatch only with explicitly authorized file fingerprints. Rejects
   mismatched files, unsupported durations and timing-overrun sequences.
   RE4_PAIR_WITHIN_CLIP requires caller opt-in. No native event semantics. */
re4_err re4_seq_pair_dispatch(re4_span fcv_raw,re4_span seq_raw,
                              re4_arena *arena,
                              re4_u32 expected_fcv_fingerprint,
                              re4_u32 expected_seq_fingerprint,
                              re4_bool allow_early_seq_end,
                              re4_u32 from_q6,re4_u32 to_q6,
                              re4_u32 max_loops,re4_bool include_zero,
                              re4_seq_fire_fn fire,void *user,
                              re4_u32 *out_fired);
#ifdef __cplusplus
}
#endif
#endif
