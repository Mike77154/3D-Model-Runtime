#include "step89_complete.h"

#define SCF STEP89_COMPLETE_CLASS_GEOMETRY
#define SCT STEP89_COMPLETE_CLASS_TOPOLOGY
#define SCP STEP89_COMPLETE_CLASS_PRODUCT
#define SCA STEP89_COMPLETE_CLASS_ASSEMBLY
#define SC242 STEP89_COMPLETE_CLASS_AP242
#define SCPMIS STEP89_COMPLETE_CLASS_PMI_SEM
#define SCPMIG STEP89_COMPLETE_CLASS_PMI_GFX
#define SCU STEP89_COMPLETE_CLASS_UNIT
#define SCV STEP89_COMPLETE_CLASS_VALIDATION
#define SCPR STEP89_COMPLETE_CLASS_PRESENTATION
#define SCN STEP89_COMPLETE_CLASS_NURBS
#define SCC STEP89_COMPLETE_CLASS_CONTEXT
#define SCM STEP89_COMPLETE_CLASS_MATERIAL
#define SCR STEP89_COMPLETE_CLASS_RELATION

static const step89_complete_schema_class g_complete_classes[] = {
    {"CARTESIAN_POINT", SCF, 2, 2},
    {"DIRECTION", SCF, 2, 2},
    {"VECTOR", SCF, 3, 3},
    {"AXIS1_PLACEMENT", SCF, 3, 3},
    {"AXIS2_PLACEMENT_2D", SCF, 3, 3},
    {"AXIS2_PLACEMENT_3D", SCF, 4, 4},
    {"GEOMETRIC_REPRESENTATION_CONTEXT", SCC, 1, 3},
    {"GEOMETRIC_REPRESENTATION_ITEM", SCF, 1, 8},
    {"VERTEX_POINT", SCT, 2, 2},
    {"LINE", SCF, 3, 3},
    {"CIRCLE", SCF, 3, 3},
    {"ELLIPSE", SCF, 4, 4},
    {"PARABOLA", SCF, 3, 3},
    {"HYPERBOLA", SCF, 4, 4},
    {"B_SPLINE_CURVE", SCF | SCN, 6, 10},
    {"B_SPLINE_CURVE_WITH_KNOTS", SCF | SCN, 8, 12},
    {"RATIONAL_B_SPLINE_CURVE", SCF | SCN, 7, 13},
    {"TRIMMED_CURVE", SCF | SCN, 5, 7},
    {"PCURVE", SCF | SCN, 3, 3},
    {"SURFACE_CURVE", SCF | SCN, 4, 4},
    {"SEAM_CURVE", SCF | SCN, 4, 4},
    {"BOUNDED_CURVE", SCF | SCN, 1, 8},
    {"PLANE", SCF, 2, 2},
    {"CYLINDRICAL_SURFACE", SCF, 3, 3},
    {"CONICAL_SURFACE", SCF, 4, 4},
    {"SPHERICAL_SURFACE", SCF, 3, 3},
    {"TOROIDAL_SURFACE", SCF, 4, 4},
    {"SURFACE_OF_LINEAR_EXTRUSION", SCF, 3, 3},
    {"SURFACE_OF_REVOLUTION", SCF, 3, 3},
    {"B_SPLINE_SURFACE", SCF | SCN, 8, 14},
    {"B_SPLINE_SURFACE_WITH_KNOTS", SCF | SCN, 10, 16},
    {"RATIONAL_B_SPLINE_SURFACE", SCF | SCN, 9, 17},
    {"RECTANGULAR_TRIMMED_SURFACE", SCF | SCN, 6, 8},
    {"CURVE_BOUNDED_SURFACE", SCF | SCN, 5, 5},
    {"BOUNDED_SURFACE", SCF | SCN, 1, 8},
    {"EDGE_CURVE", SCT, 5, 5},
    {"ORIENTED_EDGE", SCT, 5, 5},
    {"EDGE_LOOP", SCT, 2, 2},
    {"POLY_LOOP", SCT, 2, 2},
    {"FACE_BOUND", SCT, 3, 3},
    {"FACE_OUTER_BOUND", SCT, 3, 3},
    {"ADVANCED_FACE", SCT, 4, 4},
    {"FACE_SURFACE", SCT, 4, 4},
    {"OPEN_SHELL", SCT, 2, 2},
    {"CLOSED_SHELL", SCT, 2, 2},
    {"ORIENTED_CLOSED_SHELL", SCT, 4, 4},
    {"MANIFOLD_SOLID_BREP", SCT, 3, 3},
    {"BREP_WITH_VOIDS", SCT, 4, 4},
    {"SHELL_BASED_SURFACE_MODEL", SCT, 2, 2},
    {"FACETED_BREP", SCT, 3, 3},
    {"PRODUCT", SCP, 4, 4},
    {"PRODUCT_CONTEXT", SCP | SCC, 3, 3},
    {"PRODUCT_DEFINITION", SCP, 4, 4},
    {"PRODUCT_DEFINITION_FORMATION", SCP, 3, 3},
    {"PRODUCT_DEFINITION_SHAPE", SCP, 4, 4},
    {"SHAPE_DEFINITION_REPRESENTATION", SCP | SCR, 2, 2},
    {"SHAPE_REPRESENTATION", SCP, 3, 3},
    {"ADVANCED_BREP_SHAPE_REPRESENTATION", SCP | SCT, 3, 3},
    {"MANIFOLD_SURFACE_SHAPE_REPRESENTATION", SCP | SCT, 3, 3},
    {"GEOMETRICALLY_BOUNDED_SURFACE_SHAPE_REPRESENTATION", SCP | SCT, 3, 3},
    {"NEXT_ASSEMBLY_USAGE_OCCURRENCE", SCA | SCR, 6, 6},
    {"CONTEXT_DEPENDENT_SHAPE_REPRESENTATION", SCA | SCR, 2, 2},
    {"ITEM_DEFINED_TRANSFORMATION", SCA, 4, 4},
    {"REPRESENTATION_RELATIONSHIP", SCR, 4, 4},
    {"SHAPE_REPRESENTATION_RELATIONSHIP", SCR, 4, 4},
    {"MAPPED_ITEM", SCA, 3, 3},
    {"REPRESENTATION_MAP", SCA, 3, 3},
    {"SI_UNIT", SCU, 2, 4},
    {"NAMED_UNIT", SCU, 2, 2},
    {"LENGTH_UNIT", SCU, 1, 2},
    {"PLANE_ANGLE_UNIT", SCU, 1, 2},
    {"SOLID_ANGLE_UNIT", SCU, 1, 2},
    {"CONVERSION_BASED_UNIT", SCU, 3, 3},
    {"CONVERSION_BASED_UNIT_WITH_OFFSET", SCU, 4, 4},
    {"MEASURE_WITH_UNIT", SCU, 2, 2},
    {"UNCERTAINTY_MEASURE_WITH_UNIT", SCU | SCV, 4, 4},
    {"GLOBAL_UNIT_ASSIGNED_CONTEXT", SCU | SCC, 1, 2},
    {"GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT", SCU | SCC | SCV, 1, 2},
    {"GEOMETRIC_TOLERANCE", SCPMIS, 4, 8},
    {"GEOMETRIC_TOLERANCE_WITH_DATUM_REFERENCE", SCPMIS, 5, 10},
    {"POSITION_TOLERANCE", SCPMIS, 4, 10},
    {"FLATNESS_TOLERANCE", SCPMIS, 4, 8},
    {"STRAIGHTNESS_TOLERANCE", SCPMIS, 4, 8},
    {"ROUNDNESS_TOLERANCE", SCPMIS, 4, 8},
    {"CYLINDRICITY_TOLERANCE", SCPMIS, 4, 8},
    {"PARALLELISM_TOLERANCE", SCPMIS, 4, 10},
    {"PERPENDICULARITY_TOLERANCE", SCPMIS, 4, 10},
    {"ANGULARITY_TOLERANCE", SCPMIS, 4, 10},
    {"CONCENTRICITY_TOLERANCE", SCPMIS, 4, 10},
    {"SYMMETRY_TOLERANCE", SCPMIS, 4, 10},
    {"CIRCULAR_RUNOUT_TOLERANCE", SCPMIS, 4, 10},
    {"TOTAL_RUNOUT_TOLERANCE", SCPMIS, 4, 10},
    {"SURFACE_PROFILE_TOLERANCE", SCPMIS, 4, 10},
    {"LINE_PROFILE_TOLERANCE", SCPMIS, 4, 10},
    {"DATUM", SCPMIS, 4, 5},
    {"DATUM_FEATURE", SCPMIS, 5, 6},
    {"DATUM_TARGET", SCPMIS, 5, 7},
    {"DATUM_SYSTEM", SCPMIS, 2, 4},
    {"DATUM_REFERENCE", SCPMIS, 2, 4},
    {"DATUM_REFERENCE_COMPARTMENT", SCPMIS, 3, 6},
    {"DATUM_REFERENCE_ELEMENT", SCPMIS, 3, 7},
    {"SHAPE_ASPECT", SCPMIS, 5, 5},
    {"SHAPE_ASPECT_RELATIONSHIP", SCPMIS | SCR, 4, 4},
    {"DIMENSIONAL_SIZE", SCPMIS, 4, 6},
    {"DIMENSIONAL_LOCATION", SCPMIS, 4, 6},
    {"DIMENSIONAL_CHARACTERISTIC_REPRESENTATION", SCPMIS | SCR, 2, 2},
    {"SHAPE_DIMENSION_REPRESENTATION", SCPMIS, 3, 3},
    {"PLUS_MINUS_TOLERANCE", SCPMIS, 3, 4},
    {"LENGTH_MEASURE_WITH_UNIT", SCPMIS | SCU, 2, 2},
    {"MEASURE_REPRESENTATION_ITEM", SCPMIS, 2, 2},
    {"MECHANICAL_DESIGN_GEOMETRIC_PRESENTATION_REPRESENTATION", SCPMIG | SCPR, 3, 3},
    {"DRAUGHTING_MODEL", SCPMIG | SCPR, 3, 3},
    {"ANNOTATION_OCCURRENCE", SCPMIG | SCPR, 3, 5},
    {"ANNOTATION_PLANE", SCPMIG | SCPR, 3, 4},
    {"PRESENTATION_VIEW", SCPMIG | SCPR, 3, 4},
    {"CAMERA_MODEL_D3", SCPMIG | SCPR, 4, 4},
    {"STYLED_ITEM", SCPMIG | SCPR, 3, 3},
    {"PRESENTATION_STYLE_ASSIGNMENT", SCPMIG | SCPR, 1, 2},
    {"COLOUR_RGB", SCPMIG | SCPR, 4, 4},
    {"CURVE_STYLE", SCPMIG | SCPR, 4, 4},
    {"SURFACE_STYLE_USAGE", SCPMIG | SCPR, 3, 3},
    {"GEOMETRIC_VALIDATION_PROPERTY", SCV, 4, 4},
    {"VALIDATION_PROPERTY", SCV, 4, 4},
    {"VALIDATION_REPRESENTATION", SCV, 3, 3},
    {"SHAPE_DATA_QUALITY_ASSESSMENT_BY_NUMERICAL_TEST", SCV, 7, 12},
    {"APPLIED_GROUP_ASSIGNMENT", SC242 | SCR, 3, 3},
    {"APPLIED_PRESENTED_ITEM", SC242 | SCPR, 2, 3},
    {"APPLIED_CLASSIFICATION_ASSIGNMENT", SC242 | SCR, 3, 3},
    {"EXTERNAL_SOURCE", SC242, 2, 2},
    {"EXTERNALLY_DEFINED_ITEM", SC242, 3, 3},
    {"MATERIAL_DESIGNATION", SCM, 2, 2},
    {"MATERIAL_PROPERTY", SCM, 4, 4},
    {0, 0u, 0, 0}
};

