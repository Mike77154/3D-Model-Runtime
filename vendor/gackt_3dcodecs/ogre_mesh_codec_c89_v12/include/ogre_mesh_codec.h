#ifndef OGRE_MESH_CODEC_H
#define OGRE_MESH_CODEC_H

#include "omc_config.h"
#include "omc_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C89-friendly integer aliases. The code assumes char=8, short>=16, long>=32. */
typedef unsigned char OMC_U8;
typedef signed char OMC_S8;
typedef unsigned short OMC_U16;
typedef signed short OMC_S16;
typedef unsigned long OMC_U32;
typedef signed long OMC_S32;

typedef enum OMC_ResultTag {
    OMC_OK = 0,
    OMC_ERR_NULL = -1,
    OMC_ERR_CAPACITY = -2,
    OMC_ERR_FORMAT = -3,
    OMC_ERR_IO = -4,
    OMC_ERR_UNSUPPORTED = -5
} OMC_Result;

typedef enum OMC_OperationTag {
    /* Numeric values mirror Ogre::RenderOperation::OperationType. */
    OMC_OP_UNKNOWN = 0,
    OMC_OP_POINT_LIST = 1,
    OMC_OP_LINE_LIST = 2,
    OMC_OP_LINE_STRIP = 3,
    OMC_OP_TRIANGLE_LIST = 4,
    OMC_OP_TRIANGLE_STRIP = 5,
    OMC_OP_TRIANGLE_FAN = 6,
    OMC_OP_PATCH_1_CONTROL_POINT = 7,
    OMC_OP_PATCH_2_CONTROL_POINT = 8,
    OMC_OP_PATCH_3_CONTROL_POINT = 9,
    OMC_OP_PATCH_4_CONTROL_POINT = 10,
    OMC_OP_PATCH_5_CONTROL_POINT = 11,
    OMC_OP_PATCH_6_CONTROL_POINT = 12,
    OMC_OP_PATCH_7_CONTROL_POINT = 13,
    OMC_OP_PATCH_8_CONTROL_POINT = 14,
    OMC_OP_PATCH_9_CONTROL_POINT = 15,
    OMC_OP_PATCH_10_CONTROL_POINT = 16,
    OMC_OP_PATCH_11_CONTROL_POINT = 17,
    OMC_OP_PATCH_12_CONTROL_POINT = 18,
    OMC_OP_PATCH_13_CONTROL_POINT = 19,
    OMC_OP_PATCH_14_CONTROL_POINT = 20,
    OMC_OP_PATCH_15_CONTROL_POINT = 21,
    OMC_OP_PATCH_16_CONTROL_POINT = 22,
    OMC_OP_PATCH_17_CONTROL_POINT = 23,
    OMC_OP_PATCH_18_CONTROL_POINT = 24,
    OMC_OP_PATCH_19_CONTROL_POINT = 25,
    OMC_OP_PATCH_20_CONTROL_POINT = 26,
    OMC_OP_PATCH_21_CONTROL_POINT = 27,
    OMC_OP_PATCH_22_CONTROL_POINT = 28,
    OMC_OP_PATCH_23_CONTROL_POINT = 29,
    OMC_OP_PATCH_24_CONTROL_POINT = 30,
    OMC_OP_PATCH_25_CONTROL_POINT = 31,
    OMC_OP_PATCH_26_CONTROL_POINT = 32,
    OMC_OP_PATCH_27_CONTROL_POINT = 33,
    OMC_OP_PATCH_28_CONTROL_POINT = 34,
    OMC_OP_PATCH_29_CONTROL_POINT = 35,
    OMC_OP_PATCH_30_CONTROL_POINT = 36,
    OMC_OP_PATCH_31_CONTROL_POINT = 37,
    OMC_OP_PATCH_32_CONTROL_POINT = 38,
    OMC_OP_DETAIL_ADJACENCY_BIT = 64,
    OMC_OP_LINE_LIST_ADJ = OMC_OP_LINE_LIST | OMC_OP_DETAIL_ADJACENCY_BIT,
    OMC_OP_LINE_STRIP_ADJ = OMC_OP_LINE_STRIP | OMC_OP_DETAIL_ADJACENCY_BIT,
    OMC_OP_TRIANGLE_LIST_ADJ = OMC_OP_TRIANGLE_LIST | OMC_OP_DETAIL_ADJACENCY_BIT,
    OMC_OP_TRIANGLE_STRIP_ADJ = OMC_OP_TRIANGLE_STRIP | OMC_OP_DETAIL_ADJACENCY_BIT
} OMC_Operation;

