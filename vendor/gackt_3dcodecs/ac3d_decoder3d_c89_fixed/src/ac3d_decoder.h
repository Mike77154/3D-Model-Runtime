#ifndef AC3D_DECODER_H
#define AC3D_DECODER_H

/*
   ac3d_decoder.h - tiny AC3D .ac ASCII decoder, C89, fixed point, no heap.

   The core library never calls malloc/free/realloc and never uses float/double.
   The caller owns all storage through ac3d_model.

   Default fixed format is signed Q16.16 in a 32-bit long.  Multiplication is
   implemented with 16-bit partial products to keep the code C89-friendly.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AC3D_MAX_MATERIALS
#define AC3D_MAX_MATERIALS 64
#endif

#ifndef AC3D_MAX_OBJECTS
#define AC3D_MAX_OBJECTS 256
#endif

#ifndef AC3D_MAX_VERTICES
#define AC3D_MAX_VERTICES 8192
#endif

#ifndef AC3D_MAX_SURFACES
#define AC3D_MAX_SURFACES 8192
#endif

#ifndef AC3D_MAX_REFS
#define AC3D_MAX_REFS 32768
#endif

#ifndef AC3D_MAX_NAME
#define AC3D_MAX_NAME 64
#endif

#ifndef AC3D_MAX_PATH
#define AC3D_MAX_PATH 128
#endif

#ifndef AC3D_MAX_DATA
#define AC3D_MAX_DATA 128
#endif

#define AC3D_FIXED_SHIFT 16
#define AC3D_FIXED_ONE   ((long)65536L)
#define AC3D_FIXED_HALF  ((long)32768L)

#define AC3D_OK                         0
#define AC3D_ERR_BAD_ARG               -1
#define AC3D_ERR_BAD_HEADER            -2
#define AC3D_ERR_UNEXPECTED_EOF        -3
#define AC3D_ERR_OVERFLOW_MATERIALS    -4
#define AC3D_ERR_OVERFLOW_OBJECTS      -5
#define AC3D_ERR_OVERFLOW_VERTICES     -6
#define AC3D_ERR_OVERFLOW_SURFACES     -7
#define AC3D_ERR_OVERFLOW_REFS         -8
#define AC3D_ERR_PARSE                 -9
#define AC3D_ERR_UNSUPPORTED           -10
#define AC3D_ERR_OVERFLOW_SUBDIV        -11

#define AC3D_OBJ_UNKNOWN 0
#define AC3D_OBJ_WORLD   1
#define AC3D_OBJ_POLY    2
#define AC3D_OBJ_GROUP   3
#define AC3D_OBJ_LIGHT   4

#define AC3D_SURF_POLYGON     0
#define AC3D_SURF_CLOSEDLINE  1
#define AC3D_SURF_LINE        2
#define AC3D_SURF_TYPE_MASK   15
#define AC3D_SURF_SHADED      16
#define AC3D_SURF_TWOSIDED    32

#define AC3D_FLAG_HIDDEN 1
#define AC3D_FLAG_LOCKED 2
#define AC3D_FLAG_FOLDED 4

#ifndef AC3D_MAX_EVAL_SUBDIV
#define AC3D_MAX_EVAL_SUBDIV 4
#endif

/*
   Catmull-Clark evaluator workspace limits.  These are static storage limits,
   not heap allocations.  Override at compile time for bigger/smaller targets.
*/
#ifndef AC3D_MAX_CC_VERTICES
#define AC3D_MAX_CC_VERTICES 16384
#endif

#ifndef AC3D_MAX_CC_FACES
#define AC3D_MAX_CC_FACES 32768
#endif

#ifndef AC3D_MAX_CC_REFS
#define AC3D_MAX_CC_REFS 131072
#endif

#ifndef AC3D_MAX_CC_EDGES
#define AC3D_MAX_CC_EDGES 65536
#endif

struct ac3d_vec3 {
    long x;
    long y;
    long z;
};

struct ac3d_vec2 {
    long u;
    long v;
};

struct ac3d_mat3x4 {
    long m[3][3];
    long t[3];
};

struct ac3d_material {
    char name[AC3D_MAX_NAME];
    long rgb[3];
    long amb[3];
    long emis[3];
    long spec[3];
    int shi;
    long trans;
    char data[AC3D_MAX_DATA];
};

