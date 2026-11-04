#ifndef FBX_LEGACY6000_ADAPTER_H
#define FBX_LEGACY6000_ADAPTER_H

#include "fbx_fixed.h"

/*
    FBX 6000 Legacy Adapter
    -----------------------
    Special path for old binary FBX 6000 files that store mesh data in the
    classic node layout:

        Objects
          Model: Mesh
            Vertices
            PolygonVertexIndex
            LayerElementUV
              UV
              UVIndex

    This is intentionally separate from third_party/3dfbx so the engine can
    keep the regular 3dfbx importer for modern FBX and call this adapter only
    when version <= 6000 or when the modern path cannot extract geometry.

    C89-oriented:
      - caller-owned output buffers
      - no heap_alloc/release/heap_resize
      - no stdio dependency in adapter
      - optional zlib hook is stubbed by default; uncompressed arrays work
*/

#ifdef __cplusplus
extern "C" {
#endif

#define GACKT_FBX6_OK                 0
#define GACKT_FBX6_ERR_ARG          -1
#define GACKT_FBX6_ERR_NOT_FBX      -2
#define GACKT_FBX6_ERR_UNSUPPORTED  -3
#define GACKT_FBX6_ERR_TRUNCATED    -4
#define GACKT_FBX6_ERR_LIMIT        -5
#define GACKT_FBX6_ERR_NO_MESH      -6
#define GACKT_FBX6_ERR_COMPRESSED   -7

#define GACKT_FBX6_MAX_NAME          64
#define GACKT_FBX6_MAX_TEXTURE_PATH 260

typedef struct gackt_fbx6_vec3_s {
    fbx_real x, y, z;
} gackt_fbx6_vec3;

typedef struct gackt_fbx6_uv_s {
    fbx_real u, v;
} gackt_fbx6_uv;

typedef struct gackt_fbx6_tri_s {
    unsigned int v[3];
    unsigned int uv[3];
} gackt_fbx6_tri;

typedef struct gackt_fbx6_limits_s {
    unsigned int max_vertices;
    unsigned int max_uvs;
    unsigned int max_tris;
} gackt_fbx6_limits;

typedef struct gackt_fbx6_mesh_s {
    char name[GACKT_FBX6_MAX_NAME];

    gackt_fbx6_vec3 *vertices;
    gackt_fbx6_uv   *uvs;
    gackt_fbx6_tri  *tris;

    unsigned int vertex_count;
    unsigned int uv_count;
    unsigned int tri_count;

    char texture_path[GACKT_FBX6_MAX_TEXTURE_PATH];

    int fbx_version;
    int uv_flip_v_recommended;
    int axis_x_right_y_up_neg_z_forward;
} gackt_fbx6_mesh;

int gackt_fbx6_probe(const unsigned char *data, unsigned long size, int *out_version);

int gackt_fbx6_load_mesh(
    const unsigned char *data,
    unsigned long size,
    gackt_fbx6_mesh *out_mesh,
    const gackt_fbx6_limits *limits
);

const char *gackt_fbx6_error_string(int code);

#ifdef __cplusplus
}
#endif

#endif
