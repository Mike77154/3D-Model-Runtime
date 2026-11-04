#include "mf3d_zip_secure_span.h"

#define ZSS_LOCAL 0x04034b50UL
#define ZSS_CDIR  0x02014b50UL
#define ZSS_EOCD  0x06054b50UL
#define ZSS_Z64_EOCD 0x06064b50UL
#define ZSS_Z64_LOC  0x07064b50UL
#define ZSS_DDESC 0x08074b50UL
#define ZSS_ARCH_EXTRA 0x08064b50UL

static mf3d_u16 zss_rd16(const mf3d_u8 *p)
{
    return (mf3d_u16)(p[0] | ((mf3d_u16)p[1] << 8));
}

static mf3d_u32 zss_rd32(const mf3d_u8 *p)
{
    return (mf3d_u32)p[0] | ((mf3d_u32)p[1] << 8) | ((mf3d_u32)p[2] << 16) | ((mf3d_u32)p[3] << 24);
}

static mf3d_z64_u64 zss_rd64(const mf3d_u8 *p)
{
    mf3d_z64_u64 v;
    v.lo = zss_rd32(p);
    v.hi = zss_rd32(p + 4);
    return v;
}

static int zss_u64_fit32(mf3d_z64_u64 v)
{
    return v.hi == 0;
}

static void zss_zero(void *p, mf3d_u32 n)
{
    mf3d_u8 *q;
    q = (mf3d_u8*)p;
    while (n--) *q++ = 0;
}


int mf3d_zss_is_split_marker(const mf3d_u8 *data, mf3d_u32 size)
{
    mf3d_u32 sig;
    if (!data || size < 8) return 0;
    sig = zss_rd32(data);
    if ((sig == MF3D_ZSS_SPAN_SIG || sig == MF3D_ZSS_SPAN_TMP_SIG) && zss_rd32(data + 4) == ZSS_LOCAL) return 1;
    return 0;
}

int mf3d_zss_join_spans(const mf3d_zss_span *spans, mf3d_u32 span_count,
                        mf3d_u32 flags, mf3d_u8 *dst, mf3d_u32 dst_cap,
                        mf3d_u32 *dst_len)
{
    mf3d_u32 i, j, pos, off, size;
    if (!spans || !dst || !dst_len || span_count == 0) return MF3D_E_BADARG;
    pos = 0;
    for (i = 0; i < span_count; ++i) {
        if (!spans[i].data && spans[i].size) return MF3D_E_BADARG;
        off = 0;
        size = spans[i].size;
        if (i == 0 && (flags & MF3D_ZSS_JOIN_STRIP_MARKER) && mf3d_zss_is_split_marker(spans[i].data, spans[i].size)) {
            off = 4;
            size -= 4;
        }
        if (pos > dst_cap || size > dst_cap - pos) return MF3D_E_NOSPACE;
        for (j = 0; j < size; ++j) dst[pos + j] = spans[i].data[off + j];
        pos += size;
    }
    *dst_len = pos;
    return MF3D_OK;
}

/* ------------------------- legacy ZipCrypto ------------------------- */

static mf3d_u32 zss_crc32_tab[256];
static int zss_crc32_ready = 0;

static void zss_crc32_init(void)
{
    mf3d_u32 i, j, c;
    if (zss_crc32_ready) return;
    for (i = 0; i < 256; ++i) {
        c = i;
        for (j = 0; j < 8; ++j) c = (c & 1UL) ? (0xedb88320UL ^ (c >> 1)) : (c >> 1);
        zss_crc32_tab[i] = c;
    }
    zss_crc32_ready = 1;
}

static mf3d_u32 zss_crc32_update(mf3d_u32 crc, mf3d_u8 c)
{
    return zss_crc32_tab[(crc ^ c) & 0xffU] ^ (crc >> 8);
}

typedef struct zss_zc_s {
    mf3d_u32 k0, k1, k2;
} zss_zc;

static void zss_zc_update(zss_zc *z, mf3d_u8 c)
{
    z->k0 = zss_crc32_update(z->k0, c);
    z->k1 = z->k1 + (z->k0 & 0xffU);
    z->k1 = z->k1 * 134775813UL + 1UL;
    z->k2 = zss_crc32_update(z->k2, (mf3d_u8)(z->k1 >> 24));
}

static mf3d_u8 zss_zc_byte(zss_zc *z)
{
    mf3d_u32 t;
    t = (z->k2 | 2UL) & 0xffffUL;
    return (mf3d_u8)(((t * (t ^ 1UL)) >> 8) & 0xffU);
}

