#include "gltf3d_mini_pngdec.h"

#define PNG_SIG_SIZE 8UL
#define PNG_MAXBITS 15
#define PNG_MAXCODES 320
#define PNG_MAX_LITLEN 288
#define PNG_MAX_DIST 32
#define PNG_MAX_CLEN 19

static unsigned long png_be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | (unsigned long)p[3];
}

static int png_type_is(const unsigned char *p, char a, char b, char c, char d)
{
    return (p[0] == (unsigned char)a && p[1] == (unsigned char)b && p[2] == (unsigned char)c && p[3] == (unsigned char)d) ? 1 : 0;
}

static unsigned long png_crc32_update(unsigned long crc, const unsigned char *buf, unsigned long len)
{
    unsigned long c;
    unsigned long k;
    unsigned long i;
    c = crc;
    for (i = 0UL; i < len; ++i) {
        c ^= (unsigned long)buf[i];
        for (k = 0UL; k < 8UL; ++k) {
            if (c & 1UL) c = 0xedb88320UL ^ (c >> 1);
            else c >>= 1;
        }
    }
    return c;
}

static unsigned long png_crc32(const unsigned char *type, const unsigned char *buf, unsigned long len)
{
    unsigned long c;
    c = 0xffffffffUL;
    c = png_crc32_update(c, type, 4UL);
    c = png_crc32_update(c, buf, len);
    return c ^ 0xffffffffUL;
}

static int png_bits_per_pixel(int color_type, int bit_depth)
{
    if (color_type == 0) return bit_depth;
    if (color_type == 2) return bit_depth * 3;
    if (color_type == 3) return bit_depth;
    if (color_type == 4) return bit_depth * 2;
    if (color_type == 6) return bit_depth * 4;
    return 0;
}

static int png_valid_depth_type(int color_type, int bit_depth)
{
    if (color_type == 0) return (bit_depth == 1 || bit_depth == 2 || bit_depth == 4 || bit_depth == 8 || bit_depth == 16);
    if (color_type == 2) return (bit_depth == 8 || bit_depth == 16);
    if (color_type == 3) return (bit_depth == 1 || bit_depth == 2 || bit_depth == 4 || bit_depth == 8);
    if (color_type == 4) return (bit_depth == 8 || bit_depth == 16);
    if (color_type == 6) return (bit_depth == 8 || bit_depth == 16);
    return 0;
}

