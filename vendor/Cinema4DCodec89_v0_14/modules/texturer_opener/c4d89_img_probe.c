#include "c4d89_img_internal.h"
/* C89 32-bit safe image preflight: dimensions and output memory only.
   This does not replace the full PNG/JPEG checksum and entropy validation. */
static unsigned int be16(const unsigned char *p)
{ return ((unsigned int)p[0]<<8)|(unsigned int)p[1]; }
static int complete(unsigned long w,unsigned long h,unsigned int fmt,
                    unsigned int ch,C4D89_ImgInfo *info,unsigned long *needed)
{
    unsigned long bytes;
    if(!w||!h||w>C4D89_IMG_MAX_DIM||h>C4D89_IMG_MAX_DIM ||
       w>0xfffffffful/(4ul*h))return C4D89_IMG_LIMIT;
    bytes=w*h*4ul;
    info->width=(unsigned int)w;info->height=(unsigned int)h;
    info->format=fmt;info->source_channels=ch;*needed=bytes;
    return C4D89_IMG_OK;
}
static int probe_png(const unsigned char *s,unsigned long n,
                     C4D89_ImgInfo *info,unsigned long *needed)
{
    unsigned int ch=0u;
    unsigned int color;
    if(n<33ul)return C4D89_IMG_TRUNCATED;
    if(memcmp(s+12,"IHDR",4u)!=0||i32be(s+8)!=13ul)return C4D89_IMG_CORRUPT;
    if(s[24]!=8u||s[26]!=0u||s[27]!=0u||s[28]!=0u)return C4D89_IMG_UNSUPPORTED;
    color=s[25];
    if(color==0u||color==3u)ch=1u;
    else if(color==2u)ch=3u;
    else if(color==4u)ch=2u;
    else if(color==6u)ch=4u;
    else return C4D89_IMG_UNSUPPORTED;
    return complete(i32be(s+16),i32be(s+20),C4D89_IMG_FORMAT_PNG,ch,info,needed);
}
static int probe_bmp(const unsigned char *s,unsigned long n,
                     C4D89_ImgInfo *info,unsigned long *needed)
{
    unsigned long w,rh,h;
    unsigned int bpp;
    if(n<54ul)return C4D89_IMG_TRUNCATED;
    if(i32le(s+14)<40ul||i32le(s+14)>n-14ul)return C4D89_IMG_UNSUPPORTED;
    w=i32le(s+18);rh=i32le(s+22);
    if(w>0x7ffffffful||rh==0x80000000ul)return C4D89_IMG_LIMIT;
    h=(rh&0x80000000ul)?((~rh+1ul)&0xfffffffful):rh;
    if(i16le(s+26)!=1u)return C4D89_IMG_CORRUPT;
    bpp=i16le(s+28);
    if(bpp!=8u&&bpp!=16u&&bpp!=24u&&bpp!=32u)return C4D89_IMG_UNSUPPORTED;
    if(i32le(s+30)!=0ul&&i32le(s+30)!=3ul)return C4D89_IMG_UNSUPPORTED;
    return complete(w,h,C4D89_IMG_FORMAT_BMP,bpp==8u?1u:(bpp==32u?4u:3u),info,needed);
}
static int probe_tga(const unsigned char *s,unsigned long n,
                     C4D89_ImgInfo *info,unsigned long *needed)
{
    unsigned int t,d;
    if(n<18ul)return C4D89_IMG_TRUNCATED;
    t=s[2];d=s[16];
    if(t!=1u&&t!=2u&&t!=3u&&t!=9u&&t!=10u&&t!=11u)return C4D89_IMG_UNSUPPORTED;
    if(s[17]&0xc0u)return C4D89_IMG_CORRUPT;
    if((t==1u||t==9u)&&(s[1]!=1u||(d!=8u&&d!=16u)))return C4D89_IMG_UNSUPPORTED;
    if((t==2u||t==10u)&&(s[1]!=0u||(d!=16u&&d!=24u&&d!=32u)))return C4D89_IMG_UNSUPPORTED;
    if((t==3u||t==11u)&&(s[1]!=0u||(d!=8u&&d!=16u)))return C4D89_IMG_UNSUPPORTED;
    return complete(i16le(s+12),i16le(s+14),C4D89_IMG_FORMAT_TGA,
                    (t==3u||t==11u)?1u:(d==32u?4u:3u),info,needed);
}
static int probe_jpeg(const unsigned char *s,unsigned long n,
                      C4D89_ImgInfo *info,unsigned long *needed)
{
    unsigned long p=2ul;
    if(n<4ul)return C4D89_IMG_TRUNCATED;
    while(p<n){
        unsigned int marker,sz;
        while(p<n && s[p]!=0xffu)p++;
        if(p==n)return C4D89_IMG_TRUNCATED;
        while(p<n && s[p]==0xffu)p++;
        if(p==n)return C4D89_IMG_TRUNCATED;
        marker=s[p++];
        if(marker==0xd9u||marker==0xdau)return C4D89_IMG_CORRUPT;
        if(marker==0x01u||(marker>=0xd0u&&marker<=0xd7u))continue;
        if(p+2ul>n)return C4D89_IMG_TRUNCATED;
        sz=be16(s+p);
        if(sz<2u||(unsigned long)sz>n-p)return C4D89_IMG_TRUNCATED;
        if(marker==0xc0u){
            unsigned long w,h;unsigned int ch;
            if(sz<8u)return C4D89_IMG_CORRUPT;
            if(s[p+2ul]!=8u)return C4D89_IMG_UNSUPPORTED;
            h=be16(s+p+3ul);w=be16(s+p+5ul);ch=s[p+7ul];
            if(ch!=1u && ch!=3u)return C4D89_IMG_UNSUPPORTED;
            return complete(w,h,C4D89_IMG_FORMAT_JPEG,ch,info,needed);
        }
        if(marker>=0xc1u&&marker<=0xcfu&&marker!=0xc4u)return C4D89_IMG_UNSUPPORTED;
        p+=(unsigned long)sz;
    }
    return C4D89_IMG_TRUNCATED;
}
int c4d89_img_probe_info(const unsigned char *s,unsigned long n,
                         C4D89_ImgInfo *info,unsigned long *needed)
{
    if(!s||!info||!needed)return C4D89_IMG_INVALID;
    memset(info,0,sizeof(*info));*needed=0ul;
    if(n>=8ul&&memcmp(s,"\211PNG\r\n\032\n",8u)==0)
        return probe_png(s,n,info,needed);
    if(n>=2ul&&s[0]==0xffu&&s[1]==0xd8u)
        return probe_jpeg(s,n,info,needed);
    if(n>=2ul&&s[0]=='B'&&s[1]=='M')
        return probe_bmp(s,n,info,needed);
    return probe_tga(s,n,info,needed);
}