static void sc_zero(void *p, int n);
static int sc_is_ws(char c);
static int sc_is_digit(char c);
static int sc_scan_string(const char *s, int end, int pos);
static int sc_scan_ref(const char *s, int end, int pos, int *ref, int *next);
static int sc_count_top_args(const char *s, int begin, int len);
static int sc_collect_refs_for_entity(step89_complete_context *ctx, const step89_model *m, step89_complete_node *node, const step89_entity *e);
static void sc_add_message(step89_complete_context *ctx, int sev, int ref, unsigned flags, const char *txt);
static void sc_copy_name(char *dst, int cap, const char *src);
static int sc_first_ref(const step89_model *m, const step89_entity *e);
static int sc_second_ref(const step89_model *m, const step89_entity *e);
static step89_fixed sc_first_fixed(const step89_model *m, const step89_entity *e);
static int sc_name_has(const char *name, const char *needle);
static int sc_get_node_index(const step89_complete_context *ctx, int ref);
static void sc_mark_inbound_and_dangling(step89_complete_context *ctx);
static void sc_check_schema_arity(step89_complete_context *ctx, const step89_model *m, const step89_entity *e, const step89_complete_schema_class *cls);
static void sc_add_nurbs(step89_complete_context *ctx, int ref, int kind, int degree_u, int degree_v, int control_count, int knot_count, int trim_ref, unsigned flags);

