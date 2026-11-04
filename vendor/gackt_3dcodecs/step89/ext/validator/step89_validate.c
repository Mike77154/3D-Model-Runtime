#include "step89_validate.h"

#define STEP89_VAL_FAMILY_GEOM    0x0001u
#define STEP89_VAL_FAMILY_TOPO    0x0002u
#define STEP89_VAL_FAMILY_PRODUCT 0x0004u
#define STEP89_VAL_FAMILY_PMI_SEM 0x0008u
#define STEP89_VAL_FAMILY_PMI_GFX 0x0010u
#define STEP89_VAL_FAMILY_AP242   0x0020u

static const step89_express_rule g_rules[] = {
    {"CARTESIAN_POINT", 2, 2, STEP89_VAL_FAMILY_GEOM},
    {"DIRECTION", 2, 2, STEP89_VAL_FAMILY_GEOM},
    {"VECTOR", 3, 3, STEP89_VAL_FAMILY_GEOM},
    {"AXIS2_PLACEMENT_3D", 4, 4, STEP89_VAL_FAMILY_GEOM},
    {"VERTEX_POINT", 2, 2, STEP89_VAL_FAMILY_TOPO},
    {"LINE", 3, 3, STEP89_VAL_FAMILY_GEOM},
    {"CIRCLE", 3, 3, STEP89_VAL_FAMILY_GEOM},
    {"ELLIPSE", 4, 4, STEP89_VAL_FAMILY_GEOM},
    {"B_SPLINE_CURVE_WITH_KNOTS", 8, 12, STEP89_VAL_FAMILY_GEOM},
    {"PLANE", 2, 2, STEP89_VAL_FAMILY_GEOM},
    {"CYLINDRICAL_SURFACE", 3, 3, STEP89_VAL_FAMILY_GEOM},
    {"CONICAL_SURFACE", 4, 4, STEP89_VAL_FAMILY_GEOM},
    {"SPHERICAL_SURFACE", 3, 3, STEP89_VAL_FAMILY_GEOM},
    {"TOROIDAL_SURFACE", 4, 4, STEP89_VAL_FAMILY_GEOM},
    {"B_SPLINE_SURFACE_WITH_KNOTS", 10, 14, STEP89_VAL_FAMILY_GEOM},
    {"EDGE_CURVE", 5, 5, STEP89_VAL_FAMILY_TOPO},
    {"ORIENTED_EDGE", 5, 5, STEP89_VAL_FAMILY_TOPO},
    {"EDGE_LOOP", 2, 2, STEP89_VAL_FAMILY_TOPO},
    {"FACE_BOUND", 3, 3, STEP89_VAL_FAMILY_TOPO},
    {"FACE_OUTER_BOUND", 3, 3, STEP89_VAL_FAMILY_TOPO},
    {"ADVANCED_FACE", 4, 4, STEP89_VAL_FAMILY_TOPO},
    {"CLOSED_SHELL", 2, 2, STEP89_VAL_FAMILY_TOPO},
    {"OPEN_SHELL", 2, 2, STEP89_VAL_FAMILY_TOPO},
    {"MANIFOLD_SOLID_BREP", 3, 3, STEP89_VAL_FAMILY_TOPO},
    {"PRODUCT", 4, 4, STEP89_VAL_FAMILY_PRODUCT},
    {"PRODUCT_DEFINITION", 4, 4, STEP89_VAL_FAMILY_PRODUCT},
    {"PRODUCT_DEFINITION_SHAPE", 4, 4, STEP89_VAL_FAMILY_PRODUCT},
    {"SHAPE_REPRESENTATION", 3, 3, STEP89_VAL_FAMILY_PRODUCT},
    {"ADVANCED_BREP_SHAPE_REPRESENTATION", 3, 3, STEP89_VAL_FAMILY_PRODUCT},
    {"MANIFOLD_SURFACE_SHAPE_REPRESENTATION", 3, 3, STEP89_VAL_FAMILY_PRODUCT},
    {"GEOMETRICALLY_BOUNDED_SURFACE_SHAPE_REPRESENTATION", 3, 3, STEP89_VAL_FAMILY_PRODUCT},
    {"MECHANICAL_DESIGN_GEOMETRIC_PRESENTATION_REPRESENTATION", 3, 3, STEP89_VAL_FAMILY_PMI_GFX},
    {"DRAUGHTING_MODEL", 3, 3, STEP89_VAL_FAMILY_PMI_GFX},
    {"ANNOTATION_OCCURRENCE", 3, 5, STEP89_VAL_FAMILY_PMI_GFX},
    {"ANNOTATION_PLANE", 3, 4, STEP89_VAL_FAMILY_PMI_GFX},
    {"PRESENTATION_VIEW", 3, 4, STEP89_VAL_FAMILY_PMI_GFX},
    {"CAMERA_MODEL_D3", 4, 4, STEP89_VAL_FAMILY_PMI_GFX},
    {"SHAPE_ASPECT", 5, 5, STEP89_VAL_FAMILY_PMI_SEM},
    {"DATUM", 4, 5, STEP89_VAL_FAMILY_PMI_SEM},
    {"DATUM_FEATURE", 5, 6, STEP89_VAL_FAMILY_PMI_SEM},
    {"DATUM_TARGET", 5, 7, STEP89_VAL_FAMILY_PMI_SEM},
    {"DATUM_SYSTEM", 2, 4, STEP89_VAL_FAMILY_PMI_SEM},
    {"GEOMETRIC_TOLERANCE", 4, 8, STEP89_VAL_FAMILY_PMI_SEM},
    {"GEOMETRIC_TOLERANCE_WITH_DATUM_REFERENCE", 5, 10, STEP89_VAL_FAMILY_PMI_SEM},
    {"POSITION_TOLERANCE", 4, 10, STEP89_VAL_FAMILY_PMI_SEM},
    {"FLATNESS_TOLERANCE", 4, 8, STEP89_VAL_FAMILY_PMI_SEM},
    {"CYLINDRICITY_TOLERANCE", 4, 8, STEP89_VAL_FAMILY_PMI_SEM},
    {"PARALLELISM_TOLERANCE", 4, 10, STEP89_VAL_FAMILY_PMI_SEM},
    {"PERPENDICULARITY_TOLERANCE", 4, 10, STEP89_VAL_FAMILY_PMI_SEM},
    {"ANGULARITY_TOLERANCE", 4, 10, STEP89_VAL_FAMILY_PMI_SEM},
    {"DIMENSIONAL_SIZE", 4, 6, STEP89_VAL_FAMILY_PMI_SEM},
    {"DIMENSIONAL_LOCATION", 4, 6, STEP89_VAL_FAMILY_PMI_SEM},
    {"SHAPE_DIMENSION_REPRESENTATION", 3, 3, STEP89_VAL_FAMILY_PMI_SEM},
    {"PLUS_MINUS_TOLERANCE", 3, 4, STEP89_VAL_FAMILY_PMI_SEM},
    {"LENGTH_MEASURE_WITH_UNIT", 2, 2, STEP89_VAL_FAMILY_PMI_SEM},
    {"MEASURE_REPRESENTATION_ITEM", 2, 2, STEP89_VAL_FAMILY_PMI_SEM},
    {"APPLIED_GROUP_ASSIGNMENT", 3, 3, STEP89_VAL_FAMILY_AP242},
    {"APPLIED_PRESENTED_ITEM", 2, 3, STEP89_VAL_FAMILY_AP242},
    {0, 0, 0, 0u}
};

