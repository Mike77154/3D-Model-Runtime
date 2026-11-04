#ifndef GIF_MS3D_RENDER_H
#define GIF_MS3D_RENDER_H
/* Backend-agnostic material/UV render order for the MS3D v1.8.5 modes.
 * No image decode or draw calls; C89/Q16.16, static caller buffers.
 */
#include "gif_ms3d_player.h"
#define MS3D_RENDER_OPAQUE 0
#define MS3D_RENDER_ALPHA_TEST 1
#define MS3D_RENDER_ALPHA_BLEND 2
#define MS3D_TRANSPARENCY_SIMPLE 0
#define MS3D_TRANSPARENCY_ALPHA_TEST 1
#define MS3D_TRANSPARENCY_DEPTH_SORT 2
typedef struct ms3d_material_pass_s {
    int pass; /* OPAQUE / ALPHA_TEST / ALPHA_BLEND */
    ms3d_fx opacity; /* 0..1 Q16.16; material transparency */
    ms3d_fx alpha_ref; /* model alphaRef when ALPHA_TEST */
    int depth_write; /* renderer hint */
    int blend_enabled;
    const char *diffuse_path;
    const char *alpha_path;
} ms3d_material_pass;
typedef struct ms3d_render_queue_s {
    ms3d_u16 triangle[MS3D_MAX_TRIANGLES];
    ms3d_fx depth[MS3D_MAX_TRIANGLES];
    ms3d_u16 opaque_count,cutout_count,blend_count;
} ms3d_render_queue;
int ms3d_render_material_pass(const ms3d_model *m,int material_id,
                              ms3d_material_pass *out);
/* front points into scene, from camera toward objects. For mode 2,
 * alpha-blended triangles are sorted far-to-near by centroid depth.
 * Opaque/cutout triangle order is preserved. This does not rasterize.
 */
int ms3d_render_build_queue(const ms3d_model *m,
    const ms3d_uv_bindings *bindings,const ms3d_skin_vertices *vertices,
    const ms3d_fx front[3],ms3d_render_queue *queue);
#endif
