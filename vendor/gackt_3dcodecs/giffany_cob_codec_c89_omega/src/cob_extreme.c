#include "cob_private.h"

#include <stdio.h>

#ifndef COB_ADV_TRI_MAX
#define COB_ADV_TRI_MAX 32768UL
#endif

static cob_triangle g_adv_tris[COB_ADV_TRI_MAX];

static unsigned long adv_abs(cob_fixed v)
{
    if (v == COB_FIXED_MIN) return 2147483648UL;
    if (v < 0) return (unsigned long)(-v);
    return (unsigned long)v;
}

static cob_fixed adv_from_mag(unsigned long mag, int sign)
{
    if (sign < 0) {
        if (mag >= 2147483648UL) return COB_FIXED_MIN;
        return -(cob_fixed)mag;
    }
    if (mag > (unsigned long)COB_FIXED_MAX) return COB_FIXED_MAX;
    return (cob_fixed)mag;
}

static cob_fixed adv_add(cob_fixed a, cob_fixed b)
{
    if (b > 0 && a > COB_FIXED_MAX - b) return COB_FIXED_MAX;
    if (b < 0 && a < COB_FIXED_MIN - b) return COB_FIXED_MIN;
    return a + b;
}

static cob_fixed adv_mul(cob_fixed a, cob_fixed b)
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
    am = adv_abs(a);
    bm = adv_abs(b);
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
    return adv_from_mag(out, sign);
}

static cob_fixed adv_div(cob_fixed num, cob_fixed den)
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
    n = adv_abs(num);
    d = adv_abs(den);
    whole = n / d;
    rem = n % d;
    if (whole > 32767UL) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out = (whole << 16) + ((rem << 16) / d);
    return adv_from_mag(out, sign);
}

static void adv_vec_zero(cob_vec3q *v)
{
    v->x = 0;
    v->y = 0;
    v->z = 0;
}

static void adv_vec_add(cob_vec3q *dst, const cob_vec3q *src)
{
    dst->x = adv_add(dst->x, src->x);
    dst->y = adv_add(dst->y, src->y);
    dst->z = adv_add(dst->z, src->z);
}

static cob_vec3q adv_cross_edges(const cob_vec3q *a, const cob_vec3q *b, const cob_vec3q *c)
{
    cob_vec3q ab;
    cob_vec3q ac;
    cob_vec3q n;

    ab.x = b->x - a->x;
    ab.y = b->y - a->y;
    ab.z = b->z - a->z;
    ac.x = c->x - a->x;
    ac.y = c->y - a->y;
    ac.z = c->z - a->z;
    n.x = adv_add(adv_mul(ab.y, ac.z), -adv_mul(ab.z, ac.y));
    n.y = adv_add(adv_mul(ab.z, ac.x), -adv_mul(ab.x, ac.z));
    n.z = adv_add(adv_mul(ab.x, ac.y), -adv_mul(ab.y, ac.x));
    return n;
}

static cob_fixed adv_sqrt_fixed(cob_fixed v)
{
    cob_fixed x;
    cob_fixed nx;
    int i;
    if (v <= 0) return 0;
    x = v > COB_FIXED_ONE ? v : COB_FIXED_ONE;
    for (i = 0; i < 10; ++i) {
        nx = (cob_fixed)((x + adv_div(v, x)) / 2L);
        if (nx == x) break;
        x = nx;
    }
    return x;
}

