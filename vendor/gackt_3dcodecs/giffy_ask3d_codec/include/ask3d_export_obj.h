#ifndef ASK3D_EXPORT_OBJ_H
#define ASK3D_EXPORT_OBJ_H

#include "ask3d_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*Ask3dWriteFn)(void *user, const char *text);

int ask3d_export_obj(const Ask3dScene *scene, Ask3dWriteFn write_fn, void *user, const char *mtllib_name);
int ask3d_export_mtl(const Ask3dScene *scene, Ask3dWriteFn write_fn, void *user);

#ifdef __cplusplus
}
#endif

#endif
