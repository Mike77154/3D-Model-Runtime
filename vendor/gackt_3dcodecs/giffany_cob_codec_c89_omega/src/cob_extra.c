#include "cob_private.h"

/* cob_extra.c: optional C89 fixed-point processing helpers. */

static cob_fixed cobx_sat_add(cob_fixed a, cob_fixed b)
{
    if (b > 0 && a > COB_FIXED_MAX - b) {
        return COB_FIXED_MAX;
    }
    if (b < 0 && a < COB_FIXED_MIN - b) {
        return COB_FIXED_MIN;
    }
    return a + b;
}

static unsigned long cobx_abs_fixed(cob_fixed v)
{
    if (v == COB_FIXED_MIN) {
        return 2147483648UL;
    }
    if (v < 0) {
        return (unsigned long)(-v);
    }
    return (unsigned long)v;
}

static cob_fixed cobx_from_mag_sign(unsigned long mag, int sign)
{
    if (sign < 0) {
        if (mag >= 2147483648UL) {
            return COB_FIXED_MIN;
        }
        return -(cob_fixed)mag;
    }
    if (mag > (unsigned long)COB_FIXED_MAX) {
        return COB_FIXED_MAX;
    }
    return (cob_fixed)mag;
}

static cob_fixed cobx_mul(cob_fixed a, cob_fixed b)
{
    unsigned long am;
    unsigned long bm;
    unsigned long aw;
    unsigned long af;
    unsigned long bw;
    unsigned long bf;
    unsigned long out;
    int sign;

    sign = 1;
    if (a < 0) {
        sign = -sign;
    }
    if (b < 0) {
        sign = -sign;
    }
    am = cobx_abs_fixed(a);
    bm = cobx_abs_fixed(b);
    aw = am >> 16;
    af = am & 65535UL;
    bw = bm >> 16;
    bf = bm & 65535UL;

    if (aw != 0UL && bw > 32767UL / aw) {
        return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    }
    out = (aw * bw) << 16;
    if (aw != 0UL && bf > ((unsigned long)COB_FIXED_MAX - out) / aw) {
        return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    }
    out += aw * bf;
    if (bw != 0UL && af > ((unsigned long)COB_FIXED_MAX - out) / bw) {
        return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    }
    out += bw * af;
    out += (af * bf) >> 16;
    return cobx_from_mag_sign(out, sign);
}

static cob_fixed cobx_div_mag_to_fixed(cob_fixed v, unsigned long d)
{
    unsigned long mag;
    unsigned long whole;
    unsigned long rem;
    unsigned long out;
    int sign;

    if (d == 0UL) {
        return 0;
    }
    sign = v < 0 ? -1 : 1;
    mag = cobx_abs_fixed(v);
    whole = mag / d;
    rem = mag % d;
    if (whole > 32767UL) {
        return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    }
    out = (whole << 16) + ((rem << 16) / d);
    return cobx_from_mag_sign(out, sign);
}

static int cobx_ref_valid(const cob_scene *scene, const cob_mesh *m, const cob_vertex_ref *vr)
{
    if (scene == 0 || m == 0 || vr == 0) {
        return 0;
    }
    if (vr->pos_index < 0 || (unsigned long)vr->pos_index >= m->vertex_count) {
        return 0;
    }
    if (m->uv_count != 0UL) {
        if (vr->uv_index < 0 || (unsigned long)vr->uv_index >= m->uv_count) {
            return 0;
        }
    }
    return 1;
}

static void cobx_stats_zero(cob_scene_stats *stats)
{
    if (stats != 0) {
        memset(stats, 0, sizeof(*stats));
    }
}

