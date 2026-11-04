#ifndef U3D3D_H
#define U3D3D_H

/*
    u3d3d - C89 Unreal Engine 1 *.3D vertex mesh codec.

    Target format:
      <name>_d.3d : mesh data, triangle table, UV bytes, material slot bytes
      <name>_a.3d : aniv data, vertex frames with packed coordinates
      <name>.uc   : optional UnrealScript import directives, parsed separately

    Design rules:
      - C89 only
      - caller supplied memory only
      - no floating point types
      - Q16.16 fixed point for decoded coordinates and scales
      - no dependency on the C runtime allocation API
*/

#include <limits.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if UCHAR_MAX != 0xffu
#error "u3d3d requires 8-bit unsigned char"
#endif

#if USHRT_MAX != 0xffffu
#error "u3d3d requires 16-bit unsigned short"
#endif

#if UINT_MAX == 0xffffffffu
typedef unsigned int u3d3d_u32;
typedef int u3d3d_i32;
#elif ULONG_MAX == 0xfffffffful
typedef unsigned long u3d3d_u32;
typedef long u3d3d_i32;
#else
#error "u3d3d requires a 32-bit unsigned integer type"
#endif

typedef unsigned char u3d3d_u8;
typedef signed char u3d3d_i8;
typedef unsigned short u3d3d_u16;
typedef signed short u3d3d_i16;

typedef u3d3d_i32 u3d3d_fx;

#define U3D3D_FX_SHIFT 16
#define U3D3D_FX_ONE ((u3d3d_fx)65536)
#define U3D3D_D_HEADER_SIZE 48u
#define U3D3D_D_TRI_SIZE 16u
#define U3D3D_A_HEADER_SIZE 4u
#define U3D3D_VERTEX_BYTES 4u

#define U3D3D_COORD_MIN_CLASSIC (-512)
#define U3D3D_COORD_MAX_CLASSIC (511)
#define U3D3D_COORD_MIN_Z_CLASSIC (-512)
#define U3D3D_COORD_MAX_Z_CLASSIC (511)

#define U3D3D_MF_NORMAL_OS 0u
#define U3D3D_MF_NORMAL_TS 1u
#define U3D3D_MF_TRANSLUCENT_TS 2u
#define U3D3D_MF_MASKED_TS 3u
#define U3D3D_MF_MODULATED_TS 4u
#define U3D3D_MF_WEAPON_PLACEHOLDER 8u

#define U3D3D_OK 0
#define U3D3D_ERR_NULL -1
#define U3D3D_ERR_TOO_SHORT -2
#define U3D3D_ERR_BAD_COUNT -3
#define U3D3D_ERR_CAPACITY -4
#define U3D3D_ERR_SIZE_MISMATCH -5
#define U3D3D_ERR_RANGE -6
#define U3D3D_ERR_FRAME -7
#define U3D3D_ERR_UNSUPPORTED -8
#define U3D3D_ERR_IO -9
#define U3D3D_ERR_PARSE -10

#define U3D3D_VARIANT_CLASSIC 0u
#define U3D3D_VARIANT_ION_STORM 1u

typedef struct u3d3d_triangle_s {
    u3d3d_u16 v[3];
    u3d3d_u8 type;
    u3d3d_u8 color;
    u3d3d_u8 uv[3][2];
    u3d3d_u8 texture_num;
    u3d3d_u8 flags;
} u3d3d_triangle;

typedef struct u3d3d_vertex_s {
    u3d3d_fx x;
    u3d3d_fx y;
    u3d3d_fx z;
} u3d3d_vertex;

typedef struct u3d3d_model_s {
    u3d3d_u16 vertex_count;
    u3d3d_u16 triangle_count;
    u3d3d_u16 frame_count;
    u3d3d_u16 frame_size;
    u3d3d_u8 variant;
    u3d3d_u8 reserved0;
    u3d3d_triangle *triangles;
    u3d3d_u16 triangle_capacity;
} u3d3d_model;

