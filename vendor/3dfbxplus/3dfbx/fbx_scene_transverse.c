#include "fbx_scene_traverse.h"

/* ====== Internos recursivos (sin profundidad) ====== */

static void fbx_model_traverse_dfs_internal(FbxModel *m,
                                            FbxModelVisitor visitor,
                                            void *user_data)
{
    int i;

    if (!m || !visitor) return;

    /* Pre-order: visitamos primero el nodo, luego los hijos */
    visitor(m, user_data);

    for (i = 0; i < m->child_count; ++i) {
        fbx_model_traverse_dfs_internal(m->children[i], visitor, user_data);
    }
}

static int fbx_model_traverse_dfs_ex_internal(FbxModel *m,
                                              FbxModelVisitorEx visitor,
                                              void *user_data)
{
    int i;

    if (!m || !visitor) return 1; /* 1 = continuar */

    if (!visitor(m, user_data)) {
        return 0;
    }

    for (i = 0; i < m->child_count; ++i) {
        if (!fbx_model_traverse_dfs_ex_internal(m->children[i], visitor, user_data)) {
            return 0;
        }
    }

    return 1;
}

/* ====== Internos recursivos (con profundidad) ====== */

static void fbx_model_traverse_dfs_depth_internal(FbxModel *m,
                                                  int depth,
                                                  FbxModelVisitorDepth visitor,
                                                  void *user_data)
{
    int i;

    if (!m || !visitor) return;

    visitor(m, depth, user_data);

    for (i = 0; i < m->child_count; ++i) {
        fbx_model_traverse_dfs_depth_internal(m->children[i], depth + 1, visitor, user_data);
    }
}

static int fbx_model_traverse_dfs_depth_ex_internal(FbxModel *m,
                                                    int depth,
                                                    FbxModelVisitorDepthEx visitor,
                                                    void *user_data)
{
    int i;

    if (!m || !visitor) return 1;

    if (!visitor(m, depth, user_data)) {
        return 0;
    }

    for (i = 0; i < m->child_count; ++i) {
        if (!fbx_model_traverse_dfs_depth_ex_internal(m->children[i], depth + 1, visitor, user_data)) {
            return 0;
        }
    }

    return 1;
}

/* ====== API pública (sin profundidad) ====== */

void fbx_scene_traverse_dfs(FbxScene *scene,
                            FbxModelVisitor visitor,
                            void *user_data)
{
    FbxModel *m;

    if (!scene || !visitor) return;

    m = scene->models;
    while (m) {
        if (!m->parent) {
            fbx_model_traverse_dfs_internal(m, visitor, user_data);
        }
        m = m->next;
    }
}

void fbx_scene_traverse_dfs_ex(FbxScene *scene,
                               FbxModelVisitorEx visitor,
                               void *user_data)
{
    FbxModel *m;

    if (!scene || !visitor) return;

    m = scene->models;
    while (m) {
        if (!m->parent) {
            if (!fbx_model_traverse_dfs_ex_internal(m, visitor, user_data)) {
                break;
            }
        }
        m = m->next;
    }
}

void fbx_model_traverse_dfs(FbxModel *root_model,
                            FbxModelVisitor visitor,
                            void *user_data)
{
    if (!root_model || !visitor) return;
    fbx_model_traverse_dfs_internal(root_model, visitor, user_data);
}

void fbx_model_traverse_dfs_ex(FbxModel *root_model,
                               FbxModelVisitorEx visitor,
                               void *user_data)
{
    if (!root_model || !visitor) return;
    (void)fbx_model_traverse_dfs_ex_internal(root_model, visitor, user_data);
}

/* ====== API pública (con profundidad) ====== */

void fbx_scene_traverse_dfs_with_depth(FbxScene *scene,
                                       FbxModelVisitorDepth visitor,
                                       void *user_data)
{
    FbxModel *m;

    if (!scene || !visitor) return;

    m = scene->models;
    while (m) {
        if (!m->parent) {
            fbx_model_traverse_dfs_depth_internal(m, 0, visitor, user_data);
        }
        m = m->next;
    }
}

void fbx_scene_traverse_dfs_with_depth_ex(FbxScene *scene,
                                          FbxModelVisitorDepthEx visitor,
                                          void *user_data)
{
    FbxModel *m;

    if (!scene || !visitor) return;

    m = scene->models;
    while (m) {
        if (!m->parent) {
            if (!fbx_model_traverse_dfs_depth_ex_internal(m, 0, visitor, user_data)) {
                break;
            }
        }
        m = m->next;
    }
}

void fbx_model_traverse_dfs_with_depth(FbxModel *root_model,
                                       FbxModelVisitorDepth visitor,
                                       void *user_data)
{
    if (!root_model || !visitor) return;
    fbx_model_traverse_dfs_depth_internal(root_model, 0, visitor, user_data);
}

void fbx_model_traverse_dfs_with_depth_ex(FbxModel *root_model,
                                          FbxModelVisitorDepthEx visitor,
                                          void *user_data)
{
    if (!root_model || !visitor) return;
    (void)fbx_model_traverse_dfs_depth_ex_internal(root_model, 0, visitor, user_data);
}
