#include "md3_internal.h"

static md3_fixed_t md3_runtime_clamp_frac(md3_fixed_t value)
{
    if (value < 0)
    {
        return 0;
    }
    if (value > MD3_FIXED_ONE)
    {
        return MD3_FIXED_ONE;
    }
    return value;
}

static md3_i32 md3_runtime_clamp_frame_index(const md3_decoder_t *decoder,
    md3_i32 frame_index)
{
    if (frame_index < 0)
    {
        return 0;
    }
    if (frame_index >= decoder->header.num_frames)
    {
        return decoder->header.num_frames - 1;
    }
    return frame_index;
}

static void md3_axis_multiply(const md3_vec3_t in1[3],
    const md3_vec3_t in2[3], md3_vec3_t out[3])
{
    md3_vec3_t temp[3];

    temp[0].x = md3_fixed_mul(in1[0].x, in2[0].x) +
        md3_fixed_mul(in1[0].y, in2[1].x) +
        md3_fixed_mul(in1[0].z, in2[2].x);
    temp[0].y = md3_fixed_mul(in1[0].x, in2[0].y) +
        md3_fixed_mul(in1[0].y, in2[1].y) +
        md3_fixed_mul(in1[0].z, in2[2].y);
    temp[0].z = md3_fixed_mul(in1[0].x, in2[0].z) +
        md3_fixed_mul(in1[0].y, in2[1].z) +
        md3_fixed_mul(in1[0].z, in2[2].z);

    temp[1].x = md3_fixed_mul(in1[1].x, in2[0].x) +
        md3_fixed_mul(in1[1].y, in2[1].x) +
        md3_fixed_mul(in1[1].z, in2[2].x);
    temp[1].y = md3_fixed_mul(in1[1].x, in2[0].y) +
        md3_fixed_mul(in1[1].y, in2[1].y) +
        md3_fixed_mul(in1[1].z, in2[2].y);
    temp[1].z = md3_fixed_mul(in1[1].x, in2[0].z) +
        md3_fixed_mul(in1[1].y, in2[1].z) +
        md3_fixed_mul(in1[1].z, in2[2].z);

    temp[2].x = md3_fixed_mul(in1[2].x, in2[0].x) +
        md3_fixed_mul(in1[2].y, in2[1].x) +
        md3_fixed_mul(in1[2].z, in2[2].x);
    temp[2].y = md3_fixed_mul(in1[2].x, in2[0].y) +
        md3_fixed_mul(in1[2].y, in2[1].y) +
        md3_fixed_mul(in1[2].z, in2[2].y);
    temp[2].z = md3_fixed_mul(in1[2].x, in2[0].z) +
        md3_fixed_mul(in1[2].y, in2[1].z) +
        md3_fixed_mul(in1[2].z, in2[2].z);

    out[0] = temp[0];
    out[1] = temp[1];
    out[2] = temp[2];
}

static void md3_runtime_lerp_frame(const md3_frame_t *a, const md3_frame_t *b,
    md3_fixed_t t, md3_frame_t *out_frame)
{
    md3_zero_memory(out_frame, (unsigned int) sizeof(*out_frame));
    md3_vec3_lerp(&a->bounds_min, &b->bounds_min, t, &out_frame->bounds_min);
    md3_vec3_lerp(&a->bounds_max, &b->bounds_max, t, &out_frame->bounds_max);
    md3_vec3_lerp(&a->local_origin, &b->local_origin, t,
        &out_frame->local_origin);
    out_frame->radius = md3_fixed_lerp(a->radius, b->radius, t);
    md3_copy_cstr_to_field(out_frame->name, 16U, a->name);
    out_frame->has_raw_f32 = 0;
}

static void md3_runtime_lerp_tag(const md3_tag_t *a, const md3_tag_t *b,
    md3_fixed_t t, md3_tag_t *out_tag)
{
    int i;
    int rc;

    md3_zero_memory(out_tag, (unsigned int) sizeof(*out_tag));
    md3_copy_cstr_to_field(out_tag->name, MD3_MAX_QPATH, a->name);
    md3_vec3_lerp(&a->origin, &b->origin, t, &out_tag->origin);

    for (i = 0; i < 3; ++i)
    {
        md3_vec3_lerp(&a->axis[i], &b->axis[i], t, &out_tag->axis[i]);
        rc = md3_vec3_normalize(&out_tag->axis[i], &out_tag->axis[i]);
        if (rc != MD3_OK)
        {
            if (t >= MD3_FIXED_HALF)
            {
                out_tag->axis[i] = b->axis[i];
            }
            else
            {
                out_tag->axis[i] = a->axis[i];
            }
        }
    }

    out_tag->has_raw_f32 = 0;
}

