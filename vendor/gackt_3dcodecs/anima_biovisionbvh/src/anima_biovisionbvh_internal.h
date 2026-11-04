#ifndef ANIMA_BIOVISIONBVH_INTERNAL_H
#define ANIMA_BIOVISIONBVH_INTERNAL_H

#include "anima_biovisionbvh.h"
#include <stddef.h>
#include <limits.h>
#include <string.h>

#define ANIMA_BVH_TOK_EOF    0
#define ANIMA_BVH_TOK_WORD   1
#define ANIMA_BVH_TOK_LBRACE 2
#define ANIMA_BVH_TOK_RBRACE 3

#define ANIMA_BVH_STACK_MAX ANIMA_BVH_MAX_JOINTS

#ifndef ANIMA_BVH_FIXED_MAX
#define ANIMA_BVH_FIXED_MAX LONG_MAX
#endif

#ifndef ANIMA_BVH_FIXED_MIN
#define ANIMA_BVH_FIXED_MIN LONG_MIN
#endif

typedef struct AnimaBVHLexer_s {
    const char *text;
    unsigned long length;
    unsigned long pos;
    unsigned int line;
    unsigned int col;
    char token[ANIMA_BVH_TOKEN_MAX];
    int token_type;
    unsigned int token_line;
    unsigned int token_col;
} AnimaBVHLexer;

void anima_bvh_lexer_init(AnimaBVHLexer *lex, const char *text, unsigned long length);
int anima_bvh_lexer_next(AnimaBVHLexer *lex);
int anima_bvh_token_is(const AnimaBVHLexer *lex, const char *word);

void anima_bvh_set_error(AnimaBVH *bvh, const AnimaBVHLexer *lex, const char *msg);
void anima_bvh_copy_name(AnimaBVH *bvh, char *dst, const char *src);
int anima_bvh_parse_int_token(const char *text, int *out_value);

void anima_bvh_mat_identity(anima_bvh_q15 m[9]);
void anima_bvh_mat_mul(const anima_bvh_q15 a[9], const anima_bvh_q15 b[9], anima_bvh_q15 out[9]);
void anima_bvh_mat_axis_rotation(AnimaBVHChannelType axis, anima_bvh_fixed degrees, anima_bvh_q15 out[9]);
void anima_bvh_mat_transform_point(const anima_bvh_q15 m[9], const anima_bvh_fixed in_v[3], anima_bvh_fixed out_v[3]);
anima_bvh_fixed anima_bvh_fixed_add_sat(anima_bvh_fixed a, anima_bvh_fixed b);
anima_bvh_fixed anima_bvh_fixed_mul_q15(anima_bvh_fixed value, anima_bvh_q15 q15);
int anima_bvh_write_raw(AnimaBVHWriteFn write_fn, void *user, const char *text);

#endif
