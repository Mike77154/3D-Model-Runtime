#include "cob_private.h"

/* Fixed-point shader-tree evaluator for decoded Mat1 + ShBx summaries. */

static int sh_tolower(int c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int sh_contains_ci(const char *s, const char *needle)
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
            if (sh_tolower((unsigned char)s[i + j]) != sh_tolower((unsigned char)needle[j])) {
                break;
            }
        }
        if (j == nn) return 1;
    }
    return 0;
}

static cob_fixed sh_clamp01(cob_fixed v)
{
    if (v < 0) return 0;
    if (v > COB_FIXED_ONE) return COB_FIXED_ONE;
    return v;
}

static cob_fixed sh_avg3(const cob_fixed v[3])
{
    return (cob_fixed)((v[0] + v[1] + v[2]) / 3L);
}

static const cob_material *sh_find_material(const cob_scene *scene, cob_i32 mesh_id, cob_i16 matnum)
{
    unsigned long i;
    const cob_material *fallback;
    fallback = 0;
    for (i = 0; i < scene->material_count; ++i) {
        if (scene->materials[i].matnum == matnum) {
            if (scene->materials[i].parent_id == mesh_id) {
                return &scene->materials[i];
            }
            if (fallback == 0) fallback = &scene->materials[i];
        }
    }
    return fallback;
}

static int sh_stage_applies(const cob_shader_stage *s, const cob_material *m, cob_i32 mesh_id)
{
    if (s == 0 || m == 0) return 0;
    if (s->parent_id == m->id) return 1;
    if (s->parent_id == m->parent_id) return 1;
    if (s->parent_id == mesh_id) return 1;
    return 0;
}

static void sh_set_color(cob_fixed dst[3], const cob_fixed src[3])
{
    dst[0] = sh_clamp01(src[0]);
    dst[1] = sh_clamp01(src[1]);
    dst[2] = sh_clamp01(src[2]);
}

int cob_shader_tree_eval_material(const cob_scene *scene, cob_i32 mesh_id, cob_i16 matnum, cob_shader_eval *out_eval)
{
    const cob_material *m;
    const cob_shader_stage *s;
    unsigned long i;
    cob_fixed scalar;

    if (scene == 0 || out_eval == 0) return COB_ERR_ARGUMENT;
    memset(out_eval, 0, sizeof(*out_eval));
    out_eval->diffuse[0] = COB_FIXED_ONE;
    out_eval->diffuse[1] = COB_FIXED_ONE;
    out_eval->diffuse[2] = COB_FIXED_ONE;
    out_eval->opacity = COB_FIXED_ONE;
    out_eval->ambient = 0;
    out_eval->specular = 0;
    out_eval->shininess = 0;
    out_eval->refraction = COB_FIXED_ONE;

    m = sh_find_material(scene, mesh_id, matnum);
    if (m != 0) {
        out_eval->diffuse[0] = sh_clamp01(m->rgb[0]);
        out_eval->diffuse[1] = sh_clamp01(m->rgb[1]);
        out_eval->diffuse[2] = sh_clamp01(m->rgb[2]);
        out_eval->opacity = sh_clamp01(m->alpha);
        out_eval->ambient = sh_clamp01(m->ka);
        out_eval->specular = sh_clamp01(m->ks);
        out_eval->shininess = m->exp;
        out_eval->refraction = m->ior;
    }
    for (i = 0; i < scene->shader_stage_count; ++i) {
        s = &scene->shader_stages[i];
        if (m != 0 && !sh_stage_applies(s, m, mesh_id)) {
            continue;
        }
        scalar = s->has_scalar ? sh_clamp01(s->scalar) : (s->has_color ? sh_clamp01(sh_avg3(s->color)) : COB_FIXED_ONE);
        if (sh_contains_ci(s->shader_class, "color") || sh_contains_ci(s->shader_name, "color") || sh_contains_ci(s->shader_name, "diffuse")) {
            if (s->has_color) sh_set_color(out_eval->diffuse, s->color);
            ++out_eval->applied_stages;
        } else if (sh_contains_ci(s->shader_class, "trans") || sh_contains_ci(s->shader_name, "trans") || sh_contains_ci(s->shader_name, "alpha") || sh_contains_ci(s->shader_name, "opacity")) {
            if (sh_contains_ci(s->shader_name, "opacity")) {
                out_eval->opacity = scalar;
            } else {
                out_eval->opacity = sh_clamp01(COB_FIXED_ONE - scalar);
            }
            ++out_eval->applied_stages;
        } else if (sh_contains_ci(s->shader_class, "reflect") || sh_contains_ci(s->shader_name, "reflect") || sh_contains_ci(s->shader_name, "metal")) {
            out_eval->reflection = scalar;
            if (scalar > out_eval->specular) out_eval->specular = scalar;
            ++out_eval->applied_stages;
        } else if (sh_contains_ci(s->shader_class, "bump") || sh_contains_ci(s->shader_name, "bump") || sh_contains_ci(s->shader_name, "normal")) {
            out_eval->bump = scalar;
            ++out_eval->applied_stages;
        } else if (sh_contains_ci(s->shader_class, "displace") || sh_contains_ci(s->shader_name, "displace")) {
            out_eval->displacement = scalar;
            ++out_eval->applied_stages;
        } else if (sh_contains_ci(s->shader_class, "spec") || sh_contains_ci(s->shader_name, "spec") || sh_contains_ci(s->shader_name, "phong")) {
            out_eval->specular = scalar;
            ++out_eval->applied_stages;
        } else {
            ++out_eval->unknown_stages;
        }
    }
    return COB_OK;
}
