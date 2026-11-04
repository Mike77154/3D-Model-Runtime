#include "c3dae_v7.h"
#include <string.h>

#define V7_NIL ((const char *)0)

static unsigned long v7_strlen(const char *s)
{
    unsigned long n;
    n = 0UL;
    if (s == (const char *)0) return 0UL;
    while (s[n] != '\0') n++;
    return n;
}

static int v7_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void v7_trim_bounds(const char **a, const char **b)
{
    while (*a < *b && v7_is_space(**a)) (*a)++;
    while (*b > *a && v7_is_space((*b)[-1])) (*b)--;
}

static void v7_copy(char *dst, unsigned long cap, const char *a, const char *b)
{
    unsigned long n;
    if (cap == 0UL) return;
    if (a == V7_NIL || b == V7_NIL || b < a) { dst[0] = '\0'; return; }
    v7_trim_bounds(&a, &b);
    n = (unsigned long)(b - a);
    if (n >= cap) n = cap - 1UL;
    if (n > 0UL) memcpy(dst, a, (size_t)n);
    dst[n] = '\0';
}

static int v7_tag_local_matches(const char *p, const char *end, const char *name)
{
    const char *a;
    const char *b;
    const char *colon;
    unsigned long n;
    unsigned long m;
    if (p >= end || *p != '<') return 0;
    a = p + 1;
    if (a < end && *a == '/') a++;
    if (a < end && (*a == '?' || *a == '!')) return 0;
    b = a;
    while (b < end && !v7_is_space(*b) && *b != '>' && *b != '/') b++;
    colon = V7_NIL;
    {
        const char *q;
        q = a;
        while (q < b) { if (*q == ':') colon = q; q++; }
    }
    if (colon != V7_NIL) a = colon + 1;
    n = (unsigned long)(b - a);
    m = v7_strlen(name);
    if (n != m) return 0;
    return memcmp(a, name, (size_t)n) == 0;
}

static const char *v7_find_gt(const char *p, const char *end)
{
    int q;
    char quote;
    q = 0;
    quote = '\0';
    while (p < end) {
        if (q) { if (*p == quote) q = 0; }
        else {
            if (*p == '\'' || *p == '"') { q = 1; quote = *p; }
            else if (*p == '>') return p + 1;
        }
        p++;
    }
    return V7_NIL;
}

static const char *v7_find_start(const char *p, const char *end, const char *name, const char **tag_end)
{
    const char *gt;
    while (p < end) {
        while (p < end && *p != '<') p++;
        if (p >= end) break;
        if (v7_tag_local_matches(p, end, name)) {
            gt = v7_find_gt(p, end);
            if (gt == V7_NIL) return V7_NIL;
            if (tag_end != (const char **)0) *tag_end = gt;
            return p;
        }
        p++;
    }
    return V7_NIL;
}

static const char *v7_find_end(const char *p, const char *end, const char *name, const char **after)
{
    const char *gt;
    while (p < end) {
        while (p < end && *p != '<') p++;
        if (p >= end) break;
        if (p + 1 < end && p[1] == '/' && v7_tag_local_matches(p, end, name)) {
            gt = v7_find_gt(p, end);
            if (gt == V7_NIL) return V7_NIL;
            if (after != (const char **)0) *after = gt;
            return p;
        }
        p++;
    }
    return V7_NIL;
}

static int v7_is_self_close(const char *tag, const char *tag_end)
{
    const char *p;
    p = tag_end;
    while (p > tag) {
        p--;
        if (v7_is_space(*p) || *p == '>') continue;
        return *p == '/';
    }
    return 0;
}

static int v7_attr_get(const char *tag, const char *tag_end, const char *attr, char *out, unsigned long cap)
{
    const char *p;
    unsigned long alen;
    if (cap == 0UL) return 0;
    out[0] = '\0';
    alen = v7_strlen(attr);
    p = tag;
    while (p < tag_end) {
        while (p < tag_end && (v7_is_space(*p) || *p == '<' || *p == '/' || *p == '>')) p++;
        if (p + alen < tag_end && memcmp(p, attr, (size_t)alen) == 0) {
            const char *q;
            q = p + alen;
            if (q < tag_end && (*q == '=' || v7_is_space(*q))) {
                while (q < tag_end && v7_is_space(*q)) q++;
                if (q < tag_end && *q == '=') {
                    q++;
                    while (q < tag_end && v7_is_space(*q)) q++;
                    if (q < tag_end && (*q == '"' || *q == '\'')) {
                        char quote;
                        const char *a;
                        quote = *q;
                        q++;
                        a = q;
                        while (q < tag_end && *q != quote) q++;
                        v7_copy(out, cap, a, q);
                        return 1;
                    }
                }
            }
        }
        while (p < tag_end && !v7_is_space(*p) && *p != '>') p++;
    }
    return 0;
}

static void v7_ref_to_id(char *dst, unsigned long cap, const char *src)
{
    const char *a;
    a = src;
    if (a != (const char *)0 && *a == '#') a++;
    v7_copy(dst, cap, a, a + v7_strlen(a));
}

