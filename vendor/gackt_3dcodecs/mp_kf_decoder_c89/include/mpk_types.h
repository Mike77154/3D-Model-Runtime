#ifndef MPK_TYPES_H
#define MPK_TYPES_H

#include "mpk_config.h"

/* C89-friendly fixed-width-ish types. The reader masks all byte reads. */
typedef signed char        mpk_i8;
typedef unsigned char      mpk_u8;
typedef signed short       mpk_i16;
typedef unsigned short     mpk_u16;
typedef signed long        mpk_i32;
typedef unsigned long      mpk_u32;

typedef mpk_i32 mpk_fixed; /* Q16.16 */

#define MPK_TRUE  1
#define MPK_FALSE 0

#define MPK_CHUNK_TAG 0x0cU

#define MPK_ID_NODE          0x00010000UL
#define MPK_ID_CAMERA        0x00010001UL
#define MPK_ID_POINT_LIGHT   0x00010002UL
#define MPK_ID_DIR_LIGHT     0x00010003UL
#define MPK_ID_SPOT_LIGHT    0x00010004UL
#define MPK_ID_MESH          0x00010005UL
#define MPK_ID_GEOMETRY      0x00010006UL
#define MPK_ID_POLYGONS      0x00010007UL
#define MPK_ID_POLYGON       0x00010008UL
#define MPK_ID_SMOOTHING     0x0001000BUL
#define MPK_ID_POLY_MAT      0x0001000CUL
#define MPK_ID_UV_MAPPING    0x0001000EUL
#define MPK_ID_MATERIAL_LIST 0x0001000FUL
#define MPK_ID_MATERIAL      0x00010010UL
#define MPK_ID_TEXTURE       0x00010011UL
#define MPK_ID_KEYFRAME_ANIM 0x00010012UL
#define MPK_ID_ANIMATION     0x00010013UL
#define MPK_ID_SKIN          0x00010014UL
#define MPK_ID_ENVIRONMENT   0x00010015UL
#define MPK_ID_HELPER        0x00010016UL
#define MPK_ID_POINT_ANIM    0x00010017UL
#define MPK_ID_DIR_ANIM      0x00010018UL
#define MPK_ID_SPOT_ANIM     0x00010019UL
#define MPK_ID_REFERENCE     0x0001001AUL

#define MPK_T_LONG           0x00U
#define MPK_T_ULONG          0x01U
#define MPK_T_INT32          0x02U
#define MPK_T_UINT32         0x03U
#define MPK_T_SHORT          0x04U
#define MPK_T_USHORT         0x05U
#define MPK_T_CHAR           0x06U
#define MPK_T_SCHAR          0x07U
#define MPK_T_UCHAR          0x08U
#define MPK_T_FLOAT32        0x09U
#define MPK_T_DOUBLE64       0x0aU
#define MPK_T_STRING         0x0dU
#define MPK_T_BOOL           0x0eU
#define MPK_T_UINT24         0x0fU
#define MPK_T_UINT16         0x10U
#define MPK_T_UINT8          0x11U
#define MPK_T_INT24          0x12U
#define MPK_T_INT16          0x13U
#define MPK_T_INT8           0x14U
#define MPK_T_VEC2           0x15U
#define MPK_T_VEC3           0x16U
#define MPK_T_VEC4           0x17U
#define MPK_T_MAT2           0x18U
#define MPK_T_MAT3           0x19U
#define MPK_T_MAT4X3         0x1aU
#define MPK_T_MAT4           0x1bU
#define MPK_T_FLOAT16        0x26U

typedef struct mpk_chunk_header_s {
    mpk_u32 id;
    mpk_u32 version;
    mpk_u32 size;
    long data_start;
    long data_end;
} mpk_chunk_header;

typedef struct mpk_vec3_s {
    mpk_fixed x;
    mpk_fixed y;
    mpk_fixed z;
} mpk_vec3;

typedef struct mpk_uv_s {
    mpk_fixed u;
    mpk_fixed v;
    mpk_fixed w;
} mpk_uv;

typedef struct mpk_matrix4x3_s {
    mpk_fixed m[12];
} mpk_matrix4x3;

typedef struct mpk_node_s {
    char name[MPK_NAME_MAX];
    char parent[MPK_NAME_MAX];
    char user[MPK_PATH_MAX];
    mpk_matrix4x3 object_to_parent;
    int has_parent;
} mpk_node;

typedef struct mpk_material_s {
    char name[MPK_NAME_MAX];
    char diffuse_texture[MPK_PATH_MAX];
    char reflection_texture[MPK_PATH_MAX];
    char bump_texture[MPK_PATH_MAX];
    char opacity_texture[MPK_PATH_MAX];
    char mask_texture[MPK_PATH_MAX];
    mpk_fixed ambient[4];
    mpk_fixed diffuse[4];
    mpk_fixed specular[4];
    mpk_fixed vertex_alpha;
    mpk_fixed specular_exponent;
    int two_sided;
    int fogging;
    int invisible_geometry;
    int has_vertex_alpha;
} mpk_material;

typedef struct mpk_polygon_s {
    int count;
    mpk_u32 v[MPK_MAX_POLY_VERTS];
    mpk_u32 uv[MPK_MAX_POLY_VERTS];
    mpk_u32 material;
    mpk_u32 smoothing;
} mpk_polygon;

typedef struct mpk_mesh_s {
    mpk_node node;
    mpk_u32 first_vertex;
    mpk_u32 vertex_count;
    mpk_u32 first_normal;
    mpk_u32 normal_count;
    mpk_u32 first_uv;
    mpk_u32 uv_count;
    mpk_u32 first_polygon;
    mpk_u32 polygon_count;
    char reference[MPK_NAME_MAX];
} mpk_mesh;

