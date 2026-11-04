
#ifndef G3D_CODEC_H
#define G3D_CODEC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char  g3d_u8;
typedef signed char    g3d_s8;
typedef unsigned short g3d_u16;
typedef signed short   g3d_s16;
typedef unsigned int   g3d_u32;
typedef signed int     g3d_s32;

typedef g3d_s32 g3d_fix16;

#define G3D_TRUE  1
#define G3D_FALSE 0

#define G3D_OK                    0
#define G3D_ERR_BAD_ARG          -1
#define G3D_ERR_NO_SPACE         -2
#define G3D_ERR_ARENA_EXHAUSTED  -3
#define G3D_ERR_EOF              -4
#define G3D_ERR_BAD_MAGIC        -5
#define G3D_ERR_BAD_VERSION      -6
#define G3D_ERR_CORRUPT          -7
#define G3D_ERR_UNSUPPORTED      -8
#define G3D_ERR_NOT_FOUND        -9
#define G3D_ERR_OVERFLOW        -10

#define G3D_VFILE_ATTRIB_READONLY  0x00000001u
#define G3D_VFILE_ATTRIB_DIRECTORY 0x00000002u

#define G3D_BODY_MAX_LOD 4

#define G3D_BITMAP_MAX_MIPS 16
#define G3D_BITMAP_TAG_GEBM 0x6D426547u

#define G3D_PIXELFORMAT_NO_DATA        0u
#define G3D_PIXELFORMAT_8BIT           1u
#define G3D_PIXELFORMAT_8BIT_GRAY      2u
#define G3D_PIXELFORMAT_16BIT_555_RGB  3u
#define G3D_PIXELFORMAT_16BIT_555_BGR  4u
#define G3D_PIXELFORMAT_16BIT_565_RGB  5u
#define G3D_PIXELFORMAT_16BIT_565_BGR  6u
#define G3D_PIXELFORMAT_16BIT_4444_ARGB 7u
#define G3D_PIXELFORMAT_16BIT_1555_ARGB 8u
#define G3D_PIXELFORMAT_24BIT_RGB      9u
#define G3D_PIXELFORMAT_24BIT_BGR      10u
#define G3D_PIXELFORMAT_24BIT_YUV      11u
#define G3D_PIXELFORMAT_32BIT_RGBX     12u
#define G3D_PIXELFORMAT_32BIT_XRGB     13u
#define G3D_PIXELFORMAT_32BIT_BGRX     14u
#define G3D_PIXELFORMAT_32BIT_XBGR     15u
#define G3D_PIXELFORMAT_32BIT_RGBA     16u
#define G3D_PIXELFORMAT_32BIT_ARGB     17u
#define G3D_PIXELFORMAT_32BIT_BGRA     18u
#define G3D_PIXELFORMAT_32BIT_ABGR     19u
#define G3D_PIXELFORMAT_WAVELET        20u
#define G3D_PIXELFORMAT_16BIT_88_UV    21u
#define G3D_PIXELFORMAT_16BIT_556_UVL  22u
#define G3D_PIXELFORMAT_24BIT_888_UVL  23u
#define G3D_PIXELFORMAT_COUNT          24u

#define G3D_BITMAP_INFO_HAS_PALETTE    0x00000001u
#define G3D_BITMAP_INFO_HAS_ALPHA      0x00000002u
#define G3D_BITMAP_INFO_HAS_COLOR_KEY  0x00000004u
#define G3D_BITMAP_INFO_HAS_COMPRESSED_MIPS 0x00000008u
#define G3D_BITMAP_INFO_HAS_PAETH_MIPS      0x00000010u
#define G3D_BITMAP_INFO_HAS_COMPRESSED_PALETTE 0x00000020u
#define G3D_BITMAP_INFO_WAVELET_FORMAT 0x00000040u
#define G3D_BITMAP_INFO_UNSUPPORTED_COMPRESSED 0x00010000u
#define G3D_BITMAP_INFO_UNSUPPORTED_PAETH      0x00020000u
#define G3D_BITMAP_INFO_UNSUPPORTED_WAVELET    0x00040000u

#define G3D_MOTION_NODE_UNDECIDED 0
#define G3D_MOTION_NODE_BRANCH    1
#define G3D_MOTION_NODE_LEAF      2

