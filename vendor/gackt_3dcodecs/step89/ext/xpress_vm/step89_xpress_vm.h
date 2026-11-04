#ifndef STEP89_XPRESS_VM_H
#define STEP89_XPRESS_VM_H

/*
   step89_xpress_vm - EXPRESSION/EXPRESS/AP242/PMI/CAD deep seed module.

   Scope:
   - C89, no malloc/realloc/free, no float/double.
   - Parses a practical textual subset of EXPRESS: SCHEMA, ENTITY,
     SUBTYPE OF, ABSTRACT SUPERTYPE, explicit attributes, DERIVE, INVERSE,
     WHERE clauses, END_ENTITY.
   - Builds a fixed bytecode VM for WHERE/DERIVE/INVERSE style checks over
     the already parsed Part 21 model.
   - Adds sewing/healing hints, NURBS/trimming evaluators, AP242 fine checks,
     and semantic-to-graphic PMI mapping.

   This is still not a commercial CAD kernel, but it is a real expandable
   module rather than a comment-only stub.
*/

#include "step89.h"
#include "step89_cad_kernel.h"
#include "step89_validate.h"
#include "step89_complete.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STEP89_XP_MAX_ENTITIES
#define STEP89_XP_MAX_ENTITIES 384
#endif

#ifndef STEP89_XP_MAX_ATTRS
#define STEP89_XP_MAX_ATTRS 2048
#endif

#ifndef STEP89_XP_MAX_RULES
#define STEP89_XP_MAX_RULES 1024
#endif

#ifndef STEP89_XP_MAX_OPS
#define STEP89_XP_MAX_OPS 4096
#endif

#ifndef STEP89_XP_MAX_NAME
#define STEP89_XP_MAX_NAME 64
#endif

#ifndef STEP89_XP_MAX_EXPR
#define STEP89_XP_MAX_EXPR 160
#endif

#ifndef STEP89_XP_MAX_MESSAGES
#define STEP89_XP_MAX_MESSAGES 768
#endif

#ifndef STEP89_XP_MAX_SEW_EDGES
#define STEP89_XP_MAX_SEW_EDGES STEP89_CAD_MAX_EDGES
#endif

#ifndef STEP89_XP_MAX_PMI_MAPS
#define STEP89_XP_MAX_PMI_MAPS 512
#endif

#ifndef STEP89_XP_MAX_DEGREE
#define STEP89_XP_MAX_DEGREE 8
#endif

#ifndef STEP89_XP_MAX_SURFACE_TMP
#define STEP89_XP_MAX_SURFACE_TMP 64
#endif

#define STEP89_XP_ENTITY_ABSTRACT       0x0001u
#define STEP89_XP_ENTITY_HAS_SUPERTYPE  0x0002u
#define STEP89_XP_ENTITY_FROM_BOOTSTRAP 0x0004u
#define STEP89_XP_ENTITY_FROM_EXPRESS   0x0008u
#define STEP89_XP_ENTITY_AP242          0x0010u
#define STEP89_XP_ENTITY_PMI_SEM        0x0020u
#define STEP89_XP_ENTITY_PMI_GFX        0x0040u
#define STEP89_XP_ENTITY_TOPOLOGY       0x0080u
#define STEP89_XP_ENTITY_NURBS          0x0100u
#define STEP89_XP_ENTITY_VALIDATION     0x0200u

#define STEP89_XP_ATTR_OPTIONAL 0x0001u
#define STEP89_XP_ATTR_LIST     0x0002u
#define STEP89_XP_ATTR_SET      0x0004u
#define STEP89_XP_ATTR_REF      0x0008u
#define STEP89_XP_ATTR_DERIVED  0x0010u
#define STEP89_XP_ATTR_INVERSE  0x0020u

#define STEP89_XP_RULE_WHERE   1
#define STEP89_XP_RULE_DERIVE  2
#define STEP89_XP_RULE_INVERSE 3
#define STEP89_XP_RULE_SYSTEM  4

#define STEP89_XP_SEV_INFO  1
#define STEP89_XP_SEV_WARN  2
#define STEP89_XP_SEV_ERROR 3

