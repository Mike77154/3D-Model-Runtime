#ifndef USD3D_VARIANT_STACK_H
#define USD3D_VARIANT_STACK_H

#include "usd3d.h"
#include "usd3d_variant_prune.h"

#ifndef USD3D_VARIANT_STACK_MAX_DEPTH
#define USD3D_VARIANT_STACK_MAX_DEPTH 32
#endif

typedef struct usd3d_variant_stack_frame_s {
    char prim_path[USD3D_PATH_MAX];
    char set_name[USD3D_MAX_NAME];
    char variant_name[USD3D_MAX_NAME];
    int selected;
    int parse_enabled;
} usd3d_variant_stack_frame;

typedef struct usd3d_variant_stack_s {
    usd3d_variant_stack_frame frames[USD3D_VARIANT_STACK_MAX_DEPTH];
    unsigned long depth;
} usd3d_variant_stack;

void usd3d_variant_stack_init(usd3d_variant_stack *s);
int usd3d_variant_stack_push(usd3d_variant_stack *s,
                             const usd3d_variant_pruner *p,
                             const char *prim_path,
                             const char *set_name,
                             const char *variant_name);
int usd3d_variant_stack_pop(usd3d_variant_stack *s);
int usd3d_variant_stack_should_parse(const usd3d_variant_stack *s);
int usd3d_variant_parse_selection_text(usd3d_variant_pruner *p, const char *prim_path, const char *text);

#endif
