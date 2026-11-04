#ifndef ASE3D_H
#define ASE3D_H

/*
   ase3d.h - fixed-point Autodesk/3ds Max ASE decoder core.
   C89, no malloc/free/realloc, no float/double required by this library.

   The caller owns all arrays. The decoder fills as much as capacity allows
   and reports overflow flags without touching heap memory.
*/

#ifdef __cplusplus
extern "C" {
#endif

#define ASE3D_VERSION_MAJOR 0
#define ASE3D_VERSION_MINOR 5
#define ASE3D_VERSION_PATCH 0

#define ASE3D_FIXED_SHIFT 16
#define ASE3D_FIXED_ONE   65536L

#define ASE3D_NAME_MAX    64
#define ASE3D_PATH_MAX    160
#define ASE3D_STACK_MAX   80

#define ASE3D_OK                 0
#define ASE3D_ERR_BAD_ARG       -1
#define ASE3D_ERR_STACK_OVER    -2
#define ASE3D_ERR_STACK_UNDER   -3

#define ASE3D_WARN_VERTEX_OVERFLOW    0x00000001UL
#define ASE3D_WARN_FACE_OVERFLOW      0x00000002UL
#define ASE3D_WARN_TVERT_OVERFLOW     0x00000004UL
#define ASE3D_WARN_TFACE_OVERFLOW     0x00000008UL
#define ASE3D_WARN_CVERT_OVERFLOW     0x00000010UL
#define ASE3D_WARN_CFACE_OVERFLOW     0x00000020UL
#define ASE3D_WARN_MAT_OVERFLOW       0x00000040UL
#define ASE3D_WARN_OBJ_OVERFLOW       0x00000080UL
#define ASE3D_WARN_NORMAL_OVERFLOW    0x00000100UL
#define ASE3D_WARN_CORNERN_OVERFLOW   0x00000200UL
#define ASE3D_WARN_STACK_OVERFLOW     0x00000400UL
#define ASE3D_WARN_BAD_INDEX          0x00000800UL
#define ASE3D_WARN_POSKEY_OVERFLOW    0x00001000UL
#define ASE3D_WARN_ROTKEY_OVERFLOW    0x00002000UL
#define ASE3D_WARN_SCALEKEY_OVERFLOW  0x00004000UL
#define ASE3D_WARN_MESHSPAN_OVERFLOW  0x00008000UL
#define ASE3D_WARN_LINE_TRUNCATED     0x00010000UL
#define ASE3D_WARN_MAP_OVERFLOW       0x00020000UL
#define ASE3D_WARN_LIGHT_OVERFLOW     0x00040000UL
#define ASE3D_WARN_CAMERA_OVERFLOW    0x00080000UL
#define ASE3D_WARN_BOUNDS_EMPTY       0x00100000UL
#define ASE3D_WARN_UNKNOWN            0x80000000UL

#define ASE3D_OBJECT_GEOM    1
#define ASE3D_OBJECT_HELPER  2
#define ASE3D_OBJECT_LIGHT   3
#define ASE3D_OBJECT_CAMERA  4
#define ASE3D_OBJECT_SHAPE   5

#define ASE3D_KEY_STEP       0
#define ASE3D_KEY_LINEAR     1
#define ASE3D_KEY_TCB        2
#define ASE3D_KEY_BEZIER     3

#define ASE3D_MAP_UNKNOWN     0
#define ASE3D_MAP_AMBIENT     1
#define ASE3D_MAP_DIFFUSE     2
#define ASE3D_MAP_SPECULAR    3
#define ASE3D_MAP_SHINE       4
#define ASE3D_MAP_OPACITY     5
#define ASE3D_MAP_BUMP        6
#define ASE3D_MAP_SELFILLUM   7
#define ASE3D_MAP_REFLECT     8
#define ASE3D_MAP_REFRACT     9
#define ASE3D_MAP_FILTER      10

typedef long ase3d_i32;
typedef unsigned long ase3d_u32;
typedef ase3d_i32 ase3d_fixed;

typedef struct ase3d_vec2_s {
    ase3d_fixed u;
    ase3d_fixed v;
} ase3d_vec2;

typedef struct ase3d_vec3_s {
    ase3d_fixed x;
    ase3d_fixed y;
    ase3d_fixed z;
} ase3d_vec3;

typedef struct ase3d_rgb_s {
    ase3d_fixed r;
    ase3d_fixed g;
    ase3d_fixed b;
} ase3d_rgb;

typedef struct ase3d_quat_s {
    ase3d_fixed x;
    ase3d_fixed y;
    ase3d_fixed z;
    ase3d_fixed w;
} ase3d_quat;

typedef struct ase3d_mat43_s {
    ase3d_vec3 row0;
    ase3d_vec3 row1;
    ase3d_vec3 row2;
    ase3d_vec3 row3;
} ase3d_mat43;

typedef struct ase3d_scene_s {
    char filename[ASE3D_PATH_MAX];
    long first_frame;
    long last_frame;
    long frame_speed;
    long ticks_per_frame;
    ase3d_rgb background;
    ase3d_rgb ambient;
    int has_background;
    int has_ambient;
} ase3d_scene;

typedef struct ase3d_face_s {
    int index;       /* ASE-local face index inside the current MESH block. */
    int local_a;
    int local_b;
    int local_c;
    int a;           /* global vertex index in model->vertices. */
    int b;
    int c;
    int edge_ab;
    int edge_bc;
    int edge_ca;
    ase3d_u32 smoothing_mask;
    int material_id;
    int object_id;
    int mesh_span_id;
} ase3d_face;

typedef struct ase3d_tface_s {
    int index;
    int local_a;
    int local_b;
    int local_c;
    int a;           /* global texture vertex index in model->tverts. */
    int b;
    int c;
    int channel;
    int object_id;
    int mesh_span_id;
} ase3d_tface;

typedef struct ase3d_cface_s {
    int index;
    int local_a;
    int local_b;
    int local_c;
    int a;           /* global vertex-color index in model->cverts. */
    int b;
    int c;
    int object_id;
    int mesh_span_id;
} ase3d_cface;

typedef struct ase3d_material_s {
    int index;
    int parent_index;
    char name[ASE3D_NAME_MAX];
    char class_name[ASE3D_NAME_MAX];
    char bitmap[ASE3D_PATH_MAX];
    char map_type[ASE3D_NAME_MAX];
    ase3d_rgb ambient;
    ase3d_rgb diffuse;
    ase3d_rgb specular;
    ase3d_rgb color;
    ase3d_fixed shine;
    ase3d_fixed shine_strength;
    ase3d_fixed transparency;
    int has_bitmap;
    int num_submaterials;
} ase3d_material;

typedef struct ase3d_texture_map_s {
    int material_index;
    int parent_material_index;
    int kind;
    char name[ASE3D_NAME_MAX];
    char class_name[ASE3D_NAME_MAX];
    char bitmap[ASE3D_PATH_MAX];
    char texture[ASE3D_NAME_MAX];
    ase3d_fixed amount;
    ase3d_fixed u_offset;
    ase3d_fixed v_offset;
    ase3d_fixed u_tiling;
    ase3d_fixed v_tiling;
    ase3d_fixed angle;
    ase3d_fixed blur;
    int invert;
    int has_bitmap;
} ase3d_texture_map;

typedef struct ase3d_bounds_s {
    ase3d_vec3 min;
    ase3d_vec3 max;
    int valid;
} ase3d_bounds;

typedef struct ase3d_light_s {
    int object_id;
    char type_name[ASE3D_NAME_MAX];
    long timevalue;
    ase3d_rgb color;
    ase3d_fixed intensity;
    ase3d_fixed contrast;
    ase3d_fixed soften_diff_edge;
    ase3d_fixed hotspot;
    ase3d_fixed falloff;
    ase3d_fixed aspect;
    ase3d_fixed attenuation_start;
    ase3d_fixed attenuation_end;
    ase3d_fixed map_bias;
    long map_size;
    long map_range;
    int casts_shadows;
    int has_color;
} ase3d_light;

typedef struct ase3d_camera_s {
    int object_id;
    char target_name[ASE3D_NAME_MAX];
    long timevalue;
    ase3d_fixed fov;
    ase3d_fixed near_range;
    ase3d_fixed far_range;
    ase3d_fixed target_distance;
    ase3d_fixed hither;
    ase3d_fixed yon;
    int has_fov;
} ase3d_camera;

typedef struct ase3d_node_tm_s {
    ase3d_vec3 row0;
    ase3d_vec3 row1;
    ase3d_vec3 row2;
    ase3d_vec3 row3;
    ase3d_vec3 pos;
    ase3d_vec3 rot_axis;
    ase3d_fixed rot_angle;
    ase3d_vec3 scale;
    ase3d_vec3 scale_axis;
    ase3d_fixed scale_axis_angle;
    int has_row0;
    int has_row1;
    int has_row2;
    int has_row3;
    int has_pos;
    int has_rot;
    int has_scale;
    int has_scale_axis;
} ase3d_node_tm;

typedef struct ase3d_object_s {
    char name[ASE3D_NAME_MAX];
    char parent_name[ASE3D_NAME_MAX];
    int type;
    int material_ref;
    int first_vertex;
    int vertex_count;
    int first_face;
    int face_count;
    int first_tvert;
    int tvert_count;
    int first_tface;
    int tface_count;
    int first_cvert;
    int cvert_count;
    int first_cface;
    int cface_count;
    int first_mesh_span;
    int mesh_span_count;
    int first_pos_key;
    int pos_key_count;
    int first_rot_key;
    int rot_key_count;
    int first_scale_key;
    int scale_key_count;
    ase3d_bounds local_bounds;
    ase3d_bounds world_bounds;
    ase3d_node_tm tm;
} ase3d_object;

typedef struct ase3d_mesh_span_s {
    int object_id;
    long timevalue;
    int source_mesh_index;
    int first_vertex;
    int vertex_count;
    int first_face;
    int face_count;
    int first_tvert;
    int tvert_count;
    int first_tface;
    int tface_count;
    int first_cvert;
    int cvert_count;
    int first_cface;
    int cface_count;
} ase3d_mesh_span;

typedef struct ase3d_corner_normal_s {
    int face_index;          /* global face index if the face was parsed. */
    int local_face_index;
    int vertex_index;        /* global vertex index. */
    int local_vertex_index;
    int corner;
    ase3d_vec3 n;
    int object_id;
    int mesh_span_id;
} ase3d_corner_normal;

typedef struct ase3d_pos_key_s {
    int object_id;
    long tick;
    ase3d_vec3 pos;
    ase3d_vec3 in_tangent;
    ase3d_vec3 out_tangent;
    ase3d_fixed tension;
    ase3d_fixed continuity;
    ase3d_fixed bias;
    ase3d_fixed ease_to;
    ase3d_fixed ease_from;
    int kind;
    int has_in_tangent;
    int has_out_tangent;
    int has_tcb;
} ase3d_pos_key;

typedef struct ase3d_rot_key_s {
    int object_id;
    long tick;
    ase3d_vec3 axis;
    ase3d_fixed angle;
    ase3d_quat quat;
    ase3d_fixed tension;
    ase3d_fixed continuity;
    ase3d_fixed bias;
    ase3d_fixed ease_to;
    ase3d_fixed ease_from;
    int kind;
    int has_quat;
    int has_tcb;
} ase3d_rot_key;

typedef struct ase3d_scale_key_s {
    int object_id;
    long tick;
    ase3d_vec3 scale;
    ase3d_vec3 axis;
    ase3d_fixed angle;
    ase3d_vec3 in_tangent;
    ase3d_vec3 out_tangent;
    ase3d_fixed tension;
    ase3d_fixed continuity;
    ase3d_fixed bias;
    ase3d_fixed ease_to;
    ase3d_fixed ease_from;
    int kind;
    int has_in_tangent;
    int has_out_tangent;
    int has_tcb;
} ase3d_scale_key;

typedef struct ase3d_model_s {
    ase3d_scene scene;

    ase3d_vec3 *vertices;
    unsigned int vertex_capacity;
    unsigned int vertex_count;

    ase3d_face *faces;
    unsigned int face_capacity;
    unsigned int face_count;

    ase3d_vec3 *tverts; /* u, v, w. */
    unsigned int tvert_capacity;
    unsigned int tvert_count;

    ase3d_tface *tfaces;
    unsigned int tface_capacity;
    unsigned int tface_count;

    ase3d_rgb *cverts;
    unsigned int cvert_capacity;
    unsigned int cvert_count;

    ase3d_cface *cfaces;
    unsigned int cface_capacity;
    unsigned int cface_count;

    ase3d_material *materials;
    unsigned int material_capacity;
    unsigned int material_count;

    ase3d_texture_map *maps;
    unsigned int map_capacity;
    unsigned int map_count;

    ase3d_light *lights;
    unsigned int light_capacity;
    unsigned int light_count;

    ase3d_camera *cameras;
    unsigned int camera_capacity;
    unsigned int camera_count;

    ase3d_object *objects;
    unsigned int object_capacity;
    unsigned int object_count;

    ase3d_mesh_span *mesh_spans;
    unsigned int mesh_span_capacity;
    unsigned int mesh_span_count;

    ase3d_vec3 *face_normals;
    unsigned int face_normal_capacity;
    unsigned int face_normal_count;

    ase3d_corner_normal *corner_normals;
    unsigned int corner_normal_capacity;
    unsigned int corner_normal_count;

    ase3d_pos_key *pos_keys;
    unsigned int pos_key_capacity;
    unsigned int pos_key_count;

    ase3d_rot_key *rot_keys;
    unsigned int rot_key_capacity;
    unsigned int rot_key_count;

    ase3d_scale_key *scale_keys;
    unsigned int scale_key_capacity;
    unsigned int scale_key_count;

    unsigned long warnings;
    unsigned long parsed_lines;
} ase3d_model;

typedef struct ase3d_block_s {
    int type;
    int saved_object;
    int saved_material;
    int saved_mapping_channel;
    int saved_parent_material;
    int saved_mesh_span;
    int saved_anim_track;
    int saved_key_kind;
    int saved_map;
    int saved_light;
    int saved_camera;
} ase3d_block;

typedef struct ase3d_parser_s {
    ase3d_model *model;
    int current_object;
    int current_material;
    int parent_material;
    int current_mapping_channel;
    int current_normal_face;
    int current_normal_local_face;
    int current_normal_corner;
    int current_mesh_span;
    int current_anim_track;
    int current_key_kind;
    int current_map_block;
    int current_map;
    int current_light;
    int current_camera;
    ase3d_block stack[ASE3D_STACK_MAX];
    int stack_top;
    unsigned long line;
} ase3d_parser;

void ase3d_model_zero(ase3d_model *m);
void ase3d_parser_init(ase3d_parser *p, ase3d_model *m);
int ase3d_parse_text(ase3d_parser *p, const char *text, unsigned long text_len);
int ase3d_parse_line(ase3d_parser *p, const char *line);

/* Optional utility: rebuild flat normals from vertices/faces into face_normals. */
void ase3d_build_flat_normals(ase3d_model *m);
void ase3d_build_smooth_normals(ase3d_model *m);
void ase3d_compute_object_bounds(ase3d_model *m);
int ase3d_find_mesh_span_for_tick(const ase3d_model *m, int object_id, long tick);

/* Optional utility: apply each object's TM_ROW matrix to vertices in-place.
   Translation uses row3. It is fixed point and intentionally conservative. */
void ase3d_apply_object_transforms(ase3d_model *m);

/* Optional helpers for sampled transform animation. v0.3 evaluates linear, TCB
   and Bezier position/scale tracks. Rotation can be accumulated into quats and
   sampled with fixed-point SLERP. */
void ase3d_finalize_animation(ase3d_model *m);
int ase3d_find_object_index(const ase3d_model *m, const char *name);
int ase3d_find_parent_index(const ase3d_model *m, int object_id);
int ase3d_sample_position(const ase3d_model *m, int object_id, long tick, ase3d_vec3 *out);
int ase3d_sample_scale(const ase3d_model *m, int object_id, long tick, ase3d_vec3 *out);
int ase3d_sample_rotation_axis_angle(const ase3d_model *m, int object_id, long tick, ase3d_vec3 *axis, ase3d_fixed *angle);
int ase3d_sample_rotation_quat(const ase3d_model *m, int object_id, long tick, ase3d_quat *out);
int ase3d_sample_local_matrix(const ase3d_model *m, int object_id, long tick, ase3d_mat43 *out);
int ase3d_sample_world_matrix(const ase3d_model *m, int object_id, long tick, ase3d_mat43 *out);
ase3d_vec3 ase3d_transform_point(ase3d_mat43 m, ase3d_vec3 v);

/* Fixed helpers. */
ase3d_fixed ase3d_fixed_from_int(long v);
long ase3d_fixed_to_int(ase3d_fixed v);
ase3d_fixed ase3d_fixed_mul(ase3d_fixed a, ase3d_fixed b);
ase3d_fixed ase3d_fixed_div(ase3d_fixed a, ase3d_fixed b);
ase3d_fixed ase3d_fixed_lerp(ase3d_fixed a, ase3d_fixed b, ase3d_fixed t);
ase3d_fixed ase3d_fixed_sin_rad(ase3d_fixed radians);
ase3d_fixed ase3d_fixed_cos_rad(ase3d_fixed radians);
ase3d_fixed ase3d_fixed_acos(ase3d_fixed x);
ase3d_fixed ase3d_fixed_sqrt_public(ase3d_fixed v);
int ase3d_fixed_to_string(ase3d_fixed v, char *out, unsigned int out_cap, int decimals);

/* Quaternion helpers. All use Q16.16; q is expected normalized when noted. */
ase3d_quat ase3d_quat_identity(void);
ase3d_quat ase3d_quat_normalize(ase3d_quat q);
ase3d_quat ase3d_quat_mul(ase3d_quat a, ase3d_quat b);
ase3d_quat ase3d_quat_from_axis_angle(ase3d_vec3 axis, ase3d_fixed angle);
ase3d_quat ase3d_quat_slerp(ase3d_quat a, ase3d_quat b, ase3d_fixed t);
ase3d_mat43 ase3d_quat_to_mat43(ase3d_quat q);

#ifdef __cplusplus
}
#endif

#endif