static void val_zero(void *p, int n);
static void val_add(step89_validate_report *r, int sev, unsigned rule, const step89_entity *e, const char *txt);
static int val_is_ws(char c);
static int val_is_digit(char c);
static int val_scan_string(const char *s, int n, int pos);
static int val_scan_ref(const char *s, int n, int pos, int *ref, int *next_pos);
static void val_check_refs(const step89_model *m, const step89_entity *e, step89_validate_report *r);
static void val_check_entity_rule(const step89_model *m, const step89_entity *e, step89_validate_report *r);
static void val_check_pmi_hint(const step89_model *m, const step89_entity *e, step89_validate_report *r);
static void val_check_cad(const step89_cad_model *cad, step89_validate_report *r);
static int val_contains_word(const char *s, const char *word);

static void val_zero(void *p, int n)
{
    unsigned char *q;
    int i;
    q = (unsigned char *)p;
    for (i = 0; i < n; ++i) q[i] = 0u;
}

void step89_validate_init(step89_validate_report *r)
{
    if (!r) return;
    val_zero(r, (int)sizeof(*r));
}

static void val_copy(char *dst, int cap, const char *src)
{
    int i;
    if (!dst || cap <= 0) return;
    if (!src) src = "";
    i = 0;
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void val_add(step89_validate_report *r, int sev, unsigned rule, const step89_entity *e, const char *txt)
{
    step89_validate_message *m;
    if (!r) return;
    if (sev == STEP89_VALIDATE_SEV_INFO) ++r->info_count;
    else if (sev == STEP89_VALIDATE_SEV_WARN) ++r->warning_count;
    else if (sev == STEP89_VALIDATE_SEV_ERROR) ++r->error_count;
    else if (sev == STEP89_VALIDATE_SEV_FATAL) ++r->fatal_count;
    if (r->message_count >= STEP89_VALIDATE_MAX_MESSAGES) return;
    m = &r->messages[r->message_count];
    m->severity = sev;
    m->rule = rule;
    if (e) {
        m->entity_ref = e->id;
        m->line = e->line;
        val_copy(m->entity_name, STEP89_MAX_ENTITY_NAME, e->name);
    }
    val_copy(m->text, STEP89_VALIDATE_MAX_MESSAGE_TEXT, txt);
    ++r->message_count;
}

const step89_express_rule *step89_validate_find_rule(const char *entity_name)
{
    int i;
    if (!entity_name) return 0;
    for (i = 0; g_rules[i].name != 0; ++i) {
        if (step89_name_eq(g_rules[i].name, entity_name)) return &g_rules[i];
    }
    return 0;
}

static int val_is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static int val_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int val_scan_string(const char *s, int n, int pos)
{
    ++pos;
    while (pos < n) {
        if (s[pos] == '\'') {
            if (pos + 1 < n && s[pos + 1] == '\'') {
                pos += 2;
            } else {
                return pos + 1;
            }
        } else {
            ++pos;
        }
    }
    return pos;
}

int step89_validate_count_top_args(const char *s, int begin, int len)
{
    int i;
    int end;
    int depth;
    int count;
    int seen;
    int opened;
    char c;
    if (!s || begin < 0 || len <= 0) return 0;
    i = begin;
    end = begin + len;
    depth = 0;
    count = 0;
    seen = 0;
    opened = 0;
    while (i < end) {
        c = s[i];
        if (c == '\'') {
            i = val_scan_string(s, end, i);
            if (opened && depth >= 1) seen = 1;
        } else if (c == '(') {
            ++depth;
            opened = 1;
            if (depth > 1) seen = 1;
            ++i;
        } else if (c == ')') {
            if (depth > 1) seen = 1;
            if (depth > 0) --depth;
            ++i;
        } else if (c == ',' && depth == 1) {
            ++count;
            seen = 0;
            ++i;
        } else {
            if (opened && depth >= 1 && !val_is_ws(c)) seen = 1;
            ++i;
        }
    }
    if (seen || count > 0) ++count;
    return count;
}

static int val_scan_ref(const char *s, int n, int pos, int *ref, int *next_pos)
{
    long v;
    int p;
    if (ref) *ref = 0;
    if (next_pos) *next_pos = pos;
    if (!s || pos < 0 || pos >= n || s[pos] != '#') return 0;
    p = pos + 1;
    if (p >= n || !val_is_digit(s[p])) return 0;
    v = 0;
    while (p < n && val_is_digit(s[p])) {
        v = v * 10L + (long)(s[p] - '0');
        if (v > 2147483647L) return 0;
        ++p;
    }
    if (ref) *ref = (int)v;
    if (next_pos) *next_pos = p;
    return 1;
}

static void val_check_refs(const step89_model *m, const step89_entity *e, step89_validate_report *r)
{
    int i;
    int end;
    int ref;
    int next;
    if (!m || !e || !r) return;
    i = e->args_begin;
    end = e->args_begin + e->args_len;
    while (i < end) {
        if (m->text[i] == '\'') {
            i = val_scan_string(m->text, end, i);
        } else if (m->text[i] == '#') {
            if (val_scan_ref(m->text, end, i, &ref, &next)) {
                if (!step89_find_entity(m, ref)) {
                    ++r->unresolved_ref_count;
                    val_add(r, STEP89_VALIDATE_SEV_ERROR, STEP89_VALIDATE_RULE_PART21, e, "unresolved #reference");
                }
                i = next;
            } else {
                ++i;
            }
        } else {
            ++i;
        }
    }
}

static void val_check_entity_rule(const step89_model *m, const step89_entity *e, step89_validate_report *r)
{
    const step89_express_rule *rule;
    int argc;
    (void)m;
    if (!e || !r) return;
    rule = step89_validate_find_rule(e->name);
    if (!rule) {
        ++r->unknown_entity_count;
        return;
    }
    ++r->known_entity_count;
    argc = step89_validate_count_top_args(m->text, e->args_begin, e->args_len);
    if (argc < rule->min_args || argc > rule->max_args) {
        ++r->cardinality_error_count;
        val_add(r, STEP89_VALIDATE_SEV_ERROR, STEP89_VALIDATE_RULE_EXPRESS, e, "static EXPRESS arity mismatch");
    }
}

int step89_validate_is_pmi_entity_name(const char *name)
{
    const step89_express_rule *rule;
    rule = step89_validate_find_rule(name);
    if (!rule) return 0;
    return (rule->family & (STEP89_VAL_FAMILY_PMI_SEM | STEP89_VAL_FAMILY_PMI_GFX)) != 0u;
}

static char val_upper(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - ('a' - 'A'));
    return c;
}