int cob_scene_validate(const cob_scene *scene, cob_scene_stats *stats)
{
    unsigned long mi;
    unsigned long fi;
    unsigned long ri;
    unsigned long local_triangles;
    const cob_mesh *m;
    const cob_face *f;
    const cob_vertex_ref *vr;

    if (scene == 0) {
        return COB_ERR_ARGUMENT;
    }
    cobx_stats_zero(stats);
    if (stats != 0) {
        stats->meshes = scene->mesh_count;
        stats->materials = scene->material_count;
        stats->groups = scene->group_count;
        stats->lights = scene->light_count;
        stats->cameras = scene->camera_count;
        stats->shader_stages = scene->shader_stage_count;
        stats->bones = scene->bone_count;
        stats->anim_channels = scene->anim_channel_count;
        stats->anim_keys = scene->anim_key_count;
        stats->vertices = scene->vertex_count;
        stats->uvs = scene->uv_count;
        stats->faces = scene->face_count;
        stats->indices = scene->index_count;
    }
    for (mi = 0; mi < scene->mesh_count; ++mi) {
        m = &scene->meshes[mi];
        if (m->vertex_count == 0UL || m->face_count == 0UL) {
            if (stats != 0) {
                ++stats->empty_meshes;
            }
        }
        if (m->first_vertex + m->vertex_count > scene->vertex_count ||
            m->first_uv + m->uv_count > scene->uv_count ||
            m->first_face + m->face_count > scene->face_count) {
            if (stats != 0) {
                ++stats->invalid_refs;
            }
            continue;
        }
        for (fi = 0; fi < m->face_count; ++fi) {
            f = &scene->faces[m->first_face + fi];
            if ((f->flags & COB_FACE_HOLE) != 0U) {
                if (stats != 0) {
                    ++stats->hole_faces;
                }
            }
            if (f->index_count < 3U) {
                if (stats != 0) {
                    ++stats->degenerate_faces;
                }
            } else if ((f->flags & COB_FACE_HOLE) == 0U) {
                local_triangles = (unsigned long)f->index_count - 2UL;
                if (stats != 0) {
                    stats->triangles += (cob_u32)local_triangles;
                }
            }
            if (f->first_index + f->index_count > scene->index_count) {
                if (stats != 0) {
                    ++stats->invalid_refs;
                }
                continue;
            }
            for (ri = 0; ri < f->index_count; ++ri) {
                vr = &scene->indices[f->first_index + ri];
                if (!cobx_ref_valid(scene, m, vr)) {
                    if (stats != 0) {
                        ++stats->invalid_refs;
                    }
                }
            }
        }
    }
    if (stats != 0 && stats->invalid_refs != 0UL) {
        return COB_ERR_PARSE;
    }
    return COB_OK;
}

int cob_scene_repair(cob_scene *scene, unsigned int flags)
{
    unsigned long mi;
    unsigned long fi;
    unsigned long ri;
    cob_mesh *m;
    cob_face *f;
    cob_vertex_ref *vr;
    char tmp[COB_NAME_MAX];

    if (scene == 0) {
        return COB_ERR_ARGUMENT;
    }
    if ((flags & COB_REPAIR_ADD_DEFAULT_UV) != 0U && scene->uv_count == 0UL) {
        if (scene->uv_count + 1UL > COB_MAX_UVS) {
            return COB_ERR_CAPACITY;
        }
        scene->uvs[0].u = 0;
        scene->uvs[0].v = 0;
        scene->uv_count = 1UL;
        for (mi = 0; mi < scene->mesh_count; ++mi) {
            scene->meshes[mi].first_uv = 0UL;
            scene->meshes[mi].uv_count = 1UL;
        }
    }
    for (mi = 0; mi < scene->mesh_count; ++mi) {
        m = &scene->meshes[mi];
        if ((flags & COB_REPAIR_FILL_EMPTY_NAMES) != 0U && m->name[0] == '\0') {
            sprintf(tmp, "Mesh_%lu", mi);
            cob_copy_name(m->name, sizeof(m->name), tmp, cob_strlen_ul(tmp));
        }
        if (m->first_face + m->face_count > scene->face_count) {
            if (m->first_face >= scene->face_count) {
                m->face_count = 0UL;
            } else {
                m->face_count = scene->face_count - m->first_face;
            }
        }
        if (m->first_vertex + m->vertex_count > scene->vertex_count) {
            if (m->first_vertex >= scene->vertex_count) {
                m->vertex_count = 0UL;
            } else {
                m->vertex_count = scene->vertex_count - m->first_vertex;
            }
        }
        if (m->first_uv + m->uv_count > scene->uv_count) {
            if (m->first_uv >= scene->uv_count) {
                m->uv_count = 0UL;
            } else {
                m->uv_count = scene->uv_count - m->first_uv;
            }
        }
        for (fi = 0; fi < m->face_count; ++fi) {
            f = &scene->faces[m->first_face + fi];
            if (f->first_index + f->index_count > scene->index_count) {
                if (f->first_index >= scene->index_count) {
                    f->index_count = 0U;
                } else {
                    f->index_count = (cob_u16)(scene->index_count - f->first_index);
                }
            }
            if ((flags & COB_REPAIR_CLAMP_INDICES) != 0U) {
                for (ri = 0; ri < f->index_count; ++ri) {
                    vr = &scene->indices[f->first_index + ri];
                    if (m->vertex_count == 0UL) {
                        vr->pos_index = 0;
                    } else if (vr->pos_index < 0) {
                        vr->pos_index = 0;
                    } else if ((unsigned long)vr->pos_index >= m->vertex_count) {
                        vr->pos_index = (cob_i32)(m->vertex_count - 1UL);
                    }
                    if (m->uv_count == 0UL) {
                        vr->uv_index = 0;
                    } else if (vr->uv_index < 0) {
                        vr->uv_index = 0;
                    } else if ((unsigned long)vr->uv_index >= m->uv_count) {
                        vr->uv_index = (cob_i32)(m->uv_count - 1UL);
                    }
                }
            }
        }
    }
    return COB_OK;
}

