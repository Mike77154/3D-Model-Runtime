#include "p3_error.h"
#include "p3_egg_ast.h"

void p3_egg_ast_init(p3_egg_ast *ast) {
    p3_u16 root_index;
    p3_string_pool_init(&ast->pool);
    ast->node_count = 0;
    ast->atom_count = 0;
    ast->item_count = 0;
    root_index = 0;
    p3_egg_ast_add_node(ast, "Root", 4, 0, 0, 0, &root_index);
}

int p3_egg_ast_add_atom(p3_egg_ast *ast, const char *text, p3_u16 text_len, p3_u16 line, p3_u16 *out_index) {
    p3_u16 off;
    int rc;
    if (ast->atom_count >= P3_EGG_MAX_ATOMS) {
        return P3_ERR_CAPACITY;
    }
    rc = p3_string_pool_add(&ast->pool, text, text_len, &off);
    if (rc != P3_OK) {
        return rc;
    }
    ast->atoms[ast->atom_count].off = off;
    ast->atoms[ast->atom_count].len = text_len;
    ast->atoms[ast->atom_count].line = line;
    *out_index = ast->atom_count;
    ast->atom_count = (p3_u16)(ast->atom_count + 1U);
    return P3_OK;
}

int p3_egg_ast_add_node(p3_egg_ast *ast, const char *type_text, p3_u16 type_len,
    const char *name_text, p3_u16 name_len, p3_u16 line, p3_u16 *out_index) {
    p3_u16 type_off;
    p3_u16 name_off;
    int rc;
    if (ast->node_count >= P3_EGG_MAX_NODES) {
        return P3_ERR_CAPACITY;
    }
    rc = p3_string_pool_add(&ast->pool, type_text, type_len, &type_off);
    if (rc != P3_OK) {
        return rc;
    }
    name_off = P3_EGG_NO_NAME;
    if (name_text != 0 && name_len > 0U) {
        rc = p3_string_pool_add(&ast->pool, name_text, name_len, &name_off);
        if (rc != P3_OK) {
            return rc;
        }
    }
    ast->nodes[ast->node_count].type_off = type_off;
    ast->nodes[ast->node_count].type_len = type_len;
    ast->nodes[ast->node_count].name_off = name_off;
    ast->nodes[ast->node_count].name_len = name_len;
    ast->nodes[ast->node_count].first_item = P3_EGG_NO_ITEM;
    ast->nodes[ast->node_count].last_item = P3_EGG_NO_ITEM;
    ast->nodes[ast->node_count].item_count = 0;
    ast->nodes[ast->node_count].line = line;
    *out_index = ast->node_count;
    ast->node_count = (p3_u16)(ast->node_count + 1U);
    return P3_OK;
}

int p3_egg_ast_add_item(p3_egg_ast *ast, p3_u16 node_index, p3_u8 kind, p3_u16 item_index) {
    p3_u16 slot;
    if (node_index >= ast->node_count) {
        return P3_ERR_RANGE;
    }
    if (ast->item_count >= P3_EGG_MAX_ITEMS) {
        return P3_ERR_CAPACITY;
    }
    slot = ast->item_count;
    ast->items[slot].kind = kind;
    ast->items[slot].index = item_index;
    ast->items[slot].next = P3_EGG_NO_ITEM;
    ast->item_count = (p3_u16)(ast->item_count + 1U);

    if (ast->nodes[node_index].first_item == P3_EGG_NO_ITEM) {
        ast->nodes[node_index].first_item = slot;
        ast->nodes[node_index].last_item = slot;
    } else {
        ast->items[ast->nodes[node_index].last_item].next = slot;
        ast->nodes[node_index].last_item = slot;
    }
    ast->nodes[node_index].item_count = (p3_u16)(ast->nodes[node_index].item_count + 1U);
    return P3_OK;
}
