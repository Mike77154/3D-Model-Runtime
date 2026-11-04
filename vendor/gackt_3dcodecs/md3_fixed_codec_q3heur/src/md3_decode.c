#include "md3_internal.h"

static int md3_read_header_info(FILE *fp, md3_header_info_t *out_header)
{
    md3_u32 ident;
    md3_i32 version;
    md3_u8 name_bytes[MD3_MAX_QPATH];
    int rc;

    if (fp == 0 || out_header == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_io_seek(fp, 0L);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_io_read_u32_le(fp, &ident);
    if (rc != MD3_OK)
    {
        return rc;
    }
    if (ident != MD3_IDENT_U32)
    {
        return MD3_EFORMAT;
    }

    rc = md3_io_read_i32_le(fp, &version);
    if (rc != MD3_OK)
    {
        return rc;
    }
    if (version != MD3_VERSION)
    {
        return MD3_EFORMAT;
    }

    rc = md3_io_read_exact(fp, name_bytes, MD3_MAX_QPATH);
    if (rc != MD3_OK)
    {
        return rc;
    }

    md3_zero_memory(out_header, (unsigned int) sizeof(*out_header));
    md3_read_field_to_cstr(out_header->name, MD3_MAX_QPATH,
        (const char *) name_bytes, MD3_MAX_QPATH);

    rc = md3_io_read_i32_le(fp, &out_header->flags);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_header->num_frames);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_header->num_tags);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_header->num_surfaces);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_header->num_skins);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_header->ofs_frames);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_header->ofs_tags);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_header->ofs_surfaces);
    if (rc != MD3_OK) return rc;
    rc = md3_io_read_i32_le(fp, &out_header->ofs_end);
    if (rc != MD3_OK) return rc;

    return md3_validate_header_info(out_header);
}

static int md3_validate_decoder_ranges(md3_decoder_t *decoder)
{
    long pos;
    int i;
    int rc;

    if (decoder == 0)
    {
        return MD3_EINVAL;
    }

    if ((long) decoder->header.ofs_end > decoder->file_size)
    {
        return MD3_EFORMAT;
    }

    pos = (long) decoder->header.ofs_surfaces;
    for (i = 0; i < decoder->header.num_surfaces; ++i)
    {
        md3_surface_info_t surface_info;

        rc = md3_read_surface_header(decoder->fp, pos, &surface_info);
        if (rc != MD3_OK)
        {
            return rc;
        }

        rc = md3_validate_surface_info(&surface_info, decoder->header.num_frames);
        if (rc != MD3_OK)
        {
            return rc;
        }

        if ((long) surface_info.file_ofs != pos)
        {
            return MD3_EFORMAT;
        }

        if ((long) surface_info.ofs_end <= 0L)
        {
            return MD3_EFORMAT;
        }

        if (pos + (long) surface_info.ofs_end > decoder->file_size)
        {
            return MD3_EFORMAT;
        }

        decoder->surfaces[i] = surface_info;
        pos += (long) surface_info.ofs_end;
    }

    if (pos != (long) decoder->header.ofs_end)
    {
        return MD3_EFORMAT;
    }

    rc = md3_validate_model_layout(&decoder->header, decoder->surfaces,
        decoder->header.num_surfaces, MD3_VALIDATE_LAYOUT_GENERIC);
    if (rc != MD3_OK)
    {
        return rc;
    }

    return MD3_OK;
}

int md3_decoder_open(md3_decoder_t *decoder, FILE *fp)
{
    long saved_pos;
    int rc;

    if (decoder == 0 || fp == 0)
    {
        return MD3_EINVAL;
    }

    md3_zero_memory(decoder, (unsigned int) sizeof(*decoder));

    saved_pos = ftell(fp);
    if (saved_pos < 0L)
    {
        saved_pos = 0L;
    }

    if (fseek(fp, 0L, SEEK_END) != 0)
    {
        return MD3_EIO;
    }

    decoder->file_size = ftell(fp);
    if (decoder->file_size < 0L)
    {
        return MD3_EIO;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0)
    {
        return MD3_EIO;
    }

    decoder->fp = fp;
    rc = md3_read_header_info(fp, &decoder->header);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_validate_decoder_ranges(decoder);
    if (rc != MD3_OK)
    {
        return rc;
    }

    decoder->is_open = 1;

    if (saved_pos != 0L)
    {
        fseek(fp, saved_pos, SEEK_SET);
    }

    return MD3_OK;
}

void md3_decoder_close(md3_decoder_t *decoder)
{
    if (decoder != 0)
    {
        md3_zero_memory(decoder, (unsigned int) sizeof(*decoder));
    }
}

