#include "mf3d_ext_volumetric.h"
#include "mf3d_ext_xml.h"

extern int mf3d_parse_fix(const char *s, mf3d_fix *out);
extern int mf3d_parse_u32(const char *s, mf3d_u32 *out);

static void scopy(char *dst, int cap, const char *src)
{
    int i;
    if (!dst || cap <= 0) return;
    dst[0] = 0;
    if (!src) return;
    for (i = 0; i < cap - 1 && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static int object_exists(const mf3d_model *m, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!m) return 0;
    for (i = 0; i < m->object_count; ++i) if (m->objects[i].id == id) return 1;
    return 0;
}

void mf3d_volumetric_init(mf3d_volumetric *v)
{
    if (!v) return;
    v->function_count = v->image_count = v->sheet_count = 0;
    v->levelset_count = v->voldata_count = v->node_count = 0;
}

static int parse_functions(char *xml, mf3d_volumetric *out_v)
{
    const char *p, *end;
    char tmp[192];
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "function")) != 0) {
        mf3d_vol_function *f;
        if (out_v->function_count >= MF3D_EXT_MAX_VOL_FUNCTIONS) return MF3D_E_NOSPACE;
        f = &out_v->functions[out_v->function_count];
        f->id = 0; f->image3did = 0; f->valueoffset = 0; f->valuescale = (mf3d_fix)(1L << 16);
        scopy(f->kind, (int)sizeof(f->kind), "function");
        f->displayname[0] = 0;
        scopy(f->filter, (int)sizeof(f->filter), "linear");
        scopy(f->tilestyleu, (int)sizeof(f->tilestyleu), "wrap");
        scopy(f->tilestylev, (int)sizeof(f->tilestylev), "wrap");
        scopy(f->tilestylew, (int)sizeof(f->tilestylew), "wrap");
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &f->id);
        mf3d_ext_attr(p, "displayname", tmp, (int)sizeof(tmp)); if (tmp[0]) scopy(f->displayname, (int)sizeof(f->displayname), tmp);
        end = mf3d_ext_find_close(p, "function");
        if (!end) return MF3D_E_XML;
        {
            const char *fi;
            fi = mf3d_ext_next_tag(p, "functionfromimage3d");
            if (fi && fi < end) {
                scopy(f->kind, (int)sizeof(f->kind), "functionfromimage3d");
                mf3d_ext_attr(fi, "image3did", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &f->image3did);
                mf3d_ext_attr(fi, "valueoffset", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &f->valueoffset);
                mf3d_ext_attr(fi, "valuescale", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &f->valuescale);
                mf3d_ext_attr(fi, "filter", tmp, (int)sizeof(tmp)); if (tmp[0]) scopy(f->filter, (int)sizeof(f->filter), tmp);
                mf3d_ext_attr(fi, "tilestyleu", tmp, (int)sizeof(tmp)); if (tmp[0]) scopy(f->tilestyleu, (int)sizeof(f->tilestyleu), tmp);
                mf3d_ext_attr(fi, "tilestylev", tmp, (int)sizeof(tmp)); if (tmp[0]) scopy(f->tilestylev, (int)sizeof(f->tilestylev), tmp);
                mf3d_ext_attr(fi, "tilestylew", tmp, (int)sizeof(tmp)); if (tmp[0]) scopy(f->tilestylew, (int)sizeof(f->tilestylew), tmp);
            }
            fi = mf3d_ext_next_tag(p, "implicitfunction");
            if (fi && fi < end) scopy(f->kind, (int)sizeof(f->kind), "implicitfunction");
            fi = mf3d_ext_next_tag(p, "privateextensionfunction");
            if (fi && fi < end) scopy(f->kind, (int)sizeof(f->kind), "privateextensionfunction");
        }
        out_v->function_count++;
        p = end + 1;
    }
    return MF3D_OK;
}

