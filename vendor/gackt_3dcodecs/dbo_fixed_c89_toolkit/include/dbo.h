#ifndef DBO_H
#define DBO_H

#include "dbo_limits.h"
#include "dbo_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DBO_MAGIC_STRING "MAGICDBO"
#define DBO_VERSION_NUMBER 1062UL

#define DBO_INDEX_NONE 0xFFFFFFFFUL
#define DBO_VERTEX_DECL_COUNT 18UL
#define DBO_VERTEX_DECL_BYTES (DBO_VERTEX_DECL_COUNT * 8UL)

/* DBO block codes (from official DBPro source) */
#define DBOBLOCK_ROOT_FRAME 1UL
#define DBOBLOCK_ROOT_ANIMATIONSET 2UL

#define DBOBLOCK_FRAME_NAME 101UL
#define DBOBLOCK_FRAME_MATRIX 102UL
#define DBOBLOCK_FRAME_MESH 103UL
#define DBOBLOCK_FRAME_CHILD 104UL
#define DBOBLOCK_FRAME_SIBLING 105UL
#define DBOBLOCK_FRAME_OFFSET 106UL
#define DBOBLOCK_FRAME_ROTATION 107UL
#define DBOBLOCK_FRAME_SCALE 108UL

#define DBOBLOCK_MESH_FVF 111UL
#define DBOBLOCK_MESH_FVFSIZE 112UL
#define DBOBLOCK_MESH_VERTEXCOUNT 113UL
#define DBOBLOCK_MESH_INDEXCOUNT 114UL
#define DBOBLOCK_MESH_VERTEXDATA 115UL
#define DBOBLOCK_MESH_INDEXDATA 116UL
#define DBOBLOCK_MESH_PRIMTYPE 117UL
#define DBOBLOCK_MESH_DRAWVERTCOUNT 118UL
#define DBOBLOCK_MESH_DRAWPRIMCOUNT 119UL
#define DBOBLOCK_MESH_VERTEXDEC 120UL
#define DBOBLOCK_MESH_BONECOUNT 121UL
#define DBOBLOCK_MESH_BONESDATA 122UL
#define DBOBLOCK_MESH_USEMULTIMAT 123UL
#define DBOBLOCK_MESH_MULTIMATCOUNT 124UL
#define DBOBLOCK_MESH_USEMATERIAL 125UL
#define DBOBLOCK_MESH_MATERIAL 126UL
#define DBOBLOCK_MESH_TEXTURECOUNT 127UL
#define DBOBLOCK_MESH_TEXTURES 128UL
#define DBOBLOCK_MESH_WIREFRAME 129UL
#define DBOBLOCK_MESH_LIGHT 130UL
#define DBOBLOCK_MESH_CULL 131UL
#define DBOBLOCK_MESH_FOG 132UL
#define DBOBLOCK_MESH_AMBIENT 133UL
#define DBOBLOCK_MESH_TRANSPARENCY 134UL
#define DBOBLOCK_MESH_GHOST 135UL
#define DBOBLOCK_MESH_GHOSTMODE 136UL
#define DBOBLOCK_MESH_LINKED 137UL
#define DBOBLOCK_MESH_SUBFRAMES 138UL
#define DBOBLOCK_MESH_MULTIMAT 139UL
#define DBOBLOCK_MESH_VISIBLE 140UL
#define DBOBLOCK_TEXTURE_NAME 141UL
#define DBOBLOCK_TEXTURE_STAGE 142UL
#define DBOBLOCK_TEXTURE_BMODE 143UL
#define DBOBLOCK_TEXTURE_BARG1 144UL
#define DBOBLOCK_TEXTURE_BARG2 145UL
#define DBOBLOCK_TEXTURE_ADDRU 146UL
#define DBOBLOCK_TEXTURE_ADDRV 147UL
#define DBOBLOCK_TEXTURE_MAG 148UL
#define DBOBLOCK_TEXTURE_MIN 149UL
#define DBOBLOCK_TEXTURE_MIP 150UL
#define DBOBLOCK_TEXTURE_TCMODE 151UL
#define DBOBLOCK_TEXTURE_PRIMST 152UL
#define DBOBLOCK_TEXTURE_PRIMCN 153UL
#define DBOBLOCK_MESH_FXEFFECTNAME 154UL
#define DBOBLOCK_MESH_ARBITARYVALUE 155UL
#define DBOBLOCK_MESH_ZBIASFLAG 156UL
#define DBOBLOCK_MESH_ZBIASSLOPE 157UL
#define DBOBLOCK_MESH_ZBIASDEPTH 158UL
#define DBOBLOCK_MESH_ZREAD 159UL
#define DBOBLOCK_MESH_ZWRITE 160UL
#define DBOBLOCK_MULTIMAT_NAME 161UL
#define DBOBLOCK_MULTIMAT_MATERIAL 162UL
#define DBOBLOCK_MULTIMAT_START 163UL
#define DBOBLOCK_MULTIMAT_COUNT 164UL
#define DBOBLOCK_MULTIMAT_POLY 165UL
#define DBOBLOCK_MESH_ALPHATESTVALUE 166UL

