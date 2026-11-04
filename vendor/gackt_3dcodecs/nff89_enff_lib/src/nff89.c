#include "nff89.h"

#define NFF89_TRUE 1
#define NFF89_FALSE 0

#define NFF89_FIX_MAX 2147483647L
#define NFF89_FIX_MIN (-2147483647L - 1L)

struct nff89_lex_s {
    const char *text;
    long length;
    long pos;
    long line;
    int has_push;
    char push[NFF89_TOKEN_MAX];
    long push_line;
};

typedef struct nff89_lex_s nff89_lex;

static void nff89_copy_str(char *dst, unsigned dst_size, const char *src)
{
    unsigned i;
    if (dst_size == 0U) return;
    i = 0U;
    while (src[i] != '\0' && i + 1U < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int nff89_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static int nff89_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static char nff89_lower(char c)
{
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static int nff89_streq_ci(const char *a, const char *b)
{
    unsigned i;
    i = 0U;
    while (a[i] != '\0' || b[i] != '\0') {
        if (nff89_lower(a[i]) != nff89_lower(b[i])) return NFF89_FALSE;
        i++;
    }
    return NFF89_TRUE;
}

static int nff89_starts_ci(const char *a, const char *prefix)
{
    unsigned i;
    i = 0U;
    while (prefix[i] != '\0') {
        if (nff89_lower(a[i]) != nff89_lower(prefix[i])) return NFF89_FALSE;
        i++;
    }
    return NFF89_TRUE;
}

static void nff89_set_error(nff89_scene *scene, int err, long line, const char *tok)
{
    if (scene != (nff89_scene *)0) {
        scene->diag.last_error = err;
        scene->diag.line = line;
        if (tok != (const char *)0) nff89_copy_str(scene->diag.last_token, NFF89_TOKEN_MAX, tok);
    }
}

static long nff89_sat_add(long a, long b)
{
    if (b > 0L && a > NFF89_FIX_MAX - b) return NFF89_FIX_MAX;
    if (b < 0L && a < NFF89_FIX_MIN - b) return NFF89_FIX_MIN;
    return a + b;
}

static long nff89_sat_mul10(long a)
{
    if (a > NFF89_FIX_MAX / 10L) return NFF89_FIX_MAX;
    if (a < NFF89_FIX_MIN / 10L) return NFF89_FIX_MIN;
    return a * 10L;
}

static long nff89_sat_mul_int(long a, long b)
{
    long i;
    long r;
    int neg;
    if (a == 0L || b == 0L) return 0L;
    neg = NFF89_FALSE;
    if (a < 0L) { a = -a; neg = !neg; }
    if (b < 0L) { b = -b; neg = !neg; }
    r = 0L;
    for (i = 0L; i < b; i++) {
        if (r > NFF89_FIX_MAX - a) return neg ? NFF89_FIX_MIN : NFF89_FIX_MAX;
        r += a;
        if (i > 1000000L) return neg ? NFF89_FIX_MIN : NFF89_FIX_MAX;
    }
    return neg ? -r : r;
}

static void nff89_lex_init(nff89_lex *lx, const char *text, long length)
{
    lx->text = text;
    lx->length = length;
    lx->pos = 0L;
    lx->line = 1L;
    lx->has_push = NFF89_FALSE;
    lx->push[0] = '\0';
    lx->push_line = 1L;
}

static int nff89_lex_get(nff89_lex *lx, char *tok, unsigned tok_size, long *line_out)
{
    unsigned n;
    char c;
    char d;

    if (lx->has_push) {
        lx->has_push = NFF89_FALSE;
        nff89_copy_str(tok, tok_size, lx->push);
        if (line_out != (long *)0) *line_out = lx->push_line;
        return NFF89_TRUE;
    }

    for (;;) {
        if (lx->pos >= lx->length) return NFF89_FALSE;
        c = lx->text[lx->pos];
        if (nff89_is_space(c)) {
            if (c == '\n') lx->line++;
            lx->pos++;
            continue;
        }
        if (c == '#') {
            while (lx->pos < lx->length && lx->text[lx->pos] != '\n') lx->pos++;
            continue;
        }
        if (c == '/' && lx->pos + 1L < lx->length) {
            d = lx->text[lx->pos + 1L];
            if (d == '/') {
                lx->pos += 2L;
                while (lx->pos < lx->length && lx->text[lx->pos] != '\n') lx->pos++;
                continue;
            }
            if (d == '*') {
                lx->pos += 2L;
                while (lx->pos < lx->length) {
                    if (lx->text[lx->pos] == '\n') lx->line++;
                    if (lx->text[lx->pos] == '*' && lx->pos + 1L < lx->length && lx->text[lx->pos + 1L] == '/') {
                        lx->pos += 2L;
                        break;
                    }
                    lx->pos++;
                }
                continue;
            }
        }
        break;
    }

    if (line_out != (long *)0) *line_out = lx->line;
    n = 0U;
    while (lx->pos < lx->length) {
        c = lx->text[lx->pos];
        if (nff89_is_space(c)) break;
        if (c == '#') break;
        if (c == '/' && lx->pos + 1L < lx->length && (lx->text[lx->pos + 1L] == '/' || lx->text[lx->pos + 1L] == '*')) break;
        if (n + 1U < tok_size) {
            tok[n] = c;
            n++;
        }
        lx->pos++;
    }
    if (tok_size > 0U) tok[n] = '\0';
    return n > 0U ? NFF89_TRUE : NFF89_FALSE;
}

static void nff89_lex_unget(nff89_lex *lx, const char *tok, long line)
{
    lx->has_push = NFF89_TRUE;
    nff89_copy_str(lx->push, NFF89_TOKEN_MAX, tok);
    lx->push_line = line;
}

static int nff89_lex_peek(nff89_lex *lx, char *tok, unsigned tok_size, long *line_out)
{
    int ok;
    long line;
    ok = nff89_lex_get(lx, tok, tok_size, &line);
    if (ok) {
        nff89_lex_unget(lx, tok, line);
        if (line_out != (long *)0) *line_out = line;
    }
    return ok;
}

static int nff89_next_req(nff89_scene *scene, nff89_lex *lx, char *tok, long *line)
{
    if (!nff89_lex_get(lx, tok, NFF89_TOKEN_MAX, line)) {
        nff89_set_error(scene, NFF89_ERR_EOF, lx->line, "<eof>");
        return NFF89_ERR_EOF;
    }
    return NFF89_OK;
}

int nff89_parse_int_token(const char *token, long *out_value)
{
    long sign;
    long v;
    unsigned i;
    if (token == (const char *)0 || out_value == (long *)0) return NFF89_ERR_BAD_ARG;
    i = 0U;
    sign = 1L;
    if (token[i] == '+') i++;
    else if (token[i] == '-') { sign = -1L; i++; }
    if (!nff89_is_digit(token[i])) return NFF89_ERR_BAD_TOKEN;
    v = 0L;
    while (nff89_is_digit(token[i])) {
        if (v > 214748364L) return NFF89_ERR_RANGE;
        v = v * 10L + (long)(token[i] - '0');
        i++;
    }
    if (token[i] != '\0') return NFF89_ERR_BAD_TOKEN;
    *out_value = v * sign;
    return NFF89_OK;
}

static int nff89_token_is_number(const char *token)
{
    nff89_fix dummy;
    return nff89_parse_fix_token(token, &dummy) == NFF89_OK;
}

static int nff89_parse_hex(const char *token, unsigned *out)
{
    unsigned i;
    unsigned v;
    unsigned d;
    if (token == (const char *)0 || out == (unsigned *)0) return NFF89_ERR_BAD_ARG;
    i = 0U;
    if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) i = 2U;
    else return NFF89_ERR_BAD_TOKEN;
    if (token[i] == '\0') return NFF89_ERR_BAD_TOKEN;
    v = 0U;
    while (token[i] != '\0') {
        if (token[i] >= '0' && token[i] <= '9') d = (unsigned)(token[i] - '0');
        else if (token[i] >= 'a' && token[i] <= 'f') d = (unsigned)(token[i] - 'a' + 10);
        else if (token[i] >= 'A' && token[i] <= 'F') d = (unsigned)(token[i] - 'A' + 10);
        else return NFF89_ERR_BAD_TOKEN;
        v = (v << 4) | d;
        i++;
    }
    *out = v;
    return NFF89_OK;
}

int nff89_parse_fix_token(const char *token, nff89_fix *out_value)
{
    unsigned i;
    long sign;
    long int_part;
    long frac_part;
    long frac_scale;
    long value;
    long exp_sign;
    long exp_val;
    int got_digit;

    if (token == (const char *)0 || out_value == (nff89_fix *)0) return NFF89_ERR_BAD_ARG;
    i = 0U;
    sign = 1L;
    if (token[i] == '+') i++;
    else if (token[i] == '-') { sign = -1L; i++; }

    int_part = 0L;
    got_digit = NFF89_FALSE;
    while (nff89_is_digit(token[i])) {
        got_digit = NFF89_TRUE;
        if (int_part > 32767L) int_part = 32767L;
        else int_part = int_part * 10L + (long)(token[i] - '0');
        i++;
    }

    frac_part = 0L;
    frac_scale = 1L;
    if (token[i] == '.') {
        i++;
        while (nff89_is_digit(token[i])) {
            got_digit = NFF89_TRUE;
            if (frac_scale < 100000000L) {
                frac_part = frac_part * 10L + (long)(token[i] - '0');
                frac_scale *= 10L;
            }
            i++;
        }
    }

    if (!got_digit) return NFF89_ERR_BAD_TOKEN;

    value = int_part * NFF89_FIX_ONE;
    if (frac_scale > 1L) {
        value = nff89_sat_add(value, (frac_part * NFF89_FIX_ONE) / frac_scale);
    }

    if (token[i] == 'e' || token[i] == 'E') {
        i++;
        exp_sign = 1L;
        if (token[i] == '+') i++;
        else if (token[i] == '-') { exp_sign = -1L; i++; }
        if (!nff89_is_digit(token[i])) return NFF89_ERR_BAD_TOKEN;
        exp_val = 0L;
        while (nff89_is_digit(token[i])) {
            if (exp_val < 1000L) exp_val = exp_val * 10L + (long)(token[i] - '0');
            i++;
        }
        if (exp_val > 64L) exp_val = 64L;
        while (exp_val > 0L) {
            if (exp_sign > 0L) value = nff89_sat_mul10(value);
            else value /= 10L;
            exp_val--;
        }
    }

    if (token[i] != '\0') return NFF89_ERR_BAD_TOKEN;
    if (sign < 0L) value = -value;
    *out_value = value;
    return NFF89_OK;
}

long nff89_fix_to_int(nff89_fix value)
{
    return value / NFF89_FIX_ONE;
}

nff89_fix nff89_int_to_fix(long value)
{
    if (value > NFF89_FIX_MAX / NFF89_FIX_ONE) return NFF89_FIX_MAX;
    if (value < NFF89_FIX_MIN / NFF89_FIX_ONE) return NFF89_FIX_MIN;
    return value * NFF89_FIX_ONE;
}

nff89_fix nff89_fix_mul(nff89_fix a, nff89_fix b)
{
    long ai;
    long af;
    long bi;
    long bf;
    long r;
    int neg;
    neg = NFF89_FALSE;
    if (a < 0L) { a = -a; neg = !neg; }
    if (b < 0L) { b = -b; neg = !neg; }
    ai = a / NFF89_FIX_ONE;
    af = a % NFF89_FIX_ONE;
    bi = b / NFF89_FIX_ONE;
    bf = b % NFF89_FIX_ONE;
    r = nff89_sat_mul_int(ai, bi) * NFF89_FIX_ONE;
    r = nff89_sat_add(r, ai * bf);
    r = nff89_sat_add(r, bi * af);
    r = nff89_sat_add(r, (af * bf) / NFF89_FIX_ONE);
    return neg ? -r : r;
}

nff89_fix nff89_fix_div(nff89_fix a, nff89_fix b)
{
    long q;
    long r;
    long out;
    int neg;
    if (b == 0L) return a < 0L ? NFF89_FIX_MIN : NFF89_FIX_MAX;
    neg = NFF89_FALSE;
    if (a < 0L) { a = -a; neg = !neg; }
    if (b < 0L) { b = -b; neg = !neg; }
    q = a / b;
    r = a % b;
    if (q > NFF89_FIX_MAX / NFF89_FIX_ONE) out = NFF89_FIX_MAX;
    else out = q * NFF89_FIX_ONE + (r * NFF89_FIX_ONE) / b;
    return neg ? -out : out;
}

void nff89_fix_to_string(nff89_fix value, char *dst, unsigned dst_size, unsigned decimals)
{
    char tmp[32];
    char rev[32];
    unsigned n;
    unsigned i;
    long intp;
    long frac;
    long scale;
    int neg;

    if (dst_size == 0U) return;
    dst[0] = '\0';
    if (decimals > 5U) decimals = 5U;
    neg = NFF89_FALSE;
    if (value < 0L) { neg = NFF89_TRUE; value = -value; }
    intp = value / NFF89_FIX_ONE;
    frac = value % NFF89_FIX_ONE;
    n = 0U;
    if (neg && n + 1U < dst_size) dst[n++] = '-';
    i = 0U;
    if (intp == 0L) rev[i++] = '0';
    while (intp > 0L && i < sizeof(rev)) {
        rev[i++] = (char)('0' + (int)(intp % 10L));
        intp /= 10L;
    }
    while (i > 0U && n + 1U < dst_size) dst[n++] = rev[--i];
    if (decimals > 0U && n + 1U < dst_size) {
        dst[n++] = '.';
        scale = 10L;
        for (i = 0U; i < decimals; i++) {
            long digit;
            digit = (frac * scale) / NFF89_FIX_ONE;
            if (n + 1U < dst_size) dst[n++] = (char)('0' + (int)(digit % 10L));
            scale *= 10L;
        }
    }
    dst[n] = '\0';
    (void)tmp;
}

static void nff89_default_material(nff89_material *m)
{
    m->color.r = NFF89_FIX_ONE;
    m->color.g = NFF89_FIX_ONE;
    m->color.b = NFF89_FIX_ONE;
    m->kd = NFF89_FIX_ONE;
    m->ks = 0L;
    m->shine = NFF89_FIX_ONE;
    m->transmittance = 0L;
    m->ior = NFF89_FIX_ONE;
    m->ka = 13107L; /* 0.2 in Q16.16 */
    m->has_ka = NFF89_FALSE;
}

void nff89_init_scene(nff89_scene *scene)
{
    if (scene == (nff89_scene *)0) return;
    scene->format = NFF89_FORMAT_UNKNOWN;
    scene->has_view = NFF89_FALSE;
    scene->has_background = NFF89_FALSE;
    scene->background.r = 0L;
    scene->background.g = 0L;
    scene->background.b = 0L;
    scene->view.from.x = scene->view.from.y = scene->view.from.z = 0L;
    scene->view.at.x = scene->view.at.y = 0L; scene->view.at.z = NFF89_FIX_ONE;
    scene->view.up.x = 0L; scene->view.up.y = NFF89_FIX_ONE; scene->view.up.z = 0L;
    scene->view.angle = 45L * NFF89_FIX_ONE;
    scene->view.hither = NFF89_FIX_ONE;
    scene->view.xres = 0L;
    scene->view.yres = 0L;
    nff89_default_material(&scene->current_material);
    scene->lights = (nff89_light *)0; scene->lights_count = 0U; scene->lights_max = 0U;
    scene->spheres = (nff89_sphere *)0; scene->spheres_count = 0U; scene->spheres_max = 0U;
    scene->cones = (nff89_cone *)0; scene->cones_count = 0U; scene->cones_max = 0U;
    scene->polys = (nff89_poly *)0; scene->polys_count = 0U; scene->polys_max = 0U;
    scene->beziers = (nff89_bezier *)0; scene->beziers_count = 0U; scene->beziers_max = 0U;
    scene->nurbs = (nff89_nurbs *)0; scene->nurbs_count = 0U; scene->nurbs_max = 0U;
    scene->objects = (nff89_object *)0; scene->objects_count = 0U; scene->objects_max = 0U;
    scene->diag.line = 0L;
    scene->diag.last_token[0] = '\0';
    scene->diag.last_error = NFF89_OK;
    scene->diag.warnings = 0U;
    scene->diag.overflow_count = 0U;
    scene->diag.skipped_unknown = 0U;
    scene->diag.truncated_polygons = 0U;
    scene->diag.truncated_curves = 0U;
}

void nff89_set_buffers(nff89_scene *scene,
                       nff89_light *lights, unsigned lights_max,
                       nff89_sphere *spheres, unsigned spheres_max,
                       nff89_cone *cones, unsigned cones_max,
                       nff89_poly *polys, unsigned polys_max,
                       nff89_bezier *beziers, unsigned beziers_max,
                       nff89_nurbs *nurbs, unsigned nurbs_max,
                       nff89_object *objects, unsigned objects_max)
{
    if (scene == (nff89_scene *)0) return;
    scene->lights = lights; scene->lights_max = lights_max;
    scene->spheres = spheres; scene->spheres_max = spheres_max;
    scene->cones = cones; scene->cones_max = cones_max;
    scene->polys = polys; scene->polys_max = polys_max;
    scene->beziers = beziers; scene->beziers_max = beziers_max;
    scene->nurbs = nurbs; scene->nurbs_max = nurbs_max;
    scene->objects = objects; scene->objects_max = objects_max;
}

void nff89_reset_counts(nff89_scene *scene)
{
    if (scene == (nff89_scene *)0) return;
    scene->format = NFF89_FORMAT_UNKNOWN;
    scene->has_view = NFF89_FALSE;
    scene->has_background = NFF89_FALSE;
    scene->lights_count = 0U;
    scene->spheres_count = 0U;
    scene->cones_count = 0U;
    scene->polys_count = 0U;
    scene->beziers_count = 0U;
    scene->nurbs_count = 0U;
    scene->objects_count = 0U;
    scene->diag.line = 0L;
    scene->diag.last_token[0] = '\0';
    scene->diag.last_error = NFF89_OK;
    scene->diag.warnings = 0U;
    scene->diag.overflow_count = 0U;
    scene->diag.skipped_unknown = 0U;
    scene->diag.truncated_polygons = 0U;
    scene->diag.truncated_curves = 0U;
    nff89_default_material(&scene->current_material);
}

static int nff89_read_fix(nff89_scene *scene, nff89_lex *lx, nff89_fix *out)
{
    char tok[NFF89_TOKEN_MAX];
    long line;
    int r;
    r = nff89_next_req(scene, lx, tok, &line);
    if (r != NFF89_OK) return r;
    r = nff89_parse_fix_token(tok, out);
    if (r != NFF89_OK) {
        nff89_set_error(scene, r, line, tok);
        return r;
    }
    return NFF89_OK;
}

static int nff89_read_int(nff89_scene *scene, nff89_lex *lx, long *out)
{
    char tok[NFF89_TOKEN_MAX];
    long line;
    int r;
    r = nff89_next_req(scene, lx, tok, &line);
    if (r != NFF89_OK) return r;
    r = nff89_parse_int_token(tok, out);
    if (r != NFF89_OK) {
        nff89_set_error(scene, r, line, tok);
        return r;
    }
    return NFF89_OK;
}

static int nff89_read_vec3(nff89_scene *scene, nff89_lex *lx, nff89_vec3 *v)
{
    int r;
    r = nff89_read_fix(scene, lx, &v->x); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &v->y); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &v->z); return r;
}

