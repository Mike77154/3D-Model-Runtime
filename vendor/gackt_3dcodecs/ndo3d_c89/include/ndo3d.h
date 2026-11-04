#ifndef NDO3D_H
#define NDO3D_H

#include "ndo3d_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NDO3D_VERSION_MAJOR 0
#define NDO3D_VERSION_MINOR 1
#define NDO3D_VERSION_PATCH 0

#define NDO3D_OK 0
#define NDO3D_ERR_NULL -1
#define NDO3D_ERR_TOO_SMALL -2
#define NDO3D_ERR_NOT_NDO -3
#define NDO3D_ERR_UNSUPPORTED -4
#define NDO3D_ERR_BOUNDS -5
#define NDO3D_ERR_CAPACITY -6
#define NDO3D_ERR_BAD_DATA -7
#define NDO3D_ERR_OVERFLOW -8
#define NDO3D_ERR_TRUNCATED -9

#define NDO3D_PERM_VISIBLE_UNLOCKED 0
#define NDO3D_PERM_VISIBLE_LOCKED 1
#define NDO3D_PERM_HIDDEN_UNLOCKED 2
#define NDO3D_PERM_HIDDEN_LOCKED 3

#define NDO3D_Q_ONE 65536L

/* C89-friendly integral aliases.  unsigned short is expected to be >= 16-bit;
   unsigned long is expected to be >= 32-bit on all supported targets. */
typedef unsigned char ndo3d_u8;
typedef unsigned short ndo3d_u16;
typedef unsigned long ndo3d_u32;
typedef signed long ndo3d_i32;
typedef ndo3d_i32 ndo3d_fixed;

typedef struct NDO3D_ColorRec_ {
    ndo3d_u8 r;
    ndo3d_u8 g;
    ndo3d_u8 b;
    ndo3d_u8 a;
} NDO3D_Color;

typedef struct NDO3D_VertexRec_ {
    ndo3d_u16 edge;
    ndo3d_fixed x;
    ndo3d_fixed y;
    ndo3d_fixed z;
} NDO3D_Vertex;

typedef struct NDO3D_EdgeRec_ {
    ndo3d_u16 vs;
    ndo3d_u16 ve;
    ndo3d_u16 lf;
    ndo3d_u16 rf;
    ndo3d_u16 ltsu;
    ndo3d_u16 rtsu;
    ndo3d_u16 rtpr;
    ndo3d_u16 ltpr;
    ndo3d_u8 hard;
    NDO3D_Color left_color;
    NDO3D_Color right_color;
} NDO3D_Edge;

typedef struct NDO3D_FaceRec_ {
    ndo3d_u16 edge;
} NDO3D_Face;

typedef struct NDO3D_ObjectRec_ {
    char name[NDO3D_MAX_NAME];
    unsigned int name_len;
    int perm;
    ndo3d_u8 visible;
    ndo3d_u8 sensitive;
    ndo3d_u8 shaded;
    ndo3d_u8 enable_colors;
    unsigned int edge_count;
    unsigned int face_count;
    unsigned int vertex_count;
    NDO3D_Edge edges[NDO3D_MAX_EDGES];
    NDO3D_Face faces[NDO3D_MAX_FACES];
    NDO3D_Vertex vertices[NDO3D_MAX_VERTICES];
} NDO3D_Object;

typedef struct NDO3D_ModelRec_ {
    unsigned int object_count;
    int source_version; /* 10 or 11 when recognized. */
    NDO3D_Object objects[NDO3D_MAX_OBJECTS];
} NDO3D_Model;

typedef struct NDO3D_DiagRec_ {
    int code;
    unsigned long offset;
    char message[160];
} NDO3D_Diag;

typedef struct NDO3D_CursorRec_ {
    const ndo3d_u8 *data;
    unsigned long size;
    unsigned long pos;
} NDO3D_Cursor;

void ndo3d_clear_model(NDO3D_Model *model);
void ndo3d_clear_diag(NDO3D_Diag *diag);
const char *ndo3d_strerror(int code);

ndo3d_fixed ndo3d_fixed_from_int(long v);
long ndo3d_fixed_to_int(ndo3d_fixed v);
ndo3d_fixed ndo3d_fixed_div_int(ndo3d_fixed v, long d);
ndo3d_fixed ndo3d_fixed_mul_int_sat(ndo3d_fixed v, long m);
int ndo3d_fixed_to_ascii(ndo3d_fixed v, char *out, unsigned int out_cap, unsigned int frac_digits);

int ndo3d_decode(NDO3D_Model *model, const void *data, unsigned long size, NDO3D_Diag *diag);
int ndo3d_encode(const NDO3D_Model *model, void *out_data, unsigned long out_cap, unsigned long *out_size, NDO3D_Diag *diag);

int ndo3d_face_vertices(const NDO3D_Object *obj, unsigned int face_index,
                        ndo3d_u16 *out_indices, unsigned int out_cap,
                        unsigned int *out_count, NDO3D_Diag *diag);

int ndo3d_write_obj_text(const NDO3D_Model *model, char *out_text,
                         unsigned long out_cap, unsigned long *out_size,
                         NDO3D_Diag *diag);

#ifdef __cplusplus
}
#endif

#endif
