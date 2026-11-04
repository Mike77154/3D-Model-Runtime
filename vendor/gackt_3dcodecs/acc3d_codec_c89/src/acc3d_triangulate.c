#include "acc3d.h"

int acc3d_triangulate(struct acc3d_model *model, unsigned long options)
{
    long si;
    if (model == (struct acc3d_model *)0) return ACC3D_ERR_ARG;
    model->triangle_count = 0;
    for (si = 0; si < model->surface_count; ++si) {
        struct acc3d_surface *s;
        long kind;
        long ri;
        s = &model->surfaces[si];
        kind = s->flags & 0x0fL;
        if ((options & ACC3D_OPT_SKIP_NON_POLY) && kind != 0L) continue;
        if (s->ref_count < 3) continue;
        if ((options & ACC3D_OPT_STRIP_AS_STRIP) && (model->header_has_strip_suffix || model->is_acc_like)) {
            for (ri = 0; ri + 2 < s->ref_count; ++ri) {
                struct acc3d_triangle *tri;
                long r0, r1, r2;
                if (model->triangle_count >= ACC3D_MAX_TRIANGLES) {
                    model->diag_bits |= ACC3D_DIAG_LIMIT_HIT;
                    return ACC3D_ERR_LIMIT;
                }
                r0 = s->ref_start + ri;
                r1 = s->ref_start + ri + 1;
                r2 = s->ref_start + ri + 2;
                tri = &model->triangles[model->triangle_count];
                tri->surface_index = si;
                tri->material_index = s->material_index;
                if ((ri & 1L) == 0L) {
                    tri->ref_index[0] = r0; tri->ref_index[1] = r1; tri->ref_index[2] = r2;
                    tri->v[0] = model->refs[r0].vertex_index;
                    tri->v[1] = model->refs[r1].vertex_index;
                    tri->v[2] = model->refs[r2].vertex_index;
                } else {
                    tri->ref_index[0] = r1; tri->ref_index[1] = r0; tri->ref_index[2] = r2;
                    tri->v[0] = model->refs[r1].vertex_index;
                    tri->v[1] = model->refs[r0].vertex_index;
                    tri->v[2] = model->refs[r2].vertex_index;
                }
                ++model->triangle_count;
            }
        } else {
            for (ri = 1; ri + 1 < s->ref_count; ++ri) {
                struct acc3d_triangle *tri;
                long r0, r1, r2;
                if (model->triangle_count >= ACC3D_MAX_TRIANGLES) {
                    model->diag_bits |= ACC3D_DIAG_LIMIT_HIT;
                    return ACC3D_ERR_LIMIT;
                }
                r0 = s->ref_start;
                r1 = s->ref_start + ri;
                r2 = s->ref_start + ri + 1;
                tri = &model->triangles[model->triangle_count];
                tri->surface_index = si;
                tri->material_index = s->material_index;
                tri->ref_index[0] = r0; tri->ref_index[1] = r1; tri->ref_index[2] = r2;
                tri->v[0] = model->refs[r0].vertex_index;
                tri->v[1] = model->refs[r1].vertex_index;
                tri->v[2] = model->refs[r2].vertex_index;
                ++model->triangle_count;
            }
        }
    }
    return ACC3D_OK;
}