static c3dae_fixed v7_parse_fixed(const char *s)
{
    long sign;
    long ip;
    long fp;
    long scale;
    int digits;
    sign = 1L;
    ip = 0L;
    fp = 0L;
    scale = 1L;
    digits = 0;
    if (s == (const char *)0) return 0L;
    while (v7_is_space(*s)) s++;
    if (*s == '-') { sign = -1L; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') { ip = ip * 10L + (long)(*s - '0'); s++; }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9' && digits < 6) { fp = fp * 10L + (long)(*s - '0'); scale *= 10L; digits++; s++; }
    }
    return (c3dae_fixed)(sign * ((ip << C3DAE_FIXED_SHIFT) + ((fp << C3DAE_FIXED_SHIFT) / scale)));
}

static c3dae_fixed v7_parse_fixed_range(const char *a, const char *b)
{
    char tmp[64];
    v7_copy(tmp, sizeof(tmp), a, b);
    return v7_parse_fixed(tmp);
}

static int v7_parse_bool_text(const char *a, const char *b)
{
    char tmp[16];
    v7_copy(tmp, sizeof(tmp), a, b);
    if (strcmp(tmp, "false") == 0 || strcmp(tmp, "FALSE") == 0 || strcmp(tmp, "0") == 0) return 0;
    return 1;
}

static const char *v7_child_text(const char *a, const char *b, const char *name, char *out, unsigned long cap)
{
    const char *tag;
    const char *tag_end;
    const char *end_tag;
    const char *after;
    if (cap > 0UL) out[0] = '\0';
    tag = v7_find_start(a, b, name, &tag_end);
    if (tag == V7_NIL) return V7_NIL;
    if (v7_is_self_close(tag, tag_end)) return tag_end;
    end_tag = v7_find_end(tag_end, b, name, &after);
    if (end_tag == V7_NIL) return V7_NIL;
    v7_copy(out, cap, tag_end, end_tag);
    return after;
}

static c3dae_u32 v7_parse_uints(const char *a, const char *b, c3dae_u32 *out, c3dae_u32 cap)
{
    c3dae_u32 count;
    unsigned long v;
    count = 0UL;
    while (a < b) {
        while (a < b && !((*a >= '0' && *a <= '9') || *a == '-')) a++;
        if (a >= b) break;
        if (*a == '-') {
            a++;
            while (a < b && *a >= '0' && *a <= '9') a++;
            if (count < cap) out[count++] = 0UL;
        } else {
            v = 0UL;
            while (a < b && *a >= '0' && *a <= '9') { v = v * 10UL + (unsigned long)(*a - '0'); a++; }
            if (count < cap) out[count++] = (c3dae_u32)v;
        }
    }
    return count;
}

static c3dae_u32 v7_parse_fixed_vec3(const char *a, const char *b, c3dae_fixed out[3])
{
    c3dae_u32 count;
    const char *s;
    count = 0UL;
    s = a;
    while (s < b && count < 3UL) {
        while (s < b && !((*s >= '0' && *s <= '9') || *s == '-' || *s == '+')) s++;
        a = s;
        if (s >= b) break;
        if (*s == '-' || *s == '+') s++;
        while (s < b && ((*s >= '0' && *s <= '9') || *s == '.')) s++;
        out[count++] = v7_parse_fixed_range(a, s);
    }
    return count;
}

void c3dae_v7_document_init(c3dae_v7_document *doc)
{
    if (doc == (c3dae_v7_document *)0) return;
    memset(doc, 0, sizeof(*doc));
    doc->asset.unit_meter = C3DAE_FIXED_ONE;
    doc->asset.up_axis = C3DAE_UP_UNKNOWN;
}

void c3dae_v7_bind_animation_clips(c3dae_v7_document *doc,
                                   c3dae_v7_animation_clip *clips,
                                   c3dae_u32 clip_cap,
                                   c3dae_v7_animation_ref *refs,
                                   c3dae_u32 ref_cap)
{
    if (doc == (c3dae_v7_document *)0) return;
    doc->clips = clips;
    doc->clip_cap = clip_cap;
    doc->clip_count = 0UL;
    doc->clip_refs = refs;
    doc->clip_ref_cap = ref_cap;
    doc->clip_ref_count = 0UL;
}

void c3dae_v7_bind_lines(c3dae_v7_document *doc, c3dae_v7_line *lines, c3dae_u32 cap)
{
    if (doc == (c3dae_v7_document *)0) return;
    doc->lines = lines;
    doc->line_cap = cap;
    doc->line_count = 0UL;
}

void c3dae_v7_bind_fx_profiles(c3dae_v7_document *doc, c3dae_v7_fx_profile *profiles, c3dae_u32 cap)
{
    if (doc == (c3dae_v7_document *)0) return;
    doc->fx_profiles = profiles;
    doc->fx_profile_cap = cap;
    doc->fx_profile_count = 0UL;
}

