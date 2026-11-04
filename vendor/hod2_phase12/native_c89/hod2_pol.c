#include "hod2_native.h"
#include "../inherited/c89/hod2_resource_guard.h"
#include <string.h>
int hod2_pol_model(const unsigned char *data,size_t n,unsigned int index,unsigned long *root,unsigned long *end,unsigned int *model_count){
 unsigned int i=0;unsigned long start,prev=0,off,head=0,cn=0;
 if(!data||n<16)return 0;
 for(;;){
  if(!hod2_span(n,(unsigned long)i*8UL,8UL)||i>=4096U)return 0;
  start=hod2_u32(data+(unsigned long)i*8UL)&0x7fffffffUL;
  if(start==(unsigned long)n){cn=i;break;}
  if(start<prev||start>(unsigned long)n)return 0;
  prev=start;i++;
 }
 if(model_count)*model_count=(unsigned int)cn;
 if(index>=cn)return 0;
 start=hod2_u32(data+(unsigned long)index*8UL)&0x7fffffffUL;
 off=hod2_u32(data+(unsigned long)index*8UL+4UL);
 head=hod2_u32(data+(unsigned long)(index+1U)*8UL)&0x7fffffffUL;
 if(start>head||off<start||off+24UL>head)return 0;
 if(root)*root=off;
 if(end)*end=head;
 return 1;
}
int hod2_pol_scan(const unsigned char *data,size_t n,unsigned long root,unsigned long end,Hod2PolyStat *stats,Hod2VertexCb cb,void *ctx){
 unsigned long pos,flags,ctrl,count,j,tex=0xffffffffUL,matend=0UL,ref,word,need,offset;
 const unsigned char *v;long delta;int is_list;
 if(!data||!stats||root>end||!hod2_span(n,root,end-root)||root+24UL>end)return 0;
 memset(stats,0,sizeof(*stats));flags=hod2_u32(data+root+4UL);pos=root+24UL;
 for(;;){
  if(!hod2_span(n,pos,4UL)||pos+4UL>end)return 0;
  ctrl=hod2_u32(data+pos);if(ctrl==0UL)break;
  if(ctrl&0x80000000UL){
   if(matend && pos!=matend)return 0;
   if(pos+80UL>end)return 0;
   tex=hod2_u32(data+pos+32UL);matend=pos+80UL+(hod2_u32(data+pos+76UL)&~3UL);
   if(matend>end)return 0;
   stats->materials++;if(tex==0xffffffffUL)stats->untextured++;else stats->texrefs++;
   pos+=80UL;continue;
  }
  if(stats->materials==0UL||pos+8UL>end)return 0;
  count=hod2_u32(data+pos+4UL);is_list=(ctrl&8UL)!=0UL;
  if(is_list){if(count>(unsigned long)n/96UL)return 0;count*=3UL;}
  if(count>HOD2_MAX_VERTICES)return 0;
  pos+=8UL;stats->strips++;stats->vertices+=count;
  if(count>=3UL)stats->triangles+=is_list?count/3UL:count-2UL;
  for(j=0;j<count;j++){
   if(!hod2_span(n,pos,4UL)||pos+4UL>end)return 0;
   if(flags&16UL){
    word=hod2_u32(data+pos);need=(word&1UL)?56UL:8UL;
    if(pos+need>end)return 0;
    pos+=need;stats->alt_strips=1UL;continue;
   }
   if(ctrl&32UL){
    word=hod2_u32(data+pos);
    if(word&1UL){ref=pos;need=32UL;}
    else{
     if(pos+8UL>end)return 0;
     delta=hod2_s32(data+pos+4UL);
     if(delta<0L){offset=(unsigned long)(-delta);if(offset>pos+8UL)return 0;ref=pos+8UL-offset;}
     else{if((unsigned long)delta>end-pos-8UL)return 0;ref=pos+8UL+(unsigned long)delta;}
     need=8UL;
    }
   }else{ref=pos;need=32UL;}
   if(ref<root||ref+32UL>end||pos+need>end)return 0;
   v=data+ref;
   if(!hod2_uv_bits_finite(hod2_u32(v+24))||!hod2_uv_bits_finite(hod2_u32(v+28)))stats->nonfinite_uv++;
   if(cb && !cb(v,tex,ctx))return 0;
   pos+=need;
  }
 }
 if(matend && matend!=pos)return 0;
 return 1;
}
