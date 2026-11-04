#ifndef G3DS_FIXED_H
#define G3DS_FIXED_H

/*
   g3ds_fixed.h
   Static-memory 3D Studio .3DS importer for C89 engines.

   Contract:
   - no dynamic allocation calls
   - no heap ownership
   - no native C real-number types in the codec API or implementation
   - IEEE-754 32-bit real payloads are decoded from raw bits to Q16.16
   - unknown chunks are skipped by their declared chunk length
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef G3DS_MAX_OBJECTS
#define G3DS_MAX_OBJECTS       64u
#endif
#ifndef G3DS_MAX_VERTICES
#define G3DS_MAX_VERTICES      65535u
#endif
#ifndef G3DS_MAX_FACES
#define G3DS_MAX_FACES         65535u
#endif
#ifndef G3DS_MAX_MATERIALS
#define G3DS_MAX_MATERIALS     128u
#endif
#ifndef G3DS_MAX_LIGHTS
#define G3DS_MAX_LIGHTS        32u
#endif
#ifndef G3DS_MAX_CAMERAS
#define G3DS_MAX_CAMERAS       32u
#endif
#ifndef G3DS_MAX_ANIM_NODES
#define G3DS_MAX_ANIM_NODES    128u
#endif
#ifndef G3DS_MAX_POS_KEYS
#define G3DS_MAX_POS_KEYS      512u
#endif
#ifndef G3DS_MAX_ROT_KEYS
#define G3DS_MAX_ROT_KEYS      512u
#endif
#ifndef G3DS_MAX_SCL_KEYS
#define G3DS_MAX_SCL_KEYS      512u
#endif
#ifndef G3DS_MAX_COLOR_KEYS
#define G3DS_MAX_COLOR_KEYS    256u
#endif
#ifndef G3DS_MAX_SCALAR_KEYS
#define G3DS_MAX_SCALAR_KEYS   512u
#endif
#ifndef G3DS_MAX_HIDE_KEYS
#define G3DS_MAX_HIDE_KEYS     256u
#endif
#ifndef G3DS_MAX_MORPH_KEYS
#define G3DS_MAX_MORPH_KEYS    256u
#endif
#ifndef G3DS_MAX_CHUNK_AUDIT
#define G3DS_MAX_CHUNK_AUDIT   128u
#endif
#ifndef G3DS_NAME_MAX
#define G3DS_NAME_MAX          64u
#endif
#ifndef G3DS_MAPNAME_MAX
#define G3DS_MAPNAME_MAX       64u
#endif
#ifndef G3DS_BOXMAP_SIDES
#define G3DS_BOXMAP_SIDES      6u
#endif

#define G3DS_Q16_SHIFT 16
#define G3DS_Q16_ONE   65536L
#define G3DS_NO_MATERIAL 65535u
#define G3DS_NO_PARENT   65535u

#define G3DS_MAP_DIFFUSE     0u
#define G3DS_MAP_DIFFUSE2    1u
#define G3DS_MAP_OPACITY     2u
#define G3DS_MAP_BUMP        3u
#define G3DS_MAP_SPECULAR    4u
#define G3DS_MAP_SHININESS   5u
#define G3DS_MAP_SELFILLUM   6u
#define G3DS_MAP_REFLECTION  7u
#define G3DS_MAP_DIFFUSE_MASK    8u
#define G3DS_MAP_DIFFUSE2_MASK   9u
#define G3DS_MAP_OPACITY_MASK    10u
#define G3DS_MAP_BUMP_MASK       11u
#define G3DS_MAP_SPECULAR_MASK   12u
#define G3DS_MAP_SHININESS_MASK  13u
#define G3DS_MAP_SELFILLUM_MASK  14u
#define G3DS_MAP_REFLECTION_MASK 15u
#define G3DS_MAP_COUNT       16u

#define G3DS_MAT_FLAG_SELF_ILLUM  0x0001u
#define G3DS_MAT_FLAG_TWO_SIDE    0x0002u
#define G3DS_MAT_FLAG_DECAL       0x0004u
#define G3DS_MAT_FLAG_ADDITIVE    0x0008u
#define G3DS_MAT_FLAG_WIRE        0x0010u
#define G3DS_MAT_FLAG_FACEMAP     0x0020u
#define G3DS_MAT_FLAG_PHONGSOFT   0x0040u
#define G3DS_MAT_FLAG_WIREABS     0x0080u
#define G3DS_MAT_FLAG_USE_XPFALL  0x0100u
#define G3DS_MAT_FLAG_USE_REFBLUR 0x0200u

#define G3DS_OBJ_FLAG_HIDDEN       0x0001u
#define G3DS_OBJ_FLAG_VIS_LOFTER   0x0002u
#define G3DS_OBJ_FLAG_DOESNT_CAST  0x0004u
#define G3DS_OBJ_FLAG_MATTE        0x0008u
#define G3DS_OBJ_FLAG_FAST         0x0010u
#define G3DS_OBJ_FLAG_PROCEDURAL   0x0020u
#define G3DS_OBJ_FLAG_FROZEN       0x0040u
#define G3DS_OBJ_FLAG_DONT_RECEIVE 0x0080u

#define G3DS_LIGHT_FLAG_OFF          0x0001u
#define G3DS_LIGHT_FLAG_ATTENUATE    0x0002u
#define G3DS_LIGHT_FLAG_SPOT         0x0004u
#define G3DS_LIGHT_FLAG_SHADOWED     0x0008u
#define G3DS_LIGHT_FLAG_SEE_CONE     0x0010u
#define G3DS_LIGHT_FLAG_RECTANGULAR  0x0020u
#define G3DS_LIGHT_FLAG_OVERSHOOT    0x0040u
#define G3DS_LIGHT_FLAG_RAYSHAD      0x0080u

#define G3DS_BG_HAS_BITMAP     0x0001u
#define G3DS_BG_HAS_SOLID      0x0002u
#define G3DS_BG_HAS_GRADIENT   0x0004u
#define G3DS_BG_USE_BITMAP     0x0010u
#define G3DS_BG_USE_SOLID      0x0020u
#define G3DS_BG_USE_GRADIENT   0x0040u

#define G3DS_ATMOS_HAS_FOG      0x0001u
#define G3DS_ATMOS_HAS_DISTANCE 0x0002u
#define G3DS_ATMOS_HAS_LAYER    0x0004u
#define G3DS_ATMOS_USE_FOG      0x0010u
#define G3DS_ATMOS_USE_DISTANCE 0x0020u
#define G3DS_ATMOS_USE_LAYER    0x0040u
#define G3DS_ATMOS_FOG_BGND     0x0100u
#define G3DS_ATMOS_DCUE_BGND    0x0200u

#define G3DS_NODE_UNKNOWN       0u
#define G3DS_NODE_AMBIENT       1u
#define G3DS_NODE_OBJECT        2u
#define G3DS_NODE_CAMERA        3u
#define G3DS_NODE_CAMERA_TARGET 4u
#define G3DS_NODE_LIGHT         5u
#define G3DS_NODE_SPOT_TARGET   6u
#define G3DS_NODE_SPOT_LIGHT    7u

#define G3DS_SCALAR_FOV     1u
#define G3DS_SCALAR_ROLL    2u
#define G3DS_SCALAR_HOTSPOT 3u
#define G3DS_SCALAR_FALLOFF 4u

#define G3DS_TRACK_POS   1u
#define G3DS_TRACK_ROT   2u
#define G3DS_TRACK_SCL   3u
#define G3DS_TRACK_COLOR 4u
#define G3DS_TRACK_FOV   5u
#define G3DS_TRACK_ROLL  6u
#define G3DS_TRACK_HOT   7u
#define G3DS_TRACK_FALL  8u
#define G3DS_TRACK_HIDE  9u

#define G3DS_FACE_AB_VISIBLE 0x0001u
#define G3DS_FACE_BC_VISIBLE 0x0002u
#define G3DS_FACE_CA_VISIBLE 0x0004u

/* C89-friendly base types. U32/S32 use at least 32 bits. */
typedef signed char G3DS_S8;
typedef unsigned char G3DS_U8;
typedef signed short G3DS_S16;
typedef unsigned short G3DS_U16;
typedef signed long G3DS_S32;
typedef unsigned long G3DS_U32;
typedef G3DS_S32 G3DS_Q16;

