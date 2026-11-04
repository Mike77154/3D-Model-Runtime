#ifndef SIB3D_H
#define SIB3D_H

/*
   sib3d.h - tiny Silo/Nevercenter .SIB decoder core.

   Design contract:
   - ISO C89 style public API.
   - No heap allocation inside the library.
   - No float or double arithmetic/types in the decoder.
   - Input is caller-owned memory.
   - Output is caller-owned SIB_Scene, preferably static/global for large caps.

   Values that are stored as IEEE-754 binary32 in .sib are converted to Q16.16
   signed fixed point by integer bit decoding.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SIB_CFG_MAX_MATERIALS
#define SIB_CFG_MAX_MATERIALS 64u
#endif

#ifndef SIB_CFG_MAX_MESHES
#define SIB_CFG_MAX_MESHES 128u
#endif

#ifndef SIB_CFG_MAX_OBJECTS
#define SIB_CFG_MAX_OBJECTS 256u
#endif

#ifndef SIB_CFG_MAX_LIGHTS
#define SIB_CFG_MAX_LIGHTS 64u
#endif

#ifndef SIB_CFG_MAX_VERTICES
#define SIB_CFG_MAX_VERTICES 65536u
#endif

#ifndef SIB_CFG_MAX_FACES
#define SIB_CFG_MAX_FACES 32768u
#endif

#ifndef SIB_CFG_MAX_CORNERS
#define SIB_CFG_MAX_CORNERS 131072u
#endif

#ifndef SIB_CFG_MAX_EDGES
#define SIB_CFG_MAX_EDGES 131072u
#endif

#ifndef SIB_CFG_MAX_NAME
#define SIB_CFG_MAX_NAME 64u
#endif

#ifndef SIB_CFG_MAX_TEXTURE
#define SIB_CFG_MAX_TEXTURE 128u
#endif

#ifndef SIB_CFG_MAX_WARNINGS
#define SIB_CFG_MAX_WARNINGS 32u
#endif

#define SIB_FIXED_SHIFT 16
#define SIB_FIXED_ONE 65536L
#define SIB_INVALID_INDEX 0xffffffffUL

/* C89 has no stdint.h. These aliases intentionally use common C89 widths.
   The reader masks byte-built values, so unsigned long may be wider on LP64. */
typedef signed char SIB_I8;
typedef unsigned char SIB_U8;
typedef signed short SIB_I16;
typedef unsigned short SIB_U16;
typedef signed long SIB_I32;
typedef unsigned long SIB_U32;
typedef unsigned long SIB_Size;

typedef enum SIB_ResultTag {
    SIB_OK = 0,
    SIB_ERR_NULL = 1,
    SIB_ERR_TRUNCATED = 2,
    SIB_ERR_BAD_CHUNK = 3,
    SIB_ERR_UNSUPPORTED_VERSION = 4,
    SIB_ERR_CAPACITY = 5,
    SIB_ERR_BAD_INDEX = 6,
    SIB_ERR_BAD_DATA = 7
} SIB_Result;

typedef struct SIB_Vec2 {
    SIB_I32 x;
    SIB_I32 y;
} SIB_Vec2;

typedef struct SIB_Vec3 {
    SIB_I32 x;
    SIB_I32 y;
    SIB_I32 z;
} SIB_Vec3;

typedef struct SIB_Mat4 {
    /* Row-major Q16.16: [0..3], [4..7], [8..11], [12..15]. */
    SIB_I32 m[16];
} SIB_Mat4;

typedef struct SIB_Material {
    char name[SIB_CFG_MAX_NAME];
    char texture[SIB_CFG_MAX_TEXTURE];
    SIB_Vec3 diffuse;
    SIB_Vec3 ambient;
    SIB_Vec3 specular;
    SIB_Vec3 emissive;
    SIB_U32 shininess_raw;
} SIB_Material;

typedef struct SIB_Corner {
    SIB_U32 vertex;
    SIB_Vec2 uv;
    SIB_Vec3 normal;
} SIB_Corner;

