#include "md3_internal.h"

#define MD3_ENC_STAGE_NEW 0
#define MD3_ENC_STAGE_FRAMES 1
#define MD3_ENC_STAGE_TAGS 2
#define MD3_ENC_STAGE_SURFACES 3
#define MD3_ENC_STAGE_ACTIVE_SURFACE 4
#define MD3_ENC_STAGE_FINISHED 5

#define MD3_I32_MAX_VALUE 2147483647L

static int md3_checked_add_i32(md3_i32 a, md3_i32 b, md3_i32 *out_value)
{
    if (out_value == 0)
    {
        return MD3_EINVAL;
    }

    if (a < 0 || b < 0)
    {
        return MD3_ERANGE;
    }

    if ((long) a > MD3_I32_MAX_VALUE - (long) b)
    {
        return MD3_ERANGE;
    }

    *out_value = a + b;
    return MD3_OK;
}

static int md3_checked_mul_i32(md3_i32 a, md3_i32 b, md3_i32 *out_value)
{
    if (out_value == 0)
    {
        return MD3_EINVAL;
    }

    if (a < 0 || b < 0)
    {
        return MD3_ERANGE;
    }

    if (a != 0 && (long) b > MD3_I32_MAX_VALUE / (long) a)
    {
        return MD3_ERANGE;
    }

    *out_value = a * b;
    return MD3_OK;
}

static int md3_write_header_info(FILE *fp, const md3_header_info_t *header_info)
{
    md3_u8 name_bytes[MD3_MAX_QPATH];

    if (fp == 0 || header_info == 0)
    {
        return MD3_EINVAL;
    }

    md3_copy_cstr_to_field((char *) name_bytes, MD3_MAX_QPATH, header_info->name);

    if (md3_io_write_u32_le(fp, MD3_IDENT_U32) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, MD3_VERSION) != MD3_OK) return MD3_EIO;
    if (md3_io_write_exact(fp, name_bytes, MD3_MAX_QPATH) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, header_info->flags) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, header_info->num_frames) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, header_info->num_tags) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, header_info->num_surfaces) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, header_info->num_skins) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, header_info->ofs_frames) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, header_info->ofs_tags) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, header_info->ofs_surfaces) != MD3_OK) return MD3_EIO;
    if (md3_io_write_i32_le(fp, header_info->ofs_end) != MD3_OK) return MD3_EIO;

    return MD3_OK;
}

int md3_surface_compute_layout(md3_surface_info_t *surface_info)
{
    md3_i32 shader_bytes;
    md3_i32 triangle_bytes;
    md3_i32 st_bytes;
    md3_i32 xyz_count;
    md3_i32 xyznormal_bytes;
    md3_i32 cursor;
    int rc;

    if (surface_info == 0)
    {
        return MD3_EINVAL;
    }

    if (surface_info->num_frames <= 0 ||
        surface_info->num_frames > MD3_MAX_FRAMES ||
        surface_info->num_shaders < 0 ||
        surface_info->num_shaders > MD3_MAX_SHADERS ||
        surface_info->num_verts < 0 ||
        surface_info->num_verts > MD3_MAX_VERTS ||
        surface_info->num_triangles < 0 ||
        surface_info->num_triangles > MD3_MAX_TRIANGLES)
    {
        return MD3_ERANGE;
    }

    rc = md3_checked_mul_i32(surface_info->num_shaders, MD3_SHADER_SIZE,
        &shader_bytes);
    if (rc != MD3_OK) return rc;
    rc = md3_checked_mul_i32(surface_info->num_triangles, MD3_TRIANGLE_SIZE,
        &triangle_bytes);
    if (rc != MD3_OK) return rc;
    rc = md3_checked_mul_i32(surface_info->num_verts, MD3_ST_SIZE,
        &st_bytes);
    if (rc != MD3_OK) return rc;
    rc = md3_checked_mul_i32(surface_info->num_frames, surface_info->num_verts,
        &xyz_count);
    if (rc != MD3_OK) return rc;
    rc = md3_checked_mul_i32(xyz_count, MD3_XYZNORMAL_SIZE,
        &xyznormal_bytes);
    if (rc != MD3_OK) return rc;

    cursor = MD3_SURFACE_HEADER_SIZE;
    surface_info->ofs_shaders = cursor;
    rc = md3_checked_add_i32(cursor, shader_bytes, &cursor);
    if (rc != MD3_OK) return rc;
    surface_info->ofs_triangles = cursor;
    rc = md3_checked_add_i32(cursor, triangle_bytes, &cursor);
    if (rc != MD3_OK) return rc;
    surface_info->ofs_st = cursor;
    rc = md3_checked_add_i32(cursor, st_bytes, &cursor);
    if (rc != MD3_OK) return rc;
    surface_info->ofs_xyznormals = cursor;
    rc = md3_checked_add_i32(cursor, xyznormal_bytes, &cursor);
    if (rc != MD3_OK) return rc;
    surface_info->ofs_end = cursor;

    rc = md3_validate_surface_info(surface_info, surface_info->num_frames);
    if (rc != MD3_OK)
    {
        return rc;
    }

    return MD3_OK;
}

