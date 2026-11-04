#include "chiqui_amf.h"
#include <string.h>

static amf_u16 amf_rd16(const amf_u8 *p)
{
    return (amf_u16)((amf_u16)p[0] | ((amf_u16)p[1] << 8));
}

static amf_u32 amf_rd32(const amf_u8 *p)
{
    return (amf_u32)((amf_u32)p[0] | ((amf_u32)p[1] << 8) | ((amf_u32)p[2] << 16) | ((amf_u32)p[3] << 24));
}

static int amf_slice_ends(amf_slice s, const char *suffix)
{
    size_t n;
    n = 0u;
    while (suffix[n] != '\0') { n++; }
    if (s.len < n) return 0;
    return memcmp(s.ptr + s.len - n, suffix, n) == 0;
}

int amf_zip_find_first_xml(const amf_u8 *zip, size_t len, amf_zip_entry *out_entry, amf_diag *diag)
{
    size_t pos;
    int found_any;
    amf_zip_entry first;
    if (zip == 0 || out_entry == 0) {
        amf_diag_set(diag, AMF_ERR_BAD_ARG, 1u, 1u, "bad ZIP argument");
        return AMF_ERR_BAD_ARG;
    }
    memset(&first, 0, sizeof(first));
    found_any = 0;
    pos = 0u;
    while (pos + 30u <= len) {
        amf_u32 sig;
        amf_u16 flags;
        amf_u16 method;
        amf_u32 csize;
        amf_u32 usize;
        amf_u16 nlen;
        amf_u16 xlen;
        size_t data_pos;
        amf_zip_entry entry;
        sig = amf_rd32(zip + pos);
        if (sig == 0x02014b50UL || sig == 0x06054b50UL) {
            break;
        }
        if (sig != 0x04034b50UL) {
            pos++;
            continue;
        }
        flags = amf_rd16(zip + pos + 6u);
        method = amf_rd16(zip + pos + 8u);
        csize = amf_rd32(zip + pos + 18u);
        usize = amf_rd32(zip + pos + 22u);
        nlen = amf_rd16(zip + pos + 26u);
        xlen = amf_rd16(zip + pos + 28u);
        data_pos = pos + 30u + (size_t)nlen + (size_t)xlen;
        if (data_pos > len) {
            amf_diag_set(diag, AMF_ERR_BAD_ZIP, 1u, 1u, "bad ZIP local header lengths");
            return AMF_ERR_BAD_ZIP;
        }
        if ((flags & 8u) != 0u) {
            amf_diag_set(diag, AMF_ERR_UNSUPPORTED_ZIP, 1u, 1u, "ZIP data descriptor entries are not handled by the built-in scanner");
            return AMF_ERR_UNSUPPORTED_ZIP;
        }
        if (data_pos + (size_t)csize > len) {
            amf_diag_set(diag, AMF_ERR_BAD_ZIP, 1u, 1u, "ZIP compressed size exceeds buffer");
            return AMF_ERR_BAD_ZIP;
        }
        memset(&entry, 0, sizeof(entry));
        entry.name.ptr = (const char *)(zip + pos + 30u);
        entry.name.len = (size_t)nlen;
        entry.data = zip + data_pos;
        entry.compressed_size = (size_t)csize;
        entry.uncompressed_size = (size_t)usize;
        entry.method = (amf_u32)method;
        if (!found_any) {
            first = entry;
            found_any = 1;
        }
        if (amf_slice_ends(entry.name, ".amf") || amf_slice_ends(entry.name, ".xml")) {
            *out_entry = entry;
            return AMF_OK;
        }
        pos = data_pos + (size_t)csize;
    }
    if (found_any) {
        *out_entry = first;
        return AMF_OK;
    }
    amf_diag_set(diag, AMF_ERR_BAD_ZIP, 1u, 1u, "no ZIP local file entry found");
    return AMF_ERR_BAD_ZIP;
}
