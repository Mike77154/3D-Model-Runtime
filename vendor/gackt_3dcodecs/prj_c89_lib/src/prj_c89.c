#include "prj_c89.h"

#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>

#define PRJ_TOK_EOF       0
#define PRJ_TOK_WORD      1
#define PRJ_TOK_STRING    2
#define PRJ_TOK_NUMBER    3
#define PRJ_TOK_LBRACKET  4
#define PRJ_TOK_RBRACKET  5
#define PRJ_TOK_COMMA     6
#define PRJ_TOK_ERROR     7

#define PRJ_PARSE_MAX_DEPTH 48

#define PRJ_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

typedef struct PRJ_TokenTag {
    int type;
    char text[PRJ_MAX_TEXT];
    size_t raw_len;
    unsigned short line;
    unsigned short column;
    PRJ_Fixed number;
    int number_valid;
} PRJ_Token;

typedef struct PRJ_ScannerTag {
    const char *s;
    size_t pos;
    unsigned short line;
    unsigned short col;
    int has_peek;
    PRJ_Token peek;
    int truncated;
} PRJ_Scanner;

static void prj_memzero(void *p, size_t n)
{
    unsigned char *b;
    size_t i;
    b = (unsigned char *)p;
    for (i = 0U; i < n; ++i) {
        b[i] = 0U;
    }
}

