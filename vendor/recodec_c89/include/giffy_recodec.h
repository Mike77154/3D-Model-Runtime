#ifndef GIFFY_RECODEC_H
#define GIFFY_RECODEC_H

/*
   Giffy Resident Evil Model Codec - C89 public API
   Focus: Resident Evil 1/2/3 model interchange for engine/editor/viewer tooling.
   No game assets included.
*/

#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GRC_VERSION_MAJOR 0
#define GRC_VERSION_MINOR 3
#define GRC_VERSION_PATCH 0

/* C89 does not provide stdint.h. These are validated by grc_check_host(). */
typedef unsigned char  grc_u8;
typedef signed char    grc_i8;
typedef unsigned short grc_u16;
typedef signed short   grc_i16;
typedef unsigned int   grc_u32;
typedef signed int     grc_i32;

typedef enum grc_result_e {
    GRC_OK = 0,
    GRC_ERR_BAD_ARGUMENT = -1,
    GRC_ERR_IO = -2,
    GRC_ERR_MEMORY = -3,
    GRC_ERR_BAD_FORMAT = -4,
    GRC_ERR_UNSUPPORTED = -5,
    GRC_ERR_OVERFLOW = -6
} grc_result;

typedef enum grc_format_e {
    GRC_FORMAT_AUTO = 0,
    GRC_FORMAT_GFM = 1,
    GRC_FORMAT_OBJ = 2,
    GRC_FORMAT_TMD = 3,
    GRC_FORMAT_TIM = 4,
    GRC_FORMAT_EMD = 5,
    GRC_FORMAT_PLD = 6,
    GRC_FORMAT_PLW = 7,
    GRC_FORMAT_MD1 = 8,
    GRC_FORMAT_MD2 = 9,
    GRC_FORMAT_UNKNOWN = 255
} grc_format;

typedef enum grc_game_e {
    GRC_GAME_UNKNOWN = 0,
    GRC_GAME_RE1 = 1,
    GRC_GAME_RE2 = 2,
    GRC_GAME_RE3 = 3
} grc_game;

#define GRC_FACE_TRI  3u
#define GRC_FACE_QUAD 4u

#define GRC_FACE_FLAG_TEXTURED            0x00000001u
#define GRC_FACE_FLAG_NATIVE_EXPERIMENTAL 0x00000002u
#define GRC_FACE_FLAG_DEGENERATE          0x00000004u
#define GRC_FACE_FLAG_DOUBLE_SIDED        0x00000008u

#define GRC_MODEL_FLAG_HAS_PIECES         0x00000100u
#define GRC_MODEL_FLAG_GENERATED_NORMALS  0x00000200u
#define GRC_MODEL_FLAG_PSX_QUANTIZED      0x00000400u

#define GRC_CLEAN_DROP_INVALID            0x00000001u
#define GRC_CLEAN_DROP_DEGENERATE         0x00000002u
#define GRC_CLEAN_TRIANGULATE_QUADS       0x00000004u
#define GRC_CLEAN_GENERATE_NORMALS        0x00000008u
#define GRC_CLEAN_REMOVE_UNUSED_VERTICES  0x00000010u
#define GRC_CLEAN_WELD_VERTICES           0x00000020u
#define GRC_CLEAN_PSX_QUANTIZE            0x00000040u

typedef struct grc_vec3_s {
    float x;
    float y;
    float z;
} grc_vec3;

typedef struct grc_vec2_s {
    float u;
    float v;
} grc_vec2;

typedef struct grc_face_s {
    grc_u32 vertex_count;
    grc_i32 v[4];
    grc_i32 vt[4];
    grc_i32 vn[4];
    grc_u8 r;
    grc_u8 g;
    grc_u8 b;
    grc_u8 a;
    grc_i32 material_index;
    grc_u32 flags;
} grc_face;

typedef struct grc_material_s {
    char name[64];
    grc_u8 r;
    grc_u8 g;
    grc_u8 b;
    grc_u8 a;
    grc_i32 texture_index;
} grc_material;

typedef struct grc_texture_s {
    char name[64];
    grc_u32 width;
    grc_u32 height;
    grc_u8 *rgba;
} grc_texture;

typedef struct grc_piece_s {
    char name[64];
    grc_u32 first_vertex;
    grc_u32 vertex_count;
    grc_u32 first_normal;
    grc_u32 normal_count;
    grc_u32 first_uv;
    grc_u32 uv_count;
    grc_u32 first_face;
    grc_u32 face_count;
    grc_i32 parent_index;
    grc_vec3 pivot;
    grc_u32 native_id;
    grc_u32 flags;
} grc_piece;

typedef struct grc_model_stats_s {
    grc_u32 vertex_count;
    grc_u32 normal_count;
    grc_u32 uv_count;
    grc_u32 face_count;
    grc_u32 piece_count;
    grc_u32 triangle_count;
    grc_u32 quad_count;
    grc_u32 textured_face_count;
    grc_u32 invalid_face_count;
    grc_u32 degenerate_face_count;
    grc_u32 unused_vertex_count;
    grc_vec3 bounds_min;
    grc_vec3 bounds_max;
} grc_model_stats;

