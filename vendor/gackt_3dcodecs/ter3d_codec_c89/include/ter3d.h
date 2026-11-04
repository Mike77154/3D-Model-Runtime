#ifndef TER3D_H_INCLUDED
#define TER3D_H_INCLUDED

/*
   ter3d - heapless C89 Terragen/SWBF .TER terrain codec
   Public domain / CC0-style dedication by generated package author.

   Design rules:
   - C89 API.
   - No malloc/free/realloc/heap ownership.
   - No float/double API and no float/double implementation.
   - Fixed point Q16.16 for physical scales and emitted coordinates.
   - Input/output is caller-owned memory or callbacks.
*/

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TER3D_VERSION_MAJOR 0
#define TER3D_VERSION_MINOR 3
#define TER3D_VERSION_PATCH 0

#define TER3D_Q 16
#define TER3D_ONE ((ter3d_fixed)65536L)
#define TER3D_DEFAULT_TERRAGEN_SCALE_Q16 ((ter3d_fixed)(30L * 65536L))
#define TER3D_MAX_U16 65535U
#define TER3D_MAX_S16 32767
#define TER3D_MIN_S16 (-32768)

#ifndef TER3D_MAX_CHUNKS
#define TER3D_MAX_CHUNKS 96
#endif

#ifndef TER3D_MAX_SCAN_BYTES
#define TER3D_MAX_SCAN_BYTES 0xffffffffUL
#endif

typedef signed char ter3d_s8;
typedef unsigned char ter3d_u8;
typedef signed short ter3d_s16;
typedef unsigned short ter3d_u16;
typedef signed long ter3d_s32;
typedef unsigned long ter3d_u32;
typedef ter3d_s32 ter3d_fixed;

typedef enum ter3d_result_e {
    TER3D_OK = 0,
    TER3D_E_NULL = -1,
    TER3D_E_RANGE = -2,
    TER3D_E_FORMAT = -3,
    TER3D_E_NEED_MORE = -4,
    TER3D_E_UNSUPPORTED = -5,
    TER3D_E_BAD_ORDER = -6,
    TER3D_E_OVERFLOW = -7,
    TER3D_E_OUTPUT_SMALL = -8,
    TER3D_E_CALLBACK = -9,
    TER3D_E_NOT_FOUND = -10
} ter3d_result;

typedef enum ter3d_family_e {
    TER3D_FAMILY_UNKNOWN = 0,
    TER3D_FAMILY_TERRAGEN = 1,
    TER3D_FAMILY_SWBF = 2
} ter3d_family;

typedef enum ter3d_chunk_id_e {
    TER3D_CHUNK_UNKNOWN = 0,
    TER3D_CHUNK_SIZE,
    TER3D_CHUNK_XPTS,
    TER3D_CHUNK_YPTS,
    TER3D_CHUNK_SCAL,
    TER3D_CHUNK_CRAD,
    TER3D_CHUNK_CRVM,
    TER3D_CHUNK_ALTW,
    TER3D_CHUNK_EOF
} ter3d_chunk_id;

typedef struct ter3d_chunk_rec_s {
    char marker[4];
    ter3d_chunk_id id;
    ter3d_u32 offset;
    ter3d_u32 data_offset;
    ter3d_u32 data_size;
} ter3d_chunk_rec;

typedef struct ter3d_terragen_info_s {
    ter3d_u16 size_value;
    ter3d_u16 xpts;
    ter3d_u16 ypts;
    int has_size;
    int has_xpts;
    int has_ypts;
    int has_scal;
    int has_crad;
    int has_crvm;
    int has_altw;
    int has_eof;

    ter3d_fixed scale_x_q16;
    ter3d_fixed scale_y_q16;
    ter3d_fixed scale_z_q16;
    ter3d_fixed crad_km_q16;
    ter3d_u32 curve_mode;

    ter3d_s16 height_scale;
    ter3d_s16 base_height;
    ter3d_u32 altw_offset;
    ter3d_u32 elevations_offset;
    ter3d_u32 elevation_count;
    ter3d_u32 eof_offset;

    ter3d_chunk_rec chunks[TER3D_MAX_CHUNKS];
    ter3d_u16 chunk_count;
    ter3d_u32 warning_flags;
} ter3d_terragen_info;

typedef struct ter3d_swbf_layer_s {
    char diffuse[32];
    char detail[32];
} ter3d_swbf_layer;

typedef struct ter3d_swbf_water_s {
    ter3d_fixed height0_q16;
    ter3d_fixed height1_q16;
    ter3d_fixed uv_vel_u_q16;
    ter3d_fixed uv_vel_v_q16;
    ter3d_fixed uv_repeat_u_q16;
    ter3d_fixed uv_repeat_v_q16;
    ter3d_u8 rgba[4];
    char texture[32];
} ter3d_swbf_water;

typedef struct ter3d_swbf_info_s {
    int valid;
    ter3d_u32 version;
    ter3d_s16 extent_min_x;
    ter3d_s16 extent_min_y;
    ter3d_s16 extent_max_x;
    ter3d_s16 extent_max_y;
    ter3d_fixed map_height_multiplier_q16;
    ter3d_fixed grid_scale_q16;
    ter3d_u32 full_map_size;
    ter3d_u32 height_offset;
    ter3d_u32 color0_offset;
    ter3d_u32 color1_offset;
    ter3d_u32 texture_alpha_offset;
    ter3d_u32 water_offset;
    ter3d_u32 foliage_offset;
    ter3d_u32 header_size;
    ter3d_swbf_layer layers[16];
    ter3d_swbf_water water[16];
    ter3d_u32 warning_flags;
} ter3d_swbf_info;