static void adv_normalize(cob_vec3q *v)
{
    unsigned long ax;
    unsigned long ay;
    unsigned long az;
    unsigned long scale;
    cob_fixed sfix;
    cob_fixed xq;
    cob_fixed yq;
    cob_fixed zq;
    cob_fixed len;
    cob_fixed sum;

    ax = adv_abs(v->x);
    ay = adv_abs(v->y);
    az = adv_abs(v->z);
    scale = ax;
    if (ay > scale) scale = ay;
    if (az > scale) scale = az;
    if (scale == 0UL) {
        v->x = 0;
        v->y = 0;
        v->z = COB_FIXED_ONE;
        return;
    }
    sfix = scale > (unsigned long)COB_FIXED_MAX ? COB_FIXED_MAX : (cob_fixed)scale;
    xq = adv_div(v->x, sfix);
    yq = adv_div(v->y, sfix);
    zq = adv_div(v->z, sfix);
    sum = adv_add(adv_add(adv_mul(xq, xq), adv_mul(yq, yq)), adv_mul(zq, zq));
    len = adv_sqrt_fixed(sum);
    if (len == 0) {
        v->x = 0;
        v->y = 0;
        v->z = COB_FIXED_ONE;
        return;
    }
    v->x = adv_div(xq, len);
    v->y = adv_div(yq, len);
    v->z = adv_div(zq, len);
}

static unsigned long adv_global_vertex_index(const cob_scene *scene, const cob_triangle *t, int corner)
{
    unsigned long mi;
    const cob_mesh *m;
    mi = t->mesh_index;
    if (mi >= scene->mesh_count) return 0UL;
    m = &scene->meshes[mi];
    if (t->refs[corner].pos_index < 0) return m->first_vertex;
    return m->first_vertex + (unsigned long)t->refs[corner].pos_index;
}

int cob_scene_quality_report(const cob_scene *scene, cob_quality_report *report)
{
    cob_triangulate_stats tstats;
    unsigned long tri_count;
    unsigned long i;
    unsigned long rch;
    int st;

    if (scene == 0 || report == 0) return COB_ERR_ARGUMENT;
    memset(report, 0, sizeof(*report));
    report->mesh_count = scene->mesh_count;
    report->face_count = scene->face_count;
    report->shader_stage_count = scene->shader_stage_count;
    report->anim_channel_count = scene->anim_channel_count;
    tri_count = 0UL;
    memset(&tstats, 0, sizeof(tstats));
    st = cob_scene_build_triangles_ex(scene, g_adv_tris, COB_ADV_TRI_MAX, &tri_count, &tstats);
    report->triangle_count = (cob_u32)tri_count;
    report->concave_count = tstats.concave_faces;
    report->hole_count = tstats.hole_groups;
    report->bridge_count = tstats.bridge_count;
    report->failed_triangulations = tstats.failed_faces;
    for (i = 0; i < scene->shader_stage_count; ++i) {
        if (scene->shader_stages[i].shader_class[0] == '\0' && scene->shader_stages[i].shader_name[0] == '\0') {
            ++report->shader_unknown_count;
        }
    }
    for (i = 0; i < scene->anim_channel_count; ++i) {
        if (scene->anim_channels[i].semantic == 2U) ++report->anim_rotation_channels;
    }
    rch = 0UL;
    for (i = 0; i < scene->diagnostic_count; ++i) {
        if (strcmp(scene->diagnostics[i].chunk_type, "Nurb") == 0 || strcmp(scene->diagnostics[i].chunk_type, "NCur") == 0) report->nurbs_ready = 1UL;
        if (strcmp(scene->diagnostics[i].chunk_type, "Mbal") == 0) report->metaball_ready = 1UL;
        rch += 0UL;
    }
    (void)rch;
    if (st != COB_OK && tri_count == 0UL) return st;
    return COB_OK;
}

int cob_scene_build_vertex_normals(const cob_scene *scene, cob_vec3q *out_normals, unsigned long normal_capacity)
{
    unsigned long i;
    unsigned long tri_count;
    unsigned long gi;
    cob_vec3q n;
    int c;
    int st;

    if (scene == 0 || out_normals == 0) return COB_ERR_ARGUMENT;
    if (normal_capacity < scene->vertex_count) return COB_ERR_CAPACITY;
    for (i = 0; i < scene->vertex_count; ++i) adv_vec_zero(&out_normals[i]);
    tri_count = 0UL;
    st = cob_scene_build_triangles_ex(scene, g_adv_tris, COB_ADV_TRI_MAX, &tri_count, 0);
    if (st != COB_OK && tri_count == 0UL) return st;
    for (i = 0; i < tri_count; ++i) {
        n = adv_cross_edges(&g_adv_tris[i].p[0], &g_adv_tris[i].p[1], &g_adv_tris[i].p[2]);
        for (c = 0; c < 3; ++c) {
            gi = adv_global_vertex_index(scene, &g_adv_tris[i], c);
            if (gi < scene->vertex_count) adv_vec_add(&out_normals[gi], &n);
        }
    }
    for (i = 0; i < scene->vertex_count; ++i) adv_normalize(&out_normals[i]);
    return COB_OK;
}

