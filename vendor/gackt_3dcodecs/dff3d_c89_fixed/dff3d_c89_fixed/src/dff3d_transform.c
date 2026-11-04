#include <string.h>
#include "dff3d.h"

static Dff3dVec3 dff3d_vec3_add(Dff3dVec3 a, Dff3dVec3 b)
{
    Dff3dVec3 r;
    r.x = dff3d_fixed_add_sat(a.x, b.x);
    r.y = dff3d_fixed_add_sat(a.y, b.y);
    r.z = dff3d_fixed_add_sat(a.z, b.z);
    return r;
}

static Dff3dVec3 dff3d_basis_transform_dir(const Dff3dFrame *f, Dff3dVec3 v, int world)
{
    Dff3dVec3 right;
    Dff3dVec3 up;
    Dff3dVec3 at;
    Dff3dVec3 r;
    Dff3dVec3 a;
    Dff3dVec3 b;
    Dff3dVec3 c;

    if (world) {
        right = f->world_right;
        up = f->world_up;
        at = f->world_at;
    } else {
        right = f->right;
        up = f->up;
        at = f->at;
    }
    a.x = dff3d_fixed_mul(right.x, v.x);
    a.y = dff3d_fixed_mul(right.y, v.x);
    a.z = dff3d_fixed_mul(right.z, v.x);
    b.x = dff3d_fixed_mul(up.x, v.y);
    b.y = dff3d_fixed_mul(up.y, v.y);
    b.z = dff3d_fixed_mul(up.z, v.y);
    c.x = dff3d_fixed_mul(at.x, v.z);
    c.y = dff3d_fixed_mul(at.y, v.z);
    c.z = dff3d_fixed_mul(at.z, v.z);
    r = dff3d_vec3_add(dff3d_vec3_add(a, b), c);
    return r;
}

static Dff3dVec3 dff3d_basis_transform_point(const Dff3dFrame *f, Dff3dVec3 v)
{
    return dff3d_vec3_add(dff3d_basis_transform_dir(f, v, 1), f->world_pos);
}

void dff3d_compute_frame_world(Dff3dModel *model)
{
    dff_u32 i;
    Dff3dFrame *f;
    const Dff3dFrame *p;

    if (model == DFF3D_NULL) {
        return;
    }
    for (i = 0U; i < model->frame_count; ++i) {
        f = &model->frames[i];
        if (f->parent >= 0 && (dff_u32)f->parent < i) {
            p = &model->frames[(dff_u32)f->parent];
            f->world_right = dff3d_basis_transform_dir(p, f->right, 1);
            f->world_up = dff3d_basis_transform_dir(p, f->up, 1);
            f->world_at = dff3d_basis_transform_dir(p, f->at, 1);
            f->world_pos = dff3d_basis_transform_point(p, f->pos);
        } else {
            f->world_right = f->right;
            f->world_up = f->up;
            f->world_at = f->at;
            f->world_pos = f->pos;
        }
    }
}

static void dff3d_emit_vertex(FILE *f, Dff3dVec3 pos)
{
    fputs("v ", f);
    dff3d_fprint_fixed(f, pos.x);
    fputc(' ', f);
    dff3d_fprint_fixed(f, pos.y);
    fputc(' ', f);
    dff3d_fprint_fixed(f, pos.z);
    fputc('\n', f);
}

static void dff3d_emit_normal(FILE *f, Dff3dVec3 normal)
{
    fputs("vn ", f);
    dff3d_fprint_fixed(f, normal.x);
    fputc(' ', f);
    dff3d_fprint_fixed(f, normal.y);
    fputc(' ', f);
    dff3d_fprint_fixed(f, normal.z);
    fputc('\n', f);
}

int dff3d_write_obj_world(FILE *f, Dff3dModel *model)
{
    dff_u32 a;
    dff_u32 i;
    dff_u32 face_base;
    const Dff3dAtomic *atom;
    const Dff3dGeometry *geom;
    const Dff3dFrame *frame;
    const Dff3dVertex *v;
    const Dff3dTriangle *t;

    if (f == DFF3D_NULL || model == DFF3D_NULL) {
        return DFF3D_ERR_NULL;
    }
    dff3d_compute_frame_world(model);
    fprintf(f, "# dff3d_c89_fixed world-space OBJ export\n");
    fprintf(f, "# atomics %lu geometries %lu\n", (unsigned long)model->atomic_count, (unsigned long)model->geometry_count);

    face_base = 1U;
    for (a = 0U; a < model->atomic_count; ++a) {
        atom = &model->atomics[a];
        if (atom->geometry_index >= model->geometry_count || atom->frame_index >= model->frame_count) {
            continue;
        }
        geom = &model->geometries[atom->geometry_index];
        frame = &model->frames[atom->frame_index];
        fprintf(f, "o atomic_%lu_geometry_%lu\n", (unsigned long)a, (unsigned long)atom->geometry_index);
        for (i = 0U; i < geom->vertex_count; ++i) {
            v = &model->vertices[geom->vertex_offset + i];
            dff3d_emit_vertex(f, dff3d_basis_transform_point(frame, v->pos));
        }
        for (i = 0U; i < geom->vertex_count; ++i) {
            v = &model->vertices[geom->vertex_offset + i];
            fputs("vt ", f);
            dff3d_fprint_fixed(f, v->uv0.u);
            fputc(' ', f);
            dff3d_fprint_fixed(f, v->uv0.v);
            fputc('\n', f);
        }
        for (i = 0U; i < geom->vertex_count; ++i) {
            v = &model->vertices[geom->vertex_offset + i];
            dff3d_emit_normal(f, dff3d_basis_transform_dir(frame, v->normal, 1));
        }
        for (i = 0U; i < geom->triangle_count; ++i) {
            t = &model->triangles[geom->triangle_offset + i];
            fprintf(f, "f %lu/%lu/%lu %lu/%lu/%lu %lu/%lu/%lu\n",
                (unsigned long)face_base + (unsigned long)t->a,
                (unsigned long)face_base + (unsigned long)t->a,
                (unsigned long)face_base + (unsigned long)t->a,
                (unsigned long)face_base + (unsigned long)t->b,
                (unsigned long)face_base + (unsigned long)t->b,
                (unsigned long)face_base + (unsigned long)t->b,
                (unsigned long)face_base + (unsigned long)t->c,
                (unsigned long)face_base + (unsigned long)t->c,
                (unsigned long)face_base + (unsigned long)t->c);
        }
        face_base += geom->vertex_count;
    }
    return DFF3D_OK;
}
