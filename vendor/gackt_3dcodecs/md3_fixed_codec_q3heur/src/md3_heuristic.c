#include "md3_internal.h"

#define MD3_HEURISTIC_I32_MAX 2147483647L

static char md3_ascii_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return (char) (c + ('a' - 'A'));
    }
    return c;
}

static md3_i32 md3_wrap_i32(md3_i32 value, md3_i32 count)
{
    md3_u32 magnitude;
    md3_u32 remainder;

    if (count <= 0)
    {
        return 0;
    }

    if (value >= 0)
    {
        return (md3_i32) ((md3_u32) value % (md3_u32) count);
    }

    magnitude = 0U - (md3_u32) value;
    remainder = magnitude % (md3_u32) count;
    if (remainder == 0U)
    {
        return 0;
    }

    return (md3_i32) ((md3_u32) count - remainder);
}

static md3_i32 md3_q3a_frame_mod_i32(md3_i32 value, md3_i32 count)
{
    if (count <= 0)
    {
        return 0;
    }

    if (value < 0)
    {
        return value;
    }

    return (md3_i32) ((md3_u32) value % (md3_u32) count);
}

static int md3_pose_frame_pair_is_valid(const md3_pose_t *pose,
    md3_i32 frame_count)
{
    if (pose == 0 || frame_count <= 0)
    {
        return 0;
    }

    if (pose->old_frame < 0 || pose->old_frame >= frame_count ||
        pose->frame < 0 || pose->frame >= frame_count)
    {
        return 0;
    }

    return 1;
}

static md3_i32 md3_clamp_frame_i32(md3_i32 value, md3_i32 frame_count)
{
    if (frame_count <= 0)
    {
        return 0;
    }
    if (value < 0)
    {
        return 0;
    }
    if (value >= frame_count)
    {
        return frame_count - 1;
    }
    return value;
}

static md3_fixed_t md3_clamp_frac_q16(md3_fixed_t value)
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

static int md3_checked_add_summary(md3_i32 a, md3_i32 b,
    md3_i32 *out_value)
{
    if (out_value == 0)
    {
        return MD3_EINVAL;
    }
    if (a < 0 || b < 0)
    {
        return MD3_ERANGE;
    }
    if ((long) a > MD3_HEURISTIC_I32_MAX - (long) b)
    {
        return MD3_ERANGE;
    }
    *out_value = a + b;
    return MD3_OK;
}

static int md3_checked_mul_summary(md3_i32 a, md3_i32 b,
    md3_i32 *out_value)
{
    if (out_value == 0)
    {
        return MD3_EINVAL;
    }
    if (a < 0 || b < 0)
    {
        return MD3_ERANGE;
    }
    if (a != 0 && (long) b > MD3_HEURISTIC_I32_MAX / (long) a)
    {
        return MD3_ERANGE;
    }
    *out_value = a * b;
    return MD3_OK;
}

static void md3_bounds_center_from_min_max(md3_bounds_t *bounds)
{
    if (bounds == 0)
    {
        return;
    }

    bounds->local_origin.x = (bounds->mins.x >> 1) +
        (bounds->maxs.x >> 1);
    bounds->local_origin.y = (bounds->mins.y >> 1) +
        (bounds->maxs.y >> 1);
    bounds->local_origin.z = (bounds->mins.z >> 1) +
        (bounds->maxs.z >> 1);
}

static void md3_bounds_recompute_radius(md3_bounds_t *bounds)
{
    md3_fixed_t dx;
    md3_fixed_t dy;
    md3_fixed_t dz;

    if (bounds == 0)
    {
        return;
    }

    dx = bounds->maxs.x - bounds->local_origin.x;
    dy = bounds->maxs.y - bounds->local_origin.y;
    dz = bounds->maxs.z - bounds->local_origin.z;
    bounds->radius = md3_fixed_length3(dx, dy, dz);
}

static int md3_char_is_space_q3a(char c)
{
    return ((unsigned char) c <= (unsigned char) ' ') ? 1 : 0;
}

static int md3_name_has_tag_prefix_q3a(const char *name)
{
    const char *p;

    if (name == 0)
    {
        return 0;
    }

    p = name;
    while (p[0] != '\0' && p[1] != '\0' && p[2] != '\0' &&
        p[3] != '\0')
    {
        if (md3_ascii_lower(p[0]) == 't' &&
            md3_ascii_lower(p[1]) == 'a' &&
            md3_ascii_lower(p[2]) == 'g' && p[3] == '_')
        {
            return 1;
        }
        ++p;
    }

    return 0;
}

static void md3_skin_skip_ws_and_comments_q3a(const char **cursor)
{
    const char *p;

    if (cursor == 0 || *cursor == 0)
    {
        return;
    }

    p = *cursor;
    for (;;)
    {
        while (*p != '\0' && md3_char_is_space_q3a(*p))
        {
            ++p;
        }

        if (p[0] == '/' && p[1] == '/')
        {
            while (*p != '\0' && *p != '\n')
            {
                ++p;
            }
            continue;
        }

        if (p[0] == '/' && p[1] == '*')
        {
            p += 2;
            while (*p != '\0' && !(p[0] == '*' && p[1] == '/'))
            {
                ++p;
            }
            if (*p != '\0')
            {
                p += 2;
            }
            continue;
        }

        break;
    }

    *cursor = p;
}


static int md3_skin_is_newline_q3a(char c)
{
    return (c == '\n' || c == '\r') ? 1 : 0;
}

static int md3_char_is_inline_space_q3a(char c)
{
    if (md3_skin_is_newline_q3a(c))
    {
        return 0;
    }
    return md3_char_is_space_q3a(c);
}

static void md3_skin_skip_record_tail_q3a(const char **cursor)
{
    const char *p;

    if (cursor == 0 || *cursor == 0)
    {
        return;
    }

    p = *cursor;
    while (*p != '\0' && !md3_skin_is_newline_q3a(*p))
    {
        ++p;
    }
    if (p[0] == '\r' && p[1] == '\n')
    {
        p += 2;
    }
    else if (md3_skin_is_newline_q3a(*p))
    {
        ++p;
    }

    *cursor = p;
}

