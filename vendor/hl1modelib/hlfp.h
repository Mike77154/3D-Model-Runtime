#ifndef HLFP_H
#define HLFP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Half-Life / GoldSrc parser + inverse compiler helpers.
 *
 * C89, no malloc, fixed-point 16.16 math for runtime data.
 * Serializer boundaries still emit GoldSrc's float-on-disk fields.
 */

typedef unsigned char hlfp_u8;
typedef signed char hlfp_i8;
typedef unsigned short hlfp_u16;
typedef signed short hlfp_i16;
typedef unsigned int hlfp_u32;
typedef signed int hlfp_i32;
typedef hlfp_i32 hlfp_fixed;
typedef int hlfp_bool;

#define HLFP_TRUE 1
#define HLFP_FALSE 0

#define HLFP_FIXED_FRAC_BITS 16
#define HLFP_FIXED_ONE ((hlfp_fixed)65536)
#define HLFP_FIXED_HALF ((hlfp_fixed)32768)

#define HLFP_COUNTOF(a) ((int)(sizeof(a) / sizeof((a)[0])))

#define HLFP_SMD_MAX_BONES 128
#define HLFP_SMD_MAX_FRAMES 256
#define HLFP_SMD_MAX_POSES (HLFP_SMD_MAX_BONES * HLFP_SMD_MAX_FRAMES)
#define HLFP_SMD_MAX_TRIANGLES 4096
#define HLFP_SMD_MAX_LINKS 6
#define HLFP_SMD_MAX_NAME 64

#define HLFP_MDL_MAX_BONES 128
#define HLFP_MDL_MAX_BONE_CONTROLLERS 8
#define HLFP_MDL_MAX_HITBOXES 256
#define HLFP_MDL_MAX_SEQUENCES 2048
#define HLFP_MDL_MAX_SEQGROUPS 32
#define HLFP_MDL_MAX_EVENTS 1024
#define HLFP_MDL_MAX_PIVOTS 256
#define HLFP_MDL_MAX_ATTACHMENTS 512
#define HLFP_MDL_MAX_TEXTURES 100
#define HLFP_MDL_MAX_SKINREF 100
#define HLFP_MDL_MAX_SKINFAMILIES 100
#define HLFP_MDL_MAX_BODYPARTS 32
#define HLFP_MDL_MAX_MODELS 32
#define HLFP_MDL_MAX_MESHES 256
#define HLFP_MDL_MAX_TRANSITIONS 256
#define HLFP_MDL_MAX_VERTICES 2048
#define HLFP_MDL_MAX_NORMALS 2048
#define HLFP_MDL_MAX_TRIANGLES 20000
#define HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS 16
#define HLFP_MDL_MAX_BLEND_AXES 2
#define HLFP_MDL_MAX_USER_CONTROLLERS 4

#define HLFP_MDL_CACHE_MAIN_MAX 1048576U
#define HLFP_MDL_CACHE_TEXTURE_MAX 1048576U
#define HLFP_MDL_CACHE_SEQGROUP_TOTAL_MAX 2097152U

#define HLFP_MDL_MAX_EXPANDED_VERTICES 16384
#define HLFP_MDL_MAX_EXPANDED_TRIANGLES 32768
#define HLFP_MDL_MAX_TRICMD_VERTICES 4096

#define HLFP_MDL_IDST ((hlfp_u32)0x54534449U)
#define HLFP_MDL_IDSQ ((hlfp_u32)0x51534449U)
#define HLFP_MDL_VERSION 10

#define HLFP_STUDIO_X   0x0001
#define HLFP_STUDIO_Y   0x0002
#define HLFP_STUDIO_Z   0x0004
#define HLFP_STUDIO_XR  0x0008
#define HLFP_STUDIO_YR  0x0010
#define HLFP_STUDIO_ZR  0x0020
#define HLFP_STUDIO_LX  0x0040
#define HLFP_STUDIO_LY  0x0080
#define HLFP_STUDIO_LZ  0x0100
#define HLFP_STUDIO_AX  0x0200
#define HLFP_STUDIO_AY  0x0400
#define HLFP_STUDIO_AZ  0x0800
#define HLFP_STUDIO_AXR 0x1000
#define HLFP_STUDIO_AYR 0x2000
#define HLFP_STUDIO_AZR 0x4000
#define HLFP_STUDIO_TYPES 0x7FFF
#define HLFP_STUDIO_RLOOP 0x8000

