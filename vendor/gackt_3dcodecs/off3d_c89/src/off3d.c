#include "off3d.h"

#define OFF3D_MAX_EXP10 18
#define OFF3D_LONG_MAX_C89 ((long)2147483647L)
#define OFF3D_LONG_MIN_C89 ((long)(-2147483647L - 1L))

typedef struct off3d_token_s {
    const char *s;
    off3d_u32 n;
    unsigned long line;
} off3d_token;

typedef struct off3d_reader_s {
    const char *p;
    const char *end;
    unsigned long line;
    int pushed;
    off3d_token push;
} off3d_reader;

static int off3d_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static int off3d_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static char off3d_to_upper(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 'a' + 'A');
    }
    return c;
}

static int off3d_tok_eq_lit(const off3d_token *t, const char *lit, int allow_lower)
{
    off3d_u32 i;
    char a;
    char b;
    for (i = 0U; lit[i] != '\0'; ++i) {
        if (i >= t->n) {
            return 0;
        }
        a = t->s[i];
        b = lit[i];
        if (allow_lower) {
            a = off3d_to_upper(a);
            b = off3d_to_upper(b);
        }
        if (a != b) {
            return 0;
        }
    }
    return i == t->n;
}

static void off3d_reader_init(off3d_reader *r, const char *data, off3d_u32 size)
{
    r->p = data;
    r->end = data + size;
    r->line = 1UL;
    r->pushed = 0;
    r->push.s = (const char *)0;
    r->push.n = 0U;
    r->push.line = 1UL;
    if (size >= 3U) {
        if (((unsigned char)data[0]) == 0xEFU &&
            ((unsigned char)data[1]) == 0xBBU &&
            ((unsigned char)data[2]) == 0xBFU) {
            r->p = data + 3;
        }
    }
}

static void off3d_skip_ws_and_comments(off3d_reader *r)
{
    int again;
    again = 1;
    while (again && r->p < r->end) {
        again = 0;
        while (r->p < r->end && off3d_is_space(*r->p)) {
            if (*r->p == '\r') {
                ++r->line;
                ++r->p;
                if (r->p < r->end && *r->p == '\n') {
                    ++r->p;
                }
            } else if (*r->p == '\n') {
                ++r->line;
                ++r->p;
            } else {
                ++r->p;
            }
        }
        if (r->p < r->end && *r->p == '#') {
            while (r->p < r->end && *r->p != '\n' && *r->p != '\r') {
                ++r->p;
            }
            again = 1;
        }
    }
}

static int off3d_next_token(off3d_reader *r, off3d_token *t)
{
    const char *start;
    if (r->pushed) {
        *t = r->push;
        r->pushed = 0;
        return 1;
    }
    off3d_skip_ws_and_comments(r);
    if (r->p >= r->end) {
        return 0;
    }
    start = r->p;
    t->line = r->line;
    while (r->p < r->end && !off3d_is_space(*r->p) && *r->p != '#') {
        ++r->p;
    }
    t->s = start;
    t->n = (off3d_u32)(r->p - start);
    return 1;
}

static void off3d_push_token(off3d_reader *r, const off3d_token *t)
{
    r->push = *t;
    r->pushed = 1;
}

static int off3d_token_has_decimal(const off3d_token *t)
{
    off3d_u32 i;
    char c;
    for (i = 0U; i < t->n; ++i) {
        c = t->s[i];
        if (c == '.' || c == 'e' || c == 'E') {
            return 1;
        }
    }
    return 0;
}

static int off3d_parse_u32(const off3d_token *t, off3d_u32 *out)
{
    off3d_u32 i;
    off3d_u32 v;
    unsigned int d;
    if (t->n == 0U) {
        return OFF3D_ERR_BAD_NUMBER;
    }
    v = 0U;
    for (i = 0U; i < t->n; ++i) {
        if (!off3d_is_digit(t->s[i])) {
            return OFF3D_ERR_BAD_NUMBER;
        }
        d = (unsigned int)(t->s[i] - '0');
        if (v > ((off3d_u32)0xffffffffUL - (off3d_u32)d) / 10UL) {
            return OFF3D_ERR_RANGE;
        }
        v = (off3d_u32)(v * 10UL + (off3d_u32)d);
    }
    *out = v;
    return OFF3D_OK;
}