static void md3_skin_skip_inline_ws_and_comments_q3a(const char **cursor)
{
    const char *p;

    if (cursor == 0 || *cursor == 0)
    {
        return;
    }

    p = *cursor;
    for (;;)
    {
        while (*p != '\0' && md3_char_is_inline_space_q3a(*p))
        {
            ++p;
        }

        if (p[0] == '/' && p[1] == '/')
        {
            while (*p != '\0' && !md3_skin_is_newline_q3a(*p))
            {
                ++p;
            }
            break;
        }

        if (p[0] == '/' && p[1] == '*')
        {
            p += 2;
            while (*p != '\0' && !(p[0] == '*' && p[1] == '/'))
            {
                ++p;
            }
            if (*p != '\0')
            {
                p += 2;
            }
            continue;
        }

        break;
    }

    *cursor = p;
}

static int md3_skin_parse_token_inline_q3a(const char **cursor, char *dst,
    unsigned int dst_size)
{
    const char *p;
    unsigned int len;
    char c;
    int truncated;

    if (dst != 0 && dst_size > 0U)
    {
        dst[0] = '\0';
    }

    if (cursor == 0 || *cursor == 0 || dst == 0 || dst_size == 0U)
    {
        return 0;
    }

    p = *cursor;
    md3_skin_skip_inline_ws_and_comments_q3a(&p);
    len = 0U;
    truncated = 0;

    if (*p == '"')
    {
        ++p;
        while (*p != '\0' && *p != '"' &&
            !md3_skin_is_newline_q3a(*p))
        {
            if (len + 1U < dst_size)
            {
                dst[len] = *p;
                ++len;
            }
            else
            {
                truncated = 1;
            }
            ++p;
        }
        if (*p == '"')
        {
            ++p;
        }
    }
    else
    {
        c = *p;
        while (c != '\0' && !md3_skin_is_newline_q3a(c) &&
            !md3_char_is_space_q3a(c) && c != ',')
        {
            if (len + 1U < dst_size)
            {
                dst[len] = c;
                ++len;
            }
            else
            {
                truncated = 1;
            }
            ++p;
            c = *p;
        }
    }

    dst[len] = '\0';
    *cursor = p;
    return truncated;
}

static int md3_skin_parse_token_q3a(const char **cursor, char *dst,
    unsigned int dst_size)
{
    const char *p;
    unsigned int len;
    char c;
    int truncated;

    if (dst != 0 && dst_size > 0U)
    {
        dst[0] = '\0';
    }

    if (cursor == 0 || *cursor == 0 || dst == 0 || dst_size == 0U)
    {
        return 0;
    }

    p = *cursor;
    md3_skin_skip_ws_and_comments_q3a(&p);
    len = 0U;
    truncated = 0;

    if (*p == '"')
    {
        ++p;
        while (*p != '\0' && *p != '"')
        {
            if (len + 1U < dst_size)
            {
                dst[len] = *p;
                ++len;
            }
            else
            {
                truncated = 1;
            }
            ++p;
        }
        if (*p == '"')
        {
            ++p;
        }
    }
    else
    {
        c = *p;
        while (c != '\0' && !md3_char_is_space_q3a(c) && c != ',')
        {
            if (len + 1U < dst_size)
            {
                dst[len] = c;
                ++len;
            }
            else
            {
                truncated = 1;
            }
            ++p;
            c = *p;
        }
    }

    dst[len] = '\0';
    *cursor = p;
    return truncated;
}

void md3_q3a_canonical_surface_name(const char *src, char *dst,
    unsigned int dst_size)
{
    unsigned int i;
    unsigned int len;

    if (dst == 0 || dst_size == 0U)
    {
        return;
    }

    for (i = 0; i < dst_size; ++i)
    {
        dst[i] = '\0';
    }

    if (src == 0)
    {
        return;
    }

    i = 0U;
    while (i + 1U < dst_size && src[i] != '\0')
    {
        dst[i] = md3_ascii_lower(src[i]);
        ++i;
    }
    dst[i] = '\0';

    len = 0U;
    while (len < dst_size && dst[len] != '\0')
    {
        ++len;
    }

    if (len > 2U && dst[len - 2U] == '_')
    {
        dst[len - 2U] = '\0';
    }
}

int md3_q3a_surface_name_equal(const char *a, const char *b)
{
    char ca[MD3_MAX_QPATH];
    char cb[MD3_MAX_QPATH];

    if (a == 0 || b == 0)
    {
        return 0;
    }

    md3_q3a_canonical_surface_name(a, ca, MD3_MAX_QPATH);
    md3_q3a_canonical_surface_name(b, cb, MD3_MAX_QPATH);
    return (strcmp(ca, cb) == 0) ? 1 : 0;
}

int md3_decoder_find_surface_index_q3a(const md3_decoder_t *decoder,
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
        if (md3_q3a_surface_name_equal(decoder->surfaces[i].name,
            surface_name))
        {
            *out_index = i;
            return MD3_OK;
        }
    }

    return MD3_EFORMAT;
}

int md3_decoder_find_tag_index_from_q3a(const md3_decoder_t *decoder,
    const char *tag_name, md3_i32 start_index, md3_i32 *out_index)
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

    if (start_index < 0 || start_index >= decoder->header.num_tags)
    {
        return MD3_EFORMAT;
    }

    rc = md3_decoder_read_tags_for_frame(decoder, 0, tags, MD3_MAX_TAGS);
    if (rc != MD3_OK)
    {
        return rc;
    }

    for (i = start_index; i < decoder->header.num_tags; ++i)
    {
        if (strcmp(tags[i].name, tag_name) == 0)
        {
            *out_index = i;
            return MD3_OK;
        }
    }

    return MD3_EFORMAT;
}

