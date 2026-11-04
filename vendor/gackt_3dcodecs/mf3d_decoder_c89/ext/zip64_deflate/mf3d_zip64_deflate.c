#include "mf3d_zip64_deflate.h"

#define MF3D_ZIP_LOCAL 0x04034b50UL
#define MF3D_ZIP_CDIR  0x02014b50UL
#define MF3D_ZIP_EOCD  0x06054b50UL
#define MF3D_ZIP64_EOCD 0x06064b50UL
#define MF3D_ZIP64_LOC  0x07064b50UL
#define MF3D_ZIP_DDESC 0x08074b50UL
#define MF3D_ZIP64_EXTRA 0x0001U

#define MF3D_INF_MAXBITS 15
#define MF3D_INF_LIT_MAX 288
#define MF3D_INF_DIST_MAX 32
#define MF3D_INF_CLEN_MAX 19

static mf3d_u16 z_rd16(const mf3d_u8 *p)
{
    return (mf3d_u16)(p[0] | ((mf3d_u16)p[1] << 8));
}

static mf3d_u32 z_rd32(const mf3d_u8 *p)
{
    return (mf3d_u32)p[0] | ((mf3d_u32)p[1] << 8) | ((mf3d_u32)p[2] << 16) | ((mf3d_u32)p[3] << 24);
}

static mf3d_z64_u64 z_rd64(const mf3d_u8 *p)
{
    mf3d_z64_u64 v;
    v.lo = z_rd32(p);
    v.hi = z_rd32(p + 4);
    return v;
}

static mf3d_z64_u64 z_u64_from32(mf3d_u32 v)
{
    mf3d_z64_u64 r;
    r.lo = v;
    r.hi = 0;
    return r;
}

static int z_u64_fits32(mf3d_z64_u64 v)
{
    return v.hi == 0;
}

static int z_u64_add32(mf3d_z64_u64 a, mf3d_u32 b, mf3d_z64_u64 *out)
{
    mf3d_z64_u64 r;
    r.lo = a.lo + b;
    r.hi = a.hi;
    if (r.lo < a.lo) r.hi++;
    *out = r;
    return r.hi == 0;
}

static int z_streq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

static int z_ends_with(const char *s, const char *tail)
{
    int ls, lt, i;
    ls = 0; lt = 0;
    while (s[ls]) ls++;
    while (tail[lt]) lt++;
    if (lt > ls) return 0;
    for (i = 0; i < lt; ++i) if (s[ls - lt + i] != tail[i]) return 0;
    return 1;
}

static mf3d_u32 z_min_u32(mf3d_u32 a, mf3d_u32 b)
{
    return a < b ? a : b;
}

/* ----------------------- raw Deflate inflater ----------------------- */

typedef struct mf3d_inf_bits_s {
    const mf3d_u8 *src;
    mf3d_u32 len;
    mf3d_u32 pos;
    mf3d_u32 bitbuf;
    int bitcnt;
} mf3d_inf_bits;

typedef struct mf3d_inf_huff_s {
    mf3d_u16 codes[MF3D_INF_LIT_MAX];
    mf3d_u8 lens[MF3D_INF_LIT_MAX];
    int symbols;
    int maxbits;
} mf3d_inf_huff;

static int inf_need(mf3d_inf_bits *b, int n)
{
    while (b->bitcnt < n) {
        if (b->pos >= b->len) return MF3D_E_INFLATE;
        b->bitbuf |= ((mf3d_u32)b->src[b->pos++]) << b->bitcnt;
        b->bitcnt += 8;
    }
    return MF3D_OK;
}

static int inf_bits(mf3d_inf_bits *b, int n, mf3d_u32 *out)
{
    mf3d_u32 mask;
    int r;
    if (n == 0) { *out = 0; return MF3D_OK; }
    r = inf_need(b, n);
    if (r != MF3D_OK) return r;
    mask = (((mf3d_u32)1) << n) - 1UL;
    *out = b->bitbuf & mask;
    b->bitbuf >>= n;
    b->bitcnt -= n;
    return MF3D_OK;
}

static int inf_bit(mf3d_inf_bits *b, mf3d_u32 *out)
{
    return inf_bits(b, 1, out);
}