static int md3_runtime_read_tag_by_index_for_frame(const md3_decoder_t *decoder,
    md3_i32 frame_index, md3_i32 tag_index, md3_tag_t *out_tag)
{
    md3_tag_t tags[MD3_MAX_TAGS];
    int rc;

    if (decoder == 0 || out_tag == 0)
    {
        return MD3_EINVAL;
    }

    if (tag_index < 0 || tag_index >= decoder->header.num_tags)
    {
        return MD3_ERANGE;
    }

    rc = md3_decoder_read_tags_for_frame(decoder, frame_index, tags,
        MD3_MAX_TAGS);
    if (rc != MD3_OK)
    {
        return rc;
    }

    *out_tag = tags[tag_index];
    return MD3_OK;
}

static int md3_runtime_read_pose_vertices(const md3_decoder_t *decoder,
    md3_i32 surface_index, md3_i32 frame_index, md3_vertex_t *out_vertices,
    md3_i32 vertex_capacity)
{
    return md3_decoder_read_surface_vertices_for_frame(decoder, surface_index,
        frame_index, out_vertices, vertex_capacity);
}

void md3_pose_set(md3_pose_t *pose, md3_i32 old_frame, md3_i32 frame,
    md3_fixed_t frac)
{
    if (pose == 0)
    {
        return;
    }

    pose->old_frame = old_frame;
    pose->frame = frame;
    pose->frac = frac;
}

void md3_axis_identity(md3_vec3_t axis[3])
{
    if (axis == 0)
    {
        return;
    }

    md3_vec3_clear(&axis[0]);
    md3_vec3_clear(&axis[1]);
    md3_vec3_clear(&axis[2]);
    axis[0].x = MD3_FIXED_ONE;
    axis[1].y = MD3_FIXED_ONE;
    axis[2].z = MD3_FIXED_ONE;
}

void md3_transform_identity(md3_transform_t *out_transform)
{
    if (out_transform == 0)
    {
        return;
    }

    md3_vec3_clear(&out_transform->origin);
    md3_axis_identity(out_transform->axis);
}

void md3_transform_from_tag(const md3_tag_t *tag,
    md3_transform_t *out_transform)
{
    int i;

    if (tag == 0 || out_transform == 0)
    {
        return;
    }

    out_transform->origin = tag->origin;
    for (i = 0; i < 3; ++i)
    {
        out_transform->axis[i] = tag->axis[i];
    }
}

void md3_transform_vector(const md3_transform_t *transform,
    const md3_vec3_t *vector, md3_vec3_t *out_vector)
{
    md3_vec3_t temp;

    if (transform == 0 || vector == 0 || out_vector == 0)
    {
        return;
    }

    temp.x = md3_fixed_mul(transform->axis[0].x, vector->x) +
        md3_fixed_mul(transform->axis[1].x, vector->y) +
        md3_fixed_mul(transform->axis[2].x, vector->z);
    temp.y = md3_fixed_mul(transform->axis[0].y, vector->x) +
        md3_fixed_mul(transform->axis[1].y, vector->y) +
        md3_fixed_mul(transform->axis[2].y, vector->z);
    temp.z = md3_fixed_mul(transform->axis[0].z, vector->x) +
        md3_fixed_mul(transform->axis[1].z, vector->y) +
        md3_fixed_mul(transform->axis[2].z, vector->z);

    *out_vector = temp;
}

void md3_transform_point(const md3_transform_t *transform,
    const md3_vec3_t *point, md3_vec3_t *out_point)
{
    md3_vec3_t temp;

    if (transform == 0 || point == 0 || out_point == 0)
    {
        return;
    }

    md3_transform_vector(transform, point, &temp);
    md3_vec3_add(&transform->origin, &temp, out_point);
}

