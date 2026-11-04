/* Ghidra-oriented intermediate XMDL record decoder, not an XMDL file decoder.
   A synthetic test harness: XIR1 header + 16-byte record descriptors + payload.
   C89 strict: no heap, no floating-point. */
#include "xir_vendor.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

static unsigned long le32(const unsigned char *p) {
 return (unsigned long)p[0] | ((unsigned long)p[1]<<8) |
        ((unsigned long)p[2]<<16) | ((unsigned long)p[3]<<24);
}
static int read_at(FILE *f,long off,unsigned char *dst,unsigned long n) {
 if(off<0 || n>64UL || fseek(f,off,SEEK_SET)!=0)return 0;
 return fread(dst,1,(size_t)n,f)==n;
}
static long fixed16(unsigned long b) {
 unsigned long exp=(b>>23)&255UL, frac=b&0x7fffffUL,m;int shift;long v;
 if(exp==0UL || exp==255UL)return 0L;
 m=(1UL<<23)|frac;shift=(int)exp-134;
 if(shift>=0){if(shift>7 || m>(2147483647UL>>shift))return (b&0x80000000UL)?-2147483647L:2147483647L;v=(long)(m<<shift);}
 else {if(shift<-31)return 0L;v=(long)(m>>(-shift));}
 return (b&0x80000000UL)?-v:v;
}
static void printnum(FILE *f,unsigned long bits) {
 long v=fixed16(bits);unsigned long a=v<0?(unsigned long)-v:(unsigned long)v;
 unsigned long integer=a/65536UL,frac=((a%65536UL)*10000UL+32768UL)/65536UL;
 if(frac==10000UL){integer++;frac=0UL;}
 fprintf(f,"%s%lu.%04lu",v<0?"-":"",integer,frac);
}
static unsigned long attribute_size(unsigned long id) {
 if(id==0UL || id==1UL)return 12UL;
 if(id==2UL || id==3UL || id==8UL)return 4UL;
 if(id>=4UL && id<=7UL)return 8UL;
 if(id>=9UL && id<=11UL)return 16UL;
 return 0UL;
}
static int vector(FILE *f,long at,const char *prefix,int components,FILE *out) {
 unsigned char b[16];int k;
 if(!read_at(f,at,b,(unsigned long)components*4UL))return 0;
 fprintf(out,"%s",prefix);
 for(k=0;k<components;k++){fputc(' ',out);printnum(out,le32(b+k*4));}
 fputc('\n',out);return 1;
}
/* On-disk *test-container* header: ASCII XIR1 + record_count(u32) + payload_size(u32).
 * Each 16-byte descriptor: type(u32), aux(u32), relative_offset(u32), byte_length(u32).
 * The latter fields reflect the record-table layout inferred in the DLL; the
 * external XIR1 container itself was invented only for regression tests. */