typedef struct G3DS_Color_TAG {
    G3DS_U8 r;
    G3DS_U8 g;
    G3DS_U8 b;
} G3DS_Color;

typedef struct G3DS_Vertex_TAG {
    G3DS_Q16 x;
    G3DS_Q16 y;
    G3DS_Q16 z;
    G3DS_Q16 u;
    G3DS_Q16 v;
    G3DS_U8 has_uv;
    G3DS_U16 point_flags;
} G3DS_Vertex;

typedef struct G3DS_Face_TAG {
    G3DS_U16 a;
    G3DS_U16 b;
    G3DS_U16 c;
    G3DS_U16 flags;
    G3DS_U16 material;
    G3DS_U32 smoothing;
} G3DS_Face;

typedef struct G3DS_Map_TAG {
    char filename[G3DS_MAPNAME_MAX];
    G3DS_Q16 strength;  /* 0..100 as Q16.16 */
    G3DS_U16 tiling;
    G3DS_Q16 blur;
    G3DS_Q16 u_scale;
    G3DS_Q16 v_scale;
    G3DS_Q16 u_offset;
    G3DS_Q16 v_offset;
    G3DS_Q16 angle_degrees;
    G3DS_Color tint1;
    G3DS_Color tint2;
    G3DS_Color tint_r;
    G3DS_Color tint_g;
    G3DS_Color tint_b;
    G3DS_U16 tint_flags;
    G3DS_U32 sxp_data_bytes;
    G3DS_U32 sxp_mask_bytes;
    G3DS_U8 has_sxp_data;
    G3DS_U8 has_sxp_mask;
    G3DS_U8 has_map;
} G3DS_Map;