static void zss_zc_init(zss_zc *z, const char *password)
{
    const unsigned char *p;
    zss_crc32_init();
    z->k0 = 305419896UL;
    z->k1 = 591751049UL;
    z->k2 = 878082192UL;
    p = (const unsigned char*)password;
    while (*p) {
        zss_zc_update(z, (mf3d_u8)*p);
        ++p;
    }
}

int mf3d_zss_zipcrypto_decrypt(const char *password,
                               const mf3d_u8 *src, mf3d_u32 src_len,
                               mf3d_u8 *dst, mf3d_u32 dst_cap,
                               mf3d_u32 *dst_len,
                               mf3d_u8 crc_high_check,
                               mf3d_u32 *header_ok)
{
    zss_zc z;
    mf3d_u8 h[12];
    mf3d_u32 i, out_len;
    mf3d_u8 c;
    if (!password || !src || !dst || !dst_len || src_len < 12) return MF3D_E_BADARG;
    out_len = src_len - 12;
    if (out_len > dst_cap) return MF3D_E_NOSPACE;
    zss_zc_init(&z, password);
    for (i = 0; i < 12; ++i) {
        c = (mf3d_u8)(src[i] ^ zss_zc_byte(&z));
        h[i] = c;
        zss_zc_update(&z, c);
    }
    if (header_ok) *header_ok = (h[11] == crc_high_check) ? 1UL : 0UL;
    for (i = 0; i < out_len; ++i) {
        c = (mf3d_u8)(src[12 + i] ^ zss_zc_byte(&z));
        dst[i] = c;
        zss_zc_update(&z, c);
    }
    *dst_len = out_len;
    return MF3D_OK;
}

/* ------------------------- extra-field metadata ------------------------- */

static void zss_parse_extra(const mf3d_u8 *extra, mf3d_u32 extra_len, mf3d_zss_crypto_info *info)
{
    mf3d_u32 p, sz;
    mf3d_u16 tag;
    p = 0;
    while (p + 4 <= extra_len) {
        tag = zss_rd16(extra + p);
        sz = zss_rd16(extra + p + 2);
        p += 4;
        if (sz > extra_len - p) break;
        if (tag == MF3D_ZSS_EXTRA_AES && sz >= 7) {
            info->is_winzip_aes = 1;
            info->aes_vendor_version = zss_rd16(extra + p);
            info->aes_vendor_id[0] = (char)extra[p + 2];
            info->aes_vendor_id[1] = (char)extra[p + 3];
            info->aes_vendor_id[2] = 0;
            info->aes_strength = extra[p + 4];
            info->aes_actual_method = zss_rd16(extra + p + 5);
        } else if (tag == MF3D_ZSS_EXTRA_STRONG && sz >= 8) {
            info->is_strong = 1;
            info->strong_format = zss_rd16(extra + p);
            info->strong_algid = zss_rd16(extra + p + 2);
            info->strong_bitlen = zss_rd16(extra + p + 4);
            info->strong_flags = zss_rd16(extra + p + 6);
        }
        p += sz;
    }
}

int mf3d_zss_entry_crypto_info(const mf3d_u8 *zip_data, mf3d_u32 zip_size,
                               const mf3d_z64_entry *entry,
                               mf3d_zss_crypto_info *info)
{
    mf3d_u32 off, name_len, extra_len;
    if (!zip_data || !entry || !info) return MF3D_E_BADARG;
    zss_zero(info, (mf3d_u32)sizeof(*info));
    info->gp_flag = entry->flag;
    info->method = entry->method;
    info->is_encrypted = (entry->flag & MF3D_ZSS_GP_ENCRYPTED) ? 1UL : 0UL;
    info->is_strong = (entry->flag & MF3D_ZSS_GP_STRONG) ? 1UL : 0UL;
    info->local_header_masked = (entry->flag & MF3D_ZSS_GP_CENTRAL_MASKED) ? 1UL : 0UL;
    info->is_legacy_zipcrypto = (info->is_encrypted && !info->is_strong && entry->method != MF3D_ZSS_METHOD_AES_EXTRA) ? 1UL : 0UL;
    if (!zss_u64_fit32(entry->local_off)) return MF3D_E_RANGE;
    off = entry->local_off.lo;
    if (off + 30UL > zip_size || zss_rd32(zip_data + off) != ZSS_LOCAL) return MF3D_E_ZIP;
    name_len = zss_rd16(zip_data + off + 26);
    extra_len = zss_rd16(zip_data + off + 28);
    if (off + 30UL + name_len > zip_size) return MF3D_E_ZIP;
    if (extra_len > zip_size - off - 30UL - name_len) return MF3D_E_ZIP;
    zss_parse_extra(zip_data + off + 30UL + name_len, extra_len, info);
    if (info->is_winzip_aes) {
        info->is_encrypted = 1;
        info->is_legacy_zipcrypto = 0;
    }
    return MF3D_OK;
}