static void inf_align_byte(mf3d_inf_bits *b)
{
    b->bitbuf = 0;
    b->bitcnt = 0;
}

static mf3d_u16 inf_revbits(mf3d_u16 code, int len)
{
    mf3d_u16 r;
    int i;
    r = 0;
    for (i = 0; i < len; ++i) {
        r = (mf3d_u16)((r << 1) | (code & 1U));
        code = (mf3d_u16)(code >> 1);
    }
    return r;
}

static int inf_build_huff(mf3d_inf_huff *h, const mf3d_u8 *lengths, int symbols)
{
    int bl_count[MF3D_INF_MAXBITS + 1];
    int next_code[MF3D_INF_MAXBITS + 1];
    int code;
    int bits;
    int n;
    int maxbits;
    int left;

    if (!h || !lengths || symbols < 1 || symbols > MF3D_INF_LIT_MAX) return MF3D_E_BADARG;
    for (bits = 0; bits <= MF3D_INF_MAXBITS; ++bits) { bl_count[bits] = 0; next_code[bits] = 0; }
    maxbits = 0;
    for (n = 0; n < symbols; ++n) {
        if (lengths[n] > MF3D_INF_MAXBITS) return MF3D_E_INFLATE;
        h->lens[n] = lengths[n];
        h->codes[n] = 0;
        if (lengths[n]) {
            bl_count[lengths[n]]++;
            if (lengths[n] > maxbits) maxbits = lengths[n];
        }
    }
    if (maxbits == 0) return MF3D_E_INFLATE;

    /* Reject oversubscribed trees. Permit incomplete trees, which occur for
       small dynamic distance alphabets. */
    left = 1;
    for (bits = 1; bits <= MF3D_INF_MAXBITS; ++bits) {
        left <<= 1;
        left -= bl_count[bits];
        if (left < 0) return MF3D_E_INFLATE;
    }

    code = 0;
    bl_count[0] = 0;
    for (bits = 1; bits <= MF3D_INF_MAXBITS; ++bits) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    for (n = 0; n < symbols; ++n) {
        int len;
        len = lengths[n];
        if (len != 0) {
            h->codes[n] = inf_revbits((mf3d_u16)next_code[len], len);
            next_code[len]++;
        }
    }
    h->symbols = symbols;
    h->maxbits = maxbits;
    return MF3D_OK;
}

static int inf_decode(mf3d_inf_bits *b, const mf3d_inf_huff *h, int *sym)
{
    mf3d_u32 bit;
    mf3d_u16 code;
    int len;
    int n;
    code = 0;
    for (len = 1; len <= h->maxbits; ++len) {
        if (inf_bit(b, &bit) != MF3D_OK) return MF3D_E_INFLATE;
        code = (mf3d_u16)(code | ((mf3d_u16)bit << (len - 1)));
        for (n = 0; n < h->symbols; ++n) {
            if (h->lens[n] == len && h->codes[n] == code) {
                *sym = n;
                return MF3D_OK;
            }
        }
    }
    return MF3D_E_INFLATE;
}

static int inf_fixed_trees(mf3d_inf_huff *lit, mf3d_inf_huff *dist)
{
    mf3d_u8 lens[MF3D_INF_LIT_MAX];
    mf3d_u8 dlens[MF3D_INF_DIST_MAX];
    int i;
    for (i = 0; i <= 143; ++i) lens[i] = 8;
    for (i = 144; i <= 255; ++i) lens[i] = 9;
    for (i = 256; i <= 279; ++i) lens[i] = 7;
    for (i = 280; i <= 287; ++i) lens[i] = 8;
    for (i = 0; i < MF3D_INF_DIST_MAX; ++i) dlens[i] = 5;
    if (inf_build_huff(lit, lens, 288) != MF3D_OK) return MF3D_E_INFLATE;
    if (inf_build_huff(dist, dlens, 32) != MF3D_OK) return MF3D_E_INFLATE;
    return MF3D_OK;
}

