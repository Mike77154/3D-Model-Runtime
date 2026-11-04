#ifndef USD3D_VARIANT_PRUNE_H
#define USD3D_VARIANT_PRUNE_H

#include "usd3d.h"

#ifndef USD3D_VARIANT_PRUNE_MAX
#define USD3D_VARIANT_PRUNE_MAX 64
#endif

typedef struct usd3d_variant_choice_s {
    char prim_path[USD3D_PATH_MAX];
    char set_name[USD3D_MAX_NAME];
    char variant_name[USD3D_MAX_NAME];
    int active;
} usd3d_variant_choice;

typedef struct usd3d_variant_pruner_s {
    usd3d_variant_choice choices[USD3D_VARIANT_PRUNE_MAX];
    unsigned long count;
    int prune_unselected;
} usd3d_variant_pruner;

void usd3d_variant_pruner_init(usd3d_variant_pruner *p);
void usd3d_variant_pruner_set_prune(usd3d_variant_pruner *p, int enabled);
int usd3d_variant_pruner_select(usd3d_variant_pruner *p, const char *prim_path, const char *set_name, const char *variant_name);
int usd3d_variant_pruner_should_parse(const usd3d_variant_pruner *p, const char *prim_path, const char *set_name, const char *variant_name);
const char *usd3d_variant_pruner_selected(const usd3d_variant_pruner *p, const char *prim_path, const char *set_name);

#endif