static int adv_emit_vertex_line(cob_write_fn fn, void *user, const char *tag, const cob_vec3q *v)
{
    int st;
    st = cob_emit(fn, user, tag); if (st != COB_OK) return st;
    st = cob_emit_fixed(fn, user, v->x, 6); if (st != COB_OK) return st;
    st = cob_emit(fn, user, " "); if (st != COB_OK) return st;
    st = cob_emit_fixed(fn, user, v->y, 6); if (st != COB_OK) return st;
    st = cob_emit(fn, user, " "); if (st != COB_OK) return st;
    st = cob_emit_fixed(fn, user, v->z, 6); if (st != COB_OK) return st;
    return cob_emit(fn, user, "\n");
}

int cob_export_smooth_tri_obj(const cob_scene *scene, cob_write_fn write_fn, void *user)
{
    static cob_vec3q normals[COB_MAX_VERTICES];
    unsigned long i;
    unsigned long tri_count;
    unsigned long gi;
    int st;

    if (scene == 0 || write_fn == 0) return COB_ERR_ARGUMENT;
    st = cob_scene_build_vertex_normals(scene, normals, COB_MAX_VERTICES);
    if (st != COB_OK) return st;
    tri_count = 0UL;
    st = cob_scene_build_triangles_ex(scene, g_adv_tris, COB_ADV_TRI_MAX, &tri_count, 0);
    if (st != COB_OK && tri_count == 0UL) return st;
    st = cob_emit(write_fn, user, "# cob_chiqui3d smooth triangulated OBJ\n"); if (st != COB_OK) return st;
    for (i = 0; i < scene->vertex_count; ++i) {
        st = adv_emit_vertex_line(write_fn, user, "v ", &scene->vertices[i]);
        if (st != COB_OK) return st;
    }
    for (i = 0; i < scene->uv_count; ++i) {
        st = cob_emit(write_fn, user, "vt "); if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, scene->uvs[i].u, 6); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " "); if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, scene->uvs[i].v, 6); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n"); if (st != COB_OK) return st;
    }
    for (i = 0; i < scene->vertex_count; ++i) {
        st = adv_emit_vertex_line(write_fn, user, "vn ", &normals[i]);
        if (st != COB_OK) return st;
    }
    for (i = 0; i < tri_count; ++i) {
        st = cob_emit(write_fn, user, "f"); if (st != COB_OK) return st;
        gi = adv_global_vertex_index(scene, &g_adv_tris[i], 0);
        st = cob_emit(write_fn, user, " "); if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, gi + 1UL); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "/"); if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, (g_adv_tris[i].refs[0].uv_index >= 0 ? (unsigned long)g_adv_tris[i].refs[0].uv_index : 0UL) + 1UL); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "/"); if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, gi + 1UL); if (st != COB_OK) return st;
        gi = adv_global_vertex_index(scene, &g_adv_tris[i], 1);
        st = cob_emit(write_fn, user, " "); if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, gi + 1UL); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "/"); if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, (g_adv_tris[i].refs[1].uv_index >= 0 ? (unsigned long)g_adv_tris[i].refs[1].uv_index : 0UL) + 1UL); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "/"); if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, gi + 1UL); if (st != COB_OK) return st;
        gi = adv_global_vertex_index(scene, &g_adv_tris[i], 2);
        st = cob_emit(write_fn, user, " "); if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, gi + 1UL); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "/"); if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, (g_adv_tris[i].refs[2].uv_index >= 0 ? (unsigned long)g_adv_tris[i].refs[2].uv_index : 0UL) + 1UL); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "/"); if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, gi + 1UL); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n"); if (st != COB_OK) return st;
    }
    return COB_OK;
}