static int inf_dynamic_trees(mf3d_inf_bits *bits, mf3d_inf_huff *lit, mf3d_inf_huff *dist)
{
    static const int order[MF3D_INF_CLEN_MAX] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
    mf3d_u8 clen_lengths[MF3D_INF_CLEN_MAX];
    mf3d_u8 all_lengths[MF3D_INF_LIT_MAX + MF3D_INF_DIST_MAX];
    mf3d_u8 lit_lengths[MF3D_INF_LIT_MAX];
    mf3d_u8 dist_lengths[MF3D_INF_DIST_MAX];
    mf3d_inf_huff clen;
    mf3d_u32 v;
    int hlit;
    int hdist;
    int hclen;
    int i;
    int n;
    int sym;
    int repeat;
    int prev;

    if (inf_bits(bits, 5, &v) != MF3D_OK) return MF3D_E_INFLATE;
    hlit = (int)v + 257;
    if (inf_bits(bits, 5, &v) != MF3D_OK) return MF3D_E_INFLATE;
    hdist = (int)v + 1;
    if (inf_bits(bits, 4, &v) != MF3D_OK) return MF3D_E_INFLATE;
    hclen = (int)v + 4;
    if (hlit > MF3D_INF_LIT_MAX || hdist > MF3D_INF_DIST_MAX) return MF3D_E_INFLATE;

    for (i = 0; i < MF3D_INF_CLEN_MAX; ++i) clen_lengths[i] = 0;
    for (i = 0; i < hclen; ++i) {
        if (inf_bits(bits, 3, &v) != MF3D_OK) return MF3D_E_INFLATE;
        clen_lengths[order[i]] = (mf3d_u8)v;
    }
    if (inf_build_huff(&clen, clen_lengths, MF3D_INF_CLEN_MAX) != MF3D_OK) return MF3D_E_INFLATE;

    n = 0;
    prev = 0;
    while (n < hlit + hdist) {
        if (inf_decode(bits, &clen, &sym) != MF3D_OK) return MF3D_E_INFLATE;
        if (sym <= 15) {
            all_lengths[n++] = (mf3d_u8)sym;
            prev = sym;
        } else if (sym == 16) {
            if (n == 0) return MF3D_E_INFLATE;
            if (inf_bits(bits, 2, &v) != MF3D_OK) return MF3D_E_INFLATE;
            repeat = 3 + (int)v;
            while (repeat-- > 0) {
                if (n >= hlit + hdist) return MF3D_E_INFLATE;
                all_lengths[n++] = (mf3d_u8)prev;
            }
        } else if (sym == 17) {
            if (inf_bits(bits, 3, &v) != MF3D_OK) return MF3D_E_INFLATE;
            repeat = 3 + (int)v;
            while (repeat-- > 0) {
                if (n >= hlit + hdist) return MF3D_E_INFLATE;
                all_lengths[n++] = 0;
            }
            prev = 0;
        } else if (sym == 18) {
            if (inf_bits(bits, 7, &v) != MF3D_OK) return MF3D_E_INFLATE;
            repeat = 11 + (int)v;
            while (repeat-- > 0) {
                if (n >= hlit + hdist) return MF3D_E_INFLATE;
                all_lengths[n++] = 0;
            }
            prev = 0;
        } else {
            return MF3D_E_INFLATE;
        }
    }

    for (i = 0; i < hlit; ++i) lit_lengths[i] = all_lengths[i];
    for (i = hlit; i < MF3D_INF_LIT_MAX; ++i) lit_lengths[i] = 0;
    for (i = 0; i < hdist; ++i) dist_lengths[i] = all_lengths[hlit + i];
    for (i = hdist; i < MF3D_INF_DIST_MAX; ++i) dist_lengths[i] = 0;

    if (lit_lengths[256] == 0) return MF3D_E_INFLATE;
    if (inf_build_huff(lit, lit_lengths, hlit) != MF3D_OK) return MF3D_E_INFLATE;
    if (inf_build_huff(dist, dist_lengths, hdist) != MF3D_OK) return MF3D_E_INFLATE;
    return MF3D_OK;
}

static int inf_stored_block(mf3d_inf_bits *bits, mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *outpos)
{
    mf3d_u16 len;
    mf3d_u16 nlen;
    mf3d_u32 i;
    inf_align_byte(bits);
    if (bits->pos + 4UL > bits->len) return MF3D_E_INFLATE;
    len = z_rd16(bits->src + bits->pos);
    nlen = z_rd16(bits->src + bits->pos + 2);
    bits->pos += 4;
    if ((mf3d_u16)(len ^ 0xffffU) != nlen) return MF3D_E_INFLATE;
    if (*outpos + len > dst_cap) return MF3D_E_NOSPACE;
    if (bits->pos + len > bits->len) return MF3D_E_INFLATE;
    for (i = 0; i < len; ++i) dst[(*outpos)++] = bits->src[bits->pos++];
    return MF3D_OK;
}

