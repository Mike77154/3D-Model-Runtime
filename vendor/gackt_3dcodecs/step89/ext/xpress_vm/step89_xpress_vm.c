#include "step89_xpress_vm.h"

static void xp_zero(void *p, int n);
static int xp_is_ws(char c);
static int xp_is_alpha(char c);
static int xp_is_digit(char c);
static int xp_is_ident(char c);
static char xp_upper(char c);
static int xp_name_eq(const char *a, const char *b);
static int xp_name_has(const char *name, const char *needle);
static void xp_copy(char *dst, int cap, const char *src);
static int xp_match_word_at(const char *s, int n, int pos, const char *word);
static int xp_find_word(const char *s, int n, int start, int stop, const char *word);
static int xp_skip_ws(const char *s, int n, int pos);
static int xp_read_ident(const char *s, int n, int pos, char *out, int cap);
static int xp_scan_string(const char *s, int end, int pos);
static int xp_find_semicolon(const char *s, int n, int pos, int stop);
static int xp_find_char(const char *s, int start, int stop, char ch);
static void xp_copy_range_trim(char *dst, int cap, const char *s, int begin, int end);
static int xp_count_top_args(const char *s, int begin, int len);
static int xp_scan_ref(const char *s, int end, int pos, int *ref, int *next);
static int xp_first_ref(const step89_model *m, const step89_entity *e);
static int xp_last_ref(const step89_model *m, const step89_entity *e);
static int xp_entity_has_ref(const step89_model *m, const step89_entity *e, int target_ref);
static int xp_entity_ref_count(const step89_model *m, const step89_entity *e);
static step89_fixed xp_abs_fixed(step89_fixed v);
static step89_fixed xp_fixed_mul(step89_fixed a, step89_fixed b);
static step89_fixed xp_fixed_div(step89_fixed a, step89_fixed b);
static step89_fixed xp_fixed_lerp(step89_fixed a, step89_fixed b, step89_fixed t);
static int xp_add_entity(step89_xp_schema *s, const char *name, const char *super_name, unsigned flags, int min_args, int max_args);
static int xp_add_attr(step89_xp_schema *s, int owner, const char *name, const char *type_name, unsigned flags);
static int xp_add_rule(step89_xp_schema *s, int owner, int kind, const char *name, const char *expr, int severity);
static int xp_add_op(step89_xp_schema *s, int rule_index, int opcode, int a, int b, int c);
static void xp_compile_rule_expr(step89_xp_schema *s, int rule_index);
static void xp_add_msg(step89_xp_vm_report *r, int sev, int ref, const char *entity_name, const char *rule_name, const char *txt);
static const step89_entity *xp_find_step_entity(const step89_model *m, int ref);
static int xp_get_node_inbound(const step89_complete_context *c, int ref);
static int xp_pmi_has_map(const step89_xp_pmi_report *pmi, int semantic_ref);
static int xp_vertex_close(const step89_cad_vertex *a, const step89_cad_vertex *b, step89_fixed tol);
static int xp_edge_same_dir(const step89_cad_model *cad, const step89_cad_edge *a, const step89_cad_edge *b, step89_fixed tol);
static int xp_edge_reversed(const step89_cad_model *cad, const step89_cad_edge *a, const step89_cad_edge *b, step89_fixed tol);
static const step89_xp_pmi_report *g_vm_pmi_report;

static void xp_zero(void *p, int n)
{
    unsigned char *q;
    int i;
    q = (unsigned char *)p;
    for (i = 0; i < n; ++i) q[i] = 0u;
}

static int xp_is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static int xp_is_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int xp_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int xp_is_ident(char c)
{
    return xp_is_alpha(c) || xp_is_digit(c) || c == '_';
}

static char xp_upper(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - ('a' - 'A'));
    return c;
}

static int xp_name_eq(const char *a, const char *b)
{
    int i;
    if (!a || !b) return 0;
    i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (xp_upper(a[i]) != xp_upper(b[i])) return 0;
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int xp_name_has(const char *name, const char *needle)
{
    int i;
    int j;
    if (!name || !needle) return 0;
    for (i = 0; name[i] != '\0'; ++i) {
        j = 0;
        while (needle[j] != '\0' && name[i + j] != '\0') {
            if (xp_upper(name[i + j]) != xp_upper(needle[j])) break;
            ++j;
        }
        if (needle[j] == '\0') return 1;
    }
    return 0;
}

static void xp_copy(char *dst, int cap, const char *src)
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

static int xp_match_word_at(const char *s, int n, int pos, const char *word)
{
    int i;
    if (!s || !word || pos < 0 || pos >= n) return 0;
    i = 0;
    while (word[i] != '\0') {
        if (pos + i >= n) return 0;
        if (xp_upper(s[pos + i]) != xp_upper(word[i])) return 0;
        ++i;
    }
    if (pos > 0 && xp_is_ident(s[pos - 1])) return 0;
    if (pos + i < n && xp_is_ident(s[pos + i])) return 0;
    return 1;
}

static int xp_find_word(const char *s, int n, int start, int stop, const char *word)
{
    int i;
    if (!s || !word) return -1;
    if (stop <= 0 || stop > n) stop = n;
    if (start < 0) start = 0;
    i = start;
    while (i < stop) {
        if (s[i] == '\'') {
            i = xp_scan_string(s, stop, i);
        } else {
            if (xp_match_word_at(s, n, i, word)) return i;
            ++i;
        }
    }
    return -1;
}

static int xp_skip_ws(const char *s, int n, int pos)
{
    while (pos < n && xp_is_ws(s[pos])) ++pos;
    return pos;
}

static int xp_read_ident(const char *s, int n, int pos, char *out, int cap)
{
    int k;
    pos = xp_skip_ws(s, n, pos);
    if (pos >= n || !xp_is_alpha(s[pos])) return -1;
    k = 0;
    while (pos < n && xp_is_ident(s[pos])) {
        if (k + 1 < cap) {
            out[k] = xp_upper(s[pos]);
            ++k;
        }
        ++pos;
    }
    if (cap > 0) out[k] = '\0';
    return pos;
}

static int xp_scan_string(const char *s, int end, int pos)
{
    ++pos;
    while (pos < end) {
        if (s[pos] == '\'') {
            if (pos + 1 < end && s[pos + 1] == '\'') pos += 2;
            else return pos + 1;
        } else ++pos;
    }
    return pos;
}

static int xp_find_semicolon(const char *s, int n, int pos, int stop)
{
    int depth;
    if (!s) return -1;
    if (stop <= 0 || stop > n) stop = n;
    depth = 0;
    while (pos < stop) {
        if (s[pos] == '\'') pos = xp_scan_string(s, stop, pos);
        else if (s[pos] == '(') { ++depth; ++pos; }
        else if (s[pos] == ')') { if (depth > 0) --depth; ++pos; }
        else if (s[pos] == ';' && depth == 0) return pos;
        else ++pos;
    }
    return -1;
}

static int xp_find_char(const char *s, int start, int stop, char ch)
{
    int i;
    for (i = start; i < stop; ++i) if (s[i] == ch) return i;
    return -1;
}

static void xp_copy_range_trim(char *dst, int cap, const char *s, int begin, int end)
{
    int i;
    int k;
    if (!dst || cap <= 0) return;
    dst[0] = '\0';
    if (!s) return;
    while (begin < end && xp_is_ws(s[begin])) ++begin;
    while (end > begin && xp_is_ws(s[end - 1])) --end;
    k = 0;
    for (i = begin; i < end && k + 1 < cap; ++i) {
        dst[k] = xp_upper(s[i]);
        ++k;
    }
    dst[k] = '\0';
}

static int xp_count_top_args(const char *s, int begin, int len)
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
            i = xp_scan_string(s, end, i);
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
            if (depth >= 1 && !xp_is_ws(s[i])) seen = 1;
            ++i;
        }
    }
    if (seen || count > 0) ++count;
    return count;
}

