#ifndef MD3_TYPES_H
#define MD3_TYPES_H

#include <stdio.h>
#include <limits.h>

#if CHAR_BIT != 8
#error "md3 codec requires 8-bit bytes"
#endif

#if UINT_MAX != 0xFFFFFFFFUL
#error "md3 codec requires 32-bit unsigned int"
#endif

#if USHRT_MAX != 0xFFFFU
#error "md3 codec requires 16-bit unsigned short"
#endif

typedef unsigned char md3_u8;
typedef signed char md3_i8;
typedef unsigned short md3_u16;
typedef signed short md3_i16;
typedef unsigned int md3_u32;
typedef signed int md3_i32;
typedef md3_i32 md3_fixed_t;

#define MD3_MAGIC_STR "IDP3"
#define MD3_IDENT_U32 0x33504449UL
#define MD3_VERSION 15

#define MD3_MAX_QPATH 64
#define MD3_MAX_FRAMES 1024
#define MD3_MAX_TAGS 16
#define MD3_MAX_SURFACES 32
#define MD3_MAX_SHADERS 256
#define MD3_MAX_VERTS 4096
#define MD3_MAX_TRIANGLES 8192

#define MD3_HEADER_SIZE 108
#define MD3_FRAME_SIZE 56
#define MD3_TAG_SIZE 112
#define MD3_SURFACE_HEADER_SIZE 108
#define MD3_SHADER_SIZE 68
#define MD3_TRIANGLE_SIZE 12
#define MD3_ST_SIZE 8
#define MD3_XYZNORMAL_SIZE 8

#define MD3_FRAME_FLOAT_COUNT 10
#define MD3_TAG_FLOAT_COUNT 12
#define MD3_ST_FLOAT_COUNT 2

#define MD3_FIXED_ONE 65536
#define MD3_FIXED_HALF 32768
#define MD3_XYZ_SCALE_Q16 1024

enum
{
    MD3_OK = 0,
    MD3_EINVAL = -1,
    MD3_EIO = -2,
    MD3_EFORMAT = -3,
    MD3_ERANGE = -4,
    MD3_ENOSPACE = -5,
    MD3_ESTATE = -6
};

typedef struct md3_vec3_s
{
    md3_fixed_t x;
    md3_fixed_t y;
    md3_fixed_t z;
} md3_vec3_t;

typedef struct md3_frame_s
{
    md3_vec3_t bounds_min;
    md3_vec3_t bounds_max;
    md3_vec3_t local_origin;
    md3_fixed_t radius;
    char name[16];
    md3_u32 raw_f32[MD3_FRAME_FLOAT_COUNT];
    int has_raw_f32;
} md3_frame_t;

typedef struct md3_tag_s
{
    char name[MD3_MAX_QPATH];
    md3_vec3_t origin;
    md3_vec3_t axis[3];
    md3_u32 raw_f32[MD3_TAG_FLOAT_COUNT];
    int has_raw_f32;
} md3_tag_t;

typedef struct md3_shader_s
{
    char name[MD3_MAX_QPATH];
    md3_i32 shader_index;
} md3_shader_t;

typedef struct md3_triangle_s
{
    md3_i32 index[3];
} md3_triangle_t;

typedef struct md3_st_s
{
    md3_fixed_t s;
    md3_fixed_t t;
    md3_u32 raw_f32[MD3_ST_FLOAT_COUNT];
    int has_raw_f32;
} md3_st_t;

typedef struct md3_vertex_s
{
    md3_vec3_t position;
    md3_vec3_t normal;
    md3_u16 packed_normal;
    int has_packed_normal;
} md3_vertex_t;

typedef struct md3_header_info_s
{
    char name[MD3_MAX_QPATH];
    md3_i32 flags;
    md3_i32 num_frames;
    md3_i32 num_tags;
    md3_i32 num_surfaces;
    md3_i32 num_skins;
    md3_i32 ofs_frames;
    md3_i32 ofs_tags;
    md3_i32 ofs_surfaces;
    md3_i32 ofs_end;
} md3_header_info_t;

typedef struct md3_surface_info_s
{
    char name[MD3_MAX_QPATH];
    md3_i32 flags;
    md3_i32 num_frames;
    md3_i32 num_shaders;
    md3_i32 num_verts;
    md3_i32 num_triangles;
    md3_i32 ofs_triangles;
    md3_i32 ofs_shaders;
    md3_i32 ofs_st;
    md3_i32 ofs_xyznormals;
    md3_i32 ofs_end;
    md3_i32 file_ofs;
} md3_surface_info_t;

typedef struct md3_frame_accumulator_s
{
    int has_any_vertex;
    md3_vec3_t min_value;
    md3_vec3_t max_value;
} md3_frame_accumulator_t;

typedef struct md3_decoder_s
{
    FILE *fp;
    long file_size;
    md3_header_info_t header;
    md3_surface_info_t surfaces[MD3_MAX_SURFACES];
    int is_open;
} md3_decoder_t;

typedef struct md3_encoder_s
{
    FILE *fp;
    md3_header_info_t header;
    md3_surface_info_t surfaces[MD3_MAX_SURFACES];
    md3_i32 next_frame_to_write;
    md3_i32 next_tag_frame_to_write;
    md3_i32 next_surface_to_write;
    md3_i32 active_surface;
    md3_i32 surface_vertex_frames_written;
    md3_i32 surface_shaders_written;
    md3_i32 surface_triangles_written;
    md3_i32 surface_st_written;
    int stage;
} md3_encoder_t;

const char *md3_error_string(int code);

#endif