typedef struct adv_file_writer_s { FILE *f; } adv_file_writer;

static int adv_file_write(void *user, const void *bytes, unsigned long count)
{
    adv_file_writer *w;
    w = (adv_file_writer *)user;
    if (w == 0 || w->f == 0) return 1;
    if (count == 0UL) return 0;
    return fwrite(bytes, 1, (size_t)count, w->f) == (size_t)count ? 0 : 1;
}

int cob_save_smooth_tri_obj_file(const char *path, const cob_scene *scene)
{
    FILE *f;
    adv_file_writer w;
    int st;

    if (path == 0 || scene == 0) return COB_ERR_ARGUMENT;
    f = fopen(path, "wb");
    if (f == 0) return COB_ERR_ARGUMENT;
    w.f = f;
    st = cob_export_smooth_tri_obj(scene, adv_file_write, &w);
    fclose(f);
    return st;
}

static int adv_tolower(int c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int adv_contains(const char *s, const char *needle)
{
    unsigned long i;
    unsigned long j;
    unsigned long ns;
    unsigned long nn;
    if (s == 0 || needle == 0) return 0;
    ns = cob_strlen_ul(s);
    nn = cob_strlen_ul(needle);
    if (nn == 0UL || nn > ns) return 0;
    for (i = 0; i + nn <= ns; ++i) {
        for (j = 0; j < nn; ++j) {
            if (adv_tolower((unsigned char)s[i + j]) != adv_tolower((unsigned char)needle[j])) break;
        }
        if (j == nn) return 1;
    }
    return 0;
}

static cob_fixed adv_clamp01(cob_fixed v)
{
    if (v < 0) return 0;
    if (v > COB_FIXED_ONE) return COB_FIXED_ONE;
    return v;
}

static cob_fixed adv_frac2(cob_fixed v)
{
    unsigned long mag;
    if (v < 0) v = -v;
    mag = adv_abs(v);
    return (cob_fixed)(mag & 65535UL);
}

static cob_fixed adv_checker(const cob_vec2q *uv, cob_fixed scale)
{
    long u;
    long v;
    if (uv == 0) return COB_FIXED_ONE;
    if (scale <= 0) scale = cob_fixed_from_int(8);
    u = cob_fixed_to_int(adv_mul(uv->u, scale));
    v = cob_fixed_to_int(adv_mul(uv->v, scale));
    return ((u + v) & 1L) ? (COB_FIXED_ONE / 3L) : COB_FIXED_ONE;
}

static cob_fixed adv_stripe(const cob_vec2q *uv, cob_fixed scale)
{
    long u;
    if (uv == 0) return COB_FIXED_ONE;
    if (scale <= 0) scale = cob_fixed_from_int(12);
    u = cob_fixed_to_int(adv_mul(uv->u, scale));
    return (u & 1L) ? (COB_FIXED_ONE / 2L) : COB_FIXED_ONE;
}

static void adv_apply_mod(cob_fixed rgba[4], cob_fixed mod)
{
    rgba[0] = adv_clamp01(adv_mul(rgba[0], mod));
    rgba[1] = adv_clamp01(adv_mul(rgba[1], mod));
    rgba[2] = adv_clamp01(adv_mul(rgba[2], mod));
}

int cob_shader_tree_sample_material(const cob_scene *scene, cob_i32 mesh_id, cob_i16 matnum, const cob_vec2q *uv, const cob_vec3q *pos, cob_shader_sample *out_sample)
{
    cob_shader_eval eval;
    const cob_shader_stage *s;
    unsigned long i;
    cob_fixed mod;
    int st;

    if (scene == 0 || out_sample == 0) return COB_ERR_ARGUMENT;
    memset(out_sample, 0, sizeof(*out_sample));
    st = cob_shader_tree_eval_material(scene, mesh_id, matnum, &eval);
    if (st != COB_OK) return st;
    out_sample->rgba[0] = eval.diffuse[0];
    out_sample->rgba[1] = eval.diffuse[1];
    out_sample->rgba[2] = eval.diffuse[2];
    out_sample->rgba[3] = eval.opacity;
    out_sample->roughness = COB_FIXED_ONE - adv_clamp01(eval.specular);
    out_sample->metallic = adv_clamp01(eval.reflection);
    out_sample->normal_delta[2] = eval.bump;
    out_sample->flags = 0UL;
    for (i = 0; i < scene->shader_stage_count; ++i) {
        s = &scene->shader_stages[i];
        mod = COB_FIXED_ONE;
        if (adv_contains(s->shader_name, "checker") || adv_contains(s->shader_name, "check") || adv_contains(s->shader_class, "checker")) {
            mod = adv_checker(uv, s->has_scalar ? s->scalar : cob_fixed_from_int(8));
            out_sample->flags |= 1UL;
        } else if (adv_contains(s->shader_name, "stripe") || adv_contains(s->shader_name, "wrapped")) {
            mod = adv_stripe(uv, s->has_scalar ? s->scalar : cob_fixed_from_int(12));
            out_sample->flags |= 2UL;
        } else if (adv_contains(s->shader_name, "brick")) {
            mod = adv_checker(uv, s->has_scalar ? s->scalar : cob_fixed_from_int(6));
            if (mod < COB_FIXED_ONE) mod += COB_FIXED_ONE / 4L;
            out_sample->flags |= 4UL;
        } else if (adv_contains(s->shader_name, "cube") || adv_contains(s->shader_class, "cube")) {
            if (pos != 0) mod = adv_frac2(pos->x + pos->y + pos->z) > COB_FIXED_HALF ? COB_FIXED_ONE : (COB_FIXED_ONE / 2L);
            out_sample->flags |= 8UL;
        }
        if (mod != COB_FIXED_ONE) adv_apply_mod(out_sample->rgba, mod);
    }
    return COB_OK;
}

static cob_fixed adv_sin_lut(unsigned int idx)
{
    static const cob_fixed lut[7] = { 0L, 16962L, 32768L, 46341L, 56756L, 63303L, 65536L };
    if (idx > 6U) idx = 6U;
    return lut[idx];
}

static cob_fixed adv_sin_deg(cob_fixed deg)
{
    long d;
    long rem;
    unsigned int idx;
    cob_fixed a;
    cob_fixed b;
    cob_fixed t;
    int sign;

    d = cob_fixed_to_int(deg);
    while (d < 0L) d += 360L;
    while (d >= 360L) d -= 360L;
    sign = 1;
    if (d > 180L) {
        d -= 180L;
        sign = -1;
    }
    if (d > 90L) d = 180L - d;
    idx = (unsigned int)(d / 15L);
    rem = d - (long)idx * 15L;
    if (idx >= 6U) return sign < 0 ? -COB_FIXED_ONE : COB_FIXED_ONE;
    a = adv_sin_lut(idx);
    b = adv_sin_lut(idx + 1U);
    t = adv_div(cob_fixed_from_int(rem), cob_fixed_from_int(15));
    a = a + adv_mul(b - a, t);
    return sign < 0 ? -a : a;
}

static cob_fixed adv_cos_deg(cob_fixed deg)
{
    return adv_sin_deg(deg + cob_fixed_from_int(90));
}

static void adv_mat_mul(const cob_fixed a[16], const cob_fixed b[16], cob_fixed out[16])
{
    cob_fixed tmp[16];
    int r;
    int c;
    int k;
    cob_fixed sum;
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            sum = 0;
            for (k = 0; k < 4; ++k) sum = adv_add(sum, adv_mul(a[r * 4 + k], b[k * 4 + c]));
            tmp[r * 4 + c] = sum;
        }
    }
    for (r = 0; r < 16; ++r) out[r] = tmp[r];
}

