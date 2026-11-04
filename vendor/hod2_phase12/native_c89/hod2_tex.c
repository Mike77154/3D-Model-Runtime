#include "hod2_native.h"
#include "../inherited/c89/hod2_resource_guard.h"
#include <string.h>
static unsigned long twiddle(unsigned long x,unsigned long y){
 unsigned long k=0UL,i=0UL;
 while((x||y)&&i<16UL){k|=(y&1UL)<<(2UL*i);k|=(x&1UL)<<(2UL*i+1UL);x>>=1;y>>=1;i++;}
 return k;
}
static unsigned long index_twiddle(unsigned long x,unsigned long y,unsigned long w,unsigned long h){
 unsigned long small=w<h?w:h;
 if(!small)return 0UL;
 if(w>=h)return (x/small)*small*small+twiddle(x%small,y%small);
 return (y/small)*small*small+twiddle(x%small,y%small);
}
static unsigned char byte_scale(unsigned long v,unsigned int bits){
 if(bits==5U)return (unsigned char)((v<<3)|(v>>2));
 if(bits==6U)return (unsigned char)((v<<2)|(v>>4));
 return (unsigned char)(v*17UL);
}
int hod2_tex_pixel(const unsigned char *src,size_t size,const Hod2TexDesc *d,unsigned int x,unsigned int y,const unsigned long *pal,unsigned char rgba[4]){
 unsigned long j,k,v,layout,fmt,off,idx;unsigned int w,h;
 if(!src||!d||!rgba)return 0;
 w=d->width;h=d->height;off=d->offset;layout=d->fmt>>8;fmt=d->fmt&7UL;
 if(x>=w||y>=h||!hod2_texture_span((unsigned long)w,(unsigned long)h,fmt,layout,off,(unsigned long)size))return 0;
 if(layout==5UL){
  if(!pal)return 0;
  j=index_twiddle(x,y,w,h);k=off+(j>>1);
  if(!hod2_span(size,k,1UL))return 0;
  idx=(src[k]>>((j&1UL)*4UL))&15UL;v=pal[idx]&0xffffUL;
 }else if(layout==3UL){
  j=index_twiddle(x>>1,y>>1,w>>1,h>>1);k=off+0x800UL+j;
  if(!hod2_span(size,k,1UL))return 0;
  k=off+((unsigned long)src[k]*4UL+((y&1U)+2U*(x&1U)))*2UL;
  if(!hod2_span(size,k,2UL))return 0;
  v=hod2_u16(src+k);
 }else{
  if(layout==1UL)j=twiddle(x,y);
  else if(layout==9UL)j=(unsigned long)y*w+x;
  else if(layout==13UL)j=index_twiddle(x,y,w,h);
  else return 0;
  k=off+j*2UL;if(!hod2_span(size,k,2UL))return 0;
  v=hod2_u16(src+k);
 }
 if(fmt==0UL){rgba[0]=byte_scale((v>>10)&31UL,5U);rgba[1]=byte_scale((v>>5)&31UL,5U);rgba[2]=byte_scale(v&31UL,5U);rgba[3]=(v&0x8000UL)?255U:0U;}
 else if(fmt==1UL){rgba[0]=byte_scale((v>>11)&31UL,5U);rgba[1]=byte_scale((v>>5)&63UL,6U);rgba[2]=byte_scale(v&31UL,5U);rgba[3]=255U;}
 else if(fmt==2UL){rgba[0]=byte_scale((v>>8)&15UL,4U);rgba[1]=byte_scale((v>>4)&15UL,4U);rgba[2]=byte_scale(v&15UL,4U);rgba[3]=byte_scale((v>>12)&15UL,4U);}
 else return 0;
 return 1;
}
static unsigned long crc_step(unsigned long crc,unsigned char b){unsigned int i;crc^=b;for(i=0U;i<8U;i++)crc=(crc>>1)^((crc&1UL)?0xedb88320UL:0UL);return crc&0xffffffffUL;}
static int wr(FILE *f,unsigned char b,unsigned long *crc){if(fputc(b,f)==EOF)return 0;if(crc)*crc=crc_step(*crc,b);return 1;}
static int wr32be(FILE *f,unsigned long w,unsigned long *crc){
 return wr(f,(unsigned char)((w>>24)&255UL),crc)&&wr(f,(unsigned char)((w>>16)&255UL),crc)&&wr(f,(unsigned char)((w>>8)&255UL),crc)&&wr(f,(unsigned char)(w&255UL),crc);
}
static int chunk_head(FILE *f,unsigned long n,const char *tag,unsigned long *crc){
 unsigned int i;*crc=0xffffffffUL;
 if(!wr32be(f,n,(unsigned long*)0))return 0;
 for(i=0;i<4U;i++)if(!wr(f,(unsigned char)tag[i],crc))return 0;
 return 1;
}
static int chunk_tail(FILE *f,unsigned long crc){return wr32be(f,(crc^0xffffffffUL)&0xffffffffUL,(unsigned long*)0);}
int hod2_tex_png(const char *file,const unsigned char *src,size_t size,const Hod2TexDesc *d,const unsigned long *pal){
 FILE *f;unsigned long crc,ad_s1=1UL,ad_s2=0UL,raw,idat;unsigned int x,y,k,w,h;unsigned char p[4];int ok=1;
 static const unsigned char sig[8]={137U,80U,78U,71U,13U,10U,26U,10U};
 if(!file||!src||!d||d->width==0U||d->height==0U||d->width>2048U||d->height>2048U)return 0;
 w=d->width;h=d->height;raw=(1UL+4UL*w)*h;idat=2UL+raw+5UL*h+4UL;
 f=fopen(file,"wb");if(!f)return 0;
 if(fwrite(sig,1,8,f)!=8)ok=0;
 if(ok){ok=chunk_head(f,13UL,"IHDR",&crc);if(ok)ok=wr32be(f,w,&crc)&&wr32be(f,h,&crc)&&wr(f,8U,&crc)&&wr(f,6U,&crc)&&wr(f,0U,&crc)&&wr(f,0U,&crc)&&wr(f,0U,&crc);if(ok)ok=chunk_tail(f,crc);}
 if(ok){ok=chunk_head(f,idat,"IDAT",&crc);if(ok)ok=wr(f,0x78U,&crc)&&wr(f,0x01U,&crc);}
 for(y=0U;ok&&y<h;y++){
  unsigned long bytes=1UL+4UL*w;unsigned long complement=0xffffUL-bytes;
  ok=wr(f,(unsigned char)((y==h-1U)?1U:0U),&crc)&&wr(f,(unsigned char)(bytes&255UL),&crc)&&wr(f,(unsigned char)(bytes>>8),&crc)&&wr(f,(unsigned char)(complement&255UL),&crc)&&wr(f,(unsigned char)(complement>>8),&crc);
  if(!ok)break;
  ok=wr(f,0U,&crc);ad_s2=(ad_s2+ad_s1)%65521UL;
  for(x=0U;ok&&x<w;x++){
   if(!hod2_tex_pixel(src,size,d,x,h-1U-y,pal,p)){ok=0;break;}
   for(k=0U;k<4U;k++){
    ok=wr(f,p[k],&crc);ad_s1=(ad_s1+p[k])%65521UL;ad_s2=(ad_s2+ad_s1)%65521UL;
    if(!ok)break;
   }
  }
 }
 if(ok)ok=wr32be(f,((ad_s2&0xffffUL)<<16)|(ad_s1&0xffffUL),&crc)&&chunk_tail(f,crc);
 if(ok)ok=chunk_head(f,0UL,"IEND",&crc)&&chunk_tail(f,crc);
 if(fclose(f))ok=0;
 if(!ok)remove(file);
 return ok;
}