static void sc_zero(void *p, int n)
{
    unsigned char *q;
    int i;
    q = (unsigned char *)p;
    for (i = 0; i < n; ++i) q[i] = 0u;
}

void step89_complete_init(step89_complete_context *ctx)
{
    if (!ctx) return;
    sc_zero(ctx, (int)sizeof(*ctx));
    ctx->units.length_scale = STEP89_FIXED_ONE;
    ctx->units.angle_scale = STEP89_FIXED_ONE;
    ctx->units.model_tolerance = STEP89_COMPLETE_TOLERANCE_DEFAULT;
}

static int sc_is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static int sc_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int sc_scan_string(const char *s, int end, int pos)
{
    ++pos;
    while (pos < end) {
        if (s[pos] == '\'') {
            if (pos + 1 < end && s[pos + 1] == '\'') pos += 2;
            else return pos + 1;
        } else {
            ++pos;
        }
    }
    return pos;
}

static int sc_scan_ref(const char *s, int end, int pos, int *ref, int *next)
{
    long v;
    int p;
    if (ref) *ref = 0;
    if (next) *next = pos;
    if (!s || pos < 0 || pos >= end || s[pos] != '#') return 0;
    p = pos + 1;
    if (p >= end || !sc_is_digit(s[p])) return 0;
    v = 0L;
    while (p < end && sc_is_digit(s[p])) {
        if (v < 200000000L) v = v * 10L + (long)(s[p] - '0');
        ++p;
    }
    if (ref) *ref = (int)v;
    if (next) *next = p;
    return 1;
}

