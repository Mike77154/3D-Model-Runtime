#include "mf3d_ext_opc.h"
#include "mf3d_ext_xml.h"

static int contains(const char *s, const char *needle)
{
    const char *p, *q, *r;
    if (!s || !needle) return 0;
    for (p = s; *p; ++p) {
        q = p; r = needle;
        while (*q && *r && *q == *r) { q++; r++; }
        if (!*r) return 1;
    }
    return 0;
}

static int streq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

void mf3d_opc_rels_init(mf3d_opc_rels *r)
{
    if (!r) return;
    r->count = 0;
}

int mf3d_opc_rels_parse_xml(char *xml, mf3d_opc_rels *out_rels)
{
    const char *p;
    if (!xml || !out_rels) return MF3D_E_BADARG;
    mf3d_opc_rels_init(out_rels);
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "Relationship")) != 0) {
        mf3d_opc_rel *r;
        if (out_rels->count >= MF3D_EXT_MAX_RELS) return MF3D_E_NOSPACE;
        r = &out_rels->rels[out_rels->count];
        r->id[0] = r->type[0] = r->target[0] = 0;
        mf3d_ext_attr(p, "Id", r->id, (int)sizeof(r->id));
        mf3d_ext_attr(p, "Type", r->type, (int)sizeof(r->type));
        mf3d_ext_attr(p, "Target", r->target, (int)sizeof(r->target));
        out_rels->count++;
        p++;
    }
    return MF3D_OK;
}

const char *mf3d_opc_find_start_part(const mf3d_opc_rels *r)
{
    mf3d_u32 i;
    if (!r) return 0;
    for (i = 0; i < r->count; ++i) {
        if (contains(r->rels[i].type, "3dmodel") || contains(r->rels[i].type, "3DModel") || contains(r->rels[i].type, "startpart")) return r->rels[i].target;
    }
    return 0;
}

void mf3d_opc_content_types_init(mf3d_opc_content_types *ct)
{
    if (!ct) return;
    ct->count = 0;
}

int mf3d_opc_content_types_parse_xml(char *xml, mf3d_opc_content_types *out_ct)
{
    const char *p;
    if (!xml || !out_ct) return MF3D_E_BADARG;
    mf3d_opc_content_types_init(out_ct);
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "Override")) != 0) {
        mf3d_opc_content_type *it;
        if (out_ct->count >= MF3D_EXT_MAX_CONTENT_TYPES) return MF3D_E_NOSPACE;
        it = &out_ct->items[out_ct->count];
        it->part_name[0] = it->extension[0] = it->content_type[0] = 0;
        mf3d_ext_attr(p, "PartName", it->part_name, (int)sizeof(it->part_name));
        mf3d_ext_attr(p, "ContentType", it->content_type, (int)sizeof(it->content_type));
        out_ct->count++;
        p++;
    }
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "Default")) != 0) {
        mf3d_opc_content_type *it;
        if (out_ct->count >= MF3D_EXT_MAX_CONTENT_TYPES) return MF3D_E_NOSPACE;
        it = &out_ct->items[out_ct->count];
        it->part_name[0] = it->extension[0] = it->content_type[0] = 0;
        mf3d_ext_attr(p, "Extension", it->extension, (int)sizeof(it->extension));
        mf3d_ext_attr(p, "ContentType", it->content_type, (int)sizeof(it->content_type));
        out_ct->count++;
        p++;
    }
    return MF3D_OK;
}

const char *mf3d_opc_content_type_for_part(const mf3d_opc_content_types *ct, const char *part_name)
{
    mf3d_u32 i;
    const char *dot;
    if (!ct || !part_name) return 0;
    for (i = 0; i < ct->count; ++i) {
        if (ct->items[i].part_name[0] && streq(ct->items[i].part_name, part_name)) return ct->items[i].content_type;
    }
    dot = part_name;
    while (*dot) dot++;
    while (dot > part_name && dot[-1] != '.') dot--;
    for (i = 0; i < ct->count; ++i) {
        if (ct->items[i].extension[0] && streq(ct->items[i].extension, dot)) return ct->items[i].content_type;
    }
    return 0;
}