int md3_model_compute_layout(md3_header_info_t *header_info,
    md3_surface_info_t *surface_infos, md3_i32 surface_count)
{
    md3_i32 frame_bytes;
    md3_i32 tag_count;
    md3_i32 tag_bytes;
    md3_i32 cursor;
    md3_i32 i;
    int rc;

    if (header_info == 0 || (surface_infos == 0 && surface_count > 0))
    {
        return MD3_EINVAL;
    }

    if (header_info->num_frames <= 0 ||
        header_info->num_frames > MD3_MAX_FRAMES ||
        header_info->num_tags < 0 ||
        header_info->num_tags > MD3_MAX_TAGS ||
        header_info->num_surfaces < 0 ||
        header_info->num_surfaces > MD3_MAX_SURFACES ||
        header_info->num_skins < 0)
    {
        return MD3_ERANGE;
    }

    if (surface_count != header_info->num_surfaces)
    {
        return MD3_EINVAL;
    }

    rc = md3_checked_mul_i32(header_info->num_frames, MD3_FRAME_SIZE,
        &frame_bytes);
    if (rc != MD3_OK) return rc;
    rc = md3_checked_mul_i32(header_info->num_frames, header_info->num_tags,
        &tag_count);
    if (rc != MD3_OK) return rc;
    rc = md3_checked_mul_i32(tag_count, MD3_TAG_SIZE, &tag_bytes);
    if (rc != MD3_OK) return rc;

    cursor = MD3_HEADER_SIZE;
    header_info->ofs_frames = cursor;
    rc = md3_checked_add_i32(cursor, frame_bytes, &cursor);
    if (rc != MD3_OK) return rc;
    header_info->ofs_tags = cursor;
    rc = md3_checked_add_i32(cursor, tag_bytes, &cursor);
    if (rc != MD3_OK) return rc;
    header_info->ofs_surfaces = cursor;

    for (i = 0; i < surface_count; ++i)
    {
        surface_infos[i].num_frames = header_info->num_frames;
        rc = md3_surface_compute_layout(&surface_infos[i]);
        if (rc != MD3_OK)
        {
            return rc;
        }
        surface_infos[i].file_ofs = cursor;
        rc = md3_checked_add_i32(cursor, surface_infos[i].ofs_end, &cursor);
        if (rc != MD3_OK)
        {
            return rc;
        }
    }

    header_info->ofs_end = cursor;

    return md3_validate_model_layout(header_info, surface_infos, surface_count,
        MD3_VALIDATE_LAYOUT_CANONICAL);
}

void md3_frame_accumulator_begin(md3_frame_accumulator_t *accumulator)
{
    if (accumulator == 0)
    {
        return;
    }

    md3_zero_memory(accumulator, (unsigned int) sizeof(*accumulator));
}

