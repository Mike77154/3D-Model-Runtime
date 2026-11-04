#ifndef COB_CHIQUI3D_H
#define COB_CHIQUI3D_H

/*
   cob_chiqui3d - small trueSpace COB/SCN codec
   C89, fixed point, no heap allocation.

   This public header intentionally avoids stdint.h and dynamic memory.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COB_NAME_MAX
#define COB_NAME_MAX 64
#endif

#ifndef COB_PATH_MAX
#define COB_PATH_MAX 128
#endif

#ifndef COB_MAX_MESHES
#define COB_MAX_MESHES 64
#endif

#ifndef COB_MAX_GROUPS
#define COB_MAX_GROUPS 64
#endif

#ifndef COB_MAX_MATERIALS
#define COB_MAX_MATERIALS 128
#endif

#ifndef COB_MAX_LIGHTS
#define COB_MAX_LIGHTS 32
#endif

#ifndef COB_MAX_CAMERAS
#define COB_MAX_CAMERAS 32
#endif

#ifndef COB_MAX_SHADER_STAGES
#define COB_MAX_SHADER_STAGES 256
#endif

#ifndef COB_MAX_BONES
#define COB_MAX_BONES 128
#endif

#ifndef COB_MAX_ANIM_CHANNELS
#define COB_MAX_ANIM_CHANNELS 128
#endif

#ifndef COB_MAX_ANIM_KEYS
#define COB_MAX_ANIM_KEYS 4096
#endif

#ifndef COB_MAX_NURBS_KNOTS
#define COB_MAX_NURBS_KNOTS 64
#endif

#ifndef COB_MAX_NURBS_CV
#define COB_MAX_NURBS_CV 256
#endif

#ifndef COB_MAX_METABALLS
#define COB_MAX_METABALLS 64
#endif

#ifndef COB_SHADER_CLASS_MAX
#define COB_SHADER_CLASS_MAX 32
#endif

#ifndef COB_SHADER_NAME_MAX
#define COB_SHADER_NAME_MAX 64
#endif

#ifndef COB_MAX_VERTICES
#define COB_MAX_VERTICES 8192
#endif

#ifndef COB_MAX_UVS
#define COB_MAX_UVS 8192
#endif

#ifndef COB_MAX_FACES
#define COB_MAX_FACES 8192
#endif

#ifndef COB_MAX_INDICES
#define COB_MAX_INDICES 32768
#endif

#ifndef COB_DIAG_MAX
#define COB_DIAG_MAX 64
#endif

#ifndef COB_MAX_SCENE_LAYERS
#define COB_MAX_SCENE_LAYERS 32
#endif

#ifndef COB_MAX_OBJECT_LAYERS
#define COB_MAX_OBJECT_LAYERS 256
#endif

#define COB_FIXED_ONE 65536L
#define COB_FIXED_HALF 32768L
#define COB_FIXED_MAX 2147483647L
#define COB_FIXED_MIN (-2147483647L-1L)

typedef signed char cob_i8;
typedef unsigned char cob_u8;
typedef signed short cob_i16;
typedef unsigned short cob_u16;
typedef signed long cob_i32;
typedef unsigned long cob_u32;
typedef signed long cob_fixed;

typedef struct cob_vec3q_s {
    cob_fixed x;
    cob_fixed y;
    cob_fixed z;
} cob_vec3q;

typedef struct cob_vec2q_s {
    cob_fixed u;
    cob_fixed v;
} cob_vec2q;

typedef struct cob_chunk_info_s {
    char type[5];
    cob_i32 id;
    cob_i32 parent_id;
    cob_u16 major;
    cob_u16 minor;
    cob_i32 size;
} cob_chunk_info;

typedef struct cob_vertex_ref_s {
    cob_i32 pos_index;
    cob_i32 uv_index;
} cob_vertex_ref;

typedef struct cob_face_s {
    cob_u8 flags;
    cob_u16 index_count;
    cob_i16 material;
    cob_u32 first_index;
} cob_face;

typedef struct cob_mesh_s {
    char name[COB_NAME_MAX];
    cob_i32 id;
    cob_i32 parent_id;
    cob_u16 major;
    cob_u16 minor;
    cob_u16 dupe_count;
    cob_u32 draw_flags;
    cob_u32 unit_code;
    cob_fixed unit_scale;
    cob_fixed axes[12];      /* center, x axis, y axis, z axis */
    cob_fixed transform[16]; /* row-major 4x4 */
    cob_u32 first_vertex;
    cob_u32 vertex_count;
    cob_u32 first_uv;
    cob_u32 uv_count;
    cob_u32 first_face;
    cob_u32 face_count;
} cob_mesh;

