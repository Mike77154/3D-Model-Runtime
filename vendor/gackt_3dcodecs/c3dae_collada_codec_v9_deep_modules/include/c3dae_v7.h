#ifndef C3DAE_V7_H
#define C3DAE_V7_H

/*
   c3dae_v7 - optional COLLADA coverage scanner for c3dae.
   No heap, C89, caller-owned buffers. This module does not replace the core
   mesh/scene importer; it gathers the COLLADA features that are usually useful
   for a converter/report pass: animation clips, line primitives, scene refs,
   asset metadata, extras, FX profiles, constraints, formulas and B-rep counters.
*/

#include "c3dae.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef C3DAE_V7_TEXT_MAX
#define C3DAE_V7_TEXT_MAX 128
#endif

#ifndef C3DAE_V7_SNIPPET_MAX
#define C3DAE_V7_SNIPPET_MAX 192
#endif

#define C3DAE_V7_OK              0
#define C3DAE_V7_ERR_NULL       -1
#define C3DAE_V7_ERR_CAPACITY   -2
#define C3DAE_V7_ERR_XML        -3

#define C3DAE_V7_LINE_LINES      1
#define C3DAE_V7_LINE_STRIP     2

#define C3DAE_V7_FX_COMMON      1U
#define C3DAE_V7_FX_GLSL        2U
#define C3DAE_V7_FX_CG          4U
#define C3DAE_V7_FX_GLES        8U
#define C3DAE_V7_FX_GLES2       16U

#define C3DAE_V7_ARCHIVE_NONE   0U
#define C3DAE_V7_ARCHIVE_ZAE    1U

#define C3DAE_V7_EXTRA_UNKNOWN  0
#define C3DAE_V7_EXTRA_ASSET    1
#define C3DAE_V7_EXTRA_EFFECT   2
#define C3DAE_V7_EXTRA_GEOMETRY 3
#define C3DAE_V7_EXTRA_NODE     4
#define C3DAE_V7_EXTRA_SCENE    5

#define C3DAE_V7_CONSTRAINT_UNKNOWN 0
#define C3DAE_V7_CONSTRAINT_HINGE   1
#define C3DAE_V7_CONSTRAINT_SLIDER  2
#define C3DAE_V7_CONSTRAINT_GENERIC 3

typedef struct c3dae_v7_asset_info_tag {
    char created[C3DAE_V7_TEXT_MAX];
    char modified[C3DAE_V7_TEXT_MAX];
    char revision[C3DAE_V7_TEXT_MAX];
    char title[C3DAE_V7_TEXT_MAX];
    char subject[C3DAE_V7_TEXT_MAX];
    char keywords[C3DAE_V7_TEXT_MAX];
    char author[C3DAE_V7_TEXT_MAX];
    char authoring_tool[C3DAE_V7_TEXT_MAX];
    char comments[C3DAE_V7_TEXT_MAX];
    char copyright_text[C3DAE_V7_TEXT_MAX];
    char unit_name[C3DAE_ID_MAX];
    c3dae_fixed unit_meter;
    int up_axis;
} c3dae_v7_asset_info;

typedef struct c3dae_v7_scene_refs_tag {
    char visual_scene[C3DAE_ID_MAX];
    char physics_scene[C3DAE_ID_MAX];
    char kinematics_scene[C3DAE_ID_MAX];
} c3dae_v7_scene_refs;

typedef struct c3dae_v7_animation_clip_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    c3dae_fixed start;
    c3dae_fixed end;
    c3dae_u32 first_ref;
    c3dae_u32 ref_count;
} c3dae_v7_animation_clip;

typedef struct c3dae_v7_animation_ref_tag {
    char animation[C3DAE_ID_MAX];
} c3dae_v7_animation_ref;

typedef struct c3dae_v7_line_tag {
    char geometry[C3DAE_ID_MAX];
    char material_symbol[C3DAE_ID_MAX];
    c3dae_u32 a;
    c3dae_u32 b;
    int kind;
} c3dae_v7_line;

typedef struct c3dae_v7_fx_profile_tag {
    char effect[C3DAE_ID_MAX];
    unsigned int profile_mask;
    char transparent_opaque[C3DAE_ID_MAX];
    char first_param[C3DAE_ID_MAX];
} c3dae_v7_fx_profile;