int md3_frame_accumulator_add_vertices(md3_frame_accumulator_t *accumulator,
    const md3_vertex_t *vertices, md3_i32 vertex_count)
{
    md3_i32 i;

    if (accumulator == 0 || (vertices == 0 && vertex_count > 0))
    {
        return MD3_EINVAL;
    }

    for (i = 0; i < vertex_count; ++i)
    {
        if (accumulator->has_any_vertex == 0)
        {
            accumulator->min_value = vertices[i].position;
            accumulator->max_value = vertices[i].position;
            accumulator->has_any_vertex = 1;
        }
        else
        {
            if (vertices[i].position.x < accumulator->min_value.x)
                accumulator->min_value.x = vertices[i].position.x;
            if (vertices[i].position.y < accumulator->min_value.y)
                accumulator->min_value.y = vertices[i].position.y;
            if (vertices[i].position.z < accumulator->min_value.z)
                accumulator->min_value.z = vertices[i].position.z;

            if (vertices[i].position.x > accumulator->max_value.x)
                accumulator->max_value.x = vertices[i].position.x;
            if (vertices[i].position.y > accumulator->max_value.y)
                accumulator->max_value.y = vertices[i].position.y;
            if (vertices[i].position.z > accumulator->max_value.z)
                accumulator->max_value.z = vertices[i].position.z;
        }
    }

    return MD3_OK;
}

int md3_frame_accumulator_finish(md3_frame_accumulator_t *accumulator,
    const char *frame_name, md3_frame_t *out_frame)
{
    md3_fixed_t dx;
    md3_fixed_t dy;
    md3_fixed_t dz;

    if (accumulator == 0 || out_frame == 0)
    {
        return MD3_EINVAL;
    }

    if (accumulator->has_any_vertex == 0)
    {
        return MD3_EINVAL;
    }

    md3_zero_memory(out_frame, (unsigned int) sizeof(*out_frame));
    out_frame->bounds_min = accumulator->min_value;
    out_frame->bounds_max = accumulator->max_value;
    out_frame->local_origin.x = (accumulator->min_value.x >> 1) +
        (accumulator->max_value.x >> 1);
    out_frame->local_origin.y = (accumulator->min_value.y >> 1) +
        (accumulator->max_value.y >> 1);
    out_frame->local_origin.z = (accumulator->min_value.z >> 1) +
        (accumulator->max_value.z >> 1);

    dx = out_frame->bounds_max.x - out_frame->local_origin.x;
    dy = out_frame->bounds_max.y - out_frame->local_origin.y;
    dz = out_frame->bounds_max.z - out_frame->local_origin.z;
    out_frame->radius = md3_fixed_length3(dx, dy, dz);

    md3_copy_cstr_to_field(out_frame->name, 16U, frame_name);
    out_frame->has_raw_f32 = 0;
    return MD3_OK;
}

