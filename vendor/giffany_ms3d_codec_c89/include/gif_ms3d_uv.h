#ifndef GIF_MS3D_UV_H
#define GIF_MS3D_UV_H
/* Material bindings and UV convention; strictly ANSI C89/Q16.16.
 * MS3D triangle s/t are authored UVs (top-left image convention).
 * OBJ uses texture bottom-left; flip t ONCE at the export boundary.
 * Runtime callbacks keep unmodified MS3D coordinates.
 */
#include "gif_ms3d_skin.h"

typedef struct ms3d_uv_bindings_s {
    int material[MS3D_MAX_TRIANGLES];
    int group[MS3D_MAX_TRIANGLES];
    unsigned long assigned_count;
    unsigned long conflict_count;
} ms3d_uv_bindings;

int ms3d_uv_build_bindings(const ms3d_model *model, ms3d_uv_bindings *out);
/* Returns original MS3D S/T (origin at top left, T increasing down). */
void ms3d_uv_native(const ms3d_triangle *tri, ms3d_u8 corner,
                    ms3d_fx *u, ms3d_fx *v);
/* Returns OBJ-standard (S,1-T). No clamp: tiling must be preserved. */
void ms3d_uv_obj(const ms3d_triangle *tri, ms3d_u8 corner,
                 ms3d_fx *u, ms3d_fx *v);
/* If your graphics API expects a top-row-first image, pass native UV.
 * If your API expects bottom-left UV, pass OBJ UV. Never flip twice.
 */
/* Material-corrected runtime callbacks: same animated vertex and corner data,
 * with group/material resolved from explicit MilkShape group membership.
 * Pass the bindings prepared once by ms3d_uv_build_bindings().
 * Native uv[1] is never flipped here.
 */
int ms3d_emit_uv_corners(const ms3d_model *model,
                         const ms3d_corner_normals *normals,
                         const ms3d_uv_bindings *bindings,
                         const ms3d_shaded_sink *sink);
int ms3d_emit_skinned_uv_corners(const ms3d_model *model,
                                 const ms3d_skin_pose *pose,
                                 const ms3d_skin_vertices *vertices,
                                 const ms3d_corner_normals *normals,
                                 const ms3d_uv_bindings *bindings,
                                 const ms3d_shaded_sink *sink);
#endif
