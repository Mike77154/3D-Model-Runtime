#include "cob_private.h"

static int emit_matrix_ascii(cob_write_fn fn, void *user, const cob_fixed m[16])
{
    int r;
    int c;
    int st;

    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            if (c != 0) {
                st = cob_emit(fn, user, " ");
                if (st != COB_OK) return st;
            }
            st = cob_emit_fixed(fn, user, m[r * 4 + c], 6);
            if (st != COB_OK) return st;
        }
        st = cob_emit(fn, user, "\n");
        if (st != COB_OK) return st;
    }
    return COB_OK;
}

static int emit_fixed3_line(cob_write_fn fn, void *user, const char *name, cob_fixed a, cob_fixed b, cob_fixed c)
{
    int st;

    st = cob_emit(fn, user, name);
    if (st != COB_OK) return st;
    st = cob_emit(fn, user, " ");
    if (st != COB_OK) return st;
    st = cob_emit_fixed(fn, user, a, 6);
    if (st != COB_OK) return st;
    st = cob_emit(fn, user, " ");
    if (st != COB_OK) return st;
    st = cob_emit_fixed(fn, user, b, 6);
    if (st != COB_OK) return st;
    st = cob_emit(fn, user, " ");
    if (st != COB_OK) return st;
    st = cob_emit_fixed(fn, user, c, 6);
    if (st != COB_OK) return st;
    return cob_emit(fn, user, "\n");
}

static int emit_chunk_head_ascii(cob_write_fn fn, void *user, const char *type, unsigned int minor, long id, long parent)
{
    int st;

    st = cob_emit(fn, user, type);
    if (st != COB_OK) return st;
    st = cob_emit(fn, user, " V0.");
    if (st != COB_OK) return st;
    if (minor < 10U) {
        st = cob_emit(fn, user, "0");
        if (st != COB_OK) return st;
    }
    st = cob_emit_u(fn, user, (unsigned long)minor);
    if (st != COB_OK) return st;
    st = cob_emit(fn, user, " Id ");
    if (st != COB_OK) return st;
    st = cob_emit_i(fn, user, id);
    if (st != COB_OK) return st;
    st = cob_emit(fn, user, " Parent ");
    if (st != COB_OK) return st;
    st = cob_emit_i(fn, user, parent);
    if (st != COB_OK) return st;
    return cob_emit(fn, user, " Size -1\n");
}