#define HLFP_STUDIO_LOOPING 0x0001

#define HLFP_STUDIO_NF_FLATSHADE 0x0001
#define HLFP_STUDIO_NF_CHROME 0x0002
#define HLFP_STUDIO_NF_FULLBRIGHT 0x0004
#define HLFP_STUDIO_NF_NOMIPS 0x0008
#define HLFP_STUDIO_NF_ALPHA 0x0010
#define HLFP_STUDIO_NF_ADDITIVE 0x0020
#define HLFP_STUDIO_NF_MASKED 0x0040

enum {
    HLFP_OK = 0,
    HLFP_ERR_BAD_ARGUMENT = -1,
    HLFP_ERR_BAD_FORMAT = -2,
    HLFP_ERR_BAD_MAGIC = -3,
    HLFP_ERR_BAD_VERSION = -4,
    HLFP_ERR_OUT_OF_RANGE = -5,
    HLFP_ERR_CAPACITY = -6,
    HLFP_ERR_MISSING_DATA = -7,
    HLFP_ERR_UNSUPPORTED = -8,
    HLFP_ERR_DIV_BY_ZERO = -9,
    HLFP_ERR_OVERFLOW = -10,
    HLFP_ERR_IO = -11
};

typedef struct hlfp_vec2_s {
    hlfp_fixed x;
    hlfp_fixed y;
} hlfp_vec2;

typedef struct hlfp_vec3_s {
    hlfp_fixed x;
    hlfp_fixed y;
    hlfp_fixed z;
} hlfp_vec3;

typedef struct hlfp_mat3x4_s {
    hlfp_fixed m[3][4];
} hlfp_mat3x4;

typedef struct hlfp_buffer_s {
    const hlfp_u8 *data;
    size_t size;
} hlfp_buffer;

typedef struct hlfp_error_s {
    int code;
    int line;
    int column;
    char message[160];
} hlfp_error;

typedef struct hlfp_pose_s {
    int bone;
    hlfp_vec3 pos;
    hlfp_vec3 rot;
} hlfp_pose;

typedef struct hlfp_smd_node_s {
    int id;
    int parent;
    char name[HLFP_SMD_MAX_NAME];
} hlfp_smd_node;

typedef struct hlfp_smd_weight_s {
    int bone;
    hlfp_fixed weight;
} hlfp_smd_weight;

typedef struct hlfp_smd_vertex_s {
    int bone;
    hlfp_vec3 pos;
    hlfp_vec3 normal;
    hlfp_fixed u;
    hlfp_fixed v;
    int num_links;
    hlfp_smd_weight links[HLFP_SMD_MAX_LINKS];
} hlfp_smd_vertex;

typedef struct hlfp_smd_triangle_s {
    char material[HLFP_SMD_MAX_NAME];
    hlfp_smd_vertex v[3];
} hlfp_smd_triangle;

typedef struct hlfp_smd_frame_s {
    int time;
    int pose_start;
    int pose_count;
} hlfp_smd_frame;

typedef struct hlfp_smd_model_s {
    int version;
    int num_nodes;
    hlfp_smd_node nodes[HLFP_SMD_MAX_BONES];
    int num_frames;
    hlfp_smd_frame frames[HLFP_SMD_MAX_FRAMES];
    int num_poses;
    hlfp_pose poses[HLFP_SMD_MAX_POSES];
    int num_triangles;
    hlfp_smd_triangle triangles[HLFP_SMD_MAX_TRIANGLES];
    int discarded_weight_links; /* excess SMD links beyond fixed per-vertex capacity */
} hlfp_smd_model;

