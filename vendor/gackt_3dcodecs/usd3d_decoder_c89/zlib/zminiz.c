#include "zminiz.h"

#include <string.h>

#define ZMINIZ_MAX_BITS 15
#define ZMINIZ_MAX_LITLEN 288
#define ZMINIZ_MAX_DIST 32
#define ZMINIZ_MAX_CODELEN 19

struct zminiz_bits_s {
    const zminiz_u8 *src;
    zminiz_u32 size;
    zminiz_u32 pos;
    unsigned long bitbuf;
    int bitcount;
};

typedef struct zminiz_bits_s zminiz_bits;

struct zminiz_huff_s {
    unsigned short code[ZMINIZ_MAX_LITLEN];
    unsigned char len[ZMINIZ_MAX_LITLEN];
    int num;
};

typedef struct zminiz_huff_s zminiz_huff;

static int zminiz_read_bits(zminiz_bits *bs, int n, unsigned long *out_v);
static int zminiz_build_huff(zminiz_huff *h, const unsigned char *lengths, int num);
static int zminiz_decode_symbol(zminiz_bits *bs, const zminiz_huff *h, int *out_sym);
static int zminiz_inflate_blocks(zminiz_bits *bs, zminiz_u8 *dst, zminiz_u32 dst_cap, zminiz_u32 *dst_len);
static int zminiz_inflate_stored(zminiz_bits *bs, zminiz_u8 *dst, zminiz_u32 dst_cap, zminiz_u32 *out_pos);
static int zminiz_inflate_huff(zminiz_bits *bs, const zminiz_huff *lit, const zminiz_huff *dist, zminiz_u8 *dst, zminiz_u32 dst_cap, zminiz_u32 *out_pos);
static int zminiz_prepare_fixed(zminiz_huff *lit, zminiz_huff *dist);
static int zminiz_prepare_dynamic(zminiz_bits *bs, zminiz_huff *lit, zminiz_huff *dist);
static zminiz_u32 zminiz_read_be32_tail(const zminiz_u8 *src, zminiz_u32 src_len);

static const int zminiz_len_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10,
    11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115,
    131, 163, 195, 227, 258
};

static const int zminiz_len_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4,
    5, 5, 5, 5, 0
};

static const int zminiz_dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13,
    17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073,
    4097, 6145, 8193, 12289, 16385, 24577
};

static const int zminiz_dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2,
    3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10,
    11, 11, 12, 12, 13, 13
};

const char *zminiz_result_name(int code)
{
    switch (code) {
    case ZMINIZ_OK: return "ZMINIZ_OK";
    case ZMINIZ_ERR_BAD_ARG: return "ZMINIZ_ERR_BAD_ARG";
    case ZMINIZ_ERR_INPUT: return "ZMINIZ_ERR_INPUT";
    case ZMINIZ_ERR_OUTPUT: return "ZMINIZ_ERR_OUTPUT";
    case ZMINIZ_ERR_FORMAT: return "ZMINIZ_ERR_FORMAT";
    case ZMINIZ_ERR_UNSUPPORTED: return "ZMINIZ_ERR_UNSUPPORTED";
    case ZMINIZ_ERR_CHECKSUM: return "ZMINIZ_ERR_CHECKSUM";
    default: return "ZMINIZ_ERR_UNKNOWN";
    }
}

zminiz_u32 zminiz_adler32(const zminiz_u8 *buf, zminiz_u32 len)
{
    zminiz_u32 a;
    zminiz_u32 b;
    zminiz_u32 i;

    a = 1UL;
    b = 0UL;
    if (buf == 0) return 1UL;
    for (i = 0UL; i < len; ++i) {
        a += (zminiz_u32)buf[i];
        if (a >= 65521UL) a %= 65521UL;
        b += a;
        if (b >= 65521UL) b %= 65521UL;
    }
    return (b << 16) | a;
}

