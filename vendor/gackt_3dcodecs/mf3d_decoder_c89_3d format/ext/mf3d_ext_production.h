#ifndef MF3D_EXT_PRODUCTION_H
#define MF3D_EXT_PRODUCTION_H

#include "../src/mf3d.h"

#ifndef MF3D_EXT_MAX_PROD_ITEMS
#define MF3D_EXT_MAX_PROD_ITEMS 256
#endif
#ifndef MF3D_EXT_MAX_PROD_OBJECTS
#define MF3D_EXT_MAX_PROD_OBJECTS 256
#endif
#ifndef MF3D_EXT_MAX_PROD_COMPONENTS
#define MF3D_EXT_MAX_PROD_COMPONENTS 512
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_prod_build_s {
    char uuid[48];
} mf3d_prod_build;

typedef struct mf3d_prod_item_s {
    mf3d_u32 objectid;
    char uuid[48];
    char path[160];
} mf3d_prod_item;

typedef struct mf3d_prod_object_s {
    mf3d_u32 objectid;
    char uuid[48];
    char modelresolution[24];
} mf3d_prod_object;

typedef struct mf3d_prod_component_s {
    mf3d_u32 parent_objectid;
    mf3d_u32 objectid;
    char uuid[48];
    char path[160];
} mf3d_prod_component;

typedef struct mf3d_production_s {
    mf3d_prod_build build;
    mf3d_prod_item items[MF3D_EXT_MAX_PROD_ITEMS];
    mf3d_prod_object objects[MF3D_EXT_MAX_PROD_OBJECTS];
    mf3d_prod_component components[MF3D_EXT_MAX_PROD_COMPONENTS];
    mf3d_u32 item_count, object_count, component_count;
} mf3d_production;

void mf3d_production_init(mf3d_production *p);
int mf3d_production_parse_xml(char *xml, mf3d_production *out_prod);
const char *mf3d_production_item_path(const mf3d_production *p, mf3d_u32 objectid);

#ifdef __cplusplus
}
#endif
#endif
