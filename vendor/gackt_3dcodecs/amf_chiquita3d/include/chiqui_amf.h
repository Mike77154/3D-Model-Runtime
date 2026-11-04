#ifndef CHIQUI_AMF_H
#define CHIQUI_AMF_H

/*
   chiqui_amf.h - AMF v1.x reader/writer core, C89, arena-only.
   No heap allocation is performed by the library.
*/

#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#if UINT_MAX == 4294967295U
typedef signed int amf_i32;
typedef unsigned int amf_u32;
#elif ULONG_MAX == 4294967295UL
typedef signed long amf_i32;
typedef unsigned long amf_u32;
#else
#error "chiqui_amf needs a 32-bit int or long type"
#endif

typedef signed short amf_i16;
typedef unsigned short amf_u16;
typedef signed char amf_i8;
typedef unsigned char amf_u8;
typedef int amf_bool;

typedef amf_i32 amf_fx;

#define AMF_FALSE 0
#define AMF_TRUE 1
#define AMF_FX_SHIFT 12
#define AMF_FX_ONE ((amf_fx)(1L << AMF_FX_SHIFT))
#define AMF_MAX_XML_ATTRS 24
#define AMF_MAX_XML_DEPTH 64
#define AMF_MAX_DIAG_TEXT 160

#define AMF_FLAG_PRESENT 1u
#define AMF_FLAG_HAS_COLOR 2u
#define AMF_FLAG_HAS_NORMAL 4u
#define AMF_FLAG_HAS_MATERIAL 8u
#define AMF_FLAG_HAS_TEXMAP 16u

#define AMF_OWNER_DOCUMENT 0u
#define AMF_OWNER_OBJECT 1u
#define AMF_OWNER_VOLUME 2u
#define AMF_OWNER_MATERIAL 3u
#define AMF_OWNER_VERTEX 4u
#define AMF_OWNER_TRIANGLE 5u
#define AMF_OWNER_TEXTURE 6u
#define AMF_OWNER_CONSTELLATION 7u

#define AMF_ZIP_METHOD_STORE 0u
#define AMF_ZIP_METHOD_DEFLATE 8u

typedef struct amf_slice_s {
    const char *ptr;
    size_t len;
} amf_slice;

typedef struct amf_arena_s {
    amf_u8 *base;
    size_t size;
    size_t used;
} amf_arena;

typedef enum amf_error_e {
    AMF_OK = 0,
    AMF_ERR_BAD_ARG = 1,
    AMF_ERR_ARENA_FULL = 2,
    AMF_ERR_BAD_XML = 3,
    AMF_ERR_BAD_NUMBER = 4,
    AMF_ERR_RANGE = 5,
    AMF_ERR_REQUIRED = 6,
    AMF_ERR_UNSUPPORTED_ZIP = 7,
    AMF_ERR_ZIP_NEEDS_OUTPUT = 8,
    AMF_ERR_BAD_ZIP = 9,
    AMF_ERR_WRITE_FULL = 10,
    AMF_ERR_VALIDATE = 11
} amf_error;

typedef enum amf_unit_e {
    AMF_UNIT_MM = 0,
    AMF_UNIT_INCH = 1,
    AMF_UNIT_FEET = 2,
    AMF_UNIT_METER = 3,
    AMF_UNIT_MICRON = 4,
    AMF_UNIT_UNKNOWN = 5
} amf_unit;

typedef struct amf_diag_s {
    int code;
    size_t line;
    size_t column;
    amf_u32 warnings;
    char message[AMF_MAX_DIAG_TEXT];
} amf_diag;

typedef struct amf_vec3_s {
    amf_fx x;
    amf_fx y;
    amf_fx z;
} amf_vec3;

typedef struct amf_color_s {
    amf_fx r;
    amf_fx g;
    amf_fx b;
    amf_fx a;
    amf_u32 flags;
} amf_color;

typedef struct amf_metadata_s {
    amf_u32 owner_kind;
    size_t owner_index;
    amf_slice type;
    amf_slice text;
} amf_metadata;

typedef struct amf_vertex_s {
    amf_vec3 p;
    amf_vec3 n;
    amf_color color;
    amf_u32 flags;
} amf_vertex;

typedef struct amf_triangle_s {
    size_t object_index;
    size_t volume_index;
    amf_u32 v1;
    amf_u32 v2;
    amf_u32 v3;
    amf_color color;
    amf_u32 flags;
} amf_triangle;

typedef struct amf_volume_s {
    size_t object_index;
    amf_u32 material_id;
    amf_color color;
    size_t triangle_first;
    size_t triangle_count;
    size_t metadata_first;
    size_t metadata_count;
    amf_u32 flags;
} amf_volume;