void c3dae_v7_bind_constraints(c3dae_v7_document *doc, c3dae_v7_rigid_constraint *constraints, c3dae_u32 cap)
{
    if (doc == (c3dae_v7_document *)0) return;
    doc->constraints = constraints;
    doc->constraint_cap = cap;
    doc->constraint_count = 0UL;
}

void c3dae_v7_bind_kinematics15(c3dae_v7_document *doc,
                                c3dae_v7_articulated_system *systems,
                                c3dae_u32 system_cap,
                                c3dae_v7_formula *formulas,
                                c3dae_u32 formula_cap)
{
    if (doc == (c3dae_v7_document *)0) return;
    doc->articulated_systems = systems;
    doc->articulated_system_cap = system_cap;
    doc->articulated_system_count = 0UL;
    doc->formulas = formulas;
    doc->formula_cap = formula_cap;
    doc->formula_count = 0UL;
}

void c3dae_v7_bind_extras(c3dae_v7_document *doc, c3dae_v7_extra *extras, c3dae_u32 cap)
{
    if (doc == (c3dae_v7_document *)0) return;
    doc->extras = extras;
    doc->extra_cap = cap;
    doc->extra_count = 0UL;
}

static void v7_parse_asset(const char *xml, const char *end, c3dae_v7_document *doc)
{
    const char *asset;
    const char *tag_end;
    const char *asset_end;
    const char *after;
    const char *unit;
    const char *unit_end;
    char tmp[C3DAE_V7_TEXT_MAX];
    asset = v7_find_start(xml, end, "asset", &tag_end);
    if (asset == V7_NIL) return;
    asset_end = v7_find_end(tag_end, end, "asset", &after);
    if (asset_end == V7_NIL) return;
    v7_child_text(tag_end, asset_end, "created", doc->asset.created, sizeof(doc->asset.created));
    v7_child_text(tag_end, asset_end, "modified", doc->asset.modified, sizeof(doc->asset.modified));
    v7_child_text(tag_end, asset_end, "revision", doc->asset.revision, sizeof(doc->asset.revision));
    v7_child_text(tag_end, asset_end, "title", doc->asset.title, sizeof(doc->asset.title));
    v7_child_text(tag_end, asset_end, "subject", doc->asset.subject, sizeof(doc->asset.subject));
    v7_child_text(tag_end, asset_end, "keywords", doc->asset.keywords, sizeof(doc->asset.keywords));
    v7_child_text(tag_end, asset_end, "author", doc->asset.author, sizeof(doc->asset.author));
    v7_child_text(tag_end, asset_end, "authoring_tool", doc->asset.authoring_tool, sizeof(doc->asset.authoring_tool));
    v7_child_text(tag_end, asset_end, "comments", doc->asset.comments, sizeof(doc->asset.comments));
    v7_child_text(tag_end, asset_end, "copyright", doc->asset.copyright_text, sizeof(doc->asset.copyright_text));
    unit = v7_find_start(tag_end, asset_end, "unit", &unit_end);
    if (unit != V7_NIL) {
        v7_attr_get(unit, unit_end, "name", doc->asset.unit_name, sizeof(doc->asset.unit_name));
        tmp[0] = '\0';
        if (v7_attr_get(unit, unit_end, "meter", tmp, sizeof(tmp))) doc->asset.unit_meter = v7_parse_fixed(tmp);
    }
    tmp[0] = '\0';
    if (v7_child_text(tag_end, asset_end, "up_axis", tmp, sizeof(tmp)) != V7_NIL) {
        if (strcmp(tmp, "X_UP") == 0) doc->asset.up_axis = C3DAE_UP_X;
        else if (strcmp(tmp, "Y_UP") == 0) doc->asset.up_axis = C3DAE_UP_Y;
        else if (strcmp(tmp, "Z_UP") == 0) doc->asset.up_axis = C3DAE_UP_Z;
    }
}

static void v7_parse_scene(const char *xml, const char *end, c3dae_v7_document *doc)
{
    const char *scene;
    const char *tag_end;
    const char *scene_end;
    const char *after;
    const char *inst;
    const char *inst_end;
    char tmp[C3DAE_ID_MAX];
    scene = v7_find_start(xml, end, "scene", &tag_end);
    if (scene == V7_NIL) return;
    scene_end = v7_find_end(tag_end, end, "scene", &after);
    if (scene_end == V7_NIL) return;
    inst = v7_find_start(tag_end, scene_end, "instance_visual_scene", &inst_end);
    if (inst != V7_NIL) { tmp[0] = '\0'; v7_attr_get(inst, inst_end, "url", tmp, sizeof(tmp)); v7_ref_to_id(doc->scene.visual_scene, C3DAE_ID_MAX, tmp); }
    inst = v7_find_start(tag_end, scene_end, "instance_physics_scene", &inst_end);
    if (inst != V7_NIL) { tmp[0] = '\0'; v7_attr_get(inst, inst_end, "url", tmp, sizeof(tmp)); v7_ref_to_id(doc->scene.physics_scene, C3DAE_ID_MAX, tmp); }
    inst = v7_find_start(tag_end, scene_end, "instance_kinematics_scene", &inst_end);
    if (inst != V7_NIL) { tmp[0] = '\0'; v7_attr_get(inst, inst_end, "url", tmp, sizeof(tmp)); v7_ref_to_id(doc->scene.kinematics_scene, C3DAE_ID_MAX, tmp); }
}

