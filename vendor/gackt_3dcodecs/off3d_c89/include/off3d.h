#ifndef OFF3D_H_INCLUDED
#define OFF3D_H_INCLUDED

/*
   off3d.h - small OFF/COFF/NOFF/CNOFF ASCII mesh decoder.
   C89, caller-owned memory, no heap use, fixed-point numeric parsing.
*/

#ifdef __cplusplus
extern "C" {
#endif

#define OFF3D_VERSION_MAJOR 1
#define OFF3D_VERSION_MINOR 0
#define OFF3D_VERSION_PATCH 0

#define OFF3D_Q16_SHIFT 16
#define OFF3D_Q16_ONE   ((long)65536L)
#define OFF3D_Q16_HALF  ((long)32768L)

typedef long off3d_q16;
typedef long off3d_i32;
typedef unsigned long off3d_u32;

enum off3d_status {
    OFF3D_OK = 0,
    OFF3D_ERR_ARGUMENT = -1,
    OFF3D_ERR_EMPTY = -2,
    OFF3D_ERR_BAD_MAGIC = -3,
    OFF3D_ERR_UNSUPPORTED = -4,
    OFF3D_ERR_BAD_NUMBER = -5,
    OFF3D_ERR_CAPACITY = -6,
    OFF3D_ERR_BAD_INDEX = -7,
    OFF3D_ERR_UNEXPECTED_EOF = -8,
    OFF3D_ERR_RANGE = -9
};

enum off3d_file_flags {
    OFF3D_FILE_HAS_COLOR    = 1UL,
    OFF3D_FILE_HAS_NORMAL   = 2UL,
    OFF3D_FILE_HAS_TEXCOORD = 4UL,
    OFF3D_FILE_HEADER_C     = 8UL,
    OFF3D_FILE_HEADER_N     = 16UL,
    OFF3D_FILE_HEADER_ST    = 32UL
};

enum off3d_element_flags {
    OFF3D_VERTEX_HAS_COLOR     = 1,
    OFF3D_VERTEX_HAS_NORMAL    = 2,
    OFF3D_VERTEX_HAS_TEXCOORD  = 4,
    OFF3D_FACE_HAS_COLOR       = 1,
    OFF3D_FACE_HAS_CMAP_INDEX  = 2
};

enum off3d_warning_flags {
    OFF3D_WARN_IGNORED_VERTEX_EXTRA = 1UL,
    OFF3D_WARN_IGNORED_FACE_EXTRA   = 2UL,
    OFF3D_WARN_DEGENERATE_FACE      = 4UL,
    OFF3D_WARN_MISSING_OPTIONAL     = 8UL,
    OFF3D_WARN_COLOR_AMBIGUOUS      = 16UL,
    OFF3D_WARN_UNKNOWN_HEADER_FLAG  = 32UL
};

typedef struct off3d_vertex_s {
    off3d_q16 x;
    off3d_q16 y;
    off3d_q16 z;
    off3d_q16 nx;
    off3d_q16 ny;
    off3d_q16 nz;
    off3d_q16 u;
    off3d_q16 v;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    unsigned char flags;
} off3d_vertex;

typedef struct off3d_face_s {
    off3d_u32 first_index;
    off3d_u32 index_count;
    off3d_u32 color_index;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    unsigned char flags;
} off3d_face;

typedef struct off3d_mesh_s {
    off3d_vertex *vertices;
    off3d_u32 vertex_capacity;
    off3d_face *faces;
    off3d_u32 face_capacity;
    off3d_u32 *indices;
    off3d_u32 index_capacity;

    off3d_u32 vertex_count;
    off3d_u32 face_count;
    off3d_u32 edge_count;
    off3d_u32 index_count;
    off3d_u32 triangle_count;
    unsigned long file_flags;
    unsigned long warnings;
    unsigned long error_line;
} off3d_mesh;

typedef struct off3d_options_s {
    int strict;
    int allow_lowercase_magic;
    int allow_off_on_counts_line;
} off3d_options;

void off3d_mesh_init(off3d_mesh *mesh,
                     off3d_vertex *vertices,
                     off3d_u32 vertex_capacity,
                     off3d_face *faces,
                     off3d_u32 face_capacity,
                     off3d_u32 *indices,
                     off3d_u32 index_capacity);

void off3d_options_default(off3d_options *options);

int off3d_decode_memory(const char *data,
                        off3d_u32 size,
                        off3d_mesh *mesh,
                        const off3d_options *options);

int off3d_triangulate_fan(const off3d_mesh *mesh,
                          off3d_u32 *triangles,
                          off3d_u32 triangle_index_capacity,
                          off3d_u32 *triangle_count_out);

const char *off3d_error_string(int code);

long off3d_q16_to_i32_floor(off3d_q16 v);
long off3d_q16_frac_1000(off3d_q16 v);

#ifdef __cplusplus
}
#endif

#endif
