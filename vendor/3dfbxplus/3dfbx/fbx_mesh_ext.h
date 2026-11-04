#ifndef FBX_MESH_EXT_H
#define FBX_MESH_EXT_H

#include "fbx_types.h"
#include "fbx_fixed.h"
#include "fbx_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FBX_SUBMESH_DEFINED
#define FBX_SUBMESH_DEFINED
typedef struct FbxSubMeshTag {
    fbx_u32 first_index;
    fbx_u32 index_count;
    int     material_index;
} FbxSubMesh;
#endif

typedef struct FbxMeshExt {
    FbxMesh   *base;

    /* Multi-UV */
    fbx_u32   uv_set_count;
    fbx_real   **uv_sets;        /* uv_sets[i] = fbx_real[2 * vertex_count] */

    /* Tangentes / bitangentes */
    fbx_real    *tangents;       /* xyz por vértice */
    fbx_real    *bitangents;

    /* Colores de vértice */
    fbx_real    *colors;         /* rgba por vértice */

    /* Submeshes */
    fbx_u32    submesh_count;
    FbxSubMesh *submeshes;
} FbxMeshExt;

FbxMeshExt *fbx_mesh_ext_create(FbxMesh *base);
void        fbx_mesh_ext_destroy(FbxMeshExt *ext);
int         fbx_mesh_ext_add_uv_set(FbxMeshExt *ext);
int         fbx_mesh_ext_add_submesh(FbxMeshExt *ext,
                                     fbx_u32 first_index,
                                     fbx_u32 index_count,
                                     int material_index);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_MESH_EXT_H */
