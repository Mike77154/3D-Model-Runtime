#include "usd3d_prim_index.h"

#include <string.h>

static void pi_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

const char *usd3d_prim_arc_name(int arc)
{
    switch (arc) {
    case USD3D_PRIM_ARC_LOCAL: return "Local";
    case USD3D_PRIM_ARC_INHERITS: return "Inherits";
    case USD3D_PRIM_ARC_VARIANTS: return "Variants";
    case USD3D_PRIM_ARC_RELOCATES: return "Relocates";
    case USD3D_PRIM_ARC_REFERENCES: return "References";
    case USD3D_PRIM_ARC_PAYLOADS: return "Payloads";
    case USD3D_PRIM_ARC_SPECIALIZES: return "Specializes";
    case USD3D_PRIM_ARC_FALLBACK: return "Fallback";
    default: return "Unknown";
    }
}

int usd3d_prim_arc_to_livrps_strength(int arc)
{
    switch (arc) {
    case USD3D_PRIM_ARC_LOCAL: return USD3D_LIVRPS_LOCAL;
    case USD3D_PRIM_ARC_INHERITS: return USD3D_LIVRPS_INHERITS;
    case USD3D_PRIM_ARC_VARIANTS: return USD3D_LIVRPS_VARIANTS;
    case USD3D_PRIM_ARC_RELOCATES: return USD3D_LIVRPS_RELOCATES;
    case USD3D_PRIM_ARC_REFERENCES: return USD3D_LIVRPS_REFERENCES;
    case USD3D_PRIM_ARC_PAYLOADS: return USD3D_LIVRPS_PAYLOADS;
    case USD3D_PRIM_ARC_SPECIALIZES: return USD3D_LIVRPS_SPECIALIZES;
    default: return USD3D_LIVRPS_FALLBACK;
    }
}

static long pi_rank(int arc)
{
    switch (arc) {
    case USD3D_PRIM_ARC_LOCAL: return 1000L;
    case USD3D_PRIM_ARC_INHERITS: return 900L;
    case USD3D_PRIM_ARC_VARIANTS: return 800L;
    case USD3D_PRIM_ARC_RELOCATES: return 700L;
    case USD3D_PRIM_ARC_REFERENCES: return 600L;
    case USD3D_PRIM_ARC_PAYLOADS: return 500L;
    case USD3D_PRIM_ARC_SPECIALIZES: return 400L;
    case USD3D_PRIM_ARC_FALLBACK: return 100L;
    default: return 0L;
    }
}

void usd3d_prim_index_init(usd3d_prim_index *pi)
{
    if (pi == 0) return;
    memset(pi, 0, sizeof(*pi));
}

int usd3d_prim_index_add_node(usd3d_prim_index *pi,
                              const char *prim_path,
                              const char *layer_path,
                              const char *target_path,
                              int arc,
                              int node_kind,
                              long layer_index,
                              long node_index)
{
    usd3d_prim_index_node *n;
    if (pi == 0 || prim_path == 0) return USD3D_ERR_BAD_ARG;
    if (pi->node_count >= (unsigned long)USD3D_PRIM_INDEX_MAX_NODES) return USD3D_ERR_LIMIT;
    n = &pi->nodes[pi->node_count++];
    memset(n, 0, sizeof(*n));
    pi_copy(n->prim_path, sizeof(n->prim_path), prim_path);
    pi_copy(n->layer_path, sizeof(n->layer_path), layer_path);
    pi_copy(n->target_path, sizeof(n->target_path), target_path);
    pi_copy(n->arc_name, sizeof(n->arc_name), usd3d_prim_arc_name(arc));
    n->arc = arc;
    n->node_kind = node_kind;
    n->layer_index = layer_index;
    n->node_index = node_index;
    n->strength_rank = pi_rank(arc);
    n->active = 1;
    pi->sorted = 0;
    return USD3D_OK;
}

