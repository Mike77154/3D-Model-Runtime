#include "hod2_native.h"
#include <string.h>
unsigned char hod2_workspace_raw[HOD2_FILE_CAP];
unsigned char hod2_workspace_decoded[HOD2_FILE_CAP];
unsigned long hod2_u16(const unsigned char *b){return (unsigned long)b[0]|((unsigned long)b[1]<<8);}
unsigned long hod2_u32(const unsigned char *b){return (unsigned long)b[0]|((unsigned long)b[1]<<8)|((unsigned long)b[2]<<16)|((unsigned long)b[3]<<24);}
long hod2_s16(const unsigned char *b){unsigned long n=hod2_u16(b);return (n&0x8000UL)?(long)n-65536L:(long)n;}
long hod2_s32(const unsigned char *b){unsigned long n=hod2_u32(b);return (n&0x80000000UL)?-1L-(long)(0xffffffffUL-n):(long)n;}
int hod2_span(size_t size,unsigned long offset,unsigned long length){return offset<=(unsigned long)size&&length<=(unsigned long)size-offset;}
int hod2_read_file(const char *path,unsigned char *dst,size_t cap,size_t *len){
 FILE *f; size_t n,tot=0;int c;
 if(!path||!dst||!len)return 0;
 f=fopen(path,"rb");if(!f)return 0;
 while(tot<cap){n=fread(dst+tot,1,cap-tot,f);tot+=n;if(n==0)break;}
 c=fgetc(f);if(ferror(f)||c!=EOF){fclose(f);return 0;}
 fclose(f);*len=tot;return 1;
}
int hod2_write_file(const char *path,const unsigned char *src,size_t len){
 FILE *f=fopen(path,"wb");int ok;
 if(!f)return 0;
 ok=(fwrite(src,1,len,f)==len);if(fclose(f))ok=0;return ok;
}
typedef struct Hod2Bit {const unsigned char *src;size_t len,pos;unsigned int bits,remaining;} Hod2Bit;
static int nextbit(Hod2Bit *q){int v;if(!q->remaining){if(q->pos>=q->len)return -1;q->bits=q->src[q->pos++];q->remaining=8;}v=(int)(q->bits&1U);q->bits>>=1;q->remaining--;return v;}
static int nextbyte(Hod2Bit *q){if(q->pos>=q->len)return -1;return q->src[q->pos++];}
int hod2_prs_decode(const unsigned char *src,size_t len,unsigned char *out,size_t cap,size_t *wrote){
 Hod2Bit q;size_t used=0,start,k;long displacement;unsigned long cnt,w;int a,b,c;
 if(!src||!out||!wrote)return 0;
 q.src=src;q.len=len;q.pos=0;q.bits=q.remaining=0;
 for(;;){a=nextbit(&q);if(a<0)return 0;
  if(a){b=nextbyte(&q);if(b<0||used>=cap)return 0;out[used++]=(unsigned char)b;continue;}
  a=nextbit(&q);if(a<0)return 0;
  if(a){b=nextbyte(&q);c=nextbyte(&q);if(b<0||c<0)return 0;w=(unsigned long)b+((unsigned long)c<<8);if(w==0UL){*wrote=used;return 1;}
   displacement=(long)(w>>3)-8192L;cnt=w&7UL;if(cnt==0UL){b=nextbyte(&q);if(b<0)return 0;cnt=(unsigned long)b+1UL;}else cnt+=2UL;
  }else{a=nextbit(&q);b=nextbit(&q);c=nextbyte(&q);if(a<0||b<0||c<0)return 0;cnt=(unsigned long)((a<<1)|b)+2UL;displacement=(long)c-256L;}
  if(displacement>=0L||used<(size_t)(-displacement)||cnt>cap-used)return 0;
  start=used-(size_t)(-displacement);
  for(k=0;k<(size_t)cnt;k++)out[used++]=out[start+k];
 }
}
int hod2_file_decode(const unsigned char *src,size_t n,unsigned char *dst,size_t cap,size_t *out_size){
 size_t decoded;unsigned long expected;
 if(!src||!dst||!out_size)return 0;
 if(n>=5){expected=hod2_u32(src);if(expected>0UL&&expected<=cap){
   if(hod2_prs_decode(src+4,n-4,dst,cap,&decoded)&&decoded==(size_t)expected){*out_size=decoded;return 1;}
 }}
 if(n>cap)return 0;
 memcpy(dst,src,n);*out_size=n;return 1;
}
int hod2_pe_init(Hod2PE *pe,const unsigned char *data,size_t n,int profile){
 unsigned long hdr;
 if(!pe||!data||(profile!=0&&profile!=1))return 0;
 if(n>=16UL&&memcmp(data,"H2TB",4UL)==0){
  if(!hod2_tablepack_valid(data,n,profile))return 0;
  pe->file.data=data;pe->file.size=n;pe->profile=profile;return 1;
 }
 if(n<64UL||data[0]!='M'||data[1]!='Z')return 0;
 hdr=hod2_u32(data+0x3c);if(!hod2_span(n,hdr,24UL)||memcmp(data+hdr,"PE\0\0",4)!=0)return 0;
 pe->file.data=data;pe->file.size=n;pe->profile=profile;return 1;
}
int hod2_pe_data(const Hod2PE *pe,unsigned long va,size_t len,const unsigned char **out){
 unsigned long pp,num,opt,sects,k,rv,vs,rs,ra,rva,delta;const unsigned char *d;size_t n;
 if(!pe||!out||va<0x400000UL)return 0;
 d=pe->file.data;n=pe->file.size;
 if(n>=16UL&&memcmp(d,"H2TB",4UL)==0)return hod2_tablepack_data(d,n,va,len,out);
 pp=hod2_u32(d+0x3c);
 if(!hod2_span(n,pp,24UL))return 0;
 num=hod2_u16(d+pp+6);opt=hod2_u16(d+pp+20);
 if(num>32UL||!hod2_span(n,pp+24UL,opt))return 0;
 sects=pp+24UL+opt;if(!hod2_span(n,sects,num*40UL))return 0;
 rva=va-0x400000UL;
 /* Source PE files may map VA differently from contiguous raw bytes: use section headers. */
 for(k=0;k<num;k++){
  const unsigned char *s=d+sects+k*40UL;vs=hod2_u32(s+8);rv=hod2_u32(s+12);rs=hod2_u32(s+16);ra=hod2_u32(s+20);
  if(rva>=rv&&rva-rv<(vs>rs?vs:rs)){
   delta=rva-rv;
   if(delta>rs||len>(size_t)(rs-delta)||!hod2_span(n,ra+delta,(unsigned long)len))return 0;
   *out=d+ra+delta;return 1;
  }
 }
 /* Headers may be referenced directly. */
 if(rva<sects&&hod2_span(n,rva,(unsigned long)len)){*out=d+rva;return 1;}
 return 0;
}
int hod2_pe_u16(const Hod2PE *pe,unsigned long va,unsigned long *val){const unsigned char *p;if(!val||!hod2_pe_data(pe,va,2,&p))return 0;*val=hod2_u16(p);return 1;}
int hod2_pe_u32(const Hod2PE *pe,unsigned long va,unsigned long *val){const unsigned char *p;if(!val||!hod2_pe_data(pe,va,4,&p))return 0;*val=hod2_u32(p);return 1;}
unsigned long hod2_pe_shift(const Hod2PE *pe,unsigned long va,int is_data){
 if(!pe||pe->profile==0)return va;
 return va-(is_data?4080UL:4096UL);
}