void cob_transform_vec3(cob_vec3q *out, const cob_fixed transform[16], const cob_vec3q *in)
{
    cob_fixed x;
    cob_fixed y;
    cob_fixed z;

    if (out == 0 || transform == 0 || in == 0) {
        return;
    }
    x = cobx_sat_add(cobx_sat_add(cobx_mul(transform[0], in->x), cobx_mul(transform[1], in->y)), cobx_mul(transform[2], in->z));
    x = cobx_sat_add(x, transform[3]);
    y = cobx_sat_add(cobx_sat_add(cobx_mul(transform[4], in->x), cobx_mul(transform[5], in->y)), cobx_mul(transform[6], in->z));
    y = cobx_sat_add(y, transform[7]);
    z = cobx_sat_add(cobx_sat_add(cobx_mul(transform[8], in->x), cobx_mul(transform[9], in->y)), cobx_mul(transform[10], in->z));
    z = cobx_sat_add(z, transform[11]);
    out->x = x;
    out->y = y;
    out->z = z;
}

int cob_scene_make_world_copy(cob_scene *dst, const cob_scene *src)
{
    unsigned long mi;
    unsigned long vi;
    cob_mesh *m;
    cob_vec3q tmp;

    if (dst == 0 || src == 0) {
        return COB_ERR_ARGUMENT;
    }
    *dst = *src;
    for (mi = 0; mi < dst->mesh_count; ++mi) {
        m = &dst->meshes[mi];
        if (m->first_vertex + m->vertex_count > dst->vertex_count) {
            return COB_ERR_PARSE;
        }
        for (vi = 0; vi < m->vertex_count; ++vi) {
            cob_transform_vec3(&tmp, m->transform, &dst->vertices[m->first_vertex + vi]);
            dst->vertices[m->first_vertex + vi] = tmp;
        }
        cob_set_identity(m->transform);
    }
    return COB_OK;
}

static void cobx_bounds_include(cob_bounds *b, const cob_vec3q *v)
{
    if (b == 0 || v == 0) {
        return;
    }
    if (!b->valid) {
        b->valid = 1;
        b->minv = *v;
        b->maxv = *v;
        return;
    }
    if (v->x < b->minv.x) b->minv.x = v->x;
    if (v->y < b->minv.y) b->minv.y = v->y;
    if (v->z < b->minv.z) b->minv.z = v->z;
    if (v->x > b->maxv.x) b->maxv.x = v->x;
    if (v->y > b->maxv.y) b->maxv.y = v->y;
    if (v->z > b->maxv.z) b->maxv.z = v->z;
}

int cob_mesh_compute_bounds(const cob_scene *scene, unsigned long mesh_index, cob_bounds *bounds)
{
    const cob_mesh *m;
    unsigned long i;

    if (scene == 0 || bounds == 0) {
        return COB_ERR_ARGUMENT;
    }
    memset(bounds, 0, sizeof(*bounds));
    if (mesh_index >= scene->mesh_count) {
        return COB_ERR_ARGUMENT;
    }
    m = &scene->meshes[mesh_index];
    if (m->first_vertex + m->vertex_count > scene->vertex_count) {
        return COB_ERR_PARSE;
    }
    for (i = 0; i < m->vertex_count; ++i) {
        cobx_bounds_include(bounds, &scene->vertices[m->first_vertex + i]);
    }
    return COB_OK;
}

int cob_scene_compute_bounds(const cob_scene *scene, cob_bounds *bounds)
{
    unsigned long i;

    if (scene == 0 || bounds == 0) {
        return COB_ERR_ARGUMENT;
    }
    memset(bounds, 0, sizeof(*bounds));
    for (i = 0; i < scene->vertex_count; ++i) {
        cobx_bounds_include(bounds, &scene->vertices[i]);
    }
    return COB_OK;
}

