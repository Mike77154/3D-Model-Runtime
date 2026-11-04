/* Phase 05: bounded, integer-only FCurve evaluator. No floating point or heap.
   Evaluates stored action-local curves, NOT NLA/constraints/driver/modifiers.
   Constant/linear/bezier segments only; unsupported modes fail explicitly. */
#include "bc.h"
#include <string.h>

#define BC_EVAL_MAX_KEYS 500000U
#define BC_QONE 65536U
#define BC_QINTMAX 2147483647U

typedef struct BC_EvalKey {
  int x,y,lx,ly,rx,ry;
  unsigned int ipo;
} BC_EvalKey;
static int bc_eval_enabled=0;
static int bc_eval_frame=0;

void bc_eval_start(int frame) {bc_eval_frame=frame;bc_eval_enabled=1;}
void bc_eval_stop(void) {bc_eval_enabled=0;}
int bc_eval_active(void) {return bc_eval_enabled;}

static unsigned int bc_abs_i(int x)
{
  if(x>=0)return (unsigned int)x;
  return (unsigned int)(-(x+1))+1U;
}
/* Multiply an unsigned Q16.16 magnitude by [0, 65536] / 65536.
   Both partial products fit in unsigned 32-bit, no 64-bit integer. */
static unsigned int bc_scale_mag(unsigned int a,unsigned int f)
{
  return (a>>16)*f+(((a&65535U)*f+32768U)>>16);
}
/* Compute a*(1-t)+b*t safely, even for large opposing signed operands. */
static int bc_lerp(int a,int b,unsigned int t,int *out)
{
  unsigned int u,v,w;
  int neg;
  if(t>BC_QONE)return BC_FAILURE;
  u=bc_scale_mag(bc_abs_i(a),BC_QONE-t);
  v=bc_scale_mag(bc_abs_i(b),t);
  if((a<0)==(b<0)){
    w=u+v;neg=(a<0);
  } else if(u>=v){w=u-v;neg=(a<0);}
  else {w=v-u;neg=(b<0);}
  if(w>BC_QINTMAX)w=BC_QINTMAX;
  *out=neg?-(int)w:(int)w;
  return BC_OK;
}
static int bc_bez(int a,int h1,int h2,int b,unsigned int t,int *out)
{
  int p,q,r,s,u,v;
  if(bc_lerp(a,h1,t,&p)!=BC_OK ||bc_lerp(h1,h2,t,&q)!=BC_OK ||
     bc_lerp(h2,b,t,&r)!=BC_OK ||bc_lerp(p,q,t,&s)!=BC_OK ||
     bc_lerp(q,r,t,&u)!=BC_OK ||bc_lerp(s,u,t,&v)!=BC_OK)return BC_FAILURE;
  *out=v;
  return BC_OK;
}
static int bc_read_q(BC_File *b,unsigned int at,int *v)
{
  unsigned int bits;
  return bc_read_word(b,at,&bits)==BC_OK?bc_f32_q16(bits,v):BC_FAILURE;
}
static int bc_eval_read_key(BC_File *b,unsigned int base,unsigned int stride,
                            unsigned int off,unsigned int index,int bezt,BC_EvalKey *key)
{
  unsigned int at,ipo,field_offset;
  const BC_Field *ipof;
  if(index>2147483647U/stride || base>2147483647U-index*stride)return BC_FAILURE;
  at=base+index*stride;
  if(off>stride || (bezt?36U:8U)>stride-off ||at>2147483647U-off)return BC_FAILURE;
  at+=off;
  if(bezt){
    if(bc_read_q(b,at+12U,&key->x)!=BC_OK ||bc_read_q(b,at+16U,&key->y)!=BC_OK ||
       bc_read_q(b,at,&key->lx)!=BC_OK ||bc_read_q(b,at+4U,&key->ly)!=BC_OK ||
       bc_read_q(b,at+24U,&key->rx)!=BC_OK ||bc_read_q(b,at+28U,&key->ry)!=BC_OK)
      return BC_FAILURE;
    ipof=bc_get_field(b,"BezTriple","ipo");
    if(!ipof)return BC_UNSUPPORTED;
    field_offset=ipof->offset;
    if(field_offset>=stride ||bc_anim_field_byte(b,base+index*stride,"BezTriple","ipo",&ipo)!=BC_OK)return BC_FAILURE;
    key->ipo=ipo;
  } else {
    if(bc_read_q(b,at,&key->x)!=BC_OK ||bc_read_q(b,at+4U,&key->y)!=BC_OK)return BC_FAILURE;
    key->lx=key->rx=key->x;
    key->ly=key->ry=key->y;
    key->ipo=1U;
  }
  return BC_OK;
}
static int bc_eval_segment(const BC_EvalKey *a,const BC_EvalKey *b,int frame,
                           int sampled,int *out)
{
  unsigned int l=0U,r=BC_QONE,t;
  int ix,iy;
  unsigned int j;
  if(b->x<=a->x || frame<a->x ||frame>b->x)return BC_FAILURE;
  if(sampled ||a->ipo==1U){
    unsigned int denominator=(unsigned int)b->x-(unsigned int)a->x;
    unsigned int remainder=(unsigned int)frame-(unsigned int)a->x;
    unsigned int f=0U;
    /* Fractional long division, remainder never overflows by checking subtraction first. */
    for(j=0U;j<16U;j++){
      f<<=1;
      if(remainder>=denominator-remainder){
        remainder=remainder-(denominator-remainder); f|=1U;
      } else remainder+=remainder;
    }
    return bc_lerp(a->y,b->y,f,out);
  }
  if(a->ipo==0U){*out=a->y;return BC_OK;}
  if(a->ipo!=2U)return BC_UNSUPPORTED;
  /* Ensure monotonic X. Blender also corrects handle lengths at evaluation;
     our exporter clamps, making this an approximation for overshooting handles. */
  for(j=0U;j<18U;j++){
    t=(l+r)>>1;
    ix=a->rx<a->x?a->x:(a->rx>b->x?b->x:a->rx);
    iy=b->lx<a->x?a->x:(b->lx>b->x?b->x:b->lx);
    if(bc_bez(a->x,ix,iy,b->x,t,&ix)!=BC_OK)return BC_FAILURE;
    if(ix==frame){l=t;r=t;break;}
    if(ix<frame)l=t;else r=t;
  }
  t=(l+r)>>1;
  return bc_bez(a->y,a->ry,b->ly,b->y,t,out);
}
static int bc_eval_extend(BC_File *b,unsigned int at,unsigned int *extend)
{
  const BC_Field *f=bc_get_field(b,"FCurve","extend");
  unsigned char buf[2];
  if(!f){*extend=0U;return BC_OK;}
  if(f->offset>2147483647U-at)return BC_FAILURE;
  if(bc_at(b,at+f->offset,2U,buf)!=BC_OK)return BC_FAILURE;
  *extend=bc_u16(buf,b->endian_le);
  if(*extend>1U)return BC_UNSUPPORTED;
  return BC_OK;
}
static int bc_eval_value(BC_File *b,unsigned int at,unsigned int count,int *value)
{
  BC_U64 ptr;
  const BC_Struct *p;
  const BC_Field *vec;
  unsigned int base,available,stride,i,offset,extend=0U;
  int bezt,r;
  BC_EvalKey prev,next;
  if(count==0U ||count>BC_EVAL_MAX_KEYS)return BC_UNSUPPORTED;
  if(bc_eval_extend(b,at,&extend)!=BC_OK)return BC_UNSUPPORTED;
  if(bc_read_struct_ptr(b,at,"FCurve","bezt",&ptr)!=BC_OK)return BC_UNSUPPORTED;
  bezt=(ptr.lo ||ptr.hi)?1:0;
  if(!bezt &&bc_read_struct_ptr(b,at,"FCurve","fpt",&ptr)!=BC_OK)return BC_UNSUPPORTED;
  p=bc_get_struct(b,bezt?"BezTriple":"FPoint");
  if(!p ||!p->valid)return BC_UNSUPPORTED;
  stride=b->type_len[p->type];
  vec=bc_get_field(b,bezt?"BezTriple":"FPoint","vec");
  if(!vec||!stride||count>2147483647U/stride)return BC_UNSUPPORTED;
  offset=vec->offset;
  if(bc_resolve(b,ptr,count*stride,NULL,&base,&available)!=BC_OK)return BC_FAILURE;
  r=bc_eval_read_key(b,base,stride,offset,0U,bezt,&prev);
  if(r!=BC_OK)return r;
  /* Out-of-range: constant endpoint hold. This is not Blender linear extrapolation. */
  if(bc_eval_frame<prev.x){if(extend)return BC_UNSUPPORTED;*value=prev.y;return BC_OK;}
  if(bc_eval_frame==prev.x){*value=prev.y;return BC_OK;}
  for(i=1U;i<count;i++){
    r=bc_eval_read_key(b,base,stride,offset,i,bezt,&next);
    if(r!=BC_OK)return r;
    if(next.x<=prev.x)return BC_FAILURE;
    if(bc_eval_frame==next.x){*value=next.y;return BC_OK;}
    if(bc_eval_frame<next.x)return bc_eval_segment(&prev,&next,bc_eval_frame,!bezt,value);
    prev=next;
  }
  if(extend &&count>1U)return BC_UNSUPPORTED;
  *value=prev.y;
  return BC_OK;
}
int bc_eval_curve(BC_File *b,FILE *out,unsigned int at,unsigned int count,
                  unsigned int action,unsigned int layer,unsigned int strip,
                  unsigned int slot,unsigned int curve,unsigned int component,
                  const char *path)
{
  BC_U64 ptr;
  unsigned int mod_off;
  const BC_Field *mods=bc_get_field(b,"FCurve","modifiers");
  const BC_Field *first=bc_get_field(b,"ListBase","first");
  int value,r;
  /* Never silently ignore procedural curve modifiers or drivers. */
  if(bc_read_struct_ptr(b,at,"FCurve","driver",&ptr)==BC_OK &&(ptr.lo||ptr.hi))return BC_UNSUPPORTED;
  if(mods &&first){
    mod_off=mods->offset+first->offset;
    if(mod_off>2147483647U-at ||bc_read_pointer(b,at+mod_off,&ptr)!=BC_OK)return BC_FAILURE;
    if(ptr.lo ||ptr.hi)return BC_UNSUPPORTED;
  }
  r=bc_eval_value(b,at,count,&value);
  if(r!=BC_OK)return r;
  if(fprintf(out,"%u\t%u\t%u\t%u\t%u\t%s\t%u\t",action,layer,strip,slot,curve,path,component)<0)return BC_FAILURE;
  bc_print_q16(out,bc_eval_frame);fputc('\t',out);
  bc_print_q16(out,value);fputc('\n',out);
  return ferror(out)?BC_FAILURE:BC_OK;
}
/* Human-readable signed decimal -> Q16.16; maximum 4 fractional digits.
   Avoid atof, decimal floating point, and overflow. */
int bc_parse_q16(const char *text,int *out)
{
  unsigned int whole=0U,frac=0U,digits=0U,mul=1U,raw;
  int negative=0;
  const unsigned char *p=(const unsigned char *)text;
  if(*p=='-' ||*p=='+'){negative=(*p=='-');p++;}
  if(*p<'0'||*p>'9')return BC_FAILURE;
  while(*p>='0'&&*p<='9'){
    if(whole>32767U/10U)return BC_FAILURE;
    whole=whole*10U+(unsigned int)(*p-'0');
    if(whole>32767U)return BC_FAILURE;
    p++;
  }
  if(*p=='.'){
    p++;
    if(*p<'0'||*p>'9')return BC_FAILURE;
    while(*p>='0'&&*p<='9'){
      if(digits>=4U)return BC_FAILURE;
      frac=frac*10U+(unsigned int)(*p-'0');mul*=10U;
      digits++;p++;
    }
  }
  if(*p!=0)return BC_FAILURE;
  raw=(whole<<16)+(frac*65536U+mul/2U)/mul;
  if(raw>BC_QINTMAX)return BC_FAILURE;
  *out=negative?-(int)raw:(int)raw;
  return BC_OK;
}