#define DBOBLOCK_ANIMSET_NAME 201UL
#define DBOBLOCK_ANIMSET_LENGTH 202UL
#define DBOBLOCK_ANIMSET_DATA 203UL

#define DBOBLOCK_ANIM_NAME 211UL
#define DBOBLOCK_ANIM_NUMPOSKEYS 212UL
#define DBOBLOCK_ANIM_POSDATA 213UL
#define DBOBLOCK_ANIM_NUMROTKEYS 214UL
#define DBOBLOCK_ANIM_ROTDATA 215UL
#define DBOBLOCK_ANIM_NUMSCALEKEYS 216UL
#define DBOBLOCK_ANIM_SCALEDATA 217UL
#define DBOBLOCK_ANIM_NUMMATRIXKEYS 218UL
#define DBOBLOCK_ANIM_MATRIXDATA 219UL
#define DBOBLOCK_ANIM_NEXT 220UL

#define DBOBLOCK_BONES_NAME 301UL
#define DBOBLOCK_BONES_NUMINFLUENCES 302UL
#define DBOBLOCK_BONES_VERTICES 303UL
#define DBOBLOCK_BONES_WEIGHTS 304UL
#define DBOBLOCK_BONES_TRANSLATIONMATRIX 305UL

#define DBOBLOCK_FRAME_BONETYPE 400UL
#define DBOBLOCK_FRAME_BONEOFFSETLIST 401UL
#define DBOBLOCK_FRAME_BONEOFFSETLISTSIZE 402UL
#define DBOBLOCK_FRAME_BONEMATRIXSIZEA 403UL
#define DBOBLOCK_FRAME_BONEMATRIXSIZEB 404UL
#define DBOBLOCK_FRAME_BONEMATRIXLIST 405UL
#define DBOBLOCK_FRAME_BONEMATRIXDATA 406UL
#define DBOBLOCK_OBJECT_CUSTOMDATA 406UL

/* Common D3D primitive values */
#define DBO_D3DPT_POINTLIST 1UL
#define DBO_D3DPT_LINELIST 2UL
#define DBO_D3DPT_LINESTRIP 3UL
#define DBO_D3DPT_TRIANGLELIST 4UL
#define DBO_D3DPT_TRIANGLESTRIP 5UL
#define DBO_D3DPT_TRIANGLEFAN 6UL

/* D3DFVF bits */
#define DBO_D3DFVF_RESERVED0 0x001UL
#define DBO_D3DFVF_POSITION_MASK 0x400EUL
#define DBO_D3DFVF_XYZ 0x002UL
#define DBO_D3DFVF_XYZRHW 0x004UL
#define DBO_D3DFVF_XYZB1 0x006UL
#define DBO_D3DFVF_XYZB2 0x008UL
#define DBO_D3DFVF_XYZB3 0x00AUL
#define DBO_D3DFVF_XYZB4 0x00CUL
#define DBO_D3DFVF_XYZB5 0x00EUL
#define DBO_D3DFVF_XYZW 0x4002UL
#define DBO_D3DFVF_NORMAL 0x010UL
#define DBO_D3DFVF_PSIZE 0x020UL
#define DBO_D3DFVF_DIFFUSE 0x040UL
#define DBO_D3DFVF_SPECULAR 0x080UL
#define DBO_D3DFVF_TEXCOUNT_MASK 0xF00UL
#define DBO_D3DFVF_TEXCOUNT_SHIFT 8UL
#define DBO_D3DFVF_TEX0 0x000UL
#define DBO_D3DFVF_TEX1 0x100UL
#define DBO_D3DFVF_TEX2 0x200UL
#define DBO_D3DFVF_TEX3 0x300UL
#define DBO_D3DFVF_TEX4 0x400UL
#define DBO_D3DFVF_TEX5 0x500UL
#define DBO_D3DFVF_TEX6 0x600UL
#define DBO_D3DFVF_TEX7 0x700UL
#define DBO_D3DFVF_TEX8 0x800UL
#define DBO_D3DFVF_LASTBETA_UBYTE4 0x1000UL
#define DBO_D3DFVF_LASTBETA_D3DCOLOR 0x8000UL