static int off3d_parse_i32(const off3d_token *t, off3d_i32 *out)
{
    off3d_u32 i;
    int sign;
    off3d_u32 u;
    off3d_token body;
    if (t->n == 0U) {
        return OFF3D_ERR_BAD_NUMBER;
    }
    sign = 1;
    i = 0U;
    if (t->s[0] == '-') {
        sign = -1;
        i = 1U;
    } else if (t->s[0] == '+') {
        i = 1U;
    }
    if (i >= t->n) {
        return OFF3D_ERR_BAD_NUMBER;
    }
    body.s = t->s + i;
    body.n = t->n - i;
    body.line = t->line;
    if (off3d_parse_u32(&body, &u) != OFF3D_OK) {
        return OFF3D_ERR_BAD_NUMBER;
    }
    if (sign < 0) {
        if (u > 2147483648UL) {
            return OFF3D_ERR_RANGE;
        }
        if (u == 2147483648UL) {
            *out = OFF3D_LONG_MIN_C89;
        } else {
            *out = (off3d_i32)(-(off3d_i32)u);
        }
    } else {
        if (u > 2147483647UL) {
            return OFF3D_ERR_RANGE;
        }
        *out = (off3d_i32)u;
    }
    return OFF3D_OK;
}

static int off3d_checked_mul10(off3d_q16 *v)
{
    if (*v > OFF3D_LONG_MAX_C89 / 10L || *v < OFF3D_LONG_MIN_C89 / 10L) {
        return OFF3D_ERR_RANGE;
    }
    *v = (off3d_q16)(*v * 10L);
    return OFF3D_OK;
}

static int off3d_parse_fixed(const off3d_token *t, off3d_q16 *out)
{
    off3d_u32 i;
    int sign;
    int saw_digit;
    off3d_u32 int_part;
    off3d_q16 q;
    off3d_q16 frac_q;
    off3d_u32 denom;
    int exp_sign;
    int exp_val;
    int rc;
    unsigned int digit;

    if (t->n == 0U) {
        return OFF3D_ERR_BAD_NUMBER;
    }

    i = 0U;
    sign = 1;
    if (t->s[i] == '-') {
        sign = -1;
        ++i;
    } else if (t->s[i] == '+') {
        ++i;
    }

    int_part = 0U;
    saw_digit = 0;
    while (i < t->n && off3d_is_digit(t->s[i])) {
        digit = (unsigned int)(t->s[i] - '0');
        saw_digit = 1;
        if (int_part > (2147483647UL / 65536UL - (off3d_u32)digit) / 10UL) {
            return OFF3D_ERR_RANGE;
        }
        int_part = (off3d_u32)(int_part * 10UL + (off3d_u32)digit);
        ++i;
    }

    q = (off3d_q16)(int_part * 65536UL);
    frac_q = 0L;
    denom = 1U;
    if (i < t->n && t->s[i] == '.') {
        ++i;
        while (i < t->n && off3d_is_digit(t->s[i])) {
            digit = (unsigned int)(t->s[i] - '0');
            saw_digit = 1;
            if (denom <= 100000000U) {
                denom = (off3d_u32)(denom * 10U);
                frac_q = (off3d_q16)(frac_q + (off3d_q16)((digit * 65536UL + denom / 2U) / denom));
            }
            ++i;
        }
    }

    q = (off3d_q16)(q + frac_q);

    if (!saw_digit) {
        return OFF3D_ERR_BAD_NUMBER;
    }

    if (i < t->n && (t->s[i] == 'e' || t->s[i] == 'E')) {
        ++i;
        exp_sign = 1;
        if (i < t->n && t->s[i] == '-') {
            exp_sign = -1;
            ++i;
        } else if (i < t->n && t->s[i] == '+') {
            ++i;
        }
        if (i >= t->n || !off3d_is_digit(t->s[i])) {
            return OFF3D_ERR_BAD_NUMBER;
        }
        exp_val = 0;
        while (i < t->n && off3d_is_digit(t->s[i])) {
            if (exp_val < OFF3D_MAX_EXP10) {
                exp_val = exp_val * 10 + (int)(t->s[i] - '0');
            }
            ++i;
        }
        if (exp_val > OFF3D_MAX_EXP10) {
            return OFF3D_ERR_RANGE;
        }
        while (exp_val > 0) {
            if (exp_sign > 0) {
                rc = off3d_checked_mul10(&q);
                if (rc != OFF3D_OK) {
                    return rc;
                }
            } else {
                q = (off3d_q16)(q / 10L);
            }
            --exp_val;
        }
    }

    if (i != t->n) {
        return OFF3D_ERR_BAD_NUMBER;
    }

    if (sign < 0) {
        q = (off3d_q16)(-q);
    }
    *out = q;
    return OFF3D_OK;
}