static void sc_copy_name(char *dst, int cap, const char *src)
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

static void sc_add_message(step89_complete_context *ctx, int sev, int ref, unsigned flags, const char *txt)
{
    step89_complete_message *m;
    if (!ctx) return;
    if (ctx->message_count >= STEP89_COMPLETE_MAX_MESSAGES) return;
    m = &ctx->messages[ctx->message_count];
    m->severity = sev;
    m->entity_ref = ref;
    m->class_flags = flags;
    sc_copy_name(m->text, STEP89_COMPLETE_MAX_MESSAGE_TEXT, txt);
    ++ctx->message_count;
}

const step89_complete_schema_class *step89_complete_find_schema_class(const char *entity_name)
{
    int i;
    if (!entity_name) return 0;
    for (i = 0; g_complete_classes[i].name != 0; ++i) {
        if (step89_name_eq(g_complete_classes[i].name, entity_name)) return &g_complete_classes[i];
    }
    return 0;
}

int step89_complete_entity_class_flags(const char *entity_name)
{
    const step89_complete_schema_class *c;
    c = step89_complete_find_schema_class(entity_name);
    if (!c) return 0;
    return (int)c->flags;
}

const step89_complete_node *step89_complete_find_node(const step89_complete_context *ctx, int ref)
{
    int i;
    if (!ctx) return 0;
    for (i = 0; i < ctx->node_count; ++i) {
        if (ctx->nodes[i].ref == ref) return &ctx->nodes[i];
    }
    return 0;
}

static int sc_get_node_index(const step89_complete_context *ctx, int ref)
{
    int i;
    if (!ctx) return -1;
    for (i = 0; i < ctx->node_count; ++i) {
        if (ctx->nodes[i].ref == ref) return i;
    }
    return -1;
}

static int sc_count_top_args(const char *s, int begin, int len)
{
    int i;
    int end;
    int depth;
    int count;
    int seen;
    if (!s || begin < 0 || len <= 0) return 0;
    i = begin;
    end = begin + len;
    depth = 0;
    count = 0;
    seen = 0;
    while (i < end) {
        if (s[i] == '\'') {
            i = sc_scan_string(s, end, i);
            if (depth >= 1) seen = 1;
        } else if (s[i] == '(') {
            ++depth;
            if (depth > 1) seen = 1;
            ++i;
        } else if (s[i] == ')') {
            if (depth > 1) seen = 1;
            if (depth > 0) --depth;
            ++i;
        } else if (s[i] == ',' && depth == 1) {
            ++count;
            seen = 0;
            ++i;
        } else {
            if (depth >= 1 && !sc_is_ws(s[i])) seen = 1;
            ++i;
        }
    }
    if (seen || count > 0) ++count;
    return count;
}

static int sc_collect_refs_for_entity(step89_complete_context *ctx, const step89_model *m, step89_complete_node *node, const step89_entity *e)
{
    int i;
    int end;
    int ref;
    int next;
    int arg_index;
    int depth;
    if (!ctx || !m || !node || !e) return 0;
    node->first_ref_index = ctx->ref_count;
    i = e->args_begin;
    end = e->args_begin + e->args_len;
    arg_index = 0;
    depth = 0;
    while (i < end) {
        if (m->text[i] == '\'') {
            i = sc_scan_string(m->text, end, i);
        } else if (m->text[i] == '(') {
            ++depth;
            ++i;
        } else if (m->text[i] == ')') {
            if (depth > 0) --depth;
            ++i;
        } else if (m->text[i] == ',' && depth == 1) {
            ++arg_index;
            ++i;
        } else if (m->text[i] == '#') {
            if (sc_scan_ref(m->text, end, i, &ref, &next)) {
                if (ctx->ref_count < STEP89_COMPLETE_MAX_REFS) {
                    ctx->refs[ctx->ref_count].owner_ref = e->id;
                    ctx->refs[ctx->ref_count].target_ref = ref;
                    ctx->refs[ctx->ref_count].arg_index = arg_index;
                    ctx->refs[ctx->ref_count].flags = 0u;
                    ++ctx->ref_count;
                    ++node->ref_count;
                }
                i = next;
            } else ++i;
        } else ++i;
    }
    return node->ref_count;
}

static void sc_mark_inbound_and_dangling(step89_complete_context *ctx)
{
    int i;
    int owner_i;
    int target_i;
    if (!ctx) return;
    for (i = 0; i < ctx->ref_count; ++i) {
        owner_i = sc_get_node_index(ctx, ctx->refs[i].owner_ref);
        target_i = sc_get_node_index(ctx, ctx->refs[i].target_ref);
        if (target_i >= 0) {
            ++ctx->nodes[target_i].inbound_count;
            ctx->nodes[target_i].flags |= STEP89_COMPLETE_NODE_HAS_INVERSE;
        } else {
            ++ctx->dangling_ref_count;
            if (owner_i >= 0) ctx->nodes[owner_i].flags |= STEP89_COMPLETE_NODE_DANGLING_REF;
            sc_add_message(ctx, STEP89_COMPLETE_MSG_ERROR, ctx->refs[i].owner_ref, 0u, "dangling reference in typed graph");
        }
    }
}