static int val_contains_word(const char *s, const char *word)
{
    int i;
    int j;
    if (!s || !word) return 0;
    for (i = 0; s[i] != '\0'; ++i) {
        j = 0;
        while (word[j] != '\0' && s[i + j] != '\0') {
            if (val_upper(s[i + j]) != val_upper(word[j])) break;
            ++j;
        }
        if (word[j] == '\0') return 1;
    }
    return 0;
}

int step89_validate_is_ap242_schema(const step89_model *m)
{
    if (!m) return 0;
    if (val_contains_word(m->schema, "AP242")) return 1;
    if (val_contains_word(m->schema, "MANAGED_MODEL_BASED_3D_ENGINEERING")) return 1;
    return 0;
}

static void val_check_pmi_hint(const step89_model *m, const step89_entity *e, step89_validate_report *r)
{
    const step89_express_rule *rule;
    (void)m;
    if (!e || !r) return;
    rule = step89_validate_find_rule(e->name);
    if (!rule) return;
    if (rule->family & STEP89_VAL_FAMILY_PMI_SEM) {
        ++r->pmi_entity_count;
        ++r->pmi_semantic_count;
    }
    if (rule->family & STEP89_VAL_FAMILY_PMI_GFX) {
        ++r->pmi_entity_count;
        ++r->pmi_graphic_count;
    }
}

