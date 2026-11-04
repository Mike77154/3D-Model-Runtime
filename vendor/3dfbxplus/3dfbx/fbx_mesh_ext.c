/* fbx_mesh_ext.c */
#include "fbx_mesh_ext.h"
#include "fbx_alloc.h"

#include <string.h>

/* Puedes exponer:
 *   FbxMeshExt *fbx_mesh_ext_create(FbxMesh *base);
 *   void        fbx_mesh_ext_destroy(FbxMeshExt *ext);
 *   int         fbx_mesh_ext_add_uv_set(FbxMeshExt *ext);
 *   int         fbx_mesh_ext_add_submesh(FbxMeshExt *ext,
 *                                        fbx_u32 first_index,
 *                                        fbx_u32 index_count,
 *                                        int material_index);
 */

FbxMeshExt *fbx_mesh_ext_create(FbxMesh *base) {
    FbxMeshExt *ext;

    if (!base) return NULL;

    ext = (FbxMeshExt *)fbx_alloc_zero_n(1, sizeof(FbxMeshExt));
    if (!ext) return NULL;

    ext->base          = base;
    ext->uv_set_count  = 0;
    ext->uv_sets       = NULL;
    ext->tangents      = NULL;
    ext->bitangents    = NULL;
    ext->colors        = NULL;
    ext->submesh_count = 0;
    ext->submeshes     = NULL;

    return ext;
}

void fbx_mesh_ext_destroy(FbxMeshExt *ext) {
    fbx_u32 i;

    if (!ext) return;

    if (ext->uv_sets) {
        for (i = 0; i < ext->uv_set_count; ++i) {
            fbx_release(ext->uv_sets[i]);
        }
        fbx_release(ext->uv_sets);
    }

    fbx_release(ext->tangents);
    fbx_release(ext->bitangents);
    fbx_release(ext->colors);
    fbx_release(ext->submeshes);

    fbx_release(ext);
}

/* Añade un UV set vacío de tamaño 2 * vertex_count.
 * Devuelve el índice del nuevo UV set, o -1 si falla.
 */
int fbx_mesh_ext_add_uv_set(FbxMeshExt *ext) {
    fbx_real **new_sets;
    fbx_real  *uv_buf;
    fbx_u32 new_count;
    fbx_u32 vertex_count;

    if (!ext || !ext->base) return -1;

    vertex_count = ext->base->vertex_count;
    if (vertex_count == 0) return -1;

    uv_buf = (fbx_real *)fbx_alloc_zero_n((size_t)vertex_count * 2u, sizeof(fbx_real));
    if (!uv_buf) return -1;

    new_count = ext->uv_set_count + 1;

    new_sets = (fbx_real **)fbx_resize(
        ext->uv_sets,
        (size_t)new_count * sizeof(fbx_real *)
    );
    if (!new_sets) {
        fbx_release(uv_buf);
        return -1;
    }

    ext->uv_sets = new_sets;
    ext->uv_sets[ext->uv_set_count] = uv_buf;
    ext->uv_set_count = new_count;

    return (int)(new_count - 1);
}

/* Añade un submesh; devuelve índice o -1 en error */
int fbx_mesh_ext_add_submesh(FbxMeshExt *ext,
                             fbx_u32 first_index,
                             fbx_u32 index_count,
                             int      material_index)
{
    FbxSubMesh *subs;
    FbxSubMesh *sm;
    fbx_u32 new_count;

    if (!ext) return -1;
    if (index_count == 0) return -1;

    new_count = ext->submesh_count + 1;

    subs = (FbxSubMesh *)fbx_resize(
        ext->submeshes,
        (size_t)new_count * sizeof(FbxSubMesh)
    );
    if (!subs) {
        return -1;
    }

    ext->submeshes = subs;

    sm = &ext->submeshes[ext->submesh_count];
    sm->first_index    = first_index;
    sm->index_count    = index_count;
    sm->material_index = material_index;

    ext->submesh_count = new_count;

    return (int)(new_count - 1);
}