typedef struct u3d3d_uc_info_s {
    u3d3d_fx scale_x;
    u3d3d_fx scale_y;
    u3d3d_fx scale_z;
    u3d3d_u8 has_scale_x;
    u3d3d_u8 has_scale_y;
    u3d3d_u8 has_scale_z;
    u3d3d_u8 texture_slots_seen[256];
    u3d3d_u16 texture_slot_count;
} u3d3d_uc_info;

typedef struct u3d3d_diag_s {
    u3d3d_u32 d_expected_size;
    u3d3d_u32 a_expected_min_size;
    u3d3d_u32 a_expected_full_size;
    u3d3d_u16 max_vertex_index;
    u3d3d_u16 bad_triangle_indices;
    u3d3d_u16 weapon_placeholders;
    u3d3d_u8 max_texture_slot;
} u3d3d_diag;

u3d3d_fx u3d3d_fx_from_i32(u3d3d_i32 v);
u3d3d_i32 u3d3d_fx_to_i32_floor(u3d3d_fx v);
u3d3d_fx u3d3d_fx_mul(u3d3d_fx a, u3d3d_fx b);

u3d3d_u16 u3d3d_get_le16(const u3d3d_u8 *p);
u3d3d_u32 u3d3d_get_le32(const u3d3d_u8 *p);
void u3d3d_put_le16(u3d3d_u8 *p, u3d3d_u16 v);
void u3d3d_put_le32(u3d3d_u8 *p, u3d3d_u32 v);

u3d3d_i32 u3d3d_unpack_signed(u3d3d_u32 bits, unsigned width);
u3d3d_u32 u3d3d_pack_classic_vertex_i32(u3d3d_i32 x, u3d3d_i32 y, u3d3d_i32 z);
void u3d3d_unpack_classic_vertex(u3d3d_u32 packed, u3d3d_vertex *out_v);
int u3d3d_pack_classic_vertex_fx(const u3d3d_vertex *v, u3d3d_u32 *out_packed);

void u3d3d_model_init(u3d3d_model *model,
                      u3d3d_triangle *triangles,
                      u3d3d_u16 triangle_capacity);

int u3d3d_decode_d(const u3d3d_u8 *d_bytes,
                   u3d3d_u32 d_size,
                   u3d3d_model *model);

int u3d3d_decode_a_header(const u3d3d_u8 *a_bytes,
                          u3d3d_u32 a_size,
                          u3d3d_model *model);

int u3d3d_decode_a_frame(const u3d3d_u8 *a_bytes,
                         u3d3d_u32 a_size,
                         const u3d3d_model *model,
                         u3d3d_u16 frame_index,
                         u3d3d_vertex *vertices,
                         u3d3d_u16 vertex_capacity);

int u3d3d_decode_pair_frame(const u3d3d_u8 *d_bytes,
                            u3d3d_u32 d_size,
                            const u3d3d_u8 *a_bytes,
                            u3d3d_u32 a_size,
                            u3d3d_u16 frame_index,
                            u3d3d_model *model,
                            u3d3d_vertex *vertices,
                            u3d3d_u16 vertex_capacity);

int u3d3d_build_diag(const u3d3d_u8 *d_bytes,
                     u3d3d_u32 d_size,
                     const u3d3d_u8 *a_bytes,
                     u3d3d_u32 a_size,
                     const u3d3d_model *model,
                     u3d3d_diag *diag);

const char *u3d3d_error_string(int code);
const char *u3d3d_triangle_type_name(u3d3d_u8 type);

void u3d3d_uc_info_init(u3d3d_uc_info *info);
int u3d3d_parse_uc_info(const char *text, u3d3d_u32 text_size, u3d3d_uc_info *info);

#ifdef U3D3D_ENABLE_STDIO
#include <stdio.h>
int u3d3d_fread_all(const char *path, u3d3d_u8 *dst, u3d3d_u32 dst_capacity, u3d3d_u32 *out_size);
int u3d3d_export_obj(FILE *out,
                     const u3d3d_model *model,
                     const u3d3d_vertex *vertices,
                     const char *object_name,
                     const u3d3d_uc_info *uc_info,
                     unsigned flags);
void u3d3d_fprint_fx(FILE *out, u3d3d_fx v);
#endif

#ifdef __cplusplus
}
#endif

#endif
