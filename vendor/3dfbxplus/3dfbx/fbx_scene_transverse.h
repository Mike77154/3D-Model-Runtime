#ifndef FBX_SCENE_TRAVERSE_H
#define FBX_SCENE_TRAVERSE_H

#include "fbx_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Visitor simple: se llama por cada modelo visitado (DFS pre-order) */
typedef void (*FbxModelVisitor)(FbxModel *model, void *user_data);

/* Versión con control de flujo: devolver 0 para cortar la travesía. */
typedef int (*FbxModelVisitorEx)(FbxModel *model, void *user_data);

/* Visitor con profundidad (depth = 0 en cada root, 1 hijos, etc.) */
typedef void (*FbxModelVisitorDepth)(FbxModel *model, int depth, void *user_data);

/* Versión con profundidad + early-exit */
typedef int (*FbxModelVisitorDepthEx)(FbxModel *model, int depth, void *user_data);

/*
 * Recorre toda la escena en profundidad (pre-order):
 *  - Empieza en cada root (model sin parent).
 *  - Llama visitor en cada nodo.
 */
void fbx_scene_traverse_dfs(FbxScene *scene,
                            FbxModelVisitor visitor,
                            void *user_data);

/*
 * Igual que la anterior, pero el visitor puede devolver 0 para parar
 * la travesía completa (útil para búsqueda global).
 */
void fbx_scene_traverse_dfs_ex(FbxScene *scene,
                               FbxModelVisitorEx visitor,
                               void *user_data);

/*
 * Versión centrada en un solo subárbol:
 *  - Empieza en root_model (aunque tenga parent).
 */
void fbx_model_traverse_dfs(FbxModel *root_model,
                            FbxModelVisitor visitor,
                            void *user_data);

/*
 * Versión con early-exit a nivel de subárbol.
 */
void fbx_model_traverse_dfs_ex(FbxModel *root_model,
                               FbxModelVisitorEx visitor,
                               void *user_data);

/* ====== Variantes con profundidad ====== */

/*
 * DFS sobre toda la escena, pasando profundidad (depth) al visitor.
 */
void fbx_scene_traverse_dfs_with_depth(FbxScene *scene,
                                       FbxModelVisitorDepth visitor,
                                       void *user_data);

/*
 * DFS sobre escena con profundidad + early-exit.
 */
void fbx_scene_traverse_dfs_with_depth_ex(FbxScene *scene,
                                          FbxModelVisitorDepthEx visitor,
                                          void *user_data);

/*
 * DFS sobre subárbol concreto con profundidad.
 */
void fbx_model_traverse_dfs_with_depth(FbxModel *root_model,
                                       FbxModelVisitorDepth visitor,
                                       void *user_data);

/*
 * DFS sobre subárbol concreto con profundidad + early-exit.
 */
void fbx_model_traverse_dfs_with_depth_ex(FbxModel *root_model,
                                          FbxModelVisitorDepthEx visitor,
                                          void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* FBX_SCENE_TRAVERSE_H */
