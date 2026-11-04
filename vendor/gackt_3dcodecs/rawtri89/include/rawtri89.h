#ifndef RAWTRI89_H
#define RAWTRI89_H

/*
    rawtri89 - C89 arena-only RAW triangle mesh codec.

    RAW here follows the common Blender/Assimp/Rhino-style Raw Triangles
    convention: ASCII mesh data with one face per line. This library accepts
    9 fixed/decimal values for a triangle and, in compatibility mode, 12 values
    for a quad. Quads are triangulated to (0,1,2) and (0,2,3).

    This public header intentionally avoids malloc/free/realloc, stdint.h,
    float, and double. Coordinates are Q16.16 fixed point stored in signed long.
*/

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RW3D_VERSION_MAJOR 1
#define RW3D_VERSION_MINOR 1
#define RW3D_VERSION_PATCH 0

#define RW3D_FIX_SHIFT 16
#define RW3D_FIX_ONE 65536L
#define RW3D_FIX_HALF 32768L
#define RW3D_FIX_MAX 2147483647L
#define RW3D_FIX_MIN (-2147483647L - 1L)

/*
    Use this to create arena storage with conservative alignment:

        RW3D_ARENA_STORAGE(mem, 1024 * 1024);
        rw3d_arena_init(&arena, mem.data, sizeof(mem.data));
*/
#define RW3D_ARENA_STORAGE(name, bytes) \
    union { void *rw3d_align_ptr; long rw3d_align_long; unsigned char data[(bytes)]; } name

typedef signed char rw3d_i8;
typedef unsigned char rw3d_u8;
typedef signed short rw3d_i16;
typedef unsigned short rw3d_u16;
typedef signed long rw3d_i32;
typedef unsigned long rw3d_u32;
typedef rw3d_i32 rw3d_fix;

typedef enum rw3d_status_tag {
    RW3D_OK = 0,
    RW3D_ERR_ARG = 1,
    RW3D_ERR_ARENA = 2,
    RW3D_ERR_PARSE = 3,
    RW3D_ERR_OVERFLOW = 4,
    RW3D_ERR_CAPACITY = 5,
    RW3D_ERR_UNSUPPORTED = 6
} rw3d_status;

typedef enum rw3d_parse_flags_tag {
    RW3D_RAW_F_DEDUP_EXACT       = 0x0001UL,
    RW3D_RAW_F_ALLOW_QUADS       = 0x0002UL,
    RW3D_RAW_F_TRIANGULATE_QUADS = 0x0004UL,
    RW3D_RAW_F_COMPUTE_NORMALS   = 0x0008UL,
    RW3D_RAW_F_DROP_DEGENERATE   = 0x0010UL,
    RW3D_RAW_F_STRICT            = 0x0020UL,
    RW3D_RAW_F_ALLOW_COMMAS      = 0x0040UL
} rw3d_parse_flags;

#define RW3D_RAW_DEFAULT_FLAGS \
    (RW3D_RAW_F_DEDUP_EXACT | RW3D_RAW_F_ALLOW_QUADS | \
     RW3D_RAW_F_TRIANGULATE_QUADS | RW3D_RAW_F_COMPUTE_NORMALS | \
     RW3D_RAW_F_STRICT | RW3D_RAW_F_ALLOW_COMMAS)

typedef enum rw3d_write_flags_tag {
    RW3D_WRITE_F_CRLF = 0x0001UL,
    RW3D_WRITE_F_SIX_DECIMALS = 0x0002UL
} rw3d_write_flags;

typedef struct rw3d_arena_tag {
    unsigned char *base;
    size_t size;
    size_t used;
    int failed;
} rw3d_arena;

typedef struct rw3d_vec3q_tag {
    rw3d_fix x;
    rw3d_fix y;
    rw3d_fix z;
} rw3d_vec3q;

typedef struct rw3d_tri_tag {
    rw3d_u32 a;
    rw3d_u32 b;
    rw3d_u32 c;
    rw3d_vec3q normal; /* unnormalized fixed-point face normal */
} rw3d_tri;

