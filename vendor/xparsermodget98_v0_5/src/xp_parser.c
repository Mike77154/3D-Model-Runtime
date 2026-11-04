/* xp_parser.c - .x file parser (C89, arena/static storage)
   MIT License

   Supported:
   - Encodings: txt, bin, tzip, bzip (MSZIP)
   - Common D3D templates: Mesh (+Normals/UV/Colors/Materials), Frame (+Transform),
     Material (+TextureFilename), SkinWeights, AnimationSet/Animation/AnimationKey
   - Unknown objects are skipped by balancing braces.
*/
#include "xp_internal.h"

static xp_result xp_copyz(char* dst, unsigned int cap, const char* src);

/* ---------------- diagnostics / budgets ---------------- */
static void xp_diag_set_object(xp_context* ctx, const char* type, const char* name) {
    unsigned int i;
    if (!ctx) return;
    i = 0u;
    if (type) while (type[i] && i + 1u < XP_MAX_TOKEN_TEXT) { ctx->diag_type[i]=type[i]; i++; }
    ctx->diag_type[i]='\0';
    i = 0u;
    if (name) while (name[i] && i + 1u < XP_MAX_TOKEN_TEXT) { ctx->diag_name[i]=name[i]; i++; }
    ctx->diag_name[i]='\0';
}

static void xp_report_failure(xp_context* ctx, const xp_callbacks* cb, void* u,
                              xp_result result, const char* msg) {
    xp_diagnostic* d;
    if (!ctx || result == XP_OK) return;
    d = &ctx->last_diag;
    memset(d, 0, sizeof(*d));
    d->result = result;
    d->source_encoding = ctx->source_encoding;
    d->line = ctx->rd.line;
    d->col = ctx->rd.col;
    d->decoded_offset = 16UL + ctx->rd.decoded_offset;
    (void)xp_copyz(d->object_type, XP_MAX_TOKEN_TEXT, ctx->diag_type);
    (void)xp_copyz(d->object_name, XP_MAX_TOKEN_TEXT, ctx->diag_name);
    d->message = msg ? msg : xp_result_str(result);
    if (cb && cb->on_error) cb->on_error(u, d->line, d->col, d->message);
    if (cb && cb->on_diagnostic) cb->on_diagnostic(u, d);
}

static xp_result xp_budget_add(unsigned long* used, unsigned long amount, unsigned long limit) {
    if (!used) return XP_EFORMAT;
    if (amount > ULONG_MAX - *used) return XP_ELIMIT;
    if (limit != 0UL) {
        if (*used > limit || amount > limit - *used) return XP_ELIMIT;
    }
    *used += amount;
    return XP_OK;
}

static xp_result xp_budget_mul_add(unsigned long* used, unsigned long amount,
                                   unsigned long multiplier, unsigned long limit) {
    if (multiplier != 0UL && amount > ULONG_MAX / multiplier) return XP_ELIMIT;
    return xp_budget_add(used, amount * multiplier, limit);
}

static xp_result xp_copyz(char* dst, unsigned int cap, const char* src) {
    unsigned int i;
    if (!dst || cap == 0u) return XP_EFORMAT;
    i = 0u;
    if (src) {
        while (src[i] != '\0') {
            if (i + 1u >= cap) { dst[0] = '\0'; return XP_ELIMIT; }
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
    return XP_OK;
}

static xp_result xp_validate_string(const char* s) {
    unsigned int i;
    if (!s) return XP_EFORMAT;
    for (i = 0u; s[i] != '\0'; ++i) {
        unsigned char c;
        c = (unsigned char)s[i];
        if (c < 0x20u || c == 0x7fu) return XP_EFORMAT;
    }
    return XP_OK;
}

/* Some legacy DirectX exporters accidentally serialized a Windows path's
   "\\t" as an actual horizontal-tab byte inside TextureFilename.
   Texture filenames are opaque parser data, not opened by this library, so
   accept TAB only for this field while keeping all other controls rejected. */
static xp_result xp_validate_texture_string(const char* s) {
    unsigned int i;
    if (!s) return XP_EFORMAT;
    for (i = 0u; s[i] != '\0'; ++i) {
        unsigned char c;
        c = (unsigned char)s[i];
        if ((c < 0x20u && c != 0x09u) || c == 0x7fu) return XP_EFORMAT;
    }
    return XP_OK;
}

static int xp_u32_fits_int(unsigned int v) {
    return v <= (unsigned int)INT_MAX;
}

#if XP_ENABLE_BINARY
static int xp_u32_mul_ok(unsigned int v, unsigned int m) {
    return m == 0u || v <= UINT_MAX / m;
}

static int xp_u32_add_ok(unsigned int v, unsigned int a) {
    return v <= UINT_MAX - a;
}
#endif

const char* xp_result_str(xp_result r) {
    switch (r) {
        case XP_OK: return "XP_OK";
        case XP_EOF: return "XP_EOF";
        case XP_EIO: return "XP_EIO";
        case XP_EFORMAT: return "XP_EFORMAT";
        case XP_ELIMIT: return "XP_ELIMIT";
        case XP_ENOTSUP: return "XP_ENOTSUP";
        default: return "XP_???";
    }
}

/* ---------------- header / providers ---------------- */
static xp_result xp_file_read(void* user, unsigned char* dst, unsigned int cap, unsigned int* out_n) {
    FILE* f;
    size_t n;
    if (!user || !dst || !out_n) return XP_EFORMAT;
    f = (FILE*)user;
    n = fread(dst, 1, (size_t)cap, f);
    *out_n = (unsigned int)n;
    if (n != 0u) return XP_OK;
    if (ferror(f)) return XP_EIO;
    return XP_OK;
}

static xp_result xp_memory_read(void* user, unsigned char* dst, unsigned int cap, unsigned int* out_n) {
    xp_memory_io* m;
    unsigned long left;
    unsigned int n;
    if (!user || !dst || !out_n) return XP_EFORMAT;
    m = (xp_memory_io*)user;
    if (m->pos > m->size) return XP_EFORMAT;
    left = m->size - m->pos;
    n = cap;
    if ((unsigned long)n > left) n = (unsigned int)left;
    if (n != 0u) memcpy(dst, m->data + m->pos, (size_t)n);
    m->pos += (unsigned long)n;
    *out_n = n;
    return XP_OK;
}

void xp_io_init_memory(xp_io* io, xp_memory_io* state,
                       const unsigned char* data, unsigned long size) {
    if (!io || !state) return;
    state->data = data;
    state->size = size;
    state->pos = 0UL;
    io->user = state;
    io->read = xp_memory_read;
}

void xp_limits_unlimited(xp_limits* limits) {
    if (limits) memset(limits, 0, sizeof(*limits));
}

xp_result xp_set_limits(xp_context* ctx, const xp_limits* limits) {
    if (!ctx || !limits) return XP_EFORMAT;
    ctx->limits = *limits;
    ctx->rd.max_decoded_bytes = limits->max_decoded_bytes;
    return XP_OK;
}

void xp_get_usage(const xp_context* ctx, xp_usage* out_usage) {
    if (!ctx || !out_usage) return;
    *out_usage = ctx->usage;
    out_usage->decoded_bytes = ctx->rd.decoded_offset;
}

const xp_diagnostic* xp_get_last_diagnostic(const xp_context* ctx) {
    if (!ctx || ctx->last_diag.result == XP_OK) return (const xp_diagnostic*)0;
    return &ctx->last_diag;
}

static xp_result read_header_io(xp_io* io, xp_header* h) {
    unsigned char buf[16];
    xp_result rr;
    memset(h, 0, sizeof(*h));
    rr = xpio_read_exact(io, buf, 16u);
    if (rr != XP_OK) return rr == XP_EOF ? XP_EFORMAT : rr;

    if (!(buf[0]=='x' && buf[1]=='o' && buf[2]=='f' && buf[3]==' ')) return XP_EFORMAT;
    h->magic[0]='x'; h->magic[1]='o'; h->magic[2]='f'; h->magic[3]='\0';

    if (!(buf[4]>='0' && buf[4]<='9' && buf[5]>='0' && buf[5]<='9' &&
          buf[6]>='0' && buf[6]<='9' && buf[7]>='0' && buf[7]<='9')) return XP_EFORMAT;
    h->major = (buf[4]-'0') * 10 + (buf[5]-'0');
    h->minor = (buf[6]-'0') * 10 + (buf[7]-'0');

    if (memcmp(buf+8, "txt ", 4)==0) h->enc = XP_ENC_TXT;
    else if (memcmp(buf+8, "bin ", 4)==0) h->enc = XP_ENC_BIN;
    else if (memcmp(buf+8, "tzip", 4)==0) h->enc = XP_ENC_TZIP;
    else if (memcmp(buf+8, "bzip", 4)==0) h->enc = XP_ENC_BZIP;
    else return XP_EFORMAT;

    if (memcmp(buf+12, "0032", 4)==0) h->numfmt = XP_NUM_32;
    else if (memcmp(buf+12, "0064", 4)==0) h->numfmt = XP_NUM_64;
    else return XP_EFORMAT;

    return XP_OK;
}

xp_result xp_open_io(xp_context* ctx, const xp_io* io, xp_header* out_hdr) {
    xp_result rr;
#if XP_ENABLE_MSZIP
    xp_encoding packed_enc;
#endif

    if (!ctx || !io || !io->read) return XP_EFORMAT;
    memset(ctx, 0, sizeof(*ctx));
    ctx->current_frame_id = XP_FRAME_ID_NONE;
    ctx->next_frame_id = 0u;
    ctx->io = *io;

    rr = read_header_io(&ctx->io, &ctx->hdr);
    if (rr != XP_OK) return rr;
    ctx->source_encoding = ctx->hdr.enc;

#if !XP_ENABLE_TEXT
    if (ctx->hdr.enc == XP_ENC_TXT || ctx->hdr.enc == XP_ENC_TZIP) return XP_ENOTSUP;
#endif
#if !XP_ENABLE_BINARY
    if (ctx->hdr.enc == XP_ENC_BIN || ctx->hdr.enc == XP_ENC_BZIP) return XP_ENOTSUP;
#endif

    rr = xpr_init(&ctx->rd, &ctx->io);
    if (rr != XP_OK) return rr;

#if XP_ENABLE_MSZIP
    if (ctx->hdr.enc == XP_ENC_TZIP || ctx->hdr.enc == XP_ENC_BZIP) {
        packed_enc = ctx->hdr.enc;
        rr = xpm_init(&ctx->msz, &ctx->io);
        if (rr != XP_OK) return rr;
        ctx->rd.use_mszip = 1;
        ctx->rd.mszip = &ctx->msz;
        ctx->hdr.enc = (packed_enc == XP_ENC_TZIP) ? XP_ENC_TXT : XP_ENC_BIN;
    }
#else
    if (ctx->hdr.enc == XP_ENC_TZIP || ctx->hdr.enc == XP_ENC_BZIP) return XP_ENOTSUP;
#endif

    if (out_hdr) *out_hdr = ctx->hdr;
    rr = xpl_init(&ctx->lx, &ctx->rd);
    if (rr != XP_OK) return rr;
    rr = xpb_init(&ctx->blx, &ctx->rd, ctx->hdr.numfmt);
    if (rr != XP_OK) return rr;

    ctx->b_has_la = 0;
    ctx->depth = 0;
    xp_limits_unlimited(&ctx->limits);
    memset(&ctx->usage, 0, sizeof(ctx->usage));
    memset(&ctx->last_diag, 0, sizeof(ctx->last_diag));
    xp_diag_set_object(ctx, "", "");
    return XP_OK;
}

xp_result xp_open_file(xp_context* ctx, FILE* f, xp_header* out_hdr) {
    xp_io io;
    if (!f) return XP_EFORMAT;
    io.user = f;
    io.read = xp_file_read;
    return xp_open_io(ctx, &io, out_hdr);
}

void xp_close(xp_context* ctx) { (void)ctx; }

/* ---------------- text helpers ---------------- */
static xp_result t_expect(xp_context* ctx, xp_tok_kind k, xp_token* t) {
    xp_result rr = xpl_next(&ctx->lx, t);
    if (rr != XP_OK) return rr;
    return (t->kind == k) ? XP_OK : XP_EFORMAT;
}

static xp_result xp_parse_i32_text(const char* text, int* out_i) {
    const char* p;
    long v;
    int neg;
    int any;

    if (!text || !out_i) return XP_EFORMAT;
    p = text;
    neg = 0;
    any = 0;
    v = 0;

    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        p++;
    }

    while (*p >= '0' && *p <= '9') {
        int d;
        d = (int)(*p - '0');
        any = 1;
        if (v > (2147483647L - d) / 10L) return XP_ELIMIT;
        v = v * 10L + (long)d;
        p++;
    }

    if (!any || *p != '\0') return XP_EFORMAT;
    if (neg) v = -v;
    *out_i = (int)v;
    return XP_OK;
}

static xp_fx32 xp_fx_sat_mag(unsigned long mag, int neg) {
    if (neg) {
        if (mag >= 0x80000000UL) return XP_FX_MIN;
        return (xp_fx32)(-((xp_fx32)mag));
    }
    if (mag > 0x7fffffffUL) return XP_FX_MAX;
    return (xp_fx32)mag;
}

static xp_fx32 xp_fx_mul10_sat(xp_fx32 v) {
    if (v > (XP_FX_MAX / 10L)) return XP_FX_MAX;
    if (v < (XP_FX_MIN / 10L)) return XP_FX_MIN;
    return (xp_fx32)(v * 10L);
}

static xp_result xp_parse_fx_text(const char* text, xp_fx32* out_fx) {
    const char* p;
    unsigned long ip;
    unsigned long mag;
    unsigned long place;
    int neg;
    int any;
    int exp_neg;
    int exp_val;

    if (!text || !out_fx) return XP_EFORMAT;
    p = text;
    ip = 0UL;
    mag = 0UL;
    place = (unsigned long)XP_FX_ONE / 10UL;
    neg = 0;
    any = 0;
    exp_neg = 0;
    exp_val = 0;

    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        p++;
    }

    while (*p >= '0' && *p <= '9') {
        unsigned int d;
        d = (unsigned int)(*p - '0');
        any = 1;
        if (ip > ((0x7fffffffUL >> XP_FX_SHIFT) - d) / 10UL) return XP_ELIMIT;
        ip = ip * 10UL + (unsigned long)d;
        p++;
    }

    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            unsigned int d;
            d = (unsigned int)(*p - '0');
            any = 1;
            if (place != 0UL) {
                unsigned long add;
                add = (unsigned long)d * place;
                if (mag > 0x7fffffffUL - add) mag = 0x7fffffffUL;
                else mag += add;
                place = place / 10UL;
            }
            p++;
        }
    }

    if (!any) return XP_EFORMAT;

    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') {
            exp_neg = (*p == '-');
            p++;
        }
        if (!(*p >= '0' && *p <= '9')) return XP_EFORMAT;
        while (*p >= '0' && *p <= '9') {
            int d;
            d = (int)(*p - '0');
            if (exp_val < 64) exp_val = exp_val * 10 + d;
            p++;
        }
    }

    if (*p != '\0') return XP_EFORMAT;

    if (ip > (0x7fffffffUL >> XP_FX_SHIFT)) return XP_ELIMIT;
    mag += (ip << XP_FX_SHIFT);
    if (mag > 0x7fffffffUL) mag = 0x7fffffffUL;

    *out_fx = xp_fx_sat_mag(mag, neg);

    while (exp_val > 0) {
        if (exp_neg) *out_fx = (xp_fx32)(*out_fx / 10L);
        else *out_fx = xp_fx_mul10_sat(*out_fx);
        exp_val--;
    }

    return XP_OK;
}

