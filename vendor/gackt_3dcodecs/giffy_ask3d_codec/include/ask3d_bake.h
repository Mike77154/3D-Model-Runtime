#ifndef ASK3D_BAKE_H
#define ASK3D_BAKE_H

#include "ask3d_scene.h"
#include "ask3d_xform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ask3dBakedVertex_s {
    Ask3dVec3 position;
    Ask3dVec3 normal;
    Ask3dVec3 tangent;
    Ask3dVec2 uv;
    Ask3dVec3 color;
    int material_index;
    unsigned int source_object;
    unsigned int source_face;
    unsigned int source_corner;
    unsigned int has_normal;
    unsigned int has_tangent;
    unsigned int has_uv;
    unsigned int has_color;
} Ask3dBakedVertex;

typedef struct Ask3dBakedMesh_s {
    Ask3dBakedVertex vertices[ASK3D_MAX_BAKED_VERTICES];
    unsigned int vertex_count;
    unsigned int triangle_count;
    unsigned int material_count;
    unsigned int material_use[ASK3D_MAX_MATERIALS];
    unsigned int overflowed;
} Ask3dBakedMesh;

void ask3d_baked_mesh_clear(Ask3dBakedMesh *mesh);
int ask3d_bake_object_local(const Ask3dScene *scene, unsigned int object_index, Ask3dBakedMesh *mesh);
int ask3d_bake_object_world(const Ask3dScene *scene, unsigned int object_index, Ask3dBakedMesh *mesh);
int ask3d_bake_scene_local(const Ask3dScene *scene, Ask3dBakedMesh *mesh);
int ask3d_bake_scene_world(const Ask3dScene *scene, Ask3dBakedMesh *mesh);

#ifdef __cplusplus
}
#endif

#endif
