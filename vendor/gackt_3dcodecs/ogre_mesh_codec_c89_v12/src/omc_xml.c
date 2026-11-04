#include "ogre_mesh_codec.h"
#include "omc_internal.h"
#include <string.h>

static int omc_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int omc_is_name_end(char c)
{
    return c == '\0' || c == '>' || c == '/' || omc_is_space(c);
}

static int omc_starts_with_name(const char *p, const char *end, const char *name)
{
    unsigned long i;
    i = 0UL;
    while (name[i]) {
        if (p + i >= end) return 0;
        if (p[i] != name[i]) return 0;
        i++;
    }
    if (p + i >= end) return 1;
    return omc_is_name_end(p[i]);
}

static const char *omc_find_ch(const char *p, const char *end, char ch)
{
    while (p < end) {
        if (*p == ch) return p;
        p++;
    }
    return (const char *)0;
}


static int omc_xml_entity_literal(const char *p, const char *end, const char *lit)
{
    unsigned long i;
    i = 0UL;
    while (lit[i]) {
        if (p + i >= end) return 0;
        if (p[i] != lit[i]) return 0;
        i++;
    }
    return 1;
}

static int omc_xml_hex_digit(char c)
{
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (c >= 'a' && c <= 'f') return (int)(c - 'a') + 10;
    if (c >= 'A' && c <= 'F') return (int)(c - 'A') + 10;
    return -1;
}

static int omc_xml_decode_entity(const char *p, const char *end, char *out, unsigned long *used)
{
    unsigned long i;
    unsigned long val;
    int base;
    int d;
    if (!p || p >= end || *p != '&' || !out || !used) return 0;
    if (omc_xml_entity_literal(p, end, "&amp;")) { *out = '&'; *used = 5UL; return 1; }
    if (omc_xml_entity_literal(p, end, "&lt;")) { *out = '<'; *used = 4UL; return 1; }
    if (omc_xml_entity_literal(p, end, "&gt;")) { *out = '>'; *used = 4UL; return 1; }
    if (omc_xml_entity_literal(p, end, "&quot;")) { *out = '"'; *used = 6UL; return 1; }
    if (omc_xml_entity_literal(p, end, "&apos;")) { *out = '\''; *used = 6UL; return 1; }
    if (p + 3 < end && p[1] == '#') {
        i = 2UL;
        base = 10;
        if (p + i < end && (p[i] == 'x' || p[i] == 'X')) { base = 16; i++; }
        val = 0UL;
        d = omc_xml_hex_digit(p[i]);
        if (d < 0 || (base == 10 && d > 9)) return 0;
        while (p + i < end && p[i] != ';') {
            d = omc_xml_hex_digit(p[i]);
            if (d < 0 || (base == 10 && d > 9)) return 0;
            val = val * (unsigned long)base + (unsigned long)d;
            if (val > 255UL) val = (unsigned long)'?';
            i++;
        }
        if (p + i >= end || p[i] != ';') return 0;
        *out = (char)(unsigned char)val;
        *used = i + 1UL;
        return 1;
    }
    return 0;
}

static const char *omc_find_open(const char *p, const char *end, const char *name)
{
    const char *q;
    q = p;
    while (q < end) {
        q = omc_find_ch(q, end, '<');
        if (!q) return (const char *)0;
        if (q + 1 < end && q[1] != '/' && q[1] != '!' && q[1] != '?') {
            if (omc_starts_with_name(q + 1, end, name)) return q;
        }
        q++;
    }
    return (const char *)0;
}

static const char *omc_find_close(const char *p, const char *end, const char *name)
{
    const char *q;
    unsigned long i;
    q = p;
    while (q < end) {
        q = omc_find_ch(q, end, '<');
        if (!q) return (const char *)0;
        if (q + 2 < end && q[1] == '/') {
            i = 0UL;
            while (name[i] && q + 2 + i < end && q[2 + i] == name[i]) i++;
            if (!name[i] && q + 2 + i < end && omc_is_name_end(q[2 + i])) return q;
        }
        q++;
    }
    return (const char *)0;
}

static const char *omc_tag_end(const char *tag, const char *end)
{
    return omc_find_ch(tag, end, '>');
}

static int omc_get_attr(const char *tag, const char *tag_end, const char *name, char *out, unsigned long cap)
{
    const char *p;
    const char *q;
    const char *v;
    unsigned long i;
    char quote;

    if (!tag || !tag_end || !name || !out || cap == 0UL) return 0;
    out[0] = '\0';
    p = tag;
    while (p < tag_end) {
        p = omc_find_ch(p, tag_end, name[0]);
        if (!p) return 0;
        if (p > tag && p[-1] != '<' && !omc_is_space(p[-1])) {
            p++;
            continue;
        }
        q = name;
        v = p;
        while (v < tag_end && *q && *v == *q) { v++; q++; }
        if (!*q) {
            while (v < tag_end && omc_is_space(*v)) v++;
            if (v < tag_end && *v == '=') {
                v++;
                while (v < tag_end && omc_is_space(*v)) v++;
                if (v < tag_end && (*v == '"' || *v == '\'')) {
                    quote = *v;
                    v++;
                    i = 0UL;
                    while (v < tag_end && *v != quote && i + 1UL < cap) {
                        char decoded;
                        unsigned long used;
                        if (*v == '&' && omc_xml_decode_entity(v, tag_end, &decoded, &used)) {
                            out[i++] = decoded;
                            v += used;
                        } else {
                            out[i++] = *v++;
                        }
                    }
                    out[i] = '\0';
                    return 1;
                }
            }
        }
        p++;
    }
    return 0;
}

static unsigned long omc_to_ulong(const char *s)
{
    unsigned long v;
    v = 0UL;
    if (!s) return 0UL;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    while (*s >= '0' && *s <= '9') {
        if (v < 429496729UL) v = v * 10UL + (unsigned long)(*s - '0');
        s++;
    }
    return v;
}

static int omc_to_bool(const char *s)
{
    if (!s || !s[0]) return 0;
    if (s[0] == 't' || s[0] == 'T' || s[0] == '1' || s[0] == 'y' || s[0] == 'Y') return 1;
    return 0;
}

static int omc_attr_ulong(const char *tag, const char *te, const char *name, unsigned long *out)
{
    char tmp[OMC_XML_ATTR_MAX];
    if (!omc_get_attr(tag, te, name, tmp, sizeof(tmp))) return 0;
    *out = omc_to_ulong(tmp);
    return 1;
}

static int omc_attr_fixed(const char *tag, const char *te, const char *name, OMC_Fixed *out)
{
    char tmp[OMC_XML_ATTR_MAX];
    if (!omc_get_attr(tag, te, name, tmp, sizeof(tmp))) return 0;
    *out = omc_fixed_from_decimal(tmp);
    return 1;
}

static unsigned long omc_count_open_tags(const char *p, const char *end, const char *name)
{
    unsigned long n;
    const char *q;
    n = 0UL;
    q = p;
    while ((q = omc_find_open(q, end, name)) != (const char *)0) {
        n++;
        q++;
    }
    return n;
}

static int omc_decimal_string_to_ulong_strict(const char *s, unsigned long *out)
{
    unsigned long v;
    if (!s || !*s || !out) return 0;
    v = 0UL;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        v = v * 10UL + (unsigned long)(*s - '0');
        s++;
    }
    *out = v;
    return 1;
}

