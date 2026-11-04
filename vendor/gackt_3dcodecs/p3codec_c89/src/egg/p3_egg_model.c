#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "p3_error.h"
#include "p3_egg_model.h"

static int p3_text_eq_ci(const char *a, p3_u16 len, const char *b) {
    p3_u16 i;
    p3_u16 blen;
    blen = (p3_u16)strlen(b);
    if (len != blen) {
        return P3_FALSE;
    }
    for (i = 0; i < len; ++i) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
            return P3_FALSE;
        }
    }
    return P3_TRUE;
}

static int p3_model_copy_name(p3_egg_model *model, const p3_string_pool *pool,
    p3_u16 off, p3_u16 len, p3_u16 *out_off) {
    return p3_string_pool_add(&model->pool, p3_string_pool_ptr(pool, off), len, out_off);
}

static int p3_node_type_is(const p3_egg_ast *ast, p3_u16 node_index, const char *name) {
    const p3_egg_node *node;
    node = &ast->nodes[node_index];
    return p3_text_eq_ci(p3_string_pool_ptr(&ast->pool, node->type_off), node->type_len, name);
}

static int p3_parse_node_atoms_fixed(const p3_egg_ast *ast, p3_u16 node_index, p3_fixed *out_values, p3_u8 max_values, p3_u8 *out_count) {
    p3_u16 item_slot;
    p3_u8 count;
    count = 0;
    item_slot = ast->nodes[node_index].first_item;
    while (item_slot != P3_EGG_NO_ITEM) {
        const p3_egg_item *it;
        it = &ast->items[item_slot];
        if (it->kind == P3_EGG_ITEM_ATOM) {
            const p3_egg_atom *atom;
            char temp[64];
            atom = &ast->atoms[it->index];
            if (p3_string_pool_copy_out(&ast->pool, atom->off, atom->len, temp, (p3_u16)sizeof(temp)) == P3_OK) {
                if (count < max_values) {
                    if (p3_fixed_from_str(temp, &out_values[count]) == P3_OK) {
                        count = (p3_u8)(count + 1U);
                    }
                }
            }
        }
        item_slot = it->next;
    }
    *out_count = count;
    return P3_OK;
}

static int p3_parse_vertex_node(p3_egg_model *model, const p3_egg_ast *ast, p3_u16 node_index) {
    p3_egg_vertex *v;
    const p3_egg_node *node;
    p3_u16 item_slot;
    char temp[64];
    int rc;

    if (model->vertex_count >= P3_EGG_MODEL_MAX_VERTICES) {
        return P3_ERR_CAPACITY;
    }

    node = &ast->nodes[node_index];
    v = &model->vertices[model->vertex_count];
    memset(v, 0, sizeof(*v));
    v->id = 0;

    if (node->name_off != P3_EGG_NO_NAME) {
        if (p3_string_pool_copy_out(&ast->pool, node->name_off, node->name_len, temp, (p3_u16)sizeof(temp)) == P3_OK) {
            v->id = (p3_u16)atoi(temp);
        }
    }

    item_slot = node->first_item;
    while (item_slot != P3_EGG_NO_ITEM) {
        const p3_egg_item *it;
        it = &ast->items[item_slot];
        if (it->kind == P3_EGG_ITEM_ATOM) {
            const p3_egg_atom *atom;
            atom = &ast->atoms[it->index];
            if (v->pos_count < 4U) {
                rc = p3_string_pool_copy_out(&ast->pool, atom->off, atom->len, temp, (p3_u16)sizeof(temp));
                if (rc == P3_OK && p3_fixed_from_str(temp, &v->pos[v->pos_count]) == P3_OK) {
                    v->pos_count = (p3_u8)(v->pos_count + 1U);
                }
            }
        } else if (it->kind == P3_EGG_ITEM_NODE) {
            p3_u16 child;
            child = it->index;
            if (p3_node_type_is(ast, child, "UV")) {
                p3_u8 count;
                p3_parse_node_atoms_fixed(ast, child, v->uv, 2, &count);
                if (count >= 2U) {
                    v->has_uv = 1;
                }
            } else if (p3_node_type_is(ast, child, "Normal")) {
                p3_u8 count2;
                p3_parse_node_atoms_fixed(ast, child, v->normal, 3, &count2);
                if (count2 >= 3U) {
                    v->has_normal = 1;
                }
            } else if (p3_node_type_is(ast, child, "RGBA")) {
                p3_u8 count3;
                p3_parse_node_atoms_fixed(ast, child, v->rgba, 4, &count3);
                if (count3 >= 4U) {
                    v->has_rgba = 1;
                }
            }
        }
        item_slot = it->next;
    }

    model->vertex_count = (p3_u16)(model->vertex_count + 1U);
    return P3_OK;
}