int md3_encoder_begin(md3_encoder_t *encoder, FILE *fp,
    const md3_header_info_t *header_info,
    const md3_surface_info_t *surface_infos)
{
    int rc;
    md3_i32 i;

    if (encoder == 0 || fp == 0 || header_info == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_validate_model_layout(header_info, surface_infos,
        header_info->num_surfaces, MD3_VALIDATE_LAYOUT_GENERIC);
    if (rc != MD3_OK)
    {
        return rc;
    }

    md3_zero_memory(encoder, (unsigned int) sizeof(*encoder));
    encoder->fp = fp;
    encoder->header = *header_info;

    for (i = 0; i < header_info->num_surfaces; ++i)
    {
        encoder->surfaces[i] = surface_infos[i];
    }

    rc = md3_io_zero_fill(fp, (long) encoder->header.ofs_end);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_io_seek(fp, 0L);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_write_header_info(fp, &encoder->header);
    if (rc != MD3_OK)
    {
        return rc;
    }

    encoder->next_frame_to_write = 0;
    encoder->next_tag_frame_to_write = 0;
    encoder->next_surface_to_write = 0;
    encoder->active_surface = -1;
    encoder->surface_vertex_frames_written = 0;
    encoder->surface_shaders_written = 0;
    encoder->surface_triangles_written = 0;
    encoder->surface_st_written = 0;

    if (encoder->header.num_frames > 0)
    {
        encoder->stage = MD3_ENC_STAGE_FRAMES;
    }
    else if (encoder->header.num_tags > 0)
    {
        encoder->stage = MD3_ENC_STAGE_TAGS;
    }
    else
    {
        encoder->stage = MD3_ENC_STAGE_SURFACES;
    }

    return MD3_OK;
}

int md3_encoder_write_frame(md3_encoder_t *encoder, const md3_frame_t *frame)
{
    long offset;
    int rc;
    char temp_name[16];

    if (encoder == 0 || frame == 0 || encoder->fp == 0)
    {
        return MD3_EINVAL;
    }

    if (encoder->stage != MD3_ENC_STAGE_FRAMES)
    {
        return MD3_ESTATE;
    }

    if (encoder->next_frame_to_write >= encoder->header.num_frames)
    {
        return MD3_ERANGE;
    }

    offset = (long) encoder->header.ofs_frames +
        (long) encoder->next_frame_to_write * (long) MD3_FRAME_SIZE;

    rc = md3_io_seek(encoder->fp, offset);
    if (rc != MD3_OK) return rc;

    rc = md3_write_fixed_f32_raw(encoder->fp, frame->bounds_min.x,
        frame->has_raw_f32, frame->raw_f32[0]);
    if (rc != MD3_OK) return rc;
    rc = md3_write_fixed_f32_raw(encoder->fp, frame->bounds_min.y,
        frame->has_raw_f32, frame->raw_f32[1]);
    if (rc != MD3_OK) return rc;
    rc = md3_write_fixed_f32_raw(encoder->fp, frame->bounds_min.z,
        frame->has_raw_f32, frame->raw_f32[2]);
    if (rc != MD3_OK) return rc;
    rc = md3_write_fixed_f32_raw(encoder->fp, frame->bounds_max.x,
        frame->has_raw_f32, frame->raw_f32[3]);
    if (rc != MD3_OK) return rc;
    rc = md3_write_fixed_f32_raw(encoder->fp, frame->bounds_max.y,
        frame->has_raw_f32, frame->raw_f32[4]);
    if (rc != MD3_OK) return rc;
    rc = md3_write_fixed_f32_raw(encoder->fp, frame->bounds_max.z,
        frame->has_raw_f32, frame->raw_f32[5]);
    if (rc != MD3_OK) return rc;
    rc = md3_write_fixed_f32_raw(encoder->fp, frame->local_origin.x,
        frame->has_raw_f32, frame->raw_f32[6]);
    if (rc != MD3_OK) return rc;
    rc = md3_write_fixed_f32_raw(encoder->fp, frame->local_origin.y,
        frame->has_raw_f32, frame->raw_f32[7]);
    if (rc != MD3_OK) return rc;
    rc = md3_write_fixed_f32_raw(encoder->fp, frame->local_origin.z,
        frame->has_raw_f32, frame->raw_f32[8]);
    if (rc != MD3_OK) return rc;
    rc = md3_write_fixed_f32_raw(encoder->fp, frame->radius,
        frame->has_raw_f32, frame->raw_f32[9]);
    if (rc != MD3_OK) return rc;

    md3_copy_cstr_to_field(temp_name, 16U, frame->name);
    rc = md3_io_write_exact(encoder->fp, temp_name, 16U);
    if (rc != MD3_OK) return rc;

    ++encoder->next_frame_to_write;
    if (encoder->next_frame_to_write == encoder->header.num_frames)
    {
        if (encoder->header.num_tags > 0)
        {
            encoder->stage = MD3_ENC_STAGE_TAGS;
        }
        else
        {
            encoder->stage = MD3_ENC_STAGE_SURFACES;
        }
    }

    return MD3_OK;
}

int md3_encoder_write_tags_for_frame(md3_encoder_t *encoder,
    const md3_tag_t *tags, md3_i32 tag_count)
{
    long offset;
    int rc;
    md3_i32 i;

    if (encoder == 0 || encoder->fp == 0 || tags == 0)
    {
        return MD3_EINVAL;
    }

    if (encoder->stage != MD3_ENC_STAGE_TAGS)
    {
        return MD3_ESTATE;
    }

    if (encoder->next_tag_frame_to_write >= encoder->header.num_frames)
    {
        return MD3_ERANGE;
    }

    if (tag_count != encoder->header.num_tags)
    {
        return MD3_EINVAL;
    }

    offset = (long) encoder->header.ofs_tags +
        (long) encoder->next_tag_frame_to_write *
        (long) encoder->header.num_tags * (long) MD3_TAG_SIZE;

    rc = md3_io_seek(encoder->fp, offset);
    if (rc != MD3_OK) return rc;

    for (i = 0; i < tag_count; ++i)
    {
        char temp_name[MD3_MAX_QPATH];

        md3_copy_cstr_to_field(temp_name, MD3_MAX_QPATH, tags[i].name);
        rc = md3_io_write_exact(encoder->fp, temp_name, MD3_MAX_QPATH);
        if (rc != MD3_OK) return rc;

        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].origin.x,
            tags[i].has_raw_f32, tags[i].raw_f32[0]);
        if (rc != MD3_OK) return rc;
        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].origin.y,
            tags[i].has_raw_f32, tags[i].raw_f32[1]);
        if (rc != MD3_OK) return rc;
        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].origin.z,
            tags[i].has_raw_f32, tags[i].raw_f32[2]);
        if (rc != MD3_OK) return rc;

        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].axis[0].x,
            tags[i].has_raw_f32, tags[i].raw_f32[3]);
        if (rc != MD3_OK) return rc;
        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].axis[0].y,
            tags[i].has_raw_f32, tags[i].raw_f32[4]);
        if (rc != MD3_OK) return rc;
        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].axis[0].z,
            tags[i].has_raw_f32, tags[i].raw_f32[5]);
        if (rc != MD3_OK) return rc;
        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].axis[1].x,
            tags[i].has_raw_f32, tags[i].raw_f32[6]);
        if (rc != MD3_OK) return rc;
        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].axis[1].y,
            tags[i].has_raw_f32, tags[i].raw_f32[7]);
        if (rc != MD3_OK) return rc;
        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].axis[1].z,
            tags[i].has_raw_f32, tags[i].raw_f32[8]);
        if (rc != MD3_OK) return rc;
        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].axis[2].x,
            tags[i].has_raw_f32, tags[i].raw_f32[9]);
        if (rc != MD3_OK) return rc;
        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].axis[2].y,
            tags[i].has_raw_f32, tags[i].raw_f32[10]);
        if (rc != MD3_OK) return rc;
        rc = md3_write_fixed_f32_raw(encoder->fp, tags[i].axis[2].z,
            tags[i].has_raw_f32, tags[i].raw_f32[11]);
        if (rc != MD3_OK) return rc;
    }

    ++encoder->next_tag_frame_to_write;
    if (encoder->next_tag_frame_to_write == encoder->header.num_frames)
    {
        encoder->stage = MD3_ENC_STAGE_SURFACES;
    }

    return MD3_OK;
}

