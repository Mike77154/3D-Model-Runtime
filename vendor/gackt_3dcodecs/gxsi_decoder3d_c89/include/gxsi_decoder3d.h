#ifndef GXSI_DECODER3D_H
#define GXSI_DECODER3D_H

/*
   gxsi_decoder3d - small Softimage dotXSI decoder for static-memory engines.
   C89-oriented, no heap allocation in the core, fixed-point numeric storage.
   Public domain / CC0-style sample code generated for compact game runtimes.
*/

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GXSI_MAX_MESHES
#define GXSI_MAX_MESHES       64
#endif
#ifndef GXSI_MAX_MATERIALS
#define GXSI_MAX_MATERIALS    64
#endif
#ifndef GXSI_MAX_VERTICES
#define GXSI_MAX_VERTICES     32768
#endif
#ifndef GXSI_MAX_NORMALS
#define GXSI_MAX_NORMALS      32768
#endif
#ifndef GXSI_MAX_TEXCOORDS
#define GXSI_MAX_TEXCOORDS    32768
#endif
#ifndef GXSI_MAX_TRIANGLES
#define GXSI_MAX_TRIANGLES    65535
#endif
#ifndef GXSI_MAX_BONES
#define GXSI_MAX_BONES        256
#endif
#ifndef GXSI_MAX_ENVELOPES
#define GXSI_MAX_ENVELOPES    64
#endif
#ifndef GXSI_MAX_WEIGHTS
#define GXSI_MAX_WEIGHTS      65535
#endif
#ifndef GXSI_MAX_FCURVES
#define GXSI_MAX_FCURVES      512
#endif
#ifndef GXSI_MAX_FCURVE_KEYS
#define GXSI_MAX_FCURVE_KEYS  8192
#endif
#ifndef GXSI_MAX_NURBS
#define GXSI_MAX_NURBS        64
#endif
#ifndef GXSI_MAX_NURBS_CP
#define GXSI_MAX_NURBS_CP     8192
#endif
#ifndef GXSI_NAME_MAX
#define GXSI_NAME_MAX         64
#endif
#ifndef GXSI_MAX_WARNINGS
#define GXSI_MAX_WARNINGS     24
#endif

#define GXSI_VERSION_MAJOR 0
#define GXSI_VERSION_MINOR 2
#define GXSI_VERSION_PATCH 0

/* Q16.16 signed fixed point. Values beyond range are clamped by the parser. */
typedef long gxsi_fx;

#define GXSI_FX_ONE ((gxsi_fx)65536L)
#define GXSI_FX_ZERO ((gxsi_fx)0L)
#define GXSI_FX_FROM_INT(x) ((gxsi_fx)((x) << 16))
#define GXSI_FX_TO_INT(x) ((int)((x) >> 16))

/* Decoder status codes. */
enum {
    GXSI_OK = 0,
    GXSI_ERR_NULL = -1,
    GXSI_ERR_BAD_HEADER = -2,
    GXSI_ERR_OVERFLOW = -3,
    GXSI_ERR_PARSE = -4,
    GXSI_ERR_UNSUPPORTED_BINARY = -5,
    GXSI_ERR_BINARY_LAYOUT_UNKNOWN = -6
};

/* Flags for GXSI_Options.flags. */
enum {
    GXSI_OPT_ACCEPT_LOOSE_HEADER = 1,
    GXSI_OPT_PARSE_TRISTRIPS     = 2,
    GXSI_OPT_KEEP_GOING          = 4,
    GXSI_OPT_PARSE_ADVANCED      = 8,
    GXSI_OPT_APPLY_TRANSFORMS    = 16,
    GXSI_OPT_BINARY_PROBE        = 32
};

typedef struct GXSI_Vec3 {
    gxsi_fx x;
    gxsi_fx y;
    gxsi_fx z;
} GXSI_Vec3;

typedef struct GXSI_Vec4 {
    gxsi_fx x;
    gxsi_fx y;
    gxsi_fx z;
    gxsi_fx w;
} GXSI_Vec4;

typedef struct GXSI_Vec2 {
    gxsi_fx u;
    gxsi_fx v;
} GXSI_Vec2;

typedef struct GXSI_Triangle {
    unsigned short v[3];
    unsigned short n[3];
    unsigned short uv[3];
    unsigned short material;
    unsigned short mesh;
    unsigned short flags;
} GXSI_Triangle;

/* Common dotXSI SRT order: sx sy sz, rx ry rz, tx ty tz. */
typedef struct GXSI_TransformSRT {
    gxsi_fx scale[3];
    gxsi_fx rot_deg[3];
    gxsi_fx trans[3];
    int present;
} GXSI_TransformSRT;

typedef struct GXSI_Material {
    char name[GXSI_NAME_MAX];
    gxsi_fx diffuse[4];
    gxsi_fx specular[3];
    gxsi_fx ambient[3];
    gxsi_fx emission[3];
    gxsi_fx shininess;
} GXSI_Material;

