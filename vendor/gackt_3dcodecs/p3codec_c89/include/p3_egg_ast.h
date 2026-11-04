#ifndef P3_EGG_AST_H
#define P3_EGG_AST_H

#include "p3_config.h"
#include "p3_string_pool.h"
#include "p3_types.h"

#define P3_EGG_NO_NAME 0xffffU
#define P3_EGG_NO_ITEM 0xffffU

typedef enum p3_egg_item_kind {
    P3_EGG_ITEM_ATOM = 1,
    P3_EGG_ITEM_NODE = 2
} p3_egg_item_kind;

typedef struct p3_egg_atom {
    p3_u16 off;
    p3_u16 len;
    p3_u16 line;
} p3_egg_atom;

typedef struct p3_egg_item {
    p3_u8 kind;
    p3_u16 index;
    p3_u16 next;
} p3_egg_item;

typedef struct p3_egg_node {
    p3_u16 type_off;
    p3_u16 type_len;
    p3_u16 name_off;
    p3_u16 name_len;
    p3_u16 first_item;
    p3_u16 last_item;
    p3_u16 item_count;
    p3_u16 line;
} p3_egg_node;

typedef struct p3_egg_ast {
    p3_string_pool pool;
    p3_egg_node nodes[P3_EGG_MAX_NODES];
    p3_u16 node_count;
    p3_egg_atom atoms[P3_EGG_MAX_ATOMS];
    p3_u16 atom_count;
    p3_egg_item items[P3_EGG_MAX_ITEMS];
    p3_u16 item_count;
} p3_egg_ast;

void p3_egg_ast_init(p3_egg_ast *ast);
int p3_egg_ast_add_atom(p3_egg_ast *ast, const char *text, p3_u16 text_len, p3_u16 line, p3_u16 *out_index);
int p3_egg_ast_add_node(p3_egg_ast *ast, const char *type_text, p3_u16 type_len,
    const char *name_text, p3_u16 name_len, p3_u16 line, p3_u16 *out_index);
int p3_egg_ast_add_item(p3_egg_ast *ast, p3_u16 node_index, p3_u8 kind, p3_u16 item_index);

#endif