static int p3_parse_polygon_node(p3_egg_model *model, const p3_egg_ast *ast, p3_u16 node_index) {
    p3_egg_polygon *poly;
    p3_u16 item_slot;
    int rc;

    if (model->polygon_count >= P3_EGG_MODEL_MAX_POLYGONS) {
        return P3_ERR_CAPACITY;
    }

    poly = &model->polygons[model->polygon_count];
    memset(poly, 0, sizeof(*poly));
    poly->first_index = model->polygon_index_count;
    poly->tref_off = P3_EGG_NO_NAME;
    poly->mref_off = P3_EGG_NO_NAME;

    item_slot = ast->nodes[node_index].first_item;
    while (item_slot != P3_EGG_NO_ITEM) {
        const p3_egg_item *it;
        it = &ast->items[item_slot];
        if (it->kind == P3_EGG_ITEM_NODE) {
            p3_u16 child;
            child = it->index;
            if (p3_node_type_is(ast, child, "VertexRef")) {
                p3_u16 item2;
                item2 = ast->nodes[child].first_item;
                while (item2 != P3_EGG_NO_ITEM) {
                    const p3_egg_item *it2;
                    it2 = &ast->items[item2];
                    if (it2->kind == P3_EGG_ITEM_ATOM) {
                        const p3_egg_atom *atom;
                        char temp[64];
                        atom = &ast->atoms[it2->index];
                        rc = p3_string_pool_copy_out(&ast->pool, atom->off, atom->len, temp, (p3_u16)sizeof(temp));
                        if (rc == P3_OK) {
                            int id;
                            id = atoi(temp);
                            if (model->polygon_index_count >= P3_EGG_MODEL_MAX_POLY_INDICES) {
                                return P3_ERR_CAPACITY;
                            }
                            model->polygon_indices[model->polygon_index_count] = (p3_u16)id;
                            model->polygon_index_count = (p3_u16)(model->polygon_index_count + 1U);
                            poly->index_count = (p3_u16)(poly->index_count + 1U);
                        }
                    }
                    item2 = it2->next;
                }
            } else if (p3_node_type_is(ast, child, "TRef")) {
                const p3_egg_node *n2;
                p3_u16 atom_slot;
                n2 = &ast->nodes[child];
                atom_slot = n2->first_item;
                while (atom_slot != P3_EGG_NO_ITEM) {
                    const p3_egg_item *it2;
                    it2 = &ast->items[atom_slot];
                    if (it2->kind == P3_EGG_ITEM_ATOM) {
                        const p3_egg_atom *atom;
                        atom = &ast->atoms[it2->index];
                        p3_model_copy_name(model, &ast->pool, atom->off, atom->len, &poly->tref_off);
                        poly->tref_len = atom->len;
                        break;
                    }
                    atom_slot = it2->next;
                }
            } else if (p3_node_type_is(ast, child, "MRef")) {
                const p3_egg_node *n3;
                p3_u16 atom_slot2;
                n3 = &ast->nodes[child];
                atom_slot2 = n3->first_item;
                while (atom_slot2 != P3_EGG_NO_ITEM) {
                    const p3_egg_item *it3;
                    it3 = &ast->items[atom_slot2];
                    if (it3->kind == P3_EGG_ITEM_ATOM) {
                        const p3_egg_atom *atom2;
                        atom2 = &ast->atoms[it3->index];
                        p3_model_copy_name(model, &ast->pool, atom2->off, atom2->len, &poly->mref_off);
                        poly->mref_len = atom2->len;
                        break;
                    }
                    atom_slot2 = it3->next;
                }
            } else if (p3_node_type_is(ast, child, "Normal")) {
                p3_u8 count;
                p3_parse_node_atoms_fixed(ast, child, poly->normal, 3, &count);
                if (count >= 3U) {
                    poly->has_normal = 1;
                }
            } else if (p3_node_type_is(ast, child, "RGBA")) {
                p3_u8 count2;
                p3_parse_node_atoms_fixed(ast, child, poly->rgba, 4, &count2);
                if (count2 >= 4U) {
                    poly->has_rgba = 1;
                }
            }
        }
        item_slot = it->next;
    }

    model->polygon_count = (p3_u16)(model->polygon_count + 1U);
    return P3_OK;
}

