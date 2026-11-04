#ifndef P3_EGG_MODEL_H
#define P3_EGG_MODEL_H

#include "p3_config.h"
#include "p3_fixed.h"
#include "p3_egg_ast.h"
#include "p3_string_pool.h"

typedef enum p3_egg_coord_system {
    P3_EGG_COORD_UNSPECIFIED = 0,
    P3_EGG_COORD_Y_UP = 1,
    P3_EGG_COORD_Z_UP = 2,
    P3_EGG_COORD_Y_UP_LEFT = 3,
    P3_EGG_COORD_Z_UP_LEFT = 4
} p3_egg_coord_system;

typedef struct p3_egg_texture_ref {
    p3_u16 name_off;
    p3_u16 name_len;
} p3_egg_texture_ref;

typedef struct p3_egg_material_ref {
    p3_u16 name_off;
    p3_u16 name_len;
} p3_egg_material_ref;

typedef struct p3_egg_vertex {
    p3_u16 id;
    p3_fixed pos[4];
    p3_u8 pos_count;
    p3_fixed uv[2];
    p3_u8 has_uv;
    p3_fixed normal[3];
    p3_u8 has_normal;
    p3_fixed rgba[4];
    p3_u8 has_rgba;
} p3_egg_vertex;

typedef struct p3_egg_vertex_pool {
    p3_u16 name_off;
    p3_u16 name_len;
    p3_u16 first_vertex;
    p3_u16 vertex_count;
} p3_egg_vertex_pool;

typedef struct p3_egg_polygon {
    p3_u16 first_index;
    p3_u16 index_count;
    p3_u16 tref_off;
    p3_u16 tref_len;
    p3_u16 mref_off;
    p3_u16 mref_len;
    p3_fixed normal[3];
    p3_u8 has_normal;
    p3_fixed rgba[4];
    p3_u8 has_rgba;
} p3_egg_polygon;

typedef struct p3_egg_group {
    p3_u16 name_off;
    p3_u16 name_len;
} p3_egg_group;

typedef struct p3_egg_joint {
    p3_u16 name_off;
    p3_u16 name_len;
} p3_egg_joint;

typedef struct p3_egg_model {
    p3_string_pool pool;
    p3_u8 coord_system;

    p3_egg_texture_ref textures[P3_EGG_MODEL_MAX_TEXTURES];
    p3_u16 texture_count;

    p3_egg_material_ref materials[P3_EGG_MODEL_MAX_MATERIALS];
    p3_u16 material_count;

    p3_egg_vertex_pool vertex_pools[P3_EGG_MODEL_MAX_VERTEX_POOLS];
    p3_u16 vertex_pool_count;

    p3_egg_vertex vertices[P3_EGG_MODEL_MAX_VERTICES];
    p3_u16 vertex_count;

    p3_egg_polygon polygons[P3_EGG_MODEL_MAX_POLYGONS];
    p3_u16 polygon_count;

    p3_u16 polygon_indices[P3_EGG_MODEL_MAX_POLY_INDICES];
    p3_u16 polygon_index_count;

    p3_egg_group groups[P3_EGG_MODEL_MAX_GROUPS];
    p3_u16 group_count;

    p3_egg_joint joints[P3_EGG_MODEL_MAX_JOINTS];
    p3_u16 joint_count;
} p3_egg_model;

void p3_egg_model_init(p3_egg_model *model);
int p3_egg_model_from_ast(p3_egg_model *model, const p3_egg_ast *ast, char *err, p3_u32 err_cap);
int p3_egg_model_summary(const p3_egg_model *model, char *dst, p3_u32 cap);

#endif