static int xp_scan_ref(const char *s, int end, int pos, int *ref, int *next)
{
    long v;
    int p;
    if (ref) *ref = 0;
    if (next) *next = pos;
    if (!s || pos < 0 || pos >= end || s[pos] != '#') return 0;
    p = pos + 1;
    if (p >= end || !xp_is_digit(s[p])) return 0;
    v = 0L;
    while (p < end && xp_is_digit(s[p])) {
        if (v < 200000000L) v = v * 10L + (long)(s[p] - '0');
        ++p;
    }
    if (ref) *ref = (int)v;
    if (next) *next = p;
    return 1;
}

static const step89_entity *xp_find_step_entity(const step89_model *m, int ref)
{
    return step89_find_entity(m, ref);
}

static int xp_first_ref(const step89_model *m, const step89_entity *e)
{
    int i;
    int end;
    int ref;
    int next;
    if (!m || !e) return 0;
    i = e->args_begin;
    end = e->args_begin + e->args_len;
    while (i < end) {
        if (m->text[i] == '\'') i = xp_scan_string(m->text, end, i);
        else if (m->text[i] == '#') {
            if (xp_scan_ref(m->text, end, i, &ref, &next)) return ref;
            ++i;
        } else ++i;
    }
    return 0;
}

static int xp_last_ref(const step89_model *m, const step89_entity *e)
{
    int i;
    int end;
    int ref;
    int next;
    int last;
    if (!m || !e) return 0;
    i = e->args_begin;
    end = e->args_begin + e->args_len;
    last = 0;
    while (i < end) {
        if (m->text[i] == '\'') i = xp_scan_string(m->text, end, i);
        else if (m->text[i] == '#') {
            if (xp_scan_ref(m->text, end, i, &ref, &next)) {
                last = ref;
                i = next;
            } else ++i;
        } else ++i;
    }
    return last;
}

static int xp_entity_has_ref(const step89_model *m, const step89_entity *e, int target_ref)
{
    int i;
    int end;
    int ref;
    int next;
    if (!m || !e || target_ref == 0) return 0;
    i = e->args_begin;
    end = e->args_begin + e->args_len;
    while (i < end) {
        if (m->text[i] == '\'') i = xp_scan_string(m->text, end, i);
        else if (m->text[i] == '#') {
            if (xp_scan_ref(m->text, end, i, &ref, &next)) {
                if (ref == target_ref) return 1;
                i = next;
            } else ++i;
        } else ++i;
    }
    return 0;
}

static int xp_entity_ref_count(const step89_model *m, const step89_entity *e)
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
        if (m->text[i] == '\'') i = xp_scan_string(m->text, end, i);
        else if (m->text[i] == '#') {
            if (xp_scan_ref(m->text, end, i, &ref, &next)) {
                (void)ref;
                ++count;
                i = next;
            } else ++i;
        } else ++i;
    }
    return count;
}

static step89_fixed xp_abs_fixed(step89_fixed v)
{
    return v < 0L ? -v : v;
}

static step89_fixed xp_fixed_mul(step89_fixed a, step89_fixed b)
{
    return (step89_fixed)((a * b) / STEP89_FIXED_SCALE);
}

static step89_fixed xp_fixed_div(step89_fixed a, step89_fixed b)
{
    if (b == 0L) return 0L;
    return (step89_fixed)((a * STEP89_FIXED_SCALE) / b);
}

static step89_fixed xp_fixed_lerp(step89_fixed a, step89_fixed b, step89_fixed t)
{
    return a + xp_fixed_mul(b - a, t);
}

void step89_xp_schema_init(step89_xp_schema *s)
{
    if (!s) return;
    xp_zero(s, (int)sizeof(*s));
}

void step89_xp_vm_report_init(step89_xp_vm_report *r)
{
    if (!r) return;
    xp_zero(r, (int)sizeof(*r));
}

void step89_xp_sew_report_init(step89_xp_sew_report *r)
{
    if (!r) return;
    xp_zero(r, (int)sizeof(*r));
    r->tolerance = STEP89_COMPLETE_TOLERANCE_DEFAULT;
}

void step89_xp_pmi_report_init(step89_xp_pmi_report *r)
{
    if (!r) return;
    xp_zero(r, (int)sizeof(*r));
}

void step89_xp_ap242_report_init(step89_xp_ap242_report *r)
{
    if (!r) return;
    xp_zero(r, (int)sizeof(*r));
}

void step89_xp_analysis_init(step89_xp_analysis *a)
{
    if (!a) return;
    xp_zero(a, (int)sizeof(*a));
    step89_xp_schema_init(&a->schema);
    step89_xp_vm_report_init(&a->vm);
    step89_xp_sew_report_init(&a->sew);
    step89_xp_pmi_report_init(&a->pmi);
    step89_xp_ap242_report_init(&a->ap242);
}

static int xp_add_entity(step89_xp_schema *s, const char *name, const char *super_name, unsigned flags, int min_args, int max_args)
{
    step89_xp_entity_def *e;
    int i;
    if (!s || !name) return -1;
    for (i = 0; i < s->entity_count; ++i) {
        if (xp_name_eq(s->entities[i].name, name)) {
            s->entities[i].flags |= flags;
            if (super_name && super_name[0] != '\0') xp_copy(s->entities[i].super_name, STEP89_XP_MAX_NAME, super_name);
            if (min_args >= 0) s->entities[i].min_args = min_args;
            if (max_args >= 0) s->entities[i].max_args = max_args;
            return i;
        }
    }
    if (s->entity_count >= STEP89_XP_MAX_ENTITIES) {
        ++s->parse_error_count;
        return -1;
    }
    e = &s->entities[s->entity_count];
    xp_copy(e->name, STEP89_XP_MAX_NAME, name);
    xp_copy(e->super_name, STEP89_XP_MAX_NAME, super_name ? super_name : "");
    e->flags = flags;
    e->first_attr = s->attr_count;
    e->first_rule = s->rule_count;
    e->min_args = min_args;
    e->max_args = max_args;
    ++s->entity_count;
    return s->entity_count - 1;
}

