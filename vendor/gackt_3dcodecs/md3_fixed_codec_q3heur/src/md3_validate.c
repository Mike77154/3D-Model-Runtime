#include "md3_internal.h"

#define MD3_I32_MAX_VALUE 2147483647L

typedef struct md3_range_s
{
    md3_i32 start;
    md3_i32 end;
    int is_non_empty;
} md3_range_t;

static int md3_positive_or_zero(md3_i32 value)
{
    return (value >= 0) ? 1 : 0;
}

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

static int md3_make_range(md3_i32 offset, md3_i32 size, md3_i32 limit_end,
    md3_range_t *out_range)
{
    md3_i32 end_value;
    int rc;

    if (out_range == 0)
    {
        return MD3_EINVAL;
    }

    if (offset < 0 || size < 0 || limit_end < 0)
    {
        return MD3_EFORMAT;
    }

    rc = md3_checked_add_i32(offset, size, &end_value);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (end_value > limit_end)
    {
        return MD3_EFORMAT;
    }

    out_range->start = offset;
    out_range->end = end_value;
    out_range->is_non_empty = (size > 0) ? 1 : 0;
    return MD3_OK;
}

static int md3_ranges_overlap(const md3_range_t *a, const md3_range_t *b)
{
    if (a->is_non_empty == 0 || b->is_non_empty == 0)
    {
        return 0;
    }

    if (a->end <= b->start || b->end <= a->start)
    {
        return 0;
    }

    return 1;
}

static int md3_surface_sizes(const md3_surface_info_t *surface_info,
    md3_i32 *shader_bytes, md3_i32 *triangle_bytes,
    md3_i32 *st_bytes, md3_i32 *xyznormal_bytes)
{
    md3_i32 xyz_count;
    int rc;

    rc = md3_checked_mul_i32(surface_info->num_shaders, MD3_SHADER_SIZE,
        shader_bytes);
    if (rc != MD3_OK) return rc;

    rc = md3_checked_mul_i32(surface_info->num_triangles, MD3_TRIANGLE_SIZE,
        triangle_bytes);
    if (rc != MD3_OK) return rc;

    rc = md3_checked_mul_i32(surface_info->num_verts, MD3_ST_SIZE, st_bytes);
    if (rc != MD3_OK) return rc;

    rc = md3_checked_mul_i32(surface_info->num_frames, surface_info->num_verts,
        &xyz_count);
    if (rc != MD3_OK) return rc;

    rc = md3_checked_mul_i32(xyz_count, MD3_XYZNORMAL_SIZE, xyznormal_bytes);
    if (rc != MD3_OK) return rc;

    return MD3_OK;
}

static int md3_header_sizes(const md3_header_info_t *header_info,
    md3_i32 *frame_bytes, md3_i32 *tag_bytes)
{
    md3_i32 frame_count_times_size;
    md3_i32 tag_count;
    int rc;

    rc = md3_checked_mul_i32(header_info->num_frames, MD3_FRAME_SIZE,
        &frame_count_times_size);
    if (rc != MD3_OK) return rc;

    rc = md3_checked_mul_i32(header_info->num_frames, header_info->num_tags,
        &tag_count);
    if (rc != MD3_OK) return rc;

    rc = md3_checked_mul_i32(tag_count, MD3_TAG_SIZE, tag_bytes);
    if (rc != MD3_OK) return rc;

    *frame_bytes = frame_count_times_size;
    return MD3_OK;
}

