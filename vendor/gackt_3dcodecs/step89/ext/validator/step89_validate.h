#ifndef STEP89_VALIDATE_H
#define STEP89_VALIDATE_H

/*
   step89 validation base layer.

   This is the seed for a future EXPRESS/AP242/PMI "validatodo". It provides a
   static, no-heap rule table, Part 21 graph checks, argument-cardinality checks,
   AP242 schema hints, PMI entity accounting, and CAD topology hooks.
*/

#include "step89.h"
#include "step89_cad_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STEP89_VALIDATE_MAX_MESSAGES
#define STEP89_VALIDATE_MAX_MESSAGES 512
#endif

#ifndef STEP89_VALIDATE_MAX_MESSAGE_TEXT
#define STEP89_VALIDATE_MAX_MESSAGE_TEXT 96
#endif

#ifndef STEP89_VALIDATE_MAX_REFS_PER_ENTITY
#define STEP89_VALIDATE_MAX_REFS_PER_ENTITY 64
#endif

#define STEP89_VALIDATE_SEV_INFO    1
#define STEP89_VALIDATE_SEV_WARN    2
#define STEP89_VALIDATE_SEV_ERROR   3
#define STEP89_VALIDATE_SEV_FATAL   4

#define STEP89_VALIDATE_RULE_PART21      0x0001u
#define STEP89_VALIDATE_RULE_EXPRESS     0x0002u
#define STEP89_VALIDATE_RULE_AP242       0x0004u
#define STEP89_VALIDATE_RULE_PMI         0x0008u
#define STEP89_VALIDATE_RULE_TOPOLOGY    0x0010u

typedef struct step89_validate_message_s {
    int severity;
    unsigned rule;
    int entity_ref;
    unsigned line;
    char entity_name[STEP89_MAX_ENTITY_NAME];
    char text[STEP89_VALIDATE_MAX_MESSAGE_TEXT];
} step89_validate_message;

typedef struct step89_validate_report_s {
    int info_count;
    int warning_count;
    int error_count;
    int fatal_count;
    int message_count;
    step89_validate_message messages[STEP89_VALIDATE_MAX_MESSAGES];

    int unresolved_ref_count;
    int known_entity_count;
    int unknown_entity_count;
    int cardinality_error_count;
    int ap242_hint_count;
    int pmi_entity_count;
    int pmi_semantic_count;
    int pmi_graphic_count;
    int topology_error_count;
} step89_validate_report;

typedef struct step89_express_rule_s {
    const char *name;
    int min_args;
    int max_args;
    unsigned family;
} step89_express_rule;

void step89_validate_init(step89_validate_report *r);
step89_result step89_validate_model(const step89_model *m, step89_validate_report *r);
step89_result step89_validate_model_with_cad(const step89_model *m, const step89_cad_model *cad, step89_validate_report *r);

const step89_express_rule *step89_validate_find_rule(const char *entity_name);
int step89_validate_count_top_args(const char *s, int begin, int len);
int step89_validate_is_ap242_schema(const step89_model *m);
int step89_validate_is_pmi_entity_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* STEP89_VALIDATE_H */