static int nff89_read_color(nff89_scene *scene, nff89_lex *lx, nff89_color *c)
{
    int r;
    r = nff89_read_fix(scene, lx, &c->r); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &c->g); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &c->b); return r;
}

static nff89_light *nff89_add_light(nff89_scene *scene)
{
    if (scene->lights_count >= scene->lights_max || scene->lights == (nff89_light *)0) {
        scene->diag.overflow_count++;
        return (nff89_light *)0;
    }
    return &scene->lights[scene->lights_count++];
}

static nff89_sphere *nff89_add_sphere(nff89_scene *scene)
{
    if (scene->spheres_count >= scene->spheres_max || scene->spheres == (nff89_sphere *)0) {
        scene->diag.overflow_count++;
        return (nff89_sphere *)0;
    }
    return &scene->spheres[scene->spheres_count++];
}

static nff89_cone *nff89_add_cone(nff89_scene *scene)
{
    if (scene->cones_count >= scene->cones_max || scene->cones == (nff89_cone *)0) {
        scene->diag.overflow_count++;
        return (nff89_cone *)0;
    }
    return &scene->cones[scene->cones_count++];
}

static nff89_poly *nff89_add_poly(nff89_scene *scene)
{
    if (scene->polys_count >= scene->polys_max || scene->polys == (nff89_poly *)0) {
        scene->diag.overflow_count++;
        return (nff89_poly *)0;
    }
    return &scene->polys[scene->polys_count++];
}

