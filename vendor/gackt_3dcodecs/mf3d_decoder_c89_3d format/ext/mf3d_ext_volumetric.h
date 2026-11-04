#ifndef MF3D_EXT_VOLUMETRIC_H
#define MF3D_EXT_VOLUMETRIC_H

#include "../src/mf3d.h"

#ifndef MF3D_EXT_MAX_VOL_FUNCTIONS
#define MF3D_EXT_MAX_VOL_FUNCTIONS 128
#endif
#ifndef MF3D_EXT_MAX_IMAGE3D
#define MF3D_EXT_MAX_IMAGE3D 128
#endif
#ifndef MF3D_EXT_MAX_IMAGESHEETS
#define MF3D_EXT_MAX_IMAGESHEETS 4096
#endif
#ifndef MF3D_EXT_MAX_LEVELSETS
#define MF3D_EXT_MAX_LEVELSETS 128
#endif
#ifndef MF3D_EXT_MAX_VOLDATA
#define MF3D_EXT_MAX_VOLDATA 256
#endif
#ifndef MF3D_EXT_MAX_IMPLICIT_NODES
#define MF3D_EXT_MAX_IMPLICIT_NODES 2048
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_vol_function_s {
    mf3d_u32 id;
    mf3d_u32 image3did;
    char kind[32];
    char displayname[64];
    char filter[24];
    char tilestyleu[24], tilestylev[24], tilestylew[24];
    mf3d_fix valueoffset;
    mf3d_fix valuescale;
} mf3d_vol_function;

typedef struct mf3d_image3d_s {
    mf3d_u32 id;
    char name[64];
    mf3d_u32 rowcount, columncount, sheetcount;
    mf3d_u32 first_sheet, sheet_path_count;
} mf3d_image3d;

typedef struct mf3d_imagesheet_s { char path[160]; } mf3d_imagesheet;

typedef struct mf3d_levelset_s {
    mf3d_u32 owner_objectid;
    mf3d_u32 functionid;
    mf3d_u32 meshid;
    mf3d_u32 meshbboxonly;
    char channel[64];
    mf3d_fix transform[12];
    mf3d_fix minfeaturesize;
} mf3d_levelset;

typedef struct mf3d_volumetricdata_s {
    mf3d_u32 objectid;
    mf3d_u32 functionid;
    mf3d_u32 propertyid;
    char channel[64];
    char datatype[32];
    mf3d_fix transform[12];
} mf3d_volumetricdata;

typedef struct mf3d_implicit_node_s {
    mf3d_u32 functionid;
    mf3d_u32 node_index;
    char tag[40];
    char id[48];
    char op[48];
    char output[48];
    char input[48];
} mf3d_implicit_node;

typedef struct mf3d_volumetric_s {
    mf3d_vol_function functions[MF3D_EXT_MAX_VOL_FUNCTIONS];
    mf3d_image3d images[MF3D_EXT_MAX_IMAGE3D];
    mf3d_imagesheet sheets[MF3D_EXT_MAX_IMAGESHEETS];
    mf3d_levelset levelsets[MF3D_EXT_MAX_LEVELSETS];
    mf3d_volumetricdata voldata[MF3D_EXT_MAX_VOLDATA];
    mf3d_implicit_node nodes[MF3D_EXT_MAX_IMPLICIT_NODES];
    mf3d_u32 function_count, image_count, sheet_count;
    mf3d_u32 levelset_count, voldata_count, node_count;
} mf3d_volumetric;

void mf3d_volumetric_init(mf3d_volumetric *v);
int mf3d_volumetric_parse_xml(char *xml, mf3d_volumetric *out_v);
const mf3d_vol_function *mf3d_volumetric_find_function(const mf3d_volumetric *v, mf3d_u32 id);
const mf3d_image3d *mf3d_volumetric_find_image3d(const mf3d_volumetric *v, mf3d_u32 id);
int mf3d_volumetric_validate_refs(const mf3d_volumetric *v, const mf3d_model *m, mf3d_u32 *missing_refs);

#ifdef __cplusplus
}
#endif
#endif
