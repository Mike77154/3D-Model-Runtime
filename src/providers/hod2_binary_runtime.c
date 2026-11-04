/* Minimal no-global-runtime binary primitives required by the authored POL parser.
 * Derived from the supplied Phase11 C89 hod2_binary.c, without its 128MB static
 * CLI workspaces.  The runtime uses its caller-owned arena instead. */
#include "../../vendor/hod2_phase12/native_c89/hod2_native.h"
unsigned long hod2_u16(const unsigned char *p)
{ return (unsigned long)p[0] | ((unsigned long)p[1] << 8); }
unsigned long hod2_u32(const unsigned char *p)
{ return hod2_u16(p) | (hod2_u16(p+2) << 16); }
long hod2_s16(const unsigned char *p)
{ unsigned long u=hod2_u16(p);return (u&32768UL)?(long)u-65536L:(long)u; }
long hod2_s32(const unsigned char *p)
{ unsigned long u=hod2_u32(p);return (u&0x80000000UL)?-1L-(long)(0xffffffffUL-u):(long)u; }
int hod2_span(size_t n, unsigned long pos, unsigned long len)
{ return pos <= (unsigned long)n && len <= (unsigned long)n - pos; }
/* In-process PRS prefix+stream decoder. Not the original CLI workspaces. */
typedef struct H2PRS {const unsigned char *p; size_t n,pos;unsigned int bits,remain;} H2PRS;
static int h2bit(H2PRS *s){int bit;if(!s->remain){if(s->pos>=s->n)return -1;s->bits=s->p[s->pos++];s->remain=8u;}bit=(int)(s->bits&1u);s->bits>>=1;s->remain--;return bit;}
static int h2byte(H2PRS *s){return s->pos<s->n?(int)s->p[s->pos++]:-1;}
int hod2_prs_decode(const unsigned char *p,size_t n,unsigned char *dst,size_t cap,size_t *wrote)
{
 H2PRS s;size_t used=0u,k,start;long displacement;unsigned long w,cnt;int a,b,c;
 if(!p||!dst||!wrote)return 0;
 s.p=p;s.n=n;s.pos=0u;s.bits=0u;s.remain=0u;
 for(;;){
  a=h2bit(&s);if(a<0)return 0;
  if(a){b=h2byte(&s);if(b<0||used>=cap)return 0;dst[used++]=(unsigned char)b;continue;}
  a=h2bit(&s);if(a<0)return 0;
  if(a){b=h2byte(&s);c=h2byte(&s);if(b<0||c<0)return 0;
   w=(unsigned long)b+((unsigned long)c<<8);
   if(w==0UL){*wrote=used;return 1;}
   displacement=(long)(w>>3)-8192L;cnt=w&7UL;
   if(cnt==0UL){b=h2byte(&s);if(b<0)return 0;cnt=(unsigned long)b+1UL;}
   else cnt+=2UL;
  }else{
   a=h2bit(&s);b=h2bit(&s);c=h2byte(&s);
   if(a<0||b<0||c<0)return 0;
   cnt=(unsigned long)((a<<1)|b)+2UL;displacement=(long)c-256L;
  }
  if(displacement>=0L||used<(size_t)(-displacement)||cnt>cap-used)return 0;
  start=used-(size_t)(-displacement);
  for(k=0u;k<(size_t)cnt;k++)dst[used++]=dst[start+k];
 }
}

/* Phase12 standalone H2TB address resolver. Runtime intentionally accepts
 * tablepacks only; the complete vendor CLI also supports original PE files.
 * The byte buffer is caller-owned, verified once by hod2_pe_init(). */
#include <string.h>
int hod2_pe_init(Hod2PE *pe,const unsigned char *data,size_t size,int profile)
{
 if(!pe||!data||(profile!=0&&profile!=1)||
    !hod2_tablepack_valid(data,size,profile))return 0;
 pe->file.data=data;pe->file.size=size;pe->profile=profile;
 return 1;
}
int hod2_pe_data(const Hod2PE *pe,unsigned long va,size_t len,const unsigned char **out)
{
 if(!pe||!out)return 0;
 return hod2_tablepack_data(pe->file.data,pe->file.size,va,len,out);
}
int hod2_pe_u16(const Hod2PE *pe,unsigned long va,unsigned long *out)
{
 const unsigned char *p;
 if(!out||!hod2_pe_data(pe,va,2U,&p))return 0;
 *out=hod2_u16(p);return 1;
}
int hod2_pe_u32(const Hod2PE *pe,unsigned long va,unsigned long *out)
{
 const unsigned char *p;
 if(!out||!hod2_pe_data(pe,va,4U,&p))return 0;
 *out=hod2_u32(p);return 1;
}
unsigned long hod2_pe_shift(const Hod2PE *pe,unsigned long orig,int data_section)
{
 if(!pe||pe->profile==0)return orig;
 return orig-(data_section?4080UL:4096UL);
}