int cob_encode_ascii(const cob_scene *scene, cob_write_fn write_fn, void *user)
{
    unsigned long mi;
    unsigned long i;
    unsigned long fidx;
    unsigned long ridx;
    const cob_mesh *m;
    const cob_face *f;
    const cob_vertex_ref *vr;
    const cob_material *mat;
    int st;

    if (scene == 0 || write_fn == 0) {
        return COB_ERR_ARGUMENT;
    }
    st = cob_emit(write_fn, user, "Caligari V00.01ALH             \n");
    if (st != COB_OK) return st;

    for (mi = 0; mi < scene->mesh_count; ++mi) {
        m = &scene->meshes[mi];
        st = emit_chunk_head_ascii(write_fn, user, "PolH", 8U, m->id == 0 ? (long)(1000UL + mi) : m->id, m->parent_id);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "Name ");
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, m->name[0] ? m->name : "Mesh");
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n");
        if (st != COB_OK) return st;
        st = emit_fixed3_line(write_fn, user, "center", m->axes[0], m->axes[1], m->axes[2]);
        if (st != COB_OK) return st;
        st = emit_fixed3_line(write_fn, user, "x axis", m->axes[3], m->axes[4], m->axes[5]);
        if (st != COB_OK) return st;
        st = emit_fixed3_line(write_fn, user, "y axis", m->axes[6], m->axes[7], m->axes[8]);
        if (st != COB_OK) return st;
        st = emit_fixed3_line(write_fn, user, "z axis", m->axes[9], m->axes[10], m->axes[11]);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "Transform\n");
        if (st != COB_OK) return st;
        st = emit_matrix_ascii(write_fn, user, m->transform);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "World Vertices ");
        if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, m->vertex_count);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n");
        if (st != COB_OK) return st;
        for (i = 0; i < m->vertex_count; ++i) {
            const cob_vec3q *v;
            v = &scene->vertices[m->first_vertex + i];
            st = cob_emit_fixed(write_fn, user, v->x, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, " ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, v->y, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, " ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, v->z, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, "\n");
            if (st != COB_OK) return st;
        }
        st = cob_emit(write_fn, user, "Texture Vertices ");
        if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, m->uv_count);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n");
        if (st != COB_OK) return st;
        for (i = 0; i < m->uv_count; ++i) {
            const cob_vec2q *uv;
            uv = &scene->uvs[m->first_uv + i];
            st = cob_emit_fixed(write_fn, user, uv->u, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, " ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, uv->v, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, "\n");
            if (st != COB_OK) return st;
        }
        st = cob_emit(write_fn, user, "Faces ");
        if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, m->face_count);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n");
        if (st != COB_OK) return st;
        for (fidx = 0; fidx < m->face_count; ++fidx) {
            f = &scene->faces[m->first_face + fidx];
            st = cob_emit(write_fn, user, "Face verts ");
            if (st != COB_OK) return st;
            st = cob_emit_u(write_fn, user, (unsigned long)f->index_count);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, " flags ");
            if (st != COB_OK) return st;
            st = cob_emit_u(write_fn, user, (unsigned long)f->flags);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, " mat ");
            if (st != COB_OK) return st;
            st = cob_emit_i(write_fn, user, (long)f->material);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, "\n");
            if (st != COB_OK) return st;
            for (ridx = 0; ridx < f->index_count; ++ridx) {
                vr = &scene->indices[f->first_index + ridx];
                if (ridx != 0) {
                    st = cob_emit(write_fn, user, " ");
                    if (st != COB_OK) return st;
                }
                st = cob_emit(write_fn, user, "<");
                if (st != COB_OK) return st;
                st = cob_emit_i(write_fn, user, vr->pos_index);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, ",");
                if (st != COB_OK) return st;
                st = cob_emit_i(write_fn, user, vr->uv_index);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, ">");
                if (st != COB_OK) return st;
            }
            st = cob_emit(write_fn, user, "\n");
            if (st != COB_OK) return st;
        }
        st = cob_emit(write_fn, user, "DrawFlags ");
        if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, m->draw_flags);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n");
        if (st != COB_OK) return st;

        for (i = 0; i < scene->material_count; ++i) {
            mat = &scene->materials[i];
            if (mat->parent_id == m->id || (m->id == 0 && mat->parent_id == (long)(1000UL + mi))) {
                st = emit_chunk_head_ascii(write_fn, user, "Mat1", 8U, mat->id == 0 ? (long)(2000UL + i) : mat->id, m->id == 0 ? (long)(1000UL + mi) : m->id);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, "mat# ");
                if (st != COB_OK) return st;
                st = cob_emit_i(write_fn, user, mat->matnum);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, "\nshader: ");
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, mat->shader == 'm' ? "metal" : (mat->shader == 'f' ? "flat" : "phong"));
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, " facet: ");
                if (st != COB_OK) return st;
                if (mat->facet == 's') {
                    st = cob_emit(write_fn, user, "smooth");
                } else if (mat->facet == 'f') {
                    st = cob_emit(write_fn, user, "faceted");
                } else {
                    st = cob_emit(write_fn, user, "auto");
                    if (st == COB_OK) st = cob_emit_u(write_fn, user, (unsigned long)mat->autofacet_angle);
                }
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, "\nrgb ");
                if (st != COB_OK) return st;
                st = cob_emit_fixed(write_fn, user, mat->rgb[0], 6);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, ",");
                if (st != COB_OK) return st;
                st = cob_emit_fixed(write_fn, user, mat->rgb[1], 6);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, ",");
                if (st != COB_OK) return st;
                st = cob_emit_fixed(write_fn, user, mat->rgb[2], 6);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, "\nalpha ");
                if (st != COB_OK) return st;
                st = cob_emit_fixed(write_fn, user, mat->alpha, 6);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, " ka ");
                if (st != COB_OK) return st;
                st = cob_emit_fixed(write_fn, user, mat->ka, 6);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, " ks ");
                if (st != COB_OK) return st;
                st = cob_emit_fixed(write_fn, user, mat->ks, 6);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, " exp ");
                if (st != COB_OK) return st;
                st = cob_emit_fixed(write_fn, user, mat->exp, 6);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, " ior ");
                if (st != COB_OK) return st;
                st = cob_emit_fixed(write_fn, user, mat->ior, 6);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, "\n");
                if (st != COB_OK) return st;
            }
        }
    }
    return emit_chunk_head_ascii(write_fn, user, "END ", 0U, 0L, 0L);
}

