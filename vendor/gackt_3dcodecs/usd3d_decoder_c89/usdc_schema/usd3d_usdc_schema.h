#ifndef USD3D_USDC_SCHEMA_H
#define USD3D_USDC_SCHEMA_H

#include "usd3d.h"
#include "usd3d_usdc_abstract.h"

#ifndef USD3D_USDC_SCHEMA_MAX_TYPES
#define USD3D_USDC_SCHEMA_MAX_TYPES 256
#endif

#ifndef USD3D_USDC_SCHEMA_MAX_DIAGS
#define USD3D_USDC_SCHEMA_MAX_DIAGS 256
#endif

#define USD3D_USDC_SCHEMA_ROLE_UNKNOWN 0
#define USD3D_USDC_SCHEMA_ROLE_PRIM 1
#define USD3D_USDC_SCHEMA_ROLE_ATTRIBUTE 2
#define USD3D_USDC_SCHEMA_ROLE_RELATIONSHIP 3
#define USD3D_USDC_SCHEMA_ROLE_METADATA 4
#define USD3D_USDC_SCHEMA_ROLE_COMPOSITION 5

#define USD3D_USDC_SCHEMA_SEV_INFO 1
#define USD3D_USDC_SCHEMA_SEV_WARN 2
#define USD3D_USDC_SCHEMA_SEV_ERROR 3

#define USD3D_USDC_SCHEMA_HINT_UNKNOWN 0
#define USD3D_USDC_SCHEMA_HINT_TOKEN 1
#define USD3D_USDC_SCHEMA_HINT_STRING 2
#define USD3D_USDC_SCHEMA_HINT_ASSET 3
#define USD3D_USDC_SCHEMA_HINT_PATH 4
#define USD3D_USDC_SCHEMA_HINT_FIXED 5
#define USD3D_USDC_SCHEMA_HINT_VEC3 6
#define USD3D_USDC_SCHEMA_HINT_MATRIX4 7
#define USD3D_USDC_SCHEMA_HINT_INT_ARRAY 8
#define USD3D_USDC_SCHEMA_HINT_FLOAT_ARRAY 9

#ifndef USD3D_USDC_SCHEMA_USDA_MAX
#define USD3D_USDC_SCHEMA_USDA_MAX 16384UL
#endif

typedef struct usd3d_usdc_schema_type_s {
    char path[USD3D_PATH_MAX];
    char type_name[USD3D_TOKEN_MAX];
    char specifier[USD3D_MAX_NAME];
    int role;
    int active;
} usd3d_usdc_schema_type;

typedef struct usd3d_usdc_schema_diag_s {
    char path[USD3D_PATH_MAX];
    char field[USD3D_TOKEN_MAX];
    char message[USD3D_TOKEN_MAX];
    int severity;
    int active;
} usd3d_usdc_schema_diag;

typedef struct usd3d_usdc_schema_s {
    usd3d_usdc_schema_type types[USD3D_USDC_SCHEMA_MAX_TYPES];
    usd3d_usdc_schema_diag diags[USD3D_USDC_SCHEMA_MAX_DIAGS];
    unsigned long type_count;
    unsigned long diag_count;
    unsigned long attr_count;
    unsigned long rel_count;
    unsigned long metadata_count;
    unsigned long composition_count;
} usd3d_usdc_schema;

void usd3d_usdc_schema_init(usd3d_usdc_schema *s);
int usd3d_usdc_schema_import_abstract(usd3d_usdc_schema *s, const usd3d_usdc_abstract_data *a);
int usd3d_usdc_schema_add_type(usd3d_usdc_schema *s, const char *path, const char *specifier, const char *type_name, int role);
int usd3d_usdc_schema_add_diag(usd3d_usdc_schema *s, const char *path, const char *field, const char *message, int severity);
int usd3d_usdc_schema_classify_field(const usd3d_usdc_abs_field *f, int *out_role, int *out_hint);
int usd3d_usdc_schema_validate(const usd3d_usdc_schema *s, const usd3d_usdc_abstract_data *a, usd3d_usdc_schema *out_diags);
int usd3d_usdc_schema_emit_usda(const usd3d_usdc_schema *s, const usd3d_usdc_abstract_data *a, char *out_text, unsigned long out_cap);
int usd3d_usdc_schema_emit(const usd3d_usdc_schema *s,
                           int (*type_cb)(void *user, const usd3d_usdc_schema_type *t),
                           int (*diag_cb)(void *user, const usd3d_usdc_schema_diag *d),
                           void *user);
const char *usd3d_usdc_schema_role_name(int role);
const char *usd3d_usdc_schema_hint_name(int hint);
const char *usd3d_usdc_schema_severity_name(int sev);

#endif
