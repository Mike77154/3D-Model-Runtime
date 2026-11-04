#ifndef LWO3D_H
#define LWO3D_H

/*
   lwo3d.h - static-storage LightWave Object decoder.
   Goals: C89, static core storage, no allocator calls, no real-number C types.
*/

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#if UINT_MAX == 0xffffffffU
typedef unsigned int lwo3d_u32;
typedef int lwo3d_i32;
#elif ULONG_MAX == 0xffffffffUL
typedef unsigned long lwo3d_u32;
typedef long lwo3d_i32;
#else
#error "lwo3d requires a 32-bit unsigned int or unsigned long"
#endif

#if USHRT_MAX == 0xffffU
typedef unsigned short lwo3d_u16;
typedef short lwo3d_i16;
#else
#error "lwo3d requires a 16-bit unsigned short"
#endif

typedef unsigned char lwo3d_u8;
typedef signed char lwo3d_i8;
typedef lwo3d_i32 lwo3d_fixed;

#ifndef LWO3D_MAX_POINTS
#define LWO3D_MAX_POINTS 16384U
#endif
#ifndef LWO3D_MAX_POLYGONS
#define LWO3D_MAX_POLYGONS 8192U
#endif
#ifndef LWO3D_MAX_POLY_INDICES
#define LWO3D_MAX_POLY_INDICES 65536U
#endif
#ifndef LWO3D_MAX_LAYERS
#define LWO3D_MAX_LAYERS 128U
#endif
#ifndef LWO3D_MAX_TAGS
#define LWO3D_MAX_TAGS 512U
#endif
#ifndef LWO3D_MAX_SURFACES
#define LWO3D_MAX_SURFACES 512U
#endif
#ifndef LWO3D_MAX_MAPS
#define LWO3D_MAX_MAPS 64U
#endif
#ifndef LWO3D_MAX_VMAP_ENTRIES
#define LWO3D_MAX_VMAP_ENTRIES 65536U
#endif
#ifndef LWO3D_MAX_VMAD_ENTRIES
#define LWO3D_MAX_VMAD_ENTRIES 65536U
#endif
#ifndef LWO3D_MAX_VMED_ENTRIES
#define LWO3D_MAX_VMED_ENTRIES 32768U
#endif
#ifndef LWO3D_MAX_CLIPS
#define LWO3D_MAX_CLIPS 128U
#endif
#ifndef LWO3D_MAX_SURFACE_BLOCKS
#define LWO3D_MAX_SURFACE_BLOCKS 256U
#endif
#ifndef LWO3D_MAX_CHUNK_LOG
#define LWO3D_MAX_CHUNK_LOG 512U
#endif
#ifndef LWO3D_NAME_MAX
#define LWO3D_NAME_MAX 64U
#endif
#ifndef LWO3D_PATH_MAX
#define LWO3D_PATH_MAX 192U
#endif
#ifndef LWO3D_MAX_FORM_DEPTH
#define LWO3D_MAX_FORM_DEPTH 16U
#endif

#define LWO3D_FIXED_SHIFT 16
#define LWO3D_FIXED_ONE ((lwo3d_fixed)65536L)
#define LWO3D_INDEX_NONE ((lwo3d_u16)0xffffU)
#define LWO3D_U32_NONE ((lwo3d_u32)0xffffffffUL)

#define LWO3D_OK 0
#define LWO3D_ERR_ARG -1
#define LWO3D_ERR_SHORT -2
#define LWO3D_ERR_FORM -3
#define LWO3D_ERR_UNSUPPORTED -4
#define LWO3D_ERR_CORRUPT -5
#define LWO3D_ERR_DEPTH -6

#define LWO3D_WARN_LIMITS 1U
#define LWO3D_WARN_UNKNOWN_CHUNKS 2U
#define LWO3D_WARN_TRUNCATED_STRINGS 4U
#define LWO3D_WARN_UNSUPPORTED_CHUNKS 8U
#define LWO3D_WARN_RECOVERED_SIZE 16U
#define LWO3D_WARN_NESTED_FORMS 32U
#define LWO3D_WARN_LWO3_PARTIAL 64U

#define LWO3D_FOURCC(a,b,c,d) \
    ((((lwo3d_u32)(lwo3d_u8)(a)) << 24) | \
     (((lwo3d_u32)(lwo3d_u8)(b)) << 16) | \
     (((lwo3d_u32)(lwo3d_u8)(c)) << 8) | \
     ((lwo3d_u32)(lwo3d_u8)(d)))

