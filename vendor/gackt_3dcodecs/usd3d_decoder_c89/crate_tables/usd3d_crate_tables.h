#ifndef USD3D_CRATE_TABLES_H
#define USD3D_CRATE_TABLES_H

#include "usd3d.h"

#ifndef USD3D_CRATE_TABLES_MAX_SECTIONS
#define USD3D_CRATE_TABLES_MAX_SECTIONS 64
#endif

#ifndef USD3D_CRATE_TABLES_MAX_STRINGS
#define USD3D_CRATE_TABLES_MAX_STRINGS 256
#endif

#ifndef USD3D_CRATE_TABLES_TEXT_MAX
#define USD3D_CRATE_TABLES_TEXT_MAX 128
#endif

#define USD3D_CRATE_ENDIAN_LITTLE 1
#define USD3D_CRATE_ENDIAN_UNKNOWN 0

typedef struct usd3d_crate_section_s {
    char name[USD3D_MAX_NAME];
    unsigned long offset;
    unsigned long size;
    int confidence;
} usd3d_crate_section;

typedef struct usd3d_crate_string_s {
    char text[USD3D_CRATE_TABLES_TEXT_MAX];
    unsigned long offset;
    unsigned long size;
    int looks_path;
    int looks_identifier;
} usd3d_crate_string;

typedef struct usd3d_crate_tables_s {
    int has_magic;
    int endian;
    unsigned long version_major;
    unsigned long version_minor;
    usd3d_crate_section sections[USD3D_CRATE_TABLES_MAX_SECTIONS];
    unsigned long section_count;
    usd3d_crate_string strings[USD3D_CRATE_TABLES_MAX_STRINGS];
    unsigned long string_count;
    unsigned long printable_bytes;
    unsigned long nul_terminated_strings;
} usd3d_crate_tables;

void usd3d_crate_tables_init(usd3d_crate_tables *tables);
int usd3d_crate_tables_scan_memory(const unsigned char *data, unsigned long size, usd3d_crate_tables *tables);
int usd3d_crate_tables_emit(const usd3d_crate_tables *tables,
                            int (*section_cb)(void *user, const usd3d_crate_section *section),
                            int (*string_cb)(void *user, const usd3d_crate_string *string_value),
                            void *user);
int usd3d_crate_tables_read_le32(const unsigned char *data, unsigned long size, unsigned long offset, unsigned long *out_value);
int usd3d_crate_tables_read_le64_pair(const unsigned char *data, unsigned long size, unsigned long offset, unsigned long *out_lo, unsigned long *out_hi);

#endif
