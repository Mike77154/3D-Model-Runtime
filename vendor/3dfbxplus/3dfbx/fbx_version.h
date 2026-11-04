#ifndef FBX_VERSION_H
#define FBX_VERSION_H

#include "fbx_importer.h" /* FbxContext/FbxNode + integer types */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FbxVersionInfo {
    fbx_i32 version_raw;  /* ej. 6000, 7000, 7100, 7400, 7500... */
    fbx_i32 major;
    fbx_i32 minor;
    fbx_i32 revision;
    fbx_i32 year_approx;  /* año aproximado */
} FbxVersionInfo;

/* Import path selected by version + structural scan.
   This intentionally lives in versions so adapters are gated by file symptoms,
   not by exporter names such as FragMotion/Noesis. */
typedef enum FbxImportPath {
    FBX_IMPORT_PATH_MODERN = 0,
    FBX_IMPORT_PATH_LEGACY6000_EMBEDDED_MODEL,
    FBX_IMPORT_PATH_LEGACY6000_EMBEDDED_MODEL_WITH_TAKES,
    FBX_IMPORT_PATH_UNKNOWN
} FbxImportPath;

/* Bit flags from the legacy/profile scanner. */
#define FBX_VERSION_FLAG_IS_6000                    0x00000001u
#define FBX_VERSION_FLAG_MODEL_EMBEDDED_MESH        0x00000002u
#define FBX_VERSION_FLAG_NO_MODERN_GEOMETRY         0x00000004u
#define FBX_VERSION_FLAG_MODEL_MESH_NO_NUMERIC_ID   0x00000008u
#define FBX_VERSION_FLAG_MANY_AUX_ROOTS             0x00000010u
#define FBX_VERSION_FLAG_LEGACY_TAKES               0x00000020u
#define FBX_VERSION_FLAG_STRING_CONNECTIONS         0x00000040u
#define FBX_VERSION_FLAG_HYPER_LEGACY6000_CASE      0x00000080u

typedef struct FbxVersionSceneProfile {
    FbxVersionInfo info;
    fbx_u32 flags;
    FbxImportPath import_path;

    int object_count;
    int model_count;
    int modern_geometry_count;
    int embedded_model_mesh_count;
    int numeric_model_id_count;
    int no_numeric_model_id_count;
    int aux_root_hint_count;       /* Model names like Object_0/Object_1... */
    int legacy_take_count;         /* named Take nodes under Takes */
    int string_connection_count;   /* old Connect nodes using string endpoints */
} FbxVersionSceneProfile;

int         fbx_get_version_info(FbxContext *ctx, FbxVersionInfo *out);
const char *fbx_version_get_profile(const FbxVersionInfo *info);
int         fbx_is_version_supported(const FbxVersionInfo *info);

/* Analyze root tree and choose the import path.
   The legacy6000 path is selected only for the hyper-specific noisy shape:
       version == 6000
       + Objects/Model("name", "Mesh") has Vertices + PolygonVertexIndex
       + no Objects/Geometry modern nodes
       + Model Mesh uses string props instead of numeric id props
       + many Object_n auxiliary roots OR legacy named Takes
   string Connections are diagnostic only, because clean and noisy FBX 6000
   files can both use old string Connect nodes. */
int fbx_version_analyze_scene(FbxNode *root,
                              const FbxVersionInfo *info,
                              FbxVersionSceneProfile *out);

int fbx_version_should_use_legacy6000(const FbxVersionSceneProfile *profile);
const char *fbx_import_path_name(FbxImportPath path);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_VERSION_H */
