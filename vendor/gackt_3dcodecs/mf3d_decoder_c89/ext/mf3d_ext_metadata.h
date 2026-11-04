#ifndef MF3D_EXT_METADATA_H
#define MF3D_EXT_METADATA_H

#include "../src/mf3d.h"

#ifndef MF3D_EXT_MAX_METADATA
#define MF3D_EXT_MAX_METADATA 128
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_metadata_item_s {
    char name[64];
    char value[192];
} mf3d_metadata_item;

typedef struct mf3d_metadata_s {
    mf3d_metadata_item items[MF3D_EXT_MAX_METADATA];
    mf3d_u32 count;
} mf3d_metadata;

void mf3d_metadata_init(mf3d_metadata *m);
int mf3d_metadata_parse_xml(char *xml, mf3d_metadata *out_meta);
const char *mf3d_metadata_find(const mf3d_metadata *m, const char *name);

#ifdef __cplusplus
}
#endif
#endif