const md3_header_info_t *md3_decoder_header(const md3_decoder_t *decoder)
{
    if (decoder == 0 || decoder->is_open == 0)
    {
        return 0;
    }

    return &decoder->header;
}

int md3_decoder_get_surface_info(const md3_decoder_t *decoder,
    md3_i32 surface_index, md3_surface_info_t *out_info)
{
    if (decoder == 0 || out_info == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    if (surface_index < 0 || surface_index >= decoder->header.num_surfaces)
    {
        return MD3_ERANGE;
    }

    *out_info = decoder->surfaces[surface_index];
    return MD3_OK;
}

int md3_decoder_find_surface_index(const md3_decoder_t *decoder,
    const char *surface_name, md3_i32 *out_index)
{
    md3_i32 i;

    if (decoder == 0 || surface_name == 0 || out_index == 0 ||
        decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    for (i = 0; i < decoder->header.num_surfaces; ++i)
    {
        if (strcmp(decoder->surfaces[i].name, surface_name) == 0)
        {
            *out_index = i;
            return MD3_OK;
        }
    }

    return MD3_EFORMAT;
}

int md3_decoder_read_frame(const md3_decoder_t *decoder,
    md3_i32 frame_index, md3_frame_t *out_frame)
{
    long offset;
    int rc;

    if (decoder == 0 || out_frame == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    if (frame_index < 0 || frame_index >= decoder->header.num_frames)
    {
        return MD3_ERANGE;
    }

    offset = (long) decoder->header.ofs_frames +
        (long) frame_index * (long) MD3_FRAME_SIZE;

    rc = md3_io_seek(decoder->fp, offset);
    if (rc != MD3_OK)
    {
        return rc;
    }

    md3_zero_memory(out_frame, (unsigned int) sizeof(*out_frame));

    rc = md3_read_fixed_f32_raw(decoder->fp, &out_frame->bounds_min.x,
        &out_frame->raw_f32[0]);
    if (rc != MD3_OK) return rc;
    rc = md3_read_fixed_f32_raw(decoder->fp, &out_frame->bounds_min.y,
        &out_frame->raw_f32[1]);
    if (rc != MD3_OK) return rc;
    rc = md3_read_fixed_f32_raw(decoder->fp, &out_frame->bounds_min.z,
        &out_frame->raw_f32[2]);
    if (rc != MD3_OK) return rc;
    rc = md3_read_fixed_f32_raw(decoder->fp, &out_frame->bounds_max.x,
        &out_frame->raw_f32[3]);
    if (rc != MD3_OK) return rc;
    rc = md3_read_fixed_f32_raw(decoder->fp, &out_frame->bounds_max.y,
        &out_frame->raw_f32[4]);
    if (rc != MD3_OK) return rc;
    rc = md3_read_fixed_f32_raw(decoder->fp, &out_frame->bounds_max.z,
        &out_frame->raw_f32[5]);
    if (rc != MD3_OK) return rc;
    rc = md3_read_fixed_f32_raw(decoder->fp, &out_frame->local_origin.x,
        &out_frame->raw_f32[6]);
    if (rc != MD3_OK) return rc;
    rc = md3_read_fixed_f32_raw(decoder->fp, &out_frame->local_origin.y,
        &out_frame->raw_f32[7]);
    if (rc != MD3_OK) return rc;
    rc = md3_read_fixed_f32_raw(decoder->fp, &out_frame->local_origin.z,
        &out_frame->raw_f32[8]);
    if (rc != MD3_OK) return rc;
    rc = md3_read_fixed_f32_raw(decoder->fp, &out_frame->radius,
        &out_frame->raw_f32[9]);
    if (rc != MD3_OK) return rc;

    {
        char temp_name[16];
        rc = md3_io_read_exact(decoder->fp, temp_name, 16U);
        if (rc != MD3_OK)
        {
            return rc;
        }
        md3_read_field_to_cstr(out_frame->name, 16U, temp_name, 16U);
    }

    out_frame->has_raw_f32 = 1;
    return MD3_OK;
}

int md3_decoder_read_tags_for_frame(const md3_decoder_t *decoder,
    md3_i32 frame_index, md3_tag_t *out_tags, md3_i32 tag_capacity)
{
    long offset;
    int rc;
    md3_i32 i;

    if (decoder == 0 || decoder->is_open == 0 || out_tags == 0)
    {
        return MD3_EINVAL;
    }

    if (frame_index < 0 || frame_index >= decoder->header.num_frames)
    {
        return MD3_ERANGE;
    }

    if (tag_capacity < decoder->header.num_tags)
    {
        return MD3_ENOSPACE;
    }

    offset = (long) decoder->header.ofs_tags +
        ((long) frame_index * (long) decoder->header.num_tags *
        (long) MD3_TAG_SIZE);

    rc = md3_io_seek(decoder->fp, offset);
    if (rc != MD3_OK)
    {
        return rc;
    }

    for (i = 0; i < decoder->header.num_tags; ++i)
    {
        char temp_name[MD3_MAX_QPATH];

        md3_zero_memory(&out_tags[i], (unsigned int) sizeof(out_tags[i]));

        rc = md3_io_read_exact(decoder->fp, temp_name, MD3_MAX_QPATH);
        if (rc != MD3_OK) return rc;
        md3_read_field_to_cstr(out_tags[i].name, MD3_MAX_QPATH,
            temp_name, MD3_MAX_QPATH);

        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].origin.x,
            &out_tags[i].raw_f32[0]);
        if (rc != MD3_OK) return rc;
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].origin.y,
            &out_tags[i].raw_f32[1]);
        if (rc != MD3_OK) return rc;
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].origin.z,
            &out_tags[i].raw_f32[2]);
        if (rc != MD3_OK) return rc;

        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].axis[0].x,
            &out_tags[i].raw_f32[3]);
        if (rc != MD3_OK) return rc;
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].axis[0].y,
            &out_tags[i].raw_f32[4]);
        if (rc != MD3_OK) return rc;
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].axis[0].z,
            &out_tags[i].raw_f32[5]);
        if (rc != MD3_OK) return rc;
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].axis[1].x,
            &out_tags[i].raw_f32[6]);
        if (rc != MD3_OK) return rc;
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].axis[1].y,
            &out_tags[i].raw_f32[7]);
        if (rc != MD3_OK) return rc;
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].axis[1].z,
            &out_tags[i].raw_f32[8]);
        if (rc != MD3_OK) return rc;
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].axis[2].x,
            &out_tags[i].raw_f32[9]);
        if (rc != MD3_OK) return rc;
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].axis[2].y,
            &out_tags[i].raw_f32[10]);
        if (rc != MD3_OK) return rc;
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_tags[i].axis[2].z,
            &out_tags[i].raw_f32[11]);
        if (rc != MD3_OK) return rc;

        out_tags[i].has_raw_f32 = 1;
    }

    return MD3_OK;
}

