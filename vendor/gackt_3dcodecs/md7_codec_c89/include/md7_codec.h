#ifndef MD7_CODEC_H
#define MD7_CODEC_H

/*
   md7_codec.h - C89 3D GameStudio A7 MDL7 reader.

   Contract:
   - C89.
   - No malloc/free/realloc inside the codec.
   - Caller supplies every output array.
   - No float/double types in the codec. IEEE-754 little-endian f32 values
     from the file are converted to signed 16.16 fixed point.
   - Parser consumes a caller-owned memory buffer. Demo tools may use stdio.
*/

#ifdef __cplusplus
extern "C" {
#endif

#define MD7_CODEC_VERSION_MAJOR 0
#define MD7_CODEC_VERSION_MINOR 2
#define MD7_CODEC_VERSION_PATCH 0

#define MD7_FX_SHIFT 16
#define MD7_FX_ONE   65536L
#define MD7_FX_HALF  32768L

#define MD7_U32_MAX  0xffffffffUL
#define MD7_I32_MAX  2147483647L
#define MD7_I32_MIN  ((md7_i32)(-2147483647L - 1L))

typedef signed long md7_i32;
typedef unsigned long md7_u32;
typedef signed short md7_i16;
typedef unsigned short md7_u16;
typedef signed char md7_i8;
typedef unsigned char md7_u8;
typedef md7_i32 md7_fx16;

enum md7_result {
    MD7_OK = 0,
    MD7_E_NULL = -1,
    MD7_E_TOO_SMALL = -2,
    MD7_E_BAD_MAGIC = -3,
    MD7_E_UNSUPPORTED = -4,
    MD7_E_BOUNDS = -5,
    MD7_E_CAPACITY = -6,
    MD7_E_BAD_SIZE = -7,
    MD7_E_BAD_COUNT = -8,
    MD7_E_OVERFLOW = -9
};

enum md7_warning_bits {
    MD7_WARN_CAP_BONES       = 0x00000001UL,
    MD7_WARN_CAP_GROUPS      = 0x00000002UL,
    MD7_WARN_CAP_SKINS       = 0x00000004UL,
    MD7_WARN_CAP_UVS         = 0x00000008UL,
    MD7_WARN_CAP_TRIANGLES   = 0x00000010UL,
    MD7_WARN_CAP_VERTICES    = 0x00000020UL,
    MD7_WARN_CAP_FRAMES      = 0x00000040UL,
    MD7_WARN_CAP_FRAMEVERTS  = 0x00000080UL,
    MD7_WARN_CAP_TRANSFORMS  = 0x00000100UL,
    MD7_WARN_CAP_MATERIALS   = 0x00000200UL,
    MD7_WARN_CAP_DEFORMERS   = 0x00000400UL,
    MD7_WARN_CAP_ELEMENTS    = 0x00000800UL,
    MD7_WARN_CAP_WEIGHTS     = 0x00001000UL,
    MD7_WARN_SKIN_GUESS      = 0x00002000UL,
    MD7_WARN_TRAILING_GROUP  = 0x00004000UL,
    MD7_WARN_INDEX_RANGE     = 0x00008000UL,
    MD7_WARN_CLAMPED_FIXED   = 0x00010000UL,
    MD7_WARN_NON_TRI_GROUP   = 0x00020000UL,
    MD7_WARN_ASCDEF          = 0x00040000UL,
    MD7_WARN_DEFORMER_PARSE  = 0x00080000UL,
    MD7_WARN_ZERO_COUNTS     = 0x00100000UL
};

enum md7_skin_flags {
    MD7_SKIN_MIPFLAG         = 0x08U,
    MD7_SKIN_MATERIAL        = 0x10U,
    MD7_SKIN_MATERIAL_ASCDEF = 0x20U,
    MD7_SKIN_RGBFLAG         = 0x80U
};

#define MD7_NAME_GROUP 16
#define MD7_NAME_BONE  32
#define MD7_NAME_TEX   16
#define MD7_NAME_FRAME 16
#define MD7_NAME_DEFORMER_ELEMENT 20

struct md7_header {
    char ident[5];
    md7_i32 version;
    md7_u32 bones_num;
    md7_u32 groups_num;
    md7_u32 data_size;
    md7_i32 entlump_size;
    md7_i32 medlump_size;
    md7_u16 bone_stc_size;
    md7_u16 skin_stc_size;
    md7_u16 colorvalue_stc_size;
    md7_u16 material_stc_size;
    md7_u16 skinpoint_stc_size;
    md7_u16 triangle_stc_size;
    md7_u16 mainvertex_stc_size;
    md7_u16 framevertex_stc_size;
    md7_u16 bonetrans_stc_size;
    md7_u16 frame_stc_size;
};

struct md7_bounds {
    md7_fx16 min_x;
    md7_fx16 min_y;
    md7_fx16 min_z;
    md7_fx16 max_x;
    md7_fx16 max_y;
    md7_fx16 max_z;
    md7_u8 valid;
};

struct md7_bone {
    md7_u32 file_offset;
    md7_u16 parent_index;
    md7_fx16 x;
    md7_fx16 y;
    md7_fx16 z;
    char name[MD7_NAME_BONE + 1];
};

struct md7_material {
    md7_u32 file_offset;
    md7_fx16 diffuse[4];
    md7_fx16 ambient[4];
    md7_fx16 specular[4];
    md7_fx16 emissive[4];
    md7_fx16 power;
};

struct md7_skin {
    md7_u32 file_offset;
    md7_u32 payload_offset;
    md7_u32 payload_size;
    md7_u32 raw_pixels_offset;
    md7_u32 raw_pixels_size;
    md7_u32 ascdef_offset;
    md7_u32 ascdef_size;
    md7_u32 material_index;
    md7_u8 typ;
    md7_u8 bytes_per_pixel;
    md7_u8 has_mips;
    md7_i32 width;
    md7_i32 height;
    char texture_name[MD7_NAME_TEX + 1];
};

struct md7_uv {
    md7_u32 file_offset;
    md7_fx16 u;
    md7_fx16 v;
};

struct md7_triangle {
    md7_u32 file_offset;
    md7_u16 v_index[3];
    md7_u16 uv1_index[3];
    md7_i32 material1;
    md7_u16 uv2_index[3];
    md7_i32 material2;
    md7_u8 has_uv1;
    md7_u8 has_mat1;
    md7_u8 has_uv2;
    md7_u8 has_mat2;
    md7_u8 index_warning;
};

struct md7_vertex {
    md7_u32 file_offset;
    md7_fx16 x;
    md7_fx16 y;
    md7_fx16 z;
    md7_u16 index;
    md7_u8 normal_mode;       /* 0 none, 1 packed MD2-style index, 2 vector */
    md7_u8 normal_index;
    md7_fx16 nx;
    md7_fx16 ny;
    md7_fx16 nz;
};

struct md7_frame {
    md7_u32 file_offset;
    md7_u32 vertices_offset;
    md7_u32 transforms_offset;
    char name[MD7_NAME_FRAME + 1];
    md7_u32 first_framevertex;
    md7_u32 vertices_count;
    md7_u32 first_transform;
    md7_u32 transmatrix_count;
    struct md7_bounds bounds;
};

struct md7_bonetransform {
    md7_u32 file_offset;
    md7_fx16 m[16];
    md7_u16 bone_index;
};

struct md7_deformer {
    md7_u32 file_offset;
    md7_i8 version;
    md7_i8 typ;
    md7_i32 group_index;
    md7_u32 first_element;
    md7_u32 elements;
    md7_u32 data_size;
};

struct md7_deformer_element {
    md7_u32 file_offset;
    md7_u32 deformer_index;
    md7_i32 element_index;
    char element_name[MD7_NAME_DEFORMER_ELEMENT + 1];
    md7_u32 first_weight;
    md7_u32 weights;
};

struct md7_deformer_weight {
    md7_u32 file_offset;
    md7_u32 element_index;
    md7_i32 vertex_index;
    md7_fx16 weight;
};

struct md7_group {
    md7_u32 file_offset;
    md7_u32 data_offset;
    md7_u32 data_size;
    md7_u8 typ;
    md7_i8 deformers;
    md7_i8 max_weights;
    char name[MD7_NAME_GROUP + 1];

    md7_u32 first_skin;
    md7_u32 numskins;
    md7_u32 first_uv;
    md7_u32 num_stpts;
    md7_u32 first_triangle;
    md7_u32 numtris;
    md7_u32 first_vertex;
    md7_u32 numverts;
    md7_u32 first_frame;
    md7_u32 numframes;
    md7_u32 first_deformer;
    md7_u32 num_deformers_parsed;

    md7_u32 invalid_vertex_indices;
    md7_u32 invalid_uv_indices;
    md7_u32 invalid_material_indices;
    md7_u32 extra_offset;
    md7_u32 extra_size;
    struct md7_bounds bounds;
};

struct md7_model {
    struct md7_header header;

    struct md7_bone *bones;
    md7_u32 bone_cap;
    md7_u32 bone_count;
    md7_u32 bone_needed;

    struct md7_group *groups;
    md7_u32 group_cap;
    md7_u32 group_count;
    md7_u32 group_needed;

    struct md7_skin *skins;
    md7_u32 skin_cap;
    md7_u32 skin_count;
    md7_u32 skin_needed;

    struct md7_material *materials;
    md7_u32 material_cap;
    md7_u32 material_count;
    md7_u32 material_needed;

    struct md7_uv *uvs;
    md7_u32 uv_cap;
    md7_u32 uv_count;
    md7_u32 uv_needed;

    struct md7_triangle *triangles;
    md7_u32 triangle_cap;
    md7_u32 triangle_count;
    md7_u32 triangle_needed;

    struct md7_vertex *vertices;
    md7_u32 vertex_cap;
    md7_u32 vertex_count;
    md7_u32 vertex_needed;

    struct md7_frame *frames;
    md7_u32 frame_cap;
    md7_u32 frame_count;
    md7_u32 frame_needed;

    struct md7_vertex *framevertices;
    md7_u32 framevertex_cap;
    md7_u32 framevertex_count;
    md7_u32 framevertex_needed;

    struct md7_bonetransform *transforms;
    md7_u32 transform_cap;
    md7_u32 transform_count;
    md7_u32 transform_needed;

    struct md7_deformer *deformers;
    md7_u32 deformer_cap;
    md7_u32 deformer_count;
    md7_u32 deformer_needed;

    struct md7_deformer_element *deformer_elements;
    md7_u32 deformer_element_cap;
    md7_u32 deformer_element_count;
    md7_u32 deformer_element_needed;

    struct md7_deformer_weight *deformer_weights;
    md7_u32 deformer_weight_cap;
    md7_u32 deformer_weight_count;
    md7_u32 deformer_weight_needed;

    md7_u32 entlump_offset;
    md7_u32 entlump_size;
    md7_u32 medlump_offset;
    md7_u32 medlump_size;

    struct md7_bounds bounds;
    md7_u32 warnings;
};

void md7_model_reset(struct md7_model *m);
int md7_load_memory(const md7_u8 *data, md7_u32 size, struct md7_model *out_model);
const char *md7_result_name(int code);
const char *md7_warning_name(md7_u32 bit);
md7_u32 md7_warning_capacity_mask(void);
md7_fx16 md7_f32le_to_fx16(const md7_u8 *p, md7_u32 *warn_flags);
md7_i32 md7_fx16_floor_to_i32(md7_fx16 v);
md7_i32 md7_fx16_round_to_i32(md7_fx16 v);
md7_i32 md7_fx16_to_milli(md7_fx16 v);

#ifdef __cplusplus
}
#endif

#endif