static int inf_codes_block(mf3d_inf_bits *bits, mf3d_inf_huff *lit, mf3d_inf_huff *dist,
                           mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *outpos)
{
    static const int length_base[29] = {
        3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
    };
    static const int length_extra[29] = {
        0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
    };
    static const int dist_base[30] = {
        1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,
        4097,6145,8193,12289,16385,24577
    };
    static const int dist_extra[30] = {
        0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
    };
    int sym;
    int dsym;
    int len;
    int distv;
    mf3d_u32 extra;
    mf3d_u32 i;

    for (;;) {
        if (inf_decode(bits, lit, &sym) != MF3D_OK) return MF3D_E_INFLATE;
        if (sym < 256) {
            if (*outpos >= dst_cap) return MF3D_E_NOSPACE;
            dst[(*outpos)++] = (mf3d_u8)sym;
        } else if (sym == 256) {
            return MF3D_OK;
        } else if (sym >= 257 && sym <= 285) {
            int li;
            li = sym - 257;
            len = length_base[li];
            if (length_extra[li]) {
                if (inf_bits(bits, length_extra[li], &extra) != MF3D_OK) return MF3D_E_INFLATE;
                len += (int)extra;
            }
            if (inf_decode(bits, dist, &dsym) != MF3D_OK) return MF3D_E_INFLATE;
            if (dsym < 0 || dsym > 29) return MF3D_E_INFLATE;
            distv = dist_base[dsym];
            if (dist_extra[dsym]) {
                if (inf_bits(bits, dist_extra[dsym], &extra) != MF3D_OK) return MF3D_E_INFLATE;
                distv += (int)extra;
            }
            if (distv <= 0 || (mf3d_u32)distv > *outpos) return MF3D_E_INFLATE;
            if (*outpos + (mf3d_u32)len > dst_cap) return MF3D_E_NOSPACE;
            for (i = 0; i < (mf3d_u32)len; ++i) {
                dst[*outpos] = dst[*outpos - (mf3d_u32)distv];
                (*outpos)++;
            }
        } else {
            return MF3D_E_INFLATE;
        }
    }
}

int mf3d_zip64_deflate_inflate_cb(void *user, const mf3d_u8 *src, mf3d_u32 src_len,
                                  mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len)
{
    mf3d_inf_bits bits;
    mf3d_inf_huff lit;
    mf3d_inf_huff dist;
    mf3d_u32 outpos;
    mf3d_u32 bfinal;
    mf3d_u32 btype;
    int done;
    int r;

    (void)user;
    if (!src || !dst || !dst_len) return MF3D_E_BADARG;
    bits.src = src;
    bits.len = src_len;
    bits.pos = 0;
    bits.bitbuf = 0;
    bits.bitcnt = 0;
    outpos = 0;
    done = 0;
    while (!done) {
        if (inf_bits(&bits, 1, &bfinal) != MF3D_OK) return MF3D_E_INFLATE;
        if (inf_bits(&bits, 2, &btype) != MF3D_OK) return MF3D_E_INFLATE;
        done = bfinal ? 1 : 0;
        if (btype == 0) {
            r = inf_stored_block(&bits, dst, dst_cap, &outpos);
        } else if (btype == 1) {
            r = inf_fixed_trees(&lit, &dist);
            if (r == MF3D_OK) r = inf_codes_block(&bits, &lit, &dist, dst, dst_cap, &outpos);
        } else if (btype == 2) {
            r = inf_dynamic_trees(&bits, &lit, &dist);
            if (r == MF3D_OK) r = inf_codes_block(&bits, &lit, &dist, dst, dst_cap, &outpos);
        } else {
            r = MF3D_E_INFLATE;
        }
        if (r != MF3D_OK) return r;
    }
    *dst_len = outpos;
    if (outpos < dst_cap) dst[outpos] = 0;
    return MF3D_OK;
}

/* ----------------------- ZIP64 + data descriptors ----------------------- */