static int zss_find_eocd(const mf3d_u8 *data, mf3d_u32 size, mf3d_u32 *off)
{
    mf3d_u32 p, minp;
    if (!data || !off || size < 22) return MF3D_E_ZIP;
    minp = size > (22UL + 65535UL) ? size - (22UL + 65535UL) : 0;
    p = size - 22UL;
    for (;;) {
        if (zss_rd32(data + p) == ZSS_EOCD) { *off = p; return MF3D_OK; }
        if (p == minp) break;
        --p;
    }
    return MF3D_E_ZIP;
}

static int zss_find_z64(const mf3d_u8 *data, mf3d_u32 size, mf3d_u32 eocd, mf3d_u32 *z64)
{
    mf3d_u32 loc;
    if (!data || !z64 || eocd < 20) return MF3D_E_NOTFOUND;
    loc = eocd - 20UL;
    if (zss_rd32(data + loc) != ZSS_Z64_LOC) return MF3D_E_NOTFOUND;
    *z64 = zss_rd32(data + loc + 8);
    if (*z64 + 56UL > size || zss_rd32(data + *z64) != ZSS_Z64_EOCD) return MF3D_E_ZIP;
    return MF3D_OK;
}

static int zss_scan_local_masked(const mf3d_u8 *data, mf3d_u32 size)
{
    mf3d_u32 p, sig, nlen, xlen, csize;
    mf3d_u16 flag;
    p = 0;
    if (mf3d_zss_is_split_marker(data, size)) p = 4;
    while (p + 30UL <= size) {
        sig = zss_rd32(data + p);
        if (sig == ZSS_LOCAL) {
            flag = zss_rd16(data + p + 6);
            if (flag & MF3D_ZSS_GP_CENTRAL_MASKED) return 1;
            nlen = zss_rd16(data + p + 26);
            xlen = zss_rd16(data + p + 28);
            if (p + 30UL + nlen + xlen > size) return 0;
            csize = zss_rd32(data + p + 18);
            p += 30UL + nlen + xlen;
            if (csize == 0 || (flag & 8U)) return 0;
            if (csize > size - p) return 0;
            p += csize;
        } else if (sig == ZSS_DDESC || sig == ZSS_CDIR || sig == ZSS_EOCD || sig == ZSS_Z64_EOCD || sig == ZSS_Z64_LOC) {
            return 0;
        } else {
            ++p;
        }
    }
    return 0;
}

int mf3d_zss_inspect_central_directory(const mf3d_u8 *zip_data, mf3d_u32 zip_size,
                                       mf3d_zss_cdir_info *info)
{
    mf3d_u32 eocd, z64;
    int r;
    if (!zip_data || !info) return MF3D_E_BADARG;
    zss_zero(info, (mf3d_u32)sizeof(*info));
    r = zss_find_eocd(zip_data, zip_size, &eocd);
    if (r != MF3D_OK) return r;
    info->masked_local_headers = (mf3d_u32)zss_scan_local_masked(zip_data, zip_size);
    r = zss_find_z64(zip_data, zip_size, eocd, &z64);
    if (r == MF3D_OK) {
        info->present = 1;
        info->archive_header_offset.lo = zss_rd32(zip_data + z64 + 48);
        info->archive_header_offset.hi = zss_rd32(zip_data + z64 + 52);
        if (z64 + 78UL <= zip_size) {
            info->compression_method = zss_rd16(zip_data + z64 + 56);
            info->compressed_size = zss_rd64(zip_data + z64 + 58);
            info->original_size = zss_rd64(zip_data + z64 + 66);
            info->algid = zss_rd16(zip_data + z64 + 74);
            info->bitlen = zss_rd16(zip_data + z64 + 76);
            if (z64 + 84UL <= zip_size) {
                info->flags = zss_rd16(zip_data + z64 + 78);
                info->hashid = zss_rd16(zip_data + z64 + 80);
                info->hash_len = zss_rd16(zip_data + z64 + 82);
            }
            if (info->compression_method || info->compressed_size.lo || info->compressed_size.hi || info->original_size.lo || info->original_size.hi || info->algid || info->bitlen || info->flags || info->hashid) {
                info->encrypted_or_compressed = 1;
            }
        }
        return MF3D_OK;
    }
    if (info->masked_local_headers) info->encrypted_or_compressed = 1;
    return MF3D_OK;
}

