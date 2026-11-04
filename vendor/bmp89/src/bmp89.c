#include "bmp89.h"

static unsigned int rd16(const unsigned char *p){return (unsigned int)p[0]|((unsigned int)p[1]<<8);}
static unsigned int rd32(const unsigned char *p){return rd16(p)|(rd16(p+2)<<16);}
static int mask_shift(unsigned int m){int s=0;if(m==0u)return 0;while((m&1u)==0u){m>>=1;s++;}return s;}
static unsigned int mask_max(unsigned int m){int s=mask_shift(m);return m>>s;}
static unsigned char mask8(unsigned int v,unsigned int m){unsigned int x,maxv;int s;if(m==0u)return 255u;s=mask_shift(m);maxv=mask_max(m);x=(v&m)>>s;if(maxv==0u)return 0u;return (unsigned char)((x*255u+maxv/2u)/maxv);}
int bmp89_probe(const unsigned char *src,size_t size){return src!=0&&size>=2u&&src[0]=='B'&&src[1]=='M';}
int bmp89_decode_rgba(const unsigned char *src,size_t size,unsigned char *dst,size_t cap,unsigned int *ow,unsigned int *oh,size_t *used){
    unsigned int off,dib,w,h,bpp,comp,paln,row,y,x,rm,gm,bm,am;int top;
    if(!bmp89_probe(src,size)||dst==0||ow==0||oh==0||used==0||size<54u)return -1;
    off=rd32(src+10);dib=rd32(src+14);if(dib<40u||14u+(size_t)dib>size)return -2;
    w=rd32(src+18);h=rd32(src+22);top=0;if((h&0x80000000u)!=0u){h=(~h)+1u;top=1;}
    if(w==0u||h==0u||w>16384u||h>16384u)return -3;
    bpp=rd16(src+28);comp=rd32(src+30);if((size_t)w*(size_t)h>cap/4u)return -4;
    rm=gm=bm=am=0u;
    if(comp==3u){size_t mp=14u+(size_t)dib;if(dib>=52u){rm=rd32(src+54);gm=rd32(src+58);bm=rd32(src+62);if(dib>=56u)am=rd32(src+66);}else if(mp+12u<=size){rm=rd32(src+mp);gm=rd32(src+mp+4);bm=rd32(src+mp+8);}}
    if(comp!=0u&&comp!=3u)return -5;
    paln=0u;if(bpp<=8u){paln=rd32(src+46);if(paln==0u)paln=1u<<bpp;}
    row=((w*bpp+31u)/32u)*4u;
    if((size_t)off+(size_t)row*(size_t)h>size)return -6;
    for(y=0u;y<h;y++){
        const unsigned char *r=src+off+(size_t)(top?y:(h-1u-y))*row;
        for(x=0u;x<w;x++){
            unsigned char R,G,B,A;size_t di=((size_t)y*w+x)*4u;R=G=B=0u;A=255u;
            if(bpp==24u){const unsigned char*p=r+x*3u;B=p[0];G=p[1];R=p[2];}
            else if(bpp==32u){unsigned int v=rd32(r+x*4u);if(comp==3u){R=mask8(v,rm);G=mask8(v,gm);B=mask8(v,bm);A=am?mask8(v,am):(unsigned char)(v>>24);}else{const unsigned char*p=r+x*4u;B=p[0];G=p[1];R=p[2];A=p[3];if(A==0u)A=255u;}}
            else if(bpp==16u){unsigned int v=rd16(r+x*2u);if(comp==3u){R=mask8(v,rm);G=mask8(v,gm);B=mask8(v,bm);A=am?mask8(v,am):255u;}else{R=(unsigned char)(((v>>10)&31u)*255u/31u);G=(unsigned char)(((v>>5)&31u)*255u/31u);B=(unsigned char)((v&31u)*255u/31u);}}
            else if(bpp==8u||bpp==4u||bpp==1u){unsigned int idx;size_t po=14u+(size_t)dib;if(bpp==8u)idx=r[x];else if(bpp==4u){unsigned char q=r[x>>1];idx=(x&1u)?(q&15u):(q>>4);}else{idx=(r[x>>3]>>(7u-(x&7u)))&1u;}if(idx>=paln||po+(size_t)idx*4u+4u>size)return -7;B=src[po+idx*4u];G=src[po+idx*4u+1u];R=src[po+idx*4u+2u];}
            else return -8;
            dst[di]=R;dst[di+1]=G;dst[di+2]=B;dst[di+3]=A;
        }
    }
    *ow=w;*oh=h;*used=(size_t)w*h*4u;return 0;
}