static void sc_check_schema_arity(step89_complete_context *ctx, const step89_model *m, const step89_entity *e, const step89_complete_schema_class *cls)
{
    int argc;
    if (!ctx || !m || !e || !cls) return;
    if (cls->min_args <= 0 && cls->max_args <= 0) return;
    argc = sc_count_top_args(m->text, e->args_begin, e->args_len);
    if (argc < cls->min_args || argc > cls->max_args) {
        ++ctx->where_error_count;
        sc_add_message(ctx, STEP89_COMPLETE_MSG_ERROR, e->id, cls->flags, "schema arity outside seed table range");
    }
}

static int sc_first_ref(const step89_model *m, const step89_entity *e)
{
    int i;
    int end;
    int ref;
    int next;
    if (!m || !e) return 0;
    i = e->args_begin;
    end = e->args_begin + e->args_len;
    while (i < end) {
        if (m->text[i] == '\'') i = sc_scan_string(m->text, end, i);
        else if (m->text[i] == '#') {
            if (sc_scan_ref(m->text, end, i, &ref, &next)) return ref;
            ++i;
        } else ++i;
    }
    return 0;
}

static int sc_second_ref(const step89_model *m, const step89_entity *e)
{
    int i;
    int end;
    int ref;
    int next;
    int count;
    if (!m || !e) return 0;
    i = e->args_begin;
    end = e->args_begin + e->args_len;
    count = 0;
    while (i < end) {
        if (m->text[i] == '\'') i = sc_scan_string(m->text, end, i);
        else if (m->text[i] == '#') {
            if (sc_scan_ref(m->text, end, i, &ref, &next)) {
                ++count;
                if (count == 2) return ref;
                i = next;
            } else ++i;
        } else ++i;
    }
    return 0;
}

static step89_fixed sc_first_fixed(const step89_model *m, const step89_entity *e)
{
    int i;
    int end;
    int neg;
    long ip;
    long fp;
    long fs;
    long val;
    int any;
    if (!m || !e) return 0L;
    i = e->args_begin;
    end = e->args_begin + e->args_len;
    while (i < end) {
        if (m->text[i] == '\'') {
            i = sc_scan_string(m->text, end, i);
        } else if (m->text[i] == '-' || m->text[i] == '+' || m->text[i] == '.' || sc_is_digit(m->text[i])) {
            neg = 0;
            if (m->text[i] == '-' || m->text[i] == '+') {
                if (m->text[i] == '-') neg = 1;
                ++i;
            }
            ip = 0L;
            fp = 0L;
            fs = STEP89_FIXED_SCALE / 10L;
            any = 0;
            while (i < end && sc_is_digit(m->text[i])) {
                any = 1;
                if (ip < 200000000L) ip = ip * 10L + (long)(m->text[i] - '0');
                ++i;
            }
            if (i < end && m->text[i] == '.') {
                ++i;
                while (i < end && sc_is_digit(m->text[i])) {
                    any = 1;
                    if (fs > 0L) {
                        fp += (long)(m->text[i] - '0') * fs;
                        fs /= 10L;
                    }
                    ++i;
                }
            }
            if (any) {
                val = ip * STEP89_FIXED_SCALE + fp;
                if (neg) val = -val;
                return (step89_fixed)val;
            }
        } else ++i;
    }
    return 0L;
}

static int sc_name_has(const char *name, const char *needle)
{
    int i;
    int j;
    if (!name || !needle) return 0;
    for (i = 0; name[i] != '\0'; ++i) {
        j = 0;
        while (needle[j] != '\0' && name[i + j] != '\0') {
            char a;
            char b;
            a = name[i + j];
            b = needle[j];
            if (a >= 'a' && a <= 'z') a = (char)(a - ('a' - 'A'));
            if (b >= 'a' && b <= 'z') b = (char)(b - ('a' - 'A'));
            if (a != b) break;
            ++j;
        }
        if (needle[j] == '\0') return 1;
    }
    return 0;
}

int step89_complete_pmi_kind(const char *entity_name)
{
    if (!entity_name) return STEP89_COMPLETE_PMI_UNKNOWN;
    if (sc_name_has(entity_name, "DATUM")) return STEP89_COMPLETE_PMI_DATUM;
    if (sc_name_has(entity_name, "TOLERANCE")) return STEP89_COMPLETE_PMI_TOLERANCE;
    if (sc_name_has(entity_name, "DIMENSION")) return STEP89_COMPLETE_PMI_DIMENSION;
    if (sc_name_has(entity_name, "ANNOTATION")) return STEP89_COMPLETE_PMI_ANNOTATION;
    if (sc_name_has(entity_name, "PRESENTATION") || sc_name_has(entity_name, "DRAUGHTING") || sc_name_has(entity_name, "CAMERA")) return STEP89_COMPLETE_PMI_PRESENTATION;
    if (sc_name_has(entity_name, "GROUP")) return STEP89_COMPLETE_PMI_GROUP;
    return STEP89_COMPLETE_PMI_UNKNOWN;
}