int zminiz_inflate_raw(const zminiz_u8 *src,
                       zminiz_u32 src_len,
                       zminiz_u8 *dst,
                       zminiz_u32 dst_cap,
                       zminiz_u32 *dst_len)
{
    zminiz_bits bs;

    if (src == 0 || dst == 0 || dst_len == 0) return ZMINIZ_ERR_BAD_ARG;
    memset(&bs, 0, sizeof(bs));
    bs.src = src;
    bs.size = src_len;
    *dst_len = 0UL;
    return zminiz_inflate_blocks(&bs, dst, dst_cap, dst_len);
}

int zminiz_inflate_zlib(const zminiz_u8 *src,
                        zminiz_u32 src_len,
                        zminiz_u8 *dst,
                        zminiz_u32 dst_cap,
                        zminiz_u32 *dst_len)
{
    int cm;
    int cinfo;
    unsigned int header;
    zminiz_u32 out_len;
    zminiz_u32 want;
    zminiz_u32 got;
    int r;

    if (src == 0 || dst == 0 || dst_len == 0) return ZMINIZ_ERR_BAD_ARG;
    if (src_len < 6UL) return ZMINIZ_ERR_INPUT;
    cm = (int)(src[0] & 15U);
    cinfo = (int)((src[0] >> 4) & 15U);
    header = ((unsigned int)src[0] << 8) | (unsigned int)src[1];
    if (cm != 8) return ZMINIZ_ERR_UNSUPPORTED;
    if (cinfo > 7) return ZMINIZ_ERR_UNSUPPORTED;
    if ((header % 31U) != 0U) return ZMINIZ_ERR_FORMAT;
    if ((src[1] & 32U) != 0U) return ZMINIZ_ERR_UNSUPPORTED;

    out_len = 0UL;
    r = zminiz_inflate_raw(src + 2, src_len - 6UL, dst, dst_cap, &out_len);
    if (r != ZMINIZ_OK) return r;
    want = zminiz_read_be32_tail(src, src_len);
    got = zminiz_adler32(dst, out_len);
    if (want != got) return ZMINIZ_ERR_CHECKSUM;
    *dst_len = out_len;
    return ZMINIZ_OK;
}

static zminiz_u32 zminiz_read_be32_tail(const zminiz_u8 *src, zminiz_u32 src_len)
{
    zminiz_u32 p;
    p = src_len - 4UL;
    return ((zminiz_u32)src[p] << 24) |
           ((zminiz_u32)src[p + 1UL] << 16) |
           ((zminiz_u32)src[p + 2UL] << 8) |
           (zminiz_u32)src[p + 3UL];
}

static int zminiz_read_bits(zminiz_bits *bs, int n, unsigned long *out_v)
{
    unsigned long v;
    int shift;

    if (n < 0 || n > 24 || out_v == 0) return ZMINIZ_ERR_BAD_ARG;
    while (bs->bitcount < n) {
        if (bs->pos >= bs->size) return ZMINIZ_ERR_INPUT;
        bs->bitbuf |= ((unsigned long)bs->src[bs->pos]) << bs->bitcount;
        bs->pos++;
        bs->bitcount += 8;
    }
    if (n == 0) {
        *out_v = 0UL;
        return ZMINIZ_OK;
    }
    v = bs->bitbuf & ((1UL << n) - 1UL);
    shift = n;
    bs->bitbuf >>= shift;
    bs->bitcount -= n;
    *out_v = v;
    return ZMINIZ_OK;
}