static nff89_bezier *nff89_add_bezier(nff89_scene *scene)
{
    if (scene->beziers_count >= scene->beziers_max || scene->beziers == (nff89_bezier *)0) {
        scene->diag.overflow_count++;
        return (nff89_bezier *)0;
    }
    return &scene->beziers[scene->beziers_count++];
}

static nff89_nurbs *nff89_add_nurbs(nff89_scene *scene)
{
    if (scene->nurbs_count >= scene->nurbs_max || scene->nurbs == (nff89_nurbs *)0) {
        scene->diag.overflow_count++;
        return (nff89_nurbs *)0;
    }
    return &scene->nurbs[scene->nurbs_count++];
}

static nff89_object *nff89_add_object(nff89_scene *scene)
{
    if (scene->objects_count >= scene->objects_max || scene->objects == (nff89_object *)0) {
        scene->diag.overflow_count++;
        return (nff89_object *)0;
    }
    return &scene->objects[scene->objects_count++];
}

static void nff89_zero_poly(nff89_poly *p)
{
    unsigned i;
    p->vertex_count = 0U;
    for (i = 0U; i < NFF89_MAX_POLY_VERTS; i++) {
        p->vertices[i].x = p->vertices[i].y = p->vertices[i].z = 0L;
        p->normals[i].x = p->normals[i].y = p->normals[i].z = 0L;
    }
    p->flags = 0U;
    nff89_default_material(&p->mat);
    p->color12 = 0U;
    p->color24 = 0U;
    p->texture[0] = '\0';
    p->texture_type = NFF89_TEX_PLAIN;
    p->tex_rot = 0L;
    p->tex_scale = NFF89_FIX_ONE;
    p->tex_trans_u = 0L;
    p->tex_trans_v = 0L;
    p->tex_mirror = NFF89_FALSE;
    p->id = 0L;
    p->portal[0] = '\0';
    p->object_index = -1L;
    p->line = 0L;
}

