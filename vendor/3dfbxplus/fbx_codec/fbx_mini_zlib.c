#include "fbx_mini_zlib.h"

#include <string.h>

#define FBX_MINIZ_MAX_BITS 15
#define FBX_MINIZ_MAX_LITLEN 288
#define FBX_MINIZ_MAX_DIST 32
#define FBX_MINIZ_MAX_CLEN 19
#define FBX_MINIZ_MAX_SYMBOLS (FBX_MINIZ_MAX_LITLEN + FBX_MINIZ_MAX_DIST)
#define FBX_MINIZ_ADLER_MOD 65521u

typedef struct FbxMiniBitstreamTag {
    const fbx_u8 *src;
    size_t size;
    size_t pos;
    fbx_u32 bitbuf;
    int bitcount;
} FbxMiniBitstream;

typedef struct FbxMiniHuffmanTag {
    unsigned short code[FBX_MINIZ_MAX_SYMBOLS];
    unsigned char length[FBX_MINIZ_MAX_SYMBOLS];
    int symbol_count;
    int max_bits;
} FbxMiniHuffman;

typedef struct FbxMiniOutputTag {
    fbx_u8 *dst;
    size_t capacity;
    size_t pos;
} FbxMiniOutput;

static const unsigned short fbx_miniz_len_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
    35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const unsigned char fbx_miniz_len_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
    3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const unsigned short fbx_miniz_dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,
    193,257,385,513,769,1025,1537,2049,3073,4097,
    6145,8193,12289,16385,24577
};
static const unsigned char fbx_miniz_dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,
    6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};
static const unsigned char fbx_miniz_clen_order[19] = {
    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
};

static void fbx_mini_bits_init(FbxMiniBitstream *bs, const fbx_u8 *src, size_t size)
{
    bs->src = src;
    bs->size = size;
    bs->pos = 0u;
    bs->bitbuf = 0u;
    bs->bitcount = 0;
}

static int fbx_mini_read_bits(FbxMiniBitstream *bs, int bits, fbx_u32 *out)
{
    fbx_u32 val;
    int shift;

    if (!bs || !out || bits < 0 || bits > 16) return 0;
    val = 0u;
    shift = 0;
    while (bits > 0) {
        if (bs->bitcount == 0) {
            if (bs->pos >= bs->size) return 0;
            bs->bitbuf = (fbx_u32)bs->src[bs->pos++];
            bs->bitcount = 8;
        }
        {
            int take;
            fbx_u32 mask;
            take = bits < bs->bitcount ? bits : bs->bitcount;
            mask = ((fbx_u32)1u << take) - 1u;
            val |= (bs->bitbuf & mask) << shift;
            bs->bitbuf >>= take;
            bs->bitcount -= take;
            bits -= take;
            shift += take;
        }
    }
    *out = val;
    return 1;
}

static void fbx_mini_byte_align(FbxMiniBitstream *bs)
{
    if (!bs) return;
    bs->bitbuf = 0u;
    bs->bitcount = 0;
}

static int fbx_mini_huff_build(FbxMiniHuffman *h, const unsigned char *lengths, int count)
{
    int bl_count[FBX_MINIZ_MAX_BITS + 1];
    int next_code[FBX_MINIZ_MAX_BITS + 1];
    int code;
    int bits;
    int i;
    int any;

    if (!h || !lengths || count <= 0 || count > FBX_MINIZ_MAX_SYMBOLS) return FBX_MINIZ_BAD_ARGUMENT;
    memset(h, 0, sizeof(*h));
    memset(bl_count, 0, sizeof(bl_count));
    memset(next_code, 0, sizeof(next_code));
    h->symbol_count = count;
    h->max_bits = 0;
    any = 0;

    for (i = 0; i < count; ++i) {
        if (lengths[i] > FBX_MINIZ_MAX_BITS) return FBX_MINIZ_BAD_HUFFMAN;
        h->length[i] = lengths[i];
        if (lengths[i] != 0u) {
            bl_count[lengths[i]]++;
            if ((int)lengths[i] > h->max_bits) h->max_bits = (int)lengths[i];
            any = 1;
        }
    }
    if (!any) return FBX_MINIZ_BAD_HUFFMAN;

    code = 0;
    for (bits = 1; bits <= FBX_MINIZ_MAX_BITS; ++bits) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }

    for (i = 0; i < count; ++i) {
        int len;
        len = h->length[i];
        if (len != 0) {
            h->code[i] = (unsigned short)next_code[len];
            next_code[len]++;
        }
    }
    return FBX_MINIZ_OK;
}

