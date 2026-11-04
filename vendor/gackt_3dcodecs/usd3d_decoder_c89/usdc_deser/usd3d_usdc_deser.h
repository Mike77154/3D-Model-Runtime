#ifndef USD3D_USDC_DESER_H
#define USD3D_USDC_DESER_H

#include "usd3d.h"

#ifndef USD3D_USDC_DESER_MAX_VALUES
#define USD3D_USDC_DESER_MAX_VALUES 768
#endif

#ifndef USD3D_USDC_DESER_MAX_FIELDS
#define USD3D_USDC_DESER_MAX_FIELDS 384
#endif

#ifndef USD3D_USDC_DESER_TEXT_MAX
#define USD3D_USDC_DESER_TEXT_MAX 160
#endif

#define USD3D_USDC_VALUE_UNKNOWN 0
#define USD3D_USDC_VALUE_TOKEN 1
#define USD3D_USDC_VALUE_STRING 2
#define USD3D_USDC_VALUE_PATH 3
#define USD3D_USDC_VALUE_ASSET 4
#define USD3D_USDC_VALUE_INT 5
#define USD3D_USDC_VALUE_UINT 6
#define USD3D_USDC_VALUE_FIXED 7
#define USD3D_USDC_VALUE_BOOL 8
#define USD3D_USDC_VALUE_SPECIFIER 9
#define USD3D_USDC_VALUE_TYPENAME 10

#define USD3D_USDC_FIELD_UNKNOWN 0
#define USD3D_USDC_FIELD_ATTR 1
#define USD3D_USDC_FIELD_REL 2
#define USD3D_USDC_FIELD_METADATA 3
#define USD3D_USDC_FIELD_ARC 4

#define USD3D_USDC_STATUS_PARTIAL 1
#define USD3D_USDC_STATUS_COMPLETE_LITE 2

/*
   USDC/Crate deserialization helper.

   This module intentionally avoids heap ownership and exposes a finite table model.
   It can deserialize scalar atoms, string/token/path/asset-like atoms, and field-ish
   key/value pairs discovered in Crate streams or in fixture Crate-lite streams. It is
   NOT a byte-for-byte replacement for Pixar's Sdf_CrateData internals; the status
   field tells callers how complete the decode was.
*/
typedef struct usd3d_usdc_value_s {
    int type;
    unsigned long index;
    unsigned long offset;
    unsigned long size;
    long i32_value;
    unsigned long u32_value;
    usd3d_fixed fixed_value;
    char text[USD3D_USDC_DESER_TEXT_MAX];
} usd3d_usdc_value;

typedef struct usd3d_usdc_field_s {
    int kind;
    unsigned long index;
    unsigned long path_value;
    unsigned long name_value;
    unsigned long value_index;
    char path[USD3D_PATH_MAX];
    char name[USD3D_TOKEN_MAX];
    char value_text[USD3D_USDC_DESER_TEXT_MAX];
    char source_table[USD3D_MAX_NAME];
} usd3d_usdc_field;

typedef struct usd3d_usdc_deser_s {
    int has_magic;
    int status;
    unsigned long version_major;
    unsigned long version_minor;
    unsigned long value_count;
    unsigned long field_count;
    unsigned long numeric_count;
    unsigned long path_count;
    unsigned long asset_count;
    unsigned long field_like_count;
    unsigned long unsupported_bytes;
    usd3d_usdc_value values[USD3D_USDC_DESER_MAX_VALUES];
    usd3d_usdc_field fields[USD3D_USDC_DESER_MAX_FIELDS];
} usd3d_usdc_deser;

void usd3d_usdc_deser_init(usd3d_usdc_deser *d);
int usd3d_usdc_deser_scan_memory(const unsigned char *data, unsigned long size, usd3d_usdc_deser *d);
int usd3d_usdc_deser_add_value(usd3d_usdc_deser *d, int type, const char *text, unsigned long offset, unsigned long size);
int usd3d_usdc_deser_add_field(usd3d_usdc_deser *d, int kind, const char *path, const char *name, const char *value_text, const char *source_table);
int usd3d_usdc_deser_emit(const usd3d_usdc_deser *d,
                          int (*value_cb)(void *user, const usd3d_usdc_value *v),
                          int (*field_cb)(void *user, const usd3d_usdc_field *f),
                          void *user);
int usd3d_usdc_deser_read_atom_le(const unsigned char *data, unsigned long size, unsigned long offset, int type, usd3d_usdc_value *out_value);
const char *usd3d_usdc_value_type_name(int type);
const char *usd3d_usdc_field_kind_name(int kind);

#endif