static int nff89_parse_view(nff89_scene *scene, nff89_lex *lx)
{
    char tok[NFF89_TOKEN_MAX];
    long line;
    long iv;
    int r;
    r = nff89_next_req(scene, lx, tok, &line); if (r != NFF89_OK) return r;
    if (!nff89_streq_ci(tok, "from")) { nff89_set_error(scene, NFF89_ERR_FORMAT, line, tok); return NFF89_ERR_FORMAT; }
    r = nff89_read_vec3(scene, lx, &scene->view.from); if (r != NFF89_OK) return r;
    r = nff89_next_req(scene, lx, tok, &line); if (r != NFF89_OK) return r;
    if (!nff89_streq_ci(tok, "at")) { nff89_set_error(scene, NFF89_ERR_FORMAT, line, tok); return NFF89_ERR_FORMAT; }
    r = nff89_read_vec3(scene, lx, &scene->view.at); if (r != NFF89_OK) return r;
    r = nff89_next_req(scene, lx, tok, &line); if (r != NFF89_OK) return r;
    if (!nff89_streq_ci(tok, "up")) { nff89_set_error(scene, NFF89_ERR_FORMAT, line, tok); return NFF89_ERR_FORMAT; }
    r = nff89_read_vec3(scene, lx, &scene->view.up); if (r != NFF89_OK) return r;
    r = nff89_next_req(scene, lx, tok, &line); if (r != NFF89_OK) return r;
    if (!nff89_streq_ci(tok, "angle")) { nff89_set_error(scene, NFF89_ERR_FORMAT, line, tok); return NFF89_ERR_FORMAT; }
    r = nff89_read_fix(scene, lx, &scene->view.angle); if (r != NFF89_OK) return r;
    r = nff89_next_req(scene, lx, tok, &line); if (r != NFF89_OK) return r;
    if (!nff89_streq_ci(tok, "hither")) { nff89_set_error(scene, NFF89_ERR_FORMAT, line, tok); return NFF89_ERR_FORMAT; }
    r = nff89_read_fix(scene, lx, &scene->view.hither); if (r != NFF89_OK) return r;
    r = nff89_next_req(scene, lx, tok, &line); if (r != NFF89_OK) return r;
    if (!nff89_streq_ci(tok, "resolution")) { nff89_set_error(scene, NFF89_ERR_FORMAT, line, tok); return NFF89_ERR_FORMAT; }
    r = nff89_read_int(scene, lx, &iv); if (r != NFF89_OK) return r; scene->view.xres = iv;
    r = nff89_read_int(scene, lx, &iv); if (r != NFF89_OK) return r; scene->view.yres = iv;
    scene->has_view = NFF89_TRUE;
    return NFF89_OK;
}

static int nff89_parse_light(nff89_scene *scene, nff89_lex *lx, long start_line)
{
    nff89_light tmp;
    nff89_light *dst;
    char tok[NFF89_TOKEN_MAX];
    long line;
    nff89_fix nums[4];
    unsigned extra;
    int r;

    tmp.pos.x = tmp.pos.y = tmp.pos.z = 0L;
    tmp.intensity = NFF89_FIX_ONE;
    tmp.color.r = tmp.color.g = tmp.color.b = NFF89_FIX_ONE;
    tmp.flags = 0U;
    tmp.line = start_line;
    r = nff89_read_vec3(scene, lx, &tmp.pos); if (r != NFF89_OK) return r;

    extra = 0U;
    while (extra < 4U && nff89_lex_get(lx, tok, NFF89_TOKEN_MAX, &line)) {
        if (line != start_line || !nff89_token_is_number(tok)) {
            nff89_lex_unget(lx, tok, line);
            break;
        }
        r = nff89_parse_fix_token(tok, &nums[extra]);
        if (r != NFF89_OK) return r;
        extra++;
    }
    if (extra == 3U) {
        tmp.color.r = nums[0]; tmp.color.g = nums[1]; tmp.color.b = nums[2];
        tmp.flags |= NFF89_LIGHT_HAS_COLOR;
    } else if (extra == 4U) {
        tmp.intensity = nums[0];
        tmp.color.r = nums[1]; tmp.color.g = nums[2]; tmp.color.b = nums[3];
        tmp.flags |= NFF89_LIGHT_HAS_COLOR | NFF89_LIGHT_HAS_INTENSITY;
        scene->format = NFF89_FORMAT_ENFF;
    } else if (extra != 0U) {
        nff89_set_error(scene, NFF89_ERR_FORMAT, start_line, "l");
        return NFF89_ERR_FORMAT;
    }
    dst = nff89_add_light(scene);
    if (dst != (nff89_light *)0) *dst = tmp;
    return NFF89_OK;
}

static int nff89_parse_material(nff89_scene *scene, nff89_lex *lx, long start_line)
{
    int r;
    char tok[NFF89_TOKEN_MAX];
    long line;
    nff89_material m;
    nff89_default_material(&m);
    r = nff89_read_color(scene, lx, &m.color); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &m.kd); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &m.ks); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &m.shine); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &m.transmittance); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &m.ior); if (r != NFF89_OK) return r;
    if (nff89_lex_get(lx, tok, NFF89_TOKEN_MAX, &line)) {
        if (line == start_line && nff89_token_is_number(tok)) {
            r = nff89_parse_fix_token(tok, &m.ka);
            if (r != NFF89_OK) return r;
            m.has_ka = NFF89_TRUE;
            scene->format = NFF89_FORMAT_ENFF;
        } else {
            nff89_lex_unget(lx, tok, line);
        }
    }
    scene->current_material = m;
    return NFF89_OK;
}