static int p3_walk_model_node(p3_egg_model *model, const p3_egg_ast *ast, p3_u16 node_index) {
    const p3_egg_node *node;
    p3_u16 item_slot;
    int rc;
    node = &ast->nodes[node_index];

    if (p3_node_type_is(ast, node_index, "CoordinateSystem")) {
        item_slot = node->first_item;
        while (item_slot != P3_EGG_NO_ITEM) {
            const p3_egg_item *it;
            it = &ast->items[item_slot];
            if (it->kind == P3_EGG_ITEM_ATOM) {
                const p3_egg_atom *atom;
                const char *txt;
                atom = &ast->atoms[it->index];
                txt = p3_string_pool_ptr(&ast->pool, atom->off);
                if (p3_text_eq_ci(txt, atom->len, "Y-up")) {
                    model->coord_system = P3_EGG_COORD_Y_UP;
                } else if (p3_text_eq_ci(txt, atom->len, "Z-up")) {
                    model->coord_system = P3_EGG_COORD_Z_UP;
                } else if (p3_text_eq_ci(txt, atom->len, "Y-up-left")) {
                    model->coord_system = P3_EGG_COORD_Y_UP_LEFT;
                } else if (p3_text_eq_ci(txt, atom->len, "Z-up-left")) {
                    model->coord_system = P3_EGG_COORD_Z_UP_LEFT;
                }
                break;
            }
            item_slot = it->next;
        }
        return P3_OK;
    }

    if (p3_node_type_is(ast, node_index, "Texture")) {
        if (model->texture_count >= P3_EGG_MODEL_MAX_TEXTURES) {
            return P3_ERR_CAPACITY;
        }
        if (node->name_off != P3_EGG_NO_NAME) {
            rc = p3_model_copy_name(model, &ast->pool, node->name_off, node->name_len,
                &model->textures[model->texture_count].name_off);
            if (rc != P3_OK) {
                return rc;
            }
            model->textures[model->texture_count].name_len = node->name_len;
            model->texture_count = (p3_u16)(model->texture_count + 1U);
        }
    } else if (p3_node_type_is(ast, node_index, "Material")) {
        if (model->material_count >= P3_EGG_MODEL_MAX_MATERIALS) {
            return P3_ERR_CAPACITY;
        }
        if (node->name_off != P3_EGG_NO_NAME) {
            rc = p3_model_copy_name(model, &ast->pool, node->name_off, node->name_len,
                &model->materials[model->material_count].name_off);
            if (rc != P3_OK) {
                return rc;
            }
            model->materials[model->material_count].name_len = node->name_len;
            model->material_count = (p3_u16)(model->material_count + 1U);
        }
    } else if (p3_node_type_is(ast, node_index, "VertexPool")) {
        p3_u16 item2;
        p3_u16 pool_index;
        if (model->vertex_pool_count >= P3_EGG_MODEL_MAX_VERTEX_POOLS) {
            return P3_ERR_CAPACITY;
        }
        pool_index = model->vertex_pool_count;
        memset(&model->vertex_pools[pool_index], 0, sizeof(model->vertex_pools[pool_index]));
        model->vertex_pools[pool_index].first_vertex = model->vertex_count;
        if (node->name_off != P3_EGG_NO_NAME) {
            rc = p3_model_copy_name(model, &ast->pool, node->name_off, node->name_len,
                &model->vertex_pools[pool_index].name_off);
            if (rc != P3_OK) {
                return rc;
            }
            model->vertex_pools[pool_index].name_len = node->name_len;
        }
        item2 = node->first_item;
        while (item2 != P3_EGG_NO_ITEM) {
            const p3_egg_item *it2;
            it2 = &ast->items[item2];
            if (it2->kind == P3_EGG_ITEM_NODE) {
                if (p3_node_type_is(ast, it2->index, "Vertex")) {
                    rc = p3_parse_vertex_node(model, ast, it2->index);
                    if (rc != P3_OK) {
                        return rc;
                    }
                    model->vertex_pools[pool_index].vertex_count =
                        (p3_u16)(model->vertex_pools[pool_index].vertex_count + 1U);
                }
            }
            item2 = it2->next;
        }
        model->vertex_pool_count = (p3_u16)(model->vertex_pool_count + 1U);
        return P3_OK;
    } else if (p3_node_type_is(ast, node_index, "Polygon")) {
        return p3_parse_polygon_node(model, ast, node_index);
    } else if (p3_node_type_is(ast, node_index, "Group")) {
        if (model->group_count < P3_EGG_MODEL_MAX_GROUPS && node->name_off != P3_EGG_NO_NAME) {
            rc = p3_model_copy_name(model, &ast->pool, node->name_off, node->name_len,
                &model->groups[model->group_count].name_off);
            if (rc == P3_OK) {
                model->groups[model->group_count].name_len = node->name_len;
                model->group_count = (p3_u16)(model->group_count + 1U);
            }
        }
    } else if (p3_node_type_is(ast, node_index, "Joint")) {
        if (model->joint_count < P3_EGG_MODEL_MAX_JOINTS && node->name_off != P3_EGG_NO_NAME) {
            rc = p3_model_copy_name(model, &ast->pool, node->name_off, node->name_len,
                &model->joints[model->joint_count].name_off);
            if (rc == P3_OK) {
                model->joints[model->joint_count].name_len = node->name_len;
                model->joint_count = (p3_u16)(model->joint_count + 1U);
            }
        }
    }

    item_slot = node->first_item;
    while (item_slot != P3_EGG_NO_ITEM) {
        const p3_egg_item *it;
        it = &ast->items[item_slot];
        if (it->kind == P3_EGG_ITEM_NODE) {
            rc = p3_walk_model_node(model, ast, it->index);
            if (rc != P3_OK) {
                return rc;
            }
        }
        item_slot = it->next;
    }

    return P3_OK;
}