static void v7_push_clip_ref(c3dae_v7_document *doc, const char *url)
{
    if (doc->clip_refs == (c3dae_v7_animation_ref *)0 || doc->clip_ref_count >= doc->clip_ref_cap) { doc->truncated_count++; return; }
    v7_ref_to_id(doc->clip_refs[doc->clip_ref_count].animation, C3DAE_ID_MAX, url);
    doc->clip_ref_count++;
}

static void v7_parse_animation_clips(const char *xml, const char *end, c3dae_v7_document *doc)
{
    const char *lib;
    const char *lib_tag_end;
    const char *lib_end;
    const char *after_lib;
    const char *clip;
    const char *clip_end_tag;
    const char *clip_end;
    const char *after_clip;
    const char *inst;
    const char *inst_end;
    c3dae_v7_animation_clip *c;
    char tmp[C3DAE_ID_MAX];
    lib = v7_find_start(xml, end, "library_animation_clips", &lib_tag_end);
    if (lib == V7_NIL || doc->clips == (c3dae_v7_animation_clip *)0) return;
    lib_end = v7_find_end(lib_tag_end, end, "library_animation_clips", &after_lib);
    if (lib_end == V7_NIL) return;
    clip = lib_tag_end;
    while (1) {
        clip = v7_find_start(clip, lib_end, "animation_clip", &clip_end_tag);
        if (clip == V7_NIL) break;
        clip_end = v7_find_end(clip_end_tag, lib_end, "animation_clip", &after_clip);
        if (clip_end == V7_NIL) break;
        if (doc->clip_count >= doc->clip_cap) { doc->truncated_count++; return; }
        c = &doc->clips[doc->clip_count];
        memset(c, 0, sizeof(*c));
        v7_attr_get(clip, clip_end_tag, "id", c->id, C3DAE_ID_MAX);
        v7_attr_get(clip, clip_end_tag, "name", c->name, C3DAE_ID_MAX);
        tmp[0] = '\0'; if (v7_attr_get(clip, clip_end_tag, "start", tmp, sizeof(tmp))) c->start = v7_parse_fixed(tmp);
        tmp[0] = '\0'; if (v7_attr_get(clip, clip_end_tag, "end", tmp, sizeof(tmp))) c->end = v7_parse_fixed(tmp);
        c->first_ref = doc->clip_ref_count;
        inst = clip_end_tag;
        while (1) {
            inst = v7_find_start(inst, clip_end, "instance_animation", &inst_end);
            if (inst == V7_NIL) break;
            tmp[0] = '\0';
            v7_attr_get(inst, inst_end, "url", tmp, sizeof(tmp));
            v7_push_clip_ref(doc, tmp);
            inst = inst_end;
        }
        c->ref_count = doc->clip_ref_count - c->first_ref;
        doc->clip_count++;
        clip = after_clip;
    }
}

static int v7_primitive_vertex_offset(const char *a, const char *b, c3dae_u32 *stride_out, c3dae_u32 *voff_out)
{
    const char *inp;
    const char *inp_end;
    char sem[32];
    char offbuf[32];
    c3dae_u32 maxoff;
    c3dae_u32 voff;
    c3dae_u32 off;
    int found;
    maxoff = 0UL;
    voff = 0UL;
    found = 0;
    inp = a;
    while (1) {
        inp = v7_find_start(inp, b, "input", &inp_end);
        if (inp == V7_NIL) break;
        sem[0] = '\0'; offbuf[0] = '\0';
        v7_attr_get(inp, inp_end, "semantic", sem, sizeof(sem));
        v7_attr_get(inp, inp_end, "offset", offbuf, sizeof(offbuf));
        off = (c3dae_u32)v7_parse_fixed(offbuf) >> C3DAE_FIXED_SHIFT;
        if (off > maxoff) maxoff = off;
        if (!found && (strcmp(sem, "VERTEX") == 0 || strcmp(sem, "POSITION") == 0)) { voff = off; found = 1; }
        inp = inp_end;
    }
    *stride_out = maxoff + 1UL;
    *voff_out = voff;
    if (*stride_out == 0UL) *stride_out = 1UL;
    return found;
}

static void v7_emit_line(c3dae_v7_document *doc, const char *geom_id, const char *mat, c3dae_u32 a, c3dae_u32 b, int kind)
{
    c3dae_v7_line *ln;
    if (doc->lines == (c3dae_v7_line *)0) return;
    if (doc->line_count >= doc->line_cap) { doc->truncated_count++; return; }
    ln = &doc->lines[doc->line_count];
    memset(ln, 0, sizeof(*ln));
    v7_copy(ln->geometry, C3DAE_ID_MAX, geom_id, geom_id + v7_strlen(geom_id));
    v7_copy(ln->material_symbol, C3DAE_ID_MAX, mat, mat + v7_strlen(mat));
    ln->a = a;
    ln->b = b;
    ln->kind = kind;
    doc->line_count++;
}