static int xp_add_attr(step89_xp_schema *s, int owner, const char *name, const char *type_name, unsigned flags)
{
    step89_xp_attr *a;
    if (!s || owner < 0 || owner >= s->entity_count || !name) return -1;
    if (s->attr_count >= STEP89_XP_MAX_ATTRS) {
        ++s->parse_error_count;
        return -1;
    }
    a = &s->attrs[s->attr_count];
    a->owner_index = owner;
    xp_copy(a->name, STEP89_XP_MAX_NAME, name);
    xp_copy(a->type_name, STEP89_XP_MAX_NAME, type_name ? type_name : "");
    a->flags = flags;
    if (flags & STEP89_XP_ATTR_DERIVED) ++s->entities[owner].derived_attr_count;
    else if (flags & STEP89_XP_ATTR_INVERSE) ++s->entities[owner].inverse_attr_count;
    else ++s->entities[owner].explicit_attr_count;
    return s->attr_count++;
}

static int xp_add_rule(step89_xp_schema *s, int owner, int kind, const char *name, const char *expr, int severity)
{
    step89_xp_rule *r;
    if (!s || owner < 0 || owner >= s->entity_count) return -1;
    if (s->rule_count >= STEP89_XP_MAX_RULES) {
        ++s->parse_error_count;
        return -1;
    }
    r = &s->rules[s->rule_count];
    r->owner_index = owner;
    r->kind = kind;
    xp_copy(r->name, STEP89_XP_MAX_NAME, name ? name : "RULE");
    xp_copy(r->expr, STEP89_XP_MAX_EXPR, expr ? expr : "");
    r->first_op = s->op_count;
    r->severity = severity;
    ++s->entities[owner].rule_count;
    return s->rule_count++;
}

static int xp_add_op(step89_xp_schema *s, int rule_index, int opcode, int a, int b, int c)
{
    step89_xp_op *op;
    if (!s || rule_index < 0 || rule_index >= s->rule_count) return -1;
    if (s->op_count >= STEP89_XP_MAX_OPS) {
        ++s->parse_error_count;
        return -1;
    }
    op = &s->ops[s->op_count];
    op->opcode = opcode;
    op->a = a;
    op->b = b;
    op->c = c;
    ++s->rules[rule_index].op_count;
    return s->op_count++;
}

const step89_xp_entity_def *step89_xp_find_entity_def(const step89_xp_schema *schema, const char *name)
{
    int i;
    if (!schema || !name) return 0;
    for (i = 0; i < schema->entity_count; ++i) {
        if (xp_name_eq(schema->entities[i].name, name)) return &schema->entities[i];
    }
    return 0;
}

static void xp_compile_rule_expr(step89_xp_schema *s, int rule_index)
{
    step89_xp_rule *r;
    if (!s || rule_index < 0 || rule_index >= s->rule_count) return;
    r = &s->rules[rule_index];
    if (xp_name_has(r->expr, "EXISTS") || xp_name_has(r->expr, "REFERENCE")) xp_add_op(s, rule_index, STEP89_XP_OP_ENTITY_REFS_EXIST, 0, 0, 0);
    else if (xp_name_has(r->expr, "SIZEOF") || xp_name_has(r->expr, "HIINDEX") || xp_name_has(r->expr, "LOINDEX")) xp_add_op(s, rule_index, STEP89_XP_OP_ARG_RANGE, 1, 200000000, 0);
    else if (xp_name_has(r->expr, "INVERSE") || xp_name_has(r->expr, "USEDIN")) xp_add_op(s, rule_index, STEP89_XP_OP_INBOUND_AT_LEAST, 1, 0, 0);
    else xp_add_op(s, rule_index, STEP89_XP_OP_TRUE, 0, 0, 0);
}