static int parse_images(char *xml, mf3d_volumetric *out_v)
{
    const char *p, *end, *st, *st_end, *q;
    char tmp[192];
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "image3d")) != 0) {
        mf3d_image3d *im;
        if (out_v->image_count >= MF3D_EXT_MAX_IMAGE3D) return MF3D_E_NOSPACE;
        im = &out_v->images[out_v->image_count];
        im->id = 0; im->name[0] = 0;
        im->rowcount = im->columncount = im->sheetcount = 0;
        im->first_sheet = out_v->sheet_count; im->sheet_path_count = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &im->id);
        mf3d_ext_attr(p, "name", tmp, (int)sizeof(tmp)); if (tmp[0]) scopy(im->name, (int)sizeof(im->name), tmp);
        end = mf3d_ext_find_close(p, "image3d");
        if (!end) return MF3D_E_XML;
        st = mf3d_ext_next_tag(p, "imagestack");
        if (st && st < end) {
            mf3d_ext_attr(st, "rowcount", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &im->rowcount);
            mf3d_ext_attr(st, "columncount", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &im->columncount);
            mf3d_ext_attr(st, "sheetcount", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &im->sheetcount);
            st_end = mf3d_ext_find_close(st, "imagestack");
            if (!st_end) return MF3D_E_XML;
            q = st;
            while ((q = mf3d_ext_next_tag(q, "imagesheet")) != 0 && q < st_end) {
                if (out_v->sheet_count >= MF3D_EXT_MAX_IMAGESHEETS) return MF3D_E_NOSPACE;
                mf3d_ext_attr(q, "path", tmp, (int)sizeof(tmp));
                scopy(out_v->sheets[out_v->sheet_count].path, (int)sizeof(out_v->sheets[out_v->sheet_count].path), tmp);
                out_v->sheet_count++; im->sheet_path_count++;
                q++;
            }
        }
        out_v->image_count++;
        p = end + 1;
    }
    return MF3D_OK;
}

static int parse_levelsets(char *xml, mf3d_volumetric *out_v)
{
    const char *p, *obj_end, *ls;
    char tmp[192];
    mf3d_u32 owner;
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "object")) != 0) {
        owner = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &owner);
        obj_end = mf3d_ext_find_close(p, "object");
        if (!obj_end) return MF3D_E_XML;
        ls = mf3d_ext_next_tag(p, "levelset");
        if (ls && ls < obj_end) {
            mf3d_levelset *l;
            if (out_v->levelset_count >= MF3D_EXT_MAX_LEVELSETS) return MF3D_E_NOSPACE;
            l = &out_v->levelsets[out_v->levelset_count];
            l->owner_objectid = owner; l->functionid = 0; l->meshid = 0; l->meshbboxonly = 0; l->minfeaturesize = 0;
            l->channel[0] = 0; mf3d_ext_identity(l->transform);
            mf3d_ext_attr(ls, "functionid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &l->functionid);
            mf3d_ext_attr(ls, "meshid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &l->meshid);
            mf3d_ext_attr(ls, "channel", tmp, (int)sizeof(tmp)); if (tmp[0]) scopy(l->channel, (int)sizeof(l->channel), tmp);
            mf3d_ext_attr(ls, "transform", tmp, (int)sizeof(tmp)); mf3d_ext_parse_transform(tmp, l->transform);
            mf3d_ext_attr(ls, "minfeaturesize", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &l->minfeaturesize);
            mf3d_ext_attr(ls, "meshbboxonly", tmp, (int)sizeof(tmp)); if (tmp[0] == 't' || tmp[0] == '1') l->meshbboxonly = 1;
            out_v->levelset_count++;
        }
        p = obj_end + 1;
    }
    return MF3D_OK;
}

