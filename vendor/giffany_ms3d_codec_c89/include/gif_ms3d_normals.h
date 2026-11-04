#ifndef GIF_MS3D_NORMALS_H
#define GIF_MS3D_NORMALS_H
/* C89, Q16.16, no dynamic allocation or floating-point runtime.
 * Shading normals are indexed per *triangle corner*, not per vertex.
 * Keep this module independent of graphics APIs for engine vendoring.
 */
#include "gif_ms3d_codec.h"

#define MS3D_NORMAL_SOURCE 0
#define MS3D_NORMAL_SMOOTH_GROUP 1
#define MS3D_NORMAL_SMOOTH_ALL 2
#define MS3D_NORMAL_FLAT 3
#define MS3D_NORMAL_SMOOTH_WELDED 4 /* positional weld for normals only */

typedef struct ms3d_corner_normals_s {
    ms3d_fx xyz[MS3D_MAX_TRIANGLES][3][3];
} ms3d_corner_normals;

/* output buffer should be statically allocated (large); no heap.
 * MS3D_NORMAL_SOURCE: the exact authored MS3D corner normals.
 * MS3D_NORMAL_SMOOTH_GROUP: average adjacent face normals by vertex
 *    index AND matching MilkShape smoothing group (1..32); 0 is flat.
 * MS3D_NORMAL_SMOOTH_ALL: force all faces into group 1 (artistic option).
 * MS3D_NORMAL_FLAT: geometrical face normal for every corner.
 * MS3D_NORMAL_SMOOTH_WELDED: weld coincident positions for normal
 *     averaging, but keep the distinct original vertex indices/UVs.
 * Coordinates and UVs are not modified. Output is Q16.16.
 */
int ms3d_build_corner_normals(const ms3d_model *model,
                              ms3d_corner_normals *out,
                              int mode);
/* Renderer-independent engine bridge: native UV orientation, per-corner
 * Q16 position/normal/UV. Consumers choose whether to flip V for their API.
 * Surface grouping and original triangulation remain unchanged.
 */
typedef struct ms3d_draw_corner_s {
    ms3d_fx position[3];
    ms3d_fx normal[3];
    ms3d_fx uv[2];
    ms3d_u16 source_vertex;
    ms3d_u8 smoothing_group;
    ms3d_u8 group_index;
    int material_index;
} ms3d_draw_corner;

typedef struct ms3d_shaded_sink_s {
    void *user;
    int (*corner)(void *user, ms3d_u16 triangle_index,
                  ms3d_u8 corner_index, const ms3d_draw_corner *corner);
} ms3d_shaded_sink;

int ms3d_emit_shaded_corners(const ms3d_model *model,
                              const ms3d_corner_normals *normals,
                              const ms3d_shaded_sink *sink);
#endif