int cob_anim_make_euler_xyz_matrix(cob_fixed x_deg, cob_fixed y_deg, cob_fixed z_deg, cob_fixed out_transform[16])
{
    cob_fixed rx[16];
    cob_fixed ry[16];
    cob_fixed rz[16];
    cob_fixed tmp[16];
    cob_fixed sx;
    cob_fixed cx;
    cob_fixed sy;
    cob_fixed cy;
    cob_fixed sz;
    cob_fixed cz;

    if (out_transform == 0) return COB_ERR_ARGUMENT;
    cob_set_identity(rx);
    cob_set_identity(ry);
    cob_set_identity(rz);
    sx = adv_sin_deg(x_deg); cx = adv_cos_deg(x_deg);
    sy = adv_sin_deg(y_deg); cy = adv_cos_deg(y_deg);
    sz = adv_sin_deg(z_deg); cz = adv_cos_deg(z_deg);
    rx[5] = cx; rx[6] = -sx; rx[9] = sx; rx[10] = cx;
    ry[0] = cy; ry[2] = sy; ry[8] = -sy; ry[10] = cy;
    rz[0] = cz; rz[1] = -sz; rz[4] = sz; rz[5] = cz;
    adv_mat_mul(ry, rx, tmp);
    adv_mat_mul(rz, tmp, out_transform);
    return COB_OK;
}

