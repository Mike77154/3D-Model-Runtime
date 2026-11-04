#ifndef RE4_COVERAGE_H
#define RE4_COVERAGE_H
#include "re4_model.h"
#include "re4_animation.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Purely structural matching. Distinguish usable rotations from merely
   decodable FCV curves: no IK or semantic clip-name inference. */
typedef struct re4_clip_coverage_s {
    re4_u32 skeleton_bones;
    re4_u32 name_mapped_bones;
    re4_u32 fcv_channels;
    re4_u32 rotation_channels;
    re4_u32 linked_rotation_channels;
    re4_u32 unmatched_rotation_channels;
    re4_u32 special_channels;
    re4_u32 unique_linked_bones;
} re4_clip_coverage;
re4_err re4_character_clip_coverage(const re4_pmd_model *model,
    const re4_fcv *clip,re4_clip_coverage *out);
#ifdef __cplusplus
}
#endif
#endif