step89_result step89_xp_parse_express(step89_xp_schema *schema, const char *text, int text_len)
{
    int p;
    int n;
    int schema_pos;
    int entity_pos;
    char name[STEP89_XP_MAX_NAME];
    if (!schema || !text) return STEP89_ERR_NULL;
    step89_xp_schema_init(schema);
    n = text_len;
    if (n < 0) return STEP89_ERR_SYNTAX;
    schema_pos = xp_find_word(text, n, 0, n, "SCHEMA");
    if (schema_pos >= 0) {
        p = xp_read_ident(text, n, schema_pos + 6, name, STEP89_XP_MAX_NAME);
        if (p >= 0) xp_copy(schema->schema_name, STEP89_XP_MAX_NAME, name);
    }
    p = 0;
    while (p < n) {
        int end_entity;
        int owner;
        int header_end;
        int block_p;
        char super_name[STEP89_XP_MAX_NAME];
        unsigned flags;
        entity_pos = xp_find_word(text, n, p, n, "ENTITY");
        if (entity_pos < 0) break;
        end_entity = xp_find_word(text, n, entity_pos, n, "END_ENTITY");
        if (end_entity < 0) {
            ++schema->parse_error_count;
            break;
        }
        p = xp_read_ident(text, n, entity_pos + 6, name, STEP89_XP_MAX_NAME);
        if (p < 0) {
            ++schema->parse_error_count;
            p = end_entity + 10;
            continue;
        }
        super_name[0] = '\0';
        flags = STEP89_XP_ENTITY_FROM_EXPRESS;
        header_end = xp_find_semicolon(text, n, p, end_entity);
        if (header_end < 0) header_end = p;
        if (xp_find_word(text, n, p, header_end, "ABSTRACT") >= 0) flags |= STEP89_XP_ENTITY_ABSTRACT;
        {
            int sub;
            int lp;
            int rp;
            sub = xp_find_word(text, n, p, header_end, "SUBTYPE");
            if (sub >= 0) {
                lp = xp_find_char(text, sub, header_end, '(');
                rp = xp_find_char(text, lp + 1, header_end, ')');
                if (lp >= 0 && rp > lp) {
                    xp_read_ident(text, n, lp + 1, super_name, STEP89_XP_MAX_NAME);
                    flags |= STEP89_XP_ENTITY_HAS_SUPERTYPE;
                }
            }
        }
        owner = xp_add_entity(schema, name, super_name, flags, 0, 200000000);
        if (owner < 0) return STEP89_ERR_LIMIT;
        block_p = header_end + 1;
        while (block_p < end_entity) {
            int line_end;
            int colon;
            int mode;
            char attr_name[STEP89_XP_MAX_NAME];
            char type_name[STEP89_XP_MAX_NAME];
            char expr[STEP89_XP_MAX_EXPR];
            block_p = xp_skip_ws(text, n, block_p);
            if (block_p >= end_entity) break;
            mode = 0;
            if (xp_match_word_at(text, n, block_p, "DERIVE")) { mode = STEP89_XP_ATTR_DERIVED; block_p += 6; continue; }
            if (xp_match_word_at(text, n, block_p, "INVERSE")) { mode = STEP89_XP_ATTR_INVERSE; block_p += 7; continue; }
            if (xp_match_word_at(text, n, block_p, "WHERE")) { mode = -1; block_p += 5; (void)mode; continue; }
            line_end = xp_find_semicolon(text, n, block_p, end_entity);
            if (line_end < 0) break;
            colon = xp_find_char(text, block_p, line_end, ':');
            if (colon >= 0) {
                int rule_kind;
                int rule_idx;
                xp_copy_range_trim(attr_name, STEP89_XP_MAX_NAME, text, block_p, colon);
                xp_copy_range_trim(type_name, STEP89_XP_MAX_NAME, text, colon + 1, line_end);
                if (xp_find_word(text, n, block_p, line_end, "SELF") >= 0 || xp_name_has(type_name, ":=")) rule_kind = STEP89_XP_RULE_DERIVE;
                else if (xp_find_word(text, n, block_p, line_end, "FOR") >= 0) rule_kind = STEP89_XP_RULE_INVERSE;
                else if (xp_name_has(type_name, "EXISTS") || xp_name_has(type_name, "SIZEOF") || xp_name_has(type_name, "WHERE")) rule_kind = STEP89_XP_RULE_WHERE;
                else rule_kind = 0;
                if (xp_name_has(type_name, ":=")) {
                    xp_add_attr(schema, owner, attr_name, type_name, STEP89_XP_ATTR_DERIVED);
                    rule_idx = xp_add_rule(schema, owner, STEP89_XP_RULE_DERIVE, attr_name, type_name, STEP89_XP_SEV_INFO);
                    xp_add_op(schema, rule_idx, STEP89_XP_OP_DERIVE_REFCOUNT, 0, 0, 0);
                } else if (rule_kind == STEP89_XP_RULE_INVERSE) {
                    xp_add_attr(schema, owner, attr_name, type_name, STEP89_XP_ATTR_INVERSE);
                    rule_idx = xp_add_rule(schema, owner, STEP89_XP_RULE_INVERSE, attr_name, type_name, STEP89_XP_SEV_INFO);
                    xp_add_op(schema, rule_idx, STEP89_XP_OP_DERIVE_INBOUND, 0, 0, 0);
                } else if (rule_kind == STEP89_XP_RULE_WHERE || xp_name_has(attr_name, "WR")) {
                    xp_copy_range_trim(expr, STEP89_XP_MAX_EXPR, text, colon + 1, line_end);
                    rule_idx = xp_add_rule(schema, owner, STEP89_XP_RULE_WHERE, attr_name, expr, STEP89_XP_SEV_ERROR);
                    xp_compile_rule_expr(schema, rule_idx);
                } else {
                    unsigned af;
                    af = 0u;
                    if (xp_name_has(type_name, "OPTIONAL")) af |= STEP89_XP_ATTR_OPTIONAL;
                    if (xp_name_has(type_name, "LIST")) af |= STEP89_XP_ATTR_LIST;
                    if (xp_name_has(type_name, "SET")) af |= STEP89_XP_ATTR_SET;
                    if (!xp_name_has(type_name, "STRING") && !xp_name_has(type_name, "BOOLEAN") && !xp_name_has(type_name, "INTEGER") && !xp_name_has(type_name, "REAL")) af |= STEP89_XP_ATTR_REF;
                    xp_add_attr(schema, owner, attr_name, type_name, af);
                }
            }
            block_p = line_end + 1;
        }
        schema->entities[owner].min_args = schema->entities[owner].explicit_attr_count;
        schema->entities[owner].max_args = schema->entities[owner].explicit_attr_count + schema->entities[owner].derived_attr_count + 16;
        p = end_entity + 10;
    }
    return schema->parse_error_count > 0 ? STEP89_ERR_SYNTAX : STEP89_OK;
}

static void xp_boot_entity(step89_xp_schema *s, const char *name, unsigned flags, int min_args, int max_args, int op)
{
    int e;
    int r;
    e = xp_add_entity(s, name, "", flags | STEP89_XP_ENTITY_FROM_BOOTSTRAP, min_args, max_args);
    if (e < 0) return;
    r = xp_add_rule(s, e, STEP89_XP_RULE_SYSTEM, "system", "AP242 bootstrap invariant", STEP89_XP_SEV_ERROR);
    if (r >= 0) xp_add_op(s, r, op, min_args, max_args, 0);
}