static void v7_parse_lines_one(const char *prim, const char *prim_end_tag, const char *prim_end, const char *geom_id, c3dae_v7_document *doc)
{
    const char *ptag;
    const char *ptag_end;
    const char *pend;
    const char *afterp;
    c3dae_u32 idx[512];
    c3dae_u32 n;
    c3dae_u32 stride;
    c3dae_u32 voff;
    c3dae_u32 i;
    char mat[C3DAE_ID_MAX];
    mat[0] = '\0';
    v7_attr_get(prim, prim_end_tag, "material", mat, sizeof(mat));
    v7_primitive_vertex_offset(prim_end_tag, prim_end, &stride, &voff);
    ptag = v7_find_start(prim_end_tag, prim_end, "p", &ptag_end);
    if (ptag == V7_NIL) return;
    pend = v7_find_end(ptag_end, prim_end, "p", &afterp);
    if (pend == V7_NIL) return;
    n = v7_parse_uints(ptag_end, pend, idx, 512UL);
    i = 0UL;
    while (i + (2UL * stride) <= n) {
        v7_emit_line(doc, geom_id, mat, idx[i + voff], idx[i + stride + voff], C3DAE_V7_LINE_LINES);
        i += 2UL * stride;
    }
}

static void v7_parse_linestrips_one(const char *prim, const char *prim_end_tag, const char *prim_end, const char *geom_id, c3dae_v7_document *doc)
{
    const char *ptag;
    const char *ptag_end;
    const char *pend;
    const char *afterp;
    c3dae_u32 idx[512];
    c3dae_u32 n;
    c3dae_u32 stride;
    c3dae_u32 voff;
    c3dae_u32 vc;
    c3dae_u32 i;
    char mat[C3DAE_ID_MAX];
    mat[0] = '\0';
    v7_attr_get(prim, prim_end_tag, "material", mat, sizeof(mat));
    v7_primitive_vertex_offset(prim_end_tag, prim_end, &stride, &voff);
    ptag = prim_end_tag;
    while (1) {
        ptag = v7_find_start(ptag, prim_end, "p", &ptag_end);
        if (ptag == V7_NIL) break;
        pend = v7_find_end(ptag_end, prim_end, "p", &afterp);
        if (pend == V7_NIL) break;
        n = v7_parse_uints(ptag_end, pend, idx, 512UL);
        vc = n / stride;
        i = 0UL;
        while (i + 1UL < vc) {
            v7_emit_line(doc, geom_id, mat, idx[(i * stride) + voff], idx[((i + 1UL) * stride) + voff], C3DAE_V7_LINE_STRIP);
            i++;
        }
        ptag = afterp;
    }
}

static void v7_parse_geometry_report(const char *xml, const char *end, c3dae_v7_document *doc)
{
    const char *geom;
    const char *geom_end_tag;
    const char *geom_end;
    const char *after_geom;
    const char *mesh;
    const char *mesh_end_tag;
    const char *mesh_end;
    const char *after_mesh;
    const char *prim;
    const char *prim_end_tag;
    const char *prim_end;
    const char *after_prim;
    const char *tmp;
    const char *tmp_end;
    char gid[C3DAE_ID_MAX];
    geom = xml;
    while (1) {
        geom = v7_find_start(geom, end, "geometry", &geom_end_tag);
        if (geom == V7_NIL) break;
        geom_end = v7_find_end(geom_end_tag, end, "geometry", &after_geom);
        if (geom_end == V7_NIL) break;
        gid[0] = '\0';
        v7_attr_get(geom, geom_end_tag, "id", gid, sizeof(gid));
        if (v7_find_start(geom_end_tag, geom_end, "brep", &tmp_end) != V7_NIL) doc->stats.brep_count++;
        if (v7_find_start(geom_end_tag, geom_end, "spline", &tmp_end) != V7_NIL) doc->stats.spline_count++;
        if (v7_find_start(geom_end_tag, geom_end, "convex_mesh", &tmp_end) != V7_NIL) doc->stats.convex_mesh_count++;
        tmp = geom_end_tag;
        while (1) {
            tmp = v7_find_start(tmp, geom_end, "h", &tmp_end);
            if (tmp == V7_NIL) break;
            doc->stats.polygon_hole_count++;
            tmp = tmp_end;
        }
        mesh = v7_find_start(geom_end_tag, geom_end, "mesh", &mesh_end_tag);
        if (mesh != V7_NIL) {
            mesh_end = v7_find_end(mesh_end_tag, geom_end, "mesh", &after_mesh);
            if (mesh_end != V7_NIL) {
                prim = mesh_end_tag;
                while (1) {
                    prim = v7_find_start(prim, mesh_end, "lines", &prim_end_tag);
                    if (prim == V7_NIL) break;
                    prim_end = v7_find_end(prim_end_tag, mesh_end, "lines", &after_prim);
                    if (prim_end == V7_NIL) break;
                    v7_parse_lines_one(prim, prim_end_tag, prim_end, gid, doc);
                    prim = after_prim;
                }
                prim = mesh_end_tag;
                while (1) {
                    prim = v7_find_start(prim, mesh_end, "linestrips", &prim_end_tag);
                    if (prim == V7_NIL) break;
                    prim_end = v7_find_end(prim_end_tag, mesh_end, "linestrips", &after_prim);
                    if (prim_end == V7_NIL) break;
                    v7_parse_linestrips_one(prim, prim_end_tag, prim_end, gid, doc);
                    prim = after_prim;
                }
            }
        }
        geom = after_geom;
    }
}

