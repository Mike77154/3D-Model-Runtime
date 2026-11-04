/*
 * md5fx.h
 *
 * Fixed-point, allocation-free encoder/decoder for Doom 3 MD5Mesh/MD5Anim.
 *
 * Design goals:
 *   - ANSI C89 compatible syntax
 *   - no malloc/free
 *   - fixed-size pools configured at compile time
 *   - fixed-point numeric core (Q16.16 by default)
 *
 * This library reads and writes:
 *   - .md5mesh
 *   - .md5anim
 *
 * It also provides helpers to:
 *   - build an object-space skeleton pose from a .md5anim frame
 *   - skin mesh vertices against a pose
 */

#ifndef MD5FX_H
#define MD5FX_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef signed long md5fx_fixed;

#ifndef MD5FX_FIXED_SHIFT
#define MD5FX_FIXED_SHIFT 16
#endif

#define MD5FX_FIXED_ONE ((md5fx_fixed)1L << MD5FX_FIXED_SHIFT)
#define MD5FX_VERSION_DEFAULT 10
#define MD5FX_INVALID_PARENT (-1)

#ifndef MD5FX_MAX_NAME
#define MD5FX_MAX_NAME 64
#endif

#ifndef MD5FX_MAX_SHADER
#define MD5FX_MAX_SHADER 128
#endif

#ifndef MD5FX_MAX_COMMANDLINE
#define MD5FX_MAX_COMMANDLINE 128
#endif

#ifndef MD5FX_MAX_TOKEN
#define MD5FX_MAX_TOKEN 256
#endif

#ifndef MD5FX_MAX_MESHES
#define MD5FX_MAX_MESHES 8
#endif

#ifndef MD5FX_MAX_JOINTS
#define MD5FX_MAX_JOINTS 128
#endif

#ifndef MD5FX_MAX_VERTS
#define MD5FX_MAX_VERTS 16384
#endif

#ifndef MD5FX_MAX_TRIS
#define MD5FX_MAX_TRIS 16384
#endif

#ifndef MD5FX_MAX_WEIGHTS
#define MD5FX_MAX_WEIGHTS 32768
#endif

#ifndef MD5FX_MAX_FRAMES
#define MD5FX_MAX_FRAMES 256
#endif

#ifndef MD5FX_MAX_COMPONENTS
#define MD5FX_MAX_COMPONENTS (MD5FX_MAX_JOINTS * 6)
#endif

#ifndef MD5FX_DEFAULT_DECIMALS
#define MD5FX_DEFAULT_DECIMALS 6
#endif


#ifndef MD5FX_SURFACE_HASH_SIZE
#define MD5FX_SURFACE_HASH_SIZE 4096
#endif

#if MD5FX_SURFACE_HASH_SIZE < 1
#error MD5FX_SURFACE_HASH_SIZE must be at least 1
#endif

#ifndef MD5FX_SURFACE_UV_EPSILON
#define MD5FX_SURFACE_UV_EPSILON ((md5fx_fixed)1L)
#endif

#define MD5FX_SURFACE_LEGACY 0UL
#define MD5FX_SURFACE_NORMALIZE_FACE_SUMS 1UL
#define MD5FX_SURFACE_SIGNED_UV_TANGENTS 2UL
#define MD5FX_SURFACE_SMOOTH_DUPLICATE_NORMALS 4UL
#define MD5FX_SURFACE_UNSMOOTHED_DOMINANT 8UL
#define MD5FX_SURFACE_ORTHO_BITANGENT 16UL
#define MD5FX_SURFACE_FLIP_TANGENT_BY_BITANGENT 32UL
#define MD5FX_SURFACE_MIRRORED_UV_DOMINANT 64UL
#define MD5FX_SURFACE_SMOOTH_DUPLICATE_TANGENTS 128UL

#define MD5FX_SURFACE_DOOM3_SMOOTH \
    (MD5FX_SURFACE_NORMALIZE_FACE_SUMS | \
     MD5FX_SURFACE_SIGNED_UV_TANGENTS | \
     MD5FX_SURFACE_SMOOTH_DUPLICATE_NORMALS | \
     MD5FX_SURFACE_MIRRORED_UV_DOMINANT)

#define MD5FX_SURFACE_DOOM3_UNSMOOTHED \
    (MD5FX_SURFACE_SIGNED_UV_TANGENTS | \
     MD5FX_SURFACE_UNSMOOTHED_DOMINANT | \
     MD5FX_SURFACE_ORTHO_BITANGENT | \
     MD5FX_SURFACE_MIRRORED_UV_DOMINANT)

