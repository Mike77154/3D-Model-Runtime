#ifndef GIF_PLY_CODEC_H
#define GIF_PLY_CODEC_H

/*
   gif_ply_codec.h
   C89 PLY importer with caller-owned memory and Q16.16 fixed point.

   Design rules:
   - no malloc/free/realloc inside the codec
   - no C floating point types used by the public API or implementation
   - import from caller-provided memory buffer, not FILE
   - ASCII, binary_little_endian and binary_big_endian PLY 1.0

   v0.2 additions:
   - header/preflight probe
   - optional edge import
   - optional per-triangle face metadata
   - optional per-triangle UV corner metadata from face texcoord lists
   - vertex intensity/confidence scalar capture
   - broader real-world aliases for colors, UVs and indices
*/

#ifdef __cplusplus
extern "C" {
#endif

#define GIF_PLY_VERSION_MAJOR 0
#define GIF_PLY_VERSION_MINOR 2
#define GIF_PLY_VERSION_PATCH 0

#define GIF_PLY_FX_SHIFT 16
#define GIF_PLY_FX_ONE   ((long)65536)

#define GIF_PLY_VERTEX_HAS_NORMAL     1u
#define GIF_PLY_VERTEX_HAS_COLOR      2u
#define GIF_PLY_VERTEX_HAS_UV         4u
#define GIF_PLY_VERTEX_HAS_INTENSITY  8u
#define GIF_PLY_VERTEX_HAS_CONFIDENCE 16u

#define GIF_PLY_TRI_HAS_FACE_COLOR    1u
#define GIF_PLY_TRI_HAS_FACE_UV       2u
#define GIF_PLY_TRI_FROM_NGON         4u

#define GIF_PLY_EDGE_HAS_CREASE       1u

#define GIF_PLY_OK 0
#define GIF_PLY_ERR_NULL             -1
#define GIF_PLY_ERR_BAD_MAGIC        -2
#define GIF_PLY_ERR_BAD_HEADER       -3
#define GIF_PLY_ERR_UNSUPPORTED      -4
#define GIF_PLY_ERR_CAPACITY         -5
#define GIF_PLY_ERR_TRUNCATED        -6
#define GIF_PLY_ERR_PARSE            -7
#define GIF_PLY_ERR_OVERFLOW         -8
#define GIF_PLY_ERR_FACE_TOO_LARGE   -9

#define GIF_PLY_FORMAT_ASCII 1
#define GIF_PLY_FORMAT_BINARY_LE 2
#define GIF_PLY_FORMAT_BINARY_BE 3

#ifndef GIF_PLY_MAX_ELEMENTS
#define GIF_PLY_MAX_ELEMENTS 24
#endif

#ifndef GIF_PLY_MAX_PROPERTIES
#define GIF_PLY_MAX_PROPERTIES 96
#endif

#ifndef GIF_PLY_NAME_MAX
#define GIF_PLY_NAME_MAX 32
#endif

#ifndef GIF_PLY_MAX_FACE_VERTS
#define GIF_PLY_MAX_FACE_VERTS 64
#endif

typedef long gif_ply_fx;

typedef struct GifPlyVertex {
    gif_ply_fx x;
    gif_ply_fx y;
    gif_ply_fx z;
    gif_ply_fx nx;
    gif_ply_fx ny;
    gif_ply_fx nz;
    gif_ply_fx u;
    gif_ply_fx v;
    gif_ply_fx intensity;
    gif_ply_fx confidence;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    unsigned char flags;
    unsigned char reserved0;
    unsigned char reserved1;
    unsigned char reserved2;
} GifPlyVertex;

typedef struct GifPlyEdge {
    unsigned long a;
    unsigned long b;
    gif_ply_fx crease;
    unsigned char flags;
    unsigned char reserved0;
    unsigned char reserved1;
    unsigned char reserved2;
} GifPlyEdge;

typedef struct GifPlyTriangle {
    unsigned long source_face;
    unsigned char source_vertex_count;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    unsigned char flags;
    unsigned char material_index;
    unsigned char reserved0;
    unsigned char reserved1;
} GifPlyTriangle;

typedef struct GifPlyTriUV {
    gif_ply_fx u0;
    gif_ply_fx v0;
    gif_ply_fx u1;
    gif_ply_fx v1;
    gif_ply_fx u2;
    gif_ply_fx v2;
} GifPlyTriUV;

typedef struct GifPlyMesh {
    GifPlyVertex *vertices;
    unsigned long vertex_capacity;
    unsigned long vertex_count;

    unsigned long *indices;
    unsigned long index_capacity;
    unsigned long index_count;

    /* Optional. Leave NULL/capacity 0 if not needed. */
    GifPlyEdge *edges;
    unsigned long edge_capacity;
    unsigned long edge_count;

    /* Optional. One record per emitted triangle. */
    GifPlyTriangle *triangles;
    unsigned long triangle_capacity;
    unsigned long triangle_count;

    /* Optional. One record per emitted triangle only when face texcoord exists. */
    GifPlyTriUV *tri_uvs;
    unsigned long tri_uv_capacity;
    unsigned long tri_uv_count;
} GifPlyMesh;

typedef struct GifPlyInfo {
    int format;
    unsigned long declared_vertices;
    unsigned long declared_faces;
    unsigned long declared_edges;
    unsigned long declared_elements;
    unsigned long declared_properties;
    unsigned long skipped_elements;
    unsigned long skipped_properties;
    unsigned long skipped_edges;
    unsigned long emitted_triangles;
    unsigned long comments;
    unsigned long obj_info_lines;
    unsigned long texture_file_comments;
    int saw_vertex;
    int saw_face;
    int saw_edge;
} GifPlyInfo;

int gif_ply_probe_memory(const unsigned char *data,
                         unsigned long size,
                         GifPlyInfo *out_info);

int gif_ply_import_memory(const unsigned char *data,
                          unsigned long size,
                          GifPlyMesh *mesh,
                          GifPlyInfo *out_info);

const char *gif_ply_error_string(int code);

gif_ply_fx gif_ply_fx_from_int(long v);
long gif_ply_fx_to_int(gif_ply_fx v);
long gif_ply_fx_to_milli(gif_ply_fx v);

#ifdef __cplusplus
}
#endif

#endif
