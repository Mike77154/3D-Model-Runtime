#include <string.h>
#include "ask3d_lex.h"

static int ask3d_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static int ask3d_is_end_word(char c)
{
    return ask3d_is_space(c) || c == '{' || c == '}';
}

void ask3d_lex_init(Ask3dLexer *lex, const char *data, unsigned long size)
{
    if (lex == 0) {
        return;
    }
    lex->data = data;
    lex->size = size;
    lex->pos = 0UL;
    lex->line = 1UL;
    lex->has_push = 0;
    memset(&lex->push, 0, sizeof(Ask3dToken));
}

void ask3d_lex_push(Ask3dLexer *lex, const Ask3dToken *tok)
{
    if (lex == 0 || tok == 0) {
        return;
    }
    lex->push = *tok;
    lex->has_push = 1;
}

int ask3d_lex_next(Ask3dLexer *lex, Ask3dToken *tok)
{
    unsigned int n;
    char c;

    if (lex == 0 || tok == 0) {
        return ASK3D_TOK_ERROR;
    }

    if (lex->has_push != 0) {
        *tok = lex->push;
        lex->has_push = 0;
        return tok->type;
    }

    if (lex->pos == 0UL && lex->size >= 3UL) {
        if ((unsigned char)lex->data[0] == 0xEFU && (unsigned char)lex->data[1] == 0xBBU && (unsigned char)lex->data[2] == 0xBFU) {
            lex->pos = 3UL;
        }
    }

    tok->type = ASK3D_TOK_EOF;
    tok->text[0] = '\0';
    tok->line = lex->line;

    while (lex->pos < lex->size) {
        c = lex->data[lex->pos];
        if (c == '\n') {
            lex->line++;
        }
        if (!ask3d_is_space(c)) {
            break;
        }
        lex->pos++;
    }

    tok->line = lex->line;
    if (lex->pos >= lex->size) {
        tok->type = ASK3D_TOK_EOF;
        return tok->type;
    }

    c = lex->data[lex->pos];
    if (c == '{') {
        lex->pos++;
        tok->type = ASK3D_TOK_LBRACE;
        tok->text[0] = '{';
        tok->text[1] = '\0';
        return tok->type;
    }
    if (c == '}') {
        lex->pos++;
        tok->type = ASK3D_TOK_RBRACE;
        tok->text[0] = '}';
        tok->text[1] = '\0';
        return tok->type;
    }

    if (c == '"') {
        lex->pos++;
        n = 0U;
        while (lex->pos < lex->size) {
            c = lex->data[lex->pos];
            lex->pos++;
            if (c == '"') {
                break;
            }
            if (c == '\n') {
                lex->line++;
            }
            if (n + 1U < ASK3D_MAX_TOKEN) {
                tok->text[n++] = c;
            }
        }
        tok->text[n] = '\0';
        tok->type = ASK3D_TOK_WORD;
        return tok->type;
    }

    n = 0U;
    while (lex->pos < lex->size) {
        c = lex->data[lex->pos];
        if (ask3d_is_end_word(c)) {
            break;
        }
        if (n + 1U < ASK3D_MAX_TOKEN) {
            tok->text[n++] = c;
        }
        lex->pos++;
    }
    tok->text[n] = '\0';
    tok->type = ASK3D_TOK_WORD;
    return tok->type;
}

int ask3d_tok_is(const Ask3dToken *tok, const char *word)
{
    if (tok == 0 || word == 0) {
        return 0;
    }
    if (tok->type != ASK3D_TOK_WORD) {
        return 0;
    }
    return strcmp(tok->text, word) == 0;
}

int ask3d_tok_starts(const Ask3dToken *tok, const char *prefix)
{
    unsigned int i;
    if (tok == 0 || prefix == 0) {
        return 0;
    }
    if (tok->type != ASK3D_TOK_WORD) {
        return 0;
    }
    i = 0U;
    while (prefix[i] != '\0') {
        if (tok->text[i] != prefix[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}

long ask3d_tok_to_i32(const Ask3dToken *tok, int *ok)
{
    unsigned int i;
    long sign;
    long v;
    int saw;
    char c;

    if (ok != 0) {
        *ok = 0;
    }
    if (tok == 0 || tok->type != ASK3D_TOK_WORD) {
        return 0;
    }

    i = 0U;
    sign = 1L;
    v = 0L;
    saw = 0;
    if (tok->text[i] == '-') {
        sign = -1L;
        i++;
    } else if (tok->text[i] == '+') {
        i++;
    }

    while (tok->text[i] >= '0' && tok->text[i] <= '9') {
        saw = 1;
        v = (v * 10L) + (long)(tok->text[i] - '0');
        i++;
    }
    c = tok->text[i];
    if (c == ':' || c == ',' || c == ';') {
        c = '\0';
    }
    if (saw != 0 && c == '\0') {
        if (ok != 0) {
            *ok = 1;
        }
        return sign * v;
    }
    return 0;
}

ask3d_fixed ask3d_tok_to_fixed(const Ask3dToken *tok, int *ok)
{
    if (tok == 0 || tok->type != ASK3D_TOK_WORD) {
        if (ok != 0) {
            *ok = 0;
        }
        return 0;
    }
    return ask3d_fixed_parse(tok->text, ok);
}