static int off3d_parse_magic(const off3d_token *t, int allow_lower,
                             unsigned long *flags, int *unsupported_4d,
                             unsigned long *warnings)
{
    off3d_u32 i;
    off3d_u32 off_pos;
    char c0;
    char c1;
    char c2;
    int found;
    int saw_s;
    int saw_t;

    *flags = 0UL;
    *unsupported_4d = 0;
    if (t->n < 3U) {
        return OFF3D_ERR_BAD_MAGIC;
    }

    found = 0;
    off_pos = 0U;
    i = 0U;
    while (i + 2U < t->n) {
        c0 = t->s[i];
        c1 = t->s[i + 1U];
        c2 = t->s[i + 2U];
        if (allow_lower) {
            c0 = off3d_to_upper(c0);
            c1 = off3d_to_upper(c1);
            c2 = off3d_to_upper(c2);
        }
        if (c0 == 'O' && c1 == 'F' && c2 == 'F') {
            found = 1;
            off_pos = i;
            break;
        }
        ++i;
    }
    if (!found || off_pos + 3U != t->n) {
        return OFF3D_ERR_BAD_MAGIC;
    }

    saw_s = 0;
    saw_t = 0;
    for (i = 0U; i < off_pos; ++i) {
        c0 = t->s[i];
        if (allow_lower) {
            c0 = off3d_to_upper(c0);
        }
        if (c0 == '4') {
            *unsupported_4d = 1;
        } else if (c0 == 'C') {
            *flags |= OFF3D_FILE_HAS_COLOR | OFF3D_FILE_HEADER_C;
        } else if (c0 == 'N') {
            *flags |= OFF3D_FILE_HAS_NORMAL | OFF3D_FILE_HEADER_N;
        } else if (c0 == 'S') {
            saw_s = 1;
        } else if (c0 == 'T') {
            saw_t = 1;
        } else {
            *warnings |= OFF3D_WARN_UNKNOWN_HEADER_FLAG;
        }
    }
    if (saw_s && saw_t) {
        *flags |= OFF3D_FILE_HAS_TEXCOORD | OFF3D_FILE_HEADER_ST;
    } else if (saw_s || saw_t) {
        *warnings |= OFF3D_WARN_UNKNOWN_HEADER_FLAG;
    }
    return OFF3D_OK;
}

static unsigned char off3d_clamp_u8_from_long(long v)
{
    if (v < 0L) {
        return (unsigned char)0U;
    }
    if (v > 255L) {
        return (unsigned char)255U;
    }
    return (unsigned char)v;
}