typedef struct cob_material_s {
    cob_i32 id;
    cob_i32 parent_id;
    cob_u16 major;
    cob_u16 minor;
    cob_i16 matnum;
    cob_u8 shader;           /* 'f', 'p', 'm' */
    cob_u8 facet;            /* 'f', 'a', 's' */
    cob_u8 autofacet_angle;
    cob_fixed rgb[3];
    cob_fixed alpha;
    cob_fixed ka;
    cob_fixed ks;
    cob_fixed exp;
    cob_fixed ior;
    char texture_color[COB_PATH_MAX];
    char texture_env[COB_PATH_MAX];
    char texture_bump[COB_PATH_MAX];
    cob_fixed tex_color_transform[4]; /* uoff, voff, uscale, vscale */
    cob_fixed tex_bump_transform[4];
} cob_material;

typedef struct cob_group_s {
    char name[COB_NAME_MAX];
    cob_i32 id;
    cob_i32 parent_id;
    cob_u16 major;
    cob_u16 minor;
    cob_u16 dupe_count;
    cob_fixed transform[16];
} cob_group;

typedef struct cob_light_s {
    char name[COB_NAME_MAX];
    cob_i32 id;
    cob_i32 parent_id;
    cob_u16 major;
    cob_u16 minor;
    cob_u8 light_type;       /* 1 infinite, 2 local, 3 spot, 4 area-ish */
    cob_fixed color[3];
    cob_fixed cone_angle;
    cob_fixed inner_angle;
    cob_fixed transform[16];
} cob_light;

typedef struct cob_camera_s {
    char name[COB_NAME_MAX];
    cob_i32 id;
    cob_i32 parent_id;
    cob_u16 major;
    cob_u16 minor;
    cob_fixed transform[16];
} cob_camera;

typedef struct cob_shader_stage_s {
    cob_i32 id;
    cob_i32 parent_id;
    cob_u16 major;
    cob_u16 minor;
    char shader_class[COB_SHADER_CLASS_MAX];
    char shader_name[COB_SHADER_NAME_MAX];
    cob_u16 parameter_count;
    cob_u16 flags;
    cob_u8 has_color;
    cob_fixed color[3];
    cob_u8 has_scalar;
    cob_fixed scalar;
} cob_shader_stage;

typedef struct cob_bone_s {
    char name[COB_NAME_MAX];
    cob_i32 id;
    cob_i32 parent_id;
    cob_u16 major;
    cob_u16 minor;
    cob_u16 dupe_count;
    cob_fixed transform[16];
    cob_fixed length;
    cob_u32 flags;
} cob_bone;

typedef struct cob_anim_key_s {
    cob_i32 channel_id;
    cob_i32 target_id;
    cob_i32 frame;
    cob_fixed time;
    cob_fixed value[4];
    cob_u8 semantic;      /* 1 pos, 2 rot, 3 scale, 4 scalar, 0 unknown */
} cob_anim_key;

typedef struct cob_anim_channel_s {
    char name[COB_NAME_MAX];
    cob_i32 id;
    cob_i32 parent_id;
    cob_i32 target_id;
    cob_u16 major;
    cob_u16 minor;
    cob_u8 semantic;
    cob_u32 flags;
    cob_u32 first_key;
    cob_u32 key_count;
} cob_anim_channel;