#define MD5FX_SURFACE_DOOM3_ROBUST \
    (MD5FX_SURFACE_DOOM3_SMOOTH | \
     MD5FX_SURFACE_SMOOTH_DUPLICATE_TANGENTS)

enum
{
    MD5FX_OK = 0,
    MD5FX_E_IO = -1,
    MD5FX_E_TOKEN = -2,
    MD5FX_E_FORMAT = -3,
    MD5FX_E_LIMIT = -4,
    MD5FX_E_RANGE = -5,
    MD5FX_E_MISMATCH = -6
};

typedef struct md5fx_vec2_s
{
    md5fx_fixed x;
    md5fx_fixed y;
} md5fx_vec2;

typedef struct md5fx_vec3_s
{
    md5fx_fixed x;
    md5fx_fixed y;
    md5fx_fixed z;
} md5fx_vec3;

typedef struct md5fx_quat_s
{
    md5fx_fixed x;
    md5fx_fixed y;
    md5fx_fixed z;
    md5fx_fixed w;
} md5fx_quat;

typedef struct md5fx_status_s
{
    int code;
    int line;
    char message[96];
} md5fx_status;

typedef struct md5fx_joint_s
{
    char name[MD5FX_MAX_NAME];
    int parent;
    md5fx_vec3 pos;
    md5fx_quat orient;
} md5fx_joint;

typedef struct md5fx_vert_s
{
    int index;
    md5fx_vec2 uv;
    int start_weight;  /* local to the owning mesh */
    int weight_count;
} md5fx_vert;

typedef struct md5fx_tri_s
{
    int index;
    int v0;            /* local to the owning mesh */
    int v1;
    int v2;
} md5fx_tri;

typedef struct md5fx_weight_s
{
    int index;
    int joint;
    md5fx_fixed bias;
    md5fx_vec3 pos;
} md5fx_weight;

typedef struct md5fx_mesh_s
{
    char shader[MD5FX_MAX_SHADER];
    int first_vert;
    int num_verts;
    int first_tri;
    int num_tris;
    int first_weight;
    int num_weights;
} md5fx_mesh;

typedef struct md5fx_mesh_file_s
{
    int version;
    char commandline[MD5FX_MAX_COMMANDLINE];

    int num_joints;
    int num_meshes;

    md5fx_joint joints[MD5FX_MAX_JOINTS];
    md5fx_mesh meshes[MD5FX_MAX_MESHES];

    md5fx_vert verts[MD5FX_MAX_VERTS];
    md5fx_tri tris[MD5FX_MAX_TRIS];
    md5fx_weight weights[MD5FX_MAX_WEIGHTS];
} md5fx_mesh_file;

typedef struct md5fx_hierarchy_s
{
    char name[MD5FX_MAX_NAME];
    int parent;
    int flags;
    int start_index;
} md5fx_hierarchy;

typedef struct md5fx_bound_s
{
    md5fx_vec3 min;
    md5fx_vec3 max;
} md5fx_bound;

typedef struct md5fx_baseframe_joint_s
{
    md5fx_vec3 pos;
    md5fx_quat orient;
} md5fx_baseframe_joint;

typedef struct md5fx_frame_s
{
    int frame_index;
    int component_count;
    int first_component;
} md5fx_frame;

typedef struct md5fx_anim_file_s
{
    int version;
    char commandline[MD5FX_MAX_COMMANDLINE];

    int num_frames;
    int num_joints;
    int frame_rate;
    int num_animated_components;

    md5fx_hierarchy hierarchy[MD5FX_MAX_JOINTS];
    md5fx_bound bounds[MD5FX_MAX_FRAMES];
    md5fx_baseframe_joint baseframe[MD5FX_MAX_JOINTS];
    md5fx_frame frames[MD5FX_MAX_FRAMES];
    md5fx_fixed components[MD5FX_MAX_FRAMES * MD5FX_MAX_COMPONENTS];
} md5fx_anim_file;

typedef struct md5fx_pose_joint_s
{
    int parent;
    md5fx_vec3 pos;
    md5fx_quat orient;
} md5fx_pose_joint;

typedef struct md5fx_pose_s
{
    int num_joints;
    md5fx_pose_joint joints[MD5FX_MAX_JOINTS];
} md5fx_pose;

