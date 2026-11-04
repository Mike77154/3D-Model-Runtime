#ifndef GIF_MS3D_MTL_H
#define GIF_MS3D_MTL_H
/* Optional OBJ/MTL disk bridge. Does not belong to the render/runtime API.
 * Material images are copied as bytes; no image decoding or heap allocation.
 */
#include "gif_ms3d_codec.h"
#include <stddef.h>
#define MS3D_MTL_PATH_CAP 1024

typedef struct ms3d_mtl_bundle_s {
    char texture[MS3D_MAX_MATERIALS][96];
    char alpha[MS3D_MAX_MATERIALS][96];
    unsigned missing_diffuse;
    unsigned missing_alpha;
    unsigned copied_diffuse;
    unsigned copied_alpha;
} ms3d_mtl_bundle;

void ms3d_mtl_filename(const char *obj_path, char *mtl_path, size_t cap);
const char *ms3d_mtl_basename(const char *path);
/* image_root: directory with source textures; when NULL try model directory.
 * Each exported image receives a unique material index prefix.
 * Missing sources leave original models intact and appear in counters.
 */
int ms3d_mtl_export(const char *mtl_path, const char *ms3d_path,
                    const char *image_root, const ms3d_model *model,
                    ms3d_mtl_bundle *info);
#endif
