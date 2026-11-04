#ifndef B3D_C89_H
#define B3D_C89_H

/* b3d_c89: Blitz3D .b3d parser for strict/static C89 engines.
   - No heap ownership or C real scalar arithmetic inside the codec.
   - All real values from the file are converted from IEEE-754 binary32
     into signed Q16.16 fixed point.
   - Parser reads from caller-provided memory.
   - v15 keeps the Blitz Brush-map bake path, baked skin slots, render queues,
     collision flattening/BVH/raycast/capsule/static-intersection utilities,
     and adds Blitz MeshModel-style transform/flip, BVH leaf-cache helpers,
     and fixed Animator-style time stepping.
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef signed char        b3d_i8;
typedef unsigned char      b3d_u8;
typedef short              b3d_i16;
typedef unsigned short     b3d_u16;
typedef int               b3d_i32;
typedef unsigned int      b3d_u32;

typedef b3d_i32 b3d_fixed; /* Q16.16 */

#define B3D_FIXED_ONE ((b3d_fixed)65536L)
#define B3D_NAME_MAX 64
#define B3D_TEX_SLOTS_MAX 8
#define B3D_CHUNK_DEPTH_MAX 64
#define B3D_C89_VERSION_MAJOR 15
#define B3D_C89_VERSION_MINOR 0
#define B3D_C89_VERSION_STRING "v15-blitz-runtime-polish-overdrive"

#ifndef B3D_MAX_TEXTURES
#define B3D_MAX_TEXTURES 64
#endif
#ifndef B3D_MAX_BRUSHES
#define B3D_MAX_BRUSHES 64
#endif
#ifndef B3D_MAX_VERTICES
#define B3D_MAX_VERTICES 8192
#endif
#ifndef B3D_MAX_TRIS
#define B3D_MAX_TRIS 16384
#endif
#ifndef B3D_MAX_SURFACES
#define B3D_MAX_SURFACES 1024
#endif
#ifndef B3D_MAX_MESHES
#define B3D_MAX_MESHES 128
#endif
#ifndef B3D_MAX_NODES
#define B3D_MAX_NODES 256
#endif
#ifndef B3D_MAX_BONE_WEIGHTS
#define B3D_MAX_BONE_WEIGHTS 8192
#endif
#ifndef B3D_MAX_KEYS
#define B3D_MAX_KEYS 2048
#endif
#ifndef B3D_MAX_ANIMS
#define B3D_MAX_ANIMS 32
#endif

#define B3D_BLITZ_MAX_VERTEX_INFLUENCES 4
#define B3D_BLITZ_TEXCOORD_SETS 2
#define B3D_BLITZ_NO_BONE 255
#define B3D_BLITZ_COLLISION_LEAF_MAX 16
#define B3D_BLITZ_BVH_PAIR_STACK_MAX 128

enum {
    B3D_OK = 0,
    B3D_ERR_NULL = -1,
    B3D_ERR_EOF = -2,
    B3D_ERR_BAD_MAGIC = -3,
    B3D_ERR_BAD_VERSION = -4,
    B3D_ERR_OVERFLOW = -5,
    B3D_ERR_BAD_CHUNK = -6,
    B3D_ERR_BAD_VALUE = -7,
    B3D_ERR_BAD_INDEX = -8,
    B3D_ERR_TOO_DEEP = -9
};

enum {
    B3D_NODE_PIVOT = 0,
    B3D_NODE_MESH = 1,
    B3D_NODE_BONE = 2
};

enum {
    B3D_VRTS_NORMALS = 1,
    B3D_VRTS_COLORS = 2
};

enum {
    B3D_KEYS_POSITION = 1,
    B3D_KEYS_SCALE = 2,
    B3D_KEYS_ROTATION = 4
};

/* Blitz3D Animator modes. */
enum {
    B3D_ANIM_MODE_STOP = 0,
    B3D_ANIM_MODE_LOOP = 1,
    B3D_ANIM_MODE_PINGPONG = 2,
    B3D_ANIM_MODE_ONESHOT = 3
};

/* Blitz3D texture flag bits commonly emitted in TEXS chunks. */
enum {
    B3D_TEX_COLOR = 1,
    B3D_TEX_ALPHA = 2,
    B3D_TEX_MASKED = 4,
    B3D_TEX_MIPMAP = 8,
    B3D_TEX_CLAMPU = 16,
    B3D_TEX_CLAMPV = 32,
    B3D_TEX_SPHERE = 64,
    B3D_TEX_CUBIC = 128,
    B3D_TEX_VRAM = 256,
    B3D_TEX_HIGHCOLOR = 512,
    B3D_TEX_FLAGS_MASK = 0x0000ffffL,
    B3D_TEX_COORDS2 = 0x00010000L
};

/* Blend modes used by Blitz3D textures/brushes. Kept as raw integer values. */
enum {
    B3D_BLEND_NONE = 0,
    B3D_BLEND_ALPHA = 1,
    B3D_BLEND_MULTIPLY = 2,
    B3D_BLEND_ADD = 3,
    B3D_BLEND_DOT3 = 4,
    B3D_BLEND_MULTIPLY2 = 5
};

enum {
    B3D_BLITZ_QUEUE_OPAQUE = 0,
    B3D_BLITZ_QUEUE_TRANSPARENT = 1
};

enum {
    B3D_BLITZ_AXIS_X = 0,
    B3D_BLITZ_AXIS_Y = 1,
    B3D_BLITZ_AXIS_Z = 2
};

/* Blitz3D / gxScene FX bits useful for brush render-key approximation. */
enum {
    B3D_FX_FULLBRIGHT = 0x0001,
    B3D_FX_VERTEXCOLOR = 0x0002,
    B3D_FX_FLATSHADED = 0x0004,
    B3D_FX_NOFOG = 0x0008,
    B3D_FX_DOUBLESIDED = 0x0010,
    B3D_FX_VERTEXALPHA = 0x0020,
    B3D_FX_ALPHATEST = 0x2000,
    B3D_FX_CONDLIGHT = 0x4000,
    B3D_FX_EMISSIVE = 0x8000
};