int md3_transform_concat(const md3_transform_t *local_transform,
    const md3_transform_t *parent_transform,
    md3_transform_t *out_transform)
{
    md3_transform_t temp;

    if (local_transform == 0 || parent_transform == 0 || out_transform == 0)
    {
        return MD3_EINVAL;
    }

    md3_transform_vector(parent_transform, &local_transform->origin,
        &temp.origin);
    md3_vec3_add(&parent_transform->origin, &temp.origin, &temp.origin);
    md3_axis_multiply(local_transform->axis, parent_transform->axis,
        temp.axis);
    *out_transform = temp;
    return MD3_OK;
}

int md3_transform_attach_on_tag(const md3_transform_t *parent_transform,
    const md3_transform_t *tag_transform, md3_transform_t *out_transform)
{
    return md3_transform_concat(tag_transform, parent_transform, out_transform);
}

int md3_transform_attach_rotated_on_tag(
    const md3_transform_t *entity_local_transform,
    const md3_transform_t *parent_transform,
    const md3_transform_t *tag_transform,
    md3_transform_t *out_transform)
{
    md3_transform_t temp;
    int rc;

    if (entity_local_transform == 0 || parent_transform == 0 ||
        tag_transform == 0 || out_transform == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_transform_concat(entity_local_transform, tag_transform, &temp);
    if (rc != MD3_OK)
    {
        return rc;
    }

    return md3_transform_concat(&temp, parent_transform, out_transform);
}

int md3_runtime_clamp_pose(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_pose_t *out_pose)
{
    if (decoder == 0 || pose == 0 || out_pose == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    out_pose->old_frame = md3_runtime_clamp_frame_index(decoder,
        pose->old_frame);
    out_pose->frame = md3_runtime_clamp_frame_index(decoder, pose->frame);
    out_pose->frac = md3_runtime_clamp_frac(pose->frac);
    return MD3_OK;
}

int md3_runtime_find_tag_index(const md3_decoder_t *decoder,
    const char *tag_name, md3_i32 *out_index)
{
    md3_tag_t tags[MD3_MAX_TAGS];
    md3_i32 i;
    int rc;

    if (decoder == 0 || tag_name == 0 || out_index == 0 ||
        decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    if (decoder->header.num_tags <= 0)
    {
        return MD3_ERANGE;
    }

    rc = md3_decoder_read_tags_for_frame(decoder, 0, tags, MD3_MAX_TAGS);
    if (rc != MD3_OK)
    {
        return rc;
    }

    for (i = 0; i < decoder->header.num_tags; ++i)
    {
        if (strcmp(tags[i].name, tag_name) == 0)
        {
            *out_index = i;
            return MD3_OK;
        }
    }

    return MD3_EFORMAT;
}

int md3_runtime_sample_frame(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_frame_t *out_frame)
{
    md3_pose_t clamped_pose;
    md3_frame_t start_frame;
    md3_frame_t end_frame;
    int rc;

    if (decoder == 0 || pose == 0 || out_frame == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_clamp_pose(decoder, pose, &clamped_pose);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_decoder_read_frame(decoder, clamped_pose.old_frame, &start_frame);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (clamped_pose.old_frame == clamped_pose.frame || clamped_pose.frac <= 0)
    {
        *out_frame = start_frame;
        return MD3_OK;
    }

    rc = md3_decoder_read_frame(decoder, clamped_pose.frame, &end_frame);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (clamped_pose.frac >= MD3_FIXED_ONE)
    {
        *out_frame = end_frame;
        return MD3_OK;
    }

    md3_runtime_lerp_frame(&start_frame, &end_frame, clamped_pose.frac,
        out_frame);
    return MD3_OK;
}

int md3_runtime_sample_tag_index(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_i32 tag_index, md3_tag_t *out_tag)
{
    md3_pose_t clamped_pose;
    md3_tag_t start_tag;
    md3_tag_t end_tag;
    int rc;

    if (decoder == 0 || pose == 0 || out_tag == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_clamp_pose(decoder, pose, &clamped_pose);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_runtime_read_tag_by_index_for_frame(decoder,
        clamped_pose.old_frame, tag_index, &start_tag);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (clamped_pose.old_frame == clamped_pose.frame || clamped_pose.frac <= 0)
    {
        *out_tag = start_tag;
        return MD3_OK;
    }

    rc = md3_runtime_read_tag_by_index_for_frame(decoder, clamped_pose.frame,
        tag_index, &end_tag);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (clamped_pose.frac >= MD3_FIXED_ONE)
    {
        *out_tag = end_tag;
        return MD3_OK;
    }

    md3_runtime_lerp_tag(&start_tag, &end_tag, clamped_pose.frac, out_tag);
    return MD3_OK;
}

int md3_runtime_sample_tag_name(const md3_decoder_t *decoder,
    const md3_pose_t *pose, const char *tag_name, md3_tag_t *out_tag)
{
    md3_i32 tag_index;
    int rc;

    if (tag_name == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_find_tag_index(decoder, tag_name, &tag_index);
    if (rc != MD3_OK)
    {
        return rc;
    }

    return md3_runtime_sample_tag_index(decoder, pose, tag_index, out_tag);
}

int md3_runtime_sample_tag_transform_index(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_i32 tag_index,
    md3_transform_t *out_transform)
{
    md3_tag_t tag;
    int rc;

    if (out_transform == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_sample_tag_index(decoder, pose, tag_index, &tag);
    if (rc != MD3_OK)
    {
        return rc;
    }

    md3_transform_from_tag(&tag, out_transform);
    return MD3_OK;
}

int md3_runtime_sample_tag_transform_name(const md3_decoder_t *decoder,
    const md3_pose_t *pose, const char *tag_name,
    md3_transform_t *out_transform)
{
    md3_i32 tag_index;
    int rc;

    if (tag_name == 0 || out_transform == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_find_tag_index(decoder, tag_name, &tag_index);
    if (rc != MD3_OK)
    {
        return rc;
    }

    return md3_runtime_sample_tag_transform_index(decoder, pose, tag_index,
        out_transform);
}

int md3_runtime_sample_surface(const md3_decoder_t *decoder,
    md3_i32 surface_index, const md3_pose_t *pose, md3_vertex_t *out_vertices,
    md3_vertex_t *scratch_vertices, md3_i32 vertex_capacity)
{
    md3_pose_t clamped_pose;
    md3_surface_info_t surface_info;
    md3_i32 i;
    int rc;

    if (decoder == 0 || pose == 0 || out_vertices == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_decoder_get_surface_info(decoder, surface_index, &surface_info);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (vertex_capacity < surface_info.num_verts)
    {
        return MD3_ENOSPACE;
    }

    rc = md3_runtime_clamp_pose(decoder, pose, &clamped_pose);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (clamped_pose.old_frame == clamped_pose.frame || clamped_pose.frac <= 0)
    {
        return md3_runtime_read_pose_vertices(decoder, surface_index,
            clamped_pose.old_frame, out_vertices, vertex_capacity);
    }

    if (clamped_pose.frac >= MD3_FIXED_ONE)
    {
        return md3_runtime_read_pose_vertices(decoder, surface_index,
            clamped_pose.frame, out_vertices, vertex_capacity);
    }

    if (scratch_vertices == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_read_pose_vertices(decoder, surface_index,
        clamped_pose.old_frame, out_vertices, vertex_capacity);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_runtime_read_pose_vertices(decoder, surface_index,
        clamped_pose.frame, scratch_vertices, vertex_capacity);
    if (rc != MD3_OK)
    {
        return rc;
    }

    for (i = 0; i < surface_info.num_verts; ++i)
    {
        md3_vec3_t normal;

        md3_vec3_lerp(&out_vertices[i].position, &scratch_vertices[i].position,
            clamped_pose.frac, &out_vertices[i].position);
        md3_vec3_lerp(&out_vertices[i].normal, &scratch_vertices[i].normal,
            clamped_pose.frac, &normal);
        rc = md3_vec3_normalize(&normal, &out_vertices[i].normal);
        if (rc != MD3_OK)
        {
            if (clamped_pose.frac >= MD3_FIXED_HALF)
            {
                out_vertices[i].normal = scratch_vertices[i].normal;
            }
        }
        out_vertices[i].packed_normal = 0U;
        out_vertices[i].has_packed_normal = 0;
    }

    return MD3_OK;
}

int md3_runtime_sample_surface_by_name(const md3_decoder_t *decoder,
    const char *surface_name, const md3_pose_t *pose,
    md3_vertex_t *out_vertices, md3_vertex_t *scratch_vertices,
    md3_i32 vertex_capacity)
{
    md3_i32 surface_index;
    int rc;

    if (surface_name == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_decoder_find_surface_index(decoder, surface_name, &surface_index);
    if (rc != MD3_OK)
    {
        return rc;
    }

    return md3_runtime_sample_surface(decoder, surface_index, pose,
        out_vertices, scratch_vertices, vertex_capacity);
}

int md3_runtime_sample_surface_world(const md3_decoder_t *decoder,
    md3_i32 surface_index, const md3_pose_t *pose,
    const md3_transform_t *model_transform, md3_vertex_t *out_vertices,
    md3_vertex_t *scratch_vertices, md3_i32 vertex_capacity)
{
    md3_surface_info_t surface_info;
    md3_i32 i;
    int rc;

    if (model_transform == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_decoder_get_surface_info(decoder, surface_index, &surface_info);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_runtime_sample_surface(decoder, surface_index, pose, out_vertices,
        scratch_vertices, vertex_capacity);
    if (rc != MD3_OK)
    {
        return rc;
    }

    for (i = 0; i < surface_info.num_verts; ++i)
    {
        md3_vec3_t world_normal;

        md3_transform_point(model_transform, &out_vertices[i].position,
            &out_vertices[i].position);
        md3_transform_vector(model_transform, &out_vertices[i].normal,
            &world_normal);
        rc = md3_vec3_normalize(&world_normal, &out_vertices[i].normal);
        if (rc != MD3_OK)
        {
            md3_vec3_copy(&world_normal, &out_vertices[i].normal);
        }
        out_vertices[i].packed_normal = 0U;
        out_vertices[i].has_packed_normal = 0;
    }

    return MD3_OK;
}

int md3_runtime_sample_surface_world_by_name(const md3_decoder_t *decoder,
    const char *surface_name, const md3_pose_t *pose,
    const md3_transform_t *model_transform, md3_vertex_t *out_vertices,
    md3_vertex_t *scratch_vertices, md3_i32 vertex_capacity)
{
    md3_i32 surface_index;
    int rc;

    if (surface_name == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_decoder_find_surface_index(decoder, surface_name, &surface_index);
    if (rc != MD3_OK)
    {
        return rc;
    }

    return md3_runtime_sample_surface_world(decoder, surface_index, pose,
        model_transform, out_vertices, scratch_vertices, vertex_capacity);
}

int md3_runtime_attach_tag_index(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_i32 tag_index,
    const md3_transform_t *parent_transform,
    md3_transform_t *out_transform)
{
    md3_transform_t tag_transform;
    int rc;

    if (parent_transform == 0 || out_transform == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_sample_tag_transform_index(decoder, pose, tag_index,
        &tag_transform);
    if (rc != MD3_OK)
    {
        return rc;
    }

    return md3_transform_attach_on_tag(parent_transform, &tag_transform,
        out_transform);
}

int md3_runtime_attach_tag_name(const md3_decoder_t *decoder,
    const md3_pose_t *pose, const char *tag_name,
    const md3_transform_t *parent_transform,
    md3_transform_t *out_transform)
{
    md3_i32 tag_index;
    int rc;

    if (tag_name == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_find_tag_index(decoder, tag_name, &tag_index);
    if (rc != MD3_OK)
    {
        return rc;
    }

    return md3_runtime_attach_tag_index(decoder, pose, tag_index,
        parent_transform, out_transform);
}

int md3_runtime_attach_rotated_tag_index(const md3_decoder_t *decoder,
    const md3_pose_t *pose, md3_i32 tag_index,
    const md3_transform_t *entity_local_transform,
    const md3_transform_t *parent_transform,
    md3_transform_t *out_transform)
{
    md3_transform_t tag_transform;
    int rc;

    if (entity_local_transform == 0 || parent_transform == 0 ||
        out_transform == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_sample_tag_transform_index(decoder, pose, tag_index,
        &tag_transform);
    if (rc != MD3_OK)
    {
        return rc;
    }

    return md3_transform_attach_rotated_on_tag(entity_local_transform,
        parent_transform, &tag_transform, out_transform);
}

int md3_runtime_attach_rotated_tag_name(const md3_decoder_t *decoder,
    const md3_pose_t *pose, const char *tag_name,
    const md3_transform_t *entity_local_transform,
    const md3_transform_t *parent_transform,
    md3_transform_t *out_transform)
{
    md3_i32 tag_index;
    int rc;

    if (tag_name == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_find_tag_index(decoder, tag_name, &tag_index);
    if (rc != MD3_OK)
    {
        return rc;
    }

    return md3_runtime_attach_rotated_tag_index(decoder, pose, tag_index,
        entity_local_transform, parent_transform, out_transform);
}
