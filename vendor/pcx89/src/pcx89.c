/* PCX89: Quake II indexed/24-bit PCX decoder. C89, no heap, no floating point.
   Reject truncated RLE, invalid scanline width and absent 256-color palette. */
#include "pcx89.h"
static unsigned int rd16(const unsigned char *p){return (unsigned int)p[0] | ((unsigned int)p[1]<<8);}
static int info89(const unsigned char *s,size_t n,unsigned int *w,unsigned int *h,
                  unsigned int *pitch,unsigned int *planes){
 unsigned int x0,y0,x1,y1;
 if(!s||n<128u||s[0]!=10u||s[2]!=1u||s[3]!=8u)return 0;
 x0=rd16(s+4);y0=rd16(s+6);x1=rd16(s+8);y1=rd16(s+10);
 if(x1<x0||y1<y0)return 0;
 *w=x1-x0+1u;*h=y1-y0+1u;
 *pitch=rd16(s+66);*planes=s[65];
 if(*w==0u||*h==0u||*pitch<*w||*pitch>65535u||(*planes!=1u&&*planes!=3u))return 0;
 /* Protect size multiplications and nonsensical images. */
 if(*w>4096u||*h>4096u||*h>0xffffffffu/(*w))return 0;
 if(n<128u+(*planes==1u?769u:0u))return 0;
 if(*planes==1u&&s[n-769u]!=12u)return 0;
 return 1;
}
int pcx89_probe(const unsigned char*s,size_t n){unsigned int w,h,p,b;return info89(s,n,&w,&h,&p,&b);}
int pcx89_decode_rgba(const unsigned char*s,size_t n,unsigned char*d,size_t cap,
                      unsigned int*w,unsigned int*h,size_t*used){
 unsigned int width,height,pitch,planes,row,plane,col;
 size_t pos,limit,need,i;int pending=0,run_value=0,run_left=0;
 const unsigned char *palette;
 if(!d||!w||!h||!used||!info89(s,n,&width,&height,&pitch,&planes))return -1;
 if((size_t)width> ((size_t)-1)/4u/(size_t)height)return -1;
 need=(size_t)width*(size_t)height*4u;
 if(cap<need)return -2;
 *w=width;*h=height;*used=need;
 for(i=0u;i<need;i+=4u){d[i]=d[i+1u]=d[i+2u]=0u;d[i+3u]=255u;}
 limit=(planes==1u)?n-769u:n;
 palette=(planes==1u)?s+n-768u:0;
 pos=128u;
 for(row=0u;row<height;row++)for(plane=0u;plane<planes;plane++)for(col=0u;col<pitch;col++){
  unsigned int v;size_t target;
  if(run_left>0){v=(unsigned int)run_value;run_left--;}
  else {
   if(pos>=limit)return -1;
   pending=s[pos++];
   if((pending&0xc0)==0xc0){run_left=pending&0x3f;if(!run_left||pos>=limit)return -1;run_value=s[pos++];run_left--;v=(unsigned int)run_value;}
   else v=(unsigned int)pending;
  }
  if(col>=width)continue;
  target=((size_t)row*(size_t)width+(size_t)col)*4u;
  if(planes==1u){d[target]=(unsigned char)v;}
  else d[target+(size_t)plane]=(unsigned char)v;
 }
 /* Some real-world encoders pad the final RLE run. Once all requested
    pixels are present, do not read or expose extra scanline padding. */
 if(planes==1u){for(i=0;i<need;i+=4u){size_t j=(size_t)d[i]*3u;d[i]=palette[j];d[i+1u]=palette[j+1u];d[i+2u]=palette[j+2u];}}
 return 0;
}
