#include "chiqui_amf.h"
#include <string.h>

#define AMF_NONE_INDEX ((size_t)-1)

typedef struct amf_xml_attr_s {
    amf_slice name;
    amf_slice value;
} amf_xml_attr;

typedef struct amf_xml_event_s {
    int type;
    int self_close;
    amf_slice name;
    amf_slice text;
    amf_xml_attr attrs[AMF_MAX_XML_ATTRS];
    size_t attr_count;
    size_t line;
    size_t col;
} amf_xml_event;

typedef struct amf_xml_scan_s {
    const char *src;
    size_t len;
    size_t pos;
    size_t line;
    size_t col;
} amf_xml_scan;

#define X_EOF 0
#define X_START 1
#define X_END 2
#define X_TEXT 3

#define T_UNKNOWN 0
#define T_AMF 1
#define T_OBJECT 2
#define T_MESH 3
#define T_VERTICES 4
#define T_VERTEX 5
#define T_COORDINATES 6
#define T_NORMAL 7
#define T_X 8
#define T_Y 9
#define T_Z 10
#define T_NX 11
#define T_NY 12
#define T_NZ 13
#define T_VOLUME 14
#define T_REGION 15
#define T_TRIANGLE 16
#define T_V1 17
#define T_V2 18
#define T_V3 19
#define T_COLOR 20
#define T_R 21
#define T_G 22
#define T_B 23
#define T_A 24
#define T_MATERIAL 25
#define T_METADATA 26
#define T_TEXTURE 27
#define T_CONSTELLATION 28
#define T_INSTANCE 29
#define T_COMPOSITE 30
#define T_DELTAX 31
#define T_DELTAY 32
#define T_DELTAZ 33
#define T_ROTATEX 34
#define T_ROTATEY 35
#define T_ROTATEZ 36
#define T_DX 37
#define T_DY 38
#define T_DZ 39
#define T_RX 40
#define T_RY 41
#define T_RZ 42

static int amf_is_space_r(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int amf_is_name_start(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':';
}

static int amf_is_name_char(char c)
{
    return amf_is_name_start(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

static amf_slice amf_slice_trim_r(amf_slice s)
{
    while (s.len > 0u && amf_is_space_r(s.ptr[0])) {
        s.ptr++;
        s.len--;
    }
    while (s.len > 0u && amf_is_space_r(s.ptr[s.len - 1u])) {
        s.len--;
    }
    return s;
}

static amf_slice amf_local_name(amf_slice s)
{
    size_t i;
    i = s.len;
    while (i > 0u) {
        i--;
        if (s.ptr[i] == ':') {
            s.ptr = s.ptr + i + 1u;
            s.len = s.len - i - 1u;
            return s;
        }
    }
    return s;
}

static int amf_slice_eq_cs(amf_slice s, const char *lit)
{
    size_t n;
    n = 0u;
    while (lit[n] != '\0') { n++; }
    if (s.len != n) { return 0; }
    return memcmp(s.ptr, lit, n) == 0;
}

static int amf_name_eq(amf_slice name, const char *lit)
{
    return amf_slice_eq_cs(amf_local_name(name), lit);
}

static int amf_xml_has(const amf_xml_scan *sc, const char *lit)
{
    size_t n;
    n = 0u;
    while (lit[n] != '\0') { n++; }
    if (sc->pos + n > sc->len) { return 0; }
    return memcmp(sc->src + sc->pos, lit, n) == 0;
}

static void amf_xml_adv(amf_xml_scan *sc)
{
    if (sc->pos >= sc->len) { return; }
    if (sc->src[sc->pos] == '\n') {
        sc->line++;
        sc->col = 1u;
    } else {
        sc->col++;
    }
    sc->pos++;
}

static void amf_xml_adv_n(amf_xml_scan *sc, size_t n)
{
    size_t i;
    i = 0u;
    while (i < n) {
        amf_xml_adv(sc);
        i++;
    }
}

static void amf_xml_skip_space(amf_xml_scan *sc)
{
    while (sc->pos < sc->len && amf_is_space_r(sc->src[sc->pos])) {
        amf_xml_adv(sc);
    }
}

static int amf_xml_skip_until(amf_xml_scan *sc, const char *end)
{
    size_t n;
    n = 0u;
    while (end[n] != '\0') { n++; }
    while (sc->pos + n <= sc->len) {
        if (memcmp(sc->src + sc->pos, end, n) == 0) {
            amf_xml_adv_n(sc, n);
            return AMF_OK;
        }
        amf_xml_adv(sc);
    }
    return AMF_ERR_BAD_XML;
}

static int amf_xml_parse_name(amf_xml_scan *sc, amf_slice *out)
{
    size_t start;
    if (sc->pos >= sc->len || !amf_is_name_start(sc->src[sc->pos])) {
        return AMF_ERR_BAD_XML;
    }
    start = sc->pos;
    while (sc->pos < sc->len && amf_is_name_char(sc->src[sc->pos])) {
        amf_xml_adv(sc);
    }
    out->ptr = sc->src + start;
    out->len = sc->pos - start;
    return AMF_OK;
}

static int amf_xml_next(amf_xml_scan *sc, amf_xml_event *ev, amf_diag *diag)
{
    int done;
    if (ev != 0) {
        memset(ev, 0, sizeof(*ev));
    }
    done = 0;
    while (!done) {
        if (sc->pos >= sc->len) {
            ev->type = X_EOF;
            return AMF_OK;
        }
        if (sc->src[sc->pos] != '<') {
            size_t start;
            start = sc->pos;
            ev->line = sc->line;
            ev->col = sc->col;
            while (sc->pos < sc->len && sc->src[sc->pos] != '<') {
                amf_xml_adv(sc);
            }
            ev->type = X_TEXT;
            ev->text.ptr = sc->src + start;
            ev->text.len = sc->pos - start;
            return AMF_OK;
        }
        ev->line = sc->line;
        ev->col = sc->col;
        if (amf_xml_has(sc, "<!--")) {
            amf_xml_adv_n(sc, 4u);
            if (amf_xml_skip_until(sc, "-->") != AMF_OK) {
                amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "unterminated XML comment");
                return AMF_ERR_BAD_XML;
            }
            continue;
        }
        if (amf_xml_has(sc, "<![CDATA[")) {
            size_t start;
            amf_xml_adv_n(sc, 9u);
            start = sc->pos;
            while (sc->pos + 3u <= sc->len) {
                if (memcmp(sc->src + sc->pos, "]]>", 3u) == 0) {
                    ev->type = X_TEXT;
                    ev->text.ptr = sc->src + start;
                    ev->text.len = sc->pos - start;
                    amf_xml_adv_n(sc, 3u);
                    return AMF_OK;
                }
                amf_xml_adv(sc);
            }
            amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "unterminated CDATA");
            return AMF_ERR_BAD_XML;
        }
        if (amf_xml_has(sc, "<?")) {
            amf_xml_adv_n(sc, 2u);
            if (amf_xml_skip_until(sc, "?>") != AMF_OK) {
                amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "unterminated XML processing instruction");
                return AMF_ERR_BAD_XML;
            }
            continue;
        }
        if (amf_xml_has(sc, "<!")) {
            amf_xml_adv_n(sc, 2u);
            if (amf_xml_skip_until(sc, ">") != AMF_OK) {
                amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "unterminated XML declaration");
                return AMF_ERR_BAD_XML;
            }
            continue;
        }
        done = 1;
    }

    amf_xml_adv(sc); /* < */
    if (sc->pos < sc->len && sc->src[sc->pos] == '/') {
        int r;
        amf_xml_adv(sc);
        ev->type = X_END;
        r = amf_xml_parse_name(sc, &ev->name);
        if (r != AMF_OK) {
            amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "bad XML end tag name");
            return r;
        }
        amf_xml_skip_space(sc);
        if (sc->pos >= sc->len || sc->src[sc->pos] != '>') {
            amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "expected > in XML end tag");
            return AMF_ERR_BAD_XML;
        }
        amf_xml_adv(sc);
        return AMF_OK;
    }
    {
        int r;
        ev->type = X_START;
        r = amf_xml_parse_name(sc, &ev->name);
        if (r != AMF_OK) {
            amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "bad XML start tag name");
            return r;
        }
        while (sc->pos < sc->len) {
            amf_xml_skip_space(sc);
            if (sc->pos >= sc->len) {
                amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "unterminated XML start tag");
                return AMF_ERR_BAD_XML;
            }
            if (sc->src[sc->pos] == '>') {
                amf_xml_adv(sc);
                return AMF_OK;
            }
            if (sc->src[sc->pos] == '/') {
                amf_xml_adv(sc);
                if (sc->pos >= sc->len || sc->src[sc->pos] != '>') {
                    amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "expected /> in XML empty tag");
                    return AMF_ERR_BAD_XML;
                }
                amf_xml_adv(sc);
                ev->self_close = 1;
                return AMF_OK;
            }
            if (ev->attr_count >= AMF_MAX_XML_ATTRS) {
                amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "too many XML attributes on tag");
                return AMF_ERR_BAD_XML;
            }
            r = amf_xml_parse_name(sc, &ev->attrs[ev->attr_count].name);
            if (r != AMF_OK) {
                amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "bad XML attribute name");
                return r;
            }
            amf_xml_skip_space(sc);
            if (sc->pos >= sc->len || sc->src[sc->pos] != '=') {
                amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "expected = in XML attribute");
                return AMF_ERR_BAD_XML;
            }
            amf_xml_adv(sc);
            amf_xml_skip_space(sc);
            if (sc->pos >= sc->len || (sc->src[sc->pos] != '"' && sc->src[sc->pos] != '\'')) {
                amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "expected quoted XML attribute value");
                return AMF_ERR_BAD_XML;
            }
            {
                char quote;
                size_t start;
                quote = sc->src[sc->pos];
                amf_xml_adv(sc);
                start = sc->pos;
                while (sc->pos < sc->len && sc->src[sc->pos] != quote) {
                    amf_xml_adv(sc);
                }
                if (sc->pos >= sc->len) {
                    amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "unterminated XML attribute value");
                    return AMF_ERR_BAD_XML;
                }
                ev->attrs[ev->attr_count].value.ptr = sc->src + start;
                ev->attrs[ev->attr_count].value.len = sc->pos - start;
                amf_xml_adv(sc);
                ev->attr_count++;
            }
        }
    }
    amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "unterminated XML tag");
    return AMF_ERR_BAD_XML;
}

