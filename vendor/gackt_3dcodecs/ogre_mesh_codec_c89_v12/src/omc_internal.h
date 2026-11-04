#ifndef OMC_INTERNAL_H
#define OMC_INTERNAL_H

#include "ogre_mesh_codec.h"

void omc__diag_public(OMC_Mesh *mesh, int code, const char *msg);
void omc__copy_str_public(char *dst, unsigned long cap, const char *src);

#endif