static int write_bytes(cob_write_fn fn, void *user, const unsigned char *b, unsigned long n)
{
    return cob_emit_n(fn, user, b, n);
}

static int write_u8(cob_write_fn fn, void *user, unsigned int v)
{
    unsigned char b;
    b = (unsigned char)(v & 255U);
    return write_bytes(fn, user, &b, 1UL);
}

static int write_i16(cob_write_fn fn, void *user, cob_i16 v)
{
    unsigned char b[2];
    cob_put_i16_le(b, v);
    return write_bytes(fn, user, b, 2UL);
}

static int write_i32(cob_write_fn fn, void *user, cob_i32 v)
{
    unsigned char b[4];
    cob_put_i32_le(b, v);
    return write_bytes(fn, user, b, 4UL);
}

static int write_fixed_f4(cob_write_fn fn, void *user, cob_fixed v)
{
    unsigned char b[4];
    cob_fixed_to_ieee754_le(v, b);
    return write_bytes(fn, user, b, 4UL);
}

static int write_chunk_header_binary(cob_write_fn fn, void *user, const char *type, cob_u16 major, cob_u16 minor, cob_i32 id, cob_i32 parent, cob_i32 size)
{
    int st;

    st = write_bytes(fn, user, (const unsigned char *)type, 4UL);
    if (st != COB_OK) return st;
    st = write_i16(fn, user, (cob_i16)major);
    if (st != COB_OK) return st;
    st = write_i16(fn, user, (cob_i16)minor);
    if (st != COB_OK) return st;
    st = write_i32(fn, user, id);
    if (st != COB_OK) return st;
    st = write_i32(fn, user, parent);
    if (st != COB_OK) return st;
    return write_i32(fn, user, size);
}

static unsigned long bin_name_len(const char *s)
{
    unsigned long n;
    n = cob_strlen_ul(s);
    if (n > 32767UL) {
        n = 32767UL;
    }
    return n;
}

static int write_name_binary(cob_write_fn fn, void *user, const char *name)
{
    unsigned long n;
    int st;

    n = bin_name_len(name);
    st = write_i16(fn, user, (cob_i16)n);
    if (st != COB_OK) return st;
    return write_bytes(fn, user, (const unsigned char *)name, n);
}

static unsigned long mesh_binary_size(const cob_scene *scene, const cob_mesh *m)
{
    unsigned long sz;
    unsigned long i;
    const cob_face *f;

    (void)scene;
    sz = 2UL + 2UL + bin_name_len(m->name) + 48UL + 48UL;
    sz += 4UL + m->vertex_count * 12UL;
    sz += 4UL + m->uv_count * 8UL;
    sz += 4UL;
    for (i = 0; i < m->face_count; ++i) {
        f = &scene->faces[m->first_face + i];
        sz += 1UL + 2UL;
        if ((f->flags & COB_FACE_HOLE) == 0U) {
            sz += 2UL;
        }
        sz += (unsigned long)f->index_count * 8UL;
    }
    sz += 4UL;
    return sz;
}