typedef struct hlfp_mdl_header_s {
    hlfp_u32 id;
    int version;
    char name[64];
    int length;
    hlfp_vec3 eyeposition;
    hlfp_vec3 min;
    hlfp_vec3 max;
    hlfp_vec3 bbmin;
    hlfp_vec3 bbmax;
    int flags;
    int numbones;
    int boneindex;
    int numbonecontrollers;
    int bonecontrollerindex;
    int numhitboxes;
    int hitboxindex;
    int numseq;
    int seqindex;
    int numseqgroups;
    int seqgroupindex;
    int numtextures;
    int textureindex;
    int texturedataindex;
    int numskinref;
    int numskinfamilies;
    int skinindex;
    int numbodyparts;
    int bodypartindex;
    int numattachments;
    int attachmentindex;
    int soundtable;
    int soundindex;
    int soundgroups;
    int soundgroupindex;
    int numtransitions;
    int transitionindex;
} hlfp_mdl_header;

typedef struct hlfp_mdl_bone_s {
    char name[32];
    int parent;
    int flags;
    int bonecontroller[6];
    hlfp_fixed value[6];
    hlfp_fixed scale[6];
} hlfp_mdl_bone;

typedef struct hlfp_mdl_bonecontroller_s {
    int bone;
    int type;
    hlfp_fixed start;
    hlfp_fixed end;
    int rest;
    int index;
} hlfp_mdl_bonecontroller;

typedef struct hlfp_mdl_hitbox_s {
    int bone;
    int group;
    hlfp_vec3 bbmin;
    hlfp_vec3 bbmax;
} hlfp_mdl_hitbox;

typedef struct hlfp_mdl_seqgroup_s {
    char label[32];
    char name[64];
    int cache;
    int data;
} hlfp_mdl_seqgroup;

typedef struct hlfp_mdl_seqdesc_s {
    char label[32];
    hlfp_fixed fps;
    int flags;
    int activity;
    int actweight;
    int numevents;
    int eventindex;
    int numframes;
    int numpivots;
    int pivotindex;
    int motiontype;
    int motionbone;
    hlfp_vec3 linearmovement;
    int automoveposindex;
    int automoveangleindex;
    hlfp_vec3 bbmin;
    hlfp_vec3 bbmax;
    int numblends;
    int animindex;
    int blendtype[2];
    hlfp_fixed blendstart[2];
    hlfp_fixed blendend[2];
    int blendparent;
    int seqgroup;
    int entrynode;
    int exitnode;
    int nodeflags;
    int nextseq;
} hlfp_mdl_seqdesc;

typedef struct hlfp_mdl_event_s {
    int frame;
    int event;
    int type;
    char options[64];
} hlfp_mdl_event;

typedef struct hlfp_mdl_attachment_s {
    char name[32];
    int type;
    int bone;
    hlfp_vec3 org;
    hlfp_vec3 vectors[3];
} hlfp_mdl_attachment;

typedef struct hlfp_mdl_texture_s {
    char name[64];
    int flags;
    int width;
    int height;
    int index;
} hlfp_mdl_texture;

typedef struct hlfp_mdl_bodypart_s {
    char name[64];
    int nummodels;
    int base;
    int modelindex;
    int first_model;
} hlfp_mdl_bodypart;

typedef struct hlfp_mdl_model_s2 {
    char name[64];
    int type;
    hlfp_fixed boundingradius;
    int nummesh;
    int meshindex;
    int numverts;
    int vertinfoindex;
    int vertindex;
    int numnorms;
    int norminfoindex;
    int normindex;
    int numgroups;
    int groupindex;
    int first_mesh;
    int bodypart_index;
} hlfp_mdl_model;

typedef struct hlfp_mdl_mesh_s {
    int numtris;
    int triindex;
    int skinref;
    int numnorms;
    int normindex;
    int parent_model;
} hlfp_mdl_mesh;

typedef struct hlfp_mdl_trivert_s {
    hlfp_i16 vertindex;
    hlfp_i16 normindex;
    hlfp_i16 s;
    hlfp_i16 t;
} hlfp_mdl_trivert;

typedef struct hlfp_mdl_expanded_vertex_s {
    int vertindex;
    int normindex;
    int bone;
    int normal_bone;
    hlfp_i16 s;
    hlfp_i16 t;
    hlfp_fixed u;
    hlfp_fixed v;
    hlfp_vec3 pos;
    hlfp_vec3 normal;
} hlfp_mdl_expanded_vertex;