static int md3_validate_surface_generic(const md3_surface_info_t *surface_info,
    md3_i32 expected_num_frames)
{
    md3_i32 shader_bytes;
    md3_i32 triangle_bytes;
    md3_i32 st_bytes;
    md3_i32 xyznormal_bytes;
    md3_range_t shader_range;
    md3_range_t triangle_range;
    md3_range_t st_range;
    md3_range_t xyznormal_range;
    int rc;

    if (surface_info == 0)
    {
        return MD3_EINVAL;
    }

    if (expected_num_frames >= 0 &&
        surface_info->num_frames != expected_num_frames)
    {
        return MD3_EFORMAT;
    }

    if (surface_info->num_frames <= 0 ||
        surface_info->num_frames > MD3_MAX_FRAMES)
    {
        return MD3_ERANGE;
    }

    if (surface_info->num_shaders < 0 ||
        surface_info->num_shaders > MD3_MAX_SHADERS)
    {
        return MD3_ERANGE;
    }

    if (surface_info->num_verts < 0 ||
        surface_info->num_verts > MD3_MAX_VERTS)
    {
        return MD3_ERANGE;
    }

    if (surface_info->num_triangles < 0 ||
        surface_info->num_triangles > MD3_MAX_TRIANGLES)
    {
        return MD3_ERANGE;
    }

    if (surface_info->num_triangles > 0 && surface_info->num_verts <= 0)
    {
        return MD3_EFORMAT;
    }

    if (surface_info->ofs_end < MD3_SURFACE_HEADER_SIZE)
    {
        return MD3_EFORMAT;
    }

    rc = md3_surface_sizes(surface_info, &shader_bytes, &triangle_bytes,
        &st_bytes, &xyznormal_bytes);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_make_range(surface_info->ofs_shaders, shader_bytes,
        surface_info->ofs_end, &shader_range);
    if (rc != MD3_OK) return rc;

    rc = md3_make_range(surface_info->ofs_triangles, triangle_bytes,
        surface_info->ofs_end, &triangle_range);
    if (rc != MD3_OK) return rc;

    rc = md3_make_range(surface_info->ofs_st, st_bytes,
        surface_info->ofs_end, &st_range);
    if (rc != MD3_OK) return rc;

    rc = md3_make_range(surface_info->ofs_xyznormals, xyznormal_bytes,
        surface_info->ofs_end, &xyznormal_range);
    if (rc != MD3_OK) return rc;

    if (md3_ranges_overlap(&shader_range, &triangle_range) ||
        md3_ranges_overlap(&shader_range, &st_range) ||
        md3_ranges_overlap(&shader_range, &xyznormal_range) ||
        md3_ranges_overlap(&triangle_range, &st_range) ||
        md3_ranges_overlap(&triangle_range, &xyznormal_range) ||
        md3_ranges_overlap(&st_range, &xyznormal_range))
    {
        return MD3_EFORMAT;
    }

    return MD3_OK;
}

static int md3_validate_header_generic(const md3_header_info_t *header_info)
{
    md3_i32 frame_bytes;
    md3_i32 tag_bytes;
    md3_i32 surface_bytes;
    md3_range_t frame_range;
    md3_range_t tag_range;
    md3_range_t surface_range;
    int rc;

    if (header_info == 0)
    {
        return MD3_EINVAL;
    }

    if (header_info->num_frames <= 0 ||
        header_info->num_frames > MD3_MAX_FRAMES)
    {
        return MD3_ERANGE;
    }

    if (!md3_positive_or_zero(header_info->num_tags) ||
        header_info->num_tags > MD3_MAX_TAGS)
    {
        return MD3_ERANGE;
    }

    if (!md3_positive_or_zero(header_info->num_surfaces) ||
        header_info->num_surfaces > MD3_MAX_SURFACES)
    {
        return MD3_ERANGE;
    }

    if (!md3_positive_or_zero(header_info->num_skins))
    {
        return MD3_ERANGE;
    }

    if (header_info->ofs_frames < MD3_HEADER_SIZE ||
        header_info->ofs_tags < MD3_HEADER_SIZE ||
        header_info->ofs_surfaces < MD3_HEADER_SIZE ||
        header_info->ofs_end < MD3_HEADER_SIZE)
    {
        return MD3_EFORMAT;
    }

    rc = md3_header_sizes(header_info, &frame_bytes, &tag_bytes);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (header_info->ofs_surfaces > header_info->ofs_end)
    {
        return MD3_EFORMAT;
    }

    surface_bytes = header_info->ofs_end - header_info->ofs_surfaces;

    rc = md3_make_range(header_info->ofs_frames, frame_bytes,
        header_info->ofs_end, &frame_range);
    if (rc != MD3_OK) return rc;

    rc = md3_make_range(header_info->ofs_tags, tag_bytes,
        header_info->ofs_end, &tag_range);
    if (rc != MD3_OK) return rc;

    rc = md3_make_range(header_info->ofs_surfaces, surface_bytes,
        header_info->ofs_end, &surface_range);
    if (rc != MD3_OK) return rc;

    if (header_info->num_surfaces > 0 && surface_range.is_non_empty == 0)
    {
        return MD3_EFORMAT;
    }

    if (md3_ranges_overlap(&frame_range, &tag_range) ||
        md3_ranges_overlap(&frame_range, &surface_range) ||
        md3_ranges_overlap(&tag_range, &surface_range))
    {
        return MD3_EFORMAT;
    }

    return MD3_OK;
}

