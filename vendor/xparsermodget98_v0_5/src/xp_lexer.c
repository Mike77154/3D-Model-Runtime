/* xp_lexer.c - text lexer (C89, arena/static storage)
   MIT License
*/
#include "xp_internal.h"

#if XP_ENABLE_TEXT
static void tok_set(xp_token* t, xp_tok_kind k, const char* txt, int line, int col) {
    t->kind = k;
    t->line = line;
    t->col  = col;
    t->text[0] = '\0';
    if (txt) {
        strncpy(t->text, txt, XP_MAX_TOKEN_TEXT-1);
        t->text[XP_MAX_TOKEN_TEXT-1] = '\0';
    }
}

static int is_ident_start(int c) {
    return (c=='_' || isalpha(c));
}
static int is_ident(int c) {
    return (c=='_' || isalnum(c));
}

#endif /* XP_ENABLE_TEXT */

xp_result xpl_init(xp_lexer* lx, xp_reader* r) {
    memset(lx, 0, sizeof(*lx));
    lx->r = r;
    return XP_OK;
}

#if XP_ENABLE_TEXT
static xp_result read_string(xp_reader* r, xp_token* out) {
    unsigned char c;
    unsigned int n = 0;
    int line = r->line, col = r->col;
    (void)xpr_get(r, &c); /* consume " */

    while (1) {
        xp_result rr = xpr_get(r, &c);
        if (rr == XP_EOF) return XP_EFORMAT;
        if (rr != XP_OK) return rr;
        if (c=='"') break;
        /* Preserve backslashes verbatim instead of silently discarding them.
           This keeps Windows-style texture-path separators intact. */
        if (n+1 < XP_MAX_STRING) {
            if (c == 0u) return XP_EFORMAT;
            out->text[n++] = (char)c;
        } else {
            return XP_ELIMIT;
        }
    }
    out->text[n] = '\0';
    out->kind = XP_TOK_STRING;
    out->line = line; out->col = col;
    return XP_OK;
}

static xp_result read_number(xp_reader* r, xp_token* out) {
    unsigned char c;
    unsigned int n = 0;
    int line = r->line, col = r->col;

    while (1) {
        xp_result rr = xpr_peek(r, &c);
        if (rr != XP_OK) break;
        if (!(isdigit(c) || c=='+' || c=='-' || c=='.' || c=='e' || c=='E')) break;
        rr = xpr_get(r, &c);
        if (rr != XP_OK) return rr;
        if (n+1 < XP_MAX_TOKEN_TEXT) out->text[n++] = (char)c;
        else return XP_ELIMIT;
    }
    out->text[n] = '\0';
    out->kind = XP_TOK_NUMBER;
    out->line = line; out->col = col;
    return XP_OK;
}

static xp_result read_ident(xp_reader* r, xp_token* out) {
    unsigned char c;
    unsigned int n = 0;
    int line = r->line, col = r->col;

    while (1) {
        xp_result rr = xpr_peek(r, &c);
        if (rr != XP_OK) break;
        if (!is_ident(c)) break;
        rr = xpr_get(r, &c);
        if (rr != XP_OK) return rr;
        if (n+1 < XP_MAX_TOKEN_TEXT) out->text[n++] = (char)c;
        else return XP_ELIMIT;
    }
    out->text[n] = '\0';
    out->kind = XP_TOK_IDENT;
    out->line = line; out->col = col;
    return XP_OK;
}

static xp_result read_guid(xp_reader* r, xp_token* out) {
    unsigned char c;
    unsigned int n = 0;
    int line = r->line, col = r->col;

    /* consume '<' */
    (void)xpr_get(r, &c);

    while (1) {
        xp_result rr = xpr_get(r, &c);
        if (rr == XP_EOF) return XP_EFORMAT;
        if (rr != XP_OK) return rr;
        if (c=='>') break;
        if (n+1 < XP_MAX_TOKEN_TEXT) out->text[n++] = (char)c;
        else return XP_ELIMIT;
    }
    out->text[n] = '\0';
    out->kind = XP_TOK_GUID;
    out->line = line; out->col = col;
    return XP_OK;
}
#endif /* XP_ENABLE_TEXT */

xp_result xpl_next(xp_lexer* lx, xp_token* out) {
#if XP_ENABLE_TEXT
    unsigned char c;
    xp_result rr;

    if (lx->has_la) { *out = lx->la; lx->has_la = 0; return XP_OK; }

    rr = xpr_skip_ws_and_comments(lx->r);
    if (rr == XP_EOF) { tok_set(out, XP_TOK_EOF, "", lx->r->line, lx->r->col); return XP_OK; }
    if (rr != XP_OK) return rr;

    rr = xpr_peek(lx->r, &c);
    if (rr == XP_EOF) { tok_set(out, XP_TOK_EOF, "", lx->r->line, lx->r->col); return XP_OK; }
    if (rr != XP_OK) return rr;

    if (c=='{') { (void)xpr_get(lx->r, &c); tok_set(out, XP_TOK_LBRACE, "{", lx->r->line, lx->r->col); return XP_OK; }
    if (c=='}') { (void)xpr_get(lx->r, &c); tok_set(out, XP_TOK_RBRACE, "}", lx->r->line, lx->r->col); return XP_OK; }
    if (c==';') { (void)xpr_get(lx->r, &c); tok_set(out, XP_TOK_SEMI, ";", lx->r->line, lx->r->col); return XP_OK; }
    if (c==',') { (void)xpr_get(lx->r, &c); tok_set(out, XP_TOK_COMMA, ",", lx->r->line, lx->r->col); return XP_OK; }
    if (c=='(') { (void)xpr_get(lx->r, &c); tok_set(out, XP_TOK_LPAREN, "(", lx->r->line, lx->r->col); return XP_OK; }
    if (c==')') { (void)xpr_get(lx->r, &c); tok_set(out, XP_TOK_RPAREN, ")", lx->r->line, lx->r->col); return XP_OK; }
    if (c=='[') { (void)xpr_get(lx->r, &c); tok_set(out, XP_TOK_LBRACKET, "[", lx->r->line, lx->r->col); return XP_OK; }
    if (c==']') { (void)xpr_get(lx->r, &c); tok_set(out, XP_TOK_RBRACKET, "]", lx->r->line, lx->r->col); return XP_OK; }

    if (c=='"') { memset(out, 0, sizeof(*out)); return read_string(lx->r, out); }
    if (c=='<') { memset(out, 0, sizeof(*out)); return read_guid(lx->r, out); }

    if (isdigit(c) || c=='+' || c=='-' || c=='.') {
        memset(out, 0, sizeof(*out));
        return read_number(lx->r, out);
    }

    if (is_ident_start(c)) {
        memset(out, 0, sizeof(*out));
        return read_ident(lx->r, out);
    }

    return XP_EFORMAT;
#else
    (void)lx; (void)out;
    return XP_ENOTSUP;
#endif
}