#define LWO3D_ID_LWO2 LWO3D_FOURCC('L','W','O','2')
#define LWO3D_ID_LWO3 LWO3D_FOURCC('L','W','O','3')
#define LWO3D_ID_LWOB LWO3D_FOURCC('L','W','O','B')
#define LWO3D_ID_FACE LWO3D_FOURCC('F','A','C','E')
#define LWO3D_ID_SUBD LWO3D_FOURCC('S','U','B','D')
#define LWO3D_ID_PSUB LWO3D_FOURCC('P','S','U','B')
#define LWO3D_ID_PTCH LWO3D_FOURCC('P','T','C','H')
#define LWO3D_ID_SPCH LWO3D_FOURCC('S','P','C','H')
#define LWO3D_ID_CURV LWO3D_FOURCC('C','U','R','V')
#define LWO3D_ID_LINE LWO3D_FOURCC('L','I','N','E')
#define LWO3D_ID_BEZR LWO3D_FOURCC('B','E','Z','R')
#define LWO3D_ID_HCRV LWO3D_FOURCC('H','C','R','V')
#define LWO3D_ID_BCRV LWO3D_FOURCC('B','C','R','V')
#define LWO3D_ID_BSPL LWO3D_FOURCC('B','S','P','L')
#define LWO3D_ID_OPNT LWO3D_FOURCC('O','P','N','T')
#define LWO3D_ID_TXUV LWO3D_FOURCC('T','X','U','V')
#define LWO3D_ID_WGHT LWO3D_FOURCC('W','G','H','T')
#define LWO3D_ID_MORF LWO3D_FOURCC('M','O','R','F')
#define LWO3D_ID_SPOT LWO3D_FOURCC('S','P','O','T')
#define LWO3D_ID_RGB  LWO3D_FOURCC('R','G','B',' ')
#define LWO3D_ID_RGBA LWO3D_FOURCC('R','G','B','A')

typedef struct lwo3d_vec2_s {
    lwo3d_fixed x;
    lwo3d_fixed y;
} lwo3d_vec2;

typedef struct lwo3d_vec3_s {
    lwo3d_fixed x;
    lwo3d_fixed y;
    lwo3d_fixed z;
} lwo3d_vec3;

typedef struct lwo3d_bbox_s {
    int valid;
    lwo3d_vec3 minv;
    lwo3d_vec3 maxv;
} lwo3d_bbox;

typedef struct lwo3d_layer_s {
    lwo3d_u16 number;
    lwo3d_u16 flags;
    lwo3d_vec3 pivot;
    lwo3d_u16 parent;
    lwo3d_u32 point_start;
    lwo3d_u32 point_count;
    lwo3d_u32 polygon_start;
    lwo3d_u32 polygon_count;
    char name[LWO3D_NAME_MAX];
} lwo3d_layer;

typedef struct lwo3d_point_s {
    lwo3d_vec3 p;
    lwo3d_u16 layer;
} lwo3d_point;

typedef struct lwo3d_polygon_s {
    lwo3d_u32 first_index;
    lwo3d_u16 index_count;
    lwo3d_u16 flags16;
    lwo3d_u32 flags32;
    lwo3d_u16 layer;
    lwo3d_u16 surface;
    lwo3d_u16 material;
    lwo3d_u16 part;
    lwo3d_u16 smgp;
    lwo3d_u32 type;
} lwo3d_polygon;

typedef struct lwo3d_map_s {
    lwo3d_u32 type;
    lwo3d_u16 dimension;
    char name[LWO3D_NAME_MAX];
} lwo3d_map;

typedef struct lwo3d_map_entry_s {
    lwo3d_u32 type;
    lwo3d_u16 dimension;
    lwo3d_u16 map_index;
    lwo3d_u32 point;
    lwo3d_fixed value[4];
} lwo3d_map_entry;

typedef struct lwo3d_mad_entry_s {
    lwo3d_u32 type;
    lwo3d_u16 dimension;
    lwo3d_u16 map_index;
    lwo3d_u32 point;
    lwo3d_u32 polygon;
    lwo3d_fixed value[4];
} lwo3d_mad_entry;

typedef struct lwo3d_med_entry_s {
    lwo3d_u32 type;
    lwo3d_u16 dimension;
    lwo3d_u16 map_index;
    lwo3d_u32 point_a;
    lwo3d_u32 point_b;
    lwo3d_fixed value[4];
} lwo3d_med_entry;

typedef struct lwo3d_surface_s {
    char name[LWO3D_NAME_MAX];
    char source[LWO3D_NAME_MAX];
    lwo3d_fixed color[3];
    lwo3d_fixed diffuse;
    lwo3d_fixed specular;
    lwo3d_fixed luminosity;
    lwo3d_fixed transparency;
    lwo3d_fixed glossiness;
    lwo3d_fixed smooth_angle;
} lwo3d_surface;

typedef struct lwo3d_surface_block_s {
    lwo3d_u16 surface;
    lwo3d_u32 block_type;
    lwo3d_u32 channel;
    lwo3d_u32 ordinal;
    lwo3d_u32 clip_index;
    lwo3d_fixed opacity;
    lwo3d_u16 enabled;
    char map_name[LWO3D_NAME_MAX];
} lwo3d_surface_block;