step89_result step89_xp_bootstrap_ap242(step89_xp_schema *schema)
{
    if (!schema) return STEP89_ERR_NULL;
    step89_xp_schema_init(schema);
    xp_copy(schema->schema_name, STEP89_XP_MAX_NAME, "AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_SEED");
    xp_boot_entity(schema, "CARTESIAN_POINT", STEP89_XP_ENTITY_AP242, 2, 2, STEP89_XP_OP_ARG_RANGE);
    xp_boot_entity(schema, "DIRECTION", STEP89_XP_ENTITY_AP242, 2, 2, STEP89_XP_OP_ARG_RANGE);
    xp_boot_entity(schema, "AXIS2_PLACEMENT_3D", STEP89_XP_ENTITY_AP242, 4, 4, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "B_SPLINE_CURVE_WITH_KNOTS", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_NURBS, 8, 12, STEP89_XP_OP_NURBS_CURVE_VALID);
    xp_boot_entity(schema, "B_SPLINE_SURFACE_WITH_KNOTS", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_NURBS, 10, 18, STEP89_XP_OP_NURBS_SURFACE_VALID);
    xp_boot_entity(schema, "TRIMMED_CURVE", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_NURBS, 5, 7, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "PCURVE", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_NURBS, 3, 3, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "EDGE_CURVE", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_TOPOLOGY, 5, 5, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "ORIENTED_EDGE", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_TOPOLOGY, 5, 5, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "EDGE_LOOP", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_TOPOLOGY, 2, 2, STEP89_XP_OP_CAD_LOOP_CLOSED);
    xp_boot_entity(schema, "FACE_OUTER_BOUND", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_TOPOLOGY, 3, 3, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "ADVANCED_FACE", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_TOPOLOGY, 4, 4, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "CLOSED_SHELL", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_TOPOLOGY, 2, 2, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "MANIFOLD_SOLID_BREP", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_TOPOLOGY, 3, 3, STEP89_XP_OP_CAD_SOLID_CLOSED);
    xp_boot_entity(schema, "GEOMETRIC_TOLERANCE", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_PMI_SEM, 4, 8, STEP89_XP_OP_PMI_HAS_ANCHOR);
    xp_boot_entity(schema, "GEOMETRIC_TOLERANCE_WITH_DATUM_REFERENCE", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_PMI_SEM, 5, 10, STEP89_XP_OP_PMI_HAS_ANCHOR);
    xp_boot_entity(schema, "POSITION_TOLERANCE", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_PMI_SEM, 4, 10, STEP89_XP_OP_PMI_HAS_ANCHOR);
    xp_boot_entity(schema, "DATUM", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_PMI_SEM, 4, 5, STEP89_XP_OP_PMI_HAS_ANCHOR);
    xp_boot_entity(schema, "SHAPE_ASPECT", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_PMI_SEM, 5, 5, STEP89_XP_OP_PMI_HAS_ANCHOR);
    xp_boot_entity(schema, "DIMENSIONAL_SIZE", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_PMI_SEM, 4, 6, STEP89_XP_OP_PMI_HAS_ANCHOR);
    xp_boot_entity(schema, "DRAUGHTING_MODEL", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_PMI_GFX, 3, 3, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "ANNOTATION_OCCURRENCE", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_PMI_GFX, 3, 5, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "ANNOTATION_PLANE", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_PMI_GFX, 3, 4, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "MECHANICAL_DESIGN_GEOMETRIC_PRESENTATION_REPRESENTATION", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_PMI_GFX, 3, 3, STEP89_XP_OP_ENTITY_REFS_EXIST);
    xp_boot_entity(schema, "GEOMETRIC_VALIDATION_PROPERTY", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_VALIDATION, 4, 4, STEP89_XP_OP_VALIDATION_TARGET);
    xp_boot_entity(schema, "VALIDATION_PROPERTY", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_VALIDATION, 4, 4, STEP89_XP_OP_VALIDATION_TARGET);
    xp_boot_entity(schema, "SI_UNIT", STEP89_XP_ENTITY_AP242, 2, 4, STEP89_XP_OP_ARG_RANGE);
    xp_boot_entity(schema, "UNCERTAINTY_MEASURE_WITH_UNIT", STEP89_XP_ENTITY_AP242 | STEP89_XP_ENTITY_VALIDATION, 4, 4, STEP89_XP_OP_ENTITY_REFS_EXIST);
    return STEP89_OK;
}

static void xp_add_msg(step89_xp_vm_report *r, int sev, int ref, const char *entity_name, const char *rule_name, const char *txt)
{
    step89_xp_message *m;
    if (!r || r->message_count >= STEP89_XP_MAX_MESSAGES) return;
    m = &r->messages[r->message_count];
    m->severity = sev;
    m->entity_ref = ref;
    xp_copy(m->entity_name, STEP89_XP_MAX_NAME, entity_name ? entity_name : "");
    xp_copy(m->rule_name, STEP89_XP_MAX_NAME, rule_name ? rule_name : "");
    xp_copy(m->text, STEP89_XP_MAX_EXPR, txt ? txt : "");
    ++r->message_count;
}

static int xp_get_node_inbound(const step89_complete_context *c, int ref)
{
    const step89_complete_node *n;
    n = step89_complete_find_node(c, ref);
    if (!n) return 0;
    return n->inbound_count;
}

static int xp_pmi_has_map(const step89_xp_pmi_report *pmi, int semantic_ref)
{
    int i;
    if (!pmi) return 0;
    for (i = 0; i < pmi->map_count; ++i) {
        if (pmi->maps[i].semantic_ref == semantic_ref && pmi->maps[i].graphic_ref != 0) return 1;
    }
    return 0;
}

static int xp_eval_op(const step89_xp_op *op, const step89_model *model, const step89_cad_model *cad, const step89_complete_context *complete, const step89_entity *e)
{
    int argc;
    int i;
    int target;
    const step89_curve *c;
    const step89_surface *s;
    const step89_cad_loop *loop;
    if (!op || !model || !e) return 0;
    switch (op->opcode) {
    case STEP89_XP_OP_TRUE:
        return 1;
    case STEP89_XP_OP_ARG_RANGE:
        argc = xp_count_top_args(model->text, e->args_begin, e->args_len);
        return argc >= op->a && argc <= op->b;
    case STEP89_XP_OP_ENTITY_REFS_EXIST:
        i = e->args_begin;
        while (i < e->args_begin + e->args_len) {
            if (model->text[i] == '\'') i = xp_scan_string(model->text, e->args_begin + e->args_len, i);
            else if (model->text[i] == '#') {
                int next;
                if (xp_scan_ref(model->text, e->args_begin + e->args_len, i, &target, &next)) {
                    if (!xp_find_step_entity(model, target)) return 0;
                    i = next;
                } else ++i;
            } else ++i;
        }
        return 1;
    case STEP89_XP_OP_INBOUND_AT_LEAST:
        return complete ? xp_get_node_inbound(complete, e->id) >= op->a : 1;
    case STEP89_XP_OP_CAD_LOOP_CLOSED:
        if (!cad) return 1;
        loop = step89_cad_find_loop(cad, e->id);
        return loop ? loop->closed != 0 : 1;
    case STEP89_XP_OP_CAD_SOLID_CLOSED:
        target = xp_first_ref(model, e);
        if (target == 0) return 0;
        {
            const step89_entity *sh;
            sh = xp_find_step_entity(model, target);
            return sh && (step89_name_eq(sh->name, "CLOSED_SHELL") || step89_name_eq(sh->name, "ORIENTED_CLOSED_SHELL"));
        }
    case STEP89_XP_OP_NURBS_CURVE_VALID:
        c = step89_find_curve(model, e->id);
        if (!c) return xp_entity_ref_count(model, e) >= 2;
        return c->degree > 0 && c->control_count > c->degree;
    case STEP89_XP_OP_NURBS_SURFACE_VALID:
        s = step89_find_surface(model, e->id);
        if (!s) return xp_entity_ref_count(model, e) >= 2;
        return s->degree_u > 0 && s->degree_v > 0 && s->control_count > 0;
    case STEP89_XP_OP_PMI_HAS_ANCHOR:
        target = xp_first_ref(model, e);
        return target == 0 || xp_find_step_entity(model, target) != 0;
    case STEP89_XP_OP_PMI_HAS_GRAPHIC:
        if (!g_vm_pmi_report) return 1;
        return xp_pmi_has_map(g_vm_pmi_report, e->id);
    case STEP89_XP_OP_VALIDATION_TARGET:
        target = xp_last_ref(model, e);
        return target != 0 && xp_find_step_entity(model, target) != 0;
    case STEP89_XP_OP_AP242_SCHEMA:
        return step89_validate_is_ap242_schema(model);
    case STEP89_XP_OP_DERIVE_REFCOUNT:
        return xp_entity_ref_count(model, e) >= 0;
    case STEP89_XP_OP_DERIVE_INBOUND:
        return complete ? xp_get_node_inbound(complete, e->id) >= 0 : 1;
    default:
        return 1;
    }
}

