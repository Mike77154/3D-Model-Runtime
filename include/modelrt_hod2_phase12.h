#ifndef MODELRT_HOD2_PHASE12_H
#define MODELRT_HOD2_PHASE12_H
/* Phase12 stand-alone H2TB inspection API. C89, no heap and no floats.
 * The caller retains and owns the modelrt_handle workspace.  A .h2tb pack
 * contains tables for multiple character types but no POL/TEX/MOT resources.
 */
#include "model_runtime89.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct modelrt_hod2_part_info_s {
 modelrt_u32 slot;
 modelrt_i32 draw_bone;
 modelrt_i32 group_bone[4];
 modelrt_u32 source_vertices[4];
 modelrt_u32 row_count;
 modelrt_u32 draw_mode; /* 0 = unrecognized, 1=group0, 2=group2, 3=all */
} modelrt_hod2_part_info;

/* These functions accept only an open MODELRT_CODEC_HOD2 .h2tb handle.
 * All return MODELRT_ERR_BAD_ARG if the handle is not a tablepack. */
int modelrt_hod2_table_profile(const modelrt_handle *handle,modelrt_u32 *profile);
/* Select a type 0..85. Updates bone_count and the on_bone visitor output.
 * No geometry or skinning is claimed. */
int modelrt_hod2_select_character(modelrt_handle *handle,modelrt_u32 type);
int modelrt_hod2_extra_part_count(const modelrt_handle *handle,modelrt_u32 type,modelrt_u32 *count);
int modelrt_hod2_extra_part_get(const modelrt_handle *handle,modelrt_u32 type,
                               modelrt_u32 part_index,modelrt_hod2_part_info *part);
#ifdef __cplusplus
}
#endif
#endif