static int prj_ascii_lower(int c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

int prj_strieq(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;
    if (a == 0 || b == 0) return PRJ_FALSE;
    while (*a != '\0' && *b != '\0') {
        ca = (unsigned char)*a;
        cb = (unsigned char)*b;
        if (prj_ascii_lower((int)ca) != prj_ascii_lower((int)cb)) return PRJ_FALSE;
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0') ? PRJ_TRUE : PRJ_FALSE;
}

int prj_starts_with_i(const char *s, const char *prefix)
{
    if (s == 0 || prefix == 0) return PRJ_FALSE;
    while (*prefix != '\0') {
        if (*s == '\0') return PRJ_FALSE;
        if (prj_ascii_lower((unsigned char)*s) != prj_ascii_lower((unsigned char)*prefix)) return PRJ_FALSE;
        ++s;
        ++prefix;
    }
    return PRJ_TRUE;
}

static void prj_copy_text(char *dst, size_t cap, const char *src)
{
    size_t i;
    if (dst == 0 || cap == 0U) return;
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static void prj_copy_span(char *dst, size_t cap, const char *src, size_t len, int *truncated)
{
    size_t i;
    if (dst == 0 || cap == 0U) return;
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    for (i = 0U; i + 1U < cap && i < len; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
    if (i < len && truncated != 0) *truncated = PRJ_TRUE;
}

static void prj_trim_copy(char *dst, size_t cap, const char *src)
{
    const char *a;
    const char *b;
    size_t len;
    if (dst == 0 || cap == 0U) return;
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    a = src;
    while (*a != '\0' && isspace((unsigned char)*a)) ++a;
    b = a + strlen(a);
    while (b > a && isspace((unsigned char)*(b - 1))) --b;
    len = (size_t)(b - a);
    prj_copy_span(dst, cap, a, len, 0);
}

static void prj_append_char(char *out, size_t cap, size_t *used, char c, int *err)
{
    if (*err != PRJ_OK) return;
    if (out == 0 || cap == 0U || used == 0) {
        if (err != 0) *err = PRJ_ERR_NULL;
        return;
    }
    if (*used + 1U >= cap) {
        *err = PRJ_ERR_OUTPUT_SMALL;
        return;
    }
    out[*used] = c;
    *used = *used + 1U;
    out[*used] = '\0';
}

static void prj_append_text(char *out, size_t cap, size_t *used, const char *s, int *err)
{
    if (*err != PRJ_OK) return;
    if (s == 0) s = "";
    while (*s != '\0') {
        prj_append_char(out, cap, used, *s, err);
        if (*err != PRJ_OK) return;
        ++s;
    }
}

static void prj_append_quoted(char *out, size_t cap, size_t *used, const char *s, int *err)
{
    prj_append_char(out, cap, used, '"', err);
    while (s != 0 && *s != '\0' && *err == PRJ_OK) {
        if (*s == '"') {
            prj_append_text(out, cap, used, "\"\"", err);
        } else {
            prj_append_char(out, cap, used, *s, err);
        }
        ++s;
    }
    prj_append_char(out, cap, used, '"', err);
}

void prj_fixed_zero(PRJ_Fixed *fx)
{
    if (fx == 0) return;
    fx->sign = 0;
    fx->whole = 0UL;
    fx->frac = 0UL;
}

int prj_fixed_is_zero(const PRJ_Fixed *fx)
{
    if (fx == 0) return PRJ_TRUE;
    return (fx->sign == 0 || (fx->whole == 0UL && fx->frac == 0UL)) ? PRJ_TRUE : PRJ_FALSE;
}

static unsigned long prj_ul_add_digit(unsigned long v, int digit, int *overflow)
{
    if (v > (ULONG_MAX - (unsigned long)digit) / 10UL) {
        if (overflow != 0) *overflow = PRJ_TRUE;
        return ULONG_MAX;
    }
    return v * 10UL + (unsigned long)digit;
}

static void prj_fixed_normalize(PRJ_Fixed *fx)
{
    if (fx == 0) return;
    if (fx->whole == 0UL && fx->frac == 0UL) fx->sign = 0;
    if (fx->sign > 0) fx->sign = 1;
    if (fx->sign < 0) fx->sign = -1;
}

int prj_fixed_parse(PRJ_Fixed *fx, const char *text, size_t len)
{
    char digits[96];
    int nd;
    int i;
    int point_pos;
    int sign;
    int exp_sign;
    int exp_val;
    int seen_digit;
    int overflow;
    size_t p;
    unsigned long whole;
    unsigned long frac;
    int frac_digits;
    int d;

    if (fx == 0 || text == 0) return PRJ_ERR_NULL;
    prj_fixed_zero(fx);

    nd = 0;
    point_pos = -1;
    sign = 1;
    exp_sign = 1;
    exp_val = 0;
    seen_digit = PRJ_FALSE;
    p = 0U;

    while (p < len && isspace((unsigned char)text[p])) ++p;
    if (p < len && (text[p] == '+' || text[p] == '-')) {
        if (text[p] == '-') sign = -1;
        ++p;
    }

    while (p < len) {
        if (isdigit((unsigned char)text[p])) {
            seen_digit = PRJ_TRUE;
            if (nd < PRJ_ARRAY_COUNT(digits)) {
                digits[nd++] = text[p];
            }
            ++p;
        } else if (text[p] == '.') {
            if (point_pos >= 0) return PRJ_ERR_SYNTAX;
            point_pos = nd;
            ++p;
        } else {
            break;
        }
    }
    if (!seen_digit) return PRJ_ERR_SYNTAX;
    if (point_pos < 0) point_pos = nd;

    if (p < len && (text[p] == 'e' || text[p] == 'E')) {
        ++p;
        if (p < len && (text[p] == '+' || text[p] == '-')) {
            if (text[p] == '-') exp_sign = -1;
            ++p;
        }
        if (p >= len || !isdigit((unsigned char)text[p])) return PRJ_ERR_SYNTAX;
        while (p < len && isdigit((unsigned char)text[p])) {
            if (exp_val < 1000) exp_val = exp_val * 10 + (int)(text[p] - '0');
            ++p;
        }
    }

    while (p < len && isspace((unsigned char)text[p])) ++p;
    if (p != len) return PRJ_ERR_SYNTAX;

    if (exp_sign < 0) point_pos -= exp_val;
    else point_pos += exp_val;

    overflow = PRJ_FALSE;
    whole = 0UL;
    if (point_pos > 0) {
        for (i = 0; i < point_pos; ++i) {
            if (i < nd) d = (int)(digits[i] - '0');
            else d = 0;
            whole = prj_ul_add_digit(whole, d, &overflow);
        }
    }

    frac = 0UL;
    frac_digits = 0;
    for (i = point_pos; frac_digits < 9; ++i) {
        if (i < 0) d = 0;
        else if (i < nd) d = (int)(digits[i] - '0');
        else d = 0;
        frac = frac * 10UL + (unsigned long)d;
        ++frac_digits;
    }

    fx->sign = sign;
    fx->whole = whole;
    fx->frac = frac;
    prj_fixed_normalize(fx);
    if (overflow) return PRJ_ERR_UNSUPPORTED;
    return PRJ_OK;
}

int prj_fixed_to_text(const PRJ_Fixed *fx, char *out, size_t out_cap)
{
    char temp[64];
    char fracbuf[16];
    unsigned long frac;
    size_t used;
    int err;
    int i;
    int last;

    if (fx == 0 || out == 0 || out_cap == 0U) return PRJ_ERR_NULL;
    out[0] = '\0';
    used = 0U;
    err = PRJ_OK;
    if (prj_fixed_is_zero(fx)) {
        prj_append_text(out, out_cap, &used, "0", &err);
        return err;
    }
    if (fx->sign < 0) prj_append_char(out, out_cap, &used, '-', &err);

    temp[0] = '\0';
#if defined(_MSC_VER)
    _snprintf(temp, sizeof(temp), "%lu", fx->whole);
#else
    sprintf(temp, "%lu", fx->whole);
#endif
    prj_append_text(out, out_cap, &used, temp, &err);

    frac = fx->frac;
    if (frac != 0UL) {
        for (i = 8; i >= 0; --i) {
            fracbuf[i] = (char)('0' + (frac % 10UL));
            frac /= 10UL;
        }
        fracbuf[9] = '\0';
        last = 8;
        while (last >= 0 && fracbuf[last] == '0') --last;
        prj_append_char(out, out_cap, &used, '.', &err);
        for (i = 0; i <= last; ++i) {
            prj_append_char(out, out_cap, &used, fracbuf[i], &err);
        }
    }
    return err;
}

int prj_fixed_equal_int(const PRJ_Fixed *fx, long v)
{
    unsigned long av;
    int sg;
    if (fx == 0) return PRJ_FALSE;
    if (v == 0L) return prj_fixed_is_zero(fx);
    sg = (v < 0L) ? -1 : 1;
    av = (v < 0L) ? (unsigned long)(-v) : (unsigned long)v;
    return (fx->sign == sg && fx->whole == av && fx->frac == 0UL) ? PRJ_TRUE : PRJ_FALSE;
}

void prj_context_init(PRJ_Context *ctx)
{
    if (ctx == 0) return;
    prj_memzero(ctx, sizeof(*ctx));
    ctx->root = -1;
}

void prj_crs_info_init(PRJ_CRSInfo *info)
{
    if (info == 0) return;
    prj_memzero(info, sizeof(*info));
    info->kind = PRJ_KIND_UNKNOWN;
}

static void prj_add_error(PRJ_Context *ctx, int code, unsigned short line, unsigned short col, const char *msg)
{
    PRJ_Error *e;
    if (ctx == 0) return;
    if (ctx->error_count >= PRJ_MAX_ERRORS) return;
    e = &ctx->errors[ctx->error_count++];
    e->code = code;
    e->line = line;
    e->column = col;
    prj_copy_text(e->message, sizeof(e->message), msg);
}

static void scanner_init(PRJ_Scanner *sc, const char *s)
{
    prj_memzero(sc, sizeof(*sc));
    sc->s = s;
    sc->line = 1U;
    sc->col = 1U;
}

static int scanner_cur(PRJ_Scanner *sc)
{
    if (sc->s == 0 || sc->s[sc->pos] == '\0') return '\0';
    return (unsigned char)sc->s[sc->pos];
}

static int scanner_next_char(PRJ_Scanner *sc)
{
    int c;
    c = scanner_cur(sc);
    if (c == '\0') return c;
    ++sc->pos;
    if (c == '\n') {
        ++sc->line;
        sc->col = 1U;
    } else {
        if (sc->col < 65535U) ++sc->col;
    }
    return c;
}

static void scanner_skip_ws(PRJ_Scanner *sc)
{
    int c;
    c = scanner_cur(sc);
    while (c != '\0' && isspace((unsigned char)c)) {
        scanner_next_char(sc);
        c = scanner_cur(sc);
    }
}

static int is_word_start(int c)
{
    return (isalpha((unsigned char)c) || c == '_' || c == '$') ? PRJ_TRUE : PRJ_FALSE;
}

static int is_word_char(int c)
{
    return (isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-' || c == ':' || c == '/') ? PRJ_TRUE : PRJ_FALSE;
}

static int is_number_start(PRJ_Scanner *sc)
{
    int c;
    int n;
    c = scanner_cur(sc);
    if (isdigit((unsigned char)c) || c == '.') return PRJ_TRUE;
    if (c == '+' || c == '-') {
        n = (unsigned char)sc->s[sc->pos + 1U];
        if (isdigit((unsigned char)n) || n == '.') return PRJ_TRUE;
    }
    return PRJ_FALSE;
}

static PRJ_Token scanner_read_raw(PRJ_Scanner *sc)
{
    PRJ_Token tok;
    int c;
    size_t start;
    size_t len;
    int trunc;
    int quote;
    size_t out_i;

    prj_memzero(&tok, sizeof(tok));
    tok.type = PRJ_TOK_EOF;
    scanner_skip_ws(sc);
    tok.line = sc->line;
    tok.column = sc->col;
    c = scanner_cur(sc);
    if (c == '\0') {
        tok.type = PRJ_TOK_EOF;
        return tok;
    }
    if (c == '[' || c == '(') {
        tok.type = PRJ_TOK_LBRACKET;
        tok.text[0] = (char)c;
        tok.text[1] = '\0';
        scanner_next_char(sc);
        return tok;
    }
    if (c == ']' || c == ')') {
        tok.type = PRJ_TOK_RBRACKET;
        tok.text[0] = (char)c;
        tok.text[1] = '\0';
        scanner_next_char(sc);
        return tok;
    }
    if (c == ',') {
        tok.type = PRJ_TOK_COMMA;
        tok.text[0] = ',';
        tok.text[1] = '\0';
        scanner_next_char(sc);
        return tok;
    }
    if (c == '"' || c == '\'') {
        quote = c;
        scanner_next_char(sc);
        out_i = 0U;
        while ((c = scanner_cur(sc)) != '\0') {
            if (c == quote) {
                scanner_next_char(sc);
                if (scanner_cur(sc) == quote) {
                    if (out_i + 1U < sizeof(tok.text)) tok.text[out_i++] = (char)quote;
                    else sc->truncated = PRJ_TRUE;
                    scanner_next_char(sc);
                    continue;
                }
                break;
            }
            if (c == '\\') {
                scanner_next_char(sc);
                c = scanner_cur(sc);
                if (c == '\0') break;
            }
            if (out_i + 1U < sizeof(tok.text)) tok.text[out_i++] = (char)c;
            else sc->truncated = PRJ_TRUE;
            scanner_next_char(sc);
        }
        tok.text[out_i] = '\0';
        tok.type = PRJ_TOK_STRING;
        return tok;
    }
    if (is_number_start(sc)) {
        start = sc->pos;
        c = scanner_cur(sc);
        if (c == '+' || c == '-') c = scanner_next_char(sc);
        while (isdigit((unsigned char)scanner_cur(sc))) scanner_next_char(sc);
        if (scanner_cur(sc) == '.') {
            scanner_next_char(sc);
            while (isdigit((unsigned char)scanner_cur(sc))) scanner_next_char(sc);
        }
        if (scanner_cur(sc) == 'e' || scanner_cur(sc) == 'E') {
            scanner_next_char(sc);
            if (scanner_cur(sc) == '+' || scanner_cur(sc) == '-') scanner_next_char(sc);
            while (isdigit((unsigned char)scanner_cur(sc))) scanner_next_char(sc);
        }
        len = sc->pos - start;
        trunc = PRJ_FALSE;
        prj_copy_span(tok.text, sizeof(tok.text), sc->s + start, len, &trunc);
        if (trunc) sc->truncated = PRJ_TRUE;
        tok.raw_len = len;
        tok.type = PRJ_TOK_NUMBER;
        tok.number_valid = (prj_fixed_parse(&tok.number, sc->s + start, len) == PRJ_OK) ? PRJ_TRUE : PRJ_FALSE;
        return tok;
    }
    if (is_word_start(c)) {
        start = sc->pos;
        scanner_next_char(sc);
        while (is_word_char(scanner_cur(sc))) scanner_next_char(sc);
        len = sc->pos - start;
        trunc = PRJ_FALSE;
        prj_copy_span(tok.text, sizeof(tok.text), sc->s + start, len, &trunc);
        if (trunc) sc->truncated = PRJ_TRUE;
        tok.type = PRJ_TOK_WORD;
        tok.raw_len = len;
        return tok;
    }
    tok.type = PRJ_TOK_ERROR;
    tok.text[0] = (char)c;
    tok.text[1] = '\0';
    scanner_next_char(sc);
    return tok;
}

static PRJ_Token scanner_peek(PRJ_Scanner *sc)
{
    if (!sc->has_peek) {
        sc->peek = scanner_read_raw(sc);
        sc->has_peek = PRJ_TRUE;
    }
    return sc->peek;
}

static PRJ_Token scanner_next(PRJ_Scanner *sc)
{
    PRJ_Token t;
    if (sc->has_peek) {
        t = sc->peek;
        sc->has_peek = PRJ_FALSE;
        return t;
    }
    return scanner_read_raw(sc);
}

static int ctx_add_node(PRJ_Context *ctx, int type, int parent, const PRJ_Token *tok)
{
    PRJ_Node *n;
    int idx;
    if (ctx == 0) return -1;
    if (ctx->node_count >= PRJ_MAX_NODES) return -1;
    idx = ctx->node_count++;
    n = &ctx->nodes[idx];
    prj_memzero(n, sizeof(*n));
    n->type = (unsigned char)type;
    n->parent = (short)parent;
    n->first_child = -1;
    n->last_child = -1;
    n->next_sibling = -1;
    if (tok != 0) {
        n->line = tok->line;
        n->column = tok->column;
        prj_copy_text(n->text, sizeof(n->text), tok->text);
        n->number = tok->number;
        n->number_valid = (unsigned char)tok->number_valid;
    }
    if (parent >= 0 && parent < ctx->node_count) {
        PRJ_Node *p;
        p = &ctx->nodes[parent];
        if (p->first_child < 0) p->first_child = (short)idx;
        else ctx->nodes[p->last_child].next_sibling = (short)idx;
        p->last_child = (short)idx;
    }
    return idx;
}

static int parse_value(PRJ_Context *ctx, PRJ_Scanner *sc, int parent, int depth);

static int parse_element_after_word(PRJ_Context *ctx, PRJ_Scanner *sc, int parent, const PRJ_Token *word, int depth)
{
    PRJ_Token t;
    int idx;
    int child;
    if (depth > PRJ_PARSE_MAX_DEPTH) {
        prj_add_error(ctx, PRJ_ERR_DEPTH, word->line, word->column, "WKT nesting is too deep");
        return PRJ_ERR_DEPTH;
    }
    if (ctx->max_depth_seen < depth) ctx->max_depth_seen = depth;
    idx = ctx_add_node(ctx, PRJ_NODE_ELEMENT, parent, word);
    if (idx < 0) {
        prj_add_error(ctx, PRJ_ERR_NODES_FULL, word->line, word->column, "node pool is full");
        return PRJ_ERR_NODES_FULL;
    }
    t = scanner_next(sc);
    if (t.type != PRJ_TOK_LBRACKET) {
        prj_add_error(ctx, PRJ_ERR_SYNTAX, t.line, t.column, "expected [ or ( after WKT keyword");
        return PRJ_ERR_SYNTAX;
    }
    t = scanner_peek(sc);
    if (t.type == PRJ_TOK_RBRACKET) {
        scanner_next(sc);
        return idx;
    }
    for (;;) {
        child = parse_value(ctx, sc, idx, depth + 1);
        if (child < 0) return child;
        t = scanner_next(sc);
        if (t.type == PRJ_TOK_COMMA) continue;
        if (t.type == PRJ_TOK_RBRACKET) break;
        prj_add_error(ctx, PRJ_ERR_SYNTAX, t.line, t.column, "expected comma or closing bracket");
        return PRJ_ERR_SYNTAX;
    }
    return idx;
}

static int parse_value(PRJ_Context *ctx, PRJ_Scanner *sc, int parent, int depth)
{
    PRJ_Token t;
    PRJ_Token p;
    int idx;
    t = scanner_next(sc);
    if (t.type == PRJ_TOK_WORD) {
        p = scanner_peek(sc);
        if (p.type == PRJ_TOK_LBRACKET) {
            return parse_element_after_word(ctx, sc, parent, &t, depth);
        }
        idx = ctx_add_node(ctx, PRJ_NODE_WORD, parent, &t);
        if (idx < 0) {
            prj_add_error(ctx, PRJ_ERR_NODES_FULL, t.line, t.column, "node pool is full");
            return PRJ_ERR_NODES_FULL;
        }
        return idx;
    }
    if (t.type == PRJ_TOK_STRING) {
        idx = ctx_add_node(ctx, PRJ_NODE_STRING, parent, &t);
        if (idx < 0) {
            prj_add_error(ctx, PRJ_ERR_NODES_FULL, t.line, t.column, "node pool is full");
            return PRJ_ERR_NODES_FULL;
        }
        return idx;
    }
    if (t.type == PRJ_TOK_NUMBER) {
        idx = ctx_add_node(ctx, PRJ_NODE_NUMBER, parent, &t);
        if (idx < 0) {
            prj_add_error(ctx, PRJ_ERR_NODES_FULL, t.line, t.column, "node pool is full");
            return PRJ_ERR_NODES_FULL;
        }
        return idx;
    }
    prj_add_error(ctx, PRJ_ERR_SYNTAX, t.line, t.column, "expected WKT value");
    return PRJ_ERR_SYNTAX;
}

int prj_parse_wkt(PRJ_Context *ctx, const char *text)
{
    PRJ_Scanner sc;
    PRJ_Token t;
    int r;
    if (ctx == 0 || text == 0) return PRJ_ERR_NULL;
    prj_context_init(ctx);
    scanner_init(&sc, text);
    t = scanner_next(&sc);
    if (t.type != PRJ_TOK_WORD) {
        prj_add_error(ctx, PRJ_ERR_SYNTAX, t.line, t.column, "WKT must start with a keyword");
        return PRJ_ERR_SYNTAX;
    }
    if (scanner_peek(&sc).type != PRJ_TOK_LBRACKET) {
        prj_add_error(ctx, PRJ_ERR_SYNTAX, t.line, t.column, "WKT root keyword must be followed by [ or (");
        return PRJ_ERR_SYNTAX;
    }
    r = parse_element_after_word(ctx, &sc, -1, &t, 1);
    if (r < 0) return r;
    ctx->root = r;
    t = scanner_next(&sc);
    if (t.type != PRJ_TOK_EOF) {
        prj_add_error(ctx, PRJ_ERR_SYNTAX, t.line, t.column, "trailing text after root WKT element");
        return PRJ_ERR_SYNTAX;
    }
    if (sc.truncated) {
        ctx->truncated_text = PRJ_TRUE;
        prj_add_error(ctx, PRJ_ERR_TEXT_TRUNCATED, 0U, 0U, "one or more text tokens were truncated");
    }
    return PRJ_OK;
}

const PRJ_Node *prj_get_node(const PRJ_Context *ctx, int node_index)
{
    if (ctx == 0 || node_index < 0 || node_index >= ctx->node_count) return 0;
    return &ctx->nodes[node_index];
}

static int node_keyword_is(const PRJ_Context *ctx, int idx, const char *keyword)
{
    const PRJ_Node *n;
    n = prj_get_node(ctx, idx);
    if (n == 0 || n->type != PRJ_NODE_ELEMENT) return PRJ_FALSE;
    return prj_strieq(n->text, keyword);
}

int prj_find_first_child(const PRJ_Context *ctx, int node_index, const char *keyword)
{
    const PRJ_Node *n;
    int c;
    n = prj_get_node(ctx, node_index);
    if (n == 0) return -1;
    c = n->first_child;
    while (c >= 0) {
        if (keyword == 0 || node_keyword_is(ctx, c, keyword)) return c;
        c = ctx->nodes[c].next_sibling;
    }
    return -1;
}

int prj_find_next_sibling(const PRJ_Context *ctx, int node_index, const char *keyword)
{
    const PRJ_Node *n;
    int s;
    n = prj_get_node(ctx, node_index);
    if (n == 0) return -1;
    s = n->next_sibling;
    while (s >= 0) {
        if (keyword == 0 || node_keyword_is(ctx, s, keyword)) return s;
        s = ctx->nodes[s].next_sibling;
    }
    return -1;
}

static int first_child_of_type(const PRJ_Context *ctx, int node_idx, int type)
{
    const PRJ_Node *n;
    int c;
    n = prj_get_node(ctx, node_idx);
    if (n == 0) return -1;
    c = n->first_child;
    while (c >= 0) {
        if (ctx->nodes[c].type == type) return c;
        c = ctx->nodes[c].next_sibling;
    }
    return -1;
}

static int nth_child_of_type(const PRJ_Context *ctx, int node_idx, int type, int nth)
{
    const PRJ_Node *n;
    int c;
    int k;
    n = prj_get_node(ctx, node_idx);
    if (n == 0) return -1;
    k = 0;
    c = n->first_child;
    while (c >= 0) {
        if (ctx->nodes[c].type == type) {
            if (k == nth) return c;
            ++k;
        }
        c = ctx->nodes[c].next_sibling;
    }
    return -1;
}

static void copy_first_string(const PRJ_Context *ctx, int node_idx, char *dst, size_t cap)
{
    int c;
    c = first_child_of_type(ctx, node_idx, PRJ_NODE_STRING);
    if (c >= 0) prj_copy_text(dst, cap, ctx->nodes[c].text);
}

static int copy_nth_number(const PRJ_Context *ctx, int node_idx, int nth, PRJ_Fixed *dst)
{
    int c;
    c = nth_child_of_type(ctx, node_idx, PRJ_NODE_NUMBER, nth);
    if (c >= 0 && ctx->nodes[c].number_valid) {
        *dst = ctx->nodes[c].number;
        return PRJ_TRUE;
    }
    return PRJ_FALSE;
}

const char *prj_kind_name(PRJ_Kind kind)
{
    switch (kind) {
    case PRJ_KIND_GEOGCS: return "GEOGCS";
    case PRJ_KIND_PROJCS: return "PROJCS";
    case PRJ_KIND_GEOCCS: return "GEOCCS";
    case PRJ_KIND_VERT_CS: return "VERT_CS";
    case PRJ_KIND_LOCAL_CS: return "LOCAL_CS";
    case PRJ_KIND_COMPD_CS: return "COMPD_CS";
    case PRJ_KIND_GEODCRS: return "GEODCRS";
    case PRJ_KIND_PROJCRS: return "PROJCRS";
    case PRJ_KIND_GEODCRS_3D: return "GEODCRS_3D";
    case PRJ_KIND_BOUNDCRS: return "BOUNDCRS";
    case PRJ_KIND_ENGCRS: return "ENGCRS";
    case PRJ_KIND_VERTCRS: return "VERTCRS";
    case PRJ_KIND_OLD_ARCINFO: return "OLD_ARCINFO_PRJ";
    default: return "UNKNOWN";
    }
}

static PRJ_Kind kind_from_text(const char *s)
{
    if (prj_strieq(s, "GEOGCS")) return PRJ_KIND_GEOGCS;
    if (prj_strieq(s, "PROJCS")) return PRJ_KIND_PROJCS;
    if (prj_strieq(s, "GEOCCS")) return PRJ_KIND_GEOCCS;
    if (prj_strieq(s, "VERT_CS")) return PRJ_KIND_VERT_CS;
    if (prj_strieq(s, "LOCAL_CS")) return PRJ_KIND_LOCAL_CS;
    if (prj_strieq(s, "COMPD_CS")) return PRJ_KIND_COMPD_CS;
    if (prj_strieq(s, "GEODCRS")) return PRJ_KIND_GEODCRS;
    if (prj_strieq(s, "GEODETICCRS")) return PRJ_KIND_GEODCRS;
    if (prj_strieq(s, "GEODCRS3D")) return PRJ_KIND_GEODCRS_3D;
    if (prj_strieq(s, "PROJCRS")) return PRJ_KIND_PROJCRS;
    if (prj_strieq(s, "PROJECTEDCRS")) return PRJ_KIND_PROJCRS;
    if (prj_strieq(s, "BOUNDCRS")) return PRJ_KIND_BOUNDCRS;
    if (prj_strieq(s, "ENGCRS")) return PRJ_KIND_ENGCRS;
    if (prj_strieq(s, "ENGINEERINGCRS")) return PRJ_KIND_ENGCRS;
    if (prj_strieq(s, "VERTCRS")) return PRJ_KIND_VERTCRS;
    if (prj_strieq(s, "VERTICALCRS")) return PRJ_KIND_VERTCRS;
    return PRJ_KIND_UNKNOWN;
}

void prj_canonical_name(const char *src, char *dst, size_t dst_cap)
{
    size_t i;
    size_t o;
    int last_us;
    unsigned char c;
    if (dst == 0 || dst_cap == 0U) return;
    dst[0] = '\0';
    if (src == 0) return;
    o = 0U;
    last_us = PRJ_FALSE;
    for (i = 0U; src[i] != '\0' && o + 1U < dst_cap; ++i) {
        c = (unsigned char)src[i];
        if (isalnum(c)) {
            dst[o++] = (char)prj_ascii_lower(c);
            last_us = PRJ_FALSE;
        } else {
            if (!last_us && o > 0U && o + 1U < dst_cap) {
                dst[o++] = '_';
                last_us = PRJ_TRUE;
            }
        }
    }
    if (o > 0U && dst[o - 1U] == '_') --o;
    dst[o] = '\0';
}

void prj_esri_to_ogc_parameter_name(const char *src, char *dst, size_t dst_cap)
{
    char c[PRJ_MAX_TEXT];
    prj_canonical_name(src, c, sizeof(c));
    if (prj_strieq(c, "false_easting")) prj_copy_text(dst, dst_cap, "false_easting");
    else if (prj_strieq(c, "false_northing")) prj_copy_text(dst, dst_cap, "false_northing");
    else if (prj_strieq(c, "central_meridian")) prj_copy_text(dst, dst_cap, "central_meridian");
    else if (prj_strieq(c, "latitude_of_origin")) prj_copy_text(dst, dst_cap, "latitude_of_origin");
    else if (prj_strieq(c, "scale_factor")) prj_copy_text(dst, dst_cap, "scale_factor");
    else if (prj_strieq(c, "standard_parallel_1")) prj_copy_text(dst, dst_cap, "standard_parallel_1");
    else if (prj_strieq(c, "standard_parallel_2")) prj_copy_text(dst, dst_cap, "standard_parallel_2");
    else if (prj_strieq(c, "azimuth")) prj_copy_text(dst, dst_cap, "azimuth");
    else if (prj_strieq(c, "longitude_of_center")) prj_copy_text(dst, dst_cap, "longitude_of_center");
    else if (prj_strieq(c, "latitude_of_center")) prj_copy_text(dst, dst_cap, "latitude_of_center");
    else if (prj_strieq(c, "longitude_of_origin")) prj_copy_text(dst, dst_cap, "longitude_of_origin");
    else if (prj_strieq(c, "rectified_grid_angle")) prj_copy_text(dst, dst_cap, "rectified_grid_angle");
    else prj_copy_text(dst, dst_cap, c);
}

static void set_unit(PRJ_Unit *unit, const PRJ_Context *ctx, int node_idx)
{
    if (unit == 0) return;
    copy_first_string(ctx, node_idx, unit->name, sizeof(unit->name));
    if (copy_nth_number(ctx, node_idx, 0, &unit->factor)) unit->has_factor = PRJ_TRUE;
}

static int is_wkt2_unit_keyword(const char *s)
{
    if (prj_strieq(s, "ANGLEUNIT")) return PRJ_TRUE;
    if (prj_strieq(s, "LENGTHUNIT")) return PRJ_TRUE;
    if (prj_strieq(s, "SCALEUNIT")) return PRJ_TRUE;
    if (prj_strieq(s, "TIMEUNIT")) return PRJ_TRUE;
    if (prj_strieq(s, "PARAMETRICUNIT")) return PRJ_TRUE;
    return PRJ_FALSE;
}

static int node_has_ancestor_keyword(const PRJ_Context *ctx, int idx, const char *keyword)
{
    const PRJ_Node *n;
    int p;
    n = prj_get_node(ctx, idx);
    if (n == 0) return PRJ_FALSE;
    p = n->parent;
    while (p >= 0) {
        if (node_keyword_is(ctx, p, keyword)) return PRJ_TRUE;
        p = ctx->nodes[p].parent;
    }
    return PRJ_FALSE;
}

static void add_param(PRJ_CRSInfo *info, const PRJ_Context *ctx, int node_idx)
{
    PRJ_Parameter *p;
    if (info == 0 || info->param_count >= PRJ_MAX_PARAMS) return;
    p = &info->params[info->param_count];
    copy_first_string(ctx, node_idx, p->name, sizeof(p->name));
    if (p->name[0] == '\0') return;
    prj_esri_to_ogc_parameter_name(p->name, p->canonical, sizeof(p->canonical));
    if (copy_nth_number(ctx, node_idx, 0, &p->value)) p->has_value = PRJ_TRUE;
    ++info->param_count;
}

static void add_axis(PRJ_CRSInfo *info, const PRJ_Context *ctx, int node_idx)
{
    PRJ_Axis *a;
    int c;
    int got_dir;
    if (info == 0 || info->axis_count >= PRJ_MAX_AXES) return;
    a = &info->axes[info->axis_count];
    copy_first_string(ctx, node_idx, a->name, sizeof(a->name));
    got_dir = PRJ_FALSE;
    c = ctx->nodes[node_idx].first_child;
    while (c >= 0) {
        if (ctx->nodes[c].type == PRJ_NODE_WORD) {
            prj_copy_text(a->direction, sizeof(a->direction), ctx->nodes[c].text);
            got_dir = PRJ_TRUE;
            break;
        }
        if (ctx->nodes[c].type == PRJ_NODE_STRING && c != first_child_of_type(ctx, node_idx, PRJ_NODE_STRING)) {
            prj_copy_text(a->direction, sizeof(a->direction), ctx->nodes[c].text);
            got_dir = PRJ_TRUE;
            break;
        }
        c = ctx->nodes[c].next_sibling;
    }
    if (a->name[0] != '\0' || got_dir) ++info->axis_count;
}

static void add_id(PRJ_CRSInfo *info, const PRJ_Context *ctx, int node_idx)
{
    PRJ_Id *id;
    int c0;
    int c1;
    if (info == 0 || info->id_count >= PRJ_MAX_IDS) return;
    c0 = first_child_of_type(ctx, node_idx, PRJ_NODE_STRING);
    if (c0 < 0) return;
    id = &info->ids[info->id_count];
    prj_copy_text(id->authority, sizeof(id->authority), ctx->nodes[c0].text);
    c1 = ctx->nodes[c0].next_sibling;
    while (c1 >= 0) {
        if (ctx->nodes[c1].type == PRJ_NODE_STRING || ctx->nodes[c1].type == PRJ_NODE_WORD) {
            prj_copy_text(id->code, sizeof(id->code), ctx->nodes[c1].text);
            break;
        }
        if (ctx->nodes[c1].type == PRJ_NODE_NUMBER) {
            prj_fixed_to_text(&ctx->nodes[c1].number, id->code, sizeof(id->code));
            break;
        }
        c1 = ctx->nodes[c1].next_sibling;
    }
    ++info->id_count;
    info->flags |= PRJ_FLAG_HAS_AUTHORITY;
}

static void walk_extract(const PRJ_Context *ctx, PRJ_CRSInfo *info, int idx)
{
    const PRJ_Node *n;
    int c;
    int parent;
    n = prj_get_node(ctx, idx);
    if (n == 0) return;
    if (n->type == PRJ_NODE_ELEMENT) {
        if (prj_strieq(n->text, "PROJECTION")) {
            copy_first_string(ctx, idx, info->projection, sizeof(info->projection));
            info->flags |= PRJ_FLAG_HAS_PROJECTION;
        } else if (prj_strieq(n->text, "METHOD")) {
            copy_first_string(ctx, idx, info->method, sizeof(info->method));
            if (info->projection[0] == '\0') prj_copy_text(info->projection, sizeof(info->projection), info->method);
            info->flags |= PRJ_FLAG_HAS_PROJECTION | PRJ_FLAG_WKT2_HINT;
        } else if (prj_strieq(n->text, "DATUM") || prj_strieq(n->text, "GEODATUM") || prj_strieq(n->text, "TRF")) {
            if (info->datum[0] == '\0') copy_first_string(ctx, idx, info->datum, sizeof(info->datum));
            info->flags |= PRJ_FLAG_HAS_DATUM;
        } else if (prj_strieq(n->text, "SPHEROID") || prj_strieq(n->text, "ELLIPSOID")) {
            copy_first_string(ctx, idx, info->ellipsoid, sizeof(info->ellipsoid));
            if (copy_nth_number(ctx, idx, 0, &info->semi_major_axis) &&
                copy_nth_number(ctx, idx, 1, &info->inverse_flattening)) {
                info->has_ellipsoid_numbers = PRJ_TRUE;
            }
            info->flags |= PRJ_FLAG_HAS_ELLIPSOID;
            if (prj_strieq(n->text, "ELLIPSOID")) info->flags |= PRJ_FLAG_WKT2_HINT;
        } else if (prj_strieq(n->text, "PRIMEM") || prj_strieq(n->text, "PRIMEMERIDIAN")) {
            copy_first_string(ctx, idx, info->prime_meridian, sizeof(info->prime_meridian));
            if (copy_nth_number(ctx, idx, 0, &info->prime_meridian_offset)) info->has_prime_meridian = PRJ_TRUE;
        } else if (prj_strieq(n->text, "PARAMETER")) {
            add_param(info, ctx, idx);
        } else if (prj_strieq(n->text, "UNIT")) {
            parent = n->parent;
            if (parent >= 0 && (node_keyword_is(ctx, parent, "GEOGCS") || node_has_ancestor_keyword(ctx, parent, "GEOGCS") || node_has_ancestor_keyword(ctx, parent, "GEODCRS"))) {
                if (!info->angular_unit.has_factor) set_unit(&info->angular_unit, ctx, idx);
            } else if (parent >= 0 && (node_keyword_is(ctx, parent, "VERT_CS") || node_keyword_is(ctx, parent, "VERTCRS"))) {
                if (!info->vertical_unit.has_factor) set_unit(&info->vertical_unit, ctx, idx);
            } else {
                if (!info->linear_unit.has_factor) set_unit(&info->linear_unit, ctx, idx);
            }
            info->flags |= PRJ_FLAG_HAS_UNIT;
        } else if (prj_strieq(n->text, "ANGLEUNIT")) {
            set_unit(&info->angular_unit, ctx, idx);
            info->flags |= PRJ_FLAG_HAS_UNIT | PRJ_FLAG_WKT2_HINT;
        } else if (prj_strieq(n->text, "LENGTHUNIT")) {
            if (node_has_ancestor_keyword(ctx, idx, "VERTCRS")) set_unit(&info->vertical_unit, ctx, idx);
            else set_unit(&info->linear_unit, ctx, idx);
            info->flags |= PRJ_FLAG_HAS_UNIT | PRJ_FLAG_WKT2_HINT;
        } else if (is_wkt2_unit_keyword(n->text)) {
            info->flags |= PRJ_FLAG_WKT2_HINT;
        } else if (prj_strieq(n->text, "AXIS")) {
            add_axis(info, ctx, idx);
        } else if (prj_strieq(n->text, "AUTHORITY") || prj_strieq(n->text, "ID")) {
            add_id(info, ctx, idx);
            if (prj_strieq(n->text, "ID")) info->flags |= PRJ_FLAG_WKT2_HINT;
        } else if (prj_strieq(n->text, "BASEGEOGCRS") || prj_strieq(n->text, "CONVERSION") || prj_strieq(n->text, "CS")) {
            info->flags |= PRJ_FLAG_WKT2_HINT;
        }
    }
    c = n->first_child;
    while (c >= 0) {
        walk_extract(ctx, info, c);
        c = ctx->nodes[c].next_sibling;
    }
}

static void detect_esri_hint(PRJ_CRSInfo *info)
{
    int i;
    if (info == 0) return;
    if (prj_starts_with_i(info->name, "GCS_") || prj_starts_with_i(info->name, "PCS_")) info->flags |= PRJ_FLAG_ESRI_HINT;
    if (prj_starts_with_i(info->datum, "D_")) info->flags |= PRJ_FLAG_ESRI_HINT;
    for (i = 0; i < info->param_count; ++i) {
        if (strchr(info->params[i].name, '_') != 0) info->flags |= PRJ_FLAG_ESRI_HINT;
    }
}

int prj_extract_crs(const PRJ_Context *ctx, PRJ_CRSInfo *info)
{
    const PRJ_Node *root;
    if (ctx == 0 || info == 0) return PRJ_ERR_NULL;
    prj_crs_info_init(info);
    root = prj_get_node(ctx, ctx->root);
    if (root == 0 || root->type != PRJ_NODE_ELEMENT) return PRJ_ERR_SYNTAX;
    info->kind = kind_from_text(root->text);
    prj_copy_text(info->kind_text, sizeof(info->kind_text), root->text);
    copy_first_string(ctx, ctx->root, info->name, sizeof(info->name));
    walk_extract(ctx, info, ctx->root);
    detect_esri_hint(info);
    return PRJ_OK;
}

static void old_set_line(PRJ_CRSInfo *info, const char *key, const char *val)
{
    PRJ_Parameter *p;
    if (prj_strieq(key, "Projection")) {
        prj_copy_text(info->projection, sizeof(info->projection), val);
        info->flags |= PRJ_FLAG_HAS_PROJECTION;
    } else if (prj_strieq(key, "Datum")) {
        prj_copy_text(info->datum, sizeof(info->datum), val);
        info->flags |= PRJ_FLAG_HAS_DATUM;
    } else if (prj_strieq(key, "Spheroid") || prj_strieq(key, "Ellipsoid")) {
        prj_copy_text(info->ellipsoid, sizeof(info->ellipsoid), val);
        info->flags |= PRJ_FLAG_HAS_ELLIPSOID;
    } else if (prj_strieq(key, "Units") || prj_strieq(key, "Zunits")) {
        if (!prj_strieq(val, "NO")) {
            prj_copy_text(info->linear_unit.name, sizeof(info->linear_unit.name), val);
            info->linear_unit.has_factor = PRJ_FALSE;
            info->flags |= PRJ_FLAG_HAS_UNIT;
        }
    } else if (prj_strieq(key, "Zone") || prj_strieq(key, "Xshift") || prj_strieq(key, "Yshift") || prj_strieq(key, "Parameters")) {
        if (info->param_count < PRJ_MAX_PARAMS) {
            p = &info->params[info->param_count++];
            prj_copy_text(p->name, sizeof(p->name), key);
            prj_esri_to_ogc_parameter_name(key, p->canonical, sizeof(p->canonical));
            if (prj_fixed_parse(&p->value, val, strlen(val)) == PRJ_OK) p->has_value = PRJ_TRUE;
        }
    }
}

static int parse_old_arcinfo_prj(PRJ_CRSInfo *info, const char *text)
{
    const char *p;
    const char *line_start;
    const char *line_end;
    char line[192];
    char key[PRJ_MAX_TEXT];
    char val[PRJ_MAX_TEXT];
    size_t len;
    size_t i;
    size_t k;
    size_t v;
    int found;

    if (info == 0 || text == 0) return PRJ_ERR_NULL;
    prj_crs_info_init(info);
    info->kind = PRJ_KIND_OLD_ARCINFO;
    prj_copy_text(info->kind_text, sizeof(info->kind_text), "OLD_ARCINFO_PRJ");
    prj_copy_text(info->name, sizeof(info->name), "Old Arc/Info PRJ");
    info->old_arcinfo_detected = PRJ_TRUE;
    info->flags |= PRJ_FLAG_OLD_ARCINFO_HINT;
    p = text;
    found = PRJ_FALSE;
    while (*p != '\0') {
        line_start = p;
        while (*p != '\0' && *p != '\n' && *p != '\r') ++p;
        line_end = p;
        while (*p == '\n' || *p == '\r') ++p;
        len = (size_t)(line_end - line_start);
        if (len >= sizeof(line)) len = sizeof(line) - 1U;
        prj_copy_span(line, sizeof(line), line_start, len, 0);
        prj_trim_copy(line, sizeof(line), line);
        if (line[0] == '\0') continue;
        key[0] = '\0';
        val[0] = '\0';
        i = 0U;
        while (line[i] != '\0' && !isspace((unsigned char)line[i])) ++i;
        prj_copy_span(key, sizeof(key), line, i, 0);
        while (line[i] != '\0' && isspace((unsigned char)line[i])) ++i;
        k = 0U;
        v = i;
        while (line[v] != '\0' && k + 1U < sizeof(val)) val[k++] = line[v++];
        val[k] = '\0';
        prj_trim_copy(val, sizeof(val), val);
        if (key[0] != '\0') {
            found = PRJ_TRUE;
            old_set_line(info, key, val);
        }
    }
    return found ? PRJ_OK : PRJ_ERR_SYNTAX;
}

int prj_parse_prj(PRJ_Context *ctx, PRJ_CRSInfo *info, const char *text)
{
    const char *p;
    int r;
    if (ctx == 0 || info == 0 || text == 0) return PRJ_ERR_NULL;
    p = text;
    while (*p != '\0' && isspace((unsigned char)*p)) ++p;
    if (strchr(p, '[') != 0 || strchr(p, '(') != 0) {
        r = prj_parse_wkt(ctx, text);
        if (r != PRJ_OK) return r;
        return prj_extract_crs(ctx, info);
    }
    prj_context_init(ctx);
    return parse_old_arcinfo_prj(info, text);
}

int prj_validate_crs(const PRJ_CRSInfo *info, char *out, size_t out_cap)
{
    size_t used;
    int err;
    int ok;
    if (info == 0 || out == 0 || out_cap == 0U) return PRJ_ERR_NULL;
    out[0] = '\0';
    used = 0U;
    err = PRJ_OK;
    ok = PRJ_TRUE;
    if (info->kind == PRJ_KIND_UNKNOWN) {
        prj_append_text(out, out_cap, &used, "unknown root CRS keyword; ", &err);
        ok = PRJ_FALSE;
    }
    if (info->name[0] == '\0') {
        prj_append_text(out, out_cap, &used, "missing CRS name; ", &err);
        ok = PRJ_FALSE;
    }
    if ((info->kind == PRJ_KIND_PROJCS || info->kind == PRJ_KIND_PROJCRS) && info->projection[0] == '\0') {
        prj_append_text(out, out_cap, &used, "projected CRS lacks PROJECTION/METHOD; ", &err);
        ok = PRJ_FALSE;
    }
    if ((info->kind == PRJ_KIND_GEOGCS || info->kind == PRJ_KIND_PROJCS || info->kind == PRJ_KIND_GEODCRS || info->kind == PRJ_KIND_PROJCRS) && info->datum[0] == '\0') {
        prj_append_text(out, out_cap, &used, "datum not found; ", &err);
    }
    if (!(info->flags & PRJ_FLAG_HAS_UNIT)) {
        prj_append_text(out, out_cap, &used, "unit not found; ", &err);
    }
    if (out[0] == '\0') prj_append_text(out, out_cap, &used, "ok", &err);
    return (err != PRJ_OK) ? err : (ok ? PRJ_OK : PRJ_ERR_UNSUPPORTED);
}

static void write_fixed_or_zero(const PRJ_Fixed *fx, char *out, size_t cap, size_t *used, int *err)
{
    char b[64];
    if (prj_fixed_to_text(fx, b, sizeof(b)) != PRJ_OK) prj_copy_text(b, sizeof(b), "0");
    prj_append_text(out, cap, used, b, err);
}

int prj_write_wkt1(const PRJ_CRSInfo *info, char *out, size_t out_cap)
{
    size_t used;
    int err;
    int i;
    PRJ_Fixed zero;
    if (info == 0 || out == 0 || out_cap == 0U) return PRJ_ERR_NULL;
    out[0] = '\0';
    used = 0U;
    err = PRJ_OK;
    prj_fixed_zero(&zero);

    if (info->kind == PRJ_KIND_PROJCS || info->kind == PRJ_KIND_PROJCRS || info->projection[0] != '\0') {
        prj_append_text(out, out_cap, &used, "PROJCS[", &err);
        prj_append_quoted(out, out_cap, &used, (info->name[0] != '\0') ? info->name : "unnamed projected CRS", &err);
        prj_append_text(out, out_cap, &used, ",", &err);
    } else {
        prj_append_text(out, out_cap, &used, "GEOGCS[", &err);
        prj_append_quoted(out, out_cap, &used, (info->name[0] != '\0') ? info->name : "unnamed geographic CRS", &err);
        prj_append_text(out, out_cap, &used, ",", &err);
    }

    if (info->kind == PRJ_KIND_PROJCS || info->kind == PRJ_KIND_PROJCRS || info->projection[0] != '\0') {
        prj_append_text(out, out_cap, &used, "GEOGCS[", &err);
        prj_append_quoted(out, out_cap, &used, (info->datum[0] != '\0') ? info->datum : "unnamed geogcs", &err);
        prj_append_text(out, out_cap, &used, ",", &err);
    }

    prj_append_text(out, out_cap, &used, "DATUM[", &err);
    prj_append_quoted(out, out_cap, &used, (info->datum[0] != '\0') ? info->datum : "unnamed datum", &err);
    prj_append_text(out, out_cap, &used, ",SPHEROID[", &err);
    prj_append_quoted(out, out_cap, &used, (info->ellipsoid[0] != '\0') ? info->ellipsoid : "unnamed ellipsoid", &err);
    prj_append_text(out, out_cap, &used, ",", &err);
    write_fixed_or_zero(info->has_ellipsoid_numbers ? &info->semi_major_axis : &zero, out, out_cap, &used, &err);
    prj_append_text(out, out_cap, &used, ",", &err);
    write_fixed_or_zero(info->has_ellipsoid_numbers ? &info->inverse_flattening : &zero, out, out_cap, &used, &err);
    prj_append_text(out, out_cap, &used, "]]", &err);

    prj_append_text(out, out_cap, &used, ",PRIMEM[", &err);
    prj_append_quoted(out, out_cap, &used, (info->prime_meridian[0] != '\0') ? info->prime_meridian : "Greenwich", &err);
    prj_append_text(out, out_cap, &used, ",", &err);
    write_fixed_or_zero(info->has_prime_meridian ? &info->prime_meridian_offset : &zero, out, out_cap, &used, &err);
    prj_append_text(out, out_cap, &used, "]", &err);

    prj_append_text(out, out_cap, &used, ",UNIT[", &err);
    prj_append_quoted(out, out_cap, &used, (info->angular_unit.name[0] != '\0') ? info->angular_unit.name : "Degree", &err);
    prj_append_text(out, out_cap, &used, ",", &err);
    if (info->angular_unit.has_factor) write_fixed_or_zero(&info->angular_unit.factor, out, out_cap, &used, &err);
    else prj_append_text(out, out_cap, &used, "0.017453292", &err);
    prj_append_text(out, out_cap, &used, "]", &err);

    if (info->kind == PRJ_KIND_PROJCS || info->kind == PRJ_KIND_PROJCRS || info->projection[0] != '\0') {
        prj_append_text(out, out_cap, &used, "]", &err);
        prj_append_text(out, out_cap, &used, ",PROJECTION[", &err);
        prj_append_quoted(out, out_cap, &used, (info->projection[0] != '\0') ? info->projection : "Unknown", &err);
        prj_append_text(out, out_cap, &used, "]", &err);
        for (i = 0; i < info->param_count; ++i) {
            if (!info->params[i].has_value) continue;
            prj_append_text(out, out_cap, &used, ",PARAMETER[", &err);
            prj_append_quoted(out, out_cap, &used, info->params[i].name, &err);
            prj_append_text(out, out_cap, &used, ",", &err);
            write_fixed_or_zero(&info->params[i].value, out, out_cap, &used, &err);
            prj_append_text(out, out_cap, &used, "]", &err);
        }
        prj_append_text(out, out_cap, &used, ",UNIT[", &err);
        prj_append_quoted(out, out_cap, &used, (info->linear_unit.name[0] != '\0') ? info->linear_unit.name : "metre", &err);
        prj_append_text(out, out_cap, &used, ",", &err);
        if (info->linear_unit.has_factor) write_fixed_or_zero(&info->linear_unit.factor, out, out_cap, &used, &err);
        else prj_append_text(out, out_cap, &used, "1", &err);
        prj_append_text(out, out_cap, &used, "]", &err);
    }
    prj_append_text(out, out_cap, &used, "]", &err);
    return err;
}

int prj_write_summary(const PRJ_CRSInfo *info, char *out, size_t out_cap)
{
    size_t used;
    int err;
    int i;
    char num[64];
    if (info == 0 || out == 0 || out_cap == 0U) return PRJ_ERR_NULL;
    out[0] = '\0';
    used = 0U;
    err = PRJ_OK;
    prj_append_text(out, out_cap, &used, "kind=", &err);
    prj_append_text(out, out_cap, &used, prj_kind_name(info->kind), &err);
    prj_append_text(out, out_cap, &used, "\nname=", &err);
    prj_append_text(out, out_cap, &used, info->name, &err);
    prj_append_text(out, out_cap, &used, "\nprojection=", &err);
    prj_append_text(out, out_cap, &used, info->projection, &err);
    prj_append_text(out, out_cap, &used, "\ndatum=", &err);
    prj_append_text(out, out_cap, &used, info->datum, &err);
    prj_append_text(out, out_cap, &used, "\nellipsoid=", &err);
    prj_append_text(out, out_cap, &used, info->ellipsoid, &err);
    if (info->has_ellipsoid_numbers) {
        prj_append_text(out, out_cap, &used, "\nsemi_major_axis=", &err);
        prj_fixed_to_text(&info->semi_major_axis, num, sizeof(num));
        prj_append_text(out, out_cap, &used, num, &err);
        prj_append_text(out, out_cap, &used, "\ninverse_flattening=", &err);
        prj_fixed_to_text(&info->inverse_flattening, num, sizeof(num));
        prj_append_text(out, out_cap, &used, num, &err);
    }
    prj_append_text(out, out_cap, &used, "\nangular_unit=", &err);
    prj_append_text(out, out_cap, &used, info->angular_unit.name, &err);
    prj_append_text(out, out_cap, &used, "\nlinear_unit=", &err);
    prj_append_text(out, out_cap, &used, info->linear_unit.name, &err);
    for (i = 0; i < info->param_count; ++i) {
        prj_append_text(out, out_cap, &used, "\nparam.", &err);
        prj_append_text(out, out_cap, &used, info->params[i].canonical, &err);
        prj_append_text(out, out_cap, &used, "=", &err);
        if (info->params[i].has_value) {
            prj_fixed_to_text(&info->params[i].value, num, sizeof(num));
            prj_append_text(out, out_cap, &used, num, &err);
        }
    }
    for (i = 0; i < info->id_count; ++i) {
        prj_append_text(out, out_cap, &used, "\nid=", &err);
        prj_append_text(out, out_cap, &used, info->ids[i].authority, &err);
        prj_append_text(out, out_cap, &used, ":", &err);
        prj_append_text(out, out_cap, &used, info->ids[i].code, &err);
    }
    return err;
}
