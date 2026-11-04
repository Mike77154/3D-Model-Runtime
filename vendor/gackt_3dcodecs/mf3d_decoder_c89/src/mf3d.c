#include "mf3d.h"

/* private ZIP API */
typedef struct mf3d_zip_entry_s { char name[256]; mf3d_u32 method, comp_size, uncomp_size, local_off, data_off; } mf3d_zip_entry;
typedef struct mf3d_zip_s { mf3d_zip_entry entries[MF3D_MAX_ZIP_ENTRIES]; mf3d_u32 count; } mf3d_zip;
extern int mf3d_zip_read(const mf3d_u8 *data, mf3d_u32 size, mf3d_zip *zip);
extern int mf3d_zip_find(mf3d_zip *zip, const char *name);
extern int mf3d_zip_find_first_model(mf3d_zip *zip);
extern int mf3d_zip_extract(const mf3d_u8 *data, mf3d_u32 size, mf3d_zip *zip, int idx, mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len, mf3d_options *opt);
extern int mf3d_parse_model_xml(char *xml, mf3d_model *m);

void mf3d_model_init(mf3d_model *m)
{
    mf3d_u32 i;
    if (!m) return;
    m->vertex_count = m->triangle_count = m->object_count = m->builditem_count = 0;
    m->unit[0] = 0; m->lang[0] = 0;
    for (i = 0; i < MF3D_MAX_OBJECTS; ++i) { m->objects[i].id = 0; m->objects[i].name[0] = 0; m->objects[i].type[0] = 0; }
}

int mf3d_decode(const mf3d_u8 *data, mf3d_u32 size, mf3d_model *out_model, mf3d_options *opt)
{
    mf3d_zip zip;
    int err, idx;
    mf3d_u32 xml_len;
    if (!data || !out_model || !opt || !opt->scratch || opt->scratch_size < 4) return MF3D_E_BADARG;
    mf3d_model_init(out_model);
    err = mf3d_zip_read(data, size, &zip); if (err != MF3D_OK) return err;
    idx = mf3d_zip_find_first_model(&zip); if (idx < 0) return MF3D_E_NOTFOUND;
    if (zip.entries[idx].uncomp_size + 1UL > opt->scratch_size) return MF3D_E_NOSPACE;
    err = mf3d_zip_extract(data, size, &zip, idx, opt->scratch, opt->scratch_size, &xml_len, opt);
    if (err != MF3D_OK) return err;
    opt->scratch[xml_len] = 0;
    return mf3d_parse_model_xml((char*)opt->scratch, out_model);
}

const char *mf3d_error_string(int err)
{
    switch (err) {
    case MF3D_OK: return "ok";
    case MF3D_E_BADARG: return "bad argument";
    case MF3D_E_ZIP: return "bad or unsupported zip/opc package";
    case MF3D_E_NOTFOUND: return "3mf model part not found";
    case MF3D_E_NOSPACE: return "static capacity exhausted";
    case MF3D_E_UNSUPPORTED: return "unsupported compression or feature; provide inflate callback for deflate";
    case MF3D_E_XML: return "xml parse error";
    case MF3D_E_INFLATE: return "inflate error";
    case MF3D_E_RANGE: return "range error";
    default: return "unknown error";
    }
}