typedef struct cob_shader_eval_s {
    cob_fixed diffuse[3];
    cob_fixed opacity;
    cob_fixed ambient;
    cob_fixed specular;
    cob_fixed shininess;
    cob_fixed reflection;
    cob_fixed refraction;
    cob_fixed bump;
    cob_fixed displacement;
    cob_u32 applied_stages;
    cob_u32 unknown_stages;
} cob_shader_eval;

typedef struct cob_triangulate_stats_s {
    cob_u32 triangles;
    cob_u32 concave_faces;
    cob_u32 hole_groups;
    cob_u32 bridge_count;
    cob_u32 skipped_holes;
    cob_u32 degenerate_faces;
    cob_u32 failed_faces;
} cob_triangulate_stats;

typedef struct cob_nurbs_patch_s {
    cob_u16 u_degree;
    cob_u16 v_degree;
    cob_u16 u_count;
    cob_u16 v_count;
    cob_u16 u_knot_count;
    cob_u16 v_knot_count;
    cob_u8 rational;
    cob_fixed u_knots[COB_MAX_NURBS_KNOTS];
    cob_fixed v_knots[COB_MAX_NURBS_KNOTS];
    cob_vec3q control[COB_MAX_NURBS_CV];
    cob_fixed weights[COB_MAX_NURBS_CV];
} cob_nurbs_patch;

typedef struct cob_metaball_s {
    cob_vec3q center;
    cob_fixed radius;
    cob_fixed strength;
} cob_metaball;

typedef struct cob_metaball_field_s {
    cob_metaball balls[COB_MAX_METABALLS];
    cob_u32 ball_count;
    cob_fixed threshold;
    cob_vec3q minv;
    cob_vec3q maxv;
} cob_metaball_field;


typedef struct cob_scene_layer_s {
    char name[COB_NAME_MAX];
    cob_i32 id;
    cob_i32 parent_id;
    cob_u16 major;
    cob_u16 minor;
    cob_i16 layer_index;
    cob_u32 flags;
    cob_u8 visible;
    cob_u8 locked;
} cob_scene_layer;

typedef struct cob_object_layer_s {
    cob_i32 id;
    cob_i32 parent_id;
    cob_u16 major;
    cob_u16 minor;
    cob_i32 target_id;
    cob_i16 layer_index;
    cob_u32 flags;
    cob_u8 hidden;
} cob_object_layer;

typedef struct cob_tangent_frame_s {
    cob_vec3q normal;
    cob_vec3q tangent;
    cob_vec3q bitangent;
    cob_fixed handedness;
    cob_u32 flags;
} cob_tangent_frame;

typedef struct cob_hierarchy_report_s {
    cob_u32 node_count;
    cob_u32 parent_links;
    cob_u32 orphan_count;
    cob_u32 cycle_count;
    cob_u32 max_depth;
    cob_u32 layer_count;
    cob_u32 object_layer_count;
    cob_u32 hidden_objects;
} cob_hierarchy_report;

typedef struct cob_runtime_report_s {
    cob_u32 mesh_count;
    cob_u32 material_count;
    cob_u32 textured_materials;
    cob_u32 bump_materials;
    cob_u32 env_materials;
    cob_u32 light_count;
    cob_u32 camera_count;
    cob_u32 layer_count;
    cob_u32 object_layer_count;
    cob_u32 vertex_count;
    cob_u32 triangle_count;
    cob_u32 tangent_count;
    cob_u32 anim_channel_count;
    cob_u32 anim_key_count;
} cob_runtime_report;


#ifndef COB_MAX_ANIM_POSES
#define COB_MAX_ANIM_POSES 128
#endif

/* Extra renderer/export helper structs added by the EXTREME module. */
typedef struct cob_shader_sample_s {
    cob_fixed rgba[4];
    cob_fixed emission[3];
    cob_fixed normal_delta[3];
    cob_fixed roughness;
    cob_fixed metallic;
    cob_u32 flags;
} cob_shader_sample;