static void v7_parse_fx_profiles(const char *xml, const char *end, c3dae_v7_document *doc)
{
    const char *effect;
    const char *effect_end_tag;
    const char *effect_end;
    const char *after_effect;
    const char *t;
    const char *t_end;
    const char *param;
    const char *param_end;
    c3dae_v7_fx_profile *fx;
    effect = xml;
    while (1) {
        effect = v7_find_start(effect, end, "effect", &effect_end_tag);
        if (effect == V7_NIL) break;
        effect_end = v7_find_end(effect_end_tag, end, "effect", &after_effect);
        if (effect_end == V7_NIL) break;
        if (doc->fx_profiles == (c3dae_v7_fx_profile *)0) { effect = after_effect; continue; }
        if (doc->fx_profile_count >= doc->fx_profile_cap) { doc->truncated_count++; return; }
        fx = &doc->fx_profiles[doc->fx_profile_count];
        memset(fx, 0, sizeof(*fx));
        v7_attr_get(effect, effect_end_tag, "id", fx->effect, C3DAE_ID_MAX);
        if (v7_find_start(effect_end_tag, effect_end, "profile_COMMON", &t_end) != V7_NIL) fx->profile_mask |= C3DAE_V7_FX_COMMON;
        if (v7_find_start(effect_end_tag, effect_end, "profile_GLSL", &t_end) != V7_NIL) fx->profile_mask |= C3DAE_V7_FX_GLSL;
        if (v7_find_start(effect_end_tag, effect_end, "profile_CG", &t_end) != V7_NIL) fx->profile_mask |= C3DAE_V7_FX_CG;
        if (v7_find_start(effect_end_tag, effect_end, "profile_GLES", &t_end) != V7_NIL) fx->profile_mask |= C3DAE_V7_FX_GLES;
        if (v7_find_start(effect_end_tag, effect_end, "profile_GLES2", &t_end) != V7_NIL) { fx->profile_mask |= C3DAE_V7_FX_GLES2; doc->stats.profile_gles2_count++; }
        t = v7_find_start(effect_end_tag, effect_end, "transparent", &t_end);
        if (t != V7_NIL) v7_attr_get(t, t_end, "opaque", fx->transparent_opaque, C3DAE_ID_MAX);
        param = v7_find_start(effect_end_tag, effect_end, "newparam", &param_end);
        if (param == V7_NIL) param = v7_find_start(effect_end_tag, effect_end, "param", &param_end);
        if (param != V7_NIL) {
            if (!v7_attr_get(param, param_end, "sid", fx->first_param, C3DAE_ID_MAX)) v7_attr_get(param, param_end, "name", fx->first_param, C3DAE_ID_MAX);
        }
        if (fx->profile_mask != 0U || fx->transparent_opaque[0] != '\0') doc->fx_profile_count++;
        effect = after_effect;
    }
}

static void v7_parse_limits(const char *a, const char *b, const char *name, c3dae_fixed out[3])
{
    const char *tag;
    const char *tag_end;
    const char *end_tag;
    const char *after;
    tag = v7_find_start(a, b, name, &tag_end);
    if (tag == V7_NIL) return;
    end_tag = v7_find_end(tag_end, b, name, &after);
    if (end_tag == V7_NIL) return;
    v7_parse_fixed_vec3(tag_end, end_tag, out);
}

