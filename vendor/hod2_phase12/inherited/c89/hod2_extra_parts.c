#include "hod2_extra_parts.h"
#include <string.h>

static unsigned long u16p(const unsigned char *p) {
 return (unsigned long)p[0] | ((unsigned long)p[1]<<8);
}
static unsigned long u32p(const unsigned char *p) {
 return (unsigned long)p[0] | ((unsigned long)p[1]<<8) |
        ((unsigned long)p[2]<<16) | ((unsigned long)p[3]<<24);
}
/* Note: C89 requires avoiding ULL constants. signed 32-bit via complement. */
static long s32p(const unsigned char *p) {
 unsigned long w=u32p(p);
 if(w & 0x80000000UL) return -1L-(long)(0xffffffffUL-w);
 return (long)w;
}
static int ptr(const Hod2ExtraExe *ctx, unsigned long va, size_t need,
               const unsigned char **out) {
 const unsigned char *d;
 size_t n, pe, sec, i, off;
 unsigned long nsec, opts, rva, vs, rs, rv, ra;
 if(!ctx || !ctx->data || !out) return 0;
 d=ctx->data; n=ctx->size;
 if(n>=16UL&&memcmp(d,"H2TB",4UL)==0){
  unsigned long count=u32p(d+12),k;
  if(count==0UL||count>8UL||n<16UL+16UL*count)return 0;
  for(k=0UL;k<count;k++){
   const unsigned char *r=d+16UL+16UL*k;
   unsigned long addr=u32p(r),sz=u32p(r+4),raw=u32p(r+8);
   if(va>=addr&&va-addr<=sz&&need<=sz-(va-addr)&&
      raw<=n&&va-addr<=n-raw&&need<=n-raw-(va-addr)){
     *out=d+raw+(va-addr);return 1;
   }
  }return 0;
 }
 if(n<0x40UL || d[0]!='M' || d[1]!='Z')return 0;
 pe=(size_t)u32p(d+0x3c);
 if(pe>n || n-pe<24UL || d[pe]!='P' || d[pe+1]!='E')return 0;
 nsec=u16p(d+pe+6);opts=u16p(d+pe+20);
 if(nsec>32UL || (size_t)opts>n-pe-24UL)return 0;
 sec=pe+24+(size_t)opts;
 if(n-sec<nsec*40UL || va<0x400000UL)return 0;
 rva=va-0x400000UL;
 for(i=0;i<(size_t)nsec;i++){
  off=sec+i*40UL;
  vs=u32p(d+off+8);rv=u32p(d+off+12);
  rs=u32p(d+off+16);ra=u32p(d+off+20);
  if(rva>=rv && rva-rv<(vs>rs?vs:rs)) {
   unsigned long delta=rva-rv;
   if(delta>rs || need>(size_t)(rs-delta))return 0;
   if(ra>n || delta>n-ra || need>n-ra-delta)return 0;
   *out=d+ra+(size_t)delta;return 1;
  }
 }
 return 0;
}
static int rd32(const Hod2ExtraExe *ctx, unsigned long va, unsigned long *out){
 const unsigned char *p;
 if(!ptr(ctx,va,4,&p))return 0;
 *out=u32p(p);return 1;
}
static unsigned long data_va(const Hod2ExtraExe *ctx, unsigned long original){
 return ctx->profile==HOD2EX_VERSION_2000 ? original-0x1000UL : original;
}
void hod2ex_init(Hod2ExtraExe *ctx,const unsigned char *data,size_t n,int profile){
 if(!ctx)return;
 ctx->data=data;ctx->size=n;ctx->profile=profile;
}
int hod2ex_count(const Hod2ExtraExe *ctx,unsigned int type,unsigned int *n){
 unsigned long blk,count;
 if(!n || type>255U || !rd32(ctx,data_va(ctx,0x0052ed08UL)+4UL*type,&blk))return -1;
 if(!blk){*n=0;return 1;}
 if(!rd32(ctx,blk,&count) || count>HOD2EX_MAX_PARTS)return -1;
 *n=(unsigned int)count;
 return 1;
}
int hod2ex_read(const Hod2ExtraExe *ctx,unsigned int type,unsigned int part_index,Hod2ExtraPart *out){
 unsigned int count,k;
 unsigned long blk,arr,desc,dt,drawer,bbase,p;
 unsigned long n,off,size,head,np;
 const unsigned char *bytes;
 if(!out || type>255U || hod2ex_count(ctx,type,&count)!=1 || part_index>=count)return -1;
 memset(out,0,sizeof(*out));
 if(!rd32(ctx,data_va(ctx,0x0052ed08UL)+4UL*type,&blk) ||
    !rd32(ctx,blk+4UL,&arr) ||
    !rd32(ctx,arr+4UL*part_index,&desc))return -1;
 if(!desc)return 0;
 if(!ptr(ctx,desc,0x38UL,&bytes))return -1;
 out->slot=u32p(bytes);out->mesh_info_va=u32p(bytes+4);
 for(k=0;k<4U;k++){
  out->source_vertex_count[k]=u32p(bytes+8UL+k*12UL);
  out->source_vertices_va[k]=u32p(bytes+12UL+k*12UL);
  out->assignment_va[k]=u32p(bytes+16UL+k*12UL);
 }
 if(!rd32(ctx,data_va(ctx,0x004edaecUL)+4UL*type,&dt) ||
    !rd32(ctx,dt+4UL*part_index,&drawer))return -1;
 out->drawer_va=drawer;
 if(ctx->profile==HOD2EX_VERSION_2000){
  if(drawer<0xd0UL)return -1;
  drawer-=0xd0UL;
 }
 switch(drawer){
  case 0x00419e90UL: case 0x00419ea0UL:out->draw_mode=HOD2EX_DRAW_GROUP0;break;
  case 0x00419e40UL:out->draw_mode=HOD2EX_DRAW_GROUP2;break;
  case 0x004198b0UL:out->draw_mode=HOD2EX_DRAW_ALL;break;
  default:out->draw_mode=HOD2EX_DRAW_UNSUPPORTED;break;
 }
 bbase=0x004ed1e0UL;
 switch(type){
  case 0x0eU:bbase=0x004ed348UL;break;
  case 0x1fU:case 0x46U:case 0x4eU:bbase=0x004ed4b0UL;break;
  case 0x41U:bbase=0x004ed780UL;break;
  case 0x4cU:bbase=0x004ed618UL;break;
  case 0x53U:bbase=0x004ed8e8UL;break;
 }
 if(!ptr(ctx,data_va(ctx,bbase)+20UL*part_index,20UL,&bytes))return -1;
 for(k=0;k<4U;k++)out->group_bone[k]=s32p(bytes+4*k);
 out->draw_bone=s32p(bytes+16);
 if(out->draw_mode==HOD2EX_DRAW_UNSUPPORTED)return 1;
 if(!ptr(ctx,out->mesh_info_va,0x18UL,&bytes))return -1;
 off=u32p(bytes+12);size=u32p(bytes+16);head=u16p(bytes+20);np=u16p(bytes+22);
 if(np==0 || np>64 || head>64 || size>2000000UL)return -1;
 n=head*2UL+np*4UL;
 if(n==0 || size%n!=0)return -1;
 p=out->mesh_info_va+off;
 if(!ptr(ctx,p,(size_t)size,&bytes))return -1;
 out->row_count=size/n;out->row_ptr_count=np;out->row_header_bytes=2UL*head;
 out->row_stride=n;out->rows_va=p;
 return 1;
}
int hod2ex_vertex(const Hod2ExtraExe *ctx,const Hod2ExtraPart *part,
                  unsigned long source_offset,Hod2ExtraVertex *out){
 unsigned long r,k,found,ix,mode,ptrva;
 const unsigned char *p;
 int val;
 if(!ctx||!part||!out)return -1;
 memset(out,0,sizeof(*out));out->bone=part->draw_bone;
 if(part->draw_mode==HOD2EX_DRAW_UNSUPPORTED)return 0;
 found=0xffffffffUL;
 for(r=0;r<part->row_count;r++){
  if(!ptr(ctx,part->rows_va+r*part->row_stride+part->row_header_bytes,
          (size_t)(part->row_ptr_count*4UL),&p))return -1;
  for(k=0;k<part->row_ptr_count;k++){
   unsigned long v=u32p(p+k*4UL);
   if(v==0xffffffffUL)break;
   if(v==source_offset){found=r;break;}
  }
  if(found!=0xffffffffUL)break;
 }
 if(found==0xffffffffUL)return 0;
 for(k=0;k<4UL;k++){
  mode=(part->draw_mode==HOD2EX_DRAW_ALL ||
        (part->draw_mode==HOD2EX_DRAW_GROUP0 && k==0UL) ||
        (part->draw_mode==HOD2EX_DRAW_GROUP2 && k==2UL));
  if(!mode || !part->source_vertex_count[k])continue;
  if(!ptr(ctx,part->assignment_va[k]+found,1UL,&p))return -1;
  val=p[0]<128U ? (int)p[0] : (int)p[0]-256;
  if(val<0)continue;
  ix=(unsigned long)val;
  if(ix>=part->source_vertex_count[k])return -1;
  ptrva=part->source_vertices_va[k]+24UL*ix;
  if(!ptr(ctx,ptrva,24UL,&p))return -1;
  out->bone=part->group_bone[k];out->has_override=1;
  for(r=0;r<6UL;r++)out->raw_position_normal[r]=u32p(p+4UL*r);
  /* Records 0..3 deform in order. If a logical vertex is assigned by
   * multiple groups, the later writer wins (original 0x004198B0 sequence). */
 }
 return 1;
}
