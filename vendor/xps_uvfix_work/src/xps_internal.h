#ifndef XPS_INTERNAL_H
#define XPS_INTERNAL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include "../include/xps.h"

#define XPS_MAGIC_NUMBER ((xps_u32)323232UL)
#define XPS_LIMIT_1BYTE 128U

int xps_internal_probe_fp(FILE *fp, const char *path, xps_probe_result *probe);
int xps_internal_detect_fp(FILE *fp, xps_model_info *model);

int xps_internal_parse_xps_header(FILE *fp,
                                  const xps_header_sink *sink,
                                  void *user,
                                  xps_header_info *header);

int xps_internal_parse_ascii(FILE *fp,
                             const xps_sink *sink,
                             void *user,
                             xps_model_info *model);
int xps_internal_parse_bin(FILE *fp,
                           const xps_sink *sink,
                           void *user,
                           xps_model_info *model);
int xps_internal_parse_pose(FILE *fp,
                            const xps_pose_sink *sink,
                            void *user,
                            xps_pose_info *pose);
int xps_internal_parse_bonedict(FILE *fp,
                                const xps_bonedict_sink *sink,
                                void *user,
                                xps_bonedict_info *dict);
int xps_internal_parse_scene(FILE *fp,
                             const xps_scene_sink *sink,
                             void *user,
                             xps_scene_info *scene);

int xps_internal_read_u8(FILE *fp, xps_u8 *out);
int xps_internal_read_i16le(FILE *fp, xps_i16 *out);
int xps_internal_read_u16le(FILE *fp, xps_u16 *out);
int xps_internal_read_u32le(FILE *fp, xps_u32 *out);
int xps_internal_read_f32fx(FILE *fp, xps_fx *out);
int xps_internal_read_lpstring(FILE *fp, char *dst, size_t cap);
int xps_internal_read_scene_string(FILE *fp, char *dst, size_t cap);
int xps_internal_skip(FILE *fp, xps_u32 count);

int xps_internal_fx_from_ascii(const char *text, xps_fx *out);
int xps_internal_f32_bits_to_fx(xps_u32 bits, xps_fx *out);

void xps_internal_copy_capped(char *dst, size_t cap, const char *src);
void xps_internal_zero_header_info(xps_header_info *header);
void xps_internal_header_flags_default(xps_header_flags *flags);
void xps_internal_zero_scan(xps_scan_result *scan);
int xps_internal_invoke_header_begin(const xps_header_sink *sink, void *user, const xps_header_info *header);
int xps_internal_invoke_header_option(const xps_header_sink *sink, void *user, const xps_header_option *option);
int xps_internal_invoke_header_flag_pair(const xps_header_sink *sink, void *user, const xps_header_flag_pair *pair);
int xps_internal_invoke_header_flags(const xps_header_sink *sink, void *user, const xps_header_flags *flags);
int xps_internal_invoke_header_default_pose_begin(const xps_header_sink *sink, void *user, const xps_header_pose_info *pose);
int xps_internal_invoke_header_default_pose_bone(const xps_header_sink *sink, void *user, const xps_pose_bone *bone);
int xps_internal_invoke_header_default_pose_end(const xps_header_sink *sink, void *user, const xps_header_pose_info *pose);
int xps_internal_invoke_header_end(const xps_header_sink *sink, void *user, const xps_header_info *header);

int xps_internal_invoke_model_begin(const xps_sink *sink, void *user, const xps_model_info *model);
int xps_internal_invoke_model_end(const xps_sink *sink, void *user, const xps_model_info *model);
int xps_internal_invoke_bone(const xps_sink *sink, void *user, const xps_bone *bone);
int xps_internal_invoke_mesh_begin(const xps_sink *sink, void *user, const xps_mesh_info *mesh);
int xps_internal_invoke_texture(const xps_sink *sink, void *user, xps_u32 mesh_index, const xps_texture *texture);
int xps_internal_invoke_vertex(const xps_sink *sink, void *user, xps_u32 mesh_index, const xps_vertex *vertex);
int xps_internal_invoke_face(const xps_sink *sink, void *user, xps_u32 mesh_index, const xps_face *face);
int xps_internal_invoke_mesh_end(const xps_sink *sink, void *user, const xps_mesh_info *mesh);
int xps_internal_invoke_pose_begin(const xps_pose_sink *sink, void *user, const xps_pose_info *pose);
int xps_internal_invoke_pose_bone(const xps_pose_sink *sink, void *user, const xps_pose_bone *bone);
int xps_internal_invoke_pose_end(const xps_pose_sink *sink, void *user, const xps_pose_info *pose);
int xps_internal_invoke_bonedict_begin(const xps_bonedict_sink *sink, void *user, const xps_bonedict_info *dict);
int xps_internal_invoke_bonedict_entry(const xps_bonedict_sink *sink, void *user, const xps_bonedict_entry *entry);
int xps_internal_invoke_bonedict_end(const xps_bonedict_sink *sink, void *user, const xps_bonedict_info *dict);
int xps_internal_invoke_scene_begin(const xps_scene_sink *sink, void *user, const xps_scene_info *scene);
int xps_internal_invoke_scene_item_begin(const xps_scene_sink *sink, void *user, const xps_scene_item *item);
int xps_internal_invoke_scene_item_bone(const xps_scene_sink *sink, void *user, const xps_scene_bone_pose *bone);
int xps_internal_invoke_scene_accessory(const xps_scene_sink *sink, void *user, const xps_scene_accessory *accessory);
int xps_internal_invoke_scene_item_end(const xps_scene_sink *sink, void *user, const xps_scene_item *item);
int xps_internal_invoke_scene_camera(const xps_scene_sink *sink, void *user, const xps_scene_camera *camera);
int xps_internal_invoke_scene_light(const xps_scene_sink *sink, void *user, const xps_scene_light *light);
int xps_internal_invoke_scene_post(const xps_scene_sink *sink, void *user, const xps_scene_post *post);
int xps_internal_invoke_scene_background(const xps_scene_sink *sink, void *user, const xps_scene_background *background);
int xps_internal_invoke_scene_sky(const xps_scene_sink *sink, void *user, const xps_scene_sky *sky);
int xps_internal_invoke_scene_window(const xps_scene_sink *sink, void *user, const xps_scene_window *window_info);
int xps_internal_invoke_scene_end(const xps_scene_sink *sink, void *user, const xps_scene_info *scene);

#endif
