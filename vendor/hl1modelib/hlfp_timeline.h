#ifndef HLFP_TIMELINE_H
#define HLFP_TIMELINE_H

#include "hlfp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GoldSrc animation event dispatcher (additive, no heap, fixed point).
 * Event positions use unwrapped 16.16 frames, NOT normalized frames.
 * Starting position is EXCLUSIVE; ending position is INCLUSIVE.
 * Reverse playback emits hits in reverse-time order; equal-time hits
 * preserve their order in the MDL event table.
 *
 * GoldSrc looping sequences have a period of numframes-1 ticks.
 * A key on the last frame shares its timestamp with the next cycle's
 * frame zero; both entries remain distinct events.
 *
 * The caller owns hits[]. If the interval contains more events than
 * capacity, returns HLFP_ERR_CAPACITY, writes the first capacity hits,
 * and puts capacity into out_count. Never performs partial hidden writes.
 */
typedef struct hlfp_timeline_hit_s {
    int sequence_index;
    int event_index; /* index of mdl->events[] */
    int cycle;       /* cycle in which the original event key was recorded */
    int frame;       /* original 0-based frame from the MDL */
} hlfp_timeline_hit;

int hlfp_timeline_seconds_to_frames(const hlfp_mdl *mdl,
                                    int sequence_index,
                                    hlfp_fixed seconds,
                                    hlfp_fixed *out_frames,
                                    hlfp_error *err);

int hlfp_timeline_collect_frames(const hlfp_mdl *mdl,
                                 int sequence_index,
                                 hlfp_fixed previous_frame,
                                 hlfp_fixed current_frame,
                                 hlfp_timeline_hit *hits,
                                 int capacity,
                                 int *out_count,
                                 hlfp_error *err);

int hlfp_timeline_collect_seconds(const hlfp_mdl *mdl,
                                  int sequence_index,
                                  hlfp_fixed previous_seconds,
                                  hlfp_fixed current_seconds,
                                  hlfp_timeline_hit *hits,
                                  int capacity,
                                  int *out_count,
                                  hlfp_error *err);

#ifdef __cplusplus
}
#endif

#endif
