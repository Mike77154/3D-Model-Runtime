#ifndef VR1_LEXER_H
#define VR1_LEXER_H

#include "vr1_config.h"

enum {
    VR1_T_EOF = 0,
    VR1_T_IDENT = 1,
    VR1_T_NUMBER = 2,
    VR1_T_STRING = 3,
    VR1_T_LBRACE = 4,
    VR1_T_RBRACE = 5,
    VR1_T_LBRACKET = 6,
    VR1_T_RBRACKET = 7,
    VR1_T_LPAREN = 8,
    VR1_T_RPAREN = 9,
    VR1_T_COMMA = 10,
    VR1_T_PIPE = 11,
    VR1_T_OTHER = 12
};

typedef struct vr1_token_s {
    int type;
    char text[VR1_TOKEN_MAX];
    int len;
    long start;
    long end;
    int line;
    int col;
} vr1_token;

typedef struct vr1_lexer_s {
    const char *buf;
    long len;
    long pos;
    int line;
    int col;
    vr1_token tok;
} vr1_lexer;

void vr1_lexer_init(vr1_lexer *lx, const char *buf, long len, long pos);
int  vr1_lexer_next(vr1_lexer *lx);
int  vr1_lexer_peek(vr1_lexer *lx, vr1_token *out);
int  vr1_token_is(const vr1_token *t, const char *name);
int  vr1_token_is_numberish_start(int c);

#endif
