#include "uc3d.h"

static unsigned short uc3d_rd16le(const unsigned char *p)
{
    return (unsigned short)((unsigned short)p[0] | ((unsigned short)p[1] << 8));
}

static unsigned long uc3d_rd32le(const unsigned char *p)
{
    return ((unsigned long)p[0]) |
           ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) |
           ((unsigned long)p[3] << 24);
}

static uc3d_fixed uc3d_signed_unit_from_fixedbits(unsigned long raw, int frac_shift)
{
    uc3d_fixed v;
    v = (uc3d_fixed)(raw << (UC3D_FIXED_SHIFT - frac_shift));
    if (v >= (128L << UC3D_FIXED_SHIFT)) {
        v -= (256L << UC3D_FIXED_SHIFT);
    }
    return v;
}

static int uc3d_validate_ue1(const unsigned char *data_file,
                             unsigned long data_size,
                             const unsigned char *anim_file,
                             unsigned long anim_size,
                             unsigned long *poly_count,
                             unsigned long *vert_count,
                             unsigned long *frame_count,
                             unsigned long *frame_size)
{
    unsigned long face_bytes;

    if (data_file == (const unsigned char *)0 || anim_file == (const unsigned char *)0) {
        return UC3D_ERR_NULL;
    }
    if (data_size < 48UL || anim_size < 4UL) {
        return UC3D_ERR_TRUNCATED;
    }

    *poly_count = (unsigned long)uc3d_rd16le(data_file + 0UL);
    *vert_count = (unsigned long)uc3d_rd16le(data_file + 2UL);
    *frame_count = (unsigned long)uc3d_rd16le(anim_file + 0UL);
    *frame_size = (unsigned long)uc3d_rd16le(anim_file + 2UL);

    face_bytes = 48UL + (*poly_count * 16UL);
    if (face_bytes > data_size) {
        return UC3D_ERR_TRUNCATED;
    }
    if (*frame_count == 0UL || *frame_size == 0UL) {
        return UC3D_ERR_BAD_FORMAT;
    }
    if (4UL + ((*frame_count - 1UL) * (*frame_size)) >= anim_size) {
        return UC3D_ERR_TRUNCATED;
    }
    return UC3D_OK;
}