static int fbx_mini_huff_decode(FbxMiniBitstream *bs, const FbxMiniHuffman *h, int *out_sym)
{
    int count[FBX_MINIZ_MAX_BITS + 1];
    int first;
    int index;
    int code;
    int len;
    int i;

    if (!bs || !h || !out_sym) return FBX_MINIZ_BAD_ARGUMENT;
    memset(count, 0, sizeof(count));
    for (i = 0; i < h->symbol_count; ++i) {
        if (h->length[i] > 0 && h->length[i] <= FBX_MINIZ_MAX_BITS) count[h->length[i]]++;
    }

    first = 0;
    index = 0;
    code = 0;
    for (len = 1; len <= FBX_MINIZ_MAX_BITS; ++len) {
        fbx_u32 bit;
        if (!fbx_mini_read_bits(bs, 1, &bit)) return FBX_MINIZ_INPUT_OVERFLOW;
        code |= (int)bit;
        if (code - count[len] < first) {
            int ordinal;
            ordinal = index + (code - first);
            for (i = 0; i < h->symbol_count; ++i) {
                if ((int)h->length[i] == len) {
                    if (ordinal == index) {
                        *out_sym = i;
                        return FBX_MINIZ_OK;
                    }
                    index++;
                }
            }
            return FBX_MINIZ_BAD_HUFFMAN;
        }
        index += count[len];
        first += count[len];
        first <<= 1;
        code <<= 1;
    }
    return FBX_MINIZ_BAD_HUFFMAN;
}

static int fbx_mini_build_fixed(FbxMiniHuffman *litlen, FbxMiniHuffman *dist)
{
    unsigned char ll[FBX_MINIZ_MAX_LITLEN];
    unsigned char dd[FBX_MINIZ_MAX_DIST];
    int i;

    for (i = 0; i <= 143; ++i) ll[i] = 8u;
    for (i = 144; i <= 255; ++i) ll[i] = 9u;
    for (i = 256; i <= 279; ++i) ll[i] = 7u;
    for (i = 280; i <= 287; ++i) ll[i] = 8u;
    for (i = 0; i < FBX_MINIZ_MAX_DIST; ++i) dd[i] = 5u;

    i = fbx_mini_huff_build(litlen, ll, FBX_MINIZ_MAX_LITLEN);
    if (i != FBX_MINIZ_OK) return i;
    return fbx_mini_huff_build(dist, dd, FBX_MINIZ_MAX_DIST);
}

static int fbx_mini_out_byte(FbxMiniOutput *out, fbx_u8 b)
{
    if (!out || (!out->dst && out->capacity != 0u)) return FBX_MINIZ_BAD_ARGUMENT;
    if (out->pos >= out->capacity) return FBX_MINIZ_OUTPUT_OVERFLOW;
    out->dst[out->pos++] = b;
    return FBX_MINIZ_OK;
}

static int fbx_mini_copy_distance(FbxMiniOutput *out, size_t dist, size_t len)
{
    size_t i;
    if (!out || dist == 0u || dist > out->pos) return FBX_MINIZ_BAD_DISTANCE;
    for (i = 0u; i < len; ++i) {
        fbx_u8 b;
        if (out->pos >= out->capacity) return FBX_MINIZ_OUTPUT_OVERFLOW;
        b = out->dst[out->pos - dist];
        out->dst[out->pos++] = b;
    }
    return FBX_MINIZ_OK;
}

static int fbx_mini_decode_compressed(FbxMiniBitstream *bs,
                                      FbxMiniOutput *out,
                                      const FbxMiniHuffman *litlen,
                                      const FbxMiniHuffman *dist)
{
    for (;;) {
        int sym;
        int r;
        r = fbx_mini_huff_decode(bs, litlen, &sym);
        if (r != FBX_MINIZ_OK) return r;

        if (sym < 256) {
            r = fbx_mini_out_byte(out, (fbx_u8)sym);
            if (r != FBX_MINIZ_OK) return r;
        } else if (sym == 256) {
            return FBX_MINIZ_OK;
        } else if (sym >= 257 && sym <= 285) {
            int len_index;
            fbx_u32 extra;
            size_t match_len;
            int dist_sym;
            size_t distance;

            len_index = sym - 257;
            match_len = (size_t)fbx_miniz_len_base[len_index];
            extra = 0u;
            if (fbx_miniz_len_extra[len_index] != 0u) {
                if (!fbx_mini_read_bits(bs, fbx_miniz_len_extra[len_index], &extra)) return FBX_MINIZ_INPUT_OVERFLOW;
                match_len += (size_t)extra;
            }

            r = fbx_mini_huff_decode(bs, dist, &dist_sym);
            if (r != FBX_MINIZ_OK) return r;
            if (dist_sym < 0 || dist_sym >= 30) return FBX_MINIZ_BAD_DISTANCE;
            distance = (size_t)fbx_miniz_dist_base[dist_sym];
            extra = 0u;
            if (fbx_miniz_dist_extra[dist_sym] != 0u) {
                if (!fbx_mini_read_bits(bs, fbx_miniz_dist_extra[dist_sym], &extra)) return FBX_MINIZ_INPUT_OVERFLOW;
                distance += (size_t)extra;
            }
            r = fbx_mini_copy_distance(out, distance, match_len);
            if (r != FBX_MINIZ_OK) return r;
        } else {
            return FBX_MINIZ_BAD_HUFFMAN;
        }
    }
}

