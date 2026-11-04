#include <stdio.h>
#include "p3_error.h"
#include "p3_egg_parser.h"
#include "p3_egg_tokenizer.h"

typedef struct p3_egg_parser_state {
    p3_egg_tokenizer tz;
    p3_egg_token cur;
    char *err;
    p3_u32 err_cap;
} p3_egg_parser_state;

static void p3_egg_set_error(p3_egg_parser_state *ps, const char *msg, p3_u16 line) {
    if (ps->err != 0 && ps->err_cap > 0UL) {
        sprintf(ps->err, "line %u: %s", (unsigned int)line, msg);
    }
}

static int p3_egg_advance(p3_egg_parser_state *ps) {
    return p3_egg_next_token(&ps->tz, &ps->cur);
}

static int p3_egg_parse_entry(p3_egg_parser_state *ps, p3_egg_ast *ast, p3_u16 parent_index, p3_u16 *out_node_index) {
    p3_egg_token type_tok;
    p3_egg_token name_tok;
    p3_u16 node_index;
    int rc;
    int have_name;

    type_tok = ps->cur;
    have_name = P3_FALSE;

    rc = p3_egg_advance(ps);
    if (rc != P3_OK) {
        return rc;
    }

    if (ps->cur.kind == P3_EGG_TOK_ATOM) {
        name_tok = ps->cur;
        have_name = P3_TRUE;
        rc = p3_egg_advance(ps);
        if (rc != P3_OK) {
            return rc;
        }
    }

    if (ps->cur.kind != P3_EGG_TOK_LBRACE) {
        p3_egg_set_error(ps, "expected '{' after entry header", ps->cur.line);
        return P3_ERR_SYNTAX;
    }

    rc = p3_egg_ast_add_node(ast,
        p3_string_pool_ptr(&ps->tz.pool, type_tok.off), type_tok.len,
        have_name ? p3_string_pool_ptr(&ps->tz.pool, name_tok.off) : 0,
        have_name ? name_tok.len : 0,
        type_tok.line,
        &node_index);
    if (rc != P3_OK) {
        return rc;
    }

    rc = p3_egg_ast_add_item(ast, parent_index, P3_EGG_ITEM_NODE, node_index);
    if (rc != P3_OK) {
        return rc;
    }

    rc = p3_egg_advance(ps);
    if (rc != P3_OK) {
        return rc;
    }

    while (ps->cur.kind != P3_EGG_TOK_RBRACE) {
        if (ps->cur.kind == P3_EGG_TOK_EOF) {
            p3_egg_set_error(ps, "unexpected end of file inside entry", type_tok.line);
            return P3_ERR_TRUNCATED;
        }
        if (ps->cur.kind == P3_EGG_TOK_ENTRY) {
            rc = p3_egg_parse_entry(ps, ast, node_index, 0);
            if (rc != P3_OK) {
                return rc;
            }
            continue;
        }
        if (ps->cur.kind == P3_EGG_TOK_ATOM) {
            p3_u16 atom_index;
            rc = p3_egg_ast_add_atom(ast,
                p3_string_pool_ptr(&ps->tz.pool, ps->cur.off), ps->cur.len,
                ps->cur.line, &atom_index);
            if (rc != P3_OK) {
                return rc;
            }
            rc = p3_egg_ast_add_item(ast, node_index, P3_EGG_ITEM_ATOM, atom_index);
            if (rc != P3_OK) {
                return rc;
            }
            rc = p3_egg_advance(ps);
            if (rc != P3_OK) {
                return rc;
            }
            continue;
        }
        p3_egg_set_error(ps, "unexpected token in entry body", ps->cur.line);
        return P3_ERR_SYNTAX;
    }

    rc = p3_egg_advance(ps);
    if (rc != P3_OK) {
        return rc;
    }

    if (out_node_index != 0) {
        *out_node_index = node_index;
    }
    return P3_OK;
}

int p3_egg_parse(p3_egg_ast *ast, const char *src, p3_u32 size, char *err, p3_u32 err_cap) {
    p3_egg_parser_state ps;
    int rc;

    if (ast == 0 || src == 0) {
        return P3_ERR_INVALID;
    }

    p3_egg_ast_init(ast);
    ps.err = err;
    ps.err_cap = err_cap;
    p3_egg_tokenizer_init(&ps.tz, src, size);
    rc = p3_egg_advance(&ps);
    if (rc != P3_OK) {
        return rc;
    }

    while (ps.cur.kind != P3_EGG_TOK_EOF) {
        if (ps.cur.kind != P3_EGG_TOK_ENTRY) {
            p3_egg_set_error(&ps, "expected entry at top level", ps.cur.line);
            return P3_ERR_SYNTAX;
        }
        rc = p3_egg_parse_entry(&ps, ast, 0, 0);
        if (rc != P3_OK) {
            return rc;
        }
    }

    return P3_OK;
}