typedef enum OMC_VertexElementSemanticTag {
    OMC_VES_UNKNOWN = 0,
    OMC_VES_POSITION = 1,
    OMC_VES_BLEND_WEIGHTS = 2,
    OMC_VES_BLEND_INDICES = 3,
    OMC_VES_NORMAL = 4,
    OMC_VES_DIFFUSE = 5,
    OMC_VES_SPECULAR = 6,
    OMC_VES_TEXTURE_COORDINATES = 7,
    OMC_VES_BINORMAL = 8,
    OMC_VES_TANGENT = 9
} OMC_VertexElementSemantic;

typedef enum OMC_VertexElementTypeTag {
    /* Numeric values intentionally mirror OGRE's VertexElementType values. */
    OMC_VET_FLOAT1 = 0,
    OMC_VET_FLOAT2 = 1,
    OMC_VET_FLOAT3 = 2,
    OMC_VET_FLOAT4 = 3,
    OMC_VET_COLOUR = 4,          /* OGRE 1.x deprecated renderer-native colour */
    OMC_VET_SHORT1 = 5,
    OMC_VET_SHORT2 = 6,
    OMC_VET_SHORT3 = 7,
    OMC_VET_SHORT4 = 8,
    OMC_VET_UBYTE4 = 9,
    OMC_VET_COLOUR_ARGB = 10,    /* OGRE 1.x D3D packed colour */
    OMC_VET_COLOUR_ABGR = 11,    /* OGRE 1.x GL packed colour */
    OMC_VET_DOUBLE1 = 12,
    OMC_VET_DOUBLE2 = 13,
    OMC_VET_DOUBLE3 = 14,
    OMC_VET_DOUBLE4 = 15,
    OMC_VET_USHORT1 = 16,
    OMC_VET_USHORT2 = 17,
    OMC_VET_USHORT3 = 18,
    OMC_VET_USHORT4 = 19,
    OMC_VET_INT1 = 20,
    OMC_VET_INT2 = 21,
    OMC_VET_INT3 = 22,
    OMC_VET_INT4 = 23,
    OMC_VET_UINT1 = 24,
    OMC_VET_UINT2 = 25,
    OMC_VET_UINT3 = 26,
    OMC_VET_UINT4 = 27,
    OMC_VET_BYTE4 = 28,
    OMC_VET_BYTE4_NORM = 29,
    OMC_VET_UBYTE4_NORM = 30,
    OMC_VET_SHORT2_NORM = 31,
    OMC_VET_SHORT4_NORM = 32,
    OMC_VET_USHORT2_NORM = 33,
    OMC_VET_USHORT4_NORM = 34,
    OMC_VET_INT_10_10_10_2_NORM = 35,
    OMC_VET_HALF1 = 36,
    OMC_VET_HALF2 = 37,
    OMC_VET_HALF3 = 38,
    OMC_VET_HALF4 = 39
} OMC_VertexElementType;

typedef struct OMC_Vec2Tag {
    OMC_Fixed u;
    OMC_Fixed v;
} OMC_Vec2;

typedef struct OMC_Vec3Tag {
    OMC_Fixed x;
    OMC_Fixed y;
    OMC_Fixed z;
} OMC_Vec3;

