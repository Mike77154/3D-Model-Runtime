#include "ifc3d_internal.h"

static const char *parse_digits_long(const char *p, const char *end, long *out)
{
    long v;
    if (p >= end || !ifc3d_is_digit((unsigned char)*p)) return (const char *)0;
    v = 0;
    while (p < end && ifc3d_is_digit((unsigned char)*p)) {
        v = v * 10L + (long)(*p - '0');
        ++p;
    }
    *out = v;
    return p;
}

static const char *parse_matching_paren(const char *p, const char *end)
{
    int depth;
    if (p >= end || *p != '(') return (const char *)0;
    depth = 1;
    ++p;
    while (p < end) {
        if (*p == '\'') {
            ++p;
            while (p < end) {
                if (*p == '\'') {
                    if (p + 1 < end && p[1] == '\'') { p += 2; continue; }
                    ++p;
                    break;
                }
                ++p;
            }
            continue;
        }
        if (p + 1 < end && p[0] == '/' && p[1] == '*') {
            p += 2;
            while (p + 1 < end && !(p[0] == '*' && p[1] == '/')) ++p;
            if (p + 1 < end) p += 2;
            continue;
        }
        if (*p == '(') ++depth;
        else if (*p == ')') {
            --depth;
            if (depth == 0) return p;
        }
        ++p;
    }
    return (const char *)0;
}

ifc3d_status ifc3d_step_parse(const char *text, long len, ifc3d_model *m, ifc3d_diag *diag)
{
    const char *p;
    const char *end;
    const char *data;
    const char *name_start;
    const char *args_start;
    const char *args_end;
    long id;
    int name_len;
    ifc3d_entity *e;

    if (!text || len <= 0 || !m) return IFC3D_ERR_BAD_ARG;
    ifc3d_model_init(m);
    m->source = text;
    m->source_len = len;
    end = text + len;

    if (!ifc3d_find_kw(text, end, "ISO-10303-21")) {
        ifc3d_set_diag(diag, IFC3D_ERR_PARSE, 0, "missing ISO-10303-21 magic");
        return IFC3D_ERR_PARSE;
    }

    data = ifc3d_find_kw(text, end, "DATA");
    if (!data) {
        ifc3d_set_diag(diag, IFC3D_ERR_PARSE, 0, "missing DATA section");
        return IFC3D_ERR_PARSE;
    }
    p = data;
    while (p < end && *p != ';') ++p;
    if (p >= end) {
        ifc3d_set_diag(diag, IFC3D_ERR_PARSE, (long)(data - text), "malformed DATA section");
        return IFC3D_ERR_PARSE;
    }
    ++p;

    while (p < end) {
        p = ifc3d_skip_ws_comments(p, end);
        if (p >= end) break;
        if (ifc3d_match_kw(p, end, "ENDSEC")) break;
        if (*p != '#') { ++p; continue; }
        ++p;
        {
            const char *id_start = p;
            p = parse_digits_long(p, end, &id);
            if (!p) {
                ifc3d_set_diag(diag, IFC3D_ERR_PARSE, (long)(id_start - text), "bad entity id");
                return IFC3D_ERR_PARSE;
            }
        }
        p = ifc3d_skip_ws_comments(p, end);
        if (p >= end || *p != '=') {
            ifc3d_set_diag(diag, IFC3D_ERR_PARSE, (long)(p - text), "expected '=' after entity id");
            return IFC3D_ERR_PARSE;
        }
        ++p;
        p = ifc3d_skip_ws_comments(p, end);
        name_start = p;
        if (p >= end || !ifc3d_is_alpha((unsigned char)*p)) {
            ifc3d_set_diag(diag, IFC3D_ERR_PARSE, (long)(p - text), "expected entity name");
            return IFC3D_ERR_PARSE;
        }
        while (p < end && ifc3d_is_name_char((unsigned char)*p)) ++p;
        name_len = (int)(p - name_start);
        p = ifc3d_skip_ws_comments(p, end);
        if (p >= end || *p != '(') {
            ifc3d_set_diag(diag, IFC3D_ERR_PARSE, (long)(p - text), "expected '(' after entity name");
            return IFC3D_ERR_PARSE;
        }
        args_start = p + 1;
        args_end = parse_matching_paren(p, end);
        if (!args_end) {
            ifc3d_set_diag(diag, IFC3D_ERR_PARSE, (long)(p - text), "unbalanced entity arguments");
            return IFC3D_ERR_PARSE;
        }
        p = args_end + 1;
        p = ifc3d_skip_ws_comments(p, end);
        if (p < end && *p == ';') ++p;
        else {
            ifc3d_set_diag(diag, IFC3D_ERR_PARSE, (long)(p - text), "expected ';' after entity");
            return IFC3D_ERR_PARSE;
        }

        if (m->entity_count >= IFC3D_MAX_ENTITIES) {
            ifc3d_set_diag(diag, IFC3D_ERR_OVERFLOW, (long)(name_start - text), "entity table overflow");
            return IFC3D_ERR_OVERFLOW;
        }
        e = &m->entities[m->entity_count++];
        e->id = id;
        ifc3d_copy_upper(e->name, IFC3D_NAME_MAX, name_start, name_len);
        e->type = ifc3d_type_from_name(e->name);
        e->args = args_start;
        e->args_len = (int)(args_end - args_start);
    }

    ifc3d_set_diag(diag, IFC3D_OK, 0, "ok");
    return IFC3D_OK;
}

ifc3d_status ifc3d_load_text(const char *text, long len, ifc3d_model *m, ifc3d_diag *diag)
{
    ifc3d_status st;
    st = ifc3d_step_parse(text, len, m, diag);
    if (st != IFC3D_OK) return st;
    return ifc3d_build_semantics(m, diag);
}
