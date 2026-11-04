#ifndef MD3_HEURISTIC_H
#define MD3_HEURISTIC_H

#include "md3_runtime.h"
#include "md3_arena.h"

#define MD3_HEURISTIC_POSE_CLAMP_FRAMES 0x0001U
#define MD3_HEURISTIC_POSE_WRAP_FRAMES  0x0002U
#define MD3_HEURISTIC_POSE_CLAMP_FRAC   0x0004U
#define MD3_HEURISTIC_POSE_RESET_BAD     0x0008U
#define MD3_HEURISTIC_POSE_Q3A_DEFAULT  \
    (MD3_HEURISTIC_POSE_CLAMP_FRAMES | MD3_HEURISTIC_POSE_CLAMP_FRAC)
#define MD3_Q3A_RENDERFX_WRAP_FRAMES 0x0200U
#define MD3_Q3A_LOD_SCALE_MAX (MD3_FIXED_ONE * 20)
#define MD3_Q3A_SHADER_MAX_VERTEXES 1000
#define MD3_Q3A_SHADER_MAX_INDEXES 6000
#define MD3_DAEMON_Q3_SHADER_MAX_VERTEXES 10000
#define MD3_DAEMON_Q3_SHADER_MAX_INDEXES 60000

#define MD3_CULL_OUT  0
#define MD3_CULL_CLIP 1
#define MD3_CULL_IN   2

#define MD3_SKIN_PARSE_ERROR (-1)
#define MD3_SKIN_PARSE_EOF   0
#define MD3_SKIN_PARSE_ENTRY 1
#define MD3_SKIN_PARSE_SKIP  2

#define MD3_SKIN_PARSE_KEEP_TAGS 0x0001U

typedef struct md3_name_s
{
    char name[MD3_MAX_QPATH];
} md3_name_t;

typedef struct md3_catalog_s
{
    const md3_surface_info_t *surfaces;
    md3_i32 surface_count;
    const md3_name_t *tag_names;
    md3_i32 tag_count;
} md3_catalog_t;

typedef struct md3_model_summary_s
{
    md3_i32 frame_count;
    md3_i32 tag_count;
    md3_i32 surface_count;
    md3_i32 shader_count_total;
    md3_i32 vertex_count_total;
    md3_i32 triangle_count_total;
    md3_i32 max_surface_verts;
    md3_i32 max_surface_triangles;
    md3_i32 max_surface_vertex_frames;
    int canonical_model_layout;
    int has_zero_shader_surface;
    int has_empty_surface;
} md3_model_summary_t;

typedef struct md3_q3a_limits_s
{
    md3_i32 max_surface_verts;
    md3_i32 max_surface_indices;
} md3_q3a_limits_t;

typedef struct md3_q3a_tess_capacity_s
{
    md3_i32 shader_max_vertexes;
    md3_i32 shader_max_indexes;
} md3_q3a_tess_capacity_t;

typedef struct md3_bounds_s
{
    md3_vec3_t mins;
    md3_vec3_t maxs;
    md3_vec3_t local_origin;
    md3_fixed_t radius;
} md3_bounds_t;

typedef struct md3_plane_s
{
    md3_vec3_t normal;
    md3_fixed_t dist;
} md3_plane_t;

typedef struct md3_frustum_s
{
    const md3_plane_t *planes;
    md3_i32 plane_count;
} md3_frustum_t;

typedef struct md3_skin_entry_s
{
    char surface_name[MD3_MAX_QPATH];
    char shader_name[MD3_MAX_QPATH];
    md3_i32 shader_index;
} md3_skin_entry_t;

typedef struct md3_skin_s
{
    const md3_skin_entry_t *entries;
    md3_i32 entry_count;
    int had_truncation;
    int ignored_tag_entries;
} md3_skin_t;

void md3_q3a_canonical_surface_name(const char *src, char *dst,
    unsigned int dst_size);
int md3_q3a_surface_name_equal(const char *a, const char *b);
int md3_decoder_find_surface_index_q3a(const md3_decoder_t *decoder,
    const char *surface_name, md3_i32 *out_index);
int md3_decoder_find_tag_index_from_q3a(const md3_decoder_t *decoder,
    const char *tag_name, md3_i32 start_index, md3_i32 *out_index);

void md3_pose_from_q3a_backlerp(md3_pose_t *pose, md3_i32 old_frame,
    md3_i32 frame, md3_fixed_t backlerp);
int md3_runtime_sanitize_pose(const md3_decoder_t *decoder,
    const md3_pose_t *pose, unsigned int flags, md3_pose_t *out_pose);
int md3_runtime_sanitize_pose_q3a_renderfx(const md3_decoder_t *decoder,
    const md3_pose_t *pose, unsigned int renderfx, md3_pose_t *out_pose);
int md3_runtime_find_tag_index_from(const md3_decoder_t *decoder,
    const char *tag_name, md3_i32 start_index, md3_i32 *out_index);
