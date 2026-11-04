#ifndef ASK3D_POST_H
#define ASK3D_POST_H

#include "ask3d_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ask3dPostReport_s {
    unsigned int generated_face_normals;
    unsigned int generated_vertex_normals;
    unsigned int already_had_face_normals;
    unsigned int already_had_vertex_normals;
    unsigned int degenerate_faces;
    unsigned int processed_faces;
} Ask3dPostReport;

typedef struct Ask3dTopologyReport_s {
    unsigned int processed_faces;
    unsigned int degenerate_faces;
    unsigned int unique_edges;
    unsigned int boundary_edges;
    unsigned int shared_edges;
    unsigned int nonmanifold_edges;
    unsigned int visible_edges;
    unsigned int hidden_edges;
    unsigned int hard_edges;
    unsigned int smoothing_group_bits;
} Ask3dTopologyReport;

void ask3d_post_report_clear(Ask3dPostReport *report);
void ask3d_topology_report_clear(Ask3dTopologyReport *report);
int ask3d_post_fill_missing_normals(Ask3dScene *scene, Ask3dPostReport *report);
int ask3d_topology_analyze(const Ask3dScene *scene, Ask3dTopologyReport *report);

#ifdef __cplusplus
}
#endif

#endif
