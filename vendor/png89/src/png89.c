#include "png89.h"
#include "zlib89.h"

static unsigned int be32(const unsigned char *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}
static unsigned int be16(const unsigned char *p)
{
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}
static int typeeq(const unsigned char *p, const char *t)
{
    return p[0] == (unsigned char)t[0] && p[1] == (unsigned char)t[1] &&
           p[2] == (unsigned char)t[2] && p[3] == (unsigned char)t[3];
}

typedef struct png_idat_s {
    const unsigned char *s;
    size_t n;
    size_t scan;
    size_t pos;
    size_t remain;
} png_idat;

static int idat_get(void *u, unsigned char *out)
{
    png_idat *r;
    r = (png_idat *)u;
    while (r->remain == 0u) {
        while (r->scan + 12u <= r->n) {
            unsigned int len;
            size_t next;
            len = be32(r->s + r->scan);
            next = r->scan + 12u + (size_t)len;
            if (next > r->n) return 0;
            if (typeeq(r->s + r->scan + 4u, "IDAT")) {
                r->pos = r->scan + 8u;
                r->remain = (size_t)len;
                r->scan = next;
                break;
            }
            r->scan = next;
        }
        if (r->remain == 0u) return 0;
    }
    *out = r->s[r->pos++];
    r->remain--;
    return 1;
}

