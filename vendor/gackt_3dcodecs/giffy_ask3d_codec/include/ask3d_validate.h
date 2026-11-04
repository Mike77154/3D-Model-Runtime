#ifndef ASK3D_VALIDATE_H
#define ASK3D_VALIDATE_H

#include "ask3d_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ask3dValidateReport_s {
    unsigned int faces_checked;
    unsigned int faces_with_uvs;
    unsigned int faces_with_colors;
    unsigned int faces_with_normals;
    unsigned int uv_channel_faces_checked;
    unsigned int missing_vertex_refs;
    unsigned int missing_tvertex_refs;
    unsigned int missing_cvertex_refs;
    unsigned int missing_uv_channel_tvertex_refs;
    unsigned int missing_uv_channel_face_refs;
    unsigned int missing_material_refs;
    unsigned int missing_parent_refs;
} Ask3dValidateReport;

void ask3d_validate_report_clear(Ask3dValidateReport *report);
int ask3d_validate_scene(const Ask3dScene *scene, Ask3dValidateReport *report);

#ifdef __cplusplus
}
#endif

#endif