typedef struct lwo3d_clip_s {
    lwo3d_u32 index;
    char path[LWO3D_PATH_MAX];
    char server[LWO3D_NAME_MAX];
} lwo3d_clip;

typedef struct lwo3d_chunk_info_s {
    lwo3d_u32 id;
    lwo3d_u32 form_type;
    lwo3d_u32 offset;
    lwo3d_u32 size;
    lwo3d_u16 depth;
    lwo3d_u16 handled;
} lwo3d_chunk_info;

typedef struct lwo3d_triangle_s {
    lwo3d_u32 polygon;
    lwo3d_u32 point_index[3];
    lwo3d_vec3 p[3];
    lwo3d_vec3 raw_normal;
    lwo3d_vec2 uv[3];
    lwo3d_u16 has_uv[3];
    lwo3d_u16 surface;
    lwo3d_u16 material;
    lwo3d_u16 layer;
} lwo3d_triangle;

typedef void (*lwo3d_triangle_cb)(void *user, const lwo3d_triangle *tri);

typedef struct lwo3d_model_s {
    lwo3d_u32 form_type;
    lwo3d_u32 warnings;
    lwo3d_u32 unknown_chunks;
    lwo3d_u32 form_count;
    lwo3d_u32 max_depth_seen;
    lwo3d_bbox bbox;

    lwo3d_u32 layer_count;
    lwo3d_u32 point_count;
    lwo3d_u32 polygon_count;
    lwo3d_u32 poly_index_count;
    lwo3d_u32 tag_count;
    lwo3d_u32 surface_count;
    lwo3d_u32 surface_block_count;
    lwo3d_u32 map_count;
    lwo3d_u32 vmap_entry_count;
    lwo3d_u32 vmad_entry_count;
    lwo3d_u32 vmed_entry_count;
    lwo3d_u32 clip_count;
    lwo3d_u32 chunk_log_count;

    lwo3d_layer layers[LWO3D_MAX_LAYERS];
    lwo3d_point points[LWO3D_MAX_POINTS];
    lwo3d_polygon polygons[LWO3D_MAX_POLYGONS];
    lwo3d_u32 poly_indices[LWO3D_MAX_POLY_INDICES];
    char tags[LWO3D_MAX_TAGS][LWO3D_NAME_MAX];
    lwo3d_surface surfaces[LWO3D_MAX_SURFACES];
    lwo3d_surface_block surface_blocks[LWO3D_MAX_SURFACE_BLOCKS];
    lwo3d_map maps[LWO3D_MAX_MAPS];
    lwo3d_map_entry vmap_entries[LWO3D_MAX_VMAP_ENTRIES];
    lwo3d_mad_entry vmad_entries[LWO3D_MAX_VMAD_ENTRIES];
    lwo3d_med_entry vmed_entries[LWO3D_MAX_VMED_ENTRIES];
    lwo3d_clip clips[LWO3D_MAX_CLIPS];
    lwo3d_chunk_info chunk_log[LWO3D_MAX_CHUNK_LOG];
} lwo3d_model;

void lwo3d_init(lwo3d_model *model);
int lwo3d_decode_memory(const lwo3d_u8 *data, lwo3d_u32 size, lwo3d_model *model);
void lwo3d_emit_triangles(const lwo3d_model *model, lwo3d_triangle_cb cb, void *user);
int lwo3d_compute_polygon_raw_normal(const lwo3d_model *model, lwo3d_u32 polygon_index, lwo3d_vec3 *out_normal);
int lwo3d_find_vmap_value(const lwo3d_model *model, lwo3d_u32 type, const char *map_name, lwo3d_u32 point, lwo3d_fixed out_value[4], lwo3d_u16 *out_dimension);
int lwo3d_find_vmad_value(const lwo3d_model *model, lwo3d_u32 type, const char *map_name, lwo3d_u32 polygon, lwo3d_u32 point, lwo3d_fixed out_value[4], lwo3d_u16 *out_dimension);
int lwo3d_find_vmed_value(const lwo3d_model *model, lwo3d_u32 type, const char *map_name, lwo3d_u32 point_a, lwo3d_u32 point_b, lwo3d_fixed out_value[4], lwo3d_u16 *out_dimension);
int lwo3d_find_txuv(const lwo3d_model *model, const char *map_name, lwo3d_u32 polygon, lwo3d_u32 point, lwo3d_vec2 *out_uv);
void lwo3d_fourcc_text(lwo3d_u32 id, char out[5]);
const char *lwo3d_error_string(int code);

lwo3d_fixed lwo3d_fixed_from_ieee754_be(lwo3d_u32 bits);
long lwo3d_fixed_to_int(lwo3d_fixed v);
lwo3d_fixed lwo3d_fixed_mul_small(lwo3d_fixed a, lwo3d_fixed b);
lwo3d_fixed lwo3d_fixed_abs(lwo3d_fixed v);

#ifdef __cplusplus
}
#endif

#endif