static int fbx_mini_build_dynamic(FbxMiniBitstream *bs, FbxMiniHuffman *litlen, FbxMiniHuffman *dist)
{
    fbx_u32 hlit;
    fbx_u32 hdist;
    fbx_u32 hclen;
    unsigned char clen[FBX_MINIZ_MAX_CLEN];
    unsigned char lengths[FBX_MINIZ_MAX_SYMBOLS];
    FbxMiniHuffman clen_huff;
    int total;
    int i;
    int r;

    if (!fbx_mini_read_bits(bs, 5, &hlit)) return FBX_MINIZ_INPUT_OVERFLOW;
    if (!fbx_mini_read_bits(bs, 5, &hdist)) return FBX_MINIZ_INPUT_OVERFLOW;
    if (!fbx_mini_read_bits(bs, 4, &hclen)) return FBX_MINIZ_INPUT_OVERFLOW;
    hlit += 257u;
    hdist += 1u;
    hclen += 4u;
    if (hlit > FBX_MINIZ_MAX_LITLEN || hdist > FBX_MINIZ_MAX_DIST) return FBX_MINIZ_BAD_HUFFMAN;

    memset(clen, 0, sizeof(clen));
    for (i = 0; i < (int)hclen; ++i) {
        fbx_u32 v;
        if (!fbx_mini_read_bits(bs, 3, &v)) return FBX_MINIZ_INPUT_OVERFLOW;
        clen[fbx_miniz_clen_order[i]] = (unsigned char)v;
    }
    r = fbx_mini_huff_build(&clen_huff, clen, FBX_MINIZ_MAX_CLEN);
    if (r != FBX_MINIZ_OK) return r;

    total = (int)(hlit + hdist);
    memset(lengths, 0, sizeof(lengths));
    i = 0;
    while (i < total) {
        int sym;
        r = fbx_mini_huff_decode(bs, &clen_huff, &sym);
        if (r != FBX_MINIZ_OK) return r;
        if (sym <= 15) {
            lengths[i++] = (unsigned char)sym;
        } else if (sym == 16) {
            fbx_u32 rep;
            unsigned char prev;
            if (i == 0) return FBX_MINIZ_BAD_HUFFMAN;
            if (!fbx_mini_read_bits(bs, 2, &rep)) return FBX_MINIZ_INPUT_OVERFLOW;
            rep += 3u;
            prev = lengths[i - 1];
            while (rep-- != 0u && i < total) lengths[i++] = prev;
        } else if (sym == 17) {
            fbx_u32 rep2;
            if (!fbx_mini_read_bits(bs, 3, &rep2)) return FBX_MINIZ_INPUT_OVERFLOW;
            rep2 += 3u;
            while (rep2-- != 0u && i < total) lengths[i++] = 0u;
        } else if (sym == 18) {
            fbx_u32 rep3;
            if (!fbx_mini_read_bits(bs, 7, &rep3)) return FBX_MINIZ_INPUT_OVERFLOW;
            rep3 += 11u;
            while (rep3-- != 0u && i < total) lengths[i++] = 0u;
        } else {
            return FBX_MINIZ_BAD_HUFFMAN;
        }
    }
    if (i != total) return FBX_MINIZ_BAD_HUFFMAN;

    r = fbx_mini_huff_build(litlen, lengths, (int)hlit);
    if (r != FBX_MINIZ_OK) return r;
    return fbx_mini_huff_build(dist, lengths + hlit, (int)hdist);
}

