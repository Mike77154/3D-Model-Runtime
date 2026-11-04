#ifndef XPS_TYPES_H
#define XPS_TYPES_H

#include <limits.h>
#include <stddef.h>
#include "xps_config.h"

typedef signed char xps_i8;
typedef unsigned char xps_u8;
typedef signed short xps_i16;
typedef unsigned short xps_u16;
typedef signed int xps_i32;
typedef unsigned int xps_u32;
typedef xps_i32 xps_fx;

typedef char xps_chk_u8[(sizeof(xps_u8) == 1) ? 1 : -1];
typedef char xps_chk_u16[(sizeof(xps_u16) == 2) ? 1 : -1];
typedef char xps_chk_u32[(sizeof(xps_u32) == 4) ? 1 : -1];
typedef char xps_chk_fx[(sizeof(xps_fx) == 4) ? 1 : -1];

enum xps_format {
    XPS_FMT_UNKNOWN = 0,
    XPS_FMT_MESH_ASCII = 1,
    XPS_FMT_MESH_BIN = 2,
    XPS_FMT_XPS = 3,
    XPS_FMT_POSE = 4,
    XPS_FMT_BONEDICT = 5,
    XPS_FMT_SCENE = 6
};

enum xps_error {
    XPS_OK = 0,
    XPS_ERR_OPEN = 1,
    XPS_ERR_IO = 2,
    XPS_ERR_FORMAT = 3,
    XPS_ERR_LIMIT = 4,
    XPS_ERR_PARSE = 5,
    XPS_ERR_UNSUPPORTED = 6,
    XPS_ERR_CALLBACK = 7,
    XPS_ERR_OVERFLOW = 8,
    XPS_ERR_LINE_TOO_LONG = 9,
    XPS_ERR_BAD_ARGUMENT = 10
};

enum xps_scene_accessory_kind {
    XPS_SCENE_ACCESSORY_PRIMARY = 1,
    XPS_SCENE_ACCESSORY_SECONDARY = 2
};

enum xps_header_settings_kind {
    XPS_HEADER_SETTINGS_NONE = 0,
    XPS_HEADER_SETTINGS_LEGACY_RAW = 1,
    XPS_HEADER_SETTINGS_TYPED = 2
};

enum xps_header_option_kind {
    XPS_HEADER_OPTION_NONE = 0,
    XPS_HEADER_OPTION_DEFAULT_POSE = 1,
    XPS_HEADER_OPTION_FLAGS = 2,
    XPS_HEADER_OPTION_UNKNOWN = 255
};

typedef struct xps_header_info {
    enum xps_format format;
    xps_u32 magic_number;
    xps_u16 version_major;
    xps_u16 version_minor;
    char xna_aral[XPS_CFG_MAX_NAME];
    xps_u32 settings_len_words;
    xps_u32 settings_len_bytes;
    char machine[XPS_CFG_MAX_NAME];
    char user[XPS_CFG_MAX_NAME];
    char files[XPS_CFG_MAX_PATH];
    xps_u8 has_settings;
    xps_u8 settings_kind;
    xps_u32 settings_hash;
    xps_u32 settings_item_count;
    xps_u32 default_pose_declared_bones;
    xps_u32 default_pose_parsed_bones;
    xps_u32 default_pose_bytes;
    xps_u32 flags_pair_count;
    xps_u32 unknown_option_count;
} xps_header_info;

typedef struct xps_header_option {
    xps_u32 index;
    xps_u32 type_raw;
    xps_u32 count;
    xps_u32 info;
    xps_u32 payload_bytes;
    xps_u8 known_kind;
} xps_header_option;

typedef struct xps_header_flag_pair {
    xps_u32 index;
    xps_u32 flag_id;
    xps_u32 raw_value;
} xps_header_flag_pair;

typedef struct xps_header_flags {
    xps_u8 present;
    xps_u8 back_face_culling;
    xps_u8 always_force_culling;
    xps_u8 model_cast_shadows;
    xps_u8 tangent_space_red;
    xps_u8 tangent_space_green;
    xps_u8 tangent_space_blue;
    xps_u32 gloss_raw;
    xps_u8 gloss_is_nonzero;
    xps_u8 has_bone_directions;
    xps_u32 pair_count;
    xps_u32 unknown_pair_count;
} xps_header_flags;

typedef struct xps_header_pose_info {
    xps_u32 declared_bone_count;
    xps_u32 parsed_bone_count;
    xps_u32 byte_count;
} xps_header_pose_info;

typedef struct xps_probe_result {
    enum xps_format format;
    int has_header;
    xps_u16 version_major;
    xps_u16 version_minor;
} xps_probe_result;

typedef struct xps_model_info {
    enum xps_format format;
    int has_header;
    xps_u16 version_major;
    xps_u16 version_minor;
    xps_u32 bone_count;
    xps_u32 mesh_count;
} xps_model_info;

typedef struct xps_bone {
    xps_u32 index;
    char name[XPS_CFG_MAX_NAME];
    xps_i16 parent;
    xps_fx x;
    xps_fx y;
    xps_fx z;
} xps_bone;

typedef struct xps_texture {
    xps_u32 index;
    char file[XPS_CFG_MAX_TEXNAME];
    xps_u32 uv_layer;
} xps_texture;

typedef struct xps_weight {
    xps_i16 bone_id;
    xps_fx value;
} xps_weight;

