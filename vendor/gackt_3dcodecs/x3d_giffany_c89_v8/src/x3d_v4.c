#include "x3d_codec.h"

static int xv4_type_is_joint(int t)
{
    return t == X3D_NODE_BALL_JOINT || t == X3D_NODE_SINGLE_AXIS_HINGE_JOINT ||
        t == X3D_NODE_DOUBLE_AXIS_HINGE_JOINT || t == X3D_NODE_SLIDER_JOINT ||
        t == X3D_NODE_UNIVERSAL_JOINT || t == X3D_NODE_MOTOR_JOINT;
}

void x3d_physics_collect_summary(const x3d_scene *scene, x3d_physics_summary *summary)
{
    int i;
    int t;
    if (!summary) return;
    summary->collections = 0;
    summary->bodies = 0;
    summary->collidables = 0;
    summary->collision_collections = 0;
    summary->collision_spaces = 0;
    summary->contacts = 0;
    summary->joints = 0;
    if (!scene) return;
    for (i = 0; i < scene->node_count; ++i) {
        t = scene->nodes[i].type;
        if (t == X3D_NODE_RIGID_BODY_COLLECTION) summary->collections = summary->collections + 1;
        if (t == X3D_NODE_RIGID_BODY) summary->bodies = summary->bodies + 1;
        if (t == X3D_NODE_COLLIDABLE_SHAPE || t == X3D_NODE_COLLIDABLE_OFFSET) summary->collidables = summary->collidables + 1;
        if (t == X3D_NODE_COLLISION_COLLECTION) summary->collision_collections = summary->collision_collections + 1;
        if (t == X3D_NODE_COLLISION_SPACE) summary->collision_spaces = summary->collision_spaces + 1;
        if (t == X3D_NODE_CONTACT) summary->contacts = summary->contacts + 1;
        if (xv4_type_is_joint(t)) summary->joints = summary->joints + 1;
    }
}

void x3d_shader_collect_summary(const x3d_scene *scene, x3d_shader_summary *summary)
{
    int i;
    int t;
    if (!summary) return;
    summary->composed_shaders = 0;
    summary->shader_parts = 0;
    summary->shader_programs = 0;
    summary->vertex_attributes = 0;
    summary->inline_sources = 0;
    summary->urls = 0;
    if (!scene) return;
    for (i = 0; i < scene->node_count; ++i) {
        t = scene->nodes[i].type;
        if (t == X3D_NODE_COMPOSED_SHADER) summary->composed_shaders = summary->composed_shaders + 1;
        if (t == X3D_NODE_SHADER_PART) summary->shader_parts = summary->shader_parts + 1;
        if (t == X3D_NODE_SHADER_PROGRAM || t == X3D_NODE_PROGRAM_SHADER || t == X3D_NODE_PACKAGED_SHADER) summary->shader_programs = summary->shader_programs + 1;
        if (t == X3D_NODE_FLOAT_VERTEX_ATTRIBUTE || t == X3D_NODE_MATRIX3_VERTEX_ATTRIBUTE || t == X3D_NODE_MATRIX4_VERTEX_ATTRIBUTE) summary->vertex_attributes = summary->vertex_attributes + 1;
        if (scene->shader_nodes[i].source[0]) summary->inline_sources = summary->inline_sources + 1;
        if (scene->shader_nodes[i].url[0]) summary->urls = summary->urls + 1;
    }
}

int x3d_resolve_route(const x3d_scene *scene, int route_index, int *from_node_index, int *to_node_index)
{
    int f;
    int t;
    if (!scene || route_index < 0 || route_index >= scene->route_count) return X3D_STATUS_BAD_ARG;
    f = x3d_find_def(scene, scene->routes[route_index].from_node);
    t = x3d_find_def(scene, scene->routes[route_index].to_node);
    if (from_node_index) *from_node_index = f;
    if (to_node_index) *to_node_index = t;
    if (f == X3D_NONE || t == X3D_NONE) return X3D_STATUS_WARNINGS;
    return X3D_STATUS_OK;
}

int x3d_resolve_proto_field(const x3d_scene *scene, int connect_node, int *proto_decl_node)
{
    int n;
    if (!scene || connect_node < 0 || connect_node >= scene->node_count) return X3D_STATUS_BAD_ARG;
    if (scene->nodes[connect_node].type != X3D_NODE_CONNECT) return X3D_STATUS_BAD_ARG;
    n = scene->nodes[connect_node].parent;
    while (n != X3D_NONE) {
        if (scene->nodes[n].type == X3D_NODE_PROTO_DECLARE || scene->nodes[n].type == X3D_NODE_EXTERN_PROTO_DECLARE) {
            if (proto_decl_node) *proto_decl_node = n;
            return X3D_STATUS_OK;
        }
        n = scene->nodes[n].parent;
    }
    if (proto_decl_node) *proto_decl_node = X3D_NONE;
    return X3D_STATUS_WARNINGS;
}

int x3d_eval_boolean_filter(int input_true, int input_false, int *input_negate, int *output_true, int *output_false)
{
    int v;
    if (!input_negate || !output_true || !output_false) return X3D_STATUS_BAD_ARG;
    v = input_true ? X3D_TRUE : X3D_FALSE;
    if (input_false) v = X3D_FALSE;
    *input_negate = v ? X3D_FALSE : X3D_TRUE;
    *output_true = v ? X3D_TRUE : X3D_FALSE;
    *output_false = v ? X3D_FALSE : X3D_TRUE;
    return X3D_STATUS_OK;
}
