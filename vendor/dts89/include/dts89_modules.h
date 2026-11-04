#ifndef DTS89_MODULES_H
#define DTS89_MODULES_H
#include "dts89.h"
typedef struct dts89_mod_image { dts89_u32 width,height,format,data_offset; } dts89_mod_image;
typedef struct dts89_mod_vertex { dts89_fx_vec3 position; dts89_fx_vec2 uv; } dts89_mod_vertex;
int dts89_texturer_opener_probe(const dts89_u8 *data,dts89_u32 size,dts89_mod_image *out);
#define DTS89_TEX_BC1 1u
#define DTS89_TEX_BC3 3u
#define DTS89_TEX_BMP24 24u
#define DTS89_TEX_BMP32 32u
typedef struct dts89_mod_texture_info {
 dts89_u32 width,height,mip_count,encoding,level0_offset,level0_bytes;
 dts89_u32 source_stride,source_bottom_up;
} dts89_mod_texture_info;
/* Read-only decode of DDS BC1/BC3 or uncompressed BMP 24/32 (level zero).
   RGBA8 output is caller owned, top-left rows; PNG remains probe-only. */
int dts89_texturer_opener_info(const dts89_u8 *data,dts89_u32 size,
 dts89_mod_texture_info *out);
int dts89_texturer_opener_decode_rgba8(const dts89_u8 *data,dts89_u32 size,
 const dts89_mod_texture_info *info,dts89_u8 *rgba,dts89_u32 stride,
 dts89_u32 output_capacity);

/* UV convention: raw Torque top-left, OBJ bottom-left. */
#define DTS89_UV_TORQUE_TOP_LEFT 0
#define DTS89_UV_OBJ_BOTTOM_LEFT 1
int dts89_uv_texturer_get(const dts89_mesh_view *mesh,dts89_s32 layer,
 dts89_s32 index,int convention,dts89_fx_vec2 *uv);
int dts89_uv_texturer_convert(const dts89_fx_vec2 *in_uv,int convention,
 dts89_fx_vec2 *out_uv);
int dts89_uv_texturer_sample(const dts89_mesh_view *mesh,dts89_s32 index,dts89_fx_vec2 *uv);
int dts89_mesher_sample(const dts89_mesh_view *mesh,dts89_s32 index,dts89_mod_vertex *out);
int dts89_posefeamw_lerp(dts89_fx_vec3 a,dts89_fx_vec3 b,dts89_fx16_16 t,dts89_fx_vec3 *out);
int dts89_animer_cursor(const dts89_sequence *seq,dts89_fx16_16 time,dts89_sequence_cursor *out);
#endif