typedef struct OMC_Vec4Tag {
    OMC_Fixed x;
    OMC_Fixed y;
    OMC_Fixed z;
    OMC_Fixed w;
} OMC_Vec4;

typedef struct OMC_VertexTag {
    OMC_Vec3 position;
    OMC_Vec3 normal;
    OMC_Vec4 tangent;
    OMC_Vec3 binormal;
    OMC_Fixed blend_weights[4];
    OMC_U16 blend_indices[4];
    OMC_U8 blend_count;
    OMC_Vec2 texcoord[OMC_TEXCOORD_SETS];
    OMC_Fixed texcoord_w[OMC_TEXCOORD_SETS];
    OMC_Fixed texcoord_x[OMC_TEXCOORD_SETS];
    OMC_U8 texcoord_dims[OMC_TEXCOORD_SETS];
    OMC_U32 diffuse_rgba;
    OMC_U32 specular_rgba;
    OMC_U16 flags;
    OMC_U8 texcoord_count;
} OMC_Vertex;

#define OMC_VTX_HAS_POSITION 0x0001u
#define OMC_VTX_HAS_NORMAL   0x0002u
#define OMC_VTX_HAS_TANGENT  0x0004u
#define OMC_VTX_HAS_BINORMAL 0x0008u
#define OMC_VTX_HAS_DIFFUSE  0x0010u
#define OMC_VTX_HAS_SPECULAR      0x0020u
#define OMC_VTX_HAS_TEXCOORD      0x0040u
#define OMC_VTX_HAS_BLEND_WEIGHTS 0x0080u
#define OMC_VTX_HAS_BLEND_INDICES 0x0100u

typedef struct OMC_SubmeshTag {
    char material[OMC_MAX_NAME];
    char name[OMC_MAX_NAME];
    OMC_U32 vertex_start;
    OMC_U32 vertex_count;
    OMC_U32 index_start;
    OMC_U32 index_count;
    OMC_U8 use_shared_vertices;
    OMC_U8 use_32bit_indices;
    OMC_U8 operation_type;
    OMC_U8 has_dedicated_geometry;
} OMC_Submesh;

typedef struct OMC_TextureAliasTag {
    OMC_U16 submesh_index;
    char alias[OMC_MAX_NAME];
    char texture[OMC_MAX_NAME];
} OMC_TextureAlias;

typedef struct OMC_BoneAssignmentTag {
    OMC_U32 vertex_index;
    OMC_U16 bone_index;
    OMC_Fixed weight;
} OMC_BoneAssignment;

typedef struct OMC_SubmeshBoneAssignmentTag {
    OMC_U16 submesh_index;
    OMC_BoneAssignment assignment;
} OMC_SubmeshBoneAssignment;

typedef struct OMC_PoseOffsetTag {
    OMC_U16 pose_index;
    OMC_U32 vertex_index;
    OMC_Vec3 offset;
    OMC_Vec3 normal;
    OMC_U8 has_normal;
} OMC_PoseOffset;

typedef struct OMC_PoseTag {
    char name[OMC_MAX_NAME];
    OMC_U8 target_is_submesh;
    OMC_U16 target_index;
    OMC_U32 offset_start;
    OMC_U32 offset_count;
} OMC_Pose;

typedef struct OMC_AnimationTag {
    char name[OMC_MAX_NAME];
    OMC_Fixed length;
    char base_animation_name[OMC_MAX_NAME];
    OMC_Fixed base_keyframe_time;
    OMC_U8 has_baseinfo;
    OMC_U32 track_start;
    OMC_U32 track_count;
} OMC_Animation;

typedef struct OMC_AnimationTrackTag {
    OMC_U16 animation_index;
    OMC_U8 target_is_submesh;
    OMC_U16 target_index;
    OMC_U8 is_pose_track;
    OMC_U32 key_start;
    OMC_U32 key_count;
    OMC_U32 morph_key_start;
    OMC_U32 morph_key_count;
} OMC_AnimationTrack;