typedef struct b3d_vec3_fixed_s {
    b3d_fixed x, y, z;
} b3d_vec3_fixed;

typedef struct b3d_quat_fixed_s {
    b3d_fixed w, x, y, z;
} b3d_quat_fixed;

typedef struct b3d_texture_s {
    char file[B3D_NAME_MAX];
    b3d_i32 flags;      /* Blitz runtime flags: raw_flags & 0xffff */
    b3d_i32 raw_flags;  /* original TEXS flags field from the file */
    b3d_i32 coord_set;  /* 0 = first UV set, 1 = second UV set (0x10000) */
    b3d_i32 blend;
    b3d_fixed x_pos, y_pos;
    b3d_fixed x_scale, y_scale;
    b3d_fixed rotation;
} b3d_texture;

typedef struct b3d_brush_s {
    char name[B3D_NAME_MAX];
    b3d_fixed red, green, blue, alpha;
    b3d_u32 color_argb;
    b3d_fixed shininess;
    b3d_i32 blend;
    b3d_i32 fx;
    b3d_i32 texture_id[B3D_TEX_SLOTS_MAX];
    b3d_i32 n_texs;
} b3d_brush;

typedef struct b3d_vertex_s {
    b3d_fixed x, y, z;
    b3d_fixed nx, ny, nz;
    b3d_fixed red, green, blue, alpha;
    b3d_u32 color_argb;
    b3d_fixed uv[B3D_TEX_SLOTS_MAX][4];
    b3d_i32 tex_sets;
    b3d_i32 tex_set_size;
    b3d_i32 flags;
} b3d_vertex;

typedef struct b3d_tri_s {
    b3d_i32 v[3];       /* stored as file-local index inside the enclosing mesh */
    b3d_i32 brush_id;   /* -1 means no brush */
    b3d_i32 mesh_id;
} b3d_tri;

typedef struct b3d_mesh_s {
    b3d_i32 brush_id;   /* -1 means no brush */
    b3d_i32 node_id;    /* owner NODE; this is Blitz surface bone 0 */
    b3d_i32 vertex_first;
    b3d_i32 vertex_count;
    b3d_i32 tri_first;
    b3d_i32 tri_count;
    b3d_i32 vertex_flags;      /* OR of all VRTS flags encountered in this MESH */
    b3d_i32 last_vrts_flags;   /* Blitz readMesh() returns the last VRTS flags value */
    b3d_i32 tex_sets;
    b3d_i32 tex_set_size;
    b3d_i32 generated_normals; /* 1 when normals were rebuilt because VRTS lacked them */
    b3d_i32 surface_first;
    b3d_i32 surface_count;
    b3d_i32 blitz_bone_count;  /* BONE nodes scoped to this mesh, excluding mesh bone 0 */
} b3d_mesh;

typedef struct b3d_surface_s {
    b3d_i32 mesh_id;
    b3d_i32 brush_id;   /* representative TRIS brush id; -1 means no brush */
    b3d_i32 tri_first;  /* first triangle seen for this surface; triangles may be non-contiguous */
    b3d_i32 tri_count;
} b3d_surface;

typedef struct b3d_bone_weight_s {
    b3d_i32 node_id;
    b3d_i32 vertex_id;       /* global vertex index resolved through the active mesh */
    b3d_i32 mesh_id;
    b3d_i32 local_vertex_id; /* raw BONE vertex index used by Blitz MeshLoader::addBone */
    b3d_i32 blitz_bone_id;   /* mesh bone is 0; first BONE node is 1 */
    b3d_fixed weight;
} b3d_bone_weight;

typedef struct b3d_key_s {
    b3d_i32 node_id;
    b3d_i32 flags;
    b3d_i32 frame;
    b3d_vec3_fixed position;
    b3d_vec3_fixed scale;
    b3d_quat_fixed rotation;
} b3d_key;

typedef struct b3d_anim_s {
    b3d_i32 node_id;
    b3d_i32 flags;
    b3d_i32 frames;
    b3d_fixed fps;
} b3d_anim;

typedef struct b3d_node_s {
    char name[B3D_NAME_MAX];
    b3d_i32 parent;
    b3d_i32 first_child;
    b3d_i32 next_sibling;
    b3d_i32 kind;
    b3d_i32 mesh_id;
    b3d_i32 owner_mesh_id;
    b3d_i32 blitz_bone_id;
    b3d_i32 bone_weight_first;
    b3d_i32 bone_weight_count;
    b3d_i32 key_first;
    b3d_i32 key_count;
    b3d_i32 anim_id;
    b3d_vec3_fixed position;
    b3d_vec3_fixed scale;
    b3d_quat_fixed rotation;
} b3d_node;



typedef struct b3d_mat4_fixed_s {
    b3d_fixed m[16]; /* column-major: m[col*4+row], OpenGL-style */
} b3d_mat4_fixed;

typedef struct b3d_skinned_vertex_s {
    b3d_i32 source_vertex_id;
    b3d_vec3_fixed position;
    b3d_vec3_fixed normal;
    b3d_fixed weight_sum;
    b3d_i32 influence_count;
} b3d_skinned_vertex;

typedef struct b3d_blitz_surface_s {
    b3d_i32 mesh_id;
    b3d_i32 brush_id;
    b3d_i32 vertex_first;
    b3d_i32 vertex_count;
    b3d_i32 tri_first;
    b3d_i32 tri_count;
} b3d_blitz_surface;

typedef struct b3d_blitz_surface_vertex_s {
    b3d_i32 source_vertex_id;
    b3d_i32 source_local_vertex_id;
    b3d_i32 bone_node_id[B3D_BLITZ_MAX_VERTEX_INFLUENCES]; /* model node ids */
    b3d_i32 bone_id[B3D_BLITZ_MAX_VERTEX_INFLUENCES];      /* Blitz surface bone slots; 255 = none */
    b3d_fixed bone_weight[B3D_BLITZ_MAX_VERTEX_INFLUENCES];
    b3d_i32 bone_count;
    b3d_fixed bone_weight_sum;
    b3d_vertex vertex;
} b3d_blitz_surface_vertex;