typedef struct SIB_Face {
    SIB_U32 first_corner;
    SIB_U32 corner_count;
    SIB_U32 material;
} SIB_Face;

typedef struct SIB_Edge {
    SIB_U32 a;
    SIB_U32 b;
    SIB_U32 face_a;
    SIB_U32 face_b;
    SIB_U8 creased;
} SIB_Edge;

typedef struct SIB_Mesh {
    char name[SIB_CFG_MAX_NAME];
    SIB_Mat4 axis;
    SIB_U32 first_vertex;
    SIB_U32 vertex_count;
    SIB_U32 first_face;
    SIB_U32 face_count;
    SIB_U32 first_corner;
    SIB_U32 corner_count;
    SIB_U32 first_edge;
    SIB_U32 edge_count;
} SIB_Mesh;

typedef enum SIB_LightType {
    SIB_LIGHT_UNDEFINED = 0,
    SIB_LIGHT_POINT = 1,
    SIB_LIGHT_SPOT = 2,
    SIB_LIGHT_DIRECTIONAL = 3
} SIB_LightType;

typedef struct SIB_Light {
    char name[SIB_CFG_MAX_NAME];
    SIB_LightType type;
    SIB_Vec3 position;
    SIB_Vec3 direction;
    SIB_Vec3 diffuse;
    SIB_Vec3 ambient;
    SIB_Vec3 specular;
    SIB_I32 spot_exponent;
    SIB_I32 spot_cutoff;
    SIB_I32 attenuation_constant;
    SIB_I32 attenuation_linear;
    SIB_I32 attenuation_quadratic;
} SIB_Light;

typedef struct SIB_Object {
    char name[SIB_CFG_MAX_NAME];
    SIB_Mat4 axis;
    SIB_U32 mesh;
    SIB_U8 is_instance;
} SIB_Object;

typedef struct SIB_Diagnostics {
    SIB_Result result;
    SIB_U32 file_version;
    SIB_U32 warnings;
    char warning_text[SIB_CFG_MAX_WARNINGS][96];
    char error_text[128];
} SIB_Diagnostics;

typedef struct SIB_Scene {
    SIB_U32 material_count;
    SIB_U32 mesh_count;
    SIB_U32 object_count;
    SIB_U32 light_count;
    SIB_U32 vertex_count;
    SIB_U32 face_count;
    SIB_U32 corner_count;
    SIB_U32 edge_count;

    SIB_Material materials[SIB_CFG_MAX_MATERIALS];
    SIB_Mesh meshes[SIB_CFG_MAX_MESHES];
    SIB_Object objects[SIB_CFG_MAX_OBJECTS];
    SIB_Light lights[SIB_CFG_MAX_LIGHTS];
    SIB_Vec3 vertices[SIB_CFG_MAX_VERTICES];
    SIB_Face faces[SIB_CFG_MAX_FACES];
    SIB_Corner corners[SIB_CFG_MAX_CORNERS];
    SIB_Edge edges[SIB_CFG_MAX_EDGES];

    /* Internal scratch kept in-scene so the decoder still uses no heap. */
    SIB_Vec3 _face_normals[SIB_CFG_MAX_FACES];
} SIB_Scene;

void sib_scene_init(SIB_Scene *scene);
void sib_diagnostics_init(SIB_Diagnostics *diag);
SIB_Result sib_decode_memory(const SIB_U8 *data,
                             SIB_U32 size,
                             SIB_Scene *scene,
                             SIB_Diagnostics *diag);

const char *sib_result_name(SIB_Result r);
SIB_I32 sib_fixed_from_i32(SIB_I32 v);
SIB_I32 sib_fixed_to_i32_floor(SIB_I32 v);
void sib_fixed_to_decimal(SIB_I32 v, char *out, SIB_U32 out_size, SIB_U32 digits);

#ifdef __cplusplus
}
#endif

#endif
