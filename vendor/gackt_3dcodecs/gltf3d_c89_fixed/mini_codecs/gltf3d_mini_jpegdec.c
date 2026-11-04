#include "gltf3d_mini_jpegdec.h"

#define JPG_MAX_COMPS 3
#define JPG_MAX_HUFF_SYMBOLS 256
#define JPG_BLOCK_COEFFS 64
#define JPG_MAX_BLOCKS_PER_COMP 16

static const unsigned char jpg_zigzag[64] = {
 0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
 12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
 35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
 58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
};

static const int jpg_idct_basis[8][8] = {
 {45,45,45,45,45,45,45,45},
 {63,53,36,12,-12,-36,-53,-63},
 {59,24,-24,-59,-59,-24,24,59},
 {53,-12,-63,-36,36,63,12,-53},
 {45,-45,-45,45,45,-45,-45,45},
 {36,-63,12,53,-53,-12,63,-36},
 {24,-59,59,-24,-24,59,-59,24},
 {12,-36,53,-63,63,-53,36,-12}
};

typedef struct jpg_huff_s {
    unsigned char counts[16];
    unsigned char symbols[JPG_MAX_HUFF_SYMBOLS];
    int mincode[17];
    int maxcode[18];
    int valptr[17];
    int valid;
} jpg_huff;

typedef struct jpg_comp_s {
    int id;
    int h;
    int v;
    int tq;
    int td;
    int ta;
    int dc_pred;
} jpg_comp;

typedef struct jpg_ctx_s {
    unsigned short q[4][64];
    int qvalid[4];
    jpg_huff hd[4];
    jpg_huff ha[4];
    jpg_comp comp[JPG_MAX_COMPS];
    int comp_count;
    int width;
    int height;
    int max_h;
    int max_v;
    unsigned long restart_interval;
    int saw_sof0;
    int saw_sos;
    int saw_jfif;
    int saw_exif;
    int saw_adobe;
    unsigned long scan_off;
    unsigned long scan_len;
} jpg_ctx;

typedef struct jpg_br_s {
    const unsigned char *data;
    unsigned long size;
    unsigned long pos;
    unsigned int bitbuf;
    int bits;
    int marker;
} jpg_br;

static void jpg_memzero(void *p, unsigned long n)
{
    unsigned long i;
    unsigned char *b;
    b = (unsigned char *)p;
    for (i = 0UL; i < n; ++i) b[i] = 0U;
}

static unsigned int jpg_be16(const unsigned char *p)
{
    return (unsigned int)(((unsigned int)p[0] << 8) | (unsigned int)p[1]);
}

static int jpg_clamp_int(long v)
{
    if (v < 0L) return 0;
    if (v > 255L) return 255;
    return (int)v;
}

static int jpg_is_standalone(int m)
{
    if (m == 0xd8 || m == 0xd9 || m == 0x01) return 1;
    if (m >= 0xd0 && m <= 0xd7) return 1;
    return 0;
}

static int jpg_find_next_marker(const unsigned char *data, unsigned long size, unsigned long *pos, int *marker)
{
    unsigned long p;
    p = *pos;
    while (p < size && data[p] != 0xffU) p++;
    if (p >= size) return 0;
    while (p < size && data[p] == 0xffU) p++;
    if (p >= size) return 0;
    if (data[p] == 0x00U) { *pos = p + 1UL; return jpg_find_next_marker(data, size, pos, marker); }
    *marker = (int)data[p];
    *pos = p + 1UL;
    return 1;
}

static unsigned long jpg_find_scan_end(const unsigned char *data, unsigned long size, unsigned long p)
{
    while (p + 1UL < size) {
        if (data[p] == 0xffU) {
            if (data[p + 1UL] == 0x00U) { p += 2UL; continue; }
            if (data[p + 1UL] == 0xffU) { p++; continue; }
            if (data[p + 1UL] >= 0xd0U && data[p + 1UL] <= 0xd7U) { p += 2UL; continue; }
            return p;
        }
        p++;
    }
    return size;
}

static int jpg_build_huff(jpg_huff *h)
{
    int code;
    int k;
    int i;
    code = 0;
    k = 0;
    h->maxcode[0] = -1;
    for (i = 1; i <= 16; ++i) {
        int cnt;
        cnt = (int)h->counts[i - 1];
        if (cnt == 0) {
            h->mincode[i] = -1;
            h->maxcode[i] = -1;
            h->valptr[i] = k;
        } else {
            h->valptr[i] = k;
            h->mincode[i] = code;
            code += cnt - 1;
            h->maxcode[i] = code;
            code++;
            k += cnt;
        }
        code <<= 1;
    }
    h->maxcode[17] = 0x7fffffff;
    h->valid = 1;
    return GLTF3D_MINI_OK;
}

