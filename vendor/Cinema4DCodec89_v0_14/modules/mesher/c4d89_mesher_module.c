#include "c4d89_mesher.h"

int c4d89_mesher_index(const char *path,C4D89_MeshTable *out)
{return c4d89_find_meshes(path,out);}
int c4d89_mesher_stream(const char *path,unsigned int mesh_number,
    const C4D89_MeshDesc *mesh,const C4D89_MeshSink *sink,void *user)
{return c4d89_stream_mesh(path,mesh_number,mesh,sink,user);}
int c4d89_mesher_debug_obj(const char *path,const char *dest)
{return c4d89_export_obj(path,dest);}