typedef struct b3d_blitz_surface_tri_s {
    b3d_i32 source_tri_id;
    b3d_i32 v[3];
} b3d_blitz_surface_tri;

typedef struct b3d_blitz_brush_key_s {
    b3d_i32 source_brush_id;
    b3d_fixed red, green, blue, alpha;
    b3d_u32 color_argb;
    b3d_fixed shininess;
    b3d_i32 requested_blend;
    b3d_i32 render_blend;
    b3d_i32 fx;
    b3d_i32 render_fx;
    b3d_i32 max_tex;
    b3d_i32 texture_id[B3D_TEX_SLOTS_MAX];
    char texture_file[B3D_TEX_SLOTS_MAX][B3D_NAME_MAX];
    b3d_i32 texture_flags[B3D_TEX_SLOTS_MAX];
    b3d_i32 texture_raw_flags[B3D_TEX_SLOTS_MAX];
    b3d_i32 texture_coord_set[B3D_TEX_SLOTS_MAX];
    b3d_i32 texture_blend[B3D_TEX_SLOTS_MAX];
    b3d_fixed texture_x_pos[B3D_TEX_SLOTS_MAX];
    b3d_fixed texture_y_pos[B3D_TEX_SLOTS_MAX];
    b3d_fixed texture_x_scale[B3D_TEX_SLOTS_MAX];
    b3d_fixed texture_y_scale[B3D_TEX_SLOTS_MAX];
    b3d_fixed texture_rotation[B3D_TEX_SLOTS_MAX];
} b3d_blitz_brush_key;

typedef struct b3d_blitz_draw_item_s {
    b3d_i32 surface_id;      /* index inside the caller supplied baked surface array */
    b3d_i32 queue;           /* B3D_BLITZ_QUEUE_* */
    b3d_i32 mesh_id;
    b3d_i32 brush_id;
    b3d_i32 vertex_first;
    b3d_i32 vertex_count;
    b3d_i32 tri_first;
    b3d_i32 tri_count;
    b3d_blitz_brush_key render_key;
} b3d_blitz_draw_item;

typedef struct b3d_bounds_s {
    b3d_i32 valid;
    b3d_vec3_fixed min_v;
    b3d_vec3_fixed max_v;
} b3d_bounds;

typedef struct b3d_blitz_collision_vertex_s {
    b3d_i32 source_surface_id;
    b3d_i32 source_vertex_id;
    b3d_i32 source_local_vertex_id;
    b3d_vec3_fixed position;
} b3d_blitz_collision_vertex;

typedef struct b3d_blitz_collision_tri_s {
    b3d_i32 source_surface_id;
    b3d_i32 source_tri_id;
    b3d_i32 source_surface_tri_id;
    b3d_i32 v[3];
} b3d_blitz_collision_tri;

typedef struct b3d_blitz_collision_node_s {
    b3d_bounds bounds;
    b3d_i32 left_child;
    b3d_i32 right_child;
    b3d_i32 tri_first;
    b3d_i32 tri_count;
    b3d_i32 axis;
} b3d_blitz_collision_node;

typedef struct b3d_blitz_collision_hit_s {
    b3d_i32 tri_id;
    b3d_i32 source_surface_id;
    b3d_i32 source_tri_id;
    b3d_i32 source_surface_tri_id;
    b3d_fixed t;              /* segment fraction, 0..1 in Q16.16 */
    b3d_fixed u;              /* triangle barycentric u */
    b3d_fixed v;              /* triangle barycentric v */
    b3d_fixed distance_sq;    /* Q16.16 squared center/triangle distance; zero for direct ray hits */
    b3d_vec3_fixed position;
    b3d_vec3_fixed normal;
} b3d_blitz_collision_hit;

typedef struct b3d_blitz_collision_pair_s {
    b3d_i32 tri_id_a;
    b3d_i32 tri_id_b;
    b3d_i32 source_surface_id_a;
    b3d_i32 source_surface_id_b;
    b3d_i32 source_tri_id_a;
    b3d_i32 source_tri_id_b;
    b3d_i32 source_surface_tri_id_a;
    b3d_i32 source_surface_tri_id_b;
} b3d_blitz_collision_pair;

typedef struct b3d_blitz_collision_leaf_s {
    b3d_i32 node_id;
    b3d_i32 tri_first;
    b3d_i32 tri_count;
    b3d_bounds bounds;
} b3d_blitz_collision_leaf;

typedef struct b3d_blitz_collision_leaf_pair_s {
    b3d_i32 leaf_id_a;
    b3d_i32 leaf_id_b;
    b3d_i32 node_id_a;
    b3d_i32 node_id_b;
} b3d_blitz_collision_leaf_pair;

typedef void (*b3d_skin_vertex_callback)(void *user, const b3d_skinned_vertex *v);

typedef struct b3d_pose_fixed_s {
    b3d_i32 flags;
    b3d_vec3_fixed position;
    b3d_vec3_fixed scale;
    b3d_quat_fixed rotation;
} b3d_pose_fixed;

typedef struct b3d_anim_state_s {
    b3d_i32 mode;
    b3d_i32 seq;
    b3d_i32 frames;
    b3d_i32 animating;
    b3d_fixed time;
    b3d_fixed speed;
} b3d_anim_state;

