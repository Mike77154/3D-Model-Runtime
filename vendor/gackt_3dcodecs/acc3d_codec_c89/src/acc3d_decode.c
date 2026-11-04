#include "acc3d.h"
#include <string.h>

typedef struct acc3d_tok {
    const char *p;
    size_t n;
} acc3d_tok;

typedef struct acc3d_line_reader {
    const char *text;
    size_t size;
    size_t off;
    long line_no;
} acc3d_line_reader;

typedef struct acc3d_stack_ent {
    long object_index;
    long remaining;
} acc3d_stack_ent;

static int c_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static int tok_eq(acc3d_tok t, const char *s)
{
    size_t i;
    size_t sn;
    sn = strlen(s);
    if (t.n != sn) return 0;
    for (i = 0; i < sn; ++i) if (t.p[i] != s[i]) return 0;
    return 1;
}

static void copy_token(char *dst, size_t cap, acc3d_tok t)
{
    size_t i;
    if (cap == 0) return;
    for (i = 0; i + 1 < cap && i < t.n; ++i) dst[i] = t.p[i];
    dst[i] = '\0';
}

static void trim_line(const char **p, size_t *n)
{
    while (*n > 0 && c_space(**p)) {
        ++(*p);
        --(*n);
    }
    while (*n > 0 && c_space((*p)[*n - 1])) --(*n);
}

static int next_line(acc3d_line_reader *r, const char **p, size_t *n)
{
    size_t start;
    size_t end;
    if (r->off >= r->size) return 0;
    start = r->off;
    end = start;
    while (end < r->size && r->text[end] != '\n') ++end;
    r->off = end;
    if (r->off < r->size && r->text[r->off] == '\n') ++r->off;
    ++r->line_no;
    *p = r->text + start;
    *n = end - start;
    if (*n > 0 && (*p)[*n - 1] == '\r') --(*n);
    return 1;
}

static int read_token(const char **p, const char *end, acc3d_tok *tok)
{
    const char *s;
    while (*p < end && c_space(**p)) ++(*p);
    if (*p >= end) return 0;
    if (**p == '#') return 0;
    if (**p == '"') {
        ++(*p);
        s = *p;
        while (*p < end && **p != '"') ++(*p);
        tok->p = s;
        tok->n = (size_t)(*p - s);
        if (*p < end && **p == '"') ++(*p);
        return 1;
    }
    s = *p;
    while (*p < end && !c_space(**p)) ++(*p);
    tok->p = s;
    tok->n = (size_t)(*p - s);
    return 1;
}

static int parse_long_tok(acc3d_tok t, long *out)
{
    size_t i;
    long sign;
    long v;
    if (out == (long *)0 || t.n == 0) return ACC3D_ERR_PARSE;
    i = 0;
    sign = 1;
    v = 0;
    if (t.p[i] == '-' || t.p[i] == '+') {
        if (t.p[i] == '-') sign = -1;
        ++i;
    }
    if (i >= t.n) return ACC3D_ERR_PARSE;
    while (i < t.n) {
        if (t.p[i] < '0' || t.p[i] > '9') return ACC3D_ERR_PARSE;
        v = v * 10L + (long)(t.p[i] - '0');
        ++i;
    }
    *out = v * sign;
    return ACC3D_OK;
}

static int parse_hex_or_dec_tok(acc3d_tok t, long *out)
{
    size_t i;
    long v;
    int hex;
    if (out == (long *)0 || t.n == 0) return ACC3D_ERR_PARSE;
    i = 0;
    v = 0;
    hex = 0;
    if (t.n > 2 && t.p[0] == '0' && (t.p[1] == 'x' || t.p[1] == 'X')) {
        i = 2;
        hex = 1;
    }
    if (!hex) return parse_long_tok(t, out);
    if (i >= t.n) return ACC3D_ERR_PARSE;
    while (i < t.n) {
        char c;
        long d;
        c = t.p[i];
        if (c >= '0' && c <= '9') d = (long)(c - '0');
        else if (c >= 'a' && c <= 'f') d = 10L + (long)(c - 'a');
        else if (c >= 'A' && c <= 'F') d = 10L + (long)(c - 'A');
        else return ACC3D_ERR_PARSE;
        v = (v << 4) + d;
        ++i;
    }
    *out = v;
    return ACC3D_OK;
}

static int parse_fix_tok(acc3d_tok t, acc3d_fix *out)
{
    return acc3d_parse_fixed(t.p, t.n, out);
}

