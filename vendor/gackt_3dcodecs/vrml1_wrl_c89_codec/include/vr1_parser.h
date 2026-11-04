#ifndef VR1_PARSER_H
#define VR1_PARSER_H

#include "vr1_scene.h"
#include "vr1_lexer.h"

enum {
    VR1_BIND_DEFAULT = 0,
    VR1_BIND_OVERALL,
    VR1_BIND_PER_PART,
    VR1_BIND_PER_PART_INDEXED,
    VR1_BIND_PER_FACE,
    VR1_BIND_PER_FACE_INDEXED,
    VR1_BIND_PER_VERTEX,
    VR1_BIND_PER_VERTEX_INDEXED
};

enum {
    VR1_ORDER_UNKNOWN = 0,
    VR1_ORDER_CLOCKWISE,
    VR1_ORDER_COUNTERCLOCKWISE
};

enum {
    VR1_SHAPE_UNKNOWN = 0,
    VR1_SHAPE_SOLID
};

enum {
    VR1_FACE_UNKNOWN = 0,
    VR1_FACE_CONVEX
};

typedef struct vr1_state_s {
    vr1_vec3 coords[VR1_MAX_STATE_POINTS];
    vr1_vec3 normals[VR1_MAX_STATE_NORMALS];
    vr1_vec2 texcoords[VR1_MAX_STATE_TEXCOORDS];
    vr1_material mats[VR1_MAX_STATE_MATERIALS];
    int coord_count;
    int normal_count;
    int texcoord_count;
    int mat_count;
    int material_binding;
    int normal_binding;
    int texcoord_binding;
    int vertex_ordering;
    int shape_type;
    int face_type;
    vr1_fx crease_angle;
    vr1_vec2 tex_translation;
    vr1_vec2 tex_scale;
    vr1_vec2 tex_center;
    vr1_fx tex_rotation;
    vr1_fx tex_matrix[9];
    vr1_fx font_size;
    int font_family;
    int font_style;
    vr1_fx matrix[16];
} vr1_state;

typedef struct vr1_def_s {
    char name[VR1_NAME_MAX];
    long start;
    long end;
    int valid;
} vr1_def;

typedef struct vr1_parser_context_s {
    vr1_lexer lx;
    vr1_scene *scene;
    const char *source;
    long source_len;
    vr1_state state;
    vr1_state stack[VR1_MAX_STACK_DEPTH];
    vr1_fx matrix_stack[VR1_MAX_STACK_DEPTH][16];
    int stack_top;
    int matrix_stack_top;
    vr1_def defs[VR1_MAX_DEFS];
    int def_count;
    int replay_depth;
    int emit_enabled;
    long coord_index[VR1_MAX_FIELD_INDICES];
    long material_index[VR1_MAX_FIELD_INDICES];
    long normal_index[VR1_MAX_FIELD_INDICES];
    long texcoord_index[VR1_MAX_FIELD_INDICES];
    long coord_index_count;
    long material_index_count;
    long normal_index_count;
    long texcoord_index_count;
    vr1_vec3 gen_normals[VR1_MAX_STATE_POINTS];
    unsigned char gen_normal_valid[VR1_MAX_STATE_POINTS];
    int gen_normals_ready;
} vr1_parser_context;

void vr1_parser_context_init(vr1_parser_context *p);
int  vr1_parse_buffer(vr1_parser_context *p, vr1_scene *scene, const char *buf, long len);
const char *vr1_result_name(int code);

#endif
