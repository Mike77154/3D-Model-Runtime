/* fbx_importer_bin.h */
#ifndef FBX_IMPORTER_BIN_H
#define FBX_IMPORTER_BIN_H

#include "fbx_importer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Lee la escena FBX asumiendo formato binario.
   Devuelve el root (mismo tipo FbxNode*) o NULL en error. */
FbxNode *fbx_read_scene_binary(FbxContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* FBX_IMPORTER_BIN_H */