static int nff89_parse_sphere_item(nff89_scene *scene, nff89_lex *lx, long start_line)
{
    nff89_sphere tmp;
    nff89_sphere *dst;
    int r;
    tmp.mat = scene->current_material;
    tmp.line = start_line;
    r = nff89_read_vec3(scene, lx, &tmp.center); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &tmp.radius); if (r != NFF89_OK) return r;
    dst = nff89_add_sphere(scene);
    if (dst != (nff89_sphere *)0) *dst = tmp;
    return NFF89_OK;
}

static int nff89_parse_cone_item(nff89_scene *scene, nff89_lex *lx, long start_line)
{
    nff89_cone tmp;
    nff89_cone *dst;
    int r;
    tmp.mat = scene->current_material;
    tmp.line = start_line;
    r = nff89_read_vec3(scene, lx, &tmp.base); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &tmp.base_radius); if (r != NFF89_OK) return r;
    r = nff89_read_vec3(scene, lx, &tmp.apex); if (r != NFF89_OK) return r;
    r = nff89_read_fix(scene, lx, &tmp.apex_radius); if (r != NFF89_OK) return r;
    dst = nff89_add_cone(scene);
    if (dst != (nff89_cone *)0) *dst = tmp;
    return NFF89_OK;
}

static int nff89_parse_poly_item(nff89_scene *scene, nff89_lex *lx, long start_line, unsigned patch)
{
    long nlong;
    unsigned n;
    unsigned i;
    nff89_poly tmp;
    nff89_poly *dst;
    int r;
    r = nff89_read_int(scene, lx, &nlong); if (r != NFF89_OK) return r;
    if (nlong < 3L) { nff89_set_error(scene, NFF89_ERR_FORMAT, start_line, "p/pp"); return NFF89_ERR_FORMAT; }
    n = (unsigned)nlong;
    nff89_zero_poly(&tmp);
    tmp.flags = patch ? NFF89_POLY_PATCH : 0U;
    tmp.mat = scene->current_material;
    tmp.line = start_line;
    tmp.vertex_count = n;
    if (n > (unsigned)NFF89_MAX_POLY_VERTS) {
        tmp.vertex_count = (unsigned)NFF89_MAX_POLY_VERTS;
        scene->diag.truncated_polygons++;
    }
    for (i = 0U; i < n; i++) {
        nff89_vec3 pos;
        nff89_vec3 norm;
        r = nff89_read_vec3(scene, lx, &pos); if (r != NFF89_OK) return r;
        if (patch) {
            r = nff89_read_vec3(scene, lx, &norm); if (r != NFF89_OK) return r;
        } else {
            norm.x = norm.y = norm.z = 0L;
        }
        if (i < (unsigned)NFF89_MAX_POLY_VERTS) {
            tmp.vertices[i] = pos;
            tmp.normals[i] = norm;
        }
    }
    dst = nff89_add_poly(scene);
    if (dst != (nff89_poly *)0) *dst = tmp;
    return NFF89_OK;
}

static int nff89_parse_bezier(nff89_scene *scene, nff89_lex *lx, long start_line)
{
    long ud;
    long vd;
    unsigned count;
    unsigned i;
    nff89_bezier tmp;
    nff89_bezier *dst;
    int r;
    r = nff89_read_int(scene, lx, &ud); if (r != NFF89_OK) return r;
    r = nff89_read_int(scene, lx, &vd); if (r != NFF89_OK) return r;
    if (ud < 0L || vd < 0L || ud > 31L || vd > 31L) { nff89_set_error(scene, NFF89_ERR_RANGE, start_line, "pb"); return NFF89_ERR_RANGE; }
    count = (unsigned)((ud + 1L) * (vd + 1L));
    tmp.u_degree = (int)ud;
    tmp.v_degree = (int)vd;
    tmp.cp_count = count;
    if (count > (unsigned)NFF89_MAX_BEZIER_CP) {
        tmp.cp_count = (unsigned)NFF89_MAX_BEZIER_CP;
        scene->diag.truncated_curves++;
    }
    tmp.mat = scene->current_material;
    tmp.line = start_line;
    for (i = 0U; i < count; i++) {
        nff89_vec3 v;
        r = nff89_read_vec3(scene, lx, &v); if (r != NFF89_OK) return r;
        if (i < (unsigned)NFF89_MAX_BEZIER_CP) tmp.cp[i] = v;
    }
    dst = nff89_add_bezier(scene);
    if (dst != (nff89_bezier *)0) *dst = tmp;
    scene->format = NFF89_FORMAT_ENFF;
    return NFF89_OK;
}

