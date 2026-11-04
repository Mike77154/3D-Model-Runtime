#ifndef FBX_SEMANTIC_WRITER_H
#define FBX_SEMANTIC_WRITER_H

#include "fbx_importer.h"
#include "fbx_scene.h"
#include "fbx_codec_writer.h"
#include "fbx_binary_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FbxSemanticMeshTag {
    const char *name;
    const fbx_real *positions; /* xyz, size vertex_count */
    const fbx_real *normals;   /* optional xyz, size vertex_count */
    const fbx_real *uvs;       /* optional uv, size vertex_count */
    const fbx_real *colors;    /* optional rgba, size vertex_count */
    fbx_u32 vertex_count;
    const fbx_u32 *indices;    /* triangles, size index_count */
    fbx_u32 index_count;
    const char *material_name;
    const char *diffuse_texture;
} FbxSemanticMesh;

typedef struct FbxSemanticCameraTag {
    const char *name;
    fbx_real translation[3];
    fbx_real rotation_euler[3];
    fbx_real field_of_view;
    fbx_real near_plane;
    fbx_real far_plane;
    int projection_type;
} FbxSemanticCamera;

typedef struct FbxSemanticLightTag {
    const char *name;
    fbx_real translation[3];
    fbx_real rotation_euler[3];
    int light_type;
    fbx_real color[3];
    fbx_real intensity;
    fbx_real cone_angle;
} FbxSemanticLight;

typedef struct FbxSemanticBoneTag {
    const char *name;
    int parent_index;
    fbx_real translation[3];
    fbx_real rotation_euler[3];
    fbx_real scale[3];
} FbxSemanticBone;

typedef struct FbxSemanticSkinClusterTag {
    int mesh_index;
    int bone_index;
    const fbx_u32 *indices;    /* control point indices */
    const fbx_real *weights;   /* same count as indices */
    fbx_u32 count;
} FbxSemanticSkinCluster;

typedef struct FbxSemanticAnimKeyTag {
    fbx_i64 time_ticks;        /* FBX KTime ticks */
    fbx_real value;
    unsigned char interpolation;
} FbxSemanticAnimKey;

typedef struct FbxSemanticAnimCurveTag {
    int target_kind;           /* 0=model TRS, 1=morph channel */
    int target_index;          /* model/bone/mesh index depending on target_kind */
    int channel;               /* FbxAnimChannelType or morph index */
    const char *property_name; /* optional FBX property override */
    const FbxSemanticAnimKey *keys;
    int key_count;
} FbxSemanticAnimCurve;

typedef struct FbxSemanticAnimStackTag {
    const char *name;
    const FbxSemanticAnimCurve *curves;
    int curve_count;
} FbxSemanticAnimStack;

typedef struct FbxSemanticSceneTag {
    const char *creator;
    const FbxSemanticMesh *meshes;
    int mesh_count;
    const FbxSemanticCamera *cameras;
    int camera_count;
    const FbxSemanticLight *lights;
    int light_count;
    const FbxSemanticBone *bones;
    int bone_count;
    const FbxSemanticSkinCluster *skin_clusters;
    int skin_cluster_count;
    const FbxSemanticAnimStack *anim_stacks;
    int anim_stack_count;
} FbxSemanticScene;

typedef struct FbxSemanticWriteOptionsTag {
    int version;
    int include_normals;
    int include_uvs;
    int include_colors;
    int include_materials;
    int binary_compress_arrays;
} FbxSemanticWriteOptions;

void fbx_semantic_write_options_default(FbxSemanticWriteOptions *opt);
FbxNode *fbx_semantic_build_tree(const FbxSemanticScene *scene,
                                 const FbxSemanticWriteOptions *options);
int fbx_semantic_write_ascii(const FbxSemanticScene *scene,
                             const FbxCodecWriteStream *stream,
                             const FbxSemanticWriteOptions *options);
FBX_Result fbx_semantic_write_binary(const FbxSemanticScene *scene,
                                     FbxWriteBuffer *buffer,
                                     const FbxSemanticWriteOptions *options);

#ifdef __cplusplus
}
#endif

#endif /* FBX_SEMANTIC_WRITER_H */
