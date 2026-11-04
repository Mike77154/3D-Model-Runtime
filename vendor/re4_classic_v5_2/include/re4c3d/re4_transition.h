#ifndef RE4_TRANSITION_H
#define RE4_TRANSITION_H
#include "re4_types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* C89, Q16.16: envelope for explicit A->B transitions.
   Inclusive start and end. Outside, return 0 and 65536 respectively.
   All calculations are deterministic and avoid dynamic allocations/floats. */
#define RE4_TRANSITION_LINEAR 0U
#define RE4_TRANSITION_SMOOTHSTEP 1U
#define RE4_TRANSITION_SMOOTHERSTEP 2U
typedef struct re4_transition_s {
    re4_u16 first;
    re4_u16 last;
    re4_u8 curve;
} re4_transition;
re4_err re4_transition_weight(const re4_transition *transition,
    re4_u16 frame, re4_q16 *out_weight);
#ifdef __cplusplus
}
#endif
#endif