static int jpg_parse_dqt(jpg_ctx *c, const unsigned char *seg, unsigned long len)
{
    unsigned long p;
    p = 0UL;
    while (p < len) {
        int pq;
        int tq;
        int i;
        if (p >= len) return GLTF3D_MINI_ERR_BAD_DATA;
        pq = (int)(seg[p] >> 4);
        tq = (int)(seg[p] & 15U);
        p++;
        if (tq < 0 || tq > 3 || pq > 1) return GLTF3D_MINI_ERR_BAD_DATA;
        if (p + (pq ? 128UL : 64UL) > len) return GLTF3D_MINI_ERR_BAD_DATA;
        for (i = 0; i < 64; ++i) {
            if (pq) {
                c->q[tq][jpg_zigzag[i]] = (unsigned short)jpg_be16(seg + p);
                p += 2UL;
            } else {
                c->q[tq][jpg_zigzag[i]] = (unsigned short)seg[p++];
            }
        }
        c->qvalid[tq] = 1;
    }
    return GLTF3D_MINI_OK;
}

static int jpg_parse_dht(jpg_ctx *c, const unsigned char *seg, unsigned long len)
{
    unsigned long p;
    p = 0UL;
    while (p < len) {
        int tc;
        int th;
        int i;
        unsigned long n;
        jpg_huff *h;
        if (p + 17UL > len) return GLTF3D_MINI_ERR_BAD_DATA;
        tc = (int)(seg[p] >> 4);
        th = (int)(seg[p] & 15U);
        p++;
        if (tc > 1 || th > 3) return GLTF3D_MINI_ERR_BAD_DATA;
        h = (tc == 0) ? &c->hd[th] : &c->ha[th];
        n = 0UL;
        for (i = 0; i < 16; ++i) { h->counts[i] = seg[p++]; n += (unsigned long)h->counts[i]; }
        if (n > JPG_MAX_HUFF_SYMBOLS || p + n > len) return GLTF3D_MINI_ERR_BAD_DATA;
        for (i = 0; i < (int)n; ++i) h->symbols[i] = seg[p + (unsigned long)i];
        p += n;
        jpg_build_huff(h);
    }
    return GLTF3D_MINI_OK;
}

static int jpg_parse_sof0(jpg_ctx *c, const unsigned char *seg, unsigned long len)
{
    int i;
    int p;
    if (len < 8UL) return GLTF3D_MINI_ERR_BAD_DATA;
    if (seg[0] != 8U) return GLTF3D_MINI_ERR_UNSUPPORTED;
    c->height = (int)jpg_be16(seg + 1UL);
    c->width = (int)jpg_be16(seg + 3UL);
    c->comp_count = (int)seg[5];
    if (c->width <= 0 || c->height <= 0) return GLTF3D_MINI_ERR_BAD_DATA;
    if (c->comp_count != 1 && c->comp_count != 3) return GLTF3D_MINI_ERR_UNSUPPORTED;
    if (len < (unsigned long)(6 + c->comp_count * 3)) return GLTF3D_MINI_ERR_BAD_DATA;
    c->max_h = 1;
    c->max_v = 1;
    p = 6;
    for (i = 0; i < c->comp_count; ++i) {
        c->comp[i].id = (int)seg[p++];
        c->comp[i].h = (int)(seg[p] >> 4);
        c->comp[i].v = (int)(seg[p] & 15U);
        p++;
        c->comp[i].tq = (int)seg[p++];
        c->comp[i].dc_pred = 0;
        if (c->comp[i].h < 1 || c->comp[i].h > 4 || c->comp[i].v < 1 || c->comp[i].v > 4) return GLTF3D_MINI_ERR_UNSUPPORTED;
        if (c->comp[i].tq < 0 || c->comp[i].tq > 3) return GLTF3D_MINI_ERR_BAD_DATA;
        if (c->comp[i].h * c->comp[i].v > JPG_MAX_BLOCKS_PER_COMP) return GLTF3D_MINI_ERR_UNSUPPORTED;
        if (c->comp[i].h > c->max_h) c->max_h = c->comp[i].h;
        if (c->comp[i].v > c->max_v) c->max_v = c->comp[i].v;
    }
    c->saw_sof0 = 1;
    return GLTF3D_MINI_OK;
}

