#ifndef Q3D_CODEC_H_INCLUDED
#define Q3D_CODEC_H_INCLUDED

/*
   q3d_codec.h - tiny Quick3D Q3O/Q3S codec, C89, no heap.
   Public-domain/CC0-style sample library generated for static embedding.

   Design rules:
   - no malloc/realloc/free
   - no float/double in public data path
   - parses 32-bit IEEE little-endian file floats into Q16.16 fixed point
   - user owns storage by providing Q3D_Model, preferably static/global
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef Q3D_MAX_MESHES
#define Q3D_MAX_MESHES             32u
#endif
#ifndef Q3D_MAX_MATERIALS
#define Q3D_MAX_MATERIALS          64u
#endif
#ifndef Q3D_MAX_TEXTURES
#define Q3D_MAX_TEXTURES           16u
#endif
#ifndef Q3D_MAX_VERTICES
#define Q3D_MAX_VERTICES           8192u
#endif
#ifndef Q3D_MAX_NORMALS
#define Q3D_MAX_NORMALS            8192u
#endif
#ifndef Q3D_MAX_UVS
#define Q3D_MAX_UVS                8192u
#endif
#ifndef Q3D_MAX_FACES
#define Q3D_MAX_FACES              4096u
#endif
#ifndef Q3D_MAX_CORNERS
#define Q3D_MAX_CORNERS            24576u
#endif
#ifndef Q3D_MAX_TEXTURE_PIXELS
#define Q3D_MAX_TEXTURE_PIXELS     65536u
#endif
#ifndef Q3D_NAME_MAX
#define Q3D_NAME_MAX               64u
#endif

#define Q3D_FIXED_SHIFT 16
#define Q3D_FIXED_ONE   ((long)65536)
#define Q3D_INVALID_INDEX ((unsigned long)0xffffffffUL)

/* status values */
#define Q3D_OK                         0
#define Q3D_ERR_NULL                  -1
#define Q3D_ERR_TOO_SMALL             -2
#define Q3D_ERR_BAD_SIGNATURE         -3
#define Q3D_ERR_EOF                   -4
#define Q3D_ERR_CAPACITY              -5
#define Q3D_ERR_BAD_COUNT             -6
#define Q3D_ERR_BAD_INDEX             -7
#define Q3D_ERR_UNKNOWN_CHUNK         -8
#define Q3D_ERR_TEXTURE_OVERFLOW      -9
#define Q3D_ERR_WRITE_OVERFLOW       -10
#define Q3D_ERR_UNSUPPORTED          -11

/* feature flags */
#define Q3D_HAS_SCENE     0x00000001UL
#define Q3D_HAS_CAMERA    0x00000002UL
#define Q3D_HAS_LIGHT     0x00000004UL
#define Q3D_IS_SCENE_FILE 0x00000008UL
#define Q3D_TRUNCATED_NAME 0x00000010UL

/* C89-friendly fixed-width-ish types. The library expects char=8 bits,
   unsigned short >=16 bits, unsigned long >=32 bits. */
typedef unsigned char  Q3D_U8;
typedef signed char    Q3D_S8;
typedef unsigned short Q3D_U16;
typedef signed short   Q3D_S16;
typedef unsigned long  Q3D_U32;
typedef signed long    Q3D_S32;
typedef long           Q3D_Fixed;

typedef struct Q3D_Vec3_ {
    Q3D_Fixed x;
    Q3D_Fixed y;
    Q3D_Fixed z;
} Q3D_Vec3;

typedef struct Q3D_Vec2_ {
    Q3D_Fixed u;
    Q3D_Fixed v;
} Q3D_Vec2;

typedef struct Q3D_RGB_ {
    Q3D_U8 r;
    Q3D_U8 g;
    Q3D_U8 b;
} Q3D_RGB;

typedef struct Q3D_Corner_ {
    unsigned long vertex;
    unsigned long uv;
} Q3D_Corner;

