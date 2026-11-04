#ifndef DTS89_POSE37_H
#define DTS89_POSE37_H
#include "dts89_modules.h"

/* Fixed-point Q16.16 affine row-major matrix (3 rows by 4 columns).
   The caller owns every buffer. No allocator, stdio or engine global state. */
typedef struct dts89_fx_mat34 { dts89_fx16_16 a[12]; } dts89_fx_mat34;

int dts89_animer_evaluate(const dts89_shape *shape, dts89_s32 sequence_index,
                          dts89_fx16_16 position, dts89_fx_mat34 *global,
                          dts89_u32 capacity);
void dts89_pose37_identity(dts89_fx_mat34 *m);
void dts89_pose37_mul(const dts89_fx_mat34 *a, const dts89_fx_mat34 *b, dts89_fx_mat34 *out);
void dts89_pose37_point(const dts89_fx_mat34 *m, const dts89_fx_vec3 *p, dts89_fx_vec3 *out);
int dts89_pose37_evaluate(const dts89_shape *s, dts89_s32 sequence_index,
                          dts89_fx16_16 position, dts89_fx_mat34 *global,
                          dts89_u32 capacity);

/* Skin matrices use global_animated[nodeIndex[bone]] * inverse_bind[bone].
   Caller provides a palette and vertices/weight sums (no heap usage).
   The fixed integer weight sums are available for error diagnostics. */
int dts89_mesher37_palette(const dts89_shape *s, const dts89_mesh_view *mesh,
                           const dts89_fx_mat34 *global, dts89_u32 global_count,
                           dts89_fx_mat34 *palette, dts89_u32 palette_capacity);
int dts89_mesher37_skin_normals(const dts89_mesh_view *mesh,
                               const dts89_fx_mat34 *palette, dts89_u32 palette_count,
                               dts89_fx_vec3 *normals, dts89_u32 capacity);
int dts89_mesher37_skin(const dts89_mesh_view *mesh,
                        const dts89_fx_mat34 *palette, dts89_u32 palette_count,
                        dts89_fx_vec3 *vertices, dts89_fx16_16 *weight_sums,
                        dts89_u32 capacity);
#endif
