#ifndef HMB3D_H
#define HMB3D_H

/*
   hmb3d - small no-heap HMB/Caligari-style 3D chunk decoder.
   C89, no malloc/free/realloc, no real-number arithmetic types.

   Public domain / CC0 style drop-in. See README.md.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HMB3D_MAX_VERTICES
#define HMB3D_MAX_VERTICES 16384U
#endif
#ifndef HMB3D_MAX_UVS
#define HMB3D_MAX_UVS 16384U
#endif
#ifndef HMB3D_MAX_FACES
#define HMB3D_MAX_FACES 16384U
#endif
#ifndef HMB3D_MAX_CORNERS
#define HMB3D_MAX_CORNERS 65536U
#endif
#ifndef HMB3D_MAX_OBJECTS
#define HMB3D_MAX_OBJECTS 256U
#endif
#ifndef HMB3D_MAX_CHUNKS
#define HMB3D_MAX_CHUNKS 512U
#endif
#ifndef HMB3D_NAME_BYTES
#define HMB3D_NAME_BYTES 64U
#endif

#define HMB3D_FIXED_SHIFT 16
#define HMB3D_FIXED_ONE 65536L
#define HMB3D_FIXED_MAX 2147483647L
#define HMB3D_FIXED_MIN (-2147483647L - 1L)
#define HMB3D_INVALID_INDEX 4294967295UL

#define HMB3D_OK 0
#define HMB3D_ERR_ARGUMENT 1
#define HMB3D_ERR_TOO_SMALL 2
#define HMB3D_ERR_BAD_HEADER 3
#define HMB3D_ERR_TRUNCATED 4
#define HMB3D_ERR_CAPACITY 5
#define HMB3D_ERR_RANGE 6
#define HMB3D_ERR_UNSUPPORTED 7
#define HMB3D_ERR_NO_GEOMETRY 8

#define HMB3D_WARN_TRUNCATED_OUTPUT 0x0001UL
#define HMB3D_WARN_UNKNOWN_CHUNKS    0x0002UL
#define HMB3D_WARN_HEADERLESS_SCAN   0x0004UL
#define HMB3D_WARN_BIG_ENDIAN        0x0008UL
#define HMB3D_WARN_INDEX_RANGE       0x0010UL
#define HMB3D_WARN_REAL_SATURATED    0x0020UL
#define HMB3D_WARN_HOLE_FACE_SKIPPED 0x0040UL
#define HMB3D_WARN_ASSIMP_HMP       0x0080UL
#define HMB3D_WARN_HMP_SKINS_SKIPPED 0x0100UL
#define HMB3D_WARN_HMP4_PARTIAL     0x0200UL

#define HMB3D_FACE_HOLE     0x08U
#define HMB3D_FACE_BACKCULL 0x10U

typedef signed long hmb3d_i32;
typedef unsigned long hmb3d_u32;
typedef signed short hmb3d_i16;
typedef unsigned short hmb3d_u16;
typedef unsigned char hmb3d_u8;
typedef hmb3d_i32 hmb3d_fixed;

typedef struct hmb3d_vec3_tag {
    hmb3d_fixed x;
    hmb3d_fixed y;
    hmb3d_fixed z;
} hmb3d_vec3;

typedef struct hmb3d_uv_tag {
    hmb3d_fixed u;
    hmb3d_fixed v;
} hmb3d_uv;

typedef struct hmb3d_corner_tag {
    hmb3d_u32 vertex;
    hmb3d_u32 uv;
} hmb3d_corner;

typedef struct hmb3d_face_tag {
    hmb3d_u32 first_corner;
    hmb3d_u16 corner_count;
    hmb3d_i16 material_index;
    hmb3d_u8 flags;
    hmb3d_u16 object_index;
} hmb3d_face;

typedef struct hmb3d_object_tag {
    char name[HMB3D_NAME_BYTES];
    hmb3d_u32 source_chunk_offset;
    hmb3d_u32 source_chunk_id;
    hmb3d_i32 parent_chunk_id;
    hmb3d_u32 first_vertex;
    hmb3d_u32 vertex_count;
    hmb3d_u32 first_uv;
    hmb3d_u32 uv_count;
    hmb3d_u32 first_face;
    hmb3d_u32 face_count;
    hmb3d_fixed axes[12];      /* center, x axis, y axis, z axis */
    hmb3d_fixed transform[12]; /* 3 rows of a 4x4 transform; final row is implicit */
    hmb3d_fixed bbox_min[3];
    hmb3d_fixed bbox_max[3];
} hmb3d_object;

typedef struct hmb3d_chunk_tag {
    char type[5];
    hmb3d_i16 major_version;
    hmb3d_i16 minor_version;
    hmb3d_i32 chunk_id;
    hmb3d_i32 parent_id;
    hmb3d_u32 data_offset;
    hmb3d_u32 data_size;
} hmb3d_chunk;

typedef struct hmb3d_model_tag {
    hmb3d_vec3 vertices[HMB3D_MAX_VERTICES];
    hmb3d_uv uvs[HMB3D_MAX_UVS];
    hmb3d_face faces[HMB3D_MAX_FACES];
    hmb3d_corner corners[HMB3D_MAX_CORNERS];
    hmb3d_object objects[HMB3D_MAX_OBJECTS];
    hmb3d_chunk chunks[HMB3D_MAX_CHUNKS];
    hmb3d_u32 vertex_count;
    hmb3d_u32 uv_count;
    hmb3d_u32 face_count;
    hmb3d_u32 corner_count;
    hmb3d_u32 object_count;
    hmb3d_u32 chunk_count;
    hmb3d_fixed bbox_min[3];
    hmb3d_fixed bbox_max[3];
} hmb3d_model;

typedef struct hmb3d_diag_tag {
    int code;
    hmb3d_u32 warning_flags;
    hmb3d_u32 bytes_read;
    hmb3d_u32 chunks_seen;
    hmb3d_u32 chunks_unknown;
    hmb3d_u32 polh_seen;
    hmb3d_u32 hmp_seen;
    hmb3d_u32 faces_hole_skipped;
    hmb3d_u32 values_saturated;
    hmb3d_u32 clipped_vertices;
    hmb3d_u32 clipped_uvs;
    hmb3d_u32 clipped_faces;
    hmb3d_u32 clipped_corners;
    hmb3d_u32 bad_indices;
    char message[160];
} hmb3d_diag;

void hmb3d_model_init(hmb3d_model *model);
void hmb3d_diag_init(hmb3d_diag *diag);
const char *hmb3d_error_string(int code);

int hmb3d_load_memory(const hmb3d_u8 *data,
                      hmb3d_u32 size,
                      hmb3d_model *model,
                      hmb3d_diag *diag);

hmb3d_fixed hmb3d_real32_bits_to_fixed(hmb3d_u32 bits, hmb3d_diag *diag);
hmb3d_u32 hmb3d_read_u32_le(const hmb3d_u8 *p);
hmb3d_u32 hmb3d_read_u32_be(const hmb3d_u8 *p);
hmb3d_u16 hmb3d_read_u16_le(const hmb3d_u8 *p);
hmb3d_u16 hmb3d_read_u16_be(const hmb3d_u8 *p);

#ifdef __cplusplus
}
#endif

#endif
