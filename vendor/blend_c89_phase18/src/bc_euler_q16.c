/* Original integer-only Euler-to-quaternion kernel for BLEND C89.
   Six intrinsic Blender Euler orders (mode 1..6). Q16.16 radians in;
   Q16.16 quaternion wxyz out. Trigonometry uses bounded CORDIC,
   never floating point, heap allocation, or 64-bit integers.
   Approximation only; does not reproduce Blender libm bit-for-bit. */
#include "bc_rig_q16.h"
#include <limits.h>

#define BC_EUL_PI 205887
#define BC_EUL_HALF_PI 102944
#define BC_EUL_TWO_PI 411775
#define BC_EUL_K 39797

static const int bc_eul_atan[17]={51472,30386,16055,8150,4091,2047,1024,
 512,256,128,64,32,16,8,4,2,1};
static const unsigned char bc_eul_axes[6][3]={
 {0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}
};
static const unsigned char bc_eul_parity[6]={0,1,1,0,0,1};

static int bc_eul_shr(int x,unsigned int i)
{
  unsigned int a;
  if(x>=0)return x/(int)(1U<<i);
  a=(unsigned int)(-(x+1))+1U;
  return -(int)(a/(1U<<i));
}
static int bc_eul_sincos(int a,int *s,int *c)
{
  unsigned int i;
  int x=BC_EUL_K,y=0,z,xd,yd,nx,ny,cosneg=0;
  /* Eliminate repeated full turns with bounded integer remainder. */
  a%=BC_EUL_TWO_PI;
  if(a>BC_EUL_PI)a-=BC_EUL_TWO_PI;
  if(a< -BC_EUL_PI)a+=BC_EUL_TWO_PI;
  if(a>BC_EUL_HALF_PI){a=BC_EUL_PI-a;cosneg=1;}
  else if(a< -BC_EUL_HALF_PI){a=-BC_EUL_PI-a;cosneg=1;}
  z=a;
  for(i=0U;i<17U;i++){
    xd=bc_eul_shr(x,i);yd=bc_eul_shr(y,i);
    if(z>=0){
      if(bc_qadd_checked(x,-yd,&nx)!=0 ||
         bc_qadd_checked(y,xd,&ny)!=0)return BC_RIG_BAD;
      z-=bc_eul_atan[i];
    }else{
      if(bc_qadd_checked(x,yd,&nx)!=0 ||
         bc_qadd_checked(y,-xd,&ny)!=0)return BC_RIG_BAD;
      z+=bc_eul_atan[i];
    }
    x=nx;y=ny;
  }
  *s=y;*c=cosneg?-x:x;
  return BC_RIG_OK;
}
/* Match Blender's six Euler ordering/parity conventions, documented in its
   historical BLI math_rotation eulO_to_quat implementation. */
int bc_rig_euler_quat(const int euler[3],unsigned int order,int out[4])
{
  unsigned int id,i,j,k;int ti,tj,th;
  int ci,cj,ch,si,sj,sh,cc,cs,sc,ss,p0,p1;
  int v[4]={0,0,0,0};
  if(order<1U ||order>6U)return BC_RIG_UNSUPPORTED;
  id=order-1U;
  i=bc_eul_axes[id][0];j=bc_eul_axes[id][1];k=bc_eul_axes[id][2];
  ti=euler[i]/2;tj=euler[j]/2;th=euler[k]/2;
  if(bc_eul_parity[id])tj=-tj;
  if(bc_eul_sincos(ti,&si,&ci)!=0 ||
     bc_eul_sincos(tj,&sj,&cj)!=0 ||
     bc_eul_sincos(th,&sh,&ch)!=0)return BC_RIG_BAD;
#define EMUL(a,b,d) if(bc_qmul_checked((a),(b),&(d))!=0)return BC_RIG_BAD
#define EADD(a,b,d) if(bc_qadd_checked((a),(b),&(d))!=0)return BC_RIG_BAD
  EMUL(ci,ch,cc);EMUL(ci,sh,cs);EMUL(si,ch,sc);EMUL(si,sh,ss);
  EMUL(cj,cc,p0);EMUL(sj,ss,p1);EADD(p0,p1,v[0]);
  EMUL(cj,sc,p0);EMUL(sj,cs,p1);EADD(p0,-p1,v[i+1U]);
  EMUL(cj,ss,p0);EMUL(sj,cc,p1);EADD(p0,p1,v[j+1U]);
  EMUL(cj,cs,p0);EMUL(sj,sc,p1);EADD(p0,-p1,v[k+1U]);
#undef EMUL
#undef EADD
  if(bc_eul_parity[id])v[j+1U]=-v[j+1U];
  for(i=0U;i<4U;i++)out[i]=v[i];
  return BC_RIG_OK;
}