typedef struct cob_quality_report_s {
    cob_u32 mesh_count;
    cob_u32 face_count;
    cob_u32 triangle_count;
    cob_u32 concave_count;
    cob_u32 hole_count;
    cob_u32 bridge_count;
    cob_u32 failed_triangulations;
    cob_u32 shader_stage_count;
    cob_u32 shader_unknown_count;
    cob_u32 anim_channel_count;
    cob_u32 anim_rotation_channels;
    cob_u32 nurbs_ready;
    cob_u32 metaball_ready;
} cob_quality_report;

typedef struct cob_anim_pose_s {
    cob_i32 target_id;
    cob_fixed transform[16];
    cob_u32 applied_channels;
} cob_anim_pose;


/* OMEGA pipeline flags: unit baking, visibility, engine-ready exports. */
enum {
    COB_ENGINE_REPAIR = 1U,
    COB_ENGINE_BAKE_HIERARCHY = 2U,
    COB_ENGINE_APPLY_UNITS = 4U,
    COB_ENGINE_VISIBLE_ONLY = 8U,
    COB_ENGINE_DEFAULT = 15U
};

typedef struct cob_feature_report_s {
    cob_u32 source_ascii;
    cob_u32 source_binary;
    cob_u32 little_endian;
    cob_u32 geometry_chunks;
    cob_u32 material_chunks;
    cob_u32 texture_slots;
    cob_u32 shader_stages;
    cob_u32 groups;
    cob_u32 lights;
    cob_u32 cameras;
    cob_u32 layers;
    cob_u32 object_layers;
    cob_u32 hidden_objects;
    cob_u32 bones;
    cob_u32 anim_channels;
    cob_u32 anim_keys;
    cob_u32 noted_nurbs;
    cob_u32 noted_metaballs;
    cob_u32 noted_nmesh;
    cob_u32 noted_render;
    cob_u32 unsupported_chunks;
    cob_u32 concave_faces;
    cob_u32 hole_faces;
    cob_u32 failed_triangulations;
    cob_u32 engine_ready_score;
} cob_feature_report;

typedef struct cob_engine_report_s {
    cob_u32 before_meshes;
    cob_u32 after_meshes;
    cob_u32 before_vertices;
    cob_u32 after_vertices;
    cob_u32 hidden_meshes_removed;
    cob_u32 unit_scaled_meshes;
    cob_u32 repaired;
    cob_u32 hierarchy_baked;
    cob_u32 units_baked;
    cob_u32 visible_filtered;
    cob_u32 triangle_count;
    cob_u32 diagnostic_count;
} cob_engine_report;

typedef struct cob_bounds_s {
    int valid;
    cob_vec3q minv;
    cob_vec3q maxv;
} cob_bounds;

typedef struct cob_triangle_s {
    cob_u32 mesh_index;
    cob_u32 face_index;
    cob_i16 material;
    cob_u8 flags;
    cob_vertex_ref refs[3];
    cob_vec3q p[3];
    cob_vec2q uv[3];
} cob_triangle;

typedef struct cob_scene_stats_s {
    cob_u32 meshes;
    cob_u32 materials;
    cob_u32 groups;
    cob_u32 lights;
    cob_u32 cameras;
    cob_u32 shader_stages;
    cob_u32 bones;
    cob_u32 anim_channels;
    cob_u32 anim_keys;
    cob_u32 vertices;
    cob_u32 uvs;
    cob_u32 faces;
    cob_u32 indices;
    cob_u32 triangles;
    cob_u32 hole_faces;
    cob_u32 degenerate_faces;
    cob_u32 invalid_refs;
    cob_u32 empty_meshes;
} cob_scene_stats;

typedef struct cob_diag_s {
    int code;
    cob_i32 chunk_id;
    char chunk_type[5];
    char text[96];
} cob_diag;

