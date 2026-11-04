#include "ifc3d_internal.h"

static const char *skip_string(const char *p, const char *end)
{
    if (p >= end || *p != '\'') return p;
    ++p;
    while (p < end) {
        if (*p == '\'') {
            if (p + 1 < end && p[1] == '\'') { p += 2; continue; }
            ++p;
            break;
        }
        ++p;
    }
    return p;
}

ifc3d_status ifc3d_arg_get(const char *args, int len, int index, const char **out, int *out_len)
{
    const char *p;
    const char *end;
    const char *start;
    int depth;
    int cur;
    if (!args || !out || !out_len || index < 0) return IFC3D_ERR_BAD_ARG;
    p = args;
    end = args + len;
    cur = 0;
    depth = 0;
    start = p;
    while (p <= end) {
        if (p == end || (*p == ',' && depth == 0)) {
            if (cur == index) {
                const char *a = start;
                const char *b = p;
                a = ifc3d_skip_ws(a, b);
                while (b > a && ifc3d_is_space((unsigned char)b[-1])) --b;
                *out = a;
                *out_len = (int)(b - a);
                return IFC3D_OK;
            }
            ++cur;
            start = p + 1;
        } else if (*p == '\'') {
            p = skip_string(p, end);
            continue;
        } else if (*p == '(') {
            ++depth;
        } else if (*p == ')') {
            if (depth > 0) --depth;
        }
        ++p;
    }
    return IFC3D_ERR_NOT_FOUND;
}

int ifc3d_arg_is_omitted(const char *arg, int len)
{
    const char *p;
    const char *end;
    if (!arg || len <= 0) return 1;
    p = ifc3d_skip_ws(arg, arg + len);
    end = arg + len;
    while (end > p && ifc3d_is_space((unsigned char)end[-1])) --end;
    if (p >= end) return 1;
    if (end - p == 1 && (*p == '$' || *p == '*')) return 1;
    return 0;
}

ifc3d_status ifc3d_arg_parse_ref(const char *arg, int len, long *out_ref)
{
    const char *p;
    const char *end;
    long id;
    if (!arg || !out_ref) return IFC3D_ERR_BAD_ARG;
    p = ifc3d_skip_ws(arg, arg + len);
    end = arg + len;
    if (p >= end || *p != '#') return IFC3D_ERR_PARSE;
    ++p;
    id = 0;
    if (p >= end || !ifc3d_is_digit((unsigned char)*p)) return IFC3D_ERR_PARSE;
    while (p < end && ifc3d_is_digit((unsigned char)*p)) {
        id = id * 10L + (long)(*p - '0');
        ++p;
    }
    *out_ref = id;
    return IFC3D_OK;
}

ifc3d_status ifc3d_arg_parse_ref_list(const char *arg, int len, long *refs, int max_refs, int *out_count)
{
    const char *p;
    const char *end;
    long id;
    int count;
    if (!arg || !refs || !out_count) return IFC3D_ERR_BAD_ARG;
    p = ifc3d_skip_ws(arg, arg + len);
    end = arg + len;
    count = 0;
    if (p < end && *p == '(') ++p;
    while (p < end) {
        p = ifc3d_skip_ws(p, end);
        if (p >= end || *p == ')') break;
        if (*p == '#') {
            ++p;
            id = 0;
            while (p < end && ifc3d_is_digit((unsigned char)*p)) { id = id * 10L + (long)(*p - '0'); ++p; }
            if (count >= max_refs) return IFC3D_ERR_OVERFLOW;
            refs[count++] = id;
        } else {
            while (p < end && *p != ',' && *p != ')') ++p;
        }
        p = ifc3d_skip_ws(p, end);
        if (p < end && *p == ',') ++p;
    }
    *out_count = count;
    return IFC3D_OK;
}

ifc3d_status ifc3d_arg_parse_fixed_list3(const char *arg, int len, ifc3d_vec3 *out)
{
    const char *p;
    const char *end;
    const char *start;
    int idx;
    int depth;
    ifc3d_fixed vals[3];
    if (!arg || !out) return IFC3D_ERR_BAD_ARG;
    vals[0] = vals[1] = vals[2] = 0;
    p = ifc3d_skip_ws(arg, arg + len);
    end = arg + len;
    if (p < end && *p == '(') ++p;
    idx = 0;
    while (p < end && idx < 3) {
        p = ifc3d_skip_ws(p, end);
        if (p >= end || *p == ')') break;
        start = p;
        depth = 0;
        while (p < end) {
            if (*p == '(') ++depth;
            else if (*p == ')') { if (depth == 0) break; --depth; }
            else if (*p == ',' && depth == 0) break;
            ++p;
        }
        if (ifc3d_parse_fixed(start, (int)(p - start), &vals[idx]) != IFC3D_OK) return IFC3D_ERR_PARSE;
        ++idx;
        if (p < end && *p == ',') ++p;
    }
    if (idx < 2) return IFC3D_ERR_PARSE;
    out->x = vals[0];
    out->y = vals[1];
    out->z = (idx >= 3) ? vals[2] : 0;
    return IFC3D_OK;
}