static int jpg_parse_sos(jpg_ctx *c, const unsigned char *seg, unsigned long len)
{
    int ns;
    int i;
    int p;
    if (len < 6UL) return GLTF3D_MINI_ERR_BAD_DATA;
    ns = (int)seg[0];
    if (ns != c->comp_count) return GLTF3D_MINI_ERR_UNSUPPORTED;
    if (len < (unsigned long)(1 + ns * 2 + 3)) return GLTF3D_MINI_ERR_BAD_DATA;
    p = 1;
    for (i = 0; i < ns; ++i) {
        int id;
        int j;
        id = (int)seg[p++];
        for (j = 0; j < c->comp_count; ++j) if (c->comp[j].id == id) break;
        if (j == c->comp_count) return GLTF3D_MINI_ERR_BAD_DATA;
        c->comp[j].td = (int)(seg[p] >> 4);
        c->comp[j].ta = (int)(seg[p] & 15U);
        p++;
        if (c->comp[j].td < 0 || c->comp[j].td > 3 || c->comp[j].ta < 0 || c->comp[j].ta > 3) return GLTF3D_MINI_ERR_BAD_DATA;
    }
    if (seg[p] != 0U || seg[p + 1] != 63U || seg[p + 2] != 0U) return GLTF3D_MINI_ERR_UNSUPPORTED;
    c->saw_sos = 1;
    return GLTF3D_MINI_OK;
}

static int jpg_parse_headers(jpg_ctx *c, const unsigned char *data, unsigned long size)
{
    unsigned long p;
    int m;
    int r;
    if (size < 4UL || data[0] != 0xffU || data[1] != 0xd8U) return GLTF3D_MINI_ERR_BAD_DATA;
    jpg_memzero(c, (unsigned long)sizeof(*c));
    p = 2UL;
    while (jpg_find_next_marker(data, size, &p, &m)) {
        unsigned int seglen;
        unsigned long segstart;
        if (m == 0xd9) return GLTF3D_MINI_ERR_BAD_DATA;
        if (jpg_is_standalone(m)) continue;
        if (p + 2UL > size) return GLTF3D_MINI_ERR_BAD_DATA;
        seglen = jpg_be16(data + p);
        if (seglen < 2U || p + (unsigned long)seglen > size) return GLTF3D_MINI_ERR_BAD_DATA;
        segstart = p + 2UL;
        if (m >= 0xe0 && m <= 0xef) {
            if (m == 0xe0 && seglen >= 7U && data[segstart] == 'J' && data[segstart + 1UL] == 'F' && data[segstart + 2UL] == 'I' && data[segstart + 3UL] == 'F' && data[segstart + 4UL] == 0U) c->saw_jfif = 1;
            if (m == 0xe1 && seglen >= 8U && data[segstart] == 'E' && data[segstart + 1UL] == 'x' && data[segstart + 2UL] == 'i' && data[segstart + 3UL] == 'f' && data[segstart + 4UL] == 0U) c->saw_exif = 1;
            if (m == 0xee && seglen >= 7U && data[segstart] == 'A' && data[segstart + 1UL] == 'd' && data[segstart + 2UL] == 'o' && data[segstart + 3UL] == 'b' && data[segstart + 4UL] == 'e') c->saw_adobe = 1;
        } else if (m == 0xdb) {
            r = jpg_parse_dqt(c, data + segstart, (unsigned long)seglen - 2UL);
            if (r != GLTF3D_MINI_OK) return r;
        } else if (m == 0xc4) {
            r = jpg_parse_dht(c, data + segstart, (unsigned long)seglen - 2UL);
            if (r != GLTF3D_MINI_OK) return r;
        } else if (m == 0xdd) {
            if (seglen != 4U) return GLTF3D_MINI_ERR_BAD_DATA;
            c->restart_interval = (unsigned long)jpg_be16(data + segstart);
        } else if (m == 0xc0) {
            r = jpg_parse_sof0(c, data + segstart, (unsigned long)seglen - 2UL);
            if (r != GLTF3D_MINI_OK) return r;
        } else if (m == 0xc2 || m == 0xc1 || m == 0xc3 || m == 0xc5 || m == 0xc6 || m == 0xc7 || m == 0xc9 || m == 0xca || m == 0xcb || m == 0xcd || m == 0xce || m == 0xcf) {
            return GLTF3D_MINI_ERR_UNSUPPORTED;
        } else if (m == 0xda) {
            r = jpg_parse_sos(c, data + segstart, (unsigned long)seglen - 2UL);
            if (r != GLTF3D_MINI_OK) return r;
            c->scan_off = p + (unsigned long)seglen;
            c->scan_len = jpg_find_scan_end(data, size, c->scan_off) - c->scan_off;
            return GLTF3D_MINI_OK;
        }
        p += (unsigned long)seglen;
    }
    return GLTF3D_MINI_ERR_BAD_DATA;
}