typedef struct hlfp_mdl_expanded_triangle_s {
    int v0;
    int v1;
    int v2;
} hlfp_mdl_expanded_triangle;

typedef struct hlfp_mdl_mesh_expanded_s {
    int model_index;
    int mesh_index;
    int texture_index;
    int num_vertices;
    hlfp_mdl_expanded_vertex vertices[HLFP_MDL_MAX_EXPANDED_VERTICES];
    int num_triangles;
    hlfp_mdl_expanded_triangle triangles[HLFP_MDL_MAX_EXPANDED_TRIANGLES];
} hlfp_mdl_mesh_expanded;

typedef struct hlfp_mdl_pose_set_s {
    int num_bones;
    hlfp_pose poses[HLFP_MDL_MAX_BONES];
} hlfp_mdl_pose_set;

typedef struct hlfp_mdl_s {
    hlfp_mdl_header header;
    hlfp_buffer main_buffer;
    hlfp_buffer texture_buffer;
    hlfp_buffer seqgroup_buffers[HLFP_MDL_MAX_SEQGROUPS];
    int num_bones;
    hlfp_mdl_bone bones[HLFP_MDL_MAX_BONES];
    int num_bonecontrollers;
    hlfp_mdl_bonecontroller bonecontrollers[HLFP_MDL_MAX_BONE_CONTROLLERS];
    int num_hitboxes;
    hlfp_mdl_hitbox hitboxes[HLFP_MDL_MAX_HITBOXES];
    int num_seqgroups;
    hlfp_mdl_seqgroup seqgroups[HLFP_MDL_MAX_SEQGROUPS];
    int num_seqdesc;
    hlfp_mdl_seqdesc seqdesc[HLFP_MDL_MAX_SEQUENCES];
    int num_events;
    hlfp_mdl_event events[HLFP_MDL_MAX_EVENTS];
    int num_attachments;
    hlfp_mdl_attachment attachments[HLFP_MDL_MAX_ATTACHMENTS];
    int num_textures;
    hlfp_mdl_texture textures[HLFP_MDL_MAX_TEXTURES];
    int num_skinref;
    int num_skinfamilies;
    hlfp_i16 skinrefs[HLFP_MDL_MAX_SKINFAMILIES][HLFP_MDL_MAX_SKINREF];
    int num_bodyparts;
    hlfp_mdl_bodypart bodyparts[HLFP_MDL_MAX_BODYPARTS];
    int num_models;
    hlfp_mdl_model models[HLFP_MDL_MAX_MODELS];
    int num_meshes;
    hlfp_mdl_mesh meshes[HLFP_MDL_MAX_MESHES];
    int num_transitions;
    hlfp_u8 transitions[HLFP_MDL_MAX_TRANSITIONS * HLFP_MDL_MAX_TRANSITIONS];
} hlfp_mdl;

typedef int (*hlfp_write_fn)(void *user, const void *data, size_t size);

typedef struct hlfp_mem_writer_s {
    hlfp_u8 *data;
    size_t capacity;
    size_t size;
} hlfp_mem_writer;

typedef struct hlfp_mdl_compile_texture_s {
    char name[64];
    int flags;
    int width;
    int height;
    const hlfp_u8 *pixels;
    const hlfp_u8 *palette;
} hlfp_mdl_compile_texture;

typedef struct hlfp_mdl_compile_sequence_s {
    char label[32];
    const hlfp_smd_model *animation;
    int fps;
    int flags;
    int activity;
    int actweight;
    int motiontype;
    int motionbone;
} hlfp_mdl_compile_sequence;

typedef struct hlfp_mdl_compile_options_s {
    char name[64];
    hlfp_vec3 eyeposition;
    hlfp_vec3 min;
    hlfp_vec3 max;
    hlfp_vec3 bbmin;
    hlfp_vec3 bbmax;
    int flags;
    int texture_width;
    int texture_height;
    int texture_flags;
    hlfp_bool create_dummy_textures;
} hlfp_mdl_compile_options;