static int zss_raw_extract(const mf3d_u8 *zip_data, mf3d_u32 zip_size,
                           const mf3d_z64_entry *e,
                           const mf3d_zss_options *opt,
                           mf3d_u8 *plain_comp, mf3d_u32 plain_comp_cap,
                           mf3d_u32 *plain_comp_len,
                           mf3d_zss_crypto_info *ci)
{
    mf3d_u32 data_off, comp_size, header_ok;
    int r;
    if (!zss_u64_fit32(e->data_off) || !zss_u64_fit32(e->comp_size)) return MF3D_E_RANGE;
    data_off = e->data_off.lo;
    comp_size = e->comp_size.lo;
    if (data_off > zip_size || comp_size > zip_size - data_off) return MF3D_E_ZIP;
    r = mf3d_zss_entry_crypto_info(zip_data, zip_size, e, ci);
    if (r != MF3D_OK) return r;
    if (!ci->is_encrypted) {
        if (comp_size > plain_comp_cap) return MF3D_E_NOSPACE;
        r = 0;
        while ((mf3d_u32)r < comp_size) { plain_comp[r] = zip_data[data_off + (mf3d_u32)r]; ++r; }
        *plain_comp_len = comp_size;
        return MF3D_OK;
    }
    if (ci->is_legacy_zipcrypto) {
        if (!opt || !opt->password) return MF3D_E_BADARG;
        r = mf3d_zss_zipcrypto_decrypt(opt->password, zip_data + data_off, comp_size,
                                      plain_comp, plain_comp_cap, plain_comp_len,
                                      (mf3d_u8)(e->crc32 >> 24), &header_ok);
        if (r != MF3D_OK) return r;
        if (!header_ok) return MF3D_E_ZIP;
        return MF3D_OK;
    }
    if (opt && opt->crypto_cb) {
        return opt->crypto_cb(opt->crypto_user, ci, zip_data + data_off, comp_size,
                              plain_comp, plain_comp_cap, plain_comp_len);
    }
    return MF3D_E_UNSUPPORTED;
}

int mf3d_zss_extract(const mf3d_u8 *zip_data, mf3d_u32 zip_size,
                     mf3d_z64_archive *zip, int idx,
                     const mf3d_zss_options *opt,
                     mf3d_u8 *dst, mf3d_u32 dst_cap,
                     mf3d_u32 *dst_len)
{
    mf3d_z64_entry *e;
    mf3d_zss_crypto_info ci;
    mf3d_u8 *comp_buf;
    mf3d_u32 comp_len;
    int r;
    if (!zip_data || !zip || !dst || !dst_len || idx < 0 || (mf3d_u32)idx >= zip->count) return MF3D_E_BADARG;
    e = &zip->entries[idx];
    if (e->method == MF3D_Z64_METHOD_STORE && !(e->flag & MF3D_ZSS_GP_ENCRYPTED)) {
        return mf3d_zip64_extract(zip_data, zip_size, zip, idx, dst, dst_cap, dst_len);
    }
    if (e->method == MF3D_Z64_METHOD_DEFLATE && !(e->flag & MF3D_ZSS_GP_ENCRYPTED)) {
        return mf3d_zip64_extract(zip_data, zip_size, zip, idx, dst, dst_cap, dst_len);
    }
    if (!opt || !opt->scratch || opt->scratch_size == 0) return MF3D_E_BADARG;
    comp_buf = opt->scratch;
    r = zss_raw_extract(zip_data, zip_size, e, opt, comp_buf, opt->scratch_size, &comp_len, &ci);
    if (r != MF3D_OK) return r;
    if (ci.is_winzip_aes && ci.aes_actual_method) e->method = ci.aes_actual_method;
    if (e->method == MF3D_Z64_METHOD_STORE) {
        if (comp_len > dst_cap) return MF3D_E_NOSPACE;
        for (r = 0; (mf3d_u32)r < comp_len; ++r) dst[(mf3d_u32)r] = comp_buf[(mf3d_u32)r];
        *dst_len = comp_len;
        return MF3D_OK;
    }
    if (e->method == MF3D_Z64_METHOD_DEFLATE) {
        return mf3d_zip64_deflate_inflate_cb(0, comp_buf, comp_len, dst, dst_cap, dst_len);
    }
    return MF3D_E_UNSUPPORTED;
}

int mf3d_zss_extract_named(const mf3d_u8 *zip_data, mf3d_u32 zip_size,
                           const char *name, const mf3d_zss_options *opt,
                           mf3d_u8 *dst, mf3d_u32 dst_cap,
                           mf3d_u32 *dst_len)
{
    mf3d_z64_archive zip;
    int r, idx;
    if (!zip_data || !name) return MF3D_E_BADARG;
    r = mf3d_zip64_read(zip_data, zip_size, &zip);
    if (r != MF3D_OK) return r;
    idx = mf3d_zip64_find(&zip, name);
    if (idx < 0) return MF3D_E_NOTFOUND;
    return mf3d_zss_extract(zip_data, zip_size, &zip, idx, opt, dst, dst_cap, dst_len);
}