step89_result step89_xp_run_vm(const step89_xp_schema *schema, const step89_model *model, const step89_cad_model *cad, const step89_complete_context *complete, step89_xp_vm_report *report)
{
    int i;
    int r;
    int opi;
    const step89_entity *e;
    const step89_xp_entity_def *def;
    const step89_xp_rule *rule;
    if (!schema || !model || !report) return STEP89_ERR_NULL;
    step89_xp_vm_report_init(report);
    for (i = 0; i < model->entity_count; ++i) {
        e = &model->entities[i];
        def = step89_xp_find_entity_def(schema, e->name);
        if (!def) continue;
        for (r = 0; r < schema->rule_count; ++r) {
            int ok;
            rule = &schema->rules[r];
            if (schema->entities[rule->owner_index].name[0] == '\0') continue;
            if (!xp_name_eq(schema->entities[rule->owner_index].name, e->name)) continue;
            ok = 1;
            for (opi = 0; opi < rule->op_count; ++opi) {
                const step89_xp_op *op;
                op = &schema->ops[rule->first_op + opi];
                if (!xp_eval_op(op, model, cad, complete, e)) ok = 0;
            }
            if (rule->kind == STEP89_XP_RULE_WHERE || rule->kind == STEP89_XP_RULE_SYSTEM) {
                if (ok) ++report->where_ok;
                else {
                    ++report->where_fail;
                    xp_add_msg(report, rule->severity, e->id, e->name, rule->name, "WHERE/system bytecode failed");
                }
            } else if (rule->kind == STEP89_XP_RULE_DERIVE) ++report->derive_eval_count;
            else if (rule->kind == STEP89_XP_RULE_INVERSE) ++report->inverse_eval_count;
            if (rule->kind == STEP89_XP_RULE_SYSTEM) ++report->system_rule_count;
        }
    }
    return STEP89_OK;
}

static int xp_vertex_close(const step89_cad_vertex *a, const step89_cad_vertex *b, step89_fixed tol)
{
    if (!a || !b) return 0;
    return xp_abs_fixed(a->x - b->x) <= tol && xp_abs_fixed(a->y - b->y) <= tol && xp_abs_fixed(a->z - b->z) <= tol;
}

static int xp_edge_same_dir(const step89_cad_model *cad, const step89_cad_edge *a, const step89_cad_edge *b, step89_fixed tol)
{
    const step89_cad_vertex *as;
    const step89_cad_vertex *ae;
    const step89_cad_vertex *bs;
    const step89_cad_vertex *be;
    if (!cad || !a || !b) return 0;
    as = step89_cad_find_vertex(cad, a->start_vertex_ref);
    ae = step89_cad_find_vertex(cad, a->end_vertex_ref);
    bs = step89_cad_find_vertex(cad, b->start_vertex_ref);
    be = step89_cad_find_vertex(cad, b->end_vertex_ref);
    return xp_vertex_close(as, bs, tol) && xp_vertex_close(ae, be, tol);
}

static int xp_edge_reversed(const step89_cad_model *cad, const step89_cad_edge *a, const step89_cad_edge *b, step89_fixed tol)
{
    const step89_cad_vertex *as;
    const step89_cad_vertex *ae;
    const step89_cad_vertex *bs;
    const step89_cad_vertex *be;
    if (!cad || !a || !b) return 0;
    as = step89_cad_find_vertex(cad, a->start_vertex_ref);
    ae = step89_cad_find_vertex(cad, a->end_vertex_ref);
    bs = step89_cad_find_vertex(cad, b->start_vertex_ref);
    be = step89_cad_find_vertex(cad, b->end_vertex_ref);
    return xp_vertex_close(as, be, tol) && xp_vertex_close(ae, bs, tol);
}

step89_result step89_xp_build_sewing(const step89_model *model, const step89_cad_model *cad, step89_fixed tolerance, step89_xp_sew_report *report)
{
    int i;
    int j;
    (void)model;
    if (!cad || !report) return STEP89_ERR_NULL;
    step89_xp_sew_report_init(report);
    if (tolerance <= 0L) tolerance = STEP89_COMPLETE_TOLERANCE_DEFAULT;
    report->tolerance = tolerance;
    for (i = 0; i < cad->edge_count && i < STEP89_XP_MAX_SEW_EDGES; ++i) {
        const step89_cad_edge *e;
        const step89_cad_vertex *vs;
        const step89_cad_vertex *ve;
        e = &cad->edges[i];
        report->edges[i].edge_ref = e->ref;
        report->edges[i].start_vertex_ref = e->start_vertex_ref;
        report->edges[i].end_vertex_ref = e->end_vertex_ref;
        if (e->use_count <= 1) {
            report->edges[i].flags |= STEP89_XP_SEW_OPEN_EDGE;
            ++report->open_edge_count;
        }
        if (e->use_count > 2) {
            report->edges[i].flags |= STEP89_XP_SEW_NONMANIFOLD;
            ++report->nonmanifold_count;
        }
        vs = step89_cad_find_vertex(cad, e->start_vertex_ref);
        ve = step89_cad_find_vertex(cad, e->end_vertex_ref);
        if (xp_vertex_close(vs, ve, tolerance)) {
            report->edges[i].flags |= STEP89_XP_SEW_ZERO_EDGE;
            ++report->zero_edge_count;
        }
    }
    report->edge_count = i;
    for (i = 0; i < report->edge_count; ++i) {
        for (j = i + 1; j < report->edge_count; ++j) {
            if (xp_edge_reversed(cad, &cad->edges[i], &cad->edges[j], tolerance)) {
                if (report->edges[i].paired_edge_ref == 0) report->edges[i].paired_edge_ref = cad->edges[j].ref;
                if (report->edges[j].paired_edge_ref == 0) report->edges[j].paired_edge_ref = cad->edges[i].ref;
                report->edges[i].flags |= STEP89_XP_SEW_REVERSED_PAIR;
                report->edges[j].flags |= STEP89_XP_SEW_REVERSED_PAIR;
                ++report->sew_pair_count;
            } else if (xp_edge_same_dir(cad, &cad->edges[i], &cad->edges[j], tolerance)) {
                report->edges[i].flags |= STEP89_XP_SEW_DUPLICATE_EDGE;
                report->edges[j].flags |= STEP89_XP_SEW_DUPLICATE_EDGE;
                ++report->duplicate_edge_count;
            }
        }
    }
    for (i = 0; i < cad->vertex_count; ++i) {
        for (j = i + 1; j < cad->vertex_count; ++j) {
            if (cad->vertices[i].ref != cad->vertices[j].ref && xp_vertex_close(&cad->vertices[i], &cad->vertices[j], tolerance)) ++report->vertex_merge_hint_count;
        }
    }
    return STEP89_OK;
}