static int off3d_parse_color_chan(const off3d_token *t, unsigned char *out)
{
    off3d_i32 iv;
    off3d_q16 fv;
    long scaled;
    if (!off3d_token_has_decimal(t)) {
        if (off3d_parse_i32(t, &iv) != OFF3D_OK) {
            return OFF3D_ERR_BAD_NUMBER;
        }
        *out = off3d_clamp_u8_from_long((long)iv);
        return OFF3D_OK;
    }
    if (off3d_parse_fixed(t, &fv) != OFF3D_OK) {
        return OFF3D_ERR_BAD_NUMBER;
    }
    if (fv < 0L) {
        fv = 0L;
    }
    if (fv > OFF3D_Q16_ONE) {
        fv = OFF3D_Q16_ONE;
    }
    scaled = (long)((fv * 255L + OFF3D_Q16_HALF) / OFF3D_Q16_ONE);
    *out = off3d_clamp_u8_from_long(scaled);
    return OFF3D_OK;
}

static int off3d_read_fixed_on_line(off3d_reader *r, unsigned long line,
                                      off3d_q16 *out, off3d_mesh *mesh,
                                      int strict)
{
    off3d_token t;
    int rc;
    if (!off3d_next_token(r, &t)) {
        if (mesh != (off3d_mesh *)0) {
            mesh->warnings |= OFF3D_WARN_MISSING_OPTIONAL;
        }
        if (strict) {
            return OFF3D_ERR_UNEXPECTED_EOF;
        }
        return 0;
    }
    if (t.line != line) {
        off3d_push_token(r, &t);
        if (mesh != (off3d_mesh *)0) {
            mesh->warnings |= OFF3D_WARN_MISSING_OPTIONAL;
        }
        if (strict) {
            return OFF3D_ERR_UNEXPECTED_EOF;
        }
        return 0;
    }
    rc = off3d_parse_fixed(&t, out);
    if (rc != OFF3D_OK) {
        if (mesh != (off3d_mesh *)0) {
            mesh->error_line = t.line;
        }
        return rc;
    }
    return 1;
}

static int off3d_read_color_on_line(off3d_reader *r, unsigned long line,
                                    unsigned char *r8, unsigned char *g8,
                                    unsigned char *b8, unsigned char *a8,
                                    off3d_u32 *cmap_index,
                                    unsigned char *flags,
                                    unsigned long *warnings,
                                    int strict)
{
    off3d_token toks[8];
    off3d_token t;
    off3d_u32 n;
    off3d_u32 idx;
    int rc;

    n = 0U;
    while (off3d_next_token(r, &t)) {
        if (t.line != line) {
            off3d_push_token(r, &t);
            break;
        }
        if (n < 8U) {
            toks[n] = t;
        }
        ++n;
    }

    if (n == 0U) {
        return OFF3D_OK;
    }
    if (n == 1U) {
        rc = off3d_parse_u32(&toks[0], cmap_index);
        if (rc != OFF3D_OK) {
            if (strict) {
                return rc;
            }
            *warnings |= OFF3D_WARN_COLOR_AMBIGUOUS;
            return OFF3D_OK;
        }
        *flags = (unsigned char)(*flags | OFF3D_FACE_HAS_COLOR | OFF3D_FACE_HAS_CMAP_INDEX);
        return OFF3D_OK;
    }
    if (n == 2U) {
        *warnings |= OFF3D_WARN_COLOR_AMBIGUOUS;
        if (strict) {
            return OFF3D_ERR_BAD_NUMBER;
        }
        return OFF3D_OK;
    }

    rc = off3d_parse_color_chan(&toks[0], r8);
    if (rc == OFF3D_OK) {
        rc = off3d_parse_color_chan(&toks[1], g8);
    }
    if (rc == OFF3D_OK) {
        rc = off3d_parse_color_chan(&toks[2], b8);
    }
    if (rc != OFF3D_OK) {
        if (strict) {
            return rc;
        }
        *warnings |= OFF3D_WARN_COLOR_AMBIGUOUS;
        return OFF3D_OK;
    }
    *a8 = (unsigned char)255U;
    if (n >= 4U) {
        rc = off3d_parse_color_chan(&toks[3], a8);
        if (rc != OFF3D_OK) {
            if (strict) {
                return rc;
            }
            *warnings |= OFF3D_WARN_COLOR_AMBIGUOUS;
            *a8 = (unsigned char)255U;
        }
    }
    if (n > 4U) {
        *warnings |= OFF3D_WARN_IGNORED_FACE_EXTRA;
    }
    idx = 0U;
    (void)idx;
    *flags = (unsigned char)(*flags | OFF3D_FACE_HAS_COLOR);
    return OFF3D_OK;
}

