#include "mf3d_eval_png.h"

static mf3d_u32 rd32be(const mf3d_u8 *p)
{
    return ((mf3d_u32)p[0] << 24) | ((mf3d_u32)p[1] << 16) | ((mf3d_u32)p[2] << 8) | (mf3d_u32)p[3];
}

static int type_eq(const mf3d_u8 *p, const char *s)
{
    return p[0] == (mf3d_u8)s[0] && p[1] == (mf3d_u8)s[1] && p[2] == (mf3d_u8)s[2] && p[3] == (mf3d_u8)s[3];
}

static mf3d_u8 paeth(mf3d_u8 a, mf3d_u8 b, mf3d_u8 c)
{
    int p, pa, pb, pc;
    p = (int)a + (int)b - (int)c;
    pa = p - (int)a; if (pa < 0) pa = -pa;
    pb = p - (int)b; if (pb < 0) pb = -pb;
    pc = p - (int)c; if (pc < 0) pc = -pc;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

static int unfilter(mf3d_eval_image_u8 *img, mf3d_u8 *raw, mf3d_u32 raw_len)
{
    mf3d_u32 rowbytes, y, x, inpos, outpos, bpp;
    mf3d_u8 filter, left, up, ul, val;
    if (!img || !raw || !img->pixels) return MF3D_E_BADARG;
    if (img->width == 0 || img->height == 0 || img->channels == 0) return MF3D_E_RANGE;
    bpp = img->channels;
    rowbytes = img->width * img->channels;
    if ((rowbytes + 1UL) * img->height > raw_len) return MF3D_E_RANGE;
    if (rowbytes * img->height > img->pixel_cap) return MF3D_E_NOSPACE;
    inpos = 0;
    outpos = 0;
    for (y = 0; y < img->height; ++y) {
        filter = raw[inpos++];
        for (x = 0; x < rowbytes; ++x) {
            val = raw[inpos++];
            left = (x >= bpp) ? img->pixels[outpos - bpp] : 0;
            up = (y > 0) ? img->pixels[outpos - rowbytes] : 0;
            ul = (y > 0 && x >= bpp) ? img->pixels[outpos - rowbytes - bpp] : 0;
            if (filter == 1) val = (mf3d_u8)(val + left);
            else if (filter == 2) val = (mf3d_u8)(val + up);
            else if (filter == 3) val = (mf3d_u8)(val + (mf3d_u8)(((int)left + (int)up) >> 1));
            else if (filter == 4) val = (mf3d_u8)(val + paeth(left, up, ul));
            else if (filter != 0) return MF3D_E_UNSUPPORTED;
            img->pixels[outpos++] = val;
        }
    }
    return MF3D_OK;
}

int mf3d_eval_png_decode(const mf3d_u8 *png, mf3d_u32 png_size, mf3d_eval_image_u8 *out_img, mf3d_eval_png_options *opt)
{
    static const mf3d_u8 sig[8] = {137,80,78,71,13,10,26,10};
    mf3d_u32 pos, len, idat_len, raw_len, zlib_len;
    mf3d_u8 color, bitdepth, interlace;
    int have_ihdr, done, r;
    if (!png || !out_img || !opt || !opt->inflate_cb || !opt->compressed_scratch || !opt->scanline_scratch) return MF3D_E_BADARG;
    if (png_size < 8) return MF3D_E_RANGE;
    for (pos = 0; pos < 8; ++pos) if (png[pos] != sig[pos]) return MF3D_E_UNSUPPORTED;
    pos = 8;
    idat_len = 0;
    have_ihdr = 0;
    done = 0;
    while (pos + 12 <= png_size && !done) {
        len = rd32be(png + pos); pos += 4;
        if (pos + 4 + len + 4 > png_size) return MF3D_E_RANGE;
        if (type_eq(png + pos, "IHDR")) {
            if (len != 13) return MF3D_E_UNSUPPORTED;
            out_img->width = rd32be(png + pos + 4);
            out_img->height = rd32be(png + pos + 8);
            bitdepth = png[pos + 12];
            color = png[pos + 13];
            interlace = png[pos + 16];
            if (bitdepth != 8 || interlace != 0) return MF3D_E_UNSUPPORTED;
            if (color == 0) out_img->channels = 1;
            else if (color == 2) out_img->channels = 3;
            else if (color == 6) out_img->channels = 4;
            else return MF3D_E_UNSUPPORTED;
            have_ihdr = 1;
        } else if (type_eq(png + pos, "IDAT")) {
            if (!have_ihdr) return MF3D_E_RANGE;
            if (idat_len + len > opt->compressed_scratch_size) return MF3D_E_NOSPACE;
            { mf3d_u32 i; for (i = 0; i < len; ++i) opt->compressed_scratch[idat_len + i] = png[pos + 4 + i]; }
            idat_len += len;
        } else if (type_eq(png + pos, "IEND")) {
            done = 1;
        }
        pos += 4 + len + 4; /* type + data + crc */
    }
    if (!have_ihdr || idat_len < 6) return MF3D_E_NOTFOUND;
    /* PNG IDAT is zlib-wrapped Deflate; strip 2-byte zlib header and 4-byte Adler32. */
    zlib_len = idat_len;
    if (zlib_len <= 6) return MF3D_E_RANGE;
    raw_len = 0;
    r = opt->inflate_cb(opt->inflate_user, opt->compressed_scratch + 2, zlib_len - 6, opt->scanline_scratch, opt->scanline_scratch_size, &raw_len);
    if (r != MF3D_OK) return r;
    return unfilter(out_img, opt->scanline_scratch, raw_len);
}

mf3d_fix mf3d_eval_image_sample_gray16(const mf3d_eval_image_u8 *img, mf3d_fix u, mf3d_fix v)
{
    mf3d_u32 x, y, idx;
    mf3d_u8 r, g, b;
    mf3d_u32 gray;
    if (!img || !img->pixels || img->width == 0 || img->height == 0) return 0;
    if (u < 0) u = 0;
    if (v < 0) v = 0;
    if (u > 65535L) u = 65535L;
    if (v > 65535L) v = 65535L;
    x = (mf3d_u32)(((unsigned long)u * (img->width - 1UL)) >> 16);
    y = (mf3d_u32)(((unsigned long)v * (img->height - 1UL)) >> 16);
    idx = (y * img->width + x) * img->channels;
    if (idx >= img->pixel_cap) return 0;
    if (img->channels == 1) gray = img->pixels[idx];
    else {
        r = img->pixels[idx]; g = img->pixels[idx + 1]; b = img->pixels[idx + 2];
        gray = ((mf3d_u32)r * 77UL + (mf3d_u32)g * 150UL + (mf3d_u32)b * 29UL) >> 8;
    }
    return (mf3d_fix)(gray * 257UL); /* 0..255 -> 0..65535 */
}
