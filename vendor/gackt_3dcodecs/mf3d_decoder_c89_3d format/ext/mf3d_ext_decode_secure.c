#include "mf3d_ext_decode_secure.h"
#include "mf3d_ext_opc.h"

extern int mf3d_parse_model_xml(char *xml, mf3d_model *m);

int mf3d_decode_secure_zip_model_xml(const mf3d_u8 *data, mf3d_u32 size,
                                     mf3d_secure_decode_options *opt,
                                     mf3d_u8 *xml_out, mf3d_u32 xml_cap,
                                     mf3d_u32 *xml_len)
{
    mf3d_z64_archive zip;
    mf3d_zss_options zss;
    mf3d_zca_options zca;
    int r, idx, rel_idx;
    const char *start;
    char start_name[160];
    mf3d_u32 i;
    mf3d_opc_rels rels;
    if (!data || !opt || !xml_out || !xml_len || !opt->compressed_scratch) return MF3D_E_BADARG;
    r = mf3d_zip64_read(data, size, &zip);
    if (r != MF3D_OK) return r;
    mf3d_zca_make_zss_options(&zss, &zca, opt->password, opt->compressed_scratch, opt->compressed_scratch_size);
    zca.verify_auth_code = opt->verify_winzip_aes_auth ? 1UL : 0UL;

    idx = -1;
    rel_idx = mf3d_zip64_find(&zip, "_rels/.rels");
    if (rel_idx >= 0 && xml_cap > 1UL) {
        r = mf3d_zss_extract(data, size, &zip, rel_idx, &zss, xml_out, xml_cap - 1UL, xml_len);
        if (r == MF3D_OK) {
            xml_out[*xml_len] = 0;
            if (mf3d_opc_rels_parse_xml((char*)xml_out, &rels) == MF3D_OK) {
                start = mf3d_opc_find_start_part(&rels);
                if (start) {
                    if (*start == '/') start++;
                    i = 0;
                    while (start[i] && i + 1UL < sizeof(start_name)) { start_name[i] = start[i]; ++i; }
                    start_name[i] = 0;
                    idx = mf3d_zip64_find(&zip, start_name);
                }
            }
        }
    }
    if (idx < 0) idx = mf3d_zip64_find_first_model(&zip);
    if (idx < 0) return MF3D_E_NOTFOUND;
    return mf3d_zss_extract(data, size, &zip, idx, &zss, xml_out, xml_cap, xml_len);
}

int mf3d_decode_secure_zip(const mf3d_u8 *data, mf3d_u32 size,
                           mf3d_model *out_model,
                           mf3d_secure_decode_options *opt)
{
    mf3d_u32 xml_len;
    int r;
    if (!data || !out_model || !opt || !opt->xml_scratch || opt->xml_scratch_size < 4) return MF3D_E_BADARG;
    mf3d_model_init(out_model);
    r = mf3d_decode_secure_zip_model_xml(data, size, opt,
                                         opt->xml_scratch, opt->xml_scratch_size - 1UL,
                                         &xml_len);
    if (r != MF3D_OK) return r;
    opt->xml_scratch[xml_len] = 0;
    return mf3d_parse_model_xml((char*)opt->xml_scratch, out_model);
}