int xir_decode(FILE *src,FILE *obj,XIR_Stats *s,int verbose) {
 unsigned char b[32],d[16];long size,payload_start;unsigned long count,blob, i,j,n,typ,offset,len,declared;
 unsigned long attrs[XIR_MAX_ATTRIBUTES],offs[XIR_MAX_ATTRIBUTES],numattrs=0,stride=0;
 unsigned long vcount=0,tcount=0,vbase=0,vtbase=0,vnbase=0;
 long v_off=-1L,t_off=-1L;int pos_id=-1,norm_id=-1,uv_id=-1;
 if(!s || !src)return 0;
 memset(s,0,sizeof(*s));memset(attrs,0,sizeof(attrs));memset(offs,0,sizeof(offs));
 if(fseek(src,0L,SEEK_END)!=0)return 0;
 size=ftell(src);if(size<12L || !read_at(src,0L,b,12UL))return 0;
 if(memcmp(b,"XIR1",4)!=0)return 0;
 count=le32(b+4);blob=le32(b+8);
 if(count>XIR_MAX_RECORDS || count>(unsigned long)(LONG_MAX-12L)/16UL)return 0;
 payload_start=12L+(long)(count*16UL);
 if(size<payload_start || blob>(unsigned long)(size-payload_start) || payload_start+(long)blob!=size)return 0;
 s->records=count;
 for(i=0;i<count;i++) {
  if(!read_at(src,12L+(long)(i*16UL),d,16UL))return 0;
  typ=le32(d);offset=le32(d+8);len=le32(d+12);
  if(offset>blob || len>blob-offset){s->errors++;return 0;}
  if(verbose)printf("XIR record[%lu] type=%lu aux=%lu offset=%lu bytes=%lu\n",i,typ,le32(d+4),offset,len);
  if(typ==1UL){ /* attributes: u32 count, then N u32 type codes */
   if(len<4UL || !read_at(src,payload_start+(long)offset,b,4UL))return 0;
   n=le32(b);if(n>XIR_MAX_ATTRIBUTES || n>(len-4UL)/4UL)return 0;
   numattrs=n;stride=0;pos_id=-1;norm_id=-1;uv_id=-1;
   for(j=0;j<n;j++){
    if(!read_at(src,payload_start+(long)offset+4L+(long)j*4L,b,4UL))return 0;
    attrs[j]=le32(b);offs[j]=stride;declared=attribute_size(attrs[j]);
    if(!declared || stride>256UL-declared){s->errors++;return 0;}
    if(attrs[j]==0UL)pos_id=(int)j;
    if(attrs[j]==1UL)norm_id=(int)j;
    if(attrs[j]==4UL)uv_id=(int)j; /* mapping of 4 to UV0 is provisional */
    stride+=declared;
   }
   s->attributes=numattrs;s->stride=stride;
  } else if(typ==3UL){ /* vertex buffer: count + count*stride bytes */
   if(len<4UL || !read_at(src,payload_start+(long)offset,b,4UL))return 0;
   n=le32(b);if(!stride || n>XIR_MAX_VERTICES || n>(len-4UL)/stride)return 0;
   if(len-4UL!=n*stride)return 0;
   vcount=n;v_off=payload_start+(long)offset+4L;s->vertices+=n;
   if(obj && pos_id>=0){
    for(j=0;j<n;j++){
     if(!vector(src,v_off+(long)(j*stride+offs[pos_id]),"v",3,obj))return 0;
    }
    s->positions+=n;
    if(norm_id>=0){for(j=0;j<n;j++){
     if(!vector(src,v_off+(long)(j*stride+offs[norm_id]),"vn",3,obj))return 0;
    }s->normals+=n;}
    if(uv_id>=0){for(j=0;j<n;j++){
     if(!vector(src,v_off+(long)(j*stride+offs[uv_id]),"vt",2,obj))return 0;
    }s->uv0+=n;}
   }
  } else if(typ==4UL){ /* indices: count + triples of u32 */
   if(len<4UL || !read_at(src,payload_start+(long)offset,b,4UL))return 0;
   n=le32(b);if(n>XIR_MAX_TRIANGLES || n>(len-4UL)/12UL || len-4UL!=n*12UL)return 0;
   tcount=n;t_off=payload_start+(long)offset+4L;s->triangles+=n;
  }
 }
 /* Deferred faces allows triangles to precede vertices in the record table. */
 if(t_off>=0 && v_off>=0 && pos_id>=0){
  for(j=0;j<tcount;j++){
   unsigned long ids[3],k;
   if(!read_at(src,t_off+(long)(j*12UL),b,12UL))return 0;
   for(k=0;k<3;k++){ids[k]=le32(b+k*4UL);if(ids[k]>=vcount){s->errors++;return 0;}}
   if(obj){
    fprintf(obj,"f");
    for(k=0;k<3;k++){
     fprintf(obj," %lu",ids[k]+1UL+vbase);
     if(uv_id>=0)fprintf(obj,"/%lu",ids[k]+1UL+vtbase);
     else if(norm_id>=0)fprintf(obj,"/");
     if(norm_id>=0)fprintf(obj,"/%lu",ids[k]+1UL+vnbase);
    }
    fprintf(obj,"\n");
   }
  }
 }
 if(verbose)printf("XIR INTERNAL %lu recs, %lu attribs, stride=%lu, vertices=%lu, triangles=%lu\n",s->records,s->attributes,s->stride,s->vertices,s->triangles);
 return 1;
}