static int amf_tag_id(amf_slice name)
{
    if (amf_name_eq(name, "amf")) return T_AMF;
    if (amf_name_eq(name, "object")) return T_OBJECT;
    if (amf_name_eq(name, "mesh")) return T_MESH;
    if (amf_name_eq(name, "vertices")) return T_VERTICES;
    if (amf_name_eq(name, "vertex")) return T_VERTEX;
    if (amf_name_eq(name, "coordinates")) return T_COORDINATES;
    if (amf_name_eq(name, "normal")) return T_NORMAL;
    if (amf_name_eq(name, "x")) return T_X;
    if (amf_name_eq(name, "y")) return T_Y;
    if (amf_name_eq(name, "z")) return T_Z;
    if (amf_name_eq(name, "nx")) return T_NX;
    if (amf_name_eq(name, "ny")) return T_NY;
    if (amf_name_eq(name, "nz")) return T_NZ;
    if (amf_name_eq(name, "volume")) return T_VOLUME;
    if (amf_name_eq(name, "region")) return T_REGION;
    if (amf_name_eq(name, "triangle")) return T_TRIANGLE;
    if (amf_name_eq(name, "v1")) return T_V1;
    if (amf_name_eq(name, "v2")) return T_V2;
    if (amf_name_eq(name, "v3")) return T_V3;
    if (amf_name_eq(name, "color")) return T_COLOR;
    if (amf_name_eq(name, "r")) return T_R;
    if (amf_name_eq(name, "g")) return T_G;
    if (amf_name_eq(name, "b")) return T_B;
    if (amf_name_eq(name, "a")) return T_A;
    if (amf_name_eq(name, "material")) return T_MATERIAL;
    if (amf_name_eq(name, "metadata")) return T_METADATA;
    if (amf_name_eq(name, "texture")) return T_TEXTURE;
    if (amf_name_eq(name, "constellation")) return T_CONSTELLATION;
    if (amf_name_eq(name, "instance")) return T_INSTANCE;
    if (amf_name_eq(name, "composite")) return T_COMPOSITE;
    if (amf_name_eq(name, "deltax")) return T_DELTAX;
    if (amf_name_eq(name, "deltay")) return T_DELTAY;
    if (amf_name_eq(name, "deltaz")) return T_DELTAZ;
    if (amf_name_eq(name, "rotatex")) return T_ROTATEX;
    if (amf_name_eq(name, "rotatey")) return T_ROTATEY;
    if (amf_name_eq(name, "rotatez")) return T_ROTATEZ;
    if (amf_name_eq(name, "dx")) return T_DX;
    if (amf_name_eq(name, "dy")) return T_DY;
    if (amf_name_eq(name, "dz")) return T_DZ;
    if (amf_name_eq(name, "rx")) return T_RX;
    if (amf_name_eq(name, "ry")) return T_RY;
    if (amf_name_eq(name, "rz")) return T_RZ;
    return T_UNKNOWN;
}