typedef struct ter3d_info_s {
    ter3d_family family;
    ter3d_terragen_info terragen;
    ter3d_swbf_info swbf;
} ter3d_info;

typedef struct ter3d_bounds_s {
    ter3d_fixed min_z_q16;
    ter3d_fixed max_z_q16;
    ter3d_fixed min_x_q16;
    ter3d_fixed max_x_q16;
    ter3d_fixed min_y_q16;
    ter3d_fixed max_y_q16;
} ter3d_bounds;

typedef struct ter3d_mesh_vertex_s {
    ter3d_u32 index;
    ter3d_u16 ix;
    ter3d_u16 iy;
    ter3d_fixed x_q16;
    ter3d_fixed y_q16;
    ter3d_fixed z_q16;
} ter3d_mesh_vertex;

typedef struct ter3d_mesh_tri_s {
    ter3d_u32 index;
    ter3d_u32 a;
    ter3d_u32 b;
    ter3d_u32 c;
} ter3d_mesh_tri;

typedef int (*ter3d_vertex_cb)(void *user, const ter3d_mesh_vertex *v);
typedef int (*ter3d_tri_cb)(void *user, const ter3d_mesh_tri *t);

typedef struct ter3d_mesh_callbacks_s {
    ter3d_vertex_cb vertex;
    ter3d_tri_cb triangle;
    void *user;
    int emit_world_meters;
    int flip_y;
} ter3d_mesh_callbacks;

typedef struct ter3d_write_desc_s {
    ter3d_u16 xpts;
    ter3d_u16 ypts;
    ter3d_s16 height_scale;
    ter3d_s16 base_height;
    ter3d_fixed scale_x_q16;
    ter3d_fixed scale_y_q16;
    ter3d_fixed scale_z_q16;
    const ter3d_s16 *elevations;
    int write_scal;
} ter3d_write_desc;

const char *ter3d_result_name(ter3d_result r);

ter3d_u16 ter3d_read_le16u(const ter3d_u8 *p);
ter3d_s16 ter3d_read_le16s(const ter3d_u8 *p);
ter3d_u32 ter3d_read_le32u(const ter3d_u8 *p);
void ter3d_write_le16u(ter3d_u8 *p, ter3d_u16 v);
void ter3d_write_le32u(ter3d_u8 *p, ter3d_u32 v);

ter3d_fixed ter3d_fixed_from_int(ter3d_s32 v);
ter3d_s32 ter3d_fixed_to_int_floor(ter3d_fixed v);
ter3d_fixed ter3d_fixed_add_sat(ter3d_fixed a, ter3d_fixed b);
ter3d_fixed ter3d_fixed_mul_sat(ter3d_fixed a, ter3d_fixed b);
ter3d_fixed ter3d_ieee754_le_to_q16(const ter3d_u8 *p);
ter3d_u32 ter3d_q16_to_ieee754_bits(ter3d_fixed q);

ter3d_family ter3d_probe_family(const ter3d_u8 *data, ter3d_u32 size);
ter3d_result ter3d_read_info(const ter3d_u8 *data, ter3d_u32 size, ter3d_info *out_info);

ter3d_result ter3d_terragen_read_info(const ter3d_u8 *data, ter3d_u32 size, ter3d_terragen_info *out_info);
ter3d_result ter3d_terragen_get_height_q16(const ter3d_u8 *data, ter3d_u32 size, const ter3d_terragen_info *info, ter3d_u16 x, ter3d_u16 y, ter3d_fixed *out_height_q16);
ter3d_result ter3d_terragen_raw_elevation(const ter3d_u8 *data, ter3d_u32 size, const ter3d_terragen_info *info, ter3d_u16 x, ter3d_u16 y, ter3d_s16 *out_elevation);
ter3d_result ter3d_terragen_bounds(const ter3d_u8 *data, ter3d_u32 size, const ter3d_terragen_info *info, ter3d_bounds *out_bounds);
ter3d_result ter3d_terragen_emit_mesh(const ter3d_u8 *data, ter3d_u32 size, const ter3d_terragen_info *info, const ter3d_mesh_callbacks *cb);
ter3d_result ter3d_terragen_write(const ter3d_write_desc *desc, ter3d_u8 *out, ter3d_u32 out_cap, ter3d_u32 *out_size);

ter3d_result ter3d_swbf_read_info(const ter3d_u8 *data, ter3d_u32 size, ter3d_swbf_info *out_info);
ter3d_result ter3d_swbf_raw_height(const ter3d_u8 *data, ter3d_u32 size, const ter3d_swbf_info *info, ter3d_u32 x, ter3d_u32 y, ter3d_s16 *out_height);
ter3d_result ter3d_swbf_height_q16(const ter3d_u8 *data, ter3d_u32 size, const ter3d_swbf_info *info, ter3d_u32 x, ter3d_u32 y, ter3d_fixed *out_height_q16);
ter3d_result ter3d_swbf_color0_rgba(const ter3d_u8 *data, ter3d_u32 size, const ter3d_swbf_info *info, ter3d_u32 x, ter3d_u32 y, ter3d_u8 rgba[4]);
ter3d_result ter3d_swbf_texture_alpha16(const ter3d_u8 *data, ter3d_u32 size, const ter3d_swbf_info *info, ter3d_u32 x, ter3d_u32 y, ter3d_u8 alpha16[16]);

#ifdef __cplusplus
}
#endif

#endif
