#ifndef DTS89_LIGHTING40_H
#define DTS89_LIGHTING40_H
#include "dts89_shaderer.h"
#include "dts89_pose37.h"
/* Renderer-independent, caller-owned RGBA8 texture views. All coordinates and
   lighting values are Q16.16. Light and view directions point toward light
   and camera. The input normal and basis vectors must have unit length. */
typedef struct dts89_lit40_image {
 const dts89_u8 *rgba;
 dts89_u32 width,height,stride;
} dts89_lit40_image;
typedef struct dts89_lit40_maps {
 const dts89_lit40_image *diffuse;
 const dts89_lit40_image *normal;
 const dts89_lit40_image *specular;
 dts89_u32 normal_packing;
} dts89_lit40_maps;
typedef struct dts89_lit40_light {
 dts89_fx_vec3 to_light,to_view;
 dts89_fx16_16 ambient,diffuse,specular;
} dts89_lit40_light;
typedef struct dts89_lit40_basis {
 dts89_fx_vec3 n,t,b;
} dts89_lit40_basis;
/* Recover UV address from raw DTS IEEE32 bits by integer bitfield math,
   including inputs too large for Q16.16. Non-finite values are rejected. */
int dts89_lit40_address_raw_uv(dts89_u32 ieee_bits,dts89_u32 address_mode,
 dts89_fx16_16 *out);
/* Geometry uses positions/normals AFTER pose/skinning, with DTS-native UV. */
int dts89_lit40_triangle_basis(const dts89_fx_vec3 p[3],
 const dts89_fx_vec2 uv[3],const dts89_fx_vec3 normals[3],
 dts89_lit40_basis *out);
/* Normal, tangent and bitangent are expected in the posed world space.
   Single directional-light, stage-zero, nearest-sampling reference shading.
   Not a complete Torque shader/GFX recreation. */
int dts89_lit40_shade(dts89_fx_vec2 uv,const dts89_lit40_basis *basis,
 const dts89_shaderer_material *material,const dts89_lit40_maps *maps,
 const dts89_lit40_light *light,dts89_shaderer_rgba8 *out);
/* Integrates Posefeamw/Animer + Mesher; all output arrays owned by caller.
   Must have run dts89_pose37_evaluate or dts89_animer_evaluate first.
   UV conversion is NOT performed; shaderer samples native top-left UV. */
int dts89_lit40_pose_mesh(const dts89_shape *shape,
 const dts89_mesh_view *mesh,dts89_s32 object_node,
 const dts89_shaderer_material *material,
 const dts89_fx_mat34 *globals,dts89_u32 globals_count,
 dts89_fx_vec3 *positions,dts89_fx_vec3 *normals,
 dts89_fx_vec2 *uvs,dts89_fx16_16 *weight_sums,
 dts89_u32 vertex_capacity,dts89_fx_mat34 *palette,
 dts89_u32 palette_capacity);
#endif
