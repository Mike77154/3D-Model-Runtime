#include "tim89.h"

static unsigned int r16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}
static unsigned int r32(const unsigned char *p)
{
    return r16(p) | (r16(p + 2u) << 16);
}
static void c15(unsigned int v, unsigned char *o)
{
    unsigned int rgb, stp;
    rgb = v & 0x7fffu; stp = (v >> 15) & 1u;
    o[0] = (unsigned char)((v & 31u) * 255u / 31u);
    o[1] = (unsigned char)(((v >> 5) & 31u) * 255u / 31u);
    o[2] = (unsigned char)(((v >> 10) & 31u) * 255u / 31u);
    if (rgb == 0u && stp == 0u) o[3] = 0u;
    else if (rgb != 0u && stp != 0u) o[3] = 128u;
    else o[3] = 255u;
}

typedef struct t89_s {
    unsigned int mode, hasclut, pixw, h, ww;
    unsigned int image_x, image_y, clut_x, clut_y;
    unsigned int clutw, cluth, palettes, palsize;
    size_t clut_off, pixel_off, pixel_bytes;
} t89;

static int parse(const unsigned char *s, size_t n, t89 *a)
{
    unsigned int flags, blen;
    size_t p;
    if (s == 0 || a == 0 || n < 8u || r32(s) != 0x10u) return 0;
    flags = r32(s + 4u); a->mode = flags & 7u; a->hasclut = (flags & 8u) != 0u;
    a->clut_off = 0u; a->clutw = a->cluth = a->palettes = a->palsize = 0u;
    a->clut_x = a->clut_y = 0u; p = 8u;
    if (a->hasclut) {
        if (p + 12u > n) return 0;
        blen = r32(s + p);
        if (blen < 12u || p + blen > n) return 0;
        a->clut_x = r16(s + p + 4u); a->clut_y = r16(s + p + 6u);
        a->clutw = r16(s + p + 8u); a->cluth = r16(s + p + 10u);
        a->clut_off = p + 12u;
        if ((size_t)a->clutw * a->cluth * 2u > (size_t)blen - 12u) return 0;
        p += blen;
    }
    if (p + 12u > n) return 0;
    blen = r32(s + p);
    if (blen < 12u || p + blen > n) return 0;
    a->image_x = r16(s + p + 4u); a->image_y = r16(s + p + 6u);
    a->ww = r16(s + p + 8u); a->h = r16(s + p + 10u);
    a->pixel_off = p + 12u; a->pixel_bytes = (size_t)blen - 12u;
    if (a->mode == 0u) { a->pixw = a->ww * 4u; a->palsize = 16u; }
    else if (a->mode == 1u) { a->pixw = a->ww * 2u; a->palsize = 256u; }
    else if (a->mode == 2u) a->pixw = a->ww;
    else if (a->mode == 3u) a->pixw = (a->ww * 2u) / 3u;
    else return 0; /* Mixed/type 4 needs game-specific interpretation. */
    if (a->pixw == 0u || a->h == 0u || (size_t)a->ww * 2u * a->h > a->pixel_bytes) return 0;
    if (a->mode <= 1u) {
        unsigned int colors;
        if (!a->hasclut || a->palsize == 0u) return 0;
        colors = a->clutw * a->cluth;
        if (colors < a->palsize) return 0;
        a->palettes = colors / a->palsize;
        if (a->palettes == 0u) return 0;
    } else a->palettes = 1u;
    return 1;
}

int tim89_probe(const unsigned char *s, size_t n)
{
    return s != 0 && n >= 8u && r32(s) == 0x10u;
}
int tim89_get_info(const unsigned char *s, size_t n, tim89_info *o)
{
    t89 a;
    if (o == 0 || !parse(s, n, &a)) return -1;
    o->width = a.pixw; o->height = a.h;
    o->bpp = a.mode == 0u ? 4u : (a.mode == 1u ? 8u : (a.mode == 2u ? 16u : 24u));
    o->palette_count = a.mode <= 1u ? a.palettes : 0u;
    o->subresource_count = a.mode <= 1u ? a.palettes : 1u;
    o->image_x = a.image_x; o->image_y = a.image_y; o->clut_x = a.clut_x; o->clut_y = a.clut_y;
    o->flags = 0u;
    if (a.mode <= 1u) o->flags |= TIM89_FLAG_INDEXED;
    if (a.hasclut) o->flags |= TIM89_FLAG_HAS_CLUT;
    o->flags |= TIM89_FLAG_STP;
    return 0;
}

int tim89_decode_subresource_rgba(const unsigned char *s, size_t n, unsigned int sub,
                                  unsigned char *d, size_t cap, unsigned int *ow,
                                  unsigned int *oh, size_t *used)
{
    t89 a;
    unsigned int x, y;
    if (d == 0 || ow == 0 || oh == 0 || used == 0 || !parse(s, n, &a)) return -1;
    if (sub >= (a.mode <= 1u ? a.palettes : 1u)) return -2;
    if ((size_t)a.pixw * a.h > cap / 4u) return -5;
    for (y = 0u; y < a.h; ++y) for (x = 0u; x < a.pixw; ++x) {
        unsigned char c[4];
        size_t q;
        if (a.mode == 0u) {
            size_t bi;
            unsigned int idx, ci;
            bi = a.pixel_off + (size_t)y * a.ww * 2u + (x >> 1);
            idx = (x & 1u) ? (s[bi] >> 4) : (s[bi] & 15u);
            ci = sub * a.palsize + idx;
            if (ci >= a.clutw * a.cluth) return -6;
            c15(r16(s + a.clut_off + (size_t)ci * 2u), c);
        } else if (a.mode == 1u) {
            size_t bi;
            unsigned int idx, ci;
            bi = a.pixel_off + (size_t)y * a.ww * 2u + x;
            idx = s[bi]; ci = sub * a.palsize + idx;
            if (ci >= a.clutw * a.cluth) return -6;
            c15(r16(s + a.clut_off + (size_t)ci * 2u), c);
        } else if (a.mode == 2u) {
            size_t bi;
            bi = a.pixel_off + ((size_t)y * a.ww + x) * 2u;
            if (bi + 2u > n) return -7;
            c15(r16(s + bi), c);
        } else {
            size_t bi, row;
            row = a.pixel_off + (size_t)y * a.ww * 2u;
            bi = row + (size_t)x * 3u;
            if (bi + 3u > row + (size_t)a.ww * 2u || bi + 3u > n) return -7;
            c[0] = s[bi]; c[1] = s[bi + 1u]; c[2] = s[bi + 2u]; c[3] = 255u;
        }
        q = ((size_t)y * a.pixw + x) * 4u;
        d[q] = c[0]; d[q + 1u] = c[1]; d[q + 2u] = c[2]; d[q + 3u] = c[3];
    }
    *ow = a.pixw; *oh = a.h; *used = (size_t)a.pixw * a.h * 4u;
    return 0;
}
int tim89_decode_rgba(const unsigned char *s, size_t n, unsigned char *d, size_t cap,
                      unsigned int *w, unsigned int *h, size_t *used)
{
    return tim89_decode_subresource_rgba(s, n, 0u, d, cap, w, h, used);
}
