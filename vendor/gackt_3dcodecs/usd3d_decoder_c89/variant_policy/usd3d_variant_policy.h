#ifndef USD3D_VARIANT_POLICY_H
#define USD3D_VARIANT_POLICY_H

#include "usd3d.h"
#include "usd3d_variant_stack.h"
#include "usd3d_prim_index.h"

#ifndef USD3D_VARIANT_POLICY_MAX_SELECTIONS
#define USD3D_VARIANT_POLICY_MAX_SELECTIONS 128
#endif

#define USD3D_VARIANT_SEL_EXPLICIT 1
#define USD3D_VARIANT_SEL_INHERITED 2
#define USD3D_VARIANT_SEL_DEFAULT 3
#define USD3D_VARIANT_SEL_BLOCKED 4

typedef struct usd3d_variant_policy_selection_s {
    char prim_path[USD3D_PATH_MAX];
    char set_name[USD3D_MAX_NAME];
    char variant_name[USD3D_MAX_NAME];
    char source_layer[USD3D_PATH_MAX];
    int source;
    int arc;
    long layer_index;
    long node_index;
    int active;
} usd3d_variant_policy_selection;

typedef struct usd3d_variant_policy_s {
    usd3d_variant_policy_selection selections[USD3D_VARIANT_POLICY_MAX_SELECTIONS];
    unsigned long count;
    int inherit_to_children;
    int parse_unselected_when_no_selection;
} usd3d_variant_policy;

void usd3d_variant_policy_init(usd3d_variant_policy *p);
const char *usd3d_variant_selection_source_name(int source);
int usd3d_variant_policy_add_selection(usd3d_variant_policy *p,
                                       const char *prim_path,
                                       const char *set_name,
                                       const char *variant_name,
                                       const char *source_layer,
                                       int source,
                                       int arc,
                                       long layer_index,
                                       long node_index);
int usd3d_variant_policy_block_selection(usd3d_variant_policy *p,
                                         const char *prim_path,
                                         const char *set_name,
                                         const char *source_layer,
                                         int arc,
                                         long layer_index,
                                         long node_index);
int usd3d_variant_policy_parse_metadata(usd3d_variant_policy *p,
                                        const char *prim_path,
                                        const char *metadata_text,
                                        const char *source_layer,
                                        int arc,
                                        long layer_index,
                                        long node_index);
int usd3d_variant_policy_resolve(const usd3d_variant_policy *p,
                                 const char *prim_path,
                                 const char *set_name,
                                 usd3d_variant_policy_selection *out_selection);
int usd3d_variant_policy_should_parse_body(const usd3d_variant_policy *p,
                                           const char *prim_path,
                                           const char *set_name,
                                           const char *variant_name);
int usd3d_variant_policy_apply_to_stack(const usd3d_variant_policy *p,
                                        usd3d_variant_stack *stack,
                                        const char *prim_path,
                                        const char *set_name,
                                        const char *variant_name);

#endif