int cob_anim_quat_nlerp(const cob_fixed a_xyzw[4], const cob_fixed b_xyzw[4], cob_fixed t, cob_fixed out_xyzw[4])
{
    int i;
    cob_fixed dot;
    cob_fixed bb[4];
    if (a_xyzw == 0 || b_xyzw == 0 || out_xyzw == 0) return COB_ERR_ARGUMENT;
    dot = 0;
    for (i = 0; i < 4; ++i) dot = adv_add(dot, adv_mul(a_xyzw[i], b_xyzw[i]));
    for (i = 0; i < 4; ++i) bb[i] = dot < 0 ? -b_xyzw[i] : b_xyzw[i];
    for (i = 0; i < 4; ++i) out_xyzw[i] = a_xyzw[i] + adv_mul(bb[i] - a_xyzw[i], t);
    {
        cob_vec3q v;
        v.x = out_xyzw[0];
        v.y = out_xyzw[1];
        v.z = out_xyzw[2];
        adv_normalize(&v);
        out_xyzw[0] = v.x;
        out_xyzw[1] = v.y;
        out_xyzw[2] = v.z;
        if (out_xyzw[3] == 0) out_xyzw[3] = COB_FIXED_ONE;
    }
    return COB_OK;
}

int cob_anim_make_quat_matrix(const cob_fixed qv[4], cob_fixed out_transform[16])
{
    cob_fixed x;
    cob_fixed y;
    cob_fixed z;
    cob_fixed w;
    cob_fixed xx;
    cob_fixed yy;
    cob_fixed zz;
    cob_fixed xy;
    cob_fixed xz;
    cob_fixed yz;
    cob_fixed wx;
    cob_fixed wy;
    cob_fixed wz;
    cob_fixed two;

    if (qv == 0 || out_transform == 0) return COB_ERR_ARGUMENT;
    x = qv[0]; y = qv[1]; z = qv[2]; w = qv[3];
    two = COB_FIXED_ONE + COB_FIXED_ONE;
    xx = adv_mul(x, x); yy = adv_mul(y, y); zz = adv_mul(z, z);
    xy = adv_mul(x, y); xz = adv_mul(x, z); yz = adv_mul(y, z);
    wx = adv_mul(w, x); wy = adv_mul(w, y); wz = adv_mul(w, z);
    cob_set_identity(out_transform);
    out_transform[0] = COB_FIXED_ONE - adv_mul(two, yy + zz);
    out_transform[1] = adv_mul(two, xy - wz);
    out_transform[2] = adv_mul(two, xz + wy);
    out_transform[4] = adv_mul(two, xy + wz);
    out_transform[5] = COB_FIXED_ONE - adv_mul(two, xx + zz);
    out_transform[6] = adv_mul(two, yz - wx);
    out_transform[8] = adv_mul(two, xz - wy);
    out_transform[9] = adv_mul(two, yz + wx);
    out_transform[10] = COB_FIXED_ONE - adv_mul(two, xx + yy);
    return COB_OK;
}

