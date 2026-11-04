#ifndef LXO3D_H
#define LXO3D_H

/*
   lxo3d: tiny static-buffer LXO/LXOB geometry decoder.
   C89, no heap allocation, no float/double types in the public API.

   Fixed point format: signed 16.16 stored in long.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LXO3D_MAX_LAYERS
#define LXO3D_MAX_LAYERS 16u
#endif

#ifndef LXO3D_MAX_POINTS
#define LXO3D_MAX_POINTS 65536u
#endif

#ifndef LXO3D_MAX_POLYS
#define LXO3D_MAX_POLYS 65536u
#endif

#ifndef LXO3D_MAX_POLY_INDEXES
#define LXO3D_MAX_POLY_INDEXES 262144u
#endif

#ifndef LXO3D_MAX_TAGS
#define LXO3D_MAX_TAGS 512u
#endif

#ifndef LXO3D_MAX_STRING
#define LXO3D_MAX_STRING 64u
#endif

#ifndef LXO3D_MAX_MAPS
#define LXO3D_MAX_MAPS 64u
#endif

#ifndef LXO3D_MAX_ITEMS
#define LXO3D_MAX_ITEMS 1024u
#endif

#ifndef LXO3D_MAX_LINKS
#define LXO3D_MAX_LINKS 4096u
#endif

#ifndef LXO3D_MAX_CHANNEL_NAMES
#define LXO3D_MAX_CHANNEL_NAMES 2048u
#endif

#ifndef LXO3D_MAX_CHANNELS
#define LXO3D_MAX_CHANNELS 8192u
#endif

#ifndef LXO3D_MAX_ENVELOPES
#define LXO3D_MAX_ENVELOPES 1024u
#endif

#ifndef LXO3D_MAX_ENV_KEYS
#define LXO3D_MAX_ENV_KEYS 8192u
#endif

#ifndef LXO3D_MAX_ACTIONS
#define LXO3D_MAX_ACTIONS 128u
#endif

#ifndef LXO3D_MAX_BCHN
#define LXO3D_MAX_BCHN 2048u
#endif

#ifndef LXO3D_MAX_DATA
#define LXO3D_MAX_DATA 512u
#endif

#ifndef LXO3D_MAX_DATA_PAIRS
#define LXO3D_MAX_DATA_PAIRS 2048u
#endif

#ifndef LXO3D_MAX_CHANNEL_LINKS
#define LXO3D_MAX_CHANNEL_LINKS 2048u
#endif

#ifndef LXO3D_MAX_VMAP_VALUES
#define LXO3D_MAX_VMAP_VALUES 131072u
#endif

#ifndef LXO3D_MAX_VMAD_VALUES
#define LXO3D_MAX_VMAD_VALUES 131072u
#endif

#define LXO3D_FIXED_ONE 65536L

#define LXO3D_OK 0
#define LXO3D_E_BAD_ARG -1
#define LXO3D_E_TOO_SMALL -2
#define LXO3D_E_NOT_FORM -3
#define LXO3D_E_NOT_LXOB -4
#define LXO3D_E_TRUNCATED -5
#define LXO3D_E_BAD_CHUNK -6
#define LXO3D_E_OVERFLOW -7

#define LXO3D_FLAG_TRUNCATED_OUTPUT 0x00000001UL
#define LXO3D_FLAG_UNSUPPORTED_FORM  0x00000002UL
#define LXO3D_FLAG_BAD_INDEX         0x00000004UL
#define LXO3D_FLAG_FIXED_SATURATED   0x00000008UL
#define LXO3D_FLAG_UNKNOWN_CHUNKS    0x00000010UL
#define LXO3D_FLAG_SCENE_ONLY        0x00000020UL
#define LXO3D_FLAG_SCENE_META         0x00000040UL
#define LXO3D_FLAG_PROC_UNEVALUATED   0x00000080UL
#define LXO3D_FLAG_ANIM_META          0x00000100UL
#define LXO3D_FLAG_CONSTRAINT_META    0x00000200UL
#define LXO3D_FLAG_SHADER_META        0x00000400UL
#define LXO3D_FLAG_PROC_APPROX        0x00000800UL
#define LXO3D_FLAG_WORLD_XFORM_APPROX 0x00001000UL

#define LXO3D_MAP_KIND_VMAP 1u
#define LXO3D_MAP_KIND_VMAD 2u

#define LXO3D_CHAN_INT 1u
#define LXO3D_CHAN_FLOAT 2u
#define LXO3D_CHAN_STRING 3u
#define LXO3D_CHAN_ENVELOPE 0x10u

#define LXO3D_ITEM_KIND_OTHER 0u
#define LXO3D_ITEM_KIND_MESH 1u
#define LXO3D_ITEM_KIND_CAMERA 2u
#define LXO3D_ITEM_KIND_LIGHT 3u
#define LXO3D_ITEM_KIND_MATERIAL 4u
#define LXO3D_ITEM_KIND_SHADER 5u
#define LXO3D_ITEM_KIND_LOCATOR 6u
#define LXO3D_ITEM_KIND_TRANSFORM 7u
#define LXO3D_ITEM_KIND_MESHOP 8u

#define LXO3D_PROC_UNKNOWN 0u
#define LXO3D_PROC_UNION 1u
#define LXO3D_PROC_INTERSECT 2u
#define LXO3D_PROC_SUBTRACT 3u
#define LXO3D_PROC_BLEND 4u

#define LXO3D_CONSTRAINT_UNKNOWN 0u
#define LXO3D_CONSTRAINT_PARENT 1u
#define LXO3D_CONSTRAINT_CHANNEL 2u
#define LXO3D_CONSTRAINT_XFRMCORE 3u

#define LXO3D_ID(a,b,c,d) ((((unsigned long)(unsigned char)(a))<<24) | \
                           (((unsigned long)(unsigned char)(b))<<16) | \
                           (((unsigned long)(unsigned char)(c))<<8)  | \
                           ((unsigned long)(unsigned char)(d)))

#define LXO3D_ID_FACE LXO3D_ID('F','A','C','E')
#define LXO3D_ID_SUBD LXO3D_ID('S','U','B','D')
#define LXO3D_ID_PSUB LXO3D_ID('P','S','U','B')
#define LXO3D_ID_TXUV LXO3D_ID('T','X','U','V')
#define LXO3D_ID_NORM LXO3D_ID('N','O','R','M')
#define LXO3D_ID_MATR LXO3D_ID('M','A','T','R')
#define LXO3D_ID_PART LXO3D_ID('P','A','R','T')
#define LXO3D_ID_SMGP LXO3D_ID('S','M','G','P')

typedef long Lxo3D_Fixed;

typedef struct Lxo3D_Vec3_s {
    Lxo3D_Fixed x;
    Lxo3D_Fixed y;
    Lxo3D_Fixed z;
} Lxo3D_Vec3;

typedef struct Lxo3D_Layer_s {
    unsigned long legacy_index;
    unsigned long flags;
    Lxo3D_Vec3 pivot;
    Lxo3D_Vec3 scale_pivot;
    char name[LXO3D_MAX_STRING];
    long parent_index;
    unsigned long item_ref;
    unsigned long first_point;
    unsigned long point_count;
    unsigned long first_poly;
    unsigned long poly_count;
} Lxo3D_Layer;

typedef struct Lxo3D_Poly_s {
    unsigned long type_id;
    unsigned long flags;
    unsigned long layer_index;
    unsigned long first_index;
    unsigned long index_count;
    unsigned long material_tag;
    unsigned long part_tag;
    unsigned long smoothing_tag;
} Lxo3D_Poly;

typedef struct Lxo3D_Map_s {
    unsigned long kind;
    unsigned long type_id;
    unsigned long dimension;
    char name[LXO3D_MAX_STRING];
    unsigned long first_value;
    unsigned long value_count;
} Lxo3D_Map;

typedef struct Lxo3D_VMapValue_s {
    unsigned long map_index;
    unsigned long point_index;
    Lxo3D_Fixed value[6];
} Lxo3D_VMapValue;

typedef struct Lxo3D_VMadValue_s {
    unsigned long map_index;
    unsigned long point_index;
    unsigned long poly_index;
    Lxo3D_Fixed value[6];
} Lxo3D_VMadValue;

typedef struct Lxo3D_Item_s {
    unsigned long kind;
    unsigned long ref_id;
    char type[LXO3D_MAX_STRING];
    char name[LXO3D_MAX_STRING];
    char visible_name[LXO3D_MAX_STRING];
    char unique_name[LXO3D_MAX_STRING];
    unsigned long layer_flags;
    unsigned char rgba[4];
    unsigned long first_link;
    unsigned long link_count;
    unsigned long first_channel;
    unsigned long channel_count;
    unsigned long first_bchn;
    unsigned long bchn_count;
    unsigned long parent_ref;
    long parent_link_index;
} Lxo3D_Item;

typedef struct Lxo3D_Link_s {
    unsigned long item_index;
    char graph[LXO3D_MAX_STRING];
    unsigned long target_ref;
    long link_index;
} Lxo3D_Link;

typedef struct Lxo3D_Channel_s {
    unsigned long item_index;
    unsigned long action_index;
    unsigned long channel_name_index;
    unsigned long type;
    unsigned long envelope_index;
    char name[LXO3D_MAX_STRING];
    char string_value[LXO3D_MAX_STRING];
    long int_value;
    Lxo3D_Fixed fixed_value;
} Lxo3D_Channel;

typedef struct Lxo3D_Envelope_s {
    unsigned long index;
    unsigned long type;
    unsigned long pre_behavior;
    unsigned long post_behavior;
    unsigned long first_key;
    unsigned long key_count;
} Lxo3D_Envelope;

typedef struct Lxo3D_EnvKey_s {
    unsigned long envelope_array_index;
    Lxo3D_Fixed input;
    Lxo3D_Fixed value;
    unsigned long flags;
    unsigned long tani_type;
    unsigned long tano_type;
    Lxo3D_Fixed tani_weight;
    Lxo3D_Fixed tani_value;
    Lxo3D_Fixed tano_weight;
    Lxo3D_Fixed tano_value;
} Lxo3D_EnvKey;

typedef struct Lxo3D_Action_s {
    char name[LXO3D_MAX_STRING];
    char type[LXO3D_MAX_STRING];
    unsigned long first_channel;
    unsigned long channel_count;
} Lxo3D_Action;

typedef struct Lxo3D_BChannel_s {
    unsigned long item_index;
    char operation_type[LXO3D_MAX_STRING];
    unsigned long data_index;
} Lxo3D_BChannel;

typedef struct Lxo3D_DataPair_s {
    unsigned long data_array_index;
    char name[LXO3D_MAX_STRING];
    char value[LXO3D_MAX_STRING];
} Lxo3D_DataPair;

typedef struct Lxo3D_Data_s {
    unsigned long index;
    unsigned long unknown;
    char exotype[LXO3D_MAX_STRING];
    unsigned long first_pair;
    unsigned long pair_count;
} Lxo3D_Data;

typedef struct Lxo3D_Transform_s {
    Lxo3D_Vec3 position;
    Lxo3D_Vec3 rotation;
    Lxo3D_Vec3 scale;
    unsigned long has_position;
    unsigned long has_rotation;
    unsigned long has_scale;
} Lxo3D_Transform;

typedef struct Lxo3D_Mat4_s {
    Lxo3D_Fixed m[16];
} Lxo3D_Mat4;

typedef struct Lxo3D_BBox_s {
    Lxo3D_Vec3 minv;
    Lxo3D_Vec3 maxv;
    unsigned long valid;
} Lxo3D_BBox;

typedef struct Lxo3D_AdvancedTransform_s {
    Lxo3D_Transform trs;
    Lxo3D_Vec3 rotate_pivot;
    Lxo3D_Vec3 scale_pivot;
    unsigned long has_rotate_pivot;
    unsigned long has_scale_pivot;
    unsigned long parent_item_index;
    unsigned long depth;
    unsigned long flags;
    Lxo3D_Mat4 local_matrix;
    Lxo3D_Mat4 world_matrix;
} Lxo3D_AdvancedTransform;

typedef struct Lxo3D_ChannelLink_s {
    unsigned long item_index;
    char graph[LXO3D_MAX_STRING];
    char from_channel[LXO3D_MAX_STRING];
    unsigned long from_ref;
    char to_channel[LXO3D_MAX_STRING];
    long from_link_index;
    long to_link_index;
} Lxo3D_ChannelLink;

typedef struct Lxo3D_ConstraintInfo_s {
    unsigned long kind;
    unsigned long item_index;
    unsigned long target_item_index;
    char graph[LXO3D_MAX_STRING];
    char from_channel[LXO3D_MAX_STRING];
    char to_channel[LXO3D_MAX_STRING];
    long order_index;
} Lxo3D_ConstraintInfo;

typedef struct Lxo3D_MaterialInfo_s {
    unsigned long item_index;
    Lxo3D_Fixed diffuse_r;
    Lxo3D_Fixed diffuse_g;
    Lxo3D_Fixed diffuse_b;
    Lxo3D_Fixed opacity;
    Lxo3D_Fixed roughness;
    Lxo3D_Fixed specular;
    unsigned long has_color;
    unsigned long has_opacity;
} Lxo3D_MaterialInfo;

typedef struct Lxo3D_CameraInfo_s {
    unsigned long item_index;
    Lxo3D_Fixed focal_length;
    Lxo3D_Fixed film_aperture_x;
    Lxo3D_Fixed film_aperture_y;
    Lxo3D_Fixed near_clip;
    Lxo3D_Fixed far_clip;
    unsigned long has_focal_length;
    unsigned long has_clip;
} Lxo3D_CameraInfo;

typedef struct Lxo3D_LightInfo_s {
    unsigned long item_index;
    Lxo3D_Fixed intensity;
    Lxo3D_Fixed color_r;
    Lxo3D_Fixed color_g;
    Lxo3D_Fixed color_b;
    unsigned long has_intensity;
    unsigned long has_color;
} Lxo3D_LightInfo;

typedef struct Lxo3D_ProcOpInfo_s {
    unsigned long item_index;
    unsigned long op_kind;
    char op_name[LXO3D_MAX_STRING];
    char exotype[LXO3D_MAX_STRING];
    unsigned long data_index;
    unsigned long bbox_approximated;
    Lxo3D_BBox bbox;
} Lxo3D_ProcOpInfo;

typedef struct Lxo3D_Model_s {
    unsigned long form_type;
    unsigned long version_major;
    unsigned long version_minor;
    char app_name[LXO3D_MAX_STRING];

    unsigned long flags;
    unsigned long chunk_count;
    unsigned long unknown_chunk_count;

    unsigned long layer_count;
    Lxo3D_Layer layers[LXO3D_MAX_LAYERS];

    unsigned long point_count;
    Lxo3D_Vec3 points[LXO3D_MAX_POINTS];

    unsigned long poly_count;
    Lxo3D_Poly polys[LXO3D_MAX_POLYS];
    unsigned long poly_indices[LXO3D_MAX_POLY_INDEXES];
    unsigned long poly_index_count;

    unsigned long tag_count;
    char tags[LXO3D_MAX_TAGS][LXO3D_MAX_STRING];

    unsigned long map_count;
    Lxo3D_Map maps[LXO3D_MAX_MAPS];
    unsigned long vmap_value_count;
    Lxo3D_VMapValue vmap_values[LXO3D_MAX_VMAP_VALUES];
    unsigned long vmad_value_count;
    Lxo3D_VMadValue vmad_values[LXO3D_MAX_VMAD_VALUES];

    unsigned long channel_name_count;
    char channel_names[LXO3D_MAX_CHANNEL_NAMES][LXO3D_MAX_STRING];

    unsigned long item_count;
    Lxo3D_Item items[LXO3D_MAX_ITEMS];

    unsigned long link_count;
    Lxo3D_Link links[LXO3D_MAX_LINKS];

    unsigned long channel_link_count;
    Lxo3D_ChannelLink channel_links[LXO3D_MAX_CHANNEL_LINKS];

    unsigned long channel_count;
    Lxo3D_Channel channels[LXO3D_MAX_CHANNELS];

    unsigned long envelope_count;
    Lxo3D_Envelope envelopes[LXO3D_MAX_ENVELOPES];
    unsigned long env_key_count;
    Lxo3D_EnvKey env_keys[LXO3D_MAX_ENV_KEYS];

    unsigned long action_count;
    Lxo3D_Action actions[LXO3D_MAX_ACTIONS];

    unsigned long bchn_count;
    Lxo3D_BChannel bchannels[LXO3D_MAX_BCHN];

    unsigned long data_count;
    Lxo3D_Data datas[LXO3D_MAX_DATA];
    unsigned long data_pair_count;
    Lxo3D_DataPair data_pairs[LXO3D_MAX_DATA_PAIRS];
} Lxo3D_Model;

typedef int (*Lxo3D_TriangleCB)(void *user,
                                unsigned long poly_index,
                                unsigned long a,
                                unsigned long b,
                                unsigned long c,
                                unsigned long material_tag);

void lxo3d_clear(Lxo3D_Model *model);
int lxo3d_decode(const unsigned char *data, unsigned long size, Lxo3D_Model *model);
int lxo3d_emit_triangles(const Lxo3D_Model *model, Lxo3D_TriangleCB cb, void *user);
int lxo3d_find_item_by_ref(const Lxo3D_Model *model, unsigned long ref_id, unsigned long *out_index);
int lxo3d_item_transform(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_Transform *out_transform);
int lxo3d_item_advanced_transform(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_AdvancedTransform *out_transform);
int lxo3d_transform_point_world(const Lxo3D_Model *model, unsigned long item_index, const Lxo3D_Vec3 *src, Lxo3D_Vec3 *dst);
int lxo3d_eval_envelope_linear(const Lxo3D_Model *model, unsigned long envelope_index, Lxo3D_Fixed input, Lxo3D_Fixed *out_value);
int lxo3d_eval_channel_fixed(const Lxo3D_Model *model, unsigned long item_index, const char *channel_name, Lxo3D_Fixed time, Lxo3D_Fixed *out_value);
int lxo3d_extract_constraint(const Lxo3D_Model *model, unsigned long ordinal, Lxo3D_ConstraintInfo *out_info);
int lxo3d_extract_material(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_MaterialInfo *out_info);
int lxo3d_extract_camera(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_CameraInfo *out_info);
int lxo3d_extract_light(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_LightInfo *out_info);
int lxo3d_extract_proc_op(const Lxo3D_Model *model, unsigned long bchn_index, Lxo3D_ProcOpInfo *out_info);
int lxo3d_compute_item_bbox(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_BBox *out_bbox);
const Lxo3D_Channel *lxo3d_find_item_channel(const Lxo3D_Model *model, unsigned long item_index, const char *channel_name);

const char *lxo3d_result_string(int code);
void lxo3d_id_to_text(unsigned long id, char out_text[5]);

#ifdef __cplusplus
}
#endif

#endif
