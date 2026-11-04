#ifndef VR1_EXPORT_OBJ_H
#define VR1_EXPORT_OBJ_H

#include <stdio.h>
#include "vr1_scene.h"

int vr1_export_obj(FILE *f, const vr1_scene *s, const char *object_name);
int vr1_export_mtl(FILE *f, const vr1_scene *s);
void vr1_print_summary(FILE *f, const vr1_scene *s);
void vr1_fprint_fx(FILE *f, vr1_fx v);

#endif
