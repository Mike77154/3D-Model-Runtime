#include "dff3d.h"

static void dff3d_fprint_unit(FILE *f, dff_u8 v)
{
    unsigned long n;
    n = ((unsigned long)v * 1000UL + 127UL) / 255UL;
    fprintf(f, "%lu.%03lu", n / 1000UL, n % 1000UL);
}

int dff3d_write_mtl(FILE *f, const Dff3dModel *model)
{
    dff_u32 i;
    const Dff3dMaterial *m;

    if (f == DFF3D_NULL || model == DFF3D_NULL) {
        return DFF3D_ERR_NULL;
    }
    fprintf(f, "# dff3d_c89_fixed MTL export\n");
    for (i = 0U; i < model->material_count; ++i) {
        m = &model->materials[i];
        fprintf(f, "newmtl dff_mat_%lu\n", (unsigned long)i);
        fputs("Kd ", f);
        dff3d_fprint_unit(f, m->color.r);
        fputc(' ', f);
        dff3d_fprint_unit(f, m->color.g);
        fputc(' ', f);
        dff3d_fprint_unit(f, m->color.b);
        fputc('\n', f);
        fputs("d ", f);
        dff3d_fprint_unit(f, m->color.a);
        fputc('\n', f);
        fputs("Ka ", f);
        dff3d_fprint_fixed(f, m->ambient);
        fputc(' ', f);
        dff3d_fprint_fixed(f, m->ambient);
        fputc(' ', f);
        dff3d_fprint_fixed(f, m->ambient);
        fputc('\n', f);
        fputs("Ks ", f);
        dff3d_fprint_fixed(f, m->specular);
        fputc(' ', f);
        dff3d_fprint_fixed(f, m->specular);
        fputc(' ', f);
        dff3d_fprint_fixed(f, m->specular);
        fputc('\n', f);
        if (m->texture[0] != '\0') {
            fprintf(f, "map_Kd %s.png\n", m->texture);
        }
        if (m->mask[0] != '\0') {
            fprintf(f, "# rw_mask %s\n", m->mask);
        }
        if (m->matfx_count != 0U) {
            fprintf(f, "# rw_matfx first=%lu count=%lu\n", (unsigned long)m->matfx_index, (unsigned long)m->matfx_count);
        }
        fputc('\n', f);
    }
    return DFF3D_OK;
}
