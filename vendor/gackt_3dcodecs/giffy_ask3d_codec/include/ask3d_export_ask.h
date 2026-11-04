#ifndef ASK3D_EXPORT_ASK_H
#define ASK3D_EXPORT_ASK_H

#include "ask3d_export_obj.h"

#ifdef __cplusplus
extern "C" {
#endif

int ask3d_export_ask(const Ask3dScene *scene, Ask3dWriteFn write_fn, void *user);

#ifdef __cplusplus
}
#endif

#endif
