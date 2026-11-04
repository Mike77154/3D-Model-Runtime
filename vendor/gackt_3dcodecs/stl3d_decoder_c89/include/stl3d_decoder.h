#ifndef STL3D_DECODER_H
#define STL3D_DECODER_H

#include "stl3d_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STL3D_MIN_BUFFER_SIZE 128UL

#define STL3D_OK              0
#define STL3D_EOF             1
#define STL3D_ERR_ARG        -1
#define STL3D_ERR_IO         -2
#define STL3D_ERR_BUFFER     -3
#define STL3D_ERR_FORMAT     -4
#define STL3D_ERR_RANGE      -5
#define STL3D_ERR_OVERFLOW   -6

#define STL3D_FORMAT_UNKNOWN  0
#define STL3D_FORMAT_ASCII    1
#define STL3D_FORMAT_BINARY   2

#define STL3D_COLOR_NONE              0
#define STL3D_COLOR_VISCAM_SOLIDVIEW 1
#define STL3D_COLOR_MAGICS_OBJECT    2
#define STL3D_COLOR_MAGICS_FACET     3

#define STL3D_TRI_FLAG_DEGENERATE        0x0001U
#define STL3D_TRI_FLAG_ZERO_NORMAL       0x0002U
#define STL3D_TRI_FLAG_NORMAL_FLIPPED    0x0004U
#define STL3D_TRI_FLAG_NORMAL_MISMATCH   0x0008U
#define STL3D_TRI_FLAG_ATTRIBUTE_NONZERO 0x0010U
#define STL3D_TRI_FLAG_NON_FINITE        0x0020U
#define STL3D_TRI_FLAG_SATURATED         0x0040U

typedef struct stl3d_vec3_s {
    stl3d_fixed x;
    stl3d_fixed y;
    stl3d_fixed z;
} stl3d_vec3;

typedef struct stl3d_color_s {
    stl3d_u8 r;
    stl3d_u8 g;
    stl3d_u8 b;
    stl3d_u8 a;
    stl3d_u8 valid;
    stl3d_u8 kind;
} stl3d_color;

typedef struct stl3d_triangle_s {
    stl3d_vec3 normal;
    stl3d_vec3 v[3];
    stl3d_u16 attribute;
    stl3d_u32 index;
    stl3d_u16 flags;
    stl3d_color color_viscam;
    stl3d_color color_magics;
} stl3d_triangle;

typedef struct stl3d_stats_s {
    stl3d_u32 triangles;
    stl3d_vec3 minv;
    stl3d_vec3 maxv;
    int has_bounds;
    stl3d_u32 degenerate_triangles;
    stl3d_u32 zero_normals;
    stl3d_u32 normal_flipped;
    stl3d_u32 normal_mismatch;
    stl3d_u32 nonzero_attributes;
    stl3d_u32 non_finite_values;
    stl3d_u32 saturated_values;
    stl3d_u32 viscam_color_facets;
    stl3d_u32 magics_color_facets;
} stl3d_stats;

typedef int (*stl3d_read_fn)(void *user, stl3d_u8 *dst, unsigned long max_bytes, unsigned long *got_bytes);
typedef int (*stl3d_triangle_fn)(void *user, const stl3d_triangle *tri);

typedef struct stl3d_ctx_s {
    stl3d_read_fn read_fn;
    void *read_user;
    stl3d_u8 *buf;
    unsigned long cap;
    unsigned long pos;
    unsigned long len;
    int eof_seen;

    int format;
    int started;
    stl3d_u32 total_size;
    int has_total_size;
    int validate_triangles;

    stl3d_u8 binary_header[80];
    int has_binary_header;
    stl3d_color magics_object_color;

    stl3d_u32 binary_tri_total;
    stl3d_u32 binary_tri_left;
    stl3d_u32 tri_index;
} stl3d_ctx;

int stl3d_init(stl3d_ctx *ctx, stl3d_read_fn read_fn, void *read_user, stl3d_u8 *work_buf, unsigned long work_buf_size);
void stl3d_set_total_size(stl3d_ctx *ctx, stl3d_u32 total_size);
void stl3d_set_validation(stl3d_ctx *ctx, int enabled);
int stl3d_force_format(stl3d_ctx *ctx, int format);
int stl3d_detect_format(stl3d_ctx *ctx, int *format_out);
int stl3d_next_triangle(stl3d_ctx *ctx, stl3d_triangle *tri);
int stl3d_decode_all(stl3d_ctx *ctx, stl3d_triangle_fn cb, void *cb_user, stl3d_stats *stats);
void stl3d_stats_init(stl3d_stats *stats);
void stl3d_stats_add(stl3d_stats *stats, const stl3d_triangle *tri);

void stl3d_decode_viscam_color(stl3d_u16 attribute, stl3d_color *out);
void stl3d_decode_magics_facet_color(stl3d_u16 attribute, const stl3d_color *object_color, stl3d_color *out);
int stl3d_get_magics_object_color(const stl3d_ctx *ctx, stl3d_color *out);
int stl3d_get_binary_header(const stl3d_ctx *ctx, stl3d_u8 out80[80]);
stl3d_u16 stl3d_validate_triangle(stl3d_triangle *tri);

const char *stl3d_error_string(int code);

#ifdef __cplusplus
}
#endif

#endif /* STL3D_DECODER_H */