void off3d_mesh_init(off3d_mesh *mesh,
                     off3d_vertex *vertices,
                     off3d_u32 vertex_capacity,
                     off3d_face *faces,
                     off3d_u32 face_capacity,
                     off3d_u32 *indices,
                     off3d_u32 index_capacity)
{
    if (mesh == (off3d_mesh *)0) {
        return;
    }
    mesh->vertices = vertices;
    mesh->vertex_capacity = vertex_capacity;
    mesh->faces = faces;
    mesh->face_capacity = face_capacity;
    mesh->indices = indices;
    mesh->index_capacity = index_capacity;
    mesh->vertex_count = 0U;
    mesh->face_count = 0U;
    mesh->edge_count = 0U;
    mesh->index_count = 0U;
    mesh->triangle_count = 0U;
    mesh->file_flags = 0UL;
    mesh->warnings = 0UL;
    mesh->error_line = 0UL;
}

void off3d_options_default(off3d_options *options)
{
    if (options != (off3d_options *)0) {
        options->strict = 0;
        options->allow_lowercase_magic = 1;
        options->allow_off_on_counts_line = 1;
    }
}

static void off3d_set_error_line(off3d_mesh *mesh, unsigned long line)
{
    if (mesh != (off3d_mesh *)0) {
        mesh->error_line = line;
    }
}

