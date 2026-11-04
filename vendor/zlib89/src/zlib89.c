#include "zlib89.h"

#define Z89_MAX_LIT 288
#define Z89_MAX_DIST 32
#define Z89_MAX_CODELEN 19

typedef struct z89_hsym_s {
    unsigned short code;
    unsigned char len;
    unsigned short sym;
} z89_hsym;

typedef struct z89_huff_s {
    z89_hsym items[Z89_MAX_LIT];
    unsigned short count;
    unsigned char max_len;
} z89_huff;

typedef struct z89_reader_s {
    zlib89_getbyte_fn get_byte;
    void *user;
    unsigned int bitbuf;
    unsigned int bitcount;
    int failed;
} z89_reader;

typedef struct z89_mem_s {
    const unsigned char *src;
    size_t size;
    size_t pos;
} z89_mem;

static int z89_mem_get(void *user, unsigned char *out_byte)
{
    z89_mem *m;
    m = (z89_mem *)user;
    if (m == 0 || out_byte == 0 || m->pos >= m->size) return 0;
    *out_byte = m->src[m->pos++];
    return 1;
}

static int z89_get_byte(z89_reader *r, unsigned char *out)
{
    if (r == 0 || out == 0 || r->failed) return 0;
    if (!r->get_byte(r->user, out)) {
        r->failed = 1;
        return 0;
    }
    return 1;
}

static unsigned int z89_bits(z89_reader *r, unsigned int n)
{
    unsigned int v;
    unsigned char b;
    if (n == 0u) return 0u;
    while (r->bitcount < n) {
        if (!z89_get_byte(r, &b)) return 0u;
        r->bitbuf |= ((unsigned int)b) << r->bitcount;
        r->bitcount += 8u;
    }
    v = r->bitbuf & ((1u << n) - 1u);
    r->bitbuf >>= n;
    r->bitcount -= n;
    return v;
}

static void z89_align_byte(z89_reader *r)
{
    unsigned int drop;
    drop = r->bitcount & 7u;
    if (drop != 0u) {
        r->bitbuf >>= drop;
        r->bitcount -= drop;
    }
}

static unsigned short z89_reverse(unsigned short v, unsigned char len)
{
    unsigned short r;
    unsigned char i;
    r = 0u;
    for (i = 0u; i < len; ++i) {
        r = (unsigned short)((r << 1) | (v & 1u));
        v = (unsigned short)(v >> 1);
    }
    return r;
}

static int z89_huff_build(z89_huff *h, const unsigned char *lens, unsigned int n)
{
    unsigned short counts[16];
    unsigned short next[16];
    unsigned int i;
    unsigned int code;
    unsigned int len;
    unsigned int idx;
    if (h == 0 || lens == 0 || n == 0u || n > Z89_MAX_LIT) return 0;
    for (i = 0u; i < 16u; ++i) { counts[i] = 0u; next[i] = 0u; }
    for (i = 0u; i < n; ++i) {
        if (lens[i] > 15u) return 0;
        if (lens[i] != 0u) counts[lens[i]]++;
    }
    code = 0u;
    counts[0] = 0u;
    for (len = 1u; len <= 15u; ++len) {
        code = (code + counts[len - 1u]) << 1;
        next[len] = (unsigned short)code;
    }
    h->count = 0u;
    h->max_len = 0u;
    idx = 0u;
    for (i = 0u; i < n; ++i) {
        unsigned char l;
        l = lens[i];
        if (l != 0u) {
            if (idx >= Z89_MAX_LIT) return 0;
            h->items[idx].len = l;
            h->items[idx].sym = (unsigned short)i;
            h->items[idx].code = z89_reverse(next[l], l);
            next[l]++;
            if (l > h->max_len) h->max_len = l;
            ++idx;
        }
    }
    h->count = (unsigned short)idx;
    return idx != 0u;
}

static int z89_huff_decode(z89_reader *r, const z89_huff *h, unsigned int *out_sym)
{
    unsigned int code;
    unsigned int len;
    unsigned int i;
    code = 0u;
    for (len = 1u; len <= (unsigned int)h->max_len; ++len) {
        code |= z89_bits(r, 1u) << (len - 1u);
        if (r->failed) return 0;
        for (i = 0u; i < (unsigned int)h->count; ++i) {
            if ((unsigned int)h->items[i].len == len && (unsigned int)h->items[i].code == code) {
                *out_sym = (unsigned int)h->items[i].sym;
                return 1;
            }
        }
    }
    return 0;
}