#define G3D_PATH_VK_LINEAR              0
#define G3D_PATH_VK_HERMITE             1
#define G3D_PATH_VK_HERMITE_ZERO_DERIV  2
#define G3D_PATH_QK_LINEAR              3
#define G3D_PATH_QK_SLERP               4
#define G3D_PATH_QK_SQUAD               5

#define G3D_QK_LINEAR 0
#define G3D_QK_SLERP  1
#define G3D_QK_SQUAD  2

#define G3D_QK_HINGE_COMPRESSION      0x1u
#define G3D_QK_LINEARTIME_COMPRESSION 0x2u
#define G3D_VK_LINEARTIME_COMPRESSION 0x2u

#define G3D_VK_LINEAR             0
#define G3D_VK_HERMITE            1
#define G3D_VK_HERMITE_ZERO_DERIV 2

#define G3D_POSE_BLEND_LINEAR  0u
#define G3D_POSE_BLEND_HERMITE 1u

typedef struct g3d_arena
{
    g3d_u8 *base;
    g3d_u32 size;
    g3d_u32 used;
} g3d_arena;

typedef struct g3d_fvec3
{
    g3d_u32 x_bits;
    g3d_u32 y_bits;
    g3d_u32 z_bits;
} g3d_fvec3;

typedef struct g3d_fquat
{
    g3d_u32 x_bits;
    g3d_u32 y_bits;
    g3d_u32 z_bits;
    g3d_u32 w_bits;
} g3d_fquat;

typedef struct g3d_fxform
{
    g3d_u32 f[12];
} g3d_fxform;

typedef struct g3d_fixvec3
{
    g3d_fix16 x;
    g3d_fix16 y;
    g3d_fix16 z;
} g3d_fixvec3;

typedef struct g3d_q15quat
{
    g3d_s32 x;
    g3d_s32 y;
    g3d_s32 z;
    g3d_s32 w;
} g3d_q15quat;

typedef struct g3d_sample_xform
{
    g3d_q15quat rotation;
    g3d_fixvec3 translation;
} g3d_sample_xform;

typedef struct g3d_fixmat34
{
    g3d_fix16 m[12];
} g3d_fixmat34;

typedef struct g3d_skinned_mesh
{
    g3d_u32 bone_count;
    g3d_u32 vertex_count;
    g3d_u32 normal_count;
    g3d_sample_xform *local_pose;
    g3d_fixmat34 *bone_matrices;
    g3d_fixvec3 *vertices;
    g3d_fixvec3 *normals;
} g3d_skinned_mesh;

typedef struct g3d_strblock
{
    g3d_s32 count;
    g3d_s32 size;
    g3d_u8 *data;
} g3d_strblock;

typedef struct g3d_tkarray
{
    g3d_s32 num_elements;
    g3d_s32 element_size;
    g3d_u8 *elements;
} g3d_tkarray;

typedef struct g3d_tkevents
{
    g3d_u32 data_size;
    g3d_u8 *event_data;
    g3d_tkarray time_keys;
} g3d_tkevents;

typedef struct g3d_qkframe
{
    g3d_u8 interpolation;
    g3d_u8 compression;
    g3d_u8 looping;
    g3d_u8 reserved0;
    g3d_u32 count;
    g3d_u32 time_value_count;
    g3d_u32 *time_bits;
    g3d_fvec3 hinge_axis;
    g3d_u32 *angle_bits;
    g3d_fquat *quats;
} g3d_qkframe;

typedef struct g3d_vkframe
{
    g3d_u8 interpolation;
    g3d_u8 compression;
    g3d_u8 looping;
    g3d_u8 reserved0;
    g3d_u32 count;
    g3d_u32 time_value_count;
    g3d_u32 *time_bits;
    g3d_fvec3 *vectors;
} g3d_vkframe;

typedef struct g3d_path
{
    g3d_u32 raw_header;
    g3d_u8 has_translation;
    g3d_u8 has_rotation;
    g3d_u8 translation_interp;
    g3d_u8 rotation_interp;
    g3d_u8 looped;
    g3d_u8 reserved0;
    g3d_u16 reserved1;
    g3d_vkframe translation;
    g3d_qkframe rotation;
} g3d_path;

