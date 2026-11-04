#ifndef GIF_MS3D_CODEC_H
#define GIF_MS3D_CODEC_H

/*
   Giffany MS3D codec - C89, static storage, fixed point.
   Runtime constraints: no malloc, no free, no realloc, no heap ownership.
   Numeric payload is converted from raw IEEE-754 single-precision bytes
   into signed Q16.16 fixed point without using C floating types.
*/

#include <stddef.h>

#ifndef MS3D_MAX_VERTICES
#define MS3D_MAX_VERTICES 12288
#endif

#ifndef MS3D_MAX_TRIANGLES
#define MS3D_MAX_TRIANGLES 16384
#endif

#ifndef MS3D_MAX_GROUPS
#define MS3D_MAX_GROUPS 256
#endif

#ifndef MS3D_MAX_MATERIALS
#define MS3D_MAX_MATERIALS 128
#endif

#ifndef MS3D_MAX_JOINTS
#define MS3D_MAX_JOINTS 128
#endif

#ifndef MS3D_MAX_GROUP_TRIANGLE_INDICES
#define MS3D_MAX_GROUP_TRIANGLE_INDICES 16384
#endif

#ifndef MS3D_MAX_ROT_KEYS
#define MS3D_MAX_ROT_KEYS 8192
#endif

#ifndef MS3D_MAX_POS_KEYS
#define MS3D_MAX_POS_KEYS 8192
#endif

#ifndef MS3D_MAX_COMMENT_CHARS
#define MS3D_MAX_COMMENT_CHARS 256
#endif

#ifndef MS3D_MAX_MODEL_COMMENT_CHARS
#define MS3D_MAX_MODEL_COMMENT_CHARS 512
#endif

#define MS3D_MAGIC_SIZE 10
#define MS3D_MAGIC_TEXT "MS3D000000"
#define MS3D_SUPPORTED_VERSION 4

#define MS3D_FX_SHIFT 16
#define MS3D_FX_ONE 65536L
#define MS3D_FX_MAX 2147483647L
#define MS3D_FX_MIN (-2147483647L - 1L)

typedef unsigned char ms3d_u8;
typedef unsigned short ms3d_u16;
typedef unsigned long ms3d_u32;
typedef long ms3d_i32;
typedef long ms3d_fx;

#define MS3D_FLAG_SELECTED 1
#define MS3D_FLAG_HIDDEN 2
#define MS3D_FLAG_SELECTED2 4
#define MS3D_FLAG_DIRTY 8

enum ms3d_result {
    MS3D_OK = 0,
    MS3D_E_NULL = -1,
    MS3D_E_SHORT = -2,
    MS3D_E_MAGIC = -3,
    MS3D_E_VERSION = -4,
    MS3D_E_LIMIT_VERTICES = -5,
    MS3D_E_LIMIT_TRIANGLES = -6,
    MS3D_E_LIMIT_GROUPS = -7,
    MS3D_E_LIMIT_MATERIALS = -8,
    MS3D_E_LIMIT_JOINTS = -9,
    MS3D_E_LIMIT_GROUP_TRIANGLES = -10,
    MS3D_E_LIMIT_ROT_KEYS = -11,
    MS3D_E_LIMIT_POS_KEYS = -12,
    MS3D_E_BAD_INDEX = -13,
    MS3D_E_BAD_COUNT = -14,
    MS3D_E_BAD_PARENT = -15,
    MS3D_E_OVERFLOW = -16,
    MS3D_E_DIV_ZERO = -17
};

typedef struct ms3d_vertex_s {
    ms3d_u8 flags;
    ms3d_fx position[3];
    int bone_id;
    ms3d_u8 reference_count;
    int extra_bone_ids[3];
    ms3d_u8 weights[3];
    ms3d_u32 extra;
    ms3d_u32 extra2;
} ms3d_vertex;

typedef struct ms3d_influence_s {
    int bone_id;
    ms3d_fx weight;
} ms3d_influence;

typedef struct ms3d_triangle_s {
    ms3d_u16 flags;
    ms3d_u16 vertex_indices[3];
    ms3d_fx vertex_normals[3][3];
    ms3d_fx s[3];
    ms3d_fx t[3];
    ms3d_u8 smoothing_group;
    ms3d_u8 group_index;
} ms3d_triangle;

typedef struct ms3d_group_s {
    ms3d_u8 flags;
    char name[33];
    ms3d_u16 triangle_start;
    ms3d_u16 triangle_count;
    int material_index;
    char comment[MS3D_MAX_COMMENT_CHARS];
} ms3d_group;

typedef struct ms3d_material_s {
    char name[33];
    ms3d_fx ambient[4];
    ms3d_fx diffuse[4];
    ms3d_fx specular[4];
    ms3d_fx emissive[4];
    ms3d_fx shininess;
    ms3d_fx transparency;
    int mode;
    char texture[129];
    char alphamap[129];
    char comment[MS3D_MAX_COMMENT_CHARS];
} ms3d_material;

typedef struct ms3d_key_rot_s {
    ms3d_fx time;
    ms3d_fx rotation[3];
} ms3d_key_rot;

