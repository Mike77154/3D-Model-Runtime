#include "c4d89_img_internal.h"
int c4d89_img_bmp(const unsigned char *s,unsigned long n,unsigned char *out,
                  unsigned long cap,C4D89_ImgInfo *info)
{
    unsigned long off,dib,w,h,rowbytes,y,x,p,table,colors,top,bytespp;
    unsigned int bpp,compression,r,g,b,a;
    unsigned long masks[4], raw_h;
    if(n<54ul||s[0]!='B'||s[1]!='M')return C4D89_IMG_INVALID;
    off=i32le(s+10);dib=i32le(s+14);
    if(dib<40ul||dib>n-14ul)return C4D89_IMG_UNSUPPORTED;
    w=i32le(s+18);raw_h=i32le(s+22);
    if(w>0x7ffffffful||raw_h==0x80000000ul)return C4D89_IMG_LIMIT;
    top=(raw_h&0x80000000ul)?1ul:0ul;
    h=top?((~raw_h+1ul)&0xfffffffful):raw_h;
    bpp=i16le(s+28);compression=(unsigned int)i32le(s+30);
    if(i16le(s+26)!=1u||!size_ok(w,h,cap))return C4D89_IMG_LIMIT;
    if(bpp!=8u && bpp!=16u && bpp!=24u && bpp!=32u)return C4D89_IMG_UNSUPPORTED;
    if(compression!=0u && compression!=3u)return C4D89_IMG_UNSUPPORTED;
    if(compression==3u && bpp!=16u && bpp!=32u)return C4D89_IMG_UNSUPPORTED;
    if(w>(0xfffffffful-31ul)/(unsigned long)bpp)return C4D89_IMG_LIMIT;
    rowbytes=((w*(unsigned long)bpp+31ul)/32ul)*4ul;
    if(off>n || (rowbytes>0ul && h>(n-off)/rowbytes))return C4D89_IMG_TRUNCATED;
    bytespp=(unsigned long)((bpp+7u)/8u);
    table=14ul+dib;
    colors=i32le(s+46);if(!colors)colors=256ul;
    if(bpp==8u && (colors>256ul||table>off||colors>(off-table)/4ul))return C4D89_IMG_CORRUPT;
    masks[0]=0x00ff0000ul;masks[1]=0x0000ff00ul;masks[2]=0x000000fful;masks[3]=0xff000000ul;
    if(bpp==16u){masks[0]=0x7c00ul;masks[1]=0x03e0ul;masks[2]=0x001ful;masks[3]=0ul;}
    if(compression==3u) {
        if(dib>=52ul) {masks[0]=i32le(s+54);masks[1]=i32le(s+58);masks[2]=i32le(s+62);
            if(dib>=56ul)masks[3]=i32le(s+66);else masks[3]=0ul;
        } else if(table+12ul<=off) {
            masks[0]=i32le(s+table);masks[1]=i32le(s+table+4);masks[2]=i32le(s+table+8);masks[3]=0ul;
        } else return C4D89_IMG_CORRUPT;
    }
    for(y=0ul;y<h;y++)for(x=0ul;x<w;x++){
        unsigned long v,pp,srcrow;
        unsigned int k;unsigned long sh,mask;
        srcrow=top?y:(h-1ul-y);p=off+srcrow*rowbytes+x*bytespp;
        r=g=b=0u;a=255u;
        if(bpp==8u){ v=(unsigned long)s[p];if(v>=colors)return C4D89_IMG_CORRUPT;
            pp=table+v*4ul;b=s[pp];g=s[pp+1];r=s[pp+2];
        } else if(compression==3u || bpp==16u){
            v=(unsigned long)i16le(s+p);if(bpp==32u)v=i32le(s+p);
            for(k=0u;k<4u;k++){
                mask=masks[k];if(mask){
                    sh=0ul;while((mask&1ul)==0ul){mask>>=1;sh++;}
                    pp=(v>>sh)&mask;
                    if(mask>0ul && mask<=65535ul)pp=(pp*255ul+mask/2ul)/mask;
                    if(k==0u)r=(unsigned int)pp;
                    else if(k==1u)g=(unsigned int)pp;
                    else if(k==2u)b=(unsigned int)pp;
                    else a=(unsigned int)pp;
                }
            }
        } else {b=s[p];g=s[p+1];r=s[p+2];if(bpp==32u)a=255u;}
        pp=(y*w+x)*4ul;out[pp]=(unsigned char)r;out[pp+1]=(unsigned char)g;
        out[pp+2]=(unsigned char)b;out[pp+3]=(unsigned char)a;
    }
    info->width=(unsigned int)w;info->height=(unsigned int)h;info->format=C4D89_IMG_FORMAT_BMP;
    info->source_channels=bpp==8u?1u:(bpp==32u?4u:3u);return C4D89_IMG_OK;
}