typedef struct g3d_motion_leaf
{
    g3d_s32 path_count;
    g3d_s32 name_checksum;
    g3d_u32 flags;
    g3d_tkevents *events;
    g3d_strblock *name_array;
    g3d_path *paths;
} g3d_motion_leaf;

typedef struct g3d_motion
{
    char *name;
    g3d_u16 name_len;
    g3d_u8 maintain_names;
    g3d_u8 node_type;
    g3d_motion_leaf leaf;
} g3d_motion;

typedef struct g3d_body_xskin_vertex
{
    g3d_fvec3 xpoint;
    g3d_u32 u_bits;
    g3d_u32 v_bits;
    g3d_u8 lod_mask;
    g3d_u8 pad0;
    g3d_s16 bone_index;
} g3d_body_xskin_vertex;

typedef struct g3d_body_normal
{
    g3d_fvec3 normal;
    g3d_u8 lod_mask;
    g3d_u8 pad0;
    g3d_s16 bone_index;
} g3d_body_normal;

typedef struct g3d_body_bone
{
    g3d_fvec3 bbox_min;
    g3d_fvec3 bbox_max;
    g3d_fxform attachment;
    g3d_s16 parent_bone_index;
    g3d_u16 pad0;
} g3d_body_bone;

typedef struct g3d_body_triangle
{
    g3d_s16 vtx_index[3];
    g3d_s16 normal_index[3];
    g3d_s16 material_index;
} g3d_body_triangle;

typedef struct g3d_body_triangle_list
{
    g3d_u32 face_count;
    g3d_body_triangle *faces;
} g3d_body_triangle_list;

typedef struct g3d_body_material
{
    g3d_u8 has_bitmap;
    g3d_u8 reserved0;
    g3d_u16 reserved1;
    g3d_u32 red_bits;
    g3d_u32 green_bits;
    g3d_u32 blue_bits;
    g3d_u8 *bitmap_data;
    g3d_u32 bitmap_size;
} g3d_body_material;

typedef struct g3d_bitmap_mip_info
{
    g3d_u8 mip_index;
    g3d_u8 flags;
    g3d_u16 reserved0;
    g3d_u32 width;
    g3d_u32 height;
    g3d_u32 data_offset;
    g3d_u32 data_size;
    g3d_u32 stored_size;
    g3d_u32 codec_method;
} g3d_bitmap_mip_info;

typedef struct g3d_bitmap_info
{
    g3d_u32 tag;
    g3d_u8 version;
    g3d_u8 version_major;
    g3d_u8 version_minor;
    g3d_u8 format;
    g3d_u32 width;
    g3d_u32 height;
    g3d_u32 flags;
    g3d_u32 color_key;
    g3d_u8 maximum_mip;
    g3d_u8 seek_mip_count;
    g3d_u8 palette_format;
    g3d_u32 palette_size;
    g3d_u32 palette_data_offset;
    g3d_u32 palette_data_size;
    g3d_u32 palette_stored_size;
    g3d_u32 palette_codec_method;
    g3d_u32 mip_count;
    g3d_bitmap_mip_info mips[G3D_BITMAP_MAX_MIPS];
    g3d_u32 alpha_data_offset;
    g3d_u32 alpha_data_size;
    g3d_u32 used_bytes;
} g3d_bitmap_info;

typedef struct g3d_body
{
    g3d_fvec3 bbox_min;
    g3d_fvec3 bbox_max;
    g3d_s16 xskin_vertex_count;
    g3d_body_xskin_vertex *xskin_vertices;
    g3d_s16 normal_count;
    g3d_body_normal *normals;
    g3d_s16 bone_count;
    g3d_body_bone *bones;
    g3d_strblock *bone_names;
    g3d_s16 material_count;
    g3d_body_material *materials;
    g3d_strblock *material_names;
    g3d_u32 lod_count;
    g3d_body_triangle_list lods[G3D_BODY_MAX_LOD];
} g3d_body;

typedef struct g3d_actor
{
    g3d_u32 has_body;
    g3d_s32 motion_count;
    g3d_body *body;
    g3d_motion *motions;
} g3d_actor;

