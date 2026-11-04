#ifndef IQM_LOADER_H
#define IQM_LOADER_H

#include "iqm_fixed.h"

#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION 2u
#define IQM_HEADER_SIZE 124u

#define IQM_POSITION      0u
#define IQM_TEXCOORD      1u
#define IQM_NORMAL        2u
#define IQM_TANGENT       3u
#define IQM_BLENDINDEXES  4u
#define IQM_BLENDWEIGHTS  5u
#define IQM_COLOR         6u
#define IQM_CUSTOM        0x10u

#define IQM_BYTE    0u
#define IQM_UBYTE   1u
#define IQM_SHORT   2u
#define IQM_USHORT  3u
#define IQM_INT     4u
#define IQM_UINT    5u
#define IQM_HALF    6u
#define IQM_FLOAT   7u
#define IQM_DOUBLE  8u

#define IQM_LOOP (1u << 0)

#define IQM_OK 0
#define IQM_ERR_NULL -1
#define IQM_ERR_TOO_SMALL -2
#define IQM_ERR_BAD_MAGIC -3
#define IQM_ERR_BAD_VERSION -4
#define IQM_ERR_BAD_FILESIZE -5
#define IQM_ERR_BOUNDS -6
#define IQM_ERR_ALIGN -7
#define IQM_ERR_LIMIT -8
#define IQM_ERR_UNSUPPORTED_FORMAT -9
#define IQM_ERR_BAD_VERTEXARRAY -10
#define IQM_ERR_BAD_TEXT -11
#define IQM_ERR_BAD_TRIANGLE -12
#define IQM_ERR_BAD_FRAMECHANNELS -13
#define IQM_ERR_BAD_EXTENSION -14
#define IQM_ERR_VERTEXARRAY_ORDER -15

typedef struct iqm_header_s
{
    iqm_u32 version;
    iqm_u32 filesize;
    iqm_u32 flags;
    iqm_u32 num_text;
    iqm_u32 ofs_text;
    iqm_u32 num_meshes;
    iqm_u32 ofs_meshes;
    iqm_u32 num_vertexarrays;
    iqm_u32 num_vertexes;
    iqm_u32 ofs_vertexarrays;
    iqm_u32 num_triangles;
    iqm_u32 ofs_triangles;
    iqm_u32 ofs_adjacency;
    iqm_u32 num_joints;
    iqm_u32 ofs_joints;
    iqm_u32 num_poses;
    iqm_u32 ofs_poses;
    iqm_u32 num_anims;
    iqm_u32 ofs_anims;
    iqm_u32 num_frames;
    iqm_u32 num_framechannels;
    iqm_u32 ofs_frames;
    iqm_u32 ofs_bounds;
    iqm_u32 num_comment;
    iqm_u32 ofs_comment;
    iqm_u32 num_extensions;
    iqm_u32 ofs_extensions;
} iqm_header;

typedef struct iqm_mesh_s
{
    iqm_u32 name;
    iqm_u32 material;
    iqm_u32 first_vertex;
    iqm_u32 num_vertexes;
    iqm_u32 first_triangle;
    iqm_u32 num_triangles;
} iqm_mesh;

typedef struct iqm_vertex_s
{
    iqm_fixed position[3];
    iqm_fixed texcoord[2];
    iqm_fixed normal[3];
    iqm_fixed tangent[4];
    iqm_u8 blendindexes[4];
    iqm_u8 blendweights[4];
    iqm_u8 color[4];
} iqm_vertex;

typedef struct iqm_triangle_s
{
    iqm_u32 vertex[3];
} iqm_triangle;

typedef struct iqm_adjacency_s
{
    iqm_u32 triangle[3];
} iqm_adjacency;

typedef struct iqm_joint_s
{
    iqm_u32 name;
    iqm_i32 parent;
    iqm_fixed translate[3];
    iqm_fixed rotate[4];
    iqm_fixed scale[3];
} iqm_joint;

typedef struct iqm_pose_s
{
    iqm_i32 parent;
    iqm_u32 channelmask;
    iqm_fixed channeloffset[10];
    iqm_fixed channelscale[10];
    iqm_u32 first_channel;
} iqm_pose;

typedef struct iqm_anim_s
{
    iqm_u32 name;
    iqm_u32 first_frame;
    iqm_u32 num_frames;
    iqm_fixed framerate;
    iqm_u32 flags;
} iqm_anim;

typedef struct iqm_bounds_s
{
    iqm_fixed bbmins[3];
    iqm_fixed bbmaxs[3];
    iqm_fixed xyradius;
    iqm_fixed radius;
} iqm_bounds;

typedef struct iqm_custom_vertexarray_s
{
    iqm_u32 type;
    iqm_u32 name;
    iqm_u32 flags;
    iqm_u32 format;
    iqm_u32 size;
    iqm_u32 offset;
} iqm_custom_vertexarray;

typedef struct iqm_extension_s
{
    iqm_u32 name;
    iqm_u32 num_data;
    iqm_u32 ofs_data;
    iqm_u32 ofs_extensions;
} iqm_extension;

typedef struct iqm_model_s
{
    const iqm_u8 *bytes;
    iqm_u32 byte_count;
    iqm_header header;

    iqm_u32 text_offset;
    iqm_u32 text_count;
    iqm_u32 comment_offset;
    iqm_u32 comment_count;

    iqm_u32 num_meshes;
    iqm_u32 num_vertexes;
    iqm_u32 num_triangles;
    iqm_u32 num_joints;
    iqm_u32 num_poses;
    iqm_u32 num_anims;
    iqm_u32 num_frames;
    iqm_u32 num_framechannels;
    iqm_u32 num_framevalues;
    iqm_u32 num_bounds;
    iqm_u32 num_custom_vertexarrays;
    iqm_u32 num_extensions_loaded;
    iqm_u8 has_adjacency;

    iqm_mesh meshes[IQM_MAX_MESHES];
    iqm_vertex vertexes[IQM_MAX_VERTEXES];
    iqm_triangle triangles[IQM_MAX_TRIANGLES];
    iqm_adjacency adjacency[IQM_MAX_TRIANGLES];
    iqm_joint joints[IQM_MAX_JOINTS];
    iqm_pose poses[IQM_MAX_POSES];
    iqm_anim anims[IQM_MAX_ANIMS];
    iqm_bounds bounds[IQM_MAX_BOUNDS];
    iqm_custom_vertexarray custom_vertexarrays[IQM_MAX_CUSTOM_VERTEXARRAYS];
    iqm_extension extensions[IQM_MAX_EXTENSIONS];
    iqm_u16 framevalues[IQM_MAX_FRAMEVALUES];

    iqm_u8 has_position;
    iqm_u8 has_texcoord;
    iqm_u8 has_normal;
    iqm_u8 has_tangent;
    iqm_u8 has_blendindexes;
    iqm_u8 has_blendweights;
    iqm_u8 has_color;
} iqm_model;

#ifdef __cplusplus
extern "C" {
#endif

void iqm_model_clear(iqm_model *model);
int iqm_load_memory(const iqm_u8 *bytes, iqm_u32 byte_count, iqm_model *out_model);
const char *iqm_get_text(const iqm_model *model, iqm_u32 offset);
const char *iqm_error_string(int code);
int iqm_get_extension_data(const iqm_model *model, const char *name, const iqm_u8 **out_data, iqm_u32 *out_size);
int iqm_get_pose_channel(const iqm_model *model, iqm_u32 frame_index, iqm_u32 pose_index, iqm_u32 channel_index, iqm_fixed *out_value);

#ifdef __cplusplus
}
#endif

#endif