static void val_check_cad(const step89_cad_model *cad, step89_validate_report *r)
{
    if (!cad || !r) return;
    r->topology_error_count = cad->missing_ref_count + cad->open_loop_count +
                              cad->nonmanifold_edge_count + cad->zero_edge_count;
    if (cad->missing_ref_count > 0) val_add(r, STEP89_VALIDATE_SEV_ERROR, STEP89_VALIDATE_RULE_TOPOLOGY, 0, "CAD kernel has missing geometry refs");
    if (cad->open_loop_count > 0) val_add(r, STEP89_VALIDATE_SEV_ERROR, STEP89_VALIDATE_RULE_TOPOLOGY, 0, "CAD kernel has open loops");
    if (cad->nonmanifold_edge_count > 0) val_add(r, STEP89_VALIDATE_SEV_WARN, STEP89_VALIDATE_RULE_TOPOLOGY, 0, "CAD kernel has non-manifold edges");
    if (cad->zero_edge_count > 0) val_add(r, STEP89_VALIDATE_SEV_WARN, STEP89_VALIDATE_RULE_TOPOLOGY, 0, "CAD kernel has zero-length edges");
}

step89_result step89_validate_model(const step89_model *m, step89_validate_report *r)
{
    int i;
    if (!m || !r) return STEP89_ERR_NULL;
    step89_validate_init(r);
    if (m->entity_count <= 0) {
        val_add(r, STEP89_VALIDATE_SEV_FATAL, STEP89_VALIDATE_RULE_PART21, 0, "empty DATA section");
        return STEP89_ERR_SYNTAX;
    }
    if (step89_validate_is_ap242_schema(m)) {
        ++r->ap242_hint_count;
        val_add(r, STEP89_VALIDATE_SEV_INFO, STEP89_VALIDATE_RULE_AP242, 0, "AP242-like schema detected");
    }
    for (i = 0; i < m->entity_count; ++i) {
        val_check_refs(m, &m->entities[i], r);
        val_check_entity_rule(m, &m->entities[i], r);
        val_check_pmi_hint(m, &m->entities[i], r);
    }
    if (r->pmi_entity_count > 0 && !step89_validate_is_ap242_schema(m)) {
        val_add(r, STEP89_VALIDATE_SEV_WARN, STEP89_VALIDATE_RULE_PMI, 0, "PMI-like entities outside AP242-like schema");
    }
    if (r->fatal_count > 0 || r->error_count > 0) return STEP89_ERR_SYNTAX;
    if (r->warning_count > 0) return STEP89_ERR_LIMIT;
    return STEP89_OK;
}

step89_result step89_validate_model_with_cad(const step89_model *m, const step89_cad_model *cad, step89_validate_report *r)
{
    step89_result res;
    if (!m || !r) return STEP89_ERR_NULL;
    res = step89_validate_model(m, r);
    val_check_cad(cad, r);
    if (r->fatal_count > 0 || r->error_count > 0) return STEP89_ERR_SYNTAX;
    if (r->warning_count > 0) return STEP89_ERR_LIMIT;
    return res;
}