static unsigned long png_paeth(unsigned long a, unsigned long b, unsigned long c)
{
    long p;
    long pa;
    long pb;
    long pc;
    p = (long)a + (long)b - (long)c;
    pa = p - (long)a; if (pa < 0) pa = -pa;
    pb = p - (long)b; if (pb < 0) pb = -pb;
    pc = p - (long)c; if (pc < 0) pc = -pc;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

typedef struct png_bitreader_s {
    const unsigned char *data;
    unsigned long size;
    unsigned long pos;
    unsigned long bitbuf;
    int bitcount;
} png_bitreader;

static void br_init(png_bitreader *br, const unsigned char *data, unsigned long size)
{
    br->data = data;
    br->size = size;
    br->pos = 0UL;
    br->bitbuf = 0UL;
    br->bitcount = 0;
}

static int br_read_bits(png_bitreader *br, int n, unsigned long *v)
{
    unsigned long out;
    unsigned long take;
    out = 0UL;
    take = 0UL;
    while (n > 0) {
        if (br->bitcount == 0) {
            if (br->pos >= br->size) return GLTF3D_MINI_ERR_BAD_DATA;
            br->bitbuf = (unsigned long)br->data[br->pos++];
            br->bitcount = 8;
        }
        out |= (br->bitbuf & 1UL) << take;
        br->bitbuf >>= 1;
        br->bitcount--;
        take++;
        n--;
    }
    *v = out;
    return GLTF3D_MINI_OK;
}

static void br_align_byte(png_bitreader *br)
{
    br->bitbuf = 0UL;
    br->bitcount = 0;
}

typedef struct png_huff_s {
    unsigned short code[PNG_MAXCODES];
    unsigned char len[PNG_MAXCODES];
    int count;
    int max_len;
} png_huff;

static unsigned short png_revbits(unsigned long x, int n)
{
    unsigned long r;
    r = 0UL;
    while (n > 0) {
        r = (r << 1) | (x & 1UL);
        x >>= 1;
        n--;
    }
    return (unsigned short)r;
}

static int huff_build(png_huff *h, const unsigned char *lengths, int count)
{
    int bl_count[PNG_MAXBITS + 1];
    int next_code[PNG_MAXBITS + 1];
    int code;
    int bits;
    int n;
    int max_len;
    if (!h || !lengths || count < 0 || count > PNG_MAXCODES) return GLTF3D_MINI_ERR_BAD_ARG;
    for (bits = 0; bits <= PNG_MAXBITS; ++bits) bl_count[bits] = 0;
    max_len = 0;
    for (n = 0; n < count; ++n) {
        if (lengths[n] > PNG_MAXBITS) return GLTF3D_MINI_ERR_BAD_DATA;
        h->len[n] = lengths[n];
        h->code[n] = 0;
        if (lengths[n] != 0) {
            bl_count[lengths[n]]++;
            if (lengths[n] > max_len) max_len = lengths[n];
        }
    }
    code = 0;
    bl_count[0] = 0;
    for (bits = 1; bits <= PNG_MAXBITS; ++bits) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    for (n = 0; n < count; ++n) {
        bits = lengths[n];
        if (bits != 0) {
            h->code[n] = png_revbits((unsigned long)next_code[bits], bits);
            next_code[bits]++;
        }
    }
    h->count = count;
    h->max_len = max_len;
    return GLTF3D_MINI_OK;
}

static int huff_decode(png_bitreader *br, const png_huff *h, int *sym)
{
    unsigned long bit;
    unsigned long code;
    int len;
    int n;
    int r;
    code = 0UL;
    for (len = 1; len <= h->max_len; ++len) {
        r = br_read_bits(br, 1, &bit);
        if (r != GLTF3D_MINI_OK) return r;
        code |= bit << (len - 1);
        for (n = 0; n < h->count; ++n) {
            if (h->len[n] == (unsigned char)len && h->code[n] == (unsigned short)code) {
                *sym = n;
                return GLTF3D_MINI_OK;
            }
        }
    }
    return GLTF3D_MINI_ERR_BAD_DATA;
}

static const unsigned short png_len_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const unsigned char png_len_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const unsigned short png_dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
static const unsigned char png_dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

static int inflate_codes(png_bitreader *br, const png_huff *lit, const png_huff *dist, unsigned char *out, unsigned long out_size, unsigned long *out_pos)
{
    int sym;
    int dsym;
    int r;
    unsigned long extra;
    unsigned long len;
    unsigned long d;
    unsigned long i;
    for (;;) {
        r = huff_decode(br, lit, &sym);
        if (r != GLTF3D_MINI_OK) return r;
        if (sym < 256) {
            if (*out_pos >= out_size) return GLTF3D_MINI_ERR_RANGE;
            out[*out_pos] = (unsigned char)sym;
            *out_pos = *out_pos + 1UL;
        } else if (sym == 256) {
            return GLTF3D_MINI_OK;
        } else if (sym > 256 && sym < 286) {
            len = (unsigned long)png_len_base[sym - 257];
            if (png_len_extra[sym - 257] != 0) {
                r = br_read_bits(br, (int)png_len_extra[sym - 257], &extra);
                if (r != GLTF3D_MINI_OK) return r;
                len += extra;
            }
            r = huff_decode(br, dist, &dsym);
            if (r != GLTF3D_MINI_OK) return r;
            if (dsym < 0 || dsym >= 30) return GLTF3D_MINI_ERR_BAD_DATA;
            d = (unsigned long)png_dist_base[dsym];
            if (png_dist_extra[dsym] != 0) {
                r = br_read_bits(br, (int)png_dist_extra[dsym], &extra);
                if (r != GLTF3D_MINI_OK) return r;
                d += extra;
            }
            if (d == 0UL || d > *out_pos) return GLTF3D_MINI_ERR_BAD_DATA;
            if (len > out_size - *out_pos) return GLTF3D_MINI_ERR_RANGE;
            for (i = 0UL; i < len; ++i) {
                out[*out_pos] = out[*out_pos - d];
                *out_pos = *out_pos + 1UL;
            }
        } else {
            return GLTF3D_MINI_ERR_BAD_DATA;
        }
    }
}

static int inflate_fixed(png_bitreader *br, unsigned char *out, unsigned long out_size, unsigned long *out_pos)
{
    unsigned char ll[PNG_MAX_LITLEN];
    unsigned char dd[PNG_MAX_DIST];
    png_huff lit;
    png_huff dist;
    int i;
    int r;
    for (i = 0; i <= 143; ++i) ll[i] = 8;
    for (i = 144; i <= 255; ++i) ll[i] = 9;
    for (i = 256; i <= 279; ++i) ll[i] = 7;
    for (i = 280; i <= 287; ++i) ll[i] = 8;
    for (i = 0; i < PNG_MAX_DIST; ++i) dd[i] = 5;
    r = huff_build(&lit, ll, PNG_MAX_LITLEN); if (r != GLTF3D_MINI_OK) return r;
    r = huff_build(&dist, dd, PNG_MAX_DIST); if (r != GLTF3D_MINI_OK) return r;
    return inflate_codes(br, &lit, &dist, out, out_size, out_pos);
}

static int inflate_dynamic(png_bitreader *br, unsigned char *out, unsigned long out_size, unsigned long *out_pos)
{
    static const int order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    unsigned char clen[PNG_MAX_CLEN];
    unsigned char ll[PNG_MAX_LITLEN + PNG_MAX_DIST];
    png_huff ch;
    png_huff lit;
    png_huff dist;
    unsigned long v;
    unsigned long rep;
    int hlit;
    int hdist;
    int hclen;
    int total;
    int idx;
    int sym;
    int r;
    int i;
    for (i = 0; i < PNG_MAX_CLEN; ++i) clen[i] = 0;
    for (i = 0; i < PNG_MAX_LITLEN + PNG_MAX_DIST; ++i) ll[i] = 0;
    r = br_read_bits(br, 5, &v); if (r != GLTF3D_MINI_OK) return r; hlit = (int)v + 257;
    r = br_read_bits(br, 5, &v); if (r != GLTF3D_MINI_OK) return r; hdist = (int)v + 1;
    r = br_read_bits(br, 4, &v); if (r != GLTF3D_MINI_OK) return r; hclen = (int)v + 4;
    if (hlit > PNG_MAX_LITLEN || hdist > PNG_MAX_DIST) return GLTF3D_MINI_ERR_BAD_DATA;
    for (i = 0; i < hclen; ++i) {
        r = br_read_bits(br, 3, &v); if (r != GLTF3D_MINI_OK) return r;
        clen[order[i]] = (unsigned char)v;
    }
    r = huff_build(&ch, clen, PNG_MAX_CLEN); if (r != GLTF3D_MINI_OK) return r;
    total = hlit + hdist;
    idx = 0;
    while (idx < total) {
        r = huff_decode(br, &ch, &sym); if (r != GLTF3D_MINI_OK) return r;
        if (sym >= 0 && sym <= 15) {
            ll[idx++] = (unsigned char)sym;
        } else if (sym == 16) {
            if (idx == 0) return GLTF3D_MINI_ERR_BAD_DATA;
            r = br_read_bits(br, 2, &v); if (r != GLTF3D_MINI_OK) return r;
            rep = v + 3UL;
            while (rep > 0UL && idx < total) { ll[idx] = ll[idx - 1]; idx++; rep--; }
            if (rep != 0UL) return GLTF3D_MINI_ERR_BAD_DATA;
        } else if (sym == 17) {
            r = br_read_bits(br, 3, &v); if (r != GLTF3D_MINI_OK) return r;
            rep = v + 3UL;
            while (rep > 0UL && idx < total) { ll[idx++] = 0; rep--; }
            if (rep != 0UL) return GLTF3D_MINI_ERR_BAD_DATA;
        } else if (sym == 18) {
            r = br_read_bits(br, 7, &v); if (r != GLTF3D_MINI_OK) return r;
            rep = v + 11UL;
            while (rep > 0UL && idx < total) { ll[idx++] = 0; rep--; }
            if (rep != 0UL) return GLTF3D_MINI_ERR_BAD_DATA;
        } else {
            return GLTF3D_MINI_ERR_BAD_DATA;
        }
    }
    r = huff_build(&lit, ll, hlit); if (r != GLTF3D_MINI_OK) return r;
    r = huff_build(&dist, ll + hlit, hdist); if (r != GLTF3D_MINI_OK) return r;
    return inflate_codes(br, &lit, &dist, out, out_size, out_pos);
}

static unsigned long png_adler32(const unsigned char *data, unsigned long len)
{
    unsigned long s1;
    unsigned long s2;
    unsigned long i;
    s1 = 1UL;
    s2 = 0UL;
    for (i = 0UL; i < len; ++i) {
        s1 += (unsigned long)data[i];
        if (s1 >= 65521UL) s1 -= 65521UL;
        s2 += s1;
        if (s2 >= 65521UL) s2 %= 65521UL;
    }
    return (s2 << 16) | s1;
}

static int zlib_inflate(const unsigned char *src, unsigned long src_size, unsigned char *out, unsigned long out_size, unsigned long *actual_out)
{
    png_bitreader br;
    unsigned long cmf;
    unsigned long flg;
    unsigned long bfinal;
    unsigned long btype;
    unsigned long len;
    unsigned long nlen;
    unsigned long got;
    unsigned long adler;
    unsigned long expected_adler;
    int final_block;
    int r;
    if (!src || !out || !actual_out) return GLTF3D_MINI_ERR_BAD_ARG;
    if (src_size < 6UL) return GLTF3D_MINI_ERR_BAD_DATA;
    cmf = (unsigned long)src[0];
    flg = (unsigned long)src[1];
    if ((cmf & 15UL) != 8UL) return GLTF3D_MINI_ERR_UNSUPPORTED;
    if (((cmf << 8) + flg) % 31UL != 0UL) return GLTF3D_MINI_ERR_BAD_DATA;
    if (flg & 32UL) return GLTF3D_MINI_ERR_UNSUPPORTED;
    br_init(&br, src + 2UL, src_size - 6UL);
    got = 0UL;
    final_block = 0;
    while (!final_block) {
        r = br_read_bits(&br, 1, &bfinal); if (r != GLTF3D_MINI_OK) return r;
        r = br_read_bits(&br, 2, &btype); if (r != GLTF3D_MINI_OK) return r;
        final_block = (bfinal != 0UL) ? 1 : 0;
        if (btype == 0UL) {
            br_align_byte(&br);
            if (br.pos + 4UL > br.size) return GLTF3D_MINI_ERR_BAD_DATA;
            len = (unsigned long)br.data[br.pos] | ((unsigned long)br.data[br.pos + 1UL] << 8);
            nlen = (unsigned long)br.data[br.pos + 2UL] | ((unsigned long)br.data[br.pos + 3UL] << 8);
            br.pos += 4UL;
            if (((len ^ 0xffffUL) & 0xffffUL) != (nlen & 0xffffUL)) return GLTF3D_MINI_ERR_BAD_DATA;
            if (len > br.size - br.pos || len > out_size - got) return GLTF3D_MINI_ERR_RANGE;
            while (len > 0UL) { out[got++] = br.data[br.pos++]; len--; }
        } else if (btype == 1UL) {
            r = inflate_fixed(&br, out, out_size, &got); if (r != GLTF3D_MINI_OK) return r;
        } else if (btype == 2UL) {
            r = inflate_dynamic(&br, out, out_size, &got); if (r != GLTF3D_MINI_OK) return r;
        } else {
            return GLTF3D_MINI_ERR_BAD_DATA;
        }
    }
    expected_adler = png_be32(src + src_size - 4UL);
    adler = png_adler32(out, got);
    if (adler != expected_adler) return GLTF3D_MINI_ERR_BAD_DATA;
    *actual_out = got;
    return GLTF3D_MINI_OK;
}

static unsigned long read_bits_msb(const unsigned char *row, unsigned long bitpos, int bits)
{
    unsigned long v;
    int i;
    unsigned long bytep;
    int shift;
    v = 0UL;
    for (i = 0; i < bits; ++i) {
        bytep = (bitpos + (unsigned long)i) >> 3;
        shift = 7 - (int)((bitpos + (unsigned long)i) & 7UL);
        v = (v << 1) | ((unsigned long)(row[bytep] >> shift) & 1UL);
    }
    return v;
}

static void png_rgba_from_row(const unsigned char *row, unsigned long x, const gltf3d_mini_png_info *info, unsigned char *rgba)
{
    unsigned long v;
    unsigned long a;
    unsigned long off;
    int bd;
    bd = info->bit_depth;
    rgba[0] = 0; rgba[1] = 0; rgba[2] = 0; rgba[3] = 255;
    if (info->color_type == 0) {
        if (bd < 8) {
            v = read_bits_msb(row, x * (unsigned long)bd, bd);
            v = (v * 255UL) / ((1UL << bd) - 1UL);
        } else if (bd == 8) {
            v = (unsigned long)row[x];
        } else {
            v = (unsigned long)row[x * 2UL];
        }
        rgba[0] = (unsigned char)v; rgba[1] = (unsigned char)v; rgba[2] = (unsigned char)v;
        if (info->flags & GLTF3D_MINI_PNG_FLAG_HAS_TRNS) {
            a = ((unsigned long)info->trns_gray[0] << 8) | (unsigned long)info->trns_gray[1];
            if ((bd == 16 && (((unsigned long)row[x * 2UL] << 8) | row[x * 2UL + 1UL]) == a) || (bd != 16 && v == (a & 255UL))) rgba[3] = 0;
        }
    } else if (info->color_type == 2) {
        off = x * (unsigned long)((bd == 16) ? 6 : 3);
        rgba[0] = row[off];
        rgba[1] = row[off + (unsigned long)((bd == 16) ? 2 : 1)];
        rgba[2] = row[off + (unsigned long)((bd == 16) ? 4 : 2)];
        if (info->flags & GLTF3D_MINI_PNG_FLAG_HAS_TRNS) {
            if (bd == 16) {
                if (row[off] == info->trns_rgb[0] && row[off + 1UL] == info->trns_rgb[1] && row[off + 2UL] == info->trns_rgb[2] && row[off + 3UL] == info->trns_rgb[3] && row[off + 4UL] == info->trns_rgb[4] && row[off + 5UL] == info->trns_rgb[5]) rgba[3] = 0;
            } else {
                if (row[off] == info->trns_rgb[1] && row[off + 1UL] == info->trns_rgb[3] && row[off + 2UL] == info->trns_rgb[5]) rgba[3] = 0;
            }
        }
    } else if (info->color_type == 3) {
        v = read_bits_msb(row, x * (unsigned long)bd, bd);
        if (v < info->palette_count) {
            rgba[0] = info->palette[v * 4UL + 0UL];
            rgba[1] = info->palette[v * 4UL + 1UL];
            rgba[2] = info->palette[v * 4UL + 2UL];
            rgba[3] = info->palette[v * 4UL + 3UL];
        }
    } else if (info->color_type == 4) {
        off = x * (unsigned long)((bd == 16) ? 4 : 2);
        rgba[0] = row[off]; rgba[1] = row[off]; rgba[2] = row[off];
        rgba[3] = row[off + (unsigned long)((bd == 16) ? 2 : 1)];
    } else if (info->color_type == 6) {
        off = x * (unsigned long)((bd == 16) ? 8 : 4);
        rgba[0] = row[off];
        rgba[1] = row[off + (unsigned long)((bd == 16) ? 2 : 1)];
        rgba[2] = row[off + (unsigned long)((bd == 16) ? 4 : 2)];
        rgba[3] = row[off + (unsigned long)((bd == 16) ? 6 : 3)];
    }
}

int gltf3d_mini_png_parse(const unsigned char *data, unsigned long size, gltf3d_mini_png_info *info)
{
    static const unsigned char sig[8] = {137,80,78,71,13,10,26,10};
    unsigned long p;
    unsigned long len;
    unsigned long crc_file;
    unsigned long crc_calc;
    unsigned long i;
    unsigned long bpp;
    unsigned long rowbytes;
    int saw_ihdr;
    int saw_iend;
    int k;
    if (!data || !info) return GLTF3D_MINI_ERR_BAD_ARG;
    if (size < 33UL) return GLTF3D_MINI_ERR_BAD_DATA;
    for (k = 0; k < 8; ++k) if (data[k] != sig[k]) return GLTF3D_MINI_ERR_BAD_DATA;
    for (i = 0UL; i < sizeof(*info); ++i) ((unsigned char *)info)[i] = 0;
    for (i = 0UL; i < GLTF3D_MINI_PNG_MAX_PALETTE; ++i) info->palette[i * 4UL + 3UL] = 255;
    p = PNG_SIG_SIZE;
    saw_ihdr = 0;
    saw_iend = 0;
    while (p + 12UL <= size) {
        len = png_be32(data + p);
        if (len > size - p - 12UL) return GLTF3D_MINI_ERR_BAD_DATA;
        crc_file = png_be32(data + p + 8UL + len);
        crc_calc = png_crc32(data + p + 4UL, data + p + 8UL, len);
        if (crc_file != crc_calc) return GLTF3D_MINI_ERR_BAD_DATA;
        info->chunk_count++;
        if (png_type_is(data + p + 4UL, 'I','H','D','R')) {
            if (saw_ihdr || len != 13UL) return GLTF3D_MINI_ERR_BAD_DATA;
            info->width = png_be32(data + p + 8UL);
            info->height = png_be32(data + p + 12UL);
            info->bit_depth = (int)data[p + 16UL];
            info->color_type = (int)data[p + 17UL];
            info->compression_method = (int)data[p + 18UL];
            info->filter_method = (int)data[p + 19UL];
            info->interlace_method = (int)data[p + 20UL];
            if (info->width == 0UL || info->height == 0UL) return GLTF3D_MINI_ERR_BAD_DATA;
            if (!png_valid_depth_type(info->color_type, info->bit_depth)) return GLTF3D_MINI_ERR_UNSUPPORTED;
            if (info->compression_method != 0 || info->filter_method != 0) return GLTF3D_MINI_ERR_UNSUPPORTED;
            if (info->interlace_method != 0) info->flags |= GLTF3D_MINI_PNG_FLAG_INTERLACED;
            saw_ihdr = 1;
        } else if (png_type_is(data + p + 4UL, 'P','L','T','E')) {
            if (!saw_ihdr || len % 3UL != 0UL || len / 3UL > GLTF3D_MINI_PNG_MAX_PALETTE) return GLTF3D_MINI_ERR_BAD_DATA;
            info->palette_count = len / 3UL;
            for (i = 0UL; i < info->palette_count; ++i) {
                info->palette[i * 4UL + 0UL] = data[p + 8UL + i * 3UL + 0UL];
                info->palette[i * 4UL + 1UL] = data[p + 8UL + i * 3UL + 1UL];
                info->palette[i * 4UL + 2UL] = data[p + 8UL + i * 3UL + 2UL];
                info->palette[i * 4UL + 3UL] = 255;
            }
            info->flags |= GLTF3D_MINI_PNG_FLAG_HAS_PLTE;
        } else if (png_type_is(data + p + 4UL, 't','R','N','S')) {
            if (!saw_ihdr) return GLTF3D_MINI_ERR_BAD_DATA;
            if (info->color_type == 3) {
                if (len > info->palette_count) return GLTF3D_MINI_ERR_BAD_DATA;
                for (i = 0UL; i < len; ++i) info->palette[i * 4UL + 3UL] = data[p + 8UL + i];
            } else if (info->color_type == 0) {
                if (len != 2UL) return GLTF3D_MINI_ERR_BAD_DATA;
                info->trns_gray[0] = data[p + 8UL]; info->trns_gray[1] = data[p + 9UL];
            } else if (info->color_type == 2) {
                if (len != 6UL) return GLTF3D_MINI_ERR_BAD_DATA;
                for (i = 0UL; i < 6UL; ++i) info->trns_rgb[i] = data[p + 8UL + i];
            }
            info->flags |= GLTF3D_MINI_PNG_FLAG_HAS_TRNS;
        } else if (png_type_is(data + p + 4UL, 'I','D','A','T')) {
            if (!saw_ihdr) return GLTF3D_MINI_ERR_BAD_DATA;
            if (info->idat_bytes > 0xffffffffUL - len) return GLTF3D_MINI_ERR_RANGE;
            info->idat_bytes += len;
            info->flags |= GLTF3D_MINI_PNG_FLAG_HAS_IDAT;
        } else if (png_type_is(data + p + 4UL, 'I','E','N','D')) {
            saw_iend = 1;
            break;
        }
        p += 12UL + len;
    }
    if (!saw_ihdr || !saw_iend || !(info->flags & GLTF3D_MINI_PNG_FLAG_HAS_IDAT)) return GLTF3D_MINI_ERR_BAD_DATA;
    if (info->flags & GLTF3D_MINI_PNG_FLAG_INTERLACED) return GLTF3D_MINI_ERR_UNSUPPORTED;
    if (info->color_type == 3 && !(info->flags & GLTF3D_MINI_PNG_FLAG_HAS_PLTE)) return GLTF3D_MINI_ERR_BAD_DATA;
    bpp = (unsigned long)png_bits_per_pixel(info->color_type, info->bit_depth);
    rowbytes = (info->width * bpp + 7UL) / 8UL;
    if (rowbytes == 0UL || info->height > 0xffffffffUL / (rowbytes + 1UL)) return GLTF3D_MINI_ERR_RANGE;
    info->required_raw_bytes = (rowbytes + 1UL) * info->height;
    if (info->idat_bytes > 0xffffffffUL - info->required_raw_bytes) return GLTF3D_MINI_ERR_RANGE;
    info->required_work_bytes = info->idat_bytes + info->required_raw_bytes;
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_png_required_rgba8_work(const unsigned char *data, unsigned long size, unsigned long *work_bytes, unsigned long *dst_bytes, unsigned long *w, unsigned long *h)
{
    gltf3d_mini_png_info info;
    int r;
    if (!work_bytes || !dst_bytes || !w || !h) return GLTF3D_MINI_ERR_BAD_ARG;
    r = gltf3d_mini_png_parse(data, size, &info);
    if (r != GLTF3D_MINI_OK) return r;
    if (info.width > 0xffffffffUL / 4UL || info.height > 0xffffffffUL / (info.width * 4UL)) return GLTF3D_MINI_ERR_RANGE;
    *work_bytes = info.required_work_bytes;
    *dst_bytes = info.width * info.height * 4UL;
    *w = info.width;
    *h = info.height;
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_png_decode_rgba8_work(const unsigned char *data, unsigned long size, unsigned char *dst_rgba, unsigned long dst_size, unsigned char *work, unsigned long work_size, unsigned long *w, unsigned long *h)
{
    gltf3d_mini_png_info info;
    unsigned char *idat;
    unsigned char *raw;
    unsigned long p;
    unsigned long len;
    unsigned long idp;
    unsigned long actual_raw;
    unsigned long bpp_bits;
    unsigned long rowbytes;
    unsigned long pixel_bytes;
    unsigned long y;
    unsigned long x;
    unsigned long i;
    unsigned long srcp;
    unsigned long dstp;
    int filter;
    int r;
    unsigned char left;
    unsigned char up;
    unsigned char ul;
    unsigned char rgba[4];
    if (!data || !dst_rgba || !work || !w || !h) return GLTF3D_MINI_ERR_BAD_ARG;
    r = gltf3d_mini_png_parse(data, size, &info);
    if (r != GLTF3D_MINI_OK) return r;
    if (work_size < info.required_work_bytes) return GLTF3D_MINI_ERR_RANGE;
    if (info.width > 0xffffffffUL / 4UL || info.height > 0xffffffffUL / (info.width * 4UL)) return GLTF3D_MINI_ERR_RANGE;
    if (dst_size < info.width * info.height * 4UL) return GLTF3D_MINI_ERR_RANGE;
    idat = work;
    raw = work + info.idat_bytes;
    idp = 0UL;
    p = PNG_SIG_SIZE;
    while (p + 12UL <= size) {
        len = png_be32(data + p);
        if (png_type_is(data + p + 4UL, 'I','D','A','T')) {
            for (i = 0UL; i < len; ++i) idat[idp++] = data[p + 8UL + i];
        } else if (png_type_is(data + p + 4UL, 'I','E','N','D')) {
            break;
        }
        p += 12UL + len;
    }
    if (idp != info.idat_bytes) return GLTF3D_MINI_ERR_BAD_DATA;
    r = zlib_inflate(idat, info.idat_bytes, raw, info.required_raw_bytes, &actual_raw);
    if (r != GLTF3D_MINI_OK) return r;
    if (actual_raw != info.required_raw_bytes) return GLTF3D_MINI_ERR_BAD_DATA;
    bpp_bits = (unsigned long)png_bits_per_pixel(info.color_type, info.bit_depth);
    rowbytes = (info.width * bpp_bits + 7UL) / 8UL;
    pixel_bytes = (bpp_bits + 7UL) / 8UL;
    if (pixel_bytes == 0UL) pixel_bytes = 1UL;
    srcp = 0UL;
    for (y = 0UL; y < info.height; ++y) {
        filter = (int)raw[srcp++];
        if (filter < 0 || filter > 4) return GLTF3D_MINI_ERR_BAD_DATA;
        for (i = 0UL; i < rowbytes; ++i) {
            left = (i >= pixel_bytes) ? raw[srcp + i - pixel_bytes] : 0;
            up = (y != 0UL) ? raw[srcp + i - rowbytes - 1UL] : 0;
            ul = (y != 0UL && i >= pixel_bytes) ? raw[srcp + i - rowbytes - 1UL - pixel_bytes] : 0;
            if (filter == 1) raw[srcp + i] = (unsigned char)((unsigned int)raw[srcp + i] + (unsigned int)left);
            else if (filter == 2) raw[srcp + i] = (unsigned char)((unsigned int)raw[srcp + i] + (unsigned int)up);
            else if (filter == 3) raw[srcp + i] = (unsigned char)((unsigned int)raw[srcp + i] + (unsigned int)(((unsigned int)left + (unsigned int)up) >> 1));
            else if (filter == 4) raw[srcp + i] = (unsigned char)((unsigned int)raw[srcp + i] + (unsigned int)png_paeth((unsigned long)left, (unsigned long)up, (unsigned long)ul));
        }
        for (x = 0UL; x < info.width; ++x) {
            png_rgba_from_row(raw + srcp, x, &info, rgba);
            dstp = (y * info.width + x) * 4UL;
            dst_rgba[dstp + 0UL] = rgba[0];
            dst_rgba[dstp + 1UL] = rgba[1];
            dst_rgba[dstp + 2UL] = rgba[2];
            dst_rgba[dstp + 3UL] = rgba[3];
        }
        srcp += rowbytes;
    }
    *w = info.width;
    *h = info.height;
    return GLTF3D_MINI_OK;
}