static int z_parse_zip64_extra(const mf3d_u8 *extra, mf3d_u32 extra_len,
                               int need_usize, int need_csize, int need_off,
                               mf3d_z64_u64 *usize, mf3d_z64_u64 *csize, mf3d_z64_u64 *off)
{
    mf3d_u32 p;
    p = 0;
    while (p + 4UL <= extra_len) {
        mf3d_u16 tag;
        mf3d_u16 sz;
        mf3d_u32 q;
        tag = z_rd16(extra + p);
        sz = z_rd16(extra + p + 2);
        p += 4;
        if (p + sz > extra_len) return MF3D_E_ZIP;
        if (tag == MF3D_ZIP64_EXTRA) {
            q = p;
            if (need_usize) { if (q + 8UL > p + sz) return MF3D_E_ZIP; *usize = z_rd64(extra + q); q += 8; }
            if (need_csize) { if (q + 8UL > p + sz) return MF3D_E_ZIP; *csize = z_rd64(extra + q); q += 8; }
            if (need_off) { if (q + 8UL > p + sz) return MF3D_E_ZIP; *off = z_rd64(extra + q); q += 8; }
            return MF3D_OK;
        }
        p += sz;
    }
    if (need_usize || need_csize || need_off) return MF3D_E_ZIP;
    return MF3D_OK;
}

static int z_find_eocd(const mf3d_u8 *data, mf3d_u32 size, mf3d_u32 *eocd)
{
    mf3d_u32 p;
    if (size < 22) return MF3D_E_ZIP;
    p = size - 22;
    while (1) {
        if (z_rd32(data + p) == MF3D_ZIP_EOCD) { *eocd = p; return MF3D_OK; }
        if (p == 0 || size - p > 65557UL) break;
        p--;
    }
    return MF3D_E_ZIP;
}

static int z_parse_zip64_eocd(const mf3d_u8 *data, mf3d_u32 size, mf3d_u32 eocd,
                              mf3d_z64_u64 *total, mf3d_z64_u64 *cd_size, mf3d_z64_u64 *cd_off)
{
    mf3d_u32 loc;
    mf3d_z64_u64 eocd64_off;
    if (eocd < 20) return MF3D_E_ZIP;
    loc = eocd - 20;
    if (z_rd32(data + loc) != MF3D_ZIP64_LOC) return MF3D_E_ZIP;
    eocd64_off = z_rd64(data + loc + 8);
    if (!z_u64_fits32(eocd64_off) || eocd64_off.lo + 56UL > size) return MF3D_E_RANGE;
    if (z_rd32(data + eocd64_off.lo) != MF3D_ZIP64_EOCD) return MF3D_E_ZIP;
    *total = z_rd64(data + eocd64_off.lo + 32);
    *cd_size = z_rd64(data + eocd64_off.lo + 40);
    *cd_off = z_rd64(data + eocd64_off.lo + 48);
    return MF3D_OK;
}

static int z_descriptor_after(const mf3d_u8 *data, mf3d_u32 size, mf3d_z64_entry *e)
{
    mf3d_u32 p;
    mf3d_u32 crc;
    mf3d_u32 min_len;
    int zip64_sizes;
    mf3d_z64_u64 tmp1;
    mf3d_z64_u64 tmp2;

    if (!z_u64_fits32(e->data_off) || !z_u64_fits32(e->comp_size)) return MF3D_E_RANGE;
    p = e->data_off.lo + e->comp_size.lo;
    if (p >= size) return MF3D_OK;
    if (p + 4UL <= size && z_rd32(data + p) == MF3D_ZIP_DDESC) p += 4;
    min_len = 12;
    zip64_sizes = e->zip64_used ? 1 : 0;
    if (zip64_sizes) min_len = 20;
    if (p + min_len > size) return MF3D_OK;
    crc = z_rd32(data + p);
    if (crc != e->crc32) return MF3D_OK;
    if (zip64_sizes) {
        tmp1 = z_rd64(data + p + 4);
        tmp2 = z_rd64(data + p + 12);
        if (tmp1.lo == e->comp_size.lo && tmp1.hi == e->comp_size.hi && tmp2.lo == e->uncomp_size.lo && tmp2.hi == e->uncomp_size.hi) {
            e->data_descriptor_off = p;
            e->descriptor_used = 1;
        }
    } else {
        if (z_rd32(data + p + 4) == e->comp_size.lo && z_rd32(data + p + 8) == e->uncomp_size.lo) {
            e->data_descriptor_off = p;
            e->descriptor_used = 1;
        }
    }
    return MF3D_OK;
}