typedef struct g3d_vfs_entry
{
    char *name;
    g3d_u32 name_len;
    g3d_u32 time1;
    g3d_u32 time2;
    g3d_u32 attributes;
    g3d_s32 size;
    g3d_s32 offset;
    g3d_u32 hint_len;
    g3d_u8 *hint_data;
    g3d_u8 *payload;
    struct g3d_vfs_entry *child;
    struct g3d_vfs_entry *sibling;
} g3d_vfs_entry;

typedef struct g3d_vfs
{
    g3d_u8 *bytes;
    g3d_u32 size;
    g3d_u32 signature;
    g3d_u16 version;
    g3d_u8 encrypted;
    g3d_u8 xor_mask;
    g3d_u32 dispersed;
    g3d_u32 directory_offset;
    g3d_u32 data_length;
    g3d_u32 end_position;
    g3d_vfs_entry *root;
} g3d_vfs;

typedef struct g3d_asset_slot
{
    char *name;
    g3d_u32 type;
    void *object;
} g3d_asset_slot;

typedef struct g3d_asset_manager
{
    g3d_asset_slot *slots;
    g3d_u32 count;
    g3d_u32 capacity;
} g3d_asset_manager;

#define G3D_ASSET_BODY   1u
#define G3D_ASSET_MOTION 2u
#define G3D_ASSET_ACTOR  3u

void g3d_arena_init(g3d_arena *arena, void *memory, g3d_u32 size);
void g3d_arena_reset(g3d_arena *arena);
g3d_u32 g3d_arena_mark(const g3d_arena *arena);
void g3d_arena_rewind(g3d_arena *arena, g3d_u32 mark);
void *g3d_arena_alloc(g3d_arena *arena, g3d_u32 size, g3d_u32 alignment);
char *g3d_arena_strdup(g3d_arena *arena, const char *src);
char *g3d_arena_strdup_len(g3d_arena *arena, const char *src, g3d_u32 len);

const char *g3d_errstr(int code);

g3d_fix16 g3d_f32bits_to_fix16(g3d_u32 bits);
g3d_u32 g3d_fix16_to_f32bits(g3d_fix16 value);

const char *g3d_pixel_format_name(g3d_u32 format);
g3d_u32 g3d_pixel_format_bytes_per_pixel(g3d_u32 format);
int g3d_bitmap_probe(const void *data, g3d_u32 size, g3d_bitmap_info *info);
int g3d_bitmap_unpack_mip(const void *data, g3d_u32 size, g3d_u32 wanted_mip,
                          g3d_u8 *dst, g3d_u32 dst_size,
                          g3d_u32 *width_out, g3d_u32 *height_out,
                          g3d_u32 *raw_bytes_out,
                          g3d_u32 *codec_method_out);
int g3d_bitmap_unpack_mip_arena(const void *data, g3d_u32 size, g3d_u32 wanted_mip,
                                g3d_u8 *dst, g3d_u32 dst_size,
                                g3d_u32 *width_out, g3d_u32 *height_out,
                                g3d_u32 *raw_bytes_out,
                                g3d_u32 *codec_method_out,
                                g3d_arena *scratch_arena);

#define G3D_BITMAP_DECODE_APPLY_COLOR_KEY 0x00000001u
#define G3D_BITMAP_DECODE_APPLY_ALPHA     0x00000002u

int g3d_bitmap_decode_mip_rgba32(const void *data, g3d_u32 size, g3d_u32 wanted_mip,
                                  g3d_u32 decode_flags,
                                  g3d_u8 *dst_rgba, g3d_u32 dst_size,
                                  g3d_u32 *width_out, g3d_u32 *height_out,
                                  g3d_u32 *rgba_bytes_out);
int g3d_bitmap_decode_mip_rgba32_arena(const void *data, g3d_u32 size, g3d_u32 wanted_mip,
                                        g3d_u32 decode_flags,
                                        g3d_u8 *dst_rgba, g3d_u32 dst_size,
                                        g3d_u32 *width_out, g3d_u32 *height_out,
                                        g3d_u32 *rgba_bytes_out,
                                        g3d_arena *scratch_arena);

const char *g3d_strblock_get(const g3d_strblock *sb, g3d_s32 index);
g3d_s32 g3d_strblock_checksum(const g3d_strblock *sb);