typedef struct rw3d_mesh_tag {
    rw3d_vec3q *vertices;
    rw3d_tri *triangles;
    rw3d_u32 vertex_count;
    rw3d_u32 triangle_count;
    rw3d_u32 vertex_capacity;
    rw3d_u32 triangle_capacity;
    rw3d_vec3q bounds_min;
    rw3d_vec3q bounds_max;
    int has_bounds;
} rw3d_mesh;

typedef struct rw3d_report_tag {
    int status;
    const char *message;
    unsigned long line_count;
    unsigned long valid_lines;
    unsigned long blank_lines;
    unsigned long invalid_lines;
    unsigned long tri_lines;
    unsigned long quad_lines;
    unsigned long duplicate_vertices;
    unsigned long dropped_degenerate;
    unsigned long dropped_duplicate_triangles;
    unsigned long first_error_line;
    int first_error_code;
    size_t arena_used;
    size_t bytes_written;
    size_t bytes_needed;
} rw3d_report;

typedef struct rw3d_mesh_stats_tag {
    unsigned long vertices;
    unsigned long triangles;
    unsigned long edges_unique;
    unsigned long boundary_edges;
    unsigned long nonmanifold_edges;
    unsigned long duplicate_triangles;
    unsigned long degenerate_triangles;
    unsigned long zero_area_triangles;
    unsigned long isolated_vertices;
} rw3d_mesh_stats;

void rw3d_arena_init(rw3d_arena *arena, void *memory, size_t size);
void rw3d_arena_reset(rw3d_arena *arena);
void *rw3d_arena_alloc(rw3d_arena *arena, size_t size, size_t align);

void rw3d_mesh_zero(rw3d_mesh *mesh);
void rw3d_report_zero(rw3d_report *report);
void rw3d_mesh_stats_zero(rw3d_mesh_stats *stats);

rw3d_fix rw3d_fix_from_int(long value);
long rw3d_fix_to_int_floor(rw3d_fix value);
rw3d_fix rw3d_fix_mul(rw3d_fix a, rw3d_fix b);
rw3d_fix rw3d_fix_div_int(rw3d_fix a, long b);
int rw3d_fix_to_decimal(rw3d_fix value, char *out, size_t out_cap, int frac_digits);

int rw3d_estimate_raw_arena(const char *data,
                            size_t length,
                            unsigned long flags,
                            size_t *bytes_needed,
                            rw3d_report *report);

int rw3d_parse_raw_memory(const char *data,
                          size_t length,
                          rw3d_arena *arena,
                          unsigned long flags,
                          rw3d_mesh *mesh,
                          rw3d_report *report);

int rw3d_write_raw_memory(const rw3d_mesh *mesh,
                          char *out,
                          size_t out_cap,
                          unsigned long flags,
                          rw3d_report *report);

int rw3d_write_obj_memory(const rw3d_mesh *mesh,
                          char *out,
                          size_t out_cap,
                          unsigned long flags,
                          rw3d_report *report);

int rw3d_write_ply_ascii_memory(const rw3d_mesh *mesh,
                                char *out,
                                size_t out_cap,
                                unsigned long flags,
                                rw3d_report *report);

int rw3d_recompute_normals(rw3d_mesh *mesh);
int rw3d_recompute_bounds(rw3d_mesh *mesh);
int rw3d_validate_mesh(const rw3d_mesh *mesh, rw3d_report *report);
int rw3d_analyze_mesh(const rw3d_mesh *mesh, rw3d_mesh_stats *stats);
int rw3d_remove_degenerate_tris_in_place(rw3d_mesh *mesh, rw3d_report *report);
int rw3d_remove_duplicate_tris_in_place(rw3d_mesh *mesh, rw3d_report *report);
int rw3d_flip_winding(rw3d_mesh *mesh);
int rw3d_scale_translate(rw3d_mesh *mesh, rw3d_fix scale, rw3d_vec3q translate);
int rw3d_center_to_origin(rw3d_mesh *mesh);

const char *rw3d_status_name(int status);

#ifdef __cplusplus
}
#endif

#endif /* RAWTRI89_H */
