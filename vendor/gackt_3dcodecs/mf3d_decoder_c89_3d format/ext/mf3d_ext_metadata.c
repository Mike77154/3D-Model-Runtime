#include "mf3d_ext_metadata.h"
#include "mf3d_ext_xml.h"

static int streq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

void mf3d_metadata_init(mf3d_metadata *m)
{
    if (!m) return;
    m->count = 0;
}

int mf3d_metadata_parse_xml(char *xml, mf3d_metadata *out_meta)
{
    const char *p, *end;
    if (!xml || !out_meta) return MF3D_E_BADARG;
    mf3d_metadata_init(out_meta);
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "metadata")) != 0) {
        mf3d_metadata_item *it;
        if (out_meta->count >= MF3D_EXT_MAX_METADATA) return MF3D_E_NOSPACE;
        end = mf3d_ext_find_close(p, "metadata");
        if (!end) return MF3D_E_XML;
        it = &out_meta->items[out_meta->count];
        it->name[0] = it->value[0] = 0;
        mf3d_ext_attr(p, "name", it->name, (int)sizeof(it->name));
        mf3d_ext_text_between(p, end, it->value, (int)sizeof(it->value));
        out_meta->count++;
        p = end + 1;
    }
    return MF3D_OK;
}

const char *mf3d_metadata_find(const mf3d_metadata *m, const char *name)
{
    mf3d_u32 i;
    if (!m || !name) return 0;
    for (i = 0; i < m->count; ++i) if (streq(m->items[i].name, name)) return m->items[i].value;
    return 0;
}