#define STEP89_XP_OP_END                  0
#define STEP89_XP_OP_TRUE                 1
#define STEP89_XP_OP_ARG_RANGE            2
#define STEP89_XP_OP_ENTITY_REFS_EXIST    3
#define STEP89_XP_OP_INBOUND_AT_LEAST     4
#define STEP89_XP_OP_CAD_LOOP_CLOSED      5
#define STEP89_XP_OP_CAD_SOLID_CLOSED     6
#define STEP89_XP_OP_NURBS_CURVE_VALID    7
#define STEP89_XP_OP_NURBS_SURFACE_VALID  8
#define STEP89_XP_OP_PMI_HAS_ANCHOR       9
#define STEP89_XP_OP_PMI_HAS_GRAPHIC      10
#define STEP89_XP_OP_VALIDATION_TARGET    11
#define STEP89_XP_OP_AP242_SCHEMA         12
#define STEP89_XP_OP_DERIVE_REFCOUNT      13
#define STEP89_XP_OP_DERIVE_INBOUND       14

#define STEP89_XP_AP242_SCHEMA_PRESENT     0x0001u
#define STEP89_XP_AP242_HAS_UNITS          0x0002u
#define STEP89_XP_AP242_HAS_UNCERTAINTY    0x0004u
#define STEP89_XP_AP242_HAS_VALIDATION     0x0008u
#define STEP89_XP_AP242_HAS_SEMANTIC_PMI   0x0010u
#define STEP89_XP_AP242_HAS_GRAPHIC_PMI    0x0020u
#define STEP89_XP_AP242_HAS_NURBS          0x0040u
#define STEP89_XP_AP242_HAS_SOLIDS         0x0080u

#define STEP89_XP_SEW_OPEN_EDGE       0x0001u
#define STEP89_XP_SEW_REVERSED_PAIR   0x0002u
#define STEP89_XP_SEW_DUPLICATE_EDGE  0x0004u
#define STEP89_XP_SEW_ZERO_EDGE       0x0008u
#define STEP89_XP_SEW_NONMANIFOLD     0x0010u
#define STEP89_XP_SEW_MERGE_START     0x0020u
#define STEP89_XP_SEW_MERGE_END       0x0040u

#define STEP89_XP_PMI_DIRECT_REF      0x0001u
#define STEP89_XP_PMI_SHARED_ANCHOR   0x0002u
#define STEP89_XP_PMI_PRESENTATION    0x0004u
#define STEP89_XP_PMI_UNMAPPED        0x0008u

typedef struct step89_xp_attr_s {
    int owner_index;
    char name[STEP89_XP_MAX_NAME];
    char type_name[STEP89_XP_MAX_NAME];
    unsigned flags;
} step89_xp_attr;

typedef struct step89_xp_op_s {
    int opcode;
    int a;
    int b;
    int c;
} step89_xp_op;

typedef struct step89_xp_rule_s {
    int owner_index;
    int kind;
    char name[STEP89_XP_MAX_NAME];
    char expr[STEP89_XP_MAX_EXPR];
    int first_op;
    int op_count;
    int severity;
} step89_xp_rule;

typedef struct step89_xp_entity_def_s {
    char name[STEP89_XP_MAX_NAME];
    char super_name[STEP89_XP_MAX_NAME];
    unsigned flags;
    int first_attr;
    int explicit_attr_count;
    int derived_attr_count;
    int inverse_attr_count;
    int first_rule;
    int rule_count;
    int min_args;
    int max_args;
} step89_xp_entity_def;

typedef struct step89_xp_schema_s {
    char schema_name[STEP89_XP_MAX_NAME];
    int entity_count;
    int attr_count;
    int rule_count;
    int op_count;
    step89_xp_entity_def entities[STEP89_XP_MAX_ENTITIES];
    step89_xp_attr attrs[STEP89_XP_MAX_ATTRS];
    step89_xp_rule rules[STEP89_XP_MAX_RULES];
    step89_xp_op ops[STEP89_XP_MAX_OPS];
    int parse_warning_count;
    int parse_error_count;
} step89_xp_schema;

typedef struct step89_xp_message_s {
    int severity;
    int entity_ref;
    char entity_name[STEP89_XP_MAX_NAME];
    char rule_name[STEP89_XP_MAX_NAME];
    char text[STEP89_XP_MAX_EXPR];
} step89_xp_message;

typedef struct step89_xp_vm_report_s {
    int where_ok;
    int where_fail;
    int derive_eval_count;
    int inverse_eval_count;
    int system_rule_count;
    int message_count;
    step89_xp_message messages[STEP89_XP_MAX_MESSAGES];
} step89_xp_vm_report;