typedef struct GXSI_Mesh {
    char name[GXSI_NAME_MAX];
    char model_name[GXSI_NAME_MAX];
    char material_name[GXSI_NAME_MAX];
    GXSI_TransformSRT transform;
    unsigned int first_vertex;
    unsigned int vertex_count;
    unsigned int first_normal;
    unsigned int normal_count;
    unsigned int first_texcoord;
    unsigned int texcoord_count;
    unsigned int first_triangle;
    unsigned int triangle_count;
} GXSI_Mesh;

typedef struct GXSI_Bone {
    char name[GXSI_NAME_MAX];
    int parent;
    GXSI_TransformSRT transform;
} GXSI_Bone;

typedef struct GXSI_Weight {
    unsigned short vertex;
    unsigned short bone;
    gxsi_fx weight;
} GXSI_Weight;

typedef struct GXSI_Envelope {
    char name[GXSI_NAME_MAX];
    unsigned short mesh;
    unsigned int first_weight;
    unsigned int weight_count;
} GXSI_Envelope;

typedef struct GXSI_FCurveKey {
    gxsi_fx time;
    gxsi_fx value;
    gxsi_fx in_tan;
    gxsi_fx out_tan;
    unsigned short interp;
} GXSI_FCurveKey;

typedef struct GXSI_FCurve {
    char name[GXSI_NAME_MAX];
    char target[GXSI_NAME_MAX];
    char parameter[GXSI_NAME_MAX];
    unsigned int first_key;
    unsigned int key_count;
    unsigned short flags;
} GXSI_FCurve;

typedef struct GXSI_Nurbs {
    char name[GXSI_NAME_MAX];
    unsigned short type;
    unsigned short degree_u;
    unsigned short degree_v;
    unsigned short count_u;
    unsigned short count_v;
    unsigned int first_cp;
    unsigned int cp_count;
} GXSI_Nurbs;

typedef struct GXSI_BinaryProbe {
    int detected;
    int header_bytes;
    int version_hint;
    int bits_hint;
    char format_hint[16];
} GXSI_BinaryProbe;

typedef struct GXSI_Scene {
    GXSI_Mesh meshes[GXSI_MAX_MESHES];
    GXSI_Material materials[GXSI_MAX_MATERIALS];
    GXSI_Vec3 vertices[GXSI_MAX_VERTICES];
    GXSI_Vec3 normals[GXSI_MAX_NORMALS];
    GXSI_Vec2 texcoords[GXSI_MAX_TEXCOORDS];
    GXSI_Triangle triangles[GXSI_MAX_TRIANGLES];

    GXSI_Bone bones[GXSI_MAX_BONES];
    GXSI_Envelope envelopes[GXSI_MAX_ENVELOPES];
    GXSI_Weight weights[GXSI_MAX_WEIGHTS];
    GXSI_FCurve fcurves[GXSI_MAX_FCURVES];
    GXSI_FCurveKey fcurve_keys[GXSI_MAX_FCURVE_KEYS];
    GXSI_Nurbs nurbs[GXSI_MAX_NURBS];
    GXSI_Vec4 nurbs_cp[GXSI_MAX_NURBS_CP];

    unsigned int mesh_count;
    unsigned int material_count;
    unsigned int vertex_count;
    unsigned int normal_count;
    unsigned int texcoord_count;
    unsigned int triangle_count;
    unsigned int bone_count;
    unsigned int envelope_count;
    unsigned int weight_count;
    unsigned int fcurve_count;
    unsigned int fcurve_key_count;
    unsigned int nurbs_count;
    unsigned int nurbs_cp_count;

    int xsi_major;
    int xsi_minor;
    int is_binary;
    GXSI_BinaryProbe binary_probe;
    int warning_count;
    char warnings[GXSI_MAX_WARNINGS][96];
} GXSI_Scene;

typedef struct GXSI_Options {
    unsigned int flags;
} GXSI_Options;

typedef void (*GXSI_WriteFn)(void *user, const char *text);

void gxsi_scene_init(GXSI_Scene *scene);
void gxsi_default_options(GXSI_Options *opt);

int gxsi_decode_ascii(const char *data, size_t size, GXSI_Scene *scene, const GXSI_Options *opt);
int gxsi_decode_auto(const void *data, size_t size, GXSI_Scene *scene, const GXSI_Options *opt);
int gxsi_probe_binary_dotxsi(const unsigned char *data, size_t size, GXSI_Scene *scene);

const char *gxsi_status_string(int status);
int gxsi_find_material(const GXSI_Scene *scene, const char *name);
int gxsi_find_bone(const GXSI_Scene *scene, const char *name);

/* Fixed point helpers. */
gxsi_fx gxsi_fx_from_decimal(const char *text);
void gxsi_fx_to_string(gxsi_fx v, char *out, size_t out_size, int decimals);

/* Applies captured mesh SRT to vertices in-place. Rotation uses integer-degree lookup. */
void gxsi_apply_mesh_transforms(GXSI_Scene *scene);

/* Optional OBJ emission through callbacks. This is still no-heap and no-stdio in core. */
void gxsi_write_obj(const GXSI_Scene *scene, GXSI_WriteFn write_fn, void *user);

#ifdef __cplusplus
}
#endif

#endif
