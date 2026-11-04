#ifndef DFF3D_H
#define DFF3D_H

#include <stdio.h>
#include "dff3d_types.h"
#include "dff3d_config.h"
#include "dff3d_fixed.h"

#define DFF3D_OK 0
#define DFF3D_ERR_NULL -1
#define DFF3D_ERR_BAD_STREAM -2
#define DFF3D_ERR_NOT_DFF -3
#define DFF3D_ERR_OVERFLOW -4

#define DFF3D_SEC_STRUCT 0x00000001UL
#define DFF3D_SEC_STRING 0x00000002UL
#define DFF3D_SEC_EXTENSION 0x00000003UL
#define DFF3D_SEC_TEXTURE 0x00000006UL
#define DFF3D_SEC_MATERIAL 0x00000007UL
#define DFF3D_SEC_MATERIAL_LIST 0x00000008UL
#define DFF3D_SEC_FRAME_LIST 0x0000000eUL
#define DFF3D_SEC_GEOMETRY 0x0000000fUL
#define DFF3D_SEC_CLUMP 0x00000010UL
#define DFF3D_SEC_ATOMIC 0x00000014UL
#define DFF3D_SEC_TEXTURE_NATIVE 0x00000015UL
#define DFF3D_SEC_TEXTURE_DICTIONARY 0x00000016UL
#define DFF3D_SEC_GEOMETRY_LIST 0x0000001aUL
#define DFF3D_SEC_RIGHT_TO_RENDER 0x0000001fUL
#define DFF3D_SEC_UV_ANIM_DICT 0x0000002bUL

#define DFF3D_PLG_MORPH 0x00000105UL
#define DFF3D_PLG_ANIMATION 0x00000108UL
#define DFF3D_PLG_BONE 0x0000010eUL
#define DFF3D_PLG_SKIN 0x00000116UL
#define DFF3D_PLG_HANIM 0x0000011eUL
#define DFF3D_PLG_USER_DATA 0x0000011fUL
#define DFF3D_PLG_MATFX 0x00000120UL
#define DFF3D_PLG_DELTA_MORPH 0x00000122UL
#define DFF3D_PLG_UV_ANIM 0x00000135UL
#define DFF3D_PLG_NTL_MATERIAL 0x00000177UL
#define DFF3D_PLG_BIN_MESH 0x0000050eUL
#define DFF3D_PLG_NATIVE_DATA 0x00000510UL
#define DFF3D_PLG_SKYGFX 0x0000ededUL
#define DFF3D_PLG_PIPELINE_SET 0x0253f2f3UL
#define DFF3D_PLG_RSTAR_SPECULAR 0x0253f2f6UL
#define DFF3D_PLG_RSTAR_2DFX 0x0253f2f8UL
#define DFF3D_PLG_RSTAR_NIGHT_COLORS 0x0253f2f9UL
#define DFF3D_PLG_RSTAR_COLLISION 0x0253f2faUL
#define DFF3D_PLG_RSTAR_REFLECTION 0x0253f2fcUL
#define DFF3D_PLG_RSTAR_FRAME 0x0253f2feUL
#define DFF3D_PLG_SAMP_COLLISION 0x0253f2ffUL

#define DFF3D_GEOM_TRISTRIP 0x00000001UL
#define DFF3D_GEOM_POSITIONS 0x00000002UL
#define DFF3D_GEOM_TEXTURED 0x00000004UL
#define DFF3D_GEOM_PRELIT 0x00000008UL
#define DFF3D_GEOM_NORMALS 0x00000010UL
#define DFF3D_GEOM_LIGHT 0x00000020UL
#define DFF3D_GEOM_MODULATE 0x00000040UL
#define DFF3D_GEOM_TEXTURED2 0x00000080UL
#define DFF3D_GEOM_NATIVE 0x01000000UL
#define DFF3D_GEOM_NATIVE_INSTANCE 0x02000000UL

#define DFF3D_NATIVE_D3D7 0x00000001UL
#define DFF3D_NATIVE_OPENGL 0x00000002UL
#define DFF3D_NATIVE_MAC 0x00000003UL
#define DFF3D_NATIVE_PS2 0x00000004UL
#define DFF3D_NATIVE_XBOX 0x00000005UL
#define DFF3D_NATIVE_GC 0x00000006UL
#define DFF3D_NATIVE_SOFT 0x00000007UL
#define DFF3D_NATIVE_D3D8 0x00000008UL
#define DFF3D_NATIVE_D3D9 0x00000009UL
#define DFF3D_NATIVE_PSP 0x0000000aUL
#define DFF3D_NATIVE_PS2_FOURCC 0x00325350UL

typedef struct Dff3dVec2Tag {
    dff_fixed u;
    dff_fixed v;
} Dff3dVec2;

typedef struct Dff3dVec3Tag {
    dff_fixed x;
    dff_fixed y;
    dff_fixed z;
} Dff3dVec3;