typedef struct hlfp_mdl_roundtrip_info_s {
    size_t main_size;
    hlfp_bool has_texture_file;
    size_t texture_size;
    int num_seqgroups;
    hlfp_bool has_seqgroup_file[HLFP_MDL_MAX_SEQGROUPS];
    size_t seqgroup_size[HLFP_MDL_MAX_SEQGROUPS];
} hlfp_mdl_roundtrip_info;

typedef struct hlfp_quat_s {
    hlfp_fixed x;
    hlfp_fixed y;
    hlfp_fixed z;
    hlfp_fixed w;
} hlfp_quat;

typedef struct hlfp_mdl_quat_pose_s {
    int bone;
    hlfp_vec3 pos;
    hlfp_quat rot;
} hlfp_mdl_quat_pose;

typedef struct hlfp_mdl_quat_pose_set_s {
    int num_bones;
    hlfp_mdl_quat_pose poses[HLFP_MDL_MAX_BONES];
} hlfp_mdl_quat_pose_set;

typedef struct hlfp_mdl_controller_state_s {
    hlfp_fixed blend[HLFP_MDL_MAX_BLEND_AXES];
    hlfp_fixed bone[HLFP_MDL_MAX_USER_CONTROLLERS];
} hlfp_mdl_controller_state;

typedef struct hlfp_mdl_binary_cache_s {
    hlfp_u8 main[HLFP_MDL_CACHE_MAIN_MAX];
    size_t main_size;
    hlfp_u8 texture[HLFP_MDL_CACHE_TEXTURE_MAX];
    size_t texture_size;
    hlfp_u8 seqgroup_pool[HLFP_MDL_CACHE_SEQGROUP_TOTAL_MAX];
    size_t seqgroup_pool_used;
    size_t seqgroup_offset[HLFP_MDL_MAX_SEQGROUPS];
    size_t seqgroup_size[HLFP_MDL_MAX_SEQGROUPS];
} hlfp_mdl_binary_cache;

void hlfp_error_clear(hlfp_error *err);
const char *hlfp_strerror(int code);

hlfp_fixed hlfp_fixed_from_int(int value);
int hlfp_fixed_to_int_trunc(hlfp_fixed value);
int hlfp_fixed_to_int_round(hlfp_fixed value);
int hlfp_fixed_mul(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out);
int hlfp_fixed_div(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out);
int hlfp_fixed_sin_cos(hlfp_fixed radians, hlfp_fixed *out_sin, hlfp_fixed *out_cos);

void hlfp_mat3x4_identity(hlfp_mat3x4 *out);
int hlfp_angles_to_mat3x4(const hlfp_vec3 *rot, const hlfp_vec3 *pos, hlfp_mat3x4 *out, hlfp_error *err);

/* GoldSrc studio bone path. These match Valve's AngleQuaternion,
 * QuaternionSlerp and QuaternionMatrix conventions for MDL bones. */
int hlfp_goldsrc_angles_to_quat(const hlfp_vec3 *rot, hlfp_quat *out, hlfp_error *err);
int hlfp_quat_slerp(const hlfp_quat *a, const hlfp_quat *b, hlfp_fixed t, hlfp_quat *out, hlfp_error *err);
int hlfp_quat_to_mat3x4(const hlfp_quat *q, const hlfp_vec3 *pos, hlfp_mat3x4 *out, hlfp_error *err);
int hlfp_goldsrc_angles_to_bone_mat3x4(const hlfp_vec3 *rot, const hlfp_vec3 *pos, hlfp_mat3x4 *out, hlfp_error *err);

int hlfp_mat3x4_concat(const hlfp_mat3x4 *parent, const hlfp_mat3x4 *child, hlfp_mat3x4 *out, hlfp_error *err);
int hlfp_mdl_pose_to_local_matrices(const hlfp_mdl_pose_set *poses, hlfp_mat3x4 *out, int out_cap, hlfp_error *err);
int hlfp_mdl_pose_to_world_matrices(const hlfp_mdl *mdl, const hlfp_mdl_pose_set *poses, hlfp_mat3x4 *out, int out_cap, hlfp_error *err);
int hlfp_mdl_decode_sequence_world_matrices(const hlfp_mdl *mdl, int sequence_index, int blend_index, int frame,
                                            hlfp_mat3x4 *out, int out_cap, hlfp_error *err);
