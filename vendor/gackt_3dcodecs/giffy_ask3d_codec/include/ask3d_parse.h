#ifndef ASK3D_PARSE_H
#define ASK3D_PARSE_H

#include "ask3d_scene.h"
#include "ask3d_lex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASK3D_OK             0
#define ASK3D_ERR_BAD_ARG   -1
#define ASK3D_ERR_OVERFLOW  -2
#define ASK3D_ERR_SYNTAX    -3

typedef struct Ask3dWork_s {
    Ask3dLexer lex;
    unsigned int local_vmap[ASK3D_MAX_LOCAL_VERTICES];
    unsigned int local_vset[ASK3D_MAX_LOCAL_VERTICES];
    unsigned int local_tvmap[ASK3D_MAX_LOCAL_TVERTICES];
    unsigned int local_tvset[ASK3D_MAX_LOCAL_TVERTICES];
    unsigned int channel_tvmap[ASK3D_MAX_LOCAL_TVERTICES];
    unsigned int channel_tvset[ASK3D_MAX_LOCAL_TVERTICES];
    unsigned int local_cvmap[ASK3D_MAX_LOCAL_CVERTICES];
    unsigned int local_cvset[ASK3D_MAX_LOCAL_CVERTICES];
    unsigned int local_fmap[ASK3D_MAX_LOCAL_FACES];
    unsigned int local_fset[ASK3D_MAX_LOCAL_FACES];
    unsigned int local_base_face;
    unsigned int local_face_count;
    int current_object_index;
    int current_uv_channel_index;
    int current_uv_channel_id;
    unsigned int material_count_hint;
} Ask3dWork;

void ask3d_work_clear(Ask3dWork *work);
int ask3d_parse_memory(Ask3dScene *scene, Ask3dWork *work, const char *data, unsigned long size);

#ifdef __cplusplus
}
#endif

#endif