int off3d_decode_memory(const char *data,
                        off3d_u32 size,
                        off3d_mesh *mesh,
                        const off3d_options *options)
{
    off3d_options opt;
    off3d_reader reader;
    off3d_token tok;
    off3d_token count_tok;
    off3d_u32 declared_vertices;
    off3d_u32 declared_faces;
    off3d_u32 declared_edges;
    off3d_u32 i;
    off3d_u32 j;
    off3d_i32 signed_index;
    off3d_u32 face_n;
    off3d_u32 first_index;
    unsigned long flags;
    unsigned long warnings;
    int unsupported_4d;
    int rc;
    off3d_vertex *vtx;
    off3d_face *face;
    unsigned long line;
    unsigned char cflags;

    if (data == (const char *)0 || mesh == (off3d_mesh *)0) {
        return OFF3D_ERR_ARGUMENT;
    }
    if (size == 0U) {
        return OFF3D_ERR_EMPTY;
    }
    if (options == (const off3d_options *)0) {
        off3d_options_default(&opt);
    } else {
        opt = *options;
    }

    mesh->vertex_count = 0U;
    mesh->face_count = 0U;
    mesh->edge_count = 0U;
    mesh->index_count = 0U;
    mesh->triangle_count = 0U;
    mesh->file_flags = 0UL;
    mesh->warnings = 0UL;
    mesh->error_line = 0UL;

    off3d_reader_init(&reader, data, size);
    if (!off3d_next_token(&reader, &tok)) {
        return OFF3D_ERR_EMPTY;
    }

    warnings = 0UL;
    rc = off3d_parse_magic(&tok, opt.allow_lowercase_magic, &flags,
                           &unsupported_4d, &warnings);
    if (rc != OFF3D_OK) {
        off3d_set_error_line(mesh, tok.line);
        return rc;
    }
    if (unsupported_4d) {
        off3d_set_error_line(mesh, tok.line);
        return OFF3D_ERR_UNSUPPORTED;
    }
    mesh->file_flags = flags;
    mesh->warnings |= warnings;

    if (!off3d_next_token(&reader, &count_tok)) {
        off3d_set_error_line(mesh, reader.line);
        return OFF3D_ERR_UNEXPECTED_EOF;
    }
    rc = off3d_parse_u32(&count_tok, &declared_vertices);
    if (rc != OFF3D_OK && opt.allow_off_on_counts_line) {
        if (off3d_tok_eq_lit(&count_tok, "OFF", opt.allow_lowercase_magic)) {
            if (!off3d_next_token(&reader, &count_tok)) {
                off3d_set_error_line(mesh, reader.line);
                return OFF3D_ERR_UNEXPECTED_EOF;
            }
            rc = off3d_parse_u32(&count_tok, &declared_vertices);
        }
    }
    if (rc != OFF3D_OK) {
        off3d_set_error_line(mesh, count_tok.line);
        return rc;
    }
    if (!off3d_next_token(&reader, &tok)) {
        off3d_set_error_line(mesh, reader.line);
        return OFF3D_ERR_UNEXPECTED_EOF;
    }
    rc = off3d_parse_u32(&tok, &declared_faces);
    if (rc != OFF3D_OK) {
        off3d_set_error_line(mesh, tok.line);
        return rc;
    }
    if (!off3d_next_token(&reader, &tok)) {
        off3d_set_error_line(mesh, reader.line);
        return OFF3D_ERR_UNEXPECTED_EOF;
    }
    rc = off3d_parse_u32(&tok, &declared_edges);
    if (rc != OFF3D_OK) {
        off3d_set_error_line(mesh, tok.line);
        return rc;
    }

    if (declared_vertices > mesh->vertex_capacity || declared_faces > mesh->face_capacity) {
        off3d_set_error_line(mesh, count_tok.line);
        return OFF3D_ERR_CAPACITY;
    }

    mesh->edge_count = declared_edges;

    for (i = 0U; i < declared_vertices; ++i) {
        vtx = &mesh->vertices[i];
        vtx->x = 0L;
        vtx->y = 0L;
        vtx->z = 0L;
        vtx->nx = 0L;
        vtx->ny = 0L;
        vtx->nz = 0L;
        vtx->u = 0L;
        vtx->v = 0L;
        vtx->r = 255U;
        vtx->g = 255U;
        vtx->b = 255U;
        vtx->a = 255U;
        vtx->flags = 0U;

        if (!off3d_next_token(&reader, &tok)) {
            off3d_set_error_line(mesh, reader.line);
            return OFF3D_ERR_UNEXPECTED_EOF;
        }
        line = tok.line;
        rc = off3d_parse_fixed(&tok, &vtx->x);
        if (rc != OFF3D_OK) {
            off3d_set_error_line(mesh, tok.line);
            return rc;
        }
        if (!off3d_next_token(&reader, &tok)) {
            off3d_set_error_line(mesh, reader.line);
            return OFF3D_ERR_UNEXPECTED_EOF;
        }
        rc = off3d_parse_fixed(&tok, &vtx->y);
        if (rc != OFF3D_OK) {
            off3d_set_error_line(mesh, tok.line);
            return rc;
        }
        if (!off3d_next_token(&reader, &tok)) {
            off3d_set_error_line(mesh, reader.line);
            return OFF3D_ERR_UNEXPECTED_EOF;
        }
        rc = off3d_parse_fixed(&tok, &vtx->z);
        if (rc != OFF3D_OK) {
            off3d_set_error_line(mesh, tok.line);
            return rc;
        }

        if ((flags & OFF3D_FILE_HEADER_N) != 0UL) {
            rc = off3d_read_fixed_on_line(&reader, line, &vtx->nx, mesh, opt.strict);
            if (rc < 0) {
                off3d_set_error_line(mesh, line);
                return rc;
            }
            if (rc > 0) {
                rc = off3d_read_fixed_on_line(&reader, line, &vtx->ny, mesh, opt.strict);
                if (rc < 0) {
                    off3d_set_error_line(mesh, line);
                    return rc;
                }
                if (rc > 0) {
                    rc = off3d_read_fixed_on_line(&reader, line, &vtx->nz, mesh, opt.strict);
                    if (rc < 0) {
                        off3d_set_error_line(mesh, line);
                        return rc;
                    }
                    if (rc > 0) {
                        vtx->flags = (unsigned char)(vtx->flags | OFF3D_VERTEX_HAS_NORMAL);
                    }
                }
            }
        }

        if ((flags & OFF3D_FILE_HEADER_ST) != 0UL) {
            rc = off3d_read_fixed_on_line(&reader, line, &vtx->u, mesh, opt.strict);
            if (rc < 0) {
                off3d_set_error_line(mesh, line);
                return rc;
            }
            if (rc > 0) {
                rc = off3d_read_fixed_on_line(&reader, line, &vtx->v, mesh, opt.strict);
                if (rc < 0) {
                    off3d_set_error_line(mesh, line);
                    return rc;
                }
                if (rc > 0) {
                    vtx->flags = (unsigned char)(vtx->flags | OFF3D_VERTEX_HAS_TEXCOORD);
                }
            }
        }

        cflags = 0U;
        if ((flags & OFF3D_FILE_HEADER_C) != 0UL) {
            rc = off3d_read_color_on_line(&reader, line, &vtx->r, &vtx->g,
                                          &vtx->b, &vtx->a, &j, &cflags,
                                          &mesh->warnings, opt.strict);
            if (rc != OFF3D_OK) {
                off3d_set_error_line(mesh, line);
                return rc;
            }
            if ((cflags & OFF3D_FACE_HAS_COLOR) != 0U) {
                vtx->flags = (unsigned char)(vtx->flags | OFF3D_VERTEX_HAS_COLOR);
            } else {
                mesh->warnings |= OFF3D_WARN_MISSING_OPTIONAL;
            }
        } else {
            off3d_token extra;
            while (off3d_next_token(&reader, &extra)) {
                if (extra.line != line) {
                    off3d_push_token(&reader, &extra);
                    break;
                }
                mesh->warnings |= OFF3D_WARN_IGNORED_VERTEX_EXTRA;
            }
        }
        mesh->vertex_count = i + 1U;
    }

    for (i = 0U; i < declared_faces; ++i) {
        if (!off3d_next_token(&reader, &tok)) {
            off3d_set_error_line(mesh, reader.line);
            return OFF3D_ERR_UNEXPECTED_EOF;
        }
        line = tok.line;
        rc = off3d_parse_u32(&tok, &face_n);
        if (rc != OFF3D_OK) {
            off3d_set_error_line(mesh, tok.line);
            return rc;
        }
        if (face_n < 3U) {
            mesh->warnings |= OFF3D_WARN_DEGENERATE_FACE;
            if (opt.strict) {
                off3d_set_error_line(mesh, tok.line);
                return OFF3D_ERR_BAD_NUMBER;
            }
        }
        if (mesh->index_count + face_n < mesh->index_count ||
            mesh->index_count + face_n > mesh->index_capacity) {
            off3d_set_error_line(mesh, tok.line);
            return OFF3D_ERR_CAPACITY;
        }
        first_index = mesh->index_count;
        for (j = 0U; j < face_n; ++j) {
            if (!off3d_next_token(&reader, &tok)) {
                off3d_set_error_line(mesh, reader.line);
                return OFF3D_ERR_UNEXPECTED_EOF;
            }
            rc = off3d_parse_i32(&tok, &signed_index);
            if (rc != OFF3D_OK) {
                off3d_set_error_line(mesh, tok.line);
                return rc;
            }
            if (signed_index < 0L || (off3d_u32)signed_index >= declared_vertices) {
                off3d_set_error_line(mesh, tok.line);
                return OFF3D_ERR_BAD_INDEX;
            }
            mesh->indices[mesh->index_count] = (off3d_u32)signed_index;
            ++mesh->index_count;
        }

        face = &mesh->faces[i];
        face->first_index = first_index;
        face->index_count = face_n;
        face->color_index = 0U;
        face->r = 255U;
        face->g = 255U;
        face->b = 255U;
        face->a = 255U;
        face->flags = 0U;
        rc = off3d_read_color_on_line(&reader, line, &face->r, &face->g,
                                      &face->b, &face->a, &face->color_index,
                                      &face->flags, &mesh->warnings,
                                      opt.strict);
        if (rc != OFF3D_OK) {
            off3d_set_error_line(mesh, line);
            return rc;
        }
        if ((face->flags & OFF3D_FACE_HAS_COLOR) != 0U) {
            mesh->file_flags |= OFF3D_FILE_HAS_COLOR;
        }
        if (face_n >= 3U) {
            mesh->triangle_count = (off3d_u32)(mesh->triangle_count + (face_n - 2U));
        }
        mesh->face_count = i + 1U;
    }

    return OFF3D_OK;
}