typedef struct b3d_model_s {
    b3d_i32 version;
    b3d_texture textures[B3D_MAX_TEXTURES];
    b3d_brush brushes[B3D_MAX_BRUSHES];
    b3d_vertex vertices[B3D_MAX_VERTICES];
    b3d_tri tris[B3D_MAX_TRIS];
    b3d_mesh meshes[B3D_MAX_MESHES];
    b3d_surface surfaces[B3D_MAX_SURFACES];
    b3d_node nodes[B3D_MAX_NODES];
    b3d_bone_weight bone_weights[B3D_MAX_BONE_WEIGHTS];
    b3d_key keys[B3D_MAX_KEYS];
    b3d_anim anims[B3D_MAX_ANIMS];
    b3d_i32 texture_count;
    b3d_i32 brush_count;
    b3d_i32 vertex_count;
    b3d_i32 tri_count;
    b3d_i32 mesh_count;
    b3d_i32 surface_count;
    b3d_i32 node_count;
    b3d_i32 bone_weight_count;
    b3d_i32 key_count;
    b3d_i32 anim_count;
    b3d_i32 chunk_count;
    b3d_i32 unknown_chunk_count;
    b3d_i32 max_depth;
    b3d_i32 generated_normal_count;
} b3d_model;

void b3d_model_clear(b3d_model *m);
int b3d_load_memory(const b3d_u8 *data, b3d_u32 size, b3d_model *out_model);
int b3d_validate_model(const b3d_model *m);
const char *b3d_error_text(int err);

b3d_fixed b3d_fixed_from_int(b3d_i32 v);
b3d_i32 b3d_fixed_to_int_round(b3d_fixed v);
b3d_i32 b3d_fixed_to_int_floor(b3d_fixed v);


b3d_fixed b3d_fixed_mul(b3d_fixed a, b3d_fixed b);
b3d_fixed b3d_fixed_div(b3d_fixed a, b3d_fixed b);
b3d_fixed b3d_fixed_lerp(b3d_fixed a, b3d_fixed b, b3d_fixed t);

b3d_fixed b3d_vec3_dot(b3d_vec3_fixed a, b3d_vec3_fixed b);
b3d_vec3_fixed b3d_vec3_sub(b3d_vec3_fixed a, b3d_vec3_fixed b);
void b3d_vec3_cross(b3d_vec3_fixed a, b3d_vec3_fixed b, b3d_vec3_fixed *out_v);
void b3d_vec3_normalize(b3d_vec3_fixed *v);
b3d_vec3_fixed b3d_vec3_lerp(b3d_vec3_fixed a, b3d_vec3_fixed b, b3d_fixed t);

void b3d_quat_identity(b3d_quat_fixed *q);
b3d_fixed b3d_quat_dot(b3d_quat_fixed a, b3d_quat_fixed b);
void b3d_quat_normalize(b3d_quat_fixed *q);
void b3d_quat_slerp(b3d_quat_fixed a, b3d_quat_fixed b, b3d_fixed t, b3d_quat_fixed *out_q);

void b3d_mat4_identity(b3d_mat4_fixed *out_m);
void b3d_mat4_mul(const b3d_mat4_fixed *a, const b3d_mat4_fixed *b, b3d_mat4_fixed *out_m);
void b3d_mat4_from_pose(const b3d_pose_fixed *pose, b3d_mat4_fixed *out_m);
void b3d_mat4_from_inverse_pose(const b3d_pose_fixed *pose, b3d_mat4_fixed *out_m);
void b3d_mat4_transform_point(const b3d_mat4_fixed *m, const b3d_vec3_fixed *in_v, b3d_vec3_fixed *out_v);
void b3d_mat4_transform_vector(const b3d_mat4_fixed *m, const b3d_vec3_fixed *in_v, b3d_vec3_fixed *out_v);
void b3d_mat4_transform_normal_cofactor(const b3d_mat4_fixed *m, const b3d_vec3_fixed *in_v, b3d_vec3_fixed *out_v);

int b3d_sample_node_pose_lerp(const b3d_model *m, b3d_i32 node_id, b3d_fixed frame_q16, b3d_pose_fixed *out_pose);
void b3d_anim_state_clear(b3d_anim_state *state);
int b3d_anim_state_set_time(b3d_anim_state *state, b3d_i32 frames, b3d_fixed time_q16, b3d_i32 seq);
int b3d_anim_state_animate(b3d_anim_state *state, b3d_i32 mode, b3d_fixed speed_q16, b3d_i32 frames, b3d_i32 seq);
int b3d_anim_state_update(b3d_anim_state *state, b3d_fixed elapsed_q16);
int b3d_build_local_pose_stack_anim_state(const b3d_model *m, const b3d_anim_state *state, b3d_pose_fixed *out_local_poses, b3d_i32 max_poses);
int b3d_build_local_pose_stack(const b3d_model *m, b3d_fixed frame_q16, b3d_pose_fixed *out_local_poses, b3d_i32 max_poses);
int b3d_build_world_matrices_from_poses(const b3d_model *m, const b3d_pose_fixed *local_poses, b3d_i32 pose_count, b3d_mat4_fixed *out_world, b3d_i32 max_mats);
int b3d_build_bind_world_matrices(const b3d_model *m, b3d_mat4_fixed *out_bind_world, b3d_i32 max_mats);
int b3d_build_pose_world_matrices(const b3d_model *m, b3d_fixed frame_q16, b3d_mat4_fixed *out_pose_world, b3d_i32 max_mats);
int b3d_build_skin_matrices(const b3d_model *m, b3d_fixed frame_q16, b3d_mat4_fixed *out_skin_mats, b3d_i32 max_mats);
int b3d_skin_vertex_cpu(const b3d_model *m, b3d_i32 vertex_id, const b3d_mat4_fixed *skin_mats, b3d_i32 skin_mat_count, b3d_skinned_vertex *out_v);
int b3d_skin_vertex_cpu_blitz(const b3d_model *m, b3d_i32 vertex_id, const b3d_mat4_fixed *skin_mats, b3d_i32 skin_mat_count, b3d_skinned_vertex *out_v);
int b3d_skin_mesh_cpu_callback(const b3d_model *m, b3d_i32 mesh_id, b3d_fixed frame_q16, b3d_skin_vertex_callback cb, void *user);
int b3d_skin_mesh_cpu_callback_blitz(const b3d_model *m, b3d_i32 mesh_id, b3d_fixed frame_q16, b3d_skin_vertex_callback cb, void *user);