void acc3d_model_init(struct acc3d_model *m)
{
    long i;
    if (m == (struct acc3d_model *)0) return;
    memset(m, 0, sizeof(*m));
    for (i = 0; i < ACC3D_MAX_OBJECTS; ++i) {
        m->objects[i].parent = -1;
        m->objects[i].rot[0] = ACC3D_FIXED_ONE;
        m->objects[i].rot[4] = ACC3D_FIXED_ONE;
        m->objects[i].rot[8] = ACC3D_FIXED_ONE;
        m->objects[i].texrep.u = ACC3D_FIXED_ONE;
        m->objects[i].texrep.v = ACC3D_FIXED_ONE;
    }
    for (i = 0; i < ACC3D_MAX_SURFACES; ++i) {
        m->surfaces[i].material_index = -1;
    }
    for (i = 0; i < ACC3D_MAX_TRIANGLES; ++i) {
        m->triangles[i].material_index = -1;
    }
}

static int add_material(struct acc3d_model *m, const char *line, size_t n)
{
    const char *p;
    const char *end;
    acc3d_tok t;
    struct acc3d_material *mat;
    int r;

    if (m->material_count >= ACC3D_MAX_MATERIALS) {
        m->diag_bits |= ACC3D_DIAG_LIMIT_HIT;
        return ACC3D_ERR_LIMIT;
    }
    mat = &m->materials[m->material_count];
    p = line;
    end = line + n;
    if (!read_token(&p, end, &t)) return ACC3D_ERR_PARSE; /* MATERIAL */
    if (!read_token(&p, end, &t)) return ACC3D_ERR_PARSE;
    copy_token(mat->name, sizeof(mat->name), t);

    while (read_token(&p, end, &t)) {
        if (tok_eq(t, "rgb")) {
            acc3d_tok a;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->rgb[0]); if (r != ACC3D_OK) return r;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->rgb[1]); if (r != ACC3D_OK) return r;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->rgb[2]); if (r != ACC3D_OK) return r;
        } else if (tok_eq(t, "amb")) {
            acc3d_tok a;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->amb[0]); if (r != ACC3D_OK) return r;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->amb[1]); if (r != ACC3D_OK) return r;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->amb[2]); if (r != ACC3D_OK) return r;
        } else if (tok_eq(t, "emis")) {
            acc3d_tok a;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->emis[0]); if (r != ACC3D_OK) return r;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->emis[1]); if (r != ACC3D_OK) return r;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->emis[2]); if (r != ACC3D_OK) return r;
        } else if (tok_eq(t, "spec")) {
            acc3d_tok a;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->spec[0]); if (r != ACC3D_OK) return r;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->spec[1]); if (r != ACC3D_OK) return r;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->spec[2]); if (r != ACC3D_OK) return r;
        } else if (tok_eq(t, "shi")) {
            acc3d_tok a;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_long_tok(a, &mat->shi); if (r != ACC3D_OK) return r;
        } else if (tok_eq(t, "trans")) {
            acc3d_tok a;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_fix_tok(a, &mat->trans); if (r != ACC3D_OK) return r;
        } else {
            m->diag_bits |= ACC3D_DIAG_UNKNOWN_TOKEN;
        }
    }
    ++m->material_count;
    return ACC3D_OK;
}

static int parse_vector_line(const char *line, size_t n, struct acc3d_vec3 *v)
{
    const char *p;
    const char *end;
    acc3d_tok t;
    int r;
    p = line;
    end = line + n;
    if (!read_token(&p, end, &t)) return ACC3D_ERR_PARSE;
    r = parse_fix_tok(t, &v->x); if (r != ACC3D_OK) return r;
    if (!read_token(&p, end, &t)) return ACC3D_ERR_PARSE;
    r = parse_fix_tok(t, &v->y); if (r != ACC3D_OK) return r;
    if (!read_token(&p, end, &t)) return ACC3D_ERR_PARSE;
    r = parse_fix_tok(t, &v->z); if (r != ACC3D_OK) return r;
    return ACC3D_OK;
}