int cob_anim_sample_pose(const cob_scene *scene, cob_fixed time, cob_anim_pose *out_poses, unsigned long pose_capacity, unsigned long *out_count)
{
    unsigned long ci;
    unsigned long pi;
    unsigned long n;
    cob_fixed val[4];
    int st;
    cob_i32 target;

    if (scene == 0 || out_poses == 0 || out_count == 0) return COB_ERR_ARGUMENT;
    n = 0UL;
    for (ci = 0; ci < scene->anim_channel_count; ++ci) {
        target = scene->anim_channels[ci].target_id;
        for (pi = 0; pi < n; ++pi) {
            if (out_poses[pi].target_id == target) break;
        }
        if (pi == n) {
            if (n >= pose_capacity) return COB_ERR_CAPACITY;
            out_poses[n].target_id = target;
            cob_set_identity(out_poses[n].transform);
            out_poses[n].applied_channels = 0UL;
            ++n;
        }
        st = cob_anim_sample_channel(scene, ci, time, val);
        if (st != COB_OK) return st;
        st = cob_anim_apply_sample_to_transform(out_poses[pi].transform, scene->anim_channels[ci].semantic, val);
        if (st == COB_OK) ++out_poses[pi].applied_channels;
    }
    *out_count = n;
    return COB_OK;
}

int cob_nurbs_patch_validate(const cob_nurbs_patch *patch)
{
    unsigned long need;
    unsigned long i;
    if (patch == 0) return COB_ERR_ARGUMENT;
    if (patch->u_degree == 0U || patch->v_degree == 0U) return COB_ERR_ARGUMENT;
    if (patch->u_count == 0U || patch->v_count == 0U) return COB_ERR_ARGUMENT;
    if (patch->u_count * patch->v_count > COB_MAX_NURBS_CV) return COB_ERR_CAPACITY;
    need = (unsigned long)patch->u_count + (unsigned long)patch->u_degree + 1UL;
    if (patch->u_knot_count < need || patch->u_knot_count > COB_MAX_NURBS_KNOTS) return COB_ERR_PARSE;
    need = (unsigned long)patch->v_count + (unsigned long)patch->v_degree + 1UL;
    if (patch->v_knot_count < need || patch->v_knot_count > COB_MAX_NURBS_KNOTS) return COB_ERR_PARSE;
    for (i = 1UL; i < patch->u_knot_count; ++i) if (patch->u_knots[i] < patch->u_knots[i - 1UL]) return COB_ERR_PARSE;
    for (i = 1UL; i < patch->v_knot_count; ++i) if (patch->v_knots[i] < patch->v_knots[i - 1UL]) return COB_ERR_PARSE;
    return COB_OK;
}

static void adv_fill_open_uniform(cob_fixed *knots, cob_u16 count, cob_u16 degree, cob_u16 cv_count)
{
    cob_u16 i;
    cob_u16 last;
    cob_u16 inner;
    cob_u16 denom;
    last = (cob_u16)(count - 1U);
    inner = (cob_u16)(cv_count - degree);
    denom = inner == 0U ? 1U : inner;
    for (i = 0U; i < count; ++i) {
        if (i <= degree) knots[i] = 0;
        else if (i >= (cob_u16)(cv_count)) knots[i] = COB_FIXED_ONE;
        else knots[i] = adv_div(cob_fixed_from_int((long)(i - degree)), cob_fixed_from_int((long)denom));
    }
    knots[last] = COB_FIXED_ONE;
}

