/* Cinema4DCodec89 independent mesher API. C89 / CC0. */
#ifndef C4D89_MODULE_MESHER_H
#define C4D89_MODULE_MESHER_H
#include "c4d89.h"
/* Native vertices/faces; Q16.16 and corner UVs in streaming callbacks. */
int c4d89_mesher_index(const char *path,C4D89_MeshTable *out);
int c4d89_mesher_stream(const char *path,unsigned int mesh_number,
    const C4D89_MeshDesc *mesh,const C4D89_MeshSink *sink,void *user);
int c4d89_mesher_debug_obj(const char *path,const char *dest);
#endif
