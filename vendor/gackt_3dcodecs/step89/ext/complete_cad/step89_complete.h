#ifndef STEP89_COMPLETE_H
#define STEP89_COMPLETE_H

/*
   step89_complete - expansion seed for a future full CAD kernel and AP242/PMI
   validator.

   C89, no heap, no float/double. The module builds a typed instance graph over
   the Part 21 entities, derives reusable CAD/PMI/unit/tolerance summaries, and
   runs conservative rule checks that can later be replaced by generated EXPRESS
   schema code.
*/

#include "step89.h"
#include "step89_cad_kernel.h"
#include "step89_validate.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STEP89_COMPLETE_MAX_NODES
#define STEP89_COMPLETE_MAX_NODES STEP89_MAX_ENTITIES
#endif

#ifndef STEP89_COMPLETE_MAX_REFS
#define STEP89_COMPLETE_MAX_REFS 32768
#endif

#ifndef STEP89_COMPLETE_MAX_PMI
#define STEP89_COMPLETE_MAX_PMI 1024
#endif

#ifndef STEP89_COMPLETE_MAX_VALIDATION_PROPS
#define STEP89_COMPLETE_MAX_VALIDATION_PROPS 512
#endif

#ifndef STEP89_COMPLETE_MAX_UNITS
#define STEP89_COMPLETE_MAX_UNITS 128
#endif

#ifndef STEP89_COMPLETE_MAX_NURBS
#define STEP89_COMPLETE_MAX_NURBS 1024
#endif

#ifndef STEP89_COMPLETE_MAX_SHELLS
#define STEP89_COMPLETE_MAX_SHELLS 512
#endif

#ifndef STEP89_COMPLETE_MAX_SOLIDS
#define STEP89_COMPLETE_MAX_SOLIDS 256
#endif

#ifndef STEP89_COMPLETE_MAX_MESSAGES
#define STEP89_COMPLETE_MAX_MESSAGES 512
#endif

#ifndef STEP89_COMPLETE_MAX_MESSAGE_TEXT
#define STEP89_COMPLETE_MAX_MESSAGE_TEXT 96
#endif

#define STEP89_COMPLETE_CLASS_UNKNOWN      0x00000000u
#define STEP89_COMPLETE_CLASS_GEOMETRY     0x00000001u
#define STEP89_COMPLETE_CLASS_TOPOLOGY     0x00000002u
#define STEP89_COMPLETE_CLASS_PRODUCT      0x00000004u
#define STEP89_COMPLETE_CLASS_ASSEMBLY     0x00000008u
#define STEP89_COMPLETE_CLASS_AP242        0x00000010u
#define STEP89_COMPLETE_CLASS_PMI_SEM      0x00000020u
#define STEP89_COMPLETE_CLASS_PMI_GFX      0x00000040u
#define STEP89_COMPLETE_CLASS_UNIT         0x00000080u
#define STEP89_COMPLETE_CLASS_VALIDATION   0x00000100u
#define STEP89_COMPLETE_CLASS_PRESENTATION 0x00000200u
#define STEP89_COMPLETE_CLASS_NURBS        0x00000400u
#define STEP89_COMPLETE_CLASS_CONTEXT      0x00000800u
#define STEP89_COMPLETE_CLASS_MATERIAL     0x00001000u
#define STEP89_COMPLETE_CLASS_RELATION     0x00002000u

#define STEP89_COMPLETE_NODE_DANGLING_REF  0x0001u
#define STEP89_COMPLETE_NODE_UNKNOWN_CLASS 0x0002u
#define STEP89_COMPLETE_NODE_HAS_INVERSE   0x0004u
#define STEP89_COMPLETE_NODE_PMI_ANCHOR    0x0008u

#define STEP89_COMPLETE_MSG_INFO  1
#define STEP89_COMPLETE_MSG_WARN  2
#define STEP89_COMPLETE_MSG_ERROR 3

#define STEP89_COMPLETE_PMI_UNKNOWN       0
#define STEP89_COMPLETE_PMI_DATUM         1
#define STEP89_COMPLETE_PMI_TOLERANCE     2
#define STEP89_COMPLETE_PMI_DIMENSION     3
#define STEP89_COMPLETE_PMI_ANNOTATION    4
#define STEP89_COMPLETE_PMI_PRESENTATION  5
#define STEP89_COMPLETE_PMI_GROUP         6

#define STEP89_COMPLETE_NURBS_CURVE   1
#define STEP89_COMPLETE_NURBS_SURFACE 2
#define STEP89_COMPLETE_NURBS_TRIM    3

#define STEP89_COMPLETE_UNIT_LENGTH 1
#define STEP89_COMPLETE_UNIT_ANGLE  2
#define STEP89_COMPLETE_UNIT_SOLID  3
#define STEP89_COMPLETE_UNIT_PLANE  4
#define STEP89_COMPLETE_UNIT_RATIO  5