int off3d_triangulate_fan(const off3d_mesh *mesh,
                          off3d_u32 *triangles,
                          off3d_u32 triangle_index_capacity,
                          off3d_u32 *triangle_count_out)
{
    off3d_u32 fi;
    off3d_u32 j;
    off3d_u32 out_i;
    const off3d_face *f;
    off3d_u32 a;

    if (mesh == (const off3d_mesh *)0 || triangles == (off3d_u32 *)0 ||
        triangle_count_out == (off3d_u32 *)0) {
        return OFF3D_ERR_ARGUMENT;
    }

    out_i = 0U;
    for (fi = 0U; fi < mesh->face_count; ++fi) {
        f = &mesh->faces[fi];
        if (f->index_count >= 3U) {
            a = mesh->indices[f->first_index];
            for (j = 1U; j + 1U < f->index_count; ++j) {
                if (out_i + 3U > triangle_index_capacity) {
                    return OFF3D_ERR_CAPACITY;
                }
                triangles[out_i] = a;
                triangles[out_i + 1U] = mesh->indices[f->first_index + j];
                triangles[out_i + 2U] = mesh->indices[f->first_index + j + 1U];
                out_i += 3U;
            }
        }
    }
    *triangle_count_out = out_i / 3U;
    return OFF3D_OK;
}