static int amf_attr_get(const amf_xml_event *ev, const char *name, amf_slice *out)
{
    size_t i;
    i = 0u;
    while (i < ev->attr_count) {
        if (amf_name_eq(ev->attrs[i].name, name)) {
            *out = ev->attrs[i].value;
            return 1;
        }
        i++;
    }
    return 0;
}

static int amf_attr_get_any3(const amf_xml_event *ev, const char *a, const char *b, const char *c, amf_slice *out)
{
    if (a != 0 && amf_attr_get(ev, a, out)) return 1;
    if (b != 0 && amf_attr_get(ev, b, out)) return 1;
    if (c != 0 && amf_attr_get(ev, c, out)) return 1;
    return 0;
}

static int amf_parse_attr_u32(const amf_xml_event *ev, const char *a, const char *b, const char *c, amf_u32 *out)
{
    amf_slice s;
    if (amf_attr_get_any3(ev, a, b, c, &s)) {
        return amf_u32_parse(s, out);
    }
    return AMF_OK;
}

static int amf_parse_attr_fx(const amf_xml_event *ev, const char *a, const char *b, const char *c, amf_fx *out)
{
    amf_slice s;
    if (amf_attr_get_any3(ev, a, b, c, &s)) {
        return amf_fx_parse(s, out);
    }
    return AMF_OK;
}

static int amf_alloc_document(amf_document *doc, amf_arena *arena, const amf_counts *counts)
{
    amf_document_zero(doc);
    doc->object_cap = counts->objects;
    doc->material_cap = counts->materials;
    doc->texture_cap = counts->textures;
    doc->constellation_cap = counts->constellations;
    doc->instance_cap = counts->instances;
    doc->vertex_cap = counts->vertices;
    doc->volume_cap = counts->volumes;
    doc->triangle_cap = counts->triangles;
    doc->metadata_cap = counts->metadata;
    doc->composite_cap = counts->composites;
    if (doc->object_cap != 0u) {
        doc->objects = (amf_object *)amf_arena_calloc(arena, doc->object_cap, sizeof(amf_object), sizeof(void *));
        if (doc->objects == 0) return AMF_ERR_ARENA_FULL;
    }
    if (doc->material_cap != 0u) {
        doc->materials = (amf_material *)amf_arena_calloc(arena, doc->material_cap, sizeof(amf_material), sizeof(void *));
        if (doc->materials == 0) return AMF_ERR_ARENA_FULL;
    }
    if (doc->texture_cap != 0u) {
        doc->textures = (amf_texture *)amf_arena_calloc(arena, doc->texture_cap, sizeof(amf_texture), sizeof(void *));
        if (doc->textures == 0) return AMF_ERR_ARENA_FULL;
    }
    if (doc->constellation_cap != 0u) {
        doc->constellations = (amf_constellation *)amf_arena_calloc(arena, doc->constellation_cap, sizeof(amf_constellation), sizeof(void *));
        if (doc->constellations == 0) return AMF_ERR_ARENA_FULL;
    }
    if (doc->instance_cap != 0u) {
        doc->instances = (amf_instance *)amf_arena_calloc(arena, doc->instance_cap, sizeof(amf_instance), sizeof(void *));
        if (doc->instances == 0) return AMF_ERR_ARENA_FULL;
    }
    if (doc->vertex_cap != 0u) {
        doc->vertices = (amf_vertex *)amf_arena_calloc(arena, doc->vertex_cap, sizeof(amf_vertex), sizeof(void *));
        if (doc->vertices == 0) return AMF_ERR_ARENA_FULL;
    }
    if (doc->volume_cap != 0u) {
        doc->volumes = (amf_volume *)amf_arena_calloc(arena, doc->volume_cap, sizeof(amf_volume), sizeof(void *));
        if (doc->volumes == 0) return AMF_ERR_ARENA_FULL;
    }
    if (doc->triangle_cap != 0u) {
        doc->triangles = (amf_triangle *)amf_arena_calloc(arena, doc->triangle_cap, sizeof(amf_triangle), sizeof(void *));
        if (doc->triangles == 0) return AMF_ERR_ARENA_FULL;
    }
    if (doc->metadata_cap != 0u) {
        doc->metadata = (amf_metadata *)amf_arena_calloc(arena, doc->metadata_cap, sizeof(amf_metadata), sizeof(void *));
        if (doc->metadata == 0) return AMF_ERR_ARENA_FULL;
    }
    if (doc->composite_cap != 0u) {
        doc->composites = (amf_composite *)amf_arena_calloc(arena, doc->composite_cap, sizeof(amf_composite), sizeof(void *));
        if (doc->composites == 0) return AMF_ERR_ARENA_FULL;
    }
    return AMF_OK;
}

int amf_count_xml(const char *xml, size_t len, amf_counts *counts, amf_diag *diag)
{
    amf_xml_scan sc;
    amf_xml_event ev;
    int r;
    int id;
    if (xml == 0 || counts == 0) {
        amf_diag_set(diag, AMF_ERR_BAD_ARG, 1u, 1u, "bad argument");
        return AMF_ERR_BAD_ARG;
    }
    memset(counts, 0, sizeof(*counts));
    amf_diag_clear(diag);
    sc.src = xml;
    sc.len = len;
    sc.pos = 0u;
    sc.line = 1u;
    sc.col = 1u;
    for (;;) {
        r = amf_xml_next(&sc, &ev, diag);
        if (r != AMF_OK) return r;
        if (ev.type == X_EOF) break;
        if (ev.type != X_START) continue;
        id = amf_tag_id(ev.name);
        switch (id) {
        case T_OBJECT: counts->objects++; break;
        case T_MATERIAL: counts->materials++; break;
        case T_TEXTURE: counts->textures++; break;
        case T_CONSTELLATION: counts->constellations++; break;
        case T_INSTANCE: counts->instances++; break;
        case T_VERTEX: counts->vertices++; break;
        case T_VOLUME: case T_REGION: counts->volumes++; break;
        case T_TRIANGLE: counts->triangles++; break;
        case T_METADATA: counts->metadata++; break;
        case T_COMPOSITE: counts->composites++; break;
        default: break;
        }
    }
    return AMF_OK;
}