static int omc_parse_patch_operation(const char *s, OMC_U8 *out)
{
    const char *prefix;
    const char *suffix;
    unsigned long plen;
    unsigned long slen;
    unsigned long len;
    unsigned long i;
    unsigned long n;
    char tmp[16];

    prefix = "patch_";
    suffix = "_control_point";
    plen = (unsigned long)strlen(prefix);
    slen = (unsigned long)strlen(suffix);
    if (!s || !out) return 0;
    len = (unsigned long)strlen(s);
    if (len <= plen + slen || strncmp(s, prefix, plen) != 0 || strcmp(s + len - slen, suffix) != 0) return 0;
    if (len - plen - slen >= sizeof(tmp)) return 0;
    i = 0UL;
    while (i < len - plen - slen) {
        tmp[i] = s[plen + i];
        i++;
    }
    tmp[i] = '\0';
    if (!omc_decimal_string_to_ulong_strict(tmp, &n)) return 0;
    if (n < 1UL || n > 32UL) return 0;
    *out = (OMC_U8)(OMC_OP_PATCH_1_CONTROL_POINT + (OMC_U8)(n - 1UL));
    return 1;
}

static OMC_U8 omc_operation_from_string(const char *s)
{
    OMC_U8 op;
    unsigned long numeric;
    if (!s || !s[0]) return (OMC_U8)OMC_OP_TRIANGLE_LIST;
    if (strcmp(s, "point_list") == 0) return (OMC_U8)OMC_OP_POINT_LIST;
    if (strcmp(s, "line_list") == 0) return (OMC_U8)OMC_OP_LINE_LIST;
    if (strcmp(s, "line_strip") == 0) return (OMC_U8)OMC_OP_LINE_STRIP;
    if (strcmp(s, "triangle_strip") == 0) return (OMC_U8)OMC_OP_TRIANGLE_STRIP;
    if (strcmp(s, "triangle_fan") == 0) return (OMC_U8)OMC_OP_TRIANGLE_FAN;
    if (strcmp(s, "line_list_adj") == 0) return (OMC_U8)OMC_OP_LINE_LIST_ADJ;
    if (strcmp(s, "line_strip_adj") == 0) return (OMC_U8)OMC_OP_LINE_STRIP_ADJ;
    if (strcmp(s, "triangle_list_adj") == 0) return (OMC_U8)OMC_OP_TRIANGLE_LIST_ADJ;
    if (strcmp(s, "triangle_strip_adj") == 0) return (OMC_U8)OMC_OP_TRIANGLE_STRIP_ADJ;
    if (omc_parse_patch_operation(s, &op)) return op;
    if (omc_decimal_string_to_ulong_strict(s, &numeric) && numeric <= 255UL) return (OMC_U8)numeric;
    return (OMC_U8)OMC_OP_TRIANGLE_LIST;
}

static OMC_U8 omc_uv_dimension_from_string(const char *s)
{
    if (!s || !s[0]) return 2U;
    if (strcmp(s, "1") == 0 || strcmp(s, "float1") == 0 || strcmp(s, "short1") == 0 || strcmp(s, "half1") == 0) return 1U;
    if (strcmp(s, "2") == 0 || strcmp(s, "float2") == 0 || strcmp(s, "short2") == 0 || strcmp(s, "half2") == 0) return 2U;
    if (strcmp(s, "3") == 0 || strcmp(s, "float3") == 0 || strcmp(s, "short3") == 0 || strcmp(s, "half3") == 0) return 3U;
    if (strcmp(s, "4") == 0 || strcmp(s, "float4") == 0 || strcmp(s, "short4") == 0 || strcmp(s, "half4") == 0 || strcmp(s, "ubyte4") == 0 || strcmp(s, "colour") == 0 || strcmp(s, "colour_argb") == 0 || strcmp(s, "colour_abgr") == 0) return 4U;
    return 2U;
}

static void omc_parse_vertexbuffer_uv_dims(const char *vb, const char *vbt, OMC_U8 dims[OMC_TEXCOORD_SETS])
{
    char tmp[OMC_XML_ATTR_MAX];
    unsigned long i;
    static const char *names[OMC_TEXCOORD_SETS] = {
        "texture_coord_dimensions_0",
        "texture_coord_dimensions_1",
        "texture_coord_dimensions_2",
        "texture_coord_dimensions_3",
        "texture_coord_dimensions_4",
        "texture_coord_dimensions_5",
        "texture_coord_dimensions_6",
        "texture_coord_dimensions_7"
    };
    i = 0UL;
    while (i < (unsigned long)OMC_TEXCOORD_SETS) {
        dims[i] = 2U;
        if (omc_get_attr(vb, vbt, names[i], tmp, sizeof(tmp))) dims[i] = omc_uv_dimension_from_string(tmp);
        i++;
    }
}

static void omc_parse_colour_string(const char *s, OMC_U32 *out_rgba)
{
    OMC_Fixed vals[4];
    int vi;
    const char *p;
    char tmp[48];
    unsigned long ti;
    long c[4];
    OMC_U32 r;
    OMC_U32 g;
    OMC_U32 b;
    OMC_U32 a;

    vals[0] = OMC_FIXED_ONE;
    vals[1] = OMC_FIXED_ONE;
    vals[2] = OMC_FIXED_ONE;
    vals[3] = OMC_FIXED_ONE;
    vi = 0;
    p = s;
    while (p && *p && vi < 4) {
        while (*p && omc_is_space(*p)) p++;
        ti = 0UL;
        while (*p && !omc_is_space(*p) && ti + 1UL < sizeof(tmp)) {
            tmp[ti++] = *p++;
        }
        tmp[ti] = '\0';
        if (ti > 0UL) {
            vals[vi++] = omc_fixed_from_decimal(tmp);
        }
    }
    vi = 0;
    while (vi < 4) {
        c[vi] = (vals[vi] * 255L + OMC_FIXED_HALF) >> OMC_FIXED_SHIFT;
        if (c[vi] < 0L) c[vi] = 0L;
        if (c[vi] > 255L) c[vi] = 255L;
        vi++;
    }
    r = (OMC_U32)c[0];
    g = (OMC_U32)c[1];
    b = (OMC_U32)c[2];
    a = (OMC_U32)c[3];
    *out_rgba = (r << 24) | (g << 16) | (b << 8) | a;
}