static xp_result t_read_int(xp_context* ctx, int* out_i) {
    xp_token t;
    xp_result rr;
    rr = xpl_next(&ctx->lx, &t);
    if (rr != XP_OK) return rr;
    if (t.kind != XP_TOK_NUMBER) return XP_EFORMAT;
    return xp_parse_i32_text(t.text, out_i);
}

static xp_result t_read_u32(xp_context* ctx, unsigned int* out_u) {
    xp_token t;
    const char* p;
    unsigned long v;
    xp_result rr;
    rr = xpl_next(&ctx->lx, &t);
    if (rr != XP_OK) return rr;
    if (t.kind != XP_TOK_NUMBER || !out_u) return XP_EFORMAT;
    p = t.text;
    if (*p == '+') p++;
    if (*p == '-' || !(*p >= '0' && *p <= '9')) return XP_EFORMAT;
    v = 0UL;
    while (*p >= '0' && *p <= '9') {
        unsigned int d;
        d = (unsigned int)(*p - '0');
        if (v > ((unsigned long)UINT_MAX - (unsigned long)d) / 10UL) return XP_ELIMIT;
        v = v * 10UL + (unsigned long)d;
        p++;
    }
    if (*p != '\0') return XP_EFORMAT;
    *out_u = (unsigned int)v;
    return XP_OK;
}

static xp_result t_read_real(xp_context* ctx, xp_fx32* out_f) {
    xp_token t;
    xp_result rr;
    rr = xpl_next(&ctx->lx, &t);
    if (rr != XP_OK) return rr;
    if (t.kind != XP_TOK_NUMBER) return XP_EFORMAT;
    return xp_parse_fx_text(t.text, out_f);
}

static xp_result t_read_string(xp_context* ctx, char* out, unsigned int cap) {
    xp_token t;
    xp_result rr = xpl_next(&ctx->lx, &t);
    if (rr != XP_OK) return rr;
    if (t.kind != XP_TOK_STRING) return XP_EFORMAT;
    if (cap == 0u) return XP_EFORMAT;
    rr = xp_validate_string(t.text);
    if (rr != XP_OK) return rr;
    return xp_copyz(out, cap, t.text);
}

static xp_result t_read_texture_string(xp_context* ctx, char* out, unsigned int cap) {
    xp_token t;
    xp_result rr = xpl_next(&ctx->lx, &t);
    if (rr != XP_OK) return rr;
    if (t.kind != XP_TOK_STRING) return XP_EFORMAT;
    if (cap == 0u) return XP_EFORMAT;
    rr = xp_validate_texture_string(t.text);
    if (rr != XP_OK) return rr;
    return xp_copyz(out, cap, t.text);
}

static void t_pushback(xp_context* ctx, const xp_token* t) {
    ctx->lx.la = *t;
    ctx->lx.has_la = 1;
}

/* Consume any number of ',' or ';' tokens, leaving the first non-delim in lookahead. */
static xp_result t_consume_delims(xp_context* ctx) {
    xp_token t;
    xp_result rr;
    while (1) {
        rr = xpl_next(&ctx->lx, &t);
        if (rr != XP_OK) return rr;
        if (t.kind == XP_TOK_COMMA || t.kind == XP_TOK_SEMI) continue;
        t_pushback(ctx, &t);
        return XP_OK;
    }
}

/* Legacy exporters disagree on whether face-index elements are separated by
   commas or semicolons.  Both spellings are unambiguous inside a counted
   index list, so accept either between elements. */
static xp_result t_expect_index_sep(xp_context* ctx) {
    xp_token t;
    xp_result rr;
    rr = xpl_next(&ctx->lx, &t);
    if (rr != XP_OK) return rr;
    if (t.kind != XP_TOK_COMMA && t.kind != XP_TOK_SEMI) return XP_EFORMAT;
    return XP_OK;
}

static xp_result t_maybe_class_id(xp_context* ctx, const xp_callbacks* cb, void* user, const char* type, const char* name) {
    xp_token t;
    xp_result rr = xpl_next(&ctx->lx, &t);
    if (rr != XP_OK) return rr;
    if (t.kind == XP_TOK_GUID) {
        if (cb && cb->on_object_guid) cb->on_object_guid(user, type, name ? name : "", t.text);
        return XP_OK;
    }
    t_pushback(ctx, &t);
    return XP_OK;
}

static void t_emit_unknown(const xp_callbacks* cb, void* user, const xp_token* t) {
    xp_unknown_token u;
    if (!cb || !cb->on_unknown_token || !t) return;
    memset(&u, 0, sizeof(u));
    u.raw_id = (unsigned int)t->kind;
    u.text = t->text;
    switch (t->kind) {
        case XP_TOK_IDENT: u.kind = XP_UNKNOWN_IDENT; break;
        case XP_TOK_STRING: u.kind = XP_UNKNOWN_STRING; break;
        case XP_TOK_NUMBER: u.kind = XP_UNKNOWN_NUMBER_TEXT; break;
        case XP_TOK_GUID: u.kind = XP_UNKNOWN_GUID; break;
        case XP_TOK_LBRACE: u.kind = XP_UNKNOWN_LBRACE; break;
        case XP_TOK_RBRACE: u.kind = XP_UNKNOWN_RBRACE; break;
        case XP_TOK_SEMI: u.kind = XP_UNKNOWN_SEMI; break;
        case XP_TOK_COMMA: u.kind = XP_UNKNOWN_COMMA; break;
        case XP_TOK_EOF: u.kind = XP_UNKNOWN_EOF; break;
        default: u.kind = XP_UNKNOWN_OTHER; break;
    }
    cb->on_unknown_token(user, &u);
}

/* Skip an unknown object block while streaming its semantic tokens. Assumes
   next token is '{'. */
static xp_result t_skip_block(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_token t;
    int depth;
    xp_result rr;
    depth = 0;
    while (1) {
        rr = xpl_next(&ctx->lx, &t);
        if (rr != XP_OK) return rr;
        if (t.kind == XP_TOK_EOF) return XP_EFORMAT;
        t_emit_unknown(cb, user, &t);
        if (t.kind == XP_TOK_LBRACE) {
            depth++;
            if (depth > (int)XP_MAX_DEPTH) return XP_ELIMIT;
        } else if (t.kind == XP_TOK_RBRACE) {
            depth--;
            if (depth <= 0) return XP_OK;
        }
    }
}

static xp_result t_expect_lbrace(xp_context* ctx);

/* Parse an object reference: { Name }.  The opening brace must be next. */
static xp_result t_parse_data_reference(xp_context* ctx, const xp_callbacks* cb, void* user,
                                        int animation_target) {
    xp_token t;
    xp_result rr;
    char name[XP_MAX_TOKEN_TEXT];

    rr = t_expect_lbrace(ctx); if (rr != XP_OK) return rr;
    rr = xpl_next(&ctx->lx, &t); if (rr != XP_OK) return rr;
    if (t.kind != XP_TOK_IDENT) return XP_EFORMAT;
    rr = xp_copyz(name, XP_MAX_TOKEN_TEXT, t.text); if (rr != XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_RBRACE, &t); if (rr != XP_OK) return rr;
    ctx->depth--;

    if (cb && cb->on_data_reference) cb->on_data_reference(user, name);
    if (animation_target && cb && cb->on_animation_target) cb->on_animation_target(user, name);
    return XP_OK;
}

/* Forward decl (text) */
static xp_result t_parse_object(xp_context* ctx, const xp_callbacks* cb, void* user);

#if XP_ENABLE_BINARY
/* ---------------- binary helpers ---------------- */
static void b_pushback(xp_context* ctx, const xp_btoken* t) {
    ctx->bla = *t;
    ctx->b_has_la = 1;
}

static xp_result b_next(xp_context* ctx, xp_btoken* out) {
    if (ctx->b_has_la) {
        *out = ctx->bla;
        ctx->b_has_la = 0;
        return XP_OK;
    }
    return xpb_next(&ctx->blx, out);
}

static xp_result b_consume_delims(xp_context* ctx) {
    xp_btoken t;
    xp_result rr;
    while (1) {
        rr = b_next(ctx, &t);
        if (rr != XP_OK) return rr;
        if (t.kind == XP_BTOK_COMMA || t.kind == XP_BTOK_SEMI) continue;
        b_pushback(ctx, &t);
        return XP_OK;
    }
}

static xp_result b_maybe_class_id(xp_context* ctx, const xp_callbacks* cb, void* user, const char* type, const char* name) {
    xp_btoken t;
    xp_result rr;
    rr = b_next(ctx, &t);
    if (rr != XP_OK) return rr;
    if (t.kind == XP_BTOK_GUID) {
        if (cb && cb->on_object_guid) cb->on_object_guid(user, type, name ? name : "", t.text);
        return XP_OK;
    }
    b_pushback(ctx, &t);
    return XP_OK;
}

static void b_emit_unknown(const xp_callbacks* cb, void* user, const xp_btoken* t) {
    xp_unknown_token u;
    if (!cb || !cb->on_unknown_token || !t) return;
    memset(&u, 0, sizeof(u));
    u.raw_id = (unsigned int)t->raw_id;
    u.text = t->text;
    switch (t->kind) {
        case XP_BTOK_NAME: u.kind = XP_UNKNOWN_IDENT; break;
        case XP_BTOK_STRING: u.kind = XP_UNKNOWN_STRING; break;
        case XP_BTOK_GUID: u.kind = XP_UNKNOWN_GUID; break;
        case XP_BTOK_INTEGER: u.kind = XP_UNKNOWN_INTEGER; u.u32 = t->u32; break;
        case XP_BTOK_REAL: u.kind = XP_UNKNOWN_REAL; u.real = t->rtmp[0]; break;
        case XP_BTOK_OBRACE: u.kind = XP_UNKNOWN_LBRACE; break;
        case XP_BTOK_CBRACE: u.kind = XP_UNKNOWN_RBRACE; break;
        case XP_BTOK_SEMI: u.kind = XP_UNKNOWN_SEMI; break;
        case XP_BTOK_COMMA: u.kind = XP_UNKNOWN_COMMA; break;
        case XP_BTOK_EOF: u.kind = XP_UNKNOWN_EOF; break;
        default: u.kind = XP_UNKNOWN_OTHER; break;
    }
    cb->on_unknown_token(user, &u);
}