step89_result step89_xp_map_pmi(const step89_model *model, const step89_complete_context *complete, step89_xp_pmi_report *report)
{
    int i;
    int j;
    if (!model || !complete || !report) return STEP89_ERR_NULL;
    step89_xp_pmi_report_init(report);
    for (i = 0; i < complete->pmi_count; ++i) {
        if (complete->pmi[i].semantic) ++report->semantic_count;
        if (complete->pmi[i].graphic) ++report->graphic_count;
    }
    for (i = 0; i < complete->pmi_count; ++i) {
        const step89_complete_pmi *sem;
        step89_xp_pmi_map *map;
        int mapped;
        sem = &complete->pmi[i];
        if (!sem->semantic) continue;
        if (report->map_count >= STEP89_XP_MAX_PMI_MAPS) break;
        map = &report->maps[report->map_count];
        map->semantic_ref = sem->ref;
        map->anchor_ref = sem->anchor_ref;
        mapped = 0;
        for (j = 0; j < complete->pmi_count; ++j) {
            const step89_complete_pmi *gfx;
            const step89_entity *ge;
            gfx = &complete->pmi[j];
            if (!gfx->graphic) continue;
            ge = step89_find_entity(model, gfx->ref);
            if (ge && xp_entity_has_ref(model, ge, sem->ref)) {
                map->graphic_ref = gfx->ref;
                map->flags |= STEP89_XP_PMI_DIRECT_REF;
                ++report->direct_ref_count;
                mapped = 1;
                break;
            }
            if (sem->anchor_ref != 0 && gfx->anchor_ref == sem->anchor_ref) {
                map->graphic_ref = gfx->ref;
                map->flags |= STEP89_XP_PMI_SHARED_ANCHOR;
                ++report->shared_anchor_count;
                mapped = 1;
                break;
            }
            if (sem->presentation_ref != 0 && gfx->presentation_ref == sem->presentation_ref) {
                map->graphic_ref = gfx->ref;
                map->flags |= STEP89_XP_PMI_PRESENTATION;
                ++report->presentation_link_count;
                mapped = 1;
                break;
            }
        }
        if (mapped) ++report->mapped_count;
        else {
            map->flags |= STEP89_XP_PMI_UNMAPPED;
            ++report->unmapped_count;
        }
        ++report->map_count;
    }
    return STEP89_OK;
}

step89_result step89_xp_validate_ap242_fine(const step89_model *model, const step89_complete_context *complete, const step89_xp_pmi_report *pmi, step89_xp_ap242_report *report)
{
    int i;
    if (!model || !complete || !report) return STEP89_ERR_NULL;
    step89_xp_ap242_report_init(report);
    if (step89_validate_is_ap242_schema(model)) {
        report->flags |= STEP89_XP_AP242_SCHEMA_PRESENT;
        report->edition_hint = 1;
        if (xp_name_has(model->schema, "ED2")) report->edition_hint = 2;
        else if (xp_name_has(model->schema, "ED3")) report->edition_hint = 3;
        else if (xp_name_has(model->schema, "ED4")) report->edition_hint = 4;
    }
    if (complete->units.unit_count > 0) report->flags |= STEP89_XP_AP242_HAS_UNITS;
    if (complete->units.model_tolerance > 0L) report->flags |= STEP89_XP_AP242_HAS_UNCERTAINTY;
    if (complete->validation_property_count > 0) report->flags |= STEP89_XP_AP242_HAS_VALIDATION;
    if (complete->nurbs_count > 0) report->flags |= STEP89_XP_AP242_HAS_NURBS;
    if (complete->solid_count > 0) report->flags |= STEP89_XP_AP242_HAS_SOLIDS;
    if (pmi) {
        report->semantic_pmi_count = pmi->semantic_count;
        report->graphic_pmi_count = pmi->graphic_count;
        if (pmi->semantic_count > 0) report->flags |= STEP89_XP_AP242_HAS_SEMANTIC_PMI;
        if (pmi->graphic_count > 0) report->flags |= STEP89_XP_AP242_HAS_GRAPHIC_PMI;
        if (pmi->semantic_count > 0 && pmi->unmapped_count > 0) report->warning_count += pmi->unmapped_count;
    }
    report->validation_property_count = complete->validation_property_count;
    for (i = 0; i < model->entity_count; ++i) {
        if (xp_name_has(model->entities[i].name, "CONTEXT")) ++report->context_count;
        if (xp_name_has(model->entities[i].name, "PRODUCT")) ++report->product_count;
        if (xp_name_has(model->entities[i].name, "REPRESENTATION")) ++report->representation_count;
    }
    if ((report->flags & STEP89_XP_AP242_SCHEMA_PRESENT) != 0u) {
        if ((report->flags & STEP89_XP_AP242_HAS_UNITS) == 0u) ++report->warning_count;
        if ((report->flags & STEP89_XP_AP242_HAS_VALIDATION) == 0u) ++report->warning_count;
        if ((report->flags & STEP89_XP_AP242_HAS_SEMANTIC_PMI) != 0u && (report->flags & STEP89_XP_AP242_HAS_GRAPHIC_PMI) == 0u) ++report->warning_count;
    } else if (complete->pmi_count > 0) ++report->error_count;
    return STEP89_OK;
}

static int xp_find_span(int n, int degree, step89_fixed t, const step89_fixed *knots)
{
    int low;
    int high;
    int mid;
    if (t >= knots[n + 1]) return n;
    if (t <= knots[degree]) return degree;
    low = degree;
    high = n + 1;
    mid = (low + high) / 2;
    while (t < knots[mid] || t >= knots[mid + 1]) {
        if (t < knots[mid]) high = mid;
        else low = mid;
        mid = (low + high) / 2;
    }
    return mid;
}