static void omc_parse_vertex_child(OMC_Vertex *v, const char *vp, const char *vend, const OMC_U8 uv_dims[OMC_TEXCOORD_SETS])
{
    const char *t;
    const char *te;
    char tmp[OMC_XML_ATTR_MAX];
    unsigned long uv_index;

    t = omc_find_open(vp, vend, "position");
    if (t) {
        te = omc_tag_end(t, vend);
        if (te) {
            omc_attr_fixed(t, te, "x", &v->position.x);
            omc_attr_fixed(t, te, "y", &v->position.y);
            omc_attr_fixed(t, te, "z", &v->position.z);
            v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_POSITION);
        }
    }

    t = omc_find_open(vp, vend, "normal");
    if (t) {
        te = omc_tag_end(t, vend);
        if (te) {
            omc_attr_fixed(t, te, "x", &v->normal.x);
            omc_attr_fixed(t, te, "y", &v->normal.y);
            omc_attr_fixed(t, te, "z", &v->normal.z);
            v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_NORMAL);
        }
    }

    t = omc_find_open(vp, vend, "tangent");
    if (t) {
        te = omc_tag_end(t, vend);
        if (te) {
            omc_attr_fixed(t, te, "x", &v->tangent.x);
            omc_attr_fixed(t, te, "y", &v->tangent.y);
            omc_attr_fixed(t, te, "z", &v->tangent.z);
            if (!omc_attr_fixed(t, te, "w", &v->tangent.w)) v->tangent.w = OMC_FIXED_ONE;
            v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_TANGENT);
        }
    }

    t = omc_find_open(vp, vend, "binormal");
    if (t) {
        te = omc_tag_end(t, vend);
        if (te) {
            omc_attr_fixed(t, te, "x", &v->binormal.x);
            omc_attr_fixed(t, te, "y", &v->binormal.y);
            omc_attr_fixed(t, te, "z", &v->binormal.z);
            v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_BINORMAL);
        }
    }

    t = omc_find_open(vp, vend, "colour_diffuse");
    if (t) {
        te = omc_tag_end(t, vend);
        if (te && omc_get_attr(t, te, "value", tmp, sizeof(tmp))) {
            omc_parse_colour_string(tmp, &v->diffuse_rgba);
            v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_DIFFUSE);
        }
    }

    t = omc_find_open(vp, vend, "colour_specular");
    if (t) {
        te = omc_tag_end(t, vend);
        if (te && omc_get_attr(t, te, "value", tmp, sizeof(tmp))) {
            omc_parse_colour_string(tmp, &v->specular_rgba);
            v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_SPECULAR);
        }
    }

    uv_index = 0UL;
    t = vp;
    while ((t = omc_find_open(t, vend, "texcoord")) != (const char *)0) {
        te = omc_tag_end(t, vend);
        if (!te) break;
        if (uv_index < (unsigned long)OMC_TEXCOORD_SETS) {
            omc_attr_fixed(t, te, "u", &v->texcoord[uv_index].u);
            omc_attr_fixed(t, te, "v", &v->texcoord[uv_index].v);
            v->texcoord_w[uv_index] = 0;
            v->texcoord_x[uv_index] = 0;
            v->texcoord_dims[uv_index] = uv_dims ? uv_dims[uv_index] : 2U;
            if (omc_attr_fixed(t, te, "w", &v->texcoord_w[uv_index]) && v->texcoord_dims[uv_index] < 3U) v->texcoord_dims[uv_index] = 3U;
            if (omc_attr_fixed(t, te, "x", &v->texcoord_x[uv_index]) && v->texcoord_dims[uv_index] < 4U) v->texcoord_dims[uv_index] = 4U;
            v->texcoord_count = (OMC_U8)(uv_index + 1UL);
            v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_TEXCOORD);
        }
        uv_index++;
        t = te + 1;
    }
}

