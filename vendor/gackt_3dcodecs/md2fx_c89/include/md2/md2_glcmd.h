#ifndef MD2_GLCMD_H
#define MD2_GLCMD_H

#include "md2_model.h"

#define MD2_GLCMD_STRIP 1
#define MD2_GLCMD_FAN   2

typedef struct md2_glcmd_vertex_s
{
    md2_fx32_t s;
    md2_fx32_t t;
    md2_i32 vertex_index;
} md2_glcmd_vertex_t;

typedef struct md2_glcmd_cmd_s
{
    md2_i32 primitive;
    md2_i32 vertex_count;
} md2_glcmd_cmd_t;

typedef struct md2_glcmd_iter_s
{
    const md2_model_t *model;
    md2_i32 pos;
    md2_i32 remaining;
    md2_i32 primitive;
} md2_glcmd_iter_t;

typedef struct md2_glcmd_builder_s
{
    md2_model_t *model;
    md2_i32 pos;
    md2_i32 remaining;
    md2_i32 open_primitive;
} md2_glcmd_builder_t;

void md2_glcmd_iter_init(md2_glcmd_iter_t *it, const md2_model_t *model);
int md2_glcmd_iter_next(md2_glcmd_iter_t *it, md2_glcmd_cmd_t *out_cmd);
int md2_glcmd_iter_next_vertex(md2_glcmd_iter_t *it, md2_glcmd_vertex_t *out_vtx);

void md2_glcmd_builder_init(md2_glcmd_builder_t *builder, md2_model_t *model);
int md2_glcmd_builder_begin(md2_glcmd_builder_t *builder,
                            md2_i32 primitive,
                            md2_i32 vertex_count);
int md2_glcmd_builder_vertex(md2_glcmd_builder_t *builder,
                             md2_fx32_t s,
                             md2_fx32_t t,
                             md2_i32 vertex_index);
int md2_glcmd_builder_end(md2_glcmd_builder_t *builder);

#endif /* MD2_GLCMD_H */