static void br_init(jpg_br *br, const unsigned char *data, unsigned long size)
{
    br->data = data;
    br->size = size;
    br->pos = 0UL;
    br->bitbuf = 0U;
    br->bits = 0;
    br->marker = 0;
}

static int br_byte(jpg_br *br)
{
    int b;
    int m;
    if (br->pos >= br->size) return -1;
    b = (int)br->data[br->pos++];
    if (b == 0xff) {
        while (br->pos < br->size && br->data[br->pos] == 0xffU) br->pos++;
        if (br->pos >= br->size) return -1;
        m = (int)br->data[br->pos++];
        if (m == 0x00) return 0xff;
        br->marker = m;
        return -1;
    }
    return b;
}

static int br_get_bits(jpg_br *br, int n, unsigned int *out)
{
    int b;
    while (br->bits < n) {
        b = br_byte(br);
        if (b < 0) return GLTF3D_MINI_ERR_BAD_DATA;
        br->bitbuf = (br->bitbuf << 8) | (unsigned int)b;
        br->bits += 8;
    }
    *out = (br->bitbuf >> (br->bits - n)) & ((1U << n) - 1U);
    br->bits -= n;
    return GLTF3D_MINI_OK;
}

static void br_byte_align(jpg_br *br)
{
    br->bits = 0;
    br->bitbuf = 0U;
}

static int jpg_huff_decode(jpg_br *br, const jpg_huff *h, int *sym)
{
    int code;
    int len;
    unsigned int bit;
    if (!h || !h->valid) return GLTF3D_MINI_ERR_BAD_DATA;
    code = 0;
    for (len = 1; len <= 16; ++len) {
        if (br_get_bits(br, 1, &bit) != GLTF3D_MINI_OK) return GLTF3D_MINI_ERR_BAD_DATA;
        code = (code << 1) | (int)bit;
        if (h->maxcode[len] >= 0 && code <= h->maxcode[len]) {
            int idx;
            idx = h->valptr[len] + code - h->mincode[len];
            if (idx < 0 || idx >= JPG_MAX_HUFF_SYMBOLS) return GLTF3D_MINI_ERR_BAD_DATA;
            *sym = (int)h->symbols[idx];
            return GLTF3D_MINI_OK;
        }
    }
    return GLTF3D_MINI_ERR_BAD_DATA;
}

static int jpg_receive_extend(jpg_br *br, int s, int *out)
{
    unsigned int v;
    int vt;
    if (s == 0) { *out = 0; return GLTF3D_MINI_OK; }
    if (s < 0 || s > 16) return GLTF3D_MINI_ERR_BAD_DATA;
    if (br_get_bits(br, s, &v) != GLTF3D_MINI_OK) return GLTF3D_MINI_ERR_BAD_DATA;
    vt = 1 << (s - 1);
    if ((int)v < vt) *out = (int)v + ((-1) << s) + 1;
    else *out = (int)v;
    return GLTF3D_MINI_OK;
}

static void jpg_idct_block(const int in[64], unsigned char out[64])
{
    long tmp[64];
    int x;
    int y;
    int u;
    int v;
    for (y = 0; y < 8; ++y) {
        for (x = 0; x < 8; ++x) {
            long s;
            s = 0L;
            for (u = 0; u < 8; ++u) s += (long)jpg_idct_basis[u][x] * (long)in[y * 8 + u];
            if (s >= 0L) tmp[y * 8 + x] = (s + 32L) / 64L;
            else tmp[y * 8 + x] = -((-s + 32L) / 64L);
        }
    }
    for (y = 0; y < 8; ++y) {
        for (x = 0; x < 8; ++x) {
            long s2;
            s2 = 0L;
            for (v = 0; v < 8; ++v) s2 += (long)jpg_idct_basis[v][y] * tmp[v * 8 + x];
            if (s2 >= 0L) s2 = (s2 + 128L) / 256L;
            else s2 = -((-s2 + 128L) / 256L);
            out[y * 8 + x] = (unsigned char)jpg_clamp_int(s2 + 128L);
        }
    }
}

