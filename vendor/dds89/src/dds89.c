#include "dds89.h"

#define D89_KIND_BC1 1u
#define D89_KIND_BC2 2u
#define D89_KIND_BC3 3u
#define D89_KIND_RGB 4u
#define D89_KIND_RGBA8 5u
#define D89_KIND_BGRA8 6u
#define D89_KIND_BGRX8 7u

#define D89_CUBEMAP 0x00000200u
#define D89_PX      0x00000400u
#define D89_NX      0x00000800u
#define D89_PY      0x00001000u
#define D89_NY      0x00002000u
#define D89_PZ      0x00004000u
#define D89_NZ      0x00008000u

static unsigned int r16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}
static unsigned int r32(const unsigned char *p)
{
    return r16(p) | (r16(p + 2u) << 16);
}
static unsigned int fourcc(char a, char b, char c, char d)
{
    return (unsigned int)(unsigned char)a | ((unsigned int)(unsigned char)b << 8) |
           ((unsigned int)(unsigned char)c << 16) | ((unsigned int)(unsigned char)d << 24);
}
static unsigned int pop6(unsigned int m)
{
    unsigned int i, c;
    c = 0u;
    for (i = 0u; i < 6u; ++i) if (m & (1u << i)) ++c;
    return c;
}
static unsigned char ex(unsigned int v, unsigned int m)
{
    unsigned int s, maxv, x;
    s = 0u;
    if (m == 0u) return 255u;
    while ((m & 1u) == 0u) { m >>= 1; ++s; }
    maxv = m;
    x = (v >> s) & maxv;
    return maxv ? (unsigned char)((x * 255u + maxv / 2u) / maxv) : 0u;
}
static void c565(unsigned int v, unsigned char *c)
{
    c[0] = (unsigned char)(((v >> 11) & 31u) * 255u / 31u);
    c[1] = (unsigned char)(((v >> 5) & 63u) * 255u / 63u);
    c[2] = (unsigned char)((v & 31u) * 255u / 31u);
    c[3] = 255u;
}
static void bc_colors(const unsigned char *b, unsigned char c[4][4], int alpha_mode)
{
    unsigned int c0, c1, k;
    c0 = r16(b); c1 = r16(b + 2u);
    c565(c0, c[0]); c565(c1, c[1]);
    if (c0 > c1 || alpha_mode) {
        for (k = 0u; k < 3u; ++k) {
            c[2][k] = (unsigned char)((2u * c[0][k] + c[1][k]) / 3u);
            c[3][k] = (unsigned char)((c[0][k] + 2u * c[1][k]) / 3u);
        }
        c[2][3] = c[3][3] = 255u;
    } else {
        for (k = 0u; k < 3u; ++k) c[2][k] = (unsigned char)((c[0][k] + c[1][k]) / 2u);
        c[2][3] = 255u;
        c[3][0] = c[3][1] = c[3][2] = 0u; c[3][3] = 0u;
    }
}
static void write4(unsigned char *d, unsigned int w, unsigned int h,
                   unsigned int bx, unsigned int by, unsigned char c[4][4],
                   unsigned int bits, unsigned char *a)
{
    unsigned int x, y;
    for (y = 0u; y < 4u; ++y) for (x = 0u; x < 4u; ++x) {
        unsigned int px, py, idx;
        px = bx * 4u + x; py = by * 4u + y;
        idx = (bits >> (2u * (y * 4u + x))) & 3u;
        if (px < w && py < h) {
            size_t q;
            q = ((size_t)py * w + px) * 4u;
            d[q] = c[idx][0]; d[q + 1u] = c[idx][1]; d[q + 2u] = c[idx][2];
            d[q + 3u] = a ? a[y * 4u + x] : c[idx][3];
        }
    }
}
static void alpha_dxt5(const unsigned char *b, unsigned char *a)
{
    unsigned char vals[8];
    unsigned int i, bitpos, lo, hi;
    bitpos = 0u; lo = r32(b + 2u); hi = r16(b + 6u);
    vals[0] = b[0]; vals[1] = b[1];
    if (vals[0] > vals[1]) {
        for (i = 1u; i <= 6u; ++i)
            vals[i + 1u] = (unsigned char)(((7u - i) * vals[0] + i * vals[1]) / 7u);
    } else {
        for (i = 1u; i <= 4u; ++i)
            vals[i + 1u] = (unsigned char)(((5u - i) * vals[0] + i * vals[1]) / 5u);
        vals[6] = 0u; vals[7] = 255u;
    }
    for (i = 0u; i < 16u; ++i) {
        unsigned int idx;
        if (bitpos < 30u) idx = (lo >> bitpos) & 7u;
        else if (bitpos == 30u) idx = ((lo >> 30) & 3u) | ((hi & 1u) << 2);
        else idx = (hi >> (bitpos - 32u)) & 7u;
        a[i] = vals[idx]; bitpos += 3u;
    }
}

