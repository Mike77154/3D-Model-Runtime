#ifndef ASE3D_RENDER_H
#define ASE3D_RENDER_H

#include "ase3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Optional fixed-point software renderer + material/shader-tree bridge.
   Caller owns all buffers. No malloc, no free, no hidden heap. */

#define ASE3D_SHADER_WARN_NODE_OVERFLOW 0x00000001UL
#define ASE3D_SHADER_WARN_BAD_MATERIAL  0x00000002UL

#define ASE3D_SHADER_NODE_ROOT        1
#define ASE3D_SHADER_NODE_MATERIAL    2
#define ASE3D_SHADER_NODE_SUBMATERIAL 3
#define ASE3D_SHADER_NODE_COLOR       4
#define ASE3D_SHADER_NODE_MAP         5
#define ASE3D_SHADER_NODE_OPACITY     6
#define ASE3D_SHADER_NODE_BUMP        7
#define ASE3D_SHADER_NODE_SPECULAR    8
#define ASE3D_SHADER_NODE_REFLECTION  9
#define ASE3D_SHADER_NODE_REFRACTION  10
#define ASE3D_SHADER_NODE_SELFILLUM   11

#define ASE3D_SHADER_SLOT_ROOT      0
#define ASE3D_SHADER_SLOT_AMBIENT   1
#define ASE3D_SHADER_SLOT_DIFFUSE   2
#define ASE3D_SHADER_SLOT_SPECULAR  3
#define ASE3D_SHADER_SLOT_OPACITY   4
#define ASE3D_SHADER_SLOT_BUMP      5
#define ASE3D_SHADER_SLOT_REFLECT   6
#define ASE3D_SHADER_SLOT_REFRACT   7
#define ASE3D_SHADER_SLOT_SELFILLUM 8

#define ASE3D_RENDER_FLAG_TEXTURES  0x00000001UL
#define ASE3D_RENDER_FLAG_LIGHTING   0x00000002UL
#define ASE3D_RENDER_FLAG_CULL_BACK  0x00000004UL
#define ASE3D_RENDER_FLAG_ZWRITE     0x00000008UL

typedef struct ase3d_shader_node_s {
    int type;
    int parent;
    int material_index;
    int map_index;
    int slot;
    ase3d_rgb color;
    ase3d_fixed value;
    ase3d_fixed amount;
    char name[ASE3D_NAME_MAX];
    char bitmap[ASE3D_PATH_MAX];
} ase3d_shader_node;

typedef struct ase3d_shader_tree_s {
    ase3d_shader_node *nodes;
    unsigned int node_capacity;
    unsigned int node_count;
    unsigned long warnings;
} ase3d_shader_tree;

typedef ase3d_u32 (*ase3d_sample_texture_fn)(const char *bitmap, ase3d_fixed u, ase3d_fixed v, void *user);

typedef struct ase3d_shader_eval_s {
    const ase3d_model *model;
    const ase3d_shader_tree *tree;
    ase3d_sample_texture_fn sample_texture;
    void *texture_user;
    unsigned long flags;
} ase3d_shader_eval;

typedef struct ase3d_framebuffer_s {
    ase3d_u32 *color;
    ase3d_fixed *depth;
    int width;
    int height;
    ase3d_u32 clear_color;
    ase3d_fixed clear_depth;
} ase3d_framebuffer;

typedef struct ase3d_screen_vertex_s {
    ase3d_fixed x;
    ase3d_fixed y;
    ase3d_fixed z;
    ase3d_fixed u;
    ase3d_fixed v;
    ase3d_u32 color;
} ase3d_screen_vertex;

typedef struct ase3d_ortho_camera_s {
    ase3d_fixed center_x;
    ase3d_fixed center_y;
    ase3d_fixed scale_x;
    ase3d_fixed scale_y;
    long tick;
    int use_world_transform;
} ase3d_ortho_camera;

void ase3d_shader_tree_zero(ase3d_shader_tree *tree);
void ase3d_shader_node_zero(ase3d_shader_node *node);
int ase3d_shader_tree_build(ase3d_shader_tree *tree, const ase3d_model *model);
int ase3d_shader_tree_find_material_root(const ase3d_shader_tree *tree, int material_index);
int ase3d_resolve_face_material(const ase3d_model *model, const ase3d_face *face);
ase3d_u32 ase3d_shader_eval_material(const ase3d_shader_eval *eval, int material_index, ase3d_fixed u, ase3d_fixed v, ase3d_rgb vertex_color);

void ase3d_render_clear(ase3d_framebuffer *fb);
void ase3d_render_triangle(ase3d_framebuffer *fb, const ase3d_shader_eval *eval, int material_index, ase3d_screen_vertex a, ase3d_screen_vertex b, ase3d_screen_vertex c);
int ase3d_render_model_ortho(ase3d_framebuffer *fb, const ase3d_shader_eval *eval, const ase3d_model *model, const ase3d_ortho_camera *cam);

ase3d_u32 ase3d_pack_rgba_fixed(ase3d_fixed r, ase3d_fixed g, ase3d_fixed b, ase3d_fixed a);
void ase3d_unpack_rgba_fixed(ase3d_u32 c, ase3d_fixed *r, ase3d_fixed *g, ase3d_fixed *b, ase3d_fixed *a);

#ifdef __cplusplus
}
#endif

#endif
