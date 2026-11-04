#include "c4d89.h"
#include <string.h>
static unsigned int r16(const unsigned char *b)
{ return ((unsigned int)b[0]<<8)|(unsigned int)b[1]; }
static int read_at(FILE*f,unsigned long at)
{ return fseek(f,(long)at,SEEK_SET)==0; }
int c4d89_stream_mesh(const char *path,unsigned int mesh_number,
    const C4D89_MeshDesc *m,const C4D89_MeshSink *s,void *user)
{
    FILE*f;unsigned long i;
    unsigned char b[12];long x,y,z;
    C4D89_Face face;
    if(!path||!m||!s)return C4D89_ERR_CORRUPT;
    if(m->vertex_count>C4D89_MAX_MESH_VERTICES ||
       m->face_count>C4D89_MAX_MESH_FACES ||
       m->uv_count!=m->face_count*4ul)return C4D89_ERR_LIMIT;
    f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
    if(s->begin && !s->begin(user,mesh_number,m)){fclose(f);return C4D89_ERR_SINK;}
    if(!read_at(f,m->vertex_offset)){fclose(f);return C4D89_ERR_IO;}
    for(i=0ul;i<m->vertex_count;i++) {
        if(fread(b,1u,12u,f)!=12u ||
           c4d89_be_float_to_q16(b,&x)!=0 ||
           c4d89_be_float_to_q16(b+4,&y)!=0 ||
           c4d89_be_float_to_q16(b+8,&z)!=0){fclose(f);return C4D89_ERR_CORRUPT;}
        if(s->vertex && !s->vertex(user,i,x,y,z)){fclose(f);return C4D89_ERR_SINK;}
    }
    if(!read_at(f,m->uv_offset)){fclose(f);return C4D89_ERR_IO;}
    for(i=0ul;i<m->uv_count;i++) {
        if(fread(b,1u,12u,f)!=12u ||
           c4d89_be_float_to_q16(b,&x)!=0 ||
           c4d89_be_float_to_q16(b+4,&y)!=0){fclose(f);return C4D89_ERR_CORRUPT;}
        if(s->uv && !s->uv(user,i,x,y)){fclose(f);return C4D89_ERR_SINK;}
    }
    if(!read_at(f,m->face_offset)){fclose(f);return C4D89_ERR_IO;}
    for(i=0ul;i<m->face_count;i++) {
        if(fread(b,1u,8u,f)!=8u){fclose(f);return C4D89_ERR_CORRUPT;}
        face.a=r16(b); face.b=r16(b+2);face.c=r16(b+4);face.d=r16(b+6);
        if(face.a>=m->vertex_count||face.b>=m->vertex_count||
           face.c>=m->vertex_count||face.d>=m->vertex_count){fclose(f);return C4D89_ERR_CORRUPT;}
        face.corners=(face.c==face.d)?3u:4u;
        if(s->face && !s->face(user,i,&face)){fclose(f);return C4D89_ERR_SINK;}
    }
    if(s->end && !s->end(user,mesh_number)){fclose(f);return C4D89_ERR_SINK;}
    fclose(f);return C4D89_OK;
}
int c4d89_stream_scene(const char *path,const C4D89_MeshSink *sink,void *user)
{
    static C4D89_MeshTable table;unsigned int i;int rc;
    rc=c4d89_find_meshes(path,&table);if(rc)return rc;
    for(i=0u;i<table.count;i++) {
        rc=c4d89_stream_mesh(path,i,&table.meshes[i],sink,user);
        if(rc)return rc;
    }
    return C4D89_OK;
}
int c4d89_load_mesh(const char *path,void *engine_scene)
{
    C4D89_MeshReceiver *receiver=(C4D89_MeshReceiver*)engine_scene;
    if(!receiver)return C4D89_ERR_CORRUPT;
    return c4d89_stream_scene(path,&receiver->sink,receiver->user);
}
