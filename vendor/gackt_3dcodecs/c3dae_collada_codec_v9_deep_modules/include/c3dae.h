#ifndef C3DAE_H
#define C3DAE_H

/*
   c3dae - no-heap COLLADA/DAE importer
   C89, caller-owned output buffers, no malloc/free/realloc, no float/double arithmetic.

   v5 scope:
   - geometry: mesh/source/vertices/triangles/polylist
   - scene: visual_scene/node, matrix/translate/rotate/scale transforms, instance_geometry/controller
   - skin: controller/skin, joints, inverse bind matrices, vertex_weights
   - morph: controller/morph, morph targets, weights, NORMALIZED/RELATIVE
   - animation: source/sampler/channel, STEP/LINEAR/BEZIER, IN_TANGENT/OUT_TANGENT
   - FX: profile_COMMON constant/lambert/phong/blinn colors, scalar params, diffuse texture link
   - physics: physics_scene, physics_model rigid_body common fields
   - kinematics: kinematics_model joint axis/limits basics
   - metadata: asset unit/up_axis
   - optional ZAE/ZIP loader lives in c3zae.h/c3zae.c
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef C3DAE_ID_MAX
#define C3DAE_ID_MAX 64
#endif

#ifndef C3DAE_PATH_MAX
#define C3DAE_PATH_MAX 160
#endif

#ifndef C3DAE_MAX_SOURCES
#define C3DAE_MAX_SOURCES 128
#endif

#ifndef C3DAE_MAX_VERTEX_BINDINGS
#define C3DAE_MAX_VERTEX_BINDINGS 96
#endif

#ifndef C3DAE_MAX_SOURCE_FLOATS
#define C3DAE_MAX_SOURCE_FLOATS 98304UL
#endif

#ifndef C3DAE_MAX_SOURCE_NAMES
#define C3DAE_MAX_SOURCE_NAMES 4096U
#endif

#ifndef C3DAE_MAX_INPUTS
#define C3DAE_MAX_INPUTS 16
#endif

#ifndef C3DAE_MAX_POLY_VERTS
#define C3DAE_MAX_POLY_VERTS 48
#endif

#ifndef C3DAE_MAX_SKIN_INFLUENCES
#define C3DAE_MAX_SKIN_INFLUENCES 4
#endif

#ifndef C3DAE_MAX_NODE_MATERIAL_BINDINGS
#define C3DAE_MAX_NODE_MATERIAL_BINDINGS 8
#endif

typedef long c3dae_s32;
typedef unsigned long c3dae_u32;
typedef unsigned int c3dae_u16;
typedef c3dae_s32 c3dae_fixed;

#define C3DAE_FIXED_SHIFT 16
#define C3DAE_FIXED_ONE ((c3dae_fixed)65536L)

#define C3DAE_VERTEX_HAS_POS       1U
#define C3DAE_VERTEX_HAS_NORMAL    2U
#define C3DAE_VERTEX_HAS_UV        4U
#define C3DAE_VERTEX_HAS_SKIN      8U

#define C3DAE_UP_UNKNOWN 0
#define C3DAE_UP_X       1
#define C3DAE_UP_Y       2
#define C3DAE_UP_Z       3

#define C3DAE_OK                 0
#define C3DAE_ERR_NULL          -1
#define C3DAE_ERR_CAPACITY      -2
#define C3DAE_ERR_XML           -3
#define C3DAE_ERR_UNSUPPORTED   -4
#define C3DAE_ERR_BAD_NUMBER    -5
#define C3DAE_ERR_NOT_FOUND     -6

#define C3DAE_NODE_UNKNOWN       0
#define C3DAE_NODE_NODE          1
#define C3DAE_NODE_JOINT         2

#define C3DAE_CONTROLLER_UNKNOWN 0
#define C3DAE_CONTROLLER_SKIN    1
#define C3DAE_CONTROLLER_MORPH   2

#define C3DAE_MORPH_NORMALIZED   0
#define C3DAE_MORPH_RELATIVE     1

#define C3DAE_ANIM_TARGET_UNKNOWN   0
#define C3DAE_ANIM_TARGET_MATRIX    1
#define C3DAE_ANIM_TARGET_TRANSLATE 2
#define C3DAE_ANIM_TARGET_ROTATE    3
#define C3DAE_ANIM_TARGET_SCALE     4
#define C3DAE_ANIM_TARGET_SCALAR    5

#define C3DAE_INTERP_LINEAR 0
#define C3DAE_INTERP_STEP   1
#define C3DAE_INTERP_BEZIER 2

#define C3DAE_FX_SHADER_NONE     0
#define C3DAE_FX_SHADER_CONSTANT 1
#define C3DAE_FX_SHADER_LAMBERT  2
#define C3DAE_FX_SHADER_PHONG    3
#define C3DAE_FX_SHADER_BLINN    4

#define C3DAE_EFFECT_HAS_DIFFUSE_TEXTURE 1U
#define C3DAE_EFFECT_HAS_SPECULAR_TEXTURE 2U
#define C3DAE_EFFECT_HAS_EMISSION_TEXTURE 4U
#define C3DAE_EFFECT_HAS_NORMAL_TEXTURE 8U

#define C3DAE_PHYS_SHAPE_UNKNOWN 0
#define C3DAE_PHYS_SHAPE_BOX     1
#define C3DAE_PHYS_SHAPE_SPHERE  2
#define C3DAE_PHYS_SHAPE_PLANE   3
#define C3DAE_PHYS_SHAPE_MESH    4

#define C3DAE_KIN_JOINT_UNKNOWN  0
#define C3DAE_KIN_JOINT_REVOLUTE 1
#define C3DAE_KIN_JOINT_PRISMATIC 2

#define C3DAE_CAMERA_UNKNOWN 0
#define C3DAE_CAMERA_PERSPECTIVE 1
#define C3DAE_CAMERA_ORTHOGRAPHIC 2

#define C3DAE_LIGHT_UNKNOWN 0
#define C3DAE_LIGHT_AMBIENT 1
#define C3DAE_LIGHT_DIRECTIONAL 2
#define C3DAE_LIGHT_POINT 3
#define C3DAE_LIGHT_SPOT 4

typedef struct c3dae_vertex_tag {
    c3dae_fixed x;
    c3dae_fixed y;
    c3dae_fixed z;
    c3dae_fixed nx;
    c3dae_fixed ny;
    c3dae_fixed nz;
    c3dae_fixed u;
    c3dae_fixed v;
    c3dae_u32 source_vertex;
    c3dae_u16 joint[C3DAE_MAX_SKIN_INFLUENCES];
    c3dae_fixed weight[C3DAE_MAX_SKIN_INFLUENCES];
    unsigned int skin_count;
    unsigned int flags;
} c3dae_vertex;

typedef struct c3dae_triangle_tag {
    c3dae_u32 a;
    c3dae_u32 b;
    c3dae_u32 c;
    c3dae_u32 mesh_index;
    int material_index;
    char material_symbol[C3DAE_ID_MAX];
} c3dae_triangle;

typedef struct c3dae_mesh_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    c3dae_u32 first_vertex;
    c3dae_u32 vertex_count;
    c3dae_u32 first_triangle;
    c3dae_u32 triangle_count;
} c3dae_mesh;

typedef struct c3dae_node_tag {
    char id[C3DAE_ID_MAX];
    char sid[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    int parent;
    int type;
    c3dae_fixed local_matrix[16];
    c3dae_fixed base_local_matrix[16];
    c3dae_fixed world_matrix[16];
    char instance_geometry[C3DAE_ID_MAX];
    char instance_controller[C3DAE_ID_MAX];
    char instance_camera[C3DAE_ID_MAX];
    char instance_light[C3DAE_ID_MAX];
    char skeleton[C3DAE_ID_MAX];
    char material_symbol[C3DAE_MAX_NODE_MATERIAL_BINDINGS][C3DAE_ID_MAX];
    char material_target[C3DAE_MAX_NODE_MATERIAL_BINDINGS][C3DAE_ID_MAX];
    int material_index[C3DAE_MAX_NODE_MATERIAL_BINDINGS];
    unsigned int material_bind_count;
    int mesh_index;
    int controller_index;
    int camera_index;
    int light_index;
} c3dae_node;

typedef struct c3dae_skin_joint_tag {
    char sid[C3DAE_ID_MAX];
    c3dae_fixed inv_bind_matrix[16];
    int node_index;
} c3dae_skin_joint;

typedef struct c3dae_skin_vertex_tag {
    c3dae_u16 joint[C3DAE_MAX_SKIN_INFLUENCES];
    c3dae_fixed weight[C3DAE_MAX_SKIN_INFLUENCES];
    unsigned int count;
} c3dae_skin_vertex;

typedef struct c3dae_morph_target_tag {
    char target[C3DAE_ID_MAX];
    int mesh_index;
    c3dae_fixed weight;
} c3dae_morph_target;

typedef struct c3dae_controller_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    int kind;

    char skin_source[C3DAE_ID_MAX];
    int source_mesh_index;
    c3dae_fixed bind_shape_matrix[16];
    c3dae_u32 first_joint;
    c3dae_u32 joint_count;
    c3dae_u32 first_vertex_weight;
    c3dae_u32 vertex_weight_count;

    char morph_source[C3DAE_ID_MAX];
    int morph_source_mesh_index;
    int morph_method;
    c3dae_u32 first_morph_target;
    c3dae_u32 morph_target_count;
} c3dae_controller;

typedef struct c3dae_anim_key_tag {
    c3dae_fixed time;
    c3dae_fixed value[16];
    c3dae_fixed in_tangent[16];
    c3dae_fixed out_tangent[16];
    c3dae_fixed in_tangent_time;
    c3dae_fixed out_tangent_time;
    unsigned int value_count;
    unsigned int tangent_flags;
} c3dae_anim_key;

typedef struct c3dae_anim_channel_tag {
    char id[C3DAE_ID_MAX];
    char target_node[C3DAE_ID_MAX];
    char target_sid[C3DAE_ID_MAX];
    int target_node_index;
    int target_kind;
    int interpolation;
    c3dae_u32 first_key;
    c3dae_u32 key_count;
} c3dae_anim_channel;

typedef struct c3dae_image_tag {
    char id[C3DAE_ID_MAX];
    char path[C3DAE_PATH_MAX];
} c3dae_image;

typedef struct c3dae_effect_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    int shader_model;
    unsigned int flags;
    c3dae_fixed emission[4];
    c3dae_fixed ambient[4];
    c3dae_fixed diffuse[4];
    c3dae_fixed specular[4];
    c3dae_fixed reflective[4];
    c3dae_fixed transparent[4];
    c3dae_fixed shininess;
    c3dae_fixed transparency;
    c3dae_fixed index_of_refraction;
    char diffuse_texture[C3DAE_ID_MAX];
    char diffuse_image[C3DAE_ID_MAX];
    char specular_texture[C3DAE_ID_MAX];
    char specular_image[C3DAE_ID_MAX];
    char emission_texture[C3DAE_ID_MAX];
    char emission_image[C3DAE_ID_MAX];
    char normal_texture[C3DAE_ID_MAX];
    char normal_image[C3DAE_ID_MAX];
} c3dae_effect;

typedef struct c3dae_camera_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    int kind;
    c3dae_fixed xfov;
    c3dae_fixed yfov;
    c3dae_fixed xmag;
    c3dae_fixed ymag;
    c3dae_fixed aspect_ratio;
    c3dae_fixed znear;
    c3dae_fixed zfar;
} c3dae_camera;

typedef struct c3dae_light_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    int kind;
    c3dae_fixed color[3];
    c3dae_fixed constant_attenuation;
    c3dae_fixed linear_attenuation;
    c3dae_fixed quadratic_attenuation;
    c3dae_fixed falloff_angle;
    c3dae_fixed falloff_exponent;
} c3dae_light;

typedef struct c3dae_material_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    char effect[C3DAE_ID_MAX];
    int effect_index;
    c3dae_fixed diffuse[4];
} c3dae_material;

typedef struct c3dae_physics_scene_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    c3dae_fixed gravity[3];
    c3dae_fixed time_step;
} c3dae_physics_scene;

typedef struct c3dae_rigid_body_tag {
    char sid[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    char target[C3DAE_ID_MAX];
    int dynamic;
    c3dae_fixed mass;
    c3dae_fixed inertia[3];
    c3dae_fixed velocity[3];
    c3dae_fixed angular_velocity[3];
    int shape_kind;
    c3dae_fixed shape_size[3];
} c3dae_rigid_body;

typedef struct c3dae_physics_model_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    c3dae_u32 first_rigid_body;
    c3dae_u32 rigid_body_count;
} c3dae_physics_model;

typedef struct c3dae_kinematic_joint_tag {
    char sid[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    int type;
    c3dae_fixed axis[3];
    c3dae_fixed limit_min;
    c3dae_fixed limit_max;
} c3dae_kinematic_joint;

typedef struct c3dae_kinematic_model_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    c3dae_u32 first_joint;
    c3dae_u32 joint_count;
} c3dae_kinematic_model;

typedef struct c3dae_model_tag {
    c3dae_vertex *vertices;
    c3dae_u32 vertex_cap;
    c3dae_u32 vertex_count;

    c3dae_triangle *triangles;
    c3dae_u32 triangle_cap;
    c3dae_u32 triangle_count;

    c3dae_mesh *meshes;
    c3dae_u32 mesh_cap;
    c3dae_u32 mesh_count;

    c3dae_node *nodes;
    c3dae_u32 node_cap;
    c3dae_u32 node_count;

    c3dae_controller *controllers;
    c3dae_u32 controller_cap;
    c3dae_u32 controller_count;

    c3dae_skin_joint *skin_joints;
    c3dae_u32 skin_joint_cap;
    c3dae_u32 skin_joint_count;

    c3dae_skin_vertex *skin_vertices;
    c3dae_u32 skin_vertex_cap;
    c3dae_u32 skin_vertex_count;

    c3dae_morph_target *morph_targets;
    c3dae_u32 morph_target_cap;
    c3dae_u32 morph_target_count;

    c3dae_anim_channel *anim_channels;
    c3dae_u32 anim_channel_cap;
    c3dae_u32 anim_channel_count;

    c3dae_anim_key *anim_keys;
    c3dae_u32 anim_key_cap;
    c3dae_u32 anim_key_count;

    c3dae_image *images;
    c3dae_u32 image_cap;
    c3dae_u32 image_count;

    c3dae_material *materials;
    c3dae_u32 material_cap;
    c3dae_u32 material_count;

    c3dae_effect *effects;
    c3dae_u32 effect_cap;
    c3dae_u32 effect_count;

    c3dae_camera *cameras;
    c3dae_u32 camera_cap;
    c3dae_u32 camera_count;

    c3dae_light *lights;
    c3dae_u32 light_cap;
    c3dae_u32 light_count;

    c3dae_physics_scene *physics_scenes;
    c3dae_u32 physics_scene_cap;
    c3dae_u32 physics_scene_count;

    c3dae_physics_model *physics_models;
    c3dae_u32 physics_model_cap;
    c3dae_u32 physics_model_count;

    c3dae_rigid_body *rigid_bodies;
    c3dae_u32 rigid_body_cap;
    c3dae_u32 rigid_body_count;

    c3dae_kinematic_model *kinematic_models;
    c3dae_u32 kinematic_model_cap;
    c3dae_u32 kinematic_model_count;

    c3dae_kinematic_joint *kinematic_joints;
    c3dae_u32 kinematic_joint_cap;
    c3dae_u32 kinematic_joint_count;

    c3dae_fixed unit_meter;
    int up_axis;
} c3dae_model;

typedef struct c3dae_source_tag {
    char id[C3DAE_ID_MAX];
    c3dae_u32 first;
    c3dae_u32 item_count;
    c3dae_u32 raw_count;
    c3dae_u32 stride;
    c3dae_u32 offset;
    c3dae_u32 name_first;
    c3dae_u32 name_count;
    int is_name_array;
} c3dae_source;

typedef struct c3dae_vertex_binding_tag {
    char id[C3DAE_ID_MAX];
    int position_source;
} c3dae_vertex_binding;

typedef struct c3dae_context_tag {
    c3dae_fixed source_data[C3DAE_MAX_SOURCE_FLOATS];
    c3dae_u32 source_data_count;

    char source_names[C3DAE_MAX_SOURCE_NAMES][C3DAE_ID_MAX];
    c3dae_u32 source_name_count;

    c3dae_source sources[C3DAE_MAX_SOURCES];
    unsigned int source_count;

    c3dae_vertex_binding vbindings[C3DAE_MAX_VERTEX_BINDINGS];
    unsigned int vbinding_count;

    int error_code;
    c3dae_u32 error_line;
    char error_message[128];
} c3dae_context;

void c3dae_context_init(c3dae_context *ctx);
void c3dae_model_init(c3dae_model *m,
                      c3dae_vertex *verts,
                      c3dae_u32 vert_cap,
                      c3dae_triangle *tris,
                      c3dae_u32 tri_cap,
                      c3dae_mesh *meshes,
                      c3dae_u32 mesh_cap);

void c3dae_model_bind_nodes(c3dae_model *m, c3dae_node *nodes, c3dae_u32 cap);
void c3dae_model_bind_skin(c3dae_model *m,
                           c3dae_controller *controllers,
                           c3dae_u32 controller_cap,
                           c3dae_skin_joint *joints,
                           c3dae_u32 joint_cap,
                           c3dae_skin_vertex *skin_vertices,
                           c3dae_u32 skin_vertex_cap);
void c3dae_model_bind_morphs(c3dae_model *m,
                             c3dae_morph_target *targets,
                             c3dae_u32 target_cap);
void c3dae_model_bind_animations(c3dae_model *m,
                                 c3dae_anim_channel *channels,
                                 c3dae_u32 channel_cap,
                                 c3dae_anim_key *keys,
                                 c3dae_u32 key_cap);
void c3dae_model_bind_materials(c3dae_model *m,
                                c3dae_material *materials,
                                c3dae_u32 material_cap,
                                c3dae_image *images,
                                c3dae_u32 image_cap);
void c3dae_model_bind_effects(c3dae_model *m,
                              c3dae_effect *effects,
                              c3dae_u32 effect_cap);
void c3dae_model_bind_cameras_lights(c3dae_model *m,
                                     c3dae_camera *cameras,
                                     c3dae_u32 camera_cap,
                                     c3dae_light *lights,
                                     c3dae_u32 light_cap);
void c3dae_model_bind_physics(c3dae_model *m,
                              c3dae_physics_scene *scenes,
                              c3dae_u32 scene_cap,
                              c3dae_physics_model *models,
                              c3dae_u32 model_cap,
                              c3dae_rigid_body *bodies,
                              c3dae_u32 body_cap);
void c3dae_model_bind_kinematics(c3dae_model *m,
                                 c3dae_kinematic_model *models,
                                 c3dae_u32 model_cap,
                                 c3dae_kinematic_joint *joints,
                                 c3dae_u32 joint_cap);

int c3dae_load_memory(c3dae_context *ctx,
                      const char *xml,
                      c3dae_u32 xml_len,
                      c3dae_model *out_model);

int c3dae_animation_eval_channel(const c3dae_model *model,
                                 c3dae_u32 channel_index,
                                 c3dae_fixed time,
                                 c3dae_fixed *out_values,
                                 unsigned int out_cap,
                                 unsigned int *out_count);

void c3dae_build_node_world_matrices(c3dae_model *model);
int c3dae_apply_animations(c3dae_model *model, c3dae_fixed time);
int c3dae_build_skin_palette(const c3dae_model *model,
                             c3dae_u32 controller_index,
                             c3dae_fixed *out_matrices,
                             c3dae_u32 matrix_cap);
void c3dae_resolve_triangle_materials(c3dae_model *model);
int c3dae_node_resolve_triangle_material(const c3dae_model *model,
                                         c3dae_u32 node_index,
                                         c3dae_u32 triangle_index);

void c3dae_matrix_from_rotate(c3dae_fixed *m,
                              c3dae_fixed x,
                              c3dae_fixed y,
                              c3dae_fixed z,
                              c3dae_fixed degrees);
c3dae_fixed c3dae_fixed_sin_deg(c3dae_fixed degrees);
c3dae_fixed c3dae_fixed_cos_deg(c3dae_fixed degrees);

c3dae_fixed c3dae_fixed_from_int(c3dae_s32 v);
c3dae_s32 c3dae_fixed_to_int(c3dae_fixed v);
c3dae_s32 c3dae_fixed_to_milli(c3dae_fixed v);
const char *c3dae_error_string(int code);

#ifdef __cplusplus
}
#endif

#endif