typedef struct ms3d_key_pos_s {
    ms3d_fx time;
    ms3d_fx position[3];
} ms3d_key_pos;

typedef struct ms3d_joint_s {
    ms3d_u8 flags;
    char name[33];
    char parent_name[33];
    ms3d_fx rotation[3];
    ms3d_fx position[3];
    ms3d_u16 rot_key_start;
    ms3d_u16 rot_key_count;
    ms3d_u16 pos_key_start;
    ms3d_u16 pos_key_count;
    ms3d_fx color[3];
    int parent_index;
    char comment[MS3D_MAX_COMMENT_CHARS];
} ms3d_joint;

typedef struct ms3d_model_s {
    ms3d_i32 version;

    ms3d_u16 num_vertices;
    ms3d_u16 num_triangles;
    ms3d_u16 num_groups;
    ms3d_u16 num_materials;
    ms3d_u16 num_joints;

    ms3d_u16 num_group_triangle_indices;
    ms3d_u16 num_rot_keys;
    ms3d_u16 num_pos_keys;

    ms3d_fx animation_fps;
    ms3d_fx current_time;
    ms3d_i32 total_frames;

    ms3d_i32 comments_subversion;
    ms3d_i32 vertex_extra_subversion;
    ms3d_i32 joint_extra_subversion;
    ms3d_i32 model_extra_subversion;

    ms3d_i32 num_group_comments;
    ms3d_i32 num_material_comments;
    ms3d_i32 num_joint_comments;
    ms3d_i32 has_model_comment;
    ms3d_i32 comment_truncated_count;

    ms3d_fx joint_size;
    ms3d_i32 transparency_mode;
    ms3d_fx alpha_ref;
    char model_comment[MS3D_MAX_MODEL_COMMENT_CHARS];

    ms3d_vertex vertices[MS3D_MAX_VERTICES];
    ms3d_triangle triangles[MS3D_MAX_TRIANGLES];
    ms3d_group groups[MS3D_MAX_GROUPS];
    ms3d_u16 group_triangle_indices[MS3D_MAX_GROUP_TRIANGLE_INDICES];
    ms3d_material materials[MS3D_MAX_MATERIALS];
    ms3d_joint joints[MS3D_MAX_JOINTS];
    ms3d_key_rot rot_keys[MS3D_MAX_ROT_KEYS];
    ms3d_key_pos pos_keys[MS3D_MAX_POS_KEYS];
} ms3d_model;

typedef struct ms3d_parse_info_s {
    size_t bytes_total;
    size_t bytes_used;
    size_t bytes_remaining;
    int had_trailing_bytes;
    int parsed_comments;
    int parsed_vertex_extra;
    int parsed_joint_extra;
    int parsed_model_extra;
} ms3d_parse_info;

typedef struct ms3d_sink_s {
    void *user;
    int (*begin_model)(void *user, const ms3d_model *model);
    int (*vertex)(void *user, ms3d_u16 index, const ms3d_vertex *vertex);
    int (*triangle)(void *user, ms3d_u16 index, const ms3d_triangle *triangle);
    int (*group)(void *user, ms3d_u16 index, const ms3d_group *group);
    int (*material)(void *user, ms3d_u16 index, const ms3d_material *material);
    int (*joint)(void *user, ms3d_u16 index, const ms3d_joint *joint);
    int (*end_model)(void *user, const ms3d_model *model);
} ms3d_sink;

void ms3d_model_clear(ms3d_model *model);
int ms3d_parse(ms3d_model *model, const unsigned char *data, size_t size);
int ms3d_parse_ex(ms3d_model *model, const unsigned char *data, size_t size, ms3d_parse_info *info);
int ms3d_validate(const ms3d_model *model);
int ms3d_emit(const ms3d_model *model, const ms3d_sink *sink);
const char *ms3d_error_string(int code);

ms3d_fx ms3d_fx_from_int(long value);
long ms3d_fx_to_int(ms3d_fx value);
ms3d_fx ms3d_fx_add_sat(ms3d_fx a, ms3d_fx b);
ms3d_fx ms3d_fx_sub_sat(ms3d_fx a, ms3d_fx b);
ms3d_fx ms3d_fx_mul(ms3d_fx a, ms3d_fx b);
ms3d_fx ms3d_fx_div(ms3d_fx a, ms3d_fx b);
ms3d_fx ms3d_fx_lerp(ms3d_fx a, ms3d_fx b, ms3d_fx alpha);
int ms3d_fx_to_ascii(ms3d_fx value, char *out, size_t out_size, unsigned digits);

int ms3d_vertex_get_influences(const ms3d_model *model, ms3d_u16 vertex_index, ms3d_influence out_influences[4], ms3d_u8 *out_count);
int ms3d_compute_bounds(const ms3d_model *model, ms3d_fx out_min[3], ms3d_fx out_max[3]);
int ms3d_sample_joint_local(const ms3d_model *model, ms3d_u16 joint_index, ms3d_fx time, ms3d_fx out_rotation[3], ms3d_fx out_position[3]);

#endif