static void v7_parse_constraints(const char *xml, const char *end, c3dae_v7_document *doc)
{
    const char *rc;
    const char *rc_end_tag;
    const char *rc_end;
    const char *after_rc;
    const char *tag;
    const char *tag_end;
    const char *sub_end;
    const char *after;
    c3dae_v7_rigid_constraint *c;
    char tmp[C3DAE_ID_MAX];
    tag = xml;
    while (1) {
        tag = v7_find_start(tag, end, "instance_rigid_constraint", &tag_end);
        if (tag == V7_NIL) break;
        doc->stats.instance_rigid_constraint_count++;
        tag = tag_end;
    }
    tag = xml;
    while (1) {
        tag = v7_find_start(tag, end, "instance_force_field", &tag_end);
        if (tag == V7_NIL) break;
        doc->stats.instance_force_field_count++;
        tag = tag_end;
    }
    rc = xml;
    while (1) {
        rc = v7_find_start(rc, end, "rigid_constraint", &rc_end_tag);
        if (rc == V7_NIL) break;
        rc_end = v7_find_end(rc_end_tag, end, "rigid_constraint", &after_rc);
        if (rc_end == V7_NIL) break;
        if (doc->constraints == (c3dae_v7_rigid_constraint *)0) { rc = after_rc; continue; }
        if (doc->constraint_count >= doc->constraint_cap) { doc->truncated_count++; return; }
        c = &doc->constraints[doc->constraint_count];
        memset(c, 0, sizeof(*c));
        c->enabled = 1;
        c->kind = C3DAE_V7_CONSTRAINT_GENERIC;
        v7_attr_get(rc, rc_end_tag, "sid", c->sid, C3DAE_ID_MAX);
        v7_attr_get(rc, rc_end_tag, "name", c->name, C3DAE_ID_MAX);
        tag = v7_find_start(rc_end_tag, rc_end, "ref_attachment", &tag_end);
        if (tag != V7_NIL) v7_attr_get(tag, tag_end, "rigid_body", c->ref_body, C3DAE_ID_MAX);
        tag = v7_find_start(rc_end_tag, rc_end, "attachment", &tag_end);
        if (tag != V7_NIL) v7_attr_get(tag, tag_end, "rigid_body", c->target_body, C3DAE_ID_MAX);
        tmp[0] = '\0';
        if ((sub_end = v7_child_text(rc_end_tag, rc_end, "enabled", tmp, sizeof(tmp))) != V7_NIL) c->enabled = v7_parse_bool_text(tmp, tmp + v7_strlen(tmp));
        tmp[0] = '\0';
        if ((sub_end = v7_child_text(rc_end_tag, rc_end, "interpenetrate", tmp, sizeof(tmp))) != V7_NIL) c->interpenetrate = v7_parse_bool_text(tmp, tmp + v7_strlen(tmp));
        tag = v7_find_start(rc_end_tag, rc_end, "linear", &tag_end);
        if (tag != V7_NIL) {
            sub_end = v7_find_end(tag_end, rc_end, "linear", &after);
            if (sub_end != V7_NIL) { v7_parse_limits(tag_end, sub_end, "min", c->linear_min); v7_parse_limits(tag_end, sub_end, "max", c->linear_max); }
        }
        tag = v7_find_start(rc_end_tag, rc_end, "angular", &tag_end);
        if (tag != V7_NIL) {
            sub_end = v7_find_end(tag_end, rc_end, "angular", &after);
            if (sub_end != V7_NIL) { v7_parse_limits(tag_end, sub_end, "min", c->angular_min); v7_parse_limits(tag_end, sub_end, "max", c->angular_max); }
        }
        if (c->angular_min[0] != 0 || c->angular_max[0] != 0) c->kind = C3DAE_V7_CONSTRAINT_HINGE;
        if (c->linear_min[0] != 0 || c->linear_max[0] != 0) c->kind = C3DAE_V7_CONSTRAINT_SLIDER;
        doc->constraint_count++;
        rc = after_rc;
    }
}