static int zminiz_build_huff(zminiz_huff *h, const unsigned char *lengths, int num)
{
    int count[ZMINIZ_MAX_BITS + 1];
    int next_code[ZMINIZ_MAX_BITS + 1];
    int code;
    int bits;
    int n;
    int len;

    if (h == 0 || lengths == 0 || num <= 0 || num > ZMINIZ_MAX_LITLEN) return ZMINIZ_ERR_BAD_ARG;
    memset(h, 0, sizeof(*h));
    h->num = num;
    for (bits = 0; bits <= ZMINIZ_MAX_BITS; ++bits) count[bits] = 0;
    for (n = 0; n < num; ++n) {
        len = (int)lengths[n];
        if (len < 0 || len > ZMINIZ_MAX_BITS) return ZMINIZ_ERR_FORMAT;
        h->len[n] = (unsigned char)len;
        if (len != 0) count[len]++;
    }
    code = 0;
    count[0] = 0;
    for (bits = 1; bits <= ZMINIZ_MAX_BITS; ++bits) {
        code = (code + count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    for (n = 0; n < num; ++n) {
        len = (int)h->len[n];
        if (len != 0) {
            h->code[n] = (unsigned short)next_code[len];
            next_code[len]++;
        }
    }
    return ZMINIZ_OK;
}

static int zminiz_decode_symbol(zminiz_bits *bs, const zminiz_huff *h, int *out_sym)
{
    unsigned long one;
    unsigned int code;
    int len;
    int sym;
    int r;

    if (bs == 0 || h == 0 || out_sym == 0) return ZMINIZ_ERR_BAD_ARG;
    code = 0U;
    for (len = 1; len <= ZMINIZ_MAX_BITS; ++len) {
        r = zminiz_read_bits(bs, 1, &one);
        if (r != ZMINIZ_OK) return r;
        code = (code << 1) | (unsigned int)one;
        for (sym = 0; sym < h->num; ++sym) {
            if ((int)h->len[sym] == len && (unsigned int)h->code[sym] == code) {
                *out_sym = sym;
                return ZMINIZ_OK;
            }
        }
    }
    return ZMINIZ_ERR_FORMAT;
}

static int zminiz_inflate_blocks(zminiz_bits *bs, zminiz_u8 *dst, zminiz_u32 dst_cap, zminiz_u32 *dst_len)
{
    int final_block;
    int block_type;
    int r;
    unsigned long v;
    zminiz_huff lit;
    zminiz_huff dist;

    final_block = 0;
    while (!final_block) {
        r = zminiz_read_bits(bs, 1, &v);
        if (r != ZMINIZ_OK) return r;
        final_block = (int)v;
        r = zminiz_read_bits(bs, 2, &v);
        if (r != ZMINIZ_OK) return r;
        block_type = (int)v;
        if (block_type == 0) {
            r = zminiz_inflate_stored(bs, dst, dst_cap, dst_len);
            if (r != ZMINIZ_OK) return r;
        } else if (block_type == 1) {
            r = zminiz_prepare_fixed(&lit, &dist);
            if (r != ZMINIZ_OK) return r;
            r = zminiz_inflate_huff(bs, &lit, &dist, dst, dst_cap, dst_len);
            if (r != ZMINIZ_OK) return r;
        } else if (block_type == 2) {
            r = zminiz_prepare_dynamic(bs, &lit, &dist);
            if (r != ZMINIZ_OK) return r;
            r = zminiz_inflate_huff(bs, &lit, &dist, dst, dst_cap, dst_len);
            if (r != ZMINIZ_OK) return r;
        } else {
            return ZMINIZ_ERR_FORMAT;
        }
    }
    return ZMINIZ_OK;
}

static int zminiz_inflate_stored(zminiz_bits *bs, zminiz_u8 *dst, zminiz_u32 dst_cap, zminiz_u32 *out_pos)
{
    unsigned int len;
    unsigned int nlen;
    zminiz_u32 i;

    bs->bitbuf = 0UL;
    bs->bitcount = 0;
    if (bs->pos + 4UL > bs->size) return ZMINIZ_ERR_INPUT;
    len = (unsigned int)bs->src[bs->pos] | ((unsigned int)bs->src[bs->pos + 1UL] << 8);
    nlen = (unsigned int)bs->src[bs->pos + 2UL] | ((unsigned int)bs->src[bs->pos + 3UL] << 8);
    bs->pos += 4UL;
    if (((len ^ 0xffffU) & 0xffffU) != nlen) return ZMINIZ_ERR_FORMAT;
    if (bs->pos + (zminiz_u32)len > bs->size) return ZMINIZ_ERR_INPUT;
    if (*out_pos + (zminiz_u32)len > dst_cap) return ZMINIZ_ERR_OUTPUT;
    for (i = 0UL; i < (zminiz_u32)len; ++i) {
        dst[*out_pos] = bs->src[bs->pos + i];
        (*out_pos)++;
    }
    bs->pos += (zminiz_u32)len;
    return ZMINIZ_OK;
}

static int zminiz_inflate_huff(zminiz_bits *bs, const zminiz_huff *lit, const zminiz_huff *dist, zminiz_u8 *dst, zminiz_u32 dst_cap, zminiz_u32 *out_pos)
{
    int sym;
    int r;
    int len_index;
    int dist_sym;
    int copy_len;
    int distance;
    int extra;
    unsigned long v;
    zminiz_u32 from;
    int i;

    while (1) {
        r = zminiz_decode_symbol(bs, lit, &sym);
        if (r != ZMINIZ_OK) return r;
        if (sym < 256) {
            if (*out_pos >= dst_cap) return ZMINIZ_ERR_OUTPUT;
            dst[*out_pos] = (zminiz_u8)sym;
            (*out_pos)++;
        } else if (sym == 256) {
            return ZMINIZ_OK;
        } else if (sym >= 257 && sym <= 285) {
            len_index = sym - 257;
            copy_len = zminiz_len_base[len_index];
            extra = zminiz_len_extra[len_index];
            if (extra != 0) {
                r = zminiz_read_bits(bs, extra, &v);
                if (r != ZMINIZ_OK) return r;
                copy_len += (int)v;
            }
            r = zminiz_decode_symbol(bs, dist, &dist_sym);
            if (r != ZMINIZ_OK) return r;
            if (dist_sym < 0 || dist_sym >= 30) return ZMINIZ_ERR_FORMAT;
            distance = zminiz_dist_base[dist_sym];
            extra = zminiz_dist_extra[dist_sym];
            if (extra != 0) {
                r = zminiz_read_bits(bs, extra, &v);
                if (r != ZMINIZ_OK) return r;
                distance += (int)v;
            }
            if (distance <= 0 || (zminiz_u32)distance > *out_pos) return ZMINIZ_ERR_FORMAT;
            if (*out_pos + (zminiz_u32)copy_len > dst_cap) return ZMINIZ_ERR_OUTPUT;
            from = *out_pos - (zminiz_u32)distance;
            for (i = 0; i < copy_len; ++i) {
                dst[*out_pos] = dst[from];
                (*out_pos)++;
                from++;
            }
        } else {
            return ZMINIZ_ERR_FORMAT;
        }
    }
}

static int zminiz_prepare_fixed(zminiz_huff *lit, zminiz_huff *dist)
{
    unsigned char ll[ZMINIZ_MAX_LITLEN];
    unsigned char dd[ZMINIZ_MAX_DIST];
    int i;
    int r;

    for (i = 0; i <= 143; ++i) ll[i] = 8U;
    for (i = 144; i <= 255; ++i) ll[i] = 9U;
    for (i = 256; i <= 279; ++i) ll[i] = 7U;
    for (i = 280; i <= 287; ++i) ll[i] = 8U;
    for (i = 0; i < ZMINIZ_MAX_DIST; ++i) dd[i] = 5U;
    r = zminiz_build_huff(lit, ll, ZMINIZ_MAX_LITLEN);
    if (r != ZMINIZ_OK) return r;
    return zminiz_build_huff(dist, dd, ZMINIZ_MAX_DIST);
}

static int zminiz_prepare_dynamic(zminiz_bits *bs, zminiz_huff *lit, zminiz_huff *dist)
{
    static const int order[ZMINIZ_MAX_CODELEN] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };
    unsigned char clen[ZMINIZ_MAX_CODELEN];
    unsigned char all_lengths[ZMINIZ_MAX_LITLEN + ZMINIZ_MAX_DIST];
    unsigned char lit_lengths[ZMINIZ_MAX_LITLEN];
    unsigned char dist_lengths[ZMINIZ_MAX_DIST];
    zminiz_huff chuff;
    unsigned long v;
    int hlit;
    int hdist;
    int hclen;
    int total;
    int i;
    int sym;
    int prev;
    int repeat;
    int r;

    r = zminiz_read_bits(bs, 5, &v);
    if (r != ZMINIZ_OK) return r;
    hlit = (int)v + 257;
    r = zminiz_read_bits(bs, 5, &v);
    if (r != ZMINIZ_OK) return r;
    hdist = (int)v + 1;
    r = zminiz_read_bits(bs, 4, &v);
    if (r != ZMINIZ_OK) return r;
    hclen = (int)v + 4;
    if (hlit > ZMINIZ_MAX_LITLEN || hdist > ZMINIZ_MAX_DIST) return ZMINIZ_ERR_FORMAT;

    for (i = 0; i < ZMINIZ_MAX_CODELEN; ++i) clen[i] = 0U;
    for (i = 0; i < hclen; ++i) {
        r = zminiz_read_bits(bs, 3, &v);
        if (r != ZMINIZ_OK) return r;
        clen[order[i]] = (unsigned char)v;
    }
    r = zminiz_build_huff(&chuff, clen, ZMINIZ_MAX_CODELEN);
    if (r != ZMINIZ_OK) return r;

    total = hlit + hdist;
    for (i = 0; i < total; ++i) all_lengths[i] = 0U;
    i = 0;
    prev = 0;
    while (i < total) {
        r = zminiz_decode_symbol(bs, &chuff, &sym);
        if (r != ZMINIZ_OK) return r;
        if (sym >= 0 && sym <= 15) {
            all_lengths[i] = (unsigned char)sym;
            prev = sym;
            i++;
        } else if (sym == 16) {
            if (i == 0) return ZMINIZ_ERR_FORMAT;
            r = zminiz_read_bits(bs, 2, &v);
            if (r != ZMINIZ_OK) return r;
            repeat = 3 + (int)v;
            if (i + repeat > total) return ZMINIZ_ERR_FORMAT;
            while (repeat > 0) {
                all_lengths[i] = (unsigned char)prev;
                i++;
                repeat--;
            }
        } else if (sym == 17) {
            r = zminiz_read_bits(bs, 3, &v);
            if (r != ZMINIZ_OK) return r;
            repeat = 3 + (int)v;
            if (i + repeat > total) return ZMINIZ_ERR_FORMAT;
            prev = 0;
            while (repeat > 0) {
                all_lengths[i] = 0U;
                i++;
                repeat--;
            }
        } else if (sym == 18) {
            r = zminiz_read_bits(bs, 7, &v);
            if (r != ZMINIZ_OK) return r;
            repeat = 11 + (int)v;
            if (i + repeat > total) return ZMINIZ_ERR_FORMAT;
            prev = 0;
            while (repeat > 0) {
                all_lengths[i] = 0U;
                i++;
                repeat--;
            }
        } else {
            return ZMINIZ_ERR_FORMAT;
        }
    }

    for (i = 0; i < ZMINIZ_MAX_LITLEN; ++i) lit_lengths[i] = 0U;
    for (i = 0; i < ZMINIZ_MAX_DIST; ++i) dist_lengths[i] = 0U;
    for (i = 0; i < hlit; ++i) lit_lengths[i] = all_lengths[i];
    for (i = 0; i < hdist; ++i) dist_lengths[i] = all_lengths[hlit + i];
    r = zminiz_build_huff(lit, lit_lengths, ZMINIZ_MAX_LITLEN);
    if (r != ZMINIZ_OK) return r;
    return zminiz_build_huff(dist, dist_lengths, ZMINIZ_MAX_DIST);
}
