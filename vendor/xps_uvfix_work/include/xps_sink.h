#ifndef XPS_SINK_H
#define XPS_SINK_H

#include "xps_types.h"

typedef struct xps_header_sink {
    int (*on_header_begin)(void *user, const xps_header_info *header);
    int (*on_header_option)(void *user, const xps_header_option *option);
    int (*on_header_flag_pair)(void *user, const xps_header_flag_pair *pair);
    int (*on_header_flags)(void *user, const xps_header_flags *flags);
    int (*on_header_default_pose_begin)(void *user, const xps_header_pose_info *pose);
    int (*on_header_default_pose_bone)(void *user, const xps_pose_bone *bone);
    int (*on_header_default_pose_end)(void *user, const xps_header_pose_info *pose);
    int (*on_header_end)(void *user, const xps_header_info *header);
} xps_header_sink;

typedef struct xps_sink {
    int (*on_model_begin)(void *user, const xps_model_info *model);
    int (*on_bone)(void *user, const xps_bone *bone);
    int (*on_mesh_begin)(void *user, const xps_mesh_info *mesh);
    int (*on_texture)(void *user, xps_u32 mesh_index, const xps_texture *texture);
    int (*on_vertex)(void *user, xps_u32 mesh_index, const xps_vertex *vertex);
    int (*on_face)(void *user, xps_u32 mesh_index, const xps_face *face);
    int (*on_mesh_end)(void *user, const xps_mesh_info *mesh);
    int (*on_model_end)(void *user, const xps_model_info *model);
} xps_sink;

typedef struct xps_pose_sink {
    int (*on_pose_begin)(void *user, const xps_pose_info *pose);
    int (*on_pose_bone)(void *user, const xps_pose_bone *bone);
    int (*on_pose_end)(void *user, const xps_pose_info *pose);
} xps_pose_sink;

typedef struct xps_bonedict_sink {
    int (*on_bonedict_begin)(void *user, const xps_bonedict_info *dict);
    int (*on_bonedict_entry)(void *user, const xps_bonedict_entry *entry);
    int (*on_bonedict_end)(void *user, const xps_bonedict_info *dict);
} xps_bonedict_sink;

typedef struct xps_scene_sink {
    int (*on_scene_begin)(void *user, const xps_scene_info *scene);
    int (*on_scene_item_begin)(void *user, const xps_scene_item *item);
    int (*on_scene_item_bone)(void *user, const xps_scene_bone_pose *bone);
    int (*on_scene_accessory)(void *user, const xps_scene_accessory *accessory);
    int (*on_scene_item_end)(void *user, const xps_scene_item *item);
    int (*on_scene_camera)(void *user, const xps_scene_camera *camera);
    int (*on_scene_light)(void *user, const xps_scene_light *light);
    int (*on_scene_post)(void *user, const xps_scene_post *post);
    int (*on_scene_background)(void *user, const xps_scene_background *background);
    int (*on_scene_sky)(void *user, const xps_scene_sky *sky);
    int (*on_scene_window)(void *user, const xps_scene_window *window_info);
    int (*on_scene_end)(void *user, const xps_scene_info *scene);
} xps_scene_sink;

#endif
