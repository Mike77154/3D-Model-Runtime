#ifndef ASK3D_EVAL_H
#define ASK3D_EVAL_H

#include "ask3d_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ask3dEvalTransform_s {
    Ask3dVec3 position;
    Ask3dVec3 rotation_axis;
    Ask3dVec3 scale;
    ask3d_fixed rotation_angle;
    unsigned int has_position;
    unsigned int has_rotation;
    unsigned int has_scale;
} Ask3dEvalTransform;

void ask3d_eval_transform_clear(Ask3dEvalTransform *out);
int ask3d_eval_object_transform(const Ask3dScene *scene, int object_index, long timevalue, Ask3dEvalTransform *out);
int ask3d_eval_setting_scalar(const Ask3dScene *scene, int node_type, int node_index, int setting_type, long timevalue, ask3d_fixed *out);
int ask3d_eval_setting_vec3(const Ask3dScene *scene, int node_type, int node_index, int setting_type, long timevalue, Ask3dVec3 *out);

#ifdef __cplusplus
}
#endif

#endif
