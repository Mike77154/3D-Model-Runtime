#include "mf3d.h"

#define MF3D_ZIP_LOCAL 0x04034b50UL
#define MF3D_ZIP_CDIR  0x02014b50UL
#define MF3D_ZIP_EOCD  0x06054b50UL

typedef struct mf3d_zip_entry_s {
    char name[256];
    mf3d_u32 method;
    mf3d_u32 comp_size;
    mf3d_u32 uncomp_size;
    mf3d_u32 local_off;
    mf3d_u32 data_off;
} mf3d_zip_entry;

typedef struct mf3d_zip_s {
    mf3d_zip_entry entries[MF3D_MAX_ZIP_ENTRIES];
    mf3d_u32 count;
} mf3d_zip;

static mf3d_u16 rd16(const mf3d_u8 *p) { return (mf3d_u16)(p[0] | ((mf3d_u16)p[1] << 8)); }
static mf3d_u32 rd32(const mf3d_u8 *p) { return (mf3d_u32)p[0] | ((mf3d_u32)p[1] << 8) | ((mf3d_u32)p[2] << 16) | ((mf3d_u32)p[3] << 24); }
static int streq(const char *a, const char *b) { while (*a && *b && *a == *b) { a++; b++; } return *a == 0 && *b == 0; }
static int ends_with(const char *s, const char *tail)
{
    int ls, lt, i; ls = 0; lt = 0;
    while (s[ls]) ls++;
    while (tail[lt]) lt++;
    if (lt > ls) return 0;
    for (i = 0; i < lt; ++i) if (s[ls - lt + i] != tail[i]) return 0;
    return 1;
}

int mf3d_zip_read(const mf3d_u8 *data, mf3d_u32 size, mf3d_zip *zip)
{
    mf3d_u32 p, eocd, cd_off, cd_size, total, i;
    if (!data || !zip || size < 22) return MF3D_E_BADARG;
    zip->count = 0;
    eocd = 0xffffffffUL;
    p = size - 22;
    while (1) {
        if (rd32(data + p) == MF3D_ZIP_EOCD) { eocd = p; break; }
        if (p == 0 || size - p > 65557UL) break;
        p--;
    }
    if (eocd == 0xffffffffUL) return MF3D_E_ZIP;
    total = rd16(data + eocd + 10);
    cd_size = rd32(data + eocd + 12);
    cd_off = rd32(data + eocd + 16);
    if (cd_off + cd_size > size) return MF3D_E_ZIP;
    p = cd_off;
    for (i = 0; i < total && p + 46 <= size; ++i) {
        mf3d_u16 namelen, extralen, comlen;
        mf3d_zip_entry *z;
        if (rd32(data + p) != MF3D_ZIP_CDIR) return MF3D_E_ZIP;
        if (zip->count >= MF3D_MAX_ZIP_ENTRIES) return MF3D_E_NOSPACE;
        namelen = rd16(data + p + 28); extralen = rd16(data + p + 30); comlen = rd16(data + p + 32);
        if (p + 46UL + namelen + extralen + comlen > size) return MF3D_E_ZIP;
        z = &zip->entries[zip->count];
        z->method = rd16(data + p + 10);
        z->comp_size = rd32(data + p + 20);
        z->uncomp_size = rd32(data + p + 24);
        z->local_off = rd32(data + p + 42);
        if (namelen >= sizeof(z->name)) namelen = (mf3d_u16)(sizeof(z->name) - 1);
        for (comlen = 0; comlen < namelen; ++comlen) z->name[comlen] = (char)data[p + 46 + comlen];
        z->name[namelen] = 0;
        if (z->local_off + 30 > size || rd32(data + z->local_off) != MF3D_ZIP_LOCAL) return MF3D_E_ZIP;
        {
            mf3d_u16 ln = rd16(data + z->local_off + 26), le = rd16(data + z->local_off + 28);
            z->data_off = z->local_off + 30UL + ln + le;
            if (z->data_off + z->comp_size > size) return MF3D_E_ZIP;
        }
        zip->count++;
        p += 46UL + rd16(data + p + 28) + rd16(data + p + 30) + rd16(data + p + 32);
    }
    return MF3D_OK;
}

int mf3d_zip_find(mf3d_zip *zip, const char *name)
{
    mf3d_u32 i;
    if (!zip || !name) return -1;
    for (i = 0; i < zip->count; ++i) if (streq(zip->entries[i].name, name)) return (int)i;
    return -1;
}

int mf3d_zip_find_first_model(mf3d_zip *zip)
{
    mf3d_u32 i;
    if (!zip) return -1;
    if ((i = (mf3d_u32)mf3d_zip_find(zip, "3D/3dmodel.model")) != (mf3d_u32)-1) return (int)i;
    for (i = 0; i < zip->count; ++i) if (ends_with(zip->entries[i].name, ".model")) return (int)i;
    return -1;
}

int mf3d_zip_extract(const mf3d_u8 *data, mf3d_u32 size, mf3d_zip *zip, int idx,
                     mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len, mf3d_options *opt)
{
    mf3d_zip_entry *z;
    mf3d_u32 i;
    (void)size;
    if (!data || !zip || idx < 0 || (mf3d_u32)idx >= zip->count || !dst || !dst_len) return MF3D_E_BADARG;
    z = &zip->entries[idx];
    if (z->uncomp_size > dst_cap) return MF3D_E_NOSPACE;
    if (z->method == 0) {
        for (i = 0; i < z->uncomp_size; ++i) dst[i] = data[z->data_off + i];
        *dst_len = z->uncomp_size;
        if (*dst_len < dst_cap) dst[*dst_len] = 0;
        return MF3D_OK;
    }
    if (z->method == 8) {
        if (opt && opt->inflate_cb) return opt->inflate_cb(opt->inflate_user, data + z->data_off, z->comp_size, dst, dst_cap, dst_len);
        return MF3D_E_UNSUPPORTED;
    }
    return MF3D_E_UNSUPPORTED;
}