typedef struct cob_scene_s {
    int source_format;       /* COB_FORMAT_ASCII or COB_FORMAT_BINARY */
    int little_endian;
    char header_version[7];

    cob_mesh meshes[COB_MAX_MESHES];
    cob_u32 mesh_count;

    cob_group groups[COB_MAX_GROUPS];
    cob_u32 group_count;

    cob_material materials[COB_MAX_MATERIALS];
    cob_u32 material_count;

    cob_light lights[COB_MAX_LIGHTS];
    cob_u32 light_count;

    cob_camera cameras[COB_MAX_CAMERAS];
    cob_u32 camera_count;

    cob_scene_layer scene_layers[COB_MAX_SCENE_LAYERS];
    cob_u32 scene_layer_count;

    cob_object_layer object_layers[COB_MAX_OBJECT_LAYERS];
    cob_u32 object_layer_count;

    cob_shader_stage shader_stages[COB_MAX_SHADER_STAGES];
    cob_u32 shader_stage_count;

    cob_bone bones[COB_MAX_BONES];
    cob_u32 bone_count;

    cob_anim_channel anim_channels[COB_MAX_ANIM_CHANNELS];
    cob_u32 anim_channel_count;

    cob_anim_key anim_keys[COB_MAX_ANIM_KEYS];
    cob_u32 anim_key_count;

    cob_vec3q vertices[COB_MAX_VERTICES];
    cob_u32 vertex_count;

    cob_vec2q uvs[COB_MAX_UVS];
    cob_u32 uv_count;

    cob_face faces[COB_MAX_FACES];
    cob_u32 face_count;

    cob_vertex_ref indices[COB_MAX_INDICES];
    cob_u32 index_count;

    cob_diag diagnostics[COB_DIAG_MAX];
    cob_u32 diagnostic_count;
} cob_scene;

typedef int (*cob_write_fn)(void *user, const void *bytes, unsigned long count);

enum {
    COB_OK = 0,
    COB_ERR_ARGUMENT = -1,
    COB_ERR_BAD_MAGIC = -2,
    COB_ERR_UNSUPPORTED_ENDIAN = -3,
    COB_ERR_UNSUPPORTED_FORMAT = -4,
    COB_ERR_TRUNCATED = -5,
    COB_ERR_CAPACITY = -6,
    COB_ERR_PARSE = -7,
    COB_ERR_WRITE = -8
};

enum {
    COB_FORMAT_UNKNOWN = 0,
    COB_FORMAT_ASCII = 1,
    COB_FORMAT_BINARY = 2
};

enum {
    COB_FACE_HOLE = 0x08,
    COB_FACE_BACKCULL = 0x10
};

void cob_scene_clear(cob_scene *scene);
const char *cob_status_text(int status);

int cob_decode_memory(cob_scene *scene, const void *bytes, unsigned long count);
int cob_decode_ascii(cob_scene *scene, const void *bytes, unsigned long count, unsigned long start_offset);
int cob_decode_binary(cob_scene *scene, const void *bytes, unsigned long count, unsigned long start_offset);
int cob_scan_chunks_memory(const void *bytes, unsigned long count, cob_chunk_info *out_chunks, unsigned long chunk_capacity, unsigned long *out_count);

int cob_encode_ascii(const cob_scene *scene, cob_write_fn write_fn, void *user);
int cob_encode_binary(const cob_scene *scene, cob_write_fn write_fn, void *user);
int cob_export_obj(const cob_scene *scene, cob_write_fn write_fn, void *user);



/* Extra modules: validation, repair, triangulation, bounds, fixed normals, transforms. */
enum {
    COB_REPAIR_ADD_DEFAULT_UV = 1U,
    COB_REPAIR_CLAMP_INDICES = 2U,
    COB_REPAIR_FILL_EMPTY_NAMES = 4U
};