typedef struct amf_parse_state_s {
    amf_document *doc;
    amf_arena *arena;
    size_t current_object;
    size_t current_material;
    size_t current_texture;
    size_t current_constellation;
    size_t current_instance;
    size_t current_vertex;
    size_t current_volume;
    size_t current_triangle;
    size_t current_metadata;
    size_t current_composite;
    amf_color *current_color;
    int stack[AMF_MAX_XML_DEPTH];
    size_t depth;
} amf_parse_state;

static int amf_stack_has(const amf_parse_state *ps, int tag)
{
    size_t i;
    i = ps->depth;
    while (i > 0u) {
        i--;
        if (ps->stack[i] == tag) return 1;
    }
    return 0;
}

static int amf_ps_push(amf_parse_state *ps, int tag, amf_diag *diag, const amf_xml_event *ev)
{
    if (ps->depth >= AMF_MAX_XML_DEPTH) {
        amf_diag_set(diag, AMF_ERR_BAD_XML, ev->line, ev->col, "XML nesting too deep");
        return AMF_ERR_BAD_XML;
    }
    ps->stack[ps->depth] = tag;
    ps->depth++;
    return AMF_OK;
}

static void amf_ps_pop(amf_parse_state *ps)
{
    if (ps->depth > 0u) {
        ps->depth--;
    }
}

static int amf_metadata_owner(const amf_parse_state *ps, amf_u32 *kind, size_t *index)
{
    if (ps->current_triangle != AMF_NONE_INDEX) { *kind = AMF_OWNER_TRIANGLE; *index = ps->current_triangle; return 1; }
    if (ps->current_vertex != AMF_NONE_INDEX) { *kind = AMF_OWNER_VERTEX; *index = ps->current_vertex; return 1; }
    if (ps->current_volume != AMF_NONE_INDEX) { *kind = AMF_OWNER_VOLUME; *index = ps->current_volume; return 1; }
    if (ps->current_object != AMF_NONE_INDEX) { *kind = AMF_OWNER_OBJECT; *index = ps->current_object; return 1; }
    if (ps->current_material != AMF_NONE_INDEX) { *kind = AMF_OWNER_MATERIAL; *index = ps->current_material; return 1; }
    if (ps->current_texture != AMF_NONE_INDEX) { *kind = AMF_OWNER_TEXTURE; *index = ps->current_texture; return 1; }
    if (ps->current_constellation != AMF_NONE_INDEX) { *kind = AMF_OWNER_CONSTELLATION; *index = ps->current_constellation; return 1; }
    *kind = AMF_OWNER_DOCUMENT;
    *index = 0u;
    return 1;
}

static int amf_start_metadata(amf_parse_state *ps, const amf_xml_event *ev, amf_diag *diag)
{
    amf_metadata *m;
    amf_slice typ;
    amf_u32 kind;
    size_t idx;
    if (ps->doc->metadata_count >= ps->doc->metadata_cap) {
        amf_diag_set(diag, AMF_ERR_ARENA_FULL, ev->line, ev->col, "metadata array full");
        return AMF_ERR_ARENA_FULL;
    }
    m = ps->doc->metadata + ps->doc->metadata_count;
    ps->current_metadata = ps->doc->metadata_count;
    ps->doc->metadata_count++;
    amf_metadata_owner(ps, &kind, &idx);
    m->owner_kind = kind;
    m->owner_index = idx;
    if (amf_attr_get_any3(ev, "type", "name", 0, &typ)) {
        m->type = amf_arena_copy_slice(ps->arena, typ.ptr, typ.len);
        if (typ.len != 0u && m->type.len == 0u) {
            amf_diag_set(diag, AMF_ERR_ARENA_FULL, ev->line, ev->col, "arena full copying metadata type");
            return AMF_ERR_ARENA_FULL;
        }
    }
    if (kind == AMF_OWNER_DOCUMENT) {
        if (ps->doc->document_metadata_count == 0u) ps->doc->document_metadata_first = ps->current_metadata;
        ps->doc->document_metadata_count++;
    } else if (kind == AMF_OWNER_OBJECT) {
        amf_object *o;
        o = ps->doc->objects + idx;
        if (o->metadata_count == 0u) o->metadata_first = ps->current_metadata;
        o->metadata_count++;
    } else if (kind == AMF_OWNER_VOLUME) {
        amf_volume *v;
        v = ps->doc->volumes + idx;
        if (v->metadata_count == 0u) v->metadata_first = ps->current_metadata;
        v->metadata_count++;
    } else if (kind == AMF_OWNER_MATERIAL) {
        amf_material *mt;
        mt = ps->doc->materials + idx;
        if (mt->metadata_count == 0u) mt->metadata_first = ps->current_metadata;
        mt->metadata_count++;
    } else if (kind == AMF_OWNER_TEXTURE) {
        amf_texture *t;
        t = ps->doc->textures + idx;
        if (t->metadata_count == 0u) t->metadata_first = ps->current_metadata;
        t->metadata_count++;
    } else if (kind == AMF_OWNER_CONSTELLATION) {
        amf_constellation *c;
        c = ps->doc->constellations + idx;
        if (c->metadata_count == 0u) c->metadata_first = ps->current_metadata;
        c->metadata_count++;
    }
    return AMF_OK;
}