int b3d_str_equal(const char *a, const char *b);
b3d_i32 b3d_find_node(const b3d_model *m, const char *name);
b3d_i32 b3d_find_texture(const b3d_model *m, const char *file);
b3d_i32 b3d_find_brush(const b3d_model *m, const char *name);

b3d_i32 b3d_find_mesh_for_vertex(const b3d_model *m, b3d_i32 vertex_id);
b3d_i32 b3d_find_surface(const b3d_model *m, b3d_i32 mesh_id, b3d_i32 brush_id);
int b3d_mesh_local_vertex_to_global(const b3d_model *m, b3d_i32 mesh_id, b3d_i32 local_index, b3d_i32 *out_global);
int b3d_tri_get_global_indices(const b3d_model *m, b3d_i32 tri_id, b3d_i32 out_v[3]);
int b3d_vertex_get_blitz_texcoord(const b3d_vertex *v, b3d_i32 set, b3d_fixed *out_u, b3d_fixed *out_v);
int b3d_count_surface_unique_vertices(const b3d_model *m, b3d_i32 surface_id, b3d_i32 *out_count);
int b3d_rebuild_mesh_normals(b3d_model *m, b3d_i32 mesh_id);
int b3d_rebuild_missing_normals(b3d_model *m);
void b3d_blitz_surface_vertex_init(b3d_blitz_surface_vertex *v);
int b3d_fill_blitz_surface_vertex_bones(const b3d_model *m, b3d_i32 vertex_id, b3d_blitz_surface_vertex *out_v);
int b3d_skin_blitz_surface_vertex_cpu(const b3d_blitz_surface_vertex *v, b3d_i32 fallback_node_id,
                                      const b3d_mat4_fixed *skin_mats, b3d_i32 skin_mat_count,
                                      b3d_skinned_vertex *out_v);
int b3d_skin_blitz_surface_vertices_cpu(const b3d_blitz_surface_vertex *src_vertices,
                                        b3d_i32 vertex_count,
                                        b3d_i32 fallback_node_id,
                                        const b3d_mat4_fixed *skin_mats,
                                        b3d_i32 skin_mat_count,
                                        b3d_blitz_surface_vertex *out_vertices,
                                        b3d_i32 max_vertices,
                                        b3d_i32 *out_vertex_count);
int b3d_build_blitz_surfaces(const b3d_model *m,
                             b3d_blitz_surface *out_surfaces, b3d_i32 max_surfaces,
                             b3d_blitz_surface_vertex *out_vertices, b3d_i32 max_vertices,
                             b3d_blitz_surface_tri *out_tris, b3d_i32 max_tris,
                             b3d_i32 *out_surface_count,
                             b3d_i32 *out_vertex_count,
                             b3d_i32 *out_tri_count);
int b3d_build_blitz_surfaces_merged(const b3d_model *m,
                                    b3d_blitz_surface *out_surfaces, b3d_i32 max_surfaces,
                                    b3d_blitz_surface_vertex *out_vertices, b3d_i32 max_vertices,
                                    b3d_blitz_surface_tri *out_tris, b3d_i32 max_tris,
                                    b3d_i32 *out_surface_count,
                                    b3d_i32 *out_vertex_count,
                                    b3d_i32 *out_tri_count);
int b3d_count_blitz_surface_bake(const b3d_model *m, b3d_i32 merge_equivalent_brushes,
                                 b3d_i32 *out_surface_count,
                                 b3d_i32 *out_vertex_count,
                                 b3d_i32 *out_tri_count);
int b3d_make_blitz_brush_key(const b3d_model *m, b3d_i32 brush_id, b3d_blitz_brush_key *out_key);
int b3d_compose_blitz_brush_keys(const b3d_blitz_brush_key *surface_key, const b3d_blitz_brush_key *entity_key, b3d_blitz_brush_key *out_key);
int b3d_make_blitz_render_brush_key(const b3d_model *m, b3d_i32 surface_brush_id, b3d_i32 entity_brush_id, b3d_blitz_brush_key *out_key);
int b3d_make_blitz_baked_surface_render_key(const b3d_model *m, const b3d_blitz_surface *surface, b3d_blitz_brush_key *out_key);
int b3d_blitz_brush_key_render_queue(const b3d_blitz_brush_key *key, b3d_i32 *out_queue);
int b3d_blitz_baked_surface_render_queue(const b3d_model *m, const b3d_blitz_surface *surface, b3d_i32 *out_queue);
int b3d_build_blitz_baked_surface_render_keys(const b3d_model *m,
                                              const b3d_blitz_surface *surfaces,
                                              b3d_i32 surface_count,
                                              b3d_blitz_brush_key *out_keys,
                                              b3d_i32 max_keys,
                                              b3d_i32 *out_key_count);
int b3d_count_blitz_baked_surface_render_queues(const b3d_model *m,
                                                const b3d_blitz_surface *surfaces,
                                                b3d_i32 surface_count,
                                                b3d_i32 *out_opaque_count,
                                                b3d_i32 *out_transparent_count);
int b3d_count_blitz_draw_list(const b3d_model *m,
                              const b3d_blitz_surface *surfaces,
                              b3d_i32 surface_count,
                              b3d_i32 queue_filter,
                              b3d_i32 *out_item_count);
int b3d_build_blitz_draw_list(const b3d_model *m,
                              const b3d_blitz_surface *surfaces,
                              b3d_i32 surface_count,
                              b3d_i32 queue_filter,
                              b3d_blitz_draw_item *out_items,
                              b3d_i32 max_items,
                              b3d_i32 *out_item_count);
int b3d_count_blitz_collision_mesh(const b3d_blitz_surface *surfaces,
                                   b3d_i32 surface_count,
                                   const b3d_blitz_surface_vertex *vertices,
                                   b3d_i32 vertex_count,
                                   const b3d_blitz_surface_tri *tris,
                                   b3d_i32 tri_count,
                                   b3d_i32 *out_vertex_count,
                                   b3d_i32 *out_tri_count);
