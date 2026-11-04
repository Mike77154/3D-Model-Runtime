#include "usd3d_composition_runtime.h"
#include <string.h>

static void cr_copy(char *dst, unsigned long cap, const char *src)
{
    unsigned long i;
    if (!dst || cap == 0UL) return;
    if (!src) src = "";
    i = 0UL;
    while (src[i] && i + 1UL < cap) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int cr_prefix(const char *path, const char *prefix)
{
    unsigned long i;
    if (!path || !prefix) return 0;
    i = 0UL;
    while (prefix[i]) {
        if (path[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

void usd3d_comp_runtime_init(usd3d_comp_runtime *cr)
{
    if (!cr) return;
    memset(cr, 0, sizeof(*cr));
    usd3d_prim_index_init(&cr->prim_index);
    usd3d_variant_policy_init(&cr->variants);
    usd3d_listop_init(&cr->listops);
    cr->default_load_payloads = 1;
}

const char *usd3d_comp_load_rule_name(int rule)
{
    if (rule == USD3D_COMP_LOAD_EXPLICIT) return "explicit";
    if (rule == USD3D_COMP_LOAD_PAYLOADS) return "payloads";
    if (rule == USD3D_COMP_LOAD_NONE) return "none";
    return "unknown";
}

int usd3d_comp_add_relocate(usd3d_comp_runtime *cr, const char *from_path, const char *to_path)
{
    usd3d_comp_relocate *r;
    if (!cr || !from_path || !to_path) return USD3D_ERR_BAD_ARG;
    if (cr->relocate_count >= USD3D_COMP_MAX_RELOCATES) return USD3D_ERR_LIMIT;
    r = &cr->relocates[cr->relocate_count];
    memset(r, 0, sizeof(*r));
    cr_copy(r->from_path, USD3D_PATH_MAX, from_path);
    cr_copy(r->to_path, USD3D_PATH_MAX, to_path);
    r->active = 1;
    cr->relocate_count++;
    return USD3D_OK;
}

int usd3d_comp_add_load_rule(usd3d_comp_runtime *cr, const char *path, int rule)
{
    usd3d_comp_load_rule *lr;
    if (!cr || !path) return USD3D_ERR_BAD_ARG;
    if (cr->load_rule_count >= USD3D_COMP_MAX_LOAD_RULES) return USD3D_ERR_LIMIT;
    lr = &cr->load_rules[cr->load_rule_count];
    memset(lr, 0, sizeof(*lr));
    cr_copy(lr->path, USD3D_PATH_MAX, path);
    lr->rule = rule;
    lr->active = 1;
    cr->load_rule_count++;
    return USD3D_OK;
}

int usd3d_comp_should_load_payload(const usd3d_comp_runtime *cr, const char *path)
{
    long i;
    if (!cr || !path) return 0;
    i = (long)cr->load_rule_count - 1L;
    while (i >= 0L) {
        if (cr->load_rules[i].active && cr_prefix(path, cr->load_rules[i].path)) {
            return cr->load_rules[i].rule != USD3D_COMP_LOAD_NONE;
        }
        i--;
    }
    return cr->default_load_payloads ? 1 : 0;
}

int usd3d_comp_apply_relocates(const usd3d_comp_runtime *cr, const char *in_path, char *out_path, unsigned long out_cap)
{
    unsigned long i;
    unsigned long from_len;
    if (!cr || !in_path || !out_path || out_cap == 0UL) return USD3D_ERR_BAD_ARG;
    cr_copy(out_path, out_cap, in_path);
    for (i = 0UL; i < cr->relocate_count; i++) {
        if (cr->relocates[i].active && cr_prefix(out_path, cr->relocates[i].from_path)) {
            char tmp[USD3D_PATH_MAX];
            from_len = (unsigned long)strlen(cr->relocates[i].from_path);
            cr_copy(tmp, USD3D_PATH_MAX, cr->relocates[i].to_path);
            if (strlen(tmp) + strlen(out_path + from_len) + 1UL < USD3D_PATH_MAX) {
                strcat(tmp, out_path + from_len);
                cr_copy(out_path, out_cap, tmp);
            }
        }
    }
    return USD3D_OK;
}

int usd3d_comp_add_opinion(usd3d_comp_runtime *cr, const char *prim_path, const char *attr_name, const char *value, const char *layer, int arc, long layer_index, long node_index)
{
    char relocated[USD3D_PATH_MAX];
    if (!cr) return USD3D_ERR_BAD_ARG;
    (void)usd3d_comp_apply_relocates(cr, prim_path, relocated, USD3D_PATH_MAX);
    return usd3d_prim_index_add_opinion(&cr->prim_index, relocated, attr_name, value, layer, arc, layer_index, node_index);
}

int usd3d_comp_resolve_attr(usd3d_comp_runtime *cr, const char *prim_path, const char *attr_name, char *out_value, unsigned long out_cap)
{
    usd3d_prim_index_opinion op;
    char relocated[USD3D_PATH_MAX];
    int r;
    if (!cr || !prim_path || !attr_name || !out_value) return USD3D_ERR_BAD_ARG;
    (void)usd3d_comp_apply_relocates(cr, prim_path, relocated, USD3D_PATH_MAX);
    (void)usd3d_prim_index_sort(&cr->prim_index);
    r = usd3d_prim_index_resolve_attr(&cr->prim_index, relocated, attr_name, &op);
    if (r != USD3D_OK) return r;
    cr_copy(out_value, out_cap, op.value_text);
    return USD3D_OK;
}

int usd3d_comp_resolve_variant(usd3d_comp_runtime *cr, const char *prim_path, const char *set_name, char *out_variant, unsigned long out_cap)
{
    usd3d_variant_policy_selection sel;
    int r;
    if (!cr || !prim_path || !set_name || !out_variant) return USD3D_ERR_BAD_ARG;
    r = usd3d_variant_policy_resolve(&cr->variants, prim_path, set_name, &sel);
    if (r != USD3D_OK) return r;
    cr_copy(out_variant, out_cap, sel.variant_name);
    return USD3D_OK;
}

int usd3d_comp_build_flatten_trace(usd3d_comp_runtime *cr, const char *prim_path, int (*trace_cb)(void *user, const usd3d_prim_index_opinion *op), void *user)
{
    unsigned long i;
    int r;
    if (!cr || !prim_path) return USD3D_ERR_BAD_ARG;
    (void)usd3d_prim_index_sort(&cr->prim_index);
    for (i = 0UL; i < cr->prim_index.opinion_count; i++) {
        if (cr->prim_index.opinions[i].active && strcmp(cr->prim_index.opinions[i].prim_path, prim_path) == 0) {
            if (trace_cb) {
                r = trace_cb(user, &cr->prim_index.opinions[i]);
                if (r != 0) return r;
            }
        }
    }
    return USD3D_OK;
}