static int add_object(struct acc3d_model *m, acc3d_tok type, long parent, long *out_index)
{
    struct acc3d_object *ob;
    if (m->object_count >= ACC3D_MAX_OBJECTS) {
        m->diag_bits |= ACC3D_DIAG_LIMIT_HIT;
        return ACC3D_ERR_LIMIT;
    }
    ob = &m->objects[m->object_count];
    memset(ob, 0, sizeof(*ob));
    ob->parent = parent;
    ob->vertex_start = m->vertex_count;
    ob->surface_start = m->surface_count;
    ob->rot[0] = ACC3D_FIXED_ONE;
    ob->rot[4] = ACC3D_FIXED_ONE;
    ob->rot[8] = ACC3D_FIXED_ONE;
    ob->texrep.u = ACC3D_FIXED_ONE;
    ob->texrep.v = ACC3D_FIXED_ONE;
    copy_token(ob->type, sizeof(ob->type), type);
    *out_index = m->object_count;
    ++m->object_count;
    return ACC3D_OK;
}

static int parse_ref_line(struct acc3d_model *m, const char *line, size_t n, long object_index, long *out_ref_index)
{
    const char *p;
    const char *end;
    acc3d_tok t;
    long local_vi;
    long global_vi;
    long extra_i;
    struct acc3d_ref *rr;
    int r;

    if (m->ref_count >= ACC3D_MAX_REFS) {
        m->diag_bits |= ACC3D_DIAG_LIMIT_HIT;
        return ACC3D_ERR_LIMIT;
    }
    p = line;
    end = line + n;
    if (!read_token(&p, end, &t)) return ACC3D_ERR_PARSE;
    r = parse_long_tok(t, &local_vi); if (r != ACC3D_OK) return r;
    if (object_index < 0 || object_index >= m->object_count) return ACC3D_ERR_PARSE;
    if (local_vi < 0 || local_vi >= m->objects[object_index].vertex_count) {
        m->diag_bits |= ACC3D_DIAG_BAD_REFERENCE;
        return ACC3D_ERR_PARSE;
    }
    global_vi = m->objects[object_index].vertex_start + local_vi;
    rr = &m->refs[m->ref_count];
    memset(rr, 0, sizeof(*rr));
    rr->vertex_index = global_vi;

    if (!read_token(&p, end, &t)) return ACC3D_ERR_PARSE;
    r = parse_fix_tok(t, &rr->uv.u); if (r != ACC3D_OK) return r;
    if (!read_token(&p, end, &t)) return ACC3D_ERR_PARSE;
    r = parse_fix_tok(t, &rr->uv.v); if (r != ACC3D_OK) return r;

    extra_i = 0;
    while (read_token(&p, end, &t) && extra_i < 8) {
        r = parse_fix_tok(t, &rr->extra[extra_i]); if (r != ACC3D_OK) return r;
        ++extra_i;
    }
    rr->extra_count = extra_i;
    if (extra_i == 2) {
        rr->uv2.u = rr->extra[0];
        rr->uv2.v = rr->extra[1];
        m->is_acc_like = 1;
        m->diag_bits |= ACC3D_DIAG_ACC_EXT;
    } else if (extra_i >= 3) {
        rr->normal.x = rr->extra[0];
        rr->normal.y = rr->extra[1];
        rr->normal.z = rr->extra[2];
        if (extra_i >= 5) {
            rr->uv2.u = rr->extra[3];
            rr->uv2.v = rr->extra[4];
        }
        m->is_acc_like = 1;
        m->diag_bits |= ACC3D_DIAG_ACC_EXT;
    }

    *out_ref_index = m->ref_count;
    ++m->ref_count;
    return ACC3D_OK;
}

static void close_finished(acc3d_stack_ent *stack, long *stack_n)
{
    while (*stack_n > 0 && stack[*stack_n - 1].remaining <= 0) --(*stack_n);
}