static int omc_reserve_vertices(OMC_Mesh *mesh, unsigned long count, unsigned long *start)
{
    if (mesh->vertex_count + count > (OMC_U32)OMC_MAX_VERTICES) {
        omc__diag_public(mesh, OMC_ERR_CAPACITY, "vertex capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    *start = mesh->vertex_count;
    mesh->vertex_count += count;
    return OMC_OK;
}

static int omc_parse_geometry_block(OMC_Mesh *mesh, const char *g, const char *gend, unsigned long start, unsigned long declared_count)
{
    const char *vb;
    const char *vbe;
    const char *vp;
    const char *ve;
    const char *next_v;
    const char *vbt;
    OMC_U8 uv_dims[OMC_TEXCOORD_SETS];
    unsigned long local_index;

    vb = g;
    while ((vb = omc_find_open(vb, gend, "vertexbuffer")) != (const char *)0) {
        vbt = omc_tag_end(vb, gend);
        vbe = omc_find_close(vb, gend, "vertexbuffer");
        if (!vbt || !vbe) break;
        omc_parse_vertexbuffer_uv_dims(vb, vbt, uv_dims);
        vp = vb;
        local_index = 0UL;
        while ((vp = omc_find_open(vp, vbe, "vertex")) != (const char *)0) {
            ve = omc_tag_end(vp, vbe);
            if (!ve) break;
            next_v = omc_find_close(ve, vbe, "vertex");
            if (!next_v) break;
            if (local_index < declared_count && start + local_index < (OMC_U32)OMC_MAX_VERTICES) {
                omc_parse_vertex_child(&mesh->vertices[start + local_index], ve, next_v, uv_dims);
            }
            local_index++;
            vp = next_v + 1;
        }
        vb = vbe + 1;
    }
    return OMC_OK;
}

static int omc_parse_shared_geometry(OMC_Mesh *mesh, const char *p, const char *end)
{
    const char *g;
    const char *ge;
    const char *gt;
    unsigned long vc;
    unsigned long start;
    int r;

    g = omc_find_open(p, end, "sharedgeometry");
    if (!g) return OMC_OK;
    gt = omc_tag_end(g, end);
    ge = omc_find_close(g, end, "sharedgeometry");
    if (!gt || !ge) return OMC_ERR_FORMAT;
    vc = 0UL;
    omc_attr_ulong(g, gt, "vertexcount", &vc);
    if (vc == 0UL) vc = omc_count_open_tags(g, ge, "vertex");
    r = omc_reserve_vertices(mesh, vc, &start);
    if (r != OMC_OK) return r;
    mesh->shared_vertex_count = (OMC_U32)vc;
    return omc_parse_geometry_block(mesh, gt + 1, ge, start, vc);
}

static int omc_add_index(OMC_Mesh *mesh, OMC_U32 v)
{
    if (mesh->index_count + 1UL > (OMC_U32)OMC_MAX_INDICES) {
        omc__diag_public(mesh, OMC_ERR_CAPACITY, "index capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    mesh->indices[mesh->index_count++] = v;
    return OMC_OK;
}

static int omc_add_indices(OMC_Mesh *mesh, OMC_U32 a, OMC_U32 b, OMC_U32 c)
{
    int r;

    r = omc_add_index(mesh, a);
    if (r != OMC_OK) return r;
    r = omc_add_index(mesh, b);
    if (r != OMC_OK) return r;
    return omc_add_index(mesh, c);
}

static int omc_add_indices4(OMC_Mesh *mesh, OMC_U32 a, OMC_U32 b, OMC_U32 c, OMC_U32 d)
{
    int r;

    r = omc_add_indices(mesh, a, b, c);
    if (r != OMC_OK) return r;
    return omc_add_index(mesh, d);
}

static int omc_add_indices6(OMC_Mesh *mesh, OMC_U32 a, OMC_U32 b, OMC_U32 c, OMC_U32 d, OMC_U32 e, OMC_U32 f)
{
    int r;

    r = omc_add_indices4(mesh, a, b, c, d);
    if (r != OMC_OK) return r;
    r = omc_add_index(mesh, e);
    if (r != OMC_OK) return r;
    return omc_add_index(mesh, f);
}

static int omc_parse_faces(OMC_Mesh *mesh, OMC_Submesh *sm, const char *p, const char *end)
{
    const char *fb;
    const char *fe;
    const char *f;
    const char *te;
    unsigned long v1;
    unsigned long v2;
    unsigned long v3;
    unsigned long v4;
    unsigned long v5;
    unsigned long v6;
    unsigned long first_v1;
    unsigned long prev_v1;
    unsigned long prev_v2;
    unsigned long prev_v3;
    unsigned long face_no;
    OMC_U8 op;
    int has_v2;
    int has_v3;
    int has_v4;
    int has_v5;
    int has_v6;
    int converted;
    int r;

    fb = omc_find_open(p, end, "faces");
    if (!fb) return OMC_OK;
    fe = omc_find_close(fb, end, "faces");
    if (!fe) return OMC_ERR_FORMAT;

    sm->index_start = mesh->index_count;
    f = fb;
    first_v1 = 0UL; prev_v1 = 0UL; prev_v2 = 0UL; prev_v3 = 0UL;
    face_no = 0UL;
    converted = 0;
    op = sm->operation_type ? sm->operation_type : (OMC_U8)OMC_OP_TRIANGLE_LIST;
    while ((f = omc_find_open(f, fe, "face")) != (const char *)0) {
        te = omc_tag_end(f, fe);
        if (!te) break;
        v1 = 0UL; v2 = 0UL; v3 = 0UL; v4 = 0UL; v5 = 0UL; v6 = 0UL;
        omc_attr_ulong(f, te, "v1", &v1);
        has_v2 = omc_attr_ulong(f, te, "v2", &v2);
        has_v3 = omc_attr_ulong(f, te, "v3", &v3);
        has_v4 = omc_attr_ulong(f, te, "v4", &v4);
        has_v5 = omc_attr_ulong(f, te, "v5", &v5);
        has_v6 = omc_attr_ulong(f, te, "v6", &v6);

        if (op == (OMC_U8)OMC_OP_POINT_LIST) {
            r = omc_add_index(mesh, (OMC_U32)v1);
        } else if (op == (OMC_U8)OMC_OP_LINE_LIST) {
            if (!has_v2) return OMC_ERR_FORMAT;
            r = omc_add_index(mesh, (OMC_U32)v1);
            if (r == OMC_OK) r = omc_add_index(mesh, (OMC_U32)v2);
        } else if (op == (OMC_U8)OMC_OP_LINE_STRIP) {
            r = omc_add_index(mesh, (OMC_U32)v1);
            if (r == OMC_OK && face_no == 0UL && has_v2) r = omc_add_index(mesh, (OMC_U32)v2);
        } else if (op == (OMC_U8)OMC_OP_LINE_LIST_ADJ) {
            if (!has_v2 || !has_v3 || !has_v4) return OMC_ERR_FORMAT;
            r = omc_add_indices4(mesh, (OMC_U32)v1, (OMC_U32)v2, (OMC_U32)v3, (OMC_U32)v4);
        } else if (op == (OMC_U8)OMC_OP_LINE_STRIP_ADJ) {
            r = omc_add_index(mesh, (OMC_U32)v1);
            if (r == OMC_OK && face_no == 0UL && has_v2) r = omc_add_index(mesh, (OMC_U32)v2);
            if (r == OMC_OK && face_no == 0UL && has_v3) r = omc_add_index(mesh, (OMC_U32)v3);
            if (r == OMC_OK && face_no == 0UL && has_v4) r = omc_add_index(mesh, (OMC_U32)v4);
        } else if (op == (OMC_U8)OMC_OP_TRIANGLE_LIST_ADJ) {
            if (has_v4 || has_v5 || has_v6) {
                if (!has_v2 || !has_v3 || !has_v4 || !has_v5 || !has_v6) return OMC_ERR_FORMAT;
                r = omc_add_indices6(mesh, (OMC_U32)v1, (OMC_U32)v2, (OMC_U32)v3,
                                     (OMC_U32)v4, (OMC_U32)v5, (OMC_U32)v6);
            } else {
                if (!has_v2 || !has_v3) return OMC_ERR_FORMAT;
                r = omc_add_indices(mesh, (OMC_U32)v1, (OMC_U32)v2, (OMC_U32)v3);
            }
        } else if (op == (OMC_U8)OMC_OP_TRIANGLE_STRIP_ADJ) {
            if (face_no == 0UL && has_v2 && has_v3 && has_v4 && has_v5 && has_v6) {
                r = omc_add_indices6(mesh, (OMC_U32)v1, (OMC_U32)v2, (OMC_U32)v3,
                                     (OMC_U32)v4, (OMC_U32)v5, (OMC_U32)v6);
            } else {
                r = omc_add_index(mesh, (OMC_U32)v1);
                if (r == OMC_OK && has_v2) r = omc_add_index(mesh, (OMC_U32)v2);
            }
        } else if ((op == (OMC_U8)OMC_OP_TRIANGLE_STRIP || op == (OMC_U8)OMC_OP_TRIANGLE_FAN) && (!has_v2 || !has_v3) && face_no > 0UL) {
            if (op == (OMC_U8)OMC_OP_TRIANGLE_STRIP) {
                if ((face_no & 1UL) != 0UL) r = omc_add_indices(mesh, (OMC_U32)prev_v2, (OMC_U32)v1, (OMC_U32)prev_v3);
                else r = omc_add_indices(mesh, (OMC_U32)prev_v2, (OMC_U32)prev_v3, (OMC_U32)v1);
                prev_v1 = prev_v2;
                prev_v2 = prev_v3;
                prev_v3 = v1;
            } else {
                r = omc_add_indices(mesh, (OMC_U32)first_v1, (OMC_U32)prev_v3, (OMC_U32)v1);
                prev_v3 = v1;
            }
            converted = 1;
        } else {
            if (!has_v2 || !has_v3) return OMC_ERR_FORMAT;
            r = omc_add_indices(mesh, (OMC_U32)v1, (OMC_U32)v2, (OMC_U32)v3);
            if (face_no == 0UL) first_v1 = v1;
            prev_v1 = v1;
            prev_v2 = v2;
            prev_v3 = v3;
        }
        if (r != OMC_OK) return r;
        face_no++;
        f = te + 1;
    }
    if (converted) {
        sm->operation_type = (OMC_U8)OMC_OP_TRIANGLE_LIST;
        omc__diag_public(mesh, OMC_OK, "triangle strip/fan XML faces expanded to triangle_list");
    }
    (void)prev_v1;
    sm->index_count = mesh->index_count - sm->index_start;
    return OMC_OK;
}

static int omc_parse_bone_assignments_mesh(OMC_Mesh *mesh, const char *p, const char *end)
{
    const char *ba;
    const char *bae;
    const char *vba;
    const char *te;
    unsigned long a;
    OMC_BoneAssignment *dst;

    ba = omc_find_open(p, end, "boneassignments");
    if (!ba) return OMC_OK;
    bae = omc_find_close(ba, end, "boneassignments");
    if (!bae) return OMC_ERR_FORMAT;

    vba = ba;
    while ((vba = omc_find_open(vba, bae, "vertexboneassignment")) != (const char *)0) {
        te = omc_tag_end(vba, bae);
        if (!te) break;
        if (mesh->bone_assignment_count >= (OMC_U32)OMC_MAX_BONE_ASSIGNMENTS) {
            omc__diag_public(mesh, OMC_ERR_CAPACITY, "bone assignment capacity exceeded");
            return OMC_ERR_CAPACITY;
        }
        dst = &mesh->bone_assignments[mesh->bone_assignment_count++];
        a = 0UL;
        omc_attr_ulong(vba, te, "vertexindex", &a);
        dst->vertex_index = (OMC_U32)a;
        a = 0UL;
        omc_attr_ulong(vba, te, "boneindex", &a);
        dst->bone_index = (OMC_U16)a;
        if (!omc_attr_fixed(vba, te, "weight", &dst->weight)) dst->weight = OMC_FIXED_ONE;
        vba = te + 1;
    }
    return OMC_OK;
}


static int omc_parse_texture_aliases_submesh(OMC_Mesh *mesh, OMC_U16 submesh_index, const char *p, const char *end)
{
    const char *ts;
    const char *tse;
    const char *tx;
    const char *te;
    OMC_TextureAlias *dst;

    ts = omc_find_open(p, end, "textures");
    if (!ts) return OMC_OK;
    tse = omc_find_close(ts, end, "textures");
    if (!tse) return OMC_ERR_FORMAT;

    tx = ts;
    while ((tx = omc_find_open(tx, tse, "texture")) != (const char *)0) {
        te = omc_tag_end(tx, tse);
        if (!te) break;
        if (mesh->texture_alias_count >= (OMC_U32)OMC_MAX_TEXTURE_ALIASES) {
            omc__diag_public(mesh, OMC_ERR_CAPACITY, "texture alias capacity exceeded");
            return OMC_ERR_CAPACITY;
        }
        dst = &mesh->texture_aliases[mesh->texture_alias_count++];
        dst->submesh_index = submesh_index;
        dst->alias[0] = '\0';
        dst->texture[0] = '\0';
        (void)omc_get_attr(tx, te, "alias", dst->alias, sizeof(dst->alias));
        (void)omc_get_attr(tx, te, "name", dst->texture, sizeof(dst->texture));
        tx = te + 1;
    }
    return OMC_OK;
}

static int omc_parse_bone_assignments_submesh(OMC_Mesh *mesh, OMC_U16 submesh_index, const char *p, const char *end)
{
    const char *ba;
    const char *bae;
    const char *vba;
    const char *te;
    unsigned long a;
    OMC_SubmeshBoneAssignment *dst;

    ba = omc_find_open(p, end, "boneassignments");
    if (!ba) return OMC_OK;
    bae = omc_find_close(ba, end, "boneassignments");
    if (!bae) return OMC_ERR_FORMAT;

    vba = ba;
    while ((vba = omc_find_open(vba, bae, "vertexboneassignment")) != (const char *)0) {
        te = omc_tag_end(vba, bae);
        if (!te) break;
        if (mesh->submesh_bone_assignment_count >= (OMC_U32)OMC_MAX_SUBMESH_BONE_ASSIGNMENTS) {
            omc__diag_public(mesh, OMC_ERR_CAPACITY, "submesh bone assignment capacity exceeded");
            return OMC_ERR_CAPACITY;
        }
        dst = &mesh->submesh_bone_assignments[mesh->submesh_bone_assignment_count++];
        dst->submesh_index = submesh_index;
        a = 0UL;
        omc_attr_ulong(vba, te, "vertexindex", &a);
        dst->assignment.vertex_index = (OMC_U32)a;
        a = 0UL;
        omc_attr_ulong(vba, te, "boneindex", &a);
        dst->assignment.bone_index = (OMC_U16)a;
        if (!omc_attr_fixed(vba, te, "weight", &dst->assignment.weight)) dst->assignment.weight = OMC_FIXED_ONE;
        vba = te + 1;
    }
    return OMC_OK;
}

static int omc_parse_submeshes(OMC_Mesh *mesh, const char *p, const char *end)
{
    const char *ss;
    const char *sse;
    const char *s;
    const char *se;
    const char *st;
    const char *g;
    const char *ge;
    const char *gt;
    char tmp[OMC_XML_ATTR_MAX];
    OMC_Submesh *sm;
    unsigned long vc;
    unsigned long vstart;
    int r;

    ss = omc_find_open(p, end, "submeshes");
    if (!ss) return OMC_OK;
    sse = omc_find_close(ss, end, "submeshes");
    if (!sse) return OMC_ERR_FORMAT;

    s = ss;
    while ((s = omc_find_open(s, sse, "submesh")) != (const char *)0) {
        st = omc_tag_end(s, sse);
        se = omc_find_close(s, sse, "submesh");
        if (!st || !se) break;
        if (mesh->submesh_count >= (OMC_U16)OMC_MAX_SUBMESHES) {
            omc__diag_public(mesh, OMC_ERR_CAPACITY, "submesh capacity exceeded");
            return OMC_ERR_CAPACITY;
        }
        sm = &mesh->submeshes[mesh->submesh_count];
        if (omc_get_attr(s, st, "material", tmp, sizeof(tmp))) omc__copy_str_public(sm->material, sizeof(sm->material), tmp);
        else omc__copy_str_public(sm->material, sizeof(sm->material), "BaseWhite");
        sm->use_shared_vertices = 1U;
        if (omc_get_attr(s, st, "usesharedvertices", tmp, sizeof(tmp))) sm->use_shared_vertices = (OMC_U8)omc_to_bool(tmp);
        sm->use_32bit_indices = 0U;
        if (omc_get_attr(s, st, "use32bitindexes", tmp, sizeof(tmp))) sm->use_32bit_indices = (OMC_U8)omc_to_bool(tmp);
        sm->operation_type = (OMC_U8)OMC_OP_TRIANGLE_LIST;
        if (omc_get_attr(s, st, "operationtype", tmp, sizeof(tmp))) sm->operation_type = omc_operation_from_string(tmp);
        sm->vertex_start = 0UL;
        sm->vertex_count = mesh->shared_vertex_count;

        r = omc_parse_texture_aliases_submesh(mesh, mesh->submesh_count, st + 1, se);
        if (r != OMC_OK) return r;

        r = omc_parse_faces(mesh, sm, st + 1, se);
        if (r != OMC_OK) return r;

        g = omc_find_open(st + 1, se, "geometry");
        if (g) {
            gt = omc_tag_end(g, se);
            ge = omc_find_close(g, se, "geometry");
            if (gt && ge) {
                vc = 0UL;
                omc_attr_ulong(g, gt, "vertexcount", &vc);
                if (vc == 0UL) vc = omc_count_open_tags(g, ge, "vertex");
                r = omc_reserve_vertices(mesh, vc, &vstart);
                if (r != OMC_OK) return r;
                sm->has_dedicated_geometry = 1U;
                sm->use_shared_vertices = 0U;
                sm->vertex_start = (OMC_U32)vstart;
                sm->vertex_count = (OMC_U32)vc;
                r = omc_parse_geometry_block(mesh, gt + 1, ge, vstart, vc);
                if (r != OMC_OK) return r;
            }
        }

        r = omc_parse_bone_assignments_submesh(mesh, mesh->submesh_count, st + 1, se);
        if (r != OMC_OK) return r;

        mesh->submesh_count++;
        s = se + 1;
    }
    return OMC_OK;
}

static int omc_parse_skeletonlink(OMC_Mesh *mesh, const char *p, const char *end)
{
    const char *s;
    const char *se;
    char tmp[OMC_XML_ATTR_MAX];
    s = omc_find_open(p, end, "skeletonlink");
    if (!s) return OMC_OK;
    se = omc_tag_end(s, end);
    if (!se) return OMC_ERR_FORMAT;
    if (omc_get_attr(s, se, "name", tmp, sizeof(tmp))) {
        omc__copy_str_public(mesh->skeleton_name, sizeof(mesh->skeleton_name), tmp);
    }
    return OMC_OK;
}

static int omc_parse_submesh_names(OMC_Mesh *mesh, const char *p, const char *end)
{
    const char *nt;
    const char *ne;
    const char *n;
    const char *te;
    char tmp[OMC_XML_ATTR_MAX];
    unsigned long idx;

    nt = omc_find_open(p, end, "submeshnames");
    if (!nt) return OMC_OK;
    ne = omc_find_close(nt, end, "submeshnames");
    if (!ne) return OMC_ERR_FORMAT;
    n = nt;
    while ((n = omc_find_open(n, ne, "submeshname")) != (const char *)0) {
        te = omc_tag_end(n, ne);
        if (!te) break;
        idx = 0UL;
        omc_attr_ulong(n, te, "index", &idx);
        if (idx < (unsigned long)mesh->submesh_count && omc_get_attr(n, te, "name", tmp, sizeof(tmp))) {
            omc__copy_str_public(mesh->submeshes[idx].name, sizeof(mesh->submeshes[idx].name), tmp);
        }
        n = te + 1;
    }
    return OMC_OK;
}


static int omc_parse_levelofdetail(OMC_Mesh *mesh, const char *p, const char *end)
{
    const char *ls;
    const char *lse;
    const char *lt;
    const char *lm;
    const char *lmt;
    const char *lg;
    const char *lge;
    const char *lgt;
    const char *fl;
    const char *fle;
    const char *flt;
    const char *fc;
    const char *fct;
    char tmp[OMC_XML_ATTR_MAX];
    unsigned long idx;
    unsigned long v1;
    unsigned long v2;
    unsigned long v3;
    OMC_LodLevel *level;
    OMC_LodFace *face;
    OMC_U16 lod_index;
    OMC_U16 submesh_index;

    ls = omc_find_open(p, end, "levelofdetail");
    if (!ls) return OMC_OK;
    lt = omc_tag_end(ls, end);
    lse = omc_find_close(ls, end, "levelofdetail");
    if (!lt || !lse) return OMC_ERR_FORMAT;

    if (omc_get_attr(ls, lt, "strategy", tmp, sizeof(tmp))) omc__copy_str_public(mesh->lod_strategy, sizeof(mesh->lod_strategy), tmp);
    else omc__copy_str_public(mesh->lod_strategy, sizeof(mesh->lod_strategy), "Distance");
    if (omc_get_attr(ls, lt, "manual", tmp, sizeof(tmp))) mesh->lod_manual = (OMC_U8)omc_to_bool(tmp);
    idx = 0UL;
    if (omc_attr_ulong(ls, lt, "numlevels", &idx)) mesh->lod_declared_levels = (OMC_U16)idx;

    lm = lt;
    while ((lm = omc_find_open(lm, lse, "lodmanual")) != (const char *)0) {
        lmt = omc_tag_end(lm, lse);
        if (!lmt) break;
        if (mesh->lod_level_count >= (OMC_U16)OMC_MAX_LOD_LEVELS) {
            omc__diag_public(mesh, OMC_ERR_CAPACITY, "LOD level capacity exceeded");
            return OMC_ERR_CAPACITY;
        }
        lod_index = mesh->lod_level_count;
        level = &mesh->lod_levels[mesh->lod_level_count++];
        level->manual = 1U;
        if (!omc_attr_fixed(lm, lmt, "value", &level->value)) level->value = 0;
        if (omc_get_attr(lm, lmt, "meshname", tmp, sizeof(tmp))) omc__copy_str_public(level->mesh_name, sizeof(level->mesh_name), tmp);
        level->face_start = mesh->lod_face_count;
        level->face_count = 0UL;
        (void)lod_index;
        lm = lmt + 1;
    }

    lg = lt;
    while ((lg = omc_find_open(lg, lse, "lodgenerated")) != (const char *)0) {
        lgt = omc_tag_end(lg, lse);
        lge = omc_find_close(lg, lse, "lodgenerated");
        if (!lgt || !lge) break;
        if (mesh->lod_level_count >= (OMC_U16)OMC_MAX_LOD_LEVELS) {
            omc__diag_public(mesh, OMC_ERR_CAPACITY, "LOD level capacity exceeded");
            return OMC_ERR_CAPACITY;
        }
        lod_index = mesh->lod_level_count;
        level = &mesh->lod_levels[mesh->lod_level_count++];
        level->manual = 0U;
        if (!omc_attr_fixed(lg, lgt, "value", &level->value)) level->value = 0;
        if (omc_get_attr(lg, lgt, "meshname", tmp, sizeof(tmp))) omc__copy_str_public(level->mesh_name, sizeof(level->mesh_name), tmp);
        level->face_start = mesh->lod_face_count;
        fl = lgt;
        while ((fl = omc_find_open(fl, lge, "lodfacelist")) != (const char *)0) {
            flt = omc_tag_end(fl, lge);
            fle = omc_find_close(fl, lge, "lodfacelist");
            if (!flt || !fle) break;
            idx = 0UL;
            omc_attr_ulong(fl, flt, "submeshindex", &idx);
            submesh_index = (OMC_U16)idx;
            fc = flt;
            while ((fc = omc_find_open(fc, fle, "face")) != (const char *)0) {
                fct = omc_tag_end(fc, fle);
                if (!fct) break;
                if (mesh->lod_face_count >= (OMC_U32)OMC_MAX_LOD_FACES) {
                    omc__diag_public(mesh, OMC_ERR_CAPACITY, "LOD face capacity exceeded");
                    return OMC_ERR_CAPACITY;
                }
                v1 = 0UL; v2 = 0UL; v3 = 0UL;
                omc_attr_ulong(fc, fct, "v1", &v1);
                omc_attr_ulong(fc, fct, "v2", &v2);
                omc_attr_ulong(fc, fct, "v3", &v3);
                face = &mesh->lod_faces[mesh->lod_face_count++];
                face->lod_index = lod_index;
                face->submesh_index = submesh_index;
                face->v1 = (OMC_U32)v1;
                face->v2 = (OMC_U32)v2;
                face->v3 = (OMC_U32)v3;
                fc = fct + 1;
            }
            fl = fle + 1;
        }
        level->face_count = mesh->lod_face_count - level->face_start;
        lg = lge + 1;
    }

    if (mesh->lod_declared_levels == 0U) mesh->lod_declared_levels = mesh->lod_level_count;
    return OMC_OK;
}

static int omc_parse_extremes(OMC_Mesh *mesh, const char *p, const char *end)
{
    const char *es;
    const char *ese;
    const char *se;
    const char *see;
    const char *set;
    const char *pos;
    const char *post;
    unsigned long idx;
    OMC_Extreme *ex;
    OMC_U16 submesh_index;

    es = omc_find_open(p, end, "extremes");
    if (!es) return OMC_OK;
    ese = omc_find_close(es, end, "extremes");
    if (!ese) return OMC_ERR_FORMAT;

    se = es;
    while ((se = omc_find_open(se, ese, "submesh_extremes")) != (const char *)0) {
        set = omc_tag_end(se, ese);
        see = omc_find_close(se, ese, "submesh_extremes");
        if (!set || !see) break;
        idx = 0UL;
        omc_attr_ulong(se, set, "index", &idx);
        submesh_index = (OMC_U16)idx;
        pos = set;
        while ((pos = omc_find_open(pos, see, "position")) != (const char *)0) {
            post = omc_tag_end(pos, see);
            if (!post) break;
            if (mesh->extreme_count >= (OMC_U32)OMC_MAX_EXTREMES) {
                omc__diag_public(mesh, OMC_ERR_CAPACITY, "extreme point capacity exceeded");
                return OMC_ERR_CAPACITY;
            }
            ex = &mesh->extremes[mesh->extreme_count++];
            ex->submesh_index = submesh_index;
            omc_attr_fixed(pos, post, "x", &ex->position.x);
            omc_attr_fixed(pos, post, "y", &ex->position.y);
            omc_attr_fixed(pos, post, "z", &ex->position.z);
            pos = post + 1;
        }
        se = see + 1;
    }
    return OMC_OK;
}

static int omc_parse_poses(OMC_Mesh *mesh, const char *p, const char *end)
{
    const char *ps;
    const char *pse;
    const char *po;
    const char *poe;
    const char *pot;
    const char *off;
    const char *offe;
    char tmp[OMC_XML_ATTR_MAX];
    OMC_Pose *pose;
    OMC_PoseOffset *dst;
    unsigned long idx;
    OMC_U16 pose_idx;

    ps = omc_find_open(p, end, "poses");
    if (!ps) return OMC_OK;
    pse = omc_find_close(ps, end, "poses");
    if (!pse) return OMC_ERR_FORMAT;

    po = ps;
    while ((po = omc_find_open(po, pse, "pose")) != (const char *)0) {
        pot = omc_tag_end(po, pse);
        poe = omc_find_close(po, pse, "pose");
        if (!pot || !poe) break;
        if (mesh->pose_count >= (OMC_U16)OMC_MAX_POSES) {
            omc__diag_public(mesh, OMC_ERR_CAPACITY, "pose capacity exceeded");
            return OMC_ERR_CAPACITY;
        }
        pose_idx = mesh->pose_count;
        pose = &mesh->poses[pose_idx];
        pose->offset_start = mesh->pose_offset_count;
        if (omc_get_attr(po, pot, "name", tmp, sizeof(tmp))) omc__copy_str_public(pose->name, sizeof(pose->name), tmp);
        if (omc_get_attr(po, pot, "target", tmp, sizeof(tmp))) pose->target_is_submesh = (OMC_U8)(strcmp(tmp, "submesh") == 0);
        idx = 0UL;
        omc_attr_ulong(po, pot, "index", &idx);
        pose->target_index = (OMC_U16)idx;

        off = pot;
        while ((off = omc_find_open(off, poe, "poseoffset")) != (const char *)0) {
            offe = omc_tag_end(off, poe);
            if (!offe) break;
            if (mesh->pose_offset_count >= (OMC_U32)OMC_MAX_POSE_OFFSETS) {
                omc__diag_public(mesh, OMC_ERR_CAPACITY, "pose offset capacity exceeded");
                return OMC_ERR_CAPACITY;
            }
            dst = &mesh->pose_offsets[mesh->pose_offset_count++];
            dst->pose_index = pose_idx;
            idx = 0UL;
            omc_attr_ulong(off, offe, "index", &idx);
            dst->vertex_index = (OMC_U32)idx;
            omc_attr_fixed(off, offe, "x", &dst->offset.x);
            omc_attr_fixed(off, offe, "y", &dst->offset.y);
            omc_attr_fixed(off, offe, "z", &dst->offset.z);
            if (omc_attr_fixed(off, offe, "nx", &dst->normal.x)) {
                dst->has_normal = 1U;
                omc_attr_fixed(off, offe, "ny", &dst->normal.y);
                omc_attr_fixed(off, offe, "nz", &dst->normal.z);
            }
            off = offe + 1;
        }
        pose->offset_count = mesh->pose_offset_count - pose->offset_start;
        mesh->pose_count++;
        po = poe + 1;
    }
    return OMC_OK;
}

static int omc_add_morph_vertex(OMC_Mesh *mesh, OMC_U16 morph_key_index, OMC_U32 local_index, const char *pos, const char *post, const char *normal, const char *normalt)
{
    OMC_MorphVertex *mv;
    if (mesh->morph_vertex_count >= (OMC_U32)OMC_MAX_MORPH_VERTICES) {
        omc__diag_public(mesh, OMC_ERR_CAPACITY, "morph vertex capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    mv = &mesh->morph_vertices[mesh->morph_vertex_count++];
    mv->morph_key_index = morph_key_index;
    mv->local_index = local_index;
    omc_attr_fixed(pos, post, "x", &mv->position.x);
    omc_attr_fixed(pos, post, "y", &mv->position.y);
    omc_attr_fixed(pos, post, "z", &mv->position.z);
    if (normal && normalt) {
        mv->has_normal = 1U;
        omc_attr_fixed(normal, normalt, "x", &mv->normal.x);
        omc_attr_fixed(normal, normalt, "y", &mv->normal.y);
        omc_attr_fixed(normal, normalt, "z", &mv->normal.z);
    }
    return OMC_OK;
}

static int omc_parse_morph_keyframe(OMC_Mesh *mesh, OMC_U16 track_idx, const char *kf, const char *kfe, const char *kfc)
{
    const char *pos;
    const char *post;
    const char *nextpos;
    const char *normal;
    const char *normalt;
    OMC_MorphKey *mk;
    OMC_U16 morph_key_index;
    OMC_U32 local_index;
    int r;

    if (mesh->morph_key_count >= (OMC_U32)OMC_MAX_MORPH_KEYS) {
        omc__diag_public(mesh, OMC_ERR_CAPACITY, "morph key capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    morph_key_index = (OMC_U16)mesh->morph_key_count;
    mk = &mesh->morph_keys[mesh->morph_key_count++];
    mk->track_index = track_idx;
    mk->vertex_start = mesh->morph_vertex_count;
    mk->includes_normals = 0U;
    if (!omc_attr_fixed(kf, kfe, "time", &mk->time)) mk->time = 0;

    pos = kfe;
    local_index = 0UL;
    while ((pos = omc_find_open(pos, kfc, "position")) != (const char *)0) {
        post = omc_tag_end(pos, kfc);
        if (!post) break;
        nextpos = omc_find_open(post + 1, kfc, "position");
        normal = omc_find_open(post + 1, nextpos ? nextpos : kfc, "normal");
        normalt = normal ? omc_tag_end(normal, nextpos ? nextpos : kfc) : (const char *)0;
        r = omc_add_morph_vertex(mesh, morph_key_index, local_index, pos, post, normal, normalt);
        if (r != OMC_OK) return r;
        if (normal && normalt) mk->includes_normals = 1U;
        local_index++;
        pos = post + 1;
    }
    mk->vertex_count = mesh->morph_vertex_count - mk->vertex_start;
    return OMC_OK;
}

static int omc_parse_animations(OMC_Mesh *mesh, const char *p, const char *end)
{
    const char *as;
    const char *ase;
    const char *a;
    const char *ae;
    const char *at;
    const char *bi;
    const char *bit;
    const char *tr;
    const char *tre;
    const char *trt;
    const char *kf;
    const char *kfe;
    const char *kfc;
    const char *pr;
    const char *pre;
    char tmp[OMC_XML_ATTR_MAX];
    OMC_Animation *anim;
    OMC_AnimationTrack *track;
    OMC_AnimationKey *key;
    unsigned long idx;
    OMC_U16 anim_idx;
    OMC_U16 track_idx;
    int is_pose;
    int r;

    as = omc_find_open(p, end, "animations");
    if (!as) return OMC_OK;
    ase = omc_find_close(as, end, "animations");
    if (!ase) return OMC_ERR_FORMAT;

    a = as;
    while ((a = omc_find_open(a, ase, "animation")) != (const char *)0) {
        at = omc_tag_end(a, ase);
        ae = omc_find_close(a, ase, "animation");
        if (!at || !ae) break;
        if (mesh->animation_count >= (OMC_U16)OMC_MAX_ANIMATIONS) {
            omc__diag_public(mesh, OMC_ERR_CAPACITY, "animation capacity exceeded");
            return OMC_ERR_CAPACITY;
        }
        anim_idx = mesh->animation_count;
        anim = &mesh->animations[anim_idx];
        anim->track_start = mesh->track_count;
        if (omc_get_attr(a, at, "name", tmp, sizeof(tmp))) omc__copy_str_public(anim->name, sizeof(anim->name), tmp);
        if (!omc_attr_fixed(a, at, "length", &anim->length)) anim->length = 0;

        bi = omc_find_open(at, ae, "baseinfo");
        if (bi) {
            bit = omc_tag_end(bi, ae);
            if (bit) {
                anim->has_baseinfo = 1U;
                if (omc_get_attr(bi, bit, "baseanimationname", tmp, sizeof(tmp))) omc__copy_str_public(anim->base_animation_name, sizeof(anim->base_animation_name), tmp);
                if (!omc_attr_fixed(bi, bit, "basekeyframetime", &anim->base_keyframe_time)) anim->base_keyframe_time = 0;
            }
        }

        tr = at;
        while ((tr = omc_find_open(tr, ae, "track")) != (const char *)0) {
            trt = omc_tag_end(tr, ae);
            tre = omc_find_close(tr, ae, "track");
            if (!trt || !tre) break;
            if (mesh->track_count >= (OMC_U32)OMC_MAX_ANIMATION_TRACKS) {
                omc__diag_public(mesh, OMC_ERR_CAPACITY, "track capacity exceeded");
                return OMC_ERR_CAPACITY;
            }
            track_idx = (OMC_U16)mesh->track_count;
            track = &mesh->tracks[mesh->track_count++];
            track->animation_index = anim_idx;
            track->key_start = mesh->key_count;
            track->morph_key_start = mesh->morph_key_count;
            if (omc_get_attr(tr, trt, "target", tmp, sizeof(tmp))) track->target_is_submesh = (OMC_U8)(strcmp(tmp, "submesh") == 0);
            idx = 0UL;
            omc_attr_ulong(tr, trt, "index", &idx);
            track->target_index = (OMC_U16)idx;
            is_pose = 0;
            if (omc_get_attr(tr, trt, "type", tmp, sizeof(tmp))) is_pose = (strcmp(tmp, "pose") == 0);
            track->is_pose_track = (OMC_U8)is_pose;

            kf = trt;
            while ((kf = omc_find_open(kf, tre, "keyframe")) != (const char *)0) {
                kfe = omc_tag_end(kf, tre);
                if (!kfe) break;
                kfc = omc_find_close(kf, tre, "keyframe");
                if (!kfc) kfc = kfe;
                if (is_pose) {
                    pr = kfe;
                    while ((pr = omc_find_open(pr, kfc, "poseref")) != (const char *)0) {
                        pre = omc_tag_end(pr, kfc);
                        if (!pre) break;
                        if (mesh->key_count >= (OMC_U32)OMC_MAX_ANIMATION_KEYS) {
                            omc__diag_public(mesh, OMC_ERR_CAPACITY, "animation key capacity exceeded");
                            return OMC_ERR_CAPACITY;
                        }
                        key = &mesh->keys[mesh->key_count++];
                        key->track_index = track_idx;
                        omc_attr_fixed(kf, kfe, "time", &key->time);
                        idx = 0UL;
                        omc_attr_ulong(pr, pre, "poseindex", &idx);
                        key->pose_index = (OMC_U16)idx;
                        if (!omc_attr_fixed(pr, pre, "influence", &key->influence)) key->influence = OMC_FIXED_ONE;
                        pr = pre + 1;
                    }
                } else {
                    r = omc_parse_morph_keyframe(mesh, track_idx, kf, kfe, kfc);
                    if (r != OMC_OK) return r;
                }
                kf = kfc + 1;
            }
            track->key_count = mesh->key_count - track->key_start;
            track->morph_key_count = mesh->morph_key_count - track->morph_key_start;
            tr = tre + 1;
        }
        anim->track_count = mesh->track_count - anim->track_start;
        mesh->animation_count++;
        a = ae + 1;
    }
    return OMC_OK;
}

static int omc_parse_top_mesh_bone_assignments(OMC_Mesh *mesh, const char *root, const char *root_end)
{
    const char *after_submeshes;
    const char *submeshes;
    const char *submeshes_end;
    submeshes = omc_find_open(root, root_end, "submeshes");
    after_submeshes = root;
    if (submeshes) {
        submeshes_end = omc_find_close(submeshes, root_end, "submeshes");
        if (submeshes_end) after_submeshes = submeshes_end;
    }
    return omc_parse_bone_assignments_mesh(mesh, after_submeshes, root_end);
}

int omc_load_mesh_xml_from_memory(OMC_Mesh *mesh, const char *xml, unsigned long size)
{
    const char *p;
    const char *end;
    const char *root;
    const char *root_end;
    int r;

    if (!mesh || !xml) return OMC_ERR_NULL;
    omc_mesh_init(mesh);
    omc__copy_str_public(mesh->source_format, sizeof(mesh->source_format), "mesh.xml");

    p = xml;
    end = xml + size;
    root = omc_find_open(p, end, "mesh");
    if (!root) {
        omc__diag_public(mesh, OMC_ERR_FORMAT, "missing mesh root");
        return OMC_ERR_FORMAT;
    }
    root_end = omc_find_close(root, end, "mesh");
    if (!root_end) root_end = end;

    r = omc_parse_shared_geometry(mesh, root, root_end);
    if (r != OMC_OK) return r;
    r = omc_parse_submeshes(mesh, root, root_end);
    if (r != OMC_OK) return r;
    r = omc_parse_skeletonlink(mesh, root, root_end);
    if (r != OMC_OK) return r;
    r = omc_parse_top_mesh_bone_assignments(mesh, root, root_end);
    if (r != OMC_OK) return r;
    r = omc_parse_levelofdetail(mesh, root, root_end);
    if (r != OMC_OK) return r;
    r = omc_parse_submesh_names(mesh, root, root_end);
    if (r != OMC_OK) return r;
    r = omc_parse_poses(mesh, root, root_end);
    if (r != OMC_OK) return r;
    r = omc_parse_animations(mesh, root, root_end);
    if (r != OMC_OK) return r;
    r = omc_parse_extremes(mesh, root, root_end);
    if (r != OMC_OK) return r;

    omc_compute_bounds(mesh);
    return omc_validate_mesh(mesh);
}
