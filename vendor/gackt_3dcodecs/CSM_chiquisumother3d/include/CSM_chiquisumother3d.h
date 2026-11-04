#ifndef CSM_CHIQUISUMOTHER3D_H
#define CSM_CHIQUISUMOTHER3D_H

/*
   CSM_chiquisumother3d
   Small C89 Character Studio Marker (.csm) codec.

   Core rules:
   - no dynamic allocation calls
   - no C decimal hardware types
   - fixed-point coordinates: signed long scaled by 1000
   - caller-owned/static storage only
*/

#ifdef __cplusplus
extern "C" {
#endif

#define CSM3D_VERSION_MAJOR 0
#define CSM3D_VERSION_MINOR 6
#define CSM3D_VERSION_PATCH 0

#ifndef CSM3D_MAX_MARKERS
#define CSM3D_MAX_MARKERS 64
#endif

#ifndef CSM3D_MAX_FRAMES
#define CSM3D_MAX_FRAMES 4096
#endif

#ifndef CSM3D_MAX_NAME
#define CSM3D_MAX_NAME 32
#endif

#ifndef CSM3D_MAX_META
#define CSM3D_MAX_META 128
#endif

#ifndef CSM3D_MAX_DIAG
#define CSM3D_MAX_DIAG 160
#endif

#ifndef CSM3D_MAX_MNM_ENTRIES
#define CSM3D_MAX_MNM_ENTRIES 128
#endif

#ifndef CSM3D_MAX_COVERAGE_MISSING
#define CSM3D_MAX_COVERAGE_MISSING 48
#endif

#define CSM3D_FX_SCALE 1000L

#define CSM3D_ANALYSIS_DECLARED_MARKER_MISMATCH 0x00000001UL
#define CSM3D_ANALYSIS_DUPLICATE_MARKERS        0x00000002UL
#define CSM3D_ANALYSIS_NON_MONOTONIC_FRAMES    0x00000004UL
#define CSM3D_ANALYSIS_DECLARED_RANGE_MISMATCH 0x00000008UL
#define CSM3D_ANALYSIS_LEFT_PROP_PRESENT       0x00000010UL
#define CSM3D_ANALYSIS_MIDDLE_PROP_PRESENT     0x00000020UL
#define CSM3D_ANALYSIS_RIGHT_PROP_PRESENT      0x00000040UL
#define CSM3D_ANALYSIS_UNKNOWN_MARKERS         0x00000080UL
#define CSM3D_ANALYSIS_DUPLICATE_FRAMES        0x00000100UL
#define CSM3D_ANALYSIS_INCOMPLETE_PROP_MARKERS 0x00000200UL
#define CSM3D_ANALYSIS_EMPTY_DECLARED_RANGE    0x00000400UL
#define CSM3D_ANALYSIS_SPARSE_STANDARD_MARKERS 0x00000800UL

#define CSM3D_PROP_LEFT   0
#define CSM3D_PROP_MIDDLE 1
#define CSM3D_PROP_RIGHT  2

#define CSM3D_MARKER_UNKNOWN       0
#define CSM3D_MARKER_STANDARD      1
#define CSM3D_MARKER_PROP_LEFT     2
#define CSM3D_MARKER_PROP_MIDDLE   3
#define CSM3D_MARKER_PROP_RIGHT    4

#define CSM3D_AXIS_X 0
#define CSM3D_AXIS_Y 1
#define CSM3D_AXIS_Z 2

#define CSM3D_MNM_DIR_SOURCE_TO_TARGET 0
#define CSM3D_MNM_DIR_TARGET_TO_SOURCE 1

typedef signed long csm3d_i32;
typedef unsigned long csm3d_u32;

typedef enum CSM3D_ResultTag {
    CSM3D_OK = 0,
    CSM3D_ERR_NULL = -1,
    CSM3D_ERR_BAD_TOKEN = -2,
    CSM3D_ERR_BAD_NUMBER = -3,
    CSM3D_ERR_TOO_MANY_MARKERS = -4,
    CSM3D_ERR_TOO_MANY_FRAMES = -5,
    CSM3D_ERR_NO_ORDER = -6,
    CSM3D_ERR_NO_POINTS = -7,
    CSM3D_ERR_TRUNCATED_POINTS = -8,
    CSM3D_ERR_OUTPUT_FULL = -9,
    CSM3D_ERR_INDEX = -10,
    CSM3D_ERR_INVALID_DOC = -11,
    CSM3D_ERR_DUPLICATE_MARKER = -12,
    CSM3D_ERR_BAD_PROP = -13,
    CSM3D_ERR_DIV_ZERO = -14,
    CSM3D_ERR_TOO_MANY_MAP_ENTRIES = -15,
    CSM3D_ERR_BAD_MNM = -16,
    CSM3D_ERR_BAD_AXIS = -17,
    CSM3D_ERR_BAD_RANGE = -18
} CSM3D_Result;

typedef struct CSM3D_Vec3Tag {
    csm3d_i32 x;
    csm3d_i32 y;
    csm3d_i32 z;
} CSM3D_Vec3;

typedef struct CSM3D_FrameTag {
    int frame_number;
    CSM3D_Vec3 point[CSM3D_MAX_MARKERS];
} CSM3D_Frame;

typedef struct CSM3D_DocTag {
    char date[CSM3D_MAX_META];
    char time[CSM3D_MAX_META];
    char filename[CSM3D_MAX_META];
    char actor[CSM3D_MAX_META];
    char comments[CSM3D_MAX_META];

    int first_frame;
    int last_frame;
    int spine_links;
    int rate;
    int declared_marker_count;

    int marker_count;
    char marker_name[CSM3D_MAX_MARKERS][CSM3D_MAX_NAME];

    int frame_count;
    CSM3D_Frame frame[CSM3D_MAX_FRAMES];
} CSM3D_Doc;

typedef struct CSM3D_DiagnosticTag {
    int code;
    int line;
    int marker_index;
    int frame_index;
    char text[CSM3D_MAX_DIAG];
} CSM3D_Diagnostic;

typedef struct CSM3D_BoundsTag {
    CSM3D_Vec3 min_point;
    CSM3D_Vec3 max_point;
} CSM3D_Bounds;

typedef struct CSM3D_PropInfoTag {
    int present;
    int bottom_index;
    int middle_index;
    int top_index;
    csm3d_i32 approximate_average_length;
    csm3d_i32 approximate_min_length;
    csm3d_i32 approximate_max_length;
    CSM3D_Bounds bounds;
} CSM3D_PropInfo;

typedef struct CSM3D_AnalysisTag {
    csm3d_u32 flags;
    int duplicate_marker_a;
    int duplicate_marker_b;
    int frame_order_break_index;
    int duplicate_frame_a;
    int duplicate_frame_b;
    int unknown_marker_count;
    int incomplete_prop_marker_count;
    int first_data_frame;
    int last_data_frame;
    int declared_frame_span;
} CSM3D_Analysis;

typedef struct CSM3D_MarkerMapEntryTag {
    char source_name[CSM3D_MAX_NAME];
    char target_name[CSM3D_MAX_NAME];
} CSM3D_MarkerMapEntry;

typedef struct CSM3D_MarkerMapTag {
    int entry_count;
    CSM3D_MarkerMapEntry entry[CSM3D_MAX_MNM_ENTRIES];
} CSM3D_MarkerMap;

typedef struct CSM3D_SpikeTag {
    int present;
    int frame_index;
    int marker_index;
    csm3d_i32 approximate_speed;
} CSM3D_Spike;

typedef struct CSM3D_MarkerStatsTag {
    int marker_index;
    CSM3D_Bounds bounds;
    CSM3D_Vec3 first_point;
    CSM3D_Vec3 last_point;
    csm3d_i32 approximate_path_length;
    csm3d_i32 approximate_max_speed;
    int max_speed_frame_index;
} CSM3D_MarkerStats;

typedef struct CSM3D_MarkerCoverageTag {
    int standard_marker_total;
    int present_standard_count;
    int present_prop_count;
    int unknown_count;
    int missing_standard_count;
    char missing_standard[CSM3D_MAX_COVERAGE_MISSING][CSM3D_MAX_NAME];
} CSM3D_MarkerCoverage;


typedef struct CSM3D_PropPoseTag {
    int present;
    int prop_side;
    int frame_index;
    CSM3D_Vec3 origin;
    CSM3D_Vec3 bottom_to_middle;
    CSM3D_Vec3 bottom_to_top;
    CSM3D_Vec3 rough_plane_normal;
    csm3d_i32 approximate_length;
} CSM3D_PropPose;

typedef struct CSM3D_MotionQualityTag {
    int marker_count;
    int stationary_marker_count;
    int moving_marker_count;
    int high_speed_marker_count;
    csm3d_i32 total_path_length;
    csm3d_i32 max_path_length;
    int max_path_marker_index;
    csm3d_i32 max_marker_speed;
    int max_speed_marker_index;
    int max_speed_frame_index;
} CSM3D_MotionQuality;


void csm3d_doc_init(CSM3D_Doc *doc);
void csm3d_diag_init(CSM3D_Diagnostic *diag);
void csm3d_analysis_init(CSM3D_Analysis *analysis);
const char *csm3d_error_string(int code);

int csm3d_is_probably_csm(const char *text, csm3d_u32 text_len, int *out_score);
int csm3d_decode(const char *text, csm3d_u32 text_len, CSM3D_Doc *doc, CSM3D_Diagnostic *diag);
int csm3d_encode(const CSM3D_Doc *doc, char *out_text, csm3d_u32 out_cap, csm3d_u32 *out_len, CSM3D_Diagnostic *diag);
int csm3d_validate(const CSM3D_Doc *doc, CSM3D_Diagnostic *diag);
int csm3d_analyze(const CSM3D_Doc *doc, CSM3D_Analysis *analysis);

int csm3d_marker_index(const CSM3D_Doc *doc, const char *marker_name);
int csm3d_get_point(const CSM3D_Doc *doc, int frame_index, int marker_index, CSM3D_Vec3 *out_point);
int csm3d_set_point(CSM3D_Doc *doc, int frame_index, int marker_index, CSM3D_Vec3 point);
int csm3d_compute_bounds(const CSM3D_Doc *doc, CSM3D_Bounds *bounds);
int csm3d_compute_frame_bounds(const CSM3D_Doc *doc, int frame_index, CSM3D_Bounds *bounds);
int csm3d_compute_velocity(const CSM3D_Doc *doc, int frame_index, int marker_index, CSM3D_Vec3 *out_units_per_second);
int csm3d_compute_frame_center(const CSM3D_Doc *doc, int frame_index, CSM3D_Vec3 *out_center);
int csm3d_compute_marker_stats(const CSM3D_Doc *doc, int marker_index, CSM3D_MarkerStats *out_stats);
int csm3d_find_frame_index(const CSM3D_Doc *doc, int frame_number);
int csm3d_interpolate_frame(const CSM3D_Doc *doc, int frame_number, CSM3D_Frame *out_frame);
int csm3d_keep_every_nth_frame(CSM3D_Doc *doc, int step, int keep_last);
int csm3d_sort_frames(CSM3D_Doc *doc);
int csm3d_fill_missing_frames(CSM3D_Doc *doc);
int csm3d_detect_spike(const CSM3D_Doc *doc, csm3d_i32 max_units_per_second, CSM3D_Spike *out_spike);
int csm3d_despike_linear(CSM3D_Doc *doc, csm3d_i32 max_units_per_second, int *out_replaced_count);
int csm3d_remove_duplicate_frames(CSM3D_Doc *doc, int keep_last);
int csm3d_update_declared_metadata(CSM3D_Doc *doc);
int csm3d_normalize_marker_names_upper(CSM3D_Doc *doc);
int csm3d_repair_basic(CSM3D_Doc *doc);
int csm3d_crop_frame_range(CSM3D_Doc *doc, int first_frame, int last_frame);
int csm3d_offset_frame_numbers(CSM3D_Doc *doc, int delta);
int csm3d_renumber_frames(CSM3D_Doc *doc, int first_frame, int step);

int csm3d_standard_marker_count(void);
const char *csm3d_standard_marker_name(int index);
int csm3d_marker_kind_from_name(const char *marker_name);
int csm3d_marker_is_known_standard_name(const char *marker_name);
int csm3d_marker_is_prop_name(const char *marker_name);
const char *csm3d_marker_kind_string(int marker_kind);
const char *csm3d_prop_side_name(int prop_side);
int csm3d_find_prop_markers(const CSM3D_Doc *doc, int prop_side, int *bottom_index, int *middle_index, int *top_index);
int csm3d_compute_prop_info(const CSM3D_Doc *doc, int prop_side, CSM3D_PropInfo *info);
int csm3d_compute_marker_coverage(const CSM3D_Doc *doc, CSM3D_MarkerCoverage *out_coverage);
int csm3d_compute_prop_pose_at_frame(const CSM3D_Doc *doc, int prop_side, int frame_index, CSM3D_PropPose *out_pose);
int csm3d_compute_motion_quality(const CSM3D_Doc *doc, csm3d_i32 stationary_epsilon, csm3d_i32 high_speed_threshold, CSM3D_MotionQuality *out_quality);
int csm3d_reorder_markers_to_standard(CSM3D_Doc *doc);

int csm3d_translate(CSM3D_Doc *doc, CSM3D_Vec3 delta);
int csm3d_scale_uniform(CSM3D_Doc *doc, csm3d_i32 scale_fx);
int csm3d_recenter_to_origin(CSM3D_Doc *doc);
int csm3d_flip_axis(CSM3D_Doc *doc, int axis);
int csm3d_convert_up_axis(CSM3D_Doc *doc, int from_axis, int to_axis);

csm3d_i32 csm3d_fx_from_parts(int sign, csm3d_i32 whole, csm3d_i32 milli);
int csm3d_fx_parse(const char *token, csm3d_i32 *out_value);
csm3d_u32 csm3d_fx_to_text(csm3d_i32 value, char *out_text, csm3d_u32 out_cap);
csm3d_i32 csm3d_fx_mul(csm3d_i32 a, csm3d_i32 b);
csm3d_i32 csm3d_vec3_approx_length(CSM3D_Vec3 v);

void csm3d_marker_map_init(CSM3D_MarkerMap *map);
int csm3d_marker_map_add(CSM3D_MarkerMap *map, const char *source_name, const char *target_name, CSM3D_Diagnostic *diag);
int csm3d_mnm_decode(const char *text, csm3d_u32 text_len, CSM3D_MarkerMap *map, CSM3D_Diagnostic *diag);
int csm3d_marker_map_lookup(const CSM3D_MarkerMap *map, const char *source_name, char *out_target_name, csm3d_u32 out_cap);
int csm3d_marker_map_lookup_inverse(const CSM3D_MarkerMap *map, const char *target_name, char *out_source_name, csm3d_u32 out_cap);
int csm3d_apply_marker_map(CSM3D_Doc *doc, const CSM3D_MarkerMap *map, CSM3D_Diagnostic *diag);
int csm3d_apply_marker_map_inverse(CSM3D_Doc *doc, const CSM3D_MarkerMap *map, CSM3D_Diagnostic *diag);
int csm3d_apply_marker_map_best(CSM3D_Doc *doc, const CSM3D_MarkerMap *map, int *out_direction, CSM3D_Diagnostic *diag);
int csm3d_mnm_encode(const CSM3D_MarkerMap *map, char *out_text, csm3d_u32 out_cap, csm3d_u32 *out_len, CSM3D_Diagnostic *diag);

#ifdef __cplusplus
}
#endif

#endif
