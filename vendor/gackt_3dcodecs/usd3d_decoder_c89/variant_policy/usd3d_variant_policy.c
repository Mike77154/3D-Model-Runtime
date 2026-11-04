#include "usd3d_variant_policy.h"

#include <string.h>
#include <ctype.h>

static void vp_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

void usd3d_variant_policy_init(usd3d_variant_policy *p)
{
    if (p == 0) return;
    memset(p, 0, sizeof(*p));
    p->inherit_to_children = 1;
    p->parse_unselected_when_no_selection = 0;
}

const char *usd3d_variant_selection_source_name(int source)
{
    switch (source) {
    case USD3D_VARIANT_SEL_EXPLICIT: return "explicit";
    case USD3D_VARIANT_SEL_INHERITED: return "inherited";
    case USD3D_VARIANT_SEL_DEFAULT: return "default";
    case USD3D_VARIANT_SEL_BLOCKED: return "blocked";
    default: return "unknown";
    }
}

static int vp_path_applies(const char *sel_path, const char *prim_path, int inherit)
{
    unsigned long n;
    if (sel_path == 0 || prim_path == 0) return 0;
    if (strcmp(sel_path, prim_path) == 0) return 1;
    if (!inherit) return 0;
    n = (unsigned long)strlen(sel_path);
    if (n == 0UL) return 0;
    if (strncmp(sel_path, prim_path, (size_t)n) == 0 && prim_path[n] == '/') return 1;
    return 0;
}

static int vp_better(const usd3d_variant_policy_selection *a, const usd3d_variant_policy_selection *b)
{
    long ar;
    long br;
    if (b == 0) return 1;
    ar = (long)usd3d_prim_arc_to_livrps_strength(a->arc);
    br = (long)usd3d_prim_arc_to_livrps_strength(b->arc);
    if (ar != br) return ar > br;
    if (a->layer_index != b->layer_index) return a->layer_index < b->layer_index;
    return a->node_index < b->node_index;
}

int usd3d_variant_policy_add_selection(usd3d_variant_policy *p,
                                       const char *prim_path,
                                       const char *set_name,
                                       const char *variant_name,
                                       const char *source_layer,
                                       int source,
                                       int arc,
                                       long layer_index,
                                       long node_index)
{
    usd3d_variant_policy_selection *s;
    if (p == 0 || prim_path == 0 || set_name == 0) return USD3D_ERR_BAD_ARG;
    if (p->count >= (unsigned long)USD3D_VARIANT_POLICY_MAX_SELECTIONS) return USD3D_ERR_LIMIT;
    s = &p->selections[p->count++];
    memset(s, 0, sizeof(*s));
    vp_copy(s->prim_path, sizeof(s->prim_path), prim_path);
    vp_copy(s->set_name, sizeof(s->set_name), set_name);
    vp_copy(s->variant_name, sizeof(s->variant_name), variant_name);
    vp_copy(s->source_layer, sizeof(s->source_layer), source_layer);
    s->source = source;
    s->arc = arc;
    s->layer_index = layer_index;
    s->node_index = node_index;
    s->active = 1;
    return USD3D_OK;
}

int usd3d_variant_policy_block_selection(usd3d_variant_policy *p,
                                         const char *prim_path,
                                         const char *set_name,
                                         const char *source_layer,
                                         int arc,
                                         long layer_index,
                                         long node_index)
{
    return usd3d_variant_policy_add_selection(p, prim_path, set_name, "", source_layer, USD3D_VARIANT_SEL_BLOCKED, arc, layer_index, node_index);
}

static void vp_skip_ws(const char **pp)
{
    while (pp != 0 && *pp != 0 && **pp != '\0' && isspace((unsigned char)**pp)) (*pp)++;
}

static int vp_read_ident(const char **pp, char *out, unsigned int cap)
{
    const char *p;
    unsigned int n;
    if (pp == 0 || *pp == 0 || out == 0 || cap == 0U) return 0;
    p = *pp;
    vp_skip_ws(&p);
    if (*p == '"') p++;
    n = 0U;
    while (*p != '\0') {
        if (*p == '"' || *p == '=' || *p == ',' || *p == '}' || isspace((unsigned char)*p)) break;
        if (n + 1U < cap) out[n++] = *p;
        p++;
    }
    if (*p == '"') p++;
    out[n] = '\0';
    *pp = p;
    return n > 0U;
}

