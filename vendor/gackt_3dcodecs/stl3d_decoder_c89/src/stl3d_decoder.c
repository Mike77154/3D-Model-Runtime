#include "stl3d_decoder.h"

static int stl3d_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static char stl3d_lower(char c)
{
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static int stl3d_streq_ci(const char *a, const char *b)
{
    while (*a && *b) {
        if (stl3d_lower(*a) != stl3d_lower(*b)) return 0;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static int stl3d_starts_ci_buf(const stl3d_u8 *p, unsigned long n, const char *word)
{
    unsigned long i;
    i = 0UL;
    while (word[i]) {
        if (i >= n) return 0;
        if (stl3d_lower((char)p[i]) != stl3d_lower(word[i])) return 0;
        ++i;
    }
    return 1;
}

static void stl3d_memzero_color(stl3d_color *c)
{
    if (!c) return;
    c->r = 0U;
    c->g = 0U;
    c->b = 0U;
    c->a = 255U;
    c->valid = 0U;
    c->kind = STL3D_COLOR_NONE;
}

static void stl3d_tri_reset(stl3d_triangle *tri)
{
    int i;
    tri->normal.x = 0;
    tri->normal.y = 0;
    tri->normal.z = 0;
    for (i = 0; i < 3; ++i) {
        tri->v[i].x = 0;
        tri->v[i].y = 0;
        tri->v[i].z = 0;
    }
    tri->attribute = 0U;
    tri->index = 0UL;
    tri->flags = 0U;
    stl3d_memzero_color(&tri->color_viscam);
    stl3d_memzero_color(&tri->color_magics);
}

static int stl3d_move_unread_to_front(stl3d_ctx *ctx)
{
    unsigned long unread;
    unsigned long i;
    if (!ctx) return STL3D_ERR_ARG;
    if (ctx->pos == 0UL) return STL3D_OK;
    unread = ctx->len - ctx->pos;
    i = 0UL;
    while (i < unread) {
        ctx->buf[i] = ctx->buf[ctx->pos + i];
        ++i;
    }
    ctx->pos = 0UL;
    ctx->len = unread;
    return STL3D_OK;
}

static int stl3d_fill_to(stl3d_ctx *ctx, unsigned long want)
{
    int rc;
    unsigned long got;

    if (!ctx || !ctx->buf) return STL3D_ERR_ARG;
    if (want > ctx->cap) return STL3D_ERR_BUFFER;
    while ((ctx->len - ctx->pos) < want && !ctx->eof_seen) {
        rc = stl3d_move_unread_to_front(ctx);
        if (rc != STL3D_OK) return rc;
        if (ctx->len >= ctx->cap) return STL3D_ERR_BUFFER;
        got = 0UL;
        rc = ctx->read_fn(ctx->read_user, ctx->buf + ctx->len, ctx->cap - ctx->len, &got);
        if (rc != STL3D_OK) return STL3D_ERR_IO;
        if (got == 0UL) {
            ctx->eof_seen = 1;
        } else {
            ctx->len += got;
        }
    }
    if ((ctx->len - ctx->pos) >= want) return STL3D_OK;
    return STL3D_EOF;
}

static int stl3d_get_byte(stl3d_ctx *ctx, stl3d_u8 *out)
{
    int rc;
    rc = stl3d_fill_to(ctx, 1UL);
    if (rc != STL3D_OK) return rc;
    *out = ctx->buf[ctx->pos++];
    return STL3D_OK;
}

static stl3d_u32 stl3d_read_u32le_mem(const stl3d_u8 *b)
{
    return ((stl3d_u32)b[0]) |
           ((stl3d_u32)b[1] << 8) |
           ((stl3d_u32)b[2] << 16) |
           ((stl3d_u32)b[3] << 24);
}

static stl3d_u16 stl3d_read_u16le_mem(const stl3d_u8 *b)
{
    return (stl3d_u16)(((stl3d_u16)b[0]) | ((stl3d_u16)b[1] << 8));
}

static int stl3d_read_exact(stl3d_ctx *ctx, stl3d_u8 *dst, unsigned long n)
{
    unsigned long i;
    int rc;
    stl3d_u8 ch;
    i = 0UL;
    while (i < n) {
        rc = stl3d_get_byte(ctx, &ch);
        if (rc != STL3D_OK) return rc;
        dst[i++] = ch;
    }
    return STL3D_OK;
}

static int stl3d_next_token(stl3d_ctx *ctx, char *tok, unsigned long tok_cap)
{
    int rc;
    stl3d_u8 ch;
    unsigned long n;
    int in_token;

    if (!tok || tok_cap == 0UL) return STL3D_ERR_ARG;
    tok[0] = '\0';
    n = 0UL;
    in_token = 0;

    for (;;) {
        rc = stl3d_get_byte(ctx, &ch);
        if (rc == STL3D_EOF) {
            if (in_token) {
                tok[n < tok_cap ? n : tok_cap - 1UL] = '\0';
                return STL3D_OK;
            }
            return STL3D_EOF;
        }
        if (rc != STL3D_OK) return rc;

        if (stl3d_is_space((char)ch)) {
            if (in_token) {
                tok[n < tok_cap ? n : tok_cap - 1UL] = '\0';
                return STL3D_OK;
            }
        } else {
            in_token = 1;
            if (n + 1UL < tok_cap) {
                tok[n++] = (char)ch;
            }
        }
    }
}

static int stl3d_parse_vec3_tokens(stl3d_ctx *ctx, stl3d_vec3 *v)
{
    char tok[80];
    int ok;
    int rc;

    rc = stl3d_next_token(ctx, tok, sizeof(tok));
    if (rc != STL3D_OK) return rc;
    v->x = stl3d_fixed_from_ascii(tok, &ok);
    if (!ok) return STL3D_ERR_FORMAT;

    rc = stl3d_next_token(ctx, tok, sizeof(tok));
    if (rc != STL3D_OK) return rc;
    v->y = stl3d_fixed_from_ascii(tok, &ok);
    if (!ok) return STL3D_ERR_FORMAT;

    rc = stl3d_next_token(ctx, tok, sizeof(tok));
    if (rc != STL3D_OK) return rc;
    v->z = stl3d_fixed_from_ascii(tok, &ok);
    if (!ok) return STL3D_ERR_FORMAT;

    return STL3D_OK;
}

static int stl3d_find_header_token(const stl3d_u8 header[80], const char *pat)
{
    unsigned long i;
    unsigned long j;
    i = 0UL;
    while (i < 80UL) {
        j = 0UL;
        while (pat[j] && i + j < 80UL && header[i + j] == (stl3d_u8)pat[j]) {
            ++j;
        }
        if (pat[j] == '\0') return (int)i;
        ++i;
    }
    return -1;
}

static void stl3d_parse_magics_object_color(stl3d_ctx *ctx)
{
    int p;
    if (!ctx) return;
    stl3d_memzero_color(&ctx->magics_object_color);
    p = stl3d_find_header_token(ctx->binary_header, "COLOR=");
    if (p >= 0 && p + 10 <= 80) {
        ctx->magics_object_color.r = ctx->binary_header[p + 6];
        ctx->magics_object_color.g = ctx->binary_header[p + 7];
        ctx->magics_object_color.b = ctx->binary_header[p + 8];
        ctx->magics_object_color.a = ctx->binary_header[p + 9];
        ctx->magics_object_color.valid = 1U;
        ctx->magics_object_color.kind = STL3D_COLOR_MAGICS_OBJECT;
    }
}

static int stl3d_start_binary(stl3d_ctx *ctx)
{
    stl3d_u8 header[84];
    unsigned long i;
    int rc;

    rc = stl3d_read_exact(ctx, header, 84UL);
    if (rc != STL3D_OK) return STL3D_ERR_FORMAT;
    i = 0UL;
    while (i < 80UL) {
        ctx->binary_header[i] = header[i];
        ++i;
    }
    ctx->has_binary_header = 1;
    stl3d_parse_magics_object_color(ctx);
    ctx->binary_tri_total = stl3d_read_u32le_mem(header + 80UL);
    ctx->binary_tri_left = ctx->binary_tri_total;
    ctx->tri_index = 0UL;
    ctx->started = 1;
    return STL3D_OK;
}

static int stl3d_start_ascii(stl3d_ctx *ctx)
{
    ctx->tri_index = 0UL;
    ctx->started = 1;
    return STL3D_OK;
}

int stl3d_init(stl3d_ctx *ctx, stl3d_read_fn read_fn, void *read_user, stl3d_u8 *work_buf, unsigned long work_buf_size)
{
    unsigned long i;
    if (!ctx || !read_fn || !work_buf) return STL3D_ERR_ARG;
    if (work_buf_size < STL3D_MIN_BUFFER_SIZE) return STL3D_ERR_BUFFER;
    ctx->read_fn = read_fn;
    ctx->read_user = read_user;
    ctx->buf = work_buf;
    ctx->cap = work_buf_size;
    ctx->pos = 0UL;
    ctx->len = 0UL;
    ctx->eof_seen = 0;
    ctx->format = STL3D_FORMAT_UNKNOWN;
    ctx->started = 0;
    ctx->total_size = 0UL;
    ctx->has_total_size = 0;
    ctx->validate_triangles = 1;
    i = 0UL;
    while (i < 80UL) {
        ctx->binary_header[i] = 0U;
        ++i;
    }
    ctx->has_binary_header = 0;
    stl3d_memzero_color(&ctx->magics_object_color);
    ctx->binary_tri_total = 0UL;
    ctx->binary_tri_left = 0UL;
    ctx->tri_index = 0UL;
    return STL3D_OK;
}

void stl3d_set_total_size(stl3d_ctx *ctx, stl3d_u32 total_size)
{
    if (!ctx) return;
    ctx->total_size = total_size;
    ctx->has_total_size = 1;
}

void stl3d_set_validation(stl3d_ctx *ctx, int enabled)
{
    if (!ctx) return;
    ctx->validate_triangles = enabled ? 1 : 0;
}

int stl3d_force_format(stl3d_ctx *ctx, int format)
{
    if (!ctx) return STL3D_ERR_ARG;
    if (format != STL3D_FORMAT_ASCII && format != STL3D_FORMAT_BINARY && format != STL3D_FORMAT_UNKNOWN) {
        return STL3D_ERR_ARG;
    }
    if (ctx->started) return STL3D_ERR_FORMAT;
    ctx->format = format;
    return STL3D_OK;
}

static int stl3d_has_nul_sample(const stl3d_u8 *p, unsigned long n)
{
    unsigned long i;
    i = 0UL;
    while (i < n) {
        if (p[i] == 0U) return 1;
        ++i;
    }
    return 0;
}

static int stl3d_sample_has_word(const stl3d_u8 *p, unsigned long n, const char *word)
{
    unsigned long i;
    unsigned long off;
    off = 0UL;
    while (off < n) {
        while (off < n && stl3d_is_space((char)p[off])) ++off;
        i = 0UL;
        while (word[i] && off + i < n && stl3d_lower((char)p[off + i]) == stl3d_lower(word[i])) ++i;
        if (word[i] == '\0') return 1;
        while (off < n && !stl3d_is_space((char)p[off])) ++off;
    }
    return 0;
}

int stl3d_detect_format(stl3d_ctx *ctx, int *format_out)
{
    int rc;
    unsigned long off;
    unsigned long sample;
    stl3d_u32 count;
    stl3d_u32 expected;
    stl3d_u32 max_count;

    if (!ctx || !format_out) return STL3D_ERR_ARG;
    if (ctx->format != STL3D_FORMAT_UNKNOWN) {
        *format_out = ctx->format;
        return STL3D_OK;
    }

    rc = stl3d_fill_to(ctx, 84UL);
    if (rc != STL3D_OK && rc != STL3D_EOF) return rc;

    if ((ctx->len - ctx->pos) >= 84UL && ctx->has_total_size && ctx->total_size >= 84UL) {
        count = stl3d_read_u32le_mem(ctx->buf + ctx->pos + 80UL);
        max_count = (0xffffffffUL - 84UL) / 50UL;
        if (count <= max_count) {
            expected = 84UL + count * 50UL;
            if (expected == ctx->total_size && ((ctx->total_size - 84UL) % 50UL) == 0UL) {
                ctx->format = STL3D_FORMAT_BINARY;
                *format_out = ctx->format;
                return STL3D_OK;
            }
        }
    }

    sample = ctx->cap;
    if (sample < 84UL) sample = 84UL;
    if (sample > ctx->cap) sample = ctx->cap;
    if (sample > 512UL) sample = 512UL;
    rc = stl3d_fill_to(ctx, sample);
    if (rc != STL3D_OK && rc != STL3D_EOF) return rc;
    sample = ctx->len - ctx->pos;

    off = ctx->pos;
    while (off < ctx->len && stl3d_is_space((char)ctx->buf[off])) ++off;

    if ((ctx->len - off) >= 5UL && stl3d_starts_ci_buf(ctx->buf + off, ctx->len - off, "solid")) {
        if (stl3d_has_nul_sample(ctx->buf + ctx->pos, sample)) {
            ctx->format = STL3D_FORMAT_BINARY;
        } else if (stl3d_sample_has_word(ctx->buf + off, ctx->len - off, "facet") ||
                   stl3d_sample_has_word(ctx->buf + off, ctx->len - off, "endsolid")) {
            ctx->format = STL3D_FORMAT_ASCII;
        } else {
            ctx->format = STL3D_FORMAT_ASCII;
        }
    } else if ((ctx->len - ctx->pos) >= 84UL) {
        ctx->format = STL3D_FORMAT_BINARY;
    } else {
        ctx->format = STL3D_FORMAT_ASCII;
    }
    *format_out = ctx->format;
    return STL3D_OK;
}

static stl3d_fixed stl3d_f32_field(const stl3d_u8 *p, stl3d_u16 *flags)
{
    int nf;
    int sat;
    stl3d_fixed v;
    nf = 0;
    sat = 0;
    v = stl3d_fixed_from_f32le_ex(p, &nf, &sat);
    if (nf && flags) *flags = (stl3d_u16)(*flags | STL3D_TRI_FLAG_NON_FINITE);
    if (sat && flags) *flags = (stl3d_u16)(*flags | STL3D_TRI_FLAG_SATURATED);
    return v;
}

void stl3d_decode_viscam_color(stl3d_u16 attribute, stl3d_color *out)
{
    stl3d_u8 r5;
    stl3d_u8 g5;
    stl3d_u8 b5;
    if (!out) return;
    stl3d_memzero_color(out);
    if ((attribute & 0x8000U) == 0U) return;
    b5 = (stl3d_u8)(attribute & 31U);
    g5 = (stl3d_u8)((attribute >> 5) & 31U);
    r5 = (stl3d_u8)((attribute >> 10) & 31U);
    out->r = (stl3d_u8)((r5 << 3) | (r5 >> 2));
    out->g = (stl3d_u8)((g5 << 3) | (g5 >> 2));
    out->b = (stl3d_u8)((b5 << 3) | (b5 >> 2));
    out->a = 255U;
    out->valid = 1U;
    out->kind = STL3D_COLOR_VISCAM_SOLIDVIEW;
}

void stl3d_decode_magics_facet_color(stl3d_u16 attribute, const stl3d_color *object_color, stl3d_color *out)
{
    stl3d_u8 r5;
    stl3d_u8 g5;
    stl3d_u8 b5;
    if (!out) return;
    stl3d_memzero_color(out);
    if ((attribute & 0x8000U) != 0U) {
        if (object_color && object_color->valid) {
            *out = *object_color;
            out->kind = STL3D_COLOR_MAGICS_OBJECT;
        }
        return;
    }
    if (attribute == 0U) return;
    r5 = (stl3d_u8)(attribute & 31U);
    g5 = (stl3d_u8)((attribute >> 5) & 31U);
    b5 = (stl3d_u8)((attribute >> 10) & 31U);
    out->r = (stl3d_u8)((r5 << 3) | (r5 >> 2));
    out->g = (stl3d_u8)((g5 << 3) | (g5 >> 2));
    out->b = (stl3d_u8)((b5 << 3) | (b5 >> 2));
    out->a = 255U;
    out->valid = 1U;
    out->kind = STL3D_COLOR_MAGICS_FACET;
}

static int stl3d_read_binary_triangle(stl3d_ctx *ctx, stl3d_triangle *tri)
{
    stl3d_u8 b[50];
    int rc;
    int i;
    stl3d_u16 flags;

    if (ctx->binary_tri_left == 0UL) return STL3D_EOF;
    rc = stl3d_read_exact(ctx, b, 50UL);
    if (rc != STL3D_OK) return STL3D_ERR_FORMAT;

    stl3d_tri_reset(tri);
    flags = 0U;
    tri->normal.x = stl3d_f32_field(b + 0, &flags);
    tri->normal.y = stl3d_f32_field(b + 4, &flags);
    tri->normal.z = stl3d_f32_field(b + 8, &flags);
    for (i = 0; i < 3; ++i) {
        unsigned long base;
        base = 12UL + (unsigned long)i * 12UL;
        tri->v[i].x = stl3d_f32_field(b + base + 0UL, &flags);
        tri->v[i].y = stl3d_f32_field(b + base + 4UL, &flags);
        tri->v[i].z = stl3d_f32_field(b + base + 8UL, &flags);
    }
    tri->attribute = stl3d_read_u16le_mem(b + 48UL);
    if (tri->attribute != 0U) flags = (stl3d_u16)(flags | STL3D_TRI_FLAG_ATTRIBUTE_NONZERO);
    tri->index = ctx->tri_index++;
    tri->flags = flags;
    stl3d_decode_viscam_color(tri->attribute, &tri->color_viscam);
    stl3d_decode_magics_facet_color(tri->attribute, &ctx->magics_object_color, &tri->color_magics);
    if (ctx->validate_triangles) tri->flags = stl3d_validate_triangle(tri);
    --ctx->binary_tri_left;
    return STL3D_OK;
}

static int stl3d_read_ascii_triangle(stl3d_ctx *ctx, stl3d_triangle *tri)
{
    char tok[80];
    int rc;
    int vertex_count;
    int saw_facet;
    int need_normal;

    stl3d_tri_reset(tri);
    tri->index = ctx->tri_index;

    saw_facet = 0;
    for (;;) {
        rc = stl3d_next_token(ctx, tok, sizeof(tok));
        if (rc == STL3D_EOF) return STL3D_EOF;
        if (rc != STL3D_OK) return rc;
        if (stl3d_streq_ci(tok, "facet")) {
            saw_facet = 1;
            break;
        }
        if (stl3d_streq_ci(tok, "endsolid")) return STL3D_EOF;
    }

    if (!saw_facet) return STL3D_EOF;

    need_normal = 1;
    rc = stl3d_next_token(ctx, tok, sizeof(tok));
    if (rc != STL3D_OK) return rc;
    if (stl3d_streq_ci(tok, "normal")) {
        rc = stl3d_parse_vec3_tokens(ctx, &tri->normal);
        if (rc != STL3D_OK) return rc;
    } else {
        need_normal = 0;
    }

    vertex_count = 0;
    if (!need_normal && stl3d_streq_ci(tok, "vertex")) {
        rc = stl3d_parse_vec3_tokens(ctx, &tri->v[vertex_count]);
        if (rc != STL3D_OK) return rc;
        ++vertex_count;
    }

    while (vertex_count < 3) {
        rc = stl3d_next_token(ctx, tok, sizeof(tok));
        if (rc != STL3D_OK) return rc;
        if (stl3d_streq_ci(tok, "vertex")) {
            rc = stl3d_parse_vec3_tokens(ctx, &tri->v[vertex_count]);
            if (rc != STL3D_OK) return rc;
            ++vertex_count;
        } else if (stl3d_streq_ci(tok, "endfacet") || stl3d_streq_ci(tok, "endsolid")) {
            return STL3D_ERR_FORMAT;
        }
    }

    for (;;) {
        rc = stl3d_next_token(ctx, tok, sizeof(tok));
        if (rc == STL3D_EOF) break;
        if (rc != STL3D_OK) return rc;
        if (stl3d_streq_ci(tok, "endfacet")) break;
        if (stl3d_streq_ci(tok, "facet")) return STL3D_ERR_FORMAT;
    }

    tri->index = ctx->tri_index++;
    if (ctx->validate_triangles) tri->flags = stl3d_validate_triangle(tri);
    return STL3D_OK;
}

int stl3d_next_triangle(stl3d_ctx *ctx, stl3d_triangle *tri)
{
    int rc;
    int fmt;

    if (!ctx || !tri) return STL3D_ERR_ARG;
    if (ctx->format == STL3D_FORMAT_UNKNOWN) {
        rc = stl3d_detect_format(ctx, &fmt);
        if (rc != STL3D_OK) return rc;
    }
    if (!ctx->started) {
        if (ctx->format == STL3D_FORMAT_BINARY) rc = stl3d_start_binary(ctx);
        else rc = stl3d_start_ascii(ctx);
        if (rc != STL3D_OK) return rc;
    }

    if (ctx->format == STL3D_FORMAT_BINARY) return stl3d_read_binary_triangle(ctx, tri);
    if (ctx->format == STL3D_FORMAT_ASCII) return stl3d_read_ascii_triangle(ctx, tri);
    return STL3D_ERR_FORMAT;
}

static int stl3d_abs_gt(stl3d_fixed v, stl3d_fixed limit)
{
    if (v < 0) {
        if (v == STL3D_FIXED_MIN) return 1;
        return -v > limit;
    }
    return v > limit;
}

static stl3d_fixed stl3d_diff_sat(stl3d_fixed a, stl3d_fixed b)
{
    if (b < 0 && a > STL3D_FIXED_MAX + b) return STL3D_FIXED_MAX;
    if (b > 0 && a < STL3D_FIXED_MIN + b) return STL3D_FIXED_MIN;
    return (stl3d_fixed)(a - b);
}

static void stl3d_shrink3(stl3d_fixed *x, stl3d_fixed *y, stl3d_fixed *z, stl3d_fixed limit)
{
    while (stl3d_abs_gt(*x, limit) || stl3d_abs_gt(*y, limit) || stl3d_abs_gt(*z, limit)) {
        *x = (stl3d_fixed)(*x / 2L);
        *y = (stl3d_fixed)(*y / 2L);
        *z = (stl3d_fixed)(*z / 2L);
    }
}

static int stl3d_near_zero3(const stl3d_vec3 *v)
{
    stl3d_fixed eps;
    eps = 16L;
    return !stl3d_abs_gt(v->x, eps) && !stl3d_abs_gt(v->y, eps) && !stl3d_abs_gt(v->z, eps);
}

stl3d_u16 stl3d_validate_triangle(stl3d_triangle *tri)
{
    stl3d_fixed ax;
    stl3d_fixed ay;
    stl3d_fixed az;
    stl3d_fixed bx;
    stl3d_fixed by;
    stl3d_fixed bz;
    stl3d_fixed cx;
    stl3d_fixed cy;
    stl3d_fixed cz;
    stl3d_fixed nx;
    stl3d_fixed ny;
    stl3d_fixed nz;
    stl3d_s32 dot;
    stl3d_u16 flags;

    if (!tri) return 0U;
    flags = tri->flags;
    if (tri->attribute != 0U) flags = (stl3d_u16)(flags | STL3D_TRI_FLAG_ATTRIBUTE_NONZERO);

    if (stl3d_near_zero3(&tri->normal)) flags = (stl3d_u16)(flags | STL3D_TRI_FLAG_ZERO_NORMAL);

    ax = stl3d_diff_sat(tri->v[1].x, tri->v[0].x);
    ay = stl3d_diff_sat(tri->v[1].y, tri->v[0].y);
    az = stl3d_diff_sat(tri->v[1].z, tri->v[0].z);
    bx = stl3d_diff_sat(tri->v[2].x, tri->v[0].x);
    by = stl3d_diff_sat(tri->v[2].y, tri->v[0].y);
    bz = stl3d_diff_sat(tri->v[2].z, tri->v[0].z);

    stl3d_shrink3(&ax, &ay, &az, 32767L);
    stl3d_shrink3(&bx, &by, &bz, 32767L);

    cx = (stl3d_fixed)(ay * bz - az * by);
    cy = (stl3d_fixed)(az * bx - ax * bz);
    cz = (stl3d_fixed)(ax * by - ay * bx);

    if (cx == 0 && cy == 0 && cz == 0) {
        flags = (stl3d_u16)(flags | STL3D_TRI_FLAG_DEGENERATE);
    } else if ((flags & STL3D_TRI_FLAG_ZERO_NORMAL) == 0U) {
        nx = tri->normal.x;
        ny = tri->normal.y;
        nz = tri->normal.z;
        stl3d_shrink3(&nx, &ny, &nz, 16383L);
        stl3d_shrink3(&cx, &cy, &cz, 16383L);
        dot = (stl3d_s32)(nx * cx + ny * cy + nz * cz);
        if (dot < 0L) flags = (stl3d_u16)(flags | STL3D_TRI_FLAG_NORMAL_FLIPPED);
        else if (dot == 0L) flags = (stl3d_u16)(flags | STL3D_TRI_FLAG_NORMAL_MISMATCH);
    }

    tri->flags = flags;
    return flags;
}

void stl3d_stats_init(stl3d_stats *stats)
{
    if (!stats) return;
    stats->triangles = 0UL;
    stats->has_bounds = 0;
    stats->minv.x = stats->minv.y = stats->minv.z = 0;
    stats->maxv.x = stats->maxv.y = stats->maxv.z = 0;
    stats->degenerate_triangles = 0UL;
    stats->zero_normals = 0UL;
    stats->normal_flipped = 0UL;
    stats->normal_mismatch = 0UL;
    stats->nonzero_attributes = 0UL;
    stats->non_finite_values = 0UL;
    stats->saturated_values = 0UL;
    stats->viscam_color_facets = 0UL;
    stats->magics_color_facets = 0UL;
}

static void stl3d_stats_point(stl3d_stats *stats, const stl3d_vec3 *v)
{
    if (!stats->has_bounds) {
        stats->minv = *v;
        stats->maxv = *v;
        stats->has_bounds = 1;
        return;
    }
    if (v->x < stats->minv.x) stats->minv.x = v->x;
    if (v->y < stats->minv.y) stats->minv.y = v->y;
    if (v->z < stats->minv.z) stats->minv.z = v->z;
    if (v->x > stats->maxv.x) stats->maxv.x = v->x;
    if (v->y > stats->maxv.y) stats->maxv.y = v->y;
    if (v->z > stats->maxv.z) stats->maxv.z = v->z;
}

void stl3d_stats_add(stl3d_stats *stats, const stl3d_triangle *tri)
{
    if (!stats || !tri) return;
    stl3d_stats_point(stats, &tri->v[0]);
    stl3d_stats_point(stats, &tri->v[1]);
    stl3d_stats_point(stats, &tri->v[2]);
    ++stats->triangles;
    if ((tri->flags & STL3D_TRI_FLAG_DEGENERATE) != 0U) ++stats->degenerate_triangles;
    if ((tri->flags & STL3D_TRI_FLAG_ZERO_NORMAL) != 0U) ++stats->zero_normals;
    if ((tri->flags & STL3D_TRI_FLAG_NORMAL_FLIPPED) != 0U) ++stats->normal_flipped;
    if ((tri->flags & STL3D_TRI_FLAG_NORMAL_MISMATCH) != 0U) ++stats->normal_mismatch;
    if ((tri->flags & STL3D_TRI_FLAG_ATTRIBUTE_NONZERO) != 0U) ++stats->nonzero_attributes;
    if ((tri->flags & STL3D_TRI_FLAG_NON_FINITE) != 0U) ++stats->non_finite_values;
    if ((tri->flags & STL3D_TRI_FLAG_SATURATED) != 0U) ++stats->saturated_values;
    if (tri->color_viscam.valid) ++stats->viscam_color_facets;
    if (tri->color_magics.valid && tri->color_magics.kind == STL3D_COLOR_MAGICS_FACET) ++stats->magics_color_facets;
}

int stl3d_decode_all(stl3d_ctx *ctx, stl3d_triangle_fn cb, void *cb_user, stl3d_stats *stats)
{
    stl3d_triangle tri;
    int rc;
    if (stats) stl3d_stats_init(stats);
    for (;;) {
        rc = stl3d_next_triangle(ctx, &tri);
        if (rc == STL3D_EOF) return STL3D_OK;
        if (rc != STL3D_OK) return rc;
        if (stats) stl3d_stats_add(stats, &tri);
        if (cb) {
            rc = cb(cb_user, &tri);
            if (rc != STL3D_OK) return rc;
        }
    }
}

int stl3d_get_magics_object_color(const stl3d_ctx *ctx, stl3d_color *out)
{
    if (!ctx || !out) return STL3D_ERR_ARG;
    *out = ctx->magics_object_color;
    return ctx->magics_object_color.valid ? STL3D_OK : STL3D_EOF;
}

int stl3d_get_binary_header(const stl3d_ctx *ctx, stl3d_u8 out80[80])
{
    unsigned long i;
    if (!ctx || !out80) return STL3D_ERR_ARG;
    if (!ctx->has_binary_header) return STL3D_EOF;
    i = 0UL;
    while (i < 80UL) {
        out80[i] = ctx->binary_header[i];
        ++i;
    }
    return STL3D_OK;
}

const char *stl3d_error_string(int code)
{
    switch (code) {
        case STL3D_OK: return "ok";
        case STL3D_EOF: return "end of file";
        case STL3D_ERR_ARG: return "bad argument";
        case STL3D_ERR_IO: return "read error";
        case STL3D_ERR_BUFFER: return "work buffer too small";
        case STL3D_ERR_FORMAT: return "format error";
        case STL3D_ERR_RANGE: return "numeric range error";
        case STL3D_ERR_OVERFLOW: return "size/count overflow";
        default: return "unknown error";
    }
}
