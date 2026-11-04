#ifndef USD3D_COMPOSITION_RUNTIME_H
#define USD3D_COMPOSITION_RUNTIME_H

#include "usd3d.h"
#include "usd3d_prim_index.h"
#include "usd3d_variant_policy.h"
#include "usd3d_value_resolution.h"

#ifndef USD3D_COMP_MAX_RELOCATES
#define USD3D_COMP_MAX_RELOCATES 128
#endif

#ifndef USD3D_COMP_MAX_LOAD_RULES
#define USD3D_COMP_MAX_LOAD_RULES 128
#endif

#define USD3D_COMP_LOAD_EXPLICIT 1
#define USD3D_COMP_LOAD_PAYLOADS 2
#define USD3D_COMP_LOAD_NONE 3

typedef struct usd3d_comp_relocate_s {
    char from_path[USD3D_PATH_MAX];
    char to_path[USD3D_PATH_MAX];
    int active;
} usd3d_comp_relocate;

typedef struct usd3d_comp_load_rule_s {
    char path[USD3D_PATH_MAX];
    int rule;
    int active;
} usd3d_comp_load_rule;

typedef struct usd3d_comp_runtime_s {
    usd3d_prim_index prim_index;
    usd3d_variant_policy variants;
    usd3d_listop_state listops;
    usd3d_comp_relocate relocates[USD3D_COMP_MAX_RELOCATES];
    usd3d_comp_load_rule load_rules[USD3D_COMP_MAX_LOAD_RULES];
    unsigned long relocate_count;
    unsigned long load_rule_count;
    int default_load_payloads;
} usd3d_comp_runtime;

void usd3d_comp_runtime_init(usd3d_comp_runtime *cr);
int usd3d_comp_add_relocate(usd3d_comp_runtime *cr, const char *from_path, const char *to_path);
int usd3d_comp_add_load_rule(usd3d_comp_runtime *cr, const char *path, int rule);
int usd3d_comp_should_load_payload(const usd3d_comp_runtime *cr, const char *path);
int usd3d_comp_apply_relocates(const usd3d_comp_runtime *cr, const char *in_path, char *out_path, unsigned long out_cap);
int usd3d_comp_add_opinion(usd3d_comp_runtime *cr, const char *prim_path, const char *attr_name, const char *value, const char *layer, int arc, long layer_index, long node_index);
int usd3d_comp_resolve_attr(usd3d_comp_runtime *cr, const char *prim_path, const char *attr_name, char *out_value, unsigned long out_cap);
int usd3d_comp_resolve_variant(usd3d_comp_runtime *cr, const char *prim_path, const char *set_name, char *out_variant, unsigned long out_cap);
int usd3d_comp_build_flatten_trace(usd3d_comp_runtime *cr, const char *prim_path, int (*trace_cb)(void *user, const usd3d_prim_index_opinion *op), void *user);
const char *usd3d_comp_load_rule_name(int rule);

#endif
