#include "mf3d_ext_materials.h"
#include "mf3d_ext_xml.h"

extern int mf3d_parse_fix(const char *s, mf3d_fix *out);
extern int mf3d_parse_u32(const char *s, mf3d_u32 *out);

static void default_rgba(mf3d_ext_rgba *c)
{
    if (!c) return;
    c->r = 255; c->g = 255; c->b = 255; c->a = 255;
}

void mf3d_materials_init(mf3d_materials *m)
{
    mf3d_u32 i;
    if (!m) return;
    m->base_group_count = m->base_count = 0;
    m->color_group_count = m->color_count = 0;
    m->texture_count = m->tex_group_count = m->texcoord_count = 0;
    m->multiprop_count = 0;
    for (i = 0; i < MF3D_EXT_MAX_TEXTURES; ++i) m->textures[i].path[0] = 0;
}

static void parse_pid_list(const char *s, mf3d_multiprop *mp)
{
    char tmp[32];
    int n;
    const char *p;
    p = s;
    mp->layer_count = 0;
    while (p && *p && mp->layer_count < MF3D_EXT_MAX_MULTIPROP_LAYERS) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        n = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && n < 31) tmp[n++] = *p++;
        tmp[n] = 0;
        if (n > 0) {
            mf3d_parse_u32(tmp, &mp->pids[mp->layer_count]);
            mp->layer_count++;
        }
    }
}

int mf3d_materials_parse_xml(char *xml, mf3d_materials *out_mats)
{
    const char *p, *q, *end;
    char tmp[192];
    if (!xml || !out_mats) return MF3D_E_BADARG;
    mf3d_materials_init(out_mats);

    p = xml;
    while ((p = mf3d_ext_next_tag(p, "basematerials")) != 0) {
        mf3d_base_group *g;
        if (out_mats->base_group_count >= MF3D_EXT_MAX_BASEGROUPS) return MF3D_E_NOSPACE;
        g = &out_mats->base_groups[out_mats->base_group_count];
        g->id = g->displaypropertiesid = 0;
        g->first = out_mats->base_count;
        g->count = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &g->id);
        mf3d_ext_attr(p, "displaypropertiesid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &g->displaypropertiesid);
        end = mf3d_ext_find_close(p, "basematerials"); if (!end) return MF3D_E_XML;
        q = p;
        while ((q = mf3d_ext_next_tag(q, "base")) != 0 && q < end) {
            mf3d_base_material *b;
            if (out_mats->base_count >= MF3D_EXT_MAX_BASES) return MF3D_E_NOSPACE;
            b = &out_mats->bases[out_mats->base_count];
            b->name[0] = 0; default_rgba(&b->color);
            mf3d_ext_attr(q, "name", b->name, (int)sizeof(b->name));
            mf3d_ext_attr(q, "displaycolor", tmp, (int)sizeof(tmp));
            if (tmp[0]) mf3d_ext_parse_hex_color(tmp, &b->color.r, &b->color.g, &b->color.b, &b->color.a);
            out_mats->base_count++; g->count++;
            q++;
        }
        out_mats->base_group_count++;
        p = end + 1;
    }

    p = xml;
    while ((p = mf3d_ext_next_tag(p, "colorgroup")) != 0) {
        mf3d_color_group *g;
        if (out_mats->color_group_count >= MF3D_EXT_MAX_COLORGROUPS) return MF3D_E_NOSPACE;
        g = &out_mats->color_groups[out_mats->color_group_count];
        g->id = g->displaypropertiesid = 0;
        g->first = out_mats->color_count;
        g->count = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &g->id);
        mf3d_ext_attr(p, "displaypropertiesid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &g->displaypropertiesid);
        end = mf3d_ext_find_close(p, "colorgroup"); if (!end) return MF3D_E_XML;
        q = p;
        while ((q = mf3d_ext_next_tag(q, "color")) != 0 && q < end) {
            mf3d_ext_rgba *c;
            if (out_mats->color_count >= MF3D_EXT_MAX_COLORS) return MF3D_E_NOSPACE;
            c = &out_mats->colors[out_mats->color_count];
            default_rgba(c);
            mf3d_ext_attr(q, "color", tmp, (int)sizeof(tmp));
            if (tmp[0]) mf3d_ext_parse_hex_color(tmp, &c->r, &c->g, &c->b, &c->a);
            out_mats->color_count++; g->count++;
            q++;
        }
        out_mats->color_group_count++;
        p = end + 1;
    }

    p = xml;
    while ((p = mf3d_ext_next_tag(p, "texture2d")) != 0) {
        mf3d_texture2d *t;
        if (out_mats->texture_count >= MF3D_EXT_MAX_TEXTURES) return MF3D_E_NOSPACE;
        t = &out_mats->textures[out_mats->texture_count];
        t->id = 0; t->path[0] = t->contenttype[0] = t->tilestyleu[0] = t->tilestylev[0] = t->filter[0] = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &t->id);
        mf3d_ext_attr(p, "path", t->path, (int)sizeof(t->path));
        mf3d_ext_attr(p, "contenttype", t->contenttype, (int)sizeof(t->contenttype));
        mf3d_ext_attr(p, "tilestyleu", t->tilestyleu, (int)sizeof(t->tilestyleu));
        mf3d_ext_attr(p, "tilestylev", t->tilestylev, (int)sizeof(t->tilestylev));
        mf3d_ext_attr(p, "filter", t->filter, (int)sizeof(t->filter));
        out_mats->texture_count++;
        p++;
    }

    p = xml;
    while ((p = mf3d_ext_next_tag(p, "texture2dgroup")) != 0) {
        mf3d_texture2d_group *g;
        if (out_mats->tex_group_count >= MF3D_EXT_MAX_TEXGROUPS) return MF3D_E_NOSPACE;
        g = &out_mats->tex_groups[out_mats->tex_group_count];
        g->id = g->texid = g->displaypropertiesid = 0;
        g->first = out_mats->texcoord_count;
        g->count = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &g->id);
        mf3d_ext_attr(p, "texid", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &g->texid);
        mf3d_ext_attr(p, "displaypropertiesid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &g->displaypropertiesid);
        end = mf3d_ext_find_close(p, "texture2dgroup"); if (!end) return MF3D_E_XML;
        q = p;
        while ((q = mf3d_ext_next_tag(q, "tex2coord")) != 0 && q < end) {
            mf3d_texcoord *tc;
            if (out_mats->texcoord_count >= MF3D_EXT_MAX_TEXCOORDS) return MF3D_E_NOSPACE;
            tc = &out_mats->texcoords[out_mats->texcoord_count];
            tc->u = tc->v = 0;
            mf3d_ext_attr(q, "u", tmp, (int)sizeof(tmp)); mf3d_parse_fix(tmp, &tc->u);
            mf3d_ext_attr(q, "v", tmp, (int)sizeof(tmp)); mf3d_parse_fix(tmp, &tc->v);
            out_mats->texcoord_count++; g->count++;
            q++;
        }
        out_mats->tex_group_count++;
        p = end + 1;
    }

    p = xml;
    while ((p = mf3d_ext_next_tag(p, "multiproperties")) != 0) {
        mf3d_multiprop *mp;
        if (out_mats->multiprop_count >= MF3D_EXT_MAX_MULTIPROPS) return MF3D_E_NOSPACE;
        mp = &out_mats->multiprops[out_mats->multiprop_count];
        mp->id = 0; mp->layer_count = 0; mp->blendmethods[0] = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &mp->id);
        mf3d_ext_attr(p, "pids", tmp, (int)sizeof(tmp)); parse_pid_list(tmp, mp);
        mf3d_ext_attr(p, "blendmethods", mp->blendmethods, (int)sizeof(mp->blendmethods));
        out_mats->multiprop_count++;
        p++;
    }
    return MF3D_OK;
}

