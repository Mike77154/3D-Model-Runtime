#include "chiqui_amf.h"
#include <string.h>

amf_u32 amf_crc32(const amf_u8 *data, size_t len)
{
    amf_u32 crc;
    size_t i;
    crc = 0xffffffffUL;
    i = 0u;
    while (i < len) {
        amf_u32 x;
        int bit;
        x = (crc ^ (amf_u32)data[i]) & 0xffu;
        bit = 0;
        while (bit < 8) {
            if ((x & 1u) != 0u) x = (x >> 1) ^ 0xedb88320UL;
            else x >>= 1;
            bit++;
        }
        crc = (crc >> 8) ^ x;
        i++;
    }
    return crc ^ 0xffffffffUL;
}

static void zw16(amf_u8 *p, amf_u16 v)
{
    p[0] = (amf_u8)(v & 0xffu);
    p[1] = (amf_u8)((v >> 8) & 0xffu);
}

static void zw32(amf_u8 *p, amf_u32 v)
{
    p[0] = (amf_u8)(v & 0xffu);
    p[1] = (amf_u8)((v >> 8) & 0xffu);
    p[2] = (amf_u8)((v >> 16) & 0xffu);
    p[3] = (amf_u8)((v >> 24) & 0xffu);
}

static size_t zstrlen(const char *s)
{
    size_t n;
    n = 0u;
    while (s[n] != '\0') n++;
    return n;
}

int amf_write_zip_stored_xml(const char *entry_name, const amf_u8 *xml, size_t xml_len, amf_u8 *dst, size_t cap, size_t *out_len)
{
    size_t name_len;
    size_t local_pos;
    size_t data_pos;
    size_t central_pos;
    size_t eocd_pos;
    size_t total;
    amf_u32 crc;
    if (entry_name == 0 || xml == 0 || dst == 0 || out_len == 0) return AMF_ERR_BAD_ARG;
    name_len = zstrlen(entry_name);
    if (name_len == 0u || name_len > 65535u) return AMF_ERR_RANGE;
    if (xml_len > 0xffffffffUL) return AMF_ERR_RANGE;
    total = 30u + name_len + xml_len + 46u + name_len + 22u;
    if (total > cap) return AMF_ERR_WRITE_FULL;
    crc = amf_crc32(xml, xml_len);
    local_pos = 0u;
    zw32(dst + local_pos + 0u, 0x04034b50UL);
    zw16(dst + local_pos + 4u, 20u);
    zw16(dst + local_pos + 6u, 0u);
    zw16(dst + local_pos + 8u, 0u);
    zw16(dst + local_pos + 10u, 0u);
    zw16(dst + local_pos + 12u, 0u);
    zw32(dst + local_pos + 14u, crc);
    zw32(dst + local_pos + 18u, (amf_u32)xml_len);
    zw32(dst + local_pos + 22u, (amf_u32)xml_len);
    zw16(dst + local_pos + 26u, (amf_u16)name_len);
    zw16(dst + local_pos + 28u, 0u);
    memcpy(dst + local_pos + 30u, entry_name, name_len);
    data_pos = local_pos + 30u + name_len;
    memcpy(dst + data_pos, xml, xml_len);
    central_pos = data_pos + xml_len;
    zw32(dst + central_pos + 0u, 0x02014b50UL);
    zw16(dst + central_pos + 4u, 20u);
    zw16(dst + central_pos + 6u, 20u);
    zw16(dst + central_pos + 8u, 0u);
    zw16(dst + central_pos + 10u, 0u);
    zw16(dst + central_pos + 12u, 0u);
    zw16(dst + central_pos + 14u, 0u);
    zw32(dst + central_pos + 16u, crc);
    zw32(dst + central_pos + 20u, (amf_u32)xml_len);
    zw32(dst + central_pos + 24u, (amf_u32)xml_len);
    zw16(dst + central_pos + 28u, (amf_u16)name_len);
    zw16(dst + central_pos + 30u, 0u);
    zw16(dst + central_pos + 32u, 0u);
    zw16(dst + central_pos + 34u, 0u);
    zw16(dst + central_pos + 36u, 0u);
    zw32(dst + central_pos + 38u, 0u);
    zw32(dst + central_pos + 42u, (amf_u32)local_pos);
    memcpy(dst + central_pos + 46u, entry_name, name_len);
    eocd_pos = central_pos + 46u + name_len;
    zw32(dst + eocd_pos + 0u, 0x06054b50UL);
    zw16(dst + eocd_pos + 4u, 0u);
    zw16(dst + eocd_pos + 6u, 0u);
    zw16(dst + eocd_pos + 8u, 1u);
    zw16(dst + eocd_pos + 10u, 1u);
    zw32(dst + eocd_pos + 12u, (amf_u32)(46u + name_len));
    zw32(dst + eocd_pos + 16u, (amf_u32)central_pos);
    zw16(dst + eocd_pos + 20u, 0u);
    *out_len = eocd_pos + 22u;
    return AMF_OK;
}
