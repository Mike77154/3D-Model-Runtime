#ifndef LWS3D_LWO_H
#define LWS3D_LWO_H

/*
   lws3d_lwo - static LightWave Object (.lwo) geometry reader.
   C89, no malloc/free/realloc, no float/double in parser state.
   Supports FORM LWO2 geometry and a practical FORM LWOB fallback.
*/

#include "lws3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LWS3D_LWO_MAX_LAYERS
#define LWS3D_LWO_MAX_LAYERS       32
#endif
#ifndef LWS3D_LWO_MAX_POINTS
#define LWS3D_LWO_MAX_POINTS     8192
#endif
#ifndef LWS3D_LWO_MAX_POLYGONS
#define LWS3D_LWO_MAX_POLYGONS   4096
#endif
#ifndef LWS3D_LWO_MAX_POLY_VERTS
#define LWS3D_LWO_MAX_POLY_VERTS   16
#endif
#ifndef LWS3D_LWO_MAX_TRIANGLES
#define LWS3D_LWO_MAX_TRIANGLES  8192
#endif
#ifndef LWS3D_LWO_MAX_TAGS
#define LWS3D_LWO_MAX_TAGS        256
#endif
#ifndef LWS3D_LWO_MAX_SURFACES
#define LWS3D_LWO_MAX_SURFACES    128
#endif
#ifndef LWS3D_LWO_MAX_VMAPS
#define LWS3D_LWO_MAX_VMAPS        64
#endif
#ifndef LWS3D_LWO_MAX_VMAP_VALUES
#define LWS3D_LWO_MAX_VMAP_VALUES 8192
#endif
#ifndef LWS3D_LWO_MAX_CLIPS
#define LWS3D_LWO_MAX_CLIPS        64
#endif
#ifndef LWS3D_LWO_MAX_UNKNOWN
#define LWS3D_LWO_MAX_UNKNOWN      96
#endif
#ifndef LWS3D_LWO_MAX_BLOCKS
#define LWS3D_LWO_MAX_BLOCKS      256
#endif
#ifndef LWS3D_LWO_MAX_NODAL_BLOBS
#define LWS3D_LWO_MAX_NODAL_BLOBS 128
#endif
#ifndef LWS3D_LWO_MAX_NAME
#define LWS3D_LWO_MAX_NAME         96
#endif
#ifndef LWS3D_LWO_MAX_PATH
#define LWS3D_LWO_MAX_PATH        160
#endif

#define LWS3D_LWO_OK                 0
#define LWS3D_LWO_ERR_BAD_ARG       -1
#define LWS3D_LWO_ERR_NOT_IFF       -2
#define LWS3D_LWO_ERR_UNSUPPORTED   -3
#define LWS3D_LWO_ERR_TRUNCATED     -4
#define LWS3D_LWO_ERR_OVERFLOW      -5
#define LWS3D_LWO_ERR_CORRUPT       -6

#define LWS3D_LWO_FORMAT_UNKNOWN     0
#define LWS3D_LWO_FORMAT_LWO2        2
#define LWS3D_LWO_FORMAT_LWOB        5
#define LWS3D_LWO_FORMAT_LWO3        3

#define LWS3D_LWO_POLY_FACE          1
#define LWS3D_LWO_POLY_CURV          2
#define LWS3D_LWO_POLY_PTCH          3
#define LWS3D_LWO_POLY_MBAL          4
#define LWS3D_LWO_POLY_BONE          5
#define LWS3D_LWO_POLY_OTHER         6

#define LWS3D_LWO_MAP_UNKNOWN        0
#define LWS3D_LWO_MAP_TXUV           1
#define LWS3D_LWO_MAP_WGHT           2
#define LWS3D_LWO_MAP_RGB            3
#define LWS3D_LWO_MAP_RGBA           4
#define LWS3D_LWO_MAP_MORF           5
#define LWS3D_LWO_MAP_SPOT           6
#define LWS3D_LWO_MAP_NORM           7
#define LWS3D_LWO_MAP_MNVW           8
#define LWS3D_LWO_MAP_MBAL           9