int hlfp_mdl_decode_sequence_world_matrices_fixed(const hlfp_mdl *mdl, int sequence_index, int blend_index,
                                                  hlfp_fixed frame_position,
                                                  hlfp_mat3x4 *out, int out_cap, hlfp_error *err);
int hlfp_mdl_decode_sequence_world_matrices_time(const hlfp_mdl *mdl, int sequence_index, int blend_index,
                                                 hlfp_fixed seconds,
                                                 hlfp_mat3x4 *out, int out_cap, hlfp_error *err);

void hlfp_mem_writer_init(hlfp_mem_writer *w, void *data, size_t capacity);
int hlfp_mem_writer_write(void *user, const void *data, size_t size);

void hlfp_smd_init(hlfp_smd_model *out);
int hlfp_smd_parse_buffer(const char *text, size_t size, hlfp_smd_model *out, hlfp_error *err);

void hlfp_mdl_init(hlfp_mdl *out);
int hlfp_mdl_parse_main(const void *data, size_t size, hlfp_mdl *out, hlfp_error *err);
int hlfp_mdl_attach_texture_file(hlfp_mdl *out, const void *data, size_t size, hlfp_error *err);
int hlfp_mdl_attach_seqgroup_file(hlfp_mdl *out, int seqgroup_index, const void *data, size_t size, hlfp_error *err);
int hlfp_mdl_get_texture_pixels(const hlfp_mdl *mdl, int texture_index,
                                const hlfp_u8 **pixels, const hlfp_u8 **palette,
                                int *width, int *height, hlfp_error *err);
int hlfp_mdl_read_model_vertex(const hlfp_mdl *mdl, int model_index, int vertex_index,
                               hlfp_vec3 *pos, int *bone, hlfp_error *err);
int hlfp_mdl_read_model_normal(const hlfp_mdl *mdl, int model_index, int normal_index,
                               hlfp_vec3 *normal, int *bone, hlfp_error *err);
int hlfp_mdl_expand_mesh(const hlfp_mdl *mdl, int model_index, int mesh_index,
                         hlfp_mdl_mesh_expanded *out, hlfp_error *err);
int hlfp_mdl_decode_sequence_frame(const hlfp_mdl *mdl, int sequence_index,
                                   int blend_index, int frame,
                                   hlfp_mdl_pose_set *out, hlfp_error *err);
int hlfp_mdl_decode_sequence_frame_fixed(const hlfp_mdl *mdl, int sequence_index,
                                         int blend_index, hlfp_fixed frame_position,
                                         hlfp_mdl_pose_set *out, hlfp_error *err);
int hlfp_mdl_decode_sequence_time(const hlfp_mdl *mdl, int sequence_index,
                                  int blend_index, hlfp_fixed seconds,
                                  hlfp_mdl_pose_set *out, hlfp_error *err);

int hlfp_mdl_interpolate_pose_sets(const hlfp_mdl_pose_set *a,
                                   const hlfp_mdl_pose_set *b,
                                   hlfp_fixed t,
                                   hlfp_mdl_pose_set *out,
                                   hlfp_error *err);
int hlfp_mdl_blend_pose_sets(const hlfp_mdl_pose_set *pose_sets, const hlfp_fixed *weights,
                             int count, hlfp_mdl_pose_set *out, hlfp_error *err);
int hlfp_mdl_blend_sequence_frames(const hlfp_mdl *mdl,
                                   const int *sequence_indices,
                                   const int *blend_indices,
                                   const int *frames,
                                   const hlfp_fixed *weights,
                                   int count,
                                   hlfp_mdl_pose_set *out,
                                   hlfp_error *err);
int hlfp_mdl_blend_sequence_frame_positions(const hlfp_mdl *mdl,
                                            const int *sequence_indices,
                                            const int *blend_indices,
                                            const hlfp_fixed *frame_positions,
                                            const hlfp_fixed *weights,
                                            int count,
                                            hlfp_mdl_pose_set *out,
                                            hlfp_error *err);