static int fbx_mini_decode_stored(FbxMiniBitstream *bs, FbxMiniOutput *out)
{
    fbx_u32 len_lo;
    fbx_u32 len_hi;
    fbx_u32 nlen_lo;
    fbx_u32 nlen_hi;
    fbx_u32 len;
    fbx_u32 nlen;
    fbx_u32 i;

    fbx_mini_byte_align(bs);
    if (!fbx_mini_read_bits(bs, 8, &len_lo)) return FBX_MINIZ_INPUT_OVERFLOW;
    if (!fbx_mini_read_bits(bs, 8, &len_hi)) return FBX_MINIZ_INPUT_OVERFLOW;
    if (!fbx_mini_read_bits(bs, 8, &nlen_lo)) return FBX_MINIZ_INPUT_OVERFLOW;
    if (!fbx_mini_read_bits(bs, 8, &nlen_hi)) return FBX_MINIZ_INPUT_OVERFLOW;
    len = len_lo | (len_hi << 8);
    nlen = nlen_lo | (nlen_hi << 8);
    if (((len ^ nlen) & 0xFFFFu) != 0xFFFFu) return FBX_MINIZ_BAD_BLOCK;

    for (i = 0u; i < len; ++i) {
        fbx_u32 b;
        int r;
        if (!fbx_mini_read_bits(bs, 8, &b)) return FBX_MINIZ_INPUT_OVERFLOW;
        r = fbx_mini_out_byte(out, (fbx_u8)b);
        if (r != FBX_MINIZ_OK) return r;
    }
    return FBX_MINIZ_OK;
}

int fbx_mini_deflate_decode(const fbx_u8 *src,
                            size_t src_size,
                            fbx_u8 *dst,
                            size_t dst_capacity,
                            size_t *out_written)
{
    FbxMiniBitstream bs;
    FbxMiniOutput out;
    int done;

    if (!src || (!dst && dst_capacity != 0u) || !out_written) return FBX_MINIZ_BAD_ARGUMENT;

    fbx_mini_bits_init(&bs, src, src_size);
    out.dst = dst;
    out.capacity = dst_capacity;
    out.pos = 0u;
    done = 0;

    while (!done) {
        fbx_u32 bfinal;
        fbx_u32 btype;
        int r;

        if (!fbx_mini_read_bits(&bs, 1, &bfinal)) return FBX_MINIZ_INPUT_OVERFLOW;
        if (!fbx_mini_read_bits(&bs, 2, &btype)) return FBX_MINIZ_INPUT_OVERFLOW;
        if (bfinal != 0u) done = 1;

        if (btype == 0u) {
            r = fbx_mini_decode_stored(&bs, &out);
        } else if (btype == 1u) {
            FbxMiniHuffman litlen;
            FbxMiniHuffman dist;
            r = fbx_mini_build_fixed(&litlen, &dist);
            if (r != FBX_MINIZ_OK) return r;
            r = fbx_mini_decode_compressed(&bs, &out, &litlen, &dist);
        } else if (btype == 2u) {
            FbxMiniHuffman litlen2;
            FbxMiniHuffman dist2;
            r = fbx_mini_build_dynamic(&bs, &litlen2, &dist2);
            if (r != FBX_MINIZ_OK) return r;
            r = fbx_mini_decode_compressed(&bs, &out, &litlen2, &dist2);
        } else {
            return FBX_MINIZ_BAD_BLOCK;
        }
        if (r != FBX_MINIZ_OK) return r;
    }

    *out_written = out.pos;
    return FBX_MINIZ_OK;
}

fbx_u32 fbx_mini_adler32(const fbx_u8 *data, size_t size)
{
    fbx_u32 s1;
    fbx_u32 s2;
    size_t i;

    if (!data && size != 0u) return 1u;
    s1 = 1u;
    s2 = 0u;
    for (i = 0u; i < size; ++i) {
        s1 += (fbx_u32)data[i];
        if (s1 >= FBX_MINIZ_ADLER_MOD) s1 %= FBX_MINIZ_ADLER_MOD;
        s2 += s1;
        if (s2 >= FBX_MINIZ_ADLER_MOD) s2 %= FBX_MINIZ_ADLER_MOD;
    }
    return (s2 << 16) | s1;
}

