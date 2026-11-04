#ifndef USD3D_USDC_ABSTRACT_H
#define USD3D_USDC_ABSTRACT_H

#include "usd3d.h"
#include "usd3d_usdc_deser.h"

#ifndef USD3D_USDC_ABS_MAX_SPECS
#define USD3D_USDC_ABS_MAX_SPECS 512
#endif

#ifndef USD3D_USDC_ABS_MAX_FIELDS
#define USD3D_USDC_ABS_MAX_FIELDS 1024
#endif

#ifndef USD3D_USDC_ABS_USDA_MAX
#define USD3D_USDC_ABS_USDA_MAX 8192
#endif

#define USD3D_USDC_SPEC_UNKNOWN 0
#define USD3D_USDC_SPEC_DEF 1
#define USD3D_USDC_SPEC_OVER 2
#define USD3D_USDC_SPEC_CLASS 3
#define USD3D_USDC_SPEC_ATTR 4
#define USD3D_USDC_SPEC_REL 5

#define USD3D_USDC_ABS_STATUS_EMPTY 0
#define USD3D_USDC_ABS_STATUS_PARTIAL 1
#define USD3D_USDC_ABS_STATUS_SCHEMA_LITE 2

typedef struct usd3d_usdc_abs_field_s {
    char path[USD3D_PATH_MAX];
    char name[USD3D_TOKEN_MAX];
    char value[USD3D_USDC_DESER_TEXT_MAX];
    char source_table[USD3D_MAX_NAME];
    int kind;
    int active;
} usd3d_usdc_abs_field;

typedef struct usd3d_usdc_abs_spec_s {
    char path[USD3D_PATH_MAX];
    char type_name[USD3D_TOKEN_MAX];
    char specifier[USD3D_MAX_NAME];
    unsigned long field_begin;
    unsigned long field_count;
    int spec_kind;
    int active;
} usd3d_usdc_abs_spec;

typedef struct usd3d_usdc_abstract_data_s {
    usd3d_usdc_abs_spec specs[USD3D_USDC_ABS_MAX_SPECS];
    usd3d_usdc_abs_field fields[USD3D_USDC_ABS_MAX_FIELDS];
    unsigned long spec_count;
    unsigned long field_count;
    unsigned long attr_count;
    unsigned long rel_count;
    unsigned long metadata_count;
    int status;
} usd3d_usdc_abstract_data;

void usd3d_usdc_abstract_init(usd3d_usdc_abstract_data *a);
int usd3d_usdc_abstract_import_deser(usd3d_usdc_abstract_data *a, const usd3d_usdc_deser *d);
int usd3d_usdc_abstract_add_spec(usd3d_usdc_abstract_data *a, const char *path, const char *specifier, const char *type_name, int spec_kind);
int usd3d_usdc_abstract_add_field(usd3d_usdc_abstract_data *a, const char *path, const char *name, const char *value, const char *source_table, int kind);
int usd3d_usdc_abstract_resolve_field(const usd3d_usdc_abstract_data *a, const char *path, const char *name, usd3d_usdc_abs_field *out_field);
int usd3d_usdc_abstract_emit_usda_lite(const usd3d_usdc_abstract_data *a, char *out_text, unsigned long out_cap);
int usd3d_usdc_abstract_emit(const usd3d_usdc_abstract_data *a,
                             int (*spec_cb)(void *user, const usd3d_usdc_abs_spec *s),
                             int (*field_cb)(void *user, const usd3d_usdc_abs_field *f),
                             void *user);
const char *usd3d_usdc_abs_spec_kind_name(int kind);

#endif