typedef struct d89_desc_s {
    unsigned int w, h, mips, surfaces, face_mask, kind, bpp;
    unsigned int rm, gm, bm, am;
    unsigned int flags;
    size_t data_off;
} d89_desc;

static int parse(const unsigned char *s, size_t n, d89_desc *o)
{
    unsigned int pff, cc, caps2;
    if (s == 0 || o == 0 || n < 128u || s[0] != 'D' || s[1] != 'D' || s[2] != 'S' || s[3] != ' ') return 0;
    if (r32(s + 4u) != 124u || r32(s + 76u) != 32u) return 0;
    o->h = r32(s + 12u); o->w = r32(s + 16u);
    o->mips = r32(s + 28u); if (o->mips == 0u) o->mips = 1u;
    if (o->w == 0u || o->h == 0u || o->mips > 32u) return 0;
    pff = r32(s + 80u); cc = r32(s + 84u);
    o->bpp = r32(s + 88u); o->rm = r32(s + 92u); o->gm = r32(s + 96u);
    o->bm = r32(s + 100u); o->am = r32(s + 104u);
    o->kind = 0u; o->flags = 0u; o->data_off = 128u; o->surfaces = 1u; o->face_mask = 1u;
    caps2 = r32(s + 112u);
    if ((pff & 4u) != 0u) {
        if (cc == fourcc('D','X','T','1')) o->kind = D89_KIND_BC1;
        else if (cc == fourcc('D','X','T','2') || cc == fourcc('D','X','T','3')) o->kind = D89_KIND_BC2;
        else if (cc == fourcc('D','X','T','4') || cc == fourcc('D','X','T','5')) o->kind = D89_KIND_BC3;
        else if (cc == fourcc('D','X','1','0')) {
            unsigned int fmt, misc, arr;
            if (n < 148u) return 0;
            fmt = r32(s + 128u); misc = r32(s + 136u); arr = r32(s + 140u);
            if (arr == 0u) return 0;
            o->data_off = 148u; o->flags |= DDS89_FLAG_DX10;
            if (fmt == 71u || fmt == 72u) o->kind = D89_KIND_BC1;
            else if (fmt == 74u || fmt == 75u) o->kind = D89_KIND_BC2;
            else if (fmt == 77u || fmt == 78u) o->kind = D89_KIND_BC3;
            else if (fmt == 28u || fmt == 29u) { o->kind = D89_KIND_RGBA8; o->bpp = 32u; }
            else if (fmt == 87u || fmt == 91u) { o->kind = D89_KIND_BGRA8; o->bpp = 32u; }
            else if (fmt == 88u || fmt == 93u) { o->kind = D89_KIND_BGRX8; o->bpp = 32u; }
            else return 0;
            if ((misc & 4u) != 0u) {
                if (arr > 0xffffffffu / 6u) return 0;
                o->surfaces = arr * 6u;
                o->face_mask = 0x3fu;
                o->flags |= DDS89_FLAG_CUBEMAP;
            } else {
                o->surfaces = arr;
                o->face_mask = 1u;
            }
        } else return 0;
    } else if ((pff & 0x40u) != 0u && (o->bpp == 16u || o->bpp == 24u || o->bpp == 32u)) {
        o->kind = D89_KIND_RGB;
    } else return 0;
    if ((o->flags & DDS89_FLAG_DX10) == 0u && (caps2 & D89_CUBEMAP) != 0u) {
        unsigned int fm;
        fm = 0u;
        if (caps2 & D89_PX) fm |= 1u << 0;
        if (caps2 & D89_NX) fm |= 1u << 1;
        if (caps2 & D89_PY) fm |= 1u << 2;
        if (caps2 & D89_NY) fm |= 1u << 3;
        if (caps2 & D89_PZ) fm |= 1u << 4;
        if (caps2 & D89_NZ) fm |= 1u << 5;
        if (fm == 0u) fm = 0x3fu;
        o->face_mask = fm; o->surfaces = pop6(fm); o->flags |= DDS89_FLAG_CUBEMAP;
    }
    if (o->surfaces == 0u || o->surfaces > 4096u || o->mips > 0xffffffffu / o->surfaces) return 0;
    return 1;
}