static int nff89_parse_nurbs(nff89_scene *scene, nff89_lex *lx, long start_line, unsigned rational)
{
    long ud;
    long vd;
    long u_count;
    long v_count;
    long u_points;
    long v_points;
    unsigned i;
    unsigned cp_total;
    nff89_nurbs tmp;
    nff89_nurbs *dst;
    char tok[NFF89_TOKEN_MAX];
    long line;
    int r;

    r = nff89_read_int(scene, lx, &ud); if (r != NFF89_OK) return r;
    r = nff89_read_int(scene, lx, &vd); if (r != NFF89_OK) return r;
    r = nff89_read_int(scene, lx, &u_count); if (r != NFF89_OK) return r;
    r = nff89_read_int(scene, lx, &v_count); if (r != NFF89_OK) return r;
    r = nff89_read_int(scene, lx, &u_points); if (r != NFF89_OK) return r;
    r = nff89_read_int(scene, lx, &v_points); if (r != NFF89_OK) return r;
    if (ud < 0L || vd < 0L || u_count < 0L || v_count < 0L || u_points < 0L || v_points < 0L) {
        nff89_set_error(scene, NFF89_ERR_RANGE, start_line, "pn/pnn");
        return NFF89_ERR_RANGE;
    }

    tmp.flags = rational ? NFF89_NURBS_RATIONAL : 0U;
    tmp.u_degree = (int)ud;
    tmp.v_degree = (int)vd;
    tmp.u_knot_count = (unsigned)u_count;
    tmp.v_knot_count = (unsigned)v_count;
    tmp.u_point_count = (unsigned)u_points;
    tmp.v_point_count = (unsigned)v_points;
    tmp.cp_count = (unsigned)(u_points * v_points);
    tmp.trim_count = 0U;
    tmp.mat = scene->current_material;
    tmp.line = start_line;

    for (i = 0U; i < (unsigned)u_count; i++) {
        nff89_fix f;
        r = nff89_read_fix(scene, lx, &f); if (r != NFF89_OK) return r;
        if (i < (unsigned)NFF89_MAX_NURBS_KNOTS) tmp.u_knots[i] = f;
        else scene->diag.truncated_curves++;
    }
    if (tmp.u_knot_count > (unsigned)NFF89_MAX_NURBS_KNOTS) tmp.u_knot_count = (unsigned)NFF89_MAX_NURBS_KNOTS;

    for (i = 0U; i < (unsigned)v_count; i++) {
        nff89_fix f;
        r = nff89_read_fix(scene, lx, &f); if (r != NFF89_OK) return r;
        if (i < (unsigned)NFF89_MAX_NURBS_KNOTS) tmp.v_knots[i] = f;
        else scene->diag.truncated_curves++;
    }
    if (tmp.v_knot_count > (unsigned)NFF89_MAX_NURBS_KNOTS) tmp.v_knot_count = (unsigned)NFF89_MAX_NURBS_KNOTS;

    cp_total = (unsigned)(u_points * v_points);
    for (i = 0U; i < cp_total; i++) {
        nff89_vec4 cp;
        r = nff89_read_fix(scene, lx, &cp.x); if (r != NFF89_OK) return r;
        r = nff89_read_fix(scene, lx, &cp.y); if (r != NFF89_OK) return r;
        r = nff89_read_fix(scene, lx, &cp.z); if (r != NFF89_OK) return r;
        if (rational) { r = nff89_read_fix(scene, lx, &cp.w); if (r != NFF89_OK) return r; }
        else cp.w = NFF89_FIX_ONE;
        if (i < (unsigned)NFF89_MAX_NURBS_CP) tmp.cp[i] = cp;
        else scene->diag.truncated_curves++;
    }
    if (tmp.cp_count > (unsigned)NFF89_MAX_NURBS_CP) tmp.cp_count = (unsigned)NFF89_MAX_NURBS_CP;

    while (nff89_lex_get(lx, tok, NFF89_TOKEN_MAX, &line)) {
        if (nff89_streq_ci(tok, "pn_end")) break;
        if (!nff89_token_is_number(tok)) {
            nff89_set_error(scene, NFF89_ERR_FORMAT, line, tok);
            return NFF89_ERR_FORMAT;
        } else {
            long ord;
            long knot_count;
            long trim_points;
            nff89_trim *tr;
            r = nff89_parse_int_token(tok, &ord); if (r != NFF89_OK) return r;
            r = nff89_read_int(scene, lx, &knot_count); if (r != NFF89_OK) return r;
            r = nff89_read_int(scene, lx, &trim_points); if (r != NFF89_OK) return r;
            if (tmp.trim_count < (unsigned)NFF89_MAX_TRIMS) {
                tr = &tmp.trims[tmp.trim_count++];
                tr->order = (int)ord;
                tr->knot_count = (unsigned)knot_count;
                tr->cp_count = (unsigned)trim_points;
            } else {
                tr = (nff89_trim *)0;
                scene->diag.truncated_curves++;
            }
            for (i = 0U; i < (unsigned)knot_count; i++) {
                nff89_fix f;
                r = nff89_read_fix(scene, lx, &f); if (r != NFF89_OK) return r;
                if (tr != (nff89_trim *)0 && i < (unsigned)NFF89_MAX_TRIM_KNOTS) tr->knots[i] = f;
            }
            if (tr != (nff89_trim *)0 && tr->knot_count > (unsigned)NFF89_MAX_TRIM_KNOTS) tr->knot_count = (unsigned)NFF89_MAX_TRIM_KNOTS;
            for (i = 0U; i < (unsigned)trim_points; i++) {
                nff89_vec4 cp;
                r = nff89_read_fix(scene, lx, &cp.x); if (r != NFF89_OK) return r;
                r = nff89_read_fix(scene, lx, &cp.y); if (r != NFF89_OK) return r;
                r = nff89_read_fix(scene, lx, &cp.z); if (r != NFF89_OK) return r;
                r = nff89_read_fix(scene, lx, &cp.w); if (r != NFF89_OK) return r;
                if (tr != (nff89_trim *)0 && i < (unsigned)NFF89_MAX_TRIM_CP) tr->cp[i] = cp;
            }
            if (tr != (nff89_trim *)0 && tr->cp_count > (unsigned)NFF89_MAX_TRIM_CP) tr->cp_count = (unsigned)NFF89_MAX_TRIM_CP;
        }
    }
    dst = nff89_add_nurbs(scene);
    if (dst != (nff89_nurbs *)0) *dst = tmp;
    scene->format = NFF89_FORMAT_ENFF;
    return NFF89_OK;
}

static int nff89_skip_line(nff89_lex *lx, long start_line)
{
    char tok[NFF89_TOKEN_MAX];
    long line;
    while (nff89_lex_get(lx, tok, NFF89_TOKEN_MAX, &line)) {
        if (line != start_line) {
            nff89_lex_unget(lx, tok, line);
            break;
        }
    }
    return NFF89_OK;
}

int nff89_parse_haines(nff89_scene *scene, const char *text, long length)
{
    nff89_lex lx;
    char tok[NFF89_TOKEN_MAX];
    long line;
    int r;

    if (scene == (nff89_scene *)0 || text == (const char *)0 || length < 0L) return NFF89_ERR_BAD_ARG;
    nff89_reset_counts(scene);
    scene->format = NFF89_FORMAT_HAINES;
    nff89_lex_init(&lx, text, length);

    while (nff89_lex_get(&lx, tok, NFF89_TOKEN_MAX, &line)) {
        if (nff89_streq_ci(tok, "v")) r = nff89_parse_view(scene, &lx);
        else if (nff89_streq_ci(tok, "b")) { r = nff89_read_color(scene, &lx, &scene->background); scene->has_background = NFF89_TRUE; }
        else if (nff89_streq_ci(tok, "l")) r = nff89_parse_light(scene, &lx, line);
        else if (nff89_streq_ci(tok, "f")) r = nff89_parse_material(scene, &lx, line);
        else if (nff89_streq_ci(tok, "c")) r = nff89_parse_cone_item(scene, &lx, line);
        else if (nff89_streq_ci(tok, "s")) r = nff89_parse_sphere_item(scene, &lx, line);
        else if (nff89_streq_ci(tok, "p")) r = nff89_parse_poly_item(scene, &lx, line, NFF89_FALSE);
        else if (nff89_streq_ci(tok, "pp")) r = nff89_parse_poly_item(scene, &lx, line, NFF89_TRUE);
        else if (nff89_streq_ci(tok, "pb")) r = nff89_parse_bezier(scene, &lx, line);
        else if (nff89_streq_ci(tok, "pn")) r = nff89_parse_nurbs(scene, &lx, line, NFF89_TRUE);
        else if (nff89_streq_ci(tok, "pnn")) r = nff89_parse_nurbs(scene, &lx, line, NFF89_FALSE);
        else {
            scene->diag.skipped_unknown++;
            r = nff89_skip_line(&lx, line);
        }
        if (r != NFF89_OK) return r;
    }
    if (scene->diag.overflow_count > 0U) return NFF89_ERR_CAPACITY;
    return NFF89_OK;
}

static int nff89_token_is_object_header(const char *tok)
{
    if (nff89_streq_ci(tok, "version")) return NFF89_FALSE;
    if (nff89_streq_ci(tok, "viewpos")) return NFF89_FALSE;
    if (nff89_streq_ci(tok, "viewdir")) return NFF89_FALSE;
    if (nff89_streq_ci(tok, "shading=on")) return NFF89_FALSE;
    if (nff89_streq_ci(tok, "shading=off")) return NFF89_FALSE;
    return NFF89_TRUE;
}