typedef struct step89_xp_sew_edge_s {
    int edge_ref;
    int start_vertex_ref;
    int end_vertex_ref;
    int paired_edge_ref;
    unsigned flags;
} step89_xp_sew_edge;

typedef struct step89_xp_sew_report_s {
    int edge_count;
    step89_xp_sew_edge edges[STEP89_XP_MAX_SEW_EDGES];
    int sew_pair_count;
    int open_edge_count;
    int duplicate_edge_count;
    int nonmanifold_count;
    int zero_edge_count;
    int vertex_merge_hint_count;
    step89_fixed tolerance;
} step89_xp_sew_report;

typedef struct step89_xp_pmi_map_s {
    int semantic_ref;
    int graphic_ref;
    int anchor_ref;
    unsigned flags;
} step89_xp_pmi_map;

typedef struct step89_xp_pmi_report_s {
    int semantic_count;
    int graphic_count;
    int mapped_count;
    int unmapped_count;
    int direct_ref_count;
    int shared_anchor_count;
    int presentation_link_count;
    int map_count;
    step89_xp_pmi_map maps[STEP89_XP_MAX_PMI_MAPS];
} step89_xp_pmi_report;

typedef struct step89_xp_ap242_report_s {
    unsigned flags;
    int error_count;
    int warning_count;
    int semantic_pmi_count;
    int graphic_pmi_count;
    int validation_property_count;
    int context_count;
    int product_count;
    int representation_count;
    int edition_hint; /* 0 unknown, 1 AP242 generic, 2/3/4 when schema text hints it */
} step89_xp_ap242_report;

typedef struct step89_xp_point_s {
    step89_fixed x;
    step89_fixed y;
    step89_fixed z;
    step89_fixed w;
} step89_xp_point;

typedef struct step89_xp_analysis_s {
    step89_xp_schema schema;
    step89_xp_vm_report vm;
    step89_xp_sew_report sew;
    step89_xp_pmi_report pmi;
    step89_xp_ap242_report ap242;
} step89_xp_analysis;

void step89_xp_schema_init(step89_xp_schema *s);
void step89_xp_vm_report_init(step89_xp_vm_report *r);
void step89_xp_sew_report_init(step89_xp_sew_report *r);
void step89_xp_pmi_report_init(step89_xp_pmi_report *r);
void step89_xp_ap242_report_init(step89_xp_ap242_report *r);
void step89_xp_analysis_init(step89_xp_analysis *a);

step89_result step89_xp_parse_express(step89_xp_schema *schema, const char *text, int text_len);
step89_result step89_xp_bootstrap_ap242(step89_xp_schema *schema);
const step89_xp_entity_def *step89_xp_find_entity_def(const step89_xp_schema *schema, const char *name);

step89_result step89_xp_run_vm(const step89_xp_schema *schema, const step89_model *model, const step89_cad_model *cad, const step89_complete_context *complete, step89_xp_vm_report *report);
step89_result step89_xp_build_sewing(const step89_model *model, const step89_cad_model *cad, step89_fixed tolerance, step89_xp_sew_report *report);
step89_result step89_xp_map_pmi(const step89_model *model, const step89_complete_context *complete, step89_xp_pmi_report *report);
step89_result step89_xp_validate_ap242_fine(const step89_model *model, const step89_complete_context *complete, const step89_xp_pmi_report *pmi, step89_xp_ap242_report *report);
step89_result step89_xp_analyze(step89_xp_analysis *analysis, const char *express_text, int express_len, const step89_model *model, const step89_cad_model *cad, const step89_complete_context *complete);

step89_result step89_xp_eval_bspline_curve(const step89_xp_point *control, const step89_fixed *weights, int control_count, int degree, const step89_fixed *knots, int knot_count, step89_fixed t, step89_xp_point *out);
step89_result step89_xp_eval_bspline_surface(const step89_xp_point *control_grid, const step89_fixed *weights, int u_count, int v_count, int degree_u, int degree_v, const step89_fixed *u_knots, int u_knot_count, const step89_fixed *v_knots, int v_knot_count, step89_fixed u, step89_fixed v, step89_xp_point *out);
step89_result step89_xp_eval_step_curve_sample(const step89_model *model, int curve_ref, step89_fixed t, step89_xp_point *out);

#ifdef __cplusplus
}
#endif

#endif /* STEP89_XPRESS_VM_H */
