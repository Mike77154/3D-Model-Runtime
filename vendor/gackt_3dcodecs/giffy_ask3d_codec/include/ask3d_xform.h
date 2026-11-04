#ifndef ASK3D_XFORM_H
#define ASK3D_XFORM_H

#include "ask3d_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ask3dMatrix34_s {
    Ask3dVec3 row0;
    Ask3dVec3 row1;
    Ask3dVec3 row2;
    Ask3dVec3 row3;
    unsigned int valid;
} Ask3dMatrix34;

void ask3d_matrix_identity(Ask3dMatrix34 *m);
void ask3d_matrix_from_rows(Ask3dMatrix34 *m, const Ask3dVec3 *r0, const Ask3dVec3 *r1, const Ask3dVec3 *r2, const Ask3dVec3 *r3);
void ask3d_matrix_mul(const Ask3dMatrix34 *a, const Ask3dMatrix34 *b, Ask3dMatrix34 *out);
void ask3d_matrix_transform_point(const Ask3dMatrix34 *m, const Ask3dVec3 *p, Ask3dVec3 *out);
void ask3d_matrix_transform_vector(const Ask3dMatrix34 *m, const Ask3dVec3 *p, Ask3dVec3 *out);
int ask3d_get_local_matrix(const Ask3dScene *scene, int node_type, int node_index, Ask3dMatrix34 *out);
int ask3d_get_world_matrix(const Ask3dScene *scene, int node_type, int node_index, Ask3dMatrix34 *out);

#ifdef __cplusplus
}
#endif

#endif