static int amf_start_element(amf_parse_state *ps, const amf_xml_event *ev, amf_diag *diag)
{
    int tag;
    int r;
    amf_slice s;
    tag = amf_tag_id(ev->name);
    r = AMF_OK;
    switch (tag) {
    case T_AMF:
        ps->doc->unit = AMF_UNIT_MM;
        if (amf_attr_get_any3(ev, "unit", "units", 0, &s)) {
            ps->doc->unit_text = amf_arena_copy_slice(ps->arena, s.ptr, s.len);
            if (s.len != 0u && ps->doc->unit_text.len == 0u) return AMF_ERR_ARENA_FULL;
            ps->doc->unit = amf_unit_from_slice(s);
        }
        if (amf_attr_get_any3(ev, "version", 0, 0, &s)) {
            ps->doc->version_text = amf_arena_copy_slice(ps->arena, s.ptr, s.len);
            if (s.len != 0u && ps->doc->version_text.len == 0u) return AMF_ERR_ARENA_FULL;
        }
        break;
    case T_OBJECT:
        if (ps->doc->object_count >= ps->doc->object_cap) return AMF_ERR_ARENA_FULL;
        ps->current_object = ps->doc->object_count;
        ps->doc->object_count++;
        {
            amf_object *o;
            o = ps->doc->objects + ps->current_object;
            o->vertex_first = ps->doc->vertex_count;
            o->volume_first = ps->doc->volume_count;
            o->triangle_first = ps->doc->triangle_count;
            r = amf_parse_attr_u32(ev, "id", 0, 0, &o->id);
            if (r != AMF_OK) return r;
            r = amf_parse_attr_u32(ev, "materialid", "material-id", "material", &o->material_id);
            if (r != AMF_OK) return r;
            if (o->material_id != 0u) o->flags |= AMF_FLAG_HAS_MATERIAL;
        }
        break;
    case T_VERTEX:
        if (ps->doc->vertex_count >= ps->doc->vertex_cap) return AMF_ERR_ARENA_FULL;
        ps->current_vertex = ps->doc->vertex_count;
        ps->doc->vertex_count++;
        break;
    case T_VOLUME:
    case T_REGION:
        if (ps->doc->volume_count >= ps->doc->volume_cap) return AMF_ERR_ARENA_FULL;
        ps->current_volume = ps->doc->volume_count;
        ps->doc->volume_count++;
        {
            amf_volume *v;
            v = ps->doc->volumes + ps->current_volume;
            v->object_index = ps->current_object;
            v->triangle_first = ps->doc->triangle_count;
            r = amf_parse_attr_u32(ev, "materialid", "material-id", "material", &v->material_id);
            if (r != AMF_OK) return r;
            if (v->material_id != 0u) v->flags |= AMF_FLAG_HAS_MATERIAL;
        }
        break;
    case T_TRIANGLE:
        if (ps->doc->triangle_count >= ps->doc->triangle_cap) return AMF_ERR_ARENA_FULL;
        ps->current_triangle = ps->doc->triangle_count;
        ps->doc->triangle_count++;
        {
            amf_triangle *t;
            t = ps->doc->triangles + ps->current_triangle;
            t->object_index = ps->current_object;
            t->volume_index = ps->current_volume;
        }
        break;
    case T_MATERIAL:
        if (ps->doc->material_count >= ps->doc->material_cap) return AMF_ERR_ARENA_FULL;
        ps->current_material = ps->doc->material_count;
        ps->doc->material_count++;
        {
            amf_material *m;
            m = ps->doc->materials + ps->current_material;
            m->composite_first = ps->doc->composite_count;
            r = amf_parse_attr_u32(ev, "id", 0, 0, &m->id);
            if (r != AMF_OK) return r;
        }
        break;
    case T_COMPOSITE:
        if (ps->doc->composite_count >= ps->doc->composite_cap) return AMF_ERR_ARENA_FULL;
        ps->current_composite = ps->doc->composite_count;
        ps->doc->composite_count++;
        {
            amf_composite *c;
            amf_material *m;
            c = ps->doc->composites + ps->current_composite;
            c->material_index = ps->current_material;
            r = amf_parse_attr_u32(ev, "materialid", "material-id", "material", &c->material_id);
            if (r != AMF_OK) return r;
            if (ps->current_material != AMF_NONE_INDEX) {
                m = ps->doc->materials + ps->current_material;
                if (m->composite_count == 0u) m->composite_first = ps->current_composite;
                m->composite_count++;
            }
        }
        break;
    case T_TEXTURE:
        if (ps->doc->texture_count >= ps->doc->texture_cap) return AMF_ERR_ARENA_FULL;
        ps->current_texture = ps->doc->texture_count;
        ps->doc->texture_count++;
        {
            amf_texture *t;
            t = ps->doc->textures + ps->current_texture;
            r = amf_parse_attr_u32(ev, "id", 0, 0, &t->id); if (r != AMF_OK) return r;
            r = amf_parse_attr_u32(ev, "width", 0, 0, &t->width); if (r != AMF_OK) return r;
            r = amf_parse_attr_u32(ev, "height", 0, 0, &t->height); if (r != AMF_OK) return r;
            r = amf_parse_attr_u32(ev, "depth", 0, 0, &t->depth); if (r != AMF_OK) return r;
            if (amf_attr_get_any3(ev, "type", 0, 0, &s)) {
                t->type = amf_arena_copy_slice(ps->arena, s.ptr, s.len);
                if (s.len != 0u && t->type.len == 0u) return AMF_ERR_ARENA_FULL;
            }
        }
        break;
    case T_CONSTELLATION:
        if (ps->doc->constellation_count >= ps->doc->constellation_cap) return AMF_ERR_ARENA_FULL;
        ps->current_constellation = ps->doc->constellation_count;
        ps->doc->constellation_count++;
        {
            amf_constellation *c;
            c = ps->doc->constellations + ps->current_constellation;
            c->instance_first = ps->doc->instance_count;
            r = amf_parse_attr_u32(ev, "id", 0, 0, &c->id);
            if (r != AMF_OK) return r;
        }
        break;
    case T_INSTANCE:
        if (ps->doc->instance_count >= ps->doc->instance_cap) return AMF_ERR_ARENA_FULL;
        ps->current_instance = ps->doc->instance_count;
        ps->doc->instance_count++;
        {
            amf_instance *i;
            i = ps->doc->instances + ps->current_instance;
            i->constellation_index = ps->current_constellation;
            r = amf_parse_attr_u32(ev, "objectid", "object-id", "object", &i->object_id); if (r != AMF_OK) return r;
            r = amf_parse_attr_u32(ev, "constellationid", "constellation-id", "constellation", &i->constellation_id); if (r != AMF_OK) return r;
            r = amf_parse_attr_fx(ev, "deltax", "dx", "x", &i->delta.x); if (r != AMF_OK) return r;
            r = amf_parse_attr_fx(ev, "deltay", "dy", "y", &i->delta.y); if (r != AMF_OK) return r;
            r = amf_parse_attr_fx(ev, "deltaz", "dz", "z", &i->delta.z); if (r != AMF_OK) return r;
            r = amf_parse_attr_fx(ev, "rotatex", "rx", 0, &i->rotate.x); if (r != AMF_OK) return r;
            r = amf_parse_attr_fx(ev, "rotatey", "ry", 0, &i->rotate.y); if (r != AMF_OK) return r;
            r = amf_parse_attr_fx(ev, "rotatez", "rz", 0, &i->rotate.z); if (r != AMF_OK) return r;
        }
        if (ps->current_constellation != AMF_NONE_INDEX) {
            amf_constellation *c;
            c = ps->doc->constellations + ps->current_constellation;
            c->instance_count++;
        }
        break;
    case T_METADATA:
        r = amf_start_metadata(ps, ev, diag);
        if (r != AMF_OK) return r;
        break;
    case T_COLOR:
        if (ps->current_triangle != AMF_NONE_INDEX) {
            ps->current_color = &ps->doc->triangles[ps->current_triangle].color;
            ps->doc->triangles[ps->current_triangle].flags |= AMF_FLAG_HAS_COLOR;
        } else if (ps->current_vertex != AMF_NONE_INDEX) {
            ps->current_color = &ps->doc->vertices[ps->current_vertex].color;
            ps->doc->vertices[ps->current_vertex].flags |= AMF_FLAG_HAS_COLOR;
        } else if (ps->current_volume != AMF_NONE_INDEX) {
            ps->current_color = &ps->doc->volumes[ps->current_volume].color;
            ps->doc->volumes[ps->current_volume].flags |= AMF_FLAG_HAS_COLOR;
        } else if (ps->current_object != AMF_NONE_INDEX) {
            ps->current_color = &ps->doc->objects[ps->current_object].color;
            ps->doc->objects[ps->current_object].flags |= AMF_FLAG_HAS_COLOR;
        } else if (ps->current_material != AMF_NONE_INDEX) {
            ps->current_color = &ps->doc->materials[ps->current_material].color;
            ps->doc->materials[ps->current_material].flags |= AMF_FLAG_HAS_COLOR;
        } else {
            ps->current_color = 0;
        }
        if (ps->current_color != 0) {
            ps->current_color->flags |= AMF_FLAG_PRESENT;
        }
        break;
    default:
        break;
    }
    if (r != AMF_OK) {
        amf_diag_set(diag, r, ev->line, ev->col, "AMF start element parse failed");
        return r;
    }
    return amf_ps_push(ps, tag, diag, ev);
}

