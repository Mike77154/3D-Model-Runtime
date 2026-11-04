#ifndef ASK3D_RESOLVE_H
#define ASK3D_RESOLVE_H

#include "ask3d_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ask3dResolveReport_s {
    unsigned int parent_links_resolved;
    unsigned int missing_parent_links;
    unsigned int faces_checked;
    unsigned int faces_resolved;
    unsigned int material_use_count;
    unsigned int material_use[ASK3D_MAX_MATERIALS];
} Ask3dResolveReport;

void ask3d_resolve_report_clear(Ask3dResolveReport *report);
int ask3d_find_node_by_name(const Ask3dScene *scene, const char *name, int *node_type, int *node_index);
int ask3d_resolve_face_material_index(const Ask3dScene *scene, const Ask3dObject *obj, const Ask3dFace *face);
int ask3d_resolve_scene(Ask3dScene *scene, Ask3dResolveReport *report);

#ifdef __cplusplus
}
#endif

#endif