int md3_encoder_begin_surface(md3_encoder_t *encoder, md3_i32 surface_index)
{
    int rc;
    const md3_surface_info_t *surface_info;

    if (encoder == 0 || encoder->fp == 0)
    {
        return MD3_EINVAL;
    }

    if (encoder->stage != MD3_ENC_STAGE_SURFACES)
    {
        return MD3_ESTATE;
    }

    if (surface_index != encoder->next_surface_to_write)
    {
        return MD3_ESTATE;
    }

    if (surface_index < 0 || surface_index >= encoder->header.num_surfaces)
    {
        return MD3_ERANGE;
    }

    surface_info = &encoder->surfaces[surface_index];
    rc = md3_io_seek(encoder->fp, (long) surface_info->file_ofs);
    if (rc != MD3_OK) return rc;

    rc = md3_write_surface_header(encoder->fp, surface_info);
    if (rc != MD3_OK) return rc;

    encoder->active_surface = surface_index;
    encoder->surface_vertex_frames_written = 0;
    encoder->surface_shaders_written = 0;
    encoder->surface_triangles_written = 0;
    encoder->surface_st_written = 0;
    encoder->stage = MD3_ENC_STAGE_ACTIVE_SURFACE;

    return MD3_OK;
}

int md3_encoder_write_surface_shaders(md3_encoder_t *encoder,
    const md3_shader_t *shaders, md3_i32 shader_count)
{
    const md3_surface_info_t *surface_info;
    long offset;
    int rc;
    md3_i32 i;

    if (encoder == 0 || encoder->fp == 0 || shaders == 0)
    {
        return MD3_EINVAL;
    }

    if (encoder->stage != MD3_ENC_STAGE_ACTIVE_SURFACE)
    {
        return MD3_ESTATE;
    }

    surface_info = &encoder->surfaces[encoder->active_surface];
    if (shader_count != surface_info->num_shaders)
    {
        return MD3_EINVAL;
    }

    offset = (long) surface_info->file_ofs + (long) surface_info->ofs_shaders;
    rc = md3_io_seek(encoder->fp, offset);
    if (rc != MD3_OK) return rc;

    for (i = 0; i < shader_count; ++i)
    {
        char temp_name[MD3_MAX_QPATH];

        md3_copy_cstr_to_field(temp_name, MD3_MAX_QPATH, shaders[i].name);
        rc = md3_io_write_exact(encoder->fp, temp_name, MD3_MAX_QPATH);
        if (rc != MD3_OK) return rc;
        rc = md3_io_write_i32_le(encoder->fp, shaders[i].shader_index);
        if (rc != MD3_OK) return rc;
    }

    encoder->surface_shaders_written = shader_count;
    return MD3_OK;
}