int acc3d_decode_memory(const char *text, size_t size, unsigned long options, struct acc3d_model *model)
{
    acc3d_line_reader rd;
    const char *line;
    size_t line_n;
    const char *p;
    const char *end;
    acc3d_tok t;
    long current_object;
    long current_surface;
    acc3d_stack_ent stack[ACC3D_MAX_STACK];
    long stack_n;
    int r;

    (void)options;
    if (text == (const char *)0 || model == (struct acc3d_model *)0) return ACC3D_ERR_ARG;
    acc3d_model_init(model);
    rd.text = text;
    rd.size = size;
    rd.off = 0;
    rd.line_no = 0;
    current_object = -1;
    current_surface = -1;
    stack_n = 0;

    if (!next_line(&rd, &line, &line_n)) return ACC3D_ERR_HEADER;
    trim_line(&line, &line_n);
    if (line_n < 5 || line[0] != 'A' || line[1] != 'C' || line[2] != '3' || line[3] != 'D') return ACC3D_ERR_HEADER;
    {
        char c;
        c = line[4];
        if (c >= '0' && c <= '9') model->version_hex = (long)(c - '0');
        else if (c >= 'a' && c <= 'f') model->version_hex = 10L + (long)(c - 'a');
        else if (c >= 'A' && c <= 'F') model->version_hex = 10L + (long)(c - 'A');
        else model->version_hex = -1;
    }
    if (line_n > 5) {
        model->header_has_suffix = 1;
        model->diag_bits |= ACC3D_DIAG_TRAILING_HEADER;
        if (line[5] == 'S' || line[5] == 's') {
            model->header_has_strip_suffix = 1;
            model->is_acc_like = 1;
            model->diag_bits |= ACC3D_DIAG_STRIP_HEADER | ACC3D_DIAG_ACC_EXT;
        }
    }

    while (next_line(&rd, &line, &line_n)) {
        model->line = rd.line_no;
        trim_line(&line, &line_n);
        if (line_n == 0) continue;
        if (line[0] == '#') continue;
        p = line;
        end = line + line_n;
        if (!read_token(&p, end, &t)) continue;

        if (tok_eq(t, "MATERIAL")) {
            r = add_material(model, line, line_n);
            if (r != ACC3D_OK) { if (r == ACC3D_ERR_PARSE) model->diag_bits |= ACC3D_DIAG_BAD_NUMBER; return r; }
        } else if (tok_eq(t, "OBJECT")) {
            long parent;
            close_finished(stack, &stack_n);
            parent = (stack_n > 0) ? stack[stack_n - 1].object_index : -1;
            if (stack_n > 0) --stack[stack_n - 1].remaining;
            if (!read_token(&p, end, &t)) return ACC3D_ERR_PARSE;
            r = add_object(model, t, parent, &current_object);
            if (r != ACC3D_OK) return r;
            current_surface = -1;
        } else if (tok_eq(t, "name")) {
            if (current_object >= 0 && read_token(&p, end, &t)) copy_token(model->objects[current_object].name, ACC3D_NAME_LEN, t);
        } else if (tok_eq(t, "texture")) {
            if (current_object >= 0 && read_token(&p, end, &t)) copy_token(model->objects[current_object].texture, ACC3D_PATH_LEN, t);
        } else if (tok_eq(t, "url")) {
            if (current_object >= 0 && read_token(&p, end, &t)) copy_token(model->objects[current_object].url, ACC3D_PATH_LEN, t);
        } else if (tok_eq(t, "data")) {
            long bytes;
            if (read_token(&p, end, &t)) {
                if (parse_long_tok(t, &bytes) == ACC3D_OK) {
                    model->diag_bits |= ACC3D_DIAG_IGNORED_DATA;
                    /* AC3D data is byte-counted; this line-oriented parser skips one payload line. */
                    if (bytes > 0) {
                        const char *dummy;
                        size_t dummy_n;
                        if (!next_line(&rd, &dummy, &dummy_n)) return ACC3D_ERR_PARSE;
                    }
                }
            }
        } else if (tok_eq(t, "texrep")) {
            if (current_object >= 0) {
                acc3d_tok a;
                if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
                r = parse_fix_tok(a, &model->objects[current_object].texrep.u); if (r != ACC3D_OK) return r;
                if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
                r = parse_fix_tok(a, &model->objects[current_object].texrep.v); if (r != ACC3D_OK) return r;
            }
        } else if (tok_eq(t, "loc")) {
            if (current_object >= 0) {
                acc3d_tok a;
                if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
                r = parse_fix_tok(a, &model->objects[current_object].loc.x); if (r != ACC3D_OK) return r;
                if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
                r = parse_fix_tok(a, &model->objects[current_object].loc.y); if (r != ACC3D_OK) return r;
                if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
                r = parse_fix_tok(a, &model->objects[current_object].loc.z); if (r != ACC3D_OK) return r;
            }
        } else if (tok_eq(t, "rot")) {
            if (current_object >= 0) {
                int i;
                acc3d_tok a;
                for (i = 0; i < 9; ++i) {
                    if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
                    r = parse_fix_tok(a, &model->objects[current_object].rot[i]); if (r != ACC3D_OK) return r;
                }
            }
        } else if (tok_eq(t, "numvert")) {
            long count;
            long i;
            acc3d_tok a;
            if (current_object < 0) return ACC3D_ERR_PARSE;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_long_tok(a, &count); if (r != ACC3D_OK) return r;
            if (count < 0 || model->vertex_count + count > ACC3D_MAX_VERTICES) {
                model->diag_bits |= ACC3D_DIAG_LIMIT_HIT;
                return ACC3D_ERR_LIMIT;
            }
            model->objects[current_object].vertex_start = model->vertex_count;
            model->objects[current_object].vertex_count = count;
            for (i = 0; i < count; ++i) {
                if (!next_line(&rd, &line, &line_n)) return ACC3D_ERR_PARSE;
                model->line = rd.line_no;
                trim_line(&line, &line_n);
                r = parse_vector_line(line, line_n, &model->vertices[model->vertex_count]);
                if (r != ACC3D_OK) { model->diag_bits |= ACC3D_DIAG_BAD_NUMBER; return r; }
                ++model->vertex_count;
            }
        } else if (tok_eq(t, "numsurf")) {
            long declared;
            acc3d_tok a;
            if (current_object < 0) return ACC3D_ERR_PARSE;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_long_tok(a, &declared); if (r != ACC3D_OK) return r;
            model->objects[current_object].surface_start = model->surface_count;
            model->objects[current_object].surface_count = declared;
        } else if (tok_eq(t, "SURF")) {
            long flags;
            acc3d_tok a;
            if (current_object < 0) return ACC3D_ERR_PARSE;
            if (model->surface_count >= ACC3D_MAX_SURFACES) {
                model->diag_bits |= ACC3D_DIAG_LIMIT_HIT;
                return ACC3D_ERR_LIMIT;
            }
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_hex_or_dec_tok(a, &flags); if (r != ACC3D_OK) return r;
            current_surface = model->surface_count;
            memset(&model->surfaces[current_surface], 0, sizeof(model->surfaces[current_surface]));
            model->surfaces[current_surface].object_index = current_object;
            model->surfaces[current_surface].flags = flags;
            model->surfaces[current_surface].material_index = -1;
            model->surfaces[current_surface].ref_start = model->ref_count;
            ++model->surface_count;
        } else if (tok_eq(t, "mat")) {
            long mat;
            acc3d_tok a;
            if (current_surface < 0) return ACC3D_ERR_PARSE;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_long_tok(a, &mat); if (r != ACC3D_OK) return r;
            model->surfaces[current_surface].material_index = mat;
        } else if (tok_eq(t, "refs")) {
            long count;
            long i;
            acc3d_tok a;
            if (current_surface < 0) return ACC3D_ERR_PARSE;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_long_tok(a, &count); if (r != ACC3D_OK) return r;
            if (count < 0 || model->ref_count + count > ACC3D_MAX_REFS) {
                model->diag_bits |= ACC3D_DIAG_LIMIT_HIT;
                return ACC3D_ERR_LIMIT;
            }
            model->surfaces[current_surface].ref_start = model->ref_count;
            model->surfaces[current_surface].ref_count = count;
            for (i = 0; i < count; ++i) {
                long out_ref;
                if (!next_line(&rd, &line, &line_n)) return ACC3D_ERR_PARSE;
                model->line = rd.line_no;
                trim_line(&line, &line_n);
                r = parse_ref_line(model, line, line_n, model->surfaces[current_surface].object_index, &out_ref);
                if (r != ACC3D_OK) return r;
                (void)out_ref;
            }
        } else if (tok_eq(t, "kids")) {
            long kids;
            acc3d_tok a;
            if (current_object < 0) return ACC3D_ERR_PARSE;
            if (!read_token(&p, end, &a)) return ACC3D_ERR_PARSE;
            r = parse_long_tok(a, &kids); if (r != ACC3D_OK) return r;
            model->objects[current_object].kids_declared = kids;
            if (kids > 0) {
                if (stack_n >= ACC3D_MAX_STACK) {
                    model->diag_bits |= ACC3D_DIAG_LIMIT_HIT;
                    return ACC3D_ERR_LIMIT;
                }
                stack[stack_n].object_index = current_object;
                stack[stack_n].remaining = kids;
                ++stack_n;
            }
        } else {
            model->diag_bits |= ACC3D_DIAG_UNKNOWN_TOKEN;
        }
    }

    r = acc3d_triangulate(model, options);
    return r;
}