/* EXTREME modules: smoother runtime extraction, procedural shader sampling, better transforms. */
int cob_scene_quality_report(const cob_scene *scene, cob_quality_report *report);
int cob_scene_build_vertex_normals(const cob_scene *scene, cob_vec3q *out_normals, unsigned long normal_capacity);
int cob_export_smooth_tri_obj(const cob_scene *scene, cob_write_fn write_fn, void *user);
int cob_save_smooth_tri_obj_file(const char *path, const cob_scene *scene);
int cob_shader_tree_sample_material(const cob_scene *scene, cob_i32 mesh_id, cob_i16 matnum, const cob_vec2q *uv, const cob_vec3q *pos, cob_shader_sample *out_sample);
int cob_anim_make_euler_xyz_matrix(cob_fixed x_deg, cob_fixed y_deg, cob_fixed z_deg, cob_fixed out_transform[16]);
int cob_anim_make_quat_matrix(const cob_fixed quat_xyzw[4], cob_fixed out_transform[16]);
int cob_anim_quat_nlerp(const cob_fixed a_xyzw[4], const cob_fixed b_xyzw[4], cob_fixed t, cob_fixed out_xyzw[4]);
int cob_anim_sample_pose(const cob_scene *scene, cob_fixed time, cob_anim_pose *out_poses, unsigned long pose_capacity, unsigned long *out_count);
int cob_nurbs_patch_validate(const cob_nurbs_patch *patch);
int cob_nurbs_patch_make_open_uniform(cob_nurbs_patch *patch, cob_u16 u_degree, cob_u16 v_degree, cob_u16 u_count, cob_u16 v_count);
int cob_nurbs_patch_prepare_ts_bezier_like(cob_nurbs_patch *patch, cob_u16 u_count, cob_u16 v_count);
int cob_metaball_field_normal(const cob_metaball_field *field, const cob_vec3q *p, cob_vec3q *out_normal);
int cob_metaball_field_polygonize_smooth_to_scene(cob_scene *scene, const cob_metaball_field *field, const char *mesh_name, unsigned int grid);

int cob_scene_validate(const cob_scene *scene, cob_scene_stats *stats);
int cob_scene_repair(cob_scene *scene, unsigned int flags);
int cob_mesh_compute_bounds(const cob_scene *scene, unsigned long mesh_index, cob_bounds *bounds);
int cob_scene_compute_bounds(const cob_scene *scene, cob_bounds *bounds);
int cob_scene_build_triangles(const cob_scene *scene, cob_triangle *out_triangles, unsigned long triangle_capacity, unsigned long *out_count);
int cob_scene_build_triangles_ex(const cob_scene *scene, cob_triangle *out_triangles, unsigned long triangle_capacity, unsigned long *out_count, cob_triangulate_stats *stats);
int cob_scene_build_face_normals(const cob_scene *scene, cob_vec3q *out_normals, unsigned long normal_capacity);
int cob_export_tri_obj(const cob_scene *scene, cob_write_fn write_fn, void *user);
int cob_export_tri_obj_mtl(const cob_scene *scene, cob_write_fn write_fn, void *user, const char *mtl_name);
int cob_save_tri_obj_file(const char *path, const cob_scene *scene);
int cob_save_tri_obj_mtl_file(const char *path, const cob_scene *scene, const char *mtl_name);
int cob_shader_tree_eval_material(const cob_scene *scene, cob_i32 mesh_id, cob_i16 matnum, cob_shader_eval *out_eval);
int cob_anim_sample_channel(const cob_scene *scene, unsigned long channel_index, cob_fixed time, cob_fixed out_value[4]);
int cob_anim_apply_sample_to_transform(cob_fixed transform[16], cob_u8 semantic, const cob_fixed value[4]);
int cob_nurbs_patch_eval(const cob_nurbs_patch *patch, cob_fixed u, cob_fixed v, cob_vec3q *out_point);
int cob_nurbs_patch_tessellate_to_scene(cob_scene *scene, const cob_nurbs_patch *patch, const char *mesh_name, unsigned int u_steps, unsigned int v_steps);
void cob_metaball_field_clear(cob_metaball_field *field);
int cob_metaball_field_add(cob_metaball_field *field, const cob_vec3q *center, cob_fixed radius, cob_fixed strength);
cob_fixed cob_metaball_field_eval(const cob_metaball_field *field, const cob_vec3q *p);
int cob_metaball_field_polygonize_to_scene(cob_scene *scene, const cob_metaball_field *field, const char *mesh_name, unsigned int grid);
void cob_transform_vec3(cob_vec3q *out, const cob_fixed transform[16], const cob_vec3q *in);
int cob_scene_make_world_copy(cob_scene *dst, const cob_scene *src);
int cob_export_mtl(const cob_scene *scene, cob_write_fn write_fn, void *user);
int cob_export_obj_mtl(const cob_scene *scene, cob_write_fn write_fn, void *user, const char *mtl_name);
int cob_save_mtl_file(const char *path, const cob_scene *scene);
int cob_save_obj_mtl_file(const char *path, const cob_scene *scene, const char *mtl_name);