typedef struct G3DS_Material_TAG {
    char name[G3DS_NAME_MAX];
    G3DS_Color ambient;
    G3DS_Color diffuse;
    G3DS_Color specular;
    G3DS_Q16 shininess;
    G3DS_Q16 shininess_strength;
    G3DS_Q16 transparency;
    G3DS_Q16 transparency_falloff;
    G3DS_Q16 reflection_blur;
    G3DS_Q16 wire_size;
    G3DS_Q16 self_illum_pct;
    G3DS_U16 shading;
    G3DS_U16 flags;
    G3DS_U8 acubic_shade_level;
    G3DS_U8 acubic_aa_level;
    G3DS_U16 acubic_flags;
    G3DS_U32 acubic_size;
    G3DS_U32 acubic_frame_interval;
    G3DS_U8 has_acubic;
    G3DS_Map maps[G3DS_MAP_COUNT];
} G3DS_Material;

typedef struct G3DS_Object_TAG {
    char name[G3DS_NAME_MAX];
    G3DS_U16 first_vertex;
    G3DS_U16 vertex_count;
    G3DS_U16 first_face;
    G3DS_U16 face_count;
    G3DS_Q16 matrix[12];
    G3DS_Q16 tex_icon[21]; /* parsed 0x4170 mapping icon data, if present */
    char boxmap[G3DS_BOXMAP_SIDES][G3DS_NAME_MAX];
    char proc_name[G3DS_NAME_MAX];
    G3DS_U32 proc_data_bytes;
    G3DS_U16 flags;
    G3DS_U8 mesh_color;
    G3DS_U8 has_matrix;
    G3DS_U8 has_tex_icon;
    G3DS_U8 has_boxmap;
    G3DS_U8 has_proc_name;
    G3DS_U8 has_proc_data;
} G3DS_Object;

typedef struct G3DS_Light_TAG {
    char name[G3DS_NAME_MAX];
    G3DS_Q16 x;
    G3DS_Q16 y;
    G3DS_Q16 z;
    G3DS_Color color;
    G3DS_Q16 inner_range;
    G3DS_Q16 outer_range;
    G3DS_Q16 multiplier;
    G3DS_Q16 spot_tx;
    G3DS_Q16 spot_ty;
    G3DS_Q16 spot_tz;
    G3DS_Q16 spot_hotspot;
    G3DS_Q16 spot_falloff;
    G3DS_Q16 spot_roll;
    G3DS_Q16 spot_aspect;
    G3DS_Q16 ray_bias;
    G3DS_U16 local_shadow_map_size;
    G3DS_Q16 local_shadow_bias;
    G3DS_Q16 local_shadow_filter;
    char projector[G3DS_MAPNAME_MAX];
    G3DS_U16 flags;
} G3DS_Light;

typedef struct G3DS_Camera_TAG {
    char name[G3DS_NAME_MAX];
    G3DS_Q16 x;
    G3DS_Q16 y;
    G3DS_Q16 z;
    G3DS_Q16 tx;
    G3DS_Q16 ty;
    G3DS_Q16 tz;
    G3DS_Q16 bank_degrees;
    G3DS_Q16 lens_mm;
    G3DS_Q16 near_range;
    G3DS_Q16 far_range;
    G3DS_U8 see_cone;
    G3DS_U8 has_ranges;
} G3DS_Camera;