const mf3d_base_group *mf3d_materials_find_base_group(const mf3d_materials *m, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!m) return 0;
    for (i = 0; i < m->base_group_count; ++i) if (m->base_groups[i].id == id) return &m->base_groups[i];
    return 0;
}

const mf3d_color_group *mf3d_materials_find_color_group(const mf3d_materials *m, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!m) return 0;
    for (i = 0; i < m->color_group_count; ++i) if (m->color_groups[i].id == id) return &m->color_groups[i];
    return 0;
}

const mf3d_texture2d_group *mf3d_materials_find_tex_group(const mf3d_materials *m, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!m) return 0;
    for (i = 0; i < m->tex_group_count; ++i) if (m->tex_groups[i].id == id) return &m->tex_groups[i];
    return 0;
}

const mf3d_texture2d *mf3d_materials_find_texture(const mf3d_materials *m, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!m) return 0;
    for (i = 0; i < m->texture_count; ++i) if (m->textures[i].id == id) return &m->textures[i];
    return 0;
}

int mf3d_materials_triangle_color(const mf3d_materials *m, const mf3d_tri *tri, mf3d_ext_rgba *rgba)
{
    const mf3d_color_group *cg;
    const mf3d_base_group *bg;
    mf3d_u32 idx;
    if (!m || !tri || !rgba) return MF3D_E_BADARG;
    default_rgba(rgba);
    cg = mf3d_materials_find_color_group(m, tri->pid);
    if (cg) {
        idx = tri->p1;
        if (idx >= cg->count) return MF3D_E_RANGE;
        *rgba = m->colors[cg->first + idx];
        return MF3D_OK;
    }
    bg = mf3d_materials_find_base_group(m, tri->pid);
    if (bg) {
        idx = tri->p1;
        if (idx >= bg->count) return MF3D_E_RANGE;
        *rgba = m->bases[bg->first + idx].color;
        return MF3D_OK;
    }
    return MF3D_E_NOTFOUND;
}