#define STEP89_COMPLETE_SHELL_OPEN   0
#define STEP89_COMPLETE_SHELL_CLOSED 1

#define STEP89_COMPLETE_TOLERANCE_DEFAULT STEP89_FIXED_SCALE / 100000L

typedef struct step89_complete_schema_class_s {
    const char *name;
    unsigned flags;
    int min_args;
    int max_args;
} step89_complete_schema_class;

typedef struct step89_complete_ref_s {
    int owner_ref;
    int target_ref;
    int arg_index;
    unsigned flags;
} step89_complete_ref;

typedef struct step89_complete_node_s {
    int ref;
    int entity_index;
    int schema_index;
    unsigned class_flags;
    unsigned flags;
    int first_ref_index;
    int ref_count;
    int inbound_count;
} step89_complete_node;

typedef struct step89_complete_unit_s {
    int ref;
    int kind;
    step89_fixed scale_to_model;
    char name[STEP89_MAX_ENTITY_NAME];
} step89_complete_unit;

typedef struct step89_complete_unit_context_s {
    int length_unit_ref;
    int angle_unit_ref;
    step89_fixed length_scale;
    step89_fixed angle_scale;
    step89_fixed model_tolerance;
    int unit_count;
    step89_complete_unit units[STEP89_COMPLETE_MAX_UNITS];
} step89_complete_unit_context;

typedef struct step89_complete_pmi_s {
    int ref;
    int kind;
    int semantic;
    int graphic;
    int anchor_ref;
    int presentation_ref;
    unsigned flags;
} step89_complete_pmi;

typedef struct step89_complete_validation_property_s {
    int ref;
    int target_ref;
    int kind;
    step89_fixed value;
    unsigned flags;
} step89_complete_validation_property;

typedef struct step89_complete_nurbs_seed_s {
    int ref;
    int kind;
    int degree_u;
    int degree_v;
    int control_count;
    int knot_count;
    int trim_ref;
    unsigned flags;
} step89_complete_nurbs_seed;

typedef struct step89_complete_shell_s {
    int ref;
    int closed;
    int face_count;
    int first_face_ref;
    unsigned flags;
} step89_complete_shell;

typedef struct step89_complete_solid_s {
    int ref;
    int shell_ref;
    int valid_shell;
    unsigned flags;
} step89_complete_solid;

typedef struct step89_complete_message_s {
    int severity;
    int entity_ref;
    unsigned class_flags;
    char text[STEP89_COMPLETE_MAX_MESSAGE_TEXT];
} step89_complete_message;

typedef struct step89_complete_context_s {
    int node_count;
    step89_complete_node nodes[STEP89_COMPLETE_MAX_NODES];

    int ref_count;
    step89_complete_ref refs[STEP89_COMPLETE_MAX_REFS];

    step89_complete_unit_context units;

    int pmi_count;
    step89_complete_pmi pmi[STEP89_COMPLETE_MAX_PMI];

    int validation_property_count;
    step89_complete_validation_property validation_props[STEP89_COMPLETE_MAX_VALIDATION_PROPS];

    int nurbs_count;
    step89_complete_nurbs_seed nurbs[STEP89_COMPLETE_MAX_NURBS];

    int shell_count;
    step89_complete_shell shells[STEP89_COMPLETE_MAX_SHELLS];

    int solid_count;
    step89_complete_solid solids[STEP89_COMPLETE_MAX_SOLIDS];

    int unknown_class_count;
    int dangling_ref_count;
    int inverse_error_count;
    int where_error_count;
    int derive_count;
    int ap242_warning_count;
    int topology_heal_hint_count;

    int message_count;
    step89_complete_message messages[STEP89_COMPLETE_MAX_MESSAGES];
} step89_complete_context;

void step89_complete_init(step89_complete_context *ctx);
step89_result step89_complete_analyze(step89_complete_context *ctx, const step89_model *m, const step89_cad_model *cad, const step89_validate_report *report);
const step89_complete_schema_class *step89_complete_find_schema_class(const char *entity_name);
const step89_complete_node *step89_complete_find_node(const step89_complete_context *ctx, int ref);
int step89_complete_entity_class_flags(const char *entity_name);
int step89_complete_pmi_kind(const char *entity_name);

step89_result step89_complete_run_where_rules(step89_complete_context *ctx, const step89_model *m, const step89_cad_model *cad);
step89_result step89_complete_collect_units(step89_complete_context *ctx, const step89_model *m);
step89_result step89_complete_collect_pmi(step89_complete_context *ctx, const step89_model *m);
step89_result step89_complete_collect_nurbs_and_trims(step89_complete_context *ctx, const step89_model *m);
step89_result step89_complete_collect_shells_solids(step89_complete_context *ctx, const step89_model *m);
step89_result step89_complete_collect_validation_properties(step89_complete_context *ctx, const step89_model *m);

#ifdef __cplusplus
}
#endif

#endif /* STEP89_COMPLETE_H */
