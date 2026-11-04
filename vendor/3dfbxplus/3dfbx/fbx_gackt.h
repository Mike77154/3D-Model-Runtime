#ifndef FBX_GACKT_H
#define FBX_GACKT_H

#include "fbx_types.h"
#include "fbx_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

struct FbxScene;

#define FBX_GACKT_VERSION 1u
#define FBX_GACKT_HEADER_SIZE 64u
#define FBX_GACKT_MESH_RECORD_SIZE 80u

#define FBX_GACKT_FLAG_BAKE_TRANSFORMS 0x00000001u
#define FBX_GACKT_FLAG_WRITE_NORMALS   0x00000002u
#define FBX_GACKT_FLAG_WRITE_UV0       0x00000004u
#define FBX_GACKT_FLAG_WRITE_TANGENTS  0x00000008u
#define FBX_GACKT_FLAG_WRITE_COLORS    0x00000010u
#define FBX_GACKT_FLAG_DEFAULT         0x0000001eu

#define FBX_GACKT_OK              0
#define FBX_GACKT_ERR_ARGUMENT   -1
#define FBX_GACKT_ERR_LIMIT      -2
#define FBX_GACKT_ERR_CAPACITY   -3
#define FBX_GACKT_ERR_FORMAT     -4

typedef struct FbxGacktInfoTag {
    fbx_u32 version;
    fbx_u32 flags;
    fbx_u32 mesh_count;
    fbx_u32 total_vertex_count;
    fbx_u32 total_index_count;
    fbx_u32 file_size;
} FbxGacktInfo;

fbx_u32 fbx_gackt_calc_scene_size(const struct FbxScene *scene, fbx_u32 flags);
int     fbx_gackt_write_scene(const struct FbxScene *scene,
                              void *dst,
                              fbx_u32 capacity,
                              fbx_u32 *out_size,
                              fbx_u32 flags);

int     fbx_gackt_validate_memory(const void *data,
                                  fbx_u32 size,
                                  FbxGacktInfo *out_info);
const char *fbx_gackt_error_string(int code);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_GACKT_H */