#define DBO_D3DFVF_TEXTUREFORMAT1 3UL
#define DBO_D3DFVF_TEXTUREFORMAT2 0UL
#define DBO_D3DFVF_TEXTUREFORMAT3 1UL
#define DBO_D3DFVF_TEXTUREFORMAT4 2UL

#define DBO_D3DFVF_TEXCOORDSIZE1(i) (DBO_D3DFVF_TEXTUREFORMAT1 << ((i) * 2UL + 16UL))
#define DBO_D3DFVF_TEXCOORDSIZE2(i) (DBO_D3DFVF_TEXTUREFORMAT2 << ((i) * 2UL + 16UL))
#define DBO_D3DFVF_TEXCOORDSIZE3(i) (DBO_D3DFVF_TEXTUREFORMAT3 << ((i) * 2UL + 16UL))
#define DBO_D3DFVF_TEXCOORDSIZE4(i) (DBO_D3DFVF_TEXTUREFORMAT4 << ((i) * 2UL + 16UL))

typedef struct dbo_blob_ref
{
    dbo_u32 offset;
    dbo_u32 size;
} dbo_blob_ref;

#define DBO_SCOPE_SCENE 1UL
#define DBO_SCOPE_FRAME 2UL
#define DBO_SCOPE_MESH 3UL
#define DBO_SCOPE_TEXTURE 4UL
#define DBO_SCOPE_BONE 5UL
#define DBO_SCOPE_MULTIMATERIAL 6UL
#define DBO_SCOPE_ANIMATION 7UL
#define DBO_SCOPE_ANIMATION_SET 8UL

typedef struct dbo_unknown_chunk
{
    dbo_u32 scope_kind;
    dbo_u32 owner_index;
    dbo_u32 code;
    dbo_blob_ref payload;
} dbo_unknown_chunk;

typedef struct dbo_chunk_event
{
    dbo_u32 scope_kind;
    dbo_u32 owner_index;
    dbo_u32 code;
    dbo_u32 aux_index;
} dbo_chunk_event;

typedef struct dbo_material
{
    dbo_fx32 diffuse[4];
    dbo_fx32 ambient[4];
    dbo_fx32 specular[4];
    dbo_fx32 emissive[4];
    dbo_fx32 power;
} dbo_material;

typedef struct dbo_texture
{
    dbo_blob_ref name;
    dbo_u32 stage;
    dbo_u32 blend_mode;
    dbo_u32 blend_arg1;
    dbo_u32 blend_arg2;
    dbo_u32 address_u;
    dbo_u32 address_v;
    dbo_u32 mag_state;
    dbo_u32 min_state;
    dbo_u32 mip_state;
    dbo_u32 texcoord_mode;
    dbo_s32 start_index;
    dbo_s32 primitive_count;
} dbo_texture;

typedef struct dbo_multimaterial
{
    dbo_blob_ref name;
    dbo_material material;
    dbo_u32 index_start;
    dbo_u32 index_count;
    dbo_u32 poly_count;
} dbo_multimaterial;

typedef struct dbo_bone
{
    dbo_blob_ref name;
    dbo_u32 num_influences;
    dbo_blob_ref vertices;
    dbo_blob_ref weights;
    dbo_fx32 translation[16];
} dbo_bone;

typedef struct dbo_position_key
{
    dbo_u32 time;
    dbo_fx32 pos[3];
    dbo_fx32 interp[3];
} dbo_position_key;

typedef struct dbo_rotate_key
{
    dbo_u32 time;
    dbo_fx32 quat[4];
} dbo_rotate_key;

typedef struct dbo_scale_key
{
    dbo_u32 time;
    dbo_fx32 scale[3];
    dbo_fx32 interp[3];
} dbo_scale_key;

typedef struct dbo_matrix_key
{
    dbo_u32 time;
    dbo_fx32 matrix[16];
    dbo_fx32 interp[16];
} dbo_matrix_key;

typedef struct dbo_animation
{
    dbo_blob_ref name;
    dbo_u32 frame_index;
    dbo_bool looped;
    dbo_bool linear;
    dbo_u32 num_position_keys;
    dbo_u32 num_rotate_keys;
    dbo_u32 num_scale_keys;
    dbo_u32 num_matrix_keys;
    dbo_blob_ref position_keys;
    dbo_blob_ref rotate_keys;
    dbo_blob_ref scale_keys;
    dbo_blob_ref matrix_keys;
    dbo_u32 bone_type;
    dbo_u32 bone_offset_count;
    dbo_blob_ref bone_offsets;
    dbo_s32 bone_frame_a;
    dbo_s32 bone_frame_b;
    dbo_blob_ref bone_matrices;
    dbo_u32 next_animation;
} dbo_animation;

