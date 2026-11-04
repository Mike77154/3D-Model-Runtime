#ifndef MF3D_H
#define MF3D_H

/* mf3d - tiny 3MF core decoder, C89, no malloc/free/realloc/heap.
   Public domain / CC0-style sample module. */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MF3D_MAX_VERTICES
#define MF3D_MAX_VERTICES 8192
#endif
#ifndef MF3D_MAX_TRIANGLES
#define MF3D_MAX_TRIANGLES 16384
#endif
#ifndef MF3D_MAX_OBJECTS
#define MF3D_MAX_OBJECTS 128
#endif
#ifndef MF3D_MAX_BUILDITEMS
#define MF3D_MAX_BUILDITEMS 256
#endif
#ifndef MF3D_MAX_ZIP_ENTRIES
#define MF3D_MAX_ZIP_ENTRIES 256
#endif
#ifndef MF3D_XML_BUFFER_MAX
#define MF3D_XML_BUFFER_MAX 1048576L
#endif

#define MF3D_OK 0
#define MF3D_E_BADARG -1
#define MF3D_E_ZIP -2
#define MF3D_E_NOTFOUND -3
#define MF3D_E_NOSPACE -4
#define MF3D_E_UNSUPPORTED -5
#define MF3D_E_XML -6
#define MF3D_E_INFLATE -7
#define MF3D_E_RANGE -8

typedef signed long mf3d_i32;
typedef unsigned long mf3d_u32;
typedef signed short mf3d_i16;
typedef unsigned short mf3d_u16;
typedef unsigned char mf3d_u8;

typedef mf3d_i32 mf3d_fix; /* 16.16 fixed point */

typedef struct mf3d_vec3_s {
    mf3d_fix x, y, z;
} mf3d_vec3;

typedef struct mf3d_tri_s {
    mf3d_u32 v1, v2, v3;
    mf3d_u32 pid, p1, p2, p3;
} mf3d_tri;

typedef struct mf3d_object_s {
    mf3d_u32 id;
    mf3d_u32 vertex_first, vertex_count;
    mf3d_u32 tri_first, tri_count;
    char type[24];
    char name[64];
} mf3d_object;

typedef struct mf3d_builditem_s {
    mf3d_u32 objectid;
    mf3d_fix transform[12]; /* row-major 3x4; identity if absent */
} mf3d_builditem;

typedef struct mf3d_model_s {
    mf3d_vec3 vertices[MF3D_MAX_VERTICES];
    mf3d_tri triangles[MF3D_MAX_TRIANGLES];
    mf3d_object objects[MF3D_MAX_OBJECTS];
    mf3d_builditem builditems[MF3D_MAX_BUILDITEMS];
    mf3d_u32 vertex_count, triangle_count, object_count, builditem_count;
    char unit[24];
    char lang[32];
} mf3d_model;

typedef int (*mf3d_inflate_fn)(void *user, const mf3d_u8 *src, mf3d_u32 src_len,
                              mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len);

typedef struct mf3d_options_s {
    mf3d_inflate_fn inflate_cb; /* optional raw-deflate decompressor; must not allocate */
    void *inflate_user;
    mf3d_u8 *scratch;
    mf3d_u32 scratch_size;
} mf3d_options;

void mf3d_model_init(mf3d_model *m);
int mf3d_decode(const mf3d_u8 *data, mf3d_u32 size, mf3d_model *out_model, mf3d_options *opt);
const char *mf3d_error_string(int err);

/* utilities */
mf3d_fix mf3d_fix_from_int(mf3d_i32 v);
mf3d_i32 mf3d_fix_to_int(mf3d_fix v);

#ifdef __cplusplus
}
#endif
#endif
