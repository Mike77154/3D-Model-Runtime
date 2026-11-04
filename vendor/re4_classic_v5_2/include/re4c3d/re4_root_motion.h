#ifndef RE4_ROOT_MOTION_H
#define RE4_ROOT_MOTION_H
#include "re4_skeletal.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Observed FCV kind-1 position channel: its coordinate basis and scaling
   are NOT assumed equivalent to the game's complete locomotion system.
   The caller explicitly opts in to translation of the skinned scene. */
typedef struct re4_root_track_s {
    re4_bool present;
    re4_u32 channel_index;
} re4_root_track;
/* Finds unambiguous primary root (bone_no == 0); no fallback guessing. */
re4_err re4_root_find(const re4_fcv *fcv, re4_root_track *out);
re4_err re4_root_sample(re4_span bytes, const re4_fcv *fcv,
    const re4_root_track *track, re4_u16 frame, re4_q16 xyz[3]);
/* Relative translation at frame minus origin frame. Returns zero if absent.
   Errors on signed 32-bit overflow rather than wrapping. */
re4_err re4_root_relative(re4_span bytes, const re4_fcv *fcv,
    const re4_root_track *track, re4_u16 origin_frame,
    re4_u16 frame, re4_q16 offset[3]);
/* Blend relative root travel from independent clips. The coordinate basis
   is still experimental. weight_b is Q16.16 in [0,65536]. */
re4_err re4_root_blend_relative(re4_span sa,const re4_fcv *fa,
    const re4_root_track *ta,re4_u16 oa,re4_u16 at,
    re4_span sb,const re4_fcv *fb,
    const re4_root_track *tb,re4_u16 ob,re4_u16 bt,
    re4_q16 weight_b,re4_q16 xyz[3]);
/* Translate all world matrices, preserving their rotations and all part
   attachments when called on MASTER before multipart retargeting. */
re4_err re4_root_translate(re4_skel_pose *pose, const re4_q16 offset[3]);
#ifdef __cplusplus
}
#endif
#endif