typedef struct dbo_animation_set
{
    dbo_blob_ref name;
    dbo_u32 length;
    dbo_u32 first_animation;
} dbo_animation_set;

typedef struct dbo_mesh
{
    dbo_u32 fvf;
    dbo_u32 fvf_size;
    dbo_u32 vertex_count;
    dbo_u32 index_count;
    dbo_blob_ref vertex_decl;
    dbo_u32 vertex_decl_len;
    dbo_blob_ref vertex_data;
    dbo_blob_ref index_data;
    dbo_u32 primitive_type;
    dbo_u32 draw_vertex_count;
    dbo_u32 draw_primitive_count;
    dbo_u32 first_bone;
    dbo_u32 bone_count;
    dbo_bool uses_material;
    dbo_material material;
    dbo_u32 first_texture;
    dbo_u32 texture_count;
    dbo_bool use_multimaterial;
    dbo_u32 first_multimaterial;
    dbo_u32 multimaterial_count;
    dbo_bool wireframe;
    dbo_bool light;
    dbo_bool cull;
    dbo_bool fog;
    dbo_bool ambient;
    dbo_bool transparency;
    dbo_bool ghost;
    dbo_u32 ghost_mode;
    dbo_bool visible;
    dbo_bool linked;
    dbo_u32 subframe_count;
    dbo_blob_ref subframe_indices;
    dbo_bool use_vertex_shader;
    dbo_blob_ref effect_name;
    dbo_u32 arbitary_value;
    dbo_bool z_bias_active;
    dbo_fx32 z_bias_slope;
    dbo_fx32 z_bias_depth;
    dbo_bool z_read;
    dbo_bool z_write;
    dbo_u32 alpha_test_value;
} dbo_mesh;

typedef struct dbo_frame
{
    dbo_blob_ref name;
    dbo_u32 parent;
    dbo_u32 child;
    dbo_u32 sibling;
    dbo_u32 mesh;
    dbo_fx32 original[16];
    dbo_fx32 offset[3];
    dbo_fx32 rotation[3];
    dbo_fx32 scale[3];
} dbo_frame;

typedef struct dbo_scene
{
    dbo_u32 version;
    dbo_u32 reserved1;
    dbo_u32 reserved2;
    dbo_u32 root_frame;
    dbo_u32 root_animation_set;
    dbo_blob_ref custom_data;

    dbo_frame frames[DBO_MAX_FRAMES];
    dbo_u32 frame_count;

    dbo_mesh meshes[DBO_MAX_MESHES];
    dbo_u32 mesh_count;

    dbo_texture textures[DBO_MAX_TEXTURES];
    dbo_u32 texture_count;

    dbo_bone bones[DBO_MAX_BONES];
    dbo_u32 bone_count;

    dbo_multimaterial multimaterials[DBO_MAX_MULTIMATERIALS];
    dbo_u32 multimaterial_count;

    dbo_animation animations[DBO_MAX_ANIMATIONS];
    dbo_u32 animation_count;

    dbo_animation_set animation_sets[DBO_MAX_ANIMATION_SETS];
    dbo_u32 animation_set_count;

    dbo_unknown_chunk unknown_chunks[DBO_MAX_UNKNOWN_CHUNKS];
    dbo_u32 unknown_chunk_count;

    dbo_chunk_event chunk_events[DBO_MAX_CHUNK_EVENTS];
    dbo_u32 chunk_event_count;

    dbo_u8 blob[DBO_BLOB_CAPACITY];
    dbo_u32 blob_used;

    char last_error[DBO_MAX_ERROR_TEXT];
} dbo_scene;

typedef struct dbo_validation_report
{
    dbo_u32 errors;
    dbo_u32 warnings;
    char first_error[DBO_MAX_ERROR_TEXT];
    char first_warning[DBO_MAX_ERROR_TEXT];
} dbo_validation_report;


void dbo_scene_reset(dbo_scene* scene);
const char* dbo_scene_last_error(const dbo_scene* scene);
const void* dbo_blob_data(const dbo_scene* scene, dbo_blob_ref ref);
const char* dbo_blob_cstr(const dbo_scene* scene, dbo_blob_ref ref);
dbo_bool dbo_scene_copy_bytes(dbo_scene* scene, const void* src, dbo_u32 size, dbo_u32 align, dbo_blob_ref* out_ref);
dbo_bool dbo_scene_copy_string(dbo_scene* scene, const char* text, dbo_blob_ref* out_ref);

