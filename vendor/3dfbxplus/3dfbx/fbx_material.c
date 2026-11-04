/* fbx_material.c */
#include "fbx_material.h"
#include "fbx_alloc.h"

#include <string.h>

/* Puedes exponer en el .h:
 *   FbxMaterial *fbx_material_create(fbx_i64 fbx_id, const char *name);
 *   void         fbx_material_destroy(FbxMaterial *mat);
 *   int          fbx_material_add_texture(FbxMaterial *mat,
 *                                         FbxTextureUsage usage,
 *                                         const char *path,
 *                                         int uv_set_index);
 */

static char *fbx_mat_strdup(const char *s) {
    size_t len;
    char *out;
    if (!s) return NULL;
    len = strlen(s);
    out = (char *)fbx_alloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

FbxMaterial *fbx_material_create(fbx_i64 fbx_id, const char *name) {
    FbxMaterial *mat;

    mat = (FbxMaterial *)fbx_alloc_zero_n(1, sizeof(FbxMaterial));
    if (!mat) return NULL;

    mat->fbx_id = fbx_id;

    if (name) {
        mat->name = fbx_mat_strdup(name);
        if (!mat->name) {
            fbx_release(mat);
            return NULL;
        }
    }

    /* Valores por defecto razonables */
    mat->diffuse_color[0]  = fbx_real_from_ratio(4,5);
    mat->diffuse_color[1]  = fbx_real_from_ratio(4,5);
    mat->diffuse_color[2]  = fbx_real_from_ratio(4,5);

    mat->specular_color[0] = FBX_REAL_ZERO;
    mat->specular_color[1] = FBX_REAL_ZERO;
    mat->specular_color[2] = FBX_REAL_ZERO;

    mat->emissive_color[0] = FBX_REAL_ZERO;
    mat->emissive_color[1] = FBX_REAL_ZERO;
    mat->emissive_color[2] = FBX_REAL_ZERO;

    mat->opacity  = FBX_REAL_ONE;
    mat->metalness = FBX_REAL_ZERO;
    mat->roughness = FBX_REAL_ONE;

    mat->texture_count = 0;
    mat->textures      = NULL;

    return mat;
}

void fbx_material_destroy(FbxMaterial *mat) {
    int i;
    if (!mat) return;

    if (mat->textures) {
        for (i = 0; i < mat->texture_count; ++i) {
            fbx_release(mat->textures[i].file_path);
            fbx_release(mat->textures[i].relative_path);
            fbx_release(mat->textures[i].name);
            fbx_release(mat->textures[i].embedded_data);
            mat->textures[i].file_path = NULL;
            mat->textures[i].relative_path = NULL;
            mat->textures[i].name = NULL;
            mat->textures[i].embedded_data = NULL;
        }
        fbx_release(mat->textures);
    }

    fbx_release(mat->name);
    fbx_release(mat);
}

/* Devuelve índice del slot creado, o -1 en error */
int fbx_material_add_texture(FbxMaterial *mat,
                             FbxTextureUsage usage,
                             const char *path,
                             int uv_set_index)
{
    return fbx_material_add_texture_full(mat, usage, path, NULL, NULL,
                                         uv_set_index, 0, 0, NULL, 0u);
}

int fbx_material_add_texture_full(FbxMaterial *mat,
                                  FbxTextureUsage usage,
                                  const char *path,
                                  const char *relative_path,
                                  const char *name,
                                  int uv_set_index,
                                  fbx_i64 texture_id,
                                  fbx_i64 video_id,
                                  const fbx_u8 *embedded_data,
                                  fbx_u32 embedded_size)
{
    FbxTextureSlot *slots;
    FbxTextureSlot *slot;
    char *copy_path;
    char *copy_rel;
    char *copy_name;
    fbx_u8 *copy_blob;

    if (!mat) return -1;
    if ((!path || path[0] == '\0') && (!relative_path || relative_path[0] == '\0') && embedded_size == 0u) return -1;

    copy_path = NULL;
    copy_rel = NULL;
    copy_name = NULL;
    copy_blob = NULL;

    if (path && path[0] != '\0') {
        copy_path = fbx_mat_strdup(path);
        if (!copy_path) return -1;
    }
    if (relative_path && relative_path[0] != '\0') {
        copy_rel = fbx_mat_strdup(relative_path);
        if (!copy_rel) {
            fbx_release(copy_path);
            return -1;
        }
    }
    if (name && name[0] != '\0') {
        copy_name = fbx_mat_strdup(name);
        if (!copy_name) {
            fbx_release(copy_path);
            fbx_release(copy_rel);
            return -1;
        }
    }
    if (embedded_data && embedded_size > 0u) {
        copy_blob = (fbx_u8*)fbx_alloc((size_t)embedded_size);
        if (!copy_blob) {
            fbx_release(copy_path);
            fbx_release(copy_rel);
            fbx_release(copy_name);
            return -1;
        }
        memcpy(copy_blob, embedded_data, (size_t)embedded_size);
    }

    slots = (FbxTextureSlot *)fbx_resize(
        mat->textures,
        (size_t)(mat->texture_count + 1) * sizeof(FbxTextureSlot)
    );
    if (!slots) {
        fbx_release(copy_path);
        fbx_release(copy_rel);
        fbx_release(copy_name);
        fbx_release(copy_blob);
        return -1;
    }

    mat->textures = slots;
    slot = &mat->textures[mat->texture_count];
    memset(slot, 0, sizeof(*slot));
    slot->usage = usage;
    slot->file_path = copy_path ? copy_path : copy_rel;
    slot->relative_path = copy_rel;
    slot->name = copy_name;
    slot->uv_set_index = uv_set_index;
    slot->texture_id = texture_id;
    slot->video_id = video_id;
    slot->embedded = (copy_blob && embedded_size > 0u) ? 1 : 0;
    slot->embedded_size = embedded_size;
    slot->embedded_data = copy_blob;

    /* If path was absent but relative_path was duplicated into file_path, keep only
       one owner to avoid double-release with arena-compatible fbx_release(). */
    if (!copy_path && copy_rel) slot->relative_path = NULL;

    mat->texture_count += 1;
    return mat->texture_count - 1;
}
