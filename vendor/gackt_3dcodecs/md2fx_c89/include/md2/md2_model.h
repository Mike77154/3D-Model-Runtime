#ifndef MD2_MODEL_H
#define MD2_MODEL_H

#include "md2_config.h"
#include "md2_fixed.h"
#include "md2_types.h"

#define MD2_IDENT   0x32504449U
#define MD2_VERSION 8

typedef struct md2_texcoord_s
{
    md2_i16 s;
    md2_i16 t;
} md2_texcoord_t;

typedef struct md2_triangle_s
{
    md2_u16 index_xyz[3];
    md2_u16 index_st[3];
} md2_triangle_t;

typedef struct md2_packed_vertex_s
{
    md2_u8 v[3];
    md2_u8 lightnormalindex;
} md2_packed_vertex_t;

typedef struct md2_frame_s
{
    md2_fx32_t scale[3];
    md2_fx32_t translate[3];
    char name[MD2_FRAME_NAME_LEN];
    md2_packed_vertex_t verts[MD2_MAX_VERTS];
} md2_frame_t;

typedef struct md2_model_s
{
    md2_i32 skinwidth;
    md2_i32 skinheight;
    md2_i32 num_skins;
    md2_i32 num_xyz;
    md2_i32 num_st;
    md2_i32 num_tris;
    md2_i32 num_glcmds;
    md2_i32 num_frames;

    char skins[MD2_MAX_SKINS][MD2_MAX_SKINNAME];
    md2_texcoord_t texcoords[MD2_MAX_TEXCOORDS];
    md2_triangle_t triangles[MD2_MAX_TRIANGLES];
    md2_frame_t frames[MD2_MAX_FRAMES];
    md2_i32 glcmds[MD2_MAX_GLCMDS];
} md2_model_t;

void md2_model_init(md2_model_t *model);
void md2_frame_clear(md2_frame_t *frame);
void md2_copy_name16(char dst[MD2_FRAME_NAME_LEN], const char *src);
void md2_copy_skin64(char dst[MD2_MAX_SKINNAME], const char *src);

int md2_frame_unpack_positions(const md2_frame_t *frame,
                               md2_i32 num_xyz,
                               md2_fx_vec3_t *out_positions);

int md2_frame_pack_positions(md2_frame_t *frame,
                             md2_i32 num_xyz,
                             const md2_fx_vec3_t *positions,
                             const md2_u8 *normal_indices,
                             const char *frame_name);

#endif /* MD2_MODEL_H */