typedef struct Dff3dColorTag {
    dff_u8 r;
    dff_u8 g;
    dff_u8 b;
    dff_u8 a;
} Dff3dColor;

typedef struct Dff3dVertexTag {
    Dff3dVec3 pos;
    Dff3dVec3 normal;
    Dff3dVec2 uv0;
    Dff3dVec2 uv1;
    Dff3dColor color;
    dff_u8 skin_bone[4];
    dff_fixed skin_weight[4];
    dff_u8 has_pos;
    dff_u8 has_normal;
    dff_u8 has_uv0;
    dff_u8 has_uv1;
    dff_u8 has_color;
    dff_u8 has_skin;
} Dff3dVertex;

typedef struct Dff3dTriangleTag {
    dff_u16 a;
    dff_u16 b;
    dff_u16 c;
    dff_u16 material;
    dff_u32 geometry;
} Dff3dTriangle;

typedef struct Dff3dMaterialTag {
    Dff3dColor color;
    dff_fixed ambient;
    dff_fixed specular;
    dff_fixed diffuse;
    dff_u32 flags;
    dff_u32 textured;
    dff_u32 matfx_index;
    dff_u32 matfx_count;
    dff_u32 specular_index;
    dff_fixed specular_level;
    dff_fixed reflection_intensity;
    char texture[DFF3D_MAX_TEXTURE_NAME];
    char mask[DFF3D_MAX_TEXTURE_NAME];
} Dff3dMaterial;

typedef struct Dff3dFrameTag {
    Dff3dVec3 right;
    Dff3dVec3 up;
    Dff3dVec3 at;
    Dff3dVec3 pos;
    Dff3dVec3 world_right;
    Dff3dVec3 world_up;
    Dff3dVec3 world_at;
    Dff3dVec3 world_pos;
    dff_i32 parent;
    dff_u32 flags;
    dff_u32 hanim_bone_count;
    dff_u32 hanim_bone_offset;
    char name[DFF3D_MAX_TEXTURE_NAME];
} Dff3dFrame;

typedef struct Dff3dAtomicTag {
    dff_u32 frame_index;
    dff_u32 geometry_index;
    dff_u32 flags;
    dff_u32 unused;
} Dff3dAtomic;

typedef struct Dff3dGeometryTag {
    dff_u32 flags;
    dff_u32 uv_layers;
    dff_u32 morph_targets;
    dff_u32 declared_vertex_count;
    dff_u32 declared_triangle_count;
    dff_u32 vertex_offset;
    dff_u32 vertex_count;
    dff_u32 triangle_offset;
    dff_u32 triangle_count;
    dff_u32 material_offset;
    dff_u32 material_count;
    Dff3dVec3 sphere_center;
    dff_fixed sphere_radius;
    dff_u32 has_positions;
    dff_u32 has_normals;
    dff_u32 native_data;
    dff_u32 native_info_index;
    dff_u32 bin_mesh_offset;
    dff_u32 bin_mesh_count;
    dff_u32 bin_mesh_indices;
    dff_u32 skin_bone_count;
    dff_u32 skin_used_bone_count;
    dff_u32 skin_max_weights;
} Dff3dGeometry;

typedef struct Dff3dBinMeshTag {
    dff_u32 geometry;
    dff_u32 material;
    dff_u32 index_offset;
    dff_u32 index_count;
    dff_u32 is_tristrip;
} Dff3dBinMesh;

typedef struct Dff3dSkinBoneTag {
    dff_u32 geometry;
    dff_u32 bone_index;
    dff_u32 used_bone_id;
    dff_fixed matrix[16];
} Dff3dSkinBone;

typedef struct Dff3dHAnimBoneTag {
    dff_u32 frame;
    dff_i32 bone_id;
    dff_i32 bone_index;
    dff_i32 bone_type;
} Dff3dHAnimBone;

typedef struct Dff3dMatFxTag {
    dff_u32 material;
    dff_u32 effect_type;
    dff_u32 src_blend;
    dff_u32 dst_blend;
    dff_fixed coefficient;
    dff_fixed intensity;
    dff_u32 flags;
} Dff3dMatFx;

typedef struct Dff3dNativeInfoTag {
    dff_u32 geometry;
    dff_u32 platform;
    dff_u32 raw_size;
    dff_u32 version_stamp;
    dff_u32 uninstance_status;
    dff_u32 vertex_offset_bytes;
    dff_u32 vertex_stride;
    dff_u32 vertex_count;
    dff_u32 index_offset_bytes;
    dff_u32 index_size_bytes;
    dff_u32 index_count;
    dff_u32 triangles_rebuilt;
    dff_u32 candidate_score;
} Dff3dNativeInfo;