static int nff89_parse_sense_vertex(nff89_scene *scene, nff89_lex *lx, nff89_vertex *v, long vline)
{
    char tok[NFF89_TOKEN_MAX];
    long line;
    int r;
    r = nff89_read_vec3(scene, lx, &v->pos); if (r != NFF89_OK) return r;
    v->has_norm = NFF89_FALSE;
    v->auto_norm = NFF89_FALSE;
    v->norm.x = v->norm.y = v->norm.z = 0L;
    if (nff89_lex_get(lx, tok, NFF89_TOKEN_MAX, &line)) {
        if (line == vline && nff89_streq_ci(tok, "norm")) {
            r = nff89_read_vec3(scene, lx, &v->norm); if (r != NFF89_OK) return r;
            v->has_norm = NFF89_TRUE;
        } else if (line == vline && nff89_streq_ci(tok, "N")) {
            v->auto_norm = NFF89_TRUE;
        } else {
            nff89_lex_unget(lx, tok, line);
        }
    }
    return NFF89_OK;
}

static int nff89_color_to_material(unsigned color, nff89_material *m)
{
    unsigned r;
    unsigned g;
    unsigned b;
    nff89_default_material(m);
    if (color <= 0xfffU) {
        r = (color >> 8) & 15U;
        g = (color >> 4) & 15U;
        b = color & 15U;
        m->color.r = (nff89_fix)((long)r * NFF89_FIX_ONE / 15L);
        m->color.g = (nff89_fix)((long)g * NFF89_FIX_ONE / 15L);
        m->color.b = (nff89_fix)((long)b * NFF89_FIX_ONE / 15L);
    } else {
        r = (color >> 16) & 255U;
        g = (color >> 8) & 255U;
        b = color & 255U;
        m->color.r = (nff89_fix)((long)r * NFF89_FIX_ONE / 255L);
        m->color.g = (nff89_fix)((long)g * NFF89_FIX_ONE / 255L);
        m->color.b = (nff89_fix)((long)b * NFF89_FIX_ONE / 255L);
    }
    return NFF89_OK;
}

static int nff89_parse_sense_poly(nff89_scene *scene, nff89_lex *lx,
                                  nff89_vertex *verts, unsigned vert_count,
                                  long object_index, long start_line)
{
    long nlong;
    unsigned n;
    unsigned i;
    long idx;
    char tok[NFF89_TOKEN_MAX];
    long line;
    unsigned color;
    nff89_poly tmp;
    nff89_poly *dst;
    int r;

    r = nff89_read_int(scene, lx, &nlong); if (r != NFF89_OK) return r;
    if (nlong < 3L) { nff89_set_error(scene, NFF89_ERR_FORMAT, start_line, "sense8 polygon"); return NFF89_ERR_FORMAT; }
    n = (unsigned)nlong;
    nff89_zero_poly(&tmp);
    tmp.vertex_count = n;
    if (n > (unsigned)NFF89_MAX_POLY_VERTS) {
        tmp.vertex_count = (unsigned)NFF89_MAX_POLY_VERTS;
        scene->diag.truncated_polygons++;
    }
    tmp.object_index = object_index;
    tmp.line = start_line;

    for (i = 0U; i < n; i++) {
        r = nff89_read_int(scene, lx, &idx); if (r != NFF89_OK) return r;
        if (i < (unsigned)NFF89_MAX_POLY_VERTS) {
            if (idx < 0L || (unsigned)idx >= vert_count) {
                nff89_set_error(scene, NFF89_ERR_RANGE, start_line, "vertex index");
                return NFF89_ERR_RANGE;
            }
            tmp.vertices[i] = verts[(unsigned)idx].pos;
            tmp.normals[i] = verts[(unsigned)idx].norm;
            if (verts[(unsigned)idx].has_norm) tmp.flags |= NFF89_POLY_PATCH;
            if (verts[(unsigned)idx].auto_norm) tmp.flags |= NFF89_POLY_AUTO_NORMAL;
        }
    }

    r = nff89_next_req(scene, lx, tok, &line); if (r != NFF89_OK) return r;
    r = nff89_parse_hex(tok, &color);
    if (r != NFF89_OK) { nff89_set_error(scene, r, line, tok); return r; }
    tmp.color12 = color & 0xfffU;
    tmp.color24 = color;
    nff89_color_to_material(color, &tmp.mat);

    while (nff89_lex_get(lx, tok, NFF89_TOKEN_MAX, &line)) {
        if (line != start_line) {
            nff89_lex_unget(lx, tok, line);
            break;
        }
        if (nff89_streq_ci(tok, "both")) tmp.flags |= NFF89_POLY_DOUBLE_SIDED;
        else if (tok[0] == '_') {
            tmp.flags |= NFF89_POLY_HAS_TEXTURE;
            if (nff89_starts_ci(tok, "_s_")) tmp.texture_type = NFF89_TEX_SHADED;
            else if (nff89_starts_ci(tok, "_t_")) tmp.texture_type = NFF89_TEX_TRANSPARENT;
            else tmp.texture_type = NFF89_TEX_PLAIN;
            nff89_copy_str(tmp.texture, NFF89_NAME_MAX, tok);
        } else if (nff89_streq_ci(tok, "rot")) {
            r = nff89_read_fix(scene, lx, &tmp.tex_rot); if (r != NFF89_OK) return r;
        } else if (nff89_streq_ci(tok, "scale")) {
            r = nff89_read_fix(scene, lx, &tmp.tex_scale); if (r != NFF89_OK) return r;
        } else if (nff89_streq_ci(tok, "trans")) {
            r = nff89_read_fix(scene, lx, &tmp.tex_trans_u); if (r != NFF89_OK) return r;
            r = nff89_read_fix(scene, lx, &tmp.tex_trans_v); if (r != NFF89_OK) return r;
        } else if (nff89_streq_ci(tok, "mirror")) {
            tmp.tex_mirror = NFF89_TRUE;
        } else if (nff89_starts_ci(tok, "id=")) {
            r = nff89_parse_int_token(tok + 3, &tmp.id);
            if (r != NFF89_OK) { nff89_set_error(scene, r, line, tok); return r; }
            tmp.flags |= NFF89_POLY_HAS_ID;
        } else if (tok[0] == '-') {
            tmp.flags |= NFF89_POLY_HAS_PORTAL;
            nff89_copy_str(tmp.portal, NFF89_NAME_MAX, tok + 1);
        } else {
            scene->diag.skipped_unknown++;
        }
    }

    dst = nff89_add_poly(scene);
    if (dst != (nff89_poly *)0) *dst = tmp;
    return NFF89_OK;
}