int b3d_build_blitz_collision_mesh(const b3d_blitz_surface *surfaces,
                                   b3d_i32 surface_count,
                                   const b3d_blitz_surface_vertex *vertices,
                                   b3d_i32 vertex_count,
                                   const b3d_blitz_surface_tri *tris,
                                   b3d_i32 tri_count,
                                   b3d_blitz_collision_vertex *out_vertices,
                                   b3d_i32 max_vertices,
                                   b3d_blitz_collision_tri *out_tris,
                                   b3d_i32 max_tris,
                                   b3d_i32 *out_vertex_count,
                                   b3d_i32 *out_tri_count);
int b3d_transform_blitz_surface_vertices(const b3d_blitz_surface_vertex *src_vertices,
                                         b3d_i32 vertex_count,
                                         const b3d_mat4_fixed *matrix,
                                         b3d_blitz_surface_vertex *out_vertices,
                                         b3d_i32 max_vertices,
                                         b3d_i32 *out_vertex_count);
int b3d_flip_blitz_surface_vertices(const b3d_blitz_surface_vertex *src_vertices,
                                    b3d_i32 vertex_count,
                                    b3d_blitz_surface_vertex *out_vertices,
                                    b3d_i32 max_vertices,
                                    b3d_i32 *out_vertex_count);
int b3d_flip_blitz_surface_tris(const b3d_blitz_surface_tri *src_tris,
                                b3d_i32 tri_count,
                                b3d_blitz_surface_tri *out_tris,
                                b3d_i32 max_tris,
                                b3d_i32 *out_tri_count);
int b3d_compute_blitz_surface_bounds(const b3d_blitz_surface *surface,
                                    const b3d_blitz_surface_vertex *vertices,
                                    b3d_i32 vertex_count,
                                    b3d_bounds *out_bounds);
int b3d_compute_blitz_collision_bounds(const b3d_blitz_collision_vertex *vertices,
                                       b3d_i32 vertex_count,
                                       b3d_bounds *out_bounds);
int b3d_compute_blitz_collision_tri_bounds(const b3d_blitz_collision_vertex *vertices,
                                           b3d_i32 vertex_count,
                                           const b3d_blitz_collision_tri *tris,
                                           b3d_i32 tri_count,
                                           b3d_i32 tri_id,
                                           b3d_bounds *out_bounds);
int b3d_bounds_overlap(const b3d_bounds *a,
                       const b3d_bounds *b,
                       b3d_i32 *out_overlap);
int b3d_bounds_expand(b3d_bounds *bounds,
                      b3d_fixed amount);
int b3d_compute_segment_radius_bounds(const b3d_vec3_fixed *p0,
                                      const b3d_vec3_fixed *p1,
                                      b3d_fixed radius,
                                      b3d_bounds *out_bounds);
int b3d_intersect_segment_bounds(const b3d_vec3_fixed *p0,
                                 const b3d_vec3_fixed *p1,
                                 const b3d_bounds *bounds,
                                 b3d_fixed *out_enter_t,
                                 b3d_fixed *out_exit_t,
                                 b3d_i32 *out_hit);
int b3d_count_blitz_collision_bvh(b3d_i32 tri_count,
                                  b3d_i32 *out_node_count,
                                  b3d_i32 *out_index_count);
int b3d_build_blitz_collision_bvh(const b3d_blitz_collision_vertex *vertices,
                                  b3d_i32 vertex_count,
                                  const b3d_blitz_collision_tri *tris,
                                  b3d_i32 tri_count,
                                  b3d_i32 *out_tri_indices,
                                  b3d_i32 max_tri_indices,
                                  b3d_blitz_collision_node *out_nodes,
                                  b3d_i32 max_nodes,
                                  b3d_i32 *out_index_count,
                                  b3d_i32 *out_node_count);
int b3d_query_blitz_collision_bvh_bounds(const b3d_blitz_collision_node *nodes,
                                            b3d_i32 node_count,
                                            const b3d_i32 *tri_indices,
                                            b3d_i32 index_count,
                                            const b3d_bounds *query_bounds,
                                            b3d_i32 *out_tri_ids,
                                            b3d_i32 max_tri_ids,
                                            b3d_i32 *out_tri_count);
int b3d_count_blitz_collision_bvh_leaves(const b3d_blitz_collision_node *nodes,
                                         b3d_i32 node_count,
                                         b3d_i32 *out_leaf_count);
int b3d_build_blitz_collision_bvh_leaves(const b3d_blitz_collision_node *nodes,
                                         b3d_i32 node_count,
                                         b3d_blitz_collision_leaf *out_leaves,
                                         b3d_i32 max_leaves,
                                         b3d_i32 *out_leaf_count);
int b3d_query_blitz_collision_leaf_pairs(const b3d_blitz_collision_leaf *leaves_a,
                                         b3d_i32 leaf_count_a,
                                         const b3d_blitz_collision_leaf *leaves_b,
                                         b3d_i32 leaf_count_b,
                                         b3d_blitz_collision_leaf_pair *out_pairs,
                                         b3d_i32 max_pairs,
                                         b3d_i32 *out_pair_count);
int b3d_closest_point_on_blitz_collision_tri(const b3d_blitz_collision_vertex *vertices,
                                               b3d_i32 vertex_count,
                                               const b3d_blitz_collision_tri *tris,
                                               b3d_i32 tri_count,
                                               b3d_i32 tri_id,
                                               const b3d_vec3_fixed *point,
                                               b3d_vec3_fixed *out_point,
                                               b3d_fixed *out_u,
                                               b3d_fixed *out_v,
                                               b3d_fixed *out_distance_sq);
int b3d_intersect_blitz_collision_tri_segment(const b3d_blitz_collision_vertex *vertices,
                                              b3d_i32 vertex_count,
                                              const b3d_blitz_collision_tri *tris,
                                              b3d_i32 tri_count,
                                              b3d_i32 tri_id,
                                              const b3d_vec3_fixed *p0,
                                              const b3d_vec3_fixed *p1,
                                              b3d_i32 hit_backfaces,
                                              b3d_i32 *out_hit,
                                              b3d_blitz_collision_hit *out_result);
