/* Source VTF BC1/BC3 and selected uncompressed texture decoding.
   No float, malloc, 64-bit integer, endian assumptions or dependencies.
   The caller owns the input and output buffers. C89 (ANSI C). */
#include "hl2modelib.h"
#include <string.h>

static unsigned int rd16(const hl2m_u8 *d) {
    return (unsigned int)d[0] | ((unsigned int)d[1] << 8);
}
static unsigned int rd32(const hl2m_u8 *d) {
    return (unsigned int)d[0] | ((unsigned int)d[1] << 8) |
           ((unsigned int)d[2] << 16) | ((unsigned int)d[3] << 24);
}
static unsigned int vtf_block_bytes(unsigned int fmt) {
    if (fmt == HL2M_VTF_DXT1) return 8u;
    if (fmt == HL2M_VTF_DXT5) return 16u;
    return 0u;
}
static int vtf_mip_size(unsigned int w, unsigned int h,
                        unsigned int blk, unsigned int *out) {
    unsigned int bw, bh;
    if (w == 0u || h == 0u || blk == 0u || !out) return HL2M_ERR_RANGE;
    bw=(w+3u)/4u; bh=(h+3u)/4u;
    if (bw > 0xFFFFFFFFu / blk / bh) return HL2M_ERR_LIMIT;
    *out=bw*bh*blk;
    return HL2M_OK;
}
/* Formats found in the Resident Evil Source NPC corpora. */
static unsigned int vtf_pixel_bytes(unsigned int fmt) {
    if (fmt == HL2M_VTF_BGRA8888 || fmt == HL2M_VTF_ABGR8888) return 4u;
    if (fmt == HL2M_VTF_BGR888) return 3u;
    return 0u;
}
static int vtf_image_size(unsigned int w, unsigned int h,
                          unsigned int fmt, unsigned int *out) {
    unsigned int bp=vtf_pixel_bytes(fmt);
    if (bp) {
        if (!w || !h || !out || w > 0xffffffffu / bp / h) return HL2M_ERR_LIMIT;
        *out=w*h*bp;
        return HL2M_OK;
    }
    return vtf_mip_size(w,h,vtf_block_bytes(fmt),out);
}
int hl2m_vtf_read_info(const hl2m_u8 *d, unsigned int size, hl2m_vtf_info *out) {
    unsigned int major, minor, hdr, w, h, fmt, n, lo_fmt, low_w, low_h;
    unsigned int high, off, low_size, mip_size, i, frames, flags;
    if (!d || !out) return HL2M_ERR_NULL;
    memset(out,0,sizeof(*out));
    if (size < 64u) return HL2M_ERR_RANGE;
    if (memcmp(d,"VTF\0",4u)!=0) return HL2M_ERR_MAGIC;
    major=rd32(d+4);minor=rd32(d+8);hdr=rd32(d+12);
    if (major!=7u || minor>5u) return HL2M_ERR_VERSION;
    if (hdr < 64u || hdr > size) return HL2M_ERR_RANGE;
    w=rd16(d+16);h=rd16(d+18);flags=rd32(d+20);
    frames=rd16(d+24);fmt=rd32(d+52);n=d[56];lo_fmt=rd32(d+57);
    low_w=d[61];low_h=d[62];
    if (!w || !h || w>8192u || h>8192u || !n || n>16u || frames != 1u)
        return HL2M_ERR_UNSUPPORTED;
    /* Cubemaps and volume textures need a frame/face selector, not silently frame0. */
    if ((flags & 0x4000u)!=0u || (minor>=2u && rd16(d+63)!=1u))
        return HL2M_ERR_UNSUPPORTED;
    if (!vtf_block_bytes(fmt) && !vtf_pixel_bytes(fmt)) return HL2M_ERR_UNSUPPORTED;
    low_size=0u;
    if (low_w && low_h && (vtf_block_bytes(lo_fmt) || vtf_pixel_bytes(lo_fmt))) {
        if (vtf_image_size(low_w,low_h,lo_fmt,&low_size)!=HL2M_OK)
            return HL2M_ERR_LIMIT;
    } else if (low_w && low_h) {
        return HL2M_ERR_UNSUPPORTED;
    }
    if (hdr>size-low_size) return HL2M_ERR_RANGE;
    high=hdr+low_size;
    if (minor>=3u) {
        /* Resource dictionary contains 8-byte entries; 0x30 0x00 0x00 = full image. */
        for(i=80u; i+8u<=hdr; i+=8u) {
            if(d[i]==0x30u && d[i+1u]==0u && d[i+2u]==0u && !(d[i+3u]&2u)) {
                off=rd32(d+i+4u);
                if (off>0u) { high=off; break; }
            }
        }
    }
    if (high>size) return HL2M_ERR_RANGE;
    /* VTF mip levels stored from smallest to largest; largest image comes last. */
    for(i=n-1u;i>0u;--i) {
        unsigned int mw=w>>i,mh=h>>i;
        if(!mw) mw=1u;
        if(!mh) mh=1u;
        if(vtf_image_size(mw,mh,fmt,&mip_size)!=HL2M_OK)
            return HL2M_ERR_LIMIT;
        if(mip_size>size-high) return HL2M_ERR_RANGE;
        high+=mip_size;
    }
    if(vtf_image_size(w,h,fmt,&mip_size)!=HL2M_OK)
        return HL2M_ERR_LIMIT;
    if(mip_size>size-high) return HL2M_ERR_RANGE;
    out->width=w;out->height=h;out->mip_count=n;out->image_format=fmt;
    out->major_version=major;out->minor_version=minor;
    out->data_offset=high;out->data_size=mip_size;
    return HL2M_OK;
}
static void bc_color(hl2m_u8 dst[4], unsigned int c) {
    unsigned int r=(c>>11)&31u,g=(c>>5)&63u,b=c&31u;
    dst[0]=(hl2m_u8)((r<<3)|(r>>2));
    dst[1]=(hl2m_u8)((g<<2)|(g>>4));
    dst[2]=(hl2m_u8)((b<<3)|(b>>2));
    dst[3]=255;
}
static void bc_palette(const hl2m_u8 *src, hl2m_u8 colors[4][4], int bc3) {
    unsigned int a=rd16(src),b=rd16(src+2);
    unsigned int k;
    bc_color(colors[0],a);bc_color(colors[1],b);
    if(a>b || bc3) {
        for(k=0;k<3u;++k) {
            colors[2][k]=(hl2m_u8)((2u*colors[0][k]+colors[1][k])/3u);
            colors[3][k]=(hl2m_u8)((colors[0][k]+2u*colors[1][k])/3u);
        }
        colors[2][3]=255;colors[3][3]=255;
    } else {
        for(k=0;k<3u;++k) colors[2][k]=(hl2m_u8)((colors[0][k]+colors[1][k])/2u);
        colors[2][3]=255;
        for(k=0;k<4u;++k) colors[3][k]=0;
    }
}
int hl2m_vtf_decode_rgba(const hl2m_u8 *d, unsigned int size,
                         const hl2m_vtf_info *info, hl2m_u8 *rgba,
                         unsigned int capacity) {
    unsigned int w,h,block,xb,yb,bw,bh,x,y,src_off,dx,dy,outoff,bytes,k;
    const hl2m_u8 *src;
    hl2m_u8 colors[4][4],alpha[8];
    unsigned int bits,ai,ci,src_pixel_bytes;
    if (!d || !info || !rgba) return HL2M_ERR_NULL;
    w=info->width;h=info->height;bytes=vtf_block_bytes(info->image_format);
    src_pixel_bytes=vtf_pixel_bytes(info->image_format);
    if (!w || !h || (!bytes && !src_pixel_bytes) || w>8192u || h>8192u)
        return HL2M_ERR_UNSUPPORTED;
    if(w>0xffffffffu/4u/h || capacity<w*h*4u) return HL2M_ERR_LIMIT;
    if(info->data_offset>size || info->data_size>size-info->data_offset) return HL2M_ERR_RANGE;
    if(vtf_image_size(w,h,info->image_format,&block)!=HL2M_OK || info->data_size<block)
        return HL2M_ERR_RANGE;
    if (src_pixel_bytes) {
        unsigned int i, k, src_off, out_off;
        for (i=0u; i<w*h; ++i) {
            src_off=info->data_offset+i*src_pixel_bytes;
            out_off=i*4u;
            if (info->image_format==HL2M_VTF_ABGR8888) {
                for(k=0u;k<4u;++k) rgba[out_off+k]=d[src_off+3u-k];
            } else {
                rgba[out_off]=d[src_off+2u];
                rgba[out_off+1u]=d[src_off+1u];
                rgba[out_off+2u]=d[src_off];
                rgba[out_off+3u]=(src_pixel_bytes==4u)?d[src_off+3u]:255u;
            }
        }
        return HL2M_OK;
    }
    bw=(w+3u)/4u;bh=(h+3u)/4u;
    for(yb=0u;yb<bh;++yb) for(xb=0u;xb<bw;++xb) {
        src_off=info->data_offset + (yb*bw+xb)*bytes;
        src=d+src_off;
        bc_palette(src+(bytes==16u?8u:0u),colors,bytes==16u);
        if(bytes==16u) {
            alpha[0]=src[0];alpha[1]=src[1];
            if(alpha[0]>alpha[1]) {
                for(k=1u;k<7u;++k)
                    alpha[k+1u]=(hl2m_u8)(((7u-k)*alpha[0]+k*alpha[1])/7u);
            } else {
                for(k=1u;k<5u;++k)
                    alpha[k+1u]=(hl2m_u8)(((5u-k)*alpha[0]+k*alpha[1])/5u);
                alpha[6]=0;alpha[7]=255;
            }
        }
        bits=rd32(src+(bytes==16u?12u:4u));
        for(y=0u;y<4u;++y)for(x=0u;x<4u;++x) {
            dx=xb*4u+x;dy=yb*4u+y;
            if(dx>=w || dy>=h) continue;
            outoff=(dy*w+dx)*4u;
            ci=(bits>>(2u*(y*4u+x)))&3u;
            for(k=0u;k<4u;++k)rgba[outoff+k]=colors[ci][k];
            if(bytes==16u) {
                /* BC3 alpha selectors: six bytes, 16 packed 3-bit values. */
                unsigned int bitpos=3u*(y*4u+x), bytepos=bitpos>>3;
                ai=((unsigned int)src[2u+bytepos]>> (bitpos & 7u));
                if((bitpos&7u)>5u) ai|=((unsigned int)src[3u+bytepos]<<(8u-(bitpos&7u)));
                rgba[outoff+3u]=alpha[ai&7u];
            }
        }
    }
    return HL2M_OK;
}
