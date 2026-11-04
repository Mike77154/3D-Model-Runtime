#ifndef GOBJ_MTL_H
#define GOBJ_MTL_H

#include "gobj_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GOBJ_NAME_MAX
#define GOBJ_NAME_MAX 64
#endif

#ifndef GOBJ_PATH_MAX
#define GOBJ_PATH_MAX 128
#endif

enum GOBJ_Mtl_Result_Tag {
    GOBJ_MTL_OK = 0,
    GOBJ_MTL_ERR_NULL = -101,
    GOBJ_MTL_ERR_PARSE_NUMBER = -102,
    GOBJ_MTL_ERR_PARSE_INT = -103,
    GOBJ_MTL_ERR_MATERIAL_CAP = -104
};

typedef struct GOBJ_MtlMaterial_Tag {
    char name[GOBJ_NAME_MAX];
    GOBJ_Vec3 ka;
    GOBJ_Vec3 kd;
    GOBJ_Vec3 ks;
    GOBJ_Vec3 ke;
    GOBJ_Vec3 tf;
    GOBJ_Fixed ns;
    GOBJ_Fixed ni;
    GOBJ_Fixed d;
    GOBJ_Fixed tr;
    int illum;
    unsigned int flags;
    char map_ka[GOBJ_PATH_MAX];
    char map_kd[GOBJ_PATH_MAX];
    char map_ks[GOBJ_PATH_MAX];
    char map_ke[GOBJ_PATH_MAX];
    char map_d[GOBJ_PATH_MAX];
    char map_bump[GOBJ_PATH_MAX];
} GOBJ_MtlMaterial;

#define GOBJ_MTL_HAS_KA       0x00000001u
#define GOBJ_MTL_HAS_KD       0x00000002u
#define GOBJ_MTL_HAS_KS       0x00000004u
#define GOBJ_MTL_HAS_KE       0x00000008u
#define GOBJ_MTL_HAS_TF       0x00000010u
#define GOBJ_MTL_HAS_NS       0x00000020u
#define GOBJ_MTL_HAS_NI       0x00000040u
#define GOBJ_MTL_HAS_D        0x00000080u
#define GOBJ_MTL_HAS_TR       0x00000100u
#define GOBJ_MTL_HAS_ILLUM    0x00000200u
#define GOBJ_MTL_HAS_MAP_KA   0x00000400u
#define GOBJ_MTL_HAS_MAP_KD   0x00000800u
#define GOBJ_MTL_HAS_MAP_KS   0x00001000u
#define GOBJ_MTL_HAS_MAP_KE   0x00002000u
#define GOBJ_MTL_HAS_MAP_D    0x00004000u
#define GOBJ_MTL_HAS_MAP_BUMP 0x00008000u

typedef struct GOBJ_MtlReport_Tag {
    unsigned long line;
    unsigned long blank_lines;
    unsigned long comment_lines;
    unsigned long ignored_lines;
    unsigned long material_lines;
    unsigned long map_lines;
    int last_error;
} GOBJ_MtlReport;

typedef struct GOBJ_MtlLibrary_Tag {
    GOBJ_MtlMaterial *materials;
    unsigned int material_count;
    unsigned int material_cap;
} GOBJ_MtlLibrary;

void gobj_mtl_report_clear(GOBJ_MtlReport *report);
void gobj_mtl_bind(GOBJ_MtlLibrary *lib,
                   GOBJ_MtlMaterial *materials,
                   unsigned int material_cap);
void gobj_mtl_reset(GOBJ_MtlLibrary *lib);
int gobj_import_mtl_memory(GOBJ_MtlLibrary *lib,
                           const char *data,
                           unsigned long data_size,
                           GOBJ_MtlReport *report);
const GOBJ_MtlMaterial *gobj_mtl_find(const GOBJ_MtlLibrary *lib,
                                      const char *name);
const char *gobj_mtl_result_string(int code);

#ifdef __cplusplus
}
#endif

#endif