static int jpg_decode_block(jpg_br *br, jpg_ctx *ctx, jpg_comp *c, unsigned char out[64])
{
    int coeff[64];
    int i;
    int s;
    int diff;
    int rs;
    int r;
    int k;
    for (i = 0; i < 64; ++i) coeff[i] = 0;
    if (!ctx->qvalid[c->tq] || !ctx->hd[c->td].valid || !ctx->ha[c->ta].valid) return GLTF3D_MINI_ERR_BAD_DATA;
    r = jpg_huff_decode(br, &ctx->hd[c->td], &s);
    if (r != GLTF3D_MINI_OK) return r;
    if (s < 0 || s > 11) return GLTF3D_MINI_ERR_BAD_DATA;
    r = jpg_receive_extend(br, s, &diff);
    if (r != GLTF3D_MINI_OK) return r;
    c->dc_pred += diff;
    coeff[0] = c->dc_pred * (int)ctx->q[c->tq][0];
    k = 1;
    while (k < 64) {
        r = jpg_huff_decode(br, &ctx->ha[c->ta], &rs);
        if (r != GLTF3D_MINI_OK) return r;
        if (rs == 0) break;
        if (rs == 0xf0) { k += 16; continue; }
        k += (rs >> 4);
        s = rs & 15;
        if (k >= 64) return GLTF3D_MINI_ERR_BAD_DATA;
        r = jpg_receive_extend(br, s, &diff);
        if (r != GLTF3D_MINI_OK) return r;
        coeff[jpg_zigzag[k]] = diff * (int)ctx->q[c->tq][jpg_zigzag[k]];
        k++;
    }
    jpg_idct_block(coeff, out);
    return GLTF3D_MINI_OK;
}

static unsigned char jpg_get_component_sample(unsigned char blocks[JPG_MAX_BLOCKS_PER_COMP][64], int ch, int cv, int maxh, int maxv, int px, int py)
{
    int sx;
    int sy;
    int bx;
    int by;
    int ix;
    int iy;
    if (ch <= 0) ch = 1;
    if (cv <= 0) cv = 1;
    sx = (px * ch) / maxh;
    sy = (py * cv) / maxv;
    bx = sx / 8;
    by = sy / 8;
    ix = sx & 7;
    iy = sy & 7;
    return blocks[by * ch + bx][iy * 8 + ix];
}