static void v7_parse_kinematics15(const char *xml, const char *end, c3dae_v7_document *doc)
{
    const char *lib;
    const char *lib_end_tag;
    const char *lib_end;
    const char *after_lib;
    const char *as;
    const char *as_end_tag;
    const char *as_end;
    const char *after_as;
    const char *inst;
    const char *inst_end;
    const char *formula;
    const char *formula_end_tag;
    const char *formula_end;
    const char *after_formula;
    const char *tech;
    const char *tech_end;
    char tmp[C3DAE_ID_MAX];
    lib = v7_find_start(xml, end, "library_articulated_systems", &lib_end_tag);
    if (lib != V7_NIL && doc->articulated_systems != (c3dae_v7_articulated_system *)0) {
        lib_end = v7_find_end(lib_end_tag, end, "library_articulated_systems", &after_lib);
        if (lib_end != V7_NIL) {
            as = lib_end_tag;
            while (1) {
                as = v7_find_start(as, lib_end, "articulated_system", &as_end_tag);
                if (as == V7_NIL) break;
                as_end = v7_find_end(as_end_tag, lib_end, "articulated_system", &after_as);
                if (as_end == V7_NIL) break;
                if (doc->articulated_system_count >= doc->articulated_system_cap) { doc->truncated_count++; break; }
                memset(&doc->articulated_systems[doc->articulated_system_count], 0, sizeof(doc->articulated_systems[doc->articulated_system_count]));
                v7_attr_get(as, as_end_tag, "id", doc->articulated_systems[doc->articulated_system_count].id, C3DAE_ID_MAX);
                v7_attr_get(as, as_end_tag, "name", doc->articulated_systems[doc->articulated_system_count].name, C3DAE_ID_MAX);
                inst = v7_find_start(as_end_tag, as_end, "instance_kinematics_model", &inst_end);
                if (inst != V7_NIL) { tmp[0] = '\0'; v7_attr_get(inst, inst_end, "url", tmp, sizeof(tmp)); v7_ref_to_id(doc->articulated_systems[doc->articulated_system_count].kinematics_model, C3DAE_ID_MAX, tmp); }
                inst = v7_find_start(as_end_tag, as_end, "instance_articulated_system", &inst_end);
                if (inst != V7_NIL) { tmp[0] = '\0'; v7_attr_get(inst, inst_end, "url", tmp, sizeof(tmp)); v7_ref_to_id(doc->articulated_systems[doc->articulated_system_count].motion_model, C3DAE_ID_MAX, tmp); }
                doc->articulated_system_count++;
                as = after_as;
            }
        }
    }
    lib = v7_find_start(xml, end, "library_formulas", &lib_end_tag);
    if (lib != V7_NIL && doc->formulas != (c3dae_v7_formula *)0) {
        lib_end = v7_find_end(lib_end_tag, end, "library_formulas", &after_lib);
        if (lib_end != V7_NIL) {
            formula = lib_end_tag;
            while (1) {
                formula = v7_find_start(formula, lib_end, "formula", &formula_end_tag);
                if (formula == V7_NIL) break;
                formula_end = v7_find_end(formula_end_tag, lib_end, "formula", &after_formula);
                if (formula_end == V7_NIL) break;
                if (doc->formula_count >= doc->formula_cap) { doc->truncated_count++; break; }
                memset(&doc->formulas[doc->formula_count], 0, sizeof(doc->formulas[doc->formula_count]));
                v7_attr_get(formula, formula_end_tag, "id", doc->formulas[doc->formula_count].id, C3DAE_ID_MAX);
                v7_attr_get(formula, formula_end_tag, "name", doc->formulas[doc->formula_count].name, C3DAE_ID_MAX);
                v7_child_text(formula_end_tag, formula_end, "target", doc->formulas[doc->formula_count].target, C3DAE_ID_MAX);
                tech = v7_find_start(formula_end_tag, formula_end, "technique", &tech_end);
                if (tech != V7_NIL) v7_attr_get(tech, tech_end, "profile", doc->formulas[doc->formula_count].technique_profile, C3DAE_ID_MAX);
                doc->formula_count++;
                formula = after_formula;
            }
        }
    }
}

static void v7_parse_extras(const char *xml, const char *end, c3dae_v7_document *doc)
{
    const char *ex;
    const char *ex_end_tag;
    const char *ex_end;
    const char *after_ex;
    const char *tech;
    const char *tech_end;
    c3dae_v7_extra *e;
    if (doc->extras == (c3dae_v7_extra *)0) return;
    ex = xml;
    while (1) {
        ex = v7_find_start(ex, end, "extra", &ex_end_tag);
        if (ex == V7_NIL) break;
        ex_end = v7_find_end(ex_end_tag, end, "extra", &after_ex);
        if (ex_end == V7_NIL) break;
        if (doc->extra_count >= doc->extra_cap) { doc->truncated_count++; return; }
        e = &doc->extras[doc->extra_count];
        memset(e, 0, sizeof(*e));
        v7_attr_get(ex, ex_end_tag, "id", e->parent_id, C3DAE_ID_MAX);
        v7_attr_get(ex, ex_end_tag, "type", e->type, C3DAE_ID_MAX);
        tech = v7_find_start(ex_end_tag, ex_end, "technique", &tech_end);
        if (tech != V7_NIL) v7_attr_get(tech, tech_end, "profile", e->technique_profile, C3DAE_ID_MAX);
        v7_copy(e->snippet, C3DAE_V7_SNIPPET_MAX, ex_end_tag, ex_end);
        e->parent_kind = C3DAE_V7_EXTRA_UNKNOWN;
        doc->extra_count++;
        ex = after_ex;
    }
}

int c3dae_v7_scan_memory(const char *xml, c3dae_u32 xml_len, c3dae_v7_document *doc)
{
    const char *end;
    const char *p;
    const char *tag_end;
    if (xml == (const char *)0 || doc == (c3dae_v7_document *)0) return C3DAE_V7_ERR_NULL;
    end = xml + xml_len;
    v7_parse_asset(xml, end, doc);
    v7_parse_scene(xml, end, doc);
    v7_parse_animation_clips(xml, end, doc);
    v7_parse_geometry_report(xml, end, doc);
    v7_parse_fx_profiles(xml, end, doc);
    v7_parse_constraints(xml, end, doc);
    v7_parse_kinematics15(xml, end, doc);
    v7_parse_extras(xml, end, doc);
    p = xml;
    while (1) {
        p = v7_find_start(p, end, "instance_node", &tag_end);
        if (p == V7_NIL) break;
        doc->stats.instance_node_count++;
        p = tag_end;
    }
    return C3DAE_V7_OK;
}

const char *c3dae_v7_error_string(int code)
{
    switch (code) {
    case C3DAE_V7_OK: return "ok";
    case C3DAE_V7_ERR_NULL: return "null argument";
    case C3DAE_V7_ERR_CAPACITY: return "capacity exceeded";
    case C3DAE_V7_ERR_XML: return "xml parse error";
    default: return "unknown c3dae_v7 error";
    }
}