void p3_egg_model_init(p3_egg_model *model) {
    memset(model, 0, sizeof(*model));
    p3_string_pool_init(&model->pool);
    model->coord_system = P3_EGG_COORD_UNSPECIFIED;
}

int p3_egg_model_from_ast(p3_egg_model *model, const p3_egg_ast *ast, char *err, p3_u32 err_cap) {
    p3_u16 item_slot;
    int rc;
    (void)err;
    (void)err_cap;

    if (model == 0 || ast == 0) {
        return P3_ERR_INVALID;
    }

    p3_egg_model_init(model);

    item_slot = ast->nodes[0].first_item;
    while (item_slot != P3_EGG_NO_ITEM) {
        const p3_egg_item *it;
        it = &ast->items[item_slot];
        if (it->kind == P3_EGG_ITEM_NODE) {
            rc = p3_walk_model_node(model, ast, it->index);
            if (rc != P3_OK) {
                return rc;
            }
        }
        item_slot = it->next;
    }
    return P3_OK;
}

int p3_egg_model_summary(const p3_egg_model *model, char *dst, p3_u32 cap) {
    const char *coord;
    (void)cap;
    if (dst == 0) {
        return P3_ERR_INVALID;
    }
    coord = "unspecified";
    if (model->coord_system == P3_EGG_COORD_Y_UP) {
        coord = "Y-up";
    } else if (model->coord_system == P3_EGG_COORD_Z_UP) {
        coord = "Z-up";
    } else if (model->coord_system == P3_EGG_COORD_Y_UP_LEFT) {
        coord = "Y-up-left";
    } else if (model->coord_system == P3_EGG_COORD_Z_UP_LEFT) {
        coord = "Z-up-left";
    }
    sprintf(dst,
        "coord=%s textures=%u materials=%u vertex_pools=%u vertices=%u polygons=%u groups=%u joints=%u",
        coord,
        (unsigned int)model->texture_count,
        (unsigned int)model->material_count,
        (unsigned int)model->vertex_pool_count,
        (unsigned int)model->vertex_count,
        (unsigned int)model->polygon_count,
        (unsigned int)model->group_count,
        (unsigned int)model->joint_count);
    return P3_OK;
}