int cob_scene_build_triangles(const cob_scene *scene, cob_triangle *out_triangles, unsigned long triangle_capacity, unsigned long *out_count)
{
    return cob_scene_build_triangles_ex(scene, out_triangles, triangle_capacity, out_count, 0);
}

static cob_vec3q cobx_cross_from_points(const cob_vec3q *a, const cob_vec3q *b, const cob_vec3q *c)
{
    cob_vec3q ab;
    cob_vec3q ac;
    cob_vec3q n;
    unsigned long maxv;

    ab.x = b->x - a->x;
    ab.y = b->y - a->y;
    ab.z = b->z - a->z;
    ac.x = c->x - a->x;
    ac.y = c->y - a->y;
    ac.z = c->z - a->z;
    n.x = cobx_sat_add(cobx_mul(ab.y, ac.z), -cobx_mul(ab.z, ac.y));
    n.y = cobx_sat_add(cobx_mul(ab.z, ac.x), -cobx_mul(ab.x, ac.z));
    n.z = cobx_sat_add(cobx_mul(ab.x, ac.y), -cobx_mul(ab.y, ac.x));
    maxv = cobx_abs_fixed(n.x);
    if (cobx_abs_fixed(n.y) > maxv) maxv = cobx_abs_fixed(n.y);
    if (cobx_abs_fixed(n.z) > maxv) maxv = cobx_abs_fixed(n.z);
    if (maxv != 0UL) {
        n.x = cobx_div_mag_to_fixed(n.x, maxv);
        n.y = cobx_div_mag_to_fixed(n.y, maxv);
        n.z = cobx_div_mag_to_fixed(n.z, maxv);
    }
    return n;
}

int cob_scene_build_face_normals(const cob_scene *scene, cob_vec3q *out_normals, unsigned long normal_capacity)
{
    unsigned long mi;
    unsigned long fi;
    const cob_mesh *m;
    const cob_face *f;
    const cob_vertex_ref *r0;
    const cob_vertex_ref *r1;
    const cob_vertex_ref *r2;
    cob_vec3q zero;

    if (scene == 0 || out_normals == 0) {
        return COB_ERR_ARGUMENT;
    }
    if (normal_capacity < scene->face_count) {
        return COB_ERR_CAPACITY;
    }
    zero.x = zero.y = zero.z = 0;
    for (fi = 0; fi < scene->face_count; ++fi) {
        out_normals[fi] = zero;
    }
    for (mi = 0; mi < scene->mesh_count; ++mi) {
        m = &scene->meshes[mi];
        if (m->first_face + m->face_count > scene->face_count) {
            return COB_ERR_PARSE;
        }
        for (fi = 0; fi < m->face_count; ++fi) {
            f = &scene->faces[m->first_face + fi];
            if ((f->flags & COB_FACE_HOLE) != 0U || f->index_count < 3U) {
                continue;
            }
            if (f->first_index + f->index_count > scene->index_count) {
                return COB_ERR_PARSE;
            }
            r0 = &scene->indices[f->first_index + 0UL];
            r1 = &scene->indices[f->first_index + 1UL];
            r2 = &scene->indices[f->first_index + 2UL];
            if (!cobx_ref_valid(scene, m, r0) || !cobx_ref_valid(scene, m, r1) || !cobx_ref_valid(scene, m, r2)) {
                return COB_ERR_PARSE;
            }
            out_normals[m->first_face + fi] = cobx_cross_from_points(
                &scene->vertices[m->first_vertex + (unsigned long)r0->pos_index],
                &scene->vertices[m->first_vertex + (unsigned long)r1->pos_index],
                &scene->vertices[m->first_vertex + (unsigned long)r2->pos_index]);
        }
    }
    return COB_OK;
}

static int cobx_mtl_name(cob_write_fn fn, void *user, long matnum)
{
    int st;
    st = cob_emit(fn, user, "mat_");
    if (st != COB_OK) return st;
    return cob_emit_i(fn, user, matnum);
}

