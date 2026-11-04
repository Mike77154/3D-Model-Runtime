#ifndef FBX_MATERIAL_H
#define FBX_MATERIAL_H

#include "fbx_types.h"
#include "fbx_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FBX_TEX_USAGE_DIFFUSE = 0,
    FBX_TEX_USAGE_NORMAL,
    FBX_TEX_USAGE_SPECULAR,
    FBX_TEX_USAGE_METALNESS,
    FBX_TEX_USAGE_ROUGHNESS,
    FBX_TEX_USAGE_EMISSIVE,
    FBX_TEX_USAGE_OCCLUSION,
    FBX_TEX_USAGE_OPACITY,
    FBX_TEX_USAGE_UNKNOWN
} FbxTextureUsage;

typedef struct FbxTextureSlot {
    FbxTextureUsage  usage;
    char            *file_path;     /* ruta original de la textura */
    char            *relative_path; /* ruta relativa si existe */
    char            *name;          /* nombre Texture/Video si existe */
    int              uv_set_index;  /* qué UV usar */

    /* FBX object IDs for diagnostics and roundtrip. texture_id is the FbxFileTexture
       object; video_id is the linked Video/Clip object when present. */
    fbx_i64          texture_id;
    fbx_i64          video_id;

    /* Embedded textures live in Video::Content. Data is copied into the active
       arena so the scene stays valid after the raw tree is destroyed/reset as a unit. */
    int              embedded;
    fbx_u32          embedded_size;
    fbx_u8          *embedded_data;
} FbxTextureSlot;

typedef struct FbxMaterial {
    fbx_i64     fbx_id;
    char       *name;

    fbx_real       diffuse_color[3];
    fbx_real       specular_color[3];
    fbx_real       emissive_color[3];
    fbx_real       opacity;

    fbx_real       metalness;
    fbx_real       roughness;

    int             texture_count;
    FbxTextureSlot *textures;
} FbxMaterial;

FbxMaterial *fbx_material_create(fbx_i64 fbx_id, const char *name);
void         fbx_material_destroy(FbxMaterial *mat);

/* Devuelve índice del slot creado, o -1 en error */
int          fbx_material_add_texture(FbxMaterial *mat,
                                     FbxTextureUsage usage,
                                     const char *path,
                                     int uv_set_index);

int          fbx_material_add_texture_full(FbxMaterial *mat,
                                          FbxTextureUsage usage,
                                          const char *path,
                                          const char *relative_path,
                                          const char *name,
                                          int uv_set_index,
                                          fbx_i64 texture_id,
                                          fbx_i64 video_id,
                                          const fbx_u8 *embedded_data,
                                          fbx_u32 embedded_size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_MATERIAL_H */