#define LWS3D_LWO_BLOCK_UNKNOWN      0
#define LWS3D_LWO_BLOCK_IMAP         1
#define LWS3D_LWO_BLOCK_PROC         2
#define LWS3D_LWO_BLOCK_GRAD         3
#define LWS3D_LWO_BLOCK_SHDR         4

#define LWS3D_LWO_SURFACE_NONE      -1

typedef struct LWS3D_LWO_Layer_tag {
    int used;
    int number;
    int flags;
    int hidden;
    int parent;
    char name[LWS3D_LWO_MAX_NAME];
    LWS3D_Vec3 pivot;
    long first_point;
    long point_count;
    long first_polygon;
    long polygon_count;
} LWS3D_LWO_Layer;

typedef struct LWS3D_LWO_Point_tag {
    LWS3D_Fixed x;
    LWS3D_Fixed y;
    LWS3D_Fixed z;
    int layer;
} LWS3D_LWO_Point;

typedef struct LWS3D_LWO_Polygon_tag {
    int type;
    char type_id[5];
    int layer;
    int vertex_count;
    int stored_vertex_count;
    int flags;
    int vertex_truncated;
    unsigned long vertices[LWS3D_LWO_MAX_POLY_VERTS];
    int surface_tag;
    int surface_index;
    int part_tag;
    int smoothing_tag;
} LWS3D_LWO_Polygon;

typedef struct LWS3D_LWO_Triangle_tag {
    unsigned long polygon;
    unsigned long v0;
    unsigned long v1;
    unsigned long v2;
    int surface_index;
} LWS3D_LWO_Triangle;

typedef struct LWS3D_LWO_Tag_tag {
    char text[LWS3D_LWO_MAX_NAME];
} LWS3D_LWO_Tag;

typedef struct LWS3D_LWO_Surface_tag {
    char name[LWS3D_LWO_MAX_NAME];
    char source[LWS3D_LWO_MAX_NAME];
    LWS3D_Vec3 color;
    LWS3D_Fixed diffuse;
    LWS3D_Fixed luminosity;
    LWS3D_Fixed specular;
    LWS3D_Fixed glossiness;
    LWS3D_Fixed reflection;
    LWS3D_Fixed transparency;
    LWS3D_Fixed smoothing_angle;
    int sidedness;
    int has_color;
} LWS3D_LWO_Surface;

typedef struct LWS3D_LWO_VMap_tag {
    char type_id[5];
    int type;
    int dimension;
    int discontinuous;
    char name[LWS3D_LWO_MAX_NAME];
    long first_value;
    long value_count;
} LWS3D_LWO_VMap;

/* For VMAP: polygon is -1. For VMAD: polygon stores the polygon index. */
typedef struct LWS3D_LWO_VMapValue_tag {
    long map_index;
    unsigned long point;
    long polygon;
    LWS3D_Fixed value[4];
} LWS3D_LWO_VMapValue;

typedef struct LWS3D_LWO_Clip_tag {
    unsigned long index;
    char still_path[LWS3D_LWO_MAX_PATH];
} LWS3D_LWO_Clip;

typedef struct LWS3D_LWO_SurfaceBlock_tag {
    int surface_index;
    int block_type;
    char block_id[5];
    char channel[5];
    char ordinal[LWS3D_LWO_MAX_NAME];
    char algorithm[LWS3D_LWO_MAX_NAME];
    char vmap_name[LWS3D_LWO_MAX_NAME];
    int enabled;
    int opacity_type;
    LWS3D_Fixed opacity;
    int projection;
    int axis;
    unsigned long image_clip;
    unsigned long opaque_data_size;
    LWS3D_Vec3 center;
    LWS3D_Vec3 size;
    LWS3D_Vec3 rotation;
} LWS3D_LWO_SurfaceBlock;