int md3_decoder_read_surface_shaders(const md3_decoder_t *decoder,
    md3_i32 surface_index, md3_shader_t *out_shaders, md3_i32 shader_capacity)
{
    const md3_surface_info_t *surface_info;
    long offset;
    int rc;
    md3_i32 i;

    if (decoder == 0 || out_shaders == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    if (surface_index < 0 || surface_index >= decoder->header.num_surfaces)
    {
        return MD3_ERANGE;
    }

    surface_info = &decoder->surfaces[surface_index];
    if (shader_capacity < surface_info->num_shaders)
    {
        return MD3_ENOSPACE;
    }

    offset = (long) surface_info->file_ofs + (long) surface_info->ofs_shaders;
    rc = md3_io_seek(decoder->fp, offset);
    if (rc != MD3_OK)
    {
        return rc;
    }

    for (i = 0; i < surface_info->num_shaders; ++i)
    {
        char temp_name[MD3_MAX_QPATH];

        rc = md3_io_read_exact(decoder->fp, temp_name, MD3_MAX_QPATH);
        if (rc != MD3_OK) return rc;
        md3_read_field_to_cstr(out_shaders[i].name, MD3_MAX_QPATH,
            temp_name, MD3_MAX_QPATH);

        rc = md3_io_read_i32_le(decoder->fp, &out_shaders[i].shader_index);
        if (rc != MD3_OK) return rc;
    }

    return MD3_OK;
}

int md3_decoder_read_surface_triangles(const md3_decoder_t *decoder,
    md3_i32 surface_index, md3_triangle_t *out_triangles,
    md3_i32 triangle_capacity)
{
    const md3_surface_info_t *surface_info;
    long offset;
    int rc;
    md3_i32 i;

    if (decoder == 0 || out_triangles == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    if (surface_index < 0 || surface_index >= decoder->header.num_surfaces)
    {
        return MD3_ERANGE;
    }

    surface_info = &decoder->surfaces[surface_index];
    if (triangle_capacity < surface_info->num_triangles)
    {
        return MD3_ENOSPACE;
    }

    offset = (long) surface_info->file_ofs + (long) surface_info->ofs_triangles;
    rc = md3_io_seek(decoder->fp, offset);
    if (rc != MD3_OK)
    {
        return rc;
    }

    for (i = 0; i < surface_info->num_triangles; ++i)
    {
        rc = md3_io_read_i32_le(decoder->fp, &out_triangles[i].index[0]);
        if (rc != MD3_OK) return rc;
        rc = md3_io_read_i32_le(decoder->fp, &out_triangles[i].index[1]);
        if (rc != MD3_OK) return rc;
        rc = md3_io_read_i32_le(decoder->fp, &out_triangles[i].index[2]);
        if (rc != MD3_OK) return rc;

        if (out_triangles[i].index[0] < 0 ||
            out_triangles[i].index[1] < 0 ||
            out_triangles[i].index[2] < 0 ||
            out_triangles[i].index[0] >= surface_info->num_verts ||
            out_triangles[i].index[1] >= surface_info->num_verts ||
            out_triangles[i].index[2] >= surface_info->num_verts)
        {
            return MD3_EFORMAT;
        }
    }

    return MD3_OK;
}

int md3_decoder_read_surface_st(const md3_decoder_t *decoder,
    md3_i32 surface_index, md3_st_t *out_st, md3_i32 st_capacity)
{
    const md3_surface_info_t *surface_info;
    long offset;
    int rc;
    md3_i32 i;

    if (decoder == 0 || out_st == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    if (surface_index < 0 || surface_index >= decoder->header.num_surfaces)
    {
        return MD3_ERANGE;
    }

    surface_info = &decoder->surfaces[surface_index];
    if (st_capacity < surface_info->num_verts)
    {
        return MD3_ENOSPACE;
    }

    offset = (long) surface_info->file_ofs + (long) surface_info->ofs_st;
    rc = md3_io_seek(decoder->fp, offset);
    if (rc != MD3_OK)
    {
        return rc;
    }

    for (i = 0; i < surface_info->num_verts; ++i)
    {
        md3_zero_memory(&out_st[i], (unsigned int) sizeof(out_st[i]));
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_st[i].s,
            &out_st[i].raw_f32[0]);
        if (rc != MD3_OK) return rc;
        rc = md3_read_fixed_f32_raw(decoder->fp, &out_st[i].t,
            &out_st[i].raw_f32[1]);
        if (rc != MD3_OK) return rc;
        out_st[i].has_raw_f32 = 1;
    }

    return MD3_OK;
}

int md3_decoder_read_surface_vertices_for_frame(const md3_decoder_t *decoder,
    md3_i32 surface_index, md3_i32 frame_index, md3_vertex_t *out_vertices,
    md3_i32 vertex_capacity)
{
    const md3_surface_info_t *surface_info;
    long offset;
    int rc;
    md3_i32 i;
    md3_i16 raw_x;
    md3_i16 raw_y;
    md3_i16 raw_z;
    md3_u16 packed_normal;

    if (decoder == 0 || out_vertices == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    if (surface_index < 0 || surface_index >= decoder->header.num_surfaces)
    {
        return MD3_ERANGE;
    }

    if (frame_index < 0 || frame_index >= decoder->header.num_frames)
    {
        return MD3_ERANGE;
    }

    surface_info = &decoder->surfaces[surface_index];
    if (vertex_capacity < surface_info->num_verts)
    {
        return MD3_ENOSPACE;
    }

    offset = (long) surface_info->file_ofs +
        (long) surface_info->ofs_xyznormals +
        (long) frame_index * (long) surface_info->num_verts *
        (long) MD3_XYZNORMAL_SIZE;

    rc = md3_io_seek(decoder->fp, offset);
    if (rc != MD3_OK)
    {
        return rc;
    }

    for (i = 0; i < surface_info->num_verts; ++i)
    {
        md3_zero_memory(&out_vertices[i], (unsigned int) sizeof(out_vertices[i]));

        rc = md3_io_read_i16_le(decoder->fp, &raw_x);
        if (rc != MD3_OK) return rc;
        rc = md3_io_read_i16_le(decoder->fp, &raw_y);
        if (rc != MD3_OK) return rc;
        rc = md3_io_read_i16_le(decoder->fp, &raw_z);
        if (rc != MD3_OK) return rc;
        rc = md3_io_read_u16_le(decoder->fp, &packed_normal);
        if (rc != MD3_OK) return rc;

        out_vertices[i].position.x = md3_fixed_from_md3_xyz(raw_x);
        out_vertices[i].position.y = md3_fixed_from_md3_xyz(raw_y);
        out_vertices[i].position.z = md3_fixed_from_md3_xyz(raw_z);
        out_vertices[i].packed_normal = packed_normal;
        out_vertices[i].has_packed_normal = 1;
        md3_decode_packed_normal(packed_normal, &out_vertices[i].normal);
    }

    return MD3_OK;
}
