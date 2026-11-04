#include <string.h>
#include "p3_error.h"
#include "p3_egg_writer.h"

typedef struct p3_text_out {
    char *dst;
    p3_u32 cap;
    p3_u32 pos;
} p3_text_out;

static int p3_out_putc(p3_text_out *out, char c) {
    if (out->pos + 1UL >= out->cap) {
        return P3_ERR_CAPACITY;
    }
    out->dst[out->pos++] = c;
    out->dst[out->pos] = 0;
    return P3_OK;
}

static int p3_out_puts(p3_text_out *out, const char *text) {
    while (*text) {
        int rc;
        rc = p3_out_putc(out, *text++);
        if (rc != P3_OK) {
            return rc;
        }
    }
    return P3_OK;
}

static int p3_out_putn(p3_text_out *out, const char *text, p3_u16 len) {
    p3_u16 i;
    for (i = 0; i < len; ++i) {
        int rc;
        rc = p3_out_putc(out, text[i]);
        if (rc != P3_OK) {
            return rc;
        }
    }
    return P3_OK;
}

static int p3_needs_quote(const char *text, p3_u16 len) {
    p3_u16 i;
    if (len == 0U) {
        return P3_TRUE;
    }
    for (i = 0; i < len; ++i) {
        char c;
        c = text[i];
        if (c <= ' ' || c == '{' || c == '}' || c == '<' || c == '>' || c == '"' || c == '\\') {
            return P3_TRUE;
        }
    }
    return P3_FALSE;
}

static int p3_write_atom_text(p3_text_out *out, const char *text, p3_u16 len) {
    p3_u16 i;
    int rc;
    if (!p3_needs_quote(text, len)) {
        return p3_out_putn(out, text, len);
    }
    rc = p3_out_putc(out, '"');
    if (rc != P3_OK) {
        return rc;
    }
    for (i = 0; i < len; ++i) {
        char c;
        c = text[i];
        if (c == '"' || c == '\\') {
            rc = p3_out_putc(out, '\\');
            if (rc != P3_OK) {
                return rc;
            }
            rc = p3_out_putc(out, c);
            if (rc != P3_OK) {
                return rc;
            }
        } else if (c == '\n') {
            rc = p3_out_puts(out, "\\n");
            if (rc != P3_OK) {
                return rc;
            }
        } else if (c == '\r') {
            rc = p3_out_puts(out, "\\r");
            if (rc != P3_OK) {
                return rc;
            }
        } else if (c == '\t') {
            rc = p3_out_puts(out, "\\t");
            if (rc != P3_OK) {
                return rc;
            }
        } else {
            rc = p3_out_putc(out, c);
            if (rc != P3_OK) {
                return rc;
            }
        }
    }
    return p3_out_putc(out, '"');
}

static int p3_node_has_only_atoms(const p3_egg_ast *ast, p3_u16 node_index) {
    p3_u16 item_slot;
    item_slot = ast->nodes[node_index].first_item;
    while (item_slot != P3_EGG_NO_ITEM) {
        const p3_egg_item *it;
        it = &ast->items[item_slot];
        if (it->kind != P3_EGG_ITEM_ATOM) {
            return P3_FALSE;
        }
        item_slot = it->next;
    }
    return P3_TRUE;
}

static int p3_write_indent(p3_text_out *out, int indent) {
    int i;
    for (i = 0; i < indent; ++i) {
        int rc;
        rc = p3_out_putc(out, ' ');
        if (rc != P3_OK) {
            return rc;
        }
    }
    return P3_OK;
}

static int p3_write_node(const p3_egg_ast *ast, p3_u16 node_index, p3_text_out *out, int indent) {
    const p3_egg_node *node;
    int rc;
    p3_u16 item_slot;

    node = &ast->nodes[node_index];
    rc = p3_write_indent(out, indent);
    if (rc != P3_OK) {
        return rc;
    }
    rc = p3_out_putc(out, '<');
    if (rc != P3_OK) {
        return rc;
    }
    rc = p3_out_putn(out, p3_string_pool_ptr(&ast->pool, node->type_off), node->type_len);
    if (rc != P3_OK) {
        return rc;
    }
    rc = p3_out_putc(out, '>');
    if (rc != P3_OK) {
        return rc;
    }
    if (node->name_off != P3_EGG_NO_NAME) {
        rc = p3_out_putc(out, ' ');
        if (rc != P3_OK) {
            return rc;
        }
        rc = p3_write_atom_text(out, p3_string_pool_ptr(&ast->pool, node->name_off), node->name_len);
        if (rc != P3_OK) {
            return rc;
        }
    }
    rc = p3_out_puts(out, " {");
    if (rc != P3_OK) {
        return rc;
    }

    if (node->item_count == 0U) {
        rc = p3_out_puts(out, " }\n");
        return rc;
    }

    if (p3_node_has_only_atoms(ast, node_index)) {
        item_slot = node->first_item;
        while (item_slot != P3_EGG_NO_ITEM) {
            const p3_egg_item *it;
            const p3_egg_atom *atom;
            it = &ast->items[item_slot];
            atom = &ast->atoms[it->index];
            rc = p3_out_putc(out, ' ');
            if (rc != P3_OK) {
                return rc;
            }
            rc = p3_write_atom_text(out, p3_string_pool_ptr(&ast->pool, atom->off), atom->len);
            if (rc != P3_OK) {
                return rc;
            }
            item_slot = it->next;
        }
        rc = p3_out_puts(out, " }\n");
        return rc;
    }

    rc = p3_out_putc(out, '\n');
    if (rc != P3_OK) {
        return rc;
    }

    item_slot = node->first_item;
    while (item_slot != P3_EGG_NO_ITEM) {
        const p3_egg_item *it;
        it = &ast->items[item_slot];
        if (it->kind == P3_EGG_ITEM_ATOM) {
            const p3_egg_atom *atom;
            atom = &ast->atoms[it->index];
            rc = p3_write_indent(out, indent + 2);
            if (rc != P3_OK) {
                return rc;
            }
            rc = p3_write_atom_text(out, p3_string_pool_ptr(&ast->pool, atom->off), atom->len);
            if (rc != P3_OK) {
                return rc;
            }
            rc = p3_out_putc(out, '\n');
            if (rc != P3_OK) {
                return rc;
            }
        } else if (it->kind == P3_EGG_ITEM_NODE) {
            rc = p3_write_node(ast, it->index, out, indent + 2);
            if (rc != P3_OK) {
                return rc;
            }
        }
        item_slot = it->next;
    }

    rc = p3_write_indent(out, indent);
    if (rc != P3_OK) {
        return rc;
    }
    rc = p3_out_puts(out, "}\n");
    return rc;
}

int p3_egg_write_canonical(const p3_egg_ast *ast, char *dst, p3_u32 cap, p3_u32 *out_size) {
    p3_text_out out;
    p3_u16 item_slot;
    int rc;
    if (ast == 0 || dst == 0 || out_size == 0) {
        return P3_ERR_INVALID;
    }
    out.dst = dst;
    out.cap = cap;
    out.pos = 0;
    if (cap == 0UL) {
        return P3_ERR_CAPACITY;
    }
    dst[0] = 0;

    item_slot = ast->nodes[0].first_item;
    while (item_slot != P3_EGG_NO_ITEM) {
        const p3_egg_item *it;
        it = &ast->items[item_slot];
        if (it->kind == P3_EGG_ITEM_NODE) {
            rc = p3_write_node(ast, it->index, &out, 0);
            if (rc != P3_OK) {
                return rc;
            }
        }
        item_slot = it->next;
    }

    *out_size = out.pos;
    return P3_OK;
}