static int parse_voldata(char *xml, mf3d_volumetric *out_v)
{
    const char *p;
    char tmp[192];
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "volumetricdata")) != 0) {
        mf3d_volumetricdata *d;
        if (out_v->voldata_count >= MF3D_EXT_MAX_VOLDATA) return MF3D_E_NOSPACE;
        d = &out_v->voldata[out_v->voldata_count];
        d->objectid = d->functionid = d->propertyid = 0;
        d->channel[0] = 0; d->datatype[0] = 0; mf3d_ext_identity(d->transform);
        mf3d_ext_attr(p, "objectid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &d->objectid);
        mf3d_ext_attr(p, "functionid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &d->functionid);
        mf3d_ext_attr(p, "pid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &d->propertyid);
        mf3d_ext_attr(p, "channel", tmp, (int)sizeof(tmp)); if (tmp[0]) scopy(d->channel, (int)sizeof(d->channel), tmp);
        mf3d_ext_attr(p, "datatype", tmp, (int)sizeof(tmp)); if (tmp[0]) scopy(d->datatype, (int)sizeof(d->datatype), tmp);
        mf3d_ext_attr(p, "transform", tmp, (int)sizeof(tmp)); mf3d_ext_parse_transform(tmp, d->transform);
        out_v->voldata_count++;
        p++;
    }
    return MF3D_OK;
}

static int parse_implicit_nodes(char *xml, mf3d_volumetric *out_v)
{
    const char *p, *end, *q;
    char tagname[40], tmp[96];
    mf3d_u32 funcid, idx;
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "implicitfunction")) != 0) {
        funcid = 0; idx = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &funcid);
        end = mf3d_ext_find_close(p, "implicitfunction");
        if (!end) return MF3D_E_XML;
        q = p;
        while (q < end && *q) {
            if (*q == '<' && q[1] != '/' && q[1] != '!' && q[1] != '?') {
                int n, s;
                const char *r;
                r = q + 1;
                s = 0;
                while (r[s] && r[s] != ':' && r[s] != '>' && r[s] != '/' && r[s] != ' ' && r[s] != '\t' && r[s] != '\r' && r[s] != '\n') s++;
                if (r[s] == ':') r += s + 1;
                n = 0;
                while (r[n] && r[n] != '>' && r[n] != '/' && r[n] != ' ' && r[n] != '\t' && r[n] != '\r' && r[n] != '\n' && n < 39) { tagname[n] = r[n]; n++; }
                tagname[n] = 0;
                if (n > 0 && !mf3d_ext_tag_matches(q, "implicitfunction")) {
                    mf3d_implicit_node *node;
                    if (out_v->node_count >= MF3D_EXT_MAX_IMPLICIT_NODES) return MF3D_E_NOSPACE;
                    node = &out_v->nodes[out_v->node_count];
                    node->functionid = funcid; node->node_index = idx++;
                    scopy(node->tag, (int)sizeof(node->tag), tagname);
                    node->id[0] = node->op[0] = node->output[0] = node->input[0] = 0;
                    mf3d_ext_attr(q, "id", tmp, (int)sizeof(tmp)); scopy(node->id, (int)sizeof(node->id), tmp);
                    mf3d_ext_attr(q, "op", tmp, (int)sizeof(tmp)); scopy(node->op, (int)sizeof(node->op), tmp);
                    mf3d_ext_attr(q, "output", tmp, (int)sizeof(tmp)); scopy(node->output, (int)sizeof(node->output), tmp);
                    mf3d_ext_attr(q, "input", tmp, (int)sizeof(tmp)); scopy(node->input, (int)sizeof(node->input), tmp);
                    out_v->node_count++;
                }
            }
            q++;
        }
        p = end + 1;
    }
    return MF3D_OK;
}

int mf3d_volumetric_parse_xml(char *xml, mf3d_volumetric *out_v)
{
    int r;
    if (!xml || !out_v) return MF3D_E_BADARG;
    mf3d_volumetric_init(out_v);
    r = parse_images(xml, out_v); if (r != MF3D_OK) return r;
    r = parse_functions(xml, out_v); if (r != MF3D_OK) return r;
    r = parse_levelsets(xml, out_v); if (r != MF3D_OK) return r;
    r = parse_voldata(xml, out_v); if (r != MF3D_OK) return r;
    r = parse_implicit_nodes(xml, out_v); if (r != MF3D_OK) return r;
    return MF3D_OK;
}

const mf3d_vol_function *mf3d_volumetric_find_function(const mf3d_volumetric *v, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!v) return 0;
    for (i = 0; i < v->function_count; ++i) if (v->functions[i].id == id) return &v->functions[i];
    return 0;
}

const mf3d_image3d *mf3d_volumetric_find_image3d(const mf3d_volumetric *v, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!v) return 0;
    for (i = 0; i < v->image_count; ++i) if (v->images[i].id == id) return &v->images[i];
    return 0;
}

int mf3d_volumetric_validate_refs(const mf3d_volumetric *v, const mf3d_model *m, mf3d_u32 *missing_refs)
{
    mf3d_u32 i, miss;
    if (!v) return MF3D_E_BADARG;
    miss = 0;
    for (i = 0; i < v->function_count; ++i) if (v->functions[i].image3did && !mf3d_volumetric_find_image3d(v, v->functions[i].image3did)) miss++;
    for (i = 0; i < v->levelset_count; ++i) {
        if (v->levelsets[i].functionid && !mf3d_volumetric_find_function(v, v->levelsets[i].functionid)) miss++;
        if (m && v->levelsets[i].meshid && !object_exists(m, v->levelsets[i].meshid)) miss++;
    }
    for (i = 0; i < v->voldata_count; ++i) {
        if (v->voldata[i].functionid && !mf3d_volumetric_find_function(v, v->voldata[i].functionid)) miss++;
        if (m && v->voldata[i].objectid && !object_exists(m, v->voldata[i].objectid)) miss++;
    }
    if (missing_refs) *missing_refs = miss;
    return miss ? MF3D_E_NOTFOUND : MF3D_OK;
}
