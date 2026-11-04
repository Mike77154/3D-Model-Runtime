#include "usd3d_livrps.h"

#include <string.h>

static void liv_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

void usd3d_livrps_init(usd3d_livrps_state *s)
{
    if (s == 0) return;
    memset(s, 0, sizeof(*s));
}

const char *usd3d_livrps_strength_name(int strength)
{
    if (strength >= USD3D_LIVRPS_LOCAL) return "L:local";
    if (strength >= USD3D_LIVRPS_INHERITS) return "I:inherits";
    if (strength >= USD3D_LIVRPS_VARIANTS) return "V:variants";
    if (strength >= USD3D_LIVRPS_RELOCATES) return "R:relocates";
    if (strength >= USD3D_LIVRPS_REFERENCES) return "R:references";
    if (strength >= USD3D_LIVRPS_PAYLOADS) return "P:payloads";
    if (strength >= USD3D_LIVRPS_SPECIALIZES) return "S:specializes";
    return "fallback";
}

int usd3d_livrps_strength_from_arc(const char *arc_type)
{
    if (arc_type == 0) return USD3D_LIVRPS_FALLBACK;
    if (strcmp(arc_type, "local") == 0 || strcmp(arc_type, "subLayer") == 0) return USD3D_LIVRPS_LOCAL;
    if (strcmp(arc_type, "inherits") == 0 || strcmp(arc_type, "inherit") == 0) return USD3D_LIVRPS_INHERITS;
    if (strcmp(arc_type, "variant") == 0 || strcmp(arc_type, "variantSet") == 0) return USD3D_LIVRPS_VARIANTS;
    if (strcmp(arc_type, "relocates") == 0 || strcmp(arc_type, "relocate") == 0) return USD3D_LIVRPS_RELOCATES;
    if (strcmp(arc_type, "references") == 0 || strcmp(arc_type, "reference") == 0) return USD3D_LIVRPS_REFERENCES;
    if (strcmp(arc_type, "payload") == 0 || strcmp(arc_type, "payloads") == 0) return USD3D_LIVRPS_PAYLOADS;
    if (strcmp(arc_type, "specializes") == 0 || strcmp(arc_type, "specialize") == 0) return USD3D_LIVRPS_SPECIALIZES;
    return USD3D_LIVRPS_FALLBACK;
}

int usd3d_livrps_add(usd3d_livrps_state *s,
                     const char *prim_path,
                     const char *attr_name,
                     const char *value_text,
                     const char *source_layer,
                     int strength,
                     long layer_index,
                     long list_index)
{
    usd3d_livrps_opinion *o;
    if (s == 0) return USD3D_ERR_BAD_ARG;
    if (s->count >= (unsigned long)USD3D_LIVRPS_MAX_OPINIONS) return USD3D_ERR_LIMIT;
    o = &s->opinions[s->count++];
    liv_copy(o->prim_path, sizeof(o->prim_path), prim_path);
    liv_copy(o->attr_name, sizeof(o->attr_name), attr_name);
    liv_copy(o->value_text, sizeof(o->value_text), value_text);
    liv_copy(o->source_layer, sizeof(o->source_layer), source_layer);
    o->strength = strength;
    o->layer_index = layer_index;
    o->list_index = list_index;
    o->active = 1;
    return USD3D_OK;
}

static int liv_better(const usd3d_livrps_opinion *a, const usd3d_livrps_opinion *b)
{
    if (b == 0) return 1;
    if (a->strength != b->strength) return a->strength > b->strength;
    if (a->layer_index != b->layer_index) return a->layer_index < b->layer_index;
    return a->list_index < b->list_index;
}

int usd3d_livrps_resolve(const usd3d_livrps_state *s,
                         const char *prim_path,
                         const char *attr_name,
                         usd3d_livrps_opinion *out_opinion)
{
    unsigned long i;
    const usd3d_livrps_opinion *best;
    if (s == 0 || out_opinion == 0) return USD3D_ERR_BAD_ARG;
    best = 0;
    for (i = 0UL; i < s->count; ++i) {
        const usd3d_livrps_opinion *o = &s->opinions[i];
        if (!o->active) continue;
        if (prim_path != 0 && strcmp(o->prim_path, prim_path) != 0) continue;
        if (attr_name != 0 && strcmp(o->attr_name, attr_name) != 0) continue;
        if (liv_better(o, best)) best = o;
    }
    if (best == 0) return USD3D_ERR_UNSUPPORTED;
    *out_opinion = *best;
    return USD3D_OK;
}

int usd3d_livrps_resolve_all(const usd3d_livrps_state *s,
                             int (*resolved_cb)(void *user, const usd3d_livrps_opinion *opinion),
                             void *user)
{
    unsigned long i;
    unsigned long j;
    usd3d_livrps_opinion best;
    int already;
    int r;
    if (s == 0 || resolved_cb == 0) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < s->count; ++i) {
        if (!s->opinions[i].active) continue;
        already = 0;
        for (j = 0UL; j < i; ++j) {
            if (strcmp(s->opinions[i].prim_path, s->opinions[j].prim_path) == 0 && strcmp(s->opinions[i].attr_name, s->opinions[j].attr_name) == 0) {
                already = 1;
                break;
            }
        }
        if (already) continue;
        r = usd3d_livrps_resolve(s, s->opinions[i].prim_path, s->opinions[i].attr_name, &best);
        if (r == USD3D_OK) {
            if (resolved_cb(user, &best) != 0) return USD3D_ERR_PARSE;
        }
    }
    return USD3D_OK;
}