/* Skip unknown binary block while streaming flattened semantic tokens. */
static xp_result b_skip_block(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_btoken t;
    int depth;
    xp_result rr;
    depth = 0;
    while (1) {
        rr = b_next(ctx, &t);
        if (rr != XP_OK) return rr;
        if (t.kind == XP_BTOK_EOF) return XP_EFORMAT;
        b_emit_unknown(cb, user, &t);
        if (t.kind == XP_BTOK_OBRACE) {
            depth++;
            if (depth > (int)XP_MAX_DEPTH) return XP_ELIMIT;
        } else if (t.kind == XP_BTOK_CBRACE) {
            depth--;
            if (depth <= 0) return XP_OK;
        } else if (t.kind == XP_BTOK_INTEGER_LIST) {
            rr = xpb_discard_u32_list(&ctx->blx, t.count);
            if (rr != XP_OK) return rr;
        } else if (t.kind == XP_BTOK_REAL_LIST) {
            rr = xpb_discard_real_list(&ctx->blx, t.count);
            if (rr != XP_OK) return rr;
        }
    }
}

static xp_result b_expect_obrace(xp_context* ctx);

/* Parse a binary object reference: OBRACE NAME CBRACE. */
static xp_result b_parse_data_reference(xp_context* ctx, const xp_callbacks* cb, void* user,
                                        int animation_target) {
    xp_btoken t;
    xp_result rr;
    char name[XP_MAX_TOKEN_TEXT];

    rr = b_expect_obrace(ctx); if (rr != XP_OK) return rr;
    rr = b_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr != XP_OK) return rr;
    if (t.kind != XP_BTOK_NAME) return XP_EFORMAT;
    rr = xp_copyz(name, XP_MAX_TOKEN_TEXT, t.text); if (rr != XP_OK) return rr;
    rr = b_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr != XP_OK) return rr;
    if (t.kind != XP_BTOK_CBRACE) return XP_EFORMAT;
    ctx->depth--;

    if (cb && cb->on_data_reference) cb->on_data_reference(user, name);
    if (animation_target && cb && cb->on_animation_target) cb->on_animation_target(user, name);
    return XP_OK;
}

/* Forward decl (binary) */
static xp_result b_parse_object(xp_context* ctx, const xp_callbacks* cb, void* user);
#endif /* XP_ENABLE_BINARY */

