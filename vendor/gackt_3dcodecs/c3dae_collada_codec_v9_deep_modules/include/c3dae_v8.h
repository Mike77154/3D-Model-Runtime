#ifndef C3DAE_V8_H
#define C3DAE_V8_H

/*
   c3dae_v8 - optional COLLADA deep coverage scanner/converter helpers.
   C89, no heap, caller-owned buffers. This module adds the next logical
   pass after c3dae_v7: lookat/skew transform matrices, source-array catalog,
   evaluate_scene/render passes, embedded images, B-rep/spline routing and
   lightweight lint warnings for pipeline converters.
*/

#include "c3dae.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef C3DAE_V8_TEXT_MAX
#define C3DAE_V8_TEXT_MAX 160
#endif

#define C3DAE_V8_OK              0
#define C3DAE_V8_ERR_NULL       -1
#define C3DAE_V8_ERR_XML        -2

#define C3DAE_V8_TRANS_LOOKAT    1
#define C3DAE_V8_TRANS_SKEW      2

#define C3DAE_V8_ARRAY_UNKNOWN   0
#define C3DAE_V8_ARRAY_FLOAT     1
#define C3DAE_V8_ARRAY_NAME      2
#define C3DAE_V8_ARRAY_IDREF     3
#define C3DAE_V8_ARRAY_SIDREF    4
#define C3DAE_V8_ARRAY_BOOL      5
#define C3DAE_V8_ARRAY_INT       6

#define C3DAE_V8_WARN_NO_ASSET          1
#define C3DAE_V8_WARN_NO_SCENE          2
#define C3DAE_V8_WARN_SHADER_PROFILE    3
#define C3DAE_V8_WARN_BREP_ROUTE        4
#define C3DAE_V8_WARN_SPLINE_ROUTE      5
#define C3DAE_V8_WARN_POLYGON_HOLES     6
#define C3DAE_V8_WARN_EMBEDDED_IMAGE    7
#define C3DAE_V8_WARN_FORMULA_MATHML    8

#define C3DAE_V8_IMAGE_EXTERNAL  1U
#define C3DAE_V8_IMAGE_EMBEDDED  2U

#define C3DAE_V8_BREP_HAS_CURVES    1U
#define C3DAE_V8_BREP_HAS_SURFACES  2U
#define C3DAE_V8_BREP_HAS_TOPOLOGY  4U
#define C3DAE_V8_BREP_HAS_SOLIDS    8U

#define C3DAE_V8_SPLINE_OPEN        0
#define C3DAE_V8_SPLINE_CLOSED      1
#define C3DAE_V8_SPLINE_UNKNOWN     2

typedef struct c3dae_v8_transform_tag {
    char node_id[C3DAE_ID_MAX];
    char node_sid[C3DAE_ID_MAX];
    char node_name[C3DAE_ID_MAX];
    char sid[C3DAE_ID_MAX];
    int kind;
    c3dae_fixed raw[9];
    c3dae_fixed matrix[16];
} c3dae_v8_transform;

typedef struct c3dae_v8_source_info_tag {
    char source_id[C3DAE_ID_MAX];
    char array_id[C3DAE_ID_MAX];
    int array_kind;
    c3dae_u32 count;
    c3dae_u32 stride;
    c3dae_u32 offset;
    c3dae_u32 param_count;
    char first_param[C3DAE_ID_MAX];
} c3dae_v8_source_info;

typedef struct c3dae_v8_render_pass_tag {
    char visual_scene[C3DAE_ID_MAX];
    char evaluate_name[C3DAE_ID_MAX];
    char render_sid[C3DAE_ID_MAX];
    char camera_node[C3DAE_ID_MAX];
    char layer[C3DAE_ID_MAX];
} c3dae_v8_render_pass;

typedef struct c3dae_v8_image_info_tag {
    char image_id[C3DAE_ID_MAX];
    char path[C3DAE_PATH_MAX];
    char format[C3DAE_ID_MAX];
    unsigned int flags;
    c3dae_u32 embedded_bytes;
} c3dae_v8_image_info;