typedef struct xps_vertex {
    xps_u32 index;
    xps_fx x;
    xps_fx y;
    xps_fx z;
    xps_fx nx;
    xps_fx ny;
    xps_fx nz;
    xps_u8 rgba[4];
    xps_u32 uv_count;
    xps_fx uv[XPS_CFG_MAX_UV_LAYERS][2];
    xps_u8 weight_count;
    xps_weight weights[XPS_CFG_MAX_WEIGHTS];
} xps_vertex;

typedef struct xps_face {
    xps_u32 index;
    xps_u32 i0;
    xps_u32 i1;
    xps_u32 i2;
} xps_face;

typedef struct xps_mesh_info {
    xps_u32 index;
    char name[XPS_CFG_MAX_NAME];
    xps_u32 uv_layers;
    xps_u32 texture_count;
    xps_u32 vertex_count;
    xps_u32 face_count;
} xps_mesh_info;

typedef struct xps_scan_result {
    xps_model_info model;
    xps_u32 total_textures;
    xps_u32 total_vertices;
    xps_u32 total_faces;
    xps_u32 max_uv_layers;
    xps_u32 max_weights;
} xps_scan_result;

typedef struct xps_pose_info {
    xps_u32 bone_count;
} xps_pose_info;

typedef struct xps_pose_bone {
    xps_u32 index;
    char name[XPS_CFG_MAX_NAME];
    xps_fx rx;
    xps_fx ry;
    xps_fx rz;
    xps_fx tx;
    xps_fx ty;
    xps_fx tz;
    xps_fx sx;
    xps_fx sy;
    xps_fx sz;
    xps_u8 has_explicit_scale;
} xps_pose_bone;

typedef struct xps_pose_scan_result {
    xps_pose_info pose;
    xps_u32 explicit_scale_count;
    xps_u32 implicit_scale_count;
} xps_pose_scan_result;

typedef struct xps_bonedict_info {
    xps_u32 entry_count;
} xps_bonedict_info;

typedef struct xps_bonedict_entry {
    xps_u32 index;
    char old_name[XPS_CFG_MAX_NAME];
    char new_name[XPS_CFG_MAX_NAME];
} xps_bonedict_entry;

typedef struct xps_bonedict_scan_result {
    xps_bonedict_info dict;
    xps_u32 identity_count;
    xps_u32 rename_count;
} xps_bonedict_scan_result;

typedef struct xps_scene_info {
    enum xps_format format;
    xps_u16 version_major;
    xps_u16 version_minor;
    xps_u32 item_count;
} xps_scene_info;

typedef struct xps_scene_item {
    xps_u32 index;
    char name[XPS_CFG_MAX_NAME];
    char path[XPS_CFG_MAX_PATH];
    xps_u8 visible;
    xps_fx sx;
    xps_fx sy;
    xps_fx sz;
    xps_u32 bone_count;
    xps_fx x;
    xps_fx y;
    xps_fx z;
    xps_u32 primary_accessory_count;
    xps_u32 secondary_accessory_count;
    xps_u8 has_glow;
    xps_fx glow[6];
} xps_scene_item;

typedef struct xps_scene_bone_pose {
    xps_u32 item_index;
    xps_u32 index;
    char name[XPS_CFG_MAX_NAME];
    xps_fx rx;
    xps_fx ry;
    xps_fx rz;
    xps_fx tx;
    xps_fx ty;
    xps_fx tz;
    xps_fx sx;
    xps_fx sy;
    xps_fx sz;
} xps_scene_bone_pose;

typedef struct xps_scene_accessory {
    xps_u32 item_index;
    xps_u32 index;
    enum xps_scene_accessory_kind kind;
    char name[XPS_CFG_MAX_NAME];
    xps_u8 enabled;
} xps_scene_accessory;

typedef struct xps_scene_camera {
    xps_fx fov;
    xps_fx target_x;
    xps_fx target_y;
    xps_fx target_z;
    xps_fx distance;
    xps_fx rotation_horizontal;
    xps_fx rotation_vertical;
} xps_scene_camera;

typedef struct xps_scene_light {
    xps_u32 index;
    xps_fx direction_x;
    xps_fx direction_y;
    xps_fx direction_z;
    xps_fx intensity;
    xps_u8 rgb[3];
    xps_fx shadow_depth;
} xps_scene_light;

typedef struct xps_scene_post {
    xps_u8 present;
    xps_u8 has_use_post;
    xps_u8 use_post;
    xps_fx brightness;
    xps_fx gamma;
    xps_fx contrast;
    xps_fx saturation;
} xps_scene_post;

typedef struct xps_scene_background {
    xps_u8 ground_visible;
    char ground_texture[XPS_CFG_MAX_PATH];
    xps_u8 has_background_color;
    xps_u8 background_rgb[3];
    char background_texture[XPS_CFG_MAX_PATH];
    xps_u8 has_background_texture_type;
    char background_texture_type[XPS_CFG_MAX_NAME];
    char hud_texture[XPS_CFG_MAX_PATH];
} xps_scene_background;

typedef struct xps_scene_sky {
    xps_u8 present;
    xps_u8 display;
    char type[XPS_CFG_MAX_NAME];
    xps_fx rotation;
    xps_fx elevation;
} xps_scene_sky;

typedef struct xps_scene_window {
    xps_u8 present;
    xps_u8 is_maximized;
    xps_u32 width;
    xps_u32 height;
} xps_scene_window;

typedef struct xps_scene_scan_result {
    xps_scene_info scene;
    xps_u32 total_pose_bones;
    xps_u32 total_primary_accessories;
    xps_u32 total_secondary_accessories;
    xps_u32 visible_item_count;
    xps_u32 hidden_item_count;
    xps_u32 items_with_glow;
} xps_scene_scan_result;

#endif
