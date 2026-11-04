#ifndef GIF_OGEX_FIXED_H
#define GIF_OGEX_FIXED_H

/*
   gif_ogex_fixed.h
   Small OpenGEX / OGEX decoder3d core.

   Goals:
   - C89-compatible public API.
   - No malloc/free/realloc/heaps in the decoder.
   - No float/double arithmetic in the decoder.
   - Q16.16 fixed point for numeric values originally stored as OpenDDL floats.

   This is a practical mesh/scene extractor, not a full validating OpenGEX
   reference implementation. Unknown structures are skipped by design, as
   OpenDDL readers are expected to ignore unknown extensions.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OGX_MAX_NAME
#define OGX_MAX_NAME 64
#endif

#ifndef OGX_MAX_VERTICES
#define OGX_MAX_VERTICES 4096
#endif

#ifndef OGX_MAX_INDICES
#define OGX_MAX_INDICES 12288
#endif

#ifndef OGX_MAX_MESHES
#define OGX_MAX_MESHES 64
#endif

#ifndef OGX_MAX_OBJECTS
#define OGX_MAX_OBJECTS 64
#endif

#ifndef OGX_MAX_NODES
#define OGX_MAX_NODES 128
#endif

#ifndef OGX_MAX_MATERIALS
#define OGX_MAX_MATERIALS 64
#endif

#ifndef OGX_MAX_NODE_MATERIAL_REFS
#define OGX_MAX_NODE_MATERIAL_REFS 8
#endif

#ifndef OGX_MAX_ANIM_TRACKS
#define OGX_MAX_ANIM_TRACKS 64
#endif

#ifndef OGX_MAX_KEYS
#define OGX_MAX_KEYS 512
#endif

#ifndef OGX_MAX_SKIN_WEIGHTS
#define OGX_MAX_SKIN_WEIGHTS 4096
#endif

#ifndef OGX_MAX_BONES
#define OGX_MAX_BONES 256
#endif

#ifndef OGX_MAX_TEXTURES
#define OGX_MAX_TEXTURES 128
#endif

#ifndef OGX_MAX_TEXTURE_PATH
#define OGX_MAX_TEXTURE_PATH 128
#endif

#ifndef OGX_MAX_MORPH_WEIGHTS
#define OGX_MAX_MORPH_WEIGHTS 256
#endif

#ifndef OGX_MAX_LIGHTS
#define OGX_MAX_LIGHTS 32
#endif

#ifndef OGX_MAX_CAMERAS
#define OGX_MAX_CAMERAS 32
#endif

#ifndef OGX_MAX_EXTRA_PARAMS
#define OGX_MAX_EXTRA_PARAMS 128
#endif

#ifndef OGX_MAX_TRIANGLES_OUT
#define OGX_MAX_TRIANGLES_OUT 16384
#endif

#ifndef OGX_MAX_MORPH_TARGETS
#define OGX_MAX_MORPH_TARGETS 128
#endif

#ifndef OGX_MAX_MORPH_VERTICES
#define OGX_MAX_MORPH_VERTICES 16384
#endif

#define OGX_FIXED_SHIFT 16
#define OGX_FIXED_ONE   65536L
#define OGX_FIXED_HALF  32768L
#define OGX_FIXED_PI    205887L
#define OGX_FIXED_2PI   411775L
#define OGX_FIXED_PI_2  102943L

#define OGX_ATTR_POSITION 1u
#define OGX_ATTR_NORMAL   2u
#define OGX_ATTR_TEXCOORD 4u
#define OGX_ATTR_COLOR    8u

#define OGX_OK                         0
#define OGX_ERR_BAD_ARGUMENT          -1
#define OGX_ERR_SYNTAX                -2
#define OGX_ERR_CAPACITY_VERTICES     -3
#define OGX_ERR_CAPACITY_INDICES      -4
#define OGX_ERR_CAPACITY_MESHES       -5
#define OGX_ERR_CAPACITY_OBJECTS      -6
#define OGX_ERR_CAPACITY_NODES        -7
#define OGX_ERR_CAPACITY_MATERIALS    -8
#define OGX_ERR_CAPACITY_ANIMS        -9
#define OGX_ERR_CAPACITY_KEYS         -10
#define OGX_ERR_CAPACITY_SKIN         -11
#define OGX_ERR_UNSUPPORTED_DATA      -12

typedef long ogx_fixed;

typedef struct ogx_vertex_s
{
    ogx_fixed pos[3];
    ogx_fixed normal[3];
    ogx_fixed texcoord[2];
    ogx_fixed color[4];
    unsigned int flags;
} ogx_vertex;

typedef enum ogx_object_kind_e
{
    OGX_OBJECT_GEOMETRY = 0,
    OGX_OBJECT_CAMERA = 1,
    OGX_OBJECT_LIGHT = 2,
    OGX_OBJECT_UNKNOWN = 255
} ogx_object_kind;

typedef struct ogx_index_range_s
{
    unsigned long start;
    unsigned long count;
    unsigned int material_index;
    unsigned int tuple_size;
    unsigned int front_ccw;
    unsigned long restart_index;
    unsigned int has_restart;
} ogx_index_range;

typedef enum ogx_primitive_e
{
    OGX_PRIM_POINTS = 0,
    OGX_PRIM_LINES = 1,
    OGX_PRIM_LINE_STRIP = 2,
    OGX_PRIM_TRIANGLES = 3,
    OGX_PRIM_TRIANGLE_STRIP = 4,
    OGX_PRIM_QUADS = 5,
    OGX_PRIM_UNKNOWN = 255
} ogx_primitive;

typedef struct ogx_mesh_s
{
    unsigned long vertex_start;
    unsigned long vertex_count;
    unsigned long index_start;
    unsigned long index_count;
    unsigned int index_tuple_size;
    unsigned int index_range_start;
    unsigned int index_range_count;
    unsigned int lod;
    unsigned int object_index;
    ogx_primitive primitive;
    unsigned int attr_mask;
    unsigned int has_skin;
    unsigned long skin_weight_start;
    unsigned long skin_weight_count;
    unsigned int skin_bone_ref_start;
    unsigned int skin_bone_ref_count;
    unsigned int skin_bind_matrix_start;
    unsigned int skin_bind_matrix_count;
    unsigned int skin_has_mesh_bind;
    ogx_fixed skin_mesh_bind[16];
    unsigned int morph_index;
    unsigned int morph_base;
    unsigned int morph_relative;
    unsigned int morph_vertex_start;
    unsigned int morph_vertex_count;
} ogx_mesh;

typedef struct ogx_object_s
{
    char ddl_name[OGX_MAX_NAME];
    ogx_object_kind kind;
    unsigned int mesh_start;
    unsigned int mesh_count;
    unsigned int visible;
    unsigned int shadow;
    unsigned int motion_blur;
} ogx_object;

typedef struct ogx_extra_param_s
{
    unsigned int owner_kind;
    unsigned int owner_index;
    char attrib[OGX_MAX_NAME];
    ogx_fixed value[4];
    unsigned int value_count;
} ogx_extra_param;

typedef struct ogx_camera_object_s
{
    char ddl_name[OGX_MAX_NAME];
    unsigned int object_index;
    unsigned int param_start;
    unsigned int param_count;
    ogx_fixed focal_length;
    ogx_fixed near_depth;
    ogx_fixed far_depth;
    ogx_fixed aspect_ratio;
    unsigned int has_focal_length;
    unsigned int has_near_depth;
    unsigned int has_far_depth;
    unsigned int has_aspect_ratio;
} ogx_camera_object;

typedef struct ogx_light_object_s
{
    char ddl_name[OGX_MAX_NAME];
    unsigned int object_index;
    unsigned int param_start;
    unsigned int param_count;
    ogx_fixed color[4];
    ogx_fixed intensity;
    ogx_fixed range;
    char light_type[OGX_MAX_NAME];
    unsigned int has_color;
    unsigned int has_intensity;
    unsigned int has_range;
} ogx_light_object;

typedef struct ogx_texture_s
{
    char attrib[OGX_MAX_NAME];
    char path[OGX_MAX_TEXTURE_PATH];
    char swizzle[8];
    char x_address[OGX_MAX_NAME];
    char y_address[OGX_MAX_NAME];
    char z_address[OGX_MAX_NAME];
    char border[OGX_MAX_NAME];
    unsigned int texcoord;
    ogx_fixed tex_matrix[16];
} ogx_texture;

typedef struct ogx_material_s
{
    char ddl_name[OGX_MAX_NAME];
    char display_name[OGX_MAX_NAME];
    ogx_fixed diffuse[4];
    ogx_fixed specular[4];
    ogx_fixed emission[4];
    ogx_fixed opacity;
    ogx_fixed specular_power;
    unsigned int has_diffuse;
    unsigned int has_specular;
    unsigned int has_emission;
    unsigned int has_opacity;
    unsigned int has_specular_power;
    unsigned int texture_start;
    unsigned int texture_count;
} ogx_material;

typedef struct ogx_node_s
{
    char ddl_name[OGX_MAX_NAME];
    char display_name[OGX_MAX_NAME];
    char object_ref[OGX_MAX_NAME];
    unsigned int object_index;
    unsigned int object_resolved;
    unsigned int parent_index;
    unsigned int has_parent;
    ogx_fixed node_matrix[16];
    ogx_fixed object_matrix[16];
    unsigned int material_index[OGX_MAX_NODE_MATERIAL_REFS];
    unsigned int material_resolved[OGX_MAX_NODE_MATERIAL_REFS];
    char material_ref[OGX_MAX_NODE_MATERIAL_REFS][OGX_MAX_NAME];
    unsigned int material_slot[OGX_MAX_NODE_MATERIAL_REFS];
    unsigned int material_ref_count;
    unsigned int visible_set;
    unsigned int visible;
} ogx_node;

typedef struct ogx_anim_key_s
{
    ogx_fixed time;
    ogx_fixed value[16];
    unsigned int value_count;
} ogx_anim_key;

typedef struct ogx_anim_track_s
{
    unsigned int node_index;
    char target[OGX_MAX_NAME];
    unsigned int key_start;
    unsigned int key_count;
    unsigned int clip;
    char curve[OGX_MAX_NAME];
    char time_curve[OGX_MAX_NAME];
    char value_curve[OGX_MAX_NAME];
    unsigned int time_key_start;
    unsigned int time_key_count;
    unsigned int time_minus_key_start;
    unsigned int time_minus_key_count;
    unsigned int time_plus_key_start;
    unsigned int time_plus_key_count;
    unsigned int value_key_start;
    unsigned int value_key_count;
    unsigned int value_minus_key_start;
    unsigned int value_minus_key_count;
    unsigned int value_plus_key_start;
    unsigned int value_plus_key_count;
    unsigned int tension_key_start;
    unsigned int tension_key_count;
    unsigned int continuity_key_start;
    unsigned int continuity_key_count;
    unsigned int bias_key_start;
    unsigned int bias_key_count;
} ogx_anim_track;

typedef struct ogx_skin_weight_s
{
    unsigned int vertex_index;
    unsigned int bone_index;
    ogx_fixed weight;
} ogx_skin_weight;

typedef struct ogx_bone_ref_s
{
    char ref[OGX_MAX_NAME];
    unsigned int node_index;
    unsigned int resolved;
} ogx_bone_ref;

typedef struct ogx_morph_weight_s
{
    unsigned int node_index;
    unsigned int morph_index;
    ogx_fixed weight;
} ogx_morph_weight;

typedef struct ogx_morph_target_s
{
    char display_name[OGX_MAX_NAME];
    unsigned int object_index;
    unsigned int morph_index;
    unsigned int has_base;
    unsigned int base_index;
} ogx_morph_target;

typedef struct ogx_morph_vertex_s
{
    unsigned int mesh_index;
    unsigned int morph_index;
    unsigned int local_vertex_index;
    unsigned int attr_mask;
    ogx_vertex vertex;
} ogx_morph_vertex;

typedef struct ogx_triangle_s
{
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int material_index;
} ogx_triangle;

typedef void (*ogx_triangle_callback)(void *user, const ogx_triangle *tri);

typedef struct ogx_model_s
{
    ogx_vertex vertices[OGX_MAX_VERTICES];
    unsigned int indices[OGX_MAX_INDICES];
    ogx_index_range index_ranges[OGX_MAX_MESHES * 4];
    ogx_mesh meshes[OGX_MAX_MESHES];
    ogx_object objects[OGX_MAX_OBJECTS];
    ogx_node nodes[OGX_MAX_NODES];
    ogx_material materials[OGX_MAX_MATERIALS];
    ogx_texture textures[OGX_MAX_TEXTURES];
    ogx_anim_track tracks[OGX_MAX_ANIM_TRACKS];
    ogx_anim_key keys[OGX_MAX_KEYS];
    ogx_skin_weight skin_weights[OGX_MAX_SKIN_WEIGHTS];
    ogx_bone_ref bone_refs[OGX_MAX_BONES];
    ogx_fixed bone_bind_matrices[OGX_MAX_BONES][16];
    ogx_morph_weight morph_weights[OGX_MAX_MORPH_WEIGHTS];
    ogx_morph_target morph_targets[OGX_MAX_MORPH_TARGETS];
    ogx_morph_vertex morph_vertices[OGX_MAX_MORPH_VERTICES];
    ogx_camera_object cameras[OGX_MAX_CAMERAS];
    ogx_light_object lights[OGX_MAX_LIGHTS];
    ogx_extra_param extra_params[OGX_MAX_EXTRA_PARAMS];
    unsigned int vertex_count;
    unsigned int index_count;
    unsigned int index_range_count;
    unsigned int mesh_count;
    unsigned int object_count;
    unsigned int node_count;
    unsigned int material_count;
    unsigned int texture_count;
    unsigned int track_count;
    unsigned int key_count;
    unsigned int skin_weight_count;
    unsigned int bone_ref_count;
    unsigned int bone_bind_matrix_count;
    unsigned int morph_weight_count;
    unsigned int morph_target_count;
    unsigned int morph_vertex_count;
    unsigned int camera_count;
    unsigned int light_count;
    unsigned int extra_param_count;
    ogx_fixed metric_distance;
    ogx_fixed metric_angle;
    ogx_fixed metric_time;
    char up_axis[8];
    char forward_axis[8];
} ogx_model;

typedef struct ogx_report_s
{
    int error_code;
    unsigned long error_offset;
    unsigned long line;
    unsigned long column;
    unsigned int skipped_unknown_structures;
    unsigned int skipped_unsupported_structures;
    unsigned int warnings;
    char message[128];
} ogx_report;

void ogx_model_clear(ogx_model *model);
void ogx_report_clear(ogx_report *report);
int ogx_decode_memory(const char *text, unsigned long length, ogx_model *model, ogx_report *report);
int ogx_resolve_references(ogx_model *model);

ogx_fixed ogx_fixed_from_int(long value);
long ogx_fixed_to_int(ogx_fixed value);
ogx_fixed ogx_fixed_mul(ogx_fixed a, ogx_fixed b);
ogx_fixed ogx_fixed_div(ogx_fixed a, ogx_fixed b);
ogx_fixed ogx_fixed_sin(ogx_fixed radians);
ogx_fixed ogx_fixed_cos(ogx_fixed radians);

void ogx_matrix_identity(ogx_fixed m[16]);
void ogx_matrix_mul(ogx_fixed out[16], const ogx_fixed a[16], const ogx_fixed b[16]);
void ogx_matrix_translate(ogx_fixed out[16], ogx_fixed x, ogx_fixed y, ogx_fixed z);
void ogx_matrix_scale(ogx_fixed out[16], ogx_fixed x, ogx_fixed y, ogx_fixed z);
void ogx_matrix_rotate_x(ogx_fixed out[16], ogx_fixed radians);
void ogx_matrix_rotate_y(ogx_fixed out[16], ogx_fixed radians);
void ogx_matrix_rotate_z(ogx_fixed out[16], ogx_fixed radians);
void ogx_node_world_matrix(const ogx_model *model, unsigned int node_index, ogx_fixed out[16]);
int ogx_eval_track_linear(const ogx_model *model, unsigned int track_index, ogx_fixed time, ogx_fixed *out_values, unsigned int max_values, unsigned int *out_count);
int ogx_mesh_emit_triangles(const ogx_model *model, unsigned int mesh_index, ogx_triangle_callback callback, void *user);
int ogx_mesh_build_triangle_list(const ogx_model *model, unsigned int mesh_index, ogx_triangle *out_tris, unsigned int max_tris, unsigned int *out_count);
int ogx_validate_model(const ogx_model *model, ogx_report *report);
int ogx_node_model_matrix(const ogx_model *model, unsigned int node_index, ogx_fixed out[16]);
int ogx_transform_point3(const ogx_fixed m[16], const ogx_fixed in_pos[3], ogx_fixed out_pos[3]);
int ogx_node_vertex_world_position(const ogx_model *model, unsigned int node_index, unsigned int vertex_index, ogx_fixed out_pos[3]);
int ogx_vertex_bone4(const ogx_model *model, unsigned int mesh_index, unsigned int local_vertex_index, unsigned int out_bone[4], ogx_fixed out_weight[4], unsigned int *out_count);
int ogx_eval_track(const ogx_model *model, unsigned int track_index, ogx_fixed time, ogx_fixed *out_values, unsigned int max_values, unsigned int *out_count);
int ogx_find_morph_target(const ogx_model *model, unsigned int object_index, unsigned int morph_index, unsigned int *out_index);
int ogx_find_morph_vertex(const ogx_model *model, unsigned int mesh_index, unsigned int morph_index, unsigned int local_vertex_index, unsigned int *out_index);
int ogx_node_morph_weight_value(const ogx_model *model, unsigned int node_index, unsigned int morph_index, ogx_fixed *out_weight);
int ogx_node_morphed_vertex(const ogx_model *model, unsigned int node_index, unsigned int mesh_index, unsigned int local_vertex_index, ogx_vertex *out_vertex);
int ogx_node_morphed_vertex_world_position(const ogx_model *model, unsigned int node_index, unsigned int mesh_index, unsigned int local_vertex_index, ogx_fixed out_pos[3]);

#ifdef __cplusplus
}
#endif

#endif