static size_t level_bytes(const d89_desc *a, unsigned int w, unsigned int h)
{
    if (a->kind == D89_KIND_BC1) return (size_t)((w + 3u) / 4u) * ((h + 3u) / 4u) * 8u;
    if (a->kind == D89_KIND_BC2 || a->kind == D89_KIND_BC3)
        return (size_t)((w + 3u) / 4u) * ((h + 3u) / 4u) * 16u;
    return ((size_t)w * a->bpp + 7u) / 8u * h;
}
static unsigned int face_id_for_surface(const d89_desc *a, unsigned int surface)
{
    unsigned int f, seen;
    if ((a->flags & DDS89_FLAG_CUBEMAP) == 0u || (a->flags & DDS89_FLAG_DX10) != 0u) return surface % 6u;
    seen = 0u;
    for (f = 0u; f < 6u; ++f) if (a->face_mask & (1u << f)) {
        if (seen == surface) return f;
        ++seen;
    }
    return 0u;
}
static int subinfo(const d89_desc *a, size_t n, unsigned int sub, dds89_subresource_info *o)
{
    unsigned int surface, mip, sidx, mi, w, h;
    size_t off, b;
    if (sub >= a->surfaces * a->mips) return 0;
    surface = sub / a->mips; mip = sub % a->mips;
    off = a->data_off;
    for (sidx = 0u; sidx < surface; ++sidx) {
        w = a->w; h = a->h;
        for (mi = 0u; mi < a->mips; ++mi) {
            b = level_bytes(a, w, h);
            if (off > n || b > n - off) return 0;
            off += b; if (w > 1u) w >>= 1; if (h > 1u) h >>= 1;
        }
    }
    w = a->w; h = a->h;
    for (mi = 0u; mi < mip; ++mi) {
        b = level_bytes(a, w, h);
        if (off > n || b > n - off) return 0;
        off += b; if (w > 1u) w >>= 1; if (h > 1u) h >>= 1;
    }
    b = level_bytes(a, w, h);
    if (off > n || b > n - off) return 0;
    o->index = sub; o->face_index = face_id_for_surface(a, surface); o->mip_level = mip;
    o->width = w; o->height = h; o->encoded_offset = off; o->encoded_bytes = b;
    return 1;
}

int dds89_probe(const unsigned char *s, size_t n)
{
    return s != 0 && n >= 128u && s[0] == 'D' && s[1] == 'D' && s[2] == 'S' && s[3] == ' ';
}
int dds89_get_info(const unsigned char *s, size_t n, dds89_info *o)
{
    d89_desc a;
    if (o == 0 || !parse(s, n, &a)) return -1;
    o->width = a.w; o->height = a.h; o->mip_count = a.mips; o->face_count = a.surfaces;
    o->subresource_count = a.surfaces * a.mips; o->face_mask = a.face_mask;
    o->format = a.kind; o->flags = a.flags;
    return 0;
}
int dds89_get_subresource_info(const unsigned char *s, size_t n, unsigned int sub,
                               dds89_subresource_info *o)
{
    d89_desc a;
    if (o == 0 || !parse(s, n, &a)) return -1;
    return subinfo(&a, n, sub, o) ? 0 : -2;
}