dbo_bool dbo_scene_add_frame(dbo_scene* scene, dbo_u32* out_index);
dbo_bool dbo_scene_add_mesh(dbo_scene* scene, dbo_u32* out_index);
dbo_bool dbo_scene_add_texture(dbo_scene* scene, dbo_u32* out_index);
dbo_bool dbo_scene_add_bone(dbo_scene* scene, dbo_u32* out_index);
dbo_bool dbo_scene_add_multimaterial(dbo_scene* scene, dbo_u32* out_index);
dbo_bool dbo_scene_add_animation(dbo_scene* scene, dbo_u32* out_index);
dbo_bool dbo_scene_add_animation_set(dbo_scene* scene, dbo_u32* out_index);


void dbo_validation_report_reset(dbo_validation_report* report);
dbo_bool dbo_validate_scene(const dbo_scene* scene, dbo_validation_report* report);

dbo_u32 dbo_scene_total_vertices(const dbo_scene* scene);
dbo_u32 dbo_scene_total_indices(const dbo_scene* scene);
dbo_s32 dbo_scene_find_frame_by_name(const dbo_scene* scene, const char* name);
dbo_s32 dbo_scene_find_mesh_by_frame_name(const dbo_scene* scene, const char* name);
dbo_bool dbo_mesh_read_index_u16(const dbo_scene* scene, const dbo_mesh* mesh, dbo_u32 index_number, dbo_u16* out_index);
dbo_bool dbo_mesh_compute_aabb(const dbo_scene* scene, const dbo_mesh* mesh, dbo_fx32 out_min[3], dbo_fx32 out_max[3]);
dbo_bool dbo_scene_compute_aabb(const dbo_scene* scene, dbo_fx32 out_min[3], dbo_fx32 out_max[3]);

dbo_bool dbo_scene_resolve_animation_frames(dbo_scene* scene);

dbo_u32 dbo_scene_unknown_chunk_count(const dbo_scene* scene);
dbo_u32 dbo_scene_unknown_bytes(const dbo_scene* scene);
dbo_u32 dbo_scene_unknown_chunks_for_scope(const dbo_scene* scene, dbo_u32 scope_kind, dbo_u32 owner_index);
void dbo_scene_strip_unknown_chunks(dbo_scene* scene);

dbo_u32 dbo_scene_chunk_event_count(const dbo_scene* scene);
dbo_u32 dbo_scene_chunk_events_for_scope(const dbo_scene* scene, dbo_u32 scope_kind, dbo_u32 owner_index);
void dbo_scene_strip_chunk_order(dbo_scene* scene);

dbo_bool dbo_parse_memory(dbo_scene* scene, const void* data, dbo_u32 size);
dbo_bool dbo_measure_encoded_size(const dbo_scene* scene, dbo_u32* out_size);
dbo_bool dbo_encode_memory(const dbo_scene* scene, void* out_buffer, dbo_u32 capacity, dbo_u32* out_size);

dbo_u32 dbo_fvf_texcoord_count(dbo_u32 fvf);
dbo_u32 dbo_fvf_texcoord_dims(dbo_u32 fvf, dbo_u32 set_index);
dbo_u32 dbo_fvf_vertex_stride(dbo_u32 fvf);
dbo_bool dbo_fvf_decode_position(const dbo_scene* scene, const dbo_mesh* mesh, dbo_u32 vertex_index, dbo_fx32 out_xyz[3]);
dbo_bool dbo_fvf_decode_normal(const dbo_scene* scene, const dbo_mesh* mesh, dbo_u32 vertex_index, dbo_fx32 out_nrm[3]);
dbo_bool dbo_fvf_decode_texcoord(const dbo_scene* scene, const dbo_mesh* mesh, dbo_u32 vertex_index, dbo_u32 set_index, dbo_fx32 out_uv[4]);
dbo_bool dbo_fvf_encode_position(dbo_u32 fvf, void* vertex, const dbo_fx32 xyz[3]);
dbo_bool dbo_fvf_encode_normal(dbo_u32 fvf, void* vertex, const dbo_fx32 nrm[3]);
dbo_bool dbo_fvf_encode_texcoord(dbo_u32 fvf, void* vertex, dbo_u32 set_index, const dbo_fx32 uv[4]);

#ifdef __cplusplus
}
#endif

#endif
