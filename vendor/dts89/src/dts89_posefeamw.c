#include "dts89_pose37.h"
#include <string.h>

/* Signed saturation is explicit: no undefined signed overflow in matrix sums. */
static dts89_fx16_16 p37_add(dts89_fx16_16 a, dts89_fx16_16 b)
{
 if (b>0 && a>0x7FFFFFFF-b) return 0x7FFFFFFF;
 if (b<0 && a<(-2147483647-1)-b) return (-2147483647-1);
 return a+b;
}
static dts89_fx16_16 p37_neg(dts89_fx16_16 x)
{ return x==(-2147483647-1)?0x7FFFFFFF:-x; }
static dts89_fx16_16 p37_sub(dts89_fx16_16 a,dts89_fx16_16 b)
{ return p37_add(a,p37_neg(b)); }
static dts89_fx16_16 p37_mul3(dts89_fx16_16 a,dts89_fx16_16 b,dts89_fx16_16 c)
{ return p37_add(dts89_fx_mul(a,b),c); }

void dts89_pose37_identity(dts89_fx_mat34 *m)
{ unsigned i; if(!m)return; for(i=0u;i<12u;++i)m->a[i]=0;
  m->a[0]=m->a[5]=m->a[10]=DTS89_FX_ONE; }
void dts89_pose37_mul(const dts89_fx_mat34 *a,const dts89_fx_mat34 *b,dts89_fx_mat34 *out)
{
 dts89_fx_mat34 r; unsigned i,j,k;
 if(!a||!b||!out)return;
 for(i=0u;i<3u;++i)for(j=0u;j<4u;++j) {
  dts89_fx16_16 v= (j==3u)?a->a[i*4u+3u]:0;
  for(k=0u;k<3u;++k)
   v=p37_add(v,dts89_fx_mul(a->a[i*4u+k],b->a[k*4u+j]));
  r.a[i*4u+j]=v;
 }
 *out=r;
}
void dts89_pose37_point(const dts89_fx_mat34 *m,const dts89_fx_vec3 *p,dts89_fx_vec3 *out)
{
 dts89_fx_vec3 r;
 if(!m||!p||!out)return;
 r.x=p37_add(p37_mul3(m->a[0],p->x,dts89_fx_mul(m->a[1],p->y)),p37_mul3(m->a[2],p->z,m->a[3]));
 r.y=p37_add(p37_mul3(m->a[4],p->x,dts89_fx_mul(m->a[5],p->y)),p37_mul3(m->a[6],p->z,m->a[7]));
 r.z=p37_add(p37_mul3(m->a[8],p->x,dts89_fx_mul(m->a[9],p->y)),p37_mul3(m->a[10],p->z,m->a[11]));
 *out=r;
}
static void p37_unpack(const dts89_u32 *bits,dts89_fx16_16 q[4])
{
 unsigned i;
 for(i=0u;i<4u;++i) {
  dts89_s16 v=(dts89_s16)(dts89_u16)(bits[i]&65535u);
  q[i]=(dts89_fx16_16)v*2;
 }
}
static dts89_fx16_16 p37_sqrt(dts89_fx16_16 x)
{
 dts89_fx16_16 z; unsigned n;
 if(x<=0)return 0;
 z=x>DTS89_FX_ONE?x:DTS89_FX_ONE;
 for(n=0u;n<12u;++n)z=(z+dts89_fx_div(x,z))/2;
 return z;
}
static void p37_qnorm(dts89_fx16_16 q[4])
{
 dts89_fx16_16 len=0, inv; unsigned i;
 for(i=0u;i<4u;++i)len=p37_add(len,dts89_fx_mul(q[i],q[i]));
 if(len<256){q[0]=q[1]=q[2]=0;q[3]=DTS89_FX_ONE;return;}
 inv=dts89_fx_div(DTS89_FX_ONE,p37_sqrt(len));
 for(i=0u;i<4u;++i)q[i]=dts89_fx_mul(q[i],inv);
}
static void p37_qblend(const dts89_u32 *b0,const dts89_u32 *b1,dts89_fx16_16 t,dts89_fx16_16 q[4])
{
 dts89_fx16_16 x[4],y[4],dot=0;
 unsigned i;
 p37_unpack(b0,x);p37_unpack(b1,y);
 for(i=0u;i<4u;++i)dot=p37_add(dot,dts89_fx_mul(x[i],y[i]));
 if(dot<0)for(i=0u;i<4u;++i)y[i]=p37_neg(y[i]);
 for(i=0u;i<4u;++i)q[i]=p37_add(x[i],dts89_fx_mul(p37_sub(y[i],x[i]),t));
 p37_qnorm(q);
}
static void p37_from_quat_translation(const dts89_fx16_16 q[4],dts89_fx_vec3 t,dts89_fx_mat34 *m)
{
 dts89_fx16_16 xx,yy,zz,xy,xz,yz,wx,wy,wz;
 dts89_fx16_16 x=q[0],y=q[1],z=q[2],w=q[3];
 xx=dts89_fx_mul(x,x);yy=dts89_fx_mul(y,y);zz=dts89_fx_mul(z,z);
 xy=dts89_fx_mul(x,y);xz=dts89_fx_mul(x,z);yz=dts89_fx_mul(y,z);
 wx=dts89_fx_mul(w,x);wy=dts89_fx_mul(w,y);wz=dts89_fx_mul(w,z);
 m->a[0]=p37_sub(DTS89_FX_ONE,p37_add(yy,zz)*2);
 m->a[1]=p37_add(xy,wz)*2; m->a[2]=p37_sub(xz,wy)*2; m->a[3]=t.x;
 m->a[4]=p37_sub(xy,wz)*2; m->a[5]=p37_sub(DTS89_FX_ONE,p37_add(xx,zz)*2);
 m->a[6]=p37_add(yz,wx)*2; m->a[7]=t.y;
 m->a[8]=p37_add(xz,wy)*2; m->a[9]=p37_sub(yz,wx)*2;
 m->a[10]=p37_sub(DTS89_FX_ONE,p37_add(xx,yy)*2);m->a[11]=t.z;
}
int dts89_pose37_evaluate(const dts89_shape *s,dts89_s32 sequence_index,
 dts89_fx16_16 position,dts89_fx_mat34 *global,dts89_u32 capacity)
{
 dts89_s32 i,par;dts89_sequence_cursor cur; dts89_node_anim_indices ni;
 int rc;dts89_fx16_16 q[4];dts89_fx_vec3 t,t1;dts89_fx_mat34 loc;
 if(!s||!global||s->num_nodes<0||(dts89_u32)s->num_nodes>capacity)return DTS89_ERR_BAD_ARG;
 if(!s->default_rotations_bits||!s->default_translations_bits||!s->nodes)return DTS89_ERR_FORMAT;
 if(sequence_index>=s->num_sequences || sequence_index < -1)return DTS89_ERR_RANGE;
 if(sequence_index>=0){
  const dts89_sequence *seq=&s->sequences[sequence_index];
  if(seq->flags & DTS89_SEQ_BLEND)return DTS89_ERR_UNSUPPORTED;
  if(dts89_bitset_count(&seq->scale_matters))return DTS89_ERR_UNSUPPORTED;
  rc=dts89_sequence_select_keyframes(seq,position,&cur);if(rc)return rc;
 }
 for(i=0;i<s->num_nodes;++i){
  par=s->nodes[i].parent_index;
  if(par>=i||par< -1)return DTS89_ERR_FORMAT;
  p37_unpack(s->default_rotations_bits+(dts89_u32)i*4u,q);p37_qnorm(q);
  rc=dts89_fx_vec3_from_bits3(s->default_translations_bits+(dts89_u32)i*3u,&t);if(rc)return rc;
  if(sequence_index>=0){
   rc=dts89_sequence_get_node_tracks(s,sequence_index,i,&cur,&ni);if(rc)return rc;
   if(ni.rotation_index0>=0){
    p37_qblend(s->node_rotations_bits+(dts89_u32)ni.rotation_index0*4u,
     s->node_rotations_bits+(dts89_u32)ni.rotation_index1*4u,cur.key_pos,q);
   }
   if(ni.translation_index0>=0){
    dts89_fx_vec3 p0;
    rc=dts89_fx_vec3_from_bits3(s->node_translations_bits+(dts89_u32)ni.translation_index0*3u,&p0);if(rc)return rc;
    rc=dts89_fx_vec3_from_bits3(s->node_translations_bits+(dts89_u32)ni.translation_index1*3u,&t1);if(rc)return rc;
    dts89_posefeamw_lerp(p0,t1,cur.key_pos,&t);
   }
  }
  p37_from_quat_translation(q,t,&loc);
  if(par>=0)dts89_pose37_mul(global+par,&loc,global+i);
  else global[i]=loc;
 }
 return DTS89_OK;
}
int dts89_posefeamw_lerp(dts89_fx_vec3 a,dts89_fx_vec3 b,dts89_fx16_16 t,dts89_fx_vec3 *out)
{
 if(!out)return DTS89_ERR_BAD_ARG;
 if(t<0)t=0;
 if(t>DTS89_FX_ONE)t=DTS89_FX_ONE;
 out->x=p37_add(a.x,dts89_fx_mul(p37_sub(b.x,a.x),t));
 out->y=p37_add(a.y,dts89_fx_mul(p37_sub(b.y,a.y),t));
 out->z=p37_add(a.z,dts89_fx_mul(p37_sub(b.z,a.z),t));
 return DTS89_OK;
}