int hlfp_mdl_blend_sequence_times(const hlfp_mdl *mdl,
                                  const int *sequence_indices,
                                  const int *blend_indices,
                                  const hlfp_fixed *seconds,
                                  const hlfp_fixed *weights,
                                  int count,
                                  hlfp_mdl_pose_set *out,
                                  hlfp_error *err);
int hlfp_mdl_blend_sequence_world_matrices(const hlfp_mdl *mdl,
                                           const int *sequence_indices,
                                           const int *blend_indices,
                                           const int *frames,
                                           const hlfp_fixed *weights,
                                           int count,
                                           hlfp_mat3x4 *out,
                                           int out_cap,
                                           hlfp_error *err);
int hlfp_mdl_blend_sequence_world_matrices_frame_positions(const hlfp_mdl *mdl,
                                                           const int *sequence_indices,
                                                           const int *blend_indices,
                                                           const hlfp_fixed *frame_positions,
                                                           const hlfp_fixed *weights,
                                                           int count,
                                                           hlfp_mat3x4 *out,
                                                           int out_cap,
                                                           hlfp_error *err);
int hlfp_mdl_blend_sequence_world_matrices_times(const hlfp_mdl *mdl,
                                                 const int *sequence_indices,
                                                 const int *blend_indices,
                                                 const hlfp_fixed *seconds,
                                                 const hlfp_fixed *weights,
                                                 int count,
                                                 hlfp_mat3x4 *out,
                                                 int out_cap,
                                                 hlfp_error *err);

int hlfp_mdl_get_roundtrip_info(const hlfp_mdl *mdl,
                                hlfp_mdl_roundtrip_info *out,
                                hlfp_error *err);
int hlfp_mdl_roundtrip_main(const hlfp_mdl *mdl,
                            void *dst,
                            size_t dst_capacity,
                            size_t *out_size,
                            hlfp_error *err);
int hlfp_mdl_roundtrip_texture_file(const hlfp_mdl *mdl,
                                    void *dst,
                                    size_t dst_capacity,
                                    size_t *out_size,
                                    hlfp_error *err);
int hlfp_mdl_roundtrip_seqgroup_file(const hlfp_mdl *mdl,
                                     int seqgroup_index,
                                     void *dst,
                                     size_t dst_capacity,
                                     size_t *out_size,
                                     hlfp_error *err);

int hlfp_mdl_compile_from_smd(const hlfp_smd_model *reference,
                              const hlfp_mdl_compile_sequence *sequences,
                              int num_sequences,
                              const hlfp_mdl_compile_texture *textures,
                              int num_textures,
                              const hlfp_mdl_compile_options *options,
                              void *dst,
                              size_t dst_capacity,
                              size_t *out_size,
                              hlfp_error *err);

void hlfp_mdl_controller_state_init(hlfp_mdl_controller_state *out);
void hlfp_mdl_binary_cache_init(hlfp_mdl_binary_cache *cache);

int hlfp_mdl_parse_main_cached(const void *data,
                               size_t size,
                               hlfp_mdl *out,
                               hlfp_mdl_binary_cache *cache,
                               hlfp_error *err);
int hlfp_mdl_attach_texture_file_cached(hlfp_mdl *out,
                                        const void *data,
                                        size_t size,
                                        hlfp_mdl_binary_cache *cache,
                                        hlfp_error *err);
int hlfp_mdl_attach_seqgroup_file_cached(hlfp_mdl *out,
                                         int seqgroup_index,
                                         const void *data,
                                         size_t size,
                                         hlfp_mdl_binary_cache *cache,
                                         hlfp_error *err);
int hlfp_mdl_regenerate_main_binary(const hlfp_mdl *mdl,
                                    void *dst,
                                    size_t dst_capacity,
                                    size_t *out_size,
                                    hlfp_error *err);
int hlfp_mdl_regenerate_texture_binary(const hlfp_mdl *mdl,
                                       void *dst,
                                       size_t dst_capacity,
                                       size_t *out_size,
                                       hlfp_error *err);