int mf3d_zip64_read(const mf3d_u8 *data, mf3d_u32 size, mf3d_z64_archive *zip)
{
    mf3d_u32 eocd;
    mf3d_z64_u64 total64;
    mf3d_z64_u64 cd_size64;
    mf3d_z64_u64 cd_off64;
    mf3d_u32 total16;
    mf3d_u32 cd_size32;
    mf3d_u32 cd_off32;
    mf3d_u32 p;
    mf3d_u32 i;
    int zip64_need;

    if (!data || !zip || size < 22) return MF3D_E_BADARG;
    zip->count = 0;
    zip->zip64_eocd_used = 0;
    zip->central_dir_off = z_u64_from32(0);
    zip->central_dir_size = z_u64_from32(0);

    if (z_find_eocd(data, size, &eocd) != MF3D_OK) return MF3D_E_ZIP;
    total16 = z_rd16(data + eocd + 10);
    cd_size32 = z_rd32(data + eocd + 12);
    cd_off32 = z_rd32(data + eocd + 16);
    total64 = z_u64_from32(total16);
    cd_size64 = z_u64_from32(cd_size32);
    cd_off64 = z_u64_from32(cd_off32);
    zip64_need = (total16 == 0xffffU || cd_size32 == 0xffffffffUL || cd_off32 == 0xffffffffUL);
    if (zip64_need) {
        if (z_parse_zip64_eocd(data, size, eocd, &total64, &cd_size64, &cd_off64) != MF3D_OK) return MF3D_E_ZIP;
        zip->zip64_eocd_used = 1;
    }
    if (!z_u64_fits32(total64) || !z_u64_fits32(cd_size64) || !z_u64_fits32(cd_off64)) return MF3D_E_RANGE;
    if (total64.lo > MF3D_Z64_MAX_ENTRIES) return MF3D_E_NOSPACE;
    if (cd_off64.lo + cd_size64.lo > size) return MF3D_E_ZIP;
    zip->central_dir_off = cd_off64;
    zip->central_dir_size = cd_size64;

    p = cd_off64.lo;
    for (i = 0; i < total64.lo; ++i) {
        mf3d_u16 namelen;
        mf3d_u16 extralen;
        mf3d_u16 commentlen;
        mf3d_z64_entry *e;
        mf3d_u32 namecopy;
        mf3d_u32 j;
        mf3d_u32 local;
        mf3d_u16 ln;
        mf3d_u16 le;
        int need_usize;
        int need_csize;
        int need_off;
        mf3d_z64_u64 data_off;

        if (p + 46UL > size || z_rd32(data + p) != MF3D_ZIP_CDIR) return MF3D_E_ZIP;
        namelen = z_rd16(data + p + 28);
        extralen = z_rd16(data + p + 30);
        commentlen = z_rd16(data + p + 32);
        if (p + 46UL + namelen + extralen + commentlen > size) return MF3D_E_ZIP;

        e = &zip->entries[zip->count];
        e->flag = z_rd16(data + p + 8);
        e->method = z_rd16(data + p + 10);
        e->crc32 = z_rd32(data + p + 16);
        e->comp_size = z_u64_from32(z_rd32(data + p + 20));
        e->uncomp_size = z_u64_from32(z_rd32(data + p + 24));
        e->local_off = z_u64_from32(z_rd32(data + p + 42));
        e->data_off = z_u64_from32(0);
        e->data_descriptor_off = 0;
        e->zip64_used = 0;
        e->descriptor_used = 0;

        namecopy = z_min_u32(namelen, MF3D_Z64_MAX_NAME - 1UL);
        for (j = 0; j < namecopy; ++j) e->name[j] = (char)data[p + 46UL + j];
        e->name[namecopy] = 0;

        need_csize = e->comp_size.lo == 0xffffffffUL;
        need_usize = e->uncomp_size.lo == 0xffffffffUL;
        need_off = e->local_off.lo == 0xffffffffUL;
        if (need_csize || need_usize || need_off) {
            int zr;
            zr = z_parse_zip64_extra(data + p + 46UL + namelen, extralen, need_usize, need_csize, need_off,
                                     &e->uncomp_size, &e->comp_size, &e->local_off);
            if (zr != MF3D_OK) return zr;
            e->zip64_used = 1;
        }
        if (!z_u64_fits32(e->local_off) || !z_u64_fits32(e->comp_size) || !z_u64_fits32(e->uncomp_size)) return MF3D_E_RANGE;
        local = e->local_off.lo;
        if (local + 30UL > size || z_rd32(data + local) != MF3D_ZIP_LOCAL) return MF3D_E_ZIP;
        ln = z_rd16(data + local + 26);
        le = z_rd16(data + local + 28);
        if (local + 30UL + ln + le > size) return MF3D_E_ZIP;
        if (!z_u64_add32(e->local_off, 30UL + ln + le, &data_off)) return MF3D_E_RANGE;
        e->data_off = data_off;
        if (!z_u64_fits32(e->data_off)) return MF3D_E_RANGE;
        if (e->data_off.lo + e->comp_size.lo > size) return MF3D_E_ZIP;
        if ((e->flag & MF3D_Z64_GP_DATA_DESCRIPTOR) != 0U) z_descriptor_after(data, size, e);

        zip->count++;
        p += 46UL + namelen + extralen + commentlen;
    }
    return MF3D_OK;
}

