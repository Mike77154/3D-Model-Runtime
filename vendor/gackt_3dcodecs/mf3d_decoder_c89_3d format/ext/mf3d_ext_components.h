#ifndef MF3D_EXT_COMPONENTS_H
#define MF3D_EXT_COMPONENTS_H

#include "../src/mf3d.h"

#ifndef MF3D_EXT_MAX_COMPONENTS
#define MF3D_EXT_MAX_COMPONENTS 512
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_component_s {
    mf3d_u32 parent_objectid;
    mf3d_u32 objectid;
    mf3d_fix transform[12];
} mf3d_component;

typedef struct mf3d_components_s {
    mf3d_component components[MF3D_EXT_MAX_COMPONENTS];
    mf3d_u32 count;
} mf3d_components;

void mf3d_components_init(mf3d_components *c);
int mf3d_components_parse_xml(char *xml, mf3d_components *out_components);
int mf3d_components_flatten(const mf3d_model *src_model, const mf3d_components *components, mf3d_model *out_model);

#ifdef __cplusplus
}
#endif
#endif