int nff89_parse_sense8(nff89_scene *scene, const char *text, long length,
                       nff89_vertex *scratch_vertices,
                       unsigned scratch_vertices_max)
{
    nff89_lex lx;
    char tok[NFF89_TOKEN_MAX];
    long line;
    long num;
    int r;

    if (scene == (nff89_scene *)0 || text == (const char *)0 || length < 0L) return NFF89_ERR_BAD_ARG;
    if (scratch_vertices == (nff89_vertex *)0 || scratch_vertices_max == 0U) return NFF89_ERR_BAD_ARG;
    nff89_reset_counts(scene);
    scene->format = NFF89_FORMAT_SENSE8;
    nff89_lex_init(&lx, text, length);

    r = nff89_next_req(scene, &lx, tok, &line); if (r != NFF89_OK) return r;
    if (!nff89_streq_ci(tok, "nff")) { nff89_set_error(scene, NFF89_ERR_FORMAT, line, tok); return NFF89_ERR_FORMAT; }

    while (nff89_lex_get(&lx, tok, NFF89_TOKEN_MAX, &line)) {
        if (nff89_streq_ci(tok, "version")) {
            r = nff89_next_req(scene, &lx, tok, &line); if (r != NFF89_OK) return r;
        } else if (nff89_streq_ci(tok, "viewpos")) {
            r = nff89_read_vec3(scene, &lx, &scene->view.from); if (r != NFF89_OK) return r;
            scene->has_view = NFF89_TRUE;
        } else if (nff89_streq_ci(tok, "viewdir")) {
            r = nff89_read_vec3(scene, &lx, &scene->view.at); if (r != NFF89_OK) return r;
            scene->has_view = NFF89_TRUE;
        } else if (nff89_token_is_object_header(tok)) {
            nff89_object objtmp;
            nff89_object *obj;
            unsigned obj_index;
            unsigned vc;
            unsigned pc;
            unsigned i;
            nff89_copy_str(objtmp.name, NFF89_NAME_MAX, tok);
            objtmp.shading_on = NFF89_TRUE;
            objtmp.first_poly = scene->polys_count;
            objtmp.poly_count = 0U;
            objtmp.line = line;

            if (nff89_lex_get(&lx, tok, NFF89_TOKEN_MAX, &line)) {
                if (nff89_streq_ci(tok, "shading=on")) objtmp.shading_on = NFF89_TRUE;
                else if (nff89_streq_ci(tok, "shading=off")) objtmp.shading_on = NFF89_FALSE;
                else nff89_lex_unget(&lx, tok, line);
            }

            r = nff89_read_int(scene, &lx, &num); if (r != NFF89_OK) return r;
            if (num < 0L) { nff89_set_error(scene, NFF89_ERR_RANGE, line, "vertex count"); return NFF89_ERR_RANGE; }
            vc = (unsigned)num;
            if (vc > scratch_vertices_max) {
                scene->diag.overflow_count++;
                nff89_set_error(scene, NFF89_ERR_CAPACITY, line, "scratch vertices");
                return NFF89_ERR_CAPACITY;
            }
            for (i = 0U; i < vc; i++) {
                long vline;
                if (!nff89_lex_peek(&lx, tok, NFF89_TOKEN_MAX, &vline)) { nff89_set_error(scene, NFF89_ERR_EOF, lx.line, "vertex"); return NFF89_ERR_EOF; }
                r = nff89_parse_sense_vertex(scene, &lx, &scratch_vertices[i], vline); if (r != NFF89_OK) return r;
            }
            r = nff89_read_int(scene, &lx, &num); if (r != NFF89_OK) return r;
            if (num < 0L) { nff89_set_error(scene, NFF89_ERR_RANGE, line, "polygon count"); return NFF89_ERR_RANGE; }
            pc = (unsigned)num;
            obj = nff89_add_object(scene);
            if (obj != (nff89_object *)0) {
                *obj = objtmp;
                obj_index = scene->objects_count - 1U;
            } else {
                obj_index = 0U;
            }
            for (i = 0U; i < pc; i++) {
                long pline;
                if (!nff89_lex_peek(&lx, tok, NFF89_TOKEN_MAX, &pline)) { nff89_set_error(scene, NFF89_ERR_EOF, lx.line, "polygon"); return NFF89_ERR_EOF; }
                r = nff89_parse_sense_poly(scene, &lx, scratch_vertices, vc, (long)obj_index, pline); if (r != NFF89_OK) return r;
                if (obj != (nff89_object *)0) obj->poly_count++;
            }
        }
    }
    if (scene->diag.overflow_count > 0U) return NFF89_ERR_CAPACITY;
    return NFF89_OK;
}

int nff89_parse(nff89_scene *scene, const char *text, long length)
{
    nff89_lex lx;
    char tok[NFF89_TOKEN_MAX];
    long line;
    static nff89_vertex sense_scratch[256];
    if (scene == (nff89_scene *)0 || text == (const char *)0 || length < 0L) return NFF89_ERR_BAD_ARG;
    nff89_lex_init(&lx, text, length);
    if (!nff89_lex_get(&lx, tok, NFF89_TOKEN_MAX, &line)) return NFF89_ERR_EOF;
    if (nff89_streq_ci(tok, "nff")) return nff89_parse_sense8(scene, text, length, sense_scratch, 256U);
    return nff89_parse_haines(scene, text, length);
}

unsigned nff89_tri_count(const nff89_scene *scene)
{
    unsigned i;
    unsigned out;
    if (scene == (const nff89_scene *)0) return 0U;
    out = 0U;
    for (i = 0U; i < scene->polys_count; i++) {
        if (scene->polys[i].vertex_count >= 3U) out += scene->polys[i].vertex_count - 2U;
    }
    return out;
}

int nff89_emit_triangles(const nff89_scene *scene, nff89_triangle_cb cb, void *user)
{
    unsigned i;
    unsigned j;
    int r;
    if (scene == (const nff89_scene *)0 || cb == (nff89_triangle_cb)0) return NFF89_ERR_BAD_ARG;
    for (i = 0U; i < scene->polys_count; i++) {
        const nff89_poly *p;
        p = &scene->polys[i];
        if (p->vertex_count >= 3U) {
            for (j = 1U; j + 1U < p->vertex_count && j + 1U < (unsigned)NFF89_MAX_POLY_VERTS; j++) {
                r = cb(user, p, &p->vertices[0], &p->vertices[j], &p->vertices[j + 1U]);
                if (r != 0) return r;
            }
        }
    }
    return NFF89_OK;
}

const char *nff89_format_name(int format)
{
    if (format == NFF89_FORMAT_HAINES) return "Haines/SPD NFF";
    if (format == NFF89_FORMAT_SENSE8) return "Sense8/WorldToolKit NFF";
    if (format == NFF89_FORMAT_ENFF) return "ENFF";
    return "unknown";
}

const char *nff89_error_name(int code)
{
    if (code == NFF89_OK) return "ok";
    if (code == NFF89_ERR_BAD_ARG) return "bad argument";
    if (code == NFF89_ERR_BAD_TOKEN) return "bad token";
    if (code == NFF89_ERR_EOF) return "unexpected end of file";
    if (code == NFF89_ERR_FORMAT) return "format error";
    if (code == NFF89_ERR_CAPACITY) return "capacity exceeded";
    if (code == NFF89_ERR_RANGE) return "range error";
    return "unknown error";
}