typedef struct c3dae_v7_rigid_constraint_tag {
    char sid[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    char ref_body[C3DAE_ID_MAX];
    char target_body[C3DAE_ID_MAX];
    int enabled;
    int interpenetrate;
    int kind;
    c3dae_fixed linear_min[3];
    c3dae_fixed linear_max[3];
    c3dae_fixed angular_min[3];
    c3dae_fixed angular_max[3];
} c3dae_v7_rigid_constraint;

typedef struct c3dae_v7_articulated_system_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    char kinematics_model[C3DAE_ID_MAX];
    char motion_model[C3DAE_ID_MAX];
} c3dae_v7_articulated_system;

typedef struct c3dae_v7_formula_tag {
    char id[C3DAE_ID_MAX];
    char name[C3DAE_ID_MAX];
    char target[C3DAE_ID_MAX];
    char technique_profile[C3DAE_ID_MAX];
} c3dae_v7_formula;

typedef struct c3dae_v7_extra_tag {
    int parent_kind;
    char parent_id[C3DAE_ID_MAX];
    char type[C3DAE_ID_MAX];
    char technique_profile[C3DAE_ID_MAX];
    char snippet[C3DAE_V7_SNIPPET_MAX];
} c3dae_v7_extra;

typedef struct c3dae_v7_stats_tag {
    c3dae_u32 brep_count;
    c3dae_u32 spline_count;
    c3dae_u32 convex_mesh_count;
    c3dae_u32 polygon_hole_count;
    c3dae_u32 instance_rigid_constraint_count;
    c3dae_u32 instance_force_field_count;
    c3dae_u32 instance_node_count;
    c3dae_u32 profile_gles2_count;
    c3dae_u32 unsupported_note_count;
} c3dae_v7_stats;

typedef struct c3dae_v7_document_tag {
    c3dae_v7_asset_info asset;
    c3dae_v7_scene_refs scene;
    c3dae_v7_stats stats;

    c3dae_v7_animation_clip *clips;
    c3dae_u32 clip_cap;
    c3dae_u32 clip_count;

    c3dae_v7_animation_ref *clip_refs;
    c3dae_u32 clip_ref_cap;
    c3dae_u32 clip_ref_count;

    c3dae_v7_line *lines;
    c3dae_u32 line_cap;
    c3dae_u32 line_count;

    c3dae_v7_fx_profile *fx_profiles;
    c3dae_u32 fx_profile_cap;
    c3dae_u32 fx_profile_count;

    c3dae_v7_rigid_constraint *constraints;
    c3dae_u32 constraint_cap;
    c3dae_u32 constraint_count;

    c3dae_v7_articulated_system *articulated_systems;
    c3dae_u32 articulated_system_cap;
    c3dae_u32 articulated_system_count;

    c3dae_v7_formula *formulas;
    c3dae_u32 formula_cap;
    c3dae_u32 formula_count;

    c3dae_v7_extra *extras;
    c3dae_u32 extra_cap;
    c3dae_u32 extra_count;

    c3dae_u32 truncated_count;
} c3dae_v7_document;

void c3dae_v7_document_init(c3dae_v7_document *doc);
void c3dae_v7_bind_animation_clips(c3dae_v7_document *doc,
                                   c3dae_v7_animation_clip *clips,
                                   c3dae_u32 clip_cap,
                                   c3dae_v7_animation_ref *refs,
                                   c3dae_u32 ref_cap);
void c3dae_v7_bind_lines(c3dae_v7_document *doc, c3dae_v7_line *lines, c3dae_u32 cap);
void c3dae_v7_bind_fx_profiles(c3dae_v7_document *doc, c3dae_v7_fx_profile *profiles, c3dae_u32 cap);
void c3dae_v7_bind_constraints(c3dae_v7_document *doc, c3dae_v7_rigid_constraint *constraints, c3dae_u32 cap);
void c3dae_v7_bind_kinematics15(c3dae_v7_document *doc,
                                c3dae_v7_articulated_system *systems,
                                c3dae_u32 system_cap,
                                c3dae_v7_formula *formulas,
                                c3dae_u32 formula_cap);
void c3dae_v7_bind_extras(c3dae_v7_document *doc, c3dae_v7_extra *extras, c3dae_u32 cap);

int c3dae_v7_scan_memory(const char *xml, c3dae_u32 xml_len, c3dae_v7_document *doc);
const char *c3dae_v7_error_string(int code);

#ifdef __cplusplus
}
#endif

#endif
