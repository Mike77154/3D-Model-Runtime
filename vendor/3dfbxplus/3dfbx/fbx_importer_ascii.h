/* fbx_importer_ascii.h */
#ifndef FBX_IMPORTER_ASCII_H
#define FBX_IMPORTER_ASCII_H

#include "fbx_importer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parser ASCII de FBX (formato de texto).
   Devuelve un árbol FbxNode* con todos los nodos de nivel superior
   colgados de un nodo raíz sintético llamado "Root".
   Devuelve NULL en error grave. */
FbxNode *fbx_read_scene_ascii(FbxContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* FBX_IMPORTER_ASCII_H */