typedef struct G3DS_PosKey_TAG {
    G3DS_U32 frame;
    G3DS_U16 spline_flags;
    G3DS_Q16 x;
    G3DS_Q16 y;
    G3DS_Q16 z;
} G3DS_PosKey;

typedef struct G3DS_RotKey_TAG {
    G3DS_U32 frame;
    G3DS_U16 spline_flags;
    G3DS_Q16 angle_degrees;
    G3DS_Q16 axis_x;
    G3DS_Q16 axis_y;
    G3DS_Q16 axis_z;
} G3DS_RotKey;

typedef struct G3DS_SclKey_TAG {
    G3DS_U32 frame;
    G3DS_U16 spline_flags;
    G3DS_Q16 x;
    G3DS_Q16 y;
    G3DS_Q16 z;
} G3DS_SclKey;

typedef struct G3DS_ColorKey_TAG {
    G3DS_U32 frame;
    G3DS_U16 spline_flags;
    G3DS_Color color;
} G3DS_ColorKey;

typedef struct G3DS_ScalarKey_TAG {
    G3DS_U32 frame;
    G3DS_U16 spline_flags;
    G3DS_U16 kind;
    G3DS_Q16 value;
} G3DS_ScalarKey;

typedef struct G3DS_HideKey_TAG {
    G3DS_U32 frame;
    G3DS_U16 spline_flags;
} G3DS_HideKey;

typedef struct G3DS_MorphKey_TAG {
    G3DS_U32 frame;
    G3DS_U16 spline_flags;
    char target[G3DS_NAME_MAX];
} G3DS_MorphKey;

typedef struct G3DS_AnimNode_TAG {
    char name[G3DS_NAME_MAX];
    char instance_name[G3DS_NAME_MAX];
    G3DS_U16 node_id;
    G3DS_U16 parent_id;
    G3DS_U16 kind;
    G3DS_U16 flags1;
    G3DS_U16 flags2;
    G3DS_Q16 pivot[3];
    G3DS_Q16 bbox_min[3];
    G3DS_Q16 bbox_max[3];
    G3DS_U16 first_pos_key;
    G3DS_U16 pos_key_count;
    G3DS_U16 first_rot_key;
    G3DS_U16 rot_key_count;
    G3DS_U16 first_scl_key;
    G3DS_U16 scl_key_count;
    G3DS_U16 first_color_key;
    G3DS_U16 color_key_count;
    G3DS_U16 first_scalar_key;
    G3DS_U16 scalar_key_count;
    G3DS_U16 first_hide_key;
    G3DS_U16 hide_key_count;
    G3DS_U16 first_morph_key;
    G3DS_U16 morph_key_count;
    G3DS_Q16 morph_smooth_degrees;
    G3DS_U8 has_morph_smooth;
    G3DS_U8 has_pivot;
    G3DS_U8 has_bbox;
} G3DS_AnimNode;

typedef struct G3DS_Bounds_TAG {
    G3DS_Q16 min_x;
    G3DS_Q16 min_y;
    G3DS_Q16 min_z;
    G3DS_Q16 max_x;
    G3DS_Q16 max_y;
    G3DS_Q16 max_z;
} G3DS_Bounds;

#define G3DS_VIEW_NONE   0u
#define G3DS_VIEW_TOP    1u
#define G3DS_VIEW_BOTTOM 2u
#define G3DS_VIEW_LEFT   3u
#define G3DS_VIEW_RIGHT  4u
#define G3DS_VIEW_FRONT  5u
#define G3DS_VIEW_BACK   6u
#define G3DS_VIEW_USER   7u
#define G3DS_VIEW_CAMERA 8u

typedef struct G3DS_DefaultView_TAG {
    G3DS_U16 kind;
    G3DS_Q16 target[3];
    G3DS_Q16 width;
    G3DS_Q16 horizontal_angle;
    G3DS_Q16 vertical_angle;
    G3DS_Q16 bank_angle;
    char camera_name[G3DS_NAME_MAX];
    G3DS_U8 has_view;
} G3DS_DefaultView;

typedef struct G3DS_RenderSettings_TAG {
    G3DS_Q16 low_shadow_bias;
    G3DS_Q16 high_shadow_bias;
    G3DS_Q16 shadow_filter;
    G3DS_Q16 ray_bias;
    G3DS_Q16 construction_plane[3];
    G3DS_U16 shadow_map_size;
    G3DS_U16 shadow_samples;
    G3DS_U16 shadow_range;
    G3DS_U16 flags;
} G3DS_RenderSettings;