int mf3d_zip64_find(mf3d_z64_archive *zip, const char *name)
{
    mf3d_u32 i;
    if (!zip || !name) return -1;
    for (i = 0; i < zip->count; ++i) if (z_streq(zip->entries[i].name, name)) return (int)i;
    return -1;
}

int mf3d_zip64_find_first_model(mf3d_z64_archive *zip)
{
    mf3d_u32 i;
    if (!zip) return -1;
    i = (mf3d_u32)mf3d_zip64_find(zip, "3D/3dmodel.model");
    if (i != (mf3d_u32)-1) return (int)i;
    for (i = 0; i < zip->count; ++i) if (z_ends_with(zip->entries[i].name, ".model")) return (int)i;
    return -1;
}

int mf3d_zip64_extract(const mf3d_u8 *data, mf3d_u32 size, mf3d_z64_archive *zip, int idx,
                       mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len)
{
    mf3d_z64_entry *e;
    mf3d_u32 i;
    (void)size;
    if (!data || !zip || idx < 0 || (mf3d_u32)idx >= zip->count || !dst || !dst_len) return MF3D_E_BADARG;
    e = &zip->entries[idx];
    if (!z_u64_fits32(e->uncomp_size) || !z_u64_fits32(e->comp_size) || !z_u64_fits32(e->data_off)) return MF3D_E_RANGE;
    if (e->uncomp_size.lo > dst_cap) return MF3D_E_NOSPACE;
    if (e->method == MF3D_Z64_METHOD_STORE) {
        if (e->data_off.lo + e->uncomp_size.lo > size) return MF3D_E_ZIP;
        for (i = 0; i < e->uncomp_size.lo; ++i) dst[i] = data[e->data_off.lo + i];
        *dst_len = e->uncomp_size.lo;
        if (*dst_len < dst_cap) dst[*dst_len] = 0;
        return MF3D_OK;
    }
    if (e->method == MF3D_Z64_METHOD_DEFLATE) {
        if (e->data_off.lo + e->comp_size.lo > size) return MF3D_E_ZIP;
        return mf3d_zip64_deflate_inflate_cb(0, data + e->data_off.lo, e->comp_size.lo, dst, dst_cap, dst_len);
    }
    return MF3D_E_UNSUPPORTED;
}

int mf3d_zip64_extract_named(const mf3d_u8 *data, mf3d_u32 size, const char *name,
                             mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len)
{
    mf3d_z64_archive zip;
    int r;
    int idx;
    if (!name) return MF3D_E_BADARG;
    r = mf3d_zip64_read(data, size, &zip);
    if (r != MF3D_OK) return r;
    idx = mf3d_zip64_find(&zip, name);
    if (idx < 0) return MF3D_E_NOTFOUND;
    return mf3d_zip64_extract(data, size, &zip, idx, dst, dst_cap, dst_len);
}
