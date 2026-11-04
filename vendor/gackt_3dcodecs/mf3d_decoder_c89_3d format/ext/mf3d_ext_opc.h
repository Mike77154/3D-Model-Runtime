#ifndef MF3D_EXT_OPC_H
#define MF3D_EXT_OPC_H

#include "../src/mf3d.h"

#ifndef MF3D_EXT_MAX_RELS
#define MF3D_EXT_MAX_RELS 128
#endif
#ifndef MF3D_EXT_MAX_CONTENT_TYPES
#define MF3D_EXT_MAX_CONTENT_TYPES 128
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_opc_rel_s {
    char id[64];
    char type[160];
    char target[160];
} mf3d_opc_rel;

typedef struct mf3d_opc_rels_s {
    mf3d_opc_rel rels[MF3D_EXT_MAX_RELS];
    mf3d_u32 count;
} mf3d_opc_rels;

typedef struct mf3d_opc_content_type_s {
    char part_name[160];
    char extension[24];
    char content_type[160];
} mf3d_opc_content_type;

typedef struct mf3d_opc_content_types_s {
    mf3d_opc_content_type items[MF3D_EXT_MAX_CONTENT_TYPES];
    mf3d_u32 count;
} mf3d_opc_content_types;

void mf3d_opc_rels_init(mf3d_opc_rels *r);
int mf3d_opc_rels_parse_xml(char *xml, mf3d_opc_rels *out_rels);
const char *mf3d_opc_find_start_part(const mf3d_opc_rels *r);

void mf3d_opc_content_types_init(mf3d_opc_content_types *ct);
int mf3d_opc_content_types_parse_xml(char *xml, mf3d_opc_content_types *out_ct);
const char *mf3d_opc_content_type_for_part(const mf3d_opc_content_types *ct, const char *part_name);

#ifdef __cplusplus
}
#endif
#endif