step89_result step89_complete_collect_units(step89_complete_context *ctx, const step89_model *m)
{
    int i;
    const step89_entity *e;
    step89_complete_unit *u;
    if (!ctx || !m) return STEP89_ERR_NULL;
    for (i = 0; i < m->entity_count; ++i) {
        e = &m->entities[i];
        if ((step89_complete_entity_class_flags(e->name) & STEP89_COMPLETE_CLASS_UNIT) != 0) {
            if (ctx->units.unit_count < STEP89_COMPLETE_MAX_UNITS) {
                u = &ctx->units.units[ctx->units.unit_count];
                u->ref = e->id;
                u->scale_to_model = STEP89_FIXED_ONE;
                sc_copy_name(u->name, STEP89_MAX_ENTITY_NAME, e->name);
                if (sc_name_has(e->name, "LENGTH")) u->kind = STEP89_COMPLETE_UNIT_LENGTH;
                else if (sc_name_has(e->name, "ANGLE")) u->kind = STEP89_COMPLETE_UNIT_ANGLE;
                else if (sc_name_has(e->name, "SOLID")) u->kind = STEP89_COMPLETE_UNIT_SOLID;
                else if (sc_name_has(e->name, "PLANE")) u->kind = STEP89_COMPLETE_UNIT_PLANE;
                else u->kind = STEP89_COMPLETE_UNIT_RATIO;
                if (ctx->units.length_unit_ref == 0 && u->kind == STEP89_COMPLETE_UNIT_LENGTH) ctx->units.length_unit_ref = u->ref;
                if (ctx->units.angle_unit_ref == 0 && u->kind == STEP89_COMPLETE_UNIT_ANGLE) ctx->units.angle_unit_ref = u->ref;
                ++ctx->units.unit_count;
            }
        }
        if (step89_name_eq(e->name, "UNCERTAINTY_MEASURE_WITH_UNIT")) {
            ctx->units.model_tolerance = sc_first_fixed(m, e);
            if (ctx->units.model_tolerance <= 0L) ctx->units.model_tolerance = STEP89_COMPLETE_TOLERANCE_DEFAULT;
        }
    }
    return STEP89_OK;
}

step89_result step89_complete_collect_pmi(step89_complete_context *ctx, const step89_model *m)
{
    int i;
    int flags;
    const step89_entity *e;
    step89_complete_pmi *p;
    if (!ctx || !m) return STEP89_ERR_NULL;
    for (i = 0; i < m->entity_count; ++i) {
        e = &m->entities[i];
        flags = step89_complete_entity_class_flags(e->name);
        if ((flags & (STEP89_COMPLETE_CLASS_PMI_SEM | STEP89_COMPLETE_CLASS_PMI_GFX)) != 0) {
            if (ctx->pmi_count < STEP89_COMPLETE_MAX_PMI) {
                p = &ctx->pmi[ctx->pmi_count];
                p->ref = e->id;
                p->kind = step89_complete_pmi_kind(e->name);
                p->semantic = (flags & STEP89_COMPLETE_CLASS_PMI_SEM) != 0;
                p->graphic = (flags & STEP89_COMPLETE_CLASS_PMI_GFX) != 0;
                p->anchor_ref = sc_first_ref(m, e);
                p->presentation_ref = sc_second_ref(m, e);
                p->flags = 0u;
                ++ctx->pmi_count;
            }
        }
    }
    return STEP89_OK;
}

static void sc_add_nurbs(step89_complete_context *ctx, int ref, int kind, int degree_u, int degree_v, int control_count, int knot_count, int trim_ref, unsigned flags)
{
    step89_complete_nurbs_seed *n;
    if (!ctx) return;
    if (ctx->nurbs_count >= STEP89_COMPLETE_MAX_NURBS) return;
    n = &ctx->nurbs[ctx->nurbs_count];
    n->ref = ref;
    n->kind = kind;
    n->degree_u = degree_u;
    n->degree_v = degree_v;
    n->control_count = control_count;
    n->knot_count = knot_count;
    n->trim_ref = trim_ref;
    n->flags = flags;
    ++ctx->nurbs_count;
}

