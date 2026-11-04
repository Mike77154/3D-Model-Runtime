#include "hod2_native.h"
#include <string.h>
int hod2_bank(const Hod2PE *pe,unsigned int index,int motion,Hod2Bank *out){
 unsigned long p,va,n,ptr,count;const unsigned char *s;size_t k;
 if(!pe||!out||index>=HOD2_MAX_BANKS)return 0;
 va=motion?0x004d1b00UL:0x004d0ef4UL;
 if(!hod2_pe_u32(pe,hod2_pe_shift(pe,va,0)+index*4UL,&p)||!p||!hod2_pe_data(pe,p,1,&s))return 0;
 memset(out,0,sizeof(*out));
 for(k=0;k<95U;k++){
  if(!hod2_pe_data(pe,p+(unsigned long)k,1,&s))return 0;
  if(!s[0])break;
  if(s[0]<32U||s[0]>126U)return 0;
  out->name[k]=(char)s[0];
 }
 if(k>=95U||k<4U||strcmp(out->name+k-4,".bin")!=0)return 0;
 va=motion?0x004e2bdcUL:0x004e803cUL;
 if(!hod2_pe_u16(pe,hod2_pe_shift(pe,va,0)+index*2UL,&count)||count==0UL||count>=HOD2_MAX_MODELS)return 0;
 va=motion?0x004e2b14UL:0x004e794cUL;
 if(!hod2_pe_u32(pe,hod2_pe_shift(pe,va,0)+index*4UL,&ptr))return 0;
 if(!hod2_pe_data(pe,ptr,(size_t)count*2U,&s))return 0;
 n=(unsigned long)index;out->index=(unsigned int)n;out->count=(unsigned int)count;out->id_va=ptr;
 return 1;
}
int hod2_bank_id(const Hod2PE *pe,const Hod2Bank *bank,unsigned int i,long *out){
 const unsigned char *p;if(!pe||!bank||!out||i>=bank->count)return 0;
 if(!hod2_pe_data(pe,bank->id_va+(unsigned long)i*2UL,2,&p))return 0;
 *out=hod2_s16(p);return 1;
}
int hod2_find_model(const Hod2PE *pe,unsigned int model,Hod2Bank *out,unsigned int *within){
 unsigned long bank;Hod2Bank b;unsigned int k;long mid;
 if(!out||!within||model>=32768U)return 0;
 if(!hod2_pe_u16(pe,hod2_pe_shift(pe,0x004e83b4UL,0)+(unsigned long)model*2UL,&bank)||bank>=HOD2_MAX_BANKS)return 0;
 if(!hod2_bank(pe,(unsigned int)bank,0,&b))return 0;
 for(k=0;k<b.count;k++)if(hod2_bank_id(pe,&b,k,&mid)&&mid==(long)model){*out=b;*within=k;return 1;}
 return 0;
}
typedef struct TreeState {const Hod2PE *pe;Hod2Node *nodes;unsigned int max,count,depth;unsigned char seen[HOD2_MAX_NODES];} TreeState;
static int tree_rec(TreeState *t,unsigned long va,unsigned int parent){
 const unsigned char *p;unsigned int idx,children,i;unsigned long childva;Hod2Node *n;
 if(t->depth>=26U||t->count>=t->max||!hod2_pe_data(t->pe,va,24,&p))return 0;
 idx=(unsigned int)hod2_u16(p+20);children=(unsigned int)hod2_u16(p+22);
 if(idx>=t->max||children>=35U||t->seen[idx])return 0;
 t->seen[idx]=1;n=&t->nodes[t->count++];n->id=idx;n->parent=parent;n->children=children;n->mid=(unsigned int)hod2_u32(p);
 for(i=0;i<3;i++)n->pos_bits[i]=hod2_u32(p+4+i*4);
 n->va=va;t->depth++;
 for(i=0;i<children;i++){
  if(!hod2_pe_u32(t->pe,va+24UL+(unsigned long)i*4UL,&childva)||!childva||!tree_rec(t,childva,idx)){t->depth--;return 0;}
 }
 t->depth--;return 1;
}
int hod2_skeleton(const Hod2PE *pe,unsigned int type,Hod2Node *nodes,unsigned int cap,unsigned int *count){
 unsigned long ptr,n;TreeState t;const unsigned char *p;unsigned int i;
 if(!pe||!nodes||!count||type>=256U||cap==0U)return 0;
 if(cap>HOD2_MAX_NODES)cap=HOD2_MAX_NODES;
 if(!hod2_pe_u32(pe,hod2_pe_shift(pe,0x004e0430UL,0)+(unsigned long)type*4UL,&ptr)||!ptr)return 0;
 if(!hod2_pe_data(pe,hod2_pe_shift(pe,0x004e0724UL,0)+(unsigned long)type*2UL,2,&p))return 0;
 n=(unsigned long)hod2_s16(p);if(n==0UL||n>cap)return 0;
 memset(&t,0,sizeof(t));t.pe=pe;t.nodes=nodes;t.max=cap;
 if(!tree_rec(&t,ptr,0xffffU)||t.count!=n)return 0;
 for(i=0;i<t.count;i++)if(!t.seen[i])return 0;
 *count=t.count;return 1;
}
int hod2_tex_desc(const Hod2PE *pe,unsigned int bank,unsigned int index,Hod2TexDesc *desc,unsigned int *n){
 unsigned long count,ptr;const unsigned char *p;
 if(!pe||!desc||bank>=HOD2_MAX_BANKS)return 0;
 if(!hod2_pe_u16(pe,hod2_pe_shift(pe,0x0055c0a8UL,0)+(unsigned long)bank*2UL,&count)||count==0UL||count>=HOD2_MAX_TEX)return 0;
 if(n)*n=(unsigned int)count;
 if(index>=count)return 0;
 if(!hod2_pe_u32(pe,hod2_pe_shift(pe,0x0055b9b8UL,0)+(unsigned long)bank*4UL,&ptr))return 0;
 if(!hod2_pe_data(pe,ptr+(unsigned long)index*16UL,16,&p))return 0;
 desc->width=(unsigned int)hod2_u16(p);desc->height=(unsigned int)hod2_u16(p+2);
 desc->fmt=hod2_u32(p+4);desc->offset=hod2_u32(p+8);desc->slot=hod2_u32(p+12);
 return desc->width>0U&&desc->height>0U&&desc->width<=2048U&&desc->height<=2048U;
}
int hod2_mot_stride(unsigned int bones,unsigned long ticks,unsigned long block_size,unsigned long *stride,unsigned long *frames){
 unsigned long s,f;if(bones==0U||bones>256U||ticks<3UL||ticks>100000UL)return 0;
 s=(12UL+6UL*(unsigned long)bones+3UL)&~3UL;f=(ticks-1UL)/2UL+2UL;
 if(s>0x100000UL||f>0xffffffffUL/s)return 0;
 if(block_size!=4UL+f*s)return 0;
 if(stride)*stride=s;
 if(frames)*frames=f;
 return 1;
}
