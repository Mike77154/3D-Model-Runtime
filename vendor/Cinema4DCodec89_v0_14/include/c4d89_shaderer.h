/* Cinema4DCodec89 independent shaderer API. C89 / CC0. */
#ifndef C4D89_MODULE_SHADERER_H
#define C4D89_MODULE_SHADERER_H
#include "c4d89.h"
/* Crease-aware Q10 normals. This does not change geometry or UVs. */
int c4d89_shaderer_emit(const char *path,unsigned int mesh_number,
    const C4D89_MeshDesc *mesh,const C4D89_PolyRegions *regions,
    int cosine_q10,C4D89_SmoothFaceFn cb,void *user);
int c4d89_shaderer_debug_obj(const char *path,const char *dest,int cosine_q10);
#endif