int b3d_query_blitz_collision_mesh_segment(const b3d_blitz_collision_vertex *vertices,
                                           b3d_i32 vertex_count,
                                           const b3d_blitz_collision_tri *tris,
                                           b3d_i32 tri_count,
                                           const b3d_vec3_fixed *p0,
                                           const b3d_vec3_fixed *p1,
                                           b3d_i32 hit_backfaces,
                                           b3d_blitz_collision_hit *out_hits,
                                           b3d_i32 max_hits,
                                           b3d_i32 *out_hit_count);
int b3d_raycast_blitz_collision_mesh_nearest(const b3d_blitz_collision_vertex *vertices,
                                             b3d_i32 vertex_count,
                                             const b3d_blitz_collision_tri *tris,
                                             b3d_i32 tri_count,
                                             const b3d_vec3_fixed *p0,
                                             const b3d_vec3_fixed *p1,
                                             b3d_i32 hit_backfaces,
                                             b3d_i32 *out_has_hit,
                                             b3d_blitz_collision_hit *out_hit);
int b3d_intersect_blitz_collision_tri_capsule(const b3d_blitz_collision_vertex *vertices,
                                              b3d_i32 vertex_count,
                                              const b3d_blitz_collision_tri *tris,
                                              b3d_i32 tri_count,
                                              b3d_i32 tri_id,
                                              const b3d_vec3_fixed *p0,
                                              const b3d_vec3_fixed *p1,
                                              b3d_fixed radius,
                                              b3d_i32 hit_backfaces,
                                              b3d_i32 *out_hit,
                                              b3d_blitz_collision_hit *out_result);
int b3d_query_blitz_collision_mesh_capsule(const b3d_blitz_collision_vertex *vertices,
                                           b3d_i32 vertex_count,
                                           const b3d_blitz_collision_tri *tris,
                                           b3d_i32 tri_count,
                                           const b3d_vec3_fixed *p0,
                                           const b3d_vec3_fixed *p1,
                                           b3d_fixed radius,
                                           b3d_i32 hit_backfaces,
                                           b3d_blitz_collision_hit *out_hits,
                                           b3d_i32 max_hits,
                                           b3d_i32 *out_hit_count);
int b3d_capsule_blitz_collision_mesh_nearest(const b3d_blitz_collision_vertex *vertices,
                                             b3d_i32 vertex_count,
                                             const b3d_blitz_collision_tri *tris,
                                             b3d_i32 tri_count,
                                             const b3d_vec3_fixed *p0,
                                             const b3d_vec3_fixed *p1,
                                             b3d_fixed radius,
                                             b3d_i32 hit_backfaces,
                                             b3d_i32 *out_has_hit,
                                             b3d_blitz_collision_hit *out_hit);
int b3d_query_blitz_collision_bvh_segment(const b3d_blitz_collision_node *nodes,
                                          b3d_i32 node_count,
                                          const b3d_i32 *tri_indices,
                                          b3d_i32 index_count,
                                          const b3d_blitz_collision_vertex *vertices,
                                          b3d_i32 vertex_count,
                                          const b3d_blitz_collision_tri *tris,
                                          b3d_i32 tri_count,
                                          const b3d_vec3_fixed *p0,
                                          const b3d_vec3_fixed *p1,
                                          b3d_i32 hit_backfaces,
                                          b3d_blitz_collision_hit *out_hits,
                                          b3d_i32 max_hits,
                                          b3d_i32 *out_hit_count);
int b3d_raycast_blitz_collision_bvh_nearest(const b3d_blitz_collision_node *nodes,
                                            b3d_i32 node_count,
                                            const b3d_i32 *tri_indices,
                                            b3d_i32 index_count,
                                            const b3d_blitz_collision_vertex *vertices,
                                            b3d_i32 vertex_count,
                                            const b3d_blitz_collision_tri *tris,
                                            b3d_i32 tri_count,
                                            const b3d_vec3_fixed *p0,
                                            const b3d_vec3_fixed *p1,
                                            b3d_i32 hit_backfaces,
                                            b3d_i32 *out_has_hit,
                                            b3d_blitz_collision_hit *out_hit);
int b3d_query_blitz_collision_bvh_capsule(const b3d_blitz_collision_node *nodes,
                                          b3d_i32 node_count,
                                          const b3d_i32 *tri_indices,
                                          b3d_i32 index_count,
                                          const b3d_blitz_collision_vertex *vertices,
                                          b3d_i32 vertex_count,
                                          const b3d_blitz_collision_tri *tris,
                                          b3d_i32 tri_count,
                                          const b3d_vec3_fixed *p0,
                                          const b3d_vec3_fixed *p1,
                                          b3d_fixed radius,
                                          b3d_i32 hit_backfaces,
                                          b3d_blitz_collision_hit *out_hits,
                                          b3d_i32 max_hits,
                                          b3d_i32 *out_hit_count);
int b3d_capsule_blitz_collision_bvh_nearest(const b3d_blitz_collision_node *nodes,
                                            b3d_i32 node_count,
                                            const b3d_i32 *tri_indices,
                                            b3d_i32 index_count,
                                            const b3d_blitz_collision_vertex *vertices,
                                            b3d_i32 vertex_count,
                                            const b3d_blitz_collision_tri *tris,
                                            b3d_i32 tri_count,
                                            const b3d_vec3_fixed *p0,
                                            const b3d_vec3_fixed *p1,
                                            b3d_fixed radius,
                                            b3d_i32 hit_backfaces,
                                            b3d_i32 *out_has_hit,
                                            b3d_blitz_collision_hit *out_hit);
int b3d_transform_bounds(const b3d_bounds *bounds,
                         const b3d_mat4_fixed *matrix,
                         b3d_bounds *out_bounds);
int b3d_transform_blitz_collision_vertices(const b3d_blitz_collision_vertex *src_vertices,
                                           b3d_i32 vertex_count,
                                           const b3d_mat4_fixed *matrix,
                                           b3d_blitz_collision_vertex *out_vertices,
                                           b3d_i32 max_vertices,
                                           b3d_i32 *out_vertex_count);