#define G3DS_RENDER_HAS_LOW_SHADOW_BIAS   0x0001u
#define G3DS_RENDER_HAS_HIGH_SHADOW_BIAS  0x0002u
#define G3DS_RENDER_HAS_SHADOW_FILTER     0x0004u
#define G3DS_RENDER_HAS_RAY_BIAS          0x0008u
#define G3DS_RENDER_HAS_SHADOW_MAP_SIZE   0x0010u
#define G3DS_RENDER_HAS_SHADOW_SAMPLES    0x0020u
#define G3DS_RENDER_HAS_SHADOW_RANGE      0x0040u
#define G3DS_RENDER_HAS_CONSTRUCTION_PLANE 0x0080u

typedef struct G3DS_ChunkAudit_TAG {
    G3DS_U16 id;
    G3DS_U32 count;
    G3DS_U32 bytes;
} G3DS_ChunkAudit;

typedef struct G3DS_Background_TAG {
    char bitmap[G3DS_MAPNAME_MAX];
    G3DS_Color solid;
    G3DS_Color gradient_top;
    G3DS_Color gradient_mid;
    G3DS_Color gradient_bottom;
    G3DS_Q16 gradient_midpoint;
    G3DS_U16 flags;
} G3DS_Background;

typedef struct G3DS_Atmosphere_TAG {
    G3DS_Q16 fog_near_plane;
    G3DS_Q16 fog_near_density;
    G3DS_Q16 fog_far_plane;
    G3DS_Q16 fog_far_density;
    G3DS_Color fog_color;
    G3DS_Q16 distance_near_plane;
    G3DS_Q16 distance_near_dim;
    G3DS_Q16 distance_far_plane;
    G3DS_Q16 distance_far_dim;
    G3DS_Q16 layer_z_min;
    G3DS_Q16 layer_z_max;
    G3DS_Q16 layer_density;
    G3DS_U32 layer_flags;
    G3DS_Color layer_color;
    G3DS_U16 flags;
} G3DS_Atmosphere;

typedef struct G3DS_NodePose_TAG {
    G3DS_Q16 pos[3];
    G3DS_Q16 scl[3];
    G3DS_Q16 rot_angle_degrees;
    G3DS_Q16 rot_axis[3];
    G3DS_Color color;
    G3DS_Q16 fov;
    G3DS_Q16 roll;
    G3DS_Q16 hotspot;
    G3DS_Q16 falloff;
    G3DS_U8 hidden;
    G3DS_U8 has_pos;
    G3DS_U8 has_scl;
    G3DS_U8 has_rot;
    G3DS_U8 has_color;
    G3DS_U8 has_fov;
    G3DS_U8 has_roll;
    G3DS_U8 has_hotspot;
    G3DS_U8 has_falloff;
} G3DS_NodePose;

typedef struct G3DS_NormalSet_TAG {
    G3DS_Q16 face_x[G3DS_MAX_FACES];
    G3DS_Q16 face_y[G3DS_MAX_FACES];
    G3DS_Q16 face_z[G3DS_MAX_FACES];
    G3DS_Q16 corner_ax[G3DS_MAX_FACES];
    G3DS_Q16 corner_ay[G3DS_MAX_FACES];
    G3DS_Q16 corner_az[G3DS_MAX_FACES];
    G3DS_Q16 corner_bx[G3DS_MAX_FACES];
    G3DS_Q16 corner_by[G3DS_MAX_FACES];
    G3DS_Q16 corner_bz[G3DS_MAX_FACES];
    G3DS_Q16 corner_cx[G3DS_MAX_FACES];
    G3DS_Q16 corner_cy[G3DS_MAX_FACES];
    G3DS_Q16 corner_cz[G3DS_MAX_FACES];
} G3DS_NormalSet;

