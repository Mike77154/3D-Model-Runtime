#include "cob_private.h"

/* Optional NURBS evaluator/tessellator. C89, fixed point, no heap. */

static cob_fixed nb_basis[COB_MAX_NURBS_CV];
static cob_fixed nb_next[COB_MAX_NURBS_CV];

static unsigned long nb_abs_fixed(cob_fixed v)
{
    if (v == COB_FIXED_MIN) return 2147483648UL;
    if (v < 0) return (unsigned long)(-v);
    return (unsigned long)v;
}

static cob_fixed nb_from_mag_sign(unsigned long mag, int sign)
{
    if (sign < 0) {
        if (mag >= 2147483648UL) return COB_FIXED_MIN;
        return -(cob_fixed)mag;
    }
    if (mag > (unsigned long)COB_FIXED_MAX) return COB_FIXED_MAX;
    return (cob_fixed)mag;
}

static cob_fixed nb_mul(cob_fixed a, cob_fixed b)
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
    if (a < 0) sign = -sign;
    if (b < 0) sign = -sign;
    am = nb_abs_fixed(a);
    bm = nb_abs_fixed(b);
    aw = am >> 16;
    af = am & 65535UL;
    bw = bm >> 16;
    bf = bm & 65535UL;
    if (aw != 0UL && bw > 32767UL / aw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out = (aw * bw) << 16;
    if (aw != 0UL && bf > ((unsigned long)COB_FIXED_MAX - out) / aw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out += aw * bf;
    if (bw != 0UL && af > ((unsigned long)COB_FIXED_MAX - out) / bw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out += bw * af;
    out += (af * bf) >> 16;
    return nb_from_mag_sign(out, sign);
}

static cob_fixed nb_div(cob_fixed num, cob_fixed den)
{
    unsigned long n;
    unsigned long d;
    unsigned long whole;
    unsigned long rem;
    unsigned long out;
    int sign;
    if (den == 0) return 0;
    sign = 1;
    if (num < 0) sign = -sign;
    if (den < 0) sign = -sign;
    n = nb_abs_fixed(num);
    d = nb_abs_fixed(den);
    whole = n / d;
    rem = n % d;
    if (whole > 32767UL) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out = (whole << 16) + ((rem << 16) / d);
    return nb_from_mag_sign(out, sign);
}

static cob_fixed nb_div_u(cob_fixed num, unsigned long den)
{
    unsigned long n;
    unsigned long out;
    int sign;
    if (den == 0UL) return 0;
    sign = num < 0 ? -1 : 1;
    n = nb_abs_fixed(num);
    out = n / den;
    return nb_from_mag_sign(out, sign);
}

static cob_fixed nb_lerp(cob_fixed a, cob_fixed b, unsigned long step, unsigned long steps)
{
    cob_fixed t;
    if (steps == 0UL) return a;
    t = nb_div_u((cob_fixed)step * COB_FIXED_ONE, steps);
    return a + nb_mul(b - a, t);
}

static int nb_build_basis(unsigned int degree, unsigned int cv_count, const cob_fixed *knots, unsigned int knot_count, cob_fixed u, cob_fixed *out)
{
    unsigned int i;
    unsigned int p;
    cob_fixed d1;
    cob_fixed d2;
    cob_fixed a;
    cob_fixed b;
    cob_fixed last;

    if (out == 0 || knots == 0 || cv_count == 0U || cv_count > COB_MAX_NURBS_CV) return COB_ERR_ARGUMENT;
    if (knot_count < cv_count + degree + 1U || knot_count > COB_MAX_NURBS_KNOTS) return COB_ERR_ARGUMENT;
    last = knots[knot_count - 1U];
    for (i = 0U; i < cv_count; ++i) {
        if ((u >= knots[i] && u < knots[i + 1U]) || (u == last && i + 1U == cv_count)) {
            nb_basis[i] = COB_FIXED_ONE;
        } else {
            nb_basis[i] = 0;
        }
    }
    for (p = 1U; p <= degree; ++p) {
        for (i = 0U; i < cv_count; ++i) {
            a = 0;
            b = 0;
            d1 = knots[i + p] - knots[i];
            d2 = knots[i + p + 1U] - knots[i + 1U];
            if (d1 != 0) {
                a = nb_mul(nb_div(u - knots[i], d1), nb_basis[i]);
            }
            if (d2 != 0 && i + 1U < cv_count) {
                b = nb_mul(nb_div(knots[i + p + 1U] - u, d2), nb_basis[i + 1U]);
            }
            nb_next[i] = a + b;
        }
        for (i = 0U; i < cv_count; ++i) {
            nb_basis[i] = nb_next[i];
        }
    }
    for (i = 0U; i < cv_count; ++i) {
        out[i] = nb_basis[i];
    }
    return COB_OK;
}

int cob_nurbs_patch_eval(const cob_nurbs_patch *patch, cob_fixed u, cob_fixed v, cob_vec3q *out_point)
{
    cob_fixed bu[COB_MAX_NURBS_CV];
    cob_fixed bv[COB_MAX_NURBS_CV];
    cob_fixed denom;
    cob_fixed w;
    cob_fixed b;
    cob_fixed bw;
    cob_vec3q acc;
    unsigned int i;
    unsigned int j;
    unsigned long idx;
    int st;

    if (patch == 0 || out_point == 0) return COB_ERR_ARGUMENT;
    if (patch->u_count == 0U || patch->v_count == 0U) return COB_ERR_ARGUMENT;
    if ((unsigned long)patch->u_count * (unsigned long)patch->v_count > COB_MAX_NURBS_CV) return COB_ERR_ARGUMENT;
    st = nb_build_basis(patch->u_degree, patch->u_count, patch->u_knots, patch->u_knot_count, u, bu);
    if (st != COB_OK) return st;
    st = nb_build_basis(patch->v_degree, patch->v_count, patch->v_knots, patch->v_knot_count, v, bv);
    if (st != COB_OK) return st;
    acc.x = 0;
    acc.y = 0;
    acc.z = 0;
    denom = 0;
    for (j = 0U; j < patch->v_count; ++j) {
        for (i = 0U; i < patch->u_count; ++i) {
            idx = (unsigned long)j * (unsigned long)patch->u_count + (unsigned long)i;
            b = nb_mul(bu[i], bv[j]);
            w = patch->rational ? patch->weights[idx] : COB_FIXED_ONE;
            bw = nb_mul(b, w);
            acc.x += nb_mul(patch->control[idx].x, bw);
            acc.y += nb_mul(patch->control[idx].y, bw);
            acc.z += nb_mul(patch->control[idx].z, bw);
            denom += bw;
        }
    }
    if (denom == 0) return COB_ERR_PARSE;
    out_point->x = nb_div(acc.x, denom);
    out_point->y = nb_div(acc.y, denom);
    out_point->z = nb_div(acc.z, denom);
    return COB_OK;
}

static int nb_append_default_uv(cob_scene *scene, cob_mesh *m, cob_fixed u, cob_fixed v)
{
    if (scene->uv_count >= COB_MAX_UVS) return COB_ERR_CAPACITY;
    if (m->uv_count == 0UL) m->first_uv = scene->uv_count;
    scene->uvs[scene->uv_count].u = u;
    scene->uvs[scene->uv_count].v = v;
    ++scene->uv_count;
    ++m->uv_count;
    return COB_OK;
}

int cob_nurbs_patch_tessellate_to_scene(cob_scene *scene, const cob_nurbs_patch *patch, const char *mesh_name, unsigned int u_steps, unsigned int v_steps)
{
    cob_mesh *m;
    unsigned int x;
    unsigned int y;
    unsigned long total_v;
    unsigned long total_f;
    unsigned long idx0;
    unsigned long idx1;
    unsigned long idx2;
    unsigned long idx3;
    cob_fixed u0;
    cob_fixed u1;
    cob_fixed v0;
    cob_fixed v1;
    cob_fixed u;
    cob_fixed v;
    cob_vec3q p;
    cob_face *face;
    int st;

    if (scene == 0 || patch == 0) return COB_ERR_ARGUMENT;
    if (u_steps == 0U || v_steps == 0U) return COB_ERR_ARGUMENT;
    if (scene->mesh_count >= COB_MAX_MESHES) return COB_ERR_CAPACITY;
    total_v = (unsigned long)(u_steps + 1U) * (unsigned long)(v_steps + 1U);
    total_f = (unsigned long)u_steps * (unsigned long)v_steps;
    if (scene->vertex_count + total_v > COB_MAX_VERTICES) return COB_ERR_CAPACITY;
    if (scene->uv_count + total_v > COB_MAX_UVS) return COB_ERR_CAPACITY;
    if (scene->face_count + total_f > COB_MAX_FACES) return COB_ERR_CAPACITY;
    if (scene->index_count + total_f * 4UL > COB_MAX_INDICES) return COB_ERR_CAPACITY;
    if (patch->u_knot_count <= patch->u_degree + 1U || patch->v_knot_count <= patch->v_degree + 1U) return COB_ERR_ARGUMENT;

    m = &scene->meshes[scene->mesh_count];
    memset(m, 0, sizeof(*m));
    cob_zero_axes(m->axes);
    cob_set_identity(m->transform);
    m->id = 1000000L + (cob_i32)scene->mesh_count;
    m->parent_id = 0;
    m->major = 0;
    m->minor = 8;
    m->unit_scale = COB_FIXED_ONE;
    cob_copy_name(m->name, sizeof(m->name), mesh_name != 0 ? mesh_name : "NURBS_patch", cob_strlen_ul(mesh_name != 0 ? mesh_name : "NURBS_patch"));
    m->first_vertex = scene->vertex_count;
    m->first_uv = scene->uv_count;
    m->first_face = scene->face_count;
    ++scene->mesh_count;

    u0 = patch->u_knots[patch->u_degree];
    u1 = patch->u_knots[patch->u_knot_count - patch->u_degree - 1U];
    v0 = patch->v_knots[patch->v_degree];
    v1 = patch->v_knots[patch->v_knot_count - patch->v_degree - 1U];
    for (y = 0U; y <= v_steps; ++y) {
        v = nb_lerp(v0, v1, y, v_steps);
        for (x = 0U; x <= u_steps; ++x) {
            u = nb_lerp(u0, u1, x, u_steps);
            st = cob_nurbs_patch_eval(patch, u, v, &p);
            if (st != COB_OK) return st;
            scene->vertices[scene->vertex_count++] = p;
            st = nb_append_default_uv(scene, m, nb_div_u((cob_fixed)x * COB_FIXED_ONE, u_steps), nb_div_u((cob_fixed)y * COB_FIXED_ONE, v_steps));
            if (st != COB_OK) return st;
            ++m->vertex_count;
        }
    }
    for (y = 0U; y < v_steps; ++y) {
        for (x = 0U; x < u_steps; ++x) {
            idx0 = (unsigned long)y * (unsigned long)(u_steps + 1U) + (unsigned long)x;
            idx1 = idx0 + 1UL;
            idx2 = idx0 + (unsigned long)(u_steps + 1U) + 1UL;
            idx3 = idx0 + (unsigned long)(u_steps + 1U);
            face = &scene->faces[scene->face_count++];
            memset(face, 0, sizeof(*face));
            face->index_count = 4U;
            face->material = 0;
            face->first_index = scene->index_count;
            scene->indices[scene->index_count].pos_index = (cob_i32)idx0;
            scene->indices[scene->index_count++].uv_index = (cob_i32)idx0;
            scene->indices[scene->index_count].pos_index = (cob_i32)idx1;
            scene->indices[scene->index_count++].uv_index = (cob_i32)idx1;
            scene->indices[scene->index_count].pos_index = (cob_i32)idx2;
            scene->indices[scene->index_count++].uv_index = (cob_i32)idx2;
            scene->indices[scene->index_count].pos_index = (cob_i32)idx3;
            scene->indices[scene->index_count++].uv_index = (cob_i32)idx3;
            ++m->face_count;
        }
    }
    return COB_OK;
}
