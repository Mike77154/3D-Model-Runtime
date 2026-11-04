#ifndef MPK_READER_H
#define MPK_READER_H

#include <stdio.h>
#include "mpk_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void mpk_model_init(mpk_model *m);
int mpk_read_file(const char *path, mpk_model *out_model);
const char *mpk_chunk_name(mpk_u32 id);
void mpk_print_summary(FILE *out, const mpk_model *m);
int mpk_write_obj(const char *path, const mpk_model *m);
int mpk_write_posed_obj(const char *path, const mpk_model *m);
void mpk_fixed_to_string(mpk_fixed v, char *buf, int buf_size);
int mpk_build_resolved_nodes(mpk_model *m);
int mpk_apply_skin_to_bind_pose(mpk_model *m);
int mpk_find_animation(const mpk_model *m, const char *object_name);
int mpk_eval_animation_matrix(const mpk_model *m, mpk_u32 anim_index, mpk_u32 frame, mpk_matrix4x3 *out);
int mpk_eval_camera_path(const mpk_model *m, mpk_u32 camera_index, mpk_u32 frame, mpk_camera_pose *out);
const char *mpk_light_type_name(int type);

#ifdef __cplusplus
}
#endif

#endif