int md3_runtime_find_tag_index_from(const md3_decoder_t *decoder,
    const char *tag_name, md3_i32 start_index, md3_i32 *out_index)
{
    return md3_decoder_find_tag_index_from_q3a(decoder, tag_name,
        start_index, out_index);
}

int md3_runtime_find_tag_index_cached(const md3_decoder_t *decoder,
    const char *tag_name, md3_i32 *io_tag_index)
{
    md3_tag_t tags[MD3_MAX_TAGS];
    md3_i32 tag_index;
    int rc;

    if (decoder == 0 || tag_name == 0 || io_tag_index == 0 ||
        decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    if (*io_tag_index >= 0 && *io_tag_index < decoder->header.num_tags)
    {
        rc = md3_decoder_read_tags_for_frame(decoder, 0, tags, MD3_MAX_TAGS);
        if (rc != MD3_OK)
        {
            return rc;
        }
        if (strcmp(tags[*io_tag_index].name, tag_name) == 0)
        {
            return MD3_OK;
        }
    }

    rc = md3_decoder_find_tag_index_from_q3a(decoder, tag_name, 0,
        &tag_index);
    if (rc == MD3_OK)
    {
        *io_tag_index = tag_index;
    }
    else if (rc == MD3_EFORMAT)
    {
        *io_tag_index = -1;
    }

    return rc;
}

void md3_pose_from_q3a_backlerp(md3_pose_t *pose, md3_i32 old_frame,
    md3_i32 frame, md3_fixed_t backlerp)
{
    md3_fixed_t clamped_backlerp;

    if (pose == 0)
    {
        return;
    }

    clamped_backlerp = md3_clamp_frac_q16(backlerp);
    pose->old_frame = old_frame;
    pose->frame = frame;
    pose->frac = MD3_FIXED_ONE - clamped_backlerp;
}

int md3_runtime_sanitize_pose(const md3_decoder_t *decoder,
    const md3_pose_t *pose, unsigned int flags, md3_pose_t *out_pose)
{
    md3_i32 frame_count;

    if (decoder == 0 || pose == 0 || out_pose == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    frame_count = decoder->header.num_frames;
    *out_pose = *pose;

    if ((flags & MD3_HEURISTIC_POSE_WRAP_FRAMES) != 0U)
    {
        if ((flags & MD3_HEURISTIC_POSE_RESET_BAD) != 0U)
        {
            out_pose->old_frame = md3_q3a_frame_mod_i32(
                out_pose->old_frame, frame_count);
            out_pose->frame = md3_q3a_frame_mod_i32(out_pose->frame,
                frame_count);
        }
        else
        {
            out_pose->old_frame = md3_wrap_i32(out_pose->old_frame,
                frame_count);
            out_pose->frame = md3_wrap_i32(out_pose->frame, frame_count);
        }
    }
    else if ((flags & MD3_HEURISTIC_POSE_CLAMP_FRAMES) != 0U)
    {
        out_pose->old_frame = md3_clamp_frame_i32(out_pose->old_frame,
            frame_count);
        out_pose->frame = md3_clamp_frame_i32(out_pose->frame,
            frame_count);
    }

    if (!md3_pose_frame_pair_is_valid(out_pose, frame_count))
    {
        if ((flags & MD3_HEURISTIC_POSE_RESET_BAD) != 0U)
        {
            out_pose->old_frame = 0;
            out_pose->frame = 0;
        }
        else
        {
            return MD3_ERANGE;
        }
    }

    if ((flags & MD3_HEURISTIC_POSE_CLAMP_FRAC) != 0U)
    {
        out_pose->frac = md3_clamp_frac_q16(out_pose->frac);
    }
    else if (out_pose->frac < 0 || out_pose->frac > MD3_FIXED_ONE)
    {
        return MD3_ERANGE;
    }

    return MD3_OK;
}

int md3_runtime_sanitize_pose_q3a_renderfx(const md3_decoder_t *decoder,
    const md3_pose_t *pose, unsigned int renderfx, md3_pose_t *out_pose)
{
    unsigned int flags;

    flags = MD3_HEURISTIC_POSE_RESET_BAD | MD3_HEURISTIC_POSE_CLAMP_FRAC;
    if ((renderfx & MD3_Q3A_RENDERFX_WRAP_FRAMES) != 0U)
    {
        flags |= MD3_HEURISTIC_POSE_WRAP_FRAMES;
    }

    return md3_runtime_sanitize_pose(decoder, pose, flags, out_pose);
}

void md3_bounds_clear(md3_bounds_t *bounds)
{
    if (bounds != 0)
    {
        md3_zero_memory(bounds, (unsigned int) sizeof(*bounds));
    }
}

void md3_bounds_from_frame(const md3_frame_t *frame, md3_bounds_t *out_bounds)
{
    if (frame == 0 || out_bounds == 0)
    {
        return;
    }

    out_bounds->mins = frame->bounds_min;
    out_bounds->maxs = frame->bounds_max;
    md3_bounds_center_from_min_max(out_bounds);
    md3_bounds_recompute_radius(out_bounds);
    if (frame->radius > out_bounds->radius)
    {
        out_bounds->radius = frame->radius;
    }
}

void md3_bounds_from_frame_authored(const md3_frame_t *frame,
    md3_bounds_t *out_bounds)
{
    if (frame == 0 || out_bounds == 0)
    {
        return;
    }

    out_bounds->mins = frame->bounds_min;
    out_bounds->maxs = frame->bounds_max;
    out_bounds->local_origin = frame->local_origin;
    out_bounds->radius = frame->radius;
}

void md3_bounds_lerp_frames(const md3_frame_t *start_frame,
    const md3_frame_t *end_frame, md3_fixed_t frac, md3_bounds_t *out_bounds)
{
    md3_fixed_t safe_frac;

    if (start_frame == 0 || end_frame == 0 || out_bounds == 0)
    {
        return;
    }

    safe_frac = md3_clamp_frac_q16(frac);
    md3_vec3_lerp(&start_frame->bounds_min, &end_frame->bounds_min,
        safe_frac, &out_bounds->mins);
    md3_vec3_lerp(&start_frame->bounds_max, &end_frame->bounds_max,
        safe_frac, &out_bounds->maxs);
    md3_vec3_lerp(&start_frame->local_origin, &end_frame->local_origin,
        safe_frac, &out_bounds->local_origin);
    out_bounds->radius = md3_fixed_lerp(start_frame->radius,
        end_frame->radius, safe_frac);
}

int md3_runtime_model_bounds_first_frame(const md3_decoder_t *decoder,
    md3_bounds_t *out_bounds)
{
    md3_frame_t frame;
    int rc;

    if (decoder == 0 || out_bounds == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_decoder_read_frame(decoder, 0, &frame);
    if (rc != MD3_OK)
    {
        return rc;
    }

    md3_bounds_from_frame_authored(&frame, out_bounds);
    return MD3_OK;
}

void md3_bounds_add_frame(md3_bounds_t *bounds, const md3_frame_t *frame)
{
    md3_fixed_t previous_radius;

    if (bounds == 0 || frame == 0)
    {
        return;
    }

    previous_radius = bounds->radius;

    if (frame->bounds_min.x < bounds->mins.x) bounds->mins.x = frame->bounds_min.x;
    if (frame->bounds_min.y < bounds->mins.y) bounds->mins.y = frame->bounds_min.y;
    if (frame->bounds_min.z < bounds->mins.z) bounds->mins.z = frame->bounds_min.z;

    if (frame->bounds_max.x > bounds->maxs.x) bounds->maxs.x = frame->bounds_max.x;
    if (frame->bounds_max.y > bounds->maxs.y) bounds->maxs.y = frame->bounds_max.y;
    if (frame->bounds_max.z > bounds->maxs.z) bounds->maxs.z = frame->bounds_max.z;

    md3_bounds_center_from_min_max(bounds);
    md3_bounds_recompute_radius(bounds);
    if (previous_radius > bounds->radius)
    {
        bounds->radius = previous_radius;
    }
    if (frame->radius > bounds->radius)
    {
        bounds->radius = frame->radius;
    }
}

int md3_runtime_pose_bounds(const md3_decoder_t *decoder,
    const md3_pose_t *pose, unsigned int pose_flags, md3_bounds_t *out_bounds)
{
    md3_pose_t safe_pose;
    md3_frame_t old_frame;
    md3_frame_t new_frame;
    int rc;

    if (decoder == 0 || pose == 0 || out_bounds == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_sanitize_pose(decoder, pose, pose_flags, &safe_pose);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_decoder_read_frame(decoder, safe_pose.old_frame, &old_frame);
    if (rc != MD3_OK)
    {
        return rc;
    }

    md3_bounds_from_frame(&old_frame, out_bounds);
    if (safe_pose.old_frame == safe_pose.frame)
    {
        return MD3_OK;
    }

    rc = md3_decoder_read_frame(decoder, safe_pose.frame, &new_frame);
    if (rc != MD3_OK)
    {
        return rc;
    }

    md3_bounds_add_frame(out_bounds, &new_frame);
    return MD3_OK;
}

int md3_runtime_pose_bounds_lerp(const md3_decoder_t *decoder,
    const md3_pose_t *pose, unsigned int pose_flags, md3_bounds_t *out_bounds)
{
    md3_pose_t safe_pose;
    md3_frame_t old_frame;
    md3_frame_t new_frame;
    int rc;

    if (decoder == 0 || pose == 0 || out_bounds == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_sanitize_pose(decoder, pose, pose_flags, &safe_pose);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_decoder_read_frame(decoder, safe_pose.old_frame, &old_frame);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (safe_pose.old_frame == safe_pose.frame || safe_pose.frac <= 0)
    {
        md3_bounds_from_frame_authored(&old_frame, out_bounds);
        return MD3_OK;
    }

    rc = md3_decoder_read_frame(decoder, safe_pose.frame, &new_frame);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (safe_pose.frac >= MD3_FIXED_ONE)
    {
        md3_bounds_from_frame_authored(&new_frame, out_bounds);
        return MD3_OK;
    }

    md3_bounds_lerp_frames(&old_frame, &new_frame, safe_pose.frac,
        out_bounds);
    return MD3_OK;
}

static md3_fixed_t md3_plane_distance_q3a(const md3_plane_t *plane,
    const md3_vec3_t *point)
{
    return md3_vec3_dot(&plane->normal, point) - plane->dist;
}

int md3_cull_local_point_radius_q3a(const md3_vec3_t *origin,
    md3_fixed_t radius, const md3_frustum_t *frustum, int *out_cull)
{
    md3_i32 i;
    md3_fixed_t distance;
    md3_fixed_t abs_radius;
    int all_inside;

    if (origin == 0 || frustum == 0 || out_cull == 0 ||
        (frustum->planes == 0 && frustum->plane_count > 0))
    {
        return MD3_EINVAL;
    }

    if (frustum->plane_count < 0)
    {
        return MD3_ERANGE;
    }

    abs_radius = md3_fixed_abs(radius);
    all_inside = 1;

    for (i = 0; i < frustum->plane_count; ++i)
    {
        distance = md3_plane_distance_q3a(&frustum->planes[i], origin);
        if (distance < -abs_radius)
        {
            *out_cull = MD3_CULL_OUT;
            return MD3_OK;
        }
        if (distance <= abs_radius)
        {
            all_inside = 0;
        }
    }

    *out_cull = all_inside ? MD3_CULL_IN : MD3_CULL_CLIP;
    return MD3_OK;
}

int md3_cull_local_bounds_q3a(const md3_bounds_t *bounds,
    const md3_frustum_t *frustum, int *out_cull)
{
    md3_i32 i;
    md3_vec3_t vmin;
    md3_vec3_t vmax;
    md3_fixed_t dmin;
    md3_fixed_t dmax;
    const md3_plane_t *plane;
    int all_inside;

    if (bounds == 0 || frustum == 0 || out_cull == 0 ||
        (frustum->planes == 0 && frustum->plane_count > 0))
    {
        return MD3_EINVAL;
    }

    if (frustum->plane_count < 0)
    {
        return MD3_ERANGE;
    }

    all_inside = 1;
    for (i = 0; i < frustum->plane_count; ++i)
    {
        plane = &frustum->planes[i];

        if (plane->normal.x >= 0)
        {
            vmin.x = bounds->mins.x;
            vmax.x = bounds->maxs.x;
        }
        else
        {
            vmin.x = bounds->maxs.x;
            vmax.x = bounds->mins.x;
        }

        if (plane->normal.y >= 0)
        {
            vmin.y = bounds->mins.y;
            vmax.y = bounds->maxs.y;
        }
        else
        {
            vmin.y = bounds->maxs.y;
            vmax.y = bounds->mins.y;
        }

        if (plane->normal.z >= 0)
        {
            vmin.z = bounds->mins.z;
            vmax.z = bounds->maxs.z;
        }
        else
        {
            vmin.z = bounds->maxs.z;
            vmax.z = bounds->mins.z;
        }

        dmax = md3_plane_distance_q3a(plane, &vmax);
        if (dmax < 0)
        {
            *out_cull = MD3_CULL_OUT;
            return MD3_OK;
        }

        dmin = md3_plane_distance_q3a(plane, &vmin);
        if (dmin < 0)
        {
            all_inside = 0;
        }
    }

    *out_cull = all_inside ? MD3_CULL_IN : MD3_CULL_CLIP;
    return MD3_OK;
}

int md3_runtime_cull_pose_q3a(const md3_decoder_t *decoder,
    const md3_pose_t *pose, unsigned int pose_flags,
    const md3_frustum_t *frustum, int non_normalized_axes, int *out_cull)
{
    md3_pose_t safe_pose;
    md3_frame_t old_frame;
    md3_frame_t new_frame;
    md3_bounds_t bounds;
    int new_cull;
    int old_cull;
    int rc;

    if (decoder == 0 || pose == 0 || frustum == 0 || out_cull == 0 ||
        decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_runtime_sanitize_pose(decoder, pose, pose_flags, &safe_pose);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_decoder_read_frame(decoder, safe_pose.frame, &new_frame);
    if (rc != MD3_OK)
    {
        return rc;
    }

    rc = md3_decoder_read_frame(decoder, safe_pose.old_frame, &old_frame);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (non_normalized_axes == 0)
    {
        rc = md3_cull_local_point_radius_q3a(&new_frame.local_origin,
            new_frame.radius, frustum, &new_cull);
        if (rc != MD3_OK)
        {
            return rc;
        }

        if (safe_pose.frame == safe_pose.old_frame)
        {
            *out_cull = new_cull;
            return MD3_OK;
        }

        rc = md3_cull_local_point_radius_q3a(&old_frame.local_origin,
            old_frame.radius, frustum, &old_cull);
        if (rc != MD3_OK)
        {
            return rc;
        }

        if (new_cull == old_cull && new_cull != MD3_CULL_CLIP)
        {
            *out_cull = new_cull;
            return MD3_OK;
        }
    }

    md3_bounds_from_frame(&old_frame, &bounds);
    if (safe_pose.old_frame != safe_pose.frame)
    {
        md3_bounds_add_frame(&bounds, &new_frame);
    }

    return md3_cull_local_bounds_q3a(&bounds, frustum, out_cull);
}

md3_fixed_t md3_q3a_project_radius(md3_fixed_t radius,
    md3_fixed_t distance, md3_fixed_t projection_y)
{
    md3_fixed_t projected;
    md3_fixed_t ratio;

    if (distance <= 0)
    {
        return 0;
    }

    ratio = md3_fixed_div(md3_fixed_abs(radius), distance);
    projected = md3_fixed_mul(ratio, projection_y);
    if (projected < 0)
    {
        projected = -projected;
    }
    if (projected > MD3_FIXED_ONE)
    {
        projected = MD3_FIXED_ONE;
    }
    return projected;
}

int md3_q3a_compute_lod(md3_i32 num_lods, md3_fixed_t projected_radius,
    md3_fixed_t lod_scale, md3_i32 lod_bias, md3_i32 *out_lod)
{
    md3_fixed_t flod_fixed;
    md3_i32 lod;

    if (out_lod == 0)
    {
        return MD3_EINVAL;
    }

    if (num_lods <= 0)
    {
        return MD3_ERANGE;
    }

    if (num_lods < 2)
    {
        lod = 0;
    }
    else
    {
        if (lod_scale > MD3_Q3A_LOD_SCALE_MAX)
        {
            lod_scale = MD3_Q3A_LOD_SCALE_MAX;
        }

        if (projected_radius != 0)
        {
            flod_fixed = MD3_FIXED_ONE -
                md3_fixed_mul(projected_radius, lod_scale);
        }
        else
        {
            flod_fixed = 0;
        }

        flod_fixed = md3_fixed_mul(flod_fixed,
            md3_fixed_from_int((int) num_lods));
        lod = (md3_i32) md3_fixed_to_int_floor(flod_fixed);

        if (lod < 0)
        {
            lod = 0;
        }
        else if (lod >= num_lods)
        {
            lod = num_lods - 1;
        }
    }

    lod += lod_bias;
    if (lod >= num_lods)
    {
        lod = num_lods - 1;
    }
    if (lod < 0)
    {
        lod = 0;
    }

    *out_lod = lod;
    return MD3_OK;
}

int md3_q3a_compute_lod_for_bounds(const md3_bounds_t *bounds,
    md3_fixed_t distance, md3_fixed_t projection_y, md3_i32 num_lods,
    md3_fixed_t lod_scale, md3_i32 lod_bias, md3_i32 *out_lod)
{
    md3_fixed_t projected_radius;

    if (bounds == 0)
    {
        return MD3_EINVAL;
    }

    projected_radius = md3_q3a_project_radius(bounds->radius, distance,
        projection_y);
    return md3_q3a_compute_lod(num_lods, projected_radius, lod_scale,
        lod_bias, out_lod);
}

int md3_decoder_build_catalog(const md3_decoder_t *decoder,
    md3_arena_t *arena, md3_catalog_t *out_catalog)
{
    md3_surface_info_t *surface_copy;
    md3_name_t *tag_names;
    md3_tag_t tags[MD3_MAX_TAGS];
    md3_i32 i;
    int rc;
    void *ptr;

    if (decoder == 0 || arena == 0 || out_catalog == 0 ||
        decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    md3_zero_memory(out_catalog, (unsigned int) sizeof(*out_catalog));
    surface_copy = 0;
    tag_names = 0;

    if (decoder->header.num_surfaces > 0)
    {
        rc = md3_arena_alloc_array(arena,
            (unsigned int) decoder->header.num_surfaces,
            (unsigned int) sizeof(md3_surface_info_t),
            (unsigned int) sizeof(md3_i32), &ptr);
        if (rc != MD3_OK)
        {
            return rc;
        }
        surface_copy = (md3_surface_info_t *) ptr;
        for (i = 0; i < decoder->header.num_surfaces; ++i)
        {
            surface_copy[i] = decoder->surfaces[i];
        }
    }

    if (decoder->header.num_tags > 0)
    {
        rc = md3_arena_alloc_array(arena,
            (unsigned int) decoder->header.num_tags,
            (unsigned int) sizeof(md3_name_t),
            (unsigned int) sizeof(md3_i32), &ptr);
        if (rc != MD3_OK)
        {
            return rc;
        }
        tag_names = (md3_name_t *) ptr;

        rc = md3_decoder_read_tags_for_frame(decoder, 0, tags, MD3_MAX_TAGS);
        if (rc != MD3_OK)
        {
            return rc;
        }

        for (i = 0; i < decoder->header.num_tags; ++i)
        {
            md3_copy_cstr_to_field(tag_names[i].name, MD3_MAX_QPATH,
                tags[i].name);
        }
    }

    out_catalog->surfaces = surface_copy;
    out_catalog->surface_count = decoder->header.num_surfaces;
    out_catalog->tag_names = tag_names;
    out_catalog->tag_count = decoder->header.num_tags;
    return MD3_OK;
}

int md3_decoder_summarize(const md3_decoder_t *decoder,
    md3_model_summary_t *out_summary)
{
    md3_surface_info_t surfaces[MD3_MAX_SURFACES];
    md3_i32 i;
    md3_i32 vertex_frames;
    int rc;

    if (decoder == 0 || out_summary == 0 || decoder->is_open == 0)
    {
        return MD3_EINVAL;
    }

    md3_zero_memory(out_summary, (unsigned int) sizeof(*out_summary));
    out_summary->frame_count = decoder->header.num_frames;
    out_summary->tag_count = decoder->header.num_tags;
    out_summary->surface_count = decoder->header.num_surfaces;

    for (i = 0; i < decoder->header.num_surfaces; ++i)
    {
        surfaces[i] = decoder->surfaces[i];

        rc = md3_checked_add_summary(out_summary->shader_count_total,
            decoder->surfaces[i].num_shaders,
            &out_summary->shader_count_total);
        if (rc != MD3_OK) return rc;

        rc = md3_checked_add_summary(out_summary->vertex_count_total,
            decoder->surfaces[i].num_verts,
            &out_summary->vertex_count_total);
        if (rc != MD3_OK) return rc;

        rc = md3_checked_add_summary(out_summary->triangle_count_total,
            decoder->surfaces[i].num_triangles,
            &out_summary->triangle_count_total);
        if (rc != MD3_OK) return rc;

        rc = md3_checked_mul_summary(decoder->surfaces[i].num_frames,
            decoder->surfaces[i].num_verts, &vertex_frames);
        if (rc != MD3_OK) return rc;

        if (decoder->surfaces[i].num_verts > out_summary->max_surface_verts)
        {
            out_summary->max_surface_verts = decoder->surfaces[i].num_verts;
        }
        if (decoder->surfaces[i].num_triangles >
            out_summary->max_surface_triangles)
        {
            out_summary->max_surface_triangles =
                decoder->surfaces[i].num_triangles;
        }
        if (vertex_frames > out_summary->max_surface_vertex_frames)
        {
            out_summary->max_surface_vertex_frames = vertex_frames;
        }
        if (decoder->surfaces[i].num_shaders <= 0)
        {
            out_summary->has_zero_shader_surface = 1;
        }
        if (decoder->surfaces[i].num_verts <= 0 ||
            decoder->surfaces[i].num_triangles <= 0)
        {
            out_summary->has_empty_surface = 1;
        }
    }

    out_summary->canonical_model_layout = md3_model_layout_is_canonical(
        &decoder->header, surfaces, decoder->header.num_surfaces);
    return MD3_OK;
}


void md3_q3a_original_render_limits(md3_q3a_limits_t *out_limits)
{
    if (out_limits == 0)
    {
        return;
    }

    out_limits->max_surface_verts = MD3_Q3A_SHADER_MAX_VERTEXES;
    out_limits->max_surface_indices = MD3_Q3A_SHADER_MAX_INDEXES;
}

void md3_daemon_q3_render_limits(md3_q3a_limits_t *out_limits)
{
    if (out_limits == 0)
    {
        return;
    }

    out_limits->max_surface_verts = MD3_DAEMON_Q3_SHADER_MAX_VERTEXES;
    out_limits->max_surface_indices = MD3_DAEMON_Q3_SHADER_MAX_INDEXES;
}

int md3_validate_surface_q3a_render_limits(
    const md3_surface_info_t *surface_info, const md3_q3a_limits_t *limits)
{
    if (surface_info == 0 || limits == 0)
    {
        return MD3_EINVAL;
    }

    if (limits->max_surface_verts > 0 &&
        surface_info->num_verts > limits->max_surface_verts)
    {
        return MD3_ERANGE;
    }

    if (limits->max_surface_indices > 0 &&
        surface_info->num_triangles > limits->max_surface_indices / 3)
    {
        return MD3_ERANGE;
    }

    return MD3_OK;
}

int md3_validate_model_q3a_render_limits(const md3_header_info_t *header_info,
    const md3_surface_info_t *surface_infos, md3_i32 surface_count,
    const md3_q3a_limits_t *limits)
{
    md3_i32 i;
    int rc;

    if (header_info == 0 || limits == 0 ||
        (surface_infos == 0 && surface_count > 0))
    {
        return MD3_EINVAL;
    }
    if (surface_count != header_info->num_surfaces)
    {
        return MD3_EINVAL;
    }

    for (i = 0; i < surface_count; ++i)
    {
        rc = md3_validate_surface_q3a_render_limits(&surface_infos[i],
            limits);
        if (rc != MD3_OK)
        {
            return rc;
        }
    }

    return MD3_OK;
}


int md3_validate_surface_q3a_tess_capacity(
    const md3_surface_info_t *surface_info,
    const md3_q3a_tess_capacity_t *capacity)
{
    if (surface_info == 0 || capacity == 0)
    {
        return MD3_EINVAL;
    }

    if (capacity->shader_max_vertexes > 0 &&
        surface_info->num_verts >= capacity->shader_max_vertexes)
    {
        return MD3_ERANGE;
    }

    if (capacity->shader_max_indexes > 0 &&
        surface_info->num_triangles >= capacity->shader_max_indexes / 3)
    {
        return MD3_ERANGE;
    }

    return MD3_OK;
}

int md3_validate_model_q3a_tess_capacity(
    const md3_header_info_t *header_info,
    const md3_surface_info_t *surface_infos, md3_i32 surface_count,
    const md3_q3a_tess_capacity_t *capacity)
{
    md3_i32 i;
    int rc;

    if (header_info == 0 || capacity == 0 ||
        (surface_infos == 0 && surface_count > 0))
    {
        return MD3_EINVAL;
    }
    if (surface_count != header_info->num_surfaces)
    {
        return MD3_EINVAL;
    }

    for (i = 0; i < surface_count; ++i)
    {
        rc = md3_validate_surface_q3a_tess_capacity(&surface_infos[i],
            capacity);
        if (rc != MD3_OK)
        {
            return rc;
        }
    }

    return MD3_OK;
}

int md3_surface_shader_index_q3a(md3_i32 shader_count, md3_i32 skin_num,
    md3_i32 *out_shader_index)
{
    if (out_shader_index == 0)
    {
        return MD3_EINVAL;
    }

    if (shader_count <= 0)
    {
        *out_shader_index = 0;
        return MD3_OK;
    }

    *out_shader_index = md3_wrap_i32(skin_num, shader_count);
    return MD3_OK;
}

static int md3_skin_parse_next_flags_q3a(const char **cursor,
    unsigned int flags, md3_skin_entry_t *out_entry)
{
    const char *p;
    char surface[MD3_MAX_QPATH];
    char shader[MD3_MAX_QPATH];
    int is_tag_entry;

    if (cursor == 0 || out_entry == 0)
    {
        return MD3_SKIN_PARSE_ERROR;
    }

    md3_zero_memory(out_entry, (unsigned int) sizeof(*out_entry));
    out_entry->shader_index = -1;

    if (*cursor == 0)
    {
        return MD3_SKIN_PARSE_EOF;
    }

    p = *cursor;
    md3_skin_skip_ws_and_comments_q3a(&p);
    if (*p == '\0')
    {
        *cursor = p;
        return MD3_SKIN_PARSE_EOF;
    }

    (void) md3_skin_parse_token_q3a(&p, surface, MD3_MAX_QPATH);
    if (surface[0] == '\0')
    {
        *cursor = p;
        return MD3_SKIN_PARSE_EOF;
    }

    md3_skin_skip_inline_ws_and_comments_q3a(&p);
    if (*p == ',')
    {
        ++p;
    }
    else
    {
        md3_skin_skip_record_tail_q3a(&p);
        *cursor = p;
        return MD3_SKIN_PARSE_SKIP;
    }

    is_tag_entry = md3_name_has_tag_prefix_q3a(surface);
    (void) md3_skin_parse_token_inline_q3a(&p, shader, MD3_MAX_QPATH);

    if (is_tag_entry != 0)
    {
        md3_q3a_canonical_surface_name(surface, out_entry->surface_name,
            MD3_MAX_QPATH);
        md3_skin_skip_record_tail_q3a(&p);
        *cursor = p;
        if ((flags & MD3_SKIN_PARSE_KEEP_TAGS) == 0U)
        {
            return MD3_SKIN_PARSE_SKIP;
        }
    }

    if (shader[0] == '\0')
    {
        md3_skin_skip_record_tail_q3a(&p);
        *cursor = p;
        return MD3_SKIN_PARSE_SKIP;
    }

    md3_q3a_canonical_surface_name(surface, out_entry->surface_name,
        MD3_MAX_QPATH);
    md3_copy_cstr_to_field(out_entry->shader_name, MD3_MAX_QPATH, shader);
    out_entry->shader_index = -1;
    md3_skin_skip_record_tail_q3a(&p);
    *cursor = p;
    return MD3_SKIN_PARSE_ENTRY;
}

int md3_skin_parse_next_q3a(const char **cursor,
    md3_skin_entry_t *out_entry)
{
    return md3_skin_parse_next_flags_q3a(cursor, 0U, out_entry);
}

int md3_skin_parse_all_q3a(const char *skin_text, md3_arena_t *arena,
    md3_i32 max_entries, unsigned int flags, md3_skin_t *out_skin)
{
    const char *cursor;
    md3_skin_entry_t entry;
    md3_skin_entry_t *entries;
    md3_i32 count;
    int parse_rc;
    int final_rc;
    void *ptr;
    int rc;

    if (skin_text == 0 || out_skin == 0 || max_entries < 0 ||
        (arena == 0 && max_entries > 0))
    {
        return MD3_EINVAL;
    }

    md3_zero_memory(out_skin, (unsigned int) sizeof(*out_skin));
    entries = 0;
    if (max_entries > 0)
    {
        rc = md3_arena_alloc_array(arena, (unsigned int) max_entries,
            (unsigned int) sizeof(md3_skin_entry_t),
            (unsigned int) sizeof(md3_i32), &ptr);
        if (rc != MD3_OK)
        {
            return rc;
        }
        entries = (md3_skin_entry_t *) ptr;
    }

    cursor = skin_text;
    count = 0;
    final_rc = MD3_OK;
    for (;;)
    {
        parse_rc = md3_skin_parse_next_flags_q3a(&cursor, flags, &entry);
        if (parse_rc == MD3_SKIN_PARSE_EOF)
        {
            break;
        }
        if (parse_rc == MD3_SKIN_PARSE_ERROR)
        {
            return MD3_EFORMAT;
        }
        if (parse_rc == MD3_SKIN_PARSE_SKIP)
        {
            if (md3_name_has_tag_prefix_q3a(entry.surface_name) != 0)
            {
                out_skin->ignored_tag_entries++;
            }
            continue;
        }
        if (parse_rc == MD3_SKIN_PARSE_ENTRY)
        {
            if (count < max_entries)
            {
                entries[count] = entry;
                ++count;
            }
            else
            {
                out_skin->had_truncation = 1;
                final_rc = MD3_ENOSPACE;
            }
        }
    }

    out_skin->entries = entries;
    out_skin->entry_count = count;
    return final_rc;
}

int md3_skin_find_entry_q3a(const md3_skin_entry_t *entries,
    md3_i32 entry_count, const char *surface_name, md3_i32 *out_index)
{
    md3_i32 i;

    if ((entries == 0 && entry_count > 0) || surface_name == 0 ||
        out_index == 0)
    {
        return MD3_EINVAL;
    }

    if (entry_count < 0)
    {
        return MD3_ERANGE;
    }

    for (i = 0; i < entry_count; ++i)
    {
        if (md3_q3a_surface_name_equal(entries[i].surface_name,
            surface_name))
        {
            *out_index = i;
            return MD3_OK;
        }
    }

    return MD3_EFORMAT;
}


int md3_skin_find_shader_q3a(const md3_skin_t *skin,
    const char *surface_name, char *out_shader_name,
    unsigned int out_shader_name_size, md3_i32 *out_shader_index)
{
    md3_i32 entry_index;
    int rc;

    if (skin == 0 || surface_name == 0 ||
        (out_shader_name == 0 && out_shader_name_size > 0U))
    {
        return MD3_EINVAL;
    }

    rc = md3_skin_find_entry_q3a(skin->entries, skin->entry_count,
        surface_name, &entry_index);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (out_shader_name != 0 && out_shader_name_size > 0U)
    {
        md3_copy_cstr_to_field(out_shader_name, out_shader_name_size,
            skin->entries[entry_index].shader_name);
    }
    if (out_shader_index != 0)
    {
        *out_shader_index = skin->entries[entry_index].shader_index;
    }

    return MD3_OK;
}

int md3_skin_find_shader_name_q3a(const md3_skin_entry_t *entries,
    md3_i32 entry_count, const char *surface_name, char *out_shader_name,
    unsigned int out_shader_size)
{
    md3_i32 entry_index;
    int rc;

    if ((entries == 0 && entry_count > 0) || surface_name == 0 ||
        out_shader_name == 0 || out_shader_size == 0U)
    {
        return MD3_EINVAL;
    }

    rc = md3_skin_find_entry_q3a(entries, entry_count, surface_name,
        &entry_index);
    if (rc != MD3_OK)
    {
        return rc;
    }

    md3_copy_cstr_to_field(out_shader_name, out_shader_size,
        entries[entry_index].shader_name);
    return MD3_OK;
}

int md3_skin_select_shader_q3a(const md3_surface_info_t *surface_info,
    const md3_skin_entry_t *entries, md3_i32 entry_count,
    md3_i32 default_shader_index, md3_i32 *out_shader_index)
{
    md3_i32 entry_index;
    int rc;

    if (surface_info == 0 || out_shader_index == 0 ||
        (entries == 0 && entry_count > 0))
    {
        return MD3_EINVAL;
    }

    rc = md3_skin_find_entry_q3a(entries, entry_count, surface_info->name,
        &entry_index);
    if (rc == MD3_OK)
    {
        *out_shader_index = entries[entry_index].shader_index;
        return MD3_OK;
    }
    if (rc == MD3_EFORMAT)
    {
        *out_shader_index = default_shader_index;
        return MD3_OK;
    }

    return rc;
}


int md3_skin_resolve_catalog_shader_names_q3a(const md3_catalog_t *catalog,
    const md3_skin_entry_t *entries, md3_i32 entry_count,
    md3_name_t *out_shader_names, md3_i32 shader_name_capacity,
    md3_i32 *out_resolved_count)
{
    md3_i32 i;
    md3_i32 resolved_count;
    int rc;

    if (catalog == 0 || out_resolved_count == 0 ||
        (entries == 0 && entry_count > 0) ||
        (out_shader_names == 0 && shader_name_capacity > 0))
    {
        return MD3_EINVAL;
    }

    if (catalog->surface_count < 0 || entry_count < 0 ||
        shader_name_capacity < catalog->surface_count)
    {
        return MD3_ERANGE;
    }

    resolved_count = 0;
    for (i = 0; i < catalog->surface_count; ++i)
    {
        out_shader_names[i].name[0] = '\0';
        rc = md3_skin_find_shader_name_q3a(entries, entry_count,
            catalog->surfaces[i].name, out_shader_names[i].name,
            MD3_MAX_QPATH);
        if (rc == MD3_OK)
        {
            ++resolved_count;
        }
        else if (rc != MD3_EFORMAT)
        {
            return rc;
        }
    }

    *out_resolved_count = resolved_count;
    return MD3_OK;
}
