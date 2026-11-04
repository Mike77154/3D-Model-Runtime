#include "anima_biovisionbvh_internal.h"

static int anima_bvh_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static void anima_bvh_advance(AnimaBVHLexer *lex)
{
    char c;

    if (lex->pos >= lex->length) {
        return;
    }
    c = lex->text[lex->pos];
    lex->pos++;
    if (c == '\n') {
        lex->line++;
        lex->col = 1U;
    } else {
        lex->col++;
    }
}

void anima_bvh_lexer_init(AnimaBVHLexer *lex, const char *text, unsigned long length)
{
    if (lex == 0) {
        return;
    }
    lex->text = text;
    lex->length = length;
    lex->pos = 0UL;
    lex->line = 1U;
    lex->col = 1U;
    lex->token[0] = '\0';
    lex->token_type = ANIMA_BVH_TOK_EOF;
    lex->token_line = 1U;
    lex->token_col = 1U;
}

static void anima_bvh_skip_ws_and_comments(AnimaBVHLexer *lex)
{
    int again;

    if (lex->pos == 0UL && lex->length >= 3UL &&
        (unsigned char)lex->text[0] == 0xEFU &&
        (unsigned char)lex->text[1] == 0xBBU &&
        (unsigned char)lex->text[2] == 0xBFU) {
        anima_bvh_advance(lex);
        anima_bvh_advance(lex);
        anima_bvh_advance(lex);
    }

    again = 1;
    while (again) {
        again = 0;
        while (lex->pos < lex->length && anima_bvh_is_space(lex->text[lex->pos])) {
            anima_bvh_advance(lex);
        }
        if (lex->pos < lex->length && lex->text[lex->pos] == '#') {
            while (lex->pos < lex->length && lex->text[lex->pos] != '\n') {
                anima_bvh_advance(lex);
            }
            again = 1;
        } else if (lex->pos + 1UL < lex->length && lex->text[lex->pos] == '/' && lex->text[lex->pos + 1UL] == '/') {
            while (lex->pos < lex->length && lex->text[lex->pos] != '\n') {
                anima_bvh_advance(lex);
            }
            again = 1;
        } else if (lex->pos + 1UL < lex->length && lex->text[lex->pos] == '/' && lex->text[lex->pos + 1UL] == '*') {
            anima_bvh_advance(lex);
            anima_bvh_advance(lex);
            while (lex->pos + 1UL < lex->length && !(lex->text[lex->pos] == '*' && lex->text[lex->pos + 1UL] == '/')) {
                anima_bvh_advance(lex);
            }
            if (lex->pos + 1UL < lex->length) {
                anima_bvh_advance(lex);
                anima_bvh_advance(lex);
            }
            again = 1;
        }
    }
}

int anima_bvh_lexer_next(AnimaBVHLexer *lex)
{
    unsigned int n;
    char c;

    if (lex == 0) {
        return ANIMA_BVH_TOK_EOF;
    }

    anima_bvh_skip_ws_and_comments(lex);
    lex->token[0] = '\0';
    lex->token_line = lex->line;
    lex->token_col = lex->col;

    if (lex->pos >= lex->length) {
        lex->token_type = ANIMA_BVH_TOK_EOF;
        return lex->token_type;
    }

    c = lex->text[lex->pos];
    if (c == '{') {
        lex->token[0] = '{';
        lex->token[1] = '\0';
        anima_bvh_advance(lex);
        lex->token_type = ANIMA_BVH_TOK_LBRACE;
        return lex->token_type;
    }
    if (c == '}') {
        lex->token[0] = '}';
        lex->token[1] = '\0';
        anima_bvh_advance(lex);
        lex->token_type = ANIMA_BVH_TOK_RBRACE;
        return lex->token_type;
    }

    n = 0U;
    while (lex->pos < lex->length) {
        c = lex->text[lex->pos];
        if (anima_bvh_is_space(c) || c == '{' || c == '}') {
            break;
        }
        if (n + 1U < (unsigned int)sizeof(lex->token)) {
            lex->token[n] = c;
            n++;
            lex->token[n] = '\0';
        }
        anima_bvh_advance(lex);
    }

    lex->token_type = ANIMA_BVH_TOK_WORD;
    return lex->token_type;
}

static char anima_bvh_lower_ascii_token(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

int anima_bvh_token_is(const AnimaBVHLexer *lex, const char *word)
{
    unsigned int i;

    if (lex == 0 || word == 0) {
        return 0;
    }
    i = 0U;
    while (lex->token[i] != '\0' && word[i] != '\0') {
        if (anima_bvh_lower_ascii_token(lex->token[i]) != anima_bvh_lower_ascii_token(word[i])) {
            return 0;
        }
        i++;
    }
    return lex->token[i] == '\0' && word[i] == '\0';
}

int anima_bvh_parse_int_token(const char *text, int *out_value)
{
    const char *p;
    long v;
    int sign;
    int any;

    if (out_value != 0) {
        *out_value = 0;
    }
    if (text == 0) {
        return 0;
    }

    p = text;
    sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    v = 0L;
    any = 0;
    while (*p >= '0' && *p <= '9') {
        any = 1;
        if (v < (long)(INT_MAX / 10)) {
            v = v * 10L + (long)(*p - '0');
        }
        p++;
    }

    if (!any || *p != '\0') {
        return 0;
    }

    if (sign < 0) {
        v = -v;
    }
    if (v > (long)INT_MAX) {
        v = (long)INT_MAX;
    }
    if (v < (long)INT_MIN) {
        v = (long)INT_MIN;
    }
    if (out_value != 0) {
        *out_value = (int)v;
    }
    return 1;
}
