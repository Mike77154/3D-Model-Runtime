#include "hlfp_timeline.h"
#include "hlfp_internal.h"
#include <limits.h>

/* Floor quotient and nonnegative remainder, even for negative times.
   Divisors are strictly positive. All operations fit signed C89 int. */
static void tl_divmod(hlfp_fixed value, hlfp_fixed divisor,
                      int *quotient, hlfp_fixed *remainder) {
    int q = value / divisor;
    hlfp_fixed r = value % divisor;
    if (r < 0) { --q; r += divisor; }
    *quotient = q;
    *remainder = r;
}

static int tl_compare(int aq, hlfp_fixed ar, int bq, hlfp_fixed br) {
    if (aq < bq) return -1;
    if (aq > bq) return 1;
    return (ar < br) ? -1 : (ar > br) ? 1 : 0;
}

int hlfp_timeline_seconds_to_frames(const hlfp_mdl *mdl,
                                    int sequence_index,
                                    hlfp_fixed seconds,
                                    hlfp_fixed *out_frames,
                                    hlfp_error *err) {
    int rc;
    if (mdl == 0 || out_frames == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null timeline seconds input");
    }
    if (sequence_index < 0 || sequence_index >= mdl->num_seqdesc) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "timeline sequence index invalid");
    }
    if (mdl->seqdesc[sequence_index].fps <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "timeline fps must be positive");
    }
    rc = hlfp_fixed_mul(seconds, mdl->seqdesc[sequence_index].fps, out_frames);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "timeline seconds conversion overflow");
    return HLFP_OK;
}

