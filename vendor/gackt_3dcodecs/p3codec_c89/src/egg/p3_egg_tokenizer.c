#include <string.h>
#include "p3_error.h"
#include "p3_egg_tokenizer.h"

static int p3_egg_is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

static int p3_egg_skip_ws_and_comments(p3_egg_tokenizer *tz) {
    while (tz->pos < tz->size) {
        char c;
        c = tz->src[tz->pos];
        if (p3_egg_is_space(c)) {
            if (c == '\n') {
                tz->line = (p3_u16)(tz->line + 1U);
            }
            tz->pos++;
            continue;
        }
        if (c == '/' && tz->pos + 1U < tz->size) {
            char c2;
            c2 = tz->src[tz->pos + 1U];
            if (c2 == '/') {
                tz->pos += 2U;
                while (tz->pos < tz->size && tz->src[tz->pos] != '\n') {
                    tz->pos++;
                }
                continue;
            }
            if (c2 == '*') {
                tz->pos += 2U;
                while (tz->pos + 1U < tz->size) {
                    if (tz->src[tz->pos] == '\n') {
                        tz->line = (p3_u16)(tz->line + 1U);
                    }
                    if (tz->src[tz->pos] == '*' && tz->src[tz->pos + 1U] == '/') {
                        tz->pos += 2U;
                        break;
                    }
                    tz->pos++;
                }
                continue;
            }
        }
        break;
    }
    return P3_OK;
}

void p3_egg_tokenizer_init(p3_egg_tokenizer *tz, const char *src, p3_u32 size) {
    tz->src = src;
    tz->size = size;
    tz->pos = 0;
    tz->line = 1;
    p3_string_pool_init(&tz->pool);
}

static int p3_egg_emit_simple(p3_egg_token *tok, p3_egg_token_kind kind, p3_u16 line) {
    tok->kind = kind;
    tok->off = 0;
    tok->len = 0;
    tok->line = line;
    return P3_OK;
}

int p3_egg_next_token(p3_egg_tokenizer *tz, p3_egg_token *out_tok) {
    p3_u32 start;
    p3_u16 line;
    p3_u16 off;
    int rc;

    rc = p3_egg_skip_ws_and_comments(tz);
    if (rc != P3_OK) {
        return rc;
    }

    if (tz->pos >= tz->size) {
        return p3_egg_emit_simple(out_tok, P3_EGG_TOK_EOF, tz->line);
    }

    line = tz->line;
    if (tz->src[tz->pos] == '{') {
        tz->pos++;
        return p3_egg_emit_simple(out_tok, P3_EGG_TOK_LBRACE, line);
    }
    if (tz->src[tz->pos] == '}') {
        tz->pos++;
        return p3_egg_emit_simple(out_tok, P3_EGG_TOK_RBRACE, line);
    }
    if (tz->src[tz->pos] == '<') {
        start = ++tz->pos;
        while (tz->pos < tz->size && tz->src[tz->pos] != '>') {
            if (tz->src[tz->pos] == '\n') {
                tz->line = (p3_u16)(tz->line + 1U);
            }
            tz->pos++;
        }
        if (tz->pos >= tz->size) {
            return P3_ERR_TRUNCATED;
        }
        rc = p3_string_pool_add(&tz->pool, tz->src + start, (p3_u16)(tz->pos - start), &off);
        if (rc != P3_OK) {
            return rc;
        }
        tz->pos++;
        out_tok->kind = P3_EGG_TOK_ENTRY;
        out_tok->off = off;
        out_tok->len = (p3_u16)(tz->pos - start - 1U);
        out_tok->line = line;
        return P3_OK;
    }
    if (tz->src[tz->pos] == '"') {
        char temp[512];
        p3_u16 len;
        int esc;
        len = 0;
        esc = 0;
        tz->pos++;
        while (tz->pos < tz->size) {
            char c;
            c = tz->src[tz->pos++];
            if (esc) {
                char outc;
                outc = c;
                if (c == 'n') {
                    outc = '\n';
                } else if (c == 'r') {
                    outc = '\r';
                } else if (c == 't') {
                    outc = '\t';
                } else if (c == '"' || c == '\\') {
                    outc = c;
                }
                if (len >= (p3_u16)(sizeof(temp) - 1U)) {
                    return P3_ERR_CAPACITY;
                }
                temp[len++] = outc;
                esc = 0;
                continue;
            }
            if (c == '\\') {
                esc = 1;
                continue;
            }
            if (c == '"') {
                break;
            }
            if (c == '\n') {
                tz->line = (p3_u16)(tz->line + 1U);
            }
            if (len >= (p3_u16)(sizeof(temp) - 1U)) {
                return P3_ERR_CAPACITY;
            }
            temp[len++] = c;
        }
        rc = p3_string_pool_add(&tz->pool, temp, len, &off);
        if (rc != P3_OK) {
            return rc;
        }
        out_tok->kind = P3_EGG_TOK_ATOM;
        out_tok->off = off;
        out_tok->len = len;
        out_tok->line = line;
        return P3_OK;
    }

    start = tz->pos;
    while (tz->pos < tz->size) {
        char c;
        c = tz->src[tz->pos];
        if (p3_egg_is_space(c) || c == '{' || c == '}' || c == '<' || c == '"' ) {
            break;
        }
        tz->pos++;
    }
    rc = p3_string_pool_add(&tz->pool, tz->src + start, (p3_u16)(tz->pos - start), &off);
    if (rc != P3_OK) {
        return rc;
    }
    out_tok->kind = P3_EGG_TOK_ATOM;
    out_tok->off = off;
    out_tok->len = (p3_u16)(tz->pos - start);
    out_tok->line = line;
    return P3_OK;
}