int dds89_decode_subresource_rgba(const unsigned char *s, size_t n, unsigned int sub,
                                  unsigned char *d, size_t cap, unsigned int *ow,
                                  unsigned int *oh, size_t *used)
{
    d89_desc a;
    dds89_subresource_info si;
    unsigned int w, h, bx, by;
    size_t p;
    if (d == 0 || ow == 0 || oh == 0 || used == 0 || !parse(s, n, &a)) return -1;
    if (!subinfo(&a, n, sub, &si)) return -2;
    w = si.width; h = si.height; p = si.encoded_offset;
    if ((size_t)w * h > cap / 4u) return -3;
    if (a.kind == D89_KIND_BC1 || a.kind == D89_KIND_BC2 || a.kind == D89_KIND_BC3) {
        unsigned int block;
        block = a.kind == D89_KIND_BC1 ? 8u : 16u;
        for (by = 0u; by < (h + 3u) / 4u; ++by) for (bx = 0u; bx < (w + 3u) / 4u; ++bx) {
            unsigned char c[4][4], al[16];
            unsigned int bitsv;
            const unsigned char *b;
            b = s + p;
            if (p + block > n) return -4;
            if (a.kind == D89_KIND_BC1) {
                bc_colors(b, c, 0); bitsv = r32(b + 4u); write4(d, w, h, bx, by, c, bitsv, 0);
            } else if (a.kind == D89_KIND_BC2) {
                unsigned int i;
                for (i = 0u; i < 16u; ++i) {
                    unsigned int nib;
                    nib = i < 8u ? ((r32(b) >> (i * 4u)) & 15u) : ((r32(b + 4u) >> ((i - 8u) * 4u)) & 15u);
                    al[i] = (unsigned char)(nib * 17u);
                }
                bc_colors(b + 8u, c, 1); bitsv = r32(b + 12u); write4(d, w, h, bx, by, c, bitsv, al);
            } else {
                alpha_dxt5(b, al); bc_colors(b + 8u, c, 1); bitsv = r32(b + 12u);
                write4(d, w, h, bx, by, c, bitsv, al);
            }
            p += block;
        }
    } else if (a.kind == D89_KIND_RGB) {
        unsigned int bytes, x, y;
        bytes = a.bpp / 8u;
        for (y = 0u; y < h; ++y) for (x = 0u; x < w; ++x) {
            unsigned int v, k;
            size_t q;
            v = 0u;
            for (k = 0u; k < bytes; ++k) v |= (unsigned int)s[p + ((size_t)y * w + x) * bytes + k] << (8u * k);
            q = ((size_t)y * w + x) * 4u;
            d[q] = ex(v, a.rm); d[q + 1u] = ex(v, a.gm); d[q + 2u] = ex(v, a.bm); d[q + 3u] = a.am ? ex(v, a.am) : 255u;
        }
    } else {
        unsigned int x, y;
        for (y = 0u; y < h; ++y) for (x = 0u; x < w; ++x) {
            const unsigned char *q;
            size_t di;
            q = s + p + ((size_t)y * w + x) * 4u; di = ((size_t)y * w + x) * 4u;
            if (a.kind == D89_KIND_RGBA8) { d[di]=q[0]; d[di+1u]=q[1]; d[di+2u]=q[2]; d[di+3u]=q[3]; }
            else if (a.kind == D89_KIND_BGRA8) { d[di]=q[2]; d[di+1u]=q[1]; d[di+2u]=q[0]; d[di+3u]=q[3]; }
            else { d[di]=q[2]; d[di+1u]=q[1]; d[di+2u]=q[0]; d[di+3u]=255u; }
        }
    }
    *ow = w; *oh = h; *used = (size_t)w * h * 4u;
    return 0;
}
int dds89_decode_rgba(const unsigned char *s, size_t n, unsigned char *d, size_t cap,
                      unsigned int *w, unsigned int *h, size_t *used)
{
    return dds89_decode_subresource_rgba(s, n, 0u, d, cap, w, h, used);
}