static int write_mesh_binary(cob_write_fn fn, void *user, const cob_scene *scene, const cob_mesh *m, unsigned long mi)
{
    cob_i32 id;
    unsigned long i;
    unsigned long j;
    const cob_face *f;
    const cob_vertex_ref *vr;
    const cob_vec3q *v;
    const cob_vec2q *uv;
    int st;

    id = m->id == 0 ? (cob_i32)(1000UL + mi) : m->id;
    st = write_chunk_header_binary(fn, user, "PolH", 0U, 8U, id, m->parent_id, (cob_i32)mesh_binary_size(scene, m));
    if (st != COB_OK) return st;
    st = write_i16(fn, user, (cob_i16)m->dupe_count);
    if (st != COB_OK) return st;
    st = write_name_binary(fn, user, m->name[0] ? m->name : "Mesh");
    if (st != COB_OK) return st;
    for (i = 0; i < 12UL; ++i) {
        st = write_fixed_f4(fn, user, m->axes[i]);
        if (st != COB_OK) return st;
    }
    for (i = 0; i < 12UL; ++i) {
        st = write_fixed_f4(fn, user, m->transform[i]);
        if (st != COB_OK) return st;
    }
    st = write_i32(fn, user, (cob_i32)m->vertex_count);
    if (st != COB_OK) return st;
    for (i = 0; i < m->vertex_count; ++i) {
        v = &scene->vertices[m->first_vertex + i];
        st = write_fixed_f4(fn, user, v->x);
        if (st != COB_OK) return st;
        st = write_fixed_f4(fn, user, v->y);
        if (st != COB_OK) return st;
        st = write_fixed_f4(fn, user, v->z);
        if (st != COB_OK) return st;
    }
    st = write_i32(fn, user, (cob_i32)m->uv_count);
    if (st != COB_OK) return st;
    for (i = 0; i < m->uv_count; ++i) {
        uv = &scene->uvs[m->first_uv + i];
        st = write_fixed_f4(fn, user, uv->u);
        if (st != COB_OK) return st;
        st = write_fixed_f4(fn, user, uv->v);
        if (st != COB_OK) return st;
    }
    st = write_i32(fn, user, (cob_i32)m->face_count);
    if (st != COB_OK) return st;
    for (i = 0; i < m->face_count; ++i) {
        f = &scene->faces[m->first_face + i];
        st = write_u8(fn, user, f->flags);
        if (st != COB_OK) return st;
        st = write_i16(fn, user, (cob_i16)f->index_count);
        if (st != COB_OK) return st;
        if ((f->flags & COB_FACE_HOLE) == 0U) {
            st = write_i16(fn, user, f->material);
            if (st != COB_OK) return st;
        }
        for (j = 0; j < f->index_count; ++j) {
            vr = &scene->indices[f->first_index + j];
            st = write_i32(fn, user, vr->pos_index);
            if (st != COB_OK) return st;
            st = write_i32(fn, user, vr->uv_index);
            if (st != COB_OK) return st;
        }
    }
    return write_i32(fn, user, (cob_i32)m->draw_flags);
}

static int write_material_binary(cob_write_fn fn, void *user, const cob_material *mat, unsigned long idx, cob_i32 parent)
{
    int st;
    int k;
    cob_i32 id;
    unsigned long size;

    id = mat->id == 0 ? (cob_i32)(2000UL + idx) : mat->id;
    size = 2UL + 1UL + 1UL + 1UL + 32UL + 2UL;
    st = write_chunk_header_binary(fn, user, "Mat1", 0U, 8U, id, parent, (cob_i32)size);
    if (st != COB_OK) return st;
    st = write_i16(fn, user, mat->matnum);
    if (st != COB_OK) return st;
    st = write_u8(fn, user, mat->shader ? mat->shader : 'p');
    if (st != COB_OK) return st;
    st = write_u8(fn, user, mat->facet ? mat->facet : 'a');
    if (st != COB_OK) return st;
    st = write_u8(fn, user, mat->autofacet_angle ? mat->autofacet_angle : 40U);
    if (st != COB_OK) return st;
    for (k = 0; k < 3; ++k) {
        st = write_fixed_f4(fn, user, mat->rgb[k]);
        if (st != COB_OK) return st;
    }
    st = write_fixed_f4(fn, user, mat->alpha);
    if (st != COB_OK) return st;
    st = write_fixed_f4(fn, user, mat->ka);
    if (st != COB_OK) return st;
    st = write_fixed_f4(fn, user, mat->ks);
    if (st != COB_OK) return st;
    st = write_fixed_f4(fn, user, mat->exp);
    if (st != COB_OK) return st;
    st = write_fixed_f4(fn, user, mat->ior ? mat->ior : COB_FIXED_ONE);
    if (st != COB_OK) return st;
    return write_bytes(fn, user, (const unsigned char *)"xx", 2UL);
}

