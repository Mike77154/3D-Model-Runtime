#include "mf3d_ext_displacement.h"
#include "mf3d_ext_xml.h"

extern int mf3d_parse_fix(const char *s, mf3d_fix *out);
extern int mf3d_parse_u32(const char *s, mf3d_u32 *out);

static void str_copy(char *dst, int cap, const char *src)
{
    int i;
    if (!dst || cap <= 0) return;
    dst[0] = 0;
    if (!src) return;
    for (i = 0; i < cap - 1 && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}

void mf3d_displacement_init(mf3d_displacement *d)
{
    if (!d) return;
    d->disp2d_count = d->normgroup_count = d->normvec_count = 0;
    d->dispgroup_count = d->coord_count = d->mesh_count = d->disptri_count = 0;
}

static int parse_disp2d(char *xml, mf3d_displacement *out_d)
{
    const char *p;
    char tmp[192];
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "displacement2d")) != 0) {
        mf3d_disp2d *r;
        if (out_d->disp2d_count >= MF3D_EXT_MAX_DISP2D) return MF3D_E_NOSPACE;
        r = &out_d->disp2d[out_d->disp2d_count];
        r->id = 0;
        str_copy(r->channel, (int)sizeof(r->channel), "G");
        str_copy(r->tilestyleu, (int)sizeof(r->tilestyleu), "wrap");
        str_copy(r->tilestylev, (int)sizeof(r->tilestylev), "wrap");
        str_copy(r->filter, (int)sizeof(r->filter), "auto");
        r->path[0] = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &r->id);
        mf3d_ext_attr(p, "path", tmp, (int)sizeof(tmp)); str_copy(r->path, (int)sizeof(r->path), tmp);
        mf3d_ext_attr(p, "channel", tmp, (int)sizeof(tmp)); if (tmp[0]) str_copy(r->channel, (int)sizeof(r->channel), tmp);
        mf3d_ext_attr(p, "tilestyleu", tmp, (int)sizeof(tmp)); if (tmp[0]) str_copy(r->tilestyleu, (int)sizeof(r->tilestyleu), tmp);
        mf3d_ext_attr(p, "tilestylev", tmp, (int)sizeof(tmp)); if (tmp[0]) str_copy(r->tilestylev, (int)sizeof(r->tilestylev), tmp);
        mf3d_ext_attr(p, "filter", tmp, (int)sizeof(tmp)); if (tmp[0]) str_copy(r->filter, (int)sizeof(r->filter), tmp);
        out_d->disp2d_count++;
        p++;
    }
    return MF3D_OK;
}

static int parse_normgroups(char *xml, mf3d_displacement *out_d)
{
    const char *p, *end, *q;
    char tmp[96];
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "normvectorgroup")) != 0) {
        mf3d_normgroup *g;
        if (out_d->normgroup_count >= MF3D_EXT_MAX_NORMGROUPS) return MF3D_E_NOSPACE;
        g = &out_d->normgroups[out_d->normgroup_count];
        g->id = 0; g->first = out_d->normvec_count; g->count = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &g->id);
        end = mf3d_ext_find_close(p, "normvectorgroup");
        if (!end) return MF3D_E_XML;
        q = p;
        while ((q = mf3d_ext_next_tag(q, "normvector")) != 0 && q < end) {
            mf3d_normvec *v;
            if (out_d->normvec_count >= MF3D_EXT_MAX_NORMVECS) return MF3D_E_NOSPACE;
            v = &out_d->normvecs[out_d->normvec_count];
            v->x = v->y = v->z = 0;
            mf3d_ext_attr(q, "x", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &v->x);
            mf3d_ext_attr(q, "y", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &v->y);
            mf3d_ext_attr(q, "z", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &v->z);
            out_d->normvec_count++; g->count++;
            q++;
        }
        out_d->normgroup_count++;
        p = end + 1;
    }
    return MF3D_OK;
}

static int parse_dispgroups(char *xml, mf3d_displacement *out_d)
{
    const char *p, *end, *q;
    char tmp[96];
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "disp2dgroup")) != 0) {
        mf3d_dispgroup *g;
        if (out_d->dispgroup_count >= MF3D_EXT_MAX_DISPGROUPS) return MF3D_E_NOSPACE;
        g = &out_d->dispgroups[out_d->dispgroup_count];
        g->id = g->dispid = g->nid = 0;
        g->height = 0; g->offset = 0;
        g->first = out_d->coord_count; g->count = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &g->id);
        mf3d_ext_attr(p, "dispid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &g->dispid);
        mf3d_ext_attr(p, "nid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &g->nid);
        mf3d_ext_attr(p, "height", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &g->height);
        mf3d_ext_attr(p, "offset", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &g->offset);
        end = mf3d_ext_find_close(p, "disp2dgroup");
        if (!end) return MF3D_E_XML;
        q = p;
        while ((q = mf3d_ext_next_tag(q, "disp2dcoords")) != 0 && q < end) {
            mf3d_dispcoord *c;
            if (out_d->coord_count >= MF3D_EXT_MAX_DISPCOORDS) return MF3D_E_NOSPACE;
            c = &out_d->coords[out_d->coord_count];
            c->u = c->v = 0; c->n = 0; c->f = (mf3d_fix)(1L << 16);
            mf3d_ext_attr(q, "u", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &c->u);
            mf3d_ext_attr(q, "v", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &c->v);
            mf3d_ext_attr(q, "n", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &c->n);
            mf3d_ext_attr(q, "f", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &c->f);
            out_d->coord_count++; g->count++;
            q++;
        }
        out_d->dispgroup_count++;
        p = end + 1;
    }
    return MF3D_OK;
}