int uc3d_ue1_decode_frame(const unsigned char *data_file,
                          unsigned long data_size,
                          const unsigned char *anim_file,
                          unsigned long anim_size,
                          unsigned long frame_index,
                          int format,
                          uc3d_ue1_mesh_out *out_mesh)
{
    unsigned long poly_count;
    unsigned long vert_count;
    unsigned long frame_count;
    unsigned long frame_size;
    unsigned long p;
    unsigned long v;
    unsigned long base;
    unsigned long raw;
    unsigned long expected_frame_min;
    int res;
    uc3d_face *face;
    uc3d_vertex *vertex;

    if (out_mesh == (uc3d_ue1_mesh_out *)0) {
        return UC3D_ERR_NULL;
    }
    out_mesh->vertex_count = 0UL;
    out_mesh->face_count = 0UL;
    out_mesh->frame_count = 0UL;
    out_mesh->frame_size = 0UL;
    out_mesh->warning_count = 0UL;

    if (out_mesh->vertices == (uc3d_vertex *)0 || out_mesh->faces == (uc3d_face *)0) {
        return UC3D_ERR_NULL;
    }

    res = uc3d_validate_ue1(data_file, data_size, anim_file, anim_size,
                            &poly_count, &vert_count, &frame_count, &frame_size);
    if (res != UC3D_OK) {
        return res;
    }

    if (frame_index >= frame_count) {
        return UC3D_ERR_RANGE;
    }
    if (vert_count > out_mesh->vertex_capacity || poly_count > out_mesh->face_capacity) {
        return UC3D_ERR_FULL;
    }

    if (format == UC3D_UE1_FORMAT_ION) {
        expected_frame_min = vert_count * 8UL;
    } else {
        expected_frame_min = vert_count * 4UL;
    }
    if (frame_size < expected_frame_min) {
        return UC3D_ERR_TRUNCATED;
    }
    if (4UL + frame_index * frame_size + expected_frame_min > anim_size) {
        return UC3D_ERR_TRUNCATED;
    }

    for (p = 0UL; p < poly_count; ++p) {
        base = 48UL + p * 16UL;
        face = &out_mesh->faces[p];
        face->v[0] = uc3d_rd16le(data_file + base + 0UL);
        face->v[1] = uc3d_rd16le(data_file + base + 2UL);
        face->v[2] = uc3d_rd16le(data_file + base + 4UL);
        face->poly_flags = data_file[base + 6UL];
        face->mesh_color = data_file[base + 7UL];
        face->uv[0][0] = data_file[base + 8UL];
        face->uv[0][1] = data_file[base + 9UL];
        face->uv[1][0] = data_file[base + 10UL];
        face->uv[1][1] = data_file[base + 11UL];
        face->uv[2][0] = data_file[base + 12UL];
        face->uv[2][1] = data_file[base + 13UL];
        face->tex_num = data_file[base + 14UL];
        face->aux_flags = data_file[base + 15UL];
        if ((unsigned long)face->v[0] >= vert_count ||
            (unsigned long)face->v[1] >= vert_count ||
            (unsigned long)face->v[2] >= vert_count) {
            out_mesh->warning_count++;
        }
    }

    for (v = 0UL; v < vert_count; ++v) {
        vertex = &out_mesh->vertices[v];
        if (format == UC3D_UE1_FORMAT_ION) {
            base = 4UL + frame_index * frame_size + v * 8UL;
            vertex->x = uc3d_signed_unit_from_fixedbits((unsigned long)uc3d_rd16le(anim_file + base + 0UL), 8);
            vertex->y = -uc3d_signed_unit_from_fixedbits((unsigned long)uc3d_rd16le(anim_file + base + 2UL), 8);
            vertex->z = uc3d_signed_unit_from_fixedbits((unsigned long)uc3d_rd16le(anim_file + base + 4UL), 8);
        } else {
            base = 4UL + frame_index * frame_size + v * 4UL;
            raw = uc3d_rd32le(anim_file + base);
            vertex->x = uc3d_signed_unit_from_fixedbits(raw & 0x7FFUL, 3);
            vertex->y = -uc3d_signed_unit_from_fixedbits((raw >> 11) & 0x7FFUL, 3);
            vertex->z = uc3d_signed_unit_from_fixedbits((raw >> 22) & 0x3FFUL, 2);
        }
    }

    out_mesh->vertex_count = vert_count;
    out_mesh->face_count = poly_count;
    out_mesh->frame_count = frame_count;
    out_mesh->frame_size = frame_size;
    return UC3D_OK;
}

static uc3d_fixed uc3d_fixed_mul(uc3d_fixed a, uc3d_fixed b)
{
    return (uc3d_fixed)((a * b) >> UC3D_FIXED_SHIFT);
}

void uc3d_apply_mesh_ref_transform(const uc3d_mesh_ref *ref,
                                   uc3d_vertex *vertices,
                                   unsigned long vertex_count)
{
    unsigned long i;
    if (ref == (const uc3d_mesh_ref *)0 || vertices == (uc3d_vertex *)0) {
        return;
    }
    for (i = 0UL; i < vertex_count; ++i) {
        vertices[i].x = uc3d_fixed_mul(vertices[i].x, ref->scale_x) + ref->origin_x + ref->import_x;
        vertices[i].y = uc3d_fixed_mul(vertices[i].y, ref->scale_y) + ref->origin_y + ref->import_y;
        vertices[i].z = uc3d_fixed_mul(vertices[i].z, ref->scale_z) + ref->origin_z + ref->import_z;
    }
}