int md3_encoder_write_surface_triangles(md3_encoder_t *encoder,
    const md3_triangle_t *triangles, md3_i32 triangle_count)
{
    const md3_surface_info_t *surface_info;
    long offset;
    int rc;
    md3_i32 i;

    if (encoder == 0 || encoder->fp == 0 || triangles == 0)
    {
        return MD3_EINVAL;
    }

    if (encoder->stage != MD3_ENC_STAGE_ACTIVE_SURFACE)
    {
        return MD3_ESTATE;
    }

    surface_info = &encoder->surfaces[encoder->active_surface];
    if (triangle_count != surface_info->num_triangles)
    {
        return MD3_EINVAL;
    }

    offset = (long) surface_info->file_ofs + (long) surface_info->ofs_triangles;
    rc = md3_io_seek(encoder->fp, offset);
    if (rc != MD3_OK) return rc;

    for (i = 0; i < triangle_count; ++i)
    {
        if (triangles[i].index[0] < 0 || triangles[i].index[1] < 0 ||
            triangles[i].index[2] < 0 ||
            triangles[i].index[0] >= surface_info->num_verts ||
            triangles[i].index[1] >= surface_info->num_verts ||
            triangles[i].index[2] >= surface_info->num_verts)
        {
            return MD3_EFORMAT;
        }

        rc = md3_io_write_i32_le(encoder->fp, triangles[i].index[0]);
        if (rc != MD3_OK) return rc;
        rc = md3_io_write_i32_le(encoder->fp, triangles[i].index[1]);
        if (rc != MD3_OK) return rc;
        rc = md3_io_write_i32_le(encoder->fp, triangles[i].index[2]);
        if (rc != MD3_OK) return rc;
    }

    encoder->surface_triangles_written = triangle_count;
    return MD3_OK;
}

int md3_encoder_write_surface_st(md3_encoder_t *encoder,
    const md3_st_t *st, md3_i32 st_count)
{
    const md3_surface_info_t *surface_info;
    long offset;
    int rc;
    md3_i32 i;

    if (encoder == 0 || encoder->fp == 0 || st == 0)
    {
        return MD3_EINVAL;
    }

    if (encoder->stage != MD3_ENC_STAGE_ACTIVE_SURFACE)
    {
        return MD3_ESTATE;
    }

    surface_info = &encoder->surfaces[encoder->active_surface];
    if (st_count != surface_info->num_verts)
    {
        return MD3_EINVAL;
    }

    offset = (long) surface_info->file_ofs + (long) surface_info->ofs_st;
    rc = md3_io_seek(encoder->fp, offset);
    if (rc != MD3_OK) return rc;

    for (i = 0; i < st_count; ++i)
    {
        rc = md3_write_fixed_f32_raw(encoder->fp, st[i].s,
            st[i].has_raw_f32, st[i].raw_f32[0]);
        if (rc != MD3_OK) return rc;
        rc = md3_write_fixed_f32_raw(encoder->fp, st[i].t,
            st[i].has_raw_f32, st[i].raw_f32[1]);
        if (rc != MD3_OK) return rc;
    }

    encoder->surface_st_written = st_count;
    return MD3_OK;
}