typedef struct c3dae_v8_brep_info_tag {
    char geometry_id[C3DAE_ID_MAX];
    c3dae_u32 source_count;
    c3dae_u32 curve_count;
    c3dae_u32 surface_count;
    c3dae_u32 edge_count;
    c3dae_u32 wire_count;
    c3dae_u32 face_count;
    c3dae_u32 shell_count;
    c3dae_u32 solid_count;
    unsigned int flags;
} c3dae_v8_brep_info;

typedef struct c3dae_v8_spline_info_tag {
    char geometry_id[C3DAE_ID_MAX];
    c3dae_u32 source_count;
    c3dae_u32 control_vertex_input_count;
    int closed;
    c3dae_u32 degree;
} c3dae_v8_spline_info;

typedef struct c3dae_v8_issue_tag {
    int code;
    char where_id[C3DAE_ID_MAX];
    char message[C3DAE_V8_TEXT_MAX];
} c3dae_v8_issue;

typedef struct c3dae_v8_stats_tag {
    c3dae_u32 lookat_count;
    c3dae_u32 skew_count;
    c3dae_u32 source_count;
    c3dae_u32 render_pass_count;
    c3dae_u32 embedded_image_count;
    c3dae_u32 external_image_count;
    c3dae_u32 brep_count;
    c3dae_u32 spline_count;
    c3dae_u32 mathml_count;
    c3dae_u32 unknown_profile_count;
    c3dae_u32 truncated_count;
} c3dae_v8_stats;

typedef struct c3dae_v8_document_tag {
    c3dae_v8_stats stats;

    c3dae_v8_transform *transforms;
    c3dae_u32 transform_cap;
    c3dae_u32 transform_count;

    c3dae_v8_source_info *sources;
    c3dae_u32 source_cap;
    c3dae_u32 source_count;

    c3dae_v8_render_pass *render_passes;
    c3dae_u32 render_pass_cap;
    c3dae_u32 render_pass_count;

    c3dae_v8_image_info *images;
    c3dae_u32 image_cap;
    c3dae_u32 image_count;

    c3dae_v8_brep_info *breps;
    c3dae_u32 brep_cap;
    c3dae_u32 brep_count;

    c3dae_v8_spline_info *splines;
    c3dae_u32 spline_cap;
    c3dae_u32 spline_count;

    c3dae_v8_issue *issues;
    c3dae_u32 issue_cap;
    c3dae_u32 issue_count;
} c3dae_v8_document;

void c3dae_v8_document_init(c3dae_v8_document *doc);
void c3dae_v8_bind_transforms(c3dae_v8_document *doc, c3dae_v8_transform *items, c3dae_u32 cap);
void c3dae_v8_bind_sources(c3dae_v8_document *doc, c3dae_v8_source_info *items, c3dae_u32 cap);
void c3dae_v8_bind_render_passes(c3dae_v8_document *doc, c3dae_v8_render_pass *items, c3dae_u32 cap);
void c3dae_v8_bind_images(c3dae_v8_document *doc, c3dae_v8_image_info *items, c3dae_u32 cap);
void c3dae_v8_bind_breps(c3dae_v8_document *doc, c3dae_v8_brep_info *items, c3dae_u32 cap);
void c3dae_v8_bind_splines(c3dae_v8_document *doc, c3dae_v8_spline_info *items, c3dae_u32 cap);
void c3dae_v8_bind_issues(c3dae_v8_document *doc, c3dae_v8_issue *items, c3dae_u32 cap);

int c3dae_v8_scan_memory(const char *xml, c3dae_u32 xml_len, c3dae_v8_document *doc);
void c3dae_v8_matrix_from_lookat(c3dae_fixed *m, const c3dae_fixed *raw9);
void c3dae_v8_matrix_from_skew(c3dae_fixed *m, const c3dae_fixed *raw7);
const char *c3dae_v8_error_string(int code);

#ifdef __cplusplus
}
#endif

#endif
