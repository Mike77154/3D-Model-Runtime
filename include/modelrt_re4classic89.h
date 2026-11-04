#ifndef MODELRT_RE4CLASSIC89_H
#define MODELRT_RE4CLASSIC89_H
#include "model_runtime89.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Standalone FCV channel inspection. No guessed FPS or skeleton retargeting.
 * Time is an authored FCV frame/tick (not Q16 seconds).
 * These functions do not allocate and require an open RE4 .fcv handle.
 */
typedef struct modelrt_re4_fcv_track_s {
    modelrt_u32 index;
    modelrt_u32 bone_no;
    modelrt_u32 bone_type;
    modelrt_u32 data_type;
} modelrt_re4_fcv_track;
int modelrt_re4_fcv_track_count(const modelrt_handle *h,modelrt_u32 *out_count);
int modelrt_re4_fcv_track_info(const modelrt_handle *h,modelrt_u32 index,
                              modelrt_re4_fcv_track *out);
int modelrt_re4_fcv_sample(const modelrt_handle *h,modelrt_u32 track_index,
                           modelrt_u32 native_tick,int hermite,modelrt_vec3 *out_xyz);
/* Attach an independently opened .fcv using modelrt_animation_bind().
 * Then visit a native authored FCV frame; this is not seconds or guessed FPS.
 * The target/source handles and binding scratch must remain alive.
 * Bone rotations use native RE4 v5.2 FK + weighted/rigid skinning.
 * Unresolved IK/root motion remain unsupported.
 */
int modelrt_re4_visit_fcv_frame(modelrt_animation_binding *binding,
                               modelrt_u32 native_tick,
                               const modelrt_visit_sink *sink,void *user);
/* Explicit full-character PMD assembly, strict audited native bone mapping.
 * parts must list the corresponding open PMD handles in rendering order.
 * scratch is a caller-owned temporary arena for two part poses (~96 bytes/bone).
 * A mismatched hierarchy refuses assembly rather than inventing attachments.
 */
int modelrt_re4_visit_character_frame(modelrt_animation_binding *binding,
    modelrt_handle *const *parts,modelrt_u32 part_count,modelrt_u32 native_tick,
    void *scratch,size_t scratch_size,
    const modelrt_visit_sink *sink,void *user);
/* Composite material/texture index to its originating PMD handle and index. */
int modelrt_re4_character_texture_source(modelrt_handle *master,
    modelrt_handle *const *parts,modelrt_u32 part_count,modelrt_u32 global_index,
    modelrt_handle **out_handle,modelrt_u32 *local_index);
#ifdef __cplusplus
}
#endif
#endif