/* ------------------------------------------------------------------------- */
/* Status and initialization                                                 */
/* ------------------------------------------------------------------------- */

void md5fx_status_init(md5fx_status *status);
const char *md5fx_result_string(int code);

void md5fx_mesh_init(md5fx_mesh_file *mesh_file);
void md5fx_anim_init(md5fx_anim_file *anim_file);
void md5fx_pose_init(md5fx_pose *pose);

/* ------------------------------------------------------------------------- */
/* Fixed-point helpers                                                       */
/* ------------------------------------------------------------------------- */

md5fx_fixed md5fx_fixed_from_int(long value);
long md5fx_fixed_to_int(md5fx_fixed value);
int md5fx_fixed_from_str(const char *text, md5fx_fixed *out_value);
void md5fx_fixed_to_str(md5fx_fixed value, char *buffer, int buffer_size, int decimals);

md5fx_fixed md5fx_fixed_abs(md5fx_fixed value);
md5fx_fixed md5fx_fixed_mul(md5fx_fixed a, md5fx_fixed b);
md5fx_fixed md5fx_fixed_div(md5fx_fixed a, md5fx_fixed b);
md5fx_fixed md5fx_fixed_sqrt(md5fx_fixed value);
md5fx_fixed md5fx_fixed_lerp(md5fx_fixed a, md5fx_fixed b, md5fx_fixed t);

/* ------------------------------------------------------------------------- */
/* Quaternion helpers                                                        */
/* ------------------------------------------------------------------------- */

void md5fx_quat_compute_w(md5fx_quat *quat);
void md5fx_quat_normalize(md5fx_quat *quat);
md5fx_quat md5fx_quat_mul(md5fx_quat a, md5fx_quat b);
md5fx_quat md5fx_quat_nlerp(md5fx_quat a, md5fx_quat b, md5fx_fixed t);
md5fx_vec3 md5fx_quat_rotate_vec3(md5fx_quat quat, md5fx_vec3 v);

/* ------------------------------------------------------------------------- */
/* Reader / writer                                                           */
/* ------------------------------------------------------------------------- */

int md5fx_mesh_load_file(md5fx_mesh_file *mesh_file, const char *path, md5fx_status *status);
int md5fx_mesh_save_file(const md5fx_mesh_file *mesh_file, const char *path, int decimals, md5fx_status *status);

int md5fx_anim_load_file(md5fx_anim_file *anim_file, const char *path, md5fx_status *status);
int md5fx_anim_save_file(const md5fx_anim_file *anim_file, const char *path, int decimals, md5fx_status *status);

/* ------------------------------------------------------------------------- */
/* Validation and pose / skinning helpers                                    */
/* ------------------------------------------------------------------------- */

int md5fx_anim_matches_mesh(const md5fx_mesh_file *mesh_file, const md5fx_anim_file *anim_file, md5fx_status *status);
int md5fx_mesh_bind_pose(const md5fx_mesh_file *mesh_file, md5fx_pose *pose, md5fx_status *status);
int md5fx_anim_build_pose(const md5fx_anim_file *anim_file, int frame_index, md5fx_pose *pose, md5fx_status *status);
int md5fx_anim_build_pose_interp(const md5fx_anim_file *anim_file, md5fx_fixed frame_cursor, md5fx_pose *pose, md5fx_status *status);
int md5fx_anim_build_pose_time(const md5fx_anim_file *anim_file, md5fx_fixed time_seconds, md5fx_pose *pose, md5fx_status *status);
int md5fx_mesh_build_vertices(const md5fx_mesh_file *mesh_file, const md5fx_pose *pose, md5fx_vec3 *out_positions, int max_positions, md5fx_status *status);
int md5fx_mesh_build_surface_ex(const md5fx_mesh_file *mesh_file, const md5fx_pose *pose, md5fx_vec3 *out_positions, md5fx_vec3 *out_normals, md5fx_vec3 *out_tangents, md5fx_vec3 *out_bitangents, int max_vertices, unsigned long flags, md5fx_status *status);
int md5fx_mesh_build_surface(const md5fx_mesh_file *mesh_file, const md5fx_pose *pose, md5fx_vec3 *out_positions, md5fx_vec3 *out_normals, md5fx_vec3 *out_tangents, int max_vertices, md5fx_status *status);

#ifdef __cplusplus
}
#endif

#endif