typedef struct Q3D_Face_ {
    unsigned long first_corner;
    unsigned short corner_count;
    unsigned long material;
} Q3D_Face;

typedef struct Q3D_Mesh_ {
    unsigned long first_vertex;
    unsigned long vertex_count;
    unsigned long first_normal;
    unsigned long normal_count;
    unsigned long first_uv;
    unsigned long uv_count;
    unsigned long first_face;
    unsigned long face_count;
    unsigned long first_corner;
    unsigned long corner_count;
} Q3D_Mesh;

typedef struct Q3D_Material_ {
    char name[Q3D_NAME_MAX];
    Q3D_Vec3 ambient;
    Q3D_Vec3 diffuse;
    Q3D_Vec3 specular;
    Q3D_Fixed transparency;
    unsigned long texture;
} Q3D_Material;

typedef struct Q3D_Texture_ {
    char name[Q3D_NAME_MAX];
    unsigned long width;
    unsigned long height;
    unsigned long first_pixel;
    unsigned long pixel_count;
} Q3D_Texture;

typedef struct Q3D_SceneInfo_ {
    Q3D_Fixed root_matrix[16];
    Q3D_Vec3 camera_pos;
    Q3D_Vec3 foreground_color;
    Q3D_Vec3 light_color;
} Q3D_SceneInfo;

typedef struct Q3D_Model_ {
    char signature[9];
    char version_major;
    char version_minor;
    unsigned long flags;

    unsigned long mesh_count;
    unsigned long material_count;
    unsigned long texture_count;
    unsigned long vertex_count;
    unsigned long normal_count;
    unsigned long uv_count;
    unsigned long face_count;
    unsigned long corner_count;
    unsigned long texture_pixel_count;

    Q3D_Mesh meshes[Q3D_MAX_MESHES];
    Q3D_Material materials[Q3D_MAX_MATERIALS];
    Q3D_Texture textures[Q3D_MAX_TEXTURES];
    Q3D_Vec3 vertices[Q3D_MAX_VERTICES];
    Q3D_Vec3 normals[Q3D_MAX_NORMALS];
    Q3D_Vec2 uvs[Q3D_MAX_UVS];
    Q3D_Face faces[Q3D_MAX_FACES];
    Q3D_Corner corners[Q3D_MAX_CORNERS];
    Q3D_RGB pixels[Q3D_MAX_TEXTURE_PIXELS];
    Q3D_SceneInfo scene;
} Q3D_Model;

typedef struct Q3D_Diag_ {
    int code;
    unsigned long offset;
    char chunk;
    char message[96];
} Q3D_Diag;

typedef int (*Q3D_WriteFn)(void *user, const char *text, unsigned long bytes);

void q3d_clear_model(Q3D_Model *model);
void q3d_clear_diag(Q3D_Diag *diag);
int q3d_can_read(const void *data, unsigned long size);
int q3d_parse_buffer(Q3D_Model *model, Q3D_Diag *diag,
                     const void *data, unsigned long size);

/* Writes a simple Wavefront OBJ view of the decoded geometry via callback.
   It triangulates polygons fan-style for maximum compatibility. */
int q3d_emit_obj(const Q3D_Model *model, Q3D_WriteFn write_fn, void *user);

/* Basic Q3O encoder for round-tripping this library's in-memory model.
   It writes into a caller-provided buffer. No textures are compressed. */
int q3d_write_q3o(const Q3D_Model *model, void *out_data,
                  unsigned long out_size, unsigned long *written_bytes);

/* Fixed-point helpers. */
Q3D_Fixed q3d_fixed_from_int(long v);
long q3d_fixed_to_int(Q3D_Fixed v);
Q3D_Fixed q3d_fixed_mul(Q3D_Fixed a, Q3D_Fixed b);
Q3D_Fixed q3d_fixed_div(Q3D_Fixed a, Q3D_Fixed b);

#ifdef __cplusplus
}
#endif

#endif /* Q3D_CODEC_H_INCLUDED */