int usd3d_variant_policy_parse_metadata(usd3d_variant_policy *p,
                                        const char *prim_path,
                                        const char *metadata_text,
                                        const char *source_layer,
                                        int arc,
                                        long layer_index,
                                        long node_index)
{
    const char *q;
    char set_name[USD3D_MAX_NAME];
    char variant_name[USD3D_MAX_NAME];
    int added;
    if (p == 0 || prim_path == 0 || metadata_text == 0) return USD3D_ERR_BAD_ARG;
    q = metadata_text;
    added = 0;
    while (*q != '\0') {
        vp_skip_ws(&q);
        if (*q == '{' || *q == ',' || *q == '(' || *q == ')') { q++; continue; }
        if (!vp_read_ident(&q, set_name, sizeof(set_name))) break;
        vp_skip_ws(&q);
        if (*q == '=') q++;
        vp_skip_ws(&q);
        if (!vp_read_ident(&q, variant_name, sizeof(variant_name))) break;
        if (strcmp(variant_name, "none") == 0 || strcmp(variant_name, "None") == 0 || strcmp(variant_name, "") == 0) {
            usd3d_variant_policy_block_selection(p, prim_path, set_name, source_layer, arc, layer_index, node_index);
        } else {
            usd3d_variant_policy_add_selection(p, prim_path, set_name, variant_name, source_layer, USD3D_VARIANT_SEL_EXPLICIT, arc, layer_index, node_index);
        }
        added++;
        while (*q != '\0' && *q != ',') q++;
        if (*q == ',') q++;
    }
    return added > 0 ? USD3D_OK : USD3D_ERR_PARSE;
}

int usd3d_variant_policy_resolve(const usd3d_variant_policy *p,
                                 const char *prim_path,
                                 const char *set_name,
                                 usd3d_variant_policy_selection *out_selection)
{
    unsigned long i;
    const usd3d_variant_policy_selection *best;
    const usd3d_variant_policy_selection *s;
    if (p == 0 || prim_path == 0 || set_name == 0 || out_selection == 0) return USD3D_ERR_BAD_ARG;
    best = 0;
    for (i = 0UL; i < p->count; ++i) {
        s = &p->selections[i];
        if (!s->active) continue;
        if (strcmp(s->set_name, set_name) != 0) continue;
        if (!vp_path_applies(s->prim_path, prim_path, p->inherit_to_children)) continue;
        if (vp_better(s, best)) best = s;
    }
    if (best == 0) return USD3D_ERR_PARSE;
    *out_selection = *best;
    if (strcmp(out_selection->prim_path, prim_path) != 0 && out_selection->source != USD3D_VARIANT_SEL_BLOCKED) out_selection->source = USD3D_VARIANT_SEL_INHERITED;
    return USD3D_OK;
}

int usd3d_variant_policy_should_parse_body(const usd3d_variant_policy *p,
                                           const char *prim_path,
                                           const char *set_name,
                                           const char *variant_name)
{
    usd3d_variant_policy_selection s;
    int r;
    if (p == 0 || prim_path == 0 || set_name == 0 || variant_name == 0) return 0;
    r = usd3d_variant_policy_resolve(p, prim_path, set_name, &s);
    if (r != USD3D_OK) return p->parse_unselected_when_no_selection;
    if (s.source == USD3D_VARIANT_SEL_BLOCKED) return 0;
    return strcmp(s.variant_name, variant_name) == 0;
}

int usd3d_variant_policy_apply_to_stack(const usd3d_variant_policy *p,
                                        usd3d_variant_stack *stack,
                                        const char *prim_path,
                                        const char *set_name,
                                        const char *variant_name)
{
    usd3d_variant_pruner pruner;
    usd3d_variant_policy_selection sel;
    if (p == 0 || stack == 0) return USD3D_ERR_BAD_ARG;
    usd3d_variant_pruner_init(&pruner);
    if (usd3d_variant_policy_resolve(p, prim_path, set_name, &sel) == USD3D_OK && sel.source != USD3D_VARIANT_SEL_BLOCKED) {
        usd3d_variant_pruner_select(&pruner, prim_path, set_name, sel.variant_name);
    }
    return usd3d_variant_stack_push(stack, &pruner, prim_path, set_name, variant_name);
}