static unsigned char paeth(unsigned char a, unsigned char b, unsigned char c)
{
    int p, pa, pb, pc;
    p = (int)a + (int)b - (int)c;
    pa = p - (int)a;
    pb = p - (int)b;
    pc = p - (int)c;
    if (pa < 0) pa = -pa;
    if (pb < 0) pb = -pb;
    if (pc < 0) pc = -pc;
    return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

static unsigned int getpacked(const unsigned char *r, unsigned int x, unsigned int depth)
{
    unsigned int ppb, sh;
    ppb = 8u / depth;
    sh = (ppb - 1u - (x % ppb)) * depth;
    return (r[x / ppb] >> sh) & ((1u << depth) - 1u);
}

static int unfilter_rows(unsigned char *base, unsigned int rows, size_t rowbytes,
                         unsigned int bpp)
{
    unsigned int y;
    for (y = 0u; y < rows; ++y) {
        unsigned char *f;
        unsigned char *row;
        unsigned char *prev;
        unsigned int filter;
        size_t k;
        f = base + (size_t)y * (rowbytes + 1u);
        row = f + 1u;
        prev = y ? base + (size_t)(y - 1u) * (rowbytes + 1u) + 1u : 0;
        filter = f[0];
        for (k = 0u; k < rowbytes; ++k) {
            unsigned char a, b, c;
            a = k >= bpp ? row[k - bpp] : 0u;
            b = prev ? prev[k] : 0u;
            c = (prev && k >= bpp) ? prev[k - bpp] : 0u;
            if (filter == 1u) row[k] = (unsigned char)(row[k] + a);
            else if (filter == 2u) row[k] = (unsigned char)(row[k] + b);
            else if (filter == 3u)
                row[k] = (unsigned char)(row[k] +
                         (unsigned char)(((unsigned int)a + (unsigned int)b) / 2u));
            else if (filter == 4u) row[k] = (unsigned char)(row[k] + paeth(a, b, c));
            else if (filter != 0u) return 0;
        }
    }
    return 1;
}

static int pixel_rgba(const unsigned char *r, unsigned int x,
                      unsigned int depth, unsigned int ct,
                      unsigned char pal[256][4], unsigned int paln,
                      unsigned int tr_gray, unsigned int tr_r,
                      unsigned int tr_g, unsigned int tr_b,
                      unsigned char *o)
{
    unsigned char R, G, B, A;
    R = G = B = 0u;
    A = 255u;
    if (ct == 0u) {
        unsigned int v;
        if (depth < 8u) {
            unsigned int maxv;
            maxv = (1u << depth) - 1u;
            v = getpacked(r, x, depth);
            R = G = B = (unsigned char)(v * 255u / maxv);
            if (tr_gray != 0xffffffffu && v == tr_gray) A = 0u;
        } else if (depth == 8u) {
            v = r[x];
            R = G = B = (unsigned char)v;
            if (tr_gray != 0xffffffffu && v == (tr_gray & 255u)) A = 0u;
        } else {
            v = be16(r + (size_t)x * 2u);
            R = G = B = r[(size_t)x * 2u];
            if (tr_gray != 0xffffffffu && v == tr_gray) A = 0u;
        }
    } else if (ct == 2u) {
        if (depth == 8u) {
            const unsigned char *p;
            p = r + (size_t)x * 3u;
            R = p[0]; G = p[1]; B = p[2];
            if (tr_r != 0xffffffffu && R == (tr_r & 255u) &&
                G == (tr_g & 255u) && B == (tr_b & 255u)) A = 0u;
        } else {
            const unsigned char *p;
            unsigned int vr, vg, vb;
            p = r + (size_t)x * 6u;
            vr = be16(p); vg = be16(p + 2u); vb = be16(p + 4u);
            R = p[0]; G = p[2]; B = p[4];
            if (vr == tr_r && vg == tr_g && vb == tr_b) A = 0u;
        }
    } else if (ct == 3u) {
        unsigned int idx;
        idx = depth == 8u ? r[x] : getpacked(r, x, depth);
        if (idx >= paln) return 0;
        R = pal[idx][0]; G = pal[idx][1]; B = pal[idx][2]; A = pal[idx][3];
    } else if (ct == 4u) {
        if (depth == 8u) {
            R = G = B = r[(size_t)x * 2u];
            A = r[(size_t)x * 2u + 1u];
        } else {
            R = G = B = r[(size_t)x * 4u];
            A = r[(size_t)x * 4u + 2u];
        }
    } else if (ct == 6u) {
        if (depth == 8u) {
            const unsigned char *p;
            p = r + (size_t)x * 4u;
            R = p[0]; G = p[1]; B = p[2]; A = p[3];
        } else {
            const unsigned char *p;
            p = r + (size_t)x * 8u;
            R = p[0]; G = p[2]; B = p[4]; A = p[6];
        }
    } else return 0;
    o[0] = R; o[1] = G; o[2] = B; o[3] = A;
    return 1;
}

int png89_probe(const unsigned char *s, size_t n)
{
    static const unsigned char sig[8] = {137,80,78,71,13,10,26,10};
    unsigned int i;
    if (s == 0 || n < 8u) return 0;
    for (i = 0u; i < 8u; ++i) if (s[i] != sig[i]) return 0;
    return 1;
}

int png89_get_info(const unsigned char *s, size_t n, png89_info *o)
{
    if (o == 0 || !png89_probe(s, n) || n < 33u) return -1;
    if (s[12] != 'I' || s[13] != 'H' || s[14] != 'D' || s[15] != 'R') return -2;
    o->width = be32(s + 16u);
    o->height = be32(s + 20u);
    o->bit_depth = s[24u];
    o->color_type = s[25u];
    o->interlace_method = s[28u];
    if (o->width == 0u || o->height == 0u) return -2;
    return 0;
}

int png89_decode_rgba(const unsigned char *s, size_t n, unsigned char *d, size_t cap,
                      unsigned int *ow, unsigned int *oh, size_t *used)
{
    static const unsigned int ax[7] = {0u,4u,0u,2u,0u,1u,0u};
    static const unsigned int ay[7] = {0u,0u,4u,0u,2u,0u,1u};
    static const unsigned int dx[7] = {8u,8u,4u,4u,2u,2u,1u};
    static const unsigned int dy[7] = {8u,8u,8u,4u,4u,2u,2u};
    unsigned int w, h, depth, ct, inter, bitspp, bpp;
    unsigned char pal[256][4];
    unsigned int paln;
    unsigned int tr_gray, tr_r, tr_g, tr_b;
    size_t scan, rawneed, rawgot, outneed;
    png_idat rd;
    unsigned char *raw;
    int rc;
    unsigned int i, pass_count;

    w = h = depth = ct = inter = bitspp = bpp = 0u;
    paln = 0u;
    tr_gray = tr_r = tr_g = tr_b = 0xffffffffu;
    if (!png89_probe(s, n) || d == 0 || ow == 0 || oh == 0 || used == 0) return -1;
    for (i = 0u; i < 256u; ++i) {
        pal[i][0] = pal[i][1] = pal[i][2] = 0u;
        pal[i][3] = 255u;
    }
    scan = 8u;
    while (scan + 12u <= n) {
        unsigned int len;
        const unsigned char *t, *p;
        size_t next;
        len = be32(s + scan);
        t = s + scan + 4u;
        p = s + scan + 8u;
        next = scan + 12u + (size_t)len;
        if (next > n) return -2;
        if (typeeq(t, "IHDR")) {
            if (len != 13u) return -2;
            w = be32(p); h = be32(p + 4u); depth = p[8]; ct = p[9];
            if (p[10] != 0u || p[11] != 0u) return -3;
            inter = p[12];
        } else if (typeeq(t, "PLTE")) {
            if (len % 3u != 0u || len / 3u > 256u) return -2;
            paln = len / 3u;
            for (i = 0u; i < paln; ++i) {
                pal[i][0] = p[i * 3u];
                pal[i][1] = p[i * 3u + 1u];
                pal[i][2] = p[i * 3u + 2u];
            }
        } else if (typeeq(t, "tRNS")) {
            if (ct == 3u) {
                unsigned int m;
                m = len < 256u ? len : 256u;
                for (i = 0u; i < m; ++i) pal[i][3] = p[i];
            } else if (ct == 0u && len >= 2u) tr_gray = be16(p);
            else if (ct == 2u && len >= 6u) {
                tr_r = be16(p); tr_g = be16(p + 2u); tr_b = be16(p + 4u);
            }
        } else if (typeeq(t, "IEND")) break;
        scan = next;
    }
    if (w == 0u || h == 0u || w > 16384u || h > 16384u || inter > 1u) return -4;
    if (ct == 0u) bitspp = depth;
    else if (ct == 2u) bitspp = 3u * depth;
    else if (ct == 3u) bitspp = depth;
    else if (ct == 4u) bitspp = 2u * depth;
    else if (ct == 6u) bitspp = 4u * depth;
    else return -5;
    if (!(depth == 1u || depth == 2u || depth == 4u || depth == 8u || depth == 16u)) return -5;
    if ((ct == 2u || ct == 4u || ct == 6u) && depth < 8u) return -5;
    if (ct == 3u && paln == 0u) return -5;
    bpp = (bitspp + 7u) / 8u;
    if (bpp == 0u) bpp = 1u;

    rawneed = 0u;
    pass_count = inter ? 7u : 1u;
    for (i = 0u; i < pass_count; ++i) {
        unsigned int pi, pw, ph, x0, y0, xs, ys;
        size_t rowbytes;
        pi = i;
        x0 = inter ? ax[pi] : 0u;
        y0 = inter ? ay[pi] : 0u;
        xs = inter ? dx[pi] : 1u;
        ys = inter ? dy[pi] : 1u;
        if (w <= x0 || h <= y0) continue;
        pw = (w - x0 + xs - 1u) / xs;
        ph = (h - y0 + ys - 1u) / ys;
        rowbytes = ((size_t)pw * bitspp + 7u) / 8u;
        if (ph != 0u && rowbytes + 1u > ((size_t)-1) / ph) return -6;
        if (rawneed > (size_t)-1 - (rowbytes + 1u) * ph) return -6;
        rawneed += (rowbytes + 1u) * ph;
    }
    outneed = (size_t)w * h * 4u;
    if ((size_t)w * h > cap / 4u || rawneed > cap - outneed) return -7;
    raw = d + cap - rawneed;
    rd.s = s; rd.n = n; rd.scan = 8u; rd.pos = 0u; rd.remain = 0u;
    rawgot = 0u;
    rc = zlib89_inflate_reader(idat_get, &rd, raw, rawneed, &rawgot);
    if (rc != 0 || rawgot != rawneed) return -8;

    {
        size_t roff;
        roff = 0u;
        for (i = 0u; i < pass_count; ++i) {
            unsigned int pi, pw, ph, py, px, x0, y0, xs, ys;
            size_t rowbytes, passbytes;
            pi = i;
            x0 = inter ? ax[pi] : 0u;
            y0 = inter ? ay[pi] : 0u;
            xs = inter ? dx[pi] : 1u;
            ys = inter ? dy[pi] : 1u;
            if (w <= x0 || h <= y0) continue;
            pw = (w - x0 + xs - 1u) / xs;
            ph = (h - y0 + ys - 1u) / ys;
            rowbytes = ((size_t)pw * bitspp + 7u) / 8u;
            passbytes = (rowbytes + 1u) * ph;
            if (!unfilter_rows(raw + roff, ph, rowbytes, bpp)) return -9;
            for (py = 0u; py < ph; ++py) {
                const unsigned char *r;
                unsigned int gy;
                r = raw + roff + (size_t)py * (rowbytes + 1u) + 1u;
                gy = y0 + py * ys;
                for (px = 0u; px < pw; ++px) {
                    unsigned int gx;
                    size_t q;
                    gx = x0 + px * xs;
                    q = ((size_t)gy * w + gx) * 4u;
                    if (!pixel_rgba(r, px, depth, ct, pal, paln,
                                    tr_gray, tr_r, tr_g, tr_b, d + q)) return -10;
                }
            }
            roff += passbytes;
        }
    }
    *ow = w; *oh = h; *used = outneed;
    return 0;
}