int cob_export_mtl(const cob_scene *scene, cob_write_fn write_fn, void *user)
{
    unsigned long i;
    const cob_material *mat;
    int st;

    if (scene == 0 || write_fn == 0) {
        return COB_ERR_ARGUMENT;
    }
    st = cob_emit(write_fn, user, "# MTL exported by cob_chiqui3d extras\n");
    if (st != COB_OK) return st;
    for (i = 0; i < scene->material_count; ++i) {
        mat = &scene->materials[i];
        st = cob_emit(write_fn, user, "newmtl ");
        if (st != COB_OK) return st;
        st = cobx_mtl_name(write_fn, user, (long)mat->matnum);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\nKd ");
        if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, mat->rgb[0], 6);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " ");
        if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, mat->rgb[1], 6);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " ");
        if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, mat->rgb[2], 6);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\nd ");
        if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, mat->alpha, 6);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\nKs ");
        if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, mat->ks, 6);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " ");
        if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, mat->ks, 6);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " ");
        if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, mat->ks, 6);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\nNs ");
        if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, mat->exp, 4);
        if (st != COB_OK) return st;
        if (mat->texture_color[0] != '\0') {
            st = cob_emit(write_fn, user, "\nmap_Kd ");
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, mat->texture_color);
            if (st != COB_OK) return st;
        }
        if (mat->texture_bump[0] != '\0') {
            st = cob_emit(write_fn, user, "\nmap_Bump ");
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, mat->texture_bump);
            if (st != COB_OK) return st;
        }
        st = cob_emit(write_fn, user, "\n\n");
        if (st != COB_OK) return st;
    }
    return COB_OK;
}

static long cobx_find_material_number(const cob_scene *scene, cob_i32 mesh_id, cob_i16 face_mat)
{
    unsigned long i;
    const cob_material *mat;

    for (i = 0; i < scene->material_count; ++i) {
        mat = &scene->materials[i];
        if ((mat->parent_id == mesh_id || mesh_id == 0L) && mat->matnum == face_mat) {
            return (long)mat->matnum;
        }
    }
    return (long)face_mat;
}

int cob_export_obj_mtl(const cob_scene *scene, cob_write_fn write_fn, void *user, const char *mtl_name)
{
    unsigned long mi;
    unsigned long i;
    unsigned long fidx;
    unsigned long ridx;
    unsigned long vertex_base;
    unsigned long uv_base;
    long current_mat;
    long mat_to_use;
    const cob_mesh *m;
    const cob_face *f;
    const cob_vertex_ref *vr;
    const cob_vec3q *v;
    const cob_vec2q *uv;
    int st;

    if (scene == 0 || write_fn == 0) {
        return COB_ERR_ARGUMENT;
    }
    st = cob_emit(write_fn, user, "# OBJ exported by cob_chiqui3d extras\n");
    if (st != COB_OK) return st;
    if (mtl_name != 0 && mtl_name[0] != '\0') {
        st = cob_emit(write_fn, user, "mtllib ");
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, mtl_name);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n");
        if (st != COB_OK) return st;
    }
    vertex_base = 1UL;
    uv_base = 1UL;
    for (mi = 0; mi < scene->mesh_count; ++mi) {
        m = &scene->meshes[mi];
        current_mat = -2147483647L;
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
            if ((f->flags & COB_FACE_HOLE) != 0U || f->index_count < 3U) {
                continue;
            }
            mat_to_use = cobx_find_material_number(scene, m->id, f->material);
            if (mat_to_use != current_mat) {
                current_mat = mat_to_use;
                st = cob_emit(write_fn, user, "usemtl ");
                if (st != COB_OK) return st;
                st = cobx_mtl_name(write_fn, user, mat_to_use);
                if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, "\n");
                if (st != COB_OK) return st;
            }
            st = cob_emit(write_fn, user, "f");
            if (st != COB_OK) return st;
            for (ridx = 0; ridx < f->index_count; ++ridx) {
                vr = &scene->indices[f->first_index + ridx];
                st = cob_emit(write_fn, user, " ");
                if (st != COB_OK) return st;
                st = cob_emit_u(write_fn, user, vertex_base + (unsigned long)vr->pos_index);
                if (st != COB_OK) return st;
                if (m->uv_count != 0UL && vr->uv_index >= 0) {
                    st = cob_emit(write_fn, user, "/");
                    if (st != COB_OK) return st;
                    st = cob_emit_u(write_fn, user, uv_base + (unsigned long)vr->uv_index);
                    if (st != COB_OK) return st;
                }
            }
            st = cob_emit(write_fn, user, "\n");
            if (st != COB_OK) return st;
        }
        vertex_base += m->vertex_count;
        uv_base += m->uv_count;
    }
    return COB_OK;
}