ifc3d_status ifc3d_arg_parse_string_token(const char *arg, int len, char *dst, int dst_len)
{
    const char *p;
    const char *end;
    int i;
    if (!arg || !dst || dst_len <= 0) return IFC3D_ERR_BAD_ARG;
    dst[0] = '\0';
    p = ifc3d_skip_ws(arg, arg + len);
    end = arg + len;
    if (p >= end || *p != '\'') return IFC3D_ERR_PARSE;
    ++p;
    i = 0;
    while (p < end && *p != '\'') {
        if (i < dst_len - 1) dst[i++] = *p;
        ++p;
    }
    dst[i] = '\0';
    return IFC3D_OK;
}

int ifc3d_arg_parse_bool(const char *arg, int len, int default_value)
{
    const char *p;
    const char *end;
    p = ifc3d_skip_ws(arg, arg + len);
    end = arg + len;
    if (p + 2 < end && p[0] == '.' && (p[1] == 'T' || p[1] == 't') && p[2] == '.') return 1;
    if (p + 2 < end && p[0] == '.' && (p[1] == 'F' || p[1] == 'f') && p[2] == '.') return 0;
    return default_value;
}

ifc3d_status ifc3d_parse_nested_point_list3(ifc3d_model *m, ifc3d_entity *e, const char *arg, int len, ifc3d_diag *diag)
{
    const char *p;
    const char *end;
    const char *start;
    const char *r;
    int depth;
    ifc3d_vec3 v;
    if (!m || !e || !arg) return IFC3D_ERR_BAD_ARG;
    p = ifc3d_skip_ws(arg, arg + len);
    end = arg + len;
    e->first = m->point_count;
    e->count = 0;

    while (p < end) {
        if (*p == '(') {
            r = ifc3d_skip_ws(p + 1, end);
            if (r < end && *r == '(') {
                ++p; /* outer aggregate: keep scanning for inner tuples */
                continue;
            }
            start = p;
            depth = 1;
            ++p;
            while (p < end && depth > 0) {
                if (*p == '\'') { p = skip_string(p, end); continue; }
                if (*p == '(') ++depth;
                else if (*p == ')') --depth;
                ++p;
            }
            if (depth == 0) {
                if (ifc3d_arg_parse_fixed_list3(start, (int)(p - start), &v) == IFC3D_OK) {
                    if (m->point_count >= IFC3D_MAX_POINTLIST_POINTS) {
                        ifc3d_set_diag(diag, IFC3D_ERR_OVERFLOW, 0, "IFC point list storage overflow");
                        return IFC3D_ERR_OVERFLOW;
                    }
                    m->pointlist_points[m->point_count++] = v;
                    ++e->count;
                }
            }
        } else {
            ++p;
        }
    }
    return IFC3D_OK;
}

static ifc3d_status parse_int_token(const char *a, int n, int *out)
{
    const char *p;
    const char *end;
    int neg;
    long v;
    if (!a || !out) return IFC3D_ERR_BAD_ARG;
    p = ifc3d_skip_ws(a, a + n);
    end = a + n;
    neg = 0;
    if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); ++p; }
    if (p >= end || !ifc3d_is_digit((unsigned char)*p)) return IFC3D_ERR_PARSE;
    v = 0;
    while (p < end && ifc3d_is_digit((unsigned char)*p)) { v = v * 10L + (long)(*p - '0'); ++p; }
    if (neg) v = -v;
    *out = (int)v;
    return IFC3D_OK;
}

ifc3d_status ifc3d_parse_nested_tri_indices(ifc3d_model *m, ifc3d_entity *e, const char *arg, int len, ifc3d_diag *diag)
{
    const char *p;
    const char *end;
    const char *tuple_start;
    const char *q;
    const char *tok;
    const char *r;
    int depth;
    int vals[3];
    int nvals;
    if (!m || !e || !arg) return IFC3D_ERR_BAD_ARG;
    p = ifc3d_skip_ws(arg, arg + len);
    end = arg + len;
    e->first2 = m->tri_index_count;
    e->count2 = 0;
    while (p < end) {
        if (*p == '(') {
            r = ifc3d_skip_ws(p + 1, end);
            if (r < end && *r == '(') { ++p; continue; }
            tuple_start = p + 1;
            depth = 1;
            ++p;
            while (p < end && depth > 0) {
                if (*p == '(') ++depth;
                else if (*p == ')') --depth;
                ++p;
            }
            if (depth == 0) {
                q = tuple_start;
                nvals = 0;
                while (q < p - 1 && nvals < 3) {
                    q = ifc3d_skip_ws(q, p - 1);
                    tok = q;
                    while (q < p - 1 && *q != ',' && *q != ')') ++q;
                    if (parse_int_token(tok, (int)(q - tok), &vals[nvals]) == IFC3D_OK) ++nvals;
                    if (q < p - 1 && *q == ',') ++q;
                }
                if (nvals == 3) {
                    if (m->tri_index_count >= IFC3D_MAX_TRI_INDEX_TRIPLES) {
                        ifc3d_set_diag(diag, IFC3D_ERR_OVERFLOW, 0, "IFC triangle index storage overflow");
                        return IFC3D_ERR_OVERFLOW;
                    }
                    m->tri_indices[m->tri_index_count].a = vals[0];
                    m->tri_indices[m->tri_index_count].b = vals[1];
                    m->tri_indices[m->tri_index_count].c = vals[2];
                    ++m->tri_index_count;
                    ++e->count2;
                }
            }
        } else {
            ++p;
        }
    }
    return IFC3D_OK;
}
