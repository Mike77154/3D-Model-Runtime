#ifndef DTS89_SHADERER_H
#define DTS89_SHADERER_H
#include "dts89.h"
/* Renderer-neutral Torque material interpretation. Borrowed string slices.
   No ownership, allocation or I/O inside this module. */
#define DTS89_SHADERER_STAGES 4u
#define DTS89_SHADERER_NORMAL_RGB 0u
#define DTS89_SHADERER_NORMAL_DXT5_AG 1u
#define DTS89_SHADERER_CLAMP 0u
#define DTS89_SHADERER_WRAP 1u
#define DTS89_SHADERER_BLEND_OPAQUE 0u
#define DTS89_SHADERER_BLEND_ALPHA 1u
#define DTS89_SHADERER_BLEND_ADD 2u
#define DTS89_SHADERER_BLEND_SUB 3u
#define DTS89_SHADERER_BLEND_MUL 4u
#define DTS89_SHADERER_BLEND_PREMUL 5u
#define DTS89_SHADERER_BLEND_ADD_ALPHA 6u
#define DTS89_SHADERER_BLEND_LERP_ALPHA 7u
#define DTS89_SHADERER_BLEND_TONE 8u
#define DTS89_SHADERER_MIPMAP 0u
#define DTS89_SHADERER_MIP_NONE 1u
#define DTS89_SHADERER_MIP_ZERO_BORDER 2u
#define DTS89_SHADERER_MAP_DIFFUSE 0x00000001u
#define DTS89_SHADERER_MAP_NORMAL 0x00000002u
#define DTS89_SHADERER_MAP_SPECULAR 0x00000004u
#define DTS89_SHADERER_MAP_DETAIL 0x00000008u
#define DTS89_SHADERER_MAP_DETAIL_NORMAL 0x00000010u
#define DTS89_SHADERER_MAP_OVERLAY 0x00000020u
#define DTS89_SHADERER_MAP_GLOW 0x00000040u
#define DTS89_SHADERER_MAP_AO 0x00000080u
#define DTS89_SHADERER_MAP_ROUGH 0x00000100u
#define DTS89_SHADERER_MAP_METAL 0x00000200u
#define DTS89_SHADERER_MAP_ORM 0x00000400u
#define DTS89_SHADERER_CAP_NORMAL_RENDER 0x00000001u
#define DTS89_SHADERER_CAP_SPECULAR_RENDER 0x00000002u
#define DTS89_SHADERER_CAP_DETAIL_RENDER 0x00000004u
#define DTS89_SHADERER_CAP_MULTI_STAGE 0x00000008u
#define DTS89_SHADERER_CAP_ANIM_UV 0x00000010u
#define DTS89_SHADERER_CAP_ENV_MAP 0x00000020u
#define DTS89_SHADERER_CAP_TONE_MAP 0x00000040u
#define DTS89_SHADERER_CAP_SCRIPT_UNKNOWN 0x00000080u
#define DTS89_SHADERER_CAP_ALPHA_TEST 0x00000100u
#define DTS89_SHADERER_CAP_REFLECTANCE 0x00000200u
#define DTS89_SHADERER_CAP_PARALLAX 0x00000400u
#define DTS89_SHADERER_CAP_COLORSPACE 0x00000800u
#define DTS89_SHADERER_CAP_TOO_MANY_STAGES 0x00001000u

typedef struct dts89_shaderer_stage {
 dts89_string diffuse,normal,specular,detail,detail_normal,overlay,glow;
 dts89_string ao,rough,metal,orm;
 dts89_fx16_16 diffuse_color[4];
 dts89_fx16_16 specular_color[4];
 dts89_fx16_16 specular_power,detail_scale;
 dts89_u32 maps;
 dts89_u32 glow_enabled,pixel_specular;
 dts89_u32 anim_flags;
} dts89_shaderer_stage;
typedef struct dts89_shaderer_material {
 dts89_string name,map_to;
 dts89_u32 raw_flags;
 dts89_u32 sampler_s,sampler_t,mipmap,blend;
 dts89_u32 translucent,alpha_test,alpha_ref,double_sided,self_illuminating,never_envmap;
 dts89_u32 legacy_reflectance,legacy_bump,legacy_detail;
 dts89_fx16_16 legacy_detail_scale,legacy_reflection;
 dts89_u32 script_matched,unsupported_features,unknown_properties;
 dts89_shaderer_stage stage[DTS89_SHADERER_STAGES];
} dts89_shaderer_material;
typedef struct dts89_shaderer_rgba8 { dts89_u8 r,g,b,a; } dts89_shaderer_rgba8;

int dts89_shaderer_from_dts(const dts89_shape *shape,dts89_s32 idx,dts89_shaderer_material *out);
int dts89_shaderer_apply_script(dts89_shaderer_material *mat,const dts89_ts_file *script);
int dts89_shaderer_build(const dts89_shape *shape,dts89_s32 idx,
 const dts89_ts_file *script,dts89_shaderer_material *out);
/* Addresses Q16 UVs without modifying raw DTS values. Output [0,65536] for
   CLAMP and [0,65535] for WRAP (including negative and repeated tiles). */
int dts89_shaderer_address_uv(dts89_fx_vec2 uv,const dts89_shaderer_material *mat,
 dts89_fx_vec2 *out);
/* Fully caller-owned RGBA8 pixels, top-left origin; stride in bytes. */
int dts89_shaderer_sample_rgba8(const dts89_u8 *pixels,dts89_u32 width,
 dts89_u32 height,dts89_u32 stride,dts89_fx_vec2 torque_uv,
 const dts89_shaderer_material *mat,dts89_shaderer_rgba8 *out);
/* Decode packed tangent-space normals in integer Q16; AG represents DXT5nm. */
int dts89_shaderer_detect_normal_packing(const dts89_u8 *rgba,dts89_u32 width,
 dts89_u32 height,dts89_u32 stride,dts89_u32 capacity,dts89_u32 *out_mode);
int dts89_shaderer_unpack_normal(dts89_shaderer_rgba8 pixel,dts89_u32 mode,
 dts89_fx_vec3 *normal);
int dts89_shaderer_blend_rgba8(dts89_shaderer_rgba8 src,
 dts89_shaderer_rgba8 dst,dts89_u32 mode,dts89_shaderer_rgba8 *out);
#endif
