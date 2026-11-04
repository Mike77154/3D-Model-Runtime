#include "c4d89_shaderer.h"

int c4d89_shaderer_emit(const char *path,unsigned int mesh_number,
    const C4D89_MeshDesc *mesh,const C4D89_PolyRegions *regions,
    int cosine_q10,C4D89_SmoothFaceFn cb,void *user)
{return c4d89_emit_smooth_faces(path,mesh_number,mesh,regions,
    cosine_q10,cb,user);}
int c4d89_shaderer_debug_obj(const char *path,const char *dest,int cosine_q10)
{return c4d89_export_obj_smooth(path,dest,cosine_q10);}