int cob_nurbs_patch_make_open_uniform(cob_nurbs_patch *patch, cob_u16 u_degree, cob_u16 v_degree, cob_u16 u_count, cob_u16 v_count)
{
    unsigned long need_u;
    unsigned long need_v;
    unsigned long i;
    if (patch == 0) return COB_ERR_ARGUMENT;
    if (u_degree == 0U || v_degree == 0U || u_count <= u_degree || v_count <= v_degree) return COB_ERR_ARGUMENT;
    if ((unsigned long)u_count * (unsigned long)v_count > COB_MAX_NURBS_CV) return COB_ERR_CAPACITY;
    need_u = (unsigned long)u_count + (unsigned long)u_degree + 1UL;
    need_v = (unsigned long)v_count + (unsigned long)v_degree + 1UL;
    if (need_u > COB_MAX_NURBS_KNOTS || need_v > COB_MAX_NURBS_KNOTS) return COB_ERR_CAPACITY;
    memset(patch, 0, sizeof(*patch));
    patch->u_degree = u_degree;
    patch->v_degree = v_degree;
    patch->u_count = u_count;
    patch->v_count = v_count;
    patch->u_knot_count = (cob_u16)need_u;
    patch->v_knot_count = (cob_u16)need_v;
    patch->rational = 1U;
    adv_fill_open_uniform(patch->u_knots, patch->u_knot_count, u_degree, u_count);
    adv_fill_open_uniform(patch->v_knots, patch->v_knot_count, v_degree, v_count);
    for (i = 0UL; i < (unsigned long)u_count * (unsigned long)v_count; ++i) patch->weights[i] = COB_FIXED_ONE;
    return COB_OK;
}

int cob_nurbs_patch_prepare_ts_bezier_like(cob_nurbs_patch *patch, cob_u16 u_count, cob_u16 v_count)
{
    if (u_count < 6U || v_count < 6U) return COB_ERR_ARGUMENT;
    if (((u_count - 6U) % 3U) != 0U || ((v_count - 6U) % 3U) != 0U) return COB_ERR_ARGUMENT;
    return cob_nurbs_patch_make_open_uniform(patch, 3U, 3U, u_count, v_count);
}

int cob_metaball_field_normal(const cob_metaball_field *field, const cob_vec3q *p, cob_vec3q *out_normal)
{
    cob_vec3q a;
    cob_vec3q b;
    cob_fixed eps;
    if (field == 0 || p == 0 || out_normal == 0) return COB_ERR_ARGUMENT;
    eps = COB_FIXED_ONE / 64L;
    a = *p; b = *p; a.x -= eps; b.x += eps; out_normal->x = cob_metaball_field_eval(field, &b) - cob_metaball_field_eval(field, &a);
    a = *p; b = *p; a.y -= eps; b.y += eps; out_normal->y = cob_metaball_field_eval(field, &b) - cob_metaball_field_eval(field, &a);
    a = *p; b = *p; a.z -= eps; b.z += eps; out_normal->z = cob_metaball_field_eval(field, &b) - cob_metaball_field_eval(field, &a);
    adv_normalize(out_normal);
    return COB_OK;
}

int cob_metaball_field_polygonize_smooth_to_scene(cob_scene *scene, const cob_metaball_field *field, const char *mesh_name, unsigned int grid)
{
    int st;
    unsigned long vi;
    cob_vec3q n;
    cob_fixed f;
    cob_fixed diff;
    if (scene == 0 || field == 0) return COB_ERR_ARGUMENT;
    st = cob_metaball_field_polygonize_to_scene(scene, field, mesh_name, grid);
    if (st != COB_OK) return st;
    for (vi = 0UL; vi < scene->vertex_count; ++vi) {
        st = cob_metaball_field_normal(field, &scene->vertices[vi], &n);
        if (st != COB_OK) return st;
        f = cob_metaball_field_eval(field, &scene->vertices[vi]);
        diff = adv_div(f - field->threshold, cob_fixed_from_int(8));
        scene->vertices[vi].x = scene->vertices[vi].x - adv_mul(n.x, diff);
        scene->vertices[vi].y = scene->vertices[vi].y - adv_mul(n.y, diff);
        scene->vertices[vi].z = scene->vertices[vi].z - adv_mul(n.z, diff);
    }
    return COB_OK;
}