int usd3d_prim_index_add_opinion(usd3d_prim_index *pi,
                                  const char *prim_path,
                                  const char *attr_name,
                                  const char *value_text,
                                  const char *source_layer,
                                  int arc,
                                  long layer_index,
                                  long node_index)
{
    usd3d_prim_index_opinion *o;
    if (pi == 0 || prim_path == 0 || attr_name == 0) return USD3D_ERR_BAD_ARG;
    if (pi->opinion_count >= (unsigned long)USD3D_PRIM_INDEX_MAX_OPINIONS) return USD3D_ERR_LIMIT;
    o = &pi->opinions[pi->opinion_count++];
    memset(o, 0, sizeof(*o));
    pi_copy(o->prim_path, sizeof(o->prim_path), prim_path);
    pi_copy(o->attr_name, sizeof(o->attr_name), attr_name);
    pi_copy(o->value_text, sizeof(o->value_text), value_text);
    pi_copy(o->source_layer, sizeof(o->source_layer), source_layer);
    o->arc = arc;
    o->layer_index = layer_index;
    o->node_index = node_index;
    o->active = 1;
    return USD3D_OK;
}

static int pi_node_better(const usd3d_prim_index_node *a, const usd3d_prim_index_node *b)
{
    if (a->strength_rank != b->strength_rank) return a->strength_rank > b->strength_rank;
    if (a->layer_index != b->layer_index) return a->layer_index < b->layer_index;
    return a->node_index < b->node_index;
}

int usd3d_prim_index_sort(usd3d_prim_index *pi)
{
    unsigned long i;
    unsigned long j;
    usd3d_prim_index_node tmp;
    if (pi == 0) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < pi->node_count; ++i) {
        for (j = i + 1UL; j < pi->node_count; ++j) {
            if (pi_node_better(&pi->nodes[j], &pi->nodes[i])) {
                tmp = pi->nodes[i];
                pi->nodes[i] = pi->nodes[j];
                pi->nodes[j] = tmp;
            }
        }
    }
    pi->sorted = 1;
    return USD3D_OK;
}

static int pi_opinion_better(const usd3d_prim_index_opinion *a, const usd3d_prim_index_opinion *b)
{
    long ar;
    long br;
    if (b == 0) return 1;
    ar = pi_rank(a->arc);
    br = pi_rank(b->arc);
    if (ar != br) return ar > br;
    if (a->layer_index != b->layer_index) return a->layer_index < b->layer_index;
    return a->node_index < b->node_index;
}

int usd3d_prim_index_resolve_attr(const usd3d_prim_index *pi,
                                  const char *prim_path,
                                  const char *attr_name,
                                  usd3d_prim_index_opinion *out_opinion)
{
    unsigned long i;
    const usd3d_prim_index_opinion *best;
    const usd3d_prim_index_opinion *o;
    if (pi == 0 || prim_path == 0 || attr_name == 0 || out_opinion == 0) return USD3D_ERR_BAD_ARG;
    best = 0;
    for (i = 0UL; i < pi->opinion_count; ++i) {
        o = &pi->opinions[i];
        if (!o->active) continue;
        if (strcmp(o->prim_path, prim_path) != 0) continue;
        if (strcmp(o->attr_name, attr_name) != 0) continue;
        if (pi_opinion_better(o, best)) best = o;
    }
    if (best == 0) return USD3D_ERR_PARSE;
    *out_opinion = *best;
    return USD3D_OK;
}

int usd3d_prim_index_to_livrps(const usd3d_prim_index *pi, usd3d_livrps_state *out_state)
{
    unsigned long i;
    const usd3d_prim_index_opinion *o;
    if (pi == 0 || out_state == 0) return USD3D_ERR_BAD_ARG;
    usd3d_livrps_init(out_state);
    for (i = 0UL; i < pi->opinion_count; ++i) {
        o = &pi->opinions[i];
        if (!o->active) continue;
        usd3d_livrps_add(out_state, o->prim_path, o->attr_name, o->value_text, o->source_layer, usd3d_prim_arc_to_livrps_strength(o->arc), o->layer_index, o->node_index);
    }
    return USD3D_OK;
}

int usd3d_prim_index_compose_listops(const usd3d_prim_index *pi,
                                     const usd3d_listop_state *listops,
                                     const char *prim_path,
                                     const char *attr_name,
                                     int (*item_cb)(void *user, const usd3d_listop_item *item),
                                     void *user)
{
    (void)pi;
    if (listops == 0) return USD3D_ERR_BAD_ARG;
    return usd3d_listop_compose(listops, prim_path, attr_name, item_cb, user);
}