typedef struct G3DS_Model_TAG {
    G3DS_Object objects[G3DS_MAX_OBJECTS];
    G3DS_Vertex vertices[G3DS_MAX_VERTICES];
    G3DS_Face faces[G3DS_MAX_FACES];
    G3DS_Material materials[G3DS_MAX_MATERIALS];
    G3DS_Light lights[G3DS_MAX_LIGHTS];
    G3DS_Camera cameras[G3DS_MAX_CAMERAS];
    G3DS_AnimNode anim_nodes[G3DS_MAX_ANIM_NODES];
    G3DS_PosKey pos_keys[G3DS_MAX_POS_KEYS];
    G3DS_RotKey rot_keys[G3DS_MAX_ROT_KEYS];
    G3DS_SclKey scl_keys[G3DS_MAX_SCL_KEYS];
    G3DS_ColorKey color_keys[G3DS_MAX_COLOR_KEYS];
    G3DS_ScalarKey scalar_keys[G3DS_MAX_SCALAR_KEYS];
    G3DS_HideKey hide_keys[G3DS_MAX_HIDE_KEYS];
    G3DS_MorphKey morph_keys[G3DS_MAX_MORPH_KEYS];
    G3DS_ChunkAudit chunk_audit[G3DS_MAX_CHUNK_AUDIT];
    G3DS_Background background;
    G3DS_Atmosphere atmosphere;
    G3DS_DefaultView default_view;
    G3DS_RenderSettings render;
    G3DS_Color ambient_light;
    G3DS_U8 has_ambient_light;

    G3DS_U16 object_count;
    G3DS_U16 vertex_count;
    G3DS_U16 face_count;
    G3DS_U16 material_count;
    G3DS_U16 light_count;
    G3DS_U16 camera_count;
    G3DS_U16 anim_node_count;
    G3DS_U16 pos_key_count;
    G3DS_U16 rot_key_count;
    G3DS_U16 scl_key_count;
    G3DS_U16 color_key_count;
    G3DS_U16 scalar_key_count;
    G3DS_U16 hide_key_count;
    G3DS_U16 morph_key_count;
    G3DS_U16 chunk_audit_count;

    G3DS_Q16 master_scale;
    G3DS_U32 file_version;
    G3DS_U32 mesh_version;
    G3DS_U16 keyframer_revision;
    G3DS_U16 keyframer_version;
    G3DS_U32 keyframer_length;
    G3DS_U32 keyframer_segment_start;
    G3DS_U32 keyframer_segment_end;
    G3DS_U32 keyframer_current_frame;
    char keyframer_name[G3DS_NAME_MAX];

    G3DS_U32 ignored_chunk_count;
    G3DS_U32 last_chunk_id;
    G3DS_U32 error_offset;
} G3DS_Model;

enum {
    G3DS_OK = 0,
    G3DS_ERR_NULL = -1,
    G3DS_ERR_SHORT = -2,
    G3DS_ERR_BAD_CHUNK = -3,
    G3DS_ERR_BAD_MAGIC = -4,
    G3DS_ERR_CAPACITY_OBJECTS = -5,
    G3DS_ERR_CAPACITY_VERTICES = -6,
    G3DS_ERR_CAPACITY_FACES = -7,
    G3DS_ERR_CAPACITY_MATERIALS = -8,
    G3DS_ERR_BAD_INDEX = -9,
    G3DS_ERR_TRUNCATED_STRING = -10,
    G3DS_ERR_CAPACITY_LIGHTS = -11,
    G3DS_ERR_CAPACITY_CAMERAS = -12,
    G3DS_ERR_CAPACITY_ANIM_NODES = -13,
    G3DS_ERR_CAPACITY_KEYS = -14,
    G3DS_ERR_RANGE = -15
};

void g3ds_model_init(G3DS_Model *model);
int g3ds_load_memory(const G3DS_U8 *data, G3DS_U32 size, G3DS_Model *model);
const char *g3ds_result_string(int code);

G3DS_Q16 g3ds_q16_from_i32(G3DS_S32 v);
G3DS_Q16 g3ds_q16_from_ieee32(G3DS_U32 bits);
G3DS_U32 g3ds_q16_to_decimal4(G3DS_Q16 v, G3DS_U32 *whole, G3DS_U32 *frac4);
int g3ds_compute_object_bounds(const G3DS_Model *model, G3DS_U16 object_index, G3DS_Bounds *bounds);
int g3ds_sample_node_pose_step(const G3DS_Model *model, G3DS_U16 node_index, G3DS_U32 frame, G3DS_NodePose *pose);
int g3ds_build_normals(const G3DS_Model *model, G3DS_NormalSet *normals);
int g3ds_find_object(const G3DS_Model *model, const char *name);
int g3ds_find_material(const G3DS_Model *model, const char *name);
int g3ds_find_anim_node_by_id(const G3DS_Model *model, G3DS_U16 node_id);
int g3ds_object_local_matrix(const G3DS_Model *model, G3DS_U16 object_index, G3DS_Q16 out12[12]);
int g3ds_get_chunk_audit(const G3DS_Model *model, G3DS_U16 index, G3DS_ChunkAudit *out_audit);

#ifdef __cplusplus
}
#endif

#endif