int hlfp_mdl_regenerate_seqgroup_binary(const hlfp_mdl *mdl,
                                        int seqgroup_index,
                                        void *dst,
                                        size_t dst_capacity,
                                        size_t *out_size,
                                        hlfp_error *err);

int hlfp_mdl_regenerate_main_binary_semantic(const hlfp_mdl *mdl,
                                             void *dst,
                                             size_t dst_capacity,
                                             size_t *out_size,
                                             hlfp_error *err);
int hlfp_mdl_regenerate_texture_binary_semantic(const hlfp_mdl *mdl,
                                                void *dst,
                                                size_t dst_capacity,
                                                size_t *out_size,
                                                hlfp_error *err);
int hlfp_mdl_regenerate_seqgroup_binary_semantic(const hlfp_mdl *mdl,
                                                 int seqgroup_index,
                                                 void *dst,
                                                 size_t dst_capacity,
                                                 size_t *out_size,
                                                 hlfp_error *err);

int hlfp_mdl_pose_to_quat_pose_set(const hlfp_mdl_pose_set *src,
                                   hlfp_mdl_quat_pose_set *out,
                                   hlfp_error *err);
int hlfp_mdl_quat_pose_to_local_matrices(const hlfp_mdl_quat_pose_set *poses,
                                         hlfp_mat3x4 *out,
                                         int out_cap,
                                         hlfp_error *err);
int hlfp_mdl_quat_pose_to_world_matrices(const hlfp_mdl *mdl,
                                         const hlfp_mdl_quat_pose_set *poses,
                                         hlfp_mat3x4 *out,
                                         int out_cap,
                                         hlfp_error *err);
int hlfp_mdl_blend_quat_pose_sets(const hlfp_mdl_quat_pose_set *pose_sets,
                                  const hlfp_fixed *weights,
                                  int count,
                                  hlfp_mdl_quat_pose_set *out,
                                  hlfp_error *err);
int hlfp_mdl_decode_sequence_frame_fixed_quat(const hlfp_mdl *mdl,
                                                int sequence_index,
                                                int blend_index,
                                                hlfp_fixed frame_position,
                                                hlfp_mdl_quat_pose_set *out,
                                                hlfp_error *err);
int hlfp_mdl_decode_sequence_world_matrices_fixed_quat(const hlfp_mdl *mdl,
                                                       int sequence_index,
                                                       int blend_index,
                                                       hlfp_fixed frame_position,
                                                       hlfp_mat3x4 *out,
                                                       int out_cap,
                                                       hlfp_error *err);
int hlfp_mdl_decode_sequence_frame_fixed_quat_ctrl(const hlfp_mdl *mdl,
                                                   int sequence_index,
                                                   hlfp_fixed frame_position,
                                                   const hlfp_mdl_controller_state *controllers,
                                                   hlfp_mdl_quat_pose_set *out,
                                                   hlfp_error *err);
int hlfp_mdl_decode_sequence_time_quat_ctrl(const hlfp_mdl *mdl,
                                            int sequence_index,
                                            hlfp_fixed seconds,
                                            const hlfp_mdl_controller_state *controllers,
                                            hlfp_mdl_quat_pose_set *out,
                                            hlfp_error *err);
int hlfp_mdl_blend_sequence_times_quat_ctrl(const hlfp_mdl *mdl,
                                            const int *sequence_indices,
                                            const hlfp_fixed *seconds,
                                            const hlfp_mdl_controller_state *controllers,
                                            const hlfp_fixed *weights,
                                            int count,
                                            hlfp_mdl_quat_pose_set *out,
                                            hlfp_error *err);
int hlfp_mdl_blend_sequence_world_matrices_times_quat_ctrl(const hlfp_mdl *mdl,
                                                           const int *sequence_indices,
                                                           const hlfp_fixed *seconds,
                                                           const hlfp_mdl_controller_state *controllers,
                                                           const hlfp_fixed *weights,
                                                           int count,
                                                           hlfp_mat3x4 *out,
                                                           int out_cap,
                                                           hlfp_error *err);

#ifdef __cplusplus
}
#endif

#endif