step89_result step89_xp_eval_bspline_curve(const step89_xp_point *control, const step89_fixed *weights, int control_count, int degree, const step89_fixed *knots, int knot_count, step89_fixed t, step89_xp_point *out)
{
    step89_xp_point d[STEP89_XP_MAX_DEGREE + 1];
    int n;
    int span;
    int j;
    int r;
    if (!control || !knots || !out) return STEP89_ERR_NULL;
    if (control_count <= 0 || degree < 0 || degree > STEP89_XP_MAX_DEGREE) return STEP89_ERR_LIMIT;
    if (control_count <= degree || knot_count < control_count + degree + 1) return STEP89_ERR_SYNTAX;
    n = control_count - 1;
    span = xp_find_span(n, degree, t, knots);
    for (j = 0; j <= degree; ++j) {
        int idx;
        step89_fixed w;
        idx = span - degree + j;
        w = weights ? weights[idx] : STEP89_FIXED_ONE;
        d[j].x = xp_fixed_mul(control[idx].x, w);
        d[j].y = xp_fixed_mul(control[idx].y, w);
        d[j].z = xp_fixed_mul(control[idx].z, w);
        d[j].w = w;
    }
    for (r = 1; r <= degree; ++r) {
        for (j = degree; j >= r; --j) {
            int left;
            int right;
            step89_fixed den;
            step89_fixed alpha;
            left = span - degree + j;
            right = span + 1 + j - r;
            den = knots[right] - knots[left];
            alpha = den == 0L ? 0L : xp_fixed_div(t - knots[left], den);
            d[j].x = xp_fixed_lerp(d[j - 1].x, d[j].x, alpha);
            d[j].y = xp_fixed_lerp(d[j - 1].y, d[j].y, alpha);
            d[j].z = xp_fixed_lerp(d[j - 1].z, d[j].z, alpha);
            d[j].w = xp_fixed_lerp(d[j - 1].w, d[j].w, alpha);
        }
    }
    if (d[degree].w == 0L) return STEP89_ERR_SYNTAX;
    out->x = xp_fixed_div(d[degree].x, d[degree].w);
    out->y = xp_fixed_div(d[degree].y, d[degree].w);
    out->z = xp_fixed_div(d[degree].z, d[degree].w);
    out->w = STEP89_FIXED_ONE;
    return STEP89_OK;
}

step89_result step89_xp_eval_bspline_surface(const step89_xp_point *control_grid, const step89_fixed *weights, int u_count, int v_count, int degree_u, int degree_v, const step89_fixed *u_knots, int u_knot_count, const step89_fixed *v_knots, int v_knot_count, step89_fixed u, step89_fixed v, step89_xp_point *out)
{
    step89_xp_point tmp[STEP89_XP_MAX_SURFACE_TMP];
    step89_fixed wtmp[STEP89_XP_MAX_SURFACE_TMP];
    int row;
    step89_result rr;
    if (!control_grid || !u_knots || !v_knots || !out) return STEP89_ERR_NULL;
    if (v_count > STEP89_XP_MAX_SURFACE_TMP || u_count <= 0 || v_count <= 0) return STEP89_ERR_LIMIT;
    for (row = 0; row < v_count; ++row) {
        const step89_xp_point *rowp;
        const step89_fixed *roww;
        rowp = &control_grid[row * u_count];
        roww = weights ? &weights[row * u_count] : 0;
        rr = step89_xp_eval_bspline_curve(rowp, roww, u_count, degree_u, u_knots, u_knot_count, u, &tmp[row]);
        if (rr != STEP89_OK) return rr;
        wtmp[row] = STEP89_FIXED_ONE;
    }
    return step89_xp_eval_bspline_curve(tmp, wtmp, v_count, degree_v, v_knots, v_knot_count, v, out);
}

step89_result step89_xp_eval_step_curve_sample(const step89_model *model, int curve_ref, step89_fixed t, step89_xp_point *out)
{
    const step89_entity *e;
    step89_xp_point pts[32];
    step89_fixed knots[64];
    int ref_count;
    int i;
    int end;
    int next;
    int degree;
    if (!model || !out) return STEP89_ERR_NULL;
    e = step89_find_entity(model, curve_ref);
    if (!e) return STEP89_ERR_SYNTAX;
    xp_zero(pts, (int)sizeof(pts));
    ref_count = 0;
    i = e->args_begin;
    end = e->args_begin + e->args_len;
    degree = 2;
    ref_count = 0;
    i = e->args_begin;
    while (i < end) {
        if (model->text[i] == '#') {
            int rf;
            const step89_point *p;
            if (xp_scan_ref(model->text, end, i, &rf, &next)) {
                p = step89_find_point(model, rf);
                if (p && ref_count < 32) {
                    pts[ref_count].x = p->x;
                    pts[ref_count].y = p->y;
                    pts[ref_count].z = p->z;
                    pts[ref_count].w = STEP89_FIXED_ONE;
                    ++ref_count;
                }
                i = next;
            } else ++i;
        } else if (model->text[i] == '\'') i = xp_scan_string(model->text, end, i);
        else ++i;
    }
    if (ref_count == 0) {
        int rf;
        rf = xp_first_ref(model, e);
        {
            const step89_point *p;
            p = step89_find_point(model, rf);
            if (!p) return STEP89_ERR_SYNTAX;
            out->x = p->x;
            out->y = p->y;
            out->z = p->z;
            out->w = STEP89_FIXED_ONE;
            return STEP89_OK;
        }
    }
    if (ref_count <= degree) degree = ref_count - 1;
    if (degree < 1) {
        *out = pts[0];
        return STEP89_OK;
    }
    for (i = 0; i < ref_count + degree + 1 && i < 64; ++i) {
        if (i <= degree) knots[i] = 0L;
        else if (i >= ref_count) knots[i] = STEP89_FIXED_ONE;
        else knots[i] = xp_fixed_div((step89_fixed)(i - degree), (step89_fixed)(ref_count - degree));
    }
    return step89_xp_eval_bspline_curve(pts, 0, ref_count, degree, knots, ref_count + degree + 1, t, out);
}

step89_result step89_xp_analyze(step89_xp_analysis *analysis, const char *express_text, int express_len, const step89_model *model, const step89_cad_model *cad, const step89_complete_context *complete)
{
    step89_result r;
    if (!analysis || !model || !cad || !complete) return STEP89_ERR_NULL;
    step89_xp_analysis_init(analysis);
    if (express_text && express_len > 0) {
        r = step89_xp_parse_express(&analysis->schema, express_text, express_len);
        if (r != STEP89_OK || analysis->schema.entity_count == 0) step89_xp_bootstrap_ap242(&analysis->schema);
    } else step89_xp_bootstrap_ap242(&analysis->schema);
    step89_xp_map_pmi(model, complete, &analysis->pmi);
    g_vm_pmi_report = &analysis->pmi;
    step89_xp_run_vm(&analysis->schema, model, cad, complete, &analysis->vm);
    g_vm_pmi_report = 0;
    step89_xp_build_sewing(model, cad, complete->units.model_tolerance, &analysis->sew);
    step89_xp_validate_ap242_fine(model, complete, &analysis->pmi, &analysis->ap242);
    return STEP89_OK;
}
