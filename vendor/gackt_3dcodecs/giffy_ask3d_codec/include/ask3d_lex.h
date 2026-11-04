#ifndef ASK3D_LEX_H
#define ASK3D_LEX_H

#include "ask3d_config.h"
#include "ask3d_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASK3D_TOK_EOF    0
#define ASK3D_TOK_WORD   1
#define ASK3D_TOK_LBRACE 2
#define ASK3D_TOK_RBRACE 3
#define ASK3D_TOK_ERROR  4

typedef struct Ask3dToken_s {
    int type;
    char text[ASK3D_MAX_TOKEN];
    unsigned long line;
} Ask3dToken;

typedef struct Ask3dLexer_s {
    const char *data;
    unsigned long size;
    unsigned long pos;
    unsigned long line;
    int has_push;
    Ask3dToken push;
} Ask3dLexer;

void ask3d_lex_init(Ask3dLexer *lex, const char *data, unsigned long size);
int ask3d_lex_next(Ask3dLexer *lex, Ask3dToken *tok);
void ask3d_lex_push(Ask3dLexer *lex, const Ask3dToken *tok);
int ask3d_tok_is(const Ask3dToken *tok, const char *word);
int ask3d_tok_starts(const Ask3dToken *tok, const char *prefix);
long ask3d_tok_to_i32(const Ask3dToken *tok, int *ok);
ask3d_fixed ask3d_tok_to_fixed(const Ask3dToken *tok, int *ok);

#ifdef __cplusplus
}
#endif

#endif