static void z89_fixed(z89_huff *lit, z89_huff *dist)
{
    unsigned char ll[Z89_MAX_LIT];
    unsigned char dd[Z89_MAX_DIST];
    unsigned int i;
    for (i = 0u; i <= 143u; ++i) ll[i] = 8u;
    for (; i <= 255u; ++i) ll[i] = 9u;
    for (; i <= 279u; ++i) ll[i] = 7u;
    for (; i <= 287u; ++i) ll[i] = 8u;
    for (i = 0u; i < Z89_MAX_DIST; ++i) dd[i] = 5u;
    (void)z89_huff_build(lit, ll, Z89_MAX_LIT);
    (void)z89_huff_build(dist, dd, Z89_MAX_DIST);
}

static int z89_dynamic(z89_reader *r, z89_huff *lit, z89_huff *dist)
{
    static const unsigned char order[19] = {
        16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
    };
    unsigned char clens[Z89_MAX_CODELEN];
    unsigned char lens[Z89_MAX_LIT + Z89_MAX_DIST];
    z89_huff ch;
    unsigned int hlit;
    unsigned int hdist;
    unsigned int hclen;
    unsigned int i;
    unsigned int total;
    unsigned int sym;
    unsigned int prev;
    hlit = z89_bits(r, 5u) + 257u;
    hdist = z89_bits(r, 5u) + 1u;
    hclen = z89_bits(r, 4u) + 4u;
    if (r->failed || hlit > 286u || hdist > 32u) return 0;
    for (i = 0u; i < 19u; ++i) clens[i] = 0u;
    for (i = 0u; i < hclen; ++i) clens[order[i]] = (unsigned char)z89_bits(r, 3u);
    if (r->failed || !z89_huff_build(&ch, clens, 19u)) return 0;
    total = hlit + hdist;
    i = 0u;
    prev = 0u;
    while (i < total) {
        unsigned int rep;
        unsigned int val;
        if (!z89_huff_decode(r, &ch, &sym)) return 0;
        if (sym <= 15u) {
            lens[i++] = (unsigned char)sym;
            prev = sym;
        } else if (sym == 16u) {
            if (i == 0u) return 0;
            rep = z89_bits(r, 2u) + 3u;
            val = prev;
            if (i + rep > total) return 0;
            while (rep-- != 0u) lens[i++] = (unsigned char)val;
        } else if (sym == 17u) {
            rep = z89_bits(r, 3u) + 3u;
            if (i + rep > total) return 0;
            while (rep-- != 0u) lens[i++] = 0u;
            prev = 0u;
        } else if (sym == 18u) {
            rep = z89_bits(r, 7u) + 11u;
            if (i + rep > total) return 0;
            while (rep-- != 0u) lens[i++] = 0u;
            prev = 0u;
        } else return 0;
        if (r->failed) return 0;
    }
    if (!z89_huff_build(lit, lens, hlit)) return 0;
    if (!z89_huff_build(dist, lens + hlit, hdist)) return 0;
    return 1;
}

static int z89_inflate_deflate(z89_reader *r, unsigned char *dst, size_t cap, size_t *out_size)
{
    static const unsigned short length_base[29] = {
        3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
    };
    static const unsigned char length_extra[29] = {
        0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
    };
    static const unsigned short dist_base[30] = {
        1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,
        2049,3073,4097,6145,8193,12289,16385,24577
    };
    static const unsigned char dist_extra[30] = {
        0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
    };
    size_t op;
    int final_block;
    op = 0u;
    final_block = 0;
    while (!final_block) {
        unsigned int type;
        final_block = (int)z89_bits(r, 1u);
        type = z89_bits(r, 2u);
        if (r->failed) return ZLIB89_ERR_FORMAT;
        if (type == 0u) {
            unsigned char b0,b1,b2,b3;
            unsigned int len;
            unsigned int nlen;
            unsigned int i;
            z89_align_byte(r);
            if (!z89_get_byte(r,&b0) || !z89_get_byte(r,&b1) ||
                !z89_get_byte(r,&b2) || !z89_get_byte(r,&b3)) return ZLIB89_ERR_FORMAT;
            len = (unsigned int)b0 | ((unsigned int)b1 << 8);
            nlen = (unsigned int)b2 | ((unsigned int)b3 << 8);
            if ((len ^ 0xffffu) != nlen) return ZLIB89_ERR_FORMAT;
            if ((size_t)len > cap - op) return ZLIB89_ERR_SPACE;
            for (i = 0u; i < len; ++i) {
                if (!z89_get_byte(r, &dst[op])) return ZLIB89_ERR_FORMAT;
                ++op;
            }
        } else if (type == 1u || type == 2u) {
            z89_huff lit;
            z89_huff dist;
            unsigned int sym;
            if (type == 1u) z89_fixed(&lit, &dist);
            else if (!z89_dynamic(r, &lit, &dist)) return ZLIB89_ERR_FORMAT;
            for (;;) {
                if (!z89_huff_decode(r, &lit, &sym)) return ZLIB89_ERR_FORMAT;
                if (sym < 256u) {
                    if (op >= cap) return ZLIB89_ERR_SPACE;
                    dst[op++] = (unsigned char)sym;
                } else if (sym == 256u) {
                    break;
                } else if (sym >= 257u && sym <= 285u) {
                    unsigned int li;
                    unsigned int length;
                    unsigned int dsym;
                    unsigned int distance;
                    unsigned int k;
                    li = sym - 257u;
                    length = (unsigned int)length_base[li];
                    if (length_extra[li] != 0u) length += z89_bits(r, length_extra[li]);
                    if (!z89_huff_decode(r, &dist, &dsym) || dsym >= 30u) return ZLIB89_ERR_FORMAT;
                    distance = (unsigned int)dist_base[dsym];
                    if (dist_extra[dsym] != 0u) distance += z89_bits(r, dist_extra[dsym]);
                    if (r->failed || distance == 0u || (size_t)distance > op) return ZLIB89_ERR_FORMAT;
                    if ((size_t)length > cap - op) return ZLIB89_ERR_SPACE;
                    for (k = 0u; k < length; ++k) {
                        dst[op] = dst[op - (size_t)distance];
                        ++op;
                    }
                } else return ZLIB89_ERR_FORMAT;
            }
        } else return ZLIB89_ERR_FORMAT;
    }
    *out_size = op;
    return ZLIB89_OK;
}

