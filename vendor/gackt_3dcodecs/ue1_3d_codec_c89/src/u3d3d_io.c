#include "u3d3d.h"

#ifdef U3D3D_ENABLE_STDIO

int u3d3d_fread_all(const char *path, u3d3d_u8 *dst, u3d3d_u32 dst_capacity, u3d3d_u32 *out_size)
{
    FILE *f;
    long sz;
    size_t got;

    if (path == (const char *)0 || dst == (u3d3d_u8 *)0 || out_size == (u3d3d_u32 *)0) {
        return U3D3D_ERR_NULL;
    }

    f = fopen(path, "rb");
    if (f == (FILE *)0) {
        return U3D3D_ERR_IO;
    }
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return U3D3D_ERR_IO;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return U3D3D_ERR_IO;
    }
    if ((u3d3d_u32)sz > dst_capacity) {
        fclose(f);
        return U3D3D_ERR_CAPACITY;
    }
    if (fseek(f, 0L, SEEK_SET) != 0) {
        fclose(f);
        return U3D3D_ERR_IO;
    }
    got = fread(dst, 1u, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) {
        return U3D3D_ERR_IO;
    }
    *out_size = (u3d3d_u32)sz;
    return U3D3D_OK;
}

void u3d3d_fprint_fx(FILE *out, u3d3d_fx v)
{
    u3d3d_u32 uv;
    u3d3d_u32 whole;
    u3d3d_u32 frac;
    unsigned digit_i;
    unsigned digit;

    if (out == (FILE *)0) {
        return;
    }
    if (v < 0) {
        fputc('-', out);
        uv = (u3d3d_u32)(-v);
    } else {
        uv = (u3d3d_u32)v;
    }
    whole = uv >> 16;
    frac = uv & 0xffffu;
    fprintf(out, "%lu.", (unsigned long)whole);
    for (digit_i = 0u; digit_i < 6u; ++digit_i) {
        frac *= 10u;
        digit = (unsigned)(frac >> 16);
        fputc((int)('0' + digit), out);
        frac &= 0xffffu;
    }
}

static u3d3d_fx u3d3d_apply_optional_scale(u3d3d_fx v, u3d3d_fx s, u3d3d_u8 has_s)
{
    if (has_s == 0u) {
        return v;
    }
    return u3d3d_fx_mul(v, s);
}

int u3d3d_export_obj(FILE *out,
                     const u3d3d_model *model,
                     const u3d3d_vertex *vertices,
                     const char *object_name,
                     const u3d3d_uc_info *uc_info,
                     unsigned flags)
{
    u3d3d_u16 i;
    const u3d3d_triangle *t;
    u3d3d_fx x;
    u3d3d_fx y;
    u3d3d_fx z;

    (void)flags;

    if (out == (FILE *)0 || model == (const u3d3d_model *)0 || vertices == (const u3d3d_vertex *)0) {
        return U3D3D_ERR_NULL;
    }

    fprintf(out, "# OBJ exported by u3d3d C89 Unreal *.3D codec\n");
    if (object_name != (const char *)0) {
        fprintf(out, "o %s\n", object_name);
    } else {
        fprintf(out, "o u3d3d_frame\n");
    }

    for (i = 0u; i < model->vertex_count; ++i) {
        x = vertices[i].x;
        y = vertices[i].y;
        z = vertices[i].z;
        if (uc_info != (const u3d3d_uc_info *)0) {
            x = u3d3d_apply_optional_scale(x, uc_info->scale_x, uc_info->has_scale_x);
            y = u3d3d_apply_optional_scale(y, uc_info->scale_y, uc_info->has_scale_y);
            z = u3d3d_apply_optional_scale(z, uc_info->scale_z, uc_info->has_scale_z);
        }
        fputs("v ", out);
        u3d3d_fprint_fx(out, x);
        fputc(' ', out);
        u3d3d_fprint_fx(out, y);
        fputc(' ', out);
        u3d3d_fprint_fx(out, z);
        fputc('\n', out);
    }

    for (i = 0u; i < model->triangle_count; ++i) {
        t = &model->triangles[i];
        fprintf(out, "vt %lu.0 %lu.0\n", (unsigned long)t->uv[0][0], (unsigned long)t->uv[0][1]);
        fprintf(out, "vt %lu.0 %lu.0\n", (unsigned long)t->uv[1][0], (unsigned long)t->uv[1][1]);
        fprintf(out, "vt %lu.0 %lu.0\n", (unsigned long)t->uv[2][0], (unsigned long)t->uv[2][1]);
    }

    for (i = 0u; i < model->triangle_count; ++i) {
        t = &model->triangles[i];
        if (t->type != U3D3D_MF_WEAPON_PLACEHOLDER) {
            fprintf(out, "# tri %lu type=%s tex=%lu flags=%lu\n",
                    (unsigned long)i,
                    u3d3d_triangle_type_name(t->type),
                    (unsigned long)t->texture_num,
                    (unsigned long)t->flags);
            fprintf(out, "f %lu/%lu %lu/%lu %lu/%lu\n",
                    (unsigned long)t->v[0] + 1ul, (unsigned long)i * 3ul + 1ul,
                    (unsigned long)t->v[1] + 1ul, (unsigned long)i * 3ul + 2ul,
                    (unsigned long)t->v[2] + 1ul, (unsigned long)i * 3ul + 3ul);
            if (t->type == U3D3D_MF_NORMAL_TS ||
                t->type == U3D3D_MF_TRANSLUCENT_TS ||
                t->type == U3D3D_MF_MASKED_TS ||
                t->type == U3D3D_MF_MODULATED_TS) {
                fprintf(out, "f %lu/%lu %lu/%lu %lu/%lu\n",
                        (unsigned long)t->v[2] + 1ul, (unsigned long)i * 3ul + 3ul,
                        (unsigned long)t->v[1] + 1ul, (unsigned long)i * 3ul + 2ul,
                        (unsigned long)t->v[0] + 1ul, (unsigned long)i * 3ul + 1ul);
            }
        }
    }

    return U3D3D_OK;
}

#endif