int fbx_mini_zlib_decode(const fbx_u8 *src,
                         size_t src_size,
                         fbx_u8 *dst,
                         size_t dst_capacity,
                         size_t *out_written)
{
    fbx_u32 cmf;
    fbx_u32 flg;
    fbx_u32 want_adler;
    fbx_u32 got_adler;
    int r;

    if (!src || (!dst && dst_capacity != 0u) || !out_written) return FBX_MINIZ_BAD_ARGUMENT;
    *out_written = 0u;
    if (src_size < 6u) return FBX_MINIZ_BAD_HEADER;

    cmf = (fbx_u32)src[0];
    flg = (fbx_u32)src[1];
    if ((cmf & 15u) != 8u) return FBX_MINIZ_UNSUPPORTED;
    if ((cmf >> 4) > 7u) return FBX_MINIZ_UNSUPPORTED;
    if ((((cmf << 8) + flg) % 31u) != 0u) return FBX_MINIZ_BAD_HEADER;
    if ((flg & 32u) != 0u) return FBX_MINIZ_UNSUPPORTED;

    r = fbx_mini_deflate_decode(src + 2u, src_size - 6u, dst, dst_capacity, out_written);
    if (r != FBX_MINIZ_OK) return r;

    want_adler = ((fbx_u32)src[src_size - 4u] << 24)
               | ((fbx_u32)src[src_size - 3u] << 16)
               | ((fbx_u32)src[src_size - 2u] << 8)
               |  (fbx_u32)src[src_size - 1u];
    got_adler = fbx_mini_adler32(dst, *out_written);
    if (got_adler != want_adler) return FBX_MINIZ_BAD_CHECKSUM;

    return FBX_MINIZ_OK;
}

size_t fbx_mini_zlib_store_bound(size_t src_size)
{
    size_t blocks;
    blocks = src_size / 65535u;
    if ((src_size % 65535u) != 0u || blocks == 0u) blocks += 1u;
    return 2u + src_size + blocks * 5u + 4u;
}

int fbx_mini_zlib_store_encode(const fbx_u8 *src,
                               size_t src_size,
                               fbx_u8 *dst,
                               size_t dst_capacity,
                               size_t *out_written)
{
    size_t required;
    size_t in_pos;
    size_t out_pos;
    fbx_u32 adler;

    if ((!src && src_size != 0u) || !dst || !out_written) return FBX_MINIZ_BAD_ARGUMENT;
    required = fbx_mini_zlib_store_bound(src_size);
    if (dst_capacity < required) return FBX_MINIZ_OUTPUT_OVERFLOW;

    out_pos = 0u;
    dst[out_pos++] = 0x78u;
    dst[out_pos++] = 0x01u;

    in_pos = 0u;
    if (src_size == 0u) {
        dst[out_pos++] = 1u;
        dst[out_pos++] = 0u;
        dst[out_pos++] = 0u;
        dst[out_pos++] = 0xFFu;
        dst[out_pos++] = 0xFFu;
    } else {
        while (in_pos < src_size) {
            size_t chunk;
            fbx_u32 len;
            fbx_u32 nlen;
            int final_block;
            chunk = src_size - in_pos;
            if (chunk > 65535u) chunk = 65535u;
            len = (fbx_u32)chunk;
            nlen = (~len) & 0xFFFFu;
            final_block = (in_pos + chunk >= src_size) ? 1 : 0;
            dst[out_pos++] = (fbx_u8)(final_block ? 1 : 0);
            dst[out_pos++] = (fbx_u8)(len & 0xFFu);
            dst[out_pos++] = (fbx_u8)((len >> 8) & 0xFFu);
            dst[out_pos++] = (fbx_u8)(nlen & 0xFFu);
            dst[out_pos++] = (fbx_u8)((nlen >> 8) & 0xFFu);
            memcpy(dst + out_pos, src + in_pos, chunk);
            out_pos += chunk;
            in_pos += chunk;
        }
    }

    adler = fbx_mini_adler32(src, src_size);
    dst[out_pos++] = (fbx_u8)((adler >> 24) & 0xFFu);
    dst[out_pos++] = (fbx_u8)((adler >> 16) & 0xFFu);
    dst[out_pos++] = (fbx_u8)((adler >> 8) & 0xFFu);
    dst[out_pos++] = (fbx_u8)(adler & 0xFFu);
    *out_written = out_pos;
    return FBX_MINIZ_OK;
}

const char *fbx_mini_zlib_result_string(int code)
{
    switch (code) {
        case FBX_MINIZ_OK: return "OK";
        case FBX_MINIZ_BAD_ARGUMENT: return "bad argument";
        case FBX_MINIZ_BAD_HEADER: return "bad zlib header";
        case FBX_MINIZ_UNSUPPORTED: return "unsupported zlib/deflate feature";
        case FBX_MINIZ_INPUT_OVERFLOW: return "truncated compressed input";
        case FBX_MINIZ_OUTPUT_OVERFLOW: return "inflated output buffer too small";
        case FBX_MINIZ_BAD_BLOCK: return "bad deflate block";
        case FBX_MINIZ_BAD_HUFFMAN: return "bad deflate huffman tree";
        case FBX_MINIZ_BAD_DISTANCE: return "bad deflate back-reference distance";
        case FBX_MINIZ_BAD_CHECKSUM: return "bad zlib adler32 checksum";
        default: return "unknown mini zlib error";
    }
}
