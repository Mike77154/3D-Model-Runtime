#include "hlfp_goldsrc_shade89.h"
#include <limits.h>
/* Explicit fixed-point and 32-bit-safe scaling (including 32-bit long). */
static hlfp_fixed abs89(hlfp_fixed v) {
 return v == INT_MIN ? INT_MAX : (v < 0 ? -v : v);
}
static hlfp_fixed mul89(hlfp_fixed a,hlfp_fixed b) {
 hlfp_fixed v=0;
 (void)hlfp_fixed_mul(a,b,&v);
 return v;
}
static hlfp_fixed dot89(hlfp_vec3 a,hlfp_vec3 b) {
 return mul89(a.x,b.x)+mul89(a.y,b.y)+mul89(a.z,b.z);
}
static hlfp_vec3 cross89(hlfp_vec3 a,hlfp_vec3 b) {
 hlfp_vec3 v;
 v.x=mul89(a.y,b.z)-mul89(a.z,b.y);
 v.y=mul89(a.z,b.x)-mul89(a.x,b.z);
 v.z=mul89(a.x,b.y)-mul89(a.y,b.x);
 return v;
}
/* Binary-search sqrt for fixed inputs [0, 3], no 64-bit multiplication. */
static hlfp_fixed sqrt89(hlfp_fixed a) {
 hlfp_fixed lo=0,hi=2*HLFP_FIXED_ONE;
 int i;
 if(a<=0)return 0;
 for(i=0;i<18;i++){
  hlfp_fixed mid=lo+(hi-lo+1)/2, sq=0;
  if(hlfp_fixed_mul(mid,mid,&sq)!=HLFP_OK || sq>a)hi=mid-1;
  else lo=mid;
  if(lo>=hi)break;
 }
 return lo;
}
static hlfp_vec3 unit89(hlfp_vec3 v) {
 hlfp_vec3 o;
 hlfp_fixed m=abs89(v.x), z=abs89(v.y), t, lensq, len;
 if(z>m)m=z;
 z=abs89(v.z);if(z>m)m=z;
 if(!m){o.x=0;o.y=0;o.z=HLFP_FIXED_ONE;return o;}
 /* Divide by MAX before squaring: avoids overflow for distant camera. */
 o.x=0;o.y=0;o.z=0;
 if(hlfp_fixed_div(v.x,m,&o.x)!=HLFP_OK) o.x=0;
 if(hlfp_fixed_div(v.y,m,&o.y)!=HLFP_OK) o.y=0;
 if(hlfp_fixed_div(v.z,m,&o.z)!=HLFP_OK) o.z=0;
 lensq=mul89(o.x,o.x)+mul89(o.y,o.y)+mul89(o.z,o.z);
 len=sqrt89(lensq);
 if(!len){o.x=0;o.y=0;o.z=HLFP_FIXED_ONE;return o;}
 t=o.x;if(hlfp_fixed_div(t,len,&o.x)!=HLFP_OK)o.x=0;
 t=o.y;if(hlfp_fixed_div(t,len,&o.y)!=HLFP_OK)o.y=0;
 t=o.z;if(hlfp_fixed_div(t,len,&o.z)!=HLFP_OK)o.z=0;
 return o;
}
static int clamp255(int v){return v<0?0:(v>255?255:v);}
void hlfp_gs89_chrome_bone_uv(const hlfp_vec3 *world_normal,
 const hlfp_mat3x4 *bone_matrix,const hlfp_vec3 *eye,
 const hlfp_vec3 *view_right,int width,int height,int *s,int *t) {
 hlfp_vec3 tmp,up,right,n,r;
 hlfp_fixed u,v;
 if(!s||!t)return;
 *s=0;*t=0;
 if(!world_normal||!bone_matrix||!eye||!view_right||width<=0||height<=0)return;
 /* tmp is viewer->bone, as in Valve StudioModel::Chrome. */
 tmp.x=bone_matrix->m[0][3]-eye->x;
 tmp.y=bone_matrix->m[1][3]-eye->y;
 tmp.z=bone_matrix->m[2][3]-eye->z;
 tmp=unit89(tmp);
 r=unit89(*view_right);
 up=cross89(tmp,r);
 if(abs89(up.x)<256&&abs89(up.y)<256&&abs89(up.z)<256){
   r.x=0;r.y=0;r.z=HLFP_FIXED_ONE;
   up=cross89(tmp,r);
   if(abs89(up.x)<256&&abs89(up.y)<256&&abs89(up.z)<256){
     r.x=0;r.y=HLFP_FIXED_ONE;r.z=0;
     up=cross89(tmp,r);
   }
 }
 up=unit89(up);
 right=unit89(cross89(tmp,up));
 n=unit89(*world_normal);
 u=dot89(n,right);v=dot89(n,up);
 if(u< -HLFP_FIXED_ONE)u=-HLFP_FIXED_ONE;
 if(u>HLFP_FIXED_ONE)u=HLFP_FIXED_ONE;
 if(v< -HLFP_FIXED_ONE)v=-HLFP_FIXED_ONE;
 if(v>HLFP_FIXED_ONE)v=HLFP_FIXED_ONE;
 /* SDK UV: (dot+1)*32, normalized to 64 texel chrome map;
  * wrap/clamp happens in material sampler. Bounds <= 64. */
 *s=(int)((u+HLFP_FIXED_ONE)/(2*1024));
 *t=(int)((v+HLFP_FIXED_ONE)/(2*1024));
 /* Width/height intentionally unused: chrome consumes 64x64 virtual tiles;
  * normalization to arbitrary texture size is optional. */
 (void)width;(void)height;
}
int hlfp_gs89_light_value(const hlfp_vec3 *world_normal,
 const hlfp_vec3 *world_lightvec,int ambient_8,int shade_8,
 hlfp_fixed lambert,int flags){
 hlfp_vec3 n,l;
 hlfp_fixed dot,adjust,delta=0;
 int value;
 if(flags&HLFP_STUDIO_NF_FULLBRIGHT)return 255;
 ambient_8=clamp255(ambient_8);shade_8=clamp255(shade_8);
 if(flags&HLFP_STUDIO_NF_FLATSHADE)
  return clamp255(ambient_8+(shade_8*4)/5);
 value=ambient_8+shade_8;
 if(!world_normal||!world_lightvec)return clamp255(value);
 n=unit89(*world_normal);l=unit89(*world_lightvec);
 dot=dot89(n,l);
 if(dot>HLFP_FIXED_ONE)dot=HLFP_FIXED_ONE;
 if(dot< -HLFP_FIXED_ONE)dot=-HLFP_FIXED_ONE;
 if(lambert<HLFP_FIXED_ONE)lambert=HLFP_FIXED_ONE;
 /* Equivalent to (dot + lambert - 1) / lambert, clamp >=0. */
 if(lambert>4*HLFP_FIXED_ONE)lambert=4*HLFP_FIXED_ONE;
 adjust=dot+lambert-HLFP_FIXED_ONE;
 if(adjust>0 && hlfp_fixed_div(adjust,lambert,&delta)==HLFP_OK){
   if(delta>HLFP_FIXED_ONE)delta=HLFP_FIXED_ONE;
   value -= (int)(((long)shade_8*(long)(delta/256))/256L);
 }
 return clamp255(value);
}
int hlfp_gs89_interpolate_light(int a,int b,int c,
 long w0,long w1,long w2,long area){
 long value,mag;
 if(area==0)return clamp255(a);
 if(area<0){area=-area;w0=-w0;w1=-w1;w2=-w2;}
 /* Cap denominator so 255*sum(weights) fits 32-bit even on Win32. */
 while(area>65536L){area/=2;w0/=2;w1/=2;w2/=2;}
 if(area<=0)return clamp255(a);
 if(w0<0)w0=0;
 if(w1<0)w1=0;
 if(w2<0)w2=0;
 mag=w0+w1+w2;if(mag<=0)return clamp255(a);
 value=(long)clamp255(a)*w0+(long)clamp255(b)*w1+(long)clamp255(c)*w2;
 return clamp255((int)(value/mag));
}
