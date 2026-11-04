#ifndef CODEC3D_POSER_H
#define CODEC3D_POSER_H

/*
   codec3d_poser.h - static Poser-family decoder v0.3
   Targets: C89, fixed-point Q16.16, no malloc/realloc/free in the core.

   Formats targeted:
     .pz3     Poser scene text files
     .pp2     Poser prop text files
     .poser   generic Poser text-family alias/export label
     .pmd     external binary morph target sidecar (best-effort scanner)
     .ppz/.pzz compressed Poser text via optional zlib wrapper module

   Design:
     - All storage is in P3D_Context fixed-size arrays.
     - Streams are fed through byte callbacks.
     - Text parser extracts geometry refs, embedded OBJ, actor/channel data,
       morph deltas, dependency/value-operation records, shader nodes/links,
       joint/falloff/weight-zone records, and sparse skin weights.
*/

#ifdef __cplusplus
extern "C" {
#endif

#define P3D_VERSION_MAJOR 0
#define P3D_VERSION_MINOR 3
#define P3D_VERSION_PATCH 0

#ifndef P3D_MAX_VERTICES
#define P3D_MAX_VERTICES 8192u
#endif
#ifndef P3D_MAX_TEXCOORDS
#define P3D_MAX_TEXCOORDS 8192u
#endif
#ifndef P3D_MAX_NORMALS
#define P3D_MAX_NORMALS 8192u
#endif
#ifndef P3D_MAX_FACES
#define P3D_MAX_FACES 8192u
#endif
#ifndef P3D_MAX_MATERIALS
#define P3D_MAX_MATERIALS 256u
#endif
#ifndef P3D_MAX_OBJECTS
#define P3D_MAX_OBJECTS 256u
#endif
#ifndef P3D_MAX_REFS
#define P3D_MAX_REFS 512u
#endif
#ifndef P3D_MAX_CHANNELS
#define P3D_MAX_CHANNELS 1024u
#endif
#ifndef P3D_MAX_DEPENDENCIES
#define P3D_MAX_DEPENDENCIES 1024u
#endif
#ifndef P3D_MAX_MORPHS
#define P3D_MAX_MORPHS 512u
#endif
#ifndef P3D_MAX_MORPH_DELTAS
#define P3D_MAX_MORPH_DELTAS 8192u
#endif
#ifndef P3D_MAX_JOINTS
#define P3D_MAX_JOINTS 256u
#endif
#ifndef P3D_MAX_ZONES
#define P3D_MAX_ZONES 512u
#endif
#ifndef P3D_MAX_SHADER_NODES
#define P3D_MAX_SHADER_NODES 512u
#endif
#ifndef P3D_MAX_SHADER_LINKS
#define P3D_MAX_SHADER_LINKS 1024u
#endif
#ifndef P3D_MAX_SKIN_WEIGHTS
#define P3D_MAX_SKIN_WEIGHTS 8192u
#endif
#ifndef P3D_MAX_DEPTH
#define P3D_MAX_DEPTH 96u
#endif
#ifndef P3D_NAME_MAX
#define P3D_NAME_MAX 96u
#endif
#ifndef P3D_PATH_MAX
#define P3D_PATH_MAX 192u
#endif
#ifndef P3D_LINE_MAX
#define P3D_LINE_MAX 1536u
#endif

#define P3D_OK 0
#define P3D_ERR_NULL 1
#define P3D_ERR_READ 2
#define P3D_ERR_LINE_TOO_LONG 3
#define P3D_ERR_STACK_OVERFLOW 4
#define P3D_ERR_STACK_UNDERFLOW 5
#define P3D_ERR_UNSUPPORTED 6
#define P3D_ERR_OUTPUT_TOO_SMALL 7

#define P3D_FLAG_VERTEX_OVERFLOW      0x00000001UL
#define P3D_FLAG_TEX_OVERFLOW         0x00000002UL
#define P3D_FLAG_NORMAL_OVERFLOW      0x00000004UL
#define P3D_FLAG_FACE_OVERFLOW        0x00000008UL
#define P3D_FLAG_MATERIAL_OVERFLOW    0x00000010UL
#define P3D_FLAG_OBJECT_OVERFLOW      0x00000020UL
#define P3D_FLAG_REF_OVERFLOW         0x00000040UL
#define P3D_FLAG_CHANNEL_OVERFLOW     0x00000080UL
#define P3D_FLAG_DEPTH_OVERFLOW       0x00000100UL
#define P3D_FLAG_BRACE_UNDERFLOW      0x00000200UL
#define P3D_FLAG_LINE_TRUNCATED       0x00000400UL
#define P3D_FLAG_UNSUPPORTED_BIN      0x00000800UL
#define P3D_FLAG_DEP_OVERFLOW         0x00001000UL
#define P3D_FLAG_MORPH_OVERFLOW       0x00002000UL
#define P3D_FLAG_MORPH_DELTA_OVERFLOW 0x00004000UL
#define P3D_FLAG_JOINT_OVERFLOW       0x00008000UL
#define P3D_FLAG_ZONE_OVERFLOW        0x00010000UL
#define P3D_FLAG_SHADER_OVERFLOW      0x00020000UL
#define P3D_FLAG_SHADER_LINK_OVERFLOW 0x00040000UL
#define P3D_FLAG_WEIGHT_OVERFLOW      0x00080000UL
#define P3D_FLAG_PACKED_GZIP          0x00100000UL
#define P3D_FLAG_PACKED_ZLIB          0x00200000UL
#define P3D_FLAG_PACKED_UNSUPPORTED   0x00400000UL
#define P3D_FLAG_PMD_PARSED           0x00800000UL
#define P3D_FLAG_PMD_BEST_EFFORT      0x01000000UL
#define P3D_FLAG_PMD_UNSUPPORTED      0x02000000UL

#define P3D_REF_GEOMETRY 1
#define P3D_REF_TEXTURE  2
#define P3D_REF_SCRIPT   3
#define P3D_REF_MORPH    4
#define P3D_REF_OTHER    5

#define P3D_OBJ_UNKNOWN  0
#define P3D_OBJ_PROP     1
#define P3D_OBJ_FIGURE   2
#define P3D_OBJ_ACTOR    3
#define P3D_OBJ_CAMERA   4
#define P3D_OBJ_LIGHT    5
#define P3D_OBJ_GROUP    6
#define P3D_OBJ_DEFORMER 7

#define P3D_CH_NONE        0
#define P3D_CH_TRANSLATE_X 1
#define P3D_CH_TRANSLATE_Y 2
#define P3D_CH_TRANSLATE_Z 3
#define P3D_CH_ROTATE_X    4
#define P3D_CH_ROTATE_Y    5
#define P3D_CH_ROTATE_Z    6
#define P3D_CH_SCALE       7
#define P3D_CH_SCALE_X     8
#define P3D_CH_SCALE_Y     9
#define P3D_CH_SCALE_Z     10
#define P3D_CH_MORPH       11
#define P3D_CH_VALUE       12
#define P3D_CH_OTHER       13

#define P3D_DEP_DELTA_ADD  1
#define P3D_DEP_TIMES      2
#define P3D_DEP_PLUS       3
#define P3D_DEP_CONST      4
#define P3D_DEP_KEY        5
#define P3D_DEP_MIN        6
#define P3D_DEP_MAX        7
#define P3D_DEP_DIVIDE_BY  8
#define P3D_DEP_MINUS      9
#define P3D_DEP_DELTA_MUL  10
#define P3D_DEP_UNKNOWN    255

#define P3D_ZONE_SPHERE    1
#define P3D_ZONE_CAPSULE   2
#define P3D_ZONE_WEIGHTMAP 3
#define P3D_ZONE_ANGLE     4
#define P3D_ZONE_UNKNOWN   255

#define P3D_FX_ONE 65536L
#define P3D_NO_INDEX 65535u

typedef long p3d_fx;

typedef struct P3D_Vec3_s { p3d_fx x; p3d_fx y; p3d_fx z; } P3D_Vec3;
typedef struct P3D_Vec2_s { p3d_fx u; p3d_fx v; } P3D_Vec2;

typedef struct P3D_Face_s {
    unsigned short v[4];
    unsigned short vt[4];
    unsigned short vn[4];
    unsigned char count;
    unsigned short material;
    unsigned short object;
} P3D_Face;

typedef struct P3D_Material_s {
    char name[P3D_NAME_MAX];
} P3D_Material;

typedef struct P3D_Object_s {
    char name[P3D_NAME_MAX];
    unsigned char type;
    unsigned short parent;
    p3d_fx tx; p3d_fx ty; p3d_fx tz;
    p3d_fx rx; p3d_fx ry; p3d_fx rz;
    p3d_fx sx; p3d_fx sy; p3d_fx sz;
    unsigned long first_face;
    unsigned long face_count;
} P3D_Object;

typedef struct P3D_Ref_s {
    unsigned char type;
    char key[P3D_NAME_MAX];
    char path[P3D_PATH_MAX];
} P3D_Ref;

typedef struct P3D_Channel_s {
    unsigned char type;
    unsigned short object;
    unsigned short morph;
    char name[P3D_NAME_MAX];
    p3d_fx init_value;
    p3d_fx value;
    p3d_fx min_value;
    p3d_fx max_value;
    p3d_fx first_key_time;
    p3d_fx first_key_value;
    p3d_fx last_key_time;
    p3d_fx last_key_value;
    unsigned long key_count;
} P3D_Channel;

typedef struct P3D_Dependency_s {
    unsigned char op;
    unsigned short slave_channel;
    unsigned short master_object;
    unsigned short master_channel;
    char master_actor[P3D_NAME_MAX];
    char master_param[P3D_NAME_MAX];
    p3d_fx scale;
    p3d_fx add;
    p3d_fx first_key_x;
    p3d_fx first_key_y;
    p3d_fx last_key_x;
    p3d_fx last_key_y;
    unsigned long key_count;
} P3D_Dependency;

typedef struct P3D_Morph_s {
    char name[P3D_NAME_MAX];
    unsigned short object;
    unsigned long first_delta;
    unsigned long delta_count;
    p3d_fx value;
    unsigned char source; /* 1=text targetGeom, 2=PMD best-effort */
} P3D_Morph;

typedef struct P3D_MorphDelta_s {
    unsigned short morph;
    unsigned short vertex;
    P3D_Vec3 delta;
} P3D_MorphDelta;

typedef struct P3D_Joint_s {
    char name[P3D_NAME_MAX];
    unsigned short object;
    P3D_Vec3 origin;
    P3D_Vec3 endpoint;
    char order[8];
    unsigned long first_zone;
    unsigned long zone_count;
} P3D_Joint;

typedef struct P3D_Zone_s {
    unsigned char type;
    unsigned short joint;
    char name[P3D_NAME_MAX];
    P3D_Vec3 a;
    P3D_Vec3 b;
    p3d_fx inner_radius;
    p3d_fx outer_radius;
    p3d_fx min_angle;
    p3d_fx max_angle;
} P3D_Zone;

typedef struct P3D_ShaderNode_s {
    unsigned short material;
    char name[P3D_NAME_MAX];
    char kind[P3D_NAME_MAX];
    char image[P3D_PATH_MAX];
    p3d_fx color[4];
    p3d_fx value[4];
} P3D_ShaderNode;

typedef struct P3D_ShaderLink_s {
    unsigned short material;
    unsigned short from_node;
    unsigned short to_node;
    char from_socket[P3D_NAME_MAX];
    char to_socket[P3D_NAME_MAX];
} P3D_ShaderLink;

typedef struct P3D_SkinWeight_s {
    unsigned short object;
    unsigned short joint;
    unsigned short vertex;
    p3d_fx weight;
} P3D_SkinWeight;

typedef struct P3D_Context_s {
    P3D_Vec3 vertices[P3D_MAX_VERTICES];
    P3D_Vec2 texcoords[P3D_MAX_TEXCOORDS];
    P3D_Vec3 normals[P3D_MAX_NORMALS];
    P3D_Face faces[P3D_MAX_FACES];
    P3D_Material materials[P3D_MAX_MATERIALS];
    P3D_Object objects[P3D_MAX_OBJECTS];
    P3D_Ref refs[P3D_MAX_REFS];
    P3D_Channel channels[P3D_MAX_CHANNELS];
    P3D_Dependency dependencies[P3D_MAX_DEPENDENCIES];
    P3D_Morph morphs[P3D_MAX_MORPHS];
    P3D_MorphDelta morph_deltas[P3D_MAX_MORPH_DELTAS];
    P3D_Joint joints[P3D_MAX_JOINTS];
    P3D_Zone zones[P3D_MAX_ZONES];
    P3D_ShaderNode shader_nodes[P3D_MAX_SHADER_NODES];
    P3D_ShaderLink shader_links[P3D_MAX_SHADER_LINKS];
    P3D_SkinWeight skin_weights[P3D_MAX_SKIN_WEIGHTS];

    unsigned long vertex_count, texcoord_count, normal_count, face_count;
    unsigned long material_count, object_count, ref_count, channel_count;
    unsigned long dependency_count, morph_count, morph_delta_count;
    unsigned long joint_count, zone_count, shader_node_count, shader_link_count;
    unsigned long skin_weight_count, line_count;

    unsigned long flags;
    int last_error;
    char file_kind[16];
    p3d_fx version_number;
} P3D_Context;

typedef int (*P3D_ReadByteFn)(void *user);

void p3d_init(P3D_Context *ctx);
int p3d_decode_stream(P3D_Context *ctx, P3D_ReadByteFn read_byte, void *user);
int p3d_decode_memory(P3D_Context *ctx, const unsigned char *data, unsigned long size);

int p3d_decode_pmd_memory(P3D_Context *ctx, const unsigned char *data, unsigned long size);
int p3d_apply_morphs(const P3D_Context *ctx, P3D_Vec3 *out_vertices, unsigned long max_vertices);
int p3d_eval_dependencies(P3D_Context *ctx);
p3d_fx p3d_eval_zone_weight(const P3D_Context *ctx, unsigned short zone_idx, P3D_Vec3 p);
int p3d_apply_skin_translation_only(const P3D_Context *ctx, const P3D_Vec3 *joint_offsets, unsigned long joint_count, P3D_Vec3 *out_vertices, unsigned long max_vertices);

p3d_fx p3d_fx_from_int(long v);
p3d_fx p3d_fx_from_text(const char *s);
p3d_fx p3d_fx_mul(p3d_fx a, p3d_fx b);
p3d_fx p3d_fx_div(p3d_fx a, p3d_fx b);
void p3d_fx_to_text(p3d_fx v, char *out, unsigned int out_size, unsigned int decimals);

const char *p3d_ref_type_name(unsigned int t);
const char *p3d_object_type_name(unsigned int t);
const char *p3d_channel_type_name(unsigned int t);
const char *p3d_dep_type_name(unsigned int t);
const char *p3d_zone_type_name(unsigned int t);

#ifdef __cplusplus
}
#endif

#endif