int g3d_tkevents_get(const g3d_tkevents *ev, g3d_s32 index, g3d_u32 *time_bits, g3d_u32 *offset);
int g3d_tkevents_get_text(const g3d_tkevents *ev, g3d_s32 index, g3d_u32 *time_bits, const char **text);
g3d_s32 g3d_tkevents_count_between_fix16(const g3d_tkevents *ev, g3d_fix16 start_time, g3d_fix16 end_time);
int g3d_tkevents_find_next_between_fix16(const g3d_tkevents *ev, g3d_fix16 start_time, g3d_fix16 end_time,
                                         g3d_s32 after_index, g3d_s32 *out_index,
                                         g3d_u32 *time_bits, const char **text);

g3d_u8 g3d_vfs_context_mask_from_string(const char *context);
int g3d_vfs_parse(g3d_vfs *vfs, void *bytes, g3d_u32 size, g3d_arena *arena);
int g3d_vfs_parse_with_mask(g3d_vfs *vfs, void *bytes, g3d_u32 size, g3d_arena *arena, g3d_u8 xor_mask);
g3d_vfs_entry *g3d_vfs_find(g3d_vfs *vfs, const char *path);
g3d_u32 g3d_vfs_count_entries(const g3d_vfs *vfs, int include_directories);
g3d_u32 g3d_vfs_count_files(const g3d_vfs *vfs);

int g3d_strblock_parse(g3d_strblock *sb, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes);
int g3d_strblock_measure_binary(const g3d_strblock *sb, g3d_u32 *size_out);
int g3d_strblock_write_binary(const g3d_strblock *sb, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);

int g3d_tkarray_parse_binary(g3d_tkarray *arr, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes);
int g3d_tkarray_measure_binary(const g3d_tkarray *arr, g3d_u32 *size_out);
int g3d_tkarray_write_binary(const g3d_tkarray *arr, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);

int g3d_tkevents_parse(g3d_tkevents *ev, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes);
int g3d_tkevents_measure_binary(const g3d_tkevents *ev, g3d_u32 *size_out);
int g3d_tkevents_write_binary(const g3d_tkevents *ev, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);

int g3d_qkframe_parse_binary(g3d_qkframe *qk, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes);
int g3d_qkframe_measure_binary(const g3d_qkframe *qk, g3d_u32 *size_out);
int g3d_qkframe_write_binary(const g3d_qkframe *qk, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);

int g3d_vkframe_parse_binary(g3d_vkframe *vk, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes);
int g3d_vkframe_measure_binary(const g3d_vkframe *vk, g3d_u32 *size_out);
int g3d_vkframe_write_binary(const g3d_vkframe *vk, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);

int g3d_path_parse_binary(g3d_path *path, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes);
int g3d_path_measure_binary(const g3d_path *path, g3d_u32 *size_out);
int g3d_path_write_binary(const g3d_path *path, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);

int g3d_motion_parse_binary(g3d_motion *motion, const void *data, g3d_u32 size, g3d_arena *arena);
int g3d_motion_measure_binary(const g3d_motion *motion, g3d_u32 *size_out);
int g3d_motion_write_binary(const g3d_motion *motion, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);
int g3d_motion_measure_ascii(const g3d_motion *motion, g3d_u32 *size_out);
int g3d_motion_write_ascii(const g3d_motion *motion, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);

int g3d_body_parse_geometry(g3d_body *body, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes);
int g3d_body_parse_file(g3d_body *body, void *data, g3d_u32 size, g3d_arena *arena);
int g3d_body_parse_file_with_mask(g3d_body *body, void *data, g3d_u32 size, g3d_arena *arena, g3d_u8 xor_mask);
int g3d_body_measure_geometry(const g3d_body *body, g3d_u32 *size_out);
int g3d_body_measure_file(const g3d_body *body, g3d_u32 *size_out);
int g3d_body_write_geometry(const g3d_body *body, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);
int g3d_body_write_file(const g3d_body *body, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);
int g3d_body_write_file_with_mask(const g3d_body *body, void *dst, g3d_u32 cap, g3d_u32 *used_bytes, g3d_u8 xor_mask);