int gltf3d_mini_jpeg_decode_rgba8(const unsigned char *data,
                                  unsigned long size,
                                  unsigned char *dst_rgba,
                                  unsigned long dst_size,
                                  void *work,
                                  unsigned long work_size,
                                  gltf3d_mini_jpegdec_result *result)
{
    jpg_ctx ctx;
    jpg_br br;
    unsigned long need;
    unsigned long out_need;
    unsigned long mcu_x_count;
    unsigned long mcu_y_count;
    unsigned long mx;
    unsigned long my;
    unsigned long mcu_index;
    int c;
    int bx;
    int by;
    int r;
    unsigned char (*blocks)[JPG_MAX_BLOCKS_PER_COMP][64];
    if (!data || !dst_rgba || !work) return GLTF3D_MINI_ERR_BAD_ARG;
    if (work_size < GLTF3D_MINI_JPEGDEC_MIN_WORK) return GLTF3D_MINI_ERR_RANGE;
    r = jpg_parse_headers(&ctx, data, size);
    if (r != GLTF3D_MINI_OK) return r;
    if (!ctx.saw_sof0 || !ctx.saw_sos) return GLTF3D_MINI_ERR_BAD_DATA;
    if (ctx.comp_count != 1 && ctx.comp_count != 3) return GLTF3D_MINI_ERR_UNSUPPORTED;
    need = (unsigned long)JPG_MAX_COMPS * (unsigned long)JPG_MAX_BLOCKS_PER_COMP * 64UL;
    if (work_size < need) return GLTF3D_MINI_ERR_RANGE;
    out_need = (unsigned long)ctx.width * (unsigned long)ctx.height * 4UL;
    if (ctx.width <= 0 || ctx.height <= 0 || out_need / 4UL / (unsigned long)ctx.width != (unsigned long)ctx.height) return GLTF3D_MINI_ERR_RANGE;
    if (dst_size < out_need) return GLTF3D_MINI_ERR_RANGE;
    blocks = (unsigned char (*)[JPG_MAX_BLOCKS_PER_COMP][64])work;
    br_init(&br, data + ctx.scan_off, ctx.scan_len);
    mcu_x_count = ((unsigned long)ctx.width + (unsigned long)(ctx.max_h * 8 - 1)) / (unsigned long)(ctx.max_h * 8);
    mcu_y_count = ((unsigned long)ctx.height + (unsigned long)(ctx.max_v * 8 - 1)) / (unsigned long)(ctx.max_v * 8);
    mcu_index = 0UL;
    for (my = 0UL; my < mcu_y_count; ++my) {
        for (mx = 0UL; mx < mcu_x_count; ++mx) {
            int px;
            int py;
            for (c = 0; c < ctx.comp_count; ++c) {
                for (by = 0; by < ctx.comp[c].v; ++by) {
                    for (bx = 0; bx < ctx.comp[c].h; ++bx) {
                        r = jpg_decode_block(&br, &ctx, &ctx.comp[c], blocks[c][by * ctx.comp[c].h + bx]);
                        if (r != GLTF3D_MINI_OK) return r;
                    }
                }
            }
            for (py = 0; py < ctx.max_v * 8; ++py) {
                unsigned long gy;
                gy = my * (unsigned long)(ctx.max_v * 8) + (unsigned long)py;
                if (gy >= (unsigned long)ctx.height) continue;
                for (px = 0; px < ctx.max_h * 8; ++px) {
                    unsigned long gx;
                    unsigned long oi;
                    int yv;
                    int cb;
                    int cr;
                    int rr;
                    int gg;
                    int bb;
                    gx = mx * (unsigned long)(ctx.max_h * 8) + (unsigned long)px;
                    if (gx >= (unsigned long)ctx.width) continue;
                    if (ctx.comp_count == 1) {
                        yv = (int)jpg_get_component_sample(blocks[0], ctx.comp[0].h, ctx.comp[0].v, ctx.max_h, ctx.max_v, px, py);
                        rr = yv; gg = yv; bb = yv;
                    } else {
                        yv = (int)jpg_get_component_sample(blocks[0], ctx.comp[0].h, ctx.comp[0].v, ctx.max_h, ctx.max_v, px, py);
                        cb = (int)jpg_get_component_sample(blocks[1], ctx.comp[1].h, ctx.comp[1].v, ctx.max_h, ctx.max_v, px, py) - 128;
                        cr = (int)jpg_get_component_sample(blocks[2], ctx.comp[2].h, ctx.comp[2].v, ctx.max_h, ctx.max_v, px, py) - 128;
                        rr = jpg_clamp_int((long)yv + ((91881L * (long)cr) >> 16));
                        gg = jpg_clamp_int((long)yv - ((22554L * (long)cb + 46802L * (long)cr) >> 16));
                        bb = jpg_clamp_int((long)yv + ((116130L * (long)cb) >> 16));
                    }
                    oi = (gy * (unsigned long)ctx.width + gx) * 4UL;
                    dst_rgba[oi + 0UL] = (unsigned char)rr;
                    dst_rgba[oi + 1UL] = (unsigned char)gg;
                    dst_rgba[oi + 2UL] = (unsigned char)bb;
                    dst_rgba[oi + 3UL] = 255U;
                }
            }
            mcu_index++;
            if (ctx.restart_interval && (mcu_index % ctx.restart_interval) == 0UL) {
                int i;
                br_byte_align(&br);
                for (i = 0; i < ctx.comp_count; ++i) ctx.comp[i].dc_pred = 0;
                if (br.marker >= 0xd0 && br.marker <= 0xd7) br.marker = 0;
            }
        }
    }
    if (result) {
        result->width = (unsigned long)ctx.width;
        result->height = (unsigned long)ctx.height;
        result->mcu_count = mcu_index;
        result->restart_interval = ctx.restart_interval;
        result->component_count = ctx.comp_count;
        result->saw_jfif = ctx.saw_jfif;
        result->saw_exif = ctx.saw_exif;
        result->saw_adobe = ctx.saw_adobe;
    }
    return GLTF3D_MINI_OK;
}