const char *off3d_error_string(int code)
{
    switch (code) {
    case OFF3D_OK:
        return "ok";
    case OFF3D_ERR_ARGUMENT:
        return "bad argument";
    case OFF3D_ERR_EMPTY:
        return "empty input";
    case OFF3D_ERR_BAD_MAGIC:
        return "bad or unsupported OFF magic";
    case OFF3D_ERR_UNSUPPORTED:
        return "unsupported OFF extension";
    case OFF3D_ERR_BAD_NUMBER:
        return "bad numeric token";
    case OFF3D_ERR_CAPACITY:
        return "caller buffer capacity exceeded";
    case OFF3D_ERR_BAD_INDEX:
        return "face index out of range";
    case OFF3D_ERR_UNEXPECTED_EOF:
        return "unexpected end of file";
    case OFF3D_ERR_RANGE:
        return "numeric range exceeded";
    default:
        return "unknown error";
    }
}

long off3d_q16_to_i32_floor(off3d_q16 v)
{
    return v / OFF3D_Q16_ONE;
}

long off3d_q16_frac_1000(off3d_q16 v)
{
    long frac;
    if (v < 0L) {
        v = -v;
    }
    frac = v % OFF3D_Q16_ONE;
    return (frac * 1000L + OFF3D_Q16_HALF) / OFF3D_Q16_ONE;
}