step89_result step89_complete_collect_nurbs_and_trims(step89_complete_context *ctx, const step89_model *m)
{
    int i;
    const step89_entity *e;
    const step89_curve *c;
    const step89_surface *s;
    if (!ctx || !m) return STEP89_ERR_NULL;
    for (i = 0; i < m->curve_count; ++i) {
        c = &m->curves[i];
        if (c->kind == STEP89_CURVE_BSPLINE) {
            sc_add_nurbs(ctx, c->ref, STEP89_COMPLETE_NURBS_CURVE, c->degree, 0, c->control_count, 0, 0, 0u);
        }
    }
    for (i = 0; i < m->surface_count; ++i) {
        s = &m->surfaces[i];
        if (s->kind == STEP89_SURFACE_BSPLINE) {
            sc_add_nurbs(ctx, s->ref, STEP89_COMPLETE_NURBS_SURFACE, s->degree_u, s->degree_v, s->control_count, 0, 0, 0u);
        }
    }
    for (i = 0; i < m->entity_count; ++i) {
        e = &m->entities[i];
        if (step89_name_eq(e->name, "TRIMMED_CURVE") || step89_name_eq(e->name, "PCURVE") || step89_name_eq(e->name, "SURFACE_CURVE") || step89_name_eq(e->name, "CURVE_BOUNDED_SURFACE") || step89_name_eq(e->name, "RECTANGULAR_TRIMMED_SURFACE")) {
            sc_add_nurbs(ctx, e->id, STEP89_COMPLETE_NURBS_TRIM, 0, 0, 0, 0, sc_first_ref(m, e), 0u);
        }
    }
    return STEP89_OK;
}

step89_result step89_complete_collect_shells_solids(step89_complete_context *ctx, const step89_model *m)
{
    int i;
    const step89_entity *e;
    step89_complete_shell *sh;
    step89_complete_solid *so;
    if (!ctx || !m) return STEP89_ERR_NULL;
    for (i = 0; i < m->entity_count; ++i) {
        e = &m->entities[i];
        if (step89_name_eq(e->name, "CLOSED_SHELL") || step89_name_eq(e->name, "OPEN_SHELL")) {
            if (ctx->shell_count < STEP89_COMPLETE_MAX_SHELLS) {
                sh = &ctx->shells[ctx->shell_count];
                sh->ref = e->id;
                sh->closed = step89_name_eq(e->name, "CLOSED_SHELL") ? STEP89_COMPLETE_SHELL_CLOSED : STEP89_COMPLETE_SHELL_OPEN;
                sh->first_face_ref = sc_first_ref(m, e);
                sh->face_count = 0;
                if (m->text) {
                    int k;
                    int end;
                    int ref;
                    int next;
                    k = e->args_begin;
                    end = e->args_begin + e->args_len;
                    while (k < end) {
                        if (m->text[k] == '\'') k = sc_scan_string(m->text, end, k);
                        else if (m->text[k] == '#') {
                            if (sc_scan_ref(m->text, end, k, &ref, &next)) {
                                ++sh->face_count;
                                k = next;
                            } else ++k;
                        } else ++k;
                    }
                }
                sh->flags = 0u;
                ++ctx->shell_count;
            }
        } else if (step89_name_eq(e->name, "MANIFOLD_SOLID_BREP") || step89_name_eq(e->name, "BREP_WITH_VOIDS")) {
            if (ctx->solid_count < STEP89_COMPLETE_MAX_SOLIDS) {
                so = &ctx->solids[ctx->solid_count];
                so->ref = e->id;
                so->shell_ref = sc_first_ref(m, e);
                so->valid_shell = step89_find_entity(m, so->shell_ref) != 0;
                so->flags = 0u;
                ++ctx->solid_count;
            }
        }
    }
    return STEP89_OK;
}

step89_result step89_complete_collect_validation_properties(step89_complete_context *ctx, const step89_model *m)
{
    int i;
    const step89_entity *e;
    step89_complete_validation_property *v;
    if (!ctx || !m) return STEP89_ERR_NULL;
    for (i = 0; i < m->entity_count; ++i) {
        e = &m->entities[i];
        if ((step89_complete_entity_class_flags(e->name) & STEP89_COMPLETE_CLASS_VALIDATION) != 0) {
            if (ctx->validation_property_count < STEP89_COMPLETE_MAX_VALIDATION_PROPS) {
                v = &ctx->validation_props[ctx->validation_property_count];
                v->ref = e->id;
                v->target_ref = sc_first_ref(m, e);
                v->kind = 0;
                v->value = sc_first_fixed(m, e);
                v->flags = 0u;
                ++ctx->validation_property_count;
            }
        }
    }
    return STEP89_OK;
}