int md3_validate_header_info(const md3_header_info_t *header_info)
{
    return md3_validate_header_generic(header_info);
}

int md3_validate_surface_info(const md3_surface_info_t *surface_info,
    md3_i32 expected_num_frames)
{
    return md3_validate_surface_generic(surface_info, expected_num_frames);
}

int md3_surface_layout_is_canonical(const md3_surface_info_t *surface_info)
{
    md3_i32 shader_bytes;
    md3_i32 triangle_bytes;
    md3_i32 st_bytes;
    md3_i32 xyznormal_bytes;
    md3_i32 cursor;
    int rc;

    rc = md3_validate_surface_generic(surface_info, surface_info->num_frames);
    if (rc != MD3_OK)
    {
        return 0;
    }

    rc = md3_surface_sizes(surface_info, &shader_bytes, &triangle_bytes,
        &st_bytes, &xyznormal_bytes);
    if (rc != MD3_OK)
    {
        return 0;
    }

    cursor = MD3_SURFACE_HEADER_SIZE;
    if (surface_info->ofs_shaders != cursor) return 0;
    cursor += shader_bytes;
    if (surface_info->ofs_triangles != cursor) return 0;
    cursor += triangle_bytes;
    if (surface_info->ofs_st != cursor) return 0;
    cursor += st_bytes;
    if (surface_info->ofs_xyznormals != cursor) return 0;
    cursor += xyznormal_bytes;
    if (surface_info->ofs_end != cursor) return 0;

    return 1;
}

int md3_validate_model_layout(const md3_header_info_t *header_info,
    const md3_surface_info_t *surface_infos, md3_i32 surface_count,
    unsigned int flags)
{
    md3_i32 i;
    md3_i32 expected_file_ofs;
    md3_i32 frame_bytes;
    md3_i32 tag_bytes;
    int rc;

    rc = md3_validate_header_generic(header_info);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (surface_count != header_info->num_surfaces)
    {
        return MD3_EINVAL;
    }

    if (header_info->num_surfaces > 0 && surface_infos == 0)
    {
        return MD3_EINVAL;
    }

    expected_file_ofs = header_info->ofs_surfaces;
    for (i = 0; i < surface_count; ++i)
    {
        rc = md3_validate_surface_generic(&surface_infos[i],
            header_info->num_frames);
        if (rc != MD3_OK)
        {
            return rc;
        }

        if (surface_infos[i].file_ofs != expected_file_ofs)
        {
            return MD3_EFORMAT;
        }

        rc = md3_checked_add_i32(expected_file_ofs, surface_infos[i].ofs_end,
            &expected_file_ofs);
        if (rc != MD3_OK)
        {
            return rc;
        }

        if (expected_file_ofs > header_info->ofs_end)
        {
            return MD3_EFORMAT;
        }
    }

    if (expected_file_ofs != header_info->ofs_end)
    {
        return MD3_EFORMAT;
    }

    if ((flags & MD3_VALIDATE_LAYOUT_CANONICAL) != 0U)
    {
        rc = md3_header_sizes(header_info, &frame_bytes, &tag_bytes);
        if (rc != MD3_OK)
        {
            return rc;
        }

        if (header_info->ofs_frames != MD3_HEADER_SIZE)
        {
            return MD3_EFORMAT;
        }
        if (header_info->ofs_tags != header_info->ofs_frames + frame_bytes)
        {
            return MD3_EFORMAT;
        }
        if (header_info->ofs_surfaces != header_info->ofs_tags + tag_bytes)
        {
            return MD3_EFORMAT;
        }

        for (i = 0; i < surface_count; ++i)
        {
            if (!md3_surface_layout_is_canonical(&surface_infos[i]))
            {
                return MD3_EFORMAT;
            }
        }
    }

    return MD3_OK;
}

int md3_model_layout_is_canonical(const md3_header_info_t *header_info,
    const md3_surface_info_t *surface_infos, md3_i32 surface_count)
{
    if (md3_validate_model_layout(header_info, surface_infos, surface_count,
        MD3_VALIDATE_LAYOUT_CANONICAL) == MD3_OK)
    {
        return 1;
    }

    return 0;
}