typedef struct mpk_skin_weight_s {
    mpk_u32 vertex_index;
    mpk_u32 bone_index;
    mpk_fixed weight;
} mpk_skin_weight;

typedef struct mpk_bone_name_s {
    char name[MPK_NAME_MAX];
} mpk_bone_name;

typedef struct mpk_keyframe_s {
    mpk_u32 anim_index;
    mpk_u32 frame_id;
    mpk_matrix4x3 transform;
} mpk_keyframe;

typedef struct mpk_animation_s {
    char object_name[MPK_NAME_MAX];
    char parent_name[MPK_NAME_MAX];
    mpk_i32 fps;
    int looping;
    int loop_interpolation;
    mpk_u32 total_keyframes;
    mpk_u32 first_keyframe;
    mpk_u32 keyframe_count;
} mpk_animation;

typedef struct mpk_camera_s {
    mpk_node node;
    mpk_fixed fov;
    mpk_fixed front_plane;
    mpk_fixed back_plane;
} mpk_camera;

#define MPK_LIGHT_POINT       1
#define MPK_LIGHT_DIRECTIONAL 2
#define MPK_LIGHT_SPOT        3

#define MPK_LPARAM_INT        1
#define MPK_LPARAM_UINT       2
#define MPK_LPARAM_FIXED      3
#define MPK_LPARAM_BOOL       4
#define MPK_LPARAM_UCHAR      5

typedef struct mpk_light_param_s {
    mpk_u8 kind;
    mpk_u8 source_tag;
    mpk_i32 i;
    mpk_u32 u;
    mpk_fixed fx;
    mpk_u8 u8;
} mpk_light_param;

typedef struct mpk_light_s {
    int type;
    mpk_u32 version;
    mpk_node node;
    mpk_fixed color[4];
    mpk_fixed intensity;
    mpk_fixed range;
    mpk_fixed inner_cone;
    mpk_fixed outer_cone;
    mpk_vec3 vectors[MPK_MAX_LIGHT_VECS];
    mpk_u32 vector_count;
    mpk_light_param params[MPK_MAX_LIGHT_PARAMS];
    mpk_u32 param_count;
} mpk_light;

typedef struct mpk_camera_pose_s {
    mpk_matrix4x3 transform;
    mpk_fixed fov;
    mpk_fixed front_plane;
    mpk_fixed back_plane;
    mpk_u32 source_animation;
    int has_animation;
} mpk_camera_pose;

typedef struct mpk_resolved_node_s {
    char name[MPK_NAME_MAX];
    char parent[MPK_NAME_MAX];
    mpk_matrix4x3 local;
    mpk_matrix4x3 world;
    mpk_u32 parent_index;
    int has_parent;
    int resolved;
} mpk_resolved_node;

typedef struct mpk_environment_s {
    mpk_u8 ambient[4];
    mpk_u8 clearing[4];
} mpk_environment;

typedef struct mpk_model_s {
    mpk_mesh meshes[MPK_MAX_MESHES];
    mpk_u32 mesh_count;

    mpk_material materials[MPK_MAX_MATERIALS];
    mpk_u32 material_count;
    char texture_dirs[MPK_PATH_MAX];

    mpk_node nodes[MPK_MAX_NODES];
    mpk_u32 node_count;

    mpk_vec3 vertices[MPK_MAX_VERTICES];
    mpk_u32 vertex_count;
    mpk_vec3 normals[MPK_MAX_NORMALS];
    mpk_u32 normal_count;
    mpk_uv uvs[MPK_MAX_UVS];
    mpk_u32 uv_count;
    mpk_polygon polygons[MPK_MAX_POLYGONS];
    mpk_u32 polygon_count;

    mpk_skin_weight skin_weights[MPK_MAX_SKIN_WEIGHTS];
    mpk_u32 skin_weight_count;
    mpk_bone_name bones[MPK_MAX_BONES];
    mpk_u32 bone_count;
    mpk_bone_name skin_objects[MPK_MAX_BONES];
    mpk_u32 skin_object_count;

    mpk_animation animations[MPK_MAX_ANIMS];
    mpk_u32 animation_count;
    mpk_keyframe keyframes[MPK_MAX_KEYFRAMES];
    mpk_u32 keyframe_count;

    mpk_camera cameras[32];
    mpk_u32 camera_count;
    mpk_environment environments[8];
    mpk_u32 environment_count;

    mpk_light lights[MPK_MAX_LIGHTS];
    mpk_u32 light_count;

    mpk_resolved_node resolved_nodes[MPK_MAX_RESOLVED_NODES];
    mpk_u32 resolved_node_count;

    mpk_vec3 posed_vertices[MPK_MAX_VERTICES];
    mpk_u32 posed_vertex_count;
    int posed_valid;

    mpk_u32 skin_v1_vertex_indices[MPK_MAX_VERTICES];
    mpk_u32 skin_v1_vertex_index_count;
    mpk_u32 skin_v1_bones_per_vertex[MPK_MAX_VERTICES];
    mpk_u32 skin_v1_bones_per_vertex_count;
    mpk_u32 skin_v1_bones_list[MPK_MAX_SKIN_WEIGHTS];
    mpk_u32 skin_v1_bones_list_count;
    mpk_fixed skin_v1_weights[MPK_MAX_SKIN_WEIGHTS];
    mpk_u32 skin_v1_weight_count;
    mpk_u32 skin_v1_reconstructed_count;

    mpk_u32 unknown_chunk_count;
    mpk_u32 skipped_chunk_count;
    char last_error[MPK_ERROR_MAX];
} mpk_model;

#endif