typedef struct LWS3D_LWO_NodalBlob_tag {
    int surface_index;
    char id[5];
    unsigned long size;
    unsigned long offset;
    int depth;
} LWS3D_LWO_NodalBlob;

typedef struct LWS3D_LWO_Unknown_tag {
    char id[5];
    unsigned long size;
} LWS3D_LWO_Unknown;

typedef struct LWS3D_LWO_Stats_tag {
    long chunks_seen;
    long subchunks_seen;
    long unknown_chunks;
    long unknown_subchunks;
    long truncated_chunks;
    long overflow_events;
    long bad_indices;
    long polygons_triangulated;
    long surface_blocks;
    long procedural_blocks;
    long shader_blocks;
    long nodal_blobs;
    long lwo3_forms;
} LWS3D_LWO_Stats;

typedef struct LWS3D_LWO_Object_tag {
    int format;
    unsigned long form_size;
    int overflowed;
    int truncated;
    int corrupt;

    long layer_count;
    long point_count;
    long polygon_count;
    long triangle_count;
    long tag_count;
    long surface_count;
    long vmap_count;
    long vmap_value_count;
    long clip_count;
    long unknown_count;
    long block_count;
    long nodal_blob_count;

    long last_pols_first;
    long last_pols_count;

    LWS3D_Vec3 bbox_min;
    LWS3D_Vec3 bbox_max;
    int has_bbox;

    LWS3D_LWO_Layer layers[LWS3D_LWO_MAX_LAYERS];
    LWS3D_LWO_Point points[LWS3D_LWO_MAX_POINTS];
    LWS3D_LWO_Polygon polygons[LWS3D_LWO_MAX_POLYGONS];
    LWS3D_LWO_Triangle triangles[LWS3D_LWO_MAX_TRIANGLES];
    LWS3D_LWO_Tag tags[LWS3D_LWO_MAX_TAGS];
    LWS3D_LWO_Surface surfaces[LWS3D_LWO_MAX_SURFACES];
    LWS3D_LWO_VMap vmaps[LWS3D_LWO_MAX_VMAPS];
    LWS3D_LWO_VMapValue vmap_values[LWS3D_LWO_MAX_VMAP_VALUES];
    LWS3D_LWO_Clip clips[LWS3D_LWO_MAX_CLIPS];
    LWS3D_LWO_SurfaceBlock blocks[LWS3D_LWO_MAX_BLOCKS];
    LWS3D_LWO_NodalBlob nodal_blobs[LWS3D_LWO_MAX_NODAL_BLOBS];
    LWS3D_LWO_Unknown unknowns[LWS3D_LWO_MAX_UNKNOWN];
} LWS3D_LWO_Object;

void lws3d_lwo_init(LWS3D_LWO_Object *obj);
int lws3d_lwo_parse_buffer(LWS3D_LWO_Object *obj, const unsigned char *data, unsigned long size, LWS3D_LWO_Stats *stats);
int lws3d_lwo_build_triangles(LWS3D_LWO_Object *obj, LWS3D_LWO_Stats *stats);
int lws3d_lwo_resolve_surfaces(LWS3D_LWO_Object *obj);

LWS3D_Fixed lws3d_lwo_fixed_from_f4be(const unsigned char b[4]);
const char *lws3d_lwo_error_name(int err);
const char *lws3d_lwo_format_name(int format);
const char *lws3d_lwo_polygon_type_name(int type);
const char *lws3d_lwo_block_type_name(int type);
const LWS3D_LWO_Surface *lws3d_lwo_polygon_surface(const LWS3D_LWO_Object *obj, const LWS3D_LWO_Polygon *poly);
const LWS3D_LWO_Tag *lws3d_lwo_tag_at(const LWS3D_LWO_Object *obj, int tag_index);

#ifndef LWS3D_NO_STDIO
int lws3d_lwo_load_file_static(LWS3D_LWO_Object *obj, const char *filename, unsigned char *scratch, unsigned long scratch_size, LWS3D_LWO_Stats *stats);
#endif

#ifdef __cplusplus
}
#endif

#endif