unsigned int zlib89_adler32(const unsigned char *data, size_t size)
{
    unsigned int s1;
    unsigned int s2;
    size_t i;
    s1 = 1u;
    s2 = 0u;
    for (i = 0u; i < size; ++i) {
        s1 += (unsigned int)data[i];
        if (s1 >= 65521u) s1 -= 65521u;
        s2 += s1;
        if (s2 >= 65521u) s2 %= 65521u;
    }
    return (s2 << 16) | s1;
}

static int z89_inflate_common(zlib89_getbyte_fn get_byte, void *user,
                              unsigned char *dst, size_t cap, size_t *out_size,
                              int wrapped)
{
    z89_reader r;
    int rc;
    size_t produced;
    unsigned char cmf;
    unsigned char flg;
    unsigned int expected;
    unsigned char b;
    unsigned int i;
    if (get_byte == 0 || dst == 0 || out_size == 0) return ZLIB89_ERR_ARG;
    r.get_byte = get_byte;
    r.user = user;
    r.bitbuf = 0u;
    r.bitcount = 0u;
    r.failed = 0;
    if (wrapped) {
        if (!z89_get_byte(&r,&cmf) || !z89_get_byte(&r,&flg)) return ZLIB89_ERR_FORMAT;
        if ((cmf & 15u) != 8u || (cmf >> 4) > 7u) return ZLIB89_ERR_UNSUPPORTED;
        if ((((unsigned int)cmf << 8) + (unsigned int)flg) % 31u != 0u) return ZLIB89_ERR_FORMAT;
        if ((flg & 32u) != 0u) return ZLIB89_ERR_UNSUPPORTED;
    }
    produced = 0u;
    rc = z89_inflate_deflate(&r, dst, cap, &produced);
    if (rc != ZLIB89_OK) return rc;
    if (wrapped) {
        z89_align_byte(&r);
        expected = 0u;
        for (i = 0u; i < 4u; ++i) {
            if (!z89_get_byte(&r,&b)) return ZLIB89_ERR_FORMAT;
            expected = (expected << 8) | (unsigned int)b;
        }
        if (zlib89_adler32(dst, produced) != expected) return ZLIB89_ERR_CHECKSUM;
    }
    *out_size = produced;
    return ZLIB89_OK;
}

int zlib89_inflate_reader(zlib89_getbyte_fn get_byte, void *user,
                          unsigned char *dst, size_t dst_capacity,
                          size_t *out_size)
{
    return z89_inflate_common(get_byte, user, dst, dst_capacity, out_size, 1);
}

int zlib89_inflate_memory(const unsigned char *src, size_t src_size,
                          unsigned char *dst, size_t dst_capacity,
                          size_t *out_size)
{
    z89_mem m;
    if (src == 0) return ZLIB89_ERR_ARG;
    m.src = src; m.size = src_size; m.pos = 0u;
    return z89_inflate_common(z89_mem_get, &m, dst, dst_capacity, out_size, 1);
}

int zlib89_inflate_raw_memory(const unsigned char *src, size_t src_size,
                              unsigned char *dst, size_t dst_capacity,
                              size_t *out_size)
{
    z89_mem m;
    if (src == 0) return ZLIB89_ERR_ARG;
    m.src = src; m.size = src_size; m.pos = 0u;
    return z89_inflate_common(z89_mem_get, &m, dst, dst_capacity, out_size, 0);
}
