#include <ctype.h>
#include <string.h>
#include "vr1_lexer.h"

void vr1_lexer_init(vr1_lexer *lx, const char *buf, long len, long pos)
{
    if (lx == 0) return;
    lx->buf = buf;
    lx->len = len;
    lx->pos = pos;
    lx->line = 1;
    lx->col = 1;
    lx->tok.type = VR1_T_EOF;
    lx->tok.text[0] = '\0';
    lx->tok.len = 0;
    lx->tok.start = pos;
    lx->tok.end = pos;
}

int vr1_token_is_numberish_start(int c)
{
    if (isdigit((unsigned char)c)) return 1;
    if (c == '+' || c == '-' || c == '.') return 1;
    return 0;
}

static int vr1_is_delim(int c)
{
    if (c == 0) return 1;
    if (isspace((unsigned char)c)) return 1;
    if (c == '{' || c == '}' || c == '[' || c == ']') return 1;
    if (c == '(' || c == ')' || c == ',' || c == '|') return 1;
    if (c == '#') return 1;
    return 0;
}

static void vr1_lexer_advance(vr1_lexer *lx)
{
    char c;

    if (lx->pos >= lx->len) return;
    c = lx->buf[lx->pos];
    lx->pos++;
    if (c == '\n') {
        lx->line++;
        lx->col = 1;
    } else {
        lx->col++;
    }
}

static void vr1_lexer_skip_ws(vr1_lexer *lx)
{
    char c;

    while (lx->pos < lx->len) {
        c = lx->buf[lx->pos];
        if (isspace((unsigned char)c)) {
            vr1_lexer_advance(lx);
        } else if (c == '#') {
            while (lx->pos < lx->len && lx->buf[lx->pos] != '\n' && lx->buf[lx->pos] != '\r') {
                vr1_lexer_advance(lx);
            }
        } else {
            break;
        }
    }
}

static void vr1_tok_put(vr1_token *t, int c)
{
    if (t->len < VR1_TOKEN_MAX - 1) {
        t->text[t->len] = (char)c;
        t->len++;
        t->text[t->len] = '\0';
    }
}

int vr1_lexer_next(vr1_lexer *lx)
{
    int c;
    int escaped;

    if (lx == 0) return VR1_T_EOF;
    vr1_lexer_skip_ws(lx);
    lx->tok.start = lx->pos;
    lx->tok.line = lx->line;
    lx->tok.col = lx->col;
    lx->tok.len = 0;
    lx->tok.text[0] = '\0';

    if (lx->pos >= lx->len) {
        lx->tok.type = VR1_T_EOF;
        lx->tok.end = lx->pos;
        return lx->tok.type;
    }

    c = (unsigned char)lx->buf[lx->pos];
    if (c == '{') { vr1_tok_put(&lx->tok, c); vr1_lexer_advance(lx); lx->tok.type = VR1_T_LBRACE; }
    else if (c == '}') { vr1_tok_put(&lx->tok, c); vr1_lexer_advance(lx); lx->tok.type = VR1_T_RBRACE; }
    else if (c == '[') { vr1_tok_put(&lx->tok, c); vr1_lexer_advance(lx); lx->tok.type = VR1_T_LBRACKET; }
    else if (c == ']') { vr1_tok_put(&lx->tok, c); vr1_lexer_advance(lx); lx->tok.type = VR1_T_RBRACKET; }
    else if (c == '(') { vr1_tok_put(&lx->tok, c); vr1_lexer_advance(lx); lx->tok.type = VR1_T_LPAREN; }
    else if (c == ')') { vr1_tok_put(&lx->tok, c); vr1_lexer_advance(lx); lx->tok.type = VR1_T_RPAREN; }
    else if (c == ',') { vr1_tok_put(&lx->tok, c); vr1_lexer_advance(lx); lx->tok.type = VR1_T_COMMA; }
    else if (c == '|') { vr1_tok_put(&lx->tok, c); vr1_lexer_advance(lx); lx->tok.type = VR1_T_PIPE; }
    else if (c == '"') {
        vr1_lexer_advance(lx);
        escaped = 0;
        while (lx->pos < lx->len) {
            c = (unsigned char)lx->buf[lx->pos];
            vr1_lexer_advance(lx);
            if (escaped) {
                vr1_tok_put(&lx->tok, c);
                escaped = 0;
            } else if (c == '\\') {
                escaped = 1;
            } else if (c == '"') {
                break;
            } else {
                vr1_tok_put(&lx->tok, c);
            }
        }
        lx->tok.type = VR1_T_STRING;
    } else {
        while (lx->pos < lx->len) {
            c = (unsigned char)lx->buf[lx->pos];
            if (vr1_is_delim(c)) break;
            vr1_tok_put(&lx->tok, c);
            vr1_lexer_advance(lx);
        }
        if (lx->tok.len > 0 && vr1_token_is_numberish_start((unsigned char)lx->tok.text[0])) {
            lx->tok.type = VR1_T_NUMBER;
        } else {
            lx->tok.type = VR1_T_IDENT;
        }
    }
    lx->tok.end = lx->pos;
    return lx->tok.type;
}

int vr1_lexer_peek(vr1_lexer *lx, vr1_token *out)
{
    vr1_lexer save;
    int t;

    if (lx == 0) return VR1_T_EOF;
    save = *lx;
    t = vr1_lexer_next(lx);
    if (out != 0) *out = lx->tok;
    *lx = save;
    return t;
}

int vr1_token_is(const vr1_token *t, const char *name)
{
    if (t == 0 || name == 0) return 0;
    if (t->type != VR1_T_IDENT && t->type != VR1_T_NUMBER && t->type != VR1_T_STRING) return 0;
    return strcmp(t->text, name) == 0;
}
