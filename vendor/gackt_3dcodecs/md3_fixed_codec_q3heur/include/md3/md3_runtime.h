#ifndef MD3_RUNTIME_H
#define MD3_RUNTIME_H

#include "md3_decode.h"
#include "md3_fixed.h"

typedef struct md3_pose_s
{
    md3_i32 old_frame;
    md3_i32 frame;
    md3_fixed_t frac;
} md3_pose_t;

typedef struct md3_transform_s
{
    md3_vec3_t origin;
    md3_vec3_t axis[3];
} md3_transform_t;

void md3_pose_set(md3_pose_t *pose, md3_i32 old_frame, md3_i32 frame,
    md3_fixed_t frac);

void md3_axis_identity(md3_vec3_t axis[3]);
void md3_transform_identity(md3_transform_t *out_transform);
void md3_transform_from_tag(const md3_tag_t *tag,
    md3_transform_t *out_transform);
void md3_transform_point(const md3_transform_t *transform,
    const md3_vec3_t *point, md3_vec3_t *out_point);
void md3_transform_vector(const md3_transform_t *transform,
    const md3_vec3_t *vector, md3_vec3_t *out_vector);
int md3_transform_concat(const md3_transform_t *local_transform,
    const md3_transform_t *parent_transform,
    md3_transform_t *out_transform);
int md3_transform_attach_on_tag(const md3_transform_t *parent_transform,
    const md3_transform_t *tag_transform, md3_transform_t *out_transform);
int md3_transform_attach_rotated_on_tag(
    const md3_transform_t *entity_local_transform,
    const md3_transform_t *parent_transform,
    const md3_transform_t *tag_transform,
    md3_transform_t *out_transform);

int md3_runtime_clamp_pose(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_pose_t *out_pose);
int md3_runtime_find_tag_index(const md3_decoder_t *decoder,
    const char *tag_name, md3_i32 *out_index);

int md3_runtime_sample_frame(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_frame_t *out_frame);

int md3_runtime_sample_tag_index(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_i32 tag_index, md3_tag_t *out_tag);
int md3_runtime_sample_tag_name(const md3_decoder_t *decoder,
    const md3_pose_t *pose, const char *tag_name, md3_tag_t *out_tag);
int md3_runtime_sample_tag_transform_index(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_i32 tag_index,
    md3_transform_t *out_transform);
int md3_runtime_sample_tag_transform_name(const md3_decoder_t *decoder,
    const md3_pose_t *pose, const char *tag_name,
    md3_transform_t *out_transform);

int md3_runtime_sample_surface(const md3_decoder_t *decoder,
    md3_i32 surface_index, const md3_pose_t *pose, md3_vertex_t *out_vertices,
    md3_vertex_t *scratch_vertices, md3_i32 vertex_capacity);
int md3_runtime_sample_surface_by_name(const md3_decoder_t *decoder,
    const char *surface_name, const md3_pose_t *pose,
    md3_vertex_t *out_vertices, md3_vertex_t *scratch_vertices,
    md3_i32 vertex_capacity);
int md3_runtime_sample_surface_world(const md3_decoder_t *decoder,
    md3_i32 surface_index, const md3_pose_t *pose,
    const md3_transform_t *model_transform, md3_vertex_t *out_vertices,
    md3_vertex_t *scratch_vertices, md3_i32 vertex_capacity);
int md3_runtime_sample_surface_world_by_name(const md3_decoder_t *decoder,
    const char *surface_name, const md3_pose_t *pose,
    const md3_transform_t *model_transform, md3_vertex_t *out_vertices,
    md3_vertex_t *scratch_vertices, md3_i32 vertex_capacity);

int md3_runtime_attach_tag_index(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_i32 tag_index,
    const md3_transform_t *parent_transform,
    md3_transform_t *out_transform);
int md3_runtime_attach_tag_name(const md3_decoder_t *decoder,
    const md3_pose_t *pose, const char *tag_name,
    const md3_transform_t *parent_transform,
    md3_transform_t *out_transform);
int md3_runtime_attach_rotated_tag_index(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_i32 tag_index,
    const md3_transform_t *entity_local_transform,
    const md3_transform_t *parent_transform,
    md3_transform_t *out_transform);
int md3_runtime_attach_rotated_tag_name(const md3_decoder_t *decoder,
    const md3_pose_t *pose, const char *tag_name,
    const md3_transform_t *entity_local_transform,
    const md3_transform_t *parent_transform,
    md3_transform_t *out_transform);

#endif
