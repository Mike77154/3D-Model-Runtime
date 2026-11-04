#include "mf3d_ext_production.h"
#include "mf3d_ext_xml.h"

extern int mf3d_parse_u32(const char *s, mf3d_u32 *out);

void mf3d_production_init(mf3d_production *p)
{
    if (!p) return;
    p->build.uuid[0] = 0;
    p->item_count = p->object_count = p->component_count = 0;
}

int mf3d_production_parse_xml(char *xml, mf3d_production *out_prod)
{
    const char *p, *end, *q;
    char tmp[192];
    mf3d_u32 parent_id;
    if (!xml || !out_prod) return MF3D_E_BADARG;
    mf3d_production_init(out_prod);

    p = mf3d_ext_next_tag(xml, "build");
    if (p) mf3d_ext_attr(p, "UUID", out_prod->build.uuid, (int)sizeof(out_prod->build.uuid));
    if (p && !out_prod->build.uuid[0]) mf3d_ext_attr(p, "uuid", out_prod->build.uuid, (int)sizeof(out_prod->build.uuid));

    p = xml;
    while ((p = mf3d_ext_next_tag(p, "item")) != 0) {
        mf3d_prod_item *it;
        if (out_prod->item_count >= MF3D_EXT_MAX_PROD_ITEMS) return MF3D_E_NOSPACE;
        it = &out_prod->items[out_prod->item_count];
        it->objectid = 0; it->uuid[0] = 0; it->path[0] = 0;
        mf3d_ext_attr(p, "objectid", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &it->objectid);
        mf3d_ext_attr(p, "path", it->path, (int)sizeof(it->path));
        mf3d_ext_attr(p, "UUID", it->uuid, (int)sizeof(it->uuid));
        if (!it->uuid[0]) mf3d_ext_attr(p, "uuid", it->uuid, (int)sizeof(it->uuid));
        out_prod->item_count++;
        p++;
    }

    p = xml;
    while ((p = mf3d_ext_next_tag(p, "object")) != 0) {
        mf3d_prod_object *ob;
        if (out_prod->object_count >= MF3D_EXT_MAX_PROD_OBJECTS) return MF3D_E_NOSPACE;
        ob = &out_prod->objects[out_prod->object_count];
        ob->objectid = 0; ob->uuid[0] = 0; ob->modelresolution[0] = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &ob->objectid);
        mf3d_ext_attr(p, "UUID", ob->uuid, (int)sizeof(ob->uuid));
        if (!ob->uuid[0]) mf3d_ext_attr(p, "uuid", ob->uuid, (int)sizeof(ob->uuid));
        mf3d_ext_attr(p, "modelresolution", ob->modelresolution, (int)sizeof(ob->modelresolution));
        out_prod->object_count++;

        parent_id = ob->objectid;
        end = mf3d_ext_find_close(p, "object");
        if (!end) return MF3D_E_XML;
        q = p;
        while ((q = mf3d_ext_next_tag(q, "component")) != 0 && q < end) {
            mf3d_prod_component *co;
            if (out_prod->component_count >= MF3D_EXT_MAX_PROD_COMPONENTS) return MF3D_E_NOSPACE;
            co = &out_prod->components[out_prod->component_count];
            co->parent_objectid = parent_id; co->objectid = 0; co->uuid[0] = 0; co->path[0] = 0;
            mf3d_ext_attr(q, "objectid", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &co->objectid);
            mf3d_ext_attr(q, "path", co->path, (int)sizeof(co->path));
            mf3d_ext_attr(q, "UUID", co->uuid, (int)sizeof(co->uuid));
            if (!co->uuid[0]) mf3d_ext_attr(q, "uuid", co->uuid, (int)sizeof(co->uuid));
            out_prod->component_count++;
            q++;
        }
        p = end + 1;
    }
    return MF3D_OK;
}

const char *mf3d_production_item_path(const mf3d_production *p, mf3d_u32 objectid)
{
    mf3d_u32 i;
    if (!p) return 0;
    for (i = 0; i < p->item_count; ++i) {
        if (p->items[i].objectid == objectid && p->items[i].path[0]) return p->items[i].path;
    }
    return 0;
}