typedef struct amf_object_s {
    amf_u32 id;
    amf_u32 material_id;
    amf_color color;
    size_t vertex_first;
    size_t vertex_count;
    size_t volume_first;
    size_t volume_count;
    size_t triangle_first;
    size_t triangle_count;
    size_t metadata_first;
    size_t metadata_count;
    amf_vec3 bbox_min;
    amf_vec3 bbox_max;
    amf_u32 flags;
} amf_object;

typedef struct amf_composite_s {
    size_t material_index;
    amf_u32 material_id;
    amf_slice formula;
    amf_u32 flags;
} amf_composite;

typedef struct amf_material_s {
    amf_u32 id;
    amf_color color;
    size_t composite_first;
    size_t composite_count;
    size_t metadata_first;
    size_t metadata_count;
    amf_u32 flags;
} amf_material;

typedef struct amf_texture_s {
    amf_u32 id;
    amf_u32 width;
    amf_u32 height;
    amf_u32 depth;
    amf_slice type;
    amf_slice data_base64;
    size_t metadata_first;
    size_t metadata_count;
    amf_u32 flags;
} amf_texture;

typedef struct amf_instance_s {
    size_t constellation_index;
    amf_u32 object_id;
    amf_u32 constellation_id;
    amf_vec3 delta;
    amf_vec3 rotate;
    amf_u32 flags;
} amf_instance;

typedef struct amf_constellation_s {
    amf_u32 id;
    size_t instance_first;
    size_t instance_count;
    size_t metadata_first;
    size_t metadata_count;
    amf_u32 flags;
} amf_constellation;

typedef struct amf_counts_s {
    size_t objects;
    size_t materials;
    size_t textures;
    size_t constellations;
    size_t instances;
    size_t vertices;
    size_t volumes;
    size_t triangles;
    size_t metadata;
    size_t composites;
} amf_counts;

typedef struct amf_document_s {
    amf_unit unit;
    amf_slice unit_text;
    amf_slice version_text;

    amf_object *objects;
    size_t object_count;
    size_t object_cap;

    amf_material *materials;
    size_t material_count;
    size_t material_cap;

    amf_texture *textures;
    size_t texture_count;
    size_t texture_cap;

    amf_constellation *constellations;
    size_t constellation_count;
    size_t constellation_cap;

    amf_instance *instances;
    size_t instance_count;
    size_t instance_cap;

    amf_vertex *vertices;
    size_t vertex_count;
    size_t vertex_cap;

    amf_volume *volumes;
    size_t volume_count;
    size_t volume_cap;

    amf_triangle *triangles;
    size_t triangle_count;
    size_t triangle_cap;

    amf_metadata *metadata;
    size_t metadata_count;
    size_t metadata_cap;

    amf_composite *composites;
    size_t composite_count;
    size_t composite_cap;

    size_t document_metadata_first;
    size_t document_metadata_count;
    amf_u32 flags;
} amf_document;

typedef int (*amf_inflate_raw_deflate_fn)(void *user,
    const amf_u8 *src, size_t src_len,
    amf_u8 *dst, size_t dst_cap, size_t *dst_len);

typedef struct amf_options_s {
    amf_u32 flags;
    amf_u8 *zip_scratch;
    size_t zip_scratch_size;
    amf_inflate_raw_deflate_fn inflate_raw_deflate;
    void *inflate_user;
} amf_options;

typedef struct amf_zip_entry_s {
    amf_slice name;
    const amf_u8 *data;
    size_t compressed_size;
    size_t uncompressed_size;
    amf_u32 method;
} amf_zip_entry;

#define AMF_ALL_OBJECTS ((size_t)-1)
#define AMF_MAX_SUBDIV_LEVEL 8u

#define AMF_VALIDATE_BASIC       0x0001u
#define AMF_VALIDATE_REFERENCES  0x0002u
#define AMF_VALIDATE_DEGENERATE  0x0004u
#define AMF_VALIDATE_DUPLICATES  0x0008u
#define AMF_VALIDATE_MANIFOLD    0x0010u
#define AMF_VALIDATE_ALL         0x001fu

#define AMF_REPORT_MISSING_MATERIAL 0x00000001u
#define AMF_REPORT_MISSING_OBJECT   0x00000002u
#define AMF_REPORT_DUPLICATE_ID     0x00000004u
#define AMF_REPORT_DEGENERATE_TRI   0x00000008u
#define AMF_REPORT_DUPLICATE_TRI    0x00000010u
#define AMF_REPORT_BOUNDARY_EDGE    0x00000020u
#define AMF_REPORT_NONMANIFOLD_EDGE 0x00000040u

typedef struct amf_stats_s {
    amf_vec3 bbox_min;
    amf_vec3 bbox_max;
    amf_fx surface_area;
    amf_fx signed_volume;
    size_t object_count;
    size_t vertex_count;
    size_t triangle_count;
    size_t degenerate_triangle_count;
} amf_stats;