int b3d_refit_blitz_collision_bvh(const b3d_blitz_collision_vertex *vertices,
                                  b3d_i32 vertex_count,
                                  const b3d_blitz_collision_tri *tris,
                                  b3d_i32 tri_count,
                                  const b3d_i32 *tri_indices,
                                  b3d_i32 index_count,
                                  b3d_blitz_collision_node *nodes,
                                  b3d_i32 node_count);
int b3d_intersect_blitz_collision_tris(const b3d_blitz_collision_vertex *vertices_a,
                                       b3d_i32 vertex_count_a,
                                       const b3d_blitz_collision_tri *tris_a,
                                       b3d_i32 tri_count_a,
                                       b3d_i32 tri_id_a,
                                       const b3d_blitz_collision_vertex *vertices_b,
                                       b3d_i32 vertex_count_b,
                                       const b3d_blitz_collision_tri *tris_b,
                                       b3d_i32 tri_count_b,
                                       b3d_i32 tri_id_b,
                                       b3d_i32 hit_backfaces,
                                       b3d_i32 *out_intersects);
int b3d_query_blitz_collision_mesh_pairs(const b3d_blitz_collision_vertex *vertices_a,
                                         b3d_i32 vertex_count_a,
                                         const b3d_blitz_collision_tri *tris_a,
                                         b3d_i32 tri_count_a,
                                         const b3d_blitz_collision_vertex *vertices_b,
                                         b3d_i32 vertex_count_b,
                                         const b3d_blitz_collision_tri *tris_b,
                                         b3d_i32 tri_count_b,
                                         b3d_i32 hit_backfaces,
                                         b3d_blitz_collision_pair *out_pairs,
                                         b3d_i32 max_pairs,
                                         b3d_i32 *out_pair_count);
int b3d_query_blitz_collision_bvh_pairs(const b3d_blitz_collision_node *nodes_a,
                                        b3d_i32 node_count_a,
                                        const b3d_i32 *tri_indices_a,
                                        b3d_i32 index_count_a,
                                        const b3d_blitz_collision_vertex *vertices_a,
                                        b3d_i32 vertex_count_a,
                                        const b3d_blitz_collision_tri *tris_a,
                                        b3d_i32 tri_count_a,
                                        const b3d_blitz_collision_node *nodes_b,
                                        b3d_i32 node_count_b,
                                        const b3d_i32 *tri_indices_b,
                                        b3d_i32 index_count_b,
                                        const b3d_blitz_collision_vertex *vertices_b,
                                        b3d_i32 vertex_count_b,
                                        const b3d_blitz_collision_tri *tris_b,
                                        b3d_i32 tri_count_b,
                                        b3d_i32 hit_backfaces,
                                        b3d_blitz_collision_pair *out_pairs,
                                        b3d_i32 max_pairs,
                                        b3d_i32 *out_pair_count);
int b3d_intersect_blitz_collision_bvh(const b3d_blitz_collision_node *nodes_a,
                                      b3d_i32 node_count_a,
                                      const b3d_i32 *tri_indices_a,
                                      b3d_i32 index_count_a,
                                      const b3d_blitz_collision_vertex *vertices_a,
                                      b3d_i32 vertex_count_a,
                                      const b3d_blitz_collision_tri *tris_a,
                                      b3d_i32 tri_count_a,
                                      const b3d_blitz_collision_node *nodes_b,
                                      b3d_i32 node_count_b,
                                      const b3d_i32 *tri_indices_b,
                                      b3d_i32 index_count_b,
                                      const b3d_blitz_collision_vertex *vertices_b,
                                      b3d_i32 vertex_count_b,
                                      const b3d_blitz_collision_tri *tris_b,
                                      b3d_i32 tri_count_b,
                                      b3d_i32 hit_backfaces,
                                      b3d_i32 *out_intersects);
int b3d_blitz_brush_key_equal(const b3d_blitz_brush_key *a, const b3d_blitz_brush_key *b);
int b3d_blitz_brush_key_less(const b3d_blitz_brush_key *a, const b3d_blitz_brush_key *b);
b3d_i32 b3d_find_canonical_blitz_brush(const b3d_model *m, b3d_i32 brush_id);
int b3d_texture_matrix_2x3(const b3d_texture *t, b3d_fixed out_m[6]);
int b3d_texture_transform_uv(const b3d_texture *t, b3d_fixed in_u, b3d_fixed in_v, b3d_fixed *out_u, b3d_fixed *out_v);
int b3d_compute_bounds(const b3d_model *m, b3d_bounds *out_bounds);
int b3d_compute_mesh_bounds(const b3d_model *m, b3d_i32 mesh_id, b3d_bounds *out_bounds);

int b3d_node_child_count(const b3d_model *m, b3d_i32 node_id);
int b3d_node_depth(const b3d_model *m, b3d_i32 node_id);
void b3d_node_base_pose(const b3d_model *m, b3d_i32 node_id, b3d_pose_fixed *out_pose);
int b3d_find_key_at_frame(const b3d_model *m, b3d_i32 node_id, b3d_i32 frame, b3d_i32 channel_mask);
int b3d_sample_node_pose_floor(const b3d_model *m, b3d_i32 node_id, b3d_i32 frame, b3d_pose_fixed *out_pose);
int b3d_collect_vertex_influences(const b3d_model *m, b3d_i32 vertex_id, b3d_i32 max_out, b3d_i32 *out_node_ids, b3d_fixed *out_weights, b3d_i32 *out_count);
int b3d_collect_vertex_influences_blitz(const b3d_model *m, b3d_i32 vertex_id, b3d_i32 max_out, b3d_i32 *out_node_ids, b3d_i32 *out_blitz_bone_ids, b3d_fixed *out_weights, b3d_i32 *out_count);
int b3d_sum_vertex_weights(const b3d_model *m, b3d_i32 vertex_id, b3d_fixed *out_sum);

#ifdef __cplusplus
}
#endif

#endif