/* APEX modules: layer capture, hierarchy world baking, tangent frames and runtime reports. */
int cob_scene_build_hierarchy_report(const cob_scene *scene, cob_hierarchy_report *report);
int cob_scene_make_hierarchical_world_copy(cob_scene *dst, const cob_scene *src);
int cob_scene_build_tangent_frames(const cob_scene *scene, cob_tangent_frame *out_frames, unsigned long frame_capacity, unsigned long *out_count);
int cob_scene_runtime_report(const cob_scene *scene, cob_runtime_report *report);
int cob_export_tangent_obj(const cob_scene *scene, cob_write_fn write_fn, void *user);
int cob_save_tangent_obj_file(const char *path, const cob_scene *scene);
int cob_export_anim_csv(const cob_scene *scene, cob_write_fn write_fn, void *user);
int cob_save_anim_csv_file(const char *path, const cob_scene *scene);
int cob_export_report_txt(const cob_scene *scene, cob_write_fn write_fn, void *user);
int cob_save_report_txt_file(const char *path, const cob_scene *scene);


/* OMEGA helpers: feature reports, unit bake, visible filtering and engine export. */
int cob_scene_is_object_hidden(const cob_scene *scene, cob_i32 object_id);
int cob_scene_feature_report(const cob_scene *scene, cob_feature_report *report);
int cob_scene_apply_units(cob_scene *dst, const cob_scene *src);
int cob_scene_filter_hidden(cob_scene *dst, const cob_scene *src);
int cob_scene_make_engine_ready_copy(cob_scene *dst, const cob_scene *src, unsigned int flags, cob_engine_report *report);
int cob_export_manifest_json(const cob_scene *scene, cob_write_fn write_fn, void *user);
int cob_export_feature_report_txt(const cob_scene *scene, cob_write_fn write_fn, void *user);
int cob_export_engine_obj(const cob_scene *scene, unsigned int flags, cob_write_fn write_fn, void *user);
int cob_save_manifest_json_file(const char *path, const cob_scene *scene);
int cob_save_feature_report_file(const char *path, const cob_scene *scene);
int cob_save_engine_obj_file(const char *path, const cob_scene *scene, unsigned int flags);

/* Optional stdio helpers in cob_fileio.c: caller supplies the whole file buffer. */
int cob_load_file(const char *path, void *io_buffer, unsigned long io_capacity, cob_scene *scene);
int cob_save_ascii_file(const char *path, const cob_scene *scene);
int cob_save_binary_file(const char *path, const cob_scene *scene);
int cob_save_obj_file(const char *path, const cob_scene *scene);

/* Fixed-point helpers. */
cob_fixed cob_fixed_from_int(long v);
long cob_fixed_to_int(cob_fixed v);
int cob_fixed_to_cstr(cob_fixed v, char *out, unsigned long out_size, int decimals);
cob_fixed cob_fixed_parse_token(const char **cursor, const char *end, int *ok);
cob_fixed cob_fixed_from_ieee754_le(const unsigned char *p, int *ok);
void cob_fixed_to_ieee754_le(cob_fixed v, unsigned char *p);

#ifdef __cplusplus
}
#endif

#endif