typedef struct OMC_AnimationKeyTag {
    OMC_U16 track_index;
    OMC_Fixed time;
    OMC_U16 pose_index;
    OMC_Fixed influence;
} OMC_AnimationKey;


typedef struct OMC_MorphKeyTag {
    OMC_U16 track_index;
    OMC_Fixed time;
    OMC_U32 vertex_start;
    OMC_U32 vertex_count;
    OMC_U8 includes_normals;
} OMC_MorphKey;

typedef struct OMC_MorphVertexTag {
    OMC_U16 morph_key_index;
    OMC_U32 local_index;
    OMC_Vec3 position;
    OMC_Vec3 normal;
    OMC_U8 has_normal;
} OMC_MorphVertex;

typedef struct OMC_BinaryChunkInfoTag {
    OMC_U16 id;
    OMC_U32 start;
    OMC_U32 size;
    OMC_U8 depth;
} OMC_BinaryChunkInfo;

typedef struct OMC_EdgeTag {
    OMC_U16 submesh_index;
    OMC_U32 v1;
    OMC_U32 v2;
    OMC_U32 face_a;
    OMC_U32 face_b;
    OMC_U8 boundary;
} OMC_Edge;

typedef struct OMC_LodLevelTag {
    OMC_U8 manual;
    OMC_Fixed value;
    char mesh_name[OMC_MAX_NAME];
    OMC_U32 face_start;
    OMC_U32 face_count;
} OMC_LodLevel;

typedef struct OMC_LodFaceTag {
    OMC_U16 lod_index;
    OMC_U16 submesh_index;
    OMC_U32 v1;
    OMC_U32 v2;
    OMC_U32 v3;
} OMC_LodFace;

typedef struct OMC_ExtremeTag {
    OMC_U16 submesh_index;
    OMC_Vec3 position;
} OMC_Extreme;

typedef struct OMC_DiagnosticTag {
    int code;
    char message[OMC_MAX_NAME];
} OMC_Diagnostic;

typedef struct OMC_MeshTag {
    char source_format[32];
    char binary_version[48];
    char skeleton_name[OMC_MAX_NAME];

    OMC_Vertex vertices[OMC_MAX_VERTICES];
    OMC_U32 vertex_count;
    OMC_U32 shared_vertex_count;

    OMC_U32 indices[OMC_MAX_INDICES];
    OMC_U32 index_count;

    OMC_Submesh submeshes[OMC_MAX_SUBMESHES];
    OMC_U16 submesh_count;

    OMC_TextureAlias texture_aliases[OMC_MAX_TEXTURE_ALIASES];
    OMC_U32 texture_alias_count;

    OMC_BoneAssignment bone_assignments[OMC_MAX_BONE_ASSIGNMENTS];
    OMC_U32 bone_assignment_count;

    OMC_SubmeshBoneAssignment submesh_bone_assignments[OMC_MAX_SUBMESH_BONE_ASSIGNMENTS];
    OMC_U32 submesh_bone_assignment_count;

    OMC_Pose poses[OMC_MAX_POSES];
    OMC_U16 pose_count;
    OMC_PoseOffset pose_offsets[OMC_MAX_POSE_OFFSETS];
    OMC_U32 pose_offset_count;

    OMC_Animation animations[OMC_MAX_ANIMATIONS];
    OMC_U16 animation_count;
    OMC_AnimationTrack tracks[OMC_MAX_ANIMATION_TRACKS];
    OMC_U32 track_count;
    OMC_AnimationKey keys[OMC_MAX_ANIMATION_KEYS];
    OMC_U32 key_count;

    OMC_MorphKey morph_keys[OMC_MAX_MORPH_KEYS];
    OMC_U32 morph_key_count;
    OMC_MorphVertex morph_vertices[OMC_MAX_MORPH_VERTICES];
    OMC_U32 morph_vertex_count;

    OMC_BinaryChunkInfo binary_chunks[OMC_MAX_BINARY_CHUNKS];
    OMC_U16 binary_chunk_count;

    OMC_Edge edges[OMC_MAX_EDGES];
    OMC_U32 edge_count;

    char lod_strategy[OMC_MAX_NAME];
    OMC_U8 lod_manual;
    OMC_U16 lod_declared_levels;
    OMC_LodLevel lod_levels[OMC_MAX_LOD_LEVELS];
    OMC_U16 lod_level_count;
    OMC_LodFace lod_faces[OMC_MAX_LOD_FACES];
    OMC_U32 lod_face_count;

    OMC_Extreme extremes[OMC_MAX_EXTREMES];
    OMC_U32 extreme_count;

    OMC_Vec3 bounds_min;
    OMC_Vec3 bounds_max;
    OMC_Vec3 bounds_center;
    OMC_Fixed bounds_radius;
    OMC_U8 has_bounds;

    OMC_Diagnostic diagnostics[OMC_DIAG_MAX];
    OMC_U16 diagnostic_count;
} OMC_Mesh;