static void amf_finish_object(amf_parse_state *ps)
{
    if (ps->current_object != AMF_NONE_INDEX) {
        amf_object *o;
        size_t i;
        o = ps->doc->objects + ps->current_object;
        o->vertex_count = ps->doc->vertex_count - o->vertex_first;
        o->volume_count = ps->doc->volume_count - o->volume_first;
        o->triangle_count = ps->doc->triangle_count - o->triangle_first;
        if (o->vertex_count > 0u) {
            amf_vertex *v0;
            v0 = ps->doc->vertices + o->vertex_first;
            o->bbox_min = v0->p;
            o->bbox_max = v0->p;
            i = 1u;
            while (i < o->vertex_count) {
                amf_vertex *v;
                v = ps->doc->vertices + o->vertex_first + i;
                if (v->p.x < o->bbox_min.x) o->bbox_min.x = v->p.x;
                if (v->p.y < o->bbox_min.y) o->bbox_min.y = v->p.y;
                if (v->p.z < o->bbox_min.z) o->bbox_min.z = v->p.z;
                if (v->p.x > o->bbox_max.x) o->bbox_max.x = v->p.x;
                if (v->p.y > o->bbox_max.y) o->bbox_max.y = v->p.y;
                if (v->p.z > o->bbox_max.z) o->bbox_max.z = v->p.z;
                i++;
            }
        }
    }
}

static int amf_end_element(amf_parse_state *ps, const amf_xml_event *ev, amf_diag *diag)
{
    int tag;
    (void)diag;
    tag = amf_tag_id(ev->name);
    switch (tag) {
    case T_OBJECT:
        amf_finish_object(ps);
        ps->current_object = AMF_NONE_INDEX;
        break;
    case T_VERTEX:
        ps->current_vertex = AMF_NONE_INDEX;
        break;
    case T_VOLUME:
    case T_REGION:
        if (ps->current_volume != AMF_NONE_INDEX) {
            amf_volume *v;
            v = ps->doc->volumes + ps->current_volume;
            v->triangle_count = ps->doc->triangle_count - v->triangle_first;
        }
        ps->current_volume = AMF_NONE_INDEX;
        break;
    case T_TRIANGLE:
        ps->current_triangle = AMF_NONE_INDEX;
        break;
    case T_MATERIAL:
        ps->current_material = AMF_NONE_INDEX;
        break;
    case T_TEXTURE:
        ps->current_texture = AMF_NONE_INDEX;
        break;
    case T_CONSTELLATION:
        ps->current_constellation = AMF_NONE_INDEX;
        break;
    case T_INSTANCE:
        ps->current_instance = AMF_NONE_INDEX;
        break;
    case T_METADATA:
        ps->current_metadata = AMF_NONE_INDEX;
        break;
    case T_COMPOSITE:
        ps->current_composite = AMF_NONE_INDEX;
        break;
    case T_COLOR:
        ps->current_color = 0;
        break;
    default:
        break;
    }
    amf_ps_pop(ps);
    return AMF_OK;
}

