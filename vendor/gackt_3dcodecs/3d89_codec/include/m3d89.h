#ifndef M3D89_H
#define M3D89_H

/*
   m3d89 - tiny fixed-buffer Model 3D (.m3d) decoder
   C89 core, no heap allocation, Q16.16 numeric output.
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef signed char        m3d89_i8;
typedef unsigned char      m3d89_u8;
typedef signed short       m3d89_i16;
typedef unsigned short     m3d89_u16;
typedef signed long        m3d89_i32;
typedef unsigned long      m3d89_u32;

typedef m3d89_i32          m3d89_q16;

#ifndef M3D89_MAX_VERTICES
#define M3D89_MAX_VERTICES       8192
#endif
#ifndef M3D89_MAX_TEXCOORDS
#define M3D89_MAX_TEXCOORDS      8192
#endif
#ifndef M3D89_MAX_FACES
#define M3D89_MAX_FACES          16384
#endif
#ifndef M3D89_MAX_MATERIALS
#define M3D89_MAX_MATERIALS      256
#endif
#ifndef M3D89_MAX_PROPERTIES
#define M3D89_MAX_PROPERTIES     2048
#endif
#ifndef M3D89_MAX_BONES
#define M3D89_MAX_BONES          256
#endif
#ifndef M3D89_MAX_SKINS
#define M3D89_MAX_SKINS          8192
#endif
#ifndef M3D89_MAX_INFLUENCES
#define M3D89_MAX_INFLUENCES     8
#endif
#ifndef M3D89_MAX_ACTIONS
#define M3D89_MAX_ACTIONS        64
#endif
#ifndef M3D89_MAX_FRAMES
#define M3D89_MAX_FRAMES         1024
#endif
#ifndef M3D89_MAX_TRANSFORMS
#define M3D89_MAX_TRANSFORMS     4096
#endif
#ifndef M3D89_MAX_VOX_TYPES
#define M3D89_MAX_VOX_TYPES      512
#endif
#ifndef M3D89_MAX_VOX_BLOCKS
#define M3D89_MAX_VOX_BLOCKS     128
#endif
#ifndef M3D89_MAX_LABELS
#define M3D89_MAX_LABELS         256
#endif
#ifndef M3D89_MAX_ASSETS
#define M3D89_MAX_ASSETS         256
#endif
#ifndef M3D89_MAX_PROCS
#define M3D89_MAX_PROCS          64
#endif
#ifndef M3D89_MAX_SHAPES
#define M3D89_MAX_SHAPES         256
#endif
#ifndef M3D89_MAX_CHUNKS
#define M3D89_MAX_CHUNKS         1024
#endif

#define M3D89_OK                         0
#define M3D89_ERR_BAD_ARG               -1
#define M3D89_ERR_TOO_SMALL             -2
#define M3D89_ERR_BAD_MAGIC             -3
#define M3D89_ERR_RANGE                 -4
#define M3D89_ERR_BAD_HEADER            -5
#define M3D89_ERR_STREAM_PACKED         -6
#define M3D89_ERR_CAPACITY              -7
#define M3D89_ERR_TRUNCATED             -8
#define M3D89_ERR_UNSUPPORTED_TYPE      -9
#define M3D89_ERR_BAD_RECORD           -10

#define M3D89_UNDEF_U32 ((m3d89_u32)0xFFFFFFFFUL)
#define M3D89_Q16_ONE  ((m3d89_q16)65536L)
#define M3D89_Q16_HALF ((m3d89_q16)32768L)

#define M3D89_FACE_HAS_UV       1u
#define M3D89_FACE_HAS_NORMAL   2u
#define M3D89_FACE_HAS_MAXIMUM  4u
#define M3D89_FACE_FROM_NGON    8u

#define M3D89_CHUNK_FLAG_STD    1u
#define M3D89_CHUNK_FLAG_APP    2u
#define M3D89_CHUNK_FLAG_BAD    4u

typedef struct m3d89_chunk_s {
    char magic[5];
    m3d89_u32 offset;
    m3d89_u32 length;
    m3d89_u32 flags;
} m3d89_chunk;

typedef struct m3d89_vertex_s {
    m3d89_q16 x;
    m3d89_q16 y;
    m3d89_q16 z;
    m3d89_q16 w;
    m3d89_u32 rgba;
    m3d89_u32 skin;
} m3d89_vertex;

typedef struct m3d89_texcoord_s {
    m3d89_q16 u;
    m3d89_q16 v;
} m3d89_texcoord;

typedef struct m3d89_face_s {
    m3d89_u32 vertex[3];
    m3d89_u32 texcoord[3];
    m3d89_u32 normal[3];
    m3d89_u32 material_name;
    m3d89_u16 material_index;
    m3d89_u16 flags;
} m3d89_face;

typedef struct m3d89_property_s {
    m3d89_u8 type;
    m3d89_u8 format;
    m3d89_u16 reserved;
    m3d89_u32 raw;
    m3d89_q16 q;
} m3d89_property;

typedef struct m3d89_material_s {
    m3d89_u32 name;
    m3d89_u16 first_property;
    m3d89_u16 property_count;
} m3d89_material;

typedef struct m3d89_bone_s {
    m3d89_u32 parent;
    m3d89_u32 name;
    m3d89_u32 position_vertex;
    m3d89_u32 orientation_vertex;
} m3d89_bone;

typedef struct m3d89_skin_s {
    m3d89_u8 count;
    m3d89_u8 reserved[3];
    m3d89_u32 bone[M3D89_MAX_INFLUENCES];
    m3d89_q16 weight[M3D89_MAX_INFLUENCES];
} m3d89_skin;

typedef struct m3d89_transform_s {
    m3d89_u32 bone;
    m3d89_u32 position_vertex;
    m3d89_u32 orientation_vertex;
} m3d89_transform;

typedef struct m3d89_frame_s {
    m3d89_u32 msec;
    m3d89_u16 first_transform;
    m3d89_u16 transform_count;
} m3d89_frame;

typedef struct m3d89_action_s {
    m3d89_u32 name;
    m3d89_u32 duration_msec;
    m3d89_u16 first_frame;
    m3d89_u16 frame_count;
} m3d89_action;

typedef struct m3d89_vox_type_s {
    m3d89_u32 name;
    m3d89_u32 rgba;
    m3d89_u32 material_name;
    m3d89_u32 skin;
    m3d89_u16 shape;
    m3d89_u8 rotation;
    m3d89_u8 item_count;
} m3d89_vox_type;

typedef struct m3d89_vox_block_s {
    m3d89_u32 name;
    m3d89_i32 x;
    m3d89_i32 y;
    m3d89_i32 z;
    m3d89_u32 w;
    m3d89_u32 h;
    m3d89_u32 d;
    m3d89_u8 uncertainty;
    m3d89_u8 group;
    m3d89_u16 reserved;
    m3d89_u32 data_offset;
    m3d89_u32 data_length;
    m3d89_u32 encoded_runs;
    m3d89_u32 decoded_voxels;
    m3d89_u32 solid_voxels;
} m3d89_vox_block;

typedef struct m3d89_label_s {
    m3d89_u32 group_name;
    m3d89_u32 lang;
    m3d89_u32 text;
    m3d89_u32 vertex;
    m3d89_u32 rgba;
} m3d89_label;

typedef struct m3d89_asset_s {
    m3d89_u32 name;
    m3d89_u32 offset;
    m3d89_u32 length;
} m3d89_asset;

typedef struct m3d89_proc_s {
    m3d89_u32 script_name;
    m3d89_u32 offset;
    m3d89_u32 length;
} m3d89_proc;

typedef struct m3d89_shape_s {
    m3d89_u32 name;
    m3d89_u32 group;
    m3d89_u32 offset;
    m3d89_u32 length;
} m3d89_shape;

typedef struct m3d89_model_s {
    int error;
    m3d89_u32 warnings;
    m3d89_u32 flags;

    const m3d89_u8 *source;
    m3d89_u32 source_size;
    m3d89_u32 file_size;
    m3d89_u32 payload_offset;
    m3d89_u32 payload_size;
    m3d89_q16 scale_q16;
    m3d89_u32 type_bits;

    m3d89_u8 vc_s;
    m3d89_u8 vi_s;
    m3d89_u8 si_s;
    m3d89_u8 ci_s;
    m3d89_u8 ti_s;
    m3d89_u8 bi_s;
    m3d89_u8 nb_n;
    m3d89_u8 sk_s;
    m3d89_u8 fc_s;
    m3d89_u8 hi_s;
    m3d89_u8 fi_s;
    m3d89_u8 vd_s;
    m3d89_u8 vp_s;

    const m3d89_u8 *strings;
    m3d89_u32 strings_size;
    m3d89_u32 name;
    m3d89_u32 license;
    m3d89_u32 author;
    m3d89_u32 description;

    m3d89_u32 color_count;
    m3d89_u32 colors[256];

    m3d89_u16 vertex_count;
    m3d89_vertex vertices[M3D89_MAX_VERTICES];
    m3d89_u16 texcoord_count;
    m3d89_texcoord texcoords[M3D89_MAX_TEXCOORDS];
    m3d89_u16 face_count;
    m3d89_face faces[M3D89_MAX_FACES];

    m3d89_u16 material_count;
    m3d89_material materials[M3D89_MAX_MATERIALS];
    m3d89_u16 property_count;
    m3d89_property properties[M3D89_MAX_PROPERTIES];

    m3d89_u16 bone_count;
    m3d89_bone bones[M3D89_MAX_BONES];
    m3d89_u16 skin_count;
    m3d89_skin skins[M3D89_MAX_SKINS];

    m3d89_u16 action_count;
    m3d89_action actions[M3D89_MAX_ACTIONS];
    m3d89_u16 frame_count;
    m3d89_frame frames[M3D89_MAX_FRAMES];
    m3d89_u16 transform_count;
    m3d89_transform transforms[M3D89_MAX_TRANSFORMS];

    m3d89_u16 vox_type_count;
    m3d89_vox_type vox_types[M3D89_MAX_VOX_TYPES];
    m3d89_u16 vox_block_count;
    m3d89_vox_block vox_blocks[M3D89_MAX_VOX_BLOCKS];

    m3d89_u16 label_count;
    m3d89_label labels[M3D89_MAX_LABELS];
    m3d89_u16 asset_count;
    m3d89_asset assets[M3D89_MAX_ASSETS];
    m3d89_u16 proc_count;
    m3d89_proc procs[M3D89_MAX_PROCS];
    m3d89_u16 shape_count;
    m3d89_shape shapes[M3D89_MAX_SHAPES];

    m3d89_u16 chunk_count;
    m3d89_chunk chunks[M3D89_MAX_CHUNKS];
} m3d89_model;

typedef void (*m3d89_voxel_cb)(void *user, m3d89_i32 x, m3d89_i32 y, m3d89_i32 z, m3d89_u32 value);

void m3d89_zero_model(m3d89_model *m);
int m3d89_decode(m3d89_model *m, const void *data, m3d89_u32 size);
int m3d89_decode_payload(m3d89_model *m, const void *data, m3d89_u32 size, m3d89_u32 file_size, m3d89_u32 payload_offset);
const char *m3d89_string(const m3d89_model *m, m3d89_u32 offset);
int m3d89_decode_voxel_block(const m3d89_model *m, m3d89_u16 block_index, m3d89_voxel_cb cb, void *user);

m3d89_q16 m3d89_mul_q16(m3d89_q16 a, m3d89_q16 b);
m3d89_q16 m3d89_lerp_q16(m3d89_q16 a, m3d89_q16 b, m3d89_q16 t);
void m3d89_vec4_lerp_q16(m3d89_vertex *out, const m3d89_vertex *a, const m3d89_vertex *b, m3d89_q16 t);
void m3d89_quat_nlerp_q16(m3d89_vertex *out, const m3d89_vertex *a, const m3d89_vertex *b, m3d89_q16 t);
int m3d89_sample_action(const m3d89_model *m, m3d89_u16 action_index, m3d89_u32 msec, m3d89_vertex *out_pos, m3d89_vertex *out_ori, m3d89_u16 out_bones);

#ifdef __cplusplus
}
#endif

#endif