int g3d_actor_parse_file(g3d_actor *actor, void *data, g3d_u32 size, g3d_arena *arena);
int g3d_actor_parse_file_with_mask(g3d_actor *actor, void *data, g3d_u32 size, g3d_arena *arena, g3d_u8 xor_mask);
int g3d_actor_measure_file(const g3d_actor *actor, g3d_u32 *size_out);
int g3d_actor_write_file(const g3d_actor *actor, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);
int g3d_actor_write_file_with_mask(const g3d_actor *actor, void *dst, g3d_u32 cap, g3d_u32 *used_bytes, g3d_u8 xor_mask);

int g3d_path_validate(const g3d_path *path);
int g3d_motion_validate(const g3d_motion *motion);
int g3d_body_validate(const g3d_body *body);
int g3d_actor_validate(const g3d_actor *actor);

int g3d_motion_find_path_index(const g3d_motion *motion, const char *path_name, g3d_s32 *out_index);
const char *g3d_motion_get_path_name(const g3d_motion *motion, g3d_s32 path_index);
g3d_s32 g3d_motion_name_checksum(const g3d_motion *motion);
g3d_s32 g3d_motion_count_events_between_fix16(const g3d_motion *motion, g3d_fix16 start_time, g3d_fix16 end_time);
int g3d_body_find_bone_index(const g3d_body *body, const char *bone_name, g3d_s32 *out_index);
const char *g3d_body_get_bone_name(const g3d_body *body, g3d_s32 bone_index);
g3d_s32 g3d_body_bone_name_checksum(const g3d_body *body);
int g3d_body_compute_bbox_fix16(const g3d_body *body, g3d_fixvec3 *min_out, g3d_fixvec3 *max_out);
int g3d_body_find_material_index(const g3d_body *body, const char *material_name, g3d_s32 *out_index);
int g3d_body_find_material_index_prefix(const g3d_body *body, const char *material_prefix, g3d_s32 *out_index);
const char *g3d_body_get_material_name(const g3d_body *body, g3d_s32 material_index);
g3d_u32 g3d_body_count_textured_materials(const g3d_body *body);
int g3d_body_get_material_bitmap_info(const g3d_body *body, g3d_s32 material_index, g3d_bitmap_info *info);
int g3d_body_decode_material_rgba32(const g3d_body *body, g3d_s32 material_index, g3d_u32 wanted_mip,
                                    g3d_u32 decode_flags,
                                    g3d_u8 *dst_rgba, g3d_u32 dst_size,
                                    g3d_u32 *width_out, g3d_u32 *height_out,
                                    g3d_u32 *rgba_bytes_out);
int g3d_body_decode_material_rgba32_arena(const g3d_body *body, g3d_s32 material_index, g3d_u32 wanted_mip,
                                          g3d_u32 decode_flags,
                                          g3d_u8 *dst_rgba, g3d_u32 dst_size,
                                          g3d_u32 *width_out, g3d_u32 *height_out,
                                          g3d_u32 *rgba_bytes_out,
                                          g3d_arena *scratch_arena);
int g3d_body_validate_bitmaps(const g3d_body *body, g3d_s32 *bad_material_index);
int g3d_body_count_vertices_for_bone(const g3d_body *body, g3d_s32 bone_index, g3d_u32 *out_count);
int g3d_body_compute_bone_bbox_fix16(const g3d_body *body, g3d_s32 bone_index, g3d_fixvec3 *min_out, g3d_fixvec3 *max_out);
g3d_u32 g3d_body_lod_face_count(const g3d_body *body, g3d_u32 lod_index);
int g3d_actor_find_motion_index(const g3d_actor *actor, const char *motion_name, g3d_s32 *out_index);
const char *g3d_actor_get_motion_name(const g3d_actor *actor, g3d_s32 motion_index);
int g3d_motion_find_next_event_between_fix16(const g3d_motion *motion, g3d_fix16 start_time, g3d_fix16 end_time,
                                             g3d_s32 after_index, g3d_s32 *out_index,
                                             g3d_u32 *time_bits, const char **text);

void g3d_fixvec3_zero(g3d_fixvec3 *v);
void g3d_q15quat_identity(g3d_q15quat *q);
void g3d_sample_xform_identity(g3d_sample_xform *xf);
void g3d_fixmat34_identity(g3d_fixmat34 *m);