/* main API */
void omc_mesh_init(OMC_Mesh *mesh);
int omc_load_mesh_xml_from_memory(OMC_Mesh *mesh, const char *xml, unsigned long size);
int omc_load_mesh_binary_from_memory(OMC_Mesh *mesh, const unsigned char *data, unsigned long size);
int omc_save_mesh_xml_file(const OMC_Mesh *mesh, const char *path);
int omc_save_mesh_binary_file(const OMC_Mesh *mesh, const char *path);
int omc_export_obj_file(const OMC_Mesh *mesh, const char *path);
int omc_validate_mesh(const OMC_Mesh *mesh);
void omc_compute_bounds(OMC_Mesh *mesh);
int omc_merge_texcoord_sets(OMC_Mesh *mesh, OMC_U8 dst_set, OMC_U8 src_set);
int omc_generate_extremes(OMC_Mesh *mesh, OMC_U16 max_per_submesh);
int omc_recalculate_normals(OMC_Mesh *mesh);
int omc_generate_tangents(OMC_Mesh *mesh, OMC_U8 texcoord_set, OMC_U8 tangent_dimensions);
int omc_build_edge_list(OMC_Mesh *mesh);
unsigned long omc_count_triangles(const OMC_Mesh *mesh);
int omc_triangulate_primitives(OMC_Mesh *mesh);
int omc_remove_degenerate_faces(OMC_Mesh *mesh);
int omc_reverse_winding(OMC_Mesh *mesh);
int omc_flip_normals(OMC_Mesh *mesh);
int omc_compact_vertices(OMC_Mesh *mesh);
int omc_weld_vertices_exact(OMC_Mesh *mesh);
int omc_normalize_blend_weights(OMC_Mesh *mesh);
int omc_build_blend_elements_from_bone_assignments(OMC_Mesh *mesh, OMC_U8 max_influences);
int omc_apply_pose_to_vertices(const OMC_Mesh *mesh, OMC_U16 pose_index, OMC_Fixed influence, OMC_Vertex *vertices, unsigned long vertex_cap);
int omc_make_pose_animation_frame(const OMC_Mesh *mesh, const char *animation_name, OMC_Fixed time, OMC_Vertex *out_vertices, unsigned long vertex_cap, unsigned long *out_count);
int omc_make_animation_frame(const OMC_Mesh *mesh, const char *animation_name, OMC_Fixed time, OMC_Vertex *out_vertices, unsigned long vertex_cap, unsigned long *out_count);
const char *omc_result_string(int code);

/* optional file helpers. They do not allocate; caller supplies buffers. */
int omc_read_entire_file(const char *path, unsigned char *buffer, unsigned long cap, unsigned long *out_size);
int omc_load_file_auto(OMC_Mesh *mesh, const char *path, unsigned char *buffer, unsigned long cap);

#ifdef __cplusplus
}
#endif

#endif