/* ---------------- template parsers (text) ---------------- */
static xp_result t_parse_mesh(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name);
static xp_result t_parse_mesh_normals(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_mesh_texcoords(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_mesh_colors(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_mesh_material_list(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_material(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name);
static xp_result t_parse_texture_filename(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_material_wrap(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_mesh_face_wraps(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_frame(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name);
static xp_result t_parse_frame_transform(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_skin_weights(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_skin_mesh_header(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_animation_set(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name);
static xp_result t_parse_animation(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name);
static xp_result t_parse_animation_key(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_anim_ticks_per_second(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_animation_options(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_vertex_duplication_indices(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_decl_data(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result t_parse_fvf_data(xp_context* ctx, const xp_callbacks* cb, void* user);

#if XP_ENABLE_BINARY
/* ---------------- template parsers (binary) ---------------- */
static xp_result b_parse_mesh(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name);
static xp_result b_parse_mesh_normals(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_mesh_texcoords(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_mesh_colors(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_mesh_material_list(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_material(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name);
static xp_result b_parse_texture_filename(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_material_wrap(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_mesh_face_wraps(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_frame(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name);
static xp_result b_parse_frame_transform(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_skin_weights(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_skin_mesh_header(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_animation_set(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name);
static xp_result b_parse_animation(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name);
static xp_result b_parse_animation_key(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_anim_ticks_per_second(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_animation_options(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_vertex_duplication_indices(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_decl_data(xp_context* ctx, const xp_callbacks* cb, void* user);
static xp_result b_parse_fvf_data(xp_context* ctx, const xp_callbacks* cb, void* user);
#endif /* XP_ENABLE_BINARY */

/* ---------------- dispatch (text) ---------------- */
static xp_result t_dispatch(xp_context* ctx, const xp_callbacks* cb, void* user, const char* type, const char* name) {
    if (xp_is_mesh_type(type)) return t_parse_mesh(ctx, cb, user, name);
    if (xp_is_mesh_normals_type(type)) return t_parse_mesh_normals(ctx, cb, user);
    if (xp_is_mesh_texcoords_type(type)) return t_parse_mesh_texcoords(ctx, cb, user);
    if (xp_is_mesh_colors_type(type)) return t_parse_mesh_colors(ctx, cb, user);
    if (xp_is_mesh_material_list_type(type)) return t_parse_mesh_material_list(ctx, cb, user);
    if (xp_is_material_type(type)) return t_parse_material(ctx, cb, user, name);
    if (xp_is_texture_filename_type(type)) return t_parse_texture_filename(ctx, cb, user);
    if (xp_is_material_wrap_type(type)) return t_parse_material_wrap(ctx, cb, user);
    if (xp_is_mesh_face_wraps_type(type)) return t_parse_mesh_face_wraps(ctx, cb, user);
    if (xp_is_frame_type(type)) return t_parse_frame(ctx, cb, user, name);
    if (xp_is_frame_transform_matrix_type(type)) return t_parse_frame_transform(ctx, cb, user);
    if (xp_is_skin_weights_type(type)) return t_parse_skin_weights(ctx, cb, user);
    if (xp_is_skin_mesh_header_type(type)) return t_parse_skin_mesh_header(ctx, cb, user);
    if (xp_is_anim_ticks_per_second_type(type)) return t_parse_anim_ticks_per_second(ctx, cb, user);
    if (xp_is_animation_options_type(type)) return t_parse_animation_options(ctx, cb, user);
    if (xp_is_animation_set_type(type)) return t_parse_animation_set(ctx, cb, user, name);
    if (xp_is_animation_type(type)) return t_parse_animation(ctx, cb, user, name);
    if (xp_is_animation_key_type(type)) return t_parse_animation_key(ctx, cb, user);
    if (xp_is_vertex_duplication_indices_type(type)) return t_parse_vertex_duplication_indices(ctx, cb, user);
    if (xp_is_decl_data_type(type)) return t_parse_decl_data(ctx, cb, user);
    if (xp_is_fvf_data_type(type)) return t_parse_fvf_data(ctx, cb, user);

    if (cb && cb->on_unknown_object_begin) cb->on_unknown_object_begin(user, type, name ? name : "");
    {
        xp_result ur;
        ur = t_skip_block(ctx, cb, user);
        if (ur != XP_OK) return ur;
    }
    if (cb && cb->on_unknown_object_end) cb->on_unknown_object_end(user, type, name ? name : "");
    return XP_OK;
}

static xp_result t_parse_object(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_token t;
    char type_name[XP_MAX_TOKEN_TEXT];
    char obj_name[XP_MAX_TOKEN_TEXT];

    rr = xpl_next(&ctx->lx, &t);
    if (rr != XP_OK) return rr;
    if (t.kind != XP_TOK_IDENT) return XP_EFORMAT;
    rr = xp_copyz(type_name, XP_MAX_TOKEN_TEXT, t.text); if (rr != XP_OK) return rr;

    rr = xpl_next(&ctx->lx, &t);
    if (rr != XP_OK) return rr;
    obj_name[0] = '\0';
    if (t.kind == XP_TOK_IDENT) {
        rr = xp_copyz(obj_name, XP_MAX_TOKEN_TEXT, t.text); if (rr != XP_OK) return rr;
        rr = xpl_next(&ctx->lx, &t);
        if (rr != XP_OK) return rr;
    }

    if (t.kind != XP_TOK_LBRACE) return XP_EFORMAT;

    rr = xp_budget_add(&ctx->usage.objects, 1UL, ctx->limits.max_objects);
    if (rr != XP_OK) return rr;
    xp_diag_set_object(ctx, type_name, obj_name);
    if (cb && cb->on_begin_object) cb->on_begin_object(user, type_name, obj_name);

    /* handler expects '{' next */
    t_pushback(ctx, &t);
    rr = t_dispatch(ctx, cb, user, type_name, obj_name);
    if (rr != XP_OK) return rr;

    if (cb && cb->on_end_object) cb->on_end_object(user, type_name, obj_name);
    return XP_OK;
}

#if XP_ENABLE_BINARY
/* ---------------- dispatch (binary) ---------------- */
static const char* b_prim_name(unsigned short raw_id) {
    /* Token ids for primitive_type identifiers used in templates. */
    switch (raw_id) {
        case 40: return "WORD";
        case 41: return "DWORD";
        case 42: return "REAL32";
        case 43: return "REAL64";
        case 44: return "CHAR";
        case 45: return "UCHAR";
        case 46: return "SWORD";
        case 47: return "SDWORD";
        case 48: return "VOID";
        case 49: return "LPSTR";
        case 50: return "UNICODE";
        case 51: return "CSTRING";
        case 52: return "array";
        default: return "";
    }
}

static xp_result b_dispatch(xp_context* ctx, const xp_callbacks* cb, void* user, const char* type, const char* name) {
    if (xp_is_mesh_type(type)) return b_parse_mesh(ctx, cb, user, name);
    if (xp_is_mesh_normals_type(type)) return b_parse_mesh_normals(ctx, cb, user);
    if (xp_is_mesh_texcoords_type(type)) return b_parse_mesh_texcoords(ctx, cb, user);
    if (xp_is_mesh_colors_type(type)) return b_parse_mesh_colors(ctx, cb, user);
    if (xp_is_mesh_material_list_type(type)) return b_parse_mesh_material_list(ctx, cb, user);
    if (xp_is_material_type(type)) return b_parse_material(ctx, cb, user, name);
    if (xp_is_texture_filename_type(type)) return b_parse_texture_filename(ctx, cb, user);
    if (xp_is_material_wrap_type(type)) return b_parse_material_wrap(ctx, cb, user);
    if (xp_is_mesh_face_wraps_type(type)) return b_parse_mesh_face_wraps(ctx, cb, user);
    if (xp_is_frame_type(type)) return b_parse_frame(ctx, cb, user, name);
    if (xp_is_frame_transform_matrix_type(type)) return b_parse_frame_transform(ctx, cb, user);
    if (xp_is_skin_weights_type(type)) return b_parse_skin_weights(ctx, cb, user);
    if (xp_is_skin_mesh_header_type(type)) return b_parse_skin_mesh_header(ctx, cb, user);
    if (xp_is_anim_ticks_per_second_type(type)) return b_parse_anim_ticks_per_second(ctx, cb, user);
    if (xp_is_animation_options_type(type)) return b_parse_animation_options(ctx, cb, user);
    if (xp_is_animation_set_type(type)) return b_parse_animation_set(ctx, cb, user, name);
    if (xp_is_animation_type(type)) return b_parse_animation(ctx, cb, user, name);
    if (xp_is_animation_key_type(type)) return b_parse_animation_key(ctx, cb, user);
    if (xp_is_vertex_duplication_indices_type(type)) return b_parse_vertex_duplication_indices(ctx, cb, user);
    if (xp_is_decl_data_type(type)) return b_parse_decl_data(ctx, cb, user);
    if (xp_is_fvf_data_type(type)) return b_parse_fvf_data(ctx, cb, user);

    if (cb && cb->on_unknown_object_begin) cb->on_unknown_object_begin(user, type, name ? name : "");
    {
        xp_result ur;
        ur = b_skip_block(ctx, cb, user);
        if (ur != XP_OK) return ur;
    }
    if (cb && cb->on_unknown_object_end) cb->on_unknown_object_end(user, type, name ? name : "");
    return XP_OK;
}

static xp_result b_parse_object(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_btoken t;
    char type_name[XP_MAX_TOKEN_TEXT];
    char obj_name[XP_MAX_TOKEN_TEXT];

    rr = b_next(ctx, &t);
    if (rr != XP_OK) return rr;

    if (t.kind == XP_BTOK_TEMPLATE) {
        char template_name[XP_MAX_TOKEN_TEXT];
        template_name[0] = '\0';
        rr = b_consume_delims(ctx); if (rr != XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr != XP_OK) return rr;
        if (t.kind != XP_BTOK_NAME) return XP_EFORMAT;
        rr = xp_copyz(template_name, XP_MAX_TOKEN_TEXT, t.text); if (rr != XP_OK) return rr;
        rr = b_consume_delims(ctx); if (rr != XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr != XP_OK) return rr;
        if (t.kind != XP_BTOK_OBRACE) return XP_EFORMAT;
        rr = xp_budget_add(&ctx->usage.objects, 1UL, ctx->limits.max_objects);
        if (rr != XP_OK) return rr;
        xp_diag_set_object(ctx, "template", template_name);
        if (cb && cb->on_begin_object) cb->on_begin_object(user, "template", template_name);
        if (cb && cb->on_unknown_object_begin) cb->on_unknown_object_begin(user, "template", template_name);
        b_pushback(ctx, &t);
        rr = b_skip_block(ctx, cb, user);
        if (rr != XP_OK) return rr;
        if (cb && cb->on_unknown_object_end) cb->on_unknown_object_end(user, "template", template_name);
        if (cb && cb->on_end_object) cb->on_end_object(user, "template", template_name);
        return XP_OK;
    }

    /* identifier */
    type_name[0] = '\0';
    if (t.kind == XP_BTOK_NAME) {
        rr = xp_copyz(type_name, XP_MAX_TOKEN_TEXT, t.text); if (rr != XP_OK) return rr;
    } else if (t.kind == XP_BTOK_OTHER) {
        const char* pn = b_prim_name(t.raw_id);
        rr = xp_copyz(type_name, XP_MAX_TOKEN_TEXT, pn); if (rr != XP_OK) return rr;
    } else {
        return XP_EFORMAT;
    }

    /* optional object name */
    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    obj_name[0] = '\0';
    if (t.kind == XP_BTOK_NAME) {
        rr = xp_copyz(obj_name, XP_MAX_TOKEN_TEXT, t.text); if (rr != XP_OK) return rr;
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    }

    if (t.kind != XP_BTOK_OBRACE) return XP_EFORMAT;

    rr = xp_budget_add(&ctx->usage.objects, 1UL, ctx->limits.max_objects);
    if (rr != XP_OK) return rr;
    xp_diag_set_object(ctx, type_name, obj_name);
    if (cb && cb->on_begin_object) cb->on_begin_object(user, type_name, obj_name);

    b_pushback(ctx, &t);
    rr = b_dispatch(ctx, cb, user, type_name, obj_name);
    if (rr != XP_OK) return rr;

    if (cb && cb->on_end_object) cb->on_end_object(user, type_name, obj_name);
    return XP_OK;
}
#endif /* XP_ENABLE_BINARY */

/* ---------------- public parse entry ---------------- */
xp_result xp_parse(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;

    if (!ctx) return XP_EFORMAT;
    memset(&ctx->last_diag, 0, sizeof(ctx->last_diag));
    if (ctx->hdr.enc != XP_ENC_TXT && ctx->hdr.enc != XP_ENC_BIN) {
        rr = XP_ENOTSUP;
        goto fail;
    }

    if (ctx->hdr.enc == XP_ENC_TXT) {
        xp_token t;
        while (1) {
            rr = xpl_next(&ctx->lx, &t);
            if (rr != XP_OK) goto fail;
            if (t.kind == XP_TOK_EOF) {
                if (ctx->depth != 0) { rr = XP_EFORMAT; goto fail; }
                ctx->usage.decoded_bytes = ctx->rd.decoded_offset;
                return XP_OK;
            }
            if (t.kind == XP_TOK_SEMI || t.kind == XP_TOK_COMMA) continue;
            if (t.kind != XP_TOK_IDENT) { rr = XP_EFORMAT; goto fail; }
            t_pushback(ctx, &t);
            rr = t_parse_object(ctx, cb, user);
            if (rr != XP_OK) goto fail;
        }
    }

#if !XP_ENABLE_BINARY
    (void)cb; (void)user;
    rr = XP_ENOTSUP;
    goto fail;
#else
    {
        xp_btoken t;
        while (1) {
            rr = b_next(ctx, &t);
            if (rr != XP_OK) goto fail;
            if (t.kind == XP_BTOK_EOF) {
                if (ctx->depth != 0) { rr = XP_EFORMAT; goto fail; }
                ctx->usage.decoded_bytes = ctx->rd.decoded_offset;
                return XP_OK;
            }
            if (t.kind == XP_BTOK_COMMA || t.kind == XP_BTOK_SEMI) continue;
            b_pushback(ctx, &t);
            rr = b_parse_object(ctx, cb, user);
            if (rr != XP_OK) goto fail;
        }
    }
#endif

fail:
    ctx->usage.decoded_bytes = ctx->rd.decoded_offset;
    xp_report_failure(ctx, cb, user, rr, xp_result_str(rr));
    return rr;
}

/* ========================================================================== */
/*                                  TEXT                                     */
/* ========================================================================== */

static xp_result t_expect_lbrace(xp_context* ctx) {
    xp_token t;
    xp_result rr = t_expect(ctx, XP_TOK_LBRACE, &t);
    if (rr != XP_OK) return rr;
    ctx->depth++;
    if (ctx->depth > (int)XP_MAX_DEPTH) return XP_ELIMIT;
    return XP_OK;
}

/* Parse child objects/references until '}' is seen; leaves '}' consumed. */
static xp_result t_parse_children_until_rbrace(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_token t;
    xp_result rr;
    while (1) {
        rr = xpl_next(&ctx->lx, &t);
        if (rr != XP_OK) return rr;
        if (t.kind == XP_TOK_RBRACE) return XP_OK;
        if (t.kind == XP_TOK_IDENT) {
            t_pushback(ctx, &t);
            rr = t_parse_object(ctx, cb, user);
            if (rr != XP_OK) return rr;
        } else if (t.kind == XP_TOK_LBRACE) {
            /* data_reference */
            t_pushback(ctx, &t);
            rr = t_parse_data_reference(ctx, cb, user, 0);
            if (rr != XP_OK) return rr;
        } else if (t.kind == XP_TOK_SEMI || t.kind == XP_TOK_COMMA) {
            /* ignore */
        } else if (t.kind == XP_TOK_EOF) {
            return XP_EFORMAT;
        } else {
            /* ignore stray */
        }
    }
}

static xp_result t_parse_vertex_duplication_indices(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    unsigned int n_indices, n_original, i, v;
    rr = t_expect_lbrace(ctx); if (rr != XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "VertexDuplicationIndices", ""); if (rr != XP_OK) return rr;
    rr = t_read_u32(ctx, &n_indices); if (rr != XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = t_read_u32(ctx, &n_original); if (rr != XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = xp_budget_add(&ctx->usage.indices, (unsigned long)n_indices, ctx->limits.max_indices);
    if (rr != XP_OK) return rr;
    if (cb && cb->on_vertex_duplication_begin) cb->on_vertex_duplication_begin(user, n_indices, n_original);
    for (i = 0u; i < n_indices; ++i) {
        rr = t_read_u32(ctx, &v); if (rr != XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
        if (cb && cb->on_vertex_duplication_index) cb->on_vertex_duplication_index(user, i, v);
    }
    rr = t_parse_children_until_rbrace(ctx, cb, user); if (rr != XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_vertex_duplication_end) cb->on_vertex_duplication_end(user);
    return XP_OK;
}

static xp_result t_parse_decl_data(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    unsigned int n_elements, n_dwords, i;
    rr = t_expect_lbrace(ctx); if (rr != XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "DeclData", ""); if (rr != XP_OK) return rr;
    rr = t_read_u32(ctx, &n_elements); if (rr != XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = xp_budget_mul_add(&ctx->usage.raw_dwords, (unsigned long)n_elements, 4UL, ctx->limits.max_raw_dwords);
    if (rr != XP_OK) return rr;
    if (cb && cb->on_decl_data_begin) cb->on_decl_data_begin(user, n_elements);
    for (i = 0u; i < n_elements; ++i) {
        unsigned int type, method, usage, usage_index;
        rr = t_read_u32(ctx, &type); if (rr != XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
        rr = t_read_u32(ctx, &method); if (rr != XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
        rr = t_read_u32(ctx, &usage); if (rr != XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
        rr = t_read_u32(ctx, &usage_index); if (rr != XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
        if (cb && cb->on_decl_element) cb->on_decl_element(user, i, type, method, usage, usage_index);
    }
    rr = t_read_u32(ctx, &n_dwords); if (rr != XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = xp_budget_add(&ctx->usage.raw_dwords, (unsigned long)n_dwords, ctx->limits.max_raw_dwords);
    if (rr != XP_OK) return rr;
    if (cb && cb->on_decl_data_dwords) cb->on_decl_data_dwords(user, n_dwords);
    for (i = 0u; i < n_dwords; ++i) {
        unsigned int value;
        rr = t_read_u32(ctx, &value); if (rr != XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
        if (cb && cb->on_decl_data_dword) cb->on_decl_data_dword(user, i, value);
    }
    rr = t_parse_children_until_rbrace(ctx, cb, user); if (rr != XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_decl_data_end) cb->on_decl_data_end(user);
    return XP_OK;
}

static xp_result t_parse_fvf_data(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    unsigned int fvf, n_dwords, i;
    rr = t_expect_lbrace(ctx); if (rr != XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "FVFData", ""); if (rr != XP_OK) return rr;
    rr = t_read_u32(ctx, &fvf); if (rr != XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = t_read_u32(ctx, &n_dwords); if (rr != XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = xp_budget_add(&ctx->usage.raw_dwords, (unsigned long)n_dwords, ctx->limits.max_raw_dwords);
    if (rr != XP_OK) return rr;
    if (cb && cb->on_fvf_data_begin) cb->on_fvf_data_begin(user, fvf, n_dwords);
    for (i = 0u; i < n_dwords; ++i) {
        unsigned int value;
        rr = t_read_u32(ctx, &value); if (rr != XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
        if (cb && cb->on_fvf_data_dword) cb->on_fvf_data_dword(user, i, value);
    }
    rr = t_parse_children_until_rbrace(ctx, cb, user); if (rr != XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_fvf_data_end) cb->on_fvf_data_end(user);
    return XP_OK;
}

static xp_result t_parse_mesh(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name) {
    xp_result rr;
    xp_token t;
    int nV, nF;
    int i;
    char prev_mesh[XP_MAX_TOKEN_TEXT];

    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "Mesh", obj_name); if (rr!=XP_OK) return rr;

    rr = t_read_int(ctx, &nV); if (rr!=XP_OK) return rr;
    if (nV < 0) return XP_EFORMAT;
    rr = xp_budget_add(&ctx->usage.vertices, (unsigned long)nV, ctx->limits.max_vertices);
    if (rr != XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    rr = xp_copyz(prev_mesh, XP_MAX_TOKEN_TEXT, ctx->current_mesh_name); if (rr!=XP_OK) return rr;
    rr = xp_copyz(ctx->current_mesh_name, XP_MAX_TOKEN_TEXT, obj_name?obj_name:""); if (rr!=XP_OK) return rr;
    if (cb && cb->on_mesh_frame_binding)
        cb->on_mesh_frame_binding(user, obj_name?obj_name:"", ctx->current_frame_name);
    if (cb && cb->on_mesh_frame_node_binding)
        cb->on_mesh_frame_node_binding(user, obj_name?obj_name:"", ctx->current_frame_id);
    if (cb && cb->on_mesh_begin) cb->on_mesh_begin(user, obj_name?obj_name:"", nV, -1);

    for (i=0; i<nV; ++i) {
        xp_fx32 x,y,z;
        rr = t_read_real(ctx, &x); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_read_real(ctx, &y); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_read_real(ctx, &z); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;

        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
        if (cb && cb->on_vertex) cb->on_vertex(user, i, x,y,z);
    }

    rr = t_read_int(ctx, &nF); if (rr!=XP_OK) return rr;
    if (nF < 0) return XP_EFORMAT;
    rr = xp_budget_add(&ctx->usage.faces, (unsigned long)nF, ctx->limits.max_faces);
    if (rr != XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_mesh_face_count) cb->on_mesh_face_count(user, nF);

    for (i=0; i<nF; ++i) {
        int deg, j;
        int idx[XP_MAX_FACE_DEGREE];
        rr = t_read_int(ctx, &deg); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        if (deg < 0 || deg > (int)XP_MAX_FACE_DEGREE) return XP_ELIMIT;
        rr = xp_budget_add(&ctx->usage.indices, (unsigned long)deg, ctx->limits.max_indices);
        if (rr != XP_OK) return rr;
        for (j=0; j<deg; ++j) {
            rr = t_read_int(ctx, &idx[j]); if (rr!=XP_OK) return rr;
            if (j+1 < deg) {
                rr = t_expect_index_sep(ctx); if (rr!=XP_OK) return rr;
            } else {
                rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
            }
        }
        /* tolerate extra semicolons/commas */
        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
        if (cb && cb->on_face) cb->on_face(user, i, deg, idx);
    }

    /* children until '}' */
    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (rr != XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_mesh_end) cb->on_mesh_end(user, obj_name?obj_name:"");
    (void)xp_copyz(ctx->current_mesh_name, XP_MAX_TOKEN_TEXT, prev_mesh);
    return XP_OK;
}

static xp_result t_parse_mesh_normals(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_token t;
    int nN, nFN;
    int i;

    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "MeshNormals", ""); if (rr!=XP_OK) return rr;

    rr = t_read_int(ctx, &nN); if (rr!=XP_OK) return rr;
    if (nN < 0) return XP_EFORMAT;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    /* normals */
    if (cb && cb->on_mesh_normals_begin) cb->on_mesh_normals_begin(user, nN, -1);
    for (i=0;i<nN;i++) {
        xp_fx32 x,y,z;
        rr = t_read_real(ctx, &x); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_read_real(ctx, &y); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_read_real(ctx, &z); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
        if (cb && cb->on_normal) cb->on_normal(user, i, x,y,z);
    }

    rr = t_read_int(ctx, &nFN); if (rr!=XP_OK) return rr;
    if (nFN < 0) return XP_EFORMAT;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_mesh_normals_begin) cb->on_mesh_normals_begin(user, nN, nFN);

    for (i=0;i<nFN;i++) {
        int deg, j;
        int idx[XP_MAX_FACE_DEGREE];
        rr = t_read_int(ctx, &deg); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        if (deg < 0 || deg > (int)XP_MAX_FACE_DEGREE) return XP_ELIMIT;
        rr = xp_budget_add(&ctx->usage.indices, (unsigned long)deg, ctx->limits.max_indices);
        if (rr != XP_OK) return rr;
        for (j=0;j<deg;j++) {
            rr = t_read_int(ctx, &idx[j]); if (rr!=XP_OK) return rr;
            if (j+1<deg) {
                rr = t_expect_index_sep(ctx); if (rr!=XP_OK) return rr;
            } else {
                rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
            }
        }
        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
        if (cb && cb->on_face_normals) cb->on_face_normals(user, i, deg, idx);
    }

    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (rr != XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result t_parse_mesh_texcoords(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_token t;
    int nUV, i;

    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "MeshTextureCoords", ""); if (rr!=XP_OK) return rr;

    rr = t_read_int(ctx, &nUV); if (rr!=XP_OK) return rr;
    if (nUV < 0) return XP_EFORMAT;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_mesh_texcoords_begin) cb->on_mesh_texcoords_begin(user, nUV);

    for (i=0;i<nUV;i++) {
        xp_fx32 u0,v0;
        rr = t_read_real(ctx, &u0); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_read_real(ctx, &v0); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
        if (cb && cb->on_texcoord) cb->on_texcoord(user, i, u0, v0);
    }

    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (rr != XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result t_parse_mesh_colors(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_token t;
    int nC, i;

    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "MeshVertexColors", ""); if (rr!=XP_OK) return rr;

    rr = t_read_int(ctx, &nC); if (rr!=XP_OK) return rr;
    if (nC < 0) return XP_EFORMAT;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_mesh_colors_begin) cb->on_mesh_colors_begin(user, nC);

    for (i=0;i<nC;i++) {
        int idx;
        xp_fx32 r,g,b,a;
        rr = t_read_int(ctx, &idx); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_read_real(ctx, &r); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_read_real(ctx, &g); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_read_real(ctx, &b); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_read_real(ctx, &a); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

        if (cb && cb->on_vertex_color) cb->on_vertex_color(user, idx, r,g,b,a);
    }

    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (rr != XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result t_parse_mesh_material_list(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_token t;
    int nMat, nFaceIdx;
    int i;

    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "MeshMaterialList", ""); if (rr!=XP_OK) return rr;

    rr = t_read_int(ctx, &nMat); if (rr!=XP_OK) return rr;
    if (nMat < 0) return XP_EFORMAT;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    rr = t_read_int(ctx, &nFaceIdx); if (rr!=XP_OK) return rr;
    if (nFaceIdx < 0) return XP_EFORMAT;
    rr = xp_budget_add(&ctx->usage.indices, (unsigned long)nFaceIdx, ctx->limits.max_indices);
    if (rr != XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_mesh_material_list_begin) cb->on_mesh_material_list_begin(user, nMat, nFaceIdx);

    for (i=0;i<nFaceIdx;i++) {
        int mi;
        rr = t_read_int(ctx, &mi); if (rr!=XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
        if (cb && cb->on_face_material) cb->on_face_material(user, i, mi);
    }

    /* After faceIndexes, nested Material objects typically follow */
    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (rr != XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result t_parse_texture_filename(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    char fn[XP_MAX_STRING];

    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "TextureFilename", ""); if (rr!=XP_OK) return rr;

    rr = t_read_texture_string(ctx, fn, XP_MAX_STRING); if (rr!=XP_OK) return rr;
    /* optional ';' */
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_material_texture_filename) cb->on_material_texture_filename(user, fn);

    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (rr != XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result t_parse_material_wrap(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    unsigned int wrap_u, wrap_v;
    rr = t_expect_lbrace(ctx); if (rr != XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "MaterialWrap", ""); if (rr != XP_OK) return rr;
    rr = t_read_u32(ctx, &wrap_u); if (rr != XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = t_read_u32(ctx, &wrap_v); if (rr != XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
    if (cb && cb->on_material_wrap) cb->on_material_wrap(user, wrap_u ? 1u : 0u, wrap_v ? 1u : 0u);
    rr = t_parse_children_until_rbrace(ctx, cb, user); if (rr != XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result t_parse_mesh_face_wraps(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    unsigned int n_faces, i, wrap_u, wrap_v;
    rr = t_expect_lbrace(ctx); if (rr != XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "MeshFaceWraps", ""); if (rr != XP_OK) return rr;
    rr = t_read_u32(ctx, &n_faces); if (rr != XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = xp_budget_add(&ctx->usage.indices, (unsigned long)n_faces, ctx->limits.max_indices);
    if (rr != XP_OK) return rr;
    if (cb && cb->on_mesh_face_wraps_begin) cb->on_mesh_face_wraps_begin(user, n_faces);
    for (i = 0u; i < n_faces; ++i) {
        rr = t_read_u32(ctx, &wrap_u); if (rr != XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
        rr = t_read_u32(ctx, &wrap_v); if (rr != XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr != XP_OK) return rr;
        if (cb && cb->on_mesh_face_wrap)
            cb->on_mesh_face_wrap(user, i, wrap_u ? 1u : 0u, wrap_v ? 1u : 0u);
    }
    rr = t_parse_children_until_rbrace(ctx, cb, user); if (rr != XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result t_parse_material(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name) {
    xp_result rr;
    xp_token t;
    xp_fx32 r,g,b,a;
    xp_fx32 power;
    xp_fx32 sr,sg,sb;
    xp_fx32 er,eg,eb;

    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "Material", obj_name); if (rr!=XP_OK) return rr;

    if (cb && cb->on_material_begin) cb->on_material_begin(user, obj_name?obj_name:"");

    rr = t_read_real(ctx, &r); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_read_real(ctx, &g); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_read_real(ctx, &b); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_read_real(ctx, &a); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_material_color) cb->on_material_color(user, r,g,b,a);

    rr = t_read_real(ctx, &power); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
    if (cb && cb->on_material_power) cb->on_material_power(user, power);

    rr = t_read_real(ctx, &sr); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_read_real(ctx, &sg); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_read_real(ctx, &sb); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
    if (cb && cb->on_material_specular) cb->on_material_specular(user, sr,sg,sb);

    rr = t_read_real(ctx, &er); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_read_real(ctx, &eg); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_read_real(ctx, &eb); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
    if (cb && cb->on_material_emissive) cb->on_material_emissive(user, er,eg,eb);

    /* children (TextureFilename, etc) */
    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (rr != XP_OK) return rr;
    ctx->depth--;

    if (cb && cb->on_material_end) cb->on_material_end(user, obj_name?obj_name:"");
    return XP_OK;
}

static xp_result t_parse_frame_transform(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_token t;
    xp_fx32 m[16];
    int i;

    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "FrameTransformMatrix", ""); if (rr!=XP_OK) return rr;

    for (i=0;i<16;i++) {
        rr = t_read_real(ctx, &m[i]); if (rr!=XP_OK) return rr;
        /* separator can be comma or semicolon; tolerate */
        rr = xpl_next(&ctx->lx, &t); if (rr!=XP_OK) return rr;
        if (!(t.kind==XP_TOK_COMMA || t.kind==XP_TOK_SEMI)) return XP_EFORMAT;
    }
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
    if (cb && cb->on_frame_transform_matrix) cb->on_frame_transform_matrix(user, m);
    if (cb && cb->on_frame_node_transform && ctx->current_frame_id != XP_FRAME_ID_NONE)
        cb->on_frame_node_transform(user, ctx->current_frame_id, m);

    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (rr != XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result t_parse_frame(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name) {
    xp_result rr;
    char parent_name[XP_MAX_TOKEN_TEXT];
    unsigned int parent_id;
    unsigned int frame_id;
    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "Frame", obj_name); if (rr!=XP_OK) return rr;
    rr = xp_copyz(parent_name, XP_MAX_TOKEN_TEXT, ctx->current_frame_name); if (rr!=XP_OK) return rr;
    parent_id = ctx->current_frame_id;
    frame_id = ctx->next_frame_id++;
    if (cb && cb->on_frame_relation)
        cb->on_frame_relation(user, obj_name?obj_name:"", parent_name, ctx->frame_semantic_depth);
    if (cb && cb->on_frame_node_begin)
        cb->on_frame_node_begin(user, frame_id, parent_id, obj_name?obj_name:"", ctx->frame_semantic_depth);
    rr = xp_copyz(ctx->current_frame_name, XP_MAX_TOKEN_TEXT, obj_name?obj_name:""); if (rr!=XP_OK) return rr;
    ctx->current_frame_id = frame_id;
    ctx->frame_semantic_depth++;

    if (cb && cb->on_frame_begin) cb->on_frame_begin(user, obj_name?obj_name:"");
    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (ctx->frame_semantic_depth != 0u) ctx->frame_semantic_depth--;
    (void)xp_copyz(ctx->current_frame_name, XP_MAX_TOKEN_TEXT, parent_name);
    ctx->current_frame_id = parent_id;
    if (rr != XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_frame_end) cb->on_frame_end(user, obj_name?obj_name:"");
    if (cb && cb->on_frame_node_end) cb->on_frame_node_end(user, frame_id);
    return XP_OK;
}

static xp_result t_parse_skin_weights(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_token t;
    char bone[XP_MAX_TOKEN_TEXT];
    unsigned int nW, i;
    xp_fx32 w;
    xp_fx32 m[16];

    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "SkinWeights", ""); if (rr!=XP_OK) return rr;

    rr = t_read_string(ctx, bone, XP_MAX_TOKEN_TEXT); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    rr = t_read_u32(ctx, &nW); if (rr!=XP_OK) return rr;
    if (!xp_u32_fits_int(nW)) return XP_ELIMIT;
    rr = xp_budget_add(&ctx->usage.skin_weights, (unsigned long)nW, ctx->limits.max_skin_weights);
    if (rr != XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    rr = xp_copyz(ctx->current_skin_bone_name, XP_MAX_TOKEN_TEXT, bone); if (rr != XP_OK) return rr;
    if (cb && cb->on_skin_binding_begin)
        cb->on_skin_binding_begin(user, ctx->current_mesh_name, bone, (int)nW);
    if (cb && cb->on_skin_weights_begin) cb->on_skin_weights_begin(user, bone, (int)nW);

    for (i=0;i<nW;i++) {
        int vi;
        rr = t_read_int(ctx, &vi); if (rr!=XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
        if (cb && cb->on_skin_weight_index) cb->on_skin_weight_index(user, (int)i, vi);
    }

    for (i=0;i<nW;i++) {
        rr = t_read_real(ctx, &w); if (rr!=XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
        if (cb && cb->on_skin_weight_value) cb->on_skin_weight_value(user, (int)i, w);
    }

    for (i=0;i<16;i++) {
        rr = t_read_real(ctx, &m[i]); if (rr!=XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
    }
    if (cb && cb->on_skin_weights_offset_matrix) cb->on_skin_weights_offset_matrix(user, m);
    if (cb && cb->on_skin_inverse_bind_matrix)
        cb->on_skin_inverse_bind_matrix(user, ctx->current_mesh_name, bone, m);

    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (rr != XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_skin_weights_end) cb->on_skin_weights_end(user, bone);
    ctx->current_skin_bone_name[0] = '\0';
    return XP_OK;
}

static xp_result t_parse_skin_mesh_header(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_token t;
    unsigned int a, b, bones;
    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "XSkinMeshHeader", ""); if (rr!=XP_OK) return rr;
    rr = t_read_u32(ctx, &a); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = t_read_u32(ctx, &b); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = t_read_u32(ctx, &bones); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
    if (cb && cb->on_skin_mesh_header) cb->on_skin_mesh_header(user, a, b, bones);
    rr = t_parse_children_until_rbrace(ctx, cb, user); if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result t_parse_anim_ticks_per_second(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_token t;
    unsigned int ticks;
    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "AnimTicksPerSecond", ""); if (rr!=XP_OK) return rr;
    rr = t_read_u32(ctx, &ticks); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
    ctx->animation_ticks_per_second = ticks;
    ctx->animation_ticks_per_second_known = 1;
    if (cb && cb->on_anim_ticks_per_second) cb->on_anim_ticks_per_second(user, ticks);
    rr = t_parse_children_until_rbrace(ctx, cb, user); if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result t_parse_animation_options(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_token t;
    unsigned int open_closed, quality;
    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "AnimationOptions", ""); if (rr!=XP_OK) return rr;
    rr = t_read_u32(ctx, &open_closed); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = t_read_u32(ctx, &quality); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
    if (cb && cb->on_animation_options) cb->on_animation_options(user, open_closed, quality);
    rr = t_parse_children_until_rbrace(ctx, cb, user); if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result t_parse_animation_key(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_token t;
    int keyType;
    int nKeys;
    int i;
    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "AnimationKey", ""); if (rr!=XP_OK) return rr;

    rr = t_read_int(ctx, &keyType); if (rr!=XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    rr = t_read_int(ctx, &nKeys); if (rr!=XP_OK) return rr;
    if (nKeys < 0) return XP_EFORMAT;
    rr = xp_budget_add(&ctx->usage.animation_keys, (unsigned long)nKeys, ctx->limits.max_animation_keys);
    if (rr != XP_OK) return rr;
    rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
    rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_animation_key_begin) cb->on_animation_key_begin(user, keyType, nKeys);

    for (i=0;i<nKeys;i++) {
        int time;
        int nValues;
        int j;
        unsigned int flags;
        xp_anim_key_semantic semantic;
        xp_fx32 vals[XP_MAX_KEY_VALUES];

        rr = t_read_int(ctx, &time); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

        rr = t_read_int(ctx, &nValues); if (rr!=XP_OK) return rr;
        rr = t_expect(ctx, XP_TOK_SEMI, &t); if (rr!=XP_OK) return rr;
        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;

        if (nValues < 0 || (unsigned int)nValues > XP_MAX_KEY_VALUES) return XP_ELIMIT;
        for (j=0;j<nValues;j++) {
            rr = t_read_real(ctx, &vals[j]); if (rr!=XP_OK) return rr;
            rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
        }

        rr = t_consume_delims(ctx); if (rr!=XP_OK) return rr;
        if (!ctx->current_animation_has_keys) {
            ctx->current_animation_first_tick = time;
            ctx->current_animation_last_tick = time;
            ctx->current_animation_has_keys = 1;
        } else {
            if (time < ctx->current_animation_first_tick) ctx->current_animation_first_tick = time;
            if (time > ctx->current_animation_last_tick) ctx->current_animation_last_tick = time;
        }
        if (cb && cb->on_animation_key) cb->on_animation_key(user, keyType, i, time, nValues, vals);
        flags = 0u;
        semantic = xp_animation_key_semantic_kind(keyType, nValues, &flags);
        if (cb && cb->on_animation_key_semantic)
            cb->on_animation_key_semantic(user, keyType, semantic, flags, i, time, nValues, vals);
    }

    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (rr != XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_animation_key_end) cb->on_animation_key_end(user, keyType);
    return XP_OK;
}

static xp_result t_parse_animation(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name) {
    xp_result rr;
    xp_token t;
    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "Animation", obj_name); if (rr!=XP_OK) return rr;
    if (ctx->current_animation_set_name[0] != '\0') ctx->current_animation_track_count++;
    if (cb && cb->on_animation_begin) cb->on_animation_begin(user, obj_name?obj_name:"");
    while (1) {
        rr = xpl_next(&ctx->lx, &t); if (rr!=XP_OK) return rr;
        if (t.kind == XP_TOK_RBRACE) break;
        if (t.kind == XP_TOK_IDENT) {
            t_pushback(ctx, &t);
            rr = t_parse_object(ctx, cb, user); if (rr!=XP_OK) return rr;
        } else if (t.kind == XP_TOK_LBRACE) {
            t_pushback(ctx, &t);
            rr = t_parse_data_reference(ctx, cb, user, 1); if (rr!=XP_OK) return rr;
        } else if (t.kind == XP_TOK_SEMI || t.kind == XP_TOK_COMMA) {
            /* ignore delimiters */
        } else if (t.kind == XP_TOK_EOF) {
            return XP_EFORMAT;
        }
    }
    ctx->depth--;
    if (cb && cb->on_animation_end) cb->on_animation_end(user, obj_name?obj_name:"");
    return XP_OK;
}

static xp_result t_parse_animation_set(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name) {
    xp_result rr;
    rr = t_expect_lbrace(ctx); if (rr!=XP_OK) return rr;
    rr = t_maybe_class_id(ctx, cb, user, "AnimationSet", obj_name); if (rr!=XP_OK) return rr;
    rr = xp_copyz(ctx->current_animation_set_name, XP_MAX_TOKEN_TEXT, obj_name?obj_name:""); if (rr != XP_OK) return rr;
    ctx->current_animation_track_count = 0u;
    ctx->current_animation_has_keys = 0;
    ctx->current_animation_first_tick = 0;
    ctx->current_animation_last_tick = 0;
    if (cb && cb->on_animation_set_begin) cb->on_animation_set_begin(user, obj_name?obj_name:"");
    rr = t_parse_children_until_rbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_animation_set_summary)
        cb->on_animation_set_summary(user, obj_name?obj_name:"", ctx->current_animation_track_count,
            ctx->current_animation_has_keys ? ctx->current_animation_first_tick : 0,
            ctx->current_animation_has_keys ? ctx->current_animation_last_tick : 0,
            ctx->animation_ticks_per_second, ctx->animation_ticks_per_second_known);
    if (cb && cb->on_animation_set_end) cb->on_animation_set_end(user, obj_name?obj_name:"");
    ctx->current_animation_set_name[0] = '\0';
    ctx->current_animation_track_count = 0u;
    ctx->current_animation_has_keys = 0;
    return XP_OK;
}

/* ========================================================================== */
/*                                  BINARY                                   */
/* ========================================================================== */

#if XP_ENABLE_BINARY

static xp_result b_expect_obrace(xp_context* ctx) {
    xp_btoken t;
    xp_result rr = b_consume_delims(ctx);
    if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t);
    if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_OBRACE) return XP_EFORMAT;
    ctx->depth++;
    if (ctx->depth > (int)XP_MAX_DEPTH) return XP_ELIMIT;
    return XP_OK;
}

/* Read exactly N numeric values from one or more packed list records. */
static xp_result b_read_reals_exact(xp_context* ctx, unsigned int n, xp_fx32* out, unsigned int out_cap) {
    xp_result rr;
    xp_btoken t;
    unsigned int i;
    if (n > out_cap) return XP_ELIMIT;
    for (i = 0u; i < n; ++i) {
        rr = b_consume_delims(ctx); if (rr != XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr != XP_OK) return rr;
        if (t.kind != XP_BTOK_REAL) return XP_EFORMAT;
        if (out) out[i] = t.rtmp[0];
    }
    return XP_OK;
}



/* Parse child objects/references until '}' is seen; consumes '}'. */
static xp_result b_parse_children_until_cbrace(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_btoken t;
    xp_result rr;
    while (1) {
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
        if (t.kind == XP_BTOK_CBRACE) return XP_OK;
        if (t.kind == XP_BTOK_EOF) return XP_EFORMAT;
        if (t.kind == XP_BTOK_NAME || t.kind == XP_BTOK_OTHER || t.kind == XP_BTOK_TEMPLATE) {
            b_pushback(ctx, &t);
            rr = b_parse_object(ctx, cb, user);
            if (rr != XP_OK) return rr;
        } else if (t.kind == XP_BTOK_OBRACE) {
            b_pushback(ctx, &t);
            rr = b_parse_data_reference(ctx, cb, user, 0);
            if (rr != XP_OK) return rr;
        } else if (t.kind == XP_BTOK_INTEGER_LIST) {
            rr = xpb_discard_u32_list(&ctx->blx, t.count);
            if (rr != XP_OK) return rr;
        } else if (t.kind == XP_BTOK_REAL_LIST) {
            rr = xpb_discard_real_list(&ctx->blx, t.count);
            if (rr != XP_OK) return rr;
        } else {
            /* ignore */
        }
    }
}

static xp_result b_read_u32_scalar(xp_context* ctx, unsigned int* out) {
    xp_result rr;
    xp_btoken t;
    rr = b_consume_delims(ctx); if (rr != XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr != XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    *out = t.u32;
    return XP_OK;
}

static xp_result b_parse_vertex_duplication_indices(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    unsigned int n_indices, n_original, i, v;
    rr = b_expect_obrace(ctx); if (rr != XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "VertexDuplicationIndices", ""); if (rr != XP_OK) return rr;
    rr = b_read_u32_scalar(ctx, &n_indices); if (rr != XP_OK) return rr;
    rr = b_read_u32_scalar(ctx, &n_original); if (rr != XP_OK) return rr;
    rr = xp_budget_add(&ctx->usage.indices, (unsigned long)n_indices, ctx->limits.max_indices);
    if (rr != XP_OK) return rr;
    if (cb && cb->on_vertex_duplication_begin) cb->on_vertex_duplication_begin(user, n_indices, n_original);
    for (i = 0u; i < n_indices; ++i) {
        rr = b_read_u32_scalar(ctx, &v); if (rr != XP_OK) return rr;
        if (cb && cb->on_vertex_duplication_index) cb->on_vertex_duplication_index(user, i, v);
    }
    rr = b_parse_children_until_cbrace(ctx, cb, user); if (rr != XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_vertex_duplication_end) cb->on_vertex_duplication_end(user);
    return XP_OK;
}

static xp_result b_parse_decl_data(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    unsigned int n_elements, n_dwords, i;
    rr = b_expect_obrace(ctx); if (rr != XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "DeclData", ""); if (rr != XP_OK) return rr;
    rr = b_read_u32_scalar(ctx, &n_elements); if (rr != XP_OK) return rr;
    rr = xp_budget_mul_add(&ctx->usage.raw_dwords, (unsigned long)n_elements, 4UL, ctx->limits.max_raw_dwords);
    if (rr != XP_OK) return rr;
    if (cb && cb->on_decl_data_begin) cb->on_decl_data_begin(user, n_elements);
    for (i = 0u; i < n_elements; ++i) {
        unsigned int type, method, usage, usage_index;
        rr = b_read_u32_scalar(ctx, &type); if (rr != XP_OK) return rr;
        rr = b_read_u32_scalar(ctx, &method); if (rr != XP_OK) return rr;
        rr = b_read_u32_scalar(ctx, &usage); if (rr != XP_OK) return rr;
        rr = b_read_u32_scalar(ctx, &usage_index); if (rr != XP_OK) return rr;
        if (cb && cb->on_decl_element) cb->on_decl_element(user, i, type, method, usage, usage_index);
    }
    rr = b_read_u32_scalar(ctx, &n_dwords); if (rr != XP_OK) return rr;
    rr = xp_budget_add(&ctx->usage.raw_dwords, (unsigned long)n_dwords, ctx->limits.max_raw_dwords);
    if (rr != XP_OK) return rr;
    if (cb && cb->on_decl_data_dwords) cb->on_decl_data_dwords(user, n_dwords);
    for (i = 0u; i < n_dwords; ++i) {
        unsigned int value;
        rr = b_read_u32_scalar(ctx, &value); if (rr != XP_OK) return rr;
        if (cb && cb->on_decl_data_dword) cb->on_decl_data_dword(user, i, value);
    }
    rr = b_parse_children_until_cbrace(ctx, cb, user); if (rr != XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_decl_data_end) cb->on_decl_data_end(user);
    return XP_OK;
}

static xp_result b_parse_fvf_data(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    unsigned int fvf, n_dwords, i;
    rr = b_expect_obrace(ctx); if (rr != XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "FVFData", ""); if (rr != XP_OK) return rr;
    rr = b_read_u32_scalar(ctx, &fvf); if (rr != XP_OK) return rr;
    rr = b_read_u32_scalar(ctx, &n_dwords); if (rr != XP_OK) return rr;
    rr = xp_budget_add(&ctx->usage.raw_dwords, (unsigned long)n_dwords, ctx->limits.max_raw_dwords);
    if (rr != XP_OK) return rr;
    if (cb && cb->on_fvf_data_begin) cb->on_fvf_data_begin(user, fvf, n_dwords);
    for (i = 0u; i < n_dwords; ++i) {
        unsigned int value;
        rr = b_read_u32_scalar(ctx, &value); if (rr != XP_OK) return rr;
        if (cb && cb->on_fvf_data_dword) cb->on_fvf_data_dword(user, i, value);
    }
    rr = b_parse_children_until_cbrace(ctx, cb, user); if (rr != XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_fvf_data_end) cb->on_fvf_data_end(user);
    return XP_OK;
}

static xp_result b_parse_mesh(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name) {
    xp_result rr;
    xp_btoken t;
    unsigned int nV, nF, i;
    char prev_mesh[XP_MAX_TOKEN_TEXT];

    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "Mesh", obj_name); if (rr!=XP_OK) return rr;

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    nV = t.u32;
    if (!xp_u32_fits_int(nV) || !xp_u32_mul_ok(nV, 3u)) return XP_ELIMIT;
    rr = xp_budget_add(&ctx->usage.vertices, (unsigned long)nV, ctx->limits.max_vertices);
    if (rr != XP_OK) return rr;
    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;

    rr = xp_copyz(prev_mesh, XP_MAX_TOKEN_TEXT, ctx->current_mesh_name); if (rr!=XP_OK) return rr;
    rr = xp_copyz(ctx->current_mesh_name, XP_MAX_TOKEN_TEXT, obj_name?obj_name:""); if (rr!=XP_OK) return rr;
    if (cb && cb->on_mesh_frame_binding)
        cb->on_mesh_frame_binding(user, obj_name?obj_name:"", ctx->current_frame_name);
    if (cb && cb->on_mesh_frame_node_binding)
        cb->on_mesh_frame_node_binding(user, obj_name?obj_name:"", ctx->current_frame_id);
    if (cb && cb->on_mesh_begin) cb->on_mesh_begin(user, obj_name?obj_name:"", (int)nV, -1);

    /* List packing is flattened by xpb_next(), so consume grammar scalars. */
    for (i = 0u; i < nV; ++i) {
        xp_fx32 vbuf[3];
        rr = b_read_reals_exact(ctx, 3u, vbuf, 3u);
        if (rr != XP_OK) return rr;
        if (cb && cb->on_vertex) cb->on_vertex(user, (int)i, vbuf[0], vbuf[1], vbuf[2]);
    }

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    nF = t.u32;
    if (!xp_u32_fits_int(nF)) return XP_ELIMIT;
    rr = xp_budget_add(&ctx->usage.faces, (unsigned long)nF, ctx->limits.max_faces);
    if (rr != XP_OK) return rr;
    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_mesh_face_count) cb->on_mesh_face_count(user, (int)nF);

    for (i=0;i<nF;i++) {
        unsigned int deg, j;
        int idxi[XP_MAX_FACE_DEGREE];
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
        if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
        deg = t.u32;
        if (deg > XP_MAX_FACE_DEGREE) return XP_ELIMIT;
        rr = xp_budget_add(&ctx->usage.indices, (unsigned long)deg, ctx->limits.max_indices);
        if (rr != XP_OK) return rr;

        /* indices: accept INTEGER_LIST chunks or individual INTEGER records */
        j = 0;
        while (j < deg) {
            rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
            rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
            if (t.kind == XP_BTOK_INTEGER) {
                if (!xp_u32_fits_int(t.u32)) return XP_ELIMIT;
                idxi[j++] = (int)t.u32;
                continue;
            }
            if (t.kind == XP_BTOK_INTEGER_LIST) {
                unsigned int k;
                if (t.count > (deg - j)) return XP_EFORMAT;
                for (k=0;k<t.count;k++) {
                    unsigned int u;
                    rr = xpb_read_u32_list(&ctx->blx, 1u, &u, 1u);
                    if (rr!=XP_OK) return rr;
                    if (!xp_u32_fits_int(u)) return XP_ELIMIT;
                    idxi[j++] = (int)u;
                }
                continue;
            }
            return XP_EFORMAT;
        }
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        if (cb && cb->on_face) cb->on_face(user, (int)i, (int)deg, idxi);
    }

    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_mesh_end) cb->on_mesh_end(user, obj_name?obj_name:"");
    (void)xp_copyz(ctx->current_mesh_name, XP_MAX_TOKEN_TEXT, prev_mesh);
    return XP_OK;
}

static xp_result b_parse_mesh_normals(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_btoken t;
    unsigned int nN, nFN, i;

    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "MeshNormals", ""); if (rr!=XP_OK) return rr;

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    nN = t.u32;
    if (!xp_u32_fits_int(nN) || !xp_u32_mul_ok(nN, 3u)) return XP_ELIMIT;
    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_mesh_normals_begin) cb->on_mesh_normals_begin(user, (int)nN, -1);

    for (i = 0u; i < nN; ++i) {
        xp_fx32 vbuf[3];
        rr = b_read_reals_exact(ctx, 3u, vbuf, 3u);
        if (rr != XP_OK) return rr;
        if (cb && cb->on_normal) cb->on_normal(user, (int)i, vbuf[0], vbuf[1], vbuf[2]);
    }

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    nFN = t.u32;
    if (!xp_u32_fits_int(nFN)) return XP_ELIMIT;
    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_mesh_normals_begin) cb->on_mesh_normals_begin(user, (int)nN, (int)nFN);

    for (i=0;i<nFN;i++) {
        unsigned int deg, j;
        int idxi[XP_MAX_FACE_DEGREE];
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
        if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
        deg = t.u32;
        if (deg > XP_MAX_FACE_DEGREE) return XP_ELIMIT;
        rr = xp_budget_add(&ctx->usage.indices, (unsigned long)deg, ctx->limits.max_indices);
        if (rr != XP_OK) return rr;

        /* indices: accept INTEGER_LIST chunks or individual INTEGER records */
        j = 0;
        while (j < deg) {
            rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
            rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
            if (t.kind == XP_BTOK_INTEGER) {
                if (!xp_u32_fits_int(t.u32)) return XP_ELIMIT;
                idxi[j++] = (int)t.u32;
                continue;
            }
            if (t.kind == XP_BTOK_INTEGER_LIST) {
                unsigned int k;
                if (t.count > (deg - j)) return XP_EFORMAT;
                for (k=0;k<t.count;k++) {
                    unsigned int u;
                    rr = xpb_read_u32_list(&ctx->blx, 1u, &u, 1u);
                    if (rr!=XP_OK) return rr;
                    if (!xp_u32_fits_int(u)) return XP_ELIMIT;
                    idxi[j++] = (int)u;
                }
                continue;
            }
            return XP_EFORMAT;
        }
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        if (cb && cb->on_face_normals) cb->on_face_normals(user, (int)i, (int)deg, idxi);
    }

    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result b_parse_mesh_texcoords(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_btoken t;
    unsigned int nUV;

    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "MeshTextureCoords", ""); if (rr!=XP_OK) return rr;

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    nUV = t.u32;
    if (!xp_u32_fits_int(nUV) || !xp_u32_mul_ok(nUV, 2u)) return XP_ELIMIT;
    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_mesh_texcoords_begin) cb->on_mesh_texcoords_begin(user, (int)nUV);

    {
        unsigned int uvi;
        for (uvi = 0u; uvi < nUV; ++uvi) {
            xp_fx32 vbuf[2];
            rr = b_read_reals_exact(ctx, 2u, vbuf, 2u);
            if (rr != XP_OK) return rr;
            if (cb && cb->on_texcoord) cb->on_texcoord(user, (int)uvi, vbuf[0], vbuf[1]);
        }
    }

    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result b_parse_mesh_colors(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_btoken t;
    unsigned int nC, i;
    xp_fx32 rgba[4];

    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "MeshVertexColors", ""); if (rr!=XP_OK) return rr;

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    nC = t.u32;
    if (!xp_u32_fits_int(nC)) return XP_ELIMIT;
    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;

    if (cb && cb->on_mesh_colors_begin) cb->on_mesh_colors_begin(user, (int)nC);

    for (i=0;i<nC;i++) {
        unsigned int idx;
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
        if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
        idx = t.u32;
        if (!xp_u32_fits_int(idx)) return XP_ELIMIT;
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;

        rr = b_read_reals_exact(ctx, 4u, rgba, 4u);
        if (rr!=XP_OK) return rr;
        if (cb && cb->on_vertex_color) cb->on_vertex_color(user, (int)idx, rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result b_parse_mesh_material_list(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_btoken t;
    unsigned int nMat, nFaceIdx, i;

    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "MeshMaterialList", ""); if (rr!=XP_OK) return rr;

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    nMat = t.u32;
    if (!xp_u32_fits_int(nMat)) return XP_ELIMIT;

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    nFaceIdx = t.u32;
    if (!xp_u32_fits_int(nFaceIdx)) return XP_ELIMIT;
    rr = xp_budget_add(&ctx->usage.indices, (unsigned long)nFaceIdx, ctx->limits.max_indices);
    if (rr != XP_OK) return rr;

    if (cb && cb->on_mesh_material_list_begin) cb->on_mesh_material_list_begin(user, (int)nMat, (int)nFaceIdx);

    /* faceIndexes: accept INTEGER_LIST chunks or individual INTEGER records */
    i = 0;
    while (i < nFaceIdx) {
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;

        if (t.kind == XP_BTOK_INTEGER) {
            if (!xp_u32_fits_int(t.u32)) return XP_ELIMIT;
            if (cb && cb->on_face_material) cb->on_face_material(user, (int)i, (int)t.u32);
            i++;
            continue;
        }

        if (t.kind == XP_BTOK_INTEGER_LIST) {
            unsigned int j;
            if (t.count > (nFaceIdx - i)) return XP_EFORMAT;
            for (j=0;j<t.count;j++) {
                unsigned int mi;
                rr = xpb_read_u32_list(&ctx->blx, 1u, &mi, 1u);
                if (rr!=XP_OK) return rr;
                if (!xp_u32_fits_int(mi)) return XP_ELIMIT;
                if (cb && cb->on_face_material) cb->on_face_material(user, (int)i, (int)mi);
                i++;
            }
            continue;
        }

        return XP_EFORMAT;
    }

    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result b_parse_texture_filename(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_btoken t;
    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "TextureFilename", ""); if (rr!=XP_OK) return rr;

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_STRING) return XP_EFORMAT;
    rr = xp_validate_texture_string(t.text); if (rr != XP_OK) return rr;
    if (cb && cb->on_material_texture_filename) cb->on_material_texture_filename(user, t.text);

    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result b_parse_material_wrap(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    unsigned int wrap_u, wrap_v;
    rr = b_expect_obrace(ctx); if (rr != XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "MaterialWrap", ""); if (rr != XP_OK) return rr;
    rr = b_read_u32_scalar(ctx, &wrap_u); if (rr != XP_OK) return rr;
    rr = b_read_u32_scalar(ctx, &wrap_v); if (rr != XP_OK) return rr;
    if (cb && cb->on_material_wrap) cb->on_material_wrap(user, wrap_u ? 1u : 0u, wrap_v ? 1u : 0u);
    rr = b_parse_children_until_cbrace(ctx, cb, user); if (rr != XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result b_parse_mesh_face_wraps(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    unsigned int n_faces, i, wrap_u, wrap_v;
    rr = b_expect_obrace(ctx); if (rr != XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "MeshFaceWraps", ""); if (rr != XP_OK) return rr;
    rr = b_read_u32_scalar(ctx, &n_faces); if (rr != XP_OK) return rr;
    rr = xp_budget_add(&ctx->usage.indices, (unsigned long)n_faces, ctx->limits.max_indices);
    if (rr != XP_OK) return rr;
    if (cb && cb->on_mesh_face_wraps_begin) cb->on_mesh_face_wraps_begin(user, n_faces);
    for (i = 0u; i < n_faces; ++i) {
        rr = b_read_u32_scalar(ctx, &wrap_u); if (rr != XP_OK) return rr;
        rr = b_read_u32_scalar(ctx, &wrap_v); if (rr != XP_OK) return rr;
        if (cb && cb->on_mesh_face_wrap)
            cb->on_mesh_face_wrap(user, i, wrap_u ? 1u : 0u, wrap_v ? 1u : 0u);
    }
    rr = b_parse_children_until_cbrace(ctx, cb, user); if (rr != XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result b_parse_material(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name) {
    xp_result rr;
    xp_fx32 fv[11];

    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "Material", obj_name); if (rr!=XP_OK) return rr;

    if (cb && cb->on_material_begin) cb->on_material_begin(user, obj_name?obj_name:"");

    rr = b_read_reals_exact(ctx, 11u, fv, 11u);
    if (rr!=XP_OK) return rr;

    if (cb && cb->on_material_color) cb->on_material_color(user, fv[0], fv[1], fv[2], fv[3]);
    if (cb && cb->on_material_power) cb->on_material_power(user, fv[4]);
    if (cb && cb->on_material_specular) cb->on_material_specular(user, fv[5], fv[6], fv[7]);
    if (cb && cb->on_material_emissive) cb->on_material_emissive(user, fv[8], fv[9], fv[10]);

    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_material_end) cb->on_material_end(user, obj_name?obj_name:"");
    return XP_OK;
}

static xp_result b_parse_frame_transform(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_fx32 m[16];
    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "FrameTransformMatrix", ""); if (rr!=XP_OK) return rr;

    rr = b_read_reals_exact(ctx, 16u, m, 16u);
    if (rr!=XP_OK) return rr;
    if (cb && cb->on_frame_transform_matrix) cb->on_frame_transform_matrix(user, m);
    if (cb && cb->on_frame_node_transform && ctx->current_frame_id != XP_FRAME_ID_NONE)
        cb->on_frame_node_transform(user, ctx->current_frame_id, m);

    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result b_parse_frame(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name) {
    xp_result rr;
    char parent_name[XP_MAX_TOKEN_TEXT];
    unsigned int parent_id;
    unsigned int frame_id;
    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "Frame", obj_name); if (rr!=XP_OK) return rr;
    rr = xp_copyz(parent_name, XP_MAX_TOKEN_TEXT, ctx->current_frame_name); if (rr!=XP_OK) return rr;
    parent_id = ctx->current_frame_id;
    frame_id = ctx->next_frame_id++;
    if (cb && cb->on_frame_relation)
        cb->on_frame_relation(user, obj_name?obj_name:"", parent_name, ctx->frame_semantic_depth);
    if (cb && cb->on_frame_node_begin)
        cb->on_frame_node_begin(user, frame_id, parent_id, obj_name?obj_name:"", ctx->frame_semantic_depth);
    rr = xp_copyz(ctx->current_frame_name, XP_MAX_TOKEN_TEXT, obj_name?obj_name:""); if (rr!=XP_OK) return rr;
    ctx->current_frame_id = frame_id;
    ctx->frame_semantic_depth++;
    if (cb && cb->on_frame_begin) cb->on_frame_begin(user, obj_name?obj_name:"");
    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (ctx->frame_semantic_depth != 0u) ctx->frame_semantic_depth--;
    (void)xp_copyz(ctx->current_frame_name, XP_MAX_TOKEN_TEXT, parent_name);
    ctx->current_frame_id = parent_id;
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_frame_end) cb->on_frame_end(user, obj_name?obj_name:"");
    if (cb && cb->on_frame_node_end) cb->on_frame_node_end(user, frame_id);
    return XP_OK;
}

static xp_result b_parse_skin_weights(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_btoken t;
    char bone[XP_MAX_TOKEN_TEXT];
    unsigned int nW, i;
    xp_fx32 wm[16];

    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "SkinWeights", ""); if (rr!=XP_OK) return rr;

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_STRING) return XP_EFORMAT;
    rr = xp_copyz(bone, XP_MAX_TOKEN_TEXT, t.text); if (rr != XP_OK) return rr;

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    nW = t.u32;
    if (!xp_u32_fits_int(nW) || !xp_u32_add_ok(nW, 16u)) return XP_ELIMIT;
    rr = xp_budget_add(&ctx->usage.skin_weights, (unsigned long)nW, ctx->limits.max_skin_weights);
    if (rr != XP_OK) return rr;

    rr = xp_copyz(ctx->current_skin_bone_name, XP_MAX_TOKEN_TEXT, bone); if (rr != XP_OK) return rr;
    if (cb && cb->on_skin_binding_begin)
        cb->on_skin_binding_begin(user, ctx->current_mesh_name, bone, (int)nW);
    if (cb && cb->on_skin_weights_begin) cb->on_skin_weights_begin(user, bone, (int)nW);

    /* vertexIndices: accept INTEGER_LIST chunks or individual INTEGER records */
    i = 0;
    while (i < nW) {
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;

        if (t.kind == XP_BTOK_INTEGER) {
            if (!xp_u32_fits_int(t.u32)) return XP_ELIMIT;
            if (cb && cb->on_skin_weight_index) cb->on_skin_weight_index(user, (int)i, (int)t.u32);
            i++;
            continue;
        }

        if (t.kind == XP_BTOK_INTEGER_LIST) {
            unsigned int j;
            if (t.count > (nW - i)) return XP_EFORMAT;
            for (j=0;j<t.count;j++) {
                unsigned int vi;
                rr = xpb_read_u32_list(&ctx->blx, 1u, &vi, 1u);
                if (rr!=XP_OK) return rr;
                if (!xp_u32_fits_int(vi)) return XP_ELIMIT;
                if (cb && cb->on_skin_weight_index) cb->on_skin_weight_index(user, (int)i, (int)vi);
                i++;
            }
            continue;
        }

        return XP_EFORMAT;
    }

    /* Numeric record packing can cross the weights/matrix grammar boundary. */
    for (i = 0u; i < nW; ++i) {
        xp_fx32 fv[1];
        rr = b_read_reals_exact(ctx, 1u, fv, 1u);
        if (rr != XP_OK) return rr;
        if (cb && cb->on_skin_weight_value) cb->on_skin_weight_value(user, (int)i, fv[0]);
    }
    rr = b_read_reals_exact(ctx, 16u, wm, 16u);
    if (rr != XP_OK) return rr;
    if (cb && cb->on_skin_weights_offset_matrix) cb->on_skin_weights_offset_matrix(user, wm);
    if (cb && cb->on_skin_inverse_bind_matrix)
        cb->on_skin_inverse_bind_matrix(user, ctx->current_mesh_name, bone, wm);

    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_skin_weights_end) cb->on_skin_weights_end(user, bone);
    ctx->current_skin_bone_name[0] = '\0';
    return XP_OK;
}

static xp_result b_parse_skin_mesh_header(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_btoken t;
    unsigned int v[3];
    unsigned int i;
    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "XSkinMeshHeader", ""); if (rr!=XP_OK) return rr;
    for (i=0;i<3u;i++) {
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
        if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
        v[i] = t.u32;
    }
    if (cb && cb->on_skin_mesh_header) cb->on_skin_mesh_header(user, v[0], v[1], v[2]);
    rr = b_parse_children_until_cbrace(ctx, cb, user); if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result b_parse_anim_ticks_per_second(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_btoken t;
    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "AnimTicksPerSecond", ""); if (rr!=XP_OK) return rr;
    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    ctx->animation_ticks_per_second = t.u32;
    ctx->animation_ticks_per_second_known = 1;
    if (cb && cb->on_anim_ticks_per_second) cb->on_anim_ticks_per_second(user, t.u32);
    rr = b_parse_children_until_cbrace(ctx, cb, user); if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result b_parse_animation_options(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_btoken t;
    unsigned int a, b;
    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "AnimationOptions", ""); if (rr!=XP_OK) return rr;
    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    a = t.u32;
    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    b = t.u32;
    if (cb && cb->on_animation_options) cb->on_animation_options(user, a, b);
    rr = b_parse_children_until_cbrace(ctx, cb, user); if (rr!=XP_OK) return rr;
    ctx->depth--;
    return XP_OK;
}

static xp_result b_parse_animation_key(xp_context* ctx, const xp_callbacks* cb, void* user) {
    xp_result rr;
    xp_btoken t;
    unsigned int keyType, nKeys, i;

    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "AnimationKey", ""); if (rr!=XP_OK) return rr;

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    keyType = t.u32;
    if (!xp_u32_fits_int(keyType)) return XP_ELIMIT;

    rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
    rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
    if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
    nKeys = t.u32;
    if (!xp_u32_fits_int(nKeys)) return XP_ELIMIT;
    rr = xp_budget_add(&ctx->usage.animation_keys, (unsigned long)nKeys, ctx->limits.max_animation_keys);
    if (rr != XP_OK) return rr;

    if (cb && cb->on_animation_key_begin) cb->on_animation_key_begin(user, (int)keyType, (int)nKeys);

    for (i=0;i<nKeys;i++) {
        unsigned int time, nVals;
        unsigned int flags;
        xp_anim_key_semantic semantic;
        xp_fx32 vals[XP_MAX_KEY_VALUES];

        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
        if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
        time = t.u32;
        if (!xp_u32_fits_int(time)) return XP_ELIMIT;

        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
        if (t.kind != XP_BTOK_INTEGER) return XP_EFORMAT;
        nVals = t.u32;
        if (nVals > XP_MAX_KEY_VALUES) return XP_ELIMIT;

        rr = b_read_reals_exact(ctx, nVals, vals, nVals);
        if (rr!=XP_OK) return rr;

        if (!ctx->current_animation_has_keys) {
            ctx->current_animation_first_tick = (int)time;
            ctx->current_animation_last_tick = (int)time;
            ctx->current_animation_has_keys = 1;
        } else {
            if ((int)time < ctx->current_animation_first_tick) ctx->current_animation_first_tick = (int)time;
            if ((int)time > ctx->current_animation_last_tick) ctx->current_animation_last_tick = (int)time;
        }
        if (cb && cb->on_animation_key) cb->on_animation_key(user, (int)keyType, (int)i, (int)time, (int)nVals, vals);
        flags = 0u;
        semantic = xp_animation_key_semantic_kind((int)keyType, (int)nVals, &flags);
        if (cb && cb->on_animation_key_semantic)
            cb->on_animation_key_semantic(user, (int)keyType, semantic, flags, (int)i, (int)time, (int)nVals, vals);
    }

    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_animation_key_end) cb->on_animation_key_end(user, (int)keyType);
    return XP_OK;
}

static xp_result b_parse_animation(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name) {
    xp_result rr;
    xp_btoken t;
    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "Animation", obj_name); if (rr!=XP_OK) return rr;
    if (ctx->current_animation_set_name[0] != '\0') ctx->current_animation_track_count++;
    if (cb && cb->on_animation_begin) cb->on_animation_begin(user, obj_name?obj_name:"");
    while (1) {
        rr = b_consume_delims(ctx); if (rr!=XP_OK) return rr;
        rr = b_next(ctx, &t); if (rr!=XP_OK) return rr;
        if (t.kind == XP_BTOK_CBRACE) break;
        if (t.kind == XP_BTOK_EOF) return XP_EFORMAT;
        if (t.kind == XP_BTOK_NAME || t.kind == XP_BTOK_OTHER || t.kind == XP_BTOK_TEMPLATE) {
            b_pushback(ctx, &t);
            rr = b_parse_object(ctx, cb, user); if (rr!=XP_OK) return rr;
        } else if (t.kind == XP_BTOK_OBRACE) {
            b_pushback(ctx, &t);
            rr = b_parse_data_reference(ctx, cb, user, 1); if (rr!=XP_OK) return rr;
        } else if (t.kind == XP_BTOK_INTEGER_LIST) {
            rr = xpb_discard_u32_list(&ctx->blx, t.count); if (rr!=XP_OK) return rr;
        } else if (t.kind == XP_BTOK_REAL_LIST) {
            rr = xpb_discard_real_list(&ctx->blx, t.count); if (rr!=XP_OK) return rr;
        } else if (t.kind == XP_BTOK_EOF) {
            return XP_EFORMAT;
        }
    }
    ctx->depth--;
    if (cb && cb->on_animation_end) cb->on_animation_end(user, obj_name?obj_name:"");
    return XP_OK;
}

static xp_result b_parse_animation_set(xp_context* ctx, const xp_callbacks* cb, void* user, const char* obj_name) {
    xp_result rr;
    rr = b_expect_obrace(ctx); if (rr!=XP_OK) return rr;
    rr = b_maybe_class_id(ctx, cb, user, "AnimationSet", obj_name); if (rr!=XP_OK) return rr;
    rr = xp_copyz(ctx->current_animation_set_name, XP_MAX_TOKEN_TEXT, obj_name?obj_name:""); if (rr != XP_OK) return rr;
    ctx->current_animation_track_count = 0u;
    ctx->current_animation_has_keys = 0;
    ctx->current_animation_first_tick = 0;
    ctx->current_animation_last_tick = 0;
    if (cb && cb->on_animation_set_begin) cb->on_animation_set_begin(user, obj_name?obj_name:"");
    rr = b_parse_children_until_cbrace(ctx, cb, user);
    if (rr!=XP_OK) return rr;
    ctx->depth--;
    if (cb && cb->on_animation_set_summary)
        cb->on_animation_set_summary(user, obj_name?obj_name:"", ctx->current_animation_track_count,
            ctx->current_animation_has_keys ? ctx->current_animation_first_tick : 0,
            ctx->current_animation_has_keys ? ctx->current_animation_last_tick : 0,
            ctx->animation_ticks_per_second, ctx->animation_ticks_per_second_known);
    if (cb && cb->on_animation_set_end) cb->on_animation_set_end(user, obj_name?obj_name:"");
    ctx->current_animation_set_name[0] = '\0';
    ctx->current_animation_track_count = 0u;
    ctx->current_animation_has_keys = 0;
    return XP_OK;
}

#endif /* XP_ENABLE_BINARY */