typedef struct amf_topology_report_s {
    size_t checked_object_count;
    size_t checked_triangle_count;
    size_t edge_count;
    size_t boundary_edge_count;
    size_t nonmanifold_edge_count;
    size_t degenerate_triangle_count;
    size_t duplicate_triangle_count;
    size_t duplicate_id_count;
    size_t missing_material_ref_count;
    size_t missing_object_ref_count;
    amf_u32 flags;
} amf_topology_report;

typedef struct amf_flat_triangle_s {
    amf_vec3 p1;
    amf_vec3 p2;
    amf_vec3 p3;
} amf_flat_triangle;

void amf_arena_init(amf_arena *arena, void *memory, size_t size);
void amf_arena_reset(amf_arena *arena);
void *amf_arena_alloc(amf_arena *arena, size_t size, size_t align);
void *amf_arena_calloc(amf_arena *arena, size_t count, size_t size, size_t align);
amf_slice amf_arena_copy_slice(amf_arena *arena, const char *ptr, size_t len);

void amf_diag_clear(amf_diag *diag);
void amf_diag_set(amf_diag *diag, int code, size_t line, size_t col, const char *msg);
const char *amf_error_name(int code);

amf_fx amf_fx_from_i32(amf_i32 v);
amf_i32 amf_fx_to_i32(amf_fx v);
int amf_fx_parse(amf_slice s, amf_fx *out);
int amf_u32_parse(amf_slice s, amf_u32 *out);
amf_fx amf_fx_mul(amf_fx a, amf_fx b);
amf_fx amf_fx_div(amf_fx a, amf_fx b);
int amf_fx_to_text(amf_fx v, char *dst, size_t cap, size_t *out_len);

amf_unit amf_unit_from_slice(amf_slice s);
const char *amf_unit_name(amf_unit unit);
amf_fx amf_unit_to_mm_factor(amf_unit unit);

void amf_document_zero(amf_document *doc);
int amf_count_xml(const char *xml, size_t len, amf_counts *counts, amf_diag *diag);
int amf_parse_xml(amf_document *doc, amf_arena *arena, const char *xml, size_t len, const amf_options *opts, amf_diag *diag);
int amf_parse_memory(amf_document *doc, amf_arena *arena, const amf_u8 *data, size_t len, const amf_options *opts, amf_diag *diag);
int amf_validate(const amf_document *doc, amf_diag *diag);

int amf_write_xml(const amf_document *doc, char *dst, size_t cap, size_t *out_len);
int amf_base64_decode(const char *src, size_t src_len, amf_u8 *dst, size_t dst_cap, size_t *out_len);
int amf_zip_find_first_xml(const amf_u8 *zip, size_t len, amf_zip_entry *out_entry, amf_diag *diag);

size_t amf_find_object_by_id(const amf_document *doc, amf_u32 id);
size_t amf_find_material_by_id(const amf_document *doc, amf_u32 id);
size_t amf_find_texture_by_id(const amf_document *doc, amf_u32 id);
size_t amf_find_constellation_by_id(const amf_document *doc, amf_u32 id);

amf_fx amf_fx_abs(amf_fx v);
amf_fx amf_fx_sqrt(amf_fx v);
amf_vec3 amf_vec3_add(amf_vec3 a, amf_vec3 b);
amf_vec3 amf_vec3_sub(amf_vec3 a, amf_vec3 b);
amf_vec3 amf_vec3_scale(amf_vec3 v, amf_fx s);
amf_fx amf_vec3_dot(amf_vec3 a, amf_vec3 b);
amf_vec3 amf_vec3_cross(amf_vec3 a, amf_vec3 b);
amf_vec3 amf_vec3_normalize(amf_vec3 v);

int amf_recompute_bboxes(amf_document *doc);
int amf_compute_stats(const amf_document *doc, size_t object_index, amf_stats *out_stats);
int amf_validate_deep(const amf_document *doc, amf_arena *temp_arena, amf_u32 flags, amf_topology_report *out_report, amf_diag *diag);

size_t amf_flatten_triangle_count(unsigned int level);
int amf_triangle_subdivide_linear(const amf_document *doc, size_t triangle_index, unsigned int level, amf_flat_triangle *out_tris, size_t out_cap, size_t *out_count);
int amf_triangle_subdivide_curved(const amf_document *doc, size_t triangle_index, unsigned int level, amf_flat_triangle *out_tris, size_t out_cap, size_t *out_count);

int amf_formula_eval(amf_slice formula, amf_fx x, amf_fx y, amf_fx z, amf_fx *out_value, amf_diag *diag);

int amf_write_obj(const amf_document *doc, char *dst, size_t cap, size_t *out_len);
int amf_write_stl_ascii(const amf_document *doc, const char *solid_name, char *dst, size_t cap, size_t *out_len);
amf_u32 amf_crc32(const amf_u8 *data, size_t len);
int amf_write_zip_stored_xml(const char *entry_name, const amf_u8 *xml, size_t xml_len, amf_u8 *dst, size_t cap, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif
