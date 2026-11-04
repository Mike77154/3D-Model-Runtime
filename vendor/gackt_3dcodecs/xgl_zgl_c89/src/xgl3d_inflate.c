#include "xgl3d.h"

/* Small bounded DEFLATE decoder for gzip-wrapped ZGL. No heap. C89. */

typedef struct br_s {
    const unsigned char *src;
    unsigned long len;
    unsigned long pos;
    unsigned long bitbuf;
    int bits;
} br_t;

typedef struct hw_s {
    int n;
    int maxbits;
    int len[320];
    int code[320];
    int left[700];
    int right[700];
    int sym[700];
    int node_count;
} hw_t;

static const int length_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const int length_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const int dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
static const int dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};
static const int clen_order[19] = {
    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
};

static int br_need(br_t *b, int n)
{
    while (b->bits < n) {
        if (b->pos >= b->len) return 0;
        b->bitbuf |= ((unsigned long)b->src[b->pos++]) << b->bits;
        b->bits += 8;
    }
    return 1;
}

static int br_bits(br_t *b, int n, int *out)
{
    unsigned long mask;
    if (!br_need(b, n)) return 0;
    mask = (1UL << n) - 1UL;
    *out = (int)(b->bitbuf & mask);
    b->bitbuf >>= n;
    b->bits -= n;
    return 1;
}

static void br_byte_align(br_t *b)
{
    b->bitbuf = 0UL;
    b->bits = 0;
}

static int huff_build(hw_t *h, const int *lengths, int n)
{
    int bl_count[16];
    int next_code[16];
    int code;
    int bits;
    int i;
    int node;
    int bit;
    int k;
    int next;
    h->n = n;
    h->maxbits = 0;
    h->node_count = 1;
    i = 0;
    while (i < 700) { h->left[i] = -1; h->right[i] = -1; h->sym[i] = -1; ++i; }
    i = 0;
    while (i < 320) { h->len[i] = 0; h->code[i] = 0; ++i; }
    i = 0;
    while (i < 16) { bl_count[i] = 0; next_code[i] = 0; ++i; }
    i = 0;
    while (i < n) {
        if (lengths[i] < 0 || lengths[i] > 15) return 0;
        h->len[i] = lengths[i];
        if (lengths[i] != 0) {
            bl_count[lengths[i]]++;
            if (lengths[i] > h->maxbits) h->maxbits = lengths[i];
        }
        ++i;
    }
    code = 0;
    bits = 1;
    while (bits <= 15) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
        ++bits;
    }
    i = 0;
    while (i < n) {
        bits = h->len[i];
        if (bits != 0) {
            code = next_code[bits]++;
            h->code[i] = code;
            node = 0;
            k = 0;
            while (k < bits) {
                bit = (code >> (bits - 1 - k)) & 1;
                if (bit == 0) {
                    next = h->left[node];
                    if (next < 0) {
                        if (h->node_count >= 700) return 0;
                        next = h->node_count++;
                        h->left[node] = next;
                    }
                } else {
                    next = h->right[node];
                    if (next < 0) {
                        if (h->node_count >= 700) return 0;
                        next = h->node_count++;
                        h->right[node] = next;
                    }
                }
                node = next;
                ++k;
            }
            h->sym[node] = i;
        }
        ++i;
    }
    return 1;
}

static int huff_decode(br_t *b, const hw_t *h, int *sym)
{
    int node;
    int bit;
    int depth;
    node = 0;
    depth = 0;
    while (depth <= h->maxbits) {
        if (h->sym[node] >= 0) {
            *sym = h->sym[node];
            return 1;
        }
        if (!br_bits(b, 1, &bit)) return 0;
        if (bit == 0) node = h->left[node];
        else node = h->right[node];
        if (node < 0 || node >= h->node_count) return 0;
        ++depth;
    }
    return 0;
}

static int fixed_huffs(hw_t *lit, hw_t *dist)
{
    int l[288];
    int d[32];
    int i;
    i = 0;
    while (i <= 143) { l[i] = 8; ++i; }
    while (i <= 255) { l[i] = 9; ++i; }
    while (i <= 279) { l[i] = 7; ++i; }
    while (i <= 287) { l[i] = 8; ++i; }
    i = 0;
    while (i < 32) { d[i] = 5; ++i; }
    return huff_build(lit, l, 288) && huff_build(dist, d, 32);
}

static int stored_block(br_t *b, unsigned char *out, unsigned long cap, unsigned long *op)
{
    int lo;
    int hi;
    unsigned int len;
    unsigned int nlen;
    unsigned int i;
    br_byte_align(b);
    if (b->pos + 4UL > b->len) return 0;
    lo = b->src[b->pos++]; hi = b->src[b->pos++]; len = (unsigned int)(lo | (hi << 8));
    lo = b->src[b->pos++]; hi = b->src[b->pos++]; nlen = (unsigned int)(lo | (hi << 8));
    if (((len ^ 0xFFFFU) & 0xFFFFU) != nlen) return 0;
    if (b->pos + (unsigned long)len > b->len) return 0;
    if (*op + (unsigned long)len > cap) return 0;
    i = 0U;
    while (i < len) out[(*op)++] = b->src[b->pos++], ++i;
    return 1;
}

static int dynamic_huffs(br_t *b, hw_t *lit, hw_t *dist)
{
    int hlit;
    int hdist;
    int hclen;
    int clen[19];
    int all[320];
    hw_t ch;
    int i;
    int sym;
    int repeat;
    int prev;
    if (!br_bits(b, 5, &hlit)) return 0;
    if (!br_bits(b, 5, &hdist)) return 0;
    if (!br_bits(b, 4, &hclen)) return 0;
    hlit += 257;
    hdist += 1;
    hclen += 4;
    i = 0;
    while (i < 19) { clen[i] = 0; ++i; }
    i = 0;
    while (i < hclen) {
        if (!br_bits(b, 3, &clen[clen_order[i]])) return 0;
        ++i;
    }
    if (!huff_build(&ch, clen, 19)) return 0;
    i = 0;
    prev = 0;
    while (i < hlit + hdist) {
        if (!huff_decode(b, &ch, &sym)) return 0;
        if (sym <= 15) {
            all[i++] = sym;
            prev = sym;
        } else if (sym == 16) {
            if (!br_bits(b, 2, &repeat)) return 0;
            repeat += 3;
            while (repeat > 0 && i < hlit + hdist) { all[i++] = prev; --repeat; }
            if (repeat != 0) return 0;
        } else if (sym == 17) {
            if (!br_bits(b, 3, &repeat)) return 0;
            repeat += 3;
            prev = 0;
            while (repeat > 0 && i < hlit + hdist) { all[i++] = 0; --repeat; }
            if (repeat != 0) return 0;
        } else if (sym == 18) {
            if (!br_bits(b, 7, &repeat)) return 0;
            repeat += 11;
            prev = 0;
            while (repeat > 0 && i < hlit + hdist) { all[i++] = 0; --repeat; }
            if (repeat != 0) return 0;
        } else return 0;
    }
    return huff_build(lit, all, hlit) && huff_build(dist, all + hlit, hdist);
}

static int codes_block(br_t *b, const hw_t *lit, const hw_t *dist, unsigned char *out, unsigned long cap, unsigned long *op)
{
    int sym;
    int ext;
    int len;
    int dsym;
    int distance;
    unsigned long src;
    while (1) {
        if (!huff_decode(b, lit, &sym)) return 0;
        if (sym < 256) {
            if (*op >= cap) return 0;
            out[(*op)++] = (unsigned char)sym;
        } else if (sym == 256) {
            return 1;
        } else if (sym >= 257 && sym <= 285) {
            sym -= 257;
            len = length_base[sym];
            if (length_extra[sym] != 0) {
                if (!br_bits(b, length_extra[sym], &ext)) return 0;
                len += ext;
            }
            if (!huff_decode(b, dist, &dsym)) return 0;
            if (dsym < 0 || dsym >= 30) return 0;
            distance = dist_base[dsym];
            if (dist_extra[dsym] != 0) {
                if (!br_bits(b, dist_extra[dsym], &ext)) return 0;
                distance += ext;
            }
            if ((unsigned long)distance > *op) return 0;
            if (*op + (unsigned long)len > cap) return 0;
            src = *op - (unsigned long)distance;
            while (len > 0) {
                out[*op] = out[src];
                (*op)++;
                src++;
                --len;
            }
        } else return 0;
    }
}

static int inflate_deflate(const unsigned char *src, unsigned long src_len, unsigned char *out, unsigned long out_cap, unsigned long *out_len)
{
    br_t b;
    int final;
    int type;
    hw_t lit;
    hw_t dist;
    unsigned long op;
    b.src = src; b.len = src_len; b.pos = 0UL; b.bitbuf = 0UL; b.bits = 0;
    op = 0UL;
    final = 0;
    while (!final) {
        if (!br_bits(&b, 1, &final)) return 0;
        if (!br_bits(&b, 2, &type)) return 0;
        if (type == 0) {
            if (!stored_block(&b, out, out_cap, &op)) return 0;
        } else if (type == 1) {
            if (!fixed_huffs(&lit, &dist)) return 0;
            if (!codes_block(&b, &lit, &dist, out, out_cap, &op)) return 0;
        } else if (type == 2) {
            if (!dynamic_huffs(&b, &lit, &dist)) return 0;
            if (!codes_block(&b, &lit, &dist, out, out_cap, &op)) return 0;
        } else return 0;
    }
    *out_len = op;
    return 1;
}

static unsigned long get_le32(const unsigned char *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}


static unsigned long crc32_slow(const unsigned char *p, unsigned long n)
{
    unsigned long crc;
    unsigned long i;
    int k;
    crc = 0xFFFFFFFFUL;
    i = 0UL;
    while (i < n) {
        crc ^= (unsigned long)p[i];
        k = 0;
        while (k < 8) {
            if ((crc & 1UL) != 0UL) crc = (crc >> 1) ^ 0xEDB88320UL;
            else crc >>= 1;
            ++k;
        }
        ++i;
    }
    return crc ^ 0xFFFFFFFFUL;
}

int xgl3d_inflate_gzip(const unsigned char *zgl, unsigned long zgl_len, unsigned char *out, unsigned long out_cap, unsigned long *out_len)
{
    unsigned long p;
    unsigned long end;
    unsigned long tmp;
    int flg;
    if (zgl == 0 || out == 0 || out_len == 0) return XGL3D_E_NULL;
    *out_len = 0UL;
    if (zgl_len < 18UL) return XGL3D_E_GZIP;
    if (zgl[0] != 0x1FU || zgl[1] != 0x8BU || zgl[2] != 8U) return XGL3D_E_GZIP;
    flg = zgl[3];
    p = 10UL;
    if ((flg & 4) != 0) {
        if (p + 2UL > zgl_len) return XGL3D_E_GZIP;
        tmp = (unsigned long)zgl[p] | ((unsigned long)zgl[p + 1UL] << 8);
        p += 2UL + tmp;
        if (p > zgl_len) return XGL3D_E_GZIP;
    }
    if ((flg & 8) != 0) { while (p < zgl_len && zgl[p] != 0U) ++p; if (p >= zgl_len) return XGL3D_E_GZIP; ++p; }
    if ((flg & 16) != 0) { while (p < zgl_len && zgl[p] != 0U) ++p; if (p >= zgl_len) return XGL3D_E_GZIP; ++p; }
    if ((flg & 2) != 0) { if (p + 2UL > zgl_len) return XGL3D_E_GZIP; p += 2UL; }
    if (p + 8UL > zgl_len) return XGL3D_E_GZIP;
    end = zgl_len - 8UL;
    if (!inflate_deflate(zgl + p, end - p, out, out_cap, out_len)) return XGL3D_E_DEFLATE;
    tmp = get_le32(zgl + end);
    if ((tmp & 0xFFFFFFFFUL) != (crc32_slow(out, *out_len) & 0xFFFFFFFFUL)) return XGL3D_E_GZIP;
    tmp = get_le32(zgl + end + 4UL);
    if ((tmp & 0xFFFFFFFFUL) != (*out_len & 0xFFFFFFFFUL)) return XGL3D_E_GZIP;
    return XGL3D_OK;
}