struct ac3d_vertex {
    long x;
    long y;
    long z;
};

struct ac3d_ref {
    int vertex_index;          /* local-to-object vertex index from file */
    long u;                    /* raw surface UV, fixed */
    long v;
};

struct ac3d_surface {
    int object_index;
    int flags;
    int material_index;
    int first_ref;
    int ref_count;
};

struct ac3d_object {
    int type;
    int parent_index;
    int first_child;
    int child_count;
    int first_vertex;
    int vertex_count;
    int first_surface;
    int surface_count;
    int flags;
    int subdiv;
    long crease;
    char name[AC3D_MAX_NAME];
    char data[AC3D_MAX_DATA];
    char texture[AC3D_MAX_PATH];
    char url[AC3D_MAX_PATH];
    long texrep[2];
    long texoff[2];
    struct ac3d_mat3x4 local;
    struct ac3d_mat3x4 world;
};

struct ac3d_model {
    char version_char;
    int material_count;
    int object_count;
    int vertex_count;
    int surface_count;
    int ref_count;
    int root_object;
    int warning_count;
    int ignored_token_count;
    int last_error_line;
    struct ac3d_material materials[AC3D_MAX_MATERIALS];
    struct ac3d_object objects[AC3D_MAX_OBJECTS];
    struct ac3d_vertex vertices[AC3D_MAX_VERTICES];
    struct ac3d_surface surfaces[AC3D_MAX_SURFACES];
    struct ac3d_ref refs[AC3D_MAX_REFS];
};

struct ac3d_triangle {
    int object_index;
    int surface_index;
    int material_index;
    int flags;
    struct ac3d_vec3 p[3];     /* world space */
    struct ac3d_vec2 uv[3];    /* texrep/texoff already applied */
};

struct ac3d_triangle_ex {
    int object_index;
    int surface_index;
    int material_index;
    int flags;
    int subdiv_level;          /* generated/evaluated subdivision depth */
    struct ac3d_vec3 p[3];     /* world space */
    struct ac3d_vec2 uv[3];    /* texrep/texoff already applied */
    struct ac3d_vec3 n[3];     /* world-space vertex normals, Q16.16 */
    struct ac3d_vec3 face_normal;
};

struct ac3d_line_segment {
    int object_index;
    int surface_index;
    int material_index;
    int flags;
    struct ac3d_vec3 p[2];
    struct ac3d_vec2 uv[2];
};

typedef int (*ac3d_triangle_cb)(const struct ac3d_triangle *tri, void *user);
typedef int (*ac3d_triangle_ex_cb)(const struct ac3d_triangle_ex *tri, void *user);
typedef int (*ac3d_line_cb)(const struct ac3d_line_segment *seg, void *user);

void ac3d_model_init(struct ac3d_model *model);
int ac3d_decode_memory(struct ac3d_model *model, const char *data, unsigned long size);
int ac3d_build_world_matrices(struct ac3d_model *model);
int ac3d_for_each_triangle(const struct ac3d_model *model, ac3d_triangle_cb cb, void *user);
int ac3d_for_each_triangle_ex(const struct ac3d_model *model, int eval_subdiv, ac3d_triangle_ex_cb cb, void *user);
int ac3d_for_each_triangle_catmull_clark(const struct ac3d_model *model, int eval_subdiv, ac3d_triangle_ex_cb cb, void *user);
int ac3d_for_each_line_segment(const struct ac3d_model *model, ac3d_line_cb cb, void *user);
int ac3d_surface_face_normal(const struct ac3d_model *model, int surface_index, struct ac3d_vec3 *out);
int ac3d_ref_smooth_normal(const struct ac3d_model *model, int surface_index, int ref_offset, struct ac3d_vec3 *out);

long ac3d_fixed_from_int(long v);
long ac3d_fixed_mul(long a, long b);
long ac3d_fixed_div(long a, long b);
long ac3d_fixed_sqrt(long v);
long ac3d_fixed_cos_degrees(long deg_fixed);
long ac3d_fixed_div_int(long a, long b);
long ac3d_fixed_parse_cstr(const char *s, int *ok);
void ac3d_fixed_to_ascii(long v, char *out, int out_size, int frac_digits);

#ifdef __cplusplus
}
#endif

#endif