int g3d_path_get_time_extents_fix16(const g3d_path *path, g3d_fix16 *start_time, g3d_fix16 *end_time);
int g3d_motion_get_time_extents_fix16(const g3d_motion *motion, g3d_fix16 *start_time, g3d_fix16 *end_time);
int g3d_path_sample_channels_fix16(const g3d_path *path, g3d_fix16 time, g3d_q15quat *rotation, g3d_fixvec3 *translation);
int g3d_motion_sample_channels_fix16(const g3d_motion *motion, g3d_s32 path_index, g3d_fix16 time, g3d_q15quat *rotation, g3d_fixvec3 *translation);
int g3d_motion_sample_channels_named_fix16(const g3d_motion *motion, const char *path_name, g3d_fix16 time, g3d_q15quat *rotation, g3d_fixvec3 *translation);
void g3d_sample_xform_to_matrix(const g3d_q15quat *rotation, const g3d_fixvec3 *translation, g3d_fixmat34 *out_matrix);

int g3d_body_sample_local_pose_fix16(const g3d_body *body, const g3d_motion *motion, g3d_fix16 time,
                                     g3d_sample_xform *out_pose, g3d_u32 pose_count);
int g3d_body_blend_local_pose_fix16(const g3d_body *body,
                                    const g3d_motion *base_motion, g3d_fix16 base_time,
                                    const g3d_motion *blend_motion, g3d_fix16 blend_time,
                                    g3d_s32 blend_amount_q15, g3d_u32 blend_type,
                                    g3d_sample_xform *out_pose, g3d_u32 pose_count);
int g3d_body_local_pose_to_matrices_fix16(const g3d_body *body,
                                          const g3d_sample_xform *local_pose, g3d_u32 pose_count,
                                          g3d_fixmat34 *out_matrices, g3d_u32 matrix_count);
int g3d_body_skin_vertices_fix16(const g3d_body *body,
                                 const g3d_fixmat34 *bone_matrices, g3d_u32 matrix_count,
                                 g3d_fixvec3 *out_vertices, g3d_u32 vertex_count,
                                 g3d_fixvec3 *out_normals, g3d_u32 normal_count);
int g3d_body_alloc_skinned_mesh(g3d_arena *arena, const g3d_body *body, g3d_skinned_mesh *mesh);
int g3d_body_sample_skinned_fix16(const g3d_body *body, const g3d_motion *motion, g3d_fix16 time,
                                  g3d_skinned_mesh *mesh);
int g3d_body_sample_skinned_blend_fix16(const g3d_body *body,
                                        const g3d_motion *base_motion, g3d_fix16 base_time,
                                        const g3d_motion *blend_motion, g3d_fix16 blend_time,
                                        g3d_s32 blend_amount_q15, g3d_u32 blend_type,
                                        g3d_skinned_mesh *mesh);
int g3d_actor_sample_skinned_fix16(const g3d_actor *actor, g3d_s32 motion_index, g3d_fix16 time,
                                   g3d_skinned_mesh *mesh);
int g3d_actor_sample_skinned_blend_fix16(const g3d_actor *actor,
                                         g3d_s32 base_motion_index, g3d_fix16 base_time,
                                         g3d_s32 blend_motion_index, g3d_fix16 blend_time,
                                         g3d_s32 blend_amount_q15, g3d_u32 blend_type,
                                         g3d_skinned_mesh *mesh);

void g3d_asset_manager_init(g3d_asset_manager *mgr, g3d_asset_slot *slots, g3d_u32 capacity);
int g3d_asset_manager_add(g3d_asset_manager *mgr, g3d_arena *arena, const char *name, g3d_u32 type, void *object);
int g3d_asset_manager_upsert(g3d_asset_manager *mgr, g3d_arena *arena, const char *name, g3d_u32 type, void *object);
g3d_u32 g3d_asset_manager_count_type(const g3d_asset_manager *mgr, g3d_u32 type);
void *g3d_asset_manager_find(const g3d_asset_manager *mgr, const char *name, g3d_u32 type);

#ifdef __cplusplus
}
#endif

#endif
