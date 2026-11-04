#include "gobj_obj.h"

#ifndef GOBJ_LOGICAL_LINE_MAX
#define GOBJ_LOGICAL_LINE_MAX 2048u
#endif

static int gobj_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static int gobj_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static const char *gobj_skip_space(const char *p, const char *end)
{
    while (p < end && gobj_is_space(*p)) {
        ++p;
    }
    return p;
}

static int gobj_streq_range(const char *a, const char *b, const char *s)
{
    const char *p;

    p = a;
    while (p < b && *s != '\0') {
        if (*p != *s) {
            return 0;
        }
        ++p;
        ++s;
    }
    return p == b && *s == '\0';
}

static int gobj_name_equal(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static void gobj_copy_range(char *dst,
                            unsigned int dst_cap,
                            const char *a,
                            const char *b,
                            GOBJ_Report *report,
                            unsigned int trunc_flag)
{
    unsigned int i;

    if (dst_cap == 0u) {
        return;
    }
    i = 0u;
    while (a < b && *a != '#' && !gobj_is_space(*a)) {
        if (i + 1u < dst_cap) {
            dst[i] = *a;
            ++i;
        } else if (report != (GOBJ_Report *)0) {
            report->flags = report->flags | trunc_flag;
        }
        ++a;
    }
    dst[i] = '\0';
}

static void gobj_copy_next_token(char *dst,
                                 unsigned int dst_cap,
                                 const char *a,
                                 const char *b,
                                 GOBJ_Report *report,
                                 unsigned int trunc_flag)
{
    const char *p;
    const char *q;

    p = gobj_skip_space(a, b);
    q = p;
    while (q < b && *q != '#' && !gobj_is_space(*q)) {
        ++q;
    }
    gobj_copy_range(dst, dst_cap, p, q, report, trunc_flag);
}

static int gobj_parse_signed_int(const char **pp, const char *end, int *out)
{
    const char *p;
    int sign;
    int any;
    long v;

    p = *pp;
    sign = 1;
    any = 0;
    v = 0L;

    if (p < end && *p == '-') {
        sign = -1;
        ++p;
    } else if (p < end && *p == '+') {
        ++p;
    }

    while (p < end && gobj_is_digit(*p)) {
        any = 1;
        if (v < 214748364L) {
            v = (v * 10L) + (long)(*p - '0');
        }
        ++p;
    }

    if (!any) {
        return 0;
    }
    if (sign < 0) {
        v = -v;
    }
    *out = (int)v;
    *pp = p;
    return 1;
}

static int gobj_parse_fixed(const char **pp, const char *end, GOBJ_Fixed *out)
{
    const char *p;
    int sign;
    int any;
    long ip;
    long frac;
    long scale;
    long val;
    int exp_sign;
    int exp_any;
    long exp_val;
    long i;

    p = *pp;
    p = gobj_skip_space(p, end);
    sign = 1;
    any = 0;
    ip = 0L;
    frac = 0L;
    scale = 1L;
    val = 0L;
    exp_sign = 1;
    exp_any = 0;
    exp_val = 0L;

    if (p < end && *p == '-') {
        sign = -1;
        ++p;
    } else if (p < end && *p == '+') {
        ++p;
    }

    while (p < end && gobj_is_digit(*p)) {
        any = 1;
        if (ip < 32767L) {
            ip = (ip * 10L) + (long)(*p - '0');
        }
        ++p;
    }

    if (p < end && *p == '.') {
        ++p;
        while (p < end && gobj_is_digit(*p)) {
            any = 1;
            if (scale < 1000000L) {
                frac = (frac * 10L) + (long)(*p - '0');
                scale = scale * 10L;
            }
            ++p;
        }
    }

    if (!any) {
        return 0;
    }

    val = (ip << GOBJ_FIXED_SHIFT) + ((frac * GOBJ_FIXED_ONE) / scale);

    if (p < end && (*p == 'e' || *p == 'E')) {
        ++p;
        if (p < end && *p == '-') {
            exp_sign = -1;
            ++p;
        } else if (p < end && *p == '+') {
            ++p;
        }
        while (p < end && gobj_is_digit(*p)) {
            exp_any = 1;
            if (exp_val < 9L) {
                exp_val = (exp_val * 10L) + (long)(*p - '0');
            }
            ++p;
        }
        if (!exp_any) {
            return 0;
        }
        i = 0L;
        while (i < exp_val) {
            if (exp_sign > 0) {
                val = val * 10L;
            } else {
                val = val / 10L;
            }
            ++i;
        }
    }

    if (sign < 0) {
        val = -val;
    }
    *out = (GOBJ_Fixed)val;
    *pp = p;
    return 1;
}

static int gobj_has_more_values(const char *p, const char *end)
{
    p = gobj_skip_space(p, end);
    return p < end && *p != '#';
}

static int gobj_resolve_index(int raw, unsigned int count, int *out)
{
    long idx;

    if (raw > 0) {
        idx = (long)raw - 1L;
    } else if (raw < 0) {
        idx = (long)count + (long)raw;
    } else {
        return 0;
    }

    if (idx < 0L || idx >= (long)count) {
        return 0;
    }
    *out = (int)idx;
    return 1;
}

static int gobj_name_id(GOBJ_Name *slots,
                        unsigned int *count,
                        unsigned int cap,
                        const char *name,
                        unsigned short *out_id,
                        GOBJ_Report *report)
{
    unsigned int i;
    const char *p;
    const char *q;

    if (name == (const char *)0 || name[0] == '\0') {
        *out_id = GOBJ_ID_NONE;
        return GOBJ_OK;
    }

    i = 0u;
    while (i < *count) {
        if (gobj_name_equal(slots[i].name, name)) {
            *out_id = (unsigned short)(i + 1u);
            return GOBJ_OK;
        }
        ++i;
    }

    if (slots == (GOBJ_Name *)0 || *count >= cap) {
        *out_id = GOBJ_ID_NONE;
        return GOBJ_ERR_NAME_CAP;
    }

    p = name;
    q = name;
    while (*q != '\0') {
        ++q;
    }
    gobj_copy_range(slots[*count].name, (unsigned int)GOBJ_NAME_MAX, p, q, report, GOBJ_TRUNC_NAME);
    *out_id = (unsigned short)(*count + 1u);
    *count = *count + 1u;
    return GOBJ_OK;
}

static void gobj_set_current_tags(GOBJ_Model *model,
                                  unsigned short object_id,
                                  unsigned short group_id,
                                  unsigned short material_id,
                                  long smoothing_group)
{
    unsigned int i;

    model->current_tags.object_id = object_id;
    model->current_tags.group_id = group_id;
    model->current_tags.group_count = 0u;
    i = 0u;
    while (i < (unsigned int)GOBJ_GROUPS_PER_ELEMENT) {
        model->current_tags.group_ids[i] = GOBJ_ID_NONE;
        ++i;
    }
    if (group_id != GOBJ_ID_NONE) {
        model->current_tags.group_ids[0] = group_id;
        model->current_tags.group_count = 1u;
    }
    model->current_tags.material_id = material_id;
    model->current_tags.smoothing_group = smoothing_group;
}

static int gobj_parse_vref(const char **pp,
                           const char *end,
                           const GOBJ_Model *model,
                           GOBJ_VertexRef *out)
{
    const char *p;
    int raw_v;
    int raw_vt;
    int raw_vn;
    int ok;

    p = *pp;
    raw_v = 0;
    raw_vt = 0;
    raw_vn = 0;
    out->v = -1;
    out->vt = -1;
    out->vn = -1;

    if (!gobj_parse_signed_int(&p, end, &raw_v)) {
        return GOBJ_ERR_PARSE_INDEX;
    }
    if (!gobj_resolve_index(raw_v, model->position_count, &out->v)) {
        return GOBJ_ERR_INDEX_RANGE;
    }

    if (p < end && *p == '/') {
        ++p;
        if (p < end && *p == '/') {
            ++p;
            if (!gobj_parse_signed_int(&p, end, &raw_vn)) {
                return GOBJ_ERR_PARSE_INDEX;
            }
            if (!gobj_resolve_index(raw_vn, model->normal_count, &out->vn)) {
                return GOBJ_ERR_INDEX_RANGE;
            }
        } else {
            ok = gobj_parse_signed_int(&p, end, &raw_vt);
            if (!ok) {
                return GOBJ_ERR_PARSE_INDEX;
            }
            if (!gobj_resolve_index(raw_vt, model->texcoord_count, &out->vt)) {
                return GOBJ_ERR_INDEX_RANGE;
            }
            if (p < end && *p == '/') {
                ++p;
                if (!gobj_parse_signed_int(&p, end, &raw_vn)) {
                    return GOBJ_ERR_PARSE_INDEX;
                }
                if (!gobj_resolve_index(raw_vn, model->normal_count, &out->vn)) {
                    return GOBJ_ERR_INDEX_RANGE;
                }
            }
        }
    }

    *pp = p;
    return GOBJ_OK;
}

static int gobj_parse_line_ref(const char **pp,
                               const char *end,
                               const GOBJ_Model *model,
                               GOBJ_LineRef *out)
{
    const char *p;
    int raw_v;
    int raw_vt;

    p = *pp;
    raw_v = 0;
    raw_vt = 0;
    out->v = -1;
    out->vt = -1;

    if (!gobj_parse_signed_int(&p, end, &raw_v)) {
        return GOBJ_ERR_PARSE_INDEX;
    }
    if (!gobj_resolve_index(raw_v, model->position_count, &out->v)) {
        return GOBJ_ERR_INDEX_RANGE;
    }
    if (p < end && *p == '/') {
        ++p;
        if (gobj_parse_signed_int(&p, end, &raw_vt)) {
            if (!gobj_resolve_index(raw_vt, model->texcoord_count, &out->vt)) {
                return GOBJ_ERR_INDEX_RANGE;
            }
        }
    }
    *pp = p;
    return GOBJ_OK;
}

static int gobj_add_triangle(GOBJ_Model *model,
                             const GOBJ_VertexRef *a,
                             const GOBJ_VertexRef *b,
                             const GOBJ_VertexRef *c,
                             GOBJ_Report *report)
{
    GOBJ_Triangle *t;

    if (model->triangles == (GOBJ_Triangle *)0 || model->triangle_count >= model->triangle_cap) {
        return GOBJ_ERR_TRIANGLE_CAP;
    }

    t = &model->triangles[model->triangle_count];
    t->a = *a;
    t->b = *b;
    t->c = *c;
    t->tags = model->current_tags;
    model->triangle_count = model->triangle_count + 1u;
    if (report != (GOBJ_Report *)0) {
        report->generated_triangles = report->generated_triangles + 1u;
    }
    return GOBJ_OK;
}

static int gobj_parse_vertex(GOBJ_Model *model, const char *p, const char *end, GOBJ_Report *report)
{
    GOBJ_Vec3 v;
    GOBJ_Fixed extra;

    if (model->positions == (GOBJ_Vec3 *)0 || model->position_count >= model->position_cap) {
        return GOBJ_ERR_VERTEX_CAP;
    }
    if (!gobj_parse_fixed(&p, end, &v.x)) {
        return GOBJ_ERR_PARSE_NUMBER;
    }
    if (!gobj_parse_fixed(&p, end, &v.y)) {
        return GOBJ_ERR_PARSE_NUMBER;
    }
    if (!gobj_parse_fixed(&p, end, &v.z)) {
        return GOBJ_ERR_PARSE_NUMBER;
    }
    while (gobj_has_more_values(p, end)) {
        if (!gobj_parse_fixed(&p, end, &extra)) {
            break;
        }
        if (report != (GOBJ_Report *)0) {
            report->flags = report->flags | GOBJ_USED_V_EXTRA;
        }
    }
    model->positions[model->position_count] = v;
    model->position_count = model->position_count + 1u;
    return GOBJ_OK;
}

static int gobj_parse_texcoord(GOBJ_Model *model, const char *p, const char *end, GOBJ_Report *report)
{
    GOBJ_Vec2 v;
    GOBJ_Fixed extra;

    if (model->texcoords == (GOBJ_Vec2 *)0 || model->texcoord_count >= model->texcoord_cap) {
        return GOBJ_ERR_TEXCOORD_CAP;
    }
    if (!gobj_parse_fixed(&p, end, &v.u)) {
        return GOBJ_ERR_PARSE_NUMBER;
    }
    if (gobj_has_more_values(p, end)) {
        if (!gobj_parse_fixed(&p, end, &v.v)) {
            return GOBJ_ERR_PARSE_NUMBER;
        }
    } else {
        v.v = 0L;
    }
    while (gobj_has_more_values(p, end)) {
        if (!gobj_parse_fixed(&p, end, &extra)) {
            break;
        }
        if (report != (GOBJ_Report *)0) {
            report->flags = report->flags | GOBJ_USED_VT_EXTRA;
        }
    }
    model->texcoords[model->texcoord_count] = v;
    model->texcoord_count = model->texcoord_count + 1u;
    return GOBJ_OK;
}

static int gobj_parse_normal(GOBJ_Model *model, const char *p, const char *end)
{
    GOBJ_Vec3 v;

    if (model->normals == (GOBJ_Vec3 *)0 || model->normal_count >= model->normal_cap) {
        return GOBJ_ERR_NORMAL_CAP;
    }
    if (!gobj_parse_fixed(&p, end, &v.x)) {
        return GOBJ_ERR_PARSE_NUMBER;
    }
    if (!gobj_parse_fixed(&p, end, &v.y)) {
        return GOBJ_ERR_PARSE_NUMBER;
    }
    if (!gobj_parse_fixed(&p, end, &v.z)) {
        return GOBJ_ERR_PARSE_NUMBER;
    }
    model->normals[model->normal_count] = v;
    model->normal_count = model->normal_count + 1u;
    return GOBJ_OK;
}

static int gobj_parse_param_vertex(GOBJ_Model *model, const char *p, const char *end)
{
    GOBJ_Vec3 v;

    if (model->params == (GOBJ_Vec3 *)0 || model->param_count >= model->param_cap) {
        return GOBJ_ERR_PARAM_CAP;
    }
    v.x = 0L;
    v.y = 0L;
    v.z = 0L;
    if (!gobj_parse_fixed(&p, end, &v.x)) {
        return GOBJ_ERR_PARSE_NUMBER;
    }
    if (gobj_has_more_values(p, end)) {
        if (!gobj_parse_fixed(&p, end, &v.y)) {
            return GOBJ_ERR_PARSE_NUMBER;
        }
    }
    if (gobj_has_more_values(p, end)) {
        if (!gobj_parse_fixed(&p, end, &v.z)) {
            return GOBJ_ERR_PARSE_NUMBER;
        }
    }
    model->params[model->param_count] = v;
    model->param_count = model->param_count + 1u;
    return GOBJ_OK;
}

static int gobj_parse_face(GOBJ_Model *model,
                           const char *p,
                           const char *end,
                           GOBJ_Report *report)
{
    GOBJ_VertexRef first;
    GOBJ_VertexRef prev;
    GOBJ_VertexRef cur;
    int n;
    int r;

    n = 0;
    p = gobj_skip_space(p, end);
    while (p < end && *p != '#') {
        if (gobj_is_space(*p)) {
            p = gobj_skip_space(p, end);
        } else {
            r = gobj_parse_vref(&p, end, model, &cur);
            if (r != GOBJ_OK) {
                return r;
            }
            if (n == 0) {
                first = cur;
            } else if (n == 1) {
                prev = cur;
            } else {
                r = gobj_add_triangle(model, &first, &prev, &cur, report);
                if (r != GOBJ_OK) {
                    return r;
                }
                prev = cur;
            }
            ++n;
        }
    }

    if (n < 3) {
        return GOBJ_ERR_FACE_VERTS;
    }
    return GOBJ_OK;
}

static int gobj_parse_points(GOBJ_Model *model,
                             const char *p,
                             const char *end,
                             GOBJ_Report *report)
{
    int raw;
    int idx;
    GOBJ_Point *pt;

    p = gobj_skip_space(p, end);
    while (p < end && *p != '#') {
        if (gobj_is_space(*p)) {
            p = gobj_skip_space(p, end);
        } else {
            if (!gobj_parse_signed_int(&p, end, &raw)) {
                return GOBJ_ERR_PARSE_INDEX;
            }
            if (!gobj_resolve_index(raw, model->position_count, &idx)) {
                return GOBJ_ERR_INDEX_RANGE;
            }
            if (model->points == (GOBJ_Point *)0 || model->point_count >= model->point_cap) {
                return GOBJ_ERR_POINT_CAP;
            }
            pt = &model->points[model->point_count];
            pt->v = idx;
            pt->tags = model->current_tags;
            model->point_count = model->point_count + 1u;
            if (report != (GOBJ_Report *)0) {
                report->generated_points = report->generated_points + 1u;
            }
        }
    }
    return GOBJ_OK;
}

static int gobj_parse_line(GOBJ_Model *model,
                           const char *p,
                           const char *end,
                           GOBJ_Report *report)
{
    unsigned int first_ref;
    unsigned int count;
    GOBJ_LineRef lr;
    GOBJ_Line *line;
    int r;

    if (model->lines == (GOBJ_Line *)0 || model->line_count >= model->line_cap) {
        return GOBJ_ERR_LINE_CAP;
    }
    first_ref = model->line_ref_count;
    count = 0u;
    p = gobj_skip_space(p, end);
    while (p < end && *p != '#') {
        if (gobj_is_space(*p)) {
            p = gobj_skip_space(p, end);
        } else {
            r = gobj_parse_line_ref(&p, end, model, &lr);
            if (r != GOBJ_OK) {
                return r;
            }
            if (model->line_refs == (GOBJ_LineRef *)0 || model->line_ref_count >= model->line_ref_cap) {
                return GOBJ_ERR_LINE_REF_CAP;
            }
            model->line_refs[model->line_ref_count] = lr;
            model->line_ref_count = model->line_ref_count + 1u;
            count = count + 1u;
        }
    }
    if (count < 2u) {
        return GOBJ_ERR_FACE_VERTS;
    }
    line = &model->lines[model->line_count];
    line->first_ref = first_ref;
    line->ref_count = count;
    line->tags = model->current_tags;
    model->line_count = model->line_count + 1u;
    if (report != (GOBJ_Report *)0) {
        report->generated_lines = report->generated_lines + 1u;
    }
    return GOBJ_OK;
}

static int gobj_parse_object(GOBJ_Model *model, const char *p, const char *end, GOBJ_Report *report)
{
    char name[GOBJ_NAME_MAX];
    unsigned short id;
    int r;

    gobj_copy_next_token(name, (unsigned int)GOBJ_NAME_MAX, p, end, report, GOBJ_TRUNC_NAME);
    r = gobj_name_id(model->objects, &model->object_count, model->object_cap, name, &id, report);
    if (r != GOBJ_OK) {
        return r;
    }
    model->current_tags.object_id = id;
    return GOBJ_OK;
}

static int gobj_parse_group(GOBJ_Model *model, const char *p, const char *end, GOBJ_Report *report)
{
    char name[GOBJ_NAME_MAX];
    unsigned short id;
    unsigned int n;
    int r;
    const char *q;
    const char *e;

    model->current_tags.group_count = 0u;
    model->current_tags.group_id = GOBJ_ID_NONE;
    n = 0u;
    q = gobj_skip_space(p, end);
    while (q < end && *q != '#') {
        if (gobj_is_space(*q)) {
            q = gobj_skip_space(q, end);
        } else {
            e = q;
            while (e < end && *e != '#' && !gobj_is_space(*e)) {
                ++e;
            }
            gobj_copy_range(name, (unsigned int)GOBJ_NAME_MAX, q, e, report, GOBJ_TRUNC_NAME);
            r = gobj_name_id(model->groups, &model->group_count, model->group_cap, name, &id, report);
            if (r != GOBJ_OK) {
                return r;
            }
            if (n < (unsigned int)GOBJ_GROUPS_PER_ELEMENT) {
                model->current_tags.group_ids[n] = id;
                if (n == 0u) {
                    model->current_tags.group_id = id;
                }
                n = n + 1u;
            } else if (report != (GOBJ_Report *)0) {
                report->flags = report->flags | GOBJ_TRUNC_GROUPS;
            }
            q = e;
        }
    }
    model->current_tags.group_count = (unsigned short)n;
    return GOBJ_OK;
}

static int gobj_parse_usemtl(GOBJ_Model *model, const char *p, const char *end, GOBJ_Report *report)
{
    char name[GOBJ_NAME_MAX];
    unsigned short id;
    int r;

    gobj_copy_next_token(name, (unsigned int)GOBJ_NAME_MAX, p, end, report, GOBJ_TRUNC_NAME);
    r = gobj_name_id(model->materials, &model->material_count, model->material_cap, name, &id, report);
    if (r != GOBJ_OK) {
        return r;
    }
    model->current_tags.material_id = id;
    return GOBJ_OK;
}

static int gobj_parse_mtllib(GOBJ_Model *model, const char *p, const char *end, GOBJ_Report *report)
{
    const char *q;
    const char *e;
    GOBJ_Path *path;

    q = gobj_skip_space(p, end);
    while (q < end && *q != '#') {
        if (gobj_is_space(*q)) {
            q = gobj_skip_space(q, end);
        } else {
            e = q;
            while (e < end && *e != '#' && !gobj_is_space(*e)) {
                ++e;
            }
            if (model->mtllibs != (GOBJ_Path *)0 && model->mtllib_cap > 0u) {
                if (model->mtllib_count >= model->mtllib_cap) {
                    return GOBJ_ERR_MTLLIB_CAP;
                }
                path = &model->mtllibs[model->mtllib_count];
                gobj_copy_range(path->name, (unsigned int)GOBJ_PATH_MAX, q, e, report, GOBJ_TRUNC_MTLLIB);
                model->mtllib_count = model->mtllib_count + 1u;
            }
            q = e;
        }
    }
    return GOBJ_OK;
}

static int gobj_parse_smoothing(GOBJ_Model *model, const char *p, const char *end)
{
    char name[16];
    const char *q;
    const char *r;
    int value;

    q = gobj_skip_space(p, end);
    r = q;
    while (r < end && *r != '#' && !gobj_is_space(*r)) {
        ++r;
    }
    gobj_copy_range(name, 16u, q, r, (GOBJ_Report *)0, 0u);
    if (gobj_name_equal(name, "off") || gobj_name_equal(name, "0") || name[0] == '\0') {
        model->current_tags.smoothing_group = 0L;
        return GOBJ_OK;
    }
    q = name;
    r = name;
    while (*r != '\0') {
        ++r;
    }
    if (!gobj_parse_signed_int(&q, r, &value)) {
        return GOBJ_ERR_PARSE_INDEX;
    }
    model->current_tags.smoothing_group = (long)value;
    return GOBJ_OK;
}

static int gobj_is_curve_surface_keyword(const char *k0, const char *k1)
{
    if (gobj_streq_range(k0, k1, "cstype")) return 1;
    if (gobj_streq_range(k0, k1, "deg")) return 1;
    if (gobj_streq_range(k0, k1, "bmat")) return 1;
    if (gobj_streq_range(k0, k1, "step")) return 1;
    if (gobj_streq_range(k0, k1, "curv")) return 1;
    if (gobj_streq_range(k0, k1, "curv2")) return 1;
    if (gobj_streq_range(k0, k1, "surf")) return 1;
    if (gobj_streq_range(k0, k1, "parm")) return 1;
    if (gobj_streq_range(k0, k1, "trim")) return 1;
    if (gobj_streq_range(k0, k1, "hole")) return 1;
    if (gobj_streq_range(k0, k1, "scrv")) return 1;
    if (gobj_streq_range(k0, k1, "sp")) return 1;
    if (gobj_streq_range(k0, k1, "end")) return 1;
    if (gobj_streq_range(k0, k1, "con")) return 1;
    return 0;
}

static int gobj_is_render_attr_keyword(const char *k0, const char *k1)
{
    if (gobj_streq_range(k0, k1, "bevel")) return 1;
    if (gobj_streq_range(k0, k1, "c_interp")) return 1;
    if (gobj_streq_range(k0, k1, "d_interp")) return 1;
    if (gobj_streq_range(k0, k1, "lod")) return 1;
    if (gobj_streq_range(k0, k1, "shadow_obj")) return 1;
    if (gobj_streq_range(k0, k1, "trace_obj")) return 1;
    if (gobj_streq_range(k0, k1, "ctech")) return 1;
    if (gobj_streq_range(k0, k1, "stech")) return 1;
    if (gobj_streq_range(k0, k1, "mg")) return 1;
    return 0;
}

static int gobj_process_line(GOBJ_Model *model,
                             const char *line,
                             const char *end,
                             GOBJ_Report *report)
{
    const char *p;
    const char *k0;
    const char *k1;
    int r;

    p = gobj_skip_space(line, end);
    if (p >= end) {
        if (report != (GOBJ_Report *)0) {
            report->blank_lines = report->blank_lines + 1u;
        }
        return GOBJ_OK;
    }
    if (*p == '#') {
        if (report != (GOBJ_Report *)0) {
            report->comment_lines = report->comment_lines + 1u;
        }
        return GOBJ_OK;
    }

    k0 = p;
    while (p < end && !gobj_is_space(*p) && *p != '#') {
        ++p;
    }
    k1 = p;
    p = gobj_skip_space(p, end);

    if (gobj_streq_range(k0, k1, "v")) {
        return gobj_parse_vertex(model, p, end, report);
    }
    if (gobj_streq_range(k0, k1, "vt")) {
        return gobj_parse_texcoord(model, p, end, report);
    }
    if (gobj_streq_range(k0, k1, "vn")) {
        return gobj_parse_normal(model, p, end);
    }
    if (gobj_streq_range(k0, k1, "vp")) {
        if (report != (GOBJ_Report *)0) {
            report->param_vertex_lines = report->param_vertex_lines + 1u;
        }
        return gobj_parse_param_vertex(model, p, end);
    }
    if (gobj_streq_range(k0, k1, "f")) {
        return gobj_parse_face(model, p, end, report);
    }
    if (gobj_streq_range(k0, k1, "p")) {
        if (report != (GOBJ_Report *)0) {
            report->point_lines = report->point_lines + 1u;
        }
        return gobj_parse_points(model, p, end, report);
    }
    if (gobj_streq_range(k0, k1, "l")) {
        if (report != (GOBJ_Report *)0) {
            report->line_lines = report->line_lines + 1u;
        }
        return gobj_parse_line(model, p, end, report);
    }
    if (gobj_streq_range(k0, k1, "o")) {
        r = gobj_parse_object(model, p, end, report);
        if (report != (GOBJ_Report *)0) {
            report->object_lines = report->object_lines + 1u;
        }
        return r;
    }
    if (gobj_streq_range(k0, k1, "g")) {
        r = gobj_parse_group(model, p, end, report);
        if (report != (GOBJ_Report *)0) {
            report->group_lines = report->group_lines + 1u;
        }
        return r;
    }
    if (gobj_streq_range(k0, k1, "usemtl")) {
        r = gobj_parse_usemtl(model, p, end, report);
        if (report != (GOBJ_Report *)0) {
            report->material_use_lines = report->material_use_lines + 1u;
        }
        return r;
    }
    if (gobj_streq_range(k0, k1, "mtllib")) {
        r = gobj_parse_mtllib(model, p, end, report);
        if (report != (GOBJ_Report *)0) {
            report->material_library_lines = report->material_library_lines + 1u;
        }
        return r;
    }
    if (gobj_streq_range(k0, k1, "s")) {
        if (report != (GOBJ_Report *)0) {
            report->smoothing_lines = report->smoothing_lines + 1u;
        }
        return gobj_parse_smoothing(model, p, end);
    }
    if (gobj_is_curve_surface_keyword(k0, k1)) {
        if (report != (GOBJ_Report *)0) {
            report->curve_surface_lines = report->curve_surface_lines + 1u;
        }
        return GOBJ_OK;
    }
    if (gobj_is_render_attr_keyword(k0, k1)) {
        if (report != (GOBJ_Report *)0) {
            report->render_attr_lines = report->render_attr_lines + 1u;
        }
        return GOBJ_OK;
    }

    if (report != (GOBJ_Report *)0) {
        report->ignored_lines = report->ignored_lines + 1u;
    }
    return GOBJ_OK;
}

void gobj_report_clear(GOBJ_Report *report)
{
    if (report == (GOBJ_Report *)0) {
        return;
    }
    report->line = 0u;
    report->ignored_lines = 0u;
    report->comment_lines = 0u;
    report->blank_lines = 0u;
    report->material_library_lines = 0u;
    report->object_lines = 0u;
    report->group_lines = 0u;
    report->material_use_lines = 0u;
    report->smoothing_lines = 0u;
    report->point_lines = 0u;
    report->line_lines = 0u;
    report->param_vertex_lines = 0u;
    report->render_attr_lines = 0u;
    report->curve_surface_lines = 0u;
    report->line_continuations = 0u;
    report->generated_triangles = 0u;
    report->generated_points = 0u;
    report->generated_lines = 0u;
    report->flags = 0u;
    report->last_error = GOBJ_OK;
}

void gobj_model_bind(GOBJ_Model *model,
                     GOBJ_Vec3 *positions, unsigned int position_cap,
                     GOBJ_Vec2 *texcoords, unsigned int texcoord_cap,
                     GOBJ_Vec3 *normals, unsigned int normal_cap,
                     GOBJ_Triangle *triangles, unsigned int triangle_cap,
                     GOBJ_Name *objects, unsigned int object_cap,
                     GOBJ_Name *groups, unsigned int group_cap,
                     GOBJ_Name *materials, unsigned int material_cap)
{
    gobj_model_bind_ex(model,
                       positions, position_cap,
                       texcoords, texcoord_cap,
                       normals, normal_cap,
                       (GOBJ_Vec3 *)0, 0u,
                       triangles, triangle_cap,
                       (GOBJ_Point *)0, 0u,
                       (GOBJ_Line *)0, 0u,
                       (GOBJ_LineRef *)0, 0u,
                       objects, object_cap,
                       groups, group_cap,
                       materials, material_cap,
                       (GOBJ_Path *)0, 0u);
}

void gobj_model_bind_ex(GOBJ_Model *model,
                        GOBJ_Vec3 *positions, unsigned int position_cap,
                        GOBJ_Vec2 *texcoords, unsigned int texcoord_cap,
                        GOBJ_Vec3 *normals, unsigned int normal_cap,
                        GOBJ_Vec3 *params, unsigned int param_cap,
                        GOBJ_Triangle *triangles, unsigned int triangle_cap,
                        GOBJ_Point *points, unsigned int point_cap,
                        GOBJ_Line *lines, unsigned int line_cap,
                        GOBJ_LineRef *line_refs, unsigned int line_ref_cap,
                        GOBJ_Name *objects, unsigned int object_cap,
                        GOBJ_Name *groups, unsigned int group_cap,
                        GOBJ_Name *materials, unsigned int material_cap,
                        GOBJ_Path *mtllibs, unsigned int mtllib_cap)
{
    if (model == (GOBJ_Model *)0) {
        return;
    }
    model->positions = positions;
    model->position_cap = position_cap;
    model->texcoords = texcoords;
    model->texcoord_cap = texcoord_cap;
    model->normals = normals;
    model->normal_cap = normal_cap;
    model->params = params;
    model->param_cap = param_cap;
    model->triangles = triangles;
    model->triangle_cap = triangle_cap;
    model->points = points;
    model->point_cap = point_cap;
    model->lines = lines;
    model->line_cap = line_cap;
    model->line_refs = line_refs;
    model->line_ref_cap = line_ref_cap;
    model->objects = objects;
    model->object_cap = object_cap;
    model->groups = groups;
    model->group_cap = group_cap;
    model->materials = materials;
    model->material_cap = material_cap;
    model->mtllibs = mtllibs;
    model->mtllib_cap = mtllib_cap;
    gobj_model_reset(model);
}

void gobj_model_reset(GOBJ_Model *model)
{
    if (model == (GOBJ_Model *)0) {
        return;
    }
    model->position_count = 0u;
    model->texcoord_count = 0u;
    model->normal_count = 0u;
    model->param_count = 0u;
    model->triangle_count = 0u;
    model->point_count = 0u;
    model->line_count = 0u;
    model->line_ref_count = 0u;
    model->object_count = 0u;
    model->group_count = 0u;
    model->material_count = 0u;
    model->mtllib_count = 0u;
    gobj_set_current_tags(model, GOBJ_ID_NONE, GOBJ_ID_NONE, GOBJ_ID_NONE, 0L);
}

static int gobj_push_logical(char *logical,
                             unsigned int *logical_len,
                             const char *a,
                             const char *b)
{
    while (b > a && (*(b - 1) == '\r' || *(b - 1) == '\n')) {
        --b;
    }
    while (a < b) {
        if (*logical_len + 1u >= (unsigned int)GOBJ_LOGICAL_LINE_MAX) {
            return GOBJ_ERR_LOGICAL_LINE_CAP;
        }
        logical[*logical_len] = *a;
        *logical_len = *logical_len + 1u;
        ++a;
    }
    logical[*logical_len] = '\0';
    return GOBJ_OK;
}

static int gobj_line_has_join(const char *a, const char *b)
{
    while (b > a && gobj_is_space(*(b - 1))) {
        --b;
    }
    return b > a && *(b - 1) == '\\';
}

int gobj_import_obj_memory(GOBJ_Model *model,
                           const char *data,
                           unsigned long data_size,
                           GOBJ_Report *report)
{
    const char *p;
    const char *end;
    const char *line;
    const char *line_end;
    char logical[GOBJ_LOGICAL_LINE_MAX];
    unsigned int logical_len;
    int in_join;
    int join_now;
    int r;

    if (model == (GOBJ_Model *)0 || data == (const char *)0) {
        if (report != (GOBJ_Report *)0) {
            report->last_error = GOBJ_ERR_NULL;
        }
        return GOBJ_ERR_NULL;
    }

    gobj_report_clear(report);
    gobj_model_reset(model);

    p = data;
    end = data + data_size;
    logical_len = 0u;
    in_join = 0;
    while (p < end) {
        line = p;
        while (p < end && *p != '\n') {
            ++p;
        }
        line_end = p;
        if (line_end > line && *(line_end - 1) == '\r') {
            --line_end;
        }
        if (report != (GOBJ_Report *)0) {
            report->line = report->line + 1u;
        }
        join_now = gobj_line_has_join(line, line_end);
        if (join_now) {
            while (line_end > line && gobj_is_space(*(line_end - 1))) {
                --line_end;
            }
            --line_end;
        }
        if (in_join || join_now) {
            r = gobj_push_logical(logical, &logical_len, line, line_end);
            if (r != GOBJ_OK) {
                if (report != (GOBJ_Report *)0) report->last_error = r;
                return r;
            }
            if (join_now) {
                if (report != (GOBJ_Report *)0) {
                    report->line_continuations = report->line_continuations + 1u;
                }
                in_join = 1;
            } else {
                r = gobj_process_line(model, logical, logical + logical_len, report);
                if (r != GOBJ_OK) {
                    if (report != (GOBJ_Report *)0) report->last_error = r;
                    return r;
                }
                logical_len = 0u;
                in_join = 0;
            }
        } else {
            r = gobj_process_line(model, line, line_end, report);
            if (r != GOBJ_OK) {
                if (report != (GOBJ_Report *)0) report->last_error = r;
                return r;
            }
        }
        if (p < end && *p == '\n') {
            ++p;
        }
    }
    if (in_join && logical_len > 0u) {
        r = gobj_process_line(model, logical, logical + logical_len, report);
        if (r != GOBJ_OK) {
            if (report != (GOBJ_Report *)0) report->last_error = r;
            return r;
        }
    }

    if (report != (GOBJ_Report *)0) {
        report->last_error = GOBJ_OK;
    }
    return GOBJ_OK;
}

const char *gobj_result_string(int code)
{
    switch (code) {
    case GOBJ_OK: return "ok";
    case GOBJ_ERR_NULL: return "null argument";
    case GOBJ_ERR_PARSE_NUMBER: return "numeric parse error";
    case GOBJ_ERR_PARSE_INDEX: return "index parse error";
    case GOBJ_ERR_INDEX_RANGE: return "index out of range";
    case GOBJ_ERR_VERTEX_CAP: return "position capacity exceeded";
    case GOBJ_ERR_TEXCOORD_CAP: return "texcoord capacity exceeded";
    case GOBJ_ERR_NORMAL_CAP: return "normal capacity exceeded";
    case GOBJ_ERR_TRIANGLE_CAP: return "triangle capacity exceeded";
    case GOBJ_ERR_NAME_CAP: return "name capacity exceeded";
    case GOBJ_ERR_FACE_VERTS: return "element requires more references";
    case GOBJ_ERR_PARAM_CAP: return "parameter vertex capacity exceeded";
    case GOBJ_ERR_POINT_CAP: return "point capacity exceeded";
    case GOBJ_ERR_LINE_CAP: return "line capacity exceeded";
    case GOBJ_ERR_LINE_REF_CAP: return "line reference capacity exceeded";
    case GOBJ_ERR_LOGICAL_LINE_CAP: return "logical line capacity exceeded";
    case GOBJ_ERR_MTLLIB_CAP: return "material library path capacity exceeded";
    default: return "unknown error";
    }
}