step89_result step89_complete_run_where_rules(step89_complete_context *ctx, const step89_model *m, const step89_cad_model *cad)
{
    int i;
    const step89_curve *c;
    const step89_surface *s;
    const step89_complete_pmi *p;
    if (!ctx || !m) return STEP89_ERR_NULL;
    for (i = 0; i < m->curve_count; ++i) {
        c = &m->curves[i];
        if (c->kind == STEP89_CURVE_BSPLINE) {
            if (c->degree <= 0 || c->control_count <= c->degree) {
                ++ctx->where_error_count;
                sc_add_message(ctx, STEP89_COMPLETE_MSG_ERROR, c->ref, STEP89_COMPLETE_CLASS_NURBS, "B-spline curve degree/control WHERE seed failed");
            }
        }
    }
    for (i = 0; i < m->surface_count; ++i) {
        s = &m->surfaces[i];
        if (s->kind == STEP89_SURFACE_BSPLINE) {
            if (s->degree_u <= 0 || s->degree_v <= 0 || s->control_count <= 0) {
                ++ctx->where_error_count;
                sc_add_message(ctx, STEP89_COMPLETE_MSG_ERROR, s->ref, STEP89_COMPLETE_CLASS_NURBS, "B-spline surface degree/control WHERE seed failed");
            }
        }
    }
    for (i = 0; i < ctx->pmi_count; ++i) {
        p = &ctx->pmi[i];
        if (p->anchor_ref != 0 && !step89_find_entity(m, p->anchor_ref)) {
            ++ctx->inverse_error_count;
            sc_add_message(ctx, STEP89_COMPLETE_MSG_ERROR, p->ref, STEP89_COMPLETE_CLASS_PMI_SEM, "PMI anchor reference missing");
        }
    }
    if (cad) {
        if (cad->open_loop_count > 0) {
            ctx->topology_heal_hint_count += cad->open_loop_count;
            sc_add_message(ctx, STEP89_COMPLETE_MSG_WARN, 0, STEP89_COMPLETE_CLASS_TOPOLOGY, "CAD topology has open loops; sewing/healing needed");
        }
        if (cad->zero_edge_count > 0) {
            ctx->topology_heal_hint_count += cad->zero_edge_count;
            sc_add_message(ctx, STEP89_COMPLETE_MSG_WARN, 0, STEP89_COMPLETE_CLASS_TOPOLOGY, "CAD topology has zero-length edges; tolerance merge needed");
        }
        if (cad->nonmanifold_edge_count > 0) {
            ctx->topology_heal_hint_count += cad->nonmanifold_edge_count;
            sc_add_message(ctx, STEP89_COMPLETE_MSG_WARN, 0, STEP89_COMPLETE_CLASS_TOPOLOGY, "CAD topology has nonmanifold edges");
        }
    }
    return STEP89_OK;
}

step89_result step89_complete_analyze(step89_complete_context *ctx, const step89_model *m, const step89_cad_model *cad, const step89_validate_report *report)
{
    int i;
    const step89_entity *e;
    const step89_complete_schema_class *cls;
    step89_complete_node *node;
    (void)report;
    if (!ctx || !m) return STEP89_ERR_NULL;
    step89_complete_init(ctx);
    if (m->entity_count > STEP89_COMPLETE_MAX_NODES) return STEP89_ERR_LIMIT;
    for (i = 0; i < m->entity_count; ++i) {
        e = &m->entities[i];
        node = &ctx->nodes[ctx->node_count];
        node->ref = e->id;
        node->entity_index = i;
        node->schema_index = -1;
        node->class_flags = 0u;
        cls = step89_complete_find_schema_class(e->name);
        if (cls) {
            int k;
            node->class_flags = cls->flags;
            for (k = 0; g_complete_classes[k].name != 0; ++k) {
                if (&g_complete_classes[k] == cls) {
                    node->schema_index = k;
                    break;
                }
            }
            sc_check_schema_arity(ctx, m, e, cls);
        } else {
            ++ctx->unknown_class_count;
            node->flags |= STEP89_COMPLETE_NODE_UNKNOWN_CLASS;
            sc_add_message(ctx, STEP89_COMPLETE_MSG_WARN, e->id, 0u, "entity not in complete seed schema table");
        }
        sc_collect_refs_for_entity(ctx, m, node, e);
        ++ctx->node_count;
    }
    sc_mark_inbound_and_dangling(ctx);
    step89_complete_collect_units(ctx, m);
    step89_complete_collect_pmi(ctx, m);
    step89_complete_collect_nurbs_and_trims(ctx, m);
    step89_complete_collect_shells_solids(ctx, m);
    step89_complete_collect_validation_properties(ctx, m);
    step89_complete_run_where_rules(ctx, m, cad);
    if (!step89_validate_is_ap242_schema(m) && ctx->pmi_count > 0) {
        ++ctx->ap242_warning_count;
        sc_add_message(ctx, STEP89_COMPLETE_MSG_WARN, 0, STEP89_COMPLETE_CLASS_AP242, "PMI present but FILE_SCHEMA is not AP242-like");
    }
    ctx->derive_count = ctx->units.unit_count + ctx->pmi_count + ctx->nurbs_count + ctx->shell_count + ctx->solid_count + ctx->validation_property_count;
    return STEP89_OK;
}

#undef SCF
#undef SCT
#undef SCP
#undef SCA
#undef SC242
#undef SCPMIS
#undef SCPMIG
#undef SCU
#undef SCV
#undef SCPR
#undef SCN
#undef SCC
#undef SCM
#undef SCR
