/* HOD2 H2TB v1: export only initialized, non-executable PE data sections.
 * Strict C89, bounded fixed-size buffers, no dynamic allocation and no floats.
 * Format: 16-byte header (magic, version, profile, segments), then
 *         N 16-byte records (VA, bytes, file offset, FNV1a32), then bytes.
 */
#include "hod2_native.h"
#include <string.h>

static void put32(unsigned char *p,unsigned long n){
 p[0]=(unsigned char)(n&255UL);p[1]=(unsigned char)((n>>8)&255UL);
 p[2]=(unsigned char)((n>>16)&255UL);p[3]=(unsigned char)((n>>24)&255UL);
}
static unsigned long fnv32(const unsigned char *p,size_t n){
 size_t i; unsigned long h=2166136261UL;
 for(i=0;i<n;i++)h=((h^(unsigned long)p[i])*16777619UL)&0xffffffffUL;
 return h;
}
int hod2_tablepack_valid(const unsigned char *data,size_t size,int profile){
 unsigned long count,k,va,len,off,prev_end=0UL;
 const unsigned char *r;
 if(!data||size<16UL||memcmp(data,"H2TB",4)!=0||hod2_u32(data+4)!=1UL||
    hod2_u32(data+8)!=(unsigned long)profile)return 0;
 count=hod2_u32(data+12);
 if(count==0UL||count>8UL||!hod2_span(size,16UL,16UL*count))return 0;
 for(k=0UL;k<count;k++){
  r=data+16UL+16UL*k;
  va=hod2_u32(r);len=hod2_u32(r+4);off=hod2_u32(r+8);
  if(va<0x400000UL||len==0UL||va>0xffffffffUL-len||
     (k>0UL&&va<prev_end)||!hod2_span(size,off,len)||
     off<16UL+16UL*count||fnv32(data+off,(size_t)len)!=hod2_u32(r+12))return 0;
  prev_end=va+len;
 }
 return 1;
}
int hod2_tablepack_data(const unsigned char *data,size_t size,unsigned long va,
                        size_t len,const unsigned char **out){
 unsigned long count,k,start,used,off;const unsigned char *r;
 if(!out||!data||size<16UL||memcmp(data,"H2TB",4)!=0||len>0xffffffffUL||
    va>0xffffffffUL-(unsigned long)len)return 0;
 count=hod2_u32(data+12);
 if(count>8UL||!hod2_span(size,16UL,count*16UL))return 0;
 for(k=0UL;k<count;k++){
  r=data+16UL+16UL*k;start=hod2_u32(r);
  used=hod2_u32(r+4);off=hod2_u32(r+8);
  if(va>=start&&va-start<=used&&(unsigned long)len<=used-(va-start)&&
     hod2_span(size,off+(va-start),(unsigned long)len)){
     *out=data+off+(va-start);return 1;
  }
 }
 return 0;
}
int hod2_tablepack_export(const Hod2PE *pe,const char *filename){
 const unsigned char *d;unsigned char hdr[48];
 unsigned long header,nsec,opts,sects,i,rv,sz,raw,off;
 unsigned long amount=0UL;
 struct Item {unsigned long va,sz,raw;} items[2];
 FILE *f;int ok=1;
 if(!pe||!filename||!pe->file.data||pe->file.size<64UL)return 0;
 d=pe->file.data;
 if(d[0]!='M'||d[1]!='Z')return 0;
 header=hod2_u32(d+0x3cUL);
 if(!hod2_span(pe->file.size,header,24UL))return 0;
 nsec=hod2_u16(d+header+6UL);opts=hod2_u16(d+header+20UL);
 sects=header+24UL+opts;
 if(nsec>32UL||!hod2_span(pe->file.size,sects,nsec*40UL))return 0;
 memset(items,0,sizeof(items));
 for(i=0UL;i<nsec;i++){
  const unsigned char *s=d+sects+i*40UL;
  int kind=-1;
  if(memcmp(s,".rdata",6UL)==0)kind=0;
  if(memcmp(s,".data",5UL)==0 && (s[5]=='\0'||s[5]==' '))kind=1;
  if(kind<0)continue;
  if(items[kind].sz)return 0;
  rv=hod2_u32(s+12UL);sz=hod2_u32(s+16UL);raw=hod2_u32(s+20UL);
  /* Reject executable or code sections; do not ship game machine code. */
  if((hod2_u32(s+36UL)&0x20000020UL)!=0UL||sz==0UL||
      !hod2_span(pe->file.size,raw,sz)||rv>0xffffffffUL-0x400000UL)return 0;
  items[kind].va=rv+0x400000UL;items[kind].sz=sz;items[kind].raw=raw;
 }
 if(!items[0].sz||!items[1].sz||items[0].va+items[0].sz>items[1].va)return 0;
 memset(hdr,0,sizeof(hdr));memcpy(hdr,"H2TB",4UL);
 put32(hdr+4UL,1UL);put32(hdr+8UL,(unsigned long)pe->profile);put32(hdr+12UL,2UL);
 off=48UL;
 for(i=0UL;i<2UL;i++){
  put32(hdr+16UL+i*16UL,items[i].va);
  put32(hdr+20UL+i*16UL,items[i].sz);
  put32(hdr+24UL+i*16UL,off);
  put32(hdr+28UL+i*16UL,fnv32(d+items[i].raw,(size_t)items[i].sz));
  off+=items[i].sz;amount+=items[i].sz;
 }
 if(amount==0UL)return 0;
 f=fopen(filename,"wb");if(!f)return 0;
 if(fwrite(hdr,1UL,sizeof(hdr),f)!=sizeof(hdr))ok=0;
 for(i=0UL;i<2UL&&ok;i++)if(fwrite(d+items[i].raw,1UL,(size_t)items[i].sz,f)!=(size_t)items[i].sz)ok=0;
 if(fclose(f)!=0)ok=0;
 if(!ok)remove(filename);
 return ok;
}
