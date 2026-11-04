#ifndef P3_EGG_TOKENIZER_H
#define P3_EGG_TOKENIZER_H

#include "p3_string_pool.h"
#include "p3_types.h"

typedef enum p3_egg_token_kind {
    P3_EGG_TOK_EOF = 0,
    P3_EGG_TOK_ENTRY = 1,
    P3_EGG_TOK_ATOM = 2,
    P3_EGG_TOK_LBRACE = 3,
    P3_EGG_TOK_RBRACE = 4
} p3_egg_token_kind;

typedef struct p3_egg_token {
    p3_egg_token_kind kind;
    p3_u16 off;
    p3_u16 len;
    p3_u16 line;
} p3_egg_token;

typedef struct p3_egg_tokenizer {
    const char *src;
    p3_u32 size;
    p3_u32 pos;
    p3_u16 line;
    p3_string_pool pool;
} p3_egg_tokenizer;

void p3_egg_tokenizer_init(p3_egg_tokenizer *tz, const char *src, p3_u32 size);
int p3_egg_next_token(p3_egg_tokenizer *tz, p3_egg_token *out_tok);

#endif