int hlfp_timeline_collect_frames(const hlfp_mdl *mdl,
                                 int sequence_index,
                                 hlfp_fixed previous_frame,
                                 hlfp_fixed current_frame,
                                 hlfp_timeline_hit *hits,
                                 int capacity,
                                 int *out_count,
                                 hlfp_error *err) {
    const hlfp_mdl_seqdesc *seq;
    int base, count, i, j, forward, loop;
    hlfp_fixed period, last, start_rem, end_rem;
    int start_cycle, end_cycle;
    int active[HLFP_MDL_MAX_EVENTS];
    int cycles[HLFP_MDL_MAX_EVENTS];
    hlfp_fixed phases[HLFP_MDL_MAX_EVENTS];
    int emitted;

    if (mdl == 0 || out_count == 0 || capacity < 0 ||
        (capacity > 0 && hits == 0)) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "invalid timeline collection buffers");
    }
    *out_count = 0;
    hlfp_error_clear(err);
    if (sequence_index < 0 || sequence_index >= mdl->num_seqdesc) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "timeline sequence index invalid");
    }
    seq = &mdl->seqdesc[sequence_index];
    if (seq->numframes <= 0 || seq->numframes > INT_MAX / HLFP_FIXED_ONE) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "timeline sequence frame count invalid");
    }
    /* MDL parsing stores event tables concatenated in sequence order. */
    base = 0;
    for (i = 0; i < sequence_index; ++i) {
        if (mdl->seqdesc[i].numevents < 0 ||
            mdl->seqdesc[i].numevents > mdl->num_events - base) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "timeline event table inconsistent");
        }
        base += mdl->seqdesc[i].numevents;
    }
    count = seq->numevents;
    if (count < 0 || count > HLFP_MDL_MAX_EVENTS || base > mdl->num_events ||
        count > mdl->num_events - base) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "timeline sequence event count inconsistent");
    }
    if (count == 0 || previous_frame == current_frame) return HLFP_OK;

    forward = current_frame > previous_frame;
    loop = ((seq->flags & HLFP_STUDIO_LOOPING) != 0 && seq->numframes > 1);
    period = (hlfp_fixed)(seq->numframes - 1) * HLFP_FIXED_ONE;
    last = (hlfp_fixed)(seq->numframes - 1) * HLFP_FIXED_ONE;
    if (loop) {
        tl_divmod(previous_frame, period, &start_cycle, &start_rem);
        tl_divmod(current_frame, period, &end_cycle, &end_rem);
    } else {
        start_cycle = 0;
        end_cycle = 0;
        start_rem = previous_frame;
        end_rem = current_frame;
    }

    for (i = 0; i < count; ++i) {
        hlfp_fixed event_pos;
        int q;
        active[i] = 0;
        if (mdl->events[base+i].frame < 0 ||
            mdl->events[base+i].frame >= seq->numframes) continue;
        event_pos = (hlfp_fixed)mdl->events[base+i].frame * HLFP_FIXED_ONE;
        if (!loop) {
            /* No wrapping. An interval entirely outside the sequence
               naturally emits no events. */
            if (forward ? (event_pos > previous_frame && event_pos <= current_frame)
                        : (event_pos < previous_frame && event_pos >= current_frame)) {
                active[i] = 1;
                phases[i] = event_pos;
                cycles[i] = 0;
            }
        } else {
            /* Event on the last key aliases phase zero of the NEXT cycle.
               Store the canonical cycle for ordering; convert back on emit. */
            if (event_pos == last) event_pos = 0;
            q = start_cycle;
            if (forward) {
                if (event_pos <= start_rem) ++q;
            } else {
                if (event_pos >= start_rem) --q;
            }
            if (forward ? (tl_compare(q,event_pos,end_cycle,end_rem)<=0)
                        : (tl_compare(q,event_pos,end_cycle,end_rem)>=0)) {
                active[i] = 1;
                phases[i] = event_pos;
                cycles[i] = q;
            }
        }
    }

    emitted = 0;
    for (;;) {
        int pick=-1;
        for (i=0; i<count; ++i) {
            if (!active[i]) continue;
            if (pick < 0 ||
                (forward ? tl_compare(cycles[i],phases[i],cycles[pick],phases[pick]) < 0
                         : tl_compare(cycles[i],phases[i],cycles[pick],phases[pick]) > 0)) {
                pick=i;
            }
        }
        if (pick < 0) break;
        if (emitted == capacity) {
            *out_count = emitted;
            return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "timeline hit buffer full");
        }
        hits[emitted].sequence_index = sequence_index;
        hits[emitted].event_index = base+pick;
        hits[emitted].frame = mdl->events[base+pick].frame;
        hits[emitted].cycle = cycles[pick] - ((loop && hits[emitted].frame == seq->numframes-1)?1:0);
        ++emitted;
        if (!loop) {
            active[pick] = 0;
        } else {
            /* Adjust one event's next candidate. Others at the same tick
               remain active, preserving stable source order. */
            if (forward) {
                if (cycles[pick] == INT_MAX) {active[pick]=0;continue;}
                ++cycles[pick];
            } else {
                if (cycles[pick] == INT_MIN) {active[pick]=0;continue;}
                --cycles[pick];
            }
            j = tl_compare(cycles[pick],phases[pick],end_cycle,end_rem);
            if (forward ? (j > 0) : (j < 0)) active[pick]=0;
        }
    }
    *out_count = emitted;
    return HLFP_OK;
}

int hlfp_timeline_collect_seconds(const hlfp_mdl *mdl,
                                  int sequence_index,
                                  hlfp_fixed previous_seconds,
                                  hlfp_fixed current_seconds,
                                  hlfp_timeline_hit *hits,
                                  int capacity,
                                  int *out_count,
                                  hlfp_error *err) {
    hlfp_fixed start, end;
    int rc;
    rc = hlfp_timeline_seconds_to_frames(mdl,sequence_index,previous_seconds,&start,err);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_timeline_seconds_to_frames(mdl,sequence_index,current_seconds,&end,err);
    if (rc != HLFP_OK) return rc;
    return hlfp_timeline_collect_frames(mdl,sequence_index,start,end,hits,capacity,out_count,err);
}