static int amf_set_numeric_text(amf_parse_state *ps, int tag, amf_slice text, const amf_xml_event *ev, amf_diag *diag)
{
    int r;
    amf_fx fxv;
    amf_u32 u32v;
    r = AMF_OK;
    switch (tag) {
    case T_X:
    case T_Y:
    case T_Z:
        r = amf_fx_parse(text, &fxv);
        if (r != AMF_OK) break;
        if (ps->current_vertex != AMF_NONE_INDEX && amf_stack_has(ps, T_COORDINATES)) {
            amf_vertex *v;
            v = ps->doc->vertices + ps->current_vertex;
            if (tag == T_X) v->p.x = fxv;
            if (tag == T_Y) v->p.y = fxv;
            if (tag == T_Z) v->p.z = fxv;
        } else if (ps->current_vertex != AMF_NONE_INDEX && amf_stack_has(ps, T_NORMAL)) {
            amf_vertex *v2;
            v2 = ps->doc->vertices + ps->current_vertex;
            if (tag == T_X) v2->n.x = fxv;
            if (tag == T_Y) v2->n.y = fxv;
            if (tag == T_Z) v2->n.z = fxv;
            v2->flags |= AMF_FLAG_HAS_NORMAL;
        }
        break;
    case T_NX:
    case T_NY:
    case T_NZ:
        r = amf_fx_parse(text, &fxv);
        if (r != AMF_OK) break;
        if (ps->current_vertex != AMF_NONE_INDEX) {
            amf_vertex *vn;
            vn = ps->doc->vertices + ps->current_vertex;
            if (tag == T_NX) vn->n.x = fxv;
            if (tag == T_NY) vn->n.y = fxv;
            if (tag == T_NZ) vn->n.z = fxv;
            vn->flags |= AMF_FLAG_HAS_NORMAL;
        }
        break;
    case T_R:
    case T_G:
    case T_B:
    case T_A:
        r = amf_fx_parse(text, &fxv);
        if (r != AMF_OK) break;
        if (ps->current_color != 0) {
            if (tag == T_R) ps->current_color->r = fxv;
            if (tag == T_G) ps->current_color->g = fxv;
            if (tag == T_B) ps->current_color->b = fxv;
            if (tag == T_A) ps->current_color->a = fxv;
        }
        break;
    case T_V1:
    case T_V2:
    case T_V3:
        r = amf_u32_parse(text, &u32v);
        if (r != AMF_OK) break;
        if (ps->current_triangle != AMF_NONE_INDEX) {
            amf_triangle *tr;
            tr = ps->doc->triangles + ps->current_triangle;
            if (tag == T_V1) tr->v1 = u32v;
            if (tag == T_V2) tr->v2 = u32v;
            if (tag == T_V3) tr->v3 = u32v;
        }
        break;
    case T_DELTAX:
    case T_DX:
    case T_DELTAY:
    case T_DY:
    case T_DELTAZ:
    case T_DZ:
    case T_ROTATEX:
    case T_RX:
    case T_ROTATEY:
    case T_RY:
    case T_ROTATEZ:
    case T_RZ:
        r = amf_fx_parse(text, &fxv);
        if (r != AMF_OK) break;
        if (ps->current_instance != AMF_NONE_INDEX) {
            amf_instance *ins;
            ins = ps->doc->instances + ps->current_instance;
            if (tag == T_DELTAX || tag == T_DX) ins->delta.x = fxv;
            if (tag == T_DELTAY || tag == T_DY) ins->delta.y = fxv;
            if (tag == T_DELTAZ || tag == T_DZ) ins->delta.z = fxv;
            if (tag == T_ROTATEX || tag == T_RX) ins->rotate.x = fxv;
            if (tag == T_ROTATEY || tag == T_RY) ins->rotate.y = fxv;
            if (tag == T_ROTATEZ || tag == T_RZ) ins->rotate.z = fxv;
        }
        break;
    default:
        break;
    }
    if (r != AMF_OK) {
        amf_diag_set(diag, r, ev->line, ev->col, "bad numeric AMF text");
        return r;
    }
    return AMF_OK;
}

static int amf_text_element(amf_parse_state *ps, const amf_xml_event *ev, amf_diag *diag)
{
    amf_slice text;
    int tag;
    text = amf_slice_trim_r(ev->text);
    if (text.len == 0u) return AMF_OK;
    if (ps->depth == 0u) return AMF_OK;
    tag = ps->stack[ps->depth - 1u];
    if (tag == T_METADATA) {
        if (ps->current_metadata != AMF_NONE_INDEX) {
            amf_metadata *m;
            m = ps->doc->metadata + ps->current_metadata;
            m->text = amf_arena_copy_slice(ps->arena, text.ptr, text.len);
            if (text.len != 0u && m->text.len == 0u) {
                amf_diag_set(diag, AMF_ERR_ARENA_FULL, ev->line, ev->col, "arena full copying metadata text");
                return AMF_ERR_ARENA_FULL;
            }
        }
        return AMF_OK;
    }
    if (tag == T_COMPOSITE) {
        if (ps->current_composite != AMF_NONE_INDEX) {
            amf_composite *c;
            c = ps->doc->composites + ps->current_composite;
            c->formula = amf_arena_copy_slice(ps->arena, text.ptr, text.len);
            if (text.len != 0u && c->formula.len == 0u) {
                amf_diag_set(diag, AMF_ERR_ARENA_FULL, ev->line, ev->col, "arena full copying composite formula");
                return AMF_ERR_ARENA_FULL;
            }
        }
        return AMF_OK;
    }
    if (tag == T_TEXTURE && ps->current_texture != AMF_NONE_INDEX) {
        amf_texture *t;
        t = ps->doc->textures + ps->current_texture;
        t->data_base64 = amf_arena_copy_slice(ps->arena, text.ptr, text.len);
        if (text.len != 0u && t->data_base64.len == 0u) {
            amf_diag_set(diag, AMF_ERR_ARENA_FULL, ev->line, ev->col, "arena full copying texture payload");
            return AMF_ERR_ARENA_FULL;
        }
        return AMF_OK;
    }
    return amf_set_numeric_text(ps, tag, text, ev, diag);
}

static void amf_parse_state_init(amf_parse_state *ps, amf_document *doc, amf_arena *arena)
{
    memset(ps, 0, sizeof(*ps));
    ps->doc = doc;
    ps->arena = arena;
    ps->current_object = AMF_NONE_INDEX;
    ps->current_material = AMF_NONE_INDEX;
    ps->current_texture = AMF_NONE_INDEX;
    ps->current_constellation = AMF_NONE_INDEX;
    ps->current_instance = AMF_NONE_INDEX;
    ps->current_vertex = AMF_NONE_INDEX;
    ps->current_volume = AMF_NONE_INDEX;
    ps->current_triangle = AMF_NONE_INDEX;
    ps->current_metadata = AMF_NONE_INDEX;
    ps->current_composite = AMF_NONE_INDEX;
}

