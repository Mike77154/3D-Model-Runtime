#ifndef USD3D_VARIANT_RESOLVER_H
#define USD3D_VARIANT_RESOLVER_H

#include "usd3d.h"
#include "usd3d_variant_policy.h"
#include "usd3d_composition_runtime.h"

#ifndef USD3D_VARIANT_RESOLVER_MAX_SETS
#define USD3D_VARIANT_RESOLVER_MAX_SETS 256
#endif

#ifndef USD3D_VARIANT_RESOLVER_MAX_CHOICES
#define USD3D_VARIANT_RESOLVER_MAX_CHOICES 512
#endif

#define USD3D_VARIANT_SOURCE_NONE 0
#define USD3D_VARIANT_SOURCE_EXPLICIT 1
#define USD3D_VARIANT_SOURCE_INHERITED 2
#define USD3D_VARIANT_SOURCE_DEFAULT 3
#define USD3D_VARIANT_SOURCE_BLOCKED 4

typedef struct usd3d_variant_resolver_choice_s {
    char prim_path[USD3D_PATH_MAX];
    char set_name[USD3D_TOKEN_MAX];
    char variant_name[USD3D_TOKEN_MAX];
    int source;
    int active;
} usd3d_variant_resolver_choice;

typedef struct usd3d_variant_set_record_s {
    char prim_path[USD3D_PATH_MAX];
    char set_name[USD3D_TOKEN_MAX];
    char default_variant[USD3D_TOKEN_MAX];
    int blocked;
    int active;
} usd3d_variant_set_record;

typedef struct usd3d_variant_resolver_s {
    usd3d_variant_set_record sets[USD3D_VARIANT_RESOLVER_MAX_SETS];
    usd3d_variant_resolver_choice choices[USD3D_VARIANT_RESOLVER_MAX_CHOICES];
    unsigned long set_count;
    unsigned long choice_count;
    unsigned long inherited_hits;
    unsigned long default_hits;
    unsigned long blocked_hits;
} usd3d_variant_resolver;

void usd3d_variant_resolver_init(usd3d_variant_resolver *vr);
int usd3d_variant_resolver_define_set(usd3d_variant_resolver *vr, const char *prim_path, const char *set_name, const char *default_variant);
int usd3d_variant_resolver_select(usd3d_variant_resolver *vr, const char *prim_path, const char *set_name, const char *variant_name, int source);
int usd3d_variant_resolver_block(usd3d_variant_resolver *vr, const char *prim_path, const char *set_name);
int usd3d_variant_resolver_resolve(const usd3d_variant_resolver *vr, const char *prim_path, const char *set_name, char *out_variant, unsigned long out_cap, int *out_source);
int usd3d_variant_resolver_should_parse(const usd3d_variant_resolver *vr, const char *prim_path, const char *set_name, const char *variant_name);
int usd3d_variant_resolver_import_policy(usd3d_variant_resolver *vr, const usd3d_variant_policy *policy);
int usd3d_variant_resolver_import_runtime(usd3d_variant_resolver *vr, usd3d_comp_runtime *cr, const char *prim_path, const char *set_name);
const char *usd3d_variant_resolver_source_name(int source);

#endif