int cob_encode_binary(const cob_scene *scene, cob_write_fn write_fn, void *user)
{
    int st;
    unsigned long mi;
    unsigned long i;
    const cob_mesh *m;
    const cob_material *mat;
    cob_i32 mid;

    if (scene == 0 || write_fn == 0) {
        return COB_ERR_ARGUMENT;
    }
    st = cob_emit_n(write_fn, user, "Caligari V00.01BLH             \n", 32UL);
    if (st != COB_OK) return st;
    for (mi = 0; mi < scene->mesh_count; ++mi) {
        m = &scene->meshes[mi];
        mid = m->id == 0 ? (cob_i32)(1000UL + mi) : m->id;
        st = write_mesh_binary(write_fn, user, scene, m, mi);
        if (st != COB_OK) return st;
        for (i = 0; i < scene->material_count; ++i) {
            mat = &scene->materials[i];
            if (mat->parent_id == mid || (m->id != 0 && mat->parent_id == m->id)) {
                st = write_material_binary(write_fn, user, mat, i, mid);
                if (st != COB_OK) return st;
            }
        }
    }
    return write_chunk_header_binary(write_fn, user, "END ", 0U, 0U, 0L, 0L, 0L);
}

int cob_export_obj(const cob_scene *scene, cob_write_fn write_fn, void *user)
{
    unsigned long mi;
    unsigned long i;
    unsigned long fidx;
    unsigned long ridx;
    const cob_mesh *m;
    const cob_face *f;
    const cob_vertex_ref *vr;
    const cob_vec3q *v;
    const cob_vec2q *uv;
    int st;
    unsigned long vertex_base;
    unsigned long uv_base;

    if (scene == 0 || write_fn == 0) {
        return COB_ERR_ARGUMENT;
    }
    st = cob_emit(write_fn, user, "# OBJ exported by cob_chiqui3d C89 fixed-point codec\n");
    if (st != COB_OK) return st;
    vertex_base = 1UL;
    uv_base = 1UL;
    for (mi = 0; mi < scene->mesh_count; ++mi) {
        m = &scene->meshes[mi];
        st = cob_emit(write_fn, user, "o ");
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, m->name[0] ? m->name : "Mesh");
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n");
        if (st != COB_OK) return st;
        for (i = 0; i < m->vertex_count; ++i) {
            v = &scene->vertices[m->first_vertex + i];
            st = cob_emit(write_fn, user, "v ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, v->x, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, " ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, v->y, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, " ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, v->z, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, "\n");
            if (st != COB_OK) return st;
        }
        for (i = 0; i < m->uv_count; ++i) {
            uv = &scene->uvs[m->first_uv + i];
            st = cob_emit(write_fn, user, "vt ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, uv->u, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, " ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, uv->v, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, "\n");
            if (st != COB_OK) return st;
        }
        for (fidx = 0; fidx < m->face_count; ++fidx) {
            f = &scene->faces[m->first_face + fidx];
            if ((f->flags & COB_FACE_HOLE) != 0U) {
                continue;
            }
            st = cob_emit(write_fn, user, "f");
            if (st != COB_OK) return st;
            for (ridx = 0; ridx < f->index_count; ++ridx) {
                vr = &scene->indices[f->first_index + ridx];
                st = cob_emit(write_fn, user, " ");
                if (st != COB_OK) return st;
                st = cob_emit_u(write_fn, user, vertex_base + (unsigned long)vr->pos_index);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, "/");
                if (st != COB_OK) return st;
                st = cob_emit_u(write_fn, user, uv_base + (unsigned long)vr->uv_index);
                if (st != COB_OK) return st;
            }
            st = cob_emit(write_fn, user, "\n");
            if (st != COB_OK) return st;
        }
        vertex_base += m->vertex_count;
        uv_base += m->uv_count;
    }
    return COB_OK;
}