static int parse_dispmeshes(char *xml, mf3d_displacement *out_d)
{
    const char *p, *obj_end, *dm, *dm_end, *tris, *tris_end, *q;
    char tmp[96];
    mf3d_u32 owner;
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "object")) != 0) {
        owner = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &owner);
        obj_end = mf3d_ext_find_close(p, "object");
        if (!obj_end) return MF3D_E_XML;
        dm = mf3d_ext_next_tag(p, "displacementmesh");
        if (dm && dm < obj_end) {
            mf3d_dispmesh *m;
            if (out_d->mesh_count >= MF3D_EXT_MAX_DISPMESHES) return MF3D_E_NOSPACE;
            m = &out_d->meshes[out_d->mesh_count];
            m->objectid = owner;
            m->default_did = 0;
            m->first_disptri = out_d->disptri_count;
            m->disptri_count = 0;
            dm_end = mf3d_ext_find_close(dm, "displacementmesh");
            if (!dm_end) return MF3D_E_XML;
            tris = mf3d_ext_next_tag(dm, "triangles");
            if (tris && tris < dm_end) {
                mf3d_ext_attr(tris, "did", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &m->default_did);
                tris_end = mf3d_ext_find_close(tris, "triangles");
                if (!tris_end) return MF3D_E_XML;
                q = tris;
                while ((q = mf3d_ext_next_tag(q, "triangle")) != 0 && q < tris_end) {
                    mf3d_disptri *t;
                    if (out_d->disptri_count >= MF3D_EXT_MAX_DISPTRIS) return MF3D_E_NOSPACE;
                    t = &out_d->disptris[out_d->disptri_count];
                    t->owner_objectid = owner;
                    t->tri_index = m->disptri_count;
                    t->did = m->default_did;
                    t->d1 = t->d2 = t->d3 = 0;
                    t->has_d = 0;
                    mf3d_ext_attr(q, "did", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &t->did);
                    mf3d_ext_attr(q, "d1", tmp, (int)sizeof(tmp)); if (tmp[0]) { mf3d_parse_u32(tmp, &t->d1); t->has_d = 1; }
                    mf3d_ext_attr(q, "d2", tmp, (int)sizeof(tmp)); if (tmp[0]) { mf3d_parse_u32(tmp, &t->d2); t->has_d = 1; }
                    mf3d_ext_attr(q, "d3", tmp, (int)sizeof(tmp)); if (tmp[0]) { mf3d_parse_u32(tmp, &t->d3); t->has_d = 1; }
                    out_d->disptri_count++; m->disptri_count++;
                    q++;
                }
            }
            out_d->mesh_count++;
        }
        p = obj_end + 1;
    }
    return MF3D_OK;
}

int mf3d_displacement_parse_xml(char *xml, mf3d_displacement *out_d)
{
    int r;
    if (!xml || !out_d) return MF3D_E_BADARG;
    mf3d_displacement_init(out_d);
    r = parse_disp2d(xml, out_d); if (r != MF3D_OK) return r;
    r = parse_normgroups(xml, out_d); if (r != MF3D_OK) return r;
    r = parse_dispgroups(xml, out_d); if (r != MF3D_OK) return r;
    r = parse_dispmeshes(xml, out_d); if (r != MF3D_OK) return r;
    return MF3D_OK;
}

const mf3d_disp2d *mf3d_displacement_find_disp2d(const mf3d_displacement *d, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!d) return 0;
    for (i = 0; i < d->disp2d_count; ++i) if (d->disp2d[i].id == id) return &d->disp2d[i];
    return 0;
}

const mf3d_dispgroup *mf3d_displacement_find_group(const mf3d_displacement *d, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!d) return 0;
    for (i = 0; i < d->dispgroup_count; ++i) if (d->dispgroups[i].id == id) return &d->dispgroups[i];
    return 0;
}

static int has_normgroup(const mf3d_displacement *d, mf3d_u32 id)
{
    mf3d_u32 i;
    for (i = 0; i < d->normgroup_count; ++i) if (d->normgroups[i].id == id) return 1;
    return 0;
}

int mf3d_displacement_validate_refs(const mf3d_displacement *d, mf3d_u32 *missing_refs)
{
    mf3d_u32 i, miss;
    if (!d) return MF3D_E_BADARG;
    miss = 0;
    for (i = 0; i < d->dispgroup_count; ++i) {
        if (!mf3d_displacement_find_disp2d(d, d->dispgroups[i].dispid)) miss++;
        if (!has_normgroup(d, d->dispgroups[i].nid)) miss++;
    }
    for (i = 0; i < d->disptri_count; ++i) {
        if (d->disptris[i].did && !mf3d_displacement_find_group(d, d->disptris[i].did)) miss++;
    }
    if (missing_refs) *missing_refs = miss;
    return miss ? MF3D_E_NOTFOUND : MF3D_OK;
}