int amf_parse_xml(amf_document *doc, amf_arena *arena, const char *xml, size_t len, const amf_options *opts, amf_diag *diag)
{
    amf_counts counts;
    amf_xml_scan sc;
    amf_xml_event ev;
    amf_parse_state ps;
    int r;
    (void)opts;
    if (doc == 0 || arena == 0 || xml == 0) {
        amf_diag_set(diag, AMF_ERR_BAD_ARG, 1u, 1u, "bad argument");
        return AMF_ERR_BAD_ARG;
    }
    amf_diag_clear(diag);
    r = amf_count_xml(xml, len, &counts, diag);
    if (r != AMF_OK) return r;
    r = amf_alloc_document(doc, arena, &counts);
    if (r != AMF_OK) {
        amf_diag_set(diag, r, 1u, 1u, "arena too small for AMF document arrays");
        return r;
    }
    amf_parse_state_init(&ps, doc, arena);
    sc.src = xml;
    sc.len = len;
    sc.pos = 0u;
    sc.line = 1u;
    sc.col = 1u;
    for (;;) {
        r = amf_xml_next(&sc, &ev, diag);
        if (r != AMF_OK) return r;
        if (ev.type == X_EOF) break;
        if (ev.type == X_START) {
            r = amf_start_element(&ps, &ev, diag);
            if (r != AMF_OK) {
                if (diag != 0 && diag->code == AMF_OK) amf_diag_set(diag, r, ev.line, ev.col, "AMF parse start failed");
                return r;
            }
            if (ev.self_close) {
                r = amf_end_element(&ps, &ev, diag);
                if (r != AMF_OK) return r;
            }
        } else if (ev.type == X_END) {
            r = amf_end_element(&ps, &ev, diag);
            if (r != AMF_OK) return r;
        } else if (ev.type == X_TEXT) {
            r = amf_text_element(&ps, &ev, diag);
            if (r != AMF_OK) return r;
        }
    }
    return amf_validate(doc, diag);
}

static int amf_id_duplicate_object(const amf_document *doc)
{
    size_t i;
    size_t j;
    i = 0u;
    while (i < doc->object_count) {
        j = i + 1u;
        while (j < doc->object_count) {
            if (doc->objects[i].id == doc->objects[j].id) return 1;
            j++;
        }
        i++;
    }
    return 0;
}

static int amf_id_duplicate_material(const amf_document *doc)
{
    size_t i;
    size_t j;
    i = 0u;
    while (i < doc->material_count) {
        j = i + 1u;
        while (j < doc->material_count) {
            if (doc->materials[i].id == doc->materials[j].id) return 1;
            j++;
        }
        i++;
    }
    return 0;
}

int amf_validate(const amf_document *doc, amf_diag *diag)
{
    size_t oi;
    if (doc == 0) {
        amf_diag_set(diag, AMF_ERR_BAD_ARG, 1u, 1u, "bad argument");
        return AMF_ERR_BAD_ARG;
    }
    if (doc->object_count == 0u) {
        amf_diag_set(diag, AMF_ERR_REQUIRED, 1u, 1u, "AMF needs at least one object");
        return AMF_ERR_REQUIRED;
    }
    if (amf_id_duplicate_object(doc)) {
        amf_diag_set(diag, AMF_ERR_VALIDATE, 1u, 1u, "duplicate object id");
        return AMF_ERR_VALIDATE;
    }
    if (amf_id_duplicate_material(doc)) {
        amf_diag_set(diag, AMF_ERR_VALIDATE, 1u, 1u, "duplicate material id");
        return AMF_ERR_VALIDATE;
    }
    oi = 0u;
    while (oi < doc->object_count) {
        const amf_object *o;
        size_t ti;
        o = doc->objects + oi;
        if (o->vertex_count == 0u) {
            amf_diag_set(diag, AMF_ERR_REQUIRED, 1u, 1u, "object has no vertices");
            return AMF_ERR_REQUIRED;
        }
        if (o->volume_count == 0u) {
            amf_diag_set(diag, AMF_ERR_REQUIRED, 1u, 1u, "object has no volumes");
            return AMF_ERR_REQUIRED;
        }
        ti = o->triangle_first;
        while (ti < o->triangle_first + o->triangle_count) {
            const amf_triangle *t;
            t = doc->triangles + ti;
            if (t->v1 >= o->vertex_count || t->v2 >= o->vertex_count || t->v3 >= o->vertex_count) {
                amf_diag_set(diag, AMF_ERR_VALIDATE, 1u, 1u, "triangle vertex index outside object vertices");
                return AMF_ERR_VALIDATE;
            }
            ti++;
        }
        oi++;
    }
    amf_diag_clear(diag);
    return AMF_OK;
}

int amf_parse_memory(amf_document *doc, amf_arena *arena, const amf_u8 *data, size_t len, const amf_options *opts, amf_diag *diag)
{
    if (data == 0 || len == 0u) {
        amf_diag_set(diag, AMF_ERR_BAD_ARG, 1u, 1u, "empty AMF input");
        return AMF_ERR_BAD_ARG;
    }
    if (len >= 4u && data[0] == 'P' && data[1] == 'K' && data[2] == 3 && data[3] == 4) {
        amf_zip_entry entry;
        int r;
        r = amf_zip_find_first_xml(data, len, &entry, diag);
        if (r != AMF_OK) return r;
        if (entry.method == AMF_ZIP_METHOD_STORE) {
            return amf_parse_xml(doc, arena, (const char *)entry.data, entry.compressed_size, opts, diag);
        }
        if (entry.method == AMF_ZIP_METHOD_DEFLATE) {
            size_t out_len;
            if (opts == 0 || opts->zip_scratch == 0 || opts->zip_scratch_size == 0u) {
                amf_diag_set(diag, AMF_ERR_ZIP_NEEDS_OUTPUT, 1u, 1u, "compressed AMF needs caller scratch output buffer");
                return AMF_ERR_ZIP_NEEDS_OUTPUT;
            }
            if (opts->inflate_raw_deflate == 0) {
                amf_diag_set(diag, AMF_ERR_UNSUPPORTED_ZIP, 1u, 1u, "ZIP deflate entry needs user-supplied raw deflate callback");
                return AMF_ERR_UNSUPPORTED_ZIP;
            }
            r = opts->inflate_raw_deflate(opts->inflate_user, entry.data, entry.compressed_size, opts->zip_scratch, opts->zip_scratch_size, &out_len);
            if (r != AMF_OK) return r;
            return amf_parse_xml(doc, arena, (const char *)opts->zip_scratch, out_len, opts, diag);
        }
        amf_diag_set(diag, AMF_ERR_UNSUPPORTED_ZIP, 1u, 1u, "unsupported ZIP compression method");
        return AMF_ERR_UNSUPPORTED_ZIP;
    }
    return amf_parse_xml(doc, arena, (const char *)data, len, opts, diag);
}