typedef struct grc_model_s {
    char name[64];
    grc_u32 source_game;
    grc_u32 source_format;
    grc_u32 flags;

    grc_vec3 *vertices;
    grc_u32 vertex_count;
    grc_u32 vertex_capacity;

    grc_vec3 *normals;
    grc_u32 normal_count;
    grc_u32 normal_capacity;

    grc_vec2 *uvs;
    grc_u32 uv_count;
    grc_u32 uv_capacity;

    grc_face *faces;
    grc_u32 face_count;
    grc_u32 face_capacity;

    grc_material *materials;
    grc_u32 material_count;
    grc_u32 material_capacity;

    grc_texture *textures;
    grc_u32 texture_count;
    grc_u32 texture_capacity;

    grc_piece *pieces;
    grc_u32 piece_count;
    grc_u32 piece_capacity;
} grc_model;

typedef struct grc_options_s {
    float scale;
    grc_u32 native_game_hint;
    grc_u32 native_format_hint;
    grc_u32 flags;
    int palette_index;
    float weld_epsilon;
} grc_options;

typedef struct grc_section_s {
    grc_u32 offset;
    grc_u32 size;
    grc_u32 kind;
} grc_section;

typedef struct grc_section_table_s {
    grc_section *sections;
    grc_u32 section_count;
} grc_section_table;

int grc_check_host(void);
const char *grc_result_string(int code);
const char *grc_format_string(grc_u32 fmt);
const char *grc_game_string(grc_u32 game);

void grc_options_default(grc_options *opt);
void grc_model_init(grc_model *model);
void grc_model_free(grc_model *model);
void grc_texture_init(grc_texture *texture);
void grc_texture_free(grc_texture *texture);
void grc_section_table_free(grc_section_table *table);

int grc_model_add_vertex(grc_model *model, float x, float y, float z);
int grc_model_add_normal(grc_model *model, float x, float y, float z);
int grc_model_add_uv(grc_model *model, float u, float v);
int grc_model_add_face(grc_model *model, const grc_face *face);
int grc_model_add_material(grc_model *model, const grc_material *mat);
int grc_model_add_texture(grc_model *model, const grc_texture *texture);
int grc_model_add_piece(grc_model *model, const grc_piece *piece);

int grc_model_analyze(const grc_model *model, grc_model_stats *out_stats);
int grc_model_validate(const grc_model *model, grc_model_stats *out_stats, char *report, size_t report_size);
int grc_model_drop_invalid_faces(grc_model *model, grc_u32 drop_flags);
int grc_model_triangulate_quads(grc_model *model);
int grc_model_generate_flat_normals(grc_model *model);
int grc_model_remove_unused_vertices(grc_model *model);
int grc_model_weld_vertices(grc_model *model, float epsilon);
int grc_model_prepare(grc_model *model, const grc_options *opt);
void grc_model_apply_scale(grc_model *model, float sx, float sy, float sz);
void grc_model_apply_translate(grc_model *model, float tx, float ty, float tz);
void grc_model_flip_v(grc_model *model);
void grc_model_compute_bounds(const grc_model *model, grc_vec3 *min_out, grc_vec3 *max_out);

int grc_probe_file(const char *path, grc_u32 *format_out);
int grc_load_model(const char *path, grc_model *out_model, const grc_options *opt);
int grc_save_model(const char *path, const grc_model *model, const grc_options *opt);

int grc_load_gfm(const char *path, grc_model *out_model);
int grc_save_gfm(const char *path, const grc_model *model);
int grc_load_obj(const char *path, grc_model *out_model, const grc_options *opt);
int grc_save_obj(const char *path, const grc_model *model);
int grc_save_obj_ex(const char *path, const grc_model *model, int write_mtl);
int grc_load_tmd(const char *path, grc_model *out_model, const grc_options *opt);
int grc_save_tmd(const char *path, const grc_model *model, const grc_options *opt);
int grc_load_emd(const char *path, grc_model *out_model, const grc_options *opt);
int grc_load_md1(const char *path, grc_model *out_model, const grc_options *opt);
int grc_load_md2(const char *path, grc_model *out_model, const grc_options *opt);
int grc_save_emd_re1(const char *path, const grc_model *model, const grc_options *opt);
int grc_save_emd_re2(const char *path, const grc_model *model, const grc_options *opt);
int grc_save_emd_re3(const char *path, const grc_model *model, const grc_options *opt);
int grc_save_pld_re2(const char *path, const grc_model *model, const grc_options *opt);
int grc_save_pld_re3(const char *path, const grc_model *model, const grc_options *opt);
int grc_save_md1(const char *path, const grc_model *model, const grc_options *opt);
int grc_save_md2(const char *path, const grc_model *model, const grc_options *opt);

int grc_tim_decode_rgba(const grc_u8 *data, size_t size, grc_texture *out_texture, int palette_index);
int grc_tim_load_rgba(const char *path, grc_texture *out_texture, int palette_index);
int grc_texture_save_ppm(const char *path, const grc_texture *texture);

int grc_re_read_section_table(const grc_u8 *data, size_t size, grc_section_table *out_table, grc_u32 format_hint);
int grc_re_extract_sections(const char *path, const char *prefix, grc_u32 format_hint);

#ifdef __cplusplus
}
#endif

#endif /* GIFFY_RECODEC_H */
