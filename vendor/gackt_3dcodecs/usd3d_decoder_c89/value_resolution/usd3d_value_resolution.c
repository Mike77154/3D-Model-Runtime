#include "usd3d_value_resolution.h"

#include <string.h>

static void vr_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

void usd3d_listop_init(usd3d_listop_state *s)
{
    if (s == 0) return;
    memset(s, 0, sizeof(*s));
}

const char *usd3d_listop_name(int op)
{
    switch (op) {
    case USD3D_LISTOP_EXPLICIT: return "explicit";
    case USD3D_LISTOP_PREPEND: return "prepend";
    case USD3D_LISTOP_APPEND: return "append";
    case USD3D_LISTOP_DELETE: return "delete";
    case USD3D_LISTOP_ORDER: return "order";
    default: return "unknown";
    }
}

int usd3d_listop_add(usd3d_listop_state *s,
                     const char *prim_path,
                     const char *attr_name,
                     const char *item_text,
                     const char *source_layer,
                     int op,
                     int strength,
                     long layer_index,
                     long list_index)
{
    usd3d_listop_item *it;
    if (s == 0 || attr_name == 0 || item_text == 0) return USD3D_ERR_BAD_ARG;
    if (s->count >= (unsigned long)USD3D_VALUE_LISTOP_MAX) return USD3D_ERR_LIMIT;
    it = &s->items[s->count++];
    vr_copy(it->prim_path, sizeof(it->prim_path), prim_path);
    vr_copy(it->attr_name, sizeof(it->attr_name), attr_name);
    vr_copy(it->item_text, sizeof(it->item_text), item_text);
    vr_copy(it->source_layer, sizeof(it->source_layer), source_layer);
    it->op = op;
    it->strength = strength;
    it->layer_index = layer_index;
    it->list_index = list_index;
    it->active = 1;
    return USD3D_OK;
}

static int vr_listop_better(const usd3d_listop_item *a, const usd3d_listop_item *b)
{
    if (b == 0) return 1;
    if (a->strength != b->strength) return a->strength > b->strength;
    if (a->layer_index != b->layer_index) return a->layer_index < b->layer_index;
    return a->list_index < b->list_index;
}

static int vr_deleted_by_stronger(const usd3d_listop_state *s, const usd3d_listop_item *item)
{
    unsigned long i;
    const usd3d_listop_item *d;
    for (i = 0UL; i < s->count; ++i) {
        d = &s->items[i];
        if (!d->active || d->op != USD3D_LISTOP_DELETE) continue;
        if (strcmp(d->prim_path, item->prim_path) != 0 || strcmp(d->attr_name, item->attr_name) != 0) continue;
        if (strcmp(d->item_text, item->item_text) != 0) continue;
        if (vr_listop_better(d, item)) return 1;
    }
    return 0;
}

int usd3d_listop_compose(const usd3d_listop_state *s,
                         const char *prim_path,
                         const char *attr_name,
                         int (*item_cb)(void *user, const usd3d_listop_item *item),
                         void *user)
{
    unsigned long i;
    unsigned long pass;
    const usd3d_listop_item *it;
    int has_explicit;
    const usd3d_listop_item *best_explicit;
    if (s == 0 || attr_name == 0 || item_cb == 0) return USD3D_ERR_BAD_ARG;
    has_explicit = 0;
    best_explicit = 0;
    for (i = 0UL; i < s->count; ++i) {
        it = &s->items[i];
        if (!it->active || it->op != USD3D_LISTOP_EXPLICIT) continue;
        if (strcmp(it->attr_name, attr_name) != 0) continue;
        if (prim_path != 0 && strcmp(it->prim_path, prim_path) != 0) continue;
        if (vr_listop_better(it, best_explicit)) {
            best_explicit = it;
            has_explicit = 1;
        }
    }
    if (has_explicit && best_explicit != 0) {
        if (!vr_deleted_by_stronger(s, best_explicit)) {
            if (item_cb(user, best_explicit) != 0) return USD3D_ERR_PARSE;
        }
        return USD3D_OK;
    }
    for (pass = 0UL; pass < 2UL; ++pass) {
        for (i = 0UL; i < s->count; ++i) {
            it = &s->items[i];
            if (!it->active) continue;
            if (strcmp(it->attr_name, attr_name) != 0) continue;
            if (prim_path != 0 && strcmp(it->prim_path, prim_path) != 0) continue;
            if (it->op == USD3D_LISTOP_DELETE || it->op == USD3D_LISTOP_ORDER) continue;
            if (pass == 0UL && it->op != USD3D_LISTOP_PREPEND) continue;
            if (pass == 1UL && it->op != USD3D_LISTOP_APPEND) continue;
            if (vr_deleted_by_stronger(s, it)) continue;
            if (item_cb(user, it) != 0) return USD3D_ERR_PARSE;
        }
    }
    return USD3D_OK;
}

int usd3d_value_resolution_overlay(const usd3d_livrps_state *base,
                                   const usd3d_livrps_state *stronger,
                                   usd3d_livrps_state *out)
{
    unsigned long i;
    int r;
    if (base == 0 || stronger == 0 || out == 0) return USD3D_ERR_BAD_ARG;
    usd3d_livrps_init(out);
    for (i = 0UL; i < base->count; ++i) {
        r = usd3d_livrps_add(out, base->opinions[i].prim_path, base->opinions[i].attr_name, base->opinions[i].value_text, base->opinions[i].source_layer, base->opinions[i].strength, base->opinions[i].layer_index, base->opinions[i].list_index);
        if (r != USD3D_OK) return r;
    }
    for (i = 0UL; i < stronger->count; ++i) {
        r = usd3d_livrps_add(out, stronger->opinions[i].prim_path, stronger->opinions[i].attr_name, stronger->opinions[i].value_text, stronger->opinions[i].source_layer, stronger->opinions[i].strength, stronger->opinions[i].layer_index, stronger->opinions[i].list_index);
        if (r != USD3D_OK) return r;
    }
    return USD3D_OK;
}