int md3_runtime_find_tag_index_cached(const md3_decoder_t *decoder,
    const char *tag_name, md3_i32 *io_tag_index);

void md3_bounds_clear(md3_bounds_t *bounds);
void md3_bounds_from_frame(const md3_frame_t *frame, md3_bounds_t *out_bounds);
void md3_bounds_from_frame_authored(const md3_frame_t *frame,
    md3_bounds_t *out_bounds);
void md3_bounds_add_frame(md3_bounds_t *bounds, const md3_frame_t *frame);
void md3_bounds_lerp_frames(const md3_frame_t *start_frame,
    const md3_frame_t *end_frame, md3_fixed_t frac, md3_bounds_t *out_bounds);
int md3_runtime_model_bounds_first_frame(const md3_decoder_t *decoder,
    md3_bounds_t *out_bounds);
int md3_runtime_pose_bounds(const md3_decoder_t *decoder,
    const md3_pose_t *pose, unsigned int pose_flags, md3_bounds_t *out_bounds);
int md3_runtime_pose_bounds_lerp(const md3_decoder_t *decoder,
    const md3_pose_t *pose, unsigned int pose_flags, md3_bounds_t *out_bounds);
int md3_cull_local_point_radius_q3a(const md3_vec3_t *origin,
    md3_fixed_t radius, const md3_frustum_t *frustum, int *out_cull);
int md3_cull_local_bounds_q3a(const md3_bounds_t *bounds,
    const md3_frustum_t *frustum, int *out_cull);
int md3_runtime_cull_pose_q3a(const md3_decoder_t *decoder,
    const md3_pose_t *pose, unsigned int pose_flags,
    const md3_frustum_t *frustum, int non_normalized_axes, int *out_cull);

md3_fixed_t md3_q3a_project_radius(md3_fixed_t radius,
    md3_fixed_t distance, md3_fixed_t projection_y);
int md3_q3a_compute_lod(md3_i32 num_lods, md3_fixed_t projected_radius,
    md3_fixed_t lod_scale, md3_i32 lod_bias, md3_i32 *out_lod);
int md3_q3a_compute_lod_for_bounds(const md3_bounds_t *bounds,
    md3_fixed_t distance, md3_fixed_t projection_y, md3_i32 num_lods,
    md3_fixed_t lod_scale, md3_i32 lod_bias, md3_i32 *out_lod);

int md3_decoder_build_catalog(const md3_decoder_t *decoder,
    md3_arena_t *arena, md3_catalog_t *out_catalog);
int md3_decoder_summarize(const md3_decoder_t *decoder,
    md3_model_summary_t *out_summary);
void md3_q3a_original_render_limits(md3_q3a_limits_t *out_limits);
void md3_daemon_q3_render_limits(md3_q3a_limits_t *out_limits);
int md3_validate_surface_q3a_render_limits(
    const md3_surface_info_t *surface_info, const md3_q3a_limits_t *limits);
int md3_validate_model_q3a_render_limits(const md3_header_info_t *header_info,
    const md3_surface_info_t *surface_infos, md3_i32 surface_count,
    const md3_q3a_limits_t *limits);
int md3_validate_surface_q3a_tess_capacity(
    const md3_surface_info_t *surface_info,
    const md3_q3a_tess_capacity_t *capacity);
int md3_validate_model_q3a_tess_capacity(
    const md3_header_info_t *header_info,
    const md3_surface_info_t *surface_infos, md3_i32 surface_count,
    const md3_q3a_tess_capacity_t *capacity);
int md3_surface_shader_index_q3a(md3_i32 shader_count, md3_i32 skin_num,
    md3_i32 *out_shader_index);
int md3_skin_parse_next_q3a(const char **cursor,
    md3_skin_entry_t *out_entry);
int md3_skin_parse_all_q3a(const char *skin_text, md3_arena_t *arena,
    md3_i32 max_entries, unsigned int flags, md3_skin_t *out_skin);
int md3_skin_find_entry_q3a(const md3_skin_entry_t *entries,
    md3_i32 entry_count, const char *surface_name, md3_i32 *out_index);
int md3_skin_find_shader_q3a(const md3_skin_t *skin,
    const char *surface_name, char *out_shader_name,
    unsigned int out_shader_name_size, md3_i32 *out_shader_index);
int md3_skin_find_shader_name_q3a(const md3_skin_entry_t *entries,
    md3_i32 entry_count, const char *surface_name, char *out_shader_name,
    unsigned int out_shader_size);
int md3_skin_select_shader_q3a(const md3_surface_info_t *surface_info,
    const md3_skin_entry_t *entries, md3_i32 entry_count,
    md3_i32 default_shader_index, md3_i32 *out_shader_index);
int md3_skin_resolve_catalog_shader_names_q3a(const md3_catalog_t *catalog,
    const md3_skin_entry_t *entries, md3_i32 entry_count,
    md3_name_t *out_shader_names, md3_i32 shader_name_capacity,
    md3_i32 *out_resolved_count);

#endif
