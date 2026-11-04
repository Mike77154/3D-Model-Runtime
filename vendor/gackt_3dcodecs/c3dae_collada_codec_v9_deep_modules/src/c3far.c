#include "c3far.h"

unsigned int c3far_probe_archive(const unsigned char *data, c3dae_u32 len)
{
    static const unsigned char zip_lhdr[4] = { 0x50U, 0x4bU, 0x03U, 0x04U };
    static const unsigned char zip_eocd[4] = { 0x50U, 0x4bU, 0x05U, 0x06U };
    static const unsigned char rar4[7] = { 0x52U, 0x61U, 0x72U, 0x21U, 0x1aU, 0x07U, 0x00U };
    static const unsigned char rar5[8] = { 0x52U, 0x61U, 0x72U, 0x21U, 0x1aU, 0x07U, 0x01U, 0x00U };
    static const unsigned char fuchsia_far[8] = { 0xc8U, 0xbfU, 0x0bU, 0x48U, 0xadU, 0xabU, 0xc5U, 0x11U };
    static const unsigned char unix_ar[8] = { '!', '<', 'a', 'r', 'c', 'h', '>', '\n' };
    unsigned int i;
    if (data == (const unsigned char *)0) return C3FAR_KIND_UNKNOWN;
    if (len >= 4UL) {
        i = 0U;
        while (i < 4U && data[i] == zip_lhdr[i]) i++;
        if (i == 4U) return C3FAR_KIND_ZIP;
        i = 0U;
        while (i < 4U && data[i] == zip_eocd[i]) i++;
        if (i == 4U) return C3FAR_KIND_ZIP;
    }
    if (len >= 8UL) {
        i = 0U;
        while (i < 8U && data[i] == rar5[i]) i++;
        if (i == 8U) return C3FAR_KIND_RAR5;
        i = 0U;
        while (i < 8U && data[i] == fuchsia_far[i]) i++;
        if (i == 8U) return C3FAR_KIND_FUCHSIA_FAR;
        i = 0U;
        while (i < 8U && data[i] == unix_ar[i]) i++;
        if (i == 8U) return C3FAR_KIND_UNIX_AR;
    }
    if (len >= 7UL) {
        i = 0U;
        while (i < 7U && data[i] == rar4[i]) i++;
        if (i == 7U) return C3FAR_KIND_RAR4;
    }
    return C3FAR_KIND_UNKNOWN;
}

const char *c3far_kind_name(unsigned int kind)
{
    if (kind == C3FAR_KIND_ZIP) return "zip/zae";
    if (kind == C3FAR_KIND_RAR4) return "rar4";
    if (kind == C3FAR_KIND_RAR5) return "rar5";
    if (kind == C3FAR_KIND_FUCHSIA_FAR) return "fuchsia far";
    if (kind == C3FAR_KIND_UNIX_AR) return "unix ar";
    return "unknown";
}
