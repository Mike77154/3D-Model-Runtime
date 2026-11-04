#include "usd3d_variant_prune.h"

#include <string.h>

static void vp_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

void usd3d_variant_pruner_init(usd3d_variant_pruner *p)
{
    if (p == 0) return;
    memset(p, 0, sizeof(*p));
    p->prune_unselected = 1;
}

void usd3d_variant_pruner_set_prune(usd3d_variant_pruner *p, int enabled)
{
    if (p == 0) return;
    p->prune_unselected = enabled ? 1 : 0;
}

int usd3d_variant_pruner_select(usd3d_variant_pruner *p, const char *prim_path, const char *set_name, const char *variant_name)
{
    unsigned long i;
    usd3d_variant_choice *c;
    if (p == 0 || set_name == 0 || variant_name == 0) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < p->count; ++i) {
        c = &p->choices[i];
        if (strcmp(c->prim_path, prim_path != 0 ? prim_path : "") == 0 && strcmp(c->set_name, set_name) == 0) {
            vp_copy(c->variant_name, sizeof(c->variant_name), variant_name);
            c->active = 1;
            return USD3D_OK;
        }
    }
    if (p->count >= (unsigned long)USD3D_VARIANT_PRUNE_MAX) return USD3D_ERR_LIMIT;
    c = &p->choices[p->count++];
    vp_copy(c->prim_path, sizeof(c->prim_path), prim_path);
    vp_copy(c->set_name, sizeof(c->set_name), set_name);
    vp_copy(c->variant_name, sizeof(c->variant_name), variant_name);
    c->active = 1;
    return USD3D_OK;
}

const char *usd3d_variant_pruner_selected(const usd3d_variant_pruner *p, const char *prim_path, const char *set_name)
{
    unsigned long i;
    if (p == 0 || set_name == 0) return "";
    for (i = 0UL; i < p->count; ++i) {
        const usd3d_variant_choice *c = &p->choices[i];
        if (!c->active) continue;
        if (strcmp(c->set_name, set_name) != 0) continue;
        if (strcmp(c->prim_path, prim_path != 0 ? prim_path : "") == 0 || c->prim_path[0] == '\0') return c->variant_name;
    }
    return "";
}

int usd3d_variant_pruner_should_parse(const usd3d_variant_pruner *p, const char *prim_path, const char *set_name, const char *variant_name)
{
    const char *sel;
    if (p == 0 || !p->prune_unselected) return 1;
    sel = usd3d_variant_pruner_selected(p, prim_path, set_name);
    if (sel == 0 || sel[0] == '\0') return 1;
    return strcmp(sel, variant_name != 0 ? variant_name : "") == 0;
}