typedef struct Dff3dTxdTextureTag {
    char name[DFF3D_MAX_TEXTURE_NAME];
    char mask[DFF3D_MAX_TEXTURE_NAME];
    dff_u32 platform;
    dff_u32 filter_flags;
    dff_u32 raster_format;
    dff_u32 d3d_format;
    dff_u32 alpha_flags;
    dff_u32 width;
    dff_u32 height;
    dff_u32 depth;
    dff_u32 mipmap_count;
    dff_u32 texcode_type;
    dff_u32 flags;
    dff_u32 palette_bytes;
    dff_u32 image_bytes;
} Dff3dTxdTexture;

typedef struct Dff3dTxdTag {
    dff_u32 rw_version;
    dff_u32 declared_textures;
    dff_u32 texture_count;
    Dff3dTxdTexture textures[DFF3D_MAX_TXD_TEXTURES];
} Dff3dTxd;

typedef struct Dff3dDiagTag {
    dff_u32 chunks;
    dff_u32 clumps;
    dff_u32 skipped_chunks;
    dff_u32 extension_chunks;
    dff_u32 native_geometries;
    dff_u32 native_d3d_probe;
    dff_u32 native_d3d_decl_probe;
    dff_u32 native_ps2_probe;
    dff_u32 native_xbox_probe;
    dff_u32 native_mobile_probe;
    dff_u32 native_uninstanced;
    dff_u32 native_probe_fail;
    dff_u32 binmesh_tris_rebuilt;
    dff_u32 bin_mesh_plg;
    dff_u32 skin_plg;
    dff_u32 hanim_plg;
    dff_u32 matfx_plg;
    dff_u32 uv_anim_plg;
    dff_u32 user_data_plg;
    dff_u32 right_to_render;
    dff_u32 rockstar_plg;
    dff_u32 collision_plg;
    dff_u32 overflow_vertices;
    dff_u32 overflow_triangles;
    dff_u32 overflow_materials;
    dff_u32 overflow_frames;
    dff_u32 overflow_atomics;
    dff_u32 overflow_bin_meshes;
    dff_u32 overflow_skin_bones;
    dff_u32 overflow_hanim_bones;
    dff_u32 overflow_matfx;
    dff_u32 parse_warnings;
    dff_u32 last_error_pos;
} Dff3dDiag;

typedef struct Dff3dModelTag {
    dff_u32 rw_version;
    dff_u32 clump_count;
    dff_u32 vertex_count;
    dff_u32 triangle_count;
    dff_u32 geometry_count;
    dff_u32 material_count;
    dff_u32 frame_count;
    dff_u32 atomic_count;
    dff_u32 bin_mesh_count;
    dff_u32 bin_index_count;
    dff_u32 skin_bone_count;
    dff_u32 hanim_bone_count;
    dff_u32 matfx_count;
    dff_u32 native_info_count;
    Dff3dVertex vertices[DFF3D_MAX_VERTICES];
    Dff3dTriangle triangles[DFF3D_MAX_TRIANGLES];
    Dff3dGeometry geometries[DFF3D_MAX_GEOMETRIES];
    Dff3dMaterial materials[DFF3D_MAX_MATERIALS];
    Dff3dFrame frames[DFF3D_MAX_FRAMES];
    Dff3dAtomic atomics[DFF3D_MAX_ATOMICS];
    Dff3dBinMesh bin_meshes[DFF3D_MAX_BIN_MESHES];
    dff_u32 bin_indices[DFF3D_MAX_BIN_INDICES];
    Dff3dSkinBone skin_bones[DFF3D_MAX_SKIN_BONES];
    Dff3dHAnimBone hanim_bones[DFF3D_MAX_HANIM_BONES];
    Dff3dMatFx matfx[DFF3D_MAX_MATFX];
    Dff3dNativeInfo native_infos[DFF3D_MAX_NATIVE_INFOS];
} Dff3dModel;

void dff3d_model_clear(Dff3dModel *m);
void dff3d_txd_clear(Dff3dTxd *t);
void dff3d_diag_clear(Dff3dDiag *d);
int dff3d_decode_memory(const void *bytes, dff_u32 size, Dff3dModel *model, Dff3dDiag *diag);
int dff3d_decode_txd_memory(const void *bytes, dff_u32 size, Dff3dTxd *txd, Dff3dDiag *diag);
int dff3d_write_obj(FILE *f, const Dff3dModel *model);
int dff3d_uninstance_model(Dff3dModel *model, Dff3dDiag *diag);
int dff3d_write_obj_world(FILE *f, Dff3dModel *model);
int dff3d_write_mtl(FILE *f, const Dff3dModel *model);
int dff3d_dump_chunks(FILE *f, const void *bytes, dff_u32 size);
void dff3d_compute_frame_world(Dff3dModel *model);
void dff3d_fprint_fixed(FILE *f, dff_fixed value);
const char *dff3d_chunk_name(dff_u32 type);
const char *dff3d_native_platform_name(dff_u32 platform);
const char *dff3d_d3d_format_name(dff_u32 fmt);

#endif
