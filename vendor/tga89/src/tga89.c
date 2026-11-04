#include "tga89.h"
static unsigned int r16(const unsigned char*p){return (unsigned int)p[0]|((unsigned int)p[1]<<8);}
int tga89_probe(const unsigned char*s,size_t n){unsigned int t;if(s==0||n<18u)return 0;t=s[2];return t==1u||t==2u||t==3u||t==9u||t==10u||t==11u;}
static void px(const unsigned char*p,unsigned int bpp,unsigned char*out){unsigned int v;if(bpp==32u){out[2]=p[0];out[1]=p[1];out[0]=p[2];out[3]=p[3];}else if(bpp==24u){out[2]=p[0];out[1]=p[1];out[0]=p[2];out[3]=255u;}else if(bpp==16u||bpp==15u){v=r16(p);out[0]=(unsigned char)(((v>>10)&31u)*255u/31u);out[1]=(unsigned char)(((v>>5)&31u)*255u/31u);out[2]=(unsigned char)((v&31u)*255u/31u);out[3]=(bpp==16u&&(v&0x8000u)==0u)?0u:255u;}else{out[0]=out[1]=out[2]=p[0];out[3]=255u;}}
int tga89_decode_rgba(const unsigned char*s,size_t n,unsigned char*d,size_t cap,unsigned int*ow,unsigned int*oh,size_t*used){
 unsigned int id,cm,t,cmfirst,cmlen,cmbits,w,h,bpp,desc,bytes,cmbytes;size_t p;unsigned int outi,total;int rle,top,right;
 if(!tga89_probe(s,n)||d==0||ow==0||oh==0||used==0)return -1;
 id=s[0];cm=s[1];t=s[2];cmfirst=r16(s+3);cmlen=r16(s+5);cmbits=s[7];w=r16(s+12);h=r16(s+14);bpp=s[16];desc=s[17];if(w==0u||h==0u||(size_t)w*h>cap/4u)return -2;
 p=18u+id;cmbytes=(cmbits+7u)/8u;if(cm){if(cmbytes==0u||p+(size_t)cmlen*cmbytes>n)return -3;}p+=(size_t)cmlen*cmbytes;bytes=(bpp+7u)/8u;if(bytes==0u)return -4;rle=(t>=9u);top=(desc&0x20u)!=0u;right=(desc&0x10u)!=0u;total=w*h;outi=0u;
 while(outi<total){unsigned int count,k;unsigned char one[4];if(rle){unsigned int hdr;if(p>=n)return -5;hdr=s[p++];count=(hdr&127u)+1u;if((hdr&128u)!=0u){if(cm||t==9u){unsigned int idx;if(p+bytes>n)return -5;idx=(bytes==1u)?s[p]:r16(s+p);p+=bytes;if(idx<cmfirst||idx>=cmfirst+cmlen)return -6;px(s+18u+id+(size_t)(idx-cmfirst)*cmbytes,cmbits,one);}else{if(p+bytes>n)return -5;px(s+p,bpp,one);p+=bytes;}for(k=0u;k<count&&outi<total;k++,outi++){unsigned int x=outi%w,y=outi/w,dx=right?(w-1u-x):x,dy=top?y:(h-1u-y);size_t q=((size_t)dy*w+dx)*4u;d[q]=one[0];d[q+1]=one[1];d[q+2]=one[2];d[q+3]=one[3];}continue;}}
 else count=1u;
 for(k=0u;k<count&&outi<total;k++,outi++){unsigned char c[4];unsigned int x,y,dx,dy;if(cm||t==1u||t==9u){unsigned int idx;if(p+bytes>n)return -5;idx=(bytes==1u)?s[p]:r16(s+p);p+=bytes;if(idx<cmfirst||idx>=cmfirst+cmlen)return -6;px(s+18u+id+(size_t)(idx-cmfirst)*cmbytes,cmbits,c);}else{if(p+bytes>n)return -5;px(s+p,bpp,c);p+=bytes;}x=outi%w;y=outi/w;dx=right?(w-1u-x):x;dy=top?y:(h-1u-y);{size_t q=((size_t)dy*w+dx)*4u;d[q]=c[0];d[q+1]=c[1];d[q+2]=c[2];d[q+3]=c[3];}}
 }
 *ow=w;*oh=h;*used=(size_t)w*h*4u;return 0;
}