int md3_encoder_write_surface_vertices_for_frame(md3_encoder_t *encoder,
    const md3_vertex_t *vertices, md3_i32 vertex_count)
{
    const md3_surface_info_t *surface_info;
    long offset;
    int rc;
    md3_i32 i;
    md3_u16 packed_normal;

    if (encoder == 0 || encoder->fp == 0 || vertices == 0)
    {
        return MD3_EINVAL;
    }

    if (encoder->stage != MD3_ENC_STAGE_ACTIVE_SURFACE)
    {
        return MD3_ESTATE;
    }

    surface_info = &encoder->surfaces[encoder->active_surface];
    if (vertex_count != surface_info->num_verts)
    {
        return MD3_EINVAL;
    }

    if (encoder->surface_vertex_frames_written >= surface_info->num_frames)
    {
        return MD3_ERANGE;
    }

    offset = (long) surface_info->file_ofs +
        (long) surface_info->ofs_xyznormals +
        (long) encoder->surface_vertex_frames_written *
        (long) surface_info->num_verts * (long) MD3_XYZNORMAL_SIZE;

    rc = md3_io_seek(encoder->fp, offset);
    if (rc != MD3_OK) return rc;

    for (i = 0; i < vertex_count; ++i)
    {
        rc = md3_io_write_i16_le(encoder->fp,
            md3_fixed_to_md3_xyz(vertices[i].position.x));
        if (rc != MD3_OK) return rc;
        rc = md3_io_write_i16_le(encoder->fp,
            md3_fixed_to_md3_xyz(vertices[i].position.y));
        if (rc != MD3_OK) return rc;
        rc = md3_io_write_i16_le(encoder->fp,
            md3_fixed_to_md3_xyz(vertices[i].position.z));
        if (rc != MD3_OK) return rc;

        if (vertices[i].has_packed_normal)
        {
            packed_normal = vertices[i].packed_normal;
        }
        else
        {
            packed_normal = md3_encode_packed_normal(&vertices[i].normal);
        }

        rc = md3_io_write_u16_le(encoder->fp, packed_normal);
        if (rc != MD3_OK) return rc;
    }

    ++encoder->surface_vertex_frames_written;
    return MD3_OK;
}

int md3_encoder_end_surface(md3_encoder_t *encoder)
{
    const md3_surface_info_t *surface_info;

    if (encoder == 0 || encoder->fp == 0)
    {
        return MD3_EINVAL;
    }

    if (encoder->stage != MD3_ENC_STAGE_ACTIVE_SURFACE)
    {
        return MD3_ESTATE;
    }

    surface_info = &encoder->surfaces[encoder->active_surface];

    if (encoder->surface_shaders_written != surface_info->num_shaders ||
        encoder->surface_triangles_written != surface_info->num_triangles ||
        encoder->surface_st_written != surface_info->num_verts ||
        encoder->surface_vertex_frames_written != surface_info->num_frames)
    {
        return MD3_ESTATE;
    }

    ++encoder->next_surface_to_write;
    encoder->active_surface = -1;
    encoder->stage = MD3_ENC_STAGE_SURFACES;
    return MD3_OK;
}

int md3_encoder_finish(md3_encoder_t *encoder)
{
    if (encoder == 0 || encoder->fp == 0)
    {
        return MD3_EINVAL;
    }

    if (encoder->stage == MD3_ENC_STAGE_ACTIVE_SURFACE)
    {
        return MD3_ESTATE;
    }

    if (encoder->next_frame_to_write != encoder->header.num_frames)
    {
        return MD3_ESTATE;
    }

    if (encoder->header.num_tags > 0 &&
        encoder->next_tag_frame_to_write != encoder->header.num_frames)
    {
        return MD3_ESTATE;
    }

    if (encoder->next_surface_to_write != encoder->header.num_surfaces)
    {
        return MD3_ESTATE;
    }

    encoder->stage = MD3_ENC_STAGE_FINISHED;
    fflush(encoder->fp);
    return MD3_OK;
}
